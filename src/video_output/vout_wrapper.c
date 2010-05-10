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

    vout_display_state_t state;
    VoutGetDisplayCfg(vout, &state.cfg, sys->display.title);
    state.is_on_top = var_CreateGetBool(vout, "video-on-top");
    state.sar.num = 0;
    state.sar.den = 0;

    const mtime_t double_click_timeout = 300000;
    const mtime_t hide_timeout = var_CreateGetInteger(vout, "mouse-hide-timeout") * 1000;

    sys->display.vd = vout_NewDisplay(vout, &source, &state, name ? name : "$vout",
                                      double_click_timeout, hide_timeout);
    /* If we need to video filter and it fails, then try a splitter
     * XXX it is a hack for now FIXME */
    if (name && !sys->display.vd)
        sys->display.vd = vout_NewSplitter(vout, &source, &state, "$vout", name,
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
void vout_CloseWrapper(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = vout->p;

#ifdef WIN32
    var_DelCallback(vout, "direct3d-desktop", Forward, NULL);
    var_DelCallback(vout, "video-wallpaper", Forward, NULL);
#endif
    sys->decoder_pool = NULL; /* FIXME remove */

    vout_DeleteDisplay(sys->display.vd, NULL);
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

    /* XXX For non dr case, the current vout implementation force us to
     * create at most 1 direct picture (otherwise the buffers will be kept
     * referenced even through the Init/End.
     */
    sys->display.use_dr = !vout_IsDisplayFiltered(vd);
    const bool allow_dr = !vd->info.has_pictures_invalid && sys->display.use_dr;

    picture_pool_t *display_pool = vout_display_Pool(vd, allow_dr ? VOUT_MAX_PICTURES : 3);
    if (allow_dr && picture_pool_GetSize(display_pool) >= VOUT_MIN_DIRECT_PICTURES) {
        sys->decoder_pool = display_pool;
        sys->display_pool = display_pool;
        sys->is_decoder_pool_slow = vd->info.is_slow;
    } else if (!sys->decoder_pool) {
        sys->decoder_pool = picture_pool_NewFromFormat(&source, VOUT_MAX_PICTURES);
        if (sys->display.use_dr)
            sys->display_pool = display_pool;
        else
            sys->display_pool = picture_pool_Reserve(sys->decoder_pool, 1);;
        sys->is_decoder_pool_slow = false;
    }
    sys->private_pool = picture_pool_Reserve(sys->decoder_pool, 3); /* XXX 2 for filter, 1 for SPU */
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
int vout_ManageWrapper(vout_thread_t *vout)
{
    vout_thread_sys_t *sys = vout->p;
    vout_display_t *vd = sys->display.vd;

    bool reset_display_pool = sys->display.use_dr && vout_AreDisplayPicturesInvalid(vd);
    vout_ManageDisplay(vd, !sys->display.use_dr || reset_display_pool);

    if (reset_display_pool)
        sys->display_pool = vout_display_Pool(vd, 3);

    return VLC_SUCCESS;
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
    return var_Set(vout->p->display.vd, var, newval);
}
#endif

