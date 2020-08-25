/*****************************************************************************
 * common.c: Windows video output common code
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Martell Malone <martellmalone@gmail.com>
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

/*****************************************************************************
 * Preamble: This file contains the functions related to the init of the vout
 *           structure, the common display code, the screensaver, but not the
 *           events and the Window Creation (events.c)
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout_display.h>

#include <windows.h>
#include <assert.h>

#include "events.h"
#include "common.h"
#include "../../video_chroma/copy.h"

void CommonInit(display_win32_area_t *area, const vout_display_cfg_t *vdcfg)
{
    area->place_changed = false;
    area->vdcfg = *vdcfg;
}

#if !VLC_WINSTORE_APP
/* */
int CommonWindowInit(vout_display_t *vd, display_win32_area_t *area,
                     vout_display_sys_win32_t *sys, bool projection_gestures)
{
    if (unlikely(area->vdcfg.window == NULL))
        return VLC_EGENERIC;

    /* */
#if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    sys->dxgidebug_dll = LoadLibrary(TEXT("DXGIDEBUG.DLL"));
#endif
    sys->hvideownd = NULL;
    sys->hparent   = NULL;

    /* */
    sys->event = EventThreadCreate(VLC_OBJECT(vd), area->vdcfg.window);
    if (!sys->event)
        return VLC_EGENERIC;

    /* */
    event_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.width  = area->vdcfg.display.width;
    cfg.height = area->vdcfg.display.height;
    cfg.is_projected = projection_gestures;

    event_hwnd_t hwnd;
    if (EventThreadStart(sys->event, &hwnd, &cfg))
        return VLC_EGENERIC;

    sys->hparent       = hwnd.hparent;
    sys->hvideownd     = hwnd.hvideownd;

    CommonPlacePicture(vd, area, sys);

    return VLC_SUCCESS;
}
#endif /* !VLC_WINSTORE_APP */

/*****************************************************************************
* UpdateRects: update clipping rectangles
*****************************************************************************
* This function is called when the window position or size are changed, and
* its job is to update the source and destination RECTs used to display the
* picture.
*****************************************************************************/
void CommonPlacePicture(vout_display_t *vd, display_win32_area_t *area, vout_display_sys_win32_t *sys)
{
    /* Update the window position and size */
    vout_display_cfg_t place_cfg = area->vdcfg;

#if (defined(MODULE_NAME_IS_glwin32))
    /* Reverse vertical alignment as the GL tex are Y inverted */
    if (place_cfg.align.vertical == VLC_VIDEO_ALIGN_TOP)
        place_cfg.align.vertical = VLC_VIDEO_ALIGN_BOTTOM;
    else if (place_cfg.align.vertical == VLC_VIDEO_ALIGN_BOTTOM)
        place_cfg.align.vertical = VLC_VIDEO_ALIGN_TOP;
#endif

    vout_display_place_t before_place = area->place;
    vout_display_PlacePicture(&area->place, &vd->source, &place_cfg);

    /* Signal the change in size/position */
    if (!vout_display_PlaceEquals(&before_place, &area->place))
    {
        area->place_changed |= true;

#ifndef NDEBUG
        msg_Dbg(vd, "UpdateRects source offset: %i,%i visible: %ix%i decoded: %ix%i",
            vd->source.i_x_offset, vd->source.i_y_offset,
            vd->source.i_visible_width, vd->source.i_visible_height,
            vd->source.i_width, vd->source.i_height);
        msg_Dbg(vd, "UpdateRects image_dst coords: %i,%i %ix%i",
            area->place.x, area->place.y, area->place.width, area->place.height);
#endif
    }
}

#if !VLC_WINSTORE_APP
/* */
void CommonWindowClean(vlc_object_t *obj, vout_display_sys_win32_t *sys)
{
    if (sys->event) {
        EventThreadStop(sys->event);
        EventThreadDestroy(sys->event);
    }
}
#endif /* !VLC_WINSTORE_APP */

int CommonControl(vout_display_t *vd, display_win32_area_t *area, vout_display_sys_win32_t *sys, int query, va_list args)
{
    switch (query) {
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED: /* const vout_display_cfg_t *p_cfg */
    case VOUT_DISPLAY_CHANGE_ZOOM:           /* const vout_display_cfg_t *p_cfg */
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP: {
        area->vdcfg = *va_arg(args, const vout_display_cfg_t *);
        CommonPlacePicture(vd, area, sys);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:   /* const vout_display_cfg_t *p_cfg */
    {   /* Update dimensions */
        area->vdcfg = *va_arg(args, const vout_display_cfg_t *);
#if !VLC_WINSTORE_APP
        if (sys->event != NULL)
        {
            RECT clientRect;
            GetClientRect(sys->hparent, &clientRect);
            area->vdcfg.display.width  = RECTWidth(clientRect);
            area->vdcfg.display.height = RECTHeight(clientRect);

            SetWindowPos(sys->hvideownd, 0, 0, 0,
                         area->vdcfg.display.width,
                         area->vdcfg.display.height, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
        }
#endif /* !VLC_WINSTORE_APP */
        CommonPlacePicture(vd, area, sys);
        return VLC_SUCCESS;
    }

    case VOUT_DISPLAY_RESET_PICTURES:
        vlc_assert_unreachable();

    default:
        return VLC_EGENERIC;
    }
}
