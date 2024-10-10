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

#ifndef QT_VLC_STUB_MODULES
#define QT_VLC_STUB_MODULES

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_player.h>
#include <vlc_playlist.h>
#include <vlc_services_discovery.h>
#include <vlc_renderer_discovery.h>
#include <vlc_interface.h>
#include <vlc_cxx_helpers.hpp>

#include "qt.hpp"


#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
#define QCOMPARE_LT(a, b) QVERIFY((a) < (b))
#define QCOMPARE_GT(a, b) QVERIFY((a) > (b))
#define QCOMPARE_LE(a, b) QVERIFY((a) <= (b))
#define QCOMPARE_GE(a, b) QVERIFY((a) >= (b))
#endif

/**
 * This class allows to instanciate a libvlc instance with stubbed modules
 * the modules instance can be reteived and manipulated using their pointer
 * This allows testing models depending on vlc in a controlled environment
 * validity of the pointers depends of the module usage
 */
struct VLCTestingEnv
{
    VLCTestingEnv();
    ~VLCTestingEnv();

    bool init();

    //should be valid after init
    libvlc_instance_t* libvlc = nullptr;

    //pointer to the qt interface pointer
    //no qt related objects are defined in it (no MainCtx, no Compositor ...)
    //should be valid after init
    qt_intf_t* intf = nullptr;

    //render discovery instance (valid once created)
    vlc_renderer_discovery_t* renderDiscovery = nullptr;

    //if set to false no render discovery will be found
    bool renderDiscoveryProbeEnabled = true;
};

#endif /* QT_VLC_STUB_MODULES */
