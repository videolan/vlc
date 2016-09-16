/*
 * ID.cpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN and VLC authors
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

#include "ID.hpp"
#include <sstream>

using namespace adaptive;

ID::ID(const std::string &id_)
{
    id = id_;
}

ID::ID(uint64_t id_)
{
    std::stringstream ss;
    ss.imbue(std::locale("C"));
    ss << "default_id#" << id_;
    id = ss.str();
}

bool ID::operator==(const ID &other) const
{
    return (!id.empty() && id == other.id);
}

bool ID::operator<(const ID &other) const
{
    return (id.compare(other.id) < 0);
}

std::string ID::str() const
{
    return id;
}

