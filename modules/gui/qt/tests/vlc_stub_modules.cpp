/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
# include <config.h>
#endif

/* Define builtin modules for mocked parts */
#define MODULE_NAME module_faker
#undef VLC_DYNAMIC_PLUGIN

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_services_discovery.h>
#include <vlc_renderer_discovery.h>
#include <vlc_probe.h>
#include <vlc_interface.h>
#include <vlc_player.h>
#include <vlc_media_library.h>

#include "../../../../lib/libvlc_internal.h"

#include "qt.hpp"


#include "vlc_stub_modules.hpp"

static VLCTestingEnv* testenv = nullptr;

///RenderDiscovery module

static int OpenRD( vlc_object_t* p_this )
{
    testenv->renderDiscovery = (vlc_renderer_discovery_t *)p_this;
    return VLC_SUCCESS;
}

static void CloseRD( vlc_object_t* )
{
    testenv->renderDiscovery = nullptr;
}

static int vlc_rd_probe_open(vlc_object_t *obj) {
    auto probe = (struct vlc_probe_t *)obj;

    if (testenv->renderDiscoveryProbeEnabled)
        vlc_rd_probe_add(probe, "rd", "a fake renderer for testing purpose");
    //only probe ourself
    return VLC_PROBE_STOP;
}

///Interface module

namespace vlc {
class Compositor {};
}

static int OpenIntf(vlc_object_t* p_this)
{
    auto intfThread = reinterpret_cast<intf_thread_t*>(p_this);
    libvlc_int_t* libvlc = vlc_object_instance( p_this );

    /* Ensure initialization of objects in qt_intf_t. */
    qt_intf_t* qt_intf = vlc_object_create<qt_intf_t>( libvlc );
    if (!qt_intf)
        return VLC_ENOMEM;

    qt_intf->obj.logger = vlc_LogHeaderCreate(libvlc->obj.logger, "qt");
    if (!qt_intf->obj.logger)
    {
        vlc_object_delete(qt_intf);
        return VLC_EGENERIC;
    }

    qt_intf->p_playlist = vlc_intf_GetMainPlaylist(intfThread);
    qt_intf->p_player = vlc_playlist_GetPlayer(qt_intf->p_playlist);
    qt_intf->intf = intfThread;

    intfThread->p_sys = reinterpret_cast<intf_sys_t*>(qt_intf);

    testenv->intf = qt_intf;

    return VLC_SUCCESS;
}

static void CloseIntf( vlc_object_t *p_this )
{
    intf_thread_t* intfThread = (intf_thread_t*)(p_this);
    auto qt_intf = reinterpret_cast<qt_intf_t*>(intfThread->p_sys);
    if (!qt_intf)
        return;
    vlc_LogDestroy(qt_intf->obj.logger);
    vlc_object_delete(qt_intf);

    testenv->intf = nullptr;
}

//Medialib module

static void* MLGet( vlc_medialibrary_module_t*, int, va_list )
{
    return nullptr;
}

static int MLList( vlc_medialibrary_module_t*, int,
                const vlc_ml_query_params_t*, va_list )
{
    return VLC_EGENERIC;
}

static int MLControl( vlc_medialibrary_module_t*, int, va_list )
{
    return VLC_EGENERIC;
}

static int MLOpen( vlc_object_t* obj )
{
    auto* p_ml = reinterpret_cast<vlc_medialibrary_module_t*>( obj );
    p_ml->pf_control = MLControl;
    p_ml->pf_get = MLGet;
    p_ml->pf_list = MLList;
    return VLC_SUCCESS;
}

static void MLClose( vlc_object_t* obj )
{
    VLC_UNUSED(obj);
}

//module declaration

vlc_module_begin()
    set_callbacks(OpenIntf, CloseIntf)
    set_capability("interface", 0)
add_submodule()
    set_capability("renderer_discovery", 0)
    add_shortcut("rd")
    set_callbacks(OpenRD, CloseRD)
add_submodule()
    set_capability("renderer probe", 10000)
    set_callback(vlc_rd_probe_open)
add_submodule()
    set_capability("medialibrary", 10000)
    set_callbacks(MLOpen, MLClose)
vlc_module_end()

extern "C" {

const char vlc_module_name[] = MODULE_STRING;
VLC_EXPORT vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};
}

VLCTestingEnv::VLCTestingEnv()
{
    assert(testenv == nullptr);
    testenv = this;

}

VLCTestingEnv::~VLCTestingEnv()
{
    libvlc_release(libvlc);
    testenv = nullptr;
}

static const char * test_defaults_args[] = {
    "-v", "--vout=vdummy", "--aout=adummy", "--text-renderer=tdummy", "--media-library"
};

static const int test_defaults_nargs = ARRAY_SIZE(test_defaults_args);

bool VLCTestingEnv::init()
{
    //see test/libvlc/test.h
    QByteArray alarm_timeout = qgetenv("VLC_TEST_TIMEOUT");
    if (alarm_timeout.isEmpty())
        qputenv("QTEST_FUNCTION_TIMEOUT", "5000");
    else
        qputenv("QTEST_FUNCTION_TIMEOUT", alarm_timeout);

    setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
    setenv("VLC_LIB_PATH", TOP_BUILDDIR, 1);

    libvlc = libvlc_new(test_defaults_nargs, test_defaults_args);
    if (!libvlc)
        return false;
    libvlc_InternalAddIntf(libvlc->p_libvlc_int, MODULE_STRING);
    libvlc_InternalPlay(libvlc->p_libvlc_int);

    if (!intf)
        return false;

    return true;
}


