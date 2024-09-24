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

/* */
int CommonWindowInit(vout_display_t *vd, struct event_thread_t **event,
                     bool projection_gestures)
{
    *event = NULL;
    if (unlikely(vd->cfg->window == NULL))
        return VLC_EGENERIC;

    /* */
    *event = EventThreadCreate(VLC_OBJECT(vd), vd->cfg->window,
                               &vd->cfg->display,
                               projection_gestures ? &vd->owner : NULL);
    if (!*event)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

HWND CommonVideoHWND(const struct event_thread_t *event)
{
    return EventThreadVideoHWND(event);
}

/* */
void CommonWindowClean(struct event_thread_t *event)
{
    EventThreadDestroy(event);
}

void CommonDisplaySizeChanged(struct event_thread_t *event)
{
    // Update dimensions
    if (event != NULL)
    {
        EventThreadUpdateSize(event);
    }
}
