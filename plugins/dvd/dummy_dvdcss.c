/*****************************************************************************
 * dummy_dvdcss.c: Dummy libdvdcss with minimal DVD access.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: dummy_dvdcss.c,v 1.1 2001/08/06 13:28:00 sam Exp $
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
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>                                        /* struct iovec */
#include <sys/ioctl.h>

#ifdef DVD_STRUCT_IN_LINUX_CDROM_H
#   include <netinet/in.h>
#   include <linux/cdrom.h>
#else
#   error "building dummy libdvdcss on this system does not make sense !"
#endif

#include "config.h"
#include "common.h"

#include "dummy_dvdcss.h"

/*****************************************************************************
 * Local structure
 *****************************************************************************/
struct dvdcss_s
{
    /* File descriptor */
    int i_fd;
};

/*****************************************************************************
 * dvdcss_open: initialize library, open a DVD device, crack CSS key
 *****************************************************************************/
extern dvdcss_handle dummy_dvdcss_open ( char *psz_target, int i_flags )
{
    dvdcss_handle dvdcss;
    dvd_struct    dvd;

    /* Allocate the library structure */
    dvdcss = malloc( sizeof( struct dvdcss_s ) );
    if( dvdcss == NULL )
    {
        fprintf( stderr, "dvd error: "
                         "dummy libdvdcss could not allocate memory\n" );
        return NULL;
    }

    /* Open the device */
    dvdcss->i_fd = open( psz_target, 0 );
    if( dvdcss->i_fd < 0 )
    {
        fprintf( stderr, "dvd error: "
                         "dummy libdvdcss could not open device\n" );
        free( dvdcss );
        return NULL;
    }

    /* Check for encryption or ioctl failure */
    dvd.type = DVD_STRUCT_COPYRIGHT;
    dvd.copyright.layer_num = 0;
    if( ioctl( dvdcss->i_fd, DVD_READ_STRUCT, &dvd ) != 0
         || dvd.copyright.cpst )
    {
        fprintf( stderr, "dvd error: "
                         "dummy libdvdcss could not decrypt disc\n" );
        close( dvdcss->i_fd );
        free( dvdcss );
        return NULL;
    }

    return dvdcss;
}

/*****************************************************************************
 * dvdcss_error: return the last libdvdcss error message
 *****************************************************************************/
extern char * dummy_dvdcss_error ( dvdcss_handle dvdcss )
{
    return "unknown error";
}

/*****************************************************************************
 * dvdcss_seek: seek into the device
 *****************************************************************************/
extern int dummy_dvdcss_seek ( dvdcss_handle dvdcss, int i_blocks )
{
    off_t i_read;

    i_read = lseek( dvdcss->i_fd,
                    (off_t)i_blocks * (off_t)DVDCSS_BLOCK_SIZE, SEEK_SET );

    return i_read / DVDCSS_BLOCK_SIZE;
}

/*****************************************************************************
 * dvdcss_title: crack the current title key if needed
 *****************************************************************************/
extern int dummy_dvdcss_title ( dvdcss_handle dvdcss, int i_block )
{
    return 0;
}

/*****************************************************************************
 * dvdcss_read: read data from the device, decrypt if requested
 *****************************************************************************/
extern int dummy_dvdcss_read ( dvdcss_handle dvdcss, void *p_buffer,
                                                     int i_blocks,
                                                     int i_flags )
{
    int i_bytes;

    i_bytes = read( dvdcss->i_fd, p_buffer,
                    (size_t)i_blocks * DVDCSS_BLOCK_SIZE );

    return i_bytes / DVDCSS_BLOCK_SIZE;
}

/*****************************************************************************
 * dvdcss_readv: read data to an iovec structure, decrypt if reaquested
 *****************************************************************************/
extern int dummy_dvdcss_readv ( dvdcss_handle dvdcss, void *p_iovec,
                                                      int i_blocks,
                                                      int i_flags )
{
    int i_read;

    i_read = readv( dvdcss->i_fd, (struct iovec*)p_iovec, i_blocks );

    return i_read / DVDCSS_BLOCK_SIZE;
}

/*****************************************************************************
 * dvdcss_close: close the DVD device and clean up the library
 *****************************************************************************/
extern int dummy_dvdcss_close ( dvdcss_handle dvdcss )
{
    int i_ret;

    i_ret = close( dvdcss->i_fd );

    if( i_ret < 0 )
    {
        return i_ret;
    }

    free( dvdcss );

    return 0;
}

