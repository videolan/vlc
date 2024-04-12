/*****************************************************************************
 * Container.hpp : CDS Container interface
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
#ifndef CONTAINER_HPP
#define CONTAINER_HPP

#include "Object.hpp"

#include <vlc_media_library.h>
namespace cds
{
/// Opaque CDS container type
/// Specs: http://www.upnp.org/specs/av/UPnP-av-ContentDirectory-v3-Service-20080930.pdf
/// "2.2.8 - Container"
class Container : public Object
{
  public:
    struct BrowseParams
    {
        uint32_t offset;
        uint32_t requested;

        vlc_ml_query_params_t to_query_params() const {
            vlc_ml_query_params_t query_params = vlc_ml_query_params_create();
            query_params.i_nbResults = requested;
            query_params.i_offset = offset;
            return query_params;
        }
    };

    struct BrowseStats
    {
        size_t result_count;
        size_t total_matches;
    };

    Container(const int64_t id,
              const int64_t parent_id,
              const char *name) noexcept :
        Object(id, parent_id, name, Object::Type::Container) {}

    /// Go through all the container children and dump them to the given xml element
    virtual BrowseStats
    browse_direct_children(xml::Element &, BrowseParams, const Object::ExtraId &) const = 0;
};
} // namespace cds

#endif /* CONTAINER_HPP */
