#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_window.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>
#include <vlc_mouse.h>
#include <vlc_threads.h>

struct vlc_window_sys_t {
    struct vlc_window *embed_window;
    vlc_gl_t *embed_gl;
    struct vlc_window_owner window_owner;
    bool enabled;

    vlc_cond_t cond;
    vlc_mutex_t lock;
    float vsync_interval;
    vlc_thread_t vsync_generator;
};

/**
 * Owner forwarding function:
 *     forward the events to the parent window owner
 */

struct embed_ack_data {
    void *opaque;
    void (*ack)(struct vlc_window *wnd, unsigned width, unsigned height, void *);
};

static void EmbedAckResize(struct vlc_window *parent, unsigned width, unsigned height, void *opaque)
{
    struct vlc_window_sys_t *sys = parent->sys;
    struct vlc_window *wnd = sys->embed_window;

    struct embed_ack_data *ack_data = opaque;
    if (ack_data->ack == NULL)
        return;

    ack_data->ack(wnd, width, height, ack_data->opaque);
}

static void EmbedResized(struct vlc_window *wnd, unsigned width, unsigned height,
                         vlc_window_ack_cb ack_cb, void *opaque)
{
    struct vlc_window *parent = wnd->owner.sys;
    struct vlc_window_sys_t *sys = parent->sys;

    /* Resize happened during Open() of submodule */
    if (sys->embed_window == NULL)
        sys->embed_window = wnd;

    struct embed_ack_data data = { .opaque = opaque, .ack = ack_cb };
    parent->owner.cbs->resized(parent, width, height, EmbedAckResize, &data);
}

static void EmbedClosed(struct vlc_window *wnd)
{
    struct vlc_window *parent = wnd->owner.sys;
    vlc_window_ReportClose(parent);
}

static void EmbedStateChanged(struct vlc_window *wnd, unsigned state)
{
    struct vlc_window *parent = wnd->owner.sys;
    vlc_window_ReportState(parent, state);
}

static void EmbedWindowed(struct vlc_window *wnd)
{
    struct vlc_window *parent = wnd->owner.sys;
    vlc_window_ReportWindowed(parent);
}

static void EmbedFullscreened(struct vlc_window *wnd, const char *id)
{
    struct vlc_window *parent = wnd->owner.sys;
    vlc_window_ReportFullscreen(parent, id);
}

static void EmbedMouseEvent(struct vlc_window *wnd,
                            const vlc_window_mouse_event_t *mouse)
{
    struct vlc_window *parent = wnd->owner.sys;
    if (parent->owner.cbs->mouse_event != NULL)
        parent->owner.cbs->mouse_event(parent, mouse);
}

static void EmbedKeyboardEvent(struct vlc_window *wnd,
                               unsigned key)
{
    struct vlc_window *parent = wnd->owner.sys;
    if (parent->owner.cbs->keyboard_event != NULL)
        parent->owner.cbs->keyboard_event(parent, key);
}

static void EmbedOutputEvent(struct vlc_window *wnd,
                             const char *id, const char *desc)
{
    struct vlc_window *parent = wnd->owner.sys;
    vlc_window_ReportOutputDevice(parent, id, desc);
}

static void EmbedVisibilityChanged(struct vlc_window *wnd,
                                   enum vlc_window_visibility visibility)
{
    struct vlc_window *parent = wnd->owner.sys;
    vlc_window_ReportVisibilityChanged(parent, visibility);
}

static void EmbedVsyncReached(struct vlc_window *wnd,
                              vlc_tick_t pts)
{
    struct vlc_window *parent = wnd->owner.sys;
    vlc_window_ReportVsyncReached(parent, pts);
}

/**
 * Operations
 */

static int WindowEnable(struct vlc_window *wnd,
                        const vlc_window_cfg_t *cfg)
{
    struct vlc_window_sys_t *sys = wnd->sys;

    if (sys->embed_window == NULL)
    {
        static const struct vlc_window_callbacks wnd_cbs =
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
            .vsync_reached = EmbedVsyncReached,
        };

        sys->window_owner = (const struct vlc_window_owner)
        {
            .sys = wnd,
            .cbs = &wnd_cbs,
        };

        sys->embed_window = vlc_window_New(&wnd->obj, NULL, &sys->window_owner, cfg);
        if (sys->embed_window == NULL)
            return VLC_EGENERIC;
    }

    if (!sys->enabled)
    {
        msg_Info(wnd, "Enabling SK window");
        int ret = vlc_window_Enable(sys->embed_window);
        sys->enabled = ret == VLC_SUCCESS;
        return ret;
    }
    vlc_window_SetSize(sys->embed_window, cfg->width, cfg->height);
    sys->enabled = true;
    return VLC_SUCCESS;
}

static void WindowDisable(struct vlc_window *wnd)
{
    struct vlc_window_sys_t *sys = wnd->sys;

    if (var_InheritBool(wnd, "sk-keep-last-frame"))
        return;

    msg_Info(wnd, "Disabling SK window");

    if (sys->embed_gl != NULL)
    {
        vlc_gl_Delete(sys->embed_gl);
        sys->embed_gl = NULL;
    }

    vlc_window_Disable(sys->embed_window);
    sys->enabled = false;
}

static void WindowClose(struct vlc_window *wnd)
{
    struct vlc_window_sys_t *sys = wnd->sys;

    if (sys->embed_gl != NULL)
        vlc_gl_Delete(sys->embed_gl);

    if (sys->enabled)
        vlc_window_Disable(sys->embed_window);

    vlc_window_Delete(sys->embed_window);

    if (sys->vsync_interval > 0.f)
    {
        vlc_cancel(sys->vsync_generator);
        vlc_join(sys->vsync_generator, NULL);
    }
}

static void cleanupMutex(void *opaque)
{
    vlc_mutex_unlock(opaque);
}

static void *GenerateVSYNC(void *opaque)
{
    struct vlc_window *wnd = opaque;
    struct vlc_window_sys_t *sys = wnd->sys;

    vlc_mutex_lock(&sys->lock);
    vlc_cleanup_push(cleanupMutex, &sys->lock);
    vlc_tick_t last_vsync = vlc_tick_now();
    for (;;)
    {
        vlc_tick_t vsync_interval = VLC_TICK_FROM_MS(sys->vsync_interval);
        vlc_tick_t next_vsync = last_vsync + vsync_interval;
        while (vlc_cond_timedwait(&sys->cond, &sys->lock, next_vsync) == 0) {}
        int canc = vlc_savecancel();
        vlc_window_ReportVsyncReached(wnd, next_vsync + vsync_interval);
        msg_Info(wnd, "Generate VSYNC %d", (int)MS_FROM_VLC_TICK(next_vsync));
        vlc_restorecancel(canc);
        last_vsync = next_vsync;
        vlc_testcancel();
    }
    vlc_cleanup_pop();
    return NULL;
}

static int WindowOpen(struct vlc_window *wnd)
{
    vlc_object_t *parent = vlc_object_parent(wnd);

    vlc_value_t var;
    if (var_GetChecked(parent, "skwrapper-output", VLC_VAR_BOOL, &var) == VLC_SUCCESS)
        if (var.b_bool)
            return VLC_EGENERIC;

    struct vlc_window_sys_t *sys = vlc_obj_malloc(VLC_OBJECT(wnd), sizeof *sys);
    sys->embed_gl = NULL;
    sys->enabled = false;
    vlc_cond_init(&sys->cond);
    vlc_mutex_init(&sys->lock);
    vlc_mutex_lock(&sys->lock);
    wnd->sys = sys;

    var_Create(wnd, "skwrapper-output", VLC_VAR_BOOL);
    var_SetBool(wnd, "skwrapper-output", true);

    sys->vsync_interval = var_InheritFloat(wnd, "sk-generate-vsync");
    if (sys->vsync_interval > 0.f && vlc_clone(&sys->vsync_generator,
                GenerateVSYNC, wnd) != VLC_SUCCESS)
    {
        vlc_assert_unreachable();
        goto error;
    }
    sys->embed_window = NULL;

    static const struct vlc_window_operations vlc_window_ops =
    {
        .enable = WindowEnable,
        .disable = WindowDisable,
        .destroy = WindowClose,
    };
    wnd->ops = &vlc_window_ops;
    wnd->type = VOUT_WINDOW_TYPE_SK;
    wnd->handle.sk = sys;

    vlc_mutex_unlock(&sys->lock);
    return VLC_SUCCESS;

error:
    // TODO Clone error
    vlc_mutex_unlock(&sys->lock);
    var_Destroy(wnd, "skwrapper-output");
    wnd->sys = NULL;
    return VLC_EGENERIC;
}

static int OpenGLMakeCurrent(vlc_gl_t *gl)
{
    struct vlc_window_sys_t *sys = gl->sys;
    return vlc_gl_MakeCurrent(sys->embed_gl);
}

static void OpenGLReleaseCurrent(vlc_gl_t *gl)
{
    struct vlc_window_sys_t *sys = gl->sys;
    vlc_gl_ReleaseCurrent(sys->embed_gl);
}

static void OpenGLResize(vlc_gl_t *gl, unsigned width, unsigned height)
{
    struct vlc_window_sys_t *sys = gl->sys;
    vlc_gl_Resize(sys->embed_gl, width, height);
}

static void OpenGLSwap(vlc_gl_t *gl)
{
    struct vlc_window_sys_t *sys = gl->sys;
    vlc_gl_Swap(sys->embed_gl);
}

static void *OpenGLGetProcAddress(vlc_gl_t *gl, const char *name)
{
    struct vlc_window_sys_t *sys = gl->sys;
    return vlc_gl_GetProcAddress(sys->embed_gl, name);
}

static void OpenGLDestroy(vlc_gl_t *gl)
{
    struct vlc_window_sys_t *sys = gl->sys;
    if (var_InheritBool(gl, "sk-keep-last-frame"))
        return;

    if (sys->embed_gl != NULL)
    {
        vlc_gl_Delete(sys->embed_gl);
        sys->embed_gl = NULL;
    }
}

static int OpenGLOpen(vlc_gl_t *gl, unsigned width, unsigned height)
{
    if (gl->surface == NULL || gl->surface->type != VOUT_WINDOW_TYPE_SK)
        return VLC_EGENERIC;

    struct vlc_window_sys_t *sys = gl->surface->handle.sk;
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

    sys->embed_gl = vlc_gl_Create(&cfg, gl->api_type, NULL);
    if (sys->embed_gl == NULL)
        return VLC_EGENERIC;

    static const struct vlc_gl_operations gl_ops =
    {
        .make_current = OpenGLMakeCurrent,
        .release_current = OpenGLReleaseCurrent,
        .resize = OpenGLResize,
        .swap = OpenGLSwap,
        .get_proc_address = OpenGLGetProcAddress,
        .close = OpenGLDestroy,
    };

end:
    gl->ops = &gl_ops;
    gl->sys = sys;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname (N_("SK video behaviour"))
    set_description (N_("SK specific video modules"))
    set_subcategory (SUBCAT_VIDEO_VOUT)

    add_bool( "sk-keep-last-frame", false, "Keep last frame",
              "Keep the OpenGL surface and the window between media")

    add_float( "sk-generate-vsync", -1.f, "Generate new VSYNC",
              "Generate new VSYNC")

    add_submodule()
        set_capability ("opengl es2", 1000)
        set_callback(OpenGLOpen)
        add_shortcut ("sk")

    add_submodule()
        set_capability ("opengl", 1000)
        set_callback(OpenGLOpen)
        add_shortcut ("sk")

    add_submodule()
        set_capability ("vout window", 1000)
        set_callback(WindowOpen)
        add_shortcut ("sk")

vlc_module_end()
