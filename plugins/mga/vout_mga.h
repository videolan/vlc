/*****************************************************************************
 * vout_mga.h: MGA video output display method headers
 *****************************************************************************
 * Copyright (C) 1999 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 * Copyright (C) 2000, 2001 VideoLAN
 *
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
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

#ifndef __LINUX_MGAVID_H
#define __LINUX_MGAVID_H

typedef struct mga_vid_config_s
{
    u32     card_type;
    u32     ram_size;
    u32     src_width;
    u32     src_height;
    u32     dest_width;
    u32     dest_height;
    u32     x_org;
    u32     y_org;
    u8      colkey_on;
    u8      colkey_red;
    u8      colkey_green;
    u8      colkey_blue;
} mga_vid_config_t;

#define MGA_VID_CONFIG _IOR('J', 1, mga_vid_config_t)
#define MGA_VID_ON     _IO ('J', 2)
#define MGA_VID_OFF    _IO ('J', 3)

#define MGA_G200 0x1234
#define MGA_G400 0x5678

#endif

