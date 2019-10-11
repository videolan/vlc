/*****************************************************************************
 * util.cpp: Media library utils
 *****************************************************************************
 * Copyright (C) 2018 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "fs.h"

#include <algorithm>
#include <vlc_services_discovery.h>
#include <medialibrary/IDeviceLister.h>
#include <medialibrary/filesystem/IDevice.h>

#include "device.h"
#include "directory.h"
#include "util.h"

extern "C" {

static void
services_discovery_item_added(services_discovery_t *sd,
                              input_item_t *parent, input_item_t *media,
                              const char *cat)
{
    VLC_UNUSED(parent);
    VLC_UNUSED(cat);
    vlc::medialibrary::SDFileSystemFactory *that =
        static_cast<vlc::medialibrary::SDFileSystemFactory *>(sd->owner.sys);
    that->onDeviceAdded(media);
}

static void
services_discovery_item_removed(services_discovery_t *sd, input_item_t *media)
{
    vlc::medialibrary::SDFileSystemFactory *that =
        static_cast<vlc::medialibrary::SDFileSystemFactory *>(sd->owner.sys);
    that->onDeviceRemoved(media);
}

static const struct services_discovery_callbacks sd_cbs = {
    .item_added = services_discovery_item_added,
    .item_removed = services_discovery_item_removed,
};

}

namespace vlc {
  namespace medialibrary {

using namespace ::medialibrary;

SDFileSystemFactory::SDFileSystemFactory(vlc_object_t *parent,
                                         const std::string &scheme)
    : m_parent(parent)
    , m_scheme(scheme)
{
}

std::shared_ptr<IDirectory>
SDFileSystemFactory::createDirectory(const std::string &mrl)
{
    return std::make_shared<SDDirectory>(mrl, *this);
}

std::shared_ptr<IFile>
SDFileSystemFactory::createFile(const std::string& mrl)
{
    auto dir = createDirectory(mrl);
    assert(dir != nullptr);
    return dir->file(mrl);
}

std::shared_ptr<IDevice>
SDFileSystemFactory::createDevice(const std::string &uuid)
{
    vlc::threads::mutex_locker locker(m_mutex);

    vlc_tick_t deadline = vlc_tick_now() + VLC_TICK_FROM_SEC(15);
    while ( true )
    {
        auto it = std::find_if(m_devices.cbegin(), m_devices.cend(),
                [&uuid](const std::shared_ptr<IDevice>& device) {
                    return strcasecmp( uuid.c_str(), device->uuid().c_str() ) == 0;
                });
        if (it != m_devices.cend())
            return (*it);
        /* wait a bit, maybe the device is not detected yet */
        int timeout = m_itemAddedCond.timedwait(m_mutex, deadline);
        if (timeout)
            return nullptr;
    }
    vlc_assert_unreachable();
}

std::shared_ptr<IDevice>
SDFileSystemFactory::createDeviceFromMrl(const std::string &mrl)
{
    vlc::threads::mutex_locker locker(m_mutex);

    auto it = std::find_if(m_devices.cbegin(), m_devices.cend(),
            [&mrl](const std::shared_ptr<IDevice>& device) {
                auto match = device->matchesMountpoint( mrl );
                return std::get<0>( match );
            });
    if (it != m_devices.cend())
        return (*it);
    return nullptr;
}

void
SDFileSystemFactory::refreshDevices()
{
    /* nothing to do */
}

bool
SDFileSystemFactory::isMrlSupported(const std::string &path) const
{
    return !path.compare(0, m_scheme.length(), m_scheme);
}

bool
SDFileSystemFactory::isNetworkFileSystem() const
{
    return true;
}

const std::string &
SDFileSystemFactory::scheme() const
{
    return m_scheme;
}

bool
SDFileSystemFactory::start(IFileSystemFactoryCb *callbacks)
{
    this->m_callbacks = callbacks;
    struct services_discovery_owner_t owner = {
        .cbs = &sd_cbs,
        .sys = this,
    };
    char** sdLongNames;
    int* categories;
    auto releaser = [](char** ptr) {
        for ( auto i = 0u; ptr[i] != nullptr; ++i )
            free( ptr[i] );
        free( ptr );
    };
    auto sdNames = vlc_sd_GetNames( libvlc(), &sdLongNames, &categories );
    if ( sdNames == nullptr )
        return false;
    auto sdNamesPtr = vlc::wrap_carray( sdNames, releaser );
    auto sdLongNamesPtr = vlc::wrap_carray( sdLongNames, releaser );
    auto categoriesPtr = vlc::wrap_carray( categories );
    for ( auto i = 0u; sdNames[i] != nullptr; ++i )
    {
        if ( categories[i] != SD_CAT_LAN )
            continue;
        SdPtr sd{ vlc_sd_Create( libvlc(), sdNames[i], &owner ), &vlc_sd_Destroy };
        if ( sd == nullptr )
            continue;
        m_sds.push_back( std::move( sd ) );
    }
    return m_sds.empty() == false;
}

void
SDFileSystemFactory::stop()
{
    m_sds.clear();
    m_callbacks = nullptr;
}

libvlc_int_t *
SDFileSystemFactory::libvlc() const
{
    return vlc_object_instance(m_parent);
}

void
SDFileSystemFactory::onDeviceAdded(input_item_t *media)
{
    auto mrl = std::string{ media->psz_uri };
    auto name = media->psz_name;
    if ( *mrl.crbegin() != '/' )
        mrl += '/';

    if ( strncasecmp( mrl.c_str(), m_scheme.c_str(), m_scheme.length() ) != 0 )
        return;

    {
        vlc::threads::mutex_locker locker(m_mutex);
        auto it = std::find_if(m_devices.begin(), m_devices.end(),
                [name](const std::shared_ptr<IDevice>& device) {
                    return strcasecmp( name, device->uuid().c_str() ) == 0;
                });
        if (it != m_devices.end())
        {
            auto& device = (*it);
            auto match = device->matchesMountpoint( mrl );
            if ( std::get<0>( match ) == false )
            {
                device->addMountpoint( mrl );
                m_callbacks->onDeviceMounted( *device, mrl );
            }
            return; /* already exists */
        }
        auto device = std::make_shared<SDDevice>( name, mrl );
        m_devices.push_back( device );
        m_callbacks->onDeviceMounted( *device, mrl );
    }

    m_itemAddedCond.signal();
}

void
SDFileSystemFactory::onDeviceRemoved(input_item_t *media)
{
    auto name = media->psz_name;
    auto mrl = std::string{ media->psz_uri };
    if ( *mrl.crbegin() != '/' )
        mrl += '/';

    if ( strncasecmp( mrl.c_str(), m_scheme.c_str(), m_scheme.length() ) != 0 )
        return;

    {
        vlc::threads::mutex_locker locker(m_mutex);
        auto it = std::find_if(m_devices.begin(), m_devices.end(),
                [&name](const std::shared_ptr<IDevice>& device) {
                    return strcasecmp( name, device->uuid().c_str() ) == 0;
                });
        if ( it != m_devices.end() )
        {
            (*it)->removeMountpoint( mrl );
            m_callbacks->onDeviceUnmounted( *(*it), mrl );
        }
    }
}

  } /* namespace medialibrary */
} /* namespace vlc */
