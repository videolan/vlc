/*****************************************************************************
 * libvlc_events.h:  libvlc_events external API structure
 *****************************************************************************
 * Copyright (C) 1998-2010 VLC authors and VideoLAN
 *
 * Authors: Filippo Carone <littlejohn@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

#ifndef LIBVLC_EVENTS_H
#define LIBVLC_EVENTS_H 1

# include <vlc/libvlc.h>
# include <vlc/libvlc_picture.h>
# include <vlc/libvlc_media_track.h>
# include <vlc/libvlc_media.h>

/**
 * \file
 * This file defines libvlc_event external API
 */

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

typedef struct libvlc_renderer_item_t libvlc_renderer_item_t;
typedef struct libvlc_title_description_t libvlc_title_description_t;
typedef struct libvlc_picture_t libvlc_picture_t;
typedef struct libvlc_picture_list_t libvlc_picture_list_t;
typedef struct libvlc_media_t libvlc_media_t;
typedef struct libvlc_media_list_t libvlc_media_list_t;

/**
 * \ingroup libvlc_event
 * @{
 */

/**
 * A LibVLC event
 */
typedef struct libvlc_event_t
{
    int   type; /**< Event type (see @ref libvlc_event_e) */
    void *p_obj; /**< Object emitting the event */
} libvlc_event_t;


/**@} */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_EVENTS_H */
