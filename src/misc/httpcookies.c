/*****************************************************************************
 * httpcookies.c: HTTP cookie utilities
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_messages.h>
#include <vlc_strings.h>
#include <vlc_http.h>

typedef struct http_cookie_t
{
    char *psz_name;
    char *psz_value;
    char *psz_domain;
    char *psz_path;
    bool b_host_only;
    bool b_secure;
} http_cookie_t;

/* Get the NAME=VALUE part of the Cookie */
static char *cookie_get_content( const char *cookie )
{
    size_t content_length = strcspn( cookie, ";" );
    return strndup( cookie, content_length );
}

static char *cookie_get_attribute_value( const char *cookie, const char *attr )
{
    assert( attr != NULL );

    if( cookie == NULL )
        return NULL;

    size_t attrlen = strlen( attr );
    const char * str = strchr( cookie, ';' );
    while( str )
    {
        /* skip ; and blank */
        str++;
        str = str + strspn( str, " " );

        if( !vlc_ascii_strncasecmp( str, attr, attrlen )
         && (str[attrlen] == '=') )
        {
            str += attrlen + 1;
            size_t value_length = strcspn( str, ";" );
            return strndup( str, value_length );
        }

        str = strchr( str, ';' );
    }
    return NULL;
}

static bool cookie_has_attribute( const char *cookie, const char *attr )
{
    assert( attr != NULL );

    if( cookie == NULL )
        return false;

    size_t attrlen = strlen(attr);
    const char * str = strchr(cookie, ';');
    while( str )
    {
        /* skip ; and blank */
        str++;
        str += strspn( str, " " );

        if( !vlc_ascii_strncasecmp( str, attr, attrlen )
         && (str[attrlen] == '=' || str[attrlen] == ';' || str[attrlen] == '\0') )
            return true;

        str = strchr(str, ';');
    }
    return false;
}

/* Get the domain where the cookie is stored */
static char *cookie_get_domain( const char *cookie )
{
    char *domain = cookie_get_attribute_value( cookie, "domain" );
    if( domain == NULL )
        return NULL;

    if( domain[0] == '.' )
    {
        const char *real_domain = domain + strspn( domain, "." );
        memmove( domain, real_domain, strlen( real_domain ) + 1 );
    }
    return domain;
}

static bool cookie_domain_matches( const http_cookie_t *cookie,
                                   const char *host )
{
    assert( cookie == NULL || cookie->psz_domain != NULL );

    // TODO: should convert domain names to punycode before comparing

    if ( cookie == NULL || host == NULL )
        return false;
    if ( vlc_ascii_strcasecmp(cookie->psz_domain, host) == 0 )
        return true;
    else if ( cookie->b_host_only )
        return false;

    size_t host_len = strlen(host);
    size_t cookie_domain_len = strlen(cookie->psz_domain);
    bool is_suffix = false, has_dot_before_suffix = false;

    if( host_len > cookie_domain_len )
    {
        size_t i = host_len - cookie_domain_len;

        is_suffix = vlc_ascii_strcasecmp( &host[i], cookie->psz_domain ) == 0;
        has_dot_before_suffix = host[i-1] == '.';
    }

    bool host_is_ipv4 = strspn(host, "0123456789.") == host_len;
    bool host_is_ipv6 = strchr(host, ':') != NULL;
    return is_suffix && has_dot_before_suffix &&
        !( host_is_ipv4 || host_is_ipv6 );
}

static bool cookie_path_matches( const http_cookie_t * cookie, const char *uripath )
{
    if ( cookie == NULL || uripath == NULL )
        return false;
    else if ( strcmp(cookie->psz_path, uripath) == 0 )
        return true;

    size_t path_len = strlen( uripath );
    size_t prefix_len = strlen( cookie->psz_path );
    return ( path_len > prefix_len ) &&
        ( strncmp(uripath, cookie->psz_path, prefix_len) == 0 ) &&
        ( uripath[prefix_len - 1] == '/' || uripath[prefix_len] == '/' );
}

static bool cookie_domain_is_public_suffix( const char *domain )
{
    // FIXME: should check if domain is one of "public suffixes" at
    // http://publicsuffix.org/. The purpose of this check is to
    // prevent a host from setting a "too wide" cookie, for example
    // "example.com" should not be able to set a cookie for "com".
    // The current implementation prevents all top-level domains.
    return domain != NULL && !strchr(domain, '.');
}

static bool cookie_should_be_sent(const http_cookie_t *cookie, bool secure,
                                  const char *host, const char *path)
{
    bool protocol_ok = secure || !cookie->b_secure;
    bool domain_ok = cookie_domain_matches(cookie, host);
    bool path_ok = cookie_path_matches(cookie, path);
    return protocol_ok && domain_ok && path_ok;
}

/* Check if a cookie from host should be added to the cookie jar */
static bool cookie_is_valid(const http_cookie_t * cookie, bool secure,
                            const char *host, const char *path)
{
    return cookie && cookie->psz_name && strlen(cookie->psz_name) > 0 &&
        cookie->psz_domain &&
        !cookie_domain_is_public_suffix(cookie->psz_domain) &&
        cookie_domain_matches(cookie, host);
}

static char *cookie_default_path( const char *request_path )
{
    if ( request_path == NULL || request_path[0] != '/' )
        return strdup("/");

    char *path;
    const char *query_start = strchr( request_path, '?' );
    if ( query_start != NULL )
        path = strndup( request_path, query_start - request_path );
    else
        path = strdup( request_path );

    if ( path == NULL )
        return NULL;

    char *last_slash = strrchr(path, '/');
    assert(last_slash != NULL);
    if ( last_slash == path )
        path[1] = '\0';
    else
        *last_slash = '\0';

    return path;
}

static void cookie_destroy( http_cookie_t * p_cookie )
{
    if ( !p_cookie )
        return;

    free( p_cookie->psz_name );
    free( p_cookie->psz_value );
    free( p_cookie->psz_domain );
    free( p_cookie->psz_path );
    free( p_cookie );
}

struct vlc_http_cookie_jar_t
{
    vlc_array_t cookies;
    vlc_mutex_t lock;
};

vlc_http_cookie_jar_t * vlc_http_cookies_new()
{
    vlc_http_cookie_jar_t * jar = malloc( sizeof( vlc_http_cookie_jar_t ) );
    if ( unlikely(jar == NULL) )
        return NULL;

    vlc_array_init( &jar->cookies );
    vlc_mutex_init( &jar->lock );

    return jar;
}

void vlc_http_cookies_destroy( vlc_http_cookie_jar_t * p_jar )
{
    if ( !p_jar )
        return;

    int i;
    for( i = 0; i < vlc_array_count( &p_jar->cookies ); i++ )
        cookie_destroy( vlc_array_item_at_index( &p_jar->cookies, i ) );

    vlc_array_clear( &p_jar->cookies );
    vlc_mutex_destroy( &p_jar->lock );

    free( p_jar );
}

static http_cookie_t *cookie_parse(const char *value,
                                   const char *host, const char *path)
{
    http_cookie_t *cookie = calloc( 1, sizeof( http_cookie_t ) );
    if ( unlikely(cookie == NULL) )
        return NULL;

    char *content = cookie_get_content(value);
    if ( !content )
    {
        cookie_destroy( cookie );
        return NULL;
    }

    const char *eq = strchr( content, '=' );
    if ( eq )
    {
        cookie->psz_name = strndup( content, eq-content );
        cookie->psz_value = strdup( eq + 1 );
    }
    else
    {
        cookie->psz_name = strdup( content );
        cookie->psz_value = NULL;
    }

    cookie->psz_domain = cookie_get_domain(value);
    if ( cookie->psz_domain == NULL || strlen(cookie->psz_domain) == 0 )
    {
        free(cookie->psz_domain);
        cookie->psz_domain = strdup(host);
        cookie->b_host_only = true;
    }
    else
        cookie->b_host_only = false;

    cookie->psz_path = cookie_get_attribute_value(value, "path" );
    if ( cookie->psz_path == NULL || strlen(cookie->psz_path) == 0 )
    {
        free(cookie->psz_path);
        cookie->psz_path = cookie_default_path(path);
    }

    cookie->b_secure = cookie_has_attribute(value, "secure" );

    FREENULL( content );

    if ( cookie->psz_domain == NULL || cookie->psz_path == NULL
     || cookie->psz_name == NULL )
    {
        cookie_destroy( cookie );
        return NULL;
    }

    return cookie;
}

bool vlc_http_cookies_store(vlc_http_cookie_jar_t *p_jar, const char *cookies,
                            bool secure, const char *host, const char *path)
{
    assert(host != NULL);
    assert(path != NULL);

    int i;

    http_cookie_t *cookie = cookie_parse(cookies, host, path);
    if (cookie == NULL)
        return false;
    if (!cookie_is_valid(cookie, secure, host, path))
    {
        cookie_destroy(cookie);
        return false;
    }

    vlc_mutex_lock( &p_jar->lock );

    for( i = 0; i < vlc_array_count( &p_jar->cookies ); i++ )
    {
        http_cookie_t *iter = vlc_array_item_at_index( &p_jar->cookies, i );

        assert( iter->psz_name );
        assert( iter->psz_domain );
        assert( iter->psz_path );

        bool domains_match =
            vlc_ascii_strcasecmp( cookie->psz_domain, iter->psz_domain ) == 0;
        bool paths_match = strcmp( cookie->psz_path, iter->psz_path ) == 0;
        bool names_match = strcmp( cookie->psz_name, iter->psz_name ) == 0;
        if( domains_match && paths_match && names_match )
        {
            /* Remove previous value for this cookie */
            vlc_array_remove( &p_jar->cookies, i );
            cookie_destroy(iter);
            break;
        }
    }
    vlc_array_append( &p_jar->cookies, cookie );

    vlc_mutex_unlock( &p_jar->lock );

    return true;
}

bool vlc_http_cookies_append(vlc_http_cookie_jar_t *jar,
                             const char *cookies, const vlc_url_t *url)
{
    bool secure;

    if (url->psz_protocol == NULL || url->psz_host == NULL
     || url->psz_path == NULL)
        return false;
    else if (!vlc_ascii_strcasecmp(url->psz_protocol, "https"))
        secure = true;
    else
        secure = false;

    return vlc_http_cookies_store(jar, cookies, secure, url->psz_host,
                                  url->psz_path);
}

char *vlc_http_cookies_fetch(vlc_http_cookie_jar_t *p_jar, bool secure,
                             const char *host, const char *path)
{
    int i;
    char *psz_cookiebuf = NULL;

    vlc_mutex_lock( &p_jar->lock );

    for( i = 0; i < vlc_array_count( &p_jar->cookies ); i++ )
    {
        const http_cookie_t * cookie = vlc_array_item_at_index( &p_jar->cookies, i );
        if (cookie_should_be_sent(cookie, secure, host, path))
        {
            char *psz_updated_buf = NULL;
            if ( asprintf(&psz_updated_buf, "%s%s%s=%s",
                          psz_cookiebuf ? psz_cookiebuf : "",
                          psz_cookiebuf ? "; " : "",
                          cookie->psz_name ? cookie->psz_name : "",
                          cookie->psz_value ? cookie->psz_value : "") == -1 )
            {
                // TODO: report error
                free( psz_cookiebuf );
                vlc_mutex_unlock( &p_jar->lock );
                return NULL;
            }
            free( psz_cookiebuf );
            psz_cookiebuf = psz_updated_buf;
        }
    }

    vlc_mutex_unlock( &p_jar->lock );

    return psz_cookiebuf;
}

char *vlc_http_cookies_for_url(vlc_http_cookie_jar_t *jar,
                               const vlc_url_t *url)
{
    bool secure;

    if (url->psz_protocol == NULL || url->psz_host == NULL
     || url->psz_path == NULL)
        return NULL;
    else if (!vlc_ascii_strcasecmp(url->psz_protocol, "https"))
        secure = true;
    else
        secure = false;

    return vlc_http_cookies_fetch(jar, secure, url->psz_host, url->psz_path);
}
