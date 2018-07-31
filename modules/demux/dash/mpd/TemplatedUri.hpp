/*
 * TemplatedUri.hpp
 *****************************************************************************
 * Copyright (C) 2010 - 2014 VideoLAN and VLC Authors
 *               2018        VideoLabs
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
#ifndef TEMPLATEDURI_HPP
#define TEMPLATEDURI_HPP

#include <string>

namespace dash
{
    namespace mpd
    {
        class TemplatedUri
        {
            public:
                class Token
                {
                    public:
                        enum tokentype
                        {
                            TOKEN_ESCAPE,
                            TOKEN_TIME,
                            TOKEN_BANDWIDTH,
                            TOKEN_REPRESENTATION,
                            TOKEN_NUMBER,
                        } type;

                        std::string::size_type fulllength;
                        int width;
                };

                static bool IsDASHToken(const std::string &str,
                                        std::string::size_type pos,
                                        TemplatedUri::Token &token);

                class TokenReplacement
                {
                    public:
                        uint64_t value;
                        std::string str;
                };

                static std::string::size_type
                            ReplaceDASHToken(std::string &str,
                                             std::string::size_type pos,
                                             const TemplatedUri::Token &token,
                                             const TemplatedUri::TokenReplacement &repl);
        };
    }
}
#endif // TEMPLATEDURI_HPP
