/*****************************************************************************
 * egl_gbm.c
 * egl for wayland using GBM allocated buffers
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wayland-egl.h>
#include "video_output/wayland/registry.h"
#include "video_output/wayland/linux-dmabuf-client-protocol.h"

#include <gbm.h>
#include <fcntl.h>
#include <xf86drm.h>
#include "gl_common.h"

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_plugin.h>
#include <vlc_opengl.h>
#include <vlc_window.h>
#include <vlc_fs.h>

static const char *clientExts;

static PFNGLFINISHPROC Finish;

#ifdef EGL_EXT_platform_base
static PFNEGLGETPLATFORMDISPLAYEXTPROC GetPlatformDisplayEXT;
static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC CreatePlatformWindowSurfaceEXT;
#endif

typedef struct vlc_gl_sys_t
{
    struct {
        EGLDisplay display;
        EGLSurface surface;
        EGLContext context;
        EGLConfig config;
        bool is_current;
    } egl;

    struct {
        struct wl_event_queue* wl_queue;
        bool need_roundtrip;
        struct zwp_linux_dmabuf_v1* dmabuf;
        struct zwp_linux_dmabuf_feedback_v1* feedback;
    }  wayland;

    struct {
        struct gbm_device* device;
        struct gbm_surface* surface;
        struct gbm_bo* bo_prev;
        struct gbm_bo* bo_next;
        uint32_t format;
    } gbm;

} vlc_gl_sys_t;

static bool CheckAPI(EGLDisplay dpy, const char* api)
{
    const char* apis = eglQueryString(dpy, EGL_CLIENT_APIS);
    return vlc_gl_StrHasToken(apis, api);
}

static bool CheckClientExt(const char* name)
{
    return vlc_gl_StrHasToken(clientExts, name);
}

struct gl_api
{
    const char name[10];
    EGLenum    api;
    EGLint     min_minor;
    EGLint     render_bit;
    EGLint     attr[3];
};

static int MakeCurrent (vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;
    if (sys->egl.surface == EGL_NO_SURFACE)
    {
        msg_Err(gl, "no surface to make current");
        return VLC_EGENERIC;
    }

    if (eglMakeCurrent(sys->egl.display, sys->egl.surface, sys->egl.surface,
                       sys->egl.context) != EGL_TRUE)
    {
        msg_Err(gl, "failed to make current");
        return VLC_EGENERIC;
    }
    sys->egl.is_current = true;
    return VLC_SUCCESS;
}

static void ReleaseCurrent (vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;

    eglMakeCurrent(sys->egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    sys->egl.is_current = false;
}

static EGLSurface CreateSurface(vlc_gl_t* gl, unsigned int width, unsigned int height)
{
    vlc_gl_sys_t* sys = gl->sys;

    sys->gbm.surface =  gbm_surface_create(
        sys->gbm.device,
        width, height,
        sys->gbm.format,
        GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);

    if (!sys->gbm.surface)
    {
        // Nvidia proprietary driver doesn't support creation flags
        sys->gbm.surface =  gbm_surface_create(
           sys->gbm.device,
           width, height,
           sys->gbm.format,
           0);
    }

    if (!sys->gbm.surface)
    {
        msg_Err(gl, "can't create gbm surface");
        return EGL_NO_SURFACE;
    }

#ifdef EGL_EXT_platform_base
    return CreatePlatformWindowSurfaceEXT(sys->egl.display, sys->egl.config, sys->gbm.surface, NULL);
#else
    return eglCreateWindowSurface(sys->egl.display, sys->egl.config, (EGLNativeWindowType)sys->gbm.surface, NULL);
#endif
}

static void Resize(vlc_gl_t* gl, unsigned width, unsigned height)
{
    vlc_gl_sys_t* sys = gl->sys;

    if (sys->egl.surface)
        eglDestroySurface(sys->egl.display, sys->egl.surface);

    if (sys->gbm.surface)
        gbm_surface_destroy(sys->gbm.surface);

    sys->gbm.surface = NULL;
    sys->gbm.bo_prev = NULL;
    sys->gbm.bo_next = NULL;

    sys->egl.surface =  CreateSurface(gl, width, height);
}

static void ReleaseBoPrivateData(struct gbm_bo*  bo, void* data)
{
    VLC_UNUSED(bo);
    struct wl_buffer* buffer = (struct wl_buffer*) data;
    wl_buffer_destroy(buffer);
}

static void LockNext(vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;

    struct gbm_bo* bo = gbm_surface_lock_front_buffer(sys->gbm.surface);
    sys->gbm.bo_prev = sys->gbm.bo_next;
    sys->gbm.bo_next = bo;
}

static void BindBuffer(vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;

    struct gbm_bo* bo = sys->gbm.bo_next;

    if (!bo)
        return;


    uint32_t height = gbm_bo_get_height(bo);
    uint32_t width = gbm_bo_get_width(bo);

    struct wl_buffer* buffer = (struct wl_buffer*)gbm_bo_get_user_data(bo);

    if (!buffer)
    {
        uint64_t modifier = gbm_bo_get_modifier(bo);
        uint32_t format = gbm_bo_get_format(bo);

        struct zwp_linux_buffer_params_v1* params = zwp_linux_dmabuf_v1_create_params(sys->wayland.dmabuf);
        for (int i = 0; i < gbm_bo_get_plane_count(bo); ++i)
        {
            int fd = gbm_bo_get_fd_for_plane(bo, i);
            uint32_t stride = gbm_bo_get_stride_for_plane(bo, i);
            uint32_t offset = gbm_bo_get_offset(bo, i);

            if (fd <= 0)
            {
                msg_Warn(gl, "no fd for plane");
                continue;
            }

            zwp_linux_buffer_params_v1_add(params, fd, i, offset,
                                           stride,
                                           modifier >> 32,
                                           modifier & 0xffffffff);
        }
        buffer = zwp_linux_buffer_params_v1_create_immed(params, width, height, format, 0);

        zwp_linux_buffer_params_v1_destroy(params);

        if (buffer)
            gbm_bo_set_user_data(bo, buffer, ReleaseBoPrivateData);
    }

    wl_surface_attach(gl->surface->handle.wl, buffer, 0, 0);
    wl_surface_damage(gl->surface->handle.wl, 0, 0, width, height);
    wl_surface_commit(gl->surface->handle.wl);

    wl_display_roundtrip_queue(gl->surface->display.wl, sys->wayland.wl_queue);
}

static void FreePrev(vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;
    if (sys->gbm.bo_prev)
    {
        gbm_surface_release_buffer(sys->gbm.surface, sys->gbm.bo_prev);
        sys->gbm.bo_prev = NULL;
    }
}

static void SwapBuffers(vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;

    if (!gbm_surface_has_free_buffers(sys->gbm.surface))
    {
        msg_Warn(gl, "no free buffers");
        return;
    }

    if (!sys->egl.is_current)
    {
        EGLSurface s_read = eglGetCurrentSurface(EGL_READ);
        EGLSurface s_draw = eglGetCurrentSurface(EGL_DRAW);
        EGLContext previous_context = eglGetCurrentContext();

        eglMakeCurrent(sys->egl.display, sys->egl.surface, sys->egl.surface, sys->egl.context);

        eglSwapBuffers(sys->egl.display, sys->egl.surface);
        Finish();

        eglMakeCurrent(sys->egl.display, s_read, s_draw, previous_context);
    }
    else
    {
        eglSwapBuffers(sys->egl.display, sys->egl.surface);
        Finish();
    }

    LockNext(gl);

    BindBuffer(gl);

    FreePrev(gl);
}

static void DestroySurface(vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;
    gbm_surface_destroy(sys->gbm.surface);
}


static EGLConfig MatchEglConfigToGBM(vlc_gl_t* gl, EGLint cfgc, EGLConfig* cfgv, EGLint targetFormat )
{
    vlc_gl_sys_t* sys = gl->sys;
    for (int i = 0; i < cfgc; ++i) {
        EGLint nativeFormat;
        if (!eglGetConfigAttrib(sys->egl.display, cfgv[i], EGL_NATIVE_VISUAL_ID, &nativeFormat))
            continue;

        if (nativeFormat == targetFormat)
            return cfgv[i];
    }
    return EGL_CAST(EGLConfig,0);
}

static bool FindEglConfig(vlc_gl_t* gl, bool need_alpha)
{
    vlc_gl_sys_t* sys = gl->sys;

    //we require a finer config type
    const EGLint conf_attr[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_BUFFER_SIZE, 32,
        EGL_ALPHA_SIZE, need_alpha ? 8 : 0,
        EGL_RENDERABLE_TYPE, gl->api_type == VLC_OPENGL_ES2 ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };

    EGLint cfgc;
    if (!eglGetConfigs(sys->egl.display, NULL, 0, &cfgc) || cfgc == 0)
        return false;

    EGLConfig* cfgv = malloc(cfgc * sizeof(EGLConfig));

    if (eglChooseConfig(sys->egl.display, conf_attr, cfgv, cfgc, &cfgc) != EGL_TRUE
        || cfgc == 0)
    {
        msg_Err (gl, "cannot choose EGL configuration");
        return  false;
    }

    static const EGLint rgbaFormats[] = {
        GBM_FORMAT_ARGB8888,
        0
    };
    static const EGLint rgbFormats[] = {
        GBM_FORMAT_XRGB8888,
        GBM_FORMAT_ARGB8888,
        0
    };

    const EGLint* gbmFormat = need_alpha ? rgbaFormats : rgbFormats;

    EGLConfig config = 0;
    while (*gbmFormat != 0)
    {
        config = MatchEglConfigToGBM(gl, cfgc, cfgv, *gbmFormat);
        if (config != 0)
            break;
        gbmFormat++;
    }

    free(cfgv);

    if (config == 0)
    {
        msg_Err(gl, "cannot choose EGL configuration matching gbm format");
        return false;
    }

    sys->gbm.format = *gbmFormat;
    sys->egl.config = config;

    return true;
}

static bool OpenDrmDevice(vlc_gl_t* gl, drmDevice* device)
{
    vlc_gl_sys_t* sys = gl->sys;

    drmDevice* devices[64];
    char* render_node = NULL;

    int n = drmGetDevices2(0, devices, ARRAY_SIZE(devices));
    for (int i = 0; i < n; ++i)
    {
        drmDevice* dev = devices[i];
        if (device && !drmDevicesEqual(device, dev))
            continue;

        if (!(dev->available_nodes & (1 << DRM_NODE_RENDER)))
            continue;

        render_node = strdup(dev->nodes[DRM_NODE_RENDER]);
        break;
    }

    drmFreeDevices(devices, n);

    if (!render_node)
        return false;

    msg_Dbg(gl, "open drm device %s", render_node);
    int fd = vlc_open(render_node, O_RDWR);

    if (fd < 0)
    {
        msg_Err(gl, "can't open drm device %s", render_node);
        free(render_node);
        return false;
    }
    free(render_node);

    sys->gbm.device = gbm_create_device(fd);
    return sys->gbm.device != NULL;

}

static void linux_dmabuf_feedback_handle_done(void* data, struct zwp_linux_dmabuf_feedback_v1* feedback)
{
    VLC_UNUSED(data);
    VLC_UNUSED(feedback);
}

static void linux_dmabuf_feedback_handle_format_table(void* data, struct zwp_linux_dmabuf_feedback_v1* feedback, int32_t fd, uint32_t size)
{
    VLC_UNUSED(data);
    VLC_UNUSED(feedback);
    VLC_UNUSED(fd);
    VLC_UNUSED(size);
}

static void linux_dmabuf_feedback_handle_main_device(void* data, struct zwp_linux_dmabuf_feedback_v1* feedback, struct wl_array* device_arr)
{
    VLC_UNUSED(feedback);
    VLC_UNUSED(device_arr);
    vlc_gl_t* gl = data;
    vlc_gl_sys_t* sys = gl->sys;

    dev_t device;
    assert(device_arr->size == sizeof(device));
    memcpy(&device, device_arr->data, sizeof(device));

    drmDevice* drmDev;
    if (drmGetDeviceFromDevId(device, /* flags */ 0, &drmDev) != 0)
    {
        msg_Warn(gl, "unable to open main device");
        return;
    }

    assert(drmDev);
    assert(sys->gbm.device == NULL);

    OpenDrmDevice(gl, drmDev);

    drmFreeDevice(&drmDev);
}

static void linux_dmabuf_feedback_handle_tranche_done(void* data, struct zwp_linux_dmabuf_feedback_v1* feedback)
{
    VLC_UNUSED(data);
    VLC_UNUSED(feedback);
}

static void linux_dmabuf_feedback_handle_tranche_target_device(void* data, struct zwp_linux_dmabuf_feedback_v1* feedback, struct wl_array* device_arr)
{
    VLC_UNUSED(data);
    VLC_UNUSED(feedback);
    VLC_UNUSED(device_arr);
}

static void linux_dmabuf_feedback_handle_tranche_formats(void* data, struct zwp_linux_dmabuf_feedback_v1* feedback, struct wl_array* indices)
{
    VLC_UNUSED(data);
    VLC_UNUSED(feedback);
    VLC_UNUSED(indices);
}

static void linux_dmabuf_feedback_handle_tranche_flags(void* data, struct zwp_linux_dmabuf_feedback_v1* feedback, uint32_t flags)
{
    VLC_UNUSED(data);
    VLC_UNUSED(feedback);
    VLC_UNUSED(flags);
}

static const struct zwp_linux_dmabuf_feedback_v1_listener linux_dmabuf_feedback_listener = {
    linux_dmabuf_feedback_handle_done,
    linux_dmabuf_feedback_handle_format_table,
    linux_dmabuf_feedback_handle_main_device,
    linux_dmabuf_feedback_handle_tranche_done,
    linux_dmabuf_feedback_handle_tranche_target_device,
    linux_dmabuf_feedback_handle_tranche_formats,
    linux_dmabuf_feedback_handle_tranche_flags,
};

static void registry_global_cb(void* data, struct wl_registry* registry,
                               uint32_t id, const char* iface, uint32_t version)
{
    vlc_gl_t* gl = data;
    vlc_gl_sys_t* sys = gl->sys;

    if (!strcmp(iface, "zwp_linux_dmabuf_v1"))
    {
        if (version <= 3)
        {
            msg_Warn(gl, "unsupported zwp_linux_dmabuf_v1 version");
        }
        else
        {
            sys->wayland.dmabuf = wl_registry_bind(registry,
                                               id, &zwp_linux_dmabuf_v1_interface, 4);
            sys->wayland.feedback = zwp_linux_dmabuf_v1_get_default_feedback(sys->wayland.dmabuf);
            zwp_linux_dmabuf_feedback_v1_add_listener(sys->wayland.feedback,
                                                      &linux_dmabuf_feedback_listener,
                                                      gl);
            sys->wayland.need_roundtrip = true;
        }
    }
}

static void registry_global_remove_cb(void* data, struct wl_registry* registry, uint32_t id)
{
    VLC_UNUSED(data);
    VLC_UNUSED(registry);
    VLC_UNUSED(id);
}

static const struct wl_registry_listener registry_cbs =
{
    registry_global_cb,
    registry_global_remove_cb,
};

static bool GetWaylandRegistryInfo(vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;

    vlc_window_t* surface = gl->surface;

    struct wl_display* display = surface->display.wl;

    sys->wayland.wl_queue = wl_display_create_queue(surface->display.wl);
    if (sys->wayland.wl_queue == NULL)
        return false;

    struct wl_display* wrapper = wl_proxy_create_wrapper(display);
    wl_proxy_set_queue((struct wl_proxy*)wrapper, sys->wayland.wl_queue);

    struct wl_registry* registry = wl_display_get_registry(wrapper);
    if (!registry)
        return false;

    wl_proxy_wrapper_destroy(wrapper);

    wl_registry_add_listener(registry, &registry_cbs, gl);
    sys->wayland.need_roundtrip = true;
    while (sys->wayland.need_roundtrip)
    {
        sys->wayland.need_roundtrip = false;
        wl_display_roundtrip_queue(display, sys->wayland.wl_queue);
    }

    wl_registry_destroy(registry);

    if (!sys->gbm.device)
        return false;

    return true;
}


static EGLDisplay OpenDisplay(vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;

    if (!CheckClientExt("EGL_KHR_platform_gbm"))
        return EGL_NO_DISPLAY;

    bool ret;

    ret = GetWaylandRegistryInfo(gl);
    if (!ret) {
        msg_Info(gl, "can't find gbm device");
        return false;
    }

    if (!sys->gbm.device)
    {
        msg_Err(gl, "can't create GBM device");
        return EGL_NO_DISPLAY;
    }

    EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, sys->gbm.device,
                                               NULL);
    if (display == EGL_NO_DISPLAY)
    {
        msg_Err(gl, "can't get egl display from gbm device");
        if (sys->gbm.device)
        {
            int fd = gbm_device_get_fd(sys->gbm.device);
            gbm_device_destroy(sys->gbm.device);
            close(fd);
        }
        return EGL_NO_DISPLAY;
    }
    return display;
}

static void ReleaseDisplay(vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;

    if (sys->wayland.feedback != NULL)
        zwp_linux_dmabuf_feedback_v1_destroy(sys->wayland.feedback);

    if (sys->wayland.dmabuf != NULL)
        zwp_linux_dmabuf_v1_destroy(sys->wayland.dmabuf);

    if (sys->gbm.device)
    {
        int fd = gbm_device_get_fd(sys->gbm.device);
        gbm_device_destroy(sys->gbm.device);
        close(fd);
    }

    if (sys->wayland.wl_queue)
        wl_event_queue_destroy(sys->wayland.wl_queue);
}

static void* GetSymbol(vlc_gl_t* gl, const char* procname)
{
    VLC_UNUSED(gl);
    return (void*)eglGetProcAddress(procname);
}

static void Close(vlc_gl_t* gl)
{
    vlc_gl_sys_t* sys = gl->sys;

    if (sys->egl.context != EGL_NO_CONTEXT)
        eglDestroyContext(sys->egl.display, sys->egl.context);

    if (sys->egl.surface != EGL_NO_SURFACE)
    {
        eglDestroySurface(sys->egl.display, sys->egl.surface);
        DestroySurface(gl);
    }

    eglTerminate(sys->egl.display);
    ReleaseDisplay(gl);
    free (sys);
}

static void InitEGL(void)
{
    static vlc_once_t once = VLC_STATIC_ONCE;

    if (unlikely(!vlc_once_begin(&once)))
    {
        clientExts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
        if (!clientExts)
            clientExts = "";

#ifdef EGL_EXT_platform_base
        GetPlatformDisplayEXT =
            (void*) eglGetProcAddress("eglGetPlatformDisplayEXT");
        CreatePlatformWindowSurfaceEXT =
            (void*) eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
#endif

        Finish = eglGetProcAddress("glFinish");

        vlc_once_complete(&once);
    }
}

/**
 * Probe EGL display availability
 */
static int Open(vlc_gl_t* gl, const struct gl_api* api,
                unsigned width, unsigned height,
                const struct vlc_gl_cfg* gl_cfg)
{
    //only wayland surface are supported
    if (gl->surface->type != VLC_WINDOW_TYPE_WAYLAND)
        return VLC_EGENERIC;

    InitEGL();

    int ret = VLC_EGENERIC;
    vlc_object_t* obj = VLC_OBJECT(gl);
    vlc_gl_sys_t* sys = calloc(1, sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    msg_Dbg(obj, "EGL client extensions: %s", clientExts);

    gl->sys = sys;
    sys->egl.display = EGL_NO_DISPLAY;
    sys->egl.surface = EGL_NO_SURFACE;
    sys->egl.context = EGL_NO_CONTEXT;
    sys->egl.is_current = false;

    sys->egl.display = OpenDisplay(gl);
    if (sys->egl.display == EGL_NO_DISPLAY)
    {
        free(sys);
        return VLC_ENOTSUP;
    }

    /* Initialize EGL display */
    EGLint major, minor;
    if (eglInitialize(sys->egl.display, &major, &minor) != EGL_TRUE)
        goto error;
    msg_Dbg(obj, "EGL version %s by %s",
            eglQueryString(sys->egl.display, EGL_VERSION),
            eglQueryString(sys->egl.display, EGL_VENDOR));

    const char* ext = eglQueryString(sys->egl.display, EGL_EXTENSIONS);
    if (*ext)
        msg_Dbg(obj, "EGL display extensions: %s", ext);

    if (major != 1 || minor < api->min_minor
        || !CheckAPI(sys->egl.display, api->name))
    {
        msg_Err(obj, "cannot select %s API", api->name);
        goto error;
    }

    ret = FindEglConfig(gl, gl_cfg->need_alpha);
    if (!ret)
        goto error;

    /* Create a drawing surface */
    sys->egl.surface = CreateSurface(gl, width, height);
    if (sys->egl.surface == EGL_NO_SURFACE)
    {
        msg_Err(obj, "cannot create EGL window surface");
        goto error;
    }

    if (eglBindAPI(api->api) != EGL_TRUE)
    {
        msg_Err(obj, "cannot bind EGL API");
        goto error;
    }

    EGLContext ctx = eglCreateContext(sys->egl.display, sys->egl.config, EGL_NO_CONTEXT,
                                      api->attr);
    if (ctx == EGL_NO_CONTEXT)
    {
        msg_Err(obj, "cannot create EGL context");
        goto error;
    }
    sys->egl.context = ctx;

    /* Initialize OpenGL callbacks */
    static const struct vlc_gl_operations ops = {
        .make_current = MakeCurrent,
        .release_current = ReleaseCurrent,
        .resize = Resize,
        .swap = SwapBuffers,
        .get_proc_address = GetSymbol,
        .close = Close,
    };
    gl->ops = &ops;

    return VLC_SUCCESS;

error:
    Close(gl);
    return ret;
}

static int OpenGLES2(vlc_gl_t* gl, unsigned width, unsigned height,
                     const struct vlc_gl_cfg* gl_cfg)
{
    static const struct gl_api api = {
      "OpenGL_ES", EGL_OPENGL_ES_API, 4, EGL_OPENGL_ES2_BIT,
      { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE },
    };
    return Open(gl, &api, width, height, gl_cfg);
}

static int OpenGL(vlc_gl_t* gl, unsigned width, unsigned height,
                  const struct vlc_gl_cfg* gl_cfg)
{
    static const struct gl_api api = {
      "OpenGL", EGL_OPENGL_API, 4, EGL_OPENGL_BIT,
      { EGL_NONE },
    };
    return Open(gl, &api, width, height, gl_cfg);
}

vlc_module_begin ()
    set_shortname (N_("EGL GBM Wayland"))
    set_description (N_("EGL GBM wayland extension for OpenGL"))
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_callback_opengl(OpenGL, 40) //lower priority than egl+wayland
    add_shortcut ("egl")

    add_submodule ()
    set_callback_opengl_es2(OpenGLES2, 40) //lower priority than egl+wayland
    add_shortcut ("egl")

    vlc_module_end ()
