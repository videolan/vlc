/*****************************************************************************
 * xml_wrapper.hpp : Modern C++ ixml wrapper
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
#ifndef XML_WRAPPER_HPP
#define XML_WRAPPER_HPP

#include <cassert>
#include <ixml.h>
#include <memory>

/// Simple C++ wrapper around libixml xml library.
namespace xml
{

struct Document;

struct Node
{
    using Ptr = std::unique_ptr<IXML_Node, decltype(&ixmlNode_free)>;
    Ptr ptr;
};

struct Element
{
    using Ptr = std::unique_ptr<IXML_Element, decltype(&ixmlElement_free)>;

    Ptr ptr;
    Document &owner;

    void set_attribute(const char *name, const char *value)
    {
        assert(ptr != nullptr);
        ixmlElement_setAttribute(ptr.get(), name, value);
    }

    void add_child(Node&& child)
    {
        assert(ptr != nullptr);
        if (child.ptr != nullptr)
            ixmlNode_appendChild(&ptr->n, child.ptr.release());
    }

    void add_child(Element&& child)
    {
        assert(ptr != nullptr);
        if (child.ptr != nullptr)
            ixmlNode_appendChild(&ptr->n, &child.ptr.release()->n);
    }

    template <typename... Child> void add_children(Child &&...child)
    {
        (add_child(std::move(child)), ...);
    }
};

struct Document
{
    using Ptr = std::unique_ptr<IXML_Document, decltype(&ixmlDocument_free)>;
    Ptr ptr;

    Document() : ptr{ixmlDocument_createDocument(), &ixmlDocument_free} {}

    Document(Ptr) = delete;
    Document(Ptr &&) = delete;

    Node create_text_node(const char *text)
    {
        return Node{Node::Ptr{ixmlDocument_createTextNode(ptr.get(), text), ixmlNode_free}};
    }

    Element create_element(const char *name)
    {
        return Element{Element::Ptr{ixmlDocument_createElement(ptr.get(), name), &ixmlElement_free},
                       *this};
    }

    template <typename... Children> Element create_element(const char *name, Children &&...children)
    {
        Element ret = create_element(name);
        ret.add_children(children...);
        return ret;
    }

    void set_entry(Element &&entry) { ixmlNode_appendChild(&ptr->n, &entry.ptr.release()->n); }

    using WrappedDOMString = std::unique_ptr<char, decltype(&ixmlFreeDOMString)>;

    WrappedDOMString to_wrapped_cstr() const
    {
        return WrappedDOMString{ixmlDocumenttoString(ptr.get()), &ixmlFreeDOMString};
    }
};

} // namespace xml

#endif /* XML_WRAPPER_HPP */
