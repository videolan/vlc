/*****************************************************************************
 * ancillary.h: helpers to manage ancillaries from a frame or a picture
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef VLC_ANCILLARY_INTERNAL_H
#define VLC_ANCILLARY_INTERNAL_H 1

#include <vlc_ancillary.h>

/*
 * A NULL terminated array of struct vlc_ancillary *. We don't use a
 * vlc_vector here in orer to gain few bytes (2 * size_t) for each
 * ancillary users (each vlc_frame_t/picture_t). Users will likely have one
 * or zero ancillary so the optimisations of the vlc_vector are not
 * important here.
 */

static inline void
vlc_ancillary_array_Init(struct vlc_ancillary ***array)
{
    *array = NULL;
}

void
vlc_ancillary_array_Clear(struct vlc_ancillary ***array);

int
vlc_ancillary_array_Dup(struct vlc_ancillary ***dst_array,
                        struct vlc_ancillary ** const*src_array);

int
vlc_ancillary_array_Insert(struct vlc_ancillary ***array,
                           struct vlc_ancillary *ancillary);

struct vlc_ancillary *
vlc_ancillary_array_Get(struct vlc_ancillary ** const*array,
                        vlc_ancillary_id id);

#endif /* VLC_ANCILLARY_INTERNAL_H */
