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

#define KEY_ASCII            0x0000007F
#define KEY_UNSET            0

typedef struct key_descriptor_s
{
    char *psz_key_string;
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
    { "*", '*' }
};

static inline char *KeyToString( int i_key )
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


#define ACTIONID_QUIT                  1
#define ACTIONID_PLAY_PAUSE            2
#define ACTIONID_PLAY                  3
#define ACTIONID_PAUSE                 4
#define ACTIONID_STOP                  5
#define ACTIONID_PREV                  6
#define ACTIONID_NEXT                  7
#define ACTIONID_SLOWER                8
#define ACTIONID_FASTER                9
#define ACTIONID_FULLSCREEN            10
#define ACTIONID_VOL_UP                11
#define ACTIONID_VOL_DOWN              12
#define ACTIONID_NAV_ACTIVATE          13
#define ACTIONID_NAV_UP                14
#define ACTIONID_NAV_DOWN              15
#define ACTIONID_NAV_LEFT              16
#define ACTIONID_NAV_RIGHT             17
#define ACTIONID_JUMP_BACKWARD_EXTRASHORT 18
#define ACTIONID_JUMP_FORWARD_EXTRASHORT  19
#define ACTIONID_JUMP_BACKWARD_SHORT   20
#define ACTIONID_JUMP_FORWARD_SHORT    21
#define ACTIONID_JUMP_BACKWARD_MEDIUM  22
#define ACTIONID_JUMP_FORWARD_MEDIUM   23
#define ACTIONID_JUMP_BACKWARD_LONG    24
#define ACTIONID_JUMP_FORWARD_LONG     25
#define ACTIONID_POSITION              26
#define ACTIONID_VOL_MUTE              27
/* let ACTIONID_SET_BOOMARK* and ACTIONID_PLAY_BOOKMARK* be contiguous */
#define ACTIONID_SET_BOOKMARK1         28
#define ACTIONID_SET_BOOKMARK2         29
#define ACTIONID_SET_BOOKMARK3         39
#define ACTIONID_SET_BOOKMARK4         31
#define ACTIONID_SET_BOOKMARK5         32
#define ACTIONID_SET_BOOKMARK6         33
#define ACTIONID_SET_BOOKMARK7         34
#define ACTIONID_SET_BOOKMARK8         35
#define ACTIONID_SET_BOOKMARK9         36
#define ACTIONID_SET_BOOKMARK10        37
#define ACTIONID_PLAY_BOOKMARK1        38
#define ACTIONID_PLAY_BOOKMARK2        39
#define ACTIONID_PLAY_BOOKMARK3        40
#define ACTIONID_PLAY_BOOKMARK4        41
#define ACTIONID_PLAY_BOOKMARK5        42
#define ACTIONID_PLAY_BOOKMARK6        43
#define ACTIONID_PLAY_BOOKMARK7        44
#define ACTIONID_PLAY_BOOKMARK8        45
#define ACTIONID_PLAY_BOOKMARK9        46
#define ACTIONID_PLAY_BOOKMARK10       47
/* end of contiguous zone */
#define ACTIONID_SUBDELAY_UP           48
#define ACTIONID_SUBDELAY_DOWN         49
#define ACTIONID_HISTORY_BACK          50
#define ACTIONID_HISTORY_FORWARD       51
#define ACTIONID_AUDIO_TRACK           52
#define ACTIONID_SUBTITLE_TRACK        53
#define ACTIONID_CUBESPEED_UP          54
#define ACTIONID_CUBESPEED_DOWN        55
#define ACTIONID_INTF_SHOW             56
#define ACTIONID_INTF_HIDE             57
/* chapter and title navigation */
#define ACTIONID_TITLE_PREV            58
#define ACTIONID_TITLE_NEXT            59
#define ACTIONID_CHAPTER_PREV          60
#define ACTIONID_CHAPTER_NEXT          61
/* end of chapter and title navigation */
#define ACTIONID_AUDIODELAY_UP         62
#define ACTIONID_AUDIODELAY_DOWN       63
#define ACTIONID_SNAPSHOT              64
#define ACTIONID_RECORD                65
#define ACTIONID_DISC_MENU             66
#define ACTIONID_ASPECT_RATIO          67
#define ACTIONID_CROP                  68
#define ACTIONID_DEINTERLACE           69
#define ACTIONID_ZOOM                  70
#define ACTIONID_UNZOOM                71
