/**
 * @file keys.c
 * @brief X C Bindings VLC keyboard event handling
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2.0
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include <vlc_common.h>
#include <vlc_keys.h>

#include "xcb_vlc.h"

struct key_handler_t
{
    vlc_object_t      *obj;
    xcb_key_symbols_t *syms;
};

/**
 * Create an X11 key event handler for a VLC window.
 * The caller shall arrange receiving applicable X11 events, and pass them to
 * ProcessKeyEvent() later.
 *
 * @param obj VLC object owning an X window
 * @param conn XCB connection to the X server (to fetch key mappings)
 * @return NULL on error, or a key handling context.
 */
key_handler_t *CreateKeyHandler (vlc_object_t *obj, xcb_connection_t *conn)
{
    key_handler_t *ctx = malloc (sizeof (*ctx));
    if (!ctx)
        return NULL;

    ctx->obj = obj;
    ctx->syms = xcb_key_symbols_alloc (conn);
    return ctx;
}

void DestroyKeyHandler (key_handler_t *ctx)
{
    xcb_key_symbols_free (ctx->syms);
    free (ctx);
}

static int keysymcmp (const void *pa, const void *pb)
{
    int a = *(const xcb_keysym_t *)pa;
    int b = *(const xcb_keysym_t *)pb;

    return a - b;
}

static int ConvertKeySym (xcb_keysym_t sym)
{
    static const struct
    {
        xcb_keysym_t x11;
        uint32_t vlc;
    } *res, tab[] = {
    /* This list MUST be in XK_* incremental order (see keysymdef.h),
     * so that binary search works.
     * Multiple X keys can match the same VLC key.
     * X key symbols must be in the first column of the struct. */
        { XK_BackSpace,     KEY_BACKSPACE, },
        { XK_Tab,           KEY_TAB, },
        { XK_Return,        KEY_ENTER, },
        { XK_Escape,        KEY_ESC, },
        { XK_Home,          KEY_HOME, },
        { XK_Left,          KEY_LEFT, },
        { XK_Up,            KEY_UP, },
        { XK_Right,         KEY_RIGHT, },
        { XK_Down,          KEY_DOWN, },
        { XK_Page_Up,       KEY_PAGEUP, },
        { XK_Page_Down,     KEY_PAGEDOWN, },
        { XK_End,           KEY_END, },
        { XK_Begin,         KEY_HOME, },
        { XK_Insert,        KEY_INSERT, },
        { XK_Menu,          KEY_MENU },
        { XK_KP_Space,      KEY_SPACE, },
        { XK_KP_Tab,        KEY_TAB, },
        { XK_KP_Enter,      KEY_ENTER, },
        { XK_KP_F1,         KEY_F1, },
        { XK_KP_F2,         KEY_F2, },
        { XK_KP_F3,         KEY_F3, },
        { XK_KP_F4,         KEY_F4, },
        { XK_KP_Home,       KEY_HOME, },
        { XK_KP_Left,       KEY_LEFT, },
        { XK_KP_Up,         KEY_UP, },
        { XK_KP_Right,      KEY_RIGHT, },
        { XK_KP_Down,       KEY_DOWN, },
        { XK_KP_Page_Up,    KEY_PAGEUP, },
        { XK_KP_Page_Down,  KEY_PAGEDOWN, },
        { XK_KP_End,        KEY_END, },
        { XK_KP_Begin,      KEY_HOME, },
        { XK_KP_Insert,     KEY_INSERT, },
        { XK_KP_Delete,     KEY_DELETE, },
        { XK_F1,            KEY_F1, },
        { XK_F2,            KEY_F2, },
        { XK_F3,            KEY_F3, },
        { XK_F4,            KEY_F4, },
        { XK_F5,            KEY_F5, },
        { XK_F6,            KEY_F6, },
        { XK_F7,            KEY_F7, },
        { XK_F8,            KEY_F8, },
        { XK_F9,            KEY_F9, },
        { XK_F10,           KEY_F10, },
        { XK_F11,           KEY_F11, },
        { XK_F12,           KEY_F12, },
        { XK_Delete,        KEY_DELETE, },

        /* XFree86 extensions */
        { XF86XK_AudioLowerVolume, KEY_VOLUME_DOWN, },
        { XF86XK_AudioMute,        KEY_VOLUME_MUTE, },
        { XF86XK_AudioRaiseVolume, KEY_VOLUME_UP, },
        { XF86XK_AudioPlay,        KEY_MEDIA_PLAY_PAUSE, },
        { XF86XK_AudioStop,        KEY_MEDIA_STOP, },
        { XF86XK_AudioPrev,        KEY_MEDIA_PREV_TRACK, },
        { XF86XK_AudioNext,        KEY_MEDIA_NEXT_TRACK, },
        { XF86XK_HomePage,         KEY_BROWSER_HOME, },
        { XF86XK_Search,           KEY_BROWSER_SEARCH, },
        { XF86XK_Back,             KEY_BROWSER_BACK, },
        { XF86XK_Forward,          KEY_BROWSER_FORWARD, },
        { XF86XK_Stop,             KEY_BROWSER_STOP, },
        { XF86XK_Refresh,          KEY_BROWSER_REFRESH, },
        { XF86XK_Favorites,        KEY_BROWSER_FAVORITES, },
        { XF86XK_AudioPause,       KEY_MEDIA_PLAY_PAUSE, },
        { XF86XK_Reload,           KEY_BROWSER_REFRESH, },
    };

    /* X11 and VLC both use the ASCII code for printable ASCII characters,
     * except for space (only X11). */
    if (sym == XK_space)
        return KEY_SPACE;
    if (isascii(sym))
        return sym;

    /* Special keys */
    res = bsearch (&sym, tab, sizeof (tab) / sizeof (tab[0]), sizeof (tab[0]),
                   keysymcmp);
    if (res != NULL)
        return res->vlc;

    return KEY_UNSET;
}


/**
 * Process an X11 event, convert into VLC hotkey event if applicable.
 *
 * @param ctx key handler created with CreateKeyHandler()
 * @param ev XCB event to process
 * @return 0 if the event was handled and free()'d, non-zero otherwise
 */
int ProcessKeyEvent (key_handler_t *ctx, xcb_generic_event_t *ev)
{
    assert (ctx);

    switch (ev->response_type & 0x7f)
    {
        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
            xcb_keysym_t sym = xcb_key_press_lookup_keysym (ctx->syms, e, 0);
            int vk = ConvertKeySym (sym);

            if (vk == KEY_UNSET)
                break;
            if (e->state & XCB_MOD_MASK_SHIFT)
                vk |= KEY_MODIFIER_SHIFT;
            if (e->state & XCB_MOD_MASK_CONTROL)
                vk |= KEY_MODIFIER_CTRL;
            if (e->state & XCB_MOD_MASK_1)
                vk |= KEY_MODIFIER_ALT;
            if (e->state & XCB_MOD_MASK_4)
                vk |= KEY_MODIFIER_COMMAND;
            var_SetInteger (ctx->obj->p_libvlc, "key-pressed", vk);
            break;
        }

        case XCB_KEY_RELEASE:
            break;

        /*TODO: key mappings update*/
        default:
            return -1;
    }

    free (ev);
    return 0;
}
