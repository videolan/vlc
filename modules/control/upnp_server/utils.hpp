/*****************************************************************************
 * utils.hpp : UPnP server utils
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
#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <upnp/list.h>
#include <vector>

#include <vlc_media_library.h>

struct sockaddr_storage;

namespace utils
{
struct MimeType
{
    std::string media_type;
    std::string file_type;

    std::string combine() const { return media_type + '/' + file_type; }
};

MimeType get_mimetype(vlc_ml_media_type_t type, const std::string &file_extension) noexcept;

std::string file_extension(const std::string &file);

std::string get_server_url();
std::string get_root_dir();

std::string addr_to_string(const sockaddr_storage *addr);

template <typename T> using ConstRef = std::reference_wrapper<const T>;

using MediaTrackRef = ConstRef<vlc_ml_media_track_t>;
std::vector<MediaTrackRef> get_media_tracks(const vlc_ml_media_t &media, vlc_ml_track_type_t type);
using MediaFileRef = ConstRef<vlc_ml_file_t>;
std::vector<MediaFileRef> get_media_files(const vlc_ml_media_t &media, vlc_ml_file_type_t);

std::string album_thumbnail_url(const vlc_ml_album_t &);

namespace http
{
void add_response_hdr(UpnpListHead *list, const std::pair<std::string, std::string> resp);
std::string get_dlna_extra_protocol_info(const MimeType &dlna_profile);
} // namespace http

} // namespace utils

#endif /* UTILS_HPP */
