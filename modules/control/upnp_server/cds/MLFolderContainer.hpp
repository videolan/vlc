/*****************************************************************************
 * MLFolderContainer.hpp : MediaLibrary IFolder container
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
#ifndef MLFOLDERCONTAINER_HPP
#define MLFOLDERCONTAINER_HPP

#include <vlc_cxx_helpers.hpp>
#include <vlc_media_library.h>
#include <vlc_url.h>

#include "../ml.hpp"
#include "Container.hpp"
#include "Item.hpp"

namespace cds
{
class MLFolderContainer : public Container
{
    static void dump_folder_metadata(xml::Element &dest, const vlc_ml_folder_t &folder)
    {
        xml::Document &doc = dest.owner;
        auto path = vlc::wrap_cptr(vlc_uri2path(folder.psz_mrl), &free);

        // Only keep the last folder from the path
        const char *folder_name = nullptr;
        if (path != nullptr && strlen(path.get()) > 0)
        {
#ifdef _WIN32
            const char sep = '\\';
#else
            const char sep = '/';
#endif

            for (auto i = strlen(path.get()) - 1; i > 0; --i)
            {
                if (path.get()[i] == sep)
                    path.get()[i] = '\0';
                else
                    break;
            }
            folder_name = strrchr(path.get(), '/') + 1;
        }

        dest.add_children(
            doc.create_element("dc:title",
                               doc.create_text_node(folder_name ? folder_name : path.get())),
            doc.create_element("upnp:class", doc.create_text_node("object.container")));
    }

  public:
    MLFolderContainer(int64_t id,
                      int64_t parent_id,
                      const char *name,
                      const ml::MediaLibraryContext &ml,
                      const Item &child) :
        Container(id, parent_id, name),
        ml_(ml),
        child_(child)
    {}

    void dump_metadata(xml::Element &dest, const Object::ExtraId &extra) const final
    {
        if (!extra.has_value())
        {
            dest.set_attribute("childCount", "0");

            xml::Document &doc = dest.owner;
            dest.add_child(
                doc.create_element("upnp:class", doc.create_text_node("object.container")));
            return;
        }
        const auto folder = ml::Folder::get(ml_, extra->ml_id);

        if (folder != nullptr)
        {
            dump_folder_metadata(dest, *folder);
        }
    }

    BrowseStats browse_direct_children(xml::Element &dest,
                                       const BrowseParams params,
                                       const Object::ExtraId &extra) const final
    {
        const vlc_ml_query_params_t query_params = params.to_query_params();
        assert(extra.has_value());

        const auto folder = ml::Folder::get(ml_, extra->ml_id);
        assert(folder != nullptr);

        xml::Document &doc = dest.owner;

        const auto subfolder_list = ml::SubfoldersList::list(ml_, &query_params, extra->ml_id);
        if (subfolder_list)
        {
            for (auto i = 0u; i < subfolder_list->i_nb_items; ++i)
            {
                const auto &folder = subfolder_list->p_items[i];
                auto elem =
                    create_object_element(doc, ExtraIdData{folder.i_id, get_dynamic_id(extra)});
                dump_folder_metadata(elem, folder);
                dest.add_child(std::move(elem));
            }
        }

        const auto media_list = ml::MediaFolderList::list(ml_, &query_params, extra->ml_id);
        if (media_list)
        {
            for (auto i = 0u; i < media_list->i_nb_items; ++i)
            {
                const auto &media = media_list->p_items[i];
                auto elem = child_.create_object_element(
                    doc, ExtraIdData{child_.id, get_dynamic_id(extra)});
                Item::dump_mlobject_metadata(elem, media, ml_);
                dest.add_child(std::move(elem));
            }
        }

        BrowseStats stats;
        stats.result_count = subfolder_list->i_nb_items + media_list->i_nb_items;
        stats.total_matches = ml::SubfoldersList::count(ml_, extra->ml_id) +
                              ml::MediaFolderList::count(ml_, extra->ml_id);
        return stats;
    }

  public:
    const ml::MediaLibraryContext &ml_;
    const Item &child_;
};
} // namespace cds

#endif /* MLFOLDERCONTAINER_HPP */
