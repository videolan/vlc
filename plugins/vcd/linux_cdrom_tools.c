/****************************************************************************
 * linux_cdrom_tools.c: linux cdrom tools
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 *
 * Author: Johan Bilien <jobi@via.ecp.fr>
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
 * Functions declared in this file use lots of ioctl calls, thus they are 
 * Linux-specific.                                           
 ****************************************************************************/

#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#if defined( WIN32 )
#   include <io.h>                                                 /* read() */
#else
#   include <sys/uio.h>                                      /* struct iovec */
#endif

#include <sys/ioctl.h>

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#if defined( WIN32 )
#   include "input_iovec.h"
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "debug.h"

#include "modules.h"
#include "modules_export.h"

#include "linux_cdrom_tools.h"

/*****************************************************************************
 * ioctl_ReadTocHeader: Read the TOC header and return the track number.
 *****************************************************************************/
int ioctl_GetTrackCount( int i_fd )
{
    struct cdrom_tochdr   tochdr;

    /* First we read the TOC header */
    if( ioctl( i_fd, CDROMREADTOCHDR, &tochdr ) == -1 )
    {
        intf_ErrMsg( "vcd error: could not read TOCHDR" );
        return -1;
    }

    return tochdr.cdth_trk1 - tochdr.cdth_trk0 + 1;
}

/*****************************************************************************
 * ioctl_GetSectors: Read the Table of Contents and fill p_vcd.
 *****************************************************************************/
int * ioctl_GetSectors( int i_fd )
{
    int  i, i_tracks;
    int *p_sectors;
    struct cdrom_tochdr   tochdr;
    struct cdrom_tocentry tocent;

    /* First we read the TOC header */
    if( ioctl( i_fd, CDROMREADTOCHDR, &tochdr ) == -1 )
    {
        intf_ErrMsg( "vcd error: could not read TOCHDR" );
        return NULL;
    }

    i_tracks = tochdr.cdth_trk1 - tochdr.cdth_trk0 + 1;

    p_sectors = malloc( (i_tracks + 1) * sizeof(int) );
    if( p_sectors == NULL )
    {
        intf_ErrMsg( "vcd error: could not allocate p_sectors" );
        return NULL;
    }

    /* Fill the p_sectors structure with the track/sector matches */
    for( i = 0 ; i <= i_tracks ; i++ )
    {
        tocent.cdte_format = CDROM_LBA;
        tocent.cdte_track =
            ( i == i_tracks ) ? CDROM_LEADOUT : tochdr.cdth_trk0 + i;

        if( ioctl( i_fd, CDROMREADTOCENTRY, &tocent ) == -1 )
        {
            intf_ErrMsg( "vcd error: could not read TOCENTRY" );
            free( p_sectors );
            return NULL;
        }

        p_sectors[ i ] = tocent.cdte_addr.lba;
    }

    return p_sectors;
}

/****************************************************************************
 * ioctl_ReadSector: Read a sector (2324 bytes)
 ****************************************************************************/
int ioctl_ReadSector( int i_fd, int i_sector, byte_t * p_buffer )
{
    byte_t p_block[ VCD_SECTOR_SIZE ];
    int    i_dummy = i_sector + 2 * CD_FRAMES;

#define p_msf ((struct cdrom_msf0 *)p_block)
    p_msf->minute =   i_dummy / (CD_FRAMES * CD_SECS);
    p_msf->second = ( i_dummy % (CD_FRAMES * CD_SECS) ) / CD_FRAMES;
    p_msf->frame =  ( i_dummy % (CD_FRAMES * CD_SECS) ) % CD_FRAMES;

    intf_DbgMsg( "vcd debug: playing frame %d:%d-%d",
                 p_msf->minute, p_msf->second, p_msf->frame);
#undef p_msf

    if( ioctl(i_fd, CDROMREADRAW, p_block) == -1 )
    {
        intf_ErrMsg( "vcd error: could not read block %i from disc",
                     i_sector );
        return -1;
    }

    /* We don't want to keep the header of the read sector */
    p_main->fast_memcpy( p_buffer, p_block + VCD_DATA_START, VCD_DATA_SIZE );

    return 0;
}
