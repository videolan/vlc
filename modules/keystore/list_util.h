/*****************************************************************************
 * list_util.h: list helper used by memory and file_crypt keystores
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

#ifndef VLC_KEYSTORE_LIST_UTIL_H_
#define VLC_KEYSTORE_LIST_UTIL_H_

/*
 * Copy KEY_MAX values from ppsz_src to ppsz_dst
 */
int ks_values_copy(const char * ppsz_dst[KEY_MAX],
                   const char *const ppsz_src[KEY_MAX]);

struct ks_list
{
    vlc_keystore_entry *p_entries;
    unsigned            i_count;
    unsigned            i_max;
};

/*
 * Allocate and return a vlc_keystore_entry that will be stored in the ks_list
 */
vlc_keystore_entry *ks_list_new_entry(struct ks_list *p_list);

/*
 * Free all entries stored in the list
 */
void ks_list_free(struct ks_list *p_list);

/*
 * Find an entry in the ks_list that match ppsz_values
 */
vlc_keystore_entry *ks_list_find_entry(struct ks_list *p_list,
                                       const char *const ppsz_values[KEY_MAX],
                                       unsigned *p_start_index);

#endif /* VLC_KEYSTORE_LIST_UTIL_H_ */
