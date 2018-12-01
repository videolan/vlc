/**
 * @file wdummy.c
 * @brief Dummy video window provider for legacy video plugins
 */
/*****************************************************************************
 * Copyright © 2009, 2018 Rémi Denis-Courmont
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

#include <stdarg.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

static int Control(vout_window_t *wnd, int query, va_list ap)
{
    switch (query)
    {
        case VOUT_WINDOW_SET_SIZE:
        {
            unsigned width = va_arg(ap, unsigned);
            unsigned height = va_arg(ap, unsigned);

            vout_window_ReportSize(wnd, width, height);
            return VLC_SUCCESS;
        }

        case VOUT_WINDOW_SET_STATE:
        case VOUT_WINDOW_SET_FULLSCREEN:
        case VOUT_WINDOW_UNSET_FULLSCREEN:
            /* These controls deserve a proper window provider. Move along. */
            return VLC_EGENERIC;

        default:
            msg_Warn(wnd, "unsupported control query %d", query);
            return VLC_EGENERIC;
    }
}

static const struct vout_window_operations ops = {
    .control = Control,
};

static int Open(vout_window_t *wnd, const vout_window_cfg_t *cfg)
{
    wnd->type = VOUT_WINDOW_TYPE_DUMMY;
    wnd->ops = &ops;
    vout_window_ReportSize(wnd, cfg->width, cfg->height);
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("Dummy window"))
    set_description(N_("Dummy window"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 1)
    set_callbacks(Open, NULL)
    add_shortcut("dummy")
vlc_module_end()
