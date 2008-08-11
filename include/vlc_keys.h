/*****************************************************************************
 * vlc_keys.h: keycode defines
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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

#ifndef VLC_KEYS_H
#define VLC_KEYS_H 1

/**
 * \file
 * This file defines keys, functions and structures for hotkey handling in vlc
 *
 */

#define KEY_MODIFIER         0xFF000000
#define KEY_MODIFIER_ALT     0x01000000
#define KEY_MODIFIER_SHIFT   0x02000000
#define KEY_MODIFIER_CTRL    0x04000000
#define KEY_MODIFIER_META    0x08000000
#define KEY_MODIFIER_COMMAND 0x10000000

#define KEY_SPECIAL          0x00FF0000
#define KEY_LEFT             0x00010000
#define KEY_RIGHT            0x00020000
#define KEY_UP               0x00030000
#define KEY_DOWN             0x00040000
#define KEY_SPACE            0x00050000
#define KEY_ENTER            0x00060000
#define KEY_F1               0x00070000
#define KEY_F2               0x00080000
#define KEY_F3               0x00090000
#define KEY_F4               0x000A0000
#define KEY_F5               0x000B0000
#define KEY_F6               0x000C0000
#define KEY_F7               0x000D0000
#define KEY_F8               0x000E0000
#define KEY_F9               0x000F0000
#define KEY_F10              0x00100000
#define KEY_F11              0x00110000
#define KEY_F12              0x00120000
#define KEY_HOME             0x00130000
#define KEY_END              0x00140000
#define KEY_INSERT           0x00150000
#define KEY_DELETE           0x00160000
#define KEY_MENU             0x00170000
#define KEY_ESC              0x00180000
#define KEY_PAGEUP           0x00190000
#define KEY_PAGEDOWN         0x001A0000
#define KEY_TAB              0x001B0000
#define KEY_BACKSPACE        0x001C0000
#define KEY_MOUSEWHEELUP     0x001D0000
#define KEY_MOUSEWHEELDOWN   0x001E0000
#define KEY_MOUSEWHEELLEFT   0x001F0000
#define KEY_MOUSEWHEELRIGHT  0x00200000

/* TODO:
 * The media keys are only used in win32. Support for other OSes needs to
 * be added */
#define KEY_BROWSER_BACK     0x001F0000
#define KEY_BROWSER_FORWARD  0x00200000
#define KEY_BROWSER_REFRESH  0x00210000
#define KEY_BROWSER_STOP     0x00220000
#define KEY_BROWSER_SEARCH   0x00230000
#define KEY_BROWSER_FAVORITES 0x00240000
#define KEY_BROWSER_HOME     0x00250000
#define KEY_VOLUME_MUTE      0x00260000
#define KEY_VOLUME_DOWN      0x00270000
#define KEY_VOLUME_UP        0x00280000
#define KEY_MEDIA_NEXT_TRACK 0x00290000
#define KEY_MEDIA_PREV_TRACK 0x002a0000
#define KEY_MEDIA_STOP       0x002b0000
#define KEY_MEDIA_PLAY_PAUSE 0x002c0000

#define KEY_ASCII            0x0000007F
#define KEY_UNSET            0

typedef struct key_descriptor_s
{
    const char *psz_key_string;
    int i_key_code;
} key_descriptor_t;

#define ADD_KEY(a) { a, *a }

static const struct key_descriptor_s vlc_modifiers[] =
{
    { "Alt", KEY_MODIFIER_ALT },
    { "Shift", KEY_MODIFIER_SHIFT },
    { "Ctrl", KEY_MODIFIER_CTRL },
    { "Meta", KEY_MODIFIER_META },
    { "Command", KEY_MODIFIER_COMMAND }
};

static const struct key_descriptor_s vlc_keys[] =
{
    { "Unset", KEY_UNSET },
    { "Left", KEY_LEFT },
    { "Right", KEY_RIGHT },
    { "Up", KEY_UP },
    { "Down", KEY_DOWN },
    { "Space", KEY_SPACE },
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
    { "Mouse Wheel Up", KEY_MOUSEWHEELUP },
    { "Mouse Wheel Down", KEY_MOUSEWHEELDOWN },
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
    { "+", '+' },
    { "=", '=' },
    { "-", '-' },
    { ",", ',' },
    { ".", '.' },
    { "<", '<' },
    { ">", '>' },
    { "`", '`' },
    { "/", '/' },
    { ";", ';' },
    { "'", '\'' },
    { "\\", '\\' },
    { "[", '[' },
    { "]", ']' },
    { "*", '*' },
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
    { "Media Play Pause", KEY_MEDIA_PLAY_PAUSE }
};

static inline const char *KeyToString( int i_key )
{
    unsigned int i = 0;
    for ( i = 0; i < sizeof(vlc_keys) / sizeof(key_descriptor_t); i++ )
    {
        if ( vlc_keys[i].i_key_code == i_key )
        {
            return vlc_keys[i].psz_key_string;
        }
    }
    return NULL;
}

static inline int StringToKey( char *psz_key )
{
    unsigned int i = 0;
    for ( i = 0; i < sizeof(vlc_keys) / sizeof(key_descriptor_t); i++ )
    {
        if ( !strcmp( vlc_keys[i].psz_key_string, psz_key ))
        {
            return vlc_keys[i].i_key_code;
        }
    }
    return 0;
}

typedef enum vlc_key {
    ACTIONID_QUIT = 1,
    ACTIONID_PLAY_PAUSE,
    ACTIONID_PLAY,
    ACTIONID_PAUSE,
    ACTIONID_STOP,
    ACTIONID_PREV,
    ACTIONID_NEXT,
    ACTIONID_SLOWER,
    ACTIONID_FASTER,
    ACTIONID_TOGGLE_FULLSCREEN,
    ACTIONID_VOL_UP,
    ACTIONID_VOL_DOWN,
    ACTIONID_NAV_ACTIVATE,
    ACTIONID_NAV_UP,
    ACTIONID_NAV_DOWN,
    ACTIONID_NAV_LEFT,
    ACTIONID_NAV_RIGHT,
    ACTIONID_JUMP_BACKWARD_EXTRASHORT,
    ACTIONID_JUMP_FORWARD_EXTRASHORT,
    ACTIONID_JUMP_BACKWARD_SHORT,
    ACTIONID_JUMP_FORWARD_SHORT,
    ACTIONID_JUMP_BACKWARD_MEDIUM,
    ACTIONID_JUMP_FORWARD_MEDIUM,
    ACTIONID_JUMP_BACKWARD_LONG,
    ACTIONID_JUMP_FORWARD_LONG,
    ACTIONID_POSITION,
    ACTIONID_VOL_MUTE,
/* let ACTIONID_SET_BOOMARK* and ACTIONID_PLAY_BOOKMARK* be contiguous */
    ACTIONID_SET_BOOKMARK1,
    ACTIONID_SET_BOOKMARK2,
    ACTIONID_SET_BOOKMARK3,
    ACTIONID_SET_BOOKMARK4,
    ACTIONID_SET_BOOKMARK5,
    ACTIONID_SET_BOOKMARK6,
    ACTIONID_SET_BOOKMARK7,
    ACTIONID_SET_BOOKMARK8,
    ACTIONID_SET_BOOKMARK9,
    ACTIONID_SET_BOOKMARK10,
    ACTIONID_PLAY_BOOKMARK1,
    ACTIONID_PLAY_BOOKMARK2,
    ACTIONID_PLAY_BOOKMARK3,
    ACTIONID_PLAY_BOOKMARK4,
    ACTIONID_PLAY_BOOKMARK5,
    ACTIONID_PLAY_BOOKMARK6,
    ACTIONID_PLAY_BOOKMARK7,
    ACTIONID_PLAY_BOOKMARK8,
    ACTIONID_PLAY_BOOKMARK9,
    ACTIONID_PLAY_BOOKMARK10,
    /* end of contiguous zone */
    ACTIONID_SUBDELAY_UP,
    ACTIONID_SUBDELAY_DOWN,
    ACTIONID_HISTORY_BACK,
    ACTIONID_HISTORY_FORWARD,
    ACTIONID_AUDIO_TRACK,
    ACTIONID_SUBTITLE_TRACK,
    ACTIONID_CUBESPEED_UP,
    ACTIONID_CUBESPEED_DOWN,
    ACTIONID_INTF_SHOW,
    ACTIONID_INTF_HIDE,
    /* chapter and title navigation */
    ACTIONID_TITLE_PREV,
    ACTIONID_TITLE_NEXT,
    ACTIONID_CHAPTER_PREV,
    ACTIONID_CHAPTER_NEXT,
    /* end of chapter and title navigation */
    ACTIONID_AUDIODELAY_UP,
    ACTIONID_AUDIODELAY_DOWN,
    ACTIONID_SNAPSHOT,
    ACTIONID_RECORD,
    ACTIONID_DISC_MENU,
    ACTIONID_ASPECT_RATIO,
    ACTIONID_CROP,
    ACTIONID_DEINTERLACE,
    ACTIONID_ZOOM,
    ACTIONID_UNZOOM,
    ACTIONID_CROP_TOP,
    ACTIONID_UNCROP_TOP,
    ACTIONID_CROP_LEFT,
    ACTIONID_UNCROP_LEFT,
    ACTIONID_CROP_BOTTOM,
    ACTIONID_UNCROP_BOTTOM,
    ACTIONID_CROP_RIGHT,
    ACTIONID_UNCROP_RIGHT,
    ACTIONID_DUMP,
    ACTIONID_RANDOM,
    ACTIONID_LOOP,
    ACTIONID_WALLPAPER,
    ACTIONID_LEAVE_FULLSCREEN,
    ACTIONID_MENU_ON,
    ACTIONID_MENU_OFF,
    ACTIONID_MENU_RIGHT,
    ACTIONID_MENU_LEFT,
    ACTIONID_MENU_UP,
    ACTIONID_MENU_DOWN,
    ACTIONID_MENU_SELECT,
    /* Zoom */
    ACTIONID_ZOOM_QUARTER,
    ACTIONID_ZOOM_HALF,
    ACTIONID_ZOOM_ORIGINAL,
    ACTIONID_ZOOM_DOUBLE,
    /* Cycle Through Audio Devices */
    ACTIONID_AUDIODEVICE_CYCLE
} vlc_key_t;
#endif
