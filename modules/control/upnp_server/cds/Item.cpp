/*****************************************************************************
 * Item.cpp
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

#include <chrono>
#include <sstream>

#include "../utils.hpp"
#include "Item.hpp"

namespace cds
{

Item::Item(const int64_t id, const ml::MediaLibraryContext &ml) noexcept :
    Object(id, -1, nullptr, Object::Type::Item),
    medialib_(ml)
{}

template <typename Rep, typename Pediod = std::ratio<1>>
static std::string duration_to_string(const char *fmt, std::chrono::duration<Rep, Pediod> duration)
{
    char ret[32] = {0};
    using namespace std::chrono;
    // Substract 1 hour because std::localtime starts at 1 AM
    const time_t sec = duration_cast<seconds>(duration - 1h).count();
    const size_t size = std::strftime(ret, sizeof(ret), fmt, std::localtime(&sec));
    return std::string{ret, size};
}

static xml::Element make_resource(xml::Document &doc,
                                  const vlc_ml_media_t &media,
                                  const std::vector<utils::MediaTrackRef> v_tracks,
                                  const std::vector<utils::MediaFileRef> main_files,
                                  const std::string &file_extension)
{
    const auto &profile_name = "native";
    const char *mux = file_extension.c_str();

    const std::string url_base = utils::get_server_url();
    const std::string url =
        url_base + "media/" + profile_name + "/" + std::to_string(media.i_id) + "." + mux;

    auto elem = doc.create_element("res", doc.create_text_node(url.c_str()));
    const auto media_duration =
        duration_to_string("%H:%M:%S", std::chrono::milliseconds(media.i_duration));
    elem.set_attribute("duration", media_duration.c_str());

    if (media.i_type == VLC_ML_MEDIA_TYPE_VIDEO)
    {
        std::stringstream resolution;
        if (v_tracks.size() >= 1)
        {
            const vlc_ml_media_track_t &vtrack = v_tracks[0];

            resolution << vtrack.v.i_width << 'x' << vtrack.v.i_height;
        }
        elem.set_attribute("resolution", resolution.str().c_str());
    }

    const auto mime_type = utils::get_mimetype(media.i_type, mux);
    const auto protocol_info = utils::http::get_dlna_extra_protocol_info(mime_type);

    elem.set_attribute("protocolInfo", protocol_info.c_str());

    if (main_files.size() >= 1)
    {
        elem.set_attribute("size", std::to_string(main_files[0].get().i_size).c_str());
    }

    return elem;
};

static void
dump_resources(xml::Element &dest, const vlc_ml_media_t &media, const std::string &file_extension)
{
    xml::Document &doc = dest.owner;

    const auto v_tracks = utils::get_media_tracks(media, VLC_ML_TRACK_TYPE_VIDEO);
    const auto main_files = utils::get_media_files(media, VLC_ML_FILE_TYPE_MAIN);

    dest.add_child(make_resource(doc, media, v_tracks, main_files, file_extension));

    // Thumbnails
    for (int i = 0; i < VLC_ML_THUMBNAIL_SIZE_COUNT; ++i)
    {
        const auto &thumbnail = media.thumbnails[i];
        if (thumbnail.i_status != VLC_ML_THUMBNAIL_STATUS_AVAILABLE)
            continue;
        const auto thumbnail_extension = utils::file_extension(std::string(thumbnail.psz_mrl));
        const auto url = utils::thumbnail_url(media, static_cast<vlc_ml_thumbnail_size_t>(i));
        auto elem = doc.create_element("res", doc.create_text_node(url.c_str()));

        const utils::MimeType mime{"image", "jpeg"};
        const auto protocol_info = utils::http::get_dlna_extra_protocol_info(mime);
        elem.set_attribute("protocolInfo", protocol_info.c_str());

        dest.add_child(std::move(elem));
    }
}

void Item::dump_mlobject_metadata(xml::Element &dest,
                                  const vlc_ml_media_t &media,
                                  const ml::MediaLibraryContext &ml)
{

    if (media.p_files->i_nb_items == 0)
        return;

    const vlc_ml_file_t &file = media.p_files->p_items[0];

    const std::string file_extension = utils::file_extension(file.psz_mrl);

    const char *object_class = nullptr;
    switch (media.i_type)
    {
        case VLC_ML_MEDIA_TYPE_AUDIO:
            object_class = "object.item.audioItem";
            break;
        case VLC_ML_MEDIA_TYPE_VIDEO:
            object_class = "object.item.videoItem";
            break;
        default:
            return;
    }

    const std::string date = std::to_string(media.i_year) + "-01-01";

    xml::Document &doc = dest.owner;

    dest.add_children(doc.create_element("upnp:class", doc.create_text_node(object_class)),
                      doc.create_element("dc:title", doc.create_text_node(media.psz_title)),
                      doc.create_element("dc:date", doc.create_text_node(date.c_str())));

    switch (media.i_subtype)
    {
        case VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK:
        {
            const auto album = ml::Album::get(ml, media.album_track.i_album_id);
            if (album != nullptr)
            {
                const auto album_thumbnail_url = utils::album_thumbnail_url(*album);
                dest.add_children(
                    doc.create_element("upnp:album", doc.create_text_node(album->psz_title)),
                    doc.create_element("upnp:artist", doc.create_text_node(album->psz_artist)),
                    doc.create_element("upnp:albumArtURI",
                                       doc.create_text_node(album_thumbnail_url.c_str())));
            }
            break;
        }
        default:
            break;
    }

    dump_resources(dest, media, file_extension);
}

void Item::dump_metadata(xml::Element &dest, const Object::ExtraId &extra_id) const
{
    assert(extra_id.has_value());
    const auto media = ml::Media::get(medialib_, extra_id->ml_id);

    dump_mlobject_metadata(dest, *media.get(), medialib_);
}

} // namespace cds
