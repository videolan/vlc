/*****************************************************************************
 * hotkeys.h: keycode defines
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: vlc_keys.h,v 1.4 2003/10/28 20:15:48 hartman Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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
#define KEY_MENU             0x00150000
#define KEY_ESC              0x00160000
#define KEY_PAGEUP           0x00170000
#define KEY_PAGEDOWN         0x00180000
#define KEY_TAB              0x00190000
#define KEY_BACKSPACE        0x001A0000

#define KEY_ASCII            0x0000007F
#define KEY_UNSET            0

typedef struct key_descriptor_s
{
    char *psz_key_string;
    int i_key_code;
} key_descriptor_t;

#define ADD_KEY(a) { a, *a }

static const struct key_descriptor_s modifiers[] =
{
    { "Alt", KEY_MODIFIER_ALT },
    { "Shift", KEY_MODIFIER_SHIFT },
    { "Ctrl", KEY_MODIFIER_CTRL },
    { "Meta", KEY_MODIFIER_META },
    { "Command", KEY_MODIFIER_COMMAND }
};

static const struct key_descriptor_s keys[] =
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
    { "Menu", KEY_MENU },
    { "Esc", KEY_ESC },
    { "Page Up", KEY_PAGEUP },
    { "Page Down", KEY_PAGEDOWN },
    { "Tab", KEY_TAB },
    { "Backspace", KEY_BACKSPACE },
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
    for ( i = 0; i < sizeof(keys) / sizeof(key_descriptor_t); i++ )
    {
        if ( keys[i].i_key_code == i_key )
        {
            return keys[i].psz_key_string;
        }
    }
    return NULL;
}

