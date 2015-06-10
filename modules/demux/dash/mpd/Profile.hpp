/*
 * Profile.hpp
 *****************************************************************************
 * Copyright (C) 2010 - 2014 VideoLAN and VLC Authors
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
#ifndef PROFILE_HPP
#define PROFILE_HPP

#include <string>

namespace dash
{
    namespace mpd
    {
        class Profile
        {
            public:
                enum Name
                {
                    Unknown,
                    Full,
                    ISOOnDemand,
                    ISOMain,
                    ISOLive,
                    MPEG2TSMain,
                    MPEG2TSSimple,
                };
                Profile(Name);
                Profile(const std::string &);
                bool operator==(Profile &) const;
                operator Profile::Name ();
                operator std::string ();

            private:
                Name getNameByURN(const std::string &) const;
                Name type;
        };
    }
}
#endif // PROFILE_HPP
