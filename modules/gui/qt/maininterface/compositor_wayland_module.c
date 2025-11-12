/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "compositor_wayland_module.h"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <wayland-client.h>

#ifdef QT_HAS_WAYLAND_FRACTIONAL_SCALING
#include "viewporter-client-protocol.h"
#endif

#include <math.h>
#include <assert.h>
#include <float.h>

typedef struct
{
    struct wl_display* display;
    struct wl_event_queue* queue;
    struct wl_compositor* compositor;
    struct wl_subcompositor* subcompositor;

#ifdef QT_HAS_WAYLAND_FRACTIONAL_SCALING
    struct wp_viewport* viewport;
    struct wp_viewporter* viewporter;
#endif

    struct wl_surface* interface_surface;

    struct wl_surface* video_surface;
    struct wl_subsurface* video_subsurface;

    int buffer_scale;
    bool fractional_scale;

    // Store the size, because we need them
    // if the scale changes (if fractional)
    // to update the viewport:
    int width;
    int height;

    uint32_t compositor_interface_version;
} qtwayland_priv_t;

static void registry_global_cb(void* data, struct wl_registry* registry,
                               uint32_t id, const char* iface, uint32_t version)
{
    qtwayland_t* obj = (qtwayland_t*)data;
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;

    if (!strcmp(iface, "wl_subcompositor"))
        sys->subcompositor = (struct wl_subcompositor*)wl_registry_bind(registry, id, &wl_subcompositor_interface, version);
    if (!strcmp(iface, "wl_compositor"))
    {
        sys->compositor_interface_version = version;
        sys->compositor = (struct wl_compositor*)wl_registry_bind(registry, id, &wl_compositor_interface, version);
    }
#ifdef QT_HAS_WAYLAND_FRACTIONAL_SCALING
    if (!strcmp(iface, "wp_viewporter"))
        sys->viewporter = (struct wp_viewporter*)wl_registry_bind(registry, id, &wp_viewporter_interface, version);
#endif
}

static void registry_global_remove_cb(void* data, struct wl_registry* registry, uint32_t id)
{
    VLC_UNUSED(data);
    VLC_UNUSED(registry);
    VLC_UNUSED(id);
}

static const struct wl_registry_listener registry_cbs = {
    registry_global_cb,
    registry_global_remove_cb,
};

static void SetSize(struct qtwayland_t* obj, size_t width, size_t height)
{
    assert(obj);
    qtwayland_priv_t* const sys = (qtwayland_priv_t*)obj->p_sys;
    assert(sys);

    sys->width = width;
    sys->height = height;
}

static bool CommitSize(struct qtwayland_t* obj, bool commitSurface)
{
#ifdef QT_HAS_WAYLAND_FRACTIONAL_SCALING
    assert(obj);
    const qtwayland_priv_t* const sys = (qtwayland_priv_t*)obj->p_sys;
    assert(sys);
    if (!sys->video_surface)
        return false;
    if (sys->viewport)
    {
        // Non-positive size (except (-1, -1) pair) causes protocol error:
        if (unlikely(!((sys->width > 0 && sys->height > 0) || (sys->height == -1 && sys->width == -1))))
            return false;

        // width and height here represent the final size, after scaling
        // is taken into account. The fractional scaling protocol is not
        // necessary, because the (fractional) scale is retrieved from the
        // Qt Quick window which uses the fractional scale protocol itself
        // to determine the device pixel ratio.
        wp_viewport_set_destination(sys->viewport, sys->width, sys->height);
        if (commitSurface)
            wl_surface_commit(sys->video_surface);
        return true;
    }
#endif

    return false;
}

static void SetScale(struct qtwayland_t* obj, double scale)
{
    assert(obj);
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;
    assert(sys);

    // Determine if scale is fractional:
    assert(scale > 0.0);
    sys->buffer_scale = ceil(scale);

    if ((sys->buffer_scale - scale) < DBL_EPSILON)
        sys->fractional_scale = false;
    else
        sys->fractional_scale = true;
}

static void CommitScale(struct qtwayland_t* obj)
{
    assert(obj);
    qtwayland_priv_t* const sys = (qtwayland_priv_t*)obj->p_sys;
    assert(sys);

    // Once the scale becomes fractional, viewport is used.
    // If the scale becomes an integer number after that,
    // viewport is still used.
    if (sys->fractional_scale)
    {
#ifdef QT_HAS_WAYLAND_FRACTIONAL_SCALING
        if (sys->viewporter)
        {
            if (!sys->viewport)
            {
                sys->viewport = wp_viewporter_get_viewport(sys->viewporter, sys->video_surface);

                // The buffer scale must remain 1 when fractional scaling is used:
                // If compositor has viewport support, it should be at least version 3:
                if (likely(sys->compositor_interface_version >= 3))
                {
                    wl_surface_set_buffer_scale(sys->video_surface, 1);
                    wl_surface_commit(sys->video_surface);
                }

                // Started using viewport, commit size so that viewport destination is set:
                CommitSize(obj, true);
            }
        }
        else
#endif
        {
            msg_Dbg(obj, "Viewporter protocol is not available or Qt version is less than " \
                         "6.5.0, and scale is fractional. Only integer scaling may be possible.");

            if (sys->compositor_interface_version >= 3)
            {
                wl_surface_set_buffer_scale(sys->video_surface, sys->buffer_scale);
                wl_surface_commit(sys->video_surface);
            }
            else
            {
                msg_Dbg(obj, "Compositor interface version is below 3, integer scaling " \
                             "is not possible.");
            }
        }
    }
    else
    {
#ifdef QT_HAS_WAYLAND_FRACTIONAL_SCALING
        // Scale is not fractional, but if viewport has been initialized before, keep using it:
        if (!sys->viewport)
#endif
        {
            if (sys->compositor_interface_version >= 3)
            {
                wl_surface_set_buffer_scale(sys->video_surface, sys->buffer_scale);
                wl_surface_commit(sys->video_surface);
            }
            else
            {
                msg_Dbg(obj, "Compositor interface version is below 3, integer scaling " \
                             "is not possible.");
            }
        }
    }
}

static int SetupInterface(qtwayland_t* obj, void* qpni_interface_surface, double scale)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;

    if (! qpni_interface_surface)
        return VLC_EGENERIC;

    sys->interface_surface = (struct wl_surface*)qpni_interface_surface;

    SetScale(obj, scale);

    return VLC_SUCCESS;
}


static int SetupVoutWindow(qtwayland_t* obj, vlc_window_t* wnd)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;

    sys->video_surface = wl_compositor_create_surface(sys->compositor);
    if (!sys->video_surface)
        return VLC_EGENERIC;

    CommitScale(obj);

    struct wl_region* region = wl_compositor_create_region(sys->compositor);
    if (!region)
    {
        wl_surface_destroy(sys->video_surface);
        sys->video_surface = NULL;
        return VLC_EGENERIC;
    }
    wl_region_add(region, 0, 0, 0, 0);
    wl_surface_set_input_region(sys->video_surface, region);

    wl_region_destroy(region);
    wl_surface_commit(sys->video_surface);

    //setup vout window
    wnd->type = VLC_WINDOW_TYPE_WAYLAND;
    wnd->handle.wl = sys->video_surface;
    wnd->display.wl = sys->display;

    return VLC_SUCCESS;
}

static void TeardownVoutWindow(struct qtwayland_t* obj)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;

#ifdef QT_HAS_WAYLAND_FRACTIONAL_SCALING
    if (sys->viewport)
    {
        wp_viewport_destroy(sys->viewport);
        sys->viewport = NULL;
    }
#endif

    vlc_assert(sys->video_surface);
    wl_surface_destroy(sys->video_surface);
    sys->video_surface = NULL;
}

static void Enable(qtwayland_t* obj, const vlc_window_cfg_t * conf)
{
    VLC_UNUSED(conf);

    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;

    vlc_assert(sys->video_subsurface == NULL);
    vlc_assert(sys->video_surface);
    vlc_assert(sys->interface_surface);

    sys->video_subsurface = wl_subcompositor_get_subsurface(sys->subcompositor, sys->video_surface, sys->interface_surface);
    wl_subsurface_place_below(sys->video_subsurface, sys->interface_surface);
    wl_subsurface_set_desync(sys->video_subsurface);
    wl_surface_commit(sys->video_surface);
}

static void Disable(qtwayland_t* obj)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;
    vlc_assert(sys->video_subsurface);

    wl_subsurface_destroy(sys->video_subsurface);
    sys->video_subsurface = NULL;
    wl_surface_commit(sys->video_surface);
}

static void Move(struct qtwayland_t* obj, int x, int y, bool commitSurface)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;
    if(sys->video_subsurface == NULL)
        return;
    wl_subsurface_set_position(sys->video_subsurface, x, y);
    if (commitSurface)
        wl_surface_commit(sys->video_surface);
}

static void Resize(struct qtwayland_t* obj, size_t width, size_t height, bool commitSurface)
{
    assert(obj);
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;
    assert(sys);

    bool commitNecessary = false;

    {
        // Do not directly compare, because SetSize may not directly use size as is
        const int oldWidth = sys->width;
        const int oldHeight = sys->height;

        SetSize(obj, width, height);

        if (oldWidth != sys->width)
            commitNecessary = true;
        else if (oldHeight != sys->height)
            commitNecessary = true;
    }

    if (commitNecessary)
        CommitSize(obj, commitSurface);
}


static void Rescale(struct qtwayland_t* obj, double scale)
{
    assert(obj);
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;
    assert(sys);

    bool commitNecessary = false;

    {
        const int oldBufferScale = sys->buffer_scale;
        const bool oldFractionalScale = sys->fractional_scale;

        SetScale(obj, scale);

        if (oldBufferScale != sys->buffer_scale)
            commitNecessary = true;
        else if (oldFractionalScale != sys->fractional_scale)
            commitNecessary = true;
    }

    if (commitNecessary)
        CommitScale(obj);
}

static void CommitSurface(struct qtwayland_t* obj)
{
    assert(obj);
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;
    assert(sys);

    if (!sys->video_surface)
        return;

    wl_surface_commit(sys->video_surface);
}

static void Close(qtwayland_t* obj)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)(obj->p_sys);
    wl_display_flush(sys->display);

#ifdef QT_HAS_WAYLAND_FRACTIONAL_SCALING
    if (sys->viewporter)
        wp_viewporter_destroy(sys->viewporter);
#endif

    wl_subcompositor_destroy(sys->subcompositor);
    wl_compositor_destroy(sys->compositor);
    wl_event_queue_destroy(sys->queue);
}

static bool Init(qtwayland_t* obj, void* qpni_display)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)(obj->p_sys);
    struct wl_display* display = (struct wl_display*)(qpni_display);

    sys->display = display;
    if (!display)
    {
        msg_Err(obj, "wayland display is missing");
        goto error;
    }

    sys->queue = wl_display_create_queue(display);
    void* wrapper = wl_proxy_create_wrapper(display);
    wl_proxy_set_queue((struct wl_proxy*)wrapper, sys->queue);

#ifdef QT_HAS_WAYLAND_FRACTIONAL_SCALING
    sys->viewporter = NULL;
    sys->viewport = NULL;
#endif

    struct wl_registry* registry = wl_display_get_registry((struct wl_display*)wrapper);
    wl_proxy_wrapper_destroy(wrapper);

    wl_registry_add_listener(registry, &registry_cbs, obj);
    wl_display_roundtrip_queue(display, sys->queue);

    wl_registry_destroy(registry);

    if (!sys->compositor || !sys->subcompositor )
        goto error;

    return true;

error:
    if (sys->subcompositor)
        wl_subcompositor_destroy(sys->subcompositor);
    if (sys->compositor)
        wl_compositor_destroy(sys->compositor);
    if (sys->queue)
        wl_event_queue_destroy(sys->queue);
    sys->compositor = NULL;
    sys->subcompositor = NULL;
    sys->queue = NULL;

    return false;
}

int OpenCompositor(vlc_object_t* p_this)
{
    qtwayland_t* obj = (qtwayland_t*)p_this;

    struct wl_display *dpy = wl_display_connect( NULL );
    if( dpy == NULL )
    {
        //not using wayland
        return VLC_EGENERIC;
    }

    wl_display_disconnect( dpy );

    qtwayland_priv_t* sys = (qtwayland_priv_t*)vlc_obj_calloc(p_this, 1, sizeof(qtwayland_priv_t));
    if (!sys)
        return VLC_ENOMEM;


    //module functions
    obj->init = Init;
    obj->close = Close;

    //interface functions
    obj->setupInterface = SetupInterface;

    //video functions
    obj->setupVoutWindow = SetupVoutWindow;
    obj->teardownVoutWindow = TeardownVoutWindow;
    obj->enable = Enable;
    obj->disable = Disable;
    obj->move = Move;
    obj->resize = Resize;
    obj->rescale = Rescale;
    obj->commitSurface = CommitSurface;

    obj->p_sys = sys;

    return VLC_SUCCESS;
}
