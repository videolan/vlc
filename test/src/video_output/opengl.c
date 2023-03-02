/*****************************************************************************
 * opengl.c: test for the OpenGL provider
 *****************************************************************************
 * Copyright (C) 2022 VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_offscreen_mock
#define MODULE_STRING "test_offscreen_mock"
#undef VLC_DYNAMIC_PLUGIN

#include "../../libvlc/test.h"
#include "../../../lib/libvlc_internal.h"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_opengl.h>
#include <vlc_filter.h>
#include <vlc_modules.h>
#include <vlc_vout_display.h>

static char dummy;
static bool swapped;
static bool swapped_offscreen;

/**
 * Dummy decoder device implementation
 */

static void DecoderDeviceClose(struct vlc_decoder_device *device)
    { VLC_UNUSED(device); }

static const struct vlc_decoder_device_operations decoder_device_ops =
    { .close = DecoderDeviceClose, };

static int OpenDecoderDevice(
        struct vlc_decoder_device *device,
        vlc_window_t *window
) {
    VLC_UNUSED(window);

    /* Note: the decoder device type could be anything since we'll hook
     *       it in the test code below. */
    device->type = VLC_DECODER_DEVICE_VAAPI;
    device->ops = &decoder_device_ops;
    return VLC_SUCCESS;
}

/**
 * Dummy vout_window implementation
 */
static const struct vlc_window_operations wnd_ops = {
    .destroy = NULL,
};

static int OpenWindow(vlc_window_t *wnd)
{
    wnd->type = VLC_WINDOW_TYPE_DUMMY;
    wnd->ops = &wnd_ops;
    return VLC_SUCCESS;
}

/**
 * Dummy OpenGL provider
 */

static int OpenGLMakeCurrent(vlc_gl_t *gl)
{
    VLC_UNUSED(gl);
    return VLC_SUCCESS;
}

static void OpenGLReleaseCurrent(vlc_gl_t *gl)
{
    (void)gl;
}

static void OpenGLSwap(vlc_gl_t *gl)
{
    (void)gl;
    swapped = true;
}

static picture_t *OpenGLSwapOffscreen(vlc_gl_t *gl)
{
    (void)gl;
    swapped_offscreen = true;
    return NULL;
}

static void *OpenGLGetSymbol(vlc_gl_t *gl, const char *procname)
{
    (void)gl; (void)procname;
    if (strcmp(procname, "dummy") == 0)
        return &dummy;
    return NULL;
}

static void OpenGLClose(vlc_gl_t *gl)
{
    (void)gl;
}

static int
OpenOpenGLCommon(
        vlc_gl_t *gl, unsigned width, unsigned height,
        bool offscreen, enum vlc_gl_api_type api_type,
        const struct vlc_gl_cfg *cfg)
{
    (void)width; (void)height; (void) cfg;
    assert(gl->api_type == api_type);

    static const struct vlc_gl_operations onscreen_ops =
    {
        .make_current = OpenGLMakeCurrent,
        .release_current = OpenGLReleaseCurrent,
        .resize = NULL,
        .get_proc_address = OpenGLGetSymbol,
        .swap = OpenGLSwap,
        .close = OpenGLClose,
    };

    static const struct vlc_gl_operations offscreen_ops =
    {
        .make_current = OpenGLMakeCurrent,
        .release_current = OpenGLReleaseCurrent,
        .resize = NULL,
        .get_proc_address = OpenGLGetSymbol,
        .swap_offscreen = OpenGLSwapOffscreen,
        .close = OpenGLClose,
    };

    gl->ops = offscreen ? &offscreen_ops : &onscreen_ops;
    return VLC_SUCCESS;
}

static int
OpenOpenGL(vlc_gl_t *gl, unsigned width, unsigned height,
           const struct vlc_gl_cfg *cfg)
    { return OpenOpenGLCommon(gl, width, height, false, VLC_OPENGL, cfg); };

static int
OpenOpenGLES(vlc_gl_t *gl, unsigned width, unsigned height,
             const struct vlc_gl_cfg *cfg)
    { return OpenOpenGLCommon(gl, width, height, false, VLC_OPENGL_ES2, cfg); };

static int
OpenOpenGLOffscreen(vlc_gl_t *gl, unsigned width, unsigned height,
                    const struct vlc_gl_cfg *cfg)
    { return OpenOpenGLCommon(gl, width, height, true, VLC_OPENGL, cfg); };

static int
OpenOpenGLESOffscreen(vlc_gl_t *gl, unsigned width, unsigned height,
                      const struct vlc_gl_cfg *cfg)
    { return OpenOpenGLCommon(gl, width, height, true, VLC_OPENGL_ES2, cfg); };

/**
 * Inject the mocked modules as a static plugin:
 *  - decoder device for generating video context and testing release
 *  - opengl offscreen for generating video context and using decoder device
 **/
vlc_module_begin()
    set_callback(OpenDecoderDevice)
    set_capability("decoder device", 1000)
    add_shortcut("test_offscreen")

    add_submodule()
        set_callback(OpenWindow)
        set_capability("vout window", 1)

    add_submodule()
        set_callback_opengl(OpenOpenGL, 1)

    add_submodule()
        set_callback_opengl_es2(OpenOpenGLES, 1)

    add_submodule()
        set_callback_opengl_offscreen(OpenOpenGLOffscreen, 1)

    add_submodule()
        set_callback_opengl_es2_offscreen(OpenOpenGLESOffscreen, 1)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

static void test_opengl_offscreen(vlc_object_t *root, enum vlc_gl_api_type api_type)
{
    struct vlc_decoder_device *device =
        vlc_decoder_device_Create(root, NULL);
    assert(device != NULL);

    vlc_gl_t *gl = vlc_gl_CreateOffscreen(
            root, device, 800, 600, api_type, MODULE_STRING, NULL);
    assert(gl != NULL);
    vlc_decoder_device_Release(device);

    assert(vlc_gl_MakeCurrent(gl) == VLC_SUCCESS);
    assert(vlc_gl_GetProcAddress(gl, "dummy") == &dummy);
    vlc_gl_ReleaseCurrent(gl);

    swapped_offscreen = false;
    vlc_gl_SwapOffscreen(gl);
    assert(swapped_offscreen == true);

    vlc_gl_Delete(gl);
}

static void test_opengl(vlc_object_t *root, enum vlc_gl_api_type api_type)
{
    const vlc_window_cfg_t wnd_cfg = { .width = 800, .height = 600, };
    const vlc_window_owner_t owner = { .sys = NULL };
    vlc_window_t *wnd = vlc_window_New(root, MODULE_STRING, &owner, &wnd_cfg);
    assert(wnd != NULL && wnd->ops == &wnd_ops);

    const vout_display_cfg_t cfg = {
        .window = wnd,
        .display.width = wnd_cfg.width,
        .display.height = wnd_cfg.height,
    };
    vlc_gl_t *gl = vlc_gl_Create(&cfg, api_type, MODULE_STRING, NULL);
    assert(gl != NULL);

    assert(vlc_gl_MakeCurrent(gl) == VLC_SUCCESS);
    assert(vlc_gl_GetProcAddress(gl, "dummy") == &dummy);
    vlc_gl_ReleaseCurrent(gl);

    swapped = false;
    vlc_gl_Swap(gl);
    assert(swapped == true);

    vlc_gl_Delete(gl);
    vlc_window_Delete(wnd);
}

int main( int argc, char **argv )
{
    (void)argc; (void)argv;
    test_init();

    const char * const vlc_argv[] = {
        "-vvv", "--aout=dummy", "--text-renderer=dummy",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(vlc_argv), vlc_argv);
    vlc_object_t *root = &vlc->p_libvlc_int->obj;

    test_opengl(root, VLC_OPENGL);
    test_opengl(root, VLC_OPENGL_ES2);
    test_opengl_offscreen(root, VLC_OPENGL);
    test_opengl_offscreen(root, VLC_OPENGL_ES2);

    libvlc_release(vlc);
    return 0;
}
