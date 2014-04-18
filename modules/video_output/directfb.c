/*****************************************************************************
 * directfb.c: DirectFB video output display method
 *****************************************************************************
 * Copyright © 2005-2009 VLC authors and VideoLAN
 *
 * Authors: Iuri Diniz <iuridiniz@gmail.com>
 *          Laurent Aimar <fenrir@videolan.org>
 *
 * This code is based in sdl.c and fb.c, thanks for VideoLAN team.
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
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

#include <directfb.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname("DirectFB")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description(N_("DirectFB video output http://www.directfb.org/"))
    set_capability("vout display", 35)
    add_shortcut("directfb")
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_pool_t *Pool  (vout_display_t *, unsigned);
static void           Display(vout_display_t *, picture_t *, subpicture_t *);
static int            Control(vout_display_t *, int, va_list);

/* */
struct vout_display_sys_t {
    IDirectFB             *directfb;
    IDirectFBSurface      *primary;

    picture_pool_t *pool;
};

/* */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    /* Init DirectFB */
    if (DirectFBInit(NULL,NULL) != DFB_OK) {
        msg_Err(vd, "Cannot init DirectFB");
        free(sys);
        return VLC_EGENERIC;
    }

    DFBSurfaceDescription dsc;
    /*dsc.flags = DSDESC_CAPS | DSDESC_HEIGHT | DSDESC_WIDTH;*/
    dsc.flags = DSDESC_CAPS;
    dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
    /*dsc.width = 352;*/
    /*dsc.height = 240;*/

    IDirectFB *directfb = NULL;
    if (DirectFBCreate(&directfb) != DFB_OK || !directfb)
        goto error;
    sys->directfb = directfb;

    IDirectFBSurface *primary = NULL;
    if (directfb->CreateSurface(directfb, &dsc, &primary) || !primary)
        goto error;
    sys->primary = primary;

    /* */
    int width;
    int height;

    primary->GetSize(primary, &width, &height);
    primary->FillRectangle(primary, 0, 0, width, height);
    primary->Flip(primary, NULL, 0);

    vout_display_DeleteWindow(vd, NULL);

    /* */
    video_format_t fmt;
    video_format_ApplyRotation(&fmt, &vd->fmt);

    DFBSurfacePixelFormat pixel_format;
    sys->primary->GetPixelFormat(sys->primary, &pixel_format);
    switch (pixel_format) {
    case DSPF_RGB332:
        fmt.i_chroma = VLC_CODEC_RGB8;
        fmt.i_rmask = 0x7 << 5;
        fmt.i_gmask = 0x7 << 2;
        fmt.i_bmask = 0x3 << 0;
        break;
    case DSPF_RGB16: fmt.i_chroma = VLC_CODEC_RGB16; break;
    case DSPF_RGB24: fmt.i_chroma = VLC_CODEC_RGB24; break;
    case DSPF_RGB32: fmt.i_chroma = VLC_CODEC_RGB32; break;
    default:
        msg_Err(vd, "unknown screen depth %i", pixel_format);
        Close(VLC_OBJECT(vd));
        return VLC_EGENERIC;
    }

    video_format_FixRgb(&fmt);

    fmt.i_width  = width;
    fmt.i_height = height;

    /* */
    vout_display_info_t info = vd->info;
    info.has_hide_mouse = true;

    /* */
    vd->fmt     = fmt;
    vd->info    = info;
    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = NULL;

    /* */
    vout_display_SendEventFullscreen(vd, true);
    vout_display_SendEventDisplaySize(vd, fmt.i_width, fmt.i_height, true);
    return VLC_SUCCESS;

error:
    msg_Err(vd, "Cannot create primary surface");
    Close(VLC_OBJECT(vd));
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Delete(sys->pool);

    IDirectFBSurface *primary = sys->primary;
    if (primary)
        primary->Release(primary);

    IDirectFB *directfb = sys->directfb;
    if (directfb)
        directfb->Release(directfb);

    free(sys);
}

/* */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool)
        sys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
    return sys->pool;
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    IDirectFBSurface *primary = sys->primary;

    void *pixels;
    int  pitch;
    if (primary->Lock(primary, DSLF_WRITE, &pixels, &pitch) == DFB_OK) {
        picture_resource_t rsc;

        memset(&rsc, 0, sizeof(rsc));
        rsc.p[0].p_pixels = pixels;
        rsc.p[0].i_lines  = vd->fmt.i_height;
        rsc.p[0].i_pitch  = pitch;

        picture_t *direct = picture_NewFromResource(&vd->fmt, &rsc);
        if (direct) {
            picture_Copy(direct, picture);
            picture_Release(direct);
        }

        if (primary->Unlock(primary) == DFB_OK)
            primary->Flip(primary, NULL, 0);
    }
    picture_Release(picture);
    VLC_UNUSED(subpicture);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    switch (query) {
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE: {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);
        if (cfg->display.width  != vd->fmt.i_width ||
            cfg->display.height != vd->fmt.i_height)
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }
    default:
        msg_Err(vd, "Unsupported query in vout display directfb");
        return VLC_EGENERIC;
    }
}
