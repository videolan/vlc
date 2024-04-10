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

void CommonInit(display_win32_area_t *area, const video_format_t *src_fmt)
{
    ZeroMemory(&area->place, sizeof(area->place));
    area->place_changed = false;
    area->src_fmt = src_fmt;
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
/* */
int CommonWindowInit(vout_display_t *vd, display_win32_area_t *area,
                     bool projection_gestures)
{
    if (unlikely(vd->cfg->window == NULL))
        return VLC_EGENERIC;

    /* */
    area->event = EventThreadCreate(VLC_OBJECT(vd), vd->cfg->window,
                                    &vd->cfg->display,
                                    projection_gestures ? &vd->owner : NULL);
    if (!area->event)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

HWND CommonVideoHWND(const display_win32_area_t *area)
{
    return EventThreadVideoHWND(area->event);
}
#endif /* WINAPI_PARTITION_DESKTOP */

/*****************************************************************************
* UpdateRects: update clipping rectangles
*****************************************************************************
* This function is called when the window position or size are changed, and
* its job is to update the source and destination RECTs used to display the
* picture.
*****************************************************************************/
void CommonPlacePicture(vout_display_t *vd, display_win32_area_t *area)
{
    /* Update the window position and size */
    vout_display_place_t before_place = area->place;
    vout_display_PlacePicture(&area->place, area->src_fmt, &vd->cfg->display);

    /* Signal the change in size/position */
    if (!vout_display_PlaceEquals(&before_place, &area->place))
    {
        area->place_changed |= true;

#ifndef NDEBUG
        msg_Dbg(vd, "UpdateRects source offset: %i,%i visible: %ix%i decoded: %ix%i",
            area->src_fmt->i_x_offset, area->src_fmt->i_y_offset,
            area->src_fmt->i_visible_width, area->src_fmt->i_visible_height,
            area->src_fmt->i_width, area->src_fmt->i_height);
        msg_Dbg(vd, "UpdateRects image_dst coords: %i,%i %ix%i",
            area->place.x, area->place.y, area->place.width, area->place.height);
#endif
    }
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
/* */
void CommonWindowClean(display_win32_area_t *sys)
{
    EventThreadDestroy(sys->event);
}
#endif /* WINAPI_PARTITION_DESKTOP */

void CommonControl(vout_display_t *vd, display_win32_area_t *area, int query)
{
    switch (query) {
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        // Update dimensions
        if (area->event != NULL)
        {
            EventThreadUpdateSize(area->event);
        }
#endif /* WINAPI_PARTITION_DESKTOP */
        // fallthrough
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    case VOUT_DISPLAY_CHANGE_SOURCE_PLACE:
        CommonPlacePicture(vd, area);
        break;

    default:
        vlc_assert_unreachable();
    }
}
