#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>
#include <vlc_mouse.h>

struct vout_window_sys_t {
    vout_window_t *embed_window;
    vlc_gl_t *embed_gl;
    struct vout_window_owner window_owner;
    bool enabled;
};

/**
 * Owner forwarding function:
 *     forward the events to the parent window owner
 */

static void EmbedResized(struct vout_window_t *wnd, unsigned width, unsigned height,
                         void (*ack_cb)(struct vout_window_t *, void*), void *opaque)
{
    vout_window_t *parent = wnd->owner.sys;
    vout_window_ReportSize(parent, width, height);
}

static void EmbedClosed(struct vout_window_t *wnd)
{
    vout_window_t *parent = wnd->owner.sys;
    vout_window_ReportClose(parent);
}

static void EmbedStateChanged(struct vout_window_t *wnd, unsigned state)
{
    vout_window_t *parent = wnd->owner.sys;
    vout_window_ReportState(parent, state);
}

static void EmbedWindowed(struct vout_window_t *wnd)
{
    vout_window_t *parent = wnd->owner.sys;
    vout_window_ReportWindowed(parent);
}

static void EmbedFullscreened(struct vout_window_t *wnd, const char *id)
{
    vout_window_t *parent = wnd->owner.sys;
    vout_window_ReportFullscreen(parent, id);
}

static void EmbedMouseEvent(struct vout_window_t *wnd,
                            const vout_window_mouse_event_t *mouse)
{
    vout_window_t *parent = wnd->owner.sys;
    if (parent->owner.cbs->mouse_event != NULL)
        parent->owner.cbs->mouse_event(parent, mouse);
}

static void EmbedKeyboardEvent(struct vout_window_t *wnd,
                               unsigned key)
{
    vout_window_t *parent = wnd->owner.sys;
    if (parent->owner.cbs->keyboard_event != NULL)
        parent->owner.cbs->keyboard_event(parent, key);
}

static void EmbedOutputEvent(struct vout_window_t *wnd,
                             const char *id, const char *desc)
{
    vout_window_t *parent = wnd->owner.sys;
    vout_window_ReportOutputDevice(parent, id, desc);
}

static void EmbedVisibilityChanged(struct vout_window_t *wnd,
                                   enum vout_window_visibility visibility)
{
    vout_window_t *parent = wnd->owner.sys;
    vout_window_ReportVisibilityChanged(parent, visibility);
}

/**
 * Operations
 */

static int WindowEnable(struct vout_window_t *wnd,
                        const vout_window_cfg_t *restrict cfg)
{
    struct vout_window_sys_t *sys = wnd->sys;
    if (!sys->enabled)
    {
        msg_Info(wnd, "Enabling SK window");
        int ret = vout_window_Enable(sys->embed_window, cfg);
        sys->enabled = ret == VLC_SUCCESS;
        return ret;
    }
    vout_window_SetSize(sys->embed_window, cfg->width, cfg->height);
    sys->enabled = true;
    return VLC_SUCCESS;
}

static void WindowDisable(struct vout_window_t *wnd)
{
    struct vout_window_sys_t *sys = wnd->sys;

    if (var_InheritBool(wnd, "sk-keep-last-frame"))
        return;

    msg_Info(wnd, "Disabling SK window");

    if (sys->embed_gl != NULL)
    {
        vlc_gl_Release(sys->embed_gl);
        sys->embed_gl = NULL;
    }

    vout_window_Disable(sys->embed_window);
    sys->enabled = false;
}

static void WindowClose(struct vout_window_t *wnd)
{
    struct vout_window_sys_t *sys = wnd->sys;

    if (sys->embed_gl != NULL)
        vlc_gl_Release(sys->embed_gl);

    if (sys->enabled)
        vout_window_Disable(sys->embed_window);

    vout_window_Delete(sys->embed_window);
}

static int WindowOpen(struct vout_window_t *wnd)
{
    vlc_object_t *parent = vlc_object_parent(wnd);

    vlc_value_t var;
    if (var_GetChecked(parent, "skwrapper-output", VLC_VAR_BOOL, &var) == VLC_SUCCESS)
        if (var.b_bool)
            return VLC_EGENERIC;

    struct vout_window_sys_t *sys = vlc_obj_malloc(wnd, sizeof *sys);
    sys->embed_gl = NULL;
    sys->enabled = false;

    var_Create(wnd, "skwrapper-output", VLC_VAR_BOOL);
    var_SetBool(wnd, "skwrapper-output", true);


    static const struct vout_window_callbacks wnd_cbs =
    {
        .resized = EmbedResized,
        .closed = EmbedClosed,
        .state_changed = EmbedStateChanged,
        .windowed = EmbedWindowed,
        .fullscreened = EmbedFullscreened,
        .mouse_event = EmbedMouseEvent,
        .keyboard_event = EmbedKeyboardEvent,
        .output_event = EmbedOutputEvent,
        .visibility_changed = EmbedVisibilityChanged,
    };

    sys->window_owner = (const struct vout_window_owner)
    {
        .sys = wnd,
        .cbs = &wnd_cbs,
    };

    sys->embed_window = vout_window_New(&wnd->obj, NULL, &sys->window_owner);
    if (sys->embed_window == NULL)
        goto error;

    static const struct vout_window_operations vout_window_ops =
    {
        .enable = WindowEnable,
        .disable = WindowDisable,
        .destroy = WindowClose,
    };
    wnd->ops = &vout_window_ops;
    wnd->type = VOUT_WINDOW_TYPE_SK;
    wnd->handle.sk = sys;
    wnd->sys = sys;

    return VLC_SUCCESS;

error:
    var_Destroy(wnd, "skwrapper-output");
    return VLC_EGENERIC;
}

static int OpenGLMakeCurrent(vlc_gl_t *gl)
{
    struct vout_window_sys_t *sys = gl->sys;
    return vlc_gl_MakeCurrent(sys->embed_gl);
}

static void OpenGLReleaseCurrent(vlc_gl_t *gl)
{
    struct vout_window_sys_t *sys = gl->sys;
    vlc_gl_ReleaseCurrent(sys->embed_gl);
}

static void OpenGLResize(vlc_gl_t *gl, unsigned width, unsigned height)
{
    struct vout_window_sys_t *sys = gl->sys;
    vlc_gl_Resize(sys->embed_gl, width, height);
}

static void OpenGLSwap(vlc_gl_t *gl)
{
    struct vout_window_sys_t *sys = gl->sys;
    vlc_gl_Swap(sys->embed_gl);
}

static void *OpenGLGetProcAddress(vlc_gl_t *gl, const char *name)
{
    struct vout_window_sys_t *sys = gl->sys;
    return vlc_gl_GetProcAddress(sys->embed_gl, name);
}

static void OpenGLDestroy(vlc_gl_t *gl)
{
    struct vout_window_sys_t *sys = gl->sys;
    if (var_InheritBool(gl, "sk-keep-last-frame"))
        return;

    if (sys->embed_gl != NULL)
    {
        vlc_gl_Release(sys->embed_gl);
        sys->embed_gl = NULL;
    }
}

static int OpenGLOpen(vlc_gl_t *gl, unsigned width, unsigned height)
{
    if (gl->surface == NULL || gl->surface->type != VOUT_WINDOW_TYPE_SK)
        return VLC_EGENERIC;

    struct vout_window_sys_t *sys = gl->surface->handle.sk;
    if (sys->embed_gl != NULL)
    {
        vlc_gl_Resize(sys->embed_gl, width, height);
        goto end;
    }

    vout_display_cfg_t cfg = {
        .window = sys->embed_window,
        .display.width = width,
        .display.height = height,
    };

    sys->embed_gl = vlc_gl_Create(&cfg, VLC_OPENGL_ES2, NULL);
    if (sys->embed_gl == NULL)
        return VLC_EGENERIC;

end:
    gl->sys = sys;
    gl->make_current = OpenGLMakeCurrent;
    gl->release_current = OpenGLReleaseCurrent;
    gl->resize = OpenGLResize;
    gl->swap = OpenGLSwap;
    gl->get_proc_address = OpenGLGetProcAddress;
    gl->destroy = OpenGLDestroy;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname (N_("SK video behaviour"))
    set_description (N_("SK specific video modules"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)

    add_bool( "sk-keep-last-frame", false, "Keep last frame",
              "Keep the OpenGL surface and the window between media", true)

    add_submodule()
        set_capability ("opengl es2", 1000)
        set_callback(OpenGLOpen)
        add_shortcut ("sk")

    add_submodule()
        set_capability ("vout window", 1000)
        set_callback(WindowOpen)
        add_shortcut ("sk")

vlc_module_end()
