/*****************************************************************************
 * playlist/request.c
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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

#include "item.h"
#include "playlist.h"

#include <vlc_input_item.h>

int
vlc_playlist_RequestInsert(vlc_playlist_t *playlist, size_t index,
                           input_item_t *const media[], size_t count)
{
    vlc_playlist_AssertLocked(playlist);

    size_t size = vlc_playlist_Count(playlist);
    if (index > size)
        index = size;

    return vlc_playlist_Insert(playlist, index, media, count);
}

static ssize_t
vlc_playlist_FindRealIndex(vlc_playlist_t *playlist, vlc_playlist_item_t *item,
                           ssize_t index_hint)
{
    if (index_hint != -1 && (size_t) index_hint < vlc_playlist_Count(playlist))
    {
        if (item == vlc_playlist_Get(playlist, index_hint))
            /* we are lucky */
            return index_hint;
    }

    /* we are unlucky, we need to find the item */
    return vlc_playlist_IndexOf(playlist, item);
}

struct size_vector VLC_VECTOR(size_t);

static void
vlc_playlist_FindIndices(vlc_playlist_t *playlist,
                         vlc_playlist_item_t *const items[], size_t count,
                         ssize_t index_hint, struct size_vector *out)
{
    for (size_t i = 0; i < count; ++i)
    {
        ssize_t real_index = vlc_playlist_FindRealIndex(playlist, items[i],
                                                        index_hint);
        if (real_index != -1)
        {
            int ok = vlc_vector_push(out, real_index);
            assert(ok); /* cannot fail, space had been reserved */
            VLC_UNUSED(ok);
            /* the next item is expected after this one */
            index_hint = real_index + 1;
        }
    }
}

static void
vlc_playlist_RemoveBySlices(vlc_playlist_t *playlist, size_t sorted_indices[],
                            size_t count)
{
    assert(count > 0);
    size_t last_index = sorted_indices[count - 1];
    size_t slice_size = 1;
    /* size_t is unsigned, take care not to test for non-negativity */
    for (size_t i = count - 1; i != 0; --i)
    {
        size_t index = sorted_indices[i - 1];
        if (index == last_index - 1)
            slice_size++;
        else
        {
            /* the previous slice is complete */
            vlc_playlist_Remove(playlist, last_index, slice_size);
            slice_size = 1;
        }
        last_index = index;
    }
    /* remove the last slice */
    vlc_playlist_Remove(playlist, last_index, slice_size);
}

/**
 * Move all items specified by their indices to form a contiguous slice, in
 * order.
 *
 * \param playlist   the playlist
 * \param indices    the indices of the items to regroup
 * \param head_index the index where to prepend the group
 * \return the start index of the resulting slice
 */
static size_t
vlc_playlist_Regroup(vlc_playlist_t *playlist, size_t indices[],
                     size_t head_index)
{
    size_t head = indices[head_index];
    if (head_index == 0)
        /* nothing to regroup */
        return head;

    size_t slice_size = 1;
    size_t last_index = indices[head_index - 1];

    /* size_t is unsigned, take care not to test for non-negativity */
    for (size_t i = head_index - 1; i != 0; --i)
    {
        size_t index = indices[i - 1];
        if (index == last_index - 1)
            slice_size++;
        else
        {
            assert(last_index != head);
            if (last_index < head)
            {
                assert(head >= slice_size);
                head -= slice_size;

                /* update index of items that will be moved as a side-effect */
                for (size_t j = 0; j <= i; ++j)
                    if (indices[j] >= last_index + slice_size && indices[j] < head)
                        indices[j] -= slice_size;
            }
            else
            {
                /* update index of items that will be moved as a side-effect */
                for (size_t j = 0; j <= i; ++j)
                    if (indices[j] >= head && indices[j] < last_index)
                        indices[j] += slice_size;
            }
            index = indices[i - 1]; /* current index might have been updated */

            /* the slice is complete, move it to build the unique slice */
            vlc_playlist_Move(playlist, last_index, slice_size, head);
            slice_size = 1;
        }

        last_index = index;
    }

    /* move the last slice to build the unique slice */
    if (last_index < head)
    {
        assert(head >= slice_size);
        head -= slice_size;
    }
    vlc_playlist_Move(playlist, last_index, slice_size, head);
    return head;
}

static void
vlc_playlist_MoveBySlices(vlc_playlist_t *playlist, size_t indices[],
                          size_t count, size_t target)
{
    assert(count > 0);

    /* pass the last slice */
    size_t i;
    for (i = count - 1; i != 0; --i)
        if (indices[i - 1] != indices[i] - 1)
            break;

    /* regroup items to form a unique slice */
    size_t head = vlc_playlist_Regroup(playlist, indices, i);

    /* move the unique slice to the requested target */
    if (head != target)
        vlc_playlist_Move(playlist, head, count, target);
}

static int
cmp_size(const void *lhs, const void *rhs)
{
    size_t a = *(size_t *) lhs;
    size_t b = *(size_t *) rhs;
    if (a < b)
        return -1;
    if (a == b)
        return 0;
    return 1;
}

int
vlc_playlist_RequestMove(vlc_playlist_t *playlist,
                         vlc_playlist_item_t *const items[], size_t count,
                         size_t target, ssize_t index_hint)
{
    vlc_playlist_AssertLocked(playlist);

    struct size_vector vector = VLC_VECTOR_INITIALIZER;
    if (!vlc_vector_reserve(&vector, count))
        return VLC_ENOMEM;

    vlc_playlist_FindIndices(playlist, items, count, index_hint, &vector);

    size_t move_count = vector.size;
    if (move_count)
    {
        size_t size = vlc_playlist_Count(playlist);
        assert(size >= move_count);
        /* move at most to the end of the list */
        if (target + move_count > size)
            target = size - move_count;

        /* keep the items in the same order as the request (do not sort them) */
        vlc_playlist_MoveBySlices(playlist, vector.data, vector.size, target);
    }

    vlc_vector_destroy(&vector);
    return VLC_SUCCESS;
}

int
vlc_playlist_RequestRemove(vlc_playlist_t *playlist,
                           vlc_playlist_item_t *const items[], size_t count,
                           ssize_t index_hint)
{
    vlc_playlist_AssertLocked(playlist);

    struct size_vector vector = VLC_VECTOR_INITIALIZER;
    if (!vlc_vector_reserve(&vector, count))
        return VLC_ENOMEM;

    vlc_playlist_FindIndices(playlist, items, count, index_hint, &vector);

    if (vector.size > 0)
    {
        /* sort so that removing an item does not shift the other indices */
        qsort(vector.data, vector.size, sizeof(vector.data[0]), cmp_size);

        vlc_playlist_RemoveBySlices(playlist, vector.data, vector.size);
    }

    vlc_vector_destroy(&vector);
    return VLC_SUCCESS;
}

int
vlc_playlist_RequestGoTo(vlc_playlist_t *playlist, vlc_playlist_item_t *item,
                         ssize_t index_hint)
{
    vlc_playlist_AssertLocked(playlist);
    ssize_t real_index = item
                       ? vlc_playlist_FindRealIndex(playlist, item, index_hint)
                       : -1;
    return vlc_playlist_GoTo(playlist, real_index);
}
