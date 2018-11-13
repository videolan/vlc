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

struct input_item_t;
struct services_discovery_t;
struct libvlc_int_t;
extern "C" {
void vlc_sd_Destroy(services_discovery_t *sd);
}

namespace medialibrary {
struct IDeviceListerCb;
}

namespace vlc {
  namespace medialibrary {

using namespace ::medialibrary;
using namespace ::medialibrary::fs;

class SDFileSystemFactory : public IFileSystemFactory {
public:
    SDFileSystemFactory(vlc_object_t *m_parent,
                        const std::string &scheme);

    std::shared_ptr<IDirectory>
    createDirectory(const std::string &mrl) override;

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

    /* public to be called from C callback */
    void onDeviceAdded(input_item_t *media);
    void onDeviceRemoved(input_item_t *media);

private:
    vlc_object_t *const m_parent;
    const std::string m_scheme;
    using SdPtr = std::unique_ptr<services_discovery_t, decltype(&vlc_sd_Destroy)>;
    std::vector<SdPtr> m_sds;
    IFileSystemFactoryCb *m_callbacks;

    vlc::threads::mutex m_mutex;
    vlc::threads::condition_variable m_itemAddedCond;
    std::vector<std::shared_ptr<IDevice>> m_devices;
};

  } /* namespace medialibrary */
} /* namespace vlc */

#endif
