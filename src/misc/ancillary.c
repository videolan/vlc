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

#include "ancillary.h"

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
vlc_ancillary_array_Clear(struct vlc_ancillary ***array)
{
    if (*array != NULL)
    {
        for (struct vlc_ancillary **ancillary = *array;
             *ancillary != NULL; ancillary++)
        {
            vlc_ancillary_Release(*ancillary);
        }

        free(*array);
        *array = NULL;
    }
}

static size_t
vlc_ancillary_array_Count(struct vlc_ancillary **array)
{
    size_t count = 0;
    for (struct vlc_ancillary **ancillary = array;
         *ancillary != NULL; ancillary++)
    {
        count++;
    }

    return count;
}

int
vlc_ancillary_array_Dup(struct vlc_ancillary ***dst_arrayp,
                        struct vlc_ancillary ** const*src_arrayp)
{
    if (unlikely(*dst_arrayp != NULL))
        vlc_ancillary_array_Clear(dst_arrayp);

    if (*src_arrayp == NULL)
        return VLC_SUCCESS;

    struct vlc_ancillary **src_array = *src_arrayp;
    size_t count = vlc_ancillary_array_Count(src_array);

    struct vlc_ancillary **dst_array =
        vlc_alloc(count + 1, sizeof(struct vlc_ancillary *));
    if (dst_array == NULL)
        return VLC_ENOMEM;

    for (size_t i = 0; i < count; ++i)
    {
        dst_array[i] = vlc_ancillary_Hold(src_array[i]);
        assert(dst_array[i] != NULL);
    }
    dst_array[count] = NULL;
    *dst_arrayp = dst_array;

    return VLC_SUCCESS;
}

int
vlc_ancillary_array_Insert(struct vlc_ancillary ***arrayp,
                           struct vlc_ancillary *ancillary)
{
    /* First case: the array is empty */
    if (*arrayp == NULL)
    {
        struct vlc_ancillary **array = vlc_alloc(2, sizeof(struct vlc_ancillary *));
        if (array == NULL)
            return VLC_ENOMEM;

        array[0] = vlc_ancillary_Hold(ancillary);
        array[1] = NULL;

        *arrayp = array;

        return VLC_SUCCESS;
    }

    struct vlc_ancillary **array = *arrayp;
    size_t count = vlc_ancillary_array_Count(array);

    /* Second case: the array has already an ancillary of the same id (very
     * unlikely) */
    for (size_t i = 0; i < count; ++i)
    {
        if (array[i]->id == ancillary->id)
        {
            vlc_ancillary_Release(array[i]);
            array[i] = vlc_ancillary_Hold(ancillary);
            return VLC_SUCCESS;
        }
    }

    /* Third case: realloc the array to add the new ancillary */
    array = vlc_reallocarray(array, count + 2, sizeof(struct vlc_ancillary *));
    if (array == NULL)
        return VLC_ENOMEM;

    array[count] = vlc_ancillary_Hold(ancillary);
    array[count + 1] = NULL;

    *arrayp = array;

    return VLC_SUCCESS;
}

struct vlc_ancillary *
vlc_ancillary_array_Get(struct vlc_ancillary ** const*arrayp,
                        vlc_ancillary_id id)
{
    if (*arrayp == NULL)
        return NULL;

    struct vlc_ancillary **array = *arrayp;
    for (struct vlc_ancillary **ancillary = array;
         *ancillary != NULL; ancillary++)
    {
        if ((*ancillary)->id == id)
            return *ancillary;
    }
    return NULL;
}
