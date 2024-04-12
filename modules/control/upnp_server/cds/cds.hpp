/*****************************************************************************
 * cds.hpp : UPNP ContentDirectory Service entry point
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
#ifndef CDS_HPP
#define CDS_HPP

#include <vector>

#include "../ml.hpp"
#include "Object.hpp"

/// CDS is the short for ContentDirectory Service, its the services that clients should use to
/// browse the server file hierarchy
///
/// Specs: http://www.upnp.org/specs/av/UPnP-av-ContentDirectory-v3-Service-20080930.pdf
namespace cds
{

/// Split the given string and return the cds::Object id its refers to along with extra
/// informations on the object (for instance a medialibrary id).
/// A string id must follow this pattern:
///   "OBJ_ID:ML_ID(PARENT_ID)" where:
///     - OBJ_ID is the cds object index.
///     - ML_ID (optional) is a medialibrary id (A media, an album, whatever)
///     - PARENT_ID (optional) is the cds parent object, it's needed in case the parent has a ML_ID
///       bound to it
/// This pattern allows us to create "dynamics" object that reflects the structure of the
/// medialibrary database without actually duplicating it.
std::tuple<unsigned, Object::ExtraId> parse_id(const std::string &id);

/// Initialize the Upnp server objects hierarchy.
/// This needs to be called once at the startup of the server.
std::vector<std::unique_ptr<Object>> init_hierarchy(const ml::MediaLibraryContext &ml);

} // namespace cds

#endif /* CDS_HPP */
