/****************************************************************************
 * cdrom.h: cdrom tools header
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: cdrom.h,v 1.2 2003/11/26 03:34:22 rocky Exp $
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

#ifndef VCDX_CDROM_H
#define VCDX_CDROM_H

#include <cdio/cdio.h>
#include <cdio/logging.h>

/*****************************************************************************
 * The cddev structure
 *****************************************************************************/
typedef struct cddev_s
{
    int    *p_sectors;                           /* tracks layout on the vcd */
    CdIo  *cdio;                                /* libcdio uses this to read */

} cddev_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
cddev_t  *ioctl_Open         ( vlc_object_t *, const char * );
void      ioctl_Close        ( cddev_t * );
track_t   ioctl_GetTracksMap ( vlc_object_t *, const CdIo *, lsn_t ** );
int       ioctl_ReadSector   ( vlc_object_t *, const cddev_t *,
                               int, byte_t * );
#endif /*VCDX_CDROM_H*/
