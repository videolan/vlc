/*****************************************************************************
 * osd.h : Constants for use with osd modules
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: osd.h,v 1.2 2003/07/14 21:32:58 sigmunau Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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

#define OSD_ALIGN_LEFT 0
#define OSD_ALIGN_RIGHT 0x1
#define OSD_ALIGN_TOP 0
#define OSD_ALIGN_BOTTOM 0x2
struct text_style_t
{
    int i_size;
    uint32_t i_color;
    vlc_bool_t b_italic;
    vlc_bool_t b_bold;
    vlc_bool_t b_underline;
};
static const text_style_t default_text_style = { 22, 0xffffff, VLC_FALSE, VLC_FALSE, VLC_FALSE };

VLC_EXPORT( void, vout_ShowTextRelative, ( vout_thread_t *, char *, text_style_t *, int, int, int, mtime_t ) );
VLC_EXPORT( void,  vout_ShowTextAbsolute, ( vout_thread_t *, char *, text_style_t *, int, int, int, mtime_t, mtime_t ) );
