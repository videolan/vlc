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

static inline void
vlc_ancillary_array_Init(vlc_ancillary_array *array)
{
    vlc_vector_init(array);
}

void
vlc_ancillary_array_Clear(vlc_ancillary_array *array);

int
vlc_ancillary_array_Dup(vlc_ancillary_array *dst_array,
                        const vlc_ancillary_array *src_array);

int
vlc_ancillary_array_Insert(vlc_ancillary_array *array,
                           struct vlc_ancillary *ancillary);

struct vlc_ancillary *
vlc_ancillary_array_Get(const vlc_ancillary_array *array,
                        vlc_ancillary_id id);

#endif /* VLC_ANCILLARY_INTERNAL_H */
