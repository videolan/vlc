/*****************************************************************************
 * Role.hpp
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
#ifndef ROLE_HPP_
#define ROLE_HPP_

namespace adaptive
{
    namespace playlist
    {
        class Role
        {
            public:
                static const unsigned ROLE_MAIN           = 0;
                static const unsigned ROLE_ALTERNATE      = 1;
                static const unsigned ROLE_SUPPLEMENTARY  = 2;
                static const unsigned ROLE_COMMENTARY     = 3;
                static const unsigned ROLE_DUB            = 4;
                static const unsigned ROLE_CAPTION        = 5;
                static const unsigned ROLE_SUBTITLE       = 6;
                Role(unsigned = ROLE_ALTERNATE);
                bool operator==(const Role &) const;
                bool isDefault() const;
                bool autoSelectable() const;

            private:
                unsigned value;
        };
    }
}

#endif
