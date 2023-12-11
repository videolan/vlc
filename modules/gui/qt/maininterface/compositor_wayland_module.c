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

#include <assert.h>

typedef struct
{
    struct wl_display* display;
    struct wl_event_queue* queue;
    struct wl_compositor* compositor;
    struct wl_subcompositor* subcompositor;

    struct wl_surface* interface_surface;

    struct wl_surface* video_surface;
    struct wl_subsurface* video_subsurface;
} qtwayland_priv_t;

static void registry_global_cb(void* data, struct wl_registry* registry,
                               uint32_t id, const char* iface, uint32_t version)
{
    VLC_UNUSED(version);

    qtwayland_t* obj = (qtwayland_t*)data;
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;

    if (!strcmp(iface, "wl_subcompositor"))
        sys->subcompositor = (struct wl_subcompositor*)wl_registry_bind(registry, id, &wl_subcompositor_interface, version);
    if (!strcmp(iface, "wl_compositor"))
        sys->compositor = (struct wl_compositor*)wl_registry_bind(registry, id, &wl_compositor_interface, version);
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

static int SetupInterface(qtwayland_t* obj, void* qpni_interface_surface)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;

    if (! qpni_interface_surface)
        return VLC_EGENERIC;

    sys->interface_surface = (struct wl_surface*)qpni_interface_surface;

    return VLC_SUCCESS;
}


static int SetupVoutWindow(qtwayland_t* obj, vlc_window_t* wnd)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;

    sys->video_surface = wl_compositor_create_surface(sys->compositor);
    if (!sys->video_surface)
        return VLC_EGENERIC;

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

static void Move(struct qtwayland_t* obj, int x, int y)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)obj->p_sys;
    wl_subsurface_set_position(sys->video_subsurface, x, y);
    wl_surface_commit(sys->video_surface);
}

static void Resize(struct qtwayland_t* obj, size_t width, size_t height)
{
    VLC_UNUSED(obj);
    VLC_UNUSED(width);
    VLC_UNUSED(height);
}

static void Close(qtwayland_t* obj)
{
    qtwayland_priv_t* sys = (qtwayland_priv_t*)(obj->p_sys);
    wl_display_flush(sys->display);
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

    struct wl_registry* registry = wl_display_get_registry((struct wl_display*)wrapper);
    wl_proxy_wrapper_destroy(wrapper);

    wl_registry_add_listener(registry, &registry_cbs, obj);
    wl_display_roundtrip_queue(display, sys->queue);

    wl_registry_destroy(registry);

    if (!sys->compositor || !sys->subcompositor )
        goto error;

    return true;

error:
    if (sys->queue)
        wl_event_queue_destroy(sys->queue);
    return false;
}

static int Open(vlc_object_t* p_this)
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

    obj->p_sys = sys;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("QtWayland"))
    set_description(N_(" calls for compositing with Qt"))
    set_capability("qtwayland", 10)
    set_callback(Open)
vlc_module_end()
