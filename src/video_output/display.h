/*****************************************************************************
 * display.h: "vout display" managment
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <vlc_vout_wrapper.h>

#if 0
#include <vlc_vout_display.h>
#include <vlc_filter.h>

/**
 * It retreive a picture from the display
 */
static inline picture_pool_t *vout_display_Pool(vout_display_t *vd, unsigned count)
{
    return vd->pool(vd, count);
}

/**
 * It preparse a picture for display.
 */
static inline void vout_display_Prepare(vout_display_t *vd, picture_t *picture)
{
    if (vd->prepare )
        vd->prepare(vd, picture);
}

/**
 * It display a picture.
 */
static inline void vout_display_Display(vout_display_t *vd, picture_t *picture)
{
    vd->display(vd, picture);
}

/**
 * It holds a state for a vout display.
 */
typedef struct {
    vout_display_cfg_t cfg;

    bool is_on_top;
    struct {
        int num;
        int den;
    } sar;
} vout_display_state_t;

/**
 * It creates a vout managed display.
 */
vout_display_t *vout_NewDisplay( vout_thread_t *,
                                 const video_format_t *,
                                 const vout_display_state_t *,
                                 const char *psz_module,
                                 mtime_t i_double_click_timeout,
                                 mtime_t i_hide_timeout );
/**
 * It creates a vout managed display wrapping a video splitter.
 */
vout_display_t *vout_NewSplitter(vout_thread_t *,
                                 const video_format_t *source,
                                 const vout_display_state_t *state,
                                 const char *module,
                                 const char *splitter,
                                 mtime_t double_click_timeout,
                                 mtime_t hide_timeout );

/**
 * It destroy a vout managed display.
 */
void vout_DeleteDisplay(vout_display_t *, vout_display_state_t *);

picture_t *vout_FilterDisplay(vout_display_t *, picture_t *);

void vout_ManageDisplay(vout_display_t *, bool allow_reset_pictures);

void vout_SetDisplayFullscreen(vout_display_t *, bool is_fullscreen);
void vout_SetDisplayFilled(vout_display_t *, bool is_filled);
void vout_SetDisplayZoom(vout_display_t *, int num, int den);
void vout_SetWindowState(vout_display_t *, unsigned state);
void vout_SetDisplayAspect(vout_display_t *, unsigned sar_num, unsigned sar_den);
void vout_SetDisplayCrop(vout_display_t *,
                         unsigned crop_num, unsigned crop_den,
                         unsigned x, unsigned y, unsigned width, unsigned height);

#endif

vout_display_t *vout_NewSplitter(vout_thread_t *vout,
                                 const video_format_t *source,
                                 const vout_display_state_t *state,
                                 const char *module,
                                 const char *splitter_module,
                                 mtime_t double_click_timeout,
                                 mtime_t hide_timeout);

/* FIXME should not be there */
void vout_SendDisplayEventMouse(vout_thread_t *, const vlc_mouse_t *);
vout_window_t *vout_NewDisplayWindow(vout_thread_t *, vout_display_t *, const vout_window_cfg_t *);
void vout_DeleteDisplayWindow(vout_thread_t *, vout_display_t *, vout_window_t *);
void vout_UpdateDisplaySourceProperties(vout_display_t *vd, const video_format_t *);

