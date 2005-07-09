/*****************************************************************************
 * osd.h : Constants for use with osd modules
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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

/**
 * \file
 * Stucts and function prototypes to place text on the video
 */

#define OSD_ALIGN_LEFT 0x1
#define OSD_ALIGN_RIGHT 0x2
#define OSD_ALIGN_TOP 0x4
#define OSD_ALIGN_BOTTOM 0x8

#define OSD_HOR_SLIDER 1
#define OSD_VERT_SLIDER 2

#define OSD_PLAY_ICON 1
#define OSD_PAUSE_ICON 2
#define OSD_SPEAKER_ICON 3
#define OSD_MUTE_ICON 4

/**
 * Text style information.
 * This struct is currently ignored
 */
struct text_style_t
{
    int i_size;
    uint32_t i_color;
    vlc_bool_t b_italic;
    vlc_bool_t b_bold;
    vlc_bool_t b_underline;
};
static const text_style_t default_text_style = { 22, 0xffffff, VLC_FALSE, VLC_FALSE, VLC_FALSE };

VLC_EXPORT( int, vout_ShowTextRelative, ( vout_thread_t *, int, char *, text_style_t *, int, int, int, mtime_t ) );
VLC_EXPORT( int, vout_ShowTextAbsolute, ( vout_thread_t *, int, char *, text_style_t *, int, int, int, mtime_t, mtime_t ) );
VLC_EXPORT( void,  __vout_OSDMessage, ( vlc_object_t *, int, char *, ... ) );
/**
 * Same as __vlc_OSDMessage() but with automatic casting
 */
#if defined(HAVE_VARIADIC_MACROS)
#    define vout_OSDMessage( obj, chan, fmt, args...) __vout_OSDMessage( VLC_OBJECT(obj), chan, fmt, ## args )
#else
#    define vout_OSDMessage __vout_OSDMessage
#endif
VLC_EXPORT( void, vout_OSDSlider, ( vlc_object_t *, int, int , short ) );
VLC_EXPORT( void, vout_OSDIcon, ( vlc_object_t *, int, short ) );
