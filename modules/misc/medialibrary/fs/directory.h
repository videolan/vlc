/*****************************************************************************
 * directory.h: Media library network directory
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

#ifndef SD_DIRECTORY_H
#define SD_DIRECTORY_H

#include <medialibrary/filesystem/IDirectory.h>
#include <medialibrary/filesystem/IFile.h>

#include "fs.h"

namespace vlc {
  namespace medialibrary {

using namespace ::medialibrary::fs;

class SDDirectory : public IDirectory
{
public:
    explicit SDDirectory(const std::string &mrl, SDFileSystemFactory &fs);
    const std::string &mrl() const override;
    const std::vector<std::shared_ptr<fs::IFile>> &files() const override;
    const std::vector<std::shared_ptr<fs::IDirectory>> &dirs() const override;
    std::shared_ptr<fs::IDevice> device() const override;
    std::shared_ptr<fs::IFile> file( const std::string& mrl ) const override;

private:
    void read() const;

    std::string m_mrl;
    SDFileSystemFactory &m_fs;

    mutable bool m_read_done = false;
    mutable std::vector<std::shared_ptr<fs::IFile>> m_files;
    mutable std::vector<std::shared_ptr<fs::IDirectory>> m_dirs;
    mutable std::shared_ptr<IDevice> m_device;
};

  } /* namespace medialibrary */
} /* namespace vlc */

#endif
