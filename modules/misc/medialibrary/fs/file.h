/*****************************************************************************
 * file.h: Media library network file
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

#ifndef SD_FILE_H
#define SD_FILE_H

#include <medialibrary/filesystem/IFile.h>

namespace vlc {
  namespace medialibrary {

using namespace ::medialibrary::fs;

class SDFile : public IFile
{
public:
    explicit SDFile(const std::string &mrl);
    virtual ~SDFile() = default;
    const std::string& mrl() const override;
    const std::string& name() const override;
    const std::string& extension() const override;
    time_t lastModificationDate() const override;
    int64_t size() const override;
    inline bool isNetwork() const override { return true; }
    LinkedFileType linkedType() const override;
    const std::string &linkedWith() const override;

private:
    std::string m_mrl;
    std::string m_name;
    std::string m_extension;
    std::string m_linkedFile;
};

  } /* namespace medialibrary */
} /* namespace vlc */

#endif
