/*****************************************************************************
 * os_font.h: Wrapper for the OSFont class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: os_font.h,v 1.6 2003/04/28 14:12:32 asmax Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/


#if defined( WIN32 )
    #include "win32/win32_font.h"
    #define OSFont Win32Font

    #define VLC_FONT_ALIGN_LEFT    DT_LEFT
    #define VLC_FONT_ALIGN_CENTER  DT_CENTER
    #define VLC_FONT_ALIGN_RIGHT   DT_RIGHT

#elif defined GTK2_SKINS
    #include "gtk2/gtk2_font.h"
    #define OSFont GTK2Font

    #define VLC_FONT_ALIGN_LEFT    PANGO_ALIGN_LEFT
    #define VLC_FONT_ALIGN_CENTER  PANGO_ALIGN_CENTER
    #define VLC_FONT_ALIGN_RIGHT   PANGO_ALIGN_RIGHT

#elif defined X11_SKINS
    #include "x11/x11_font.h"
    #define OSFont X11Font

    #define VLC_FONT_ALIGN_LEFT    0
    #define VLC_FONT_ALIGN_CENTER  1
    #define VLC_FONT_ALIGN_RIGHT   2

#endif
