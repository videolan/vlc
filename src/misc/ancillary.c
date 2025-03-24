/*****************************************************************************
 * ancillary.c: ancillary management functions
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_atomic.h>

#include <vlc_ancillary.h>

struct vlc_ancillary
{
    vlc_atomic_rc_t rc;

    vlc_ancillary_id id;
    void *data;
    vlc_ancillary_free_cb free_cb;
};

struct vlc_ancillary *
vlc_ancillary_CreateWithFreeCb(void *data,
                               vlc_ancillary_id id,
                               vlc_ancillary_free_cb free_cb)
{
    struct vlc_ancillary *ancillary = malloc(sizeof(*ancillary));

    if (ancillary == NULL)
        return NULL;

    vlc_atomic_rc_init(&ancillary->rc);
    ancillary->id = id;
    ancillary->data = data;
    ancillary->free_cb = free_cb;

    return ancillary;
}

void
vlc_ancillary_Release(struct vlc_ancillary *ancillary)
{
    if (vlc_atomic_rc_dec(&ancillary->rc))
    {
        if (ancillary->free_cb != NULL)
            ancillary->free_cb(ancillary->data);
        free(ancillary);
    }
}

struct vlc_ancillary *
vlc_ancillary_Hold(struct vlc_ancillary *ancillary)
{
    vlc_atomic_rc_inc(&ancillary->rc);
    return ancillary;
}

void *
vlc_ancillary_GetData(const struct vlc_ancillary *ancillary)
{
    return ancillary->data;
}

void
vlc_ancillary_array_Clear(vlc_ancillary_array *array)
{
    struct vlc_ancillary *ancillary;
    vlc_vector_foreach(ancillary, array)
        vlc_ancillary_Release(ancillary);
    vlc_vector_clear(array);
}

int
vlc_ancillary_array_Merge(vlc_ancillary_array *dst_array,
                          const vlc_ancillary_array *src_array)
{
    if (src_array->size == 0)
        return VLC_SUCCESS;

    int ret = VLC_SUCCESS;
    for (size_t i = 0; i < src_array->size && ret == VLC_SUCCESS; ++i)
        ret = vlc_ancillary_array_Insert(dst_array, src_array->data[i]);

    return VLC_SUCCESS;
}

int
vlc_ancillary_array_MergeAndClear(vlc_ancillary_array *dst_array,
                                  vlc_ancillary_array *src_array)
{
    if (dst_array->size == 0)
    {
        *dst_array = *src_array;
        vlc_ancillary_array_Init(src_array);
        return VLC_SUCCESS;
    }

    int ret = vlc_ancillary_array_Merge(dst_array, src_array);
    if (ret == VLC_SUCCESS)
        vlc_ancillary_array_Clear(src_array);
    return ret;
}

int
vlc_ancillary_array_Insert(vlc_ancillary_array *array,
                           struct vlc_ancillary *ancillary)
{
    assert(ancillary != NULL);

    for (size_t i = 0; i < array->size; ++i)
    {
        struct vlc_ancillary *ancillary_it = array->data[i];
        if (ancillary_it->id == ancillary->id)
        {
            vlc_ancillary_Release(ancillary_it);
            array->data[i] = vlc_ancillary_Hold(ancillary);
            return VLC_SUCCESS;
        }
    }

    bool success = vlc_vector_push(array, ancillary);
    if (!success)
        return VLC_ENOMEM;
    vlc_ancillary_Hold(ancillary);

    return VLC_SUCCESS;
}

struct vlc_ancillary *
vlc_ancillary_array_Get(const vlc_ancillary_array *array,
                        vlc_ancillary_id id)
{
    struct vlc_ancillary *ancillary_it;
    vlc_vector_foreach(ancillary_it, array)
    {
        if (ancillary_it->id == id)
            return ancillary_it;
    }
    return NULL;
}
