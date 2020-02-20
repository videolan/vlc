/*****************************************************************************
 * httpcookies.c: HTTP cookie utilities
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
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

static char *cookie_get_attribute_value( const char *cookie, const char *attr )
{
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
    // TODO: should convert domain names to punycode before comparing

    if (host == NULL)
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

static char *cookie_get_path(const char *cookie)
{
    return cookie_get_attribute_value(cookie, "path");
}

static bool cookie_path_matches( const http_cookie_t * cookie, const char *uripath )
{
    if (uripath == NULL )
        return false;
    else if ( strcmp(cookie->psz_path, uripath) == 0 )
        return true;

    size_t path_len = strlen( uripath );
    size_t prefix_len = strlen( cookie->psz_path );
    return ( path_len > prefix_len ) &&
        ( strncmp(uripath, cookie->psz_path, prefix_len) == 0 ) &&
        ( uripath[prefix_len - 1] == '/' || uripath[prefix_len] == '/' );
}

static bool cookie_should_be_sent(const http_cookie_t *cookie, bool secure,
                                  const char *host, const char *path)
{
    bool protocol_ok = secure || !cookie->b_secure;
    bool domain_ok = cookie_domain_matches(cookie, host);
    bool path_ok = cookie_path_matches(cookie, path);
    return protocol_ok && domain_ok && path_ok;
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

static void cookie_destroy(http_cookie_t *cookie)
{
    assert(cookie != NULL);
    free(cookie->psz_name);
    free(cookie->psz_value);
    free(cookie->psz_domain);
    free(cookie->psz_path);
    free(cookie);
}

VLC_MALLOC VLC_USED
static http_cookie_t *cookie_parse(const char *value,
                                   const char *host, const char *path)
{
    http_cookie_t *cookie = malloc(sizeof (*cookie));
    if (unlikely(cookie == NULL))
        return NULL;

    cookie->psz_domain = NULL;
    cookie->psz_path = NULL;

    /* Get the NAME=VALUE part of the Cookie */
    size_t value_length = strcspn(value, ";");
    const char *p = memchr(value, '=', value_length);

    if (p != NULL)
    {
        cookie->psz_name = strndup(value, p - value);
        p++;
        cookie->psz_value = strndup(p, value_length - (p - value));
        if (unlikely(cookie->psz_value == NULL))
            goto error;
    }
    else
    {
        cookie->psz_name = strndup(value, value_length);
        cookie->psz_value = NULL;
    }

    if (unlikely(cookie->psz_name == NULL))
        goto error;

    /* Cookie name is a token; it cannot be empty. */
    if (cookie->psz_name[0] == '\0')
        goto error;

    /* Get domain */
    cookie->psz_domain = cookie_get_domain(value);
    if (cookie->psz_domain == NULL)
    {
        cookie->psz_domain = strdup(host);
        if (unlikely(cookie->psz_domain == NULL))
            goto error;

        cookie->b_host_only = true;
    }
    else
        cookie->b_host_only = false;

    /* Get path */
    cookie->psz_path = cookie_get_path(value);
    if (cookie->psz_path == NULL)
    {
        cookie->psz_path = cookie_default_path(path);
        if (unlikely(cookie->psz_path == NULL))
            goto error;
    }

    /* Get secure flag */
    cookie->b_secure = cookie_has_attribute(value, "secure");

    return cookie;

error:
    cookie_destroy(cookie);
    return NULL;
}

struct vlc_http_cookie_jar_t
{
    vlc_array_t cookies;
    vlc_mutex_t lock;
};

vlc_http_cookie_jar_t * vlc_http_cookies_new(void)
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

    for( size_t i = 0; i < vlc_array_count( &p_jar->cookies ); i++ )
        cookie_destroy( vlc_array_item_at_index( &p_jar->cookies, i ) );

    vlc_array_clear( &p_jar->cookies );

    free( p_jar );
}

bool vlc_http_cookies_store(vlc_http_cookie_jar_t *p_jar, const char *cookies,
                            const char *host, const char *path)
{
    assert(host != NULL);
    assert(path != NULL);

    http_cookie_t *cookie = cookie_parse(cookies, host, path);
    if (cookie == NULL)
        return false;

    /* Check if a cookie from host should be added to the cookie jar */
    // FIXME: should check if domain is one of "public suffixes" at
    // http://publicsuffix.org/. The purpose of this check is to
    // prevent a host from setting a "too wide" cookie, for example
    // "example.com" should not be able to set a cookie for "com".
    // The current implementation prevents all top-level domains.
    if (strchr(cookie->psz_domain, '.') == NULL
     || !cookie_domain_matches(cookie, host))
    {
        cookie_destroy(cookie);
        return false;
    }

    vlc_mutex_lock( &p_jar->lock );

    for( size_t i = 0; i < vlc_array_count( &p_jar->cookies ); i++ )
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

    bool b_ret = (vlc_array_append( &p_jar->cookies, cookie ) == 0);
    if( !b_ret )
        cookie_destroy( cookie );

    vlc_mutex_unlock( &p_jar->lock );

    return b_ret;
}

char *vlc_http_cookies_fetch(vlc_http_cookie_jar_t *p_jar, bool secure,
                             const char *host, const char *path)
{
    char *psz_cookiebuf = NULL;

    vlc_mutex_lock( &p_jar->lock );

    for( size_t i = 0; i < vlc_array_count( &p_jar->cookies ); i++ )
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
