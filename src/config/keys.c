/*****************************************************************************
 * keys.c: keys configuration
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

/**
 * \file
 * This file defines functions and structures for hotkey handling in vlc
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <limits.h>
#ifdef HAVE_SEARCH_H
# include <search.h>
#endif
#include <errno.h>

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
{   /* Alphabetical order */
    { N_("Backspace"),         KEY_BACKSPACE         },
    { N_("Brightness Down"),   KEY_BRIGHTNESS_DOWN   },
    { N_("Brightness Up"),     KEY_BRIGHTNESS_UP     },
    { N_("Browser Back"),      KEY_BROWSER_BACK      },
    { N_("Browser Favorites"), KEY_BROWSER_FAVORITES },
    { N_("Browser Forward"),   KEY_BROWSER_FORWARD   },
    { N_("Browser Home"),      KEY_BROWSER_HOME      },
    { N_("Browser Refresh"),   KEY_BROWSER_REFRESH   },
    { N_("Browser Search"),    KEY_BROWSER_SEARCH    },
    { N_("Browser Stop"),      KEY_BROWSER_STOP      },
    { N_("Delete"),            KEY_DELETE            },
    { N_("Down"),              KEY_DOWN              },
    { N_("End"),               KEY_END               },
    { N_("Enter"),             KEY_ENTER             },
    { N_("Esc"),               KEY_ESC               },
    { N_("F1"),                KEY_F1                },
    { N_("F10"),               KEY_F10               },
    { N_("F11"),               KEY_F11               },
    { N_("F12"),               KEY_F12               },
    { N_("F2"),                KEY_F2                },
    { N_("F3"),                KEY_F3                },
    { N_("F4"),                KEY_F4                },
    { N_("F5"),                KEY_F5                },
    { N_("F6"),                KEY_F6                },
    { N_("F7"),                KEY_F7                },
    { N_("F8"),                KEY_F8                },
    { N_("F9"),                KEY_F9                },
    { N_("Home"),              KEY_HOME              },
    { N_("Insert"),            KEY_INSERT            },
    { N_("Left"),              KEY_LEFT              },
    { N_("Media Angle"),       KEY_MEDIA_ANGLE       },
    { N_("Media Audio Track"), KEY_MEDIA_AUDIO       },
    { N_("Media Forward"),     KEY_MEDIA_FORWARD     },
    { N_("Media Menu"),        KEY_MEDIA_MENU        },
    { N_("Media Next Frame"),  KEY_MEDIA_FRAME_NEXT  },
    { N_("Media Next Track"),  KEY_MEDIA_NEXT_TRACK  },
    { N_("Media Play Pause"),  KEY_MEDIA_PLAY_PAUSE  },
    { N_("Media Prev Frame"),  KEY_MEDIA_FRAME_PREV  },
    { N_("Media Prev Track"),  KEY_MEDIA_PREV_TRACK  },
    { N_("Media Record"),      KEY_MEDIA_RECORD      },
    { N_("Media Repeat"),      KEY_MEDIA_REPEAT      },
    { N_("Media Rewind"),      KEY_MEDIA_REWIND      },
    { N_("Media Select"),      KEY_MEDIA_SELECT      },
    { N_("Media Shuffle"),     KEY_MEDIA_SHUFFLE     },
    { N_("Media Stop"),        KEY_MEDIA_STOP        },
    { N_("Media Subtitle"),    KEY_MEDIA_SUBTITLE    },
    { N_("Media Time"),        KEY_MEDIA_TIME        },
    { N_("Media View"),        KEY_MEDIA_VIEW        },
    { N_("Menu"),              KEY_MENU              },
    { N_("Mouse Wheel Down"),  KEY_MOUSEWHEELDOWN    },
    { N_("Mouse Wheel Left"),  KEY_MOUSEWHEELLEFT    },
    { N_("Mouse Wheel Right"), KEY_MOUSEWHEELRIGHT   },
    { N_("Mouse Wheel Up"),    KEY_MOUSEWHEELUP      },
    { N_("Page Down"),         KEY_PAGEDOWN          },
    { N_("Page Up"),           KEY_PAGEUP            },
    { N_("Right"),             KEY_RIGHT             },
    { N_("Space"),             ' '                   },
    { N_("Tab"),               KEY_TAB               },
    { N_("Unset"),             KEY_UNSET             },
    { N_("Up"),                KEY_UP                },
    { N_("Volume Down"),       KEY_VOLUME_DOWN       },
    { N_("Volume Mute"),       KEY_VOLUME_MUTE       },
    { N_("Volume Up"),         KEY_VOLUME_UP         },
    { N_("Zoom In"),           KEY_ZOOM_IN           },
    { N_("Zoom Out"),          KEY_ZOOM_OUT          },
};
#define KEYS_COUNT (sizeof(vlc_keys)/sizeof(vlc_keys[0]))

static int keystrcmp (const void *key, const void *elem)
{
    const char *sa = key, *sb = elem;
    return strcmp (sa, sb);
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

/**
 * Parse a human-readable string representation of a VLC key code.
 * @note This only works with the American English representation
 * (a.k.a. C or POSIX), not with the local representation returned from
 * vlc_keycode2str().
 * @return a VLC key code, or KEY_UNSET on failure.
 */
uint_fast32_t vlc_str2keycode (const char *name)
{
    uint_fast32_t mods = 0;
    uint32_t code;

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

    key_descriptor_t *d = bsearch (name, vlc_keys, KEYS_COUNT,
                                   sizeof (vlc_keys[0]), keystrcmp);
    if (d != NULL)
        code = d->i_key_code;
    else
    if (vlc_towc (name, &code) <= 0)
        code = KEY_UNSET;

    if (code != KEY_UNSET)
        code |= mods;
    return code;
}

static char *nooptext (const char *txt)
{
    return (char *)txt;
}

/**
 * Format a human-readable and unique representation of a VLC key code
 * (including modifiers).
 * @param code key code to translate to a string
 * @param locale true to get a localized string,
 *               false to get a C string suitable for 'vlcrc'
 * @return a heap-allocated string, or NULL on error.
 */
char *vlc_keycode2str (uint_fast32_t code, bool locale)
{
    char *(*tr) (const char *) = locale ? vlc_gettext : nooptext;
    const char *name;
    char *str, buf[5];
    uintptr_t key = code & ~KEY_MODIFIER;

    for (size_t i = 0; i < KEYS_COUNT; i++)
        if (vlc_keys[i].i_key_code == key)
        {
            name = vlc_keys[i].psz_key_string;
            goto found;
        }

    if (utf8_cp (key, buf) == NULL)
        return NULL;
    name = buf;

found:
    if (asprintf (&str, "%s%s%s%s%s%s",
                  (code & KEY_MODIFIER_CTRL) ? tr(N_("Ctrl+")) : "",
                  (code & KEY_MODIFIER_ALT) ? tr(N_("Alt+")) : "",
                  (code & KEY_MODIFIER_SHIFT) ? tr(N_("Shift+")) : "",
                  (code & KEY_MODIFIER_META) ? tr(N_("Meta+")) : "",
                  (code & KEY_MODIFIER_COMMAND) ? tr(N_("Command+")) : "",
                  tr(name)) == -1)
        return NULL;
    return str;
}


/*** VLC key map ***/

#define MAXACTION 20
struct action
{
    char name[MAXACTION];
    vlc_action_t value;
};

static const struct action actions[] =
{
    /* *MUST* be sorted (ASCII order) */
    { "aspect-ratio", ACTIONID_ASPECT_RATIO, },
    { "audio-track", ACTIONID_AUDIO_TRACK, },
    { "audiodelay-down", ACTIONID_AUDIODELAY_DOWN, },
    { "audiodelay-up", ACTIONID_AUDIODELAY_UP, },
    { "audiodevice-cycle", ACTIONID_AUDIODEVICE_CYCLE, },
    { "chapter-next", ACTIONID_CHAPTER_NEXT, },
    { "chapter-prev", ACTIONID_CHAPTER_PREV, },
    { "clear-playlist", ACTIONID_PLAY_CLEAR, },
    { "crop", ACTIONID_CROP, },
    { "crop-bottom", ACTIONID_CROP_BOTTOM, },
    { "crop-left", ACTIONID_CROP_LEFT, },
    { "crop-right", ACTIONID_CROP_RIGHT, },
    { "crop-top", ACTIONID_CROP_TOP, },
    { "decr-scalefactor", ACTIONID_SCALE_DOWN, },
    { "deinterlace", ACTIONID_DEINTERLACE, },
    { "deinterlace-mode", ACTIONID_DEINTERLACE_MODE, },
    { "disc-menu", ACTIONID_DISC_MENU, },
    { "faster", ACTIONID_FASTER, },
    { "frame-next", ACTIONID_FRAME_NEXT, },
    { "incr-scalefactor", ACTIONID_SCALE_UP, },
    { "intf-boss", ACTIONID_INTF_BOSS, },
    { "intf-popup-menu", ACTIONID_INTF_POPUP_MENU, },
    { "intf-show", ACTIONID_INTF_TOGGLE_FSC, },
    { "jump+extrashort", ACTIONID_JUMP_FORWARD_EXTRASHORT, },
    { "jump+long", ACTIONID_JUMP_FORWARD_LONG, },
    { "jump+medium", ACTIONID_JUMP_FORWARD_MEDIUM, },
    { "jump+short", ACTIONID_JUMP_FORWARD_SHORT, },
    { "jump-extrashort", ACTIONID_JUMP_BACKWARD_EXTRASHORT, },
    { "jump-long", ACTIONID_JUMP_BACKWARD_LONG, },
    { "jump-medium", ACTIONID_JUMP_BACKWARD_MEDIUM, },
    { "jump-short", ACTIONID_JUMP_BACKWARD_SHORT, },
    { "leave-fullscreen", ACTIONID_LEAVE_FULLSCREEN, },
    { "loop", ACTIONID_LOOP, },
    { "nav-activate", ACTIONID_NAV_ACTIVATE, },
    { "nav-down", ACTIONID_NAV_DOWN, },
    { "nav-left", ACTIONID_NAV_LEFT, },
    { "nav-right", ACTIONID_NAV_RIGHT, },
    { "nav-up", ACTIONID_NAV_UP, },
    { "next", ACTIONID_NEXT, },
    { "pause", ACTIONID_PAUSE, },
    { "play", ACTIONID_PLAY, },
    { "play-bookmark1", ACTIONID_PLAY_BOOKMARK1, },
    { "play-bookmark10", ACTIONID_PLAY_BOOKMARK10, },
    { "play-bookmark2", ACTIONID_PLAY_BOOKMARK2, },
    { "play-bookmark3", ACTIONID_PLAY_BOOKMARK3, },
    { "play-bookmark4", ACTIONID_PLAY_BOOKMARK4, },
    { "play-bookmark5", ACTIONID_PLAY_BOOKMARK5, },
    { "play-bookmark6", ACTIONID_PLAY_BOOKMARK6, },
    { "play-bookmark7", ACTIONID_PLAY_BOOKMARK7, },
    { "play-bookmark8", ACTIONID_PLAY_BOOKMARK8, },
    { "play-bookmark9", ACTIONID_PLAY_BOOKMARK9, },
    { "play-pause", ACTIONID_PLAY_PAUSE, },
    { "position", ACTIONID_POSITION, },
    { "prev", ACTIONID_PREV, },
    { "program-sid-next", ACTIONID_PROGRAM_SID_NEXT, },
    { "program-sid-prev", ACTIONID_PROGRAM_SID_PREV, },
    { "quit", ACTIONID_QUIT, },
    { "random", ACTIONID_RANDOM, },
    { "rate-faster-fine", ACTIONID_RATE_FASTER_FINE, },
    { "rate-normal", ACTIONID_RATE_NORMAL, },
    { "rate-slower-fine", ACTIONID_RATE_SLOWER_FINE, },
    { "record", ACTIONID_RECORD, },
    { "set-bookmark1", ACTIONID_SET_BOOKMARK1, },
    { "set-bookmark10", ACTIONID_SET_BOOKMARK10, },
    { "set-bookmark2", ACTIONID_SET_BOOKMARK2, },
    { "set-bookmark3", ACTIONID_SET_BOOKMARK3, },
    { "set-bookmark4", ACTIONID_SET_BOOKMARK4, },
    { "set-bookmark5", ACTIONID_SET_BOOKMARK5, },
    { "set-bookmark6", ACTIONID_SET_BOOKMARK6, },
    { "set-bookmark7", ACTIONID_SET_BOOKMARK7, },
    { "set-bookmark8", ACTIONID_SET_BOOKMARK8, },
    { "set-bookmark9", ACTIONID_SET_BOOKMARK9, },
    { "slower", ACTIONID_SLOWER, },
    { "snapshot", ACTIONID_SNAPSHOT, },
    { "stop", ACTIONID_STOP, },
    { "subdelay-down", ACTIONID_SUBDELAY_DOWN, },
    { "subdelay-up", ACTIONID_SUBDELAY_UP, },
    { "subpos-down", ACTIONID_SUBPOS_DOWN, },
    { "subpos-up", ACTIONID_SUBPOS_UP, },
    { "subsync-apply", ACTIONID_SUBSYNC_APPLY, },
    { "subsync-markaudio", ACTIONID_SUBSYNC_MARKAUDIO, },
    { "subsync-marksub", ACTIONID_SUBSYNC_MARKSUB, },
    { "subsync-reset", ACTIONID_SUBSYNC_RESET, },
    { "subtitle-track", ACTIONID_SUBTITLE_TRACK, },
    { "title-next", ACTIONID_TITLE_NEXT, },
    { "title-prev", ACTIONID_TITLE_PREV, },
    { "toggle-autoscale", ACTIONID_TOGGLE_AUTOSCALE, },
    { "toggle-fullscreen", ACTIONID_TOGGLE_FULLSCREEN, },
    { "uncrop-bottom", ACTIONID_UNCROP_BOTTOM, },
    { "uncrop-left", ACTIONID_UNCROP_LEFT, },
    { "uncrop-right", ACTIONID_UNCROP_RIGHT, },
    { "uncrop-top", ACTIONID_UNCROP_TOP, },
    { "unzoom", ACTIONID_UNZOOM, },
    { "vol-down", ACTIONID_VOL_DOWN, },
    { "vol-mute", ACTIONID_VOL_MUTE, },
    { "vol-up", ACTIONID_VOL_UP, },
    { "wallpaper", ACTIONID_WALLPAPER, },
    { "zoom", ACTIONID_ZOOM, },
    { "zoom-double", ACTIONID_ZOOM_DOUBLE, },
    { "zoom-half", ACTIONID_ZOOM_HALF, },
    { "zoom-original", ACTIONID_ZOOM_ORIGINAL, },
    { "zoom-quarter", ACTIONID_ZOOM_QUARTER, },
};
#define ACTIONS_COUNT (sizeof (actions) / sizeof (actions[0]))

struct mapping
{
    uint32_t     key; ///< Key code
    vlc_action_t action; ///< Action ID
};

static int keycmp (const void *a, const void *b)
{
    const struct mapping *ka = a, *kb = b;

#if (INT_MAX >= 0x7fffffff)
    return ka->key - kb->key;
#else
    return (ka->key < kb->key) ? -1 : (ka->key > kb->key) ? +1 : 0;
#endif
}

struct vlc_actions
{
    void *map; /* Key map */
    void *global_map; /* Grabbed/global key map */
    struct hotkey keys[0];
};

static int vlc_key_to_action (vlc_object_t *obj, const char *varname,
                              vlc_value_t prevkey, vlc_value_t curkey, void *d)
{
    void *const *map = d;
    const struct mapping **pent;
    uint32_t keycode = curkey.i_int;

    pent = tfind (&keycode, map, keycmp);
    if (pent == NULL)
        return VLC_SUCCESS;

    (void) varname;
    (void) prevkey;
    return var_SetInteger (obj, "key-action", (*pent)->action);
}

/**
 * Adds a mapping from a certain key code to a certain action.
 */
static int vlc_AddMapping (void **map, uint32_t keycode, vlc_action_t action)
{
    struct mapping *entry = malloc (sizeof (*entry));
    if (entry == NULL)
        return ENOMEM;
    entry->key = keycode;
    entry->action = action;

    struct mapping **pent = tsearch (entry, map, keycmp);
    if (unlikely(pent == NULL))
        return ENOMEM;
    if (*pent != entry)
    {
        free (entry);
        return EEXIST;
    }
    return 0;
}

/**
 * Sets up all key mappings for a given action.
 * \param map tree (of struct mapping entries) to write mappings to
 * \param confname VLC configuration item to read mappings from
 * \param action action ID
 */
static void vlc_InitAction (vlc_object_t *obj, void **map,
                            const char *confname, vlc_action_t action)
{
    char *keys = var_InheritString (obj, confname);
    if (keys == NULL)
        return;

    for (char *buf, *key = strtok_r (keys, "\t", &buf);
         key != NULL;
         key = strtok_r (NULL, "\t", &buf))
    {
        uint32_t code = vlc_str2keycode (key);
        if (code == KEY_UNSET)
        {
            msg_Warn (obj, "Key \"%s\" unrecognized", key);
            continue;
        }

        if (vlc_AddMapping (map, code, action) == EEXIST)
            msg_Warn (obj, "Key \"%s\" bound to multiple actions", key);
    }
    free (keys);
}

/**
 * Initializes the key map from configuration.
 */
struct vlc_actions *vlc_InitActions (libvlc_int_t *libvlc)
{
    vlc_object_t *obj = VLC_OBJECT(libvlc);
    struct hotkey *keys;
    struct vlc_actions *as = malloc (sizeof (*as) + (ACTIONS_COUNT + 1) * sizeof (*keys));

    if (unlikely(as == NULL))
        return NULL;
    as->map = NULL;
    as->global_map = NULL;
    keys = as->keys;

    var_Create (obj, "key-pressed", VLC_VAR_INTEGER);
    var_Create (obj, "global-key-pressed", VLC_VAR_INTEGER);
    var_Create (obj, "key-action", VLC_VAR_INTEGER);

    /* Initialize from configuration */
    for (size_t i = 0; i < ACTIONS_COUNT; i++)
    {
#ifndef NDEBUG
        if (i > 0
         && strcmp (actions[i-1].name, actions[i].name) >= 0)
        {
            msg_Err (libvlc, "key-%s and key-%s are not ordered properly",
                     actions[i-1].name, actions[i].name);
            abort ();
        }
#endif
        keys->psz_action = actions[i].name;
        keys++;

        char name[12 + MAXACTION];

        snprintf (name, sizeof (name), "global-key-%s", actions[i].name);
        vlc_InitAction (obj, &as->map, name + 7, actions[i].value);
        vlc_InitAction (obj, &as->global_map, name, actions[i].value);
    }
    keys->psz_action = NULL;

    /* Initialize mouse wheel events */
    int mousemode = var_InheritInteger (obj, "hotkeys-mousewheel-mode");
    if (mousemode < 2)
    {
        vlc_AddMapping (&as->map,
                        mousemode ? KEY_MOUSEWHEELRIGHT : KEY_MOUSEWHEELUP,
                        ACTIONID_VOL_UP);
        vlc_AddMapping (&as->map,
                        mousemode ? KEY_MOUSEWHEELLEFT : KEY_MOUSEWHEELDOWN,
                        ACTIONID_VOL_DOWN);
        vlc_AddMapping (&as->map,
                        mousemode ? KEY_MOUSEWHEELUP : KEY_MOUSEWHEELRIGHT,
                        ACTIONID_JUMP_FORWARD_EXTRASHORT);
        vlc_AddMapping (&as->map,
                        mousemode ? KEY_MOUSEWHEELDOWN : KEY_MOUSEWHEELLEFT,
                        ACTIONID_JUMP_BACKWARD_EXTRASHORT);
    }


    libvlc->p_hotkeys = as->keys;
    var_AddCallback (obj, "key-pressed", vlc_key_to_action, &as->map);
    var_AddCallback (obj, "global-key-pressed", vlc_key_to_action,
                     &as->global_map);
    return as;
}

/**
 * Destroys the key map.
 */
void vlc_DeinitActions (libvlc_int_t *libvlc, struct vlc_actions *as)
{
    if (unlikely(as == NULL))
        return;

    var_DelCallback (libvlc, "global-key-pressed", vlc_key_to_action,
                     &as->global_map);
    var_DelCallback (libvlc, "key-pressed", vlc_key_to_action, &as->map);

    tdestroy (as->global_map, free);
    tdestroy (as->map, free);
    free (as);
    libvlc->p_hotkeys = NULL;
}


static int actcmp(const void *key, const void *ent)
{
    const struct action *act = ent;
    return strcmp(key, act->name);
}

/**
 * Get the action ID from the action name in the configuration subsystem.
 * @return the action ID or ACTIONID_NONE on error.
 */
vlc_action_t vlc_GetActionId (const char *name)
{
    const struct action *act;

    if (strncmp (name, "key-", 4))
        return ACTIONID_NONE;
    name += 4;

    act = bsearch(name, actions, ACTIONS_COUNT, sizeof(*act), actcmp);
    return (act != NULL) ? act->value : ACTIONID_NONE;
}
