/*****************************************************************************
 * glwin32.c: Windows OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>

#include <windows.h>
#include <versionhelpers.h>

#define GLEW_STATIC
#include "../opengl/renderer.h"
#include "../opengl/vout_helper.h"

#include "common.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vout_display_t *,
                  video_format_t *, vlc_video_context *);
static void Close(vout_display_t *);

vlc_module_begin()
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_shortname("OpenGL")
    set_description(N_("OpenGL video output for Windows"))
    add_shortcut("glwin32", "opengl")
    set_callback_display(Open, 275)
    add_glopts()

    add_opengl_submodule_renderer()
vlc_module_end()

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
typedef struct vout_display_sys_t
{
    display_win32_area_t     area;

    vlc_gl_t              *gl;
    vout_display_opengl_t *vgl;
} vout_display_sys_t;

static void           Prepare(vout_display_t *, picture_t *, subpicture_t *, vlc_tick_t);
static void           Display(vout_display_t *, picture_t *);

static int SetViewpoint(vout_display_t *vd, const vlc_viewpoint_t *vp)
{
    vout_display_sys_t *sys = vd->sys;
    return vout_display_opengl_SetViewpoint(sys->vgl, vp);
}

static int Control(vout_display_t *vd, int query)
{
    vout_display_sys_t *sys = vd->sys;
    CommonControl(vd, &sys->area, query);
    return VLC_SUCCESS;
}

static const struct vlc_window_operations embedVideoWindow_Ops =
{
};

static vlc_window_t *EmbedVideoWindow_Create(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    vlc_window_t *wnd = vlc_object_create(vd, sizeof(vlc_window_t));
    if (!wnd)
        return NULL;

    wnd->type = VLC_WINDOW_TYPE_HWND;
    wnd->handle.hwnd = CommonVideoHWND(&sys->area);
    wnd->ops = &embedVideoWindow_Ops;
    return wnd;
}

static const struct vlc_display_operations ops = {
    .close = Close,
    .prepare = Prepare,
    .display = Display,
    .control = Control,
    .set_viewpoint = SetViewpoint,
};

/**
 * It creates an OpenGL vout display.
 */
static int Open(vout_display_t *vd,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys;

    /* do not use OpenGL on XP unless forced */
    if(!vd->obj.force && !IsWindowsVistaOrGreater())
        return VLC_EGENERIC;

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    /* */
    CommonInit(&sys->area, vd->source);
    if (CommonWindowInit(vd, &sys->area,
                   vd->source->projection_mode != PROJECTION_MODE_RECTANGULAR))
        goto error;
    CommonPlacePicture(vd, &sys->area);

    vlc_window_SetTitle(vd->cfg->window, VOUT_TITLE " (OpenGL output)");

    vout_display_cfg_t embed_cfg = *vd->cfg;
    embed_cfg.window = EmbedVideoWindow_Create(vd);
    if (!embed_cfg.window)
        goto error;

    char *modlist = var_InheritString(embed_cfg.window, "gl");
    sys->gl = vlc_gl_Create(&embed_cfg, VLC_OPENGL, modlist, NULL);
    free(modlist);
    if (!sys->gl)
    {
        vlc_object_delete(embed_cfg.window);
        goto error;
    }

    vlc_gl_Resize (sys->gl, vd->cfg->display.width, vd->cfg->display.height);

    const vlc_fourcc_t *subpicture_chromas;
    if (vlc_gl_MakeCurrent (sys->gl))
        goto error;
    sys->vgl = vout_display_opengl_New(fmtp, &subpicture_chromas, sys->gl,
                                       &vd->cfg->viewpoint, context);
    vlc_gl_ReleaseCurrent (sys->gl);
    if (!sys->vgl)
        goto error;

    /* Setup vout_display now that everything is fine */
    vd->info.subpicture_chromas = subpicture_chromas;

    vd->ops = &ops;

    return VLC_SUCCESS;

error:
    Close(vd);
    return VLC_EGENERIC;
}

/**
 * It destroys an OpenGL vout display.
 */
static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    vlc_gl_t *gl = sys->gl;

    if (gl)
    {
        vlc_window_t *surface = gl->surface;
        if (sys->vgl)
        {
            vlc_gl_MakeCurrent (gl);
            vout_display_opengl_Delete(sys->vgl);
            vlc_gl_ReleaseCurrent (gl);
        }
        vlc_gl_Delete(gl);
        vlc_object_delete(surface);
    }

    CommonWindowClean(&sys->area);

    free(sys);
}

/* */
static void Prepare(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture,
                    vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;

    if (vlc_gl_MakeCurrent (sys->gl) != VLC_SUCCESS)
        return;
    if (sys->area.place_changed)
    {
        struct vout_display_placement place_cfg = vd->cfg->display;
        vout_display_place_t place;

        /* Reverse vertical alignment as the GL tex are Y inverted */
        if (place_cfg.align.vertical == VLC_VIDEO_ALIGN_TOP)
            place_cfg.align.vertical = VLC_VIDEO_ALIGN_BOTTOM;
        else if (place_cfg.align.vertical == VLC_VIDEO_ALIGN_BOTTOM)
            place_cfg.align.vertical = VLC_VIDEO_ALIGN_TOP;

        vout_display_PlacePicture(&place, vd->source, &place_cfg);

        const int width  = place.width;
        const int height = place.height;
        vlc_gl_Resize (sys->gl, width, height);
        vout_display_opengl_SetOutputSize(sys->vgl, width, height);
        vout_display_opengl_Viewport(sys->vgl, place.x, place.y, width, height);
        sys->area.place_changed = false;
    }
    vout_display_opengl_Prepare (sys->vgl, picture, subpicture);
    vlc_gl_ReleaseCurrent (sys->gl);
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(picture);

    if (vlc_gl_MakeCurrent (sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Display(sys->vgl);
        vlc_gl_ReleaseCurrent (sys->gl);
        vlc_gl_Swap(sys->gl);
    }
}
