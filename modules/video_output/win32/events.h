/*****************************************************************************
 * events.h: Windows video output header file
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Damien Fouilleul <damienf@videolan.org>
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

#include <vlc_vout_window.h>
#include <vlc_vout_display.h>

/**
 * HWNDs manager.
 */
typedef struct event_thread_t event_thread_t;

typedef struct {
    bool use_desktop; /* direct3d */
    int x;
    int y;
    unsigned width;
    unsigned height;
} event_cfg_t;

typedef struct {
    vout_window_t *parent_window;
    HWND hparent;
    HWND hwnd;
    HWND hvideownd;
    HWND hfswnd;
} event_hwnd_t;

event_thread_t *EventThreadCreate( vout_display_t *, const vout_display_cfg_t *);
void            EventThreadDestroy( event_thread_t * );
int             EventThreadStart( event_thread_t *, event_hwnd_t *, const event_cfg_t * );
void            EventThreadStop( event_thread_t * );

void            EventThreadUpdateTitle( event_thread_t *, const char *psz_fallback );
int             EventThreadGetWindowStyle( event_thread_t * );
void            EventThreadUpdateWindowPosition( event_thread_t *, const RECT * );
void            EventThreadUpdateSourceAndPlace( event_thread_t *p_event,
                                                 const video_format_t *p_source,
                                                 const vout_display_place_t *p_place );
bool            EventThreadGetAndResetHasMoved( event_thread_t * );

# ifdef __cplusplus
extern "C" {
# endif
void* HookWindowsSensors(vout_display_t*, HWND);
void UnhookWindowsSensors(void*);
# ifdef __cplusplus
}
# endif

