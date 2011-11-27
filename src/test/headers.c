/*****************************************************************************
 * headers.c: Test for public headers usability
 *****************************************************************************
 * Copyright (C) 2007 Rémi Denis-Courmont
 * $Id$
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

/* config.h is NOT installed, headers MUST NOT depend on it.
 # include <config.h> */
/* One thing we don't check is the CPPFLAGS - these MUST be exposed publicly,
 * e.g. using pkg-config. */

#define PACKAGE "vlc"

/* Because we are from src/ __LIBVLC__ is defined, but we don't want that,
 * as we act here as a third-party program just linking to libvlc */
#ifdef __LIBVLC__
# undef __LIBVLC__
#endif

#include <vlc/vlc.h>
#include <vlc/deprecated.h>
#include <vlc/libvlc.h>
#include <vlc/libvlc_structures.h>
#include <vlc/libvlc_events.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_discoverer.h>
#include <vlc/libvlc_media_library.h>
#include <vlc/libvlc_media_list.h>
#include <vlc/libvlc_media_list_player.h>
#include <vlc/libvlc_media_player.h>
#include <vlc/libvlc_vlm.h>

#include <stdio.h>

int main (void)
{
    puts ("Public headers can be used for external compilation.");
    return 0;
}

