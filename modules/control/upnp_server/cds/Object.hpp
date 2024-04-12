/*****************************************************************************
 * Object.hpp : CDS Object interface implementation
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
#ifndef OBJECT_HPP
#define OBJECT_HPP

#include "../xml_wrapper.hpp"

#include <optional>
#include <string>

namespace cds
{
/// Opaque CDS object type
/// Specs: http://www.upnp.org/specs/av/UPnP-av-ContentDirectory-v3-Service-20080930.pdf
/// "2.2.2 - Object"
struct Object
{
    enum class Type
    {
        Item,
        Container
    };

    struct ExtraIdData
    {
        int64_t ml_id;
        std::string parent;
    };

    using ExtraId = std::optional<ExtraIdData>;

    int64_t id;
    int64_t parent_id;
    const char *name;
    const Type type;

    Object(int64_t id, int64_t parent_id, const char *name, const Type type) noexcept :
        id(id),
        parent_id(parent_id),
        name(name),
        type(type)
    {}

    virtual ~Object() = default;

    /// Create an xml element describing the object.
    xml::Element browse_metadata(xml::Document &doc, const ExtraId &extra_id) const
    {
        auto ret = create_object_element(doc, extra_id);

        dump_metadata(ret, extra_id);

        return ret;
    }

    /// Utility function to create the xml common representation of an object.
    xml::Element create_object_element(xml::Document &doc, const ExtraId &extra_id) const
    {
        auto ret = doc.create_element(type == Type::Item ? "item" : "container");

        if (extra_id.has_value())
        {
            ret.set_attribute("id", get_dynamic_id(extra_id).c_str());
            ret.set_attribute("parentID", extra_id->parent.c_str());
        }
        else
        {
            ret.set_attribute("id", std::to_string(id).c_str());
            ret.set_attribute("parentID", std::to_string(parent_id).c_str());
        }

        if (name)
        {
            ret.add_child(doc.create_element("dc:title", doc.create_text_node(name)));
        }
        ret.set_attribute("restricted", "1");
        return ret;
    }

  protected:
    /// Build an Object id based on the extra id provided,
    /// Some Objects can have a changing id based on the medialib id they expose, for example,
    /// "1:3" or "1:43" are both valid, they just expose different contents through the same object
    /// tied to the id "1".
    std::string get_dynamic_id(const ExtraId &extra_id) const
    {
        if (!extra_id.has_value())
            return std::to_string(id);
        const std::string ml_id_str = std::to_string(extra_id->ml_id);
        if (!extra_id->parent.empty())
            return std::to_string(id) + ':' + ml_id_str + '(' + extra_id->parent + ')';
        return std::to_string(id) + ':' + ml_id_str;
    }

    /// Dump Object specialization specific informations in the fiven xml element
    virtual void dump_metadata(xml::Element &dest, const ExtraId &extra_id) const = 0;
};
} // namespace cds

#endif /* OBJECT_HPP */
