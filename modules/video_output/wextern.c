/**
 * @file wextern.c
 * @brief Dummy video window provider where the size is handled externally
 */
/*****************************************************************************
 * Copyright © 2019 VideoLabs, VideoLAN and VideoLAN Authors
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

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>

static int Open(vout_window_t *);

vlc_module_begin()
    set_shortname(N_("Callback window"))
    set_description(N_("External callback window"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 0)
    set_callback(Open)
vlc_module_end()

typedef struct {
    void                                   *opaque;
    libvlc_video_output_set_resize_cb      setResizeCb;
} wextern_t;

static void WindowResize(void *opaque, unsigned width, unsigned height)
{
    vout_window_t *window = opaque;
    vout_window_ReportSize(window, width, height);
}

static int Enable(struct vout_window_t *wnd, const vout_window_cfg_t *wcfg)
{
    wextern_t *sys = wnd->sys;

    if ( sys->setResizeCb != NULL )
        /* bypass the size handling as the window doesn't handle the size */
        sys->setResizeCb( sys->opaque, WindowResize, wnd );

    (void) wcfg;
    return VLC_SUCCESS;
}

static void Disable(struct vout_window_t *wnd)
{
    wextern_t *sys = wnd->sys;

    if ( sys->setResizeCb != NULL )
        sys->setResizeCb( sys->opaque, NULL, NULL );
}

static const struct vout_window_operations ops = {
    .enable  = Enable,
    .disable = Disable,
    // .resize: don't let the core resize us on zoom/crop/ar changes
    //          the display module should do the ReportSize for us
};

static int Open(vout_window_t *wnd)
{
    wextern_t *sys = vlc_obj_malloc(VLC_OBJECT(wnd), sizeof(*sys));
    if (unlikely(sys==NULL))
        return VLC_ENOMEM;
    sys->opaque          = var_InheritAddress( wnd, "vout-cb-opaque" );
    sys->setResizeCb     = var_InheritAddress( wnd, "vout-cb-resize-cb" );

    wnd->sys = sys;
    wnd->type = VOUT_WINDOW_TYPE_DUMMY;
    wnd->ops = &ops;
    return VLC_SUCCESS;
}
