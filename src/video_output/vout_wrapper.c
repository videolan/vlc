/*****************************************************************************
 * vout_wrapper.c: "vout display" -> "video output" wrapper
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <assert.h>
#include "vout_private.h"
#include "vout_internal.h"
#include "display.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef _WIN32
static int  Forward(vlc_object_t *, char const *,
                    vlc_value_t, vlc_value_t, void *);
#endif

static void VoutViewpointMoved(void *sys, const vlc_viewpoint_t *vp)
{
    vout_thread_t *vout = sys;
    var_SetAddress(vout, "viewpoint-moved", (void*)vp);
}

/* Minimum number of display picture */
#define DISPLAY_PICTURE_COUNT (1)

/*****************************************************************************
 *
 *****************************************************************************/
vout_display_t *vout_OpenWrapper(vout_thread_t *vout, vout_thread_private_t *sys,
                     const char *splitter_name, const vout_display_cfg_t *cfg,
                     video_format_t *fmt, vlc_video_context *vctx)
{
    vout_display_t *vd;
    vout_display_owner_t owner = {
        .viewpoint_moved = VoutViewpointMoved, .sys = vout,
    };
    const char *modlist;
    char *modlistbuf = NULL;

    msg_Dbg(vout, "Opening vout display wrapper");

    if (splitter_name == NULL)
        modlist = modlistbuf = var_InheritString(vout, "vout");
    else
        modlist = "splitter,none";

    vd = vout_display_New(VLC_OBJECT(vout), fmt, vctx, cfg, modlist, &owner);
    free(modlistbuf);

    if (vd == NULL)
        return NULL;

    sys->display_pool = NULL;

    const unsigned private_picture  = 4; /* XXX 3 for filter, 1 for SPU */
    const unsigned kept_picture     = 1; /* last displayed picture */
    const unsigned reserved_picture = DISPLAY_PICTURE_COUNT +
                                      private_picture +
                                      kept_picture;

    picture_pool_t *display_pool = vout_GetPool(vd, reserved_picture);
    if (display_pool == NULL)
        goto error;

#ifndef NDEBUG
    if ( picture_pool_GetSize(display_pool) < reserved_picture )
        msg_Warn(vout, "Not enough display buffers in the pool, requested %u got %u",
                 reserved_picture, picture_pool_GetSize(display_pool));
#endif

    if (!vout_IsDisplayFiltered(vd) &&
        picture_pool_GetSize(display_pool) >= reserved_picture) {
        sys->private_pool = picture_pool_Reserve(display_pool, private_picture);
    } else {
        sys->private_pool =
            picture_pool_NewFromFormat(&vd->source,
                                       __MAX(VOUT_MAX_PICTURES,
                                             reserved_picture - DISPLAY_PICTURE_COUNT));
    }
    if (sys->private_pool == NULL) {
        picture_pool_Release(display_pool);
        goto error;
    }
    sys->display_pool = display_pool;

#ifdef _WIN32
    var_Create(vout, "video-wallpaper", VLC_VAR_BOOL|VLC_VAR_DOINHERIT);
    var_AddCallback(vout, "video-wallpaper", Forward, vd);
#endif
    var_SetBool(VLC_OBJECT(vout), "viewpoint-changeable",
                vd->fmt.projection_mode != PROJECTION_MODE_RECTANGULAR);
    return vd;

error:
    vout_display_Delete(vd);
    return NULL;
}

/*****************************************************************************
 *
 *****************************************************************************/
void vout_CloseWrapper(vout_thread_t *vout, vout_thread_private_t *sys, vout_display_t *vd)
{
    assert(sys->display_pool && sys->private_pool);

    picture_pool_Release(sys->private_pool);
    sys->display_pool = NULL;

#ifdef _WIN32
    var_DelCallback(vout, "video-wallpaper", Forward, vd);
#endif

    vout_display_Delete(vd);
}

#ifdef _WIN32
static int Forward(vlc_object_t *object, char const *var,
                   vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vout_display_t *vd = data;

    VLC_UNUSED(object); VLC_UNUSED(oldval);
    return var_Set(vd, var, newval);
}
#endif

