/*
 * AuthStorage.hpp
 *****************************************************************************
 * Copyright (C) 2017 - VideoLabs and VideoLAN Authors
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
#ifndef AUTHSTORAGE_HPP_
#define AUTHSTORAGE_HPP_

#include <vlc_common.h>
#include <vlc_http.h>

#include <string>

namespace adaptive
{
    namespace http
    {
        class ConnectionParams;

        class AuthStorage
        {
            public:
                AuthStorage(vlc_object_t *p_obj);
                ~AuthStorage();
                void addCookie( const std::string &cookie, const ConnectionParams & );
                std::string getCookie( const ConnectionParams &, bool secure );

            private:
                vlc_http_cookie_jar_t *p_cookies_jar;
        };
    }
}

#endif
