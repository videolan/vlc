/**
 * @file xkb.c
 * @brief XKeyboard symbols mapping for VLC
 */
/*****************************************************************************
 * Copyright © 2009-2018 Rémi Denis-Courmont
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
#include <stdint.h>
#include <wctype.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <vlc_common.h>
#include <vlc_actions.h>
#include "video_output/xcb/vlc_xkb.h"

static int keysymcmp (const void *pa, const void *pb)
{
    int a = *(const uint32_t *)pa;
    int b = *(const uint32_t *)pb;

    return a - b;
}

static uint_fast32_t vlc_xkb_convert_keysym(uint_fast32_t sym)
{
    static const struct
    {
        uint32_t x11;
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

#ifdef HAVE_XKBCOMMON
# include <xkbcommon/xkbcommon.h>

struct modifiers
{
    char name[8];
    uint32_t mask;
};

static const struct modifiers modifiers[] =
{
    { XKB_MOD_NAME_SHIFT, KEY_MODIFIER_SHIFT },
    { XKB_MOD_NAME_CTRL,  KEY_MODIFIER_CTRL },
    { XKB_MOD_NAME_ALT,   KEY_MODIFIER_ALT },
    { XKB_MOD_NAME_LOGO,  KEY_MODIFIER_META },
};

uint_fast32_t vlc_xkb_get_one(struct xkb_state *state, uint_fast32_t keycode)
{
    xkb_keysym_t keysym = xkb_state_key_get_one_sym(state, keycode);
    uint_fast32_t vk = vlc_xkb_convert_keysym(keysym);

    if (vk != KEY_UNSET) {
        if (vk < 0x110000)
            vk = towlower(vk); /* Lower case for Unicode code points */

        for (size_t i = 0; i < ARRAY_SIZE(modifiers); i++)
            if (xkb_state_mod_name_is_active(state, modifiers[i].name,
                                             XKB_STATE_MODS_EFFECTIVE) > 0)
                vk |= modifiers[i].mask;
    }

    return vk;
}
#endif /* HAVE_XKBCOMMON */
