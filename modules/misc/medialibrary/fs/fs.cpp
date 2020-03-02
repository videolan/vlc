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

#include <algorithm>
#include <vlc_services_discovery.h>
#include <medialibrary/IDeviceLister.h>
#include <medialibrary/filesystem/IDevice.h>
#include <medialibrary/IMediaLibrary.h>

#include "device.h"
#include "directory.h"
#include "util.h"
#include "fs.h"

namespace vlc {
  namespace medialibrary {

using namespace ::medialibrary;

SDFileSystemFactory::SDFileSystemFactory(vlc_object_t *parent,
                                         IMediaLibrary* ml,
                                         const std::string &scheme)
    : m_parent(parent)
    , m_ml( ml )
    , m_scheme(scheme)
    , m_deviceLister( m_ml->deviceLister( scheme ) )
    , m_callbacks( nullptr )
{
    m_isNetwork = strncasecmp( m_scheme.c_str(), "file://",
                               m_scheme.length() ) != 0;
}

std::shared_ptr<fs::IDirectory>
SDFileSystemFactory::createDirectory(const std::string &mrl)
{
    return std::make_shared<SDDirectory>(mrl, *this);
}

std::shared_ptr<fs::IFile>
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

    assert( isStarted() == true );

    return deviceByUuid(uuid);
}

std::shared_ptr<IDevice>
SDFileSystemFactory::createDeviceFromMrl(const std::string &mrl)
{
    vlc::threads::mutex_locker locker(m_mutex);

    assert( isStarted() == true );
    return deviceByMrl(mrl);
}

void
SDFileSystemFactory::refreshDevices()
{
    m_deviceLister->refresh();
}

bool
SDFileSystemFactory::isMrlSupported(const std::string &path) const
{
    return !path.compare(0, m_scheme.length(), m_scheme);
}

bool
SDFileSystemFactory::isNetworkFileSystem() const
{
    return m_isNetwork;
}

const std::string &
SDFileSystemFactory::scheme() const
{
    return m_scheme;
}

bool
SDFileSystemFactory::start(IFileSystemFactoryCb *callbacks)
{
    assert( isStarted() == false );
    m_callbacks = callbacks;
    return m_deviceLister->start( this );
}

void
SDFileSystemFactory::stop()
{
    assert( isStarted() == true );
    m_deviceLister->stop();
    m_callbacks = nullptr;
}

libvlc_int_t *
SDFileSystemFactory::libvlc() const
{
    return vlc_object_instance(m_parent);
}

void SDFileSystemFactory::onDeviceMounted(const std::string& uuid,
                                          const std::string& mountpoint,
                                          bool removable)
{
    if ( strncasecmp( mountpoint.c_str(), m_scheme.c_str(), m_scheme.length() ) != 0 )
        return;

    std::shared_ptr<fs::IDevice> device;
    {
        vlc::threads::mutex_locker lock(m_mutex);
        device = deviceByUuid(uuid);
        if (device == nullptr)
        {
            device = std::make_shared<SDDevice>(uuid, m_scheme,
                                                removable, isNetworkFileSystem() );
            m_devices.push_back(device);
        }
        device->addMountpoint(mountpoint);
    }

    m_callbacks->onDeviceMounted( *device );
}

void vlc::medialibrary::SDFileSystemFactory::onDeviceUnmounted(const std::string& uuid,
                                                               const std::string& mountpoint)
{
    if ( strncasecmp( mountpoint.c_str(), m_scheme.c_str(), m_scheme.length() ) != 0 )
        return;

    std::shared_ptr<fs::IDevice> device;
    {
        vlc::threads::mutex_locker lock(m_mutex);
        device = deviceByUuid(uuid);
    }
    if ( device == nullptr )
    {
        assert( !"Unknown device was unmounted" );
        return;
    }
    device->removeMountpoint(mountpoint);
    m_callbacks->onDeviceUnmounted(*device);
}

std::shared_ptr<IDevice> SDFileSystemFactory::deviceByUuid(const std::string& uuid)
{
    auto it = std::find_if( begin( m_devices ), end( m_devices ),
                            [&uuid]( const std::shared_ptr<fs::IDevice>& d ) {
        return strcasecmp( d->uuid().c_str(), uuid.c_str() ) == 0;
    });
    if ( it == end( m_devices ) )
        return nullptr;
    return *it;
}

bool SDFileSystemFactory::isStarted() const
{
    return m_callbacks != nullptr;
}

std::shared_ptr<IDevice> SDFileSystemFactory::deviceByMrl(const std::string& mrl)
{
    std::shared_ptr<fs::IDevice> res;
    std::string mountpoint;
    for ( const auto& d : m_devices )
    {
        auto match = d->matchesMountpoint( mrl );
        if ( std::get<0>( match ) == false )
            continue;
        auto newMountpoint = std::get<1>( match );
        if ( res == nullptr || newMountpoint.length() > mountpoint.length() )
        {
            res = d;
            mountpoint = std::move( newMountpoint );
        }
    }
    return res;
}

  } /* namespace medialibrary */
} /* namespace vlc */
