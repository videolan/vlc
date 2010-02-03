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

#include <vlc_common.h>
#include <vlc_keys.h>
#include "configuration.h"

typedef struct key_descriptor_s
{
    const char *psz_key_string;
    uint32_t i_key_code;
} key_descriptor_t;

static const struct key_descriptor_s vlc_modifiers[] =
{
    { "Alt", KEY_MODIFIER_ALT },
    { "Shift", KEY_MODIFIER_SHIFT },
    { "Ctrl", KEY_MODIFIER_CTRL },
    { "Meta", KEY_MODIFIER_META },
    { "Command", KEY_MODIFIER_COMMAND }
};
enum { vlc_num_modifiers=sizeof(vlc_modifiers)
                        /sizeof(struct key_descriptor_s) };

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

/* Convert UTF-8 to Unicode code point */
static uint_fast32_t cp_utf8 (const char *utf8)
{
    uint8_t f = utf8[0];
    size_t l = strlen (utf8);

    if (f < 0x80) /* ASCII (7 bits) */
        return f;
    if (f < 0xC0 || l < 2) /* bad */
        return 0;
    if (f < 0xE0) /* two bytes (11 bits) */
        return ((f & 0x1F) << 6) | (utf8[1] & 0x3F);
    if (l < 3) /* bad */
        return 0;
    if (f < 0xF0) /* three bytes (16 bits) */
        return ((f & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6)
               | (utf8[2] & 0x3F);
    if (l < 4)
        return 0;
    if (f < 0xF8) /* four bytes (21 bits) */
        return ((f & 0x07) << 18) | ((utf8[1] & 0x3F) << 12)
               | ((utf8[2] & 0x3F) << 6) | (utf8[3] & 0x3F);
    return 0;
}

char *KeyToString (uint_fast32_t sym)
{
    key_descriptor_t *d;

    d = bsearch ((void *)(uintptr_t)sym, vlc_keys, vlc_num_keys,
                 sizeof (vlc_keys[0]), cmpkey);
    if (d)
        return strdup (d->psz_key_string);

    char buf[5];
    if (utf8_cp (sym, buf))
        return strdup (buf);

    return NULL;
}

uint_fast32_t StringToKey (char *name)
{
    for (size_t i = 0; i < vlc_num_keys; i++)
        if (!strcmp (vlc_keys[i].psz_key_string, name))
            return vlc_keys[i].i_key_code;

    return cp_utf8 (name);
}

uint_fast32_t ConfigStringToKey (const char *name)
{
    uint_fast32_t mods = 0;

    for (;;)
    {
        const char *psz_parser = strchr (name, '-');
        if (psz_parser == NULL || psz_parser == name)
            break;

        for (size_t i = 0; i < vlc_num_modifiers; i++)
        {
            if (!strncasecmp (vlc_modifiers[i].psz_key_string, name,
                              strlen (vlc_modifiers[i].psz_key_string)))
            {
                mods |= vlc_modifiers[i].i_key_code;
            }
        }
        name = psz_parser + 1;
    }

    for (size_t i = 0; i < vlc_num_keys; i++)
        if (!strcasecmp( vlc_keys[i].psz_key_string, name))
            return vlc_keys[i].i_key_code | mods;

    return cp_utf8 (name) | mods;
}

char *ConfigKeyToString (uint_fast32_t i_key)
{
    // Worst case appears to be 45 characters:
    // "Command-Meta-Ctrl-Shift-Alt-Browser Favorites"
    char *psz_key = malloc (64);
    if (!psz_key)
        return NULL;

    char *p = psz_key, *psz_end = psz_key + 54;
    *p = '\0';

    for (size_t i = 0; i < vlc_num_modifiers; i++)
    {
        if (i_key & vlc_modifiers[i].i_key_code)
        {
            p += snprintf (p, psz_end - p, "%s-",
                           vlc_modifiers[i].psz_key_string);
        }
    }

    key_descriptor_t *d;
    char buf[5];

    i_key &= ~KEY_MODIFIER;
    d = bsearch ((void *)(uintptr_t)i_key, vlc_keys, vlc_num_keys,
                 sizeof (vlc_keys[0]), cmpkey);
    if (d)
        p += snprintf (p, psz_end - p, "%s", d->psz_key_string);
    else if (utf8_cp (i_key, buf))
        p += snprintf (p, psz_end - p, "%s", buf);
    else
        return NULL;

    return psz_key;
}
