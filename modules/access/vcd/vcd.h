/*****************************************************************************
 * vcd.h: thread structure of the VCD plugin
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: vcd.h,v 1.2 2002/10/15 19:56:59 gbazin Exp $
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

/* where the data start on a VCD sector */
#define VCD_DATA_START 24
/* size of the availablr data on a VCD sector */
#define VCD_DATA_SIZE 2324
/* size of a VCD sector, header and tail included */
#define VCD_SECTOR_SIZE 2352
/* size of a CD sector */
#define CD_SECTOR_SIZE 2048

#ifndef VCDDEV_T
typedef struct vcddev_s vcddev_t;
#endif

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
vcddev_t *ioctl_Open         ( vlc_object_t *, const char * );
void      ioctl_Close        ( vlc_object_t *, vcddev_t * );
int       ioctl_GetTracksMap ( vlc_object_t *, const vcddev_t *, int ** );
int       ioctl_ReadSector   ( vlc_object_t *, const vcddev_t *,
			       int, byte_t * );
