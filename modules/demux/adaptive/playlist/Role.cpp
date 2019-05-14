/*****************************************************************************
 * Role.cpp
 *****************************************************************************
 * Copyright (C) 2019 VideoLabs, VideoLAN and VLC Authors
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

#include "Role.hpp"

using namespace adaptive;
using namespace adaptive::playlist;

Role::Role(unsigned r)
{
    value = r;
}

bool Role::operator ==(const Role &other) const
{
    return value == other.value;
}

bool Role::isDefault() const
{
    return value == ROLE_MAIN;
}

bool Role::autoSelectable() const
{
    return value == ROLE_MAIN ||
           value == ROLE_ALTERNATE ||
           value == ROLE_SUBTITLE ||
           value == ROLE_CAPTION;
}
