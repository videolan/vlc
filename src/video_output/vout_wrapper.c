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
#include <vlc_vout_display.h>
#include <vlc_vout_wrapper.h>
#include <vlc_vout.h>
#include <assert.h>
#include "vout_internal.h"
#include "display.h"

/*****************************************************************************
 *
 *****************************************************************************/
struct vout_sys_t {
    char           *title;
    vout_display_t *vd;
    bool           use_dr;

    picture_t      *filtered;
};

/* Minimum number of direct pictures the video output will accept without
 * creating additional pictures in system memory */
#ifdef OPTIMIZE_MEMORY
#   define VOUT_MIN_DIRECT_PICTURES        (VOUT_MAX_PICTURES/2)
#else
#   define VOUT_MIN_DIRECT_PICTURES        (3*VOUT_MAX_PICTURES/4)
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void VoutGetDisplayCfg(vout_thread_t *,
                              vout_display_cfg_t *, const char *title);
#ifdef WIN32
static int  Forward(vlc_object_t *, char const *,
                    vlc_value_t, vlc_value_t, void *);
#endif

/*****************************************************************************
 *
 *****************************************************************************/
int vout_OpenWrapper(vout_thread_t *vout, const char *name)
{
    vout_sys_t *sys;

    msg_Dbg(vout, "Opening vout display wrapper");

    /* */
    sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->title = var_CreateGetNonEmptyString(vout, "video-title");

    /* */
    video_format_t source   = vout->fmt_render;
    source.i_visible_width  = source.i_width;
    source.i_visible_height = source.i_height;
    source.i_x_offset       = 0;
    source.i_y_offset       = 0;

    vout_display_state_t state;
    VoutGetDisplayCfg(vout, &state.cfg, sys->title);
    state.is_on_top = var_CreateGetBool(vout, "video-on-top");
    state.sar.num = 0;
    state.sar.den = 0;

    const mtime_t double_click_timeout = 300000;
    const mtime_t hide_timeout = var_CreateGetInteger(vout, "mouse-hide-timeout") * 1000;

    sys->vd = vout_NewDisplay(vout, &source, &state, name ? name : "$vout",
                              double_click_timeout, hide_timeout);
    /* If we need to video filter and it fails, then try a splitter
     * XXX it is a hack for now FIXME */
    if (name && !sys->vd)
        sys->vd = vout_NewSplitter(vout, &source, &state, "$vout", name,
                                   double_click_timeout, hide_timeout);
    if (!sys->vd) {
        free(sys->title);
        free(sys);
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
    vout->p->p_sys = sys;
    vout->p->decoder_pool = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
void vout_CloseWrapper(vout_thread_t *vout)
{
    vout_sys_t *sys = vout->p->p_sys;

#ifdef WIN32
    var_DelCallback(vout, "direct3d-desktop", Forward, NULL);
    var_DelCallback(vout, "video-wallpaper", Forward, NULL);
#endif
    vout->p->decoder_pool = NULL; /* FIXME remove */

    vout_DeleteDisplay(sys->vd, NULL);
    free(sys->title);
    free(sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
int vout_InitWrapper(vout_thread_t *vout)
{
    vout_sys_t *sys = vout->p->p_sys;
    vout_display_t *vd = sys->vd;

    /* */
    video_format_t source = vd->source;

    vout->fmt_out.i_chroma         = source.i_chroma;
    vout->fmt_out.i_width          =
    vout->fmt_out.i_visible_width  = source.i_width;
    vout->fmt_out.i_height         =
    vout->fmt_out.i_visible_height = source.i_height;
    vout->fmt_out.i_sar_num        = source.i_sar_num;
    vout->fmt_out.i_sar_den        = source.i_sar_den;
    vout->fmt_out.i_x_offset       = 0;
    vout->fmt_out.i_y_offset       = 0;
    vout->fmt_out.i_rmask          = source.i_rmask;
    vout->fmt_out.i_gmask          = source.i_gmask;
    vout->fmt_out.i_bmask          = source.i_bmask;

    if (vout->fmt_in.i_visible_width  != source.i_visible_width ||
        vout->fmt_in.i_visible_height != source.i_visible_height ||
        vout->fmt_in.i_x_offset       != source.i_x_offset ||
        vout->fmt_in.i_y_offset       != source.i_y_offset )
        vout->p->i_changes |= VOUT_CROP_CHANGE;

#warning "vout_InitWrapper: vout_SetWindowState should NOT be called there"
    if (vout->p->b_on_top)
        vout_SetWindowState(vd, VOUT_WINDOW_STATE_ABOVE);

    /* XXX For non dr case, the current vout implementation force us to
     * create at most 1 direct picture (otherwise the buffers will be kept
     * referenced even through the Init/End.
     */
    sys->use_dr = !vout_IsDisplayFiltered(vd);
    const bool allow_dr = !vd->info.has_pictures_invalid && sys->use_dr;

    picture_pool_t *display_pool = vout_display_Pool(vd, allow_dr ? VOUT_MAX_PICTURES : 3);
    if (allow_dr && picture_pool_GetSize(display_pool) >= VOUT_MIN_DIRECT_PICTURES) {
        vout->p->decoder_pool = display_pool;
        vout->p->display_pool = display_pool;
        vout->p->is_decoder_pool_slow = vd->info.is_slow;
    } else if (!vout->p->decoder_pool) {
        vout->p->decoder_pool = picture_pool_NewFromFormat(&source, VOUT_MAX_PICTURES);
        if (sys->use_dr)
            vout->p->display_pool = display_pool;
        else
            vout->p->display_pool = picture_pool_Reserve(vout->p->decoder_pool, 1);;
        vout->p->is_decoder_pool_slow = false;
    }
    vout->p->private_pool = picture_pool_Reserve(vout->p->decoder_pool, 3); /* XXX 2 for filter, 1 for SPU */
    sys->filtered = NULL;
    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
void vout_EndWrapper(vout_thread_t *vout)
{
    vout_sys_t *sys = vout->p->p_sys;

    assert(!sys->filtered);
    if (vout->p->private_pool)
        picture_pool_Delete(vout->p->private_pool);

    if (vout->p->decoder_pool != vout->p->display_pool) {
        if (!sys->use_dr)
            picture_pool_Delete(vout->p->display_pool);
        picture_pool_Delete(vout->p->decoder_pool);
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
int vout_ManageWrapper(vout_thread_t *vout)
{
    vout_sys_t *sys = vout->p->p_sys;
    vout_display_t *vd = sys->vd;

    while (vout->p->i_changes & (VOUT_FULLSCREEN_CHANGE |
                              VOUT_ASPECT_CHANGE |
                              VOUT_ZOOM_CHANGE |
                              VOUT_SCALE_CHANGE |
                              VOUT_ON_TOP_CHANGE |
                              VOUT_CROP_CHANGE)) {
        /* */
        if (vout->p->i_changes & VOUT_FULLSCREEN_CHANGE) {
            vout->p->b_fullscreen = !vout->p->b_fullscreen;

            var_SetBool(vout, "fullscreen", vout->p->b_fullscreen);
            vout_SetDisplayFullscreen(vd, vout->p->b_fullscreen);
            vout->p->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
        }
        if (vout->p->i_changes & VOUT_ASPECT_CHANGE) {
            vout->fmt_out.i_sar_num = vout->fmt_in.i_sar_num;
            vout->fmt_out.i_sar_den = vout->fmt_in.i_sar_den;

            vout_SetDisplayAspect(vd, vout->fmt_in.i_sar_num, vout->fmt_in.i_sar_den);

            vout->p->i_changes &= ~VOUT_ASPECT_CHANGE;
        }
        if (vout->p->i_changes & VOUT_ZOOM_CHANGE) {
            const float zoom = var_GetFloat(vout, "scale");

            unsigned den = ZOOM_FP_FACTOR;
            unsigned num = den * zoom;
            if (num < (ZOOM_FP_FACTOR+9) / 10)
                num = (ZOOM_FP_FACTOR+9) / 10;
            else if (num > ZOOM_FP_FACTOR * 10)
                num = ZOOM_FP_FACTOR * 10;

            vout_SetDisplayZoom(vd, num, den);

            vout->p->i_changes &= ~VOUT_ZOOM_CHANGE;
        }
        if (vout->p->i_changes & VOUT_SCALE_CHANGE) {
            const bool is_display_filled = var_GetBool(vout, "autoscale");

            vout_SetDisplayFilled(vd, is_display_filled);

            vout->p->i_changes &= ~VOUT_SCALE_CHANGE;
        }
        if (vout->p->i_changes & VOUT_ON_TOP_CHANGE) {
            vout_SetWindowState(vd, vout->p->b_on_top
                ? VOUT_WINDOW_STATE_ABOVE
                : VOUT_WINDOW_STATE_NORMAL);

            vout->p->i_changes &= ~VOUT_ON_TOP_CHANGE;
        }
        if (vout->p->i_changes & VOUT_CROP_CHANGE) {
            const video_format_t crop = vout->fmt_in;
            const video_format_t org = vout->fmt_render;
            /* FIXME because of rounding errors, the reconstructed ratio is wrong */
            unsigned num = 0;
            unsigned den = 0;
            if (crop.i_x_offset == org.i_x_offset &&
                crop.i_visible_width == org.i_visible_width &&
                crop.i_y_offset == org.i_y_offset + (org.i_visible_height - crop.i_visible_height)/2) {
                vlc_ureduce(&num, &den,
                            crop.i_visible_width * crop.i_sar_num,
                            crop.i_visible_height * crop.i_sar_den, 0);
            } else if (crop.i_y_offset == org.i_y_offset &&
                       crop.i_visible_height == org.i_visible_height &&
                       crop.i_x_offset == org.i_x_offset + (org.i_visible_width - crop.i_visible_width)/2) {
                vlc_ureduce(&num, &den,
                            crop.i_visible_width * crop.i_sar_num,
                            crop.i_visible_height * crop.i_sar_den, 0);
            }
            vout_SetDisplayCrop(vd, num, den,
                                crop.i_x_offset, crop.i_y_offset,
                                crop.i_visible_width, crop.i_visible_height);
            vout->p->i_changes &= ~VOUT_CROP_CHANGE;
        }

    }

    bool reset_display_pool = sys->use_dr && vout_AreDisplayPicturesInvalid(vd);
    vout_ManageDisplay(vd, !sys->use_dr || reset_display_pool);

    if (reset_display_pool)
        vout->p->display_pool = vout_display_Pool(vd, 3);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Render
 *****************************************************************************/
void vout_RenderWrapper(vout_thread_t *vout, picture_t *picture)
{
    vout_sys_t *sys = vout->p->p_sys;
    vout_display_t *vd = sys->vd;

    assert(vout_IsDisplayFiltered(vd) == !sys->use_dr);

    if (sys->use_dr) {
        vout_display_Prepare(vd, picture);
    } else {
        sys->filtered = vout_FilterDisplay(vd, picture);
        if (sys->filtered)
            vout_display_Prepare(vd, sys->filtered);
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
void vout_DisplayWrapper(vout_thread_t *vout, picture_t *picture)
{
    vout_sys_t *sys = vout->p->p_sys;
    vout_display_t *vd = sys->vd;

     vout_display_Display(vd, sys->filtered ? sys->filtered : picture);
     sys->filtered = NULL;
}

static void VoutGetDisplayCfg(vout_thread_t *vout, vout_display_cfg_t *cfg, const char *title)
{
    /* Load configuration */
    cfg->is_fullscreen = var_CreateGetBool(vout, "fullscreen");
    cfg->display.title = title;
    const int display_width = var_CreateGetInteger(vout, "width");
    const int display_height = var_CreateGetInteger(vout, "height");
    cfg->display.width   = display_width > 0  ? display_width  : 0;
    cfg->display.height  = display_height > 0 ? display_height : 0;
    cfg->is_display_filled  = var_CreateGetBool(vout, "autoscale");
    cfg->display.sar.num = 1; /* TODO monitor AR */
    cfg->display.sar.den = 1;
    unsigned zoom_den = 1000;
    unsigned zoom_num = zoom_den * var_CreateGetFloat(vout, "scale");
    vlc_ureduce(&zoom_num, &zoom_den, zoom_num, zoom_den, 0);
    cfg->zoom.num = zoom_num;
    cfg->zoom.den = zoom_den;
    cfg->align.vertical = VOUT_DISPLAY_ALIGN_CENTER;
    cfg->align.horizontal = VOUT_DISPLAY_ALIGN_CENTER;
    const int align_mask = var_CreateGetInteger(vout, "align");
    if (align_mask & 0x1)
        cfg->align.horizontal = VOUT_DISPLAY_ALIGN_LEFT;
    else if (align_mask & 0x2)
        cfg->align.horizontal = VOUT_DISPLAY_ALIGN_RIGHT;
    if (align_mask & 0x4)
        cfg->align.horizontal = VOUT_DISPLAY_ALIGN_TOP;
    else if (align_mask & 0x8)
        cfg->align.horizontal = VOUT_DISPLAY_ALIGN_BOTTOM;
}

#ifdef WIN32
static int Forward(vlc_object_t *object, char const *var,
                   vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vout_thread_t *vout = (vout_thread_t*)object;

    VLC_UNUSED(oldval);
    VLC_UNUSED(data);
    return var_Set(vout->p->p_sys->vd, var, newval);
}
#endif
