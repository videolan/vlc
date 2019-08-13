/*****************************************************************************
 * player_title.c: Player title implementation
 *****************************************************************************
 * Copyright © 2018-2019 VLC authors and VideoLAN
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

#include <limits.h>

#include <vlc_common.h>
#include "player.h"

struct vlc_player_title_list *
vlc_player_title_list_Hold(struct vlc_player_title_list *titles)
{
    vlc_atomic_rc_inc(&titles->rc);
    return titles;
}

void
vlc_player_title_list_Release(struct vlc_player_title_list *titles)
{
    if (!vlc_atomic_rc_dec(&titles->rc))
        return;
    for (size_t title_idx = 0; title_idx < titles->count; ++title_idx)
    {
        struct vlc_player_title *title = &titles->array[title_idx];
        free((char *)title->name);
        for (size_t chapter_idx = 0; chapter_idx < title->chapter_count;
             ++chapter_idx)
        {
            const struct vlc_player_chapter *chapter =
                &title->chapters[chapter_idx];
            free((char *)chapter->name);
        }
        free((void *)title->chapters);
    }
    free(titles);
}

static char *
input_title_GetName(const struct input_title_t *input_title, int idx,
                    int title_offset)
{
    int ret;
    char length_str[MSTRTIME_MAX_SIZE + sizeof(" []")];

    if (input_title->i_length > 0)
    {
        strcpy(length_str, " [");
        secstotimestr(&length_str[2], SEC_FROM_VLC_TICK(input_title->i_length));
        strcat(length_str, "]");
    }
    else
        length_str[0] = '\0';

    char *dup;
    if (input_title->psz_name && input_title->psz_name[0] != '\0')
        ret = asprintf(&dup, "%s%s", input_title->psz_name, length_str);
    else
        ret = asprintf(&dup, _("Title %i%s"), idx + title_offset, length_str);
    if (ret == -1)
        return NULL;
    return dup;
}

static char *
seekpoint_GetName(seekpoint_t *seekpoint, int idx, int chapter_offset)
{
    if (seekpoint->psz_name && seekpoint->psz_name[0] != '\0' )
        return strdup(seekpoint->psz_name);

    char *dup;
    int ret = asprintf(&dup, _("Chapter %i"), idx + chapter_offset);
    if (ret == -1)
        return NULL;
    return dup;
}

struct vlc_player_title_list *
vlc_player_title_list_Create(input_title_t *const *array, size_t count,
                             int title_offset, int chapter_offset)
{
    if (count == 0)
        return NULL;

    /* Allocate the struct + the whole list */
    size_t size;
    if (mul_overflow(count, sizeof(struct vlc_player_title), &size))
        return NULL;
    if (add_overflow(size, sizeof(struct vlc_player_title_list), &size))
        return NULL;
    struct vlc_player_title_list *titles = malloc(size);
    if (!titles)
        return NULL;

    vlc_atomic_rc_init(&titles->rc);
    titles->count = count;

    for (size_t title_idx = 0; title_idx < titles->count; ++title_idx)
    {
        const struct input_title_t *input_title = array[title_idx];
        struct vlc_player_title *title = &titles->array[title_idx];

        title->name = input_title_GetName(input_title, title_idx, title_offset);
        title->length = input_title->i_length;
        title->flags = input_title->i_flags;
        const size_t seekpoint_count = input_title->i_seekpoint > 0 ?
                                       input_title->i_seekpoint : 0;
        title->chapter_count = seekpoint_count;

        struct vlc_player_chapter *chapters = title->chapter_count == 0 ? NULL :
            vlc_alloc(title->chapter_count, sizeof(*chapters));

        if (chapters)
        {
            for (size_t chapter_idx = 0; chapter_idx < title->chapter_count;
                 ++chapter_idx)
            {
                struct vlc_player_chapter *chapter = &chapters[chapter_idx];
                seekpoint_t *seekpoint = input_title->seekpoint[chapter_idx];

                chapter->name = seekpoint_GetName(seekpoint, chapter_idx,
                                                  chapter_offset);
                chapter->time = seekpoint->i_time_offset;
                if (!chapter->name) /* Will trigger the error path */
                    title->chapter_count = chapter_idx;
            }
        }
        else if (seekpoint_count > 0) /* Will trigger the error path */
            title->chapter_count = 0;

        title->chapters = chapters;

        if (!title->name || seekpoint_count != title->chapter_count)
        {
            /* Release titles up to title_idx */
            titles->count = title_idx;
            vlc_player_title_list_Release(titles);
            return NULL;
        }
    }
    return titles;
}

const struct vlc_player_title *
vlc_player_title_list_GetAt(struct vlc_player_title_list *titles, size_t idx)
{
    assert(idx < titles->count);
    return &titles->array[idx];
}

size_t
vlc_player_title_list_GetCount(struct vlc_player_title_list *titles)
{
    return titles->count;
}
