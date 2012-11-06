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
static void           Manage (vout_display_t *);

/* */
static int  OpenDisplay (vout_display_t *);
static void CloseDisplay(vout_display_t *);

/* */
struct vout_display_sys_t {
    /* */
    IDirectFB             *directfb;
    IDirectFBSurface      *primary;
    DFBSurfacePixelFormat pixel_format;

    /* */
    int width;
    int height;

    /* */
    picture_pool_t *pool;
};

/* */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    /* Allocate structure */
    vd->sys = sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->directfb = NULL;
    sys->primary  = NULL;
    sys->width    = 0;
    sys->height   = 0;
    sys->pool     = NULL;

    /* Init DirectFB */
    if (DirectFBInit(NULL,NULL) != DFB_OK) {
        msg_Err(vd, "Cannot init DirectFB");
        free(sys);
        return VLC_EGENERIC;
    }

    if (OpenDisplay(vd)) {
        msg_Err(vd, "Cannot create primary surface");
        Close(VLC_OBJECT(vd));
        return VLC_EGENERIC;
    }
    vout_display_DeleteWindow(vd, NULL);

    /* */
    video_format_t fmt = vd->fmt;

    switch (sys->pixel_format) {
    case DSPF_RGB332:
        /* 8 bit RGB (1 byte, red 3@5, green 3@2, blue 2@0) */
        fmt.i_chroma = VLC_CODEC_RGB8;
        fmt.i_rmask = 0x7 << 5;
        fmt.i_gmask = 0x7 << 2;
        fmt.i_bmask = 0x3 << 0;
        break;
    case DSPF_RGB16:
        /* 16 bit RGB (2 byte, red 5@11, green 6@5, blue 5@0) */
        fmt.i_chroma = VLC_CODEC_RGB16;
        fmt.i_rmask = 0x1f << 11;
        fmt.i_gmask = 0x3f <<  5;
        fmt.i_bmask = 0x1f <<  0;
        break;
    case DSPF_RGB24:
        /* 24 bit RGB (3 byte, red 8@16, green 8@8, blue 8@0) */
        fmt.i_chroma = VLC_CODEC_RGB24;
        fmt.i_rmask = 0xff << 16;
        fmt.i_gmask = 0xff <<  8;
        fmt.i_bmask = 0xff <<  0;
        break;
    case DSPF_RGB32:
        /* 24 bit RGB (4 byte, nothing@24, red 8@16, green 8@8, blue 8@0) */
        fmt.i_chroma = VLC_CODEC_RGB32;
        fmt.i_rmask = 0xff << 16;
        fmt.i_gmask = 0xff <<  8;
        fmt.i_bmask = 0xff <<  0;
        break;
    default:
        msg_Err(vd, "unknown screen depth %i", sys->pixel_format);
        Close(VLC_OBJECT(vd));
        return VLC_EGENERIC;
    }

    fmt.i_width  = sys->width;
    fmt.i_height = sys->height;

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
    vd->manage  = Manage;

    /* */
    vout_display_SendEventFullscreen(vd, true);
    vout_display_SendEventDisplaySize(vd, fmt.i_width, fmt.i_height, true);
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Delete(sys->pool);

    CloseDisplay(vd);
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
        rsc.p[0].i_lines  = sys->height;
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

static void Manage (vout_display_t *vd)
{
    VLC_UNUSED(vd);
}

static int OpenDisplay(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    DFBSurfaceDescription dsc;
    /*dsc.flags = DSDESC_CAPS | DSDESC_HEIGHT | DSDESC_WIDTH;*/
    dsc.flags = DSDESC_CAPS;
    dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
    /*dsc.width = 352;*/
    /*dsc.height = 240;*/

    IDirectFB *directfb = NULL;
    if (DirectFBCreate(&directfb) != DFB_OK || !directfb)
        return VLC_EGENERIC;
    sys->directfb = directfb;

    IDirectFBSurface *primary = NULL;
    if (directfb->CreateSurface(directfb, &dsc, &primary) || !primary)
        return VLC_EGENERIC;
    sys->primary = primary;

    primary->GetSize(primary, &sys->width, &sys->height);
    primary->GetPixelFormat(primary, &sys->pixel_format);
    primary->FillRectangle(primary, 0, 0, sys->width, sys->height);
    primary->Flip(primary, NULL, 0);

    return VLC_SUCCESS;
}

static void CloseDisplay(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    IDirectFBSurface *primary = sys->primary;
    if (primary)
        primary->Release(primary);

    IDirectFB *directfb = sys->directfb;
    if (directfb)
        directfb->Release(directfb);
}

