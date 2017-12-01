/*****************************************************************************
 * renderer_discoverer_internal.h:  libvlc external API
 *****************************************************************************
 * Copyright © 2016 VLC authors and VideoLAN
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

#ifndef _LIBVLC_RENDERER_DISCOVERER_INTERNAL_H
#define _LIBVLC_RENDERER_DISCOVERER_INTERNAL_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_renderer_discovery.h>

vlc_renderer_item_t *
libvlc_renderer_item_to_vlc( libvlc_renderer_item_t *p_item );

#endif
