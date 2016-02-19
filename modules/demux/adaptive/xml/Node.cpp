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
#include <vlc_common.h>
#include <vlc_xml.h>

using namespace adaptive::xml;

const std::string   Node::EmptyString = "";

Node::Node() :
    type( -1 )
{
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
    return this->name;
}
void                                Node::setName               (const std::string& name)
{
    this->name = name;
}

bool                                Node::hasAttribute        (const std::string& name) const
{
    if(this->attributes.find(name) != this->attributes.end())
        return true;

    return false;
}
const std::string&                  Node::getAttributeValue     (const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator  it = this->attributes.find( key );

    if ( it != this->attributes.end() )
        return it->second;
    return EmptyString;
}

void                                Node::addAttribute          ( const std::string& key, const std::string& value)
{
    this->attributes[key] = value;
}
std::vector<std::string>            Node::getAttributeKeys      () const
{
    std::vector<std::string> keys;
    std::map<std::string, std::string>::const_iterator it;

    for(it = this->attributes.begin(); it != this->attributes.end(); ++it)
    {
        keys.push_back(it->first);
    }
    return keys;
}

const std::string&                         Node::getText               () const
{
    return text;
}

void Node::setText(const std::string &text)
{
    this->text = text;
}

const std::map<std::string,std::string>&   Node::getAttributes         () const
{
    return this->attributes;
}

int Node::getType() const
{
    return this->type;
}

void Node::setType(int type)
{
    this->type = type;
}

std::vector<std::string> Node::toString(int indent) const
{
    std::vector<std::string> ret;
    std::string text(indent, ' ');
    text.append(getName());
    ret.push_back(text);
    std::vector<Node *>::const_iterator l;
    for(l = subNodes.begin(); l < subNodes.end(); ++l)
    {
        std::vector<std::string> sub = (*l)->toString(indent + 1);
        ret.insert(ret.end(), sub.begin(), sub.end());
    }
    return ret;
}
