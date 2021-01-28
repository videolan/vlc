/*****************************************************************************
 * ios.m: iOS OpenGL ES provider
 *****************************************************************************
 * Copyright (C) 2001-2017 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Rémi Denis-Courmont
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Eric Petit <titer@m0k.org>
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

#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <QuartzCore/QuartzCore.h>
#import <dlfcn.h>

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_vout_display.h>
#import <vlc_opengl.h>
#import <vlc_dialog.h>
#import "opengl/filter_draw.h"
#import "opengl/renderer.h"
#import "opengl/vout_helper.h"

/**
 * Forward declarations
 */
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmt, vlc_video_context *context);
static void Close(vout_display_t *vd);

static void PictureRender(vout_display_t *, picture_t *, subpicture_t *, vlc_tick_t);
static void PictureDisplay(vout_display_t *, picture_t *);
static int Control(vout_display_t*, int);

/**
 * Module declaration
 */
vlc_module_begin ()
    set_shortname("iOS vout")
    set_description("iOS OpenGL video output")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_callback_display(Open, 300)

    add_shortcut("vout_ios2", "vout_ios")
    add_glopts()

    add_opengl_submodule_renderer()
    add_opengl_submodule_draw()
vlc_module_end ()

struct vout_display_sys_t
{
    VLCOpenGLES2VideoView *glESView;

    vlc_gl_t *gl;

    vout_window_t *embed;
};

struct gl_sys
{
    VLCOpenGLES2VideoView *glESView;
    vout_display_opengl_t *vgl;
    GLuint renderBuffer;
    GLuint frameBuffer;
    EAGLContext *previousEaglContext;
};


static int SetViewpoint(vout_display_t *vd, const vlc_viewpoint_t *vp)
{
    vout_display_sys_t *sys = vd->sys;
    struct gl_sys *glsys = sys->gl->sys;
    return vout_display_opengl_SetViewpoint (glsys->vgl, vp);
}

static const struct vlc_display_operations ops = {
    Close, PictureRender, PictureDisplay, Control, NULL, SetViewpoint,
};

static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmt, vlc_video_context *context)
{
    vout_window_t *wnd = cfg->window;
    if (wnd->type != VOUT_WINDOW_TYPE_NSOBJECT)
        return VLC_EGENERIC;

    vout_display_sys_t *sys = vlc_obj_calloc(VLC_OBJECT(vd), 1, sizeof(*sys));

    if (!sys)
        return VLC_ENOMEM;

    vd->sys = sys;
    sys->gl = NULL;

    @autoreleasepool {
        /* setup the actual OpenGL ES view */

        [VLCOpenGLES2VideoView performSelectorOnMainThread:@selector(getNewView:)
                                                withObject:[NSArray arrayWithObjects:
                                                           [NSValue valueWithPointer:vd],
                                                           [NSValue valueWithPointer:wnd], nil]
                                             waitUntilDone:YES];
        if (!sys->glESView) {
            msg_Err(vd, "Creating OpenGL ES 2 view failed");
            return VLC_EGENERIC;
        }

        const vlc_fourcc_t *subpicture_chromas;

        sys->embed = cfg->window;
        sys->gl = vlc_object_create(vd, sizeof(*sys->gl));
        if (!sys->gl)
            goto bailout;

        struct gl_sys *glsys = sys->gl->sys =
            vlc_obj_malloc(VLC_OBJECT(vd), sizeof(struct gl_sys));
        if (unlikely(!sys->gl->sys))
            goto bailout;
        glsys->glESView = sys->glESView;
        glsys->vgl = NULL;
        glsys->renderBuffer = glsys->frameBuffer = 0;

        /* Initialize common OpenGL video display */
        sys->gl->make_current = GLESMakeCurrent;
        sys->gl->release_current = GLESReleaseCurrent;
        sys->gl->swap = GLESSwap;
        sys->gl->get_proc_address = OurGetProcAddress;
        sys->gl->api_type = VLC_OPENGL_ES2;

        if (vlc_gl_MakeCurrent(sys->gl) != VLC_SUCCESS)
            goto bailout;

        vout_display_opengl_t *vgl = vout_display_opengl_New(fmt, &subpicture_chromas,
                                                             sys->gl, &cfg->viewpoint,
                                                             context);
        vlc_gl_ReleaseCurrent(sys->gl);
        if (!vgl)
            goto bailout;
        glsys->vgl = vgl;

        /* Setup vout_display_t once everything is fine */
        vd->info.subpicture_chromas = subpicture_chromas;

        vd->ops = &ops;

        return VLC_SUCCESS;

    bailout:
        Close(vd);
        return VLC_EGENERIC;
    }
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    @autoreleasepool {
        BOOL flushed = NO;
        if (sys->gl != NULL) {
            struct gl_sys *glsys = sys->gl->sys;
            msg_Dbg(vd, "deleting display");

            if (likely(glsys->vgl))
            {
                int ret = vlc_gl_MakeCurrent(sys->gl);
                vout_display_opengl_Delete(glsys->vgl);
                if (ret == VLC_SUCCESS)
                {
                    vlc_gl_ReleaseCurrent(sys->gl);
                    flushed = YES;
                }
            }
            vlc_object_delete(sys->gl);
        }

        [sys->glESView cleanAndRelease:flushed];
    }
}

/*****************************************************************************
 * vout display callbacks
 *****************************************************************************/

static int Control(vout_display_t *vd, int query)
{
    vout_display_sys_t *sys = vd->sys;
    struct gl_sys *glsys = sys->gl->sys;

    switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        {
            assert(vd->cfg);

            [sys->glESView updateVoutCfg:vd->cfg withVGL:glsys->vgl];

            return VLC_SUCCESS;
        }

        default:
            msg_Err(vd, "Unknown request %d", query);
            return VLC_EGENERIC;
    }
}

static void PictureDisplay(vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    struct gl_sys *glsys = sys->gl->sys;
    VLC_UNUSED(pic);

    if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Display(glsys->vgl);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
}

static void PictureRender(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture,
                          vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;
    struct gl_sys *glsys = sys->gl->sys;

    if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Prepare(glsys->vgl, pic, subpicture);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
}
