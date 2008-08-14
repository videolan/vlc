/*****************************************************************************
 * vlc.h: global header for libvlc
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_VLC_H
#define VLC_VLC_H 1

/**
 * \file
 * This file defines libvlc new external API
 */

# ifdef __cplusplus
extern "C" {
# endif

#include <vlc/libvlc_structures.h>
#include <vlc/libvlc_events.h>
#include <vlc/libvlc.h>
#include <vlc/libvlc_media_list.h>
#include <vlc/libvlc_vlm.h>
#include <vlc/deprecated.h>

# ifdef __cplusplus
}
# endif

#endif /* _VLC_VLC_H */
