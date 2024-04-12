/*****************************************************************************
 * FileHandler.hpp : UPnP server module header
 *****************************************************************************
 * Copyright © 2021 VLC authors and VideoLAN
 *
 * Authors: Alaric Senat <alaric@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <memory>

#include <upnp.h>
#if UPNP_VERSION >= 11400
#include <upnp/UpnpFileInfo.h>
#else
#include <upnp/FileInfo.h>
#endif

struct vlc_object_t;

namespace ml
{
struct MediaLibraryContext;
};

/// This interface reflects the common behaviour of upnp's file handlers.
/// The FileHandler is used to serve the content of a named file to the http server.
/// The file can be whatever: present on the fs, live streamed, etc.
class FileHandler
{
  public:
    virtual bool get_info(UpnpFileInfo &info) = 0;
    virtual bool open(vlc_object_t *) = 0;
    virtual size_t read(uint8_t[], size_t) noexcept = 0;

    enum class SeekType : int
    {
        Set = SEEK_SET,
        Current = SEEK_CUR,
        End = SEEK_END
    };
    virtual bool seek(SeekType, off_t) noexcept = 0;

    virtual ~FileHandler() = default;
};

/// Parses the url and return the needed FileHandler implementation
/// All the informations about what FileHandler implementation is to be chosen is locatied either in
/// the url
std::unique_ptr<FileHandler> parse_url(const char *url, const ml::MediaLibraryContext &);

#endif /* FILEHANDLER_HPP */
