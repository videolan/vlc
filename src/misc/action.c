/*****************************************************************************
 * action.c: key to action mapping
 *****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
 *           © 2009 Antoine Cellerier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "../libvlc.h"
#include <vlc_keys.h>
#include <stdlib.h>
#include <limits.h>

static int keycmp (const void *a, const void *b)
{
    const struct hotkey *ka = a, *kb = b;
#if (INT_MAX >= 0x7fffffff)
    return ka->i_key - kb->i_key;
#else
    return (ka->i_key < kb->i_key) ? -1 : (ka->i_key > kb->i_key) ? +1 : 0;
#endif
}

/**
 * Get the action associated with a VLC key code, if any.
 */
static
vlc_key_t vlc_TranslateKey (const vlc_object_t *obj, uint_fast32_t keycode)
{
    struct hotkey k = { .psz_action = NULL, .i_key = keycode, .i_action = 0 };
    const struct hotkey *key;

    key = bsearch (&k, obj->p_libvlc->p_hotkeys, libvlc_actions_count,
                   sizeof (*key), keycmp);
    return (key != NULL) ? key->i_action : ACTIONID_NONE;
}

static int vlc_key_to_action (vlc_object_t *libvlc, const char *varname,
                              vlc_value_t prevkey, vlc_value_t curkey, void *d)
{
    (void)varname;
    (void)prevkey;
    (void)d;

    vlc_key_t action = vlc_TranslateKey (libvlc, curkey.i_int);
    if (!action)
        return VLC_SUCCESS;
    return var_SetInteger (libvlc, "key-action", action);
}


int vlc_InitActions (libvlc_int_t *libvlc)
{
    struct hotkey *keys;

    var_Create (libvlc, "key-pressed", VLC_VAR_INTEGER);
    var_Create (libvlc, "key-action", VLC_VAR_INTEGER);

    keys = malloc ((libvlc_actions_count + 1) * sizeof (*keys));
    if (keys == NULL)
    {
        libvlc->p_hotkeys = NULL;
        return VLC_ENOMEM;
    }

    /* Initialize from configuration */
    for (size_t i = 0; i < libvlc_actions_count; i++)
    {
        keys[i].psz_action = libvlc_actions[i].name;
        keys[i].i_key = var_InheritInteger (libvlc, libvlc_actions[i].name );
        keys[i].i_action = libvlc_actions[i].value;
#ifndef NDEBUG
        if (i > 0
         && strcmp (libvlc_actions[i-1].name, libvlc_actions[i].name) >= 0)
        {
            msg_Err (libvlc, "%s and %s are not ordered properly",
                     libvlc_actions[i-1].name, libvlc_actions[i].name);
            abort ();
        }
#endif
    }
    qsort (keys, libvlc_actions_count, sizeof (*keys), keycmp);

    keys[libvlc_actions_count].psz_action = NULL;
    keys[libvlc_actions_count].i_key = 0;
    keys[libvlc_actions_count].i_action = 0;

    libvlc->p_hotkeys = keys;
    var_AddCallback (libvlc, "key-pressed", vlc_key_to_action, NULL);
    return VLC_SUCCESS;
}

void vlc_DeinitActions (libvlc_int_t *libvlc)
{
    if (unlikely(libvlc->p_hotkeys == NULL))
        return;
    var_DelCallback (libvlc, "key-pressed", vlc_key_to_action, NULL);
    free ((void *)libvlc->p_hotkeys);
}


static int actcmp(const void *key, const void *ent)
{
    const struct action *act = ent;
    return strcmp(key, act->name);
}

vlc_key_t vlc_GetActionId(const char *name)
{
    const struct action *act;

    act = bsearch(name, libvlc_actions, libvlc_actions_count, sizeof(*act),
                  actcmp);
    return (act != NULL) ? act->value : ACTIONID_NONE;
}

