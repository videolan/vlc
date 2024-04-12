/*****************************************************************************
 * Clients.cpp
 *****************************************************************************
 * Copyright © 2024 VLC authors and VideoLAN
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <cassert>
#include <sstream>
#include <stream_out/dlna/dlna.hpp>
#include <string>
#include <upnp/upnp.h>
#if UPNP_VERSION >= 11400
#include <upnp/UpnpExtraHeaders.h>
#else
#include <upnp/ExtraHeaders.h>
#endif

#if defined(_WIN32)
#include <winsock2.h>
#elif defined(HAVE_SYS_SOCKET_H)
#include <sys/socket.h>
#endif

#include <vlc_common.h>

#include <vlc_configuration.h>

#include "utils.hpp"

namespace utils
{
MimeType get_mimetype(vlc_ml_media_type_t type, const std::string &file_extension) noexcept
{
    const char *mime_end = file_extension.c_str();
    // special case for the transcode muxer to be widely accepted by a majority of players.
    if (file_extension == "ts")
        mime_end = "mpeg";
    else if (file_extension == "mp3")
        mime_end = "mpeg";
    switch (type)
    {
        case VLC_ML_MEDIA_TYPE_AUDIO:
            return {"audio", mime_end};
        case VLC_ML_MEDIA_TYPE_UNKNOWN: // Intended pass through
        case VLC_ML_MEDIA_TYPE_VIDEO:
            return {"video", mime_end};
        default:
            vlc_assert_unreachable();
    }
}

std::string file_extension(const std::string &file)
{
    auto pos = file.find_last_of('.');
    if (pos == std::string::npos)
        return {};
    return file.substr(pos + 1);
}

std::string get_server_url()
{
    // TODO support ipv6
    const std::string addr = UpnpGetServerIpAddress();
    const std::string port = std::to_string(UpnpGetServerPort());
    return "http://" + addr + ':' + port + '/';
}

std::string get_root_dir()
{
    std::stringstream ret;

    char *path = config_GetSysPath(VLC_PKG_DATA_DIR, NULL);
    assert(path);

    ret << path << "/upnp_server/";

    free(path);
    return ret.str();
}

std::string addr_to_string(const sockaddr_storage *addr)
{
    const void *ip;
    if (addr->ss_family == AF_INET6)
    {
        ip = &reinterpret_cast<const struct sockaddr_in6 *>(addr)->sin6_addr;
    }
    else
    {
        ip = &reinterpret_cast<const struct sockaddr_in *>(addr)->sin_addr;
    }

    char buff[INET6_ADDRSTRLEN];
    inet_ntop(addr->ss_family, ip, buff, sizeof(buff));
    return buff;
}

std::vector<MediaTrackRef> get_media_tracks(const vlc_ml_media_t &media, vlc_ml_track_type_t type)
{
    std::vector<MediaTrackRef> ret;

    if (media.p_tracks == nullptr)
        return ret;
    for (unsigned i = 0; i < media.p_tracks->i_nb_items; ++i)
    {
        const auto &track = media.p_tracks->p_items[i];
        if (track.i_type == type)
            ret.emplace_back(track);
    }
    return ret;
}

std::vector<MediaFileRef> get_media_files(const vlc_ml_media_t &media, vlc_ml_file_type_t type)
{
    std::vector<MediaFileRef> ret;

    if (media.p_files == nullptr)
        return ret;
    for (unsigned i = 0; i < media.p_files->i_nb_items; ++i)
    {
        const auto &file = media.p_files->p_items[i];
        if (file.i_type == type)
            ret.emplace_back(file);
    }
    return ret;
}

std::string album_thumbnail_url(const vlc_ml_album_t &album)
{
    const auto &thumbnail = album.thumbnails[VLC_ML_THUMBNAIL_SMALL];
    if (thumbnail.i_status != VLC_ML_THUMBNAIL_STATUS_AVAILABLE)
        return "";
    const auto thumbnail_extension = file_extension(std::string(thumbnail.psz_mrl));
    return get_server_url() + "thumbnail/small/album/" + std::to_string(album.i_id) + "." +
           thumbnail_extension;
}

namespace http
{

static UpnpExtraHeaders *get_hdr(UpnpListHead *list, const std::string name)
{
    for (auto *it = UpnpListBegin(list); it != UpnpListEnd(list); it = UpnpListNext(list, it))
    {
        UpnpExtraHeaders *hd = reinterpret_cast<UpnpExtraHeaders *>(it);
        std::string hdr_name = UpnpExtraHeaders_get_name_cstr(hd);
        //        std::cerr << hdr_name << ": " << UpnpExtraHeaders_get_value_cstr( hd ) << "\n";
        std::transform(std::begin(hdr_name), std::end(hdr_name), std::begin(hdr_name), ::tolower);
        if (hdr_name == name)
        {
            return hd;
        }
    }
    return nullptr;
}

void add_response_hdr(UpnpListHead *list, const std::pair<std::string, std::string> resp)
{

    auto *hdr = get_hdr(list, resp.first);
    const auto resp_str = resp.first + ": " + resp.second;
    if (!hdr)
    {
        hdr = UpnpExtraHeaders_new();
    }
    UpnpExtraHeaders_set_resp(hdr, resp_str.c_str());
    UpnpListInsert(
        list, UpnpListEnd(list), const_cast<UpnpListHead *>(UpnpExtraHeaders_get_node(hdr)));
}

std::string get_dlna_extra_protocol_info(const MimeType &mime)
{
    // TODO We should change that to a better profile selection using profiles in dlna.hpp
    // as soon as more info on media tracks are available in the medialibrary
    dlna_profile_t profile =
        mime.media_type == "audio" ? default_audio_profile : default_video_profile;
    profile.mime = mime.combine();

    const protocol_info_t info{
        DLNA_TRANSPORT_PROTOCOL_HTTP,
        DLNA_ORG_CONVERSION_NONE,
        profile,
    };

    const dlna_org_flags_t flags = DLNA_ORG_FLAG_STREAMING_TRANSFER_MODE |
                                   DLNA_ORG_FLAG_BACKGROUND_TRANSFERT_MODE |
                                   DLNA_ORG_FLAG_CONNECTION_STALL | DLNA_ORG_FLAG_DLNA_V15;
    const dlna_org_operation_t op = DLNA_ORG_OPERATION_RANGE;

    return dlna_write_protocol_info(info, flags, op);
}

} // namespace http

} // namespace utils
