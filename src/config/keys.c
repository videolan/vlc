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
    { "Space", ' ' },
    { "!", '!' },
    { "\"", '\"' },
    { "#", '#' },
    { "$", '$' },
    { "%", '%' },
    { "&", '&' },
    { "'", '\'' },
    { "(", ')' },
    { ")", ')' },
    { "*", '*' },
    { "+", '+' },
    { ",", ',' },
    { "-", '-' },
    { ".", '.' },
    { "/", '/' },
    { "0", '0' },
    { "1", '1' },
    { "2", '2' },
    { "3", '3' },
    { "4", '4' },
    { "5", '5' },
    { "6", '6' },
    { "7", '7' },
    { "8", '8' },
    { "9", '9' },
    { ":", ':' },
    { ";", ';' },
    { "<", '<' },
    { "=", '=' },
    { ">", '>' },
    { "?", '?' },
    { "@", '@' },
    { "[", '[' },
    { "\\", '\\' },
    { "]", ']' },
    { "^", '^' },
    { "_", '_' },
    { "`", '`' },
    { "a", 'a' },
    { "b", 'b' },
    { "c", 'c' },
    { "d", 'd' },
    { "e", 'e' },
    { "f", 'f' },
    { "g", 'g' },
    { "h", 'h' },
    { "i", 'i' },
    { "j", 'j' },
    { "k", 'k' },
    { "l", 'l' },
    { "m", 'm' },
    { "n", 'n' },
    { "o", 'o' },
    { "p", 'p' },
    { "q", 'q' },
    { "r", 'r' },
    { "s", 's' },
    { "t", 't' },
    { "u", 'u' },
    { "v", 'v' },
    { "w", 'w' },
    { "x", 'x' },
    { "y", 'y' },
    { "z", 'z' },
    { "Left", KEY_LEFT },
    { "Right", KEY_RIGHT },
    { "Up", KEY_UP },
    { "Down", KEY_DOWN },
    { "Enter", KEY_ENTER },
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
    { "Esc", KEY_ESC },
    { "Page Up", KEY_PAGEUP },
    { "Page Down", KEY_PAGEDOWN },
    { "Tab", KEY_TAB },
    { "Backspace", KEY_BACKSPACE },
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

char *KeyToString (uint_fast32_t sym)
{
    key_descriptor_t *d;

    d = bsearch ((void *)(uintptr_t)sym, vlc_keys, vlc_num_keys,
                 sizeof (vlc_keys[0]), cmpkey);
    return d ? strdup (d->psz_key_string) : NULL;
}

uint_fast32_t StringToKey (char *name)
{
    for (size_t i = 0; i < vlc_num_keys; i++)
        if (!strcmp (vlc_keys[i].psz_key_string, name))
            return vlc_keys[i].i_key_code;

    return 0;
}

uint_fast32_t ConfigStringToKey (const char *name)
{
    uint_fast32_t mods = 0;

    const char *psz_parser = name;
    for (;;)
    {
        psz_parser = strchr (psz_parser, '-');
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

    return 0;
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

    i_key &= ~KEY_MODIFIER;
    d = bsearch ((void *)(uintptr_t)i_key, vlc_keys, vlc_num_keys,
                 sizeof (vlc_keys[0]), cmpkey);
    if (d)
        p += snprintf (p, psz_end - p, "%s", d->psz_key_string);
    else
        return NULL;

    return psz_key;
}
