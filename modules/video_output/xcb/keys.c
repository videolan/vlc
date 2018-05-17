/**
 * @file keys.c
 * @brief X C Bindings VLC keyboard event handling
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
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

#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <vlc_common.h>
#include "events.h"

#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <vlc_actions.h>

static int keysymcmp (const void *pa, const void *pb)
{
    int a = *(const xcb_keysym_t *)pa;
    int b = *(const xcb_keysym_t *)pb;

    return a - b;
}

static uint_fast32_t ConvertKeySym (xcb_keysym_t sym)
{
    static const struct
    {
        xcb_keysym_t x11;
        uint32_t vlc;
    } *res, tab[] = {
#include "xcb_keysym.h"
    }, old[] = {
#include "keysym.h"
    };

    /* X11 Latin-1 range */
    if (sym <= 0xff)
        return sym;
    /* X11 Unicode range */
    if (sym >= 0x1000100 && sym <= 0x110ffff)
        return sym - 0x1000000;

#if 0
    for (size_t i = 0; i < sizeof (tab) / sizeof (tab[0]); i++)
        if (i > 0 && tab[i-1].x11 >= tab[i].x11)
        {
            fprintf (stderr, "key %x and %x are not ordered properly\n",
                     tab[i-1].x11, tab[i].x11);
            abort ();
        }
    for (size_t i = 0; i < sizeof (old) / sizeof (old[0]); i++)
        if (i > 0 && old[i-1].x11 >= old[i].x11)
        {
            fprintf (stderr, "key %x and %x are not ordered properly\n",
                     old[i-1].x11, old[i].x11);
            abort ();
        }
#endif

    /* Special keys */
    res = bsearch (&sym, tab, sizeof (tab) / sizeof (tab[0]), sizeof (tab[0]),
                   keysymcmp);
    if (res != NULL)
        return res->vlc;
    /* Legacy X11 symbols outside the Unicode range */
    res = bsearch (&sym, old, sizeof (old) / sizeof (old[0]), sizeof (old[0]),
                   keysymcmp);
    if (res != NULL)
        return res->vlc;

    return KEY_UNSET;
}


/**
 * Process an X11 event, convert into VLC hotkey event if applicable.
 *
 * @param syms XCB key symbols (created by xcb_key_symbols_alloc())
 * @param ev XCB event to process
 * @return 0 if the event was handled and free()'d, non-zero otherwise
 */
int XCB_keyHandler_Process(xcb_key_symbols_t *syms, xcb_generic_event_t *ev,
                           vout_window_t *window)
{
    assert(syms != NULL);

    switch (ev->response_type & 0x7f)
    {
        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
            xcb_keysym_t sym = xcb_key_press_lookup_keysym(syms, e, 0);
            uint_fast32_t vk = ConvertKeySym (sym);

            msg_Dbg(window, "key: 0x%08"PRIxFAST32" (X11: 0x%04"PRIx32")",
                    vk, sym);
            if (vk == KEY_UNSET)
                break;
            if (e->state & XCB_MOD_MASK_SHIFT) /* Shift */
                vk |= KEY_MODIFIER_SHIFT;
            /* XCB_MOD_MASK_LOCK */ /* Caps Lock */
            if (e->state & XCB_MOD_MASK_CONTROL) /* Control */
                vk |= KEY_MODIFIER_CTRL;
            if (e->state & XCB_MOD_MASK_1) /* Alternate */
                vk |= KEY_MODIFIER_ALT;
            /* XCB_MOD_MASK_2 */ /* Numeric Pad Lock */
            if (e->state & XCB_MOD_MASK_3) /* Super */
                vk |= KEY_MODIFIER_META;
            if (e->state & XCB_MOD_MASK_4) /* Meta */
                vk |= KEY_MODIFIER_META;
            if (e->state & XCB_MOD_MASK_5) /* Alternate Graphic */
                vk |= KEY_MODIFIER_ALT;
            vout_window_ReportKeyPress(window, vk);
            break;
        }

        case XCB_KEY_RELEASE:
            break;

        case XCB_MAPPING_NOTIFY:
        {
            xcb_mapping_notify_event_t *e = (xcb_mapping_notify_event_t *)ev;
            msg_Dbg(window, "refreshing keyboard mapping");
            xcb_refresh_keyboard_mapping(syms, e);
            break;
        }

        default:
            return -1;
    }

    free (ev);
    return 0;
}
