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

#define MODULE_NAME vcd
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
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

#include "input_vcd.h"
#include "linux_cdrom_tools.h"

/*****************************************************************************
 * VCDReadToc: Read the Table of Contents and fill p_vcd.
 *****************************************************************************/
int VCDReadToc( thread_vcd_data_t * p_vcd )
{
    int i;
    struct cdrom_tochdr   tochdr;
    struct cdrom_tocentry tocent;

    /* First we read the TOC header */
    if( ioctl( p_vcd->i_handle, CDROMREADTOCHDR, &tochdr ) == -1 )
    {
        intf_ErrMsg( "vcd error: could not read TOCHDR" );
        return -1;
    }

    p_vcd->nb_tracks = tochdr.cdth_trk1 - tochdr.cdth_trk0 + 1;

    /* nb_tracks + 1 because we put the lead_out tracks for computing last
     * track's size */
    p_vcd->p_sectors = malloc( ( p_vcd->nb_tracks + 1 ) * sizeof( int ) );
    if ( p_vcd->p_sectors == NULL )
    {
        intf_ErrMsg( "vcd error: could not allocate p_sectors" );
        return -1;
    }

    /* Fill the p_sectors structure with the track/sector matches */
    for( i = 0 ; i <= p_vcd->nb_tracks ; i++ )
    {
        tocent.cdte_format = CDROM_LBA;
        tocent.cdte_track =
            ( i == p_vcd->nb_tracks ) ? CDROM_LEADOUT : tochdr.cdth_trk0 + i;

        if( ioctl( p_vcd->i_handle, CDROMREADTOCENTRY, &tocent ) == -1 )
        {
            intf_ErrMsg( "vcd error: could not read TOCENTRY" );
            free ( p_vcd->p_sectors );
            return -1;
        }

        p_vcd->p_sectors[ i ] = tocent.cdte_addr.lba;
    }

    return 1;
}

/****************************************************************************
 * VCDReadSector: Read a sector (2324 bytes)
 ****************************************************************************/
int VCDReadSector( struct thread_vcd_data_s * p_vcd, byte_t * p_buffer )
{
    byte_t p_block[ VCD_SECTOR_SIZE ];
    int    i_dummy = p_vcd->i_sector + 2 * CD_FRAMES;

#define p_msf ((struct cdrom_msf0 *)p_block)
    p_msf->minute =   i_dummy / (CD_FRAMES * CD_SECS);
    p_msf->second = ( i_dummy % (CD_FRAMES * CD_SECS) ) / CD_FRAMES;
    p_msf->frame =  ( i_dummy % (CD_FRAMES * CD_SECS) ) % CD_FRAMES;
#undef p_msf

#ifdef DEBUG
    intf_DbgMsg( "vcd debug: playing frame %d:%d-%d",
                 p_msf->minute, p_msf->second, p_msf->frame);
#endif

    if( ioctl(p_vcd->i_handle, CDROMREADRAW, p_block) == -1 )
    {
        intf_ErrMsg( "vcd error: could not read block from disc" );
        return -1;
    }

    /* We don't want to keep the header of the read sector */
    p_main->fast_memcpy( p_buffer, p_block + VCD_DATA_START, VCD_DATA_SIZE );

    p_vcd->i_sector++;

    if( p_vcd->i_sector >= p_vcd->p_sectors[p_vcd->i_track + 1] )
    {
        p_vcd->b_end_of_track = 1;
    }

    return 1;
}

