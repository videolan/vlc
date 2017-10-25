/*
 * AuthStorage.cpp
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "AuthStorage.hpp"
#include "ConnectionParams.hpp"

using namespace adaptive::http;

AuthStorage::AuthStorage( vlc_object_t *p_obj )
{
    if ( var_InheritBool( p_obj, "http-forward-cookies" ) )
        p_cookies_jar = static_cast<vlc_http_cookie_jar_t *>
                (var_InheritAddress( p_obj, "http-cookies" ));
    else
        p_cookies_jar = NULL;
}

AuthStorage::~AuthStorage()
{
}

void AuthStorage::addCookie( const std::string &cookie, const ConnectionParams &params )
{
    if( !p_cookies_jar )
        return;
    vlc_http_cookies_store( p_cookies_jar, cookie.c_str(),
                            params.getHostname().c_str(), params.getPath().c_str() );
}

std::string AuthStorage::getCookie( const ConnectionParams &params, bool secure )
{
    if( !p_cookies_jar )
        return std::string();
    char *psz = vlc_http_cookies_fetch( p_cookies_jar, secure,
                                        params.getHostname().c_str(),
                                        params.getPath().c_str() );
    std::string ret;
    if( psz )
    {
        ret = std::string(psz);
        free( psz );
    }
    return ret;
}
