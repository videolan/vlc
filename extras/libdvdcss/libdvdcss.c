/*****************************************************************************
 * libdvdcss.c: DVD reading library.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: libdvdcss.c,v 1.5 2001/07/11 02:01:03 sam Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if defined( WIN32 )
#   include <io.h>
#   include "input_iovec.h"
#else
#   include <sys/uio.h>                                      /* struct iovec */
#endif

#include "config.h"
#include "common.h"

#include "videolan/dvdcss.h"
#include "libdvdcss.h"
#include "ioctl.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int _dvdcss_open  ( dvdcss_handle, char *psz_target );
static int _dvdcss_close ( dvdcss_handle );
static int _dvdcss_seek  ( dvdcss_handle, int i_blocks );
static int _dvdcss_read  ( dvdcss_handle, void *p_buffer, int i_blocks );
static int _dvdcss_readv ( dvdcss_handle, struct iovec *p_iovec, int i_blocks );

/*****************************************************************************
 * Local prototypes, win32 specific
 *****************************************************************************/
#if defined( WIN32 )
static int _win32_dvdcss_readv  ( int i_fd, struct iovec *p_iovec,
                                  int i_num_buffers );
static int _win32_dvdcss_aopen  ( char c_drive, dvdcss_handle dvdcss );
static int _win32_dvdcss_aclose ( int i_fd );
static int _win32_dvdcss_aseek  ( int i_fd, int i_blocks, int i_method );
static int _win32_dvdcss_aread  ( int i_fd, void *p_data, int i_blocks );
#endif

/*****************************************************************************
 * dvdcss_open: initialize library, open a DVD device, crack CSS key
 *****************************************************************************/
extern dvdcss_handle dvdcss_open ( char *psz_target, int i_flags )
{
    int i_ret;

    dvdcss_handle dvdcss;

    /* Allocate the library structure */
    dvdcss = malloc( sizeof( struct dvdcss_s ) );
    if( dvdcss == NULL )
    {
        if( ! (i_flags & DVDCSS_INIT_QUIET) )
        {
            DVDCSS_ERROR( "could not initialize library" );
        }

        return NULL;
    }

    /* Initialize structure */
    dvdcss->p_keys = NULL;
    dvdcss->b_debug = i_flags & DVDCSS_INIT_DEBUG;
    dvdcss->b_errors = !(i_flags & DVDCSS_INIT_QUIET);
    dvdcss->psz_error = "no error";

    i_ret = _dvdcss_open( dvdcss, psz_target );
    if( i_ret < 0 )
    {
        free( dvdcss );
        return NULL;
    }

    i_ret = CSSTest( dvdcss );
    if( i_ret < 0 )
    {
        _dvdcss_error( dvdcss, "css test failed" );
        _dvdcss_close( dvdcss );
        free( dvdcss );
        return NULL;
    }

    dvdcss->b_encrypted = i_ret;

    /* If drive is encrypted, crack its key */
    if( dvdcss->b_encrypted )
    {
        i_ret = CSSInit( dvdcss );

        if( i_ret < 0 )
        {
            _dvdcss_close( dvdcss );
            free( dvdcss );
            return NULL;
        }
    }

    return dvdcss;
}

/*****************************************************************************
 * dvdcss_error: return the last libdvdcss error message
 *****************************************************************************/
extern char * dvdcss_error ( dvdcss_handle dvdcss )
{
    return dvdcss->psz_error;
}

/*****************************************************************************
 * dvdcss_seek: seek into the device
 *****************************************************************************/
extern int dvdcss_seek ( dvdcss_handle dvdcss, int i_blocks )
{
    return _dvdcss_seek( dvdcss, i_blocks );
}

/*****************************************************************************
 * dvdcss_crack: crack the current title key
 *****************************************************************************/
extern int dvdcss_crack ( dvdcss_handle dvdcss, int i_block )
{
    title_key_t **pp_writekey;
    title_key_t **pp_currentkey;
    title_key_t *p_titlekey;
    dvd_key_t p_key;
    int i_ret;

    if( ! dvdcss->b_encrypted )
    {
        return 0;
    }

    /* Check if we've already cracked this key */
    p_titlekey = dvdcss->p_keys;
    while( p_titlekey != NULL
            && p_titlekey->p_next != NULL
            && p_titlekey->p_next->i_startlb < i_block )
    {
        p_titlekey = p_titlekey->p_next;
    }

    if( p_titlekey != NULL && p_titlekey->i_startlb == i_block )
    {
        /* We've already cracked this key, nothing to do */
        return 0;
    }

    /* Crack CSS title key for current VTS */
    i_ret = CSSGetKey( dvdcss, i_block, p_key );

    if( i_ret < 0 )
    {
        _dvdcss_error( dvdcss, "fatal error in vts css key" );
        return i_ret;
    }
    else if( i_ret > 0 )
    {
        _dvdcss_error( dvdcss, "decryption unavailable" );
        return -1;
    }

    /* Add key to keytable if it isn't empty */
    if( p_key[0] || p_key[1] || p_key[2] || p_key[3] || p_key[4] )
    {
        /* Find our spot in the list */
        pp_writekey = &(dvdcss->p_keys);
        pp_currentkey = pp_writekey;
        while( *pp_currentkey != NULL
                && (*pp_currentkey)->i_startlb < i_block )
        {
            pp_writekey = pp_currentkey;
            pp_currentkey = &((*pp_currentkey)->p_next);
        }

        /* Write in the new key */
        p_titlekey = *pp_writekey;
        *pp_writekey = malloc( sizeof( title_key_t ) );
        (*pp_writekey)->i_startlb = i_block;
        memcpy( (*pp_writekey)->p_key, p_key, KEY_SIZE );
        (*pp_writekey)->p_next = p_titlekey;
    }

    return 0;
}

/*****************************************************************************
 * dvdcss_read: read data from the device, decrypt if requested
 *****************************************************************************/
extern int dvdcss_read ( dvdcss_handle dvdcss, void *p_buffer,
                                               int i_blocks,
                                               int i_flags )
{
    title_key_t *p_current;
    int i_ret, i_index;

    i_ret = _dvdcss_read( dvdcss, p_buffer, i_blocks );

    if( i_ret <= 0
         || !dvdcss->b_encrypted
         || !(i_flags & DVDCSS_READ_DECRYPT) )
    {
        return i_ret;
    }

    /* find our key */
    p_current = dvdcss->p_keys;
    while( p_current != NULL
            && p_current->p_next
            && p_current->p_next->i_startlb < dvdcss->i_seekpos )
    {
        p_current = p_current->p_next;
    }

    if( p_current == NULL )
    {
        /* no css key found to use, so no decryption to do */
        return 0;
    }

    /* Decrypt the blocks we managed to read */
    for( i_index = i_ret; i_index; i_index-- )
    {
        CSSDescrambleSector( p_current->p_key, p_buffer );
        ((u8*)p_buffer)[0x14] &= 0x8f;
        (u8*)p_buffer += DVDCSS_BLOCK_SIZE;
    }

    return i_ret;
}

/*****************************************************************************
 * dvdcss_readv: read data to an iovec structure, decrypt if reaquested
 *****************************************************************************/
extern int dvdcss_readv ( dvdcss_handle dvdcss, void *p_iovec,
                                                int i_blocks,
                                                int i_flags )
{
#define P_IOVEC ((struct iovec*)p_iovec)
    title_key_t *p_current;
    int i_ret, i_index;
    void *iov_base;
    size_t iov_len;

    i_ret = _dvdcss_readv( dvdcss, P_IOVEC, i_blocks );

    if( i_ret <= 0
         || !dvdcss->b_encrypted
         || !(i_flags & DVDCSS_READ_DECRYPT) )
    {
        return i_ret;
    }

    /* Find our key */
    p_current = dvdcss->p_keys;
    while( p_current != NULL
            && p_current->p_next
            && p_current->p_next->i_startlb < dvdcss->i_seekpos )
    {
        p_current = p_current->p_next;
    }

    if( p_current == NULL )
    {
        /* no css key found to use, so no decryption to do */
        return 0;
    }


    /* Initialize loop for decryption */
    iov_base = P_IOVEC->iov_base;
    iov_len = P_IOVEC->iov_len;

    /* Decrypt the blocks we managed to read */
    for( i_index = i_ret; i_index; i_index-- )
    {
        /* Check that iov_len is a multiple of 2048 */
        if( iov_len & 0x7ff )
        {
            return -1;
        }

        while( iov_len == 0 )
        {
            P_IOVEC++;
            iov_base = P_IOVEC->iov_base;
            iov_len = P_IOVEC->iov_len;
        }

        CSSDescrambleSector( p_current->p_key, iov_base );
        ((u8*)iov_base)[0x14] &= 0x8f;

        (u8*)iov_base += DVDCSS_BLOCK_SIZE;
        (u8*)iov_len -= DVDCSS_BLOCK_SIZE;
    }

    return i_ret;
#undef P_IOVEC
}

/*****************************************************************************
 * dvdcss_close: close the DVD device and clean up the library
 *****************************************************************************/
extern int dvdcss_close ( dvdcss_handle dvdcss )
{
    title_key_t *p_currentkey;
    int i_ret;

    /* Free our list of keys */
    p_currentkey = dvdcss->p_keys;
    while( p_currentkey )
    {
        title_key_t *p_tmpkey = p_currentkey->p_next;
        free( p_currentkey );
        p_currentkey = p_tmpkey;
    }

    i_ret = _dvdcss_close( dvdcss );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    free( dvdcss );

    return 0;
}

/* Following functions are local */

static int _dvdcss_open ( dvdcss_handle dvdcss, char *psz_target )
{
#if defined( WIN32 )
    if( WIN2K )
    {
        char psz_dvd[7];
        _snprintf( psz_dvd, 7, "\\\\.\\%c:", psz_target[0] );
        (HANDLE) dvdcss->i_fd =
                CreateFile( psz_dvd, GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, 0, NULL );
        if( (HANDLE) dvdcss->i_fd == INVALID_HANDLE_VALUE )
        {
            _dvdcss_error( dvdcss, "failed opening device" );
            return -1;
        }
    }
    else
    {
        dvdcss->i_fd = _win32_dvdcss_aopen( psz_target[0], dvdcss );
        if( dvdcss->i_fd == -1 )
        {
            _dvdcss_error( dvdcss, "failed opening device" );
            return -1;
        }
    }

#else
    dvdcss->i_fd = open( psz_target, 0 );

    if( dvdcss->i_fd == -1 )
    {
        _dvdcss_error( dvdcss, "failed opening device" );
        return -1;
    }

#endif

    return 0;
}

static int _dvdcss_close ( dvdcss_handle dvdcss )
{
#if defined( WIN32 )
    if( WIN2K )
    {
        CloseHandle( (HANDLE) dvdcss->i_fd );
    }
    else
    {
        _win32_dvdcss_aclose( dvdcss->i_fd );
    }
#else
    close( dvdcss->i_fd );
#endif

    return 0;
}

static int _dvdcss_seek ( dvdcss_handle dvdcss, int i_blocks )
{
#if defined( WIN32 )
    if( WIN2K )
    {
        LARGE_INTEGER li_read;

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

        li_read.QuadPart = (LONGLONG)i_blocks * DVDCSS_BLOCK_SIZE;

        li_read.LowPart = SetFilePointer( (HANDLE) dvdcss->i_fd,
                                          li_read.LowPart,
                                          &li_read.HighPart, FILE_BEGIN );
        if( (li_read.LowPart == INVALID_SET_FILE_POINTER)
            && GetLastError() != NO_ERROR)
        {
            li_read.QuadPart = -DVDCSS_BLOCK_SIZE;
        }

        li_read.QuadPart /= DVDCSS_BLOCK_SIZE;
        return (int)li_read.QuadPart;
    }
    else
    {
        return ( _win32_dvdcss_aseek( dvdcss->i_fd, i_blocks, SEEK_SET ) );
    }
#else
    off_t i_read;

    dvdcss->i_seekpos = i_blocks;

    i_read = lseek( dvdcss->i_fd,
                    (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE, SEEK_SET );

    return i_read / DVDCSS_BLOCK_SIZE;
#endif

}

static int _dvdcss_read ( dvdcss_handle dvdcss, void *p_buffer, int i_blocks )
{
#if defined( WIN32 ) 
    if( WIN2K )
    {
        int i_bytes;

        if( !ReadFile( (HANDLE) dvdcss->i_fd, p_buffer,
                  i_blocks * DVDCSS_BLOCK_SIZE,
                  (LPDWORD)&i_bytes, NULL ) )
        {
            return -1;
        }
        return i_bytes / DVDCSS_BLOCK_SIZE;
    }
    else
    {
        return _win32_dvdcss_aread( dvdcss->i_fd, p_buffer, i_blocks );
    }

#else
    int i_bytes;

    i_bytes = read( dvdcss->i_fd, p_buffer, (size_t)i_blocks * DVDCSS_BLOCK_SIZE );
    return i_bytes / DVDCSS_BLOCK_SIZE;
#endif

}

static int _dvdcss_readv ( dvdcss_handle dvdcss, struct iovec *p_iovec, int i_blocks )
{
    int i_read;

#if defined( WIN32 )
    i_read = _win32_dvdcss_readv( dvdcss->i_fd, p_iovec, i_blocks );
    return i_read;
#else
    i_read = readv( dvdcss->i_fd, p_iovec, i_blocks );
    return i_read / DVDCSS_BLOCK_SIZE;
#endif
}

#if defined( WIN32 )

/*****************************************************************************
 * _win32_dvdcss_readv: vectored read using ReadFile for Win2K and
 *                      _win32_dvdcss_aread for win9x
 *****************************************************************************/
static int _win32_dvdcss_readv( int i_fd, struct iovec *p_iovec,
                                int i_num_buffers )
{
    int i_index, i_len, i_total = 0;
    char *p_base;
    int i_blocks;

    for( i_index = i_num_buffers; i_index; i_index-- )
    {

        i_len  = p_iovec->iov_len;
        p_base = p_iovec->iov_base;

        if( i_len > 0 )
        {
            if( WIN2K )
            {
                unsigned long int i_bytes;
                if( !ReadFile( (HANDLE) i_fd, p_base, i_len, &i_bytes, NULL ) )
                {
                    return -1;
                    /* One of the reads failed, too bad.
                       We won't even bother returning the reads that went well,
                       and like in the posix spec the file postition is left
                       unspecified after a failure */
                }
                i_blocks = i_bytes / DVDCSS_BLOCK_SIZE;
            }
            else  /* Win9x */
            {
                i_blocks = _win32_dvdcss_aread( i_fd, p_base, i_len );
                if( i_blocks < 0 )
                {
                    return -1;  /* idem */
                }
            }

            if( i_blocks != (i_len / DVDCSS_BLOCK_SIZE) )
            {
                /* we reached the end of the file */
                return i_total;
            }

            i_total += i_blocks;
        }

        p_iovec++;
    }

    return i_total;
}

/*****************************************************************************
 * _win32_dvdcss_aopen: open dvd drive (load aspi and init w32_aspidev
 *                      structure)
 *****************************************************************************/
static int _win32_dvdcss_aopen( char c_drive, dvdcss_handle dvdcss )
{
    HMODULE hASPI;
    DWORD dwSupportInfo;
    struct w32_aspidev *fd;
    int i, j, i_hostadapters;
    long (*lpGetSupport)( void );
    long (*lpSendCommand)( void* );
    
    hASPI = LoadLibrary( "wnaspi32.dll" );
    if( hASPI == NULL )
    {
        _dvdcss_error( dvdcss, "unable to load wnaspi32.dll" );
        return -1;
    }

    (FARPROC) lpGetSupport = GetProcAddress( hASPI, "GetASPI32SupportInfo" );
    (FARPROC) lpSendCommand = GetProcAddress( hASPI, "SendASPI32Command" );
    
    if(lpGetSupport == NULL || lpSendCommand == NULL )
    {
        _dvdcss_debug( dvdcss, "unable to get aspi function pointers" );
        FreeLibrary( hASPI );
        return -1;
    }

    dwSupportInfo = lpGetSupport();

    if( HIBYTE( LOWORD ( dwSupportInfo ) ) == SS_NO_ADAPTERS )
    {
        _dvdcss_debug( dvdcss, "no host adapters found (aspi)" );
        FreeLibrary( hASPI );
        return -1;
    }

    if( HIBYTE( LOWORD ( dwSupportInfo ) ) != SS_COMP )
    {
        _dvdcss_error( dvdcss, "unable to initalize aspi layer" );
        FreeLibrary( hASPI );
        return -1;
    }

    i_hostadapters = LOBYTE( LOWORD( dwSupportInfo ) );
    if( i_hostadapters == 0 )
    {
        FreeLibrary( hASPI );
        return -1;
    }

    fd = malloc( sizeof( struct w32_aspidev ) );
    if( fd == NULL )
    {
        FreeLibrary( hASPI );
        return -1;
    }

    fd->i_blocks = 0;
    fd->hASPI = (long) hASPI;
    fd->lpSendCommand = lpSendCommand;

    if( !WIN2K )
    {
        fd->i_sid = MAKEWORD( ASPI_HAID, ASPI_TARGET );
        return (int) fd;
    }

    c_drive = c_drive > 'Z' ? c_drive - 'a' : c_drive - 'A';

    for( i = 0; i < i_hostadapters; i++ )
    {
        for( j = 0; j < 15; j++ )
        {
            struct SRB_GetDiskInfo srbDiskInfo;

            srbDiskInfo.SRB_Cmd         = SC_GET_DISK_INFO;
            srbDiskInfo.SRB_HaId        = i;
            srbDiskInfo.SRB_Flags       = 0;
            srbDiskInfo.SRB_Hdr_Rsvd    = 0;
            srbDiskInfo.SRB_Target      = j;
            srbDiskInfo.SRB_Lun         = 0;

            lpSendCommand( (void*) &srbDiskInfo );

            if( srbDiskInfo.SRB_Status == SS_COMP &&
                srbDiskInfo.SRB_Int13HDriveInfo == c_drive )
            {
                fd->i_sid = MAKEWORD( i, j );
                return (int) fd;
            }
        }
    }

    free( (void*) fd );
    FreeLibrary( hASPI );
    _dvdcss_debug( dvdcss, "unable to get haid and target (aspi)" );

    return( -1 );
}

/*****************************************************************************
 * _win32_dvdcss_aclose: close dvd drive (unload aspi and free w32_aspidev
 *                       structure)
 *****************************************************************************/
static int _win32_dvdcss_aclose( int i_fd )
{
    struct w32_aspidev *fd = (struct w32_aspidev *) i_fd;

    FreeLibrary( (HMODULE) fd->hASPI );
    free( (void*) i_fd );

    return 0;
}

/*****************************************************************************
 * _win32_dvdcss_aseek: aspi version of _dvdcss_seek
 * 
 * returns the number of blocks read.
 *****************************************************************************/
static int _win32_dvdcss_aseek( int i_fd, int i_blocks, int i_method )
{
    int i_old_blocks;
    char sz_buf[ DVDCSS_BLOCK_SIZE ];
    struct w32_aspidev *fd = (struct w32_aspidev *) i_fd;
    
    i_old_blocks = fd->i_blocks;
    fd->i_blocks = i_blocks;

    if( _win32_dvdcss_aread( i_fd, sz_buf, 1 ) == -1 )
    {
        fd->i_blocks = i_old_blocks;
        return -1;
    }

    (fd->i_blocks)--;

    return fd->i_blocks;
}

/*****************************************************************************
 * _win32_dvdcss_aread: aspi version of _dvdcss_read
 *
 * returns the number of blocks read.
 *****************************************************************************/
static int _win32_dvdcss_aread( int i_fd, void *p_data, int i_blocks )
{
    HANDLE hEvent;
    DWORD dwStart, dwLen;
    struct SRB_ExecSCSICmd ssc;
    struct w32_aspidev *fd = (struct w32_aspidev *) i_fd;

    memset( &ssc, 0, sizeof( ssc ) );

    dwStart = fd->i_blocks;
    dwLen = i_blocks;

    hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if( hEvent == NULL )
    {
        return -1;
    }

    ssc.SRB_Cmd         = SC_EXEC_SCSI_CMD;
    ssc.SRB_Flags       = SRB_DIR_IN | SRB_EVENT_NOTIFY;
    ssc.SRB_HaId        = LOBYTE( fd->i_sid );
    ssc.SRB_Target      = HIBYTE( fd->i_sid );
    ssc.SRB_SenseLen    = SENSE_LEN;
    ssc.SRB_PostProc    = (LPVOID) hEvent;

    ssc.SRB_BufLen      = dwLen * DVDCSS_BLOCK_SIZE;
    ssc.SRB_BufPointer  = p_data;
    ssc.SRB_CDBLen      = 12;

    ssc.CDBByte[0]      = 0xA8; /* RAW */
    ssc.CDBByte[2]      = (UCHAR) dwStart >> 24;
    ssc.CDBByte[3]      = (UCHAR) (dwStart >> 16) & 0xff;
    ssc.CDBByte[4]      = (UCHAR) (dwStart >> 8) & 0xff;
    ssc.CDBByte[5]      = (UCHAR) (dwStart) & 0xff;
    ssc.CDBByte[6]      = (UCHAR) dwLen >> 24;
    ssc.CDBByte[7]      = (UCHAR) (dwLen >> 16) & 0xff;
    ssc.CDBByte[8]      = (UCHAR) (dwLen >> 8) & 0xff;
    ssc.CDBByte[9]      = (UCHAR) (dwLen) & 0xff;

    ResetEvent( hEvent );
    if( fd->lpSendCommand( (void*) &ssc ) == SS_PENDING )
    {
        WaitForSingleObject( hEvent, INFINITE );
    }

    CloseHandle( hEvent );

    if( ssc.SRB_Status != SS_COMP )
    {
        return -1;
    }
        
    fd->i_blocks += i_blocks;

    return i_blocks;
}

#endif

