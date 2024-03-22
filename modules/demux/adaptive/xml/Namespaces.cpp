/*
 * Namespaces.cpp
 *****************************************************************************
 * Copyright (C) 2024 - VideoLabs, VideoLAN and VLC Authors
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

#include "Namespaces.hpp"

#include <algorithm>
#include <cstring>

using namespace adaptive::xml;

Namespaces::Ptr Namespaces::registerNamespace(const char *ns)
{
    if(ns == nullptr)
        ns = "";
    Ptr ptr = getNamespace(ns);
    if(ptr == nullptr)
        ptr = std::make_shared<Entry>(std::string(ns));
    return ptr;
}

Namespaces::Ptr Namespaces::getNamespace(const std::string &ns)
{
    auto it = std::find_if(nss.begin(), nss.end(),
                        [ns](Ptr &e){ return *e == ns; });
    return it != nss.end() ? *it : nullptr;
}

Namespaces::Ptr Namespaces::getNamespace(const char *ns)
{
    auto it = std::find_if(nss.begin(), nss.end(),
                        [ns](Ptr &e){ return !std::strcmp(e.get()->c_str(), ns); });
    return it != nss.end() ? *it : nullptr;
}
