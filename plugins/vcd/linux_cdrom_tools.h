/****************************************************************************
 * linux_cdrom_tools.h: linux cdrom tools header
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

#if defined(HAVE_BSD_DVD_STRUCT) || defined(DVD_STRUCT_IN_BSDI_DVDIOCTL_DVD_H) || defined(DVD_STRUCT_IN_DVD_H)
#   include <dvd.h>
#else
#   include <linux/cdrom.h>
#endif

/* where the data start on a VCD sector */
#define VCD_DATA_START 24
/* size of the availablr data on a VCD sector */
#define VCD_DATA_SIZE 2324
/* size of a VCD sector, header and tail included */
#define VCD_SECTOR_SIZE 2352

/******************************************************************************
* Prototypes                                                                  *
******************************************************************************/
int ioctl_GetTrackCount ( int );
int * ioctl_GetSectors  ( int );
int ioctl_ReadSector    ( int, int, byte_t * );

