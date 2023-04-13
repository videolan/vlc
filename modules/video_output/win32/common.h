/*****************************************************************************
 * common.h: Windows video output header file
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Damien Fouilleul <damienf@videolan.org>
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

#include <vlc_vout_display.h>

#ifdef __cplusplus
extern "C" {
#endif// __cplusplus

/*****************************************************************************
 * event_thread_t: event thread
 *****************************************************************************/

typedef struct display_win32_area_t
{
    /* Coordinates of dest images (used when blitting to display) */
    vout_display_place_t  place;
    bool                  place_changed;
    struct event_thread_t *event; // only use if sys.event is not NULL
} display_win32_area_t;

#define RECTWidth(r)   (LONG)((r).right - (r).left)
#define RECTHeight(r)  (LONG)((r).bottom - (r).top)

/*****************************************************************************
 * Prototypes from common.c
 *****************************************************************************/
#ifndef VLC_WINSTORE_APP
int  CommonWindowInit(vout_display_t *, display_win32_area_t *,
                      bool projection_gestures);
void CommonWindowClean(display_win32_area_t *);
HWND CommonVideoHWND(const display_win32_area_t *);
#endif /* !VLC_WINSTORE_APP */
void CommonControl(vout_display_t *, display_win32_area_t *, int );

void CommonPlacePicture (vout_display_t *, display_win32_area_t *);

void CommonInit(display_win32_area_t *);

void* HookWindowsSensors(vout_display_t*, HWND);
void UnhookWindowsSensors(void*);
# ifdef __cplusplus
}
# endif
