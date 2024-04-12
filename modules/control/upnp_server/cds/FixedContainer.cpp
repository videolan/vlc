/*****************************************************************************
 * FixedContainer.cpp
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

#include "FixedContainer.hpp"

namespace cds
{

FixedContainer::FixedContainer(const int64_t id,
                               const int64_t parent_id,
                               const char *name,
                               std::initializer_list<ObjRef> children) :
    Container(id, parent_id, name),
    children(children)
{}

FixedContainer::FixedContainer(const int64_t id, const int64_t parent_id, const char *name)
    :
    Container(id, parent_id, name)
{}

Container::BrowseStats FixedContainer::browse_direct_children(xml::Element &dest,
                                                              BrowseParams params,
                                                              const Object::ExtraId &extra) const
{
    params.requested =
        std::min(static_cast<size_t>(params.offset) + params.requested, children.size());

    unsigned i = 0;
    for (; i + params.offset < params.requested; ++i)
    {
        const Object &child = children.at(i + params.offset);
        dest.add_child(child.browse_metadata(dest.owner, extra));
    }
    return {i, children.size()};
}

void FixedContainer::dump_metadata(xml::Element &dest, const Object::ExtraId &) const
{
    dest.set_attribute("childCount", std::to_string(children.size()).c_str());

    xml::Document &doc = dest.owner;
    dest.add_child(doc.create_element("upnp:class", doc.create_text_node("object.container")));
}

void FixedContainer::add_children(std::initializer_list<ObjRef> l)
{
    for (Object &child : l)
    {
        child.parent_id = id;
    }
    children.insert(children.end(), l.begin(), l.end());
}
} // namespace cds
