/*
 * Url.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC Authors
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

#include "Url.hpp"
#include "BaseRepresentation.h"
#include "SegmentTemplate.h"

#include <vlc_url.h>

using namespace adaptive::playlist;

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

bool Url::empty() const
{
    return components.empty();
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
    if(!components.empty() && url.components.front().b_absolute)
    {
        if(components.front().b_scheme)
        {
            while(components.size() > 1)
                components.pop_back();
            std::string scheme(components.front().component);
            std::size_t schemepos = scheme.find_first_of("://");
            if(schemepos != std::string::npos)
            {
                std::size_t pathpos = scheme.find_first_of('/', schemepos + 3);
                if(pathpos != std::string::npos)
                    components.front().component = scheme.substr(0, pathpos);
                /* otherwise should be domain only */
            }
        }
    }

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
        std::string part;
        const Component *comp = & (*it);
        if(rep)
            part = rep->contextualize(index, comp->component, comp->templ);
        else
            part = comp->component;

        if( ret.empty() )
            ret = part;
        else
        {
            char *psz_fixup = vlc_uri_fixup( part.c_str() );
            char *psz_resolved = vlc_uri_resolve( ret.c_str(),
                                                  psz_fixup ? psz_fixup : part.c_str() );
            free(psz_fixup);
            if( psz_resolved )
            {
                ret = std::string( psz_resolved );
                free( psz_resolved );
            }
        }
    }

    return ret;
}

Url::Component::Component(const std::string & str, const BaseSegmentTemplate *templ_)
 : component(str), templ(templ_), b_scheme(false), b_dir(false), b_absolute(false)
{
    if(!component.empty())
    {
        b_dir = (component[component.length()-1]=='/');
        b_scheme = (component.find_first_of("://") == (component.find_first_of('/') - 1));
        b_absolute = (component[0] =='/');
    }
}
