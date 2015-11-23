/*****************************************************************************
 * keystore.c:
 *****************************************************************************
 * Copyright (C) 2015-2016  VLC authors, VideoLAN and VideoLabs
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_keystore.h>
#include <vlc_modules.h>
#include <libvlc.h>

#include <assert.h>
#include <limits.h>

#undef vlc_keystore_create
vlc_keystore *
vlc_keystore_create(vlc_object_t *p_parent)
{
    assert(p_parent);
    vlc_keystore *p_keystore = vlc_custom_create(p_parent, sizeof (*p_keystore),
                                                 "keystore");
    if (unlikely(p_keystore == NULL))
        return NULL;

    p_keystore->p_module = module_need(p_keystore, "keystore", "$keystore",
                                       true);
    if (p_keystore->p_module == NULL)
    {
        vlc_object_release(p_keystore);
        return NULL;
    }
    assert(p_keystore->pf_store);
    assert(p_keystore->pf_find);
    assert(p_keystore->pf_remove);

    return p_keystore;
}

void
vlc_keystore_release(vlc_keystore *p_keystore)
{
    assert(p_keystore);
    module_unneed(p_keystore, p_keystore->p_module);

    vlc_object_release(p_keystore);
}

int
vlc_keystore_store(vlc_keystore *p_keystore,
                   const char * const ppsz_values[KEY_MAX],
                   const uint8_t *p_secret, ssize_t i_secret_len,
                   const char *psz_label)
{
    assert(p_keystore && ppsz_values && p_secret && i_secret_len);

    if (!ppsz_values[KEY_PROTOCOL] || !ppsz_values[KEY_SERVER])
    {
        msg_Err(p_keystore, "invalid store request: "
                "protocol and server should be valid");
        return VLC_EGENERIC;
    }
    if (ppsz_values[KEY_PORT])
    {
        long int i_port = strtol(ppsz_values[KEY_PORT], NULL, 10);
        if (i_port == LONG_MIN || i_port == LONG_MAX)
        {
            msg_Err(p_keystore, "invalid store request: "
                    "port is not valid number");
            return VLC_EGENERIC;
        }
    }
    if (i_secret_len < 0)
        i_secret_len = strlen((const char *)p_secret) + 1;
    return p_keystore->pf_store(p_keystore, ppsz_values, p_secret, i_secret_len,
                                psz_label);
}

unsigned int
vlc_keystore_find(vlc_keystore *p_keystore, 
                  const char * const ppsz_values[KEY_MAX],
                  vlc_keystore_entry **pp_entries)
{
    assert(p_keystore && ppsz_values && pp_entries);
    return p_keystore->pf_find(p_keystore, ppsz_values, pp_entries);
}

unsigned int
vlc_keystore_remove(vlc_keystore *p_keystore,
                    const char *const ppsz_values[KEY_MAX])
{
    assert(p_keystore && ppsz_values);
    return p_keystore->pf_remove(p_keystore, ppsz_values);
}

void
vlc_keystore_release_entries(vlc_keystore_entry *p_entries, unsigned int i_count)
{
    for (unsigned int i = 0; i < i_count; ++i)
    {
        vlc_keystore_entry *p_entry = &p_entries[i];
        for (unsigned int j = 0; j < KEY_MAX; ++j)
            free(p_entry->ppsz_values[j]);
        free(p_entry->p_secret);
    }
    free (p_entries);
}
