/*****************************************************************************
 * device.h: Media library network device
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

#ifndef SD_DEVICE_H
#define SD_DEVICE_H

#include <medialibrary/filesystem/IDevice.h>
#include <vector>

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_cxx_helpers.hpp>

namespace vlc {
  namespace medialibrary {

using namespace ::medialibrary::fs;

class SDDevice : public IDevice
{
public:
    SDDevice(const std::string& uuid, std::string scheme,
              bool removable, bool isNetwork);

    const std::string &uuid() const override;
    bool isRemovable() const override;
    bool isPresent() const override;
    bool isNetwork() const override;
    const std::string &mountpoint() const override;
    void addMountpoint( std::string mrl ) override;
    void removeMountpoint( const std::string& mrl ) override;
    std::tuple<bool, std::string> matchesMountpoint( const std::string& mrl ) const override;
    std::string relativeMrl( const std::string& absoluteMrl ) const override;
    std::string absoluteMrl( const std::string& relativeMrl ) const override;
    const std::string& scheme() const override;

private:
    struct Mountpoint
    {
        Mountpoint( std::string m )
            : mrl( std::move( m ) )
            , url( mrl )
        {
        }
        std::string mrl;
        vlc::url url;
    };
    std::string m_uuid;
    std::vector<Mountpoint> m_mountpoints;
    std::string m_scheme;
    bool m_removable;
    bool m_isNetwork;
};

  } /* namespace medialibrary */
} /* namespace vlc */

#endif
