/****************************************************************************
 * cdrom.c: cdrom tools
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id$
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <string.h>
#include <errno.h>

#include "cdrom.h"

/*****************************************************************************
 * Local Prototypes
 *****************************************************************************/
static void cd_log_handler (cdio_log_level_t level, const char message[]);

/*****************************************************************************
 * ioctl_Open: Opens a VCD device or file and returns an opaque handle
 *****************************************************************************/
cddev_t *ioctl_Open( vlc_object_t *p_this, const char *psz_dev )
{
    cddev_t *p_cddev;

    if( !psz_dev ) return NULL;

    /*
     *  Initialize structure with default values
     */
    p_cddev = (cddev_t *)malloc( sizeof(cddev_t) );
    if( p_cddev == NULL )
    {
        msg_Err( p_this, "out of memory" );
        return NULL;
    }

    /* Set where to log errors messages from libcdio. */
    cdio_log_set_handler ( cd_log_handler );

    p_cddev->cdio = cdio_open(psz_dev, DRIVER_UNKNOWN);

    if( p_cddev->cdio == NULL )
    {
        free( p_cddev );
        p_cddev = NULL;
    }

    return p_cddev;
}

/*****************************************************************************
 * ioctl_Close: Closes an already opened VCD device or file.
 *****************************************************************************/
void ioctl_Close( cddev_t *p_cddev )
{
    cdio_destroy(p_cddev->cdio);
}

/*****************************************************************************
 * ioctl_GetTracksMap: Read the Table of Contents, fill in the pp_sectors map
 *                     if pp_sectors is not null and return the number of
 *                     tracks available.
 *                     We allocate and fill one more track than are on 
 *                     the CD. The last "track" is leadout track information.
 *                     This makes finding the end of the last track uniform
 *                     how it is done for other tracks.
 *****************************************************************************/
track_t ioctl_GetTracksMap( vlc_object_t *p_this, const CdIo *cdio,
                            lsn_t **pp_sectors )
{
    track_t i_tracks     = cdio_get_num_tracks(cdio);
    track_t first_track  = cdio_get_first_track_num(cdio);
    track_t i;


    *pp_sectors = malloc( (i_tracks + 1) * sizeof(lsn_t) );
    if( *pp_sectors == NULL )
      {
        msg_Err( p_this, "out of memory" );
        return 0;
      }

    /* Fill the p_sectors structure with the track/sector matches.
       Note cdio_get_track_lsn when given num_tracks + 1 will return
       the leadout LSN.
     */
    for( i = 0 ; i <= i_tracks ; i++ )
      {
        (*pp_sectors)[ i ] = cdio_get_track_lsn(cdio, first_track+i);
      }
    
    return i_tracks;
}

/****************************************************************************
 * ioctl_ReadSector: Read a sector (2324 bytes)
 ****************************************************************************/
int ioctl_ReadSector( vlc_object_t *p_this, const cddev_t *p_cddev,
                      int i_sector, byte_t * p_buffer )
{
  typedef struct {
    uint8_t subheader   [8];
    uint8_t data        [M2F2_SECTOR_SIZE];
  } vcdsector_t;
  vcdsector_t vcd_sector;
  
  if( cdio_read_mode2_sector(p_cddev->cdio, &vcd_sector, i_sector, VLC_TRUE) 
      != 0)
  {
      // msg_Err( p_this, "Could not read sector %d", i_sector );
      return -1;
  }
    
  memcpy (p_buffer, vcd_sector.data, M2F2_SECTOR_SIZE);
  
  return( 0 );
}

/****************************************************************************
 * Private functions
 ****************************************************************************/

/* For now we're going to just discard error messages from libcdio... */
static void
cd_log_handler (cdio_log_level_t level, const char message[])
{
  return;
}
