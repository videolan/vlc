/*
 * Node.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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
# include "config.h"
#endif

#include "Node.h"

#include <cassert>
#include <algorithm>
#include <vlc_common.h>
#include <vlc_xml.h>

using namespace adaptive::xml;

const std::string   Node::EmptyString = "";

bool Node::Attribute::matches(const std::string &name, const std::string &ns) const
{
    return *this->ns == ns && this->name == name;
}

Node::Node(std::unique_ptr<std::string> name, Namespaces::Ptr ns)
{
    this->name = std::move(name);
    this->ns = ns;
}

Node::~Node ()
{
    for(size_t i = 0; i < this->subNodes.size(); i++)
        delete(this->subNodes.at(i));
}

const std::vector<Node*>&           Node::getSubNodes           () const
{
    return this->subNodes;
}
void                                Node::addSubNode            (Node *node)
{
    this->subNodes.push_back(node);
}
const std::string&                  Node::getName               () const
{
    return *name;
}

const std::string & Node::getNamespace() const
{
    if(ns != nullptr)
        return *ns;
    return EmptyString;
}

bool Node::hasAttribute(const std::string& name) const
{
    return hasAttribute(name, EmptyString);
}

bool Node::hasAttribute(const std::string& name, const std::string &ns) const
{
    auto it = std::find_if(attributes.cbegin(),attributes.cend(),
                           [name, ns](const class Attribute &a)
                                {return a.name == name && ns == *a.ns;});
    return it != attributes.cend();
}

const std::string& Node::getAttributeValue(const std::string& key) const
{
    return getAttributeValue(key, EmptyString);
}

const std::string& Node::getAttributeValue(const std::string& key, const std::string &ns) const
{
    auto it = std::find_if(attributes.cbegin(),attributes.cend(),
                           [key, ns](const class Attribute &a)
                                {return a.name == key && ns == *a.ns;});
    if (it != attributes.cend())
        return (*it).value;
    return EmptyString;
}

void Node::addAttribute(const std::string& key, Namespaces::Ptr ns, const std::string& value)
{
    class Attribute attr;
    attr.name = key;
    attr.ns = ns;
    attr.value = value;
    attributes.push_back(std::move(attr));
}

const std::string&                         Node::getText               () const
{
    return text;
}

void Node::setText(const std::string &text)
{
    this->text = text;
}

const Node::Attributes& Node::getAttributes() const
{
    return this->attributes;
}

bool Node::matches(const std::string &name, const std::string &ns) const
{
    return *this->ns == ns && *this->name == name;
}
