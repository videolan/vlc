/****************************************************************************
 * cdrom.h: cdrom tools header
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: cdrom.h,v 1.1 2002/08/04 17:23:42 sam Exp $
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

/******************************************************************************
* Prototypes                                                                  *
******************************************************************************/
int ioctl_GetTrackCount ( int, const char *psz_dev );
int * ioctl_GetSectors  ( int, const char *psz_dev );
int ioctl_ReadSector    ( int, int, byte_t * );

