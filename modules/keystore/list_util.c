/*****************************************************************************
 * list_util.c: list helper used by memory and file_crypt keystores
 *****************************************************************************
 * Copyright © 2015-2016 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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
#include <vlc_keystore.h>

#include "list_util.h"

void
ks_list_free(struct ks_list *p_list)
{
    vlc_keystore_release_entries(p_list->p_entries, p_list->i_count);
    p_list->p_entries = NULL;
    p_list->i_count = 0;
    p_list->i_max = 0;
}

int
ks_values_copy(const char * ppsz_dst[KEY_MAX],
               const char *const ppsz_src[KEY_MAX])
{
    for (unsigned int i = 0; i < KEY_MAX; ++i)
    {
        if (ppsz_src[i])
        {
            ppsz_dst[i] = strdup(ppsz_src[i]);
            if (!ppsz_dst[i])
                return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

vlc_keystore_entry *
ks_list_new_entry(struct ks_list *p_list)
{
    if (p_list->i_count + 1 > p_list->i_max)
    {
        p_list->i_max += 10;
        vlc_keystore_entry *p_entries = realloc(p_list->p_entries, p_list->i_max
                                                * sizeof(*p_list->p_entries));
        if (!p_entries)
        {
            ks_list_free(p_list);
            return NULL;
        }
        p_list->p_entries = p_entries;
    }
    vlc_keystore_entry *p_entry = &p_list->p_entries[p_list->i_count];
    p_entry->p_secret = NULL;
    VLC_KEYSTORE_VALUES_INIT(p_entry->ppsz_values);
    p_list->i_count++;

    return p_entry;
}

vlc_keystore_entry *
ks_list_find_entry(struct ks_list *p_list, const char *const ppsz_values[KEY_MAX],
                   unsigned *p_start_index)
{
    for (unsigned int i = p_start_index ? *p_start_index : 0;
         i < p_list->i_count; ++i)
    {
        vlc_keystore_entry *p_entry = &p_list->p_entries[i];
        if (!p_entry->p_secret)
            continue;

        bool b_match = true;
        for (unsigned int j = 0; j < KEY_MAX; ++j)
        {
            const char *psz_value1 = ppsz_values[j];
            const char *psz_value2 = p_entry->ppsz_values[j];

            if (!psz_value1)
                continue;
            if (!psz_value2 || strcmp(psz_value1, psz_value2))
                b_match = false;
        }
        if (b_match)
        {
            if (p_start_index)
                *p_start_index = i + 1;
            return p_entry;
        }
    }
    return NULL;
}
