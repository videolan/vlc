/*****************************************************************************
 * memory.c: Memory keystore
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

#include <stdio.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_memory.h>
#include <vlc_keystore.h>
#include <vlc_strings.h>

#include <assert.h>

#include "list_util.h"

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("Memory keystore"))
    set_description(N_("Secrets are stored in memory"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("keystore", 0)
    set_callbacks(Open, Close)
    add_shortcut("memory")
vlc_module_end ()

struct vlc_keystore_sys
{
    struct ks_list  list;
    vlc_mutex_t     lock;
};

static int
Store(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX],
      const uint8_t *p_secret, size_t i_secret_len, const char *psz_label)
{
    (void) psz_label;
    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    struct ks_list *p_list = &p_sys->list;
    vlc_keystore_entry *p_entry;
    int i_ret = VLC_EGENERIC;

    vlc_mutex_lock(&p_sys->lock);
    p_entry = ks_list_find_entry(p_list, ppsz_values, NULL);
    if (p_entry)
        vlc_keystore_release_entry(p_entry);
    else
    {
        p_entry = ks_list_new_entry(p_list);
        if (!p_entry)
            goto end;
    }
    if (ks_values_copy((const char **)p_entry->ppsz_values, ppsz_values))
        goto end;

    if (vlc_keystore_entry_set_secret(p_entry, p_secret, i_secret_len))
        goto end;

    i_ret = VLC_SUCCESS;
end:
    vlc_mutex_unlock(&p_sys->lock);
    return i_ret;
}

static unsigned int
Find(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX],
     vlc_keystore_entry **pp_entries)
{
    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    struct ks_list *p_list = &p_sys->list;
    struct ks_list out_list = { 0 };
    vlc_keystore_entry *p_entry;
    unsigned i_index = 0;

    vlc_mutex_lock(&p_sys->lock);
    while ((p_entry = ks_list_find_entry(p_list, ppsz_values, &i_index)))
    {
        vlc_keystore_entry *p_out_entry = ks_list_new_entry(&out_list);

        if (!p_out_entry
         || ks_values_copy((const char **)p_out_entry->ppsz_values,
                           (const char *const*)p_entry->ppsz_values))
        {
            ks_list_free(&out_list);
            break;
        }

        if (vlc_keystore_entry_set_secret(p_out_entry, p_entry->p_secret,
                                          p_entry->i_secret_len))
        {
            ks_list_free(&out_list);
            break;
        }
    }
    vlc_mutex_unlock(&p_sys->lock);

    *pp_entries = out_list.p_entries;

    return out_list.i_count;
}

static unsigned int
Remove(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX])
{
    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    struct ks_list *p_list = &p_sys->list;
    vlc_keystore_entry *p_entry;
    unsigned i_index = 0, i_count = 0;

    vlc_mutex_lock(&p_sys->lock);
    while ((p_entry = ks_list_find_entry(p_list, ppsz_values, &i_index)))
    {
        vlc_keystore_release_entry(p_entry);
        i_count++;
    }
    vlc_mutex_unlock(&p_sys->lock);

    return i_count;
}

static void
Close(vlc_object_t *p_this)
{
    vlc_keystore *p_keystore = (vlc_keystore *)p_this;
    vlc_keystore_sys *p_sys = p_keystore->p_sys;

    ks_list_free(&p_sys->list);
    vlc_mutex_destroy(&p_keystore->p_sys->lock);
    free(p_sys);
}

static int
Open(vlc_object_t *p_this)
{
    vlc_keystore *p_keystore = (vlc_keystore *)p_this;
    p_keystore->p_sys = calloc(1, sizeof(vlc_keystore_sys));
    if (!p_keystore->p_sys)
        return VLC_EGENERIC;

    vlc_mutex_init(&p_keystore->p_sys->lock);
    p_keystore->pf_store = Store;
    p_keystore->pf_find = Find;
    p_keystore->pf_remove = Remove;

    return VLC_SUCCESS;
}
