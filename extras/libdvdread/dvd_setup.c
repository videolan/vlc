/**
 * dvd_setup.c: setup read functions with either libdvdcss
 * or minimal DVD access.
 */

/**
 * Copyright (C) 2001 VideoLAN
 * $Id: dvd_setup.c,v 1.2 2002/01/23 03:56:51 stef Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 */

/**
 * Preamble
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/uio.h>                                        /* struct iovec */

#include "dvd_reader.h"
#include "dvd_setup.h"

#ifdef GOD_DAMN_DMCA
/**
 * dvd_open: initialize library, open a DVD device.
 */
static dvd_handle dvd_open ( char *psz_target )
{
    dvd_handle dev;

    /* Allocate the library structure */
    dev = malloc( sizeof( dvd_handle ) );
    if( dev == NULL )
    {
        fprintf( stderr, "libdvdread: Could not allocate memory.\n" );
        return NULL;
    }

    /* Open the device */
    dev->i_fd = open( psz_target, 0 );
    if( dev->i_fd < 0 )
    {
        fprintf( stderr, "libdvdread: Could not open device.\n" );
        free( dev );
        return NULL;
    }

    return dev;
}

/**
 * dvd_error: return the last error message
 */
static char * dvd_error ( dvd_handle dev )
{
    return "unknown error";
}

/**
 * dvd_seek: seek into the device.
 */
static int dvd_seek ( dvd_handle dev, int i_blocks, int i_flags )
{
    off_t i_read;

    i_read = lseek( dev->i_fd,
                    (off_t)i_blocks * (off_t)DVD_VIDEO_LB_LEN, SEEK_SET );

    return i_read / DVD_VIDEO_LB_LEN;
}

/**
 * dvd_read: read data from the device.
 */
static int dvd_read ( dvd_handle dev, void *p_buffer,
                      int i_blocks,
                      int i_flags )
{
    int i_bytes;

    i_bytes = read( dev->i_fd, p_buffer,
                    (size_t)i_blocks * DVD_VIDEO_LB_LEN );

    return i_bytes / DVD_VIDEO_LB_LEN;
}

/**
 * dvd_readv: read data to an iovec structure.
 */
static int dvd_readv ( dvd_handle dev, void *p_iovec,
                       int i_blocks,
                       int i_flags )
{
    int i_read;

    i_read = readv( dev->i_fd, (struct iovec*)p_iovec, i_blocks );

    return i_read / DVD_VIDEO_LB_LEN;
}

/**
 * dvd_close: close the DVD device and clean up the library.
 */
static int dvd_close ( dvd_handle dev )
{
    int i_ret;

    i_ret = close( dev->i_fd );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    free( dev );

    return 0;
}

void DVDSetupRead( void )
{
    void * dvdcss_library = NULL;

    if( ( dvdcss_library = dlopen( "libdvdcss.so.1", RTLD_LAZY ) ) )
    {
        pf_dvd_open = dlsym( dvdcss_library, "dvdcss_open" );
        pf_dvd_close = dlsym( dvdcss_library, "dvdcss_close" );
        pf_dvd_seek = dlsym( dvdcss_library, "dvdcss_seek" );
        pf_dvd_read = dlsym( dvdcss_library, "dvdcss_read" );
        pf_dvd_readv = dlsym( dvdcss_library, "dvdcss_readv" );
        pf_dvd_error = dlsym( dvdcss_library, "dvdcss_error" );

        if( pf_dvd_open == NULL || pf_dvd_close == NULL
             || pf_dvd_seek == NULL || pf_dvd_read == NULL
             || pf_dvd_readv == NULL || pf_dvd_error == NULL )
        {
            fprintf( stderr,  "libdvdread: Missing symbols in libdvdcss.so.1, "
                              "this shouldn't happen !" );
            dlclose( dvdcss_library );
            dvdcss_library = NULL;
        }
        else
        {
            printf( "libdvdread: Using libdvdcss.so.1 for DVD access\n" );
        }
    }
    else
    {
        fprintf( stderr, "libdvdread: Can't open libdvdcss.so.1: %s.\n",
                 dlerror() );
    }
    
    if( !dvdcss_library )
    {
        /* Replacement functions */
        pf_dvd_open = dvd_open;
        pf_dvd_close = dvd_close;
        pf_dvd_seek = dvd_seek;
        pf_dvd_read = dvd_read;
        pf_dvd_readv = dvd_readv;
        pf_dvd_error = dvd_error;
    }
}
#else
void DVDSetupRead( void )
{   
    pf_dvd_open = dvdcss_open;
    pf_dvd_close = dvdcss_close;
    pf_dvd_seek = dvdcss_seek;
    pf_dvd_read = dvdcss_read;
    pf_dvd_readv = dvdcss_readv;
    pf_dvd_error = dvdcss_error;
}
#endif
