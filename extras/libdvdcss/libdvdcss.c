/*****************************************************************************
 * libdvdcss.c: DVD reading library.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: libdvdcss.c,v 1.1 2001/06/12 22:14:44 sam Exp $
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
#include <unistd.h>

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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int _dvdcss_open  ( dvdcss_handle, char *psz_target );
static int _dvdcss_close ( dvdcss_handle );
static int _dvdcss_seek  ( dvdcss_handle, int i_blocks );
static int _dvdcss_read  ( dvdcss_handle, void *p_buffer, int i_blocks );
static int _dvdcss_readv ( dvdcss_handle, struct iovec *p_iovec, int i_blocks );

/*****************************************************************************
 * dvdcss_init: initialize libdvdcss
 *****************************************************************************/
extern dvdcss_handle dvdcss_init ( int i_flags )
{
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
    dvdcss->i_status = DVDCSS_STATUS_NONE;

    dvdcss->b_debug = i_flags & DVDCSS_INIT_DEBUG;
    dvdcss->b_errors = !(i_flags & DVDCSS_INIT_QUIET);
    dvdcss->psz_error = "no error";

    /* XXX: additional initialization stuff might come here */

    dvdcss->i_status |= DVDCSS_STATUS_INIT;

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
 * dvdcss_open: open a DVD device, crack CSS key if disc is encrypted
 *****************************************************************************/
extern int dvdcss_open ( dvdcss_handle dvdcss, char *psz_target )
{
    int i_ret;

    if( ! (dvdcss->i_status & DVDCSS_STATUS_INIT) )
    {
        _dvdcss_error( dvdcss, "library not initialized" );
        return -1;
    }

    if( dvdcss->i_status & DVDCSS_STATUS_OPEN )
    {
        _dvdcss_error( dvdcss, "a device is already opened" );
        return -1;
    }

    i_ret = _dvdcss_open( dvdcss, psz_target );
    if( i_ret < 0 )
    {
        return i_ret;
    }

    i_ret = CSSTest( dvdcss );
    if( i_ret < 0 )
    {
        _dvdcss_error( dvdcss, "css test failed" );
        _dvdcss_close( dvdcss );
        return i_ret;
    }

    dvdcss->b_encrypted = i_ret;

    /* If drive is encrypted, crack its key */
    if( dvdcss->b_encrypted )
    {
        i_ret = CSSInit( dvdcss );

        if( i_ret < 0 )
        {
            _dvdcss_close( dvdcss );
            return i_ret;
        }
    }

    dvdcss->i_status |= DVDCSS_STATUS_OPEN;

    return 0;
}

/*****************************************************************************
 * dvdcss_seek: seek into the device
 *****************************************************************************/
extern int dvdcss_seek ( dvdcss_handle dvdcss, int i_blocks )
{
    if( ! (dvdcss->i_status & DVDCSS_STATUS_OPEN) )
    {
        _dvdcss_error( dvdcss, "no device opened" );
        return -1;
    }

    return _dvdcss_seek( dvdcss, i_blocks );
}

/*****************************************************************************
 * dvdcss_crack: crack the current title key
 *****************************************************************************/
extern int dvdcss_crack ( dvdcss_handle dvdcss, int i_title, int i_block )
{
    int i_ret;

    if( ! (dvdcss->i_status & DVDCSS_STATUS_OPEN) )
    {
        _dvdcss_error( dvdcss, "no device opened" );
        return -1;
    }

    if( ! dvdcss->b_encrypted )
    {
        return 0;
    }

    /* Crack CSS title key for current VTS */
    dvdcss->css.i_title = i_title;
    dvdcss->css.i_title_pos = i_block;

    i_ret = CSSGetKey( dvdcss );

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

    return 0;
}

/*****************************************************************************
 * dvdcss_read: read data from the device, decrypt if requested
 *****************************************************************************/
extern int dvdcss_read ( dvdcss_handle dvdcss, void *p_buffer,
                                               int i_blocks,
                                               int i_flags )
{
    int i_ret;

    if( ! (dvdcss->i_status & DVDCSS_STATUS_OPEN) )
    {
        _dvdcss_error( dvdcss, "no device opened" );
        return -1;
    }

    i_ret = _dvdcss_read( dvdcss, p_buffer, i_blocks );

    if( i_ret != i_blocks
         || !dvdcss->b_encrypted
         || !(i_flags & DVDCSS_READ_DECRYPT) )
    {
        return i_ret;
    }

    while( i_ret )
    {
        CSSDescrambleSector( dvdcss->css.pi_title_key, p_buffer );
        ((u8*)p_buffer)[0x14] &= 0x8f;
        (u8*)p_buffer += DVDCSS_BLOCK_SIZE;
        i_ret--;
    }

    return i_blocks;
}

/*****************************************************************************
 * dvdcss_readv: read data to an iovec structure, decrypt if reaquested
 *****************************************************************************/
extern int dvdcss_readv ( dvdcss_handle dvdcss, void *p_iovec,
                                                int i_blocks,
                                                int i_flags )
{
#define P_IOVEC ((struct iovec*)p_iovec)
    int i_ret;
    void *iov_base;
    size_t iov_len;

    if( ! (dvdcss->i_status & DVDCSS_STATUS_OPEN) )
    {
        _dvdcss_error( dvdcss, "no device opened" );
        return -1;
    }

    i_ret = _dvdcss_readv( dvdcss, P_IOVEC, i_blocks );

    if( i_ret != i_blocks
         || !dvdcss->b_encrypted
         || !(i_flags & DVDCSS_READ_DECRYPT) )
    {
        return i_ret;
    }

    /* Initialize loop for decryption */
    iov_base = P_IOVEC->iov_base;
    iov_len = P_IOVEC->iov_len;

    while( i_ret )
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

        CSSDescrambleSector( dvdcss->css.pi_title_key, iov_base );
        ((u8*)iov_base)[0x14] &= 0x8f;

        (u8*)iov_base += DVDCSS_BLOCK_SIZE;
        (u8*)iov_len -= DVDCSS_BLOCK_SIZE;

        i_ret--;
    }

    return i_blocks;
#undef P_IOVEC
}

/*****************************************************************************
 * dvdcss_close: close the DVD device
 *****************************************************************************/
extern int dvdcss_close ( dvdcss_handle dvdcss )
{
    int i_ret;

    if( ! (dvdcss->i_status & DVDCSS_STATUS_OPEN) )
    {
        _dvdcss_error( dvdcss, "no device opened" );
        return -1;
    }

    i_ret = _dvdcss_close( dvdcss );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    dvdcss->i_status &= ~DVDCSS_STATUS_OPEN;

    return 0;
}

/*****************************************************************************
 * dvdcss_end: clean up the library
 *****************************************************************************/
extern int dvdcss_end ( dvdcss_handle dvdcss )
{
    if( dvdcss->i_status & DVDCSS_STATUS_OPEN )
    {
        _dvdcss_error( dvdcss, "a device is still open" );
        return -1;
    }

    free( dvdcss );

    return 0;
}

/* Following functions are local */

static int _dvdcss_open ( dvdcss_handle dvdcss, char *psz_target )
{
#if defined( WIN32 )
    snprintf( buf, 7, "\\\\.\\%c:", psz_target[0] );
    (HANDLE) dvdcss->i_fd =
            CreateFile( psz_target, GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING, 0, NULL );
    if( (HANDLE) dvdcss->i_fd == INVALID_HANDLE_VALUE )
    {
        _dvdcss_error( dvdcss, "failed opening device" );
        return -1;
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
    CloseHandle( (HANDLE) dvdcss->i_fd );
#else
    close( dvdcss->i_fd );
#endif

    return 0;
}

static int _dvdcss_seek ( dvdcss_handle dvdcss, int i_blocks )
{
    off_t i_read;

#if defined( WIN32 )
    i_read = SetFilePointer( (HANDLE) dvdcss->i_fd,
                             (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE,
                             NULL, FILE_BEGIN );
#else
    i_read = lseek( dvdcss->i_fd,
                    (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE, SEEK_SET );

#endif

    return i_read / DVDCSS_BLOCK_SIZE;
}

static int _dvdcss_read ( dvdcss_handle dvdcss, void *p_buffer, int i_blocks )
{
#if defined( WIN32 )
    DWORD i_read;
    if( ReadFile( (HANDLE) dvdcss->i_fd, p_buffer,
                  (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE,
                  &i_read, NULL ) == -1 )
    {
        return 0;
    }

#else
    off_t i_read;
    i_read = read( dvdcss->i_fd, p_buffer,
                   (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE );

#endif

    return i_read / DVDCSS_BLOCK_SIZE;
}

static int _dvdcss_readv ( dvdcss_handle dvdcss, struct iovec *p_iovec, int i_blocks )
{
    off_t i_read;
    i_read = readv( dvdcss->i_fd, p_iovec, i_blocks );

    return i_read / DVDCSS_BLOCK_SIZE;
}

