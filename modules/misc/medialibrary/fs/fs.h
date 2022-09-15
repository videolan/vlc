/*****************************************************************************
 * fs.h: Media library network file system
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

#ifndef SD_FS_H
#define SD_FS_H

#include <memory>
#include <vector>
#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_cxx_helpers.hpp>
#include <medialibrary/filesystem/IFileSystemFactory.h>
#include <medialibrary/IDeviceLister.h>

struct libvlc_int_t;

namespace medialibrary {
class IDeviceListerCb;
class IMediaLibrary;
}

namespace vlc {
  namespace medialibrary {

using namespace ::medialibrary;
using namespace ::medialibrary::fs;

class SDFileSystemFactory : public IFileSystemFactory, private IDeviceListerCb {
public:
    SDFileSystemFactory(vlc_object_t *m_parent,
                        const std::string &scheme);

    bool
    initialize( const IMediaLibrary* ml ) override;

    std::shared_ptr<IDirectory>
    createDirectory(const std::string &mrl) override;

    std::shared_ptr<fs::IFile>
    createFile(const std::string& mrl) override;

    std::shared_ptr<IDevice>
    createDevice(const std::string &uuid) override;

    std::shared_ptr<IDevice>
    createDeviceFromMrl(const std::string &path) override;

    void
    refreshDevices() override;

    bool
    isMrlSupported(const std::string &path) const override;

    bool
    isNetworkFileSystem() const override;

    const std::string &
    scheme() const override;

    bool
    start(IFileSystemFactoryCb *m_callbacks) override;

    void
    stop() override;

    libvlc_int_t *
    libvlc() const;

    void
    onDeviceMounted(const std::string& uuid, const std::string& mountpoint, bool removable) override;

    void
    onDeviceUnmounted(const std::string& uuid, const std::string& mountpoint) override;

    bool
    waitForDevice(const std::string& mrl, uint32_t timeout) const override;

private:
    std::shared_ptr<fs::IDevice>
    deviceByUuid(const std::string& uuid);

    bool isStarted() const override;

    std::shared_ptr<fs::IDevice> deviceByMrl(const std::string& mrl) const;

private:
    vlc_object_t *const m_parent;
    const std::string m_scheme;
    IFileSystemFactoryCb *m_callbacks;
    bool m_isNetwork;

    mutable vlc::threads::mutex m_mutex;
    mutable vlc::threads::condition_variable m_cond;
    std::vector<std::shared_ptr<IDevice>> m_devices;
    std::shared_ptr<IDeviceLister> m_deviceLister;
};

  } /* namespace medialibrary */
} /* namespace vlc */

#endif
