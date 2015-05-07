/*
 * Url.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN Authors
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
#include "Url.hpp"
#include "BaseRepresentation.h"
#include "SegmentTemplate.h"

using namespace adaptative::playlist;

Url::Url()
{
}

Url::Url(const Component & comp)
{
    prepend(comp);
}

Url::Url(const std::string &str)
{
    prepend(Component(str));
}

bool Url::hasScheme() const
{
    if(components.empty())
        return false;

    return components[0].b_scheme;
}

Url & Url::prepend(const Component & comp)
{
    components.insert(components.begin(), comp);
    return *this;
}

Url & Url::append(const Component & comp)
{
    if(!components.empty() && !components.back().b_dir)
        components.pop_back();
    components.push_back(comp);
    return *this;
}

Url & Url::prepend(const Url &url)
{
    components.insert(components.begin(), url.components.begin(), url.components.end());
    return *this;
}

Url & Url::append(const Url &url)
{
    if(!components.empty() && !components.back().b_dir)
        components.pop_back();
    components.insert(components.end(), url.components.begin(), url.components.end());
    return *this;
}

std::string Url::toString() const
{
    return toString(0, NULL);
}

std::string Url::toString(size_t index, const BaseRepresentation *rep) const
{
    std::string ret;
    std::vector<Component>::const_iterator it;
    for(it = components.begin(); it != components.end(); ++it)
    {
        const Component *comp = & (*it);
        if(rep)
            ret.append(rep->contextualize(index, comp->component, comp->templ));
        else
            ret.append(comp->component);
    }
    return ret;
}

Url::Component::Component(const std::string & str, const MediaSegmentTemplate *templ_)
{
    component = str;
    templ = templ_;
    if(!component.empty())
    {
        b_dir = (component[component.length()-1]=='/');
        b_scheme = !component.compare(0, 7, "http://") || !component.compare(0, 8, "https://");
    }
}

