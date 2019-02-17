/*****************************************************************************
 * playlist/sort.c
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

#include <vlc_common.h>
#include <vlc_rand.h>
#include <vlc_sort.h>
#include "control.h"
#include "item.h"
#include "notify.h"
#include "playlist.h"

/**
 * Struct containing a copy of (parsed) media metadata, used for sorting
 * without locking all the items.
 */
struct vlc_playlist_item_meta {
    vlc_playlist_item_t *item;
    const char *title_or_name;
    vlc_tick_t duration;
    const char *artist;
    const char *album;
    const char *album_artist;
    const char *genre;
    const char *url;
    int64_t date;
    int64_t track_number;
    int64_t disc_number;
    int64_t rating;
    bool has_date;
    bool has_track_number;
    bool has_disc_number;
    bool has_rating;
};

static int
vlc_playlist_item_meta_CopyString(const char **to, const char *from)
{
    if (from)
    {
        *to = strdup(from);
        if (unlikely(!*to))
            return VLC_ENOMEM;
    }
    else
        *to = NULL;
    return VLC_SUCCESS;
}

static int
vlc_playlist_item_meta_InitField(struct vlc_playlist_item_meta *meta,
                                 enum vlc_playlist_sort_key key)
{
    input_item_t *media = meta->item->media;
    switch (key)
    {
        case VLC_PLAYLIST_SORT_KEY_TITLE:
        {
            const char *value = input_item_GetMetaLocked(media, vlc_meta_Title);
            if (EMPTY_STR(value))
                value = media->psz_name;
            return vlc_playlist_item_meta_CopyString(&meta->title_or_name,
                                                     value);
        }
        case VLC_PLAYLIST_SORT_KEY_DURATION:
        {
            if (media->i_duration == INPUT_DURATION_INDEFINITE
             || media->i_duration == INPUT_DURATION_UNSET)
                meta->duration = 0;
            else
                meta->duration = media->i_duration;
            return VLC_SUCCESS;
        }
        case VLC_PLAYLIST_SORT_KEY_ARTIST:
        {
            const char *value = input_item_GetMetaLocked(media,
                                                         vlc_meta_Artist);
            return vlc_playlist_item_meta_CopyString(&meta->artist, value);
        }
        case VLC_PLAYLIST_SORT_KEY_ALBUM:
        {
            const char *value = input_item_GetMetaLocked(media, vlc_meta_Album);
            return vlc_playlist_item_meta_CopyString(&meta->album, value);
        }
        case VLC_PLAYLIST_SORT_KEY_ALBUM_ARTIST:
        {
            const char *value = input_item_GetMetaLocked(media,
                                                         vlc_meta_AlbumArtist);
            return vlc_playlist_item_meta_CopyString(&meta->album_artist,
                                                     value);
        }
        case VLC_PLAYLIST_SORT_KEY_GENRE:
        {
            const char *value = input_item_GetMetaLocked(media, vlc_meta_Genre);
            return vlc_playlist_item_meta_CopyString(&meta->genre, value);
        }
        case VLC_PLAYLIST_SORT_KEY_DATE:
        {
            const char *str = input_item_GetMetaLocked(media, vlc_meta_Date);
            meta->has_date = !EMPTY_STR(str);
            if (meta->has_date)
                meta->date = atoll(str);
            return VLC_SUCCESS;
        }
        case VLC_PLAYLIST_SORT_KEY_TRACK_NUMBER:
        {
            const char *str = input_item_GetMetaLocked(media,
                                                       vlc_meta_TrackNumber);
            meta->has_track_number = !EMPTY_STR(str);
            if (meta->has_track_number)
                meta->track_number = atoll(str);
            return VLC_SUCCESS;
        }
        case VLC_PLAYLIST_SORT_KEY_DISC_NUMBER:
        {
            const char *str = input_item_GetMetaLocked(media,
                                                       vlc_meta_DiscNumber);
            meta->has_disc_number = !EMPTY_STR(str);
            if (meta->has_disc_number)
                meta->disc_number = atoll(str);
            return VLC_SUCCESS;
        }
        case VLC_PLAYLIST_SORT_KEY_URL:
        {
            const char *value = input_item_GetMetaLocked(media, vlc_meta_URL);
            return vlc_playlist_item_meta_CopyString(&meta->url, value);
        }
        case VLC_PLAYLIST_SORT_KEY_RATING:
        {
            const char *str = input_item_GetMetaLocked(media, vlc_meta_Rating);
            meta->has_rating = !EMPTY_STR(str);
            if (meta->has_rating)
                meta->rating = atoll(str);
            return VLC_SUCCESS;
        }
        default:
            assert(!"Unknown sort key");
            vlc_assert_unreachable();
    }
}

static void
vlc_playlist_item_meta_DestroyFields(struct vlc_playlist_item_meta *meta)
{
    free((void *) meta->title_or_name);
    free((void *) meta->artist);
    free((void *) meta->album);
    free((void *) meta->album_artist);
    free((void *) meta->genre);
    free((void *) meta->url);
}

static int
vlc_playlist_item_meta_InitFields(struct vlc_playlist_item_meta *meta,
        const struct vlc_playlist_sort_criterion criteria[], size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        const struct vlc_playlist_sort_criterion *criterion = &criteria[i];
        int ret = vlc_playlist_item_meta_InitField(meta, criterion->key);
        if (unlikely(ret != VLC_SUCCESS))
        {
            vlc_playlist_item_meta_DestroyFields(meta);
            return ret;
        }
    }
    return VLC_SUCCESS;
}

static struct vlc_playlist_item_meta *
vlc_playlist_item_meta_New(vlc_playlist_item_t *item,
                           const struct vlc_playlist_sort_criterion criteria[],
                           size_t count)
{
    /* assume that NULL representation is all-zeros */
    struct vlc_playlist_item_meta *meta = calloc(1, sizeof(*meta));
    if (unlikely(!meta))
        return NULL;

    meta->item = item;

    vlc_mutex_lock(&item->media->lock);
    int ret = vlc_playlist_item_meta_InitFields(meta, criteria, count);
    vlc_mutex_unlock(&item->media->lock);

    if (unlikely(ret != VLC_SUCCESS))
    {
        free(meta);
        return NULL;
    }

    return meta;
}

static void
vlc_playlist_item_meta_Delete(struct vlc_playlist_item_meta *meta)
{
    vlc_playlist_item_meta_DestroyFields(meta);
    free(meta);
}

static inline int
CompareStrings(const char *a, const char *b)
{
    if (a && b)
        return strcasecmp(a, b);
    if (!a && !b)
        return 0;
    return a ? 1 : -1;
}

static inline int
CompareIntegers(int64_t a, int64_t b)
{
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static inline int
CompareOptionalIntegers(bool has_a, int64_t a, bool has_b, int64_t b)
{
    if (has_a && has_b)
        return CompareIntegers(a, b);

    if (!has_a && !has_b)
        return 0;

    return a ? 1 : -1;
}

static inline int
CompareMetaByKey(const struct vlc_playlist_item_meta *a,
                 const struct vlc_playlist_item_meta *b,
                 enum vlc_playlist_sort_key key)
{
    switch (key)
    {
        case VLC_PLAYLIST_SORT_KEY_TITLE:
            return CompareStrings(a->title_or_name, b->title_or_name);
        case VLC_PLAYLIST_SORT_KEY_DURATION:
            return CompareIntegers(a->duration, b->duration);
        case VLC_PLAYLIST_SORT_KEY_ARTIST:
            return CompareStrings(a->artist, b->artist);
        case VLC_PLAYLIST_SORT_KEY_ALBUM:
            return CompareStrings(a->album, b->album);
        case VLC_PLAYLIST_SORT_KEY_ALBUM_ARTIST:
            return CompareStrings(a->album_artist, b->album_artist);
        case VLC_PLAYLIST_SORT_KEY_GENRE:
            return CompareStrings(a->genre, b->genre);
        case VLC_PLAYLIST_SORT_KEY_DATE:
            return CompareOptionalIntegers(a->has_date, a->date,
                                           b->has_date, b->date);
        case VLC_PLAYLIST_SORT_KEY_TRACK_NUMBER:
            return CompareOptionalIntegers(a->has_track_number, a->track_number,
                                           b->has_track_number, b->track_number);
        case VLC_PLAYLIST_SORT_KEY_DISC_NUMBER:
            return CompareOptionalIntegers(a->has_disc_number, a->disc_number,
                                           b->has_disc_number, b->disc_number);
        case VLC_PLAYLIST_SORT_KEY_URL:
            return CompareStrings(a->url, b->url);
        case VLC_PLAYLIST_SORT_KEY_RATING:
            return CompareOptionalIntegers(a->has_rating, a->rating,
                                           b->has_rating, b->rating);
        default:
            assert(!"Unknown sort key");
            vlc_assert_unreachable();
     }
}

/* context for qsort_r() */
struct sort_request
{
    const struct vlc_playlist_sort_criterion *criteria;
    size_t count;
};

static int
compare_meta(const void *lhs, const void *rhs, void *userdata)
{
    struct sort_request *req = userdata;
    const struct vlc_playlist_item_meta *a =
            *(const struct vlc_playlist_item_meta **) lhs;
    const struct vlc_playlist_item_meta *b =
            *(const struct vlc_playlist_item_meta **) rhs;

    for (size_t i = 0; i < req->count; ++i)
    {
        const struct vlc_playlist_sort_criterion *criterion = &req->criteria[i];
        int ret = CompareMetaByKey(a, b, criterion->key);
        if (ret)
        {
            if (criterion->order == VLC_PLAYLIST_SORT_ORDER_DESCENDING)
                /* do not return -ret, it's undefined if ret == INT_MIN */
                return ret > 0 ? -1 : 1;
            return ret;
        }
    }
    return 0;
}

static void
vlc_playlist_DeleteMetaArray(struct vlc_playlist_item_meta *array[],
                             size_t count)
{
    for (size_t i = 0; i < count; ++i)
        vlc_playlist_item_meta_Delete(array[i]);
    free(array);
}

static struct vlc_playlist_item_meta **
vlc_playlist_NewMetaArray(vlc_playlist_t *playlist,
        const struct vlc_playlist_sort_criterion criteria[], size_t count)
{
    struct vlc_playlist_item_meta **array =
            vlc_alloc(playlist->items.size, sizeof(*array));

    if (unlikely(!array))
        return NULL;

    size_t i;
    for (i = 0; i < playlist->items.size; ++i)
    {
        array[i] = vlc_playlist_item_meta_New(playlist->items.data[i],
                                              criteria, count);
        if (unlikely(!array[i]))
            break;
    }

    if (i < playlist->items.size)
    {
        /* allocation failure */
        vlc_playlist_DeleteMetaArray(array, i);
        return NULL;
    }

    return array;
}

int
vlc_playlist_Sort(vlc_playlist_t *playlist,
                  const struct vlc_playlist_sort_criterion criteria[],
                  size_t count)
{
    assert(count > 0);
    vlc_playlist_AssertLocked(playlist);

    vlc_playlist_item_t *current = playlist->current != -1
                                 ? playlist->items.data[playlist->current]
                                 : NULL;

    struct vlc_playlist_item_meta **array =
        vlc_playlist_NewMetaArray(playlist, criteria, count);
    if (unlikely(!array))
        return VLC_ENOMEM;

    struct sort_request req = { criteria, count };

    vlc_qsort(array, playlist->items.size, sizeof(*array), compare_meta, &req);

    /* apply the sorting result to the playlist */
    for (size_t i = 0; i < playlist->items.size; ++i)
        playlist->items.data[i] = array[i]->item;

    vlc_playlist_DeleteMetaArray(array, playlist->items.size);

    struct vlc_playlist_state state;
    if (current)
    {
        /* the current position have changed after the shuffle */
        vlc_playlist_state_Save(playlist, &state);
        playlist->current = vlc_playlist_IndexOf(playlist, current);
        playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
        playlist->has_next = vlc_playlist_ComputeHasNext(playlist);
    }

    vlc_playlist_Notify(playlist, on_items_reset, playlist->items.data,
                        playlist->items.size);
    if (current)
        vlc_playlist_state_NotifyChanges(playlist, &state);

    return VLC_SUCCESS;
}
