/*****************************************************************************
 * vout_display.c: "vout display" -> "video output" wrapper
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_wrapper.h>
#include <vlc_vout.h>
#include <assert.h>
#include "vout_internal.h"
#include "display.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef WIN32
static int  Forward(vlc_object_t *, char const *,
                    vlc_value_t, vlc_value_t, void *);
#endif

/*****************************************************************************
 *
 *****************************************************************************/
int vout_OpenWrapper(vout_thread_t *vout,
                     const char *name, const vout_display_state_t *state)
{
    vout_thread_sys_t *sys = vout->p;
    msg_Dbg(vout, "Opening vout display wrapper");

    /* */
    sys->display.title = var_CreateGetNonEmptyString(vout, "video-title");

    /* */
    video_format_t source   = vout->p->original;
    source.i_visible_width  = source.i_width;
    source.i_visible_height = source.i_height;
    source.i_x_offset       = 0;
    source.i_y_offset       = 0;

    const mtime_t double_click_timeout = 300000;
    const mtime_t hide_timeout = var_CreateGetInteger(vout, "mouse-hide-timeout") * 1000;

    sys->display.vd = vout_NewDisplay(vout, &source, state, name ? name : "$vout",
                                      double_click_timeout, hide_timeout);
    /* If we need to video filter and it fails, then try a splitter
     * XXX it is a hack for now FIXME */
    if (name && !sys->display.vd)
        sys->display.vd = vout_NewSplitter(vout, &source, state, "$vout", name,
                                           double_click_timeout, hide_timeout);
    if (!sys->display.vd) {
        free(sys->display.title);
        return VLC_EGENERIC;
    }

    /* */
#ifdef WIN32
    var_Create(vout, "direct3d-desktop", VLC_VAR_BOOL|VLC_VAR_DOINHERIT);
    var_AddCallback(vout, "direct3d-desktop", Forward, NULL);
    var_Create(vout, "video-wallpaper", VLC_VAR_BOOL|VLC_VAR_DOINHERIT);
    var_AddCallback(vout, "video-wallpaper", Forward, NULL);
#endif

    /* */
    sys->decoder_pool = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
void vout_CloseWrapper(vout_thread_t *vout, vout_display_state_t *state)
{
    vout_thread_sys_t *sys = vout->p;

#ifdef WIN32
    var_DelCallback(vout, "direct3d-desktop", Forward, NULL);
    var_DelCallback(vout, "video-wallpaper", Forward, NULL);
#endif
    sys->decoder_pool = NULL; /* FIXME remove */

    vout_DeleteDisplay(sys->display.vd, state);
    free(sys->display.title);
}

/*****************************************************************************
 *
 *****************************************************************************/
int vout_InitWrapper(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = vout->p;
    vout_display_t *vd = sys->display.vd;
    video_format_t source = vd->source;

    sys->display.use_dr = !vout_IsDisplayFiltered(vd);
    const bool allow_dr = !vd->info.has_pictures_invalid && sys->display.use_dr;
    const unsigned private_picture  = 3; /* XXX 2 for filter, 1 for SPU */
    const unsigned display_picture  = 1; /* Minimum number of display picture */
    const unsigned decoder_picture  = 1 + sys->dpb_size;
    const unsigned kept_picture     = 1; /* last displayed picture */
    const unsigned reserved_picture = display_picture +
                                      private_picture +
                                      kept_picture;
    picture_pool_t *display_pool =
        vout_display_Pool(vd, allow_dr ? __MAX(VOUT_MAX_PICTURES,
                                               reserved_picture + decoder_picture) : 3);
    if (allow_dr &&
        picture_pool_GetSize(display_pool) >= reserved_picture + decoder_picture) {
        sys->dpb_size     = picture_pool_GetSize(display_pool) - reserved_picture;
        sys->decoder_pool = display_pool;
        sys->display_pool = display_pool;
        sys->is_decoder_pool_slow = vd->info.is_slow;
    } else if (!sys->decoder_pool) {
        sys->decoder_pool =
            picture_pool_NewFromFormat(&source,
                                       __MAX(VOUT_MAX_PICTURES,
                                             reserved_picture + decoder_picture));
        if (allow_dr) {
            msg_Warn(vout, "Not enough direct buffers, using system memory");
            sys->dpb_size = 0;
        } else {
            sys->dpb_size = picture_pool_GetSize(sys->decoder_pool) - reserved_picture;
        }
        if (sys->display.use_dr)
            sys->display_pool = display_pool;
        else
            sys->display_pool = picture_pool_Reserve(sys->decoder_pool, display_picture);
        sys->is_decoder_pool_slow = false;
    }
    sys->private_pool = picture_pool_Reserve(sys->decoder_pool, private_picture);
    sys->display.filtered = NULL;
    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
void vout_EndWrapper(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = vout->p;

    assert(!sys->display.filtered);
    if (sys->private_pool)
        picture_pool_Delete(sys->private_pool);

    if (sys->decoder_pool != sys->display_pool) {
        if (!sys->display.use_dr)
            picture_pool_Delete(sys->display_pool);
        picture_pool_Delete(sys->decoder_pool);
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
void vout_ManageWrapper(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = vout->p;
    vout_display_t *vd = sys->display.vd;

    bool reset_display_pool = sys->display.use_dr && vout_AreDisplayPicturesInvalid(vd);
    vout_ManageDisplay(vd, !sys->display.use_dr || reset_display_pool);

    if (reset_display_pool)
        sys->display_pool = vout_display_Pool(vd, 3);
}

/*****************************************************************************
 * Render
 *****************************************************************************/
void vout_RenderWrapper(vout_thread_t *vout, picture_t *picture)
{
    vout_thread_sys_t *sys = vout->p;
    vout_display_t *vd = sys->display.vd;

    assert(vout_IsDisplayFiltered(vd) == !sys->display.use_dr);

    if (sys->display.use_dr) {
        vout_display_Prepare(vd, picture);
    } else {
        sys->display.filtered = vout_FilterDisplay(vd, picture);
        if (sys->display.filtered)
            vout_display_Prepare(vd, sys->display.filtered);
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
void vout_DisplayWrapper(vout_thread_t *vout, picture_t *picture)
{
    vout_thread_sys_t *sys = vout->p;
    vout_display_t *vd = sys->display.vd;

     vout_display_Display(vd, sys->display.filtered ? sys->display.filtered : picture);
     sys->display.filtered = NULL;
}

#ifdef WIN32
static int Forward(vlc_object_t *object, char const *var,
                   vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vout_thread_t *vout = (vout_thread_t*)object;

    VLC_UNUSED(oldval);
    VLC_UNUSED(data);
    return var_Set(vout->p->display.vd, var, newval);
}
#endif

