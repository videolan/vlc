/*****************************************************************************
 * libdvdcss.c: DVD reading library.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: libdvdcss.c,v 1.29 2002/01/21 07:00:21 gbazin Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Håkan Hjort <d95hjort@dtek.chalmers.se>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if defined( WIN32 )
#   include <io.h>                                                 /* read() */
#else
#   include <sys/uio.h>                                      /* struct iovec */
#endif

#if defined( WIN32 )
#   include "input_iovec.h"
#endif

#include "videolan/dvdcss.h"
#include "libdvdcss.h"
#include "ioctl.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int _dvdcss_open  ( dvdcss_handle, char *psz_target );
static int _dvdcss_close ( dvdcss_handle );
static int _dvdcss_readv ( dvdcss_handle, struct iovec *p_iovec, int i_blocks );

/*****************************************************************************
 * Local prototypes, win32 specific
 *****************************************************************************/
#if defined( WIN32 )
static int _win32_dvdcss_readv  ( int i_fd, struct iovec *p_iovec,
                                  int i_num_buffers, char *p_tmp_buffer );
static int _win32_dvdcss_aopen  ( char c_drive, dvdcss_handle dvdcss );
static int _win32_dvdcss_aclose ( int i_fd );
static int _win32_dvdcss_aseek  ( int i_fd, int i_blocks, int i_method );
static int _win32_dvdcss_aread  ( int i_fd, void *p_data, int i_blocks );
#else
static int _dvdcss_raw_open     ( dvdcss_handle, char *psz_target );
#endif

/*****************************************************************************
 * dvdcss_open: initialize library, open a DVD device, crack CSS key
 *****************************************************************************/
extern dvdcss_handle dvdcss_open ( char *psz_target )
{
    struct stat fileinfo;
    int         i_ret;

    char *psz_method = getenv( "DVDCSS_METHOD" );
    char *psz_verbose = getenv( "DVDCSS_VERBOSE" );
#ifndef WIN32
    char *psz_raw_device = getenv( "DVDCSS_RAW_DEVICE" );
#endif

    dvdcss_handle dvdcss;

    /* Allocate the library structure */
    dvdcss = malloc( sizeof( struct dvdcss_s ) );
    if( dvdcss == NULL )
    {
        return NULL;
    }

    /* Initialize structure with default values */
    dvdcss->p_titles = NULL;
    dvdcss->psz_error = "no error";
    dvdcss->i_method = DVDCSS_METHOD_TITLE;
    dvdcss->b_debug = 0;
    dvdcss->b_errors = 1;

    /* Find method from DVDCSS_METHOD environment variable */
    if( psz_method != NULL )
    {
        if( !strncmp( psz_method, "key", 4 ) )
        {
            dvdcss->i_method = DVDCSS_METHOD_KEY;
        }
        else if( !strncmp( psz_method, "disc", 5 ) )
        {
            dvdcss->i_method = DVDCSS_METHOD_DISC;
        }
        else if( !strncmp( psz_method, "title", 5 ) )
        {
            dvdcss->i_method = DVDCSS_METHOD_TITLE;
        }
        else
        {
            _dvdcss_error( dvdcss, "unknown decrypt method, please choose "
                                   "from 'title', 'key' or 'disc'" );
            free( dvdcss );
            return NULL;
        }
    }

    /* Find verbosity from DVDCSS_VERBOSE environment variable */
    if( psz_verbose != NULL )
    {
        switch( atoi( psz_verbose ) )
        {
        case 0:
            dvdcss->b_errors = 0;
            break;
        case 1:
            break;
        case 2:
            dvdcss->b_debug = 1;
            break;
        default:
            _dvdcss_error( dvdcss, "unknown verbose level, please choose "
                                   "from '0', '1' or '2'" );
            free( dvdcss );
            return NULL;
            break;
        }
    }

    /* Open device */
    i_ret = _dvdcss_open( dvdcss, psz_target );
    if( i_ret < 0 )
    {
        free( dvdcss );
        return NULL;
    }

#if defined( WIN32 )
    /* it's not possible to stat a drive letter. Fake a block device */
    fileinfo.st_mode = S_IFBLK;
#else
    if( stat( psz_target, &fileinfo ) < 0 )
    {
        _dvdcss_error( dvdcss, "dvdcss: can't stat target" );
    }
#endif

    if( S_ISBLK( fileinfo.st_mode ) || 
        S_ISCHR( fileinfo.st_mode ) )
    {        
        i_ret = CSSTest( dvdcss );
        if( i_ret < 0 )
        {
            _dvdcss_error( dvdcss, "CSS test failed" );
            /* Disable the CSS ioctls and hope that it works? */
            dvdcss->b_ioctls = 0;
            dvdcss->b_encrypted = 1;
        }
        else
        {
            dvdcss->b_ioctls = 1;
            dvdcss->b_encrypted = i_ret;
        }
    }
    else
    {
        _dvdcss_debug( dvdcss, "dvdcss: file mode, using title method" );
        dvdcss->i_method = DVDCSS_METHOD_TITLE;
        dvdcss->b_ioctls = 0;
        dvdcss->b_encrypted = 1;
        memset( &dvdcss->css.disc, 0, sizeof(dvdcss->css.disc) );
    }

    /* If disc is CSS protected and the ioctls work, authenticate the drive */
    if( dvdcss->b_encrypted && dvdcss->b_ioctls )
    {
        i_ret = CSSGetDiscKey( dvdcss );

        if( i_ret < 0 )
        {
            _dvdcss_close( dvdcss );
            free( dvdcss );
            return NULL;
        }
        
        memset( dvdcss->css.p_unenc_key, 0, KEY_SIZE );
    }

#ifndef WIN32
    if( psz_raw_device != NULL )
    {
        _dvdcss_raw_open( dvdcss, psz_raw_device );
    }
#endif

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
extern int dvdcss_seek ( dvdcss_handle dvdcss, int i_blocks, int i_flags )
{
    /* title cracking method is too slow to be used at each seek */
    if( ( ( i_flags & DVDCSS_SEEK_MPEG )
             && ( dvdcss->i_method != DVDCSS_METHOD_TITLE ) )
             && dvdcss->b_encrypted )
    {
        int     i_ret;

        /* Crack or decrypt CSS title key for current VTS */
        i_ret = CSSGetTitleKey( dvdcss, i_blocks );

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
    }
    else if( i_flags & DVDCSS_SEEK_KEY )
    {
        /* check the title key */
        if( dvdcss_title( dvdcss, i_blocks ) ) 
        {
            return -1;
        }
    }

    return _dvdcss_seek( dvdcss, i_blocks );
}

/*****************************************************************************
 * dvdcss_title: crack or decrypt the current title key if needed
 *****************************************************************************
 * This function should only be called by dvdcss_seek and should eventually
 * not be external if possible.
 *****************************************************************************/
extern int dvdcss_title ( dvdcss_handle dvdcss, int i_block )
{
    dvd_title_t *p_title;
    dvd_title_t *p_newtitle;
    int          i_ret;

    if( ! dvdcss->b_encrypted )
    {
        return 0;
    }

    /* Check if we've already cracked this key */
    p_title = dvdcss->p_titles;
    while( p_title != NULL
            && p_title->p_next != NULL
            && p_title->p_next->i_startlb <= i_block )
    {
        p_title = p_title->p_next;
    }

    if( p_title != NULL
         && p_title->i_startlb == i_block )
    {
        /* We've already cracked this key, nothing to do */
        memcpy( dvdcss->css.p_title_key, p_title->p_key, sizeof(dvd_key_t) );
        return 0;
    }

    /* Crack or decrypt CSS title key for current VTS */
    i_ret = CSSGetTitleKey( dvdcss, i_block );

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

    /* Find our spot in the list */
    p_newtitle = NULL;
    p_title = dvdcss->p_titles;
    while( ( p_title != NULL ) && ( p_title->i_startlb < i_block ) )
    {
        p_newtitle = p_title;
        p_title = p_title->p_next;
    }

    /* Save the found title */
    p_title = p_newtitle;

    /* Write in the new title and its key */
    p_newtitle = malloc( sizeof( dvd_title_t ) );
    p_newtitle->i_startlb = i_block;
    memcpy( p_newtitle->p_key, dvdcss->css.p_title_key, KEY_SIZE );

    /* Link the new title, either at the beginning or inside the list */
    if( p_title == NULL )
    {
        dvdcss->p_titles = p_newtitle;
        p_newtitle->p_next = NULL;
    }
    else
    {
        p_newtitle->p_next = p_title->p_next;
        p_title->p_next = p_newtitle;
    }

    return 0;
}

#define Pkey dvdcss->css.p_title_key
/*****************************************************************************
 * dvdcss_read: read data from the device, decrypt if requested
 *****************************************************************************/
extern int dvdcss_read ( dvdcss_handle dvdcss, void *p_buffer,
                                               int i_blocks,
                                               int i_flags )
{
    int i_ret, i_index;

    i_ret = _dvdcss_read( dvdcss, p_buffer, i_blocks );

    if( i_ret <= 0
         || !dvdcss->b_encrypted
         || !(i_flags & DVDCSS_READ_DECRYPT) )
    {
        return i_ret;
    }

    /* For what we believe is an unencrypted title, 
       check that there are no encrypted blocks */
    if( !( Pkey[0] | Pkey[1] | Pkey[2] | Pkey[3] | Pkey[4] ) ) 
    {
        for( i_index = i_ret; i_index; i_index-- )
        {
            if( ((u8*)p_buffer)[0x14] & 0x30 )
            {
                _dvdcss_error( dvdcss, "no key but found encrypted block" );
                /* Only return the initial range of unscrambled blocks? */
                i_ret = i_index;
                /* or fail completely? return 0; */
            }
            (u8*)p_buffer += DVDCSS_BLOCK_SIZE;
        }
    }

    /* Decrypt the blocks we managed to read */
    for( i_index = i_ret; i_index; i_index-- )
    {
        CSSDescrambleSector( dvdcss->css.p_title_key, p_buffer );
        ((u8*)p_buffer)[0x14] &= 0x8f;
        (u8*)p_buffer += DVDCSS_BLOCK_SIZE;
    }

    return i_ret;
}

/*****************************************************************************
 * dvdcss_readv: read data to an iovec structure, decrypt if requested
 *****************************************************************************/
extern int dvdcss_readv ( dvdcss_handle dvdcss, void *p_iovec,
                                                int i_blocks,
                                                int i_flags )
{
#define P_IOVEC ((struct iovec*)p_iovec)
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

        CSSDescrambleSector( dvdcss->css.p_title_key, iov_base );
        ((u8*)iov_base)[0x14] &= 0x8f;

        (u8*)iov_base += DVDCSS_BLOCK_SIZE;
        (u8*)iov_len -= DVDCSS_BLOCK_SIZE;
    }

    return i_ret;
#undef P_IOVEC
}
#undef Pkey

/*****************************************************************************
 * dvdcss_close: close the DVD device and clean up the library
 *****************************************************************************/
extern int dvdcss_close ( dvdcss_handle dvdcss )
{
    dvd_title_t *p_title;
    int i_ret;

    /* Free our list of keys */
    p_title = dvdcss->p_titles;
    while( p_title )
    {
        dvd_title_t *p_tmptitle = p_title->p_next;
        free( p_title );
        p_title = p_tmptitle;
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

        /* To have access to ioctls, we need read and write access to the
         * device. This is only allowed if you have administrator priviledges
         * so we allow for a fallback method where ioctls are not available but
         * we at least have read access to the device.
         * (See Microsoft Q241374: Read and Write Access Required for SCSI
         * Pass Through Requests) */
        (HANDLE) dvdcss->i_fd =
                CreateFile( psz_dvd, GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING,
                                FILE_FLAG_RANDOM_ACCESS, NULL );

        if( (HANDLE) dvdcss->i_fd == INVALID_HANDLE_VALUE )
            (HANDLE) dvdcss->i_fd =
                    CreateFile( psz_dvd, GENERIC_READ, FILE_SHARE_READ,
                                    NULL, OPEN_EXISTING,
                                    FILE_FLAG_RANDOM_ACCESS, NULL );

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

    /* initialise readv temporary buffer */
    dvdcss->p_readv_buffer   = NULL;
    dvdcss->i_readv_buf_size = 0;

#else
    dvdcss->i_fd = dvdcss->i_read_fd = open( psz_target, 0 );

    if( dvdcss->i_fd == -1 )
    {
        _dvdcss_error( dvdcss, "failed opening device" );
        return -1;
    }

#endif

    return 0;
}

#ifndef WIN32
static int _dvdcss_raw_open ( dvdcss_handle dvdcss, char *psz_target )
{
    dvdcss->i_raw_fd = open( psz_target, 0 );

    if( dvdcss->i_raw_fd == -1 )
    {
        _dvdcss_error( dvdcss, "failed opening raw device, continuing" );
        return -1;
    }
    else
    {
        dvdcss->i_read_fd = dvdcss->i_raw_fd;
    }

    return 0;
}
#endif

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

    /* Free readv temporary buffer */
    if( dvdcss->p_readv_buffer )
    {
        free( dvdcss->p_readv_buffer );
        dvdcss->p_readv_buffer   = NULL;
        dvdcss->i_readv_buf_size = 0;
    }

#else
    close( dvdcss->i_fd );

    if( dvdcss->i_raw_fd >= 0 )
    {
        close( dvdcss->i_raw_fd );
        dvdcss->i_raw_fd = -1;
    }

#endif

    return 0;
}

int _dvdcss_seek ( dvdcss_handle dvdcss, int i_blocks )
{
#if defined( WIN32 )
    dvdcss->i_seekpos = i_blocks;

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
    off_t   i_read;

    dvdcss->i_seekpos = i_blocks;

    i_read = lseek( dvdcss->i_read_fd,
                    (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE, SEEK_SET );

    return i_read / DVDCSS_BLOCK_SIZE;
#endif

}

int _dvdcss_read ( dvdcss_handle dvdcss, void *p_buffer, int i_blocks )
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

    i_bytes = read( dvdcss->i_read_fd, p_buffer,
                    (size_t)i_blocks * DVDCSS_BLOCK_SIZE );
    return i_bytes / DVDCSS_BLOCK_SIZE;
#endif

}

static int _dvdcss_readv ( dvdcss_handle dvdcss, struct iovec *p_iovec,
                           int i_blocks )
{
    int i_read;

#if defined( WIN32 )
    /* Check the size of the readv temp buffer, just in case we need to
     * realloc something bigger */
    if( dvdcss->i_readv_buf_size < i_blocks * DVDCSS_BLOCK_SIZE )
    {
        dvdcss->i_readv_buf_size = i_blocks * DVDCSS_BLOCK_SIZE;

        if( dvdcss->p_readv_buffer ) free( dvdcss->p_readv_buffer );

        /* Allocate a buffer which will be used as a temporary storage
         * for readv */
        dvdcss->p_readv_buffer = malloc( dvdcss->i_readv_buf_size );
        if( !dvdcss->p_readv_buffer )
        {
            _dvdcss_error( dvdcss, " failed (readv)" );
            return -1;
        }
    }

    i_read = _win32_dvdcss_readv( dvdcss->i_fd, p_iovec, i_blocks,
                                  dvdcss->p_readv_buffer );
    return i_read;

#else
    i_read = readv( dvdcss->i_read_fd, p_iovec, i_blocks );
    return i_read / DVDCSS_BLOCK_SIZE;

#endif
}


#if defined( WIN32 )

/*****************************************************************************
 * _win32_dvdcss_readv: vectored read using ReadFile for Win2K and
 *                      _win32_dvdcss_aread for win9x
 *****************************************************************************/
static int _win32_dvdcss_readv( int i_fd, struct iovec *p_iovec,
                                int i_num_buffers, char *p_tmp_buffer )
{
    int i_index;
    int i_blocks, i_blocks_total = 0;

    for( i_index = i_num_buffers; i_index; i_index-- )
    {
        i_blocks_total += p_iovec[i_index-1].iov_len; 
    }

    if( i_blocks_total <= 0 ) return 0;

    i_blocks_total /= DVDCSS_BLOCK_SIZE;

    if( WIN2K )
    {
        unsigned long int i_bytes;
        if( !ReadFile( (HANDLE)i_fd, p_tmp_buffer,
                       i_blocks_total * DVDCSS_BLOCK_SIZE, &i_bytes, NULL ) )
        {
            return -1;
            /* The read failed... too bad.
               As in the posix spec the file postition is left
               unspecified after a failure */
        }
        i_blocks = i_bytes / DVDCSS_BLOCK_SIZE;
    }
    else /* Win9x */
    {
        i_blocks = _win32_dvdcss_aread( i_fd, p_tmp_buffer, i_blocks_total );
        if( i_blocks < 0 )
        {
            return -1;  /* idem */
        }
    }

    /* We just have to copy the content of the temp buffer into the iovecs */
    i_index = 0;
    i_blocks_total = i_blocks;
    while( i_blocks_total > 0 )
    {
        memcpy( p_iovec[i_index].iov_base,
                &p_tmp_buffer[(i_blocks - i_blocks_total) * DVDCSS_BLOCK_SIZE],
                p_iovec[i_index].iov_len );
        /* if we read less blocks than asked, we'll just end up copying
           garbage, this isn't an issue as we return the number of
           blocks actually read */
        i_blocks_total -= ( p_iovec[i_index].iov_len / DVDCSS_BLOCK_SIZE );
        i_index++;
    } 

    return i_blocks;
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

            if( (srbDiskInfo.SRB_Status == SS_COMP) &&
                (srbDiskInfo.SRB_Int13HDriveInfo == c_drive) )
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
    struct SRB_ExecSCSICmd ssc;
    struct w32_aspidev *fd = (struct w32_aspidev *) i_fd;

    /* Create the transfer completion event */
    hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if( hEvent == NULL )
    {
        return -1;
    }

    memset( &ssc, 0, sizeof( ssc ) );

    ssc.SRB_Cmd         = SC_EXEC_SCSI_CMD;
    ssc.SRB_Flags       = SRB_DIR_IN | SRB_EVENT_NOTIFY;
    ssc.SRB_HaId        = LOBYTE( fd->i_sid );
    ssc.SRB_Target      = HIBYTE( fd->i_sid );
    ssc.SRB_SenseLen    = SENSE_LEN;
    
    ssc.SRB_PostProc = (LPVOID) hEvent;
    ssc.SRB_BufPointer  = p_data;
    ssc.SRB_CDBLen      = 12;
    
    ssc.CDBByte[0]      = 0xA8; /* RAW */
    ssc.CDBByte[2]      = (UCHAR) (fd->i_blocks >> 24);
    ssc.CDBByte[3]      = (UCHAR) (fd->i_blocks >> 16) & 0xff;
    ssc.CDBByte[4]      = (UCHAR) (fd->i_blocks >> 8) & 0xff;
    ssc.CDBByte[5]      = (UCHAR) (fd->i_blocks) & 0xff;
    
    /* We have to break down the reads into 64kb pieces (ASPI restriction) */
    if( i_blocks > 32 )
    {
        ssc.SRB_BufLen = 32 * DVDCSS_BLOCK_SIZE;
        ssc.CDBByte[9] = 32;
        fd->i_blocks  += 32;

        /* Initiate transfer */  
        ResetEvent( hEvent );
        fd->lpSendCommand( (void*) &ssc );

        /* transfer the next 64kb (_win32_dvdcss_aread is called recursively)
         * We need to check the status of the read on return */
        if( _win32_dvdcss_aread( i_fd, (u8*) p_data + 32 * DVDCSS_BLOCK_SIZE,
                                 i_blocks - 32) < 0 )
        {
            return -1;
        }
    }
    else
    {
        /* This is the last transfer */
        ssc.SRB_BufLen   = i_blocks * DVDCSS_BLOCK_SIZE;
        ssc.CDBByte[9]   = (UCHAR) i_blocks;
        fd->i_blocks += i_blocks;

        /* Initiate transfer */  
        ResetEvent( hEvent );
        fd->lpSendCommand( (void*) &ssc );

    }

    /* If the command has still not been processed, wait until it's finished */
    if( ssc.SRB_Status == SS_PENDING )
    {
        WaitForSingleObject( hEvent, INFINITE );
    }
    CloseHandle( hEvent );

    /* check that the transfer went as planned */
    if( ssc.SRB_Status != SS_COMP )
    {
      return -1;
    }

    return i_blocks;
}

#endif

