/*****************************************************************************
 * access.h : VCD access.c routine headers
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef VCD_ACCESS_H
#define VCD_ACCESS_H

void VCDSetOrigin( access_t *p_access, lsn_t i_lsn, track_t i_track,
                   const vcdinfo_itemid_t *p_itemid );

int  VCDOpen       ( vlc_object_t * );
void VCDClose      ( vlc_object_t * );


#endif /* VCD_ACCESS_H */

