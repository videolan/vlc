/*****************************************************************************
 * keys.c: keys configuration
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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
# include <config.h>
#endif

/**
 * \file
 * This file defines functions and structures for hotkey handling in vlc
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_keys.h>
#include "configuration.h"
#include "libvlc.h"

typedef struct key_descriptor_s
{
    const char psz_key_string[20];
    uint32_t i_key_code;
} key_descriptor_t;

static const struct key_descriptor_s vlc_keys[] =
{
    { "Unset", KEY_UNSET },
    { "Backspace", KEY_BACKSPACE },
    { "Tab", KEY_TAB },
    { "Enter", KEY_ENTER },
    { "Esc", KEY_ESC },
    { "Space", ' ' },
    { "Left", KEY_LEFT },
    { "Right", KEY_RIGHT },
    { "Up", KEY_UP },
    { "Down", KEY_DOWN },
    { "F1", KEY_F1 },
    { "F2", KEY_F2 },
    { "F3", KEY_F3 },
    { "F4", KEY_F4 },
    { "F5", KEY_F5 },
    { "F6", KEY_F6 },
    { "F7", KEY_F7 },
    { "F8", KEY_F8 },
    { "F9", KEY_F9 },
    { "F10", KEY_F10 },
    { "F11", KEY_F11 },
    { "F12", KEY_F12 },
    { "Home", KEY_HOME },
    { "End", KEY_END },
    { "Insert", KEY_INSERT },
    { "Delete", KEY_DELETE },
    { "Menu", KEY_MENU },
    { "Page Up", KEY_PAGEUP },
    { "Page Down", KEY_PAGEDOWN },
    { "Browser Back", KEY_BROWSER_BACK },
    { "Browser Forward", KEY_BROWSER_FORWARD },
    { "Browser Refresh", KEY_BROWSER_REFRESH },
    { "Browser Stop", KEY_BROWSER_STOP },
    { "Browser Search", KEY_BROWSER_SEARCH },
    { "Browser Favorites", KEY_BROWSER_FAVORITES },
    { "Browser Home", KEY_BROWSER_HOME },
    { "Volume Mute", KEY_VOLUME_MUTE },
    { "Volume Down", KEY_VOLUME_DOWN },
    { "Volume Up", KEY_VOLUME_UP },
    { "Media Next Track", KEY_MEDIA_NEXT_TRACK },
    { "Media Prev Track", KEY_MEDIA_PREV_TRACK },
    { "Media Stop", KEY_MEDIA_STOP },
    { "Media Play Pause", KEY_MEDIA_PLAY_PAUSE },
    { "Mouse Wheel Up", KEY_MOUSEWHEELUP },
    { "Mouse Wheel Down", KEY_MOUSEWHEELDOWN },
    { "Mouse Wheel Left", KEY_MOUSEWHEELLEFT },
    { "Mouse Wheel Right", KEY_MOUSEWHEELRIGHT },
};
enum { vlc_num_keys=sizeof(vlc_keys)/sizeof(struct key_descriptor_s) };

static int cmpkey (const void *key, const void *elem)
{
    return ((uintptr_t)key) - ((key_descriptor_t *)elem)->i_key_code;
}

/* Convert Unicode code point to UTF-8 */
static char *utf8_cp (uint_fast32_t cp, char *buf)
{
    if (cp < (1 << 7))
    {
        buf[1] = 0;
        buf[0] = cp;
    }
    else if (cp < (1 << 11))
    {
        buf[2] = 0;
        buf[1] = 0x80 | (cp & 0x3F);
        cp >>= 6;
        buf[0] = 0xC0 | cp;
    }
    else if (cp < (1 << 16))
    {
        buf[3] = 0;
        buf[2] = 0x80 | (cp & 0x3F);
        cp >>= 6;
        buf[1] = 0x80 | (cp & 0x3F);
        cp >>= 6;
        buf[0] = 0xE0 | cp;
    }
    else if (cp < (1 << 21))
    {
        buf[4] = 0;
        buf[3] = 0x80 | (cp & 0x3F);
        cp >>= 6;
        buf[2] = 0x80 | (cp & 0x3F);
        cp >>= 6;
        buf[1] = 0x80 | (cp & 0x3F);
        cp >>= 6;
        buf[0] = 0xE0 | cp;
    }
    else
        return NULL;
    return buf;
}

uint_fast32_t ConfigStringToKey (const char *name)
{
    uint_fast32_t mods = 0;
    uint32_t cp;

    for (;;)
    {
        size_t len = strcspn (name, "-+");
        if (len == 0 || name[len] == '\0')
            break;

        if (len == 4 && !strncasecmp (name, "Ctrl", 4))
            mods |= KEY_MODIFIER_CTRL;
        if (len == 3 && !strncasecmp (name, "Alt", 3))
            mods |= KEY_MODIFIER_ALT;
        if (len == 5 && !strncasecmp (name, "Shift", 5))
            mods |= KEY_MODIFIER_SHIFT;
        if (len == 4 && !strncasecmp (name, "Meta", 4))
            mods |= KEY_MODIFIER_META;
        if (len == 7 && !strncasecmp (name, "Command", 7))
            mods |= KEY_MODIFIER_COMMAND;

        name += len + 1;
    }

    for (size_t i = 0; i < vlc_num_keys; i++)
        if (!strcasecmp( vlc_keys[i].psz_key_string, name))
            return vlc_keys[i].i_key_code | mods;

    return (vlc_towc (name, &cp) > 0) ? (mods | cp) : KEY_UNSET;
}

char *vlc_keycode2str (uint_fast32_t code)
{
    char *str, buf[5];
    uintptr_t key = code & ~KEY_MODIFIER;

    key_descriptor_t *d = bsearch ((void *)key, vlc_keys, vlc_num_keys,
                                   sizeof (vlc_keys[0]), cmpkey);
    if (d == NULL && utf8_cp (key, buf) == NULL)
        return NULL;

    if (asprintf (&str, "%s%s%s%s%s%s",
                  (code & KEY_MODIFIER_CTRL) ? "Ctrl+" : "",
                  (code & KEY_MODIFIER_ALT) ? "Alt+" : "",
                  (code & KEY_MODIFIER_SHIFT) ? "Shift+" : "",
                  (code & KEY_MODIFIER_META) ? "Meta+" : "",
                  (code & KEY_MODIFIER_COMMAND) ? "Command+" : "",
                  (d != NULL) ? d->psz_key_string : buf) == -1)
        return NULL;

    return str;
}


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
