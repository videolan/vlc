/*****************************************************************************
 * httpcookies.h: HTTP cookie utilities
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antti Ajanki <antti.ajanki@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef HTTPCOOKIES_H_
#define HTTPCOOKIES_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <vlc_url.h>
#include <vlc_arrays.h>

typedef struct vlc_array_t http_cookie_jar_t;

http_cookie_jar_t * http_cookies_new( void );
void http_cookies_destroy( http_cookie_jar_t * p_jar );

/**
 * Parse a value of an incoming Set-Cookie header and append the
 * cookie to the cookie jar if appropriate.
 *
 * @param p_jar cookie jar object
 * @param psz_cookie_header value of Set-Cookie
 * @return true, if the cookie was added, false otherwise
 */
bool http_cookies_append( http_cookie_jar_t * p_jar, const char * psz_cookie_header, const vlc_url_t * p_url );

/**
 * Returns a cookie value that match the given URL.
 *
 * @params p_jar a cookie jar
 * @params p_url the URL for which the cookies are returned
 * @return A string consisting of semicolon-separated cookie NAME=VALUE pairs.
 */
char *http_cookies_for_url( http_cookie_jar_t * p_jar, const vlc_url_t * p_url );

#ifdef __cplusplus
}
#endif

#endif
