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

/*****************************************************************************
 * event_thread_t: event thread
 *****************************************************************************/

typedef struct event_thread_t event_thread_t;

typedef struct display_win32_area_t
{
    /* Coordinates of dest images (used when blitting to display) */
    vout_display_place_t  place;
    bool                  place_changed;

    vout_display_cfg_t    vdcfg;
} display_win32_area_t;

#define RECTWidth(r)   (LONG)((r).right - (r).left)
#define RECTHeight(r)  (LONG)((r).bottom - (r).top)

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the module specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_display_sys_win32_t
{
    /* */
    event_thread_t *event;

    /* */
    HWND                 hvideownd;        /* Handle of the video sub-window */
    HWND                 hparent;             /* Handle of the parent window */

# if !defined(NDEBUG) && defined(HAVE_DXGIDEBUG_H)
    HINSTANCE     dxgidebug_dll;
# endif
} vout_display_sys_win32_t;


/*****************************************************************************
 * Prototypes from common.c
 *****************************************************************************/
#if !VLC_WINSTORE_APP
int  CommonWindowInit(vout_display_t *, display_win32_area_t *, vout_display_sys_win32_t *,
                      bool projection_gestures);
void CommonWindowClean(vlc_object_t *, vout_display_sys_win32_t *);
#endif /* !VLC_WINSTORE_APP */
int  CommonControl(vout_display_t *, display_win32_area_t *, vout_display_sys_win32_t *, int , va_list );

void CommonPlacePicture (vout_display_t *, display_win32_area_t *, vout_display_sys_win32_t *);

void CommonInit(vout_display_t *, display_win32_area_t *, const vout_display_cfg_t *);

# ifdef __cplusplus
extern "C" {
# endif
void* HookWindowsSensors(vout_display_t*, HWND);
void UnhookWindowsSensors(void*);
# ifdef __cplusplus
}
# endif
