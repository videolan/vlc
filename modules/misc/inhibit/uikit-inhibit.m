/*****************************************************************************
 * Copyright © 2021 Videolabs
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

/**
 * \file uikit-inhibit.m
 * \brief iOS display and idle sleep inhibitor using UIKit
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_inhibit.h>
#include <vlc_vout_window.h>

#include <UIKit/UIKit.h>

static void UpdateInhibit(vlc_inhibit_t *ih, unsigned mask)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [UIApplication sharedApplication].idleTimerDisabled =
            (mask & VLC_INHIBIT_DISPLAY) == VLC_INHIBIT_DISPLAY;
    });
}

static int OpenInhibit(vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    vout_window_t *wnd = vlc_inhibit_GetWindow(ih);
    if (wnd->type != VOUT_WINDOW_TYPE_NSOBJECT)
        return VLC_EGENERIC;

    UIView * view = (__bridge UIView*)wnd->handle.nsobject;

    if (unlikely(![view respondsToSelector:@selector(isKindOfClass:)]))
        return VLC_EGENERIC;

    if (![view isKindOfClass:[UIView class]])
        return VLC_EGENERIC;

    ih->inhibit = UpdateInhibit;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("UIKit sleep inhibition")
    set_description("UIKit screen sleep inhibition for iOS and tvOS")
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("inhibit", 10)
    set_callback(OpenInhibit)
vlc_module_end()
