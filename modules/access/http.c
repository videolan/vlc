/*****************************************************************************
 * http.c: HTTP input module
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>


#include <vlc_access.h>

#include <vlc_dialog.h>
#include <vlc_meta.h>
#include <vlc_network.h>
#include <vlc_url.h>
#include <vlc_tls.h>
#include <vlc_strings.h>
#include <vlc_charset.h>
#include <vlc_input.h>
#include <vlc_md5.h>
#include <vlc_http.h>

#ifdef HAVE_ZLIB_H
#   include <zlib.h>
#endif

#include <assert.h>
#include <limits.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define PROXY_TEXT N_("HTTP proxy")
#define PROXY_LONGTEXT N_( \
    "HTTP proxy to be used It must be of the form " \
    "http://[user@]myproxy.mydomain:myport/ ; " \
    "if empty, the http_proxy environment variable will be tried." )

#define PROXY_PASS_TEXT N_("HTTP proxy password")
#define PROXY_PASS_LONGTEXT N_( \
    "If your HTTP proxy requires a password, set it here." )

#define RECONNECT_TEXT N_("Auto re-connect")
#define RECONNECT_LONGTEXT N_( \
    "Automatically try to reconnect to the stream in case of a sudden " \
    "disconnect." )

#define CONTINUOUS_TEXT N_("Continuous stream")
#define CONTINUOUS_LONGTEXT N_("Read a file that is " \
    "being constantly updated (for example, a JPG file on a server). " \
    "You should not globally enable this option as it will break all other " \
    "types of HTTP streams." )

#define FORWARD_COOKIES_TEXT N_("Forward Cookies")
#define FORWARD_COOKIES_LONGTEXT N_("Forward Cookies across http redirections.")

#define REFERER_TEXT N_("HTTP referer value")
#define REFERER_LONGTEXT N_("Customize the HTTP referer, simulating a previous document")

#define UA_TEXT N_("User Agent")
#define UA_LONGTEXT N_("The name and version of the program will be " \
    "provided to the HTTP server. They must be separated by a forward " \
    "slash, e.g. FooBar/1.2.3. This option can only be specified per input " \
    "item, not globally.")

vlc_module_begin ()
    set_description( N_("HTTP input") )
    set_capability( "access", 0 )
    set_shortname( N_( "HTTP(S)" ) )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_string( "http-proxy", NULL, PROXY_TEXT, PROXY_LONGTEXT,
                false )
    add_password( "http-proxy-pwd", NULL,
                  PROXY_PASS_TEXT, PROXY_PASS_LONGTEXT, false )
    add_obsolete_bool( "http-use-IE-proxy" )
    add_string( "http-referrer", NULL, REFERER_TEXT, REFERER_LONGTEXT, false )
        change_safe()
    add_string( "http-user-agent", NULL, UA_TEXT, UA_LONGTEXT, false )
        change_safe()
        change_private()
    add_bool( "http-reconnect", false, RECONNECT_TEXT,
              RECONNECT_LONGTEXT, true )
    add_bool( "http-continuous", false, CONTINUOUS_TEXT,
              CONTINUOUS_LONGTEXT, true )
        change_safe()
    add_bool( "http-forward-cookies", true, FORWARD_COOKIES_TEXT,
              FORWARD_COOKIES_LONGTEXT, true )
    /* 'itpc' = iTunes Podcast */
    add_shortcut( "http", "https", "unsv", "itpc", "icyx" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

struct access_sys_t
{
    int fd;
    bool b_error;
    vlc_tls_creds_t *p_creds;
    vlc_tls_t *p_tls;
    v_socket_t *p_vs;

    /* From uri */
    vlc_url_t url;
    char    *psz_user_agent;
    char    *psz_referrer;
    http_auth_t auth;

    /* Proxy */
    bool b_proxy;
    vlc_url_t  proxy;
    http_auth_t proxy_auth;
    char       *psz_proxy_passbuf;

    /* */
    int        i_code;
    const char *psz_protocol;
    int        i_version;

    char       *psz_mime;
    char       *psz_pragma;
    char       *psz_location;
    bool b_mms;
    bool b_icecast;
#ifdef HAVE_ZLIB_H
    bool b_compressed;
    struct
    {
        z_stream   stream;
        uint8_t   *p_buffer;
    } inflate;
#endif

    bool b_chunked;
    int64_t    i_chunk;

    int        i_icy_meta;
    uint64_t   i_icy_offset;
    char       *psz_icy_name;
    char       *psz_icy_genre;
    char       *psz_icy_title;

    uint64_t i_remaining;

    bool b_seekable;
    bool b_reconnect;
    bool b_continuous;
    bool b_pace_control;
    bool b_persist;
    bool b_has_size;

    vlc_array_t * cookies;
};

/* */
static int OpenWithCookies( vlc_object_t *p_this, const char *psz_access,
                            unsigned i_redirect, vlc_array_t *cookies );

/* */
static ssize_t Read( access_t *, uint8_t *, size_t );
static ssize_t ReadCompressed( access_t *, uint8_t *, size_t );
static int Seek( access_t *, uint64_t );
static int Control( access_t *, int, va_list );

/* */
static int Connect( access_t *, uint64_t );
static int Request( access_t *p_access, uint64_t i_tell );
static void Disconnect( access_t * );

/* Small Cookie utilities. Cookies support is partial. */
static char * cookie_get_content( const char * cookie );
static char * cookie_get_domain( const char * cookie );
static char * cookie_get_name( const char * cookie );
static void cookie_append( vlc_array_t * cookies, char * cookie );


static void AuthReply( access_t *p_acces, const char *psz_prefix,
                       vlc_url_t *p_url, http_auth_t *p_auth );
static int AuthCheckReply( access_t *p_access, const char *psz_header,
                           vlc_url_t *p_url, http_auth_t *p_auth );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t *p_access = (access_t*)p_this;
    return OpenWithCookies( p_this, p_access->psz_access, 5, NULL );
}

/**
 * Open the given url using the given cookies
 * @param p_this: the vlc object
 * @psz_access: the acces to use (http, https, ...) (this value must be used
 *              instead of p_access->psz_access)
 * @i_redirect: number of redirections remaining
 * @cookies: the available cookies
 * @return vlc error codes
 */
static int OpenWithCookies( vlc_object_t *p_this, const char *psz_access,
                            unsigned i_redirect, vlc_array_t *cookies )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    char         *psz, *p;

    /* Only forward an store cookies if the corresponding option is activated */
    bool   b_forward_cookies = var_InheritBool( p_access, "http-forward-cookies" );
    vlc_array_t * saved_cookies = b_forward_cookies ? (cookies ? cookies : vlc_array_new()) : NULL;

    /* Set up p_access */
    STANDARD_READ_ACCESS_INIT;
#ifdef HAVE_ZLIB_H
    p_access->pf_read = ReadCompressed;
#endif
    p_sys->fd = -1;
    p_sys->b_proxy = false;
    p_sys->psz_proxy_passbuf = NULL;
    p_sys->i_version = 1;
    p_sys->b_seekable = true;
    p_sys->psz_mime = NULL;
    p_sys->psz_pragma = NULL;
    p_sys->b_mms = false;
    p_sys->b_icecast = false;
    p_sys->psz_location = NULL;
    p_sys->psz_user_agent = NULL;
    p_sys->psz_referrer = NULL;
    p_sys->b_pace_control = true;
#ifdef HAVE_ZLIB_H
    p_sys->b_compressed = false;
    /* 15 is the max windowBits, +32 to enable optional gzip decoding */
    if( inflateInit2( &p_sys->inflate.stream, 32+15 ) != Z_OK )
        msg_Warn( p_access, "Error during zlib initialisation: %s",
                  p_sys->inflate.stream.msg );
    if( zlibCompileFlags() & (1<<17) )
        msg_Warn( p_access, "Your zlib was compiled without gzip support." );
    p_sys->inflate.p_buffer = NULL;
#endif
    p_sys->p_tls = NULL;
    p_sys->p_vs = NULL;
    p_sys->i_icy_meta = 0;
    p_sys->i_icy_offset = 0;
    p_sys->psz_icy_name = NULL;
    p_sys->psz_icy_genre = NULL;
    p_sys->psz_icy_title = NULL;
    p_sys->i_remaining = 0;
    p_sys->b_persist = false;
    p_sys->b_has_size = false;
    p_access->info.i_size = 0;
    p_access->info.i_pos  = 0;
    p_access->info.b_eof  = false;

    p_sys->cookies = saved_cookies;

    http_auth_Init( &p_sys->auth );
    http_auth_Init( &p_sys->proxy_auth );

    /* Parse URI - remove spaces */
    p = psz = strdup( p_access->psz_location );
    while( (p = strchr( p, ' ' )) != NULL )
        *p = '+';
    vlc_UrlParse( &p_sys->url, psz, 0 );
    free( psz );

    if( p_sys->url.psz_host == NULL || *p_sys->url.psz_host == '\0' )
    {
        msg_Warn( p_access, "invalid host" );
        goto error;
    }
    if( !strncmp( psz_access, "https", 5 ) )
    {
        /* HTTP over SSL */
        p_sys->p_creds = vlc_tls_ClientCreate( p_this );
        if( p_sys->p_creds == NULL )
            goto error;
        if( p_sys->url.i_port <= 0 )
            p_sys->url.i_port = 443;
    }
    else
    {
        if( p_sys->url.i_port <= 0 )
            p_sys->url.i_port = 80;
    }

    /* Determine the HTTP user agent */
    /* See RFC2616 §2.2 token and comment definition, and §3.8 and
     * §14.43 user-agent header */
    p_sys->psz_user_agent = var_InheritString( p_access, "http-user-agent" );
    if (p_sys->psz_user_agent)
    {
        unsigned comment_level = 0;
        for( char *p = p_sys->psz_user_agent; *p; p++ )
        {
            uint8_t c = *p;
            if (comment_level == 0)
            {
                if( c < 32 || strchr( ")<>@,;:\\\"[]?={}", c ) )
                    *p = '_'; /* remove potentially harmful characters */
            }
            else
            {
                if (c == ')')
                    comment_level--;
                else if( c < 32 && strchr( "\t\r\n", c ) == NULL)
                    *p = '_'; /* remove potentially harmful characters */
            }
            if (c == '(')
            {
                if (comment_level == UINT_MAX)
                    break;
                comment_level++;
            }
        }
        /* truncate evil unclosed comments */
        if (comment_level > 0)
        {
            char *p = strchr(p_sys->psz_user_agent, '(');
            *p = '\0';
        }
    }

    /* HTTP referrer */
    p_sys->psz_referrer = var_InheritString( p_access, "http-referrer" );

    /* Check proxy */
    psz = var_InheritString( p_access, "http-proxy" );
    if( psz == NULL )
    {
        char *url;

        if (likely(asprintf(&url, "%s://%s", psz_access,
                            p_access->psz_location) != -1))
        {
            msg_Dbg(p_access, "querying proxy for %s", url);
            psz = vlc_getProxyUrl(url);
            free(url);
        }

        if (psz != NULL)
            msg_Dbg(p_access, "proxy: %s", psz);
        else
            msg_Dbg(p_access, "no proxy");
    }
    if( psz != NULL )
    {
        p_sys->b_proxy = true;
        vlc_UrlParse( &p_sys->proxy, psz, 0 );
        free( psz );

        psz = var_InheritString( p_access, "http-proxy-pwd" );
        if( psz )
            p_sys->proxy.psz_password = p_sys->psz_proxy_passbuf = psz;

        if( p_sys->proxy.psz_host == NULL || *p_sys->proxy.psz_host == '\0' )
        {
            msg_Warn( p_access, "invalid proxy host" );
            goto error;
        }
        if( p_sys->proxy.i_port <= 0 )
        {
            p_sys->proxy.i_port = 80;
        }
    }

    msg_Dbg( p_access, "http: server='%s' port=%d file='%s'",
             p_sys->url.psz_host, p_sys->url.i_port,
             p_sys->url.psz_path != NULL ? p_sys->url.psz_path : "" );
    if( p_sys->b_proxy )
    {
        msg_Dbg( p_access, "      proxy %s:%d", p_sys->proxy.psz_host,
                 p_sys->proxy.i_port );
    }
    if( p_sys->url.psz_username && *p_sys->url.psz_username )
    {
        msg_Dbg( p_access, "      user='%s'", p_sys->url.psz_username );
    }

    p_sys->b_reconnect = var_InheritBool( p_access, "http-reconnect" );
    p_sys->b_continuous = var_InheritBool( p_access, "http-continuous" );

connect:
    /* Connect */
    switch( Connect( p_access, 0 ) )
    {
        case -1:
            goto error;

        case -2:
            /* Retry with http 1.0 */
            msg_Dbg( p_access, "switching to HTTP version 1.0" );
            p_sys->i_version = 0;
            p_sys->b_seekable = false;

            if( !vlc_object_alive (p_access) || Connect( p_access, 0 ) )
                goto error;

        case 0:
            break;

        default:
            assert(0);
    }

    if( p_sys->i_code == 401 )
    {
        if( p_sys->auth.psz_realm == NULL )
        {
            msg_Err( p_access, "authentication failed without realm" );
            goto error;
        }
        char *psz_login, *psz_password;
        /* FIXME ? */
        if( p_sys->url.psz_username && p_sys->url.psz_password &&
            p_sys->auth.psz_nonce && p_sys->auth.i_nonce == 0 )
        {
            Disconnect( p_access );
            goto connect;
        }
        msg_Dbg( p_access, "authentication failed for realm %s",
                 p_sys->auth.psz_realm );
        dialog_Login( p_access, &psz_login, &psz_password,
                      _("HTTP authentication"),
             _("Please enter a valid login name and a password for realm %s."),
                      p_sys->auth.psz_realm );
        if( psz_login != NULL && psz_password != NULL )
        {
            msg_Dbg( p_access, "retrying with user=%s", psz_login );
            p_sys->url.psz_username = psz_login;
            p_sys->url.psz_password = psz_password;
            Disconnect( p_access );
            goto connect;
        }
        else
        {
            free( psz_login );
            free( psz_password );
            goto error;
        }
    }

    if( ( p_sys->i_code == 301 || p_sys->i_code == 302 ||
          p_sys->i_code == 303 || p_sys->i_code == 307 ) &&
        p_sys->psz_location && *p_sys->psz_location )
    {
        msg_Dbg( p_access, "redirection to %s", p_sys->psz_location );

        /* Check the number of redirection already done */
        if( i_redirect == 0 )
        {
            msg_Err( p_access, "Too many redirection: break potential infinite"
                     "loop" );
            goto error;
        }

        const char *psz_protocol;
        if( !strncmp( p_sys->psz_location, "http://", 7 ) )
            psz_protocol = "http";
        else if( !strncmp( p_sys->psz_location, "https://", 8 ) )
            psz_protocol = "https";
        else
        {   /* Do not accept redirection outside of HTTP */
            msg_Err( p_access, "unsupported redirection ignored" );
            goto error;
        }
        free( p_access->psz_location );
        p_access->psz_location = strdup( p_sys->psz_location
                                       + strlen( psz_protocol ) + 3 );
        /* Clean up current Open() run */
        vlc_UrlClean( &p_sys->url );
        http_auth_Reset( &p_sys->auth );
        vlc_UrlClean( &p_sys->proxy );
        free( p_sys->psz_proxy_passbuf );
        http_auth_Reset( &p_sys->proxy_auth );
        free( p_sys->psz_mime );
        free( p_sys->psz_pragma );
        free( p_sys->psz_location );
        free( p_sys->psz_user_agent );
        free( p_sys->psz_referrer );

        Disconnect( p_access );
        vlc_tls_Delete( p_sys->p_creds );
        cookies = p_sys->cookies;
#ifdef HAVE_ZLIB_H
        inflateEnd( &p_sys->inflate.stream );
#endif
        free( p_sys );

        /* Do new Open() run with new data */
        return OpenWithCookies( p_this, psz_protocol, i_redirect - 1,
                                cookies );
    }

    if( p_sys->b_mms )
    {
        msg_Dbg( p_access, "this is actually a live mms server, BAIL" );
        goto error;
    }

    if( !strcmp( p_sys->psz_protocol, "ICY" ) || p_sys->b_icecast )
    {
        if( p_sys->psz_mime && strcasecmp( p_sys->psz_mime, "application/ogg" ) )
        {
            if( !strcasecmp( p_sys->psz_mime, "video/nsv" ) ||
                !strcasecmp( p_sys->psz_mime, "video/nsa" ) )
            {
                free( p_access->psz_demux );
                p_access->psz_demux = strdup( "nsv" );
            }
            else if( !strcasecmp( p_sys->psz_mime, "audio/aac" ) ||
                     !strcasecmp( p_sys->psz_mime, "audio/aacp" ) )
            {
                free( p_access->psz_demux );
                p_access->psz_demux = strdup( "m4a" );
            }
            else if( !strcasecmp( p_sys->psz_mime, "audio/mpeg" ) )
            {
                free( p_access->psz_demux );
                p_access->psz_demux = strdup( "mp3" );
            }

            msg_Info( p_access, "Raw-audio server found, %s demuxer selected",
                      p_access->psz_demux );

#if 0       /* Doesn't work really well because of the pre-buffering in
             * shoutcast servers (the buffer content will be sent as fast as
             * possible). */
            p_sys->b_pace_control = false;
#endif
        }
        else if( !p_sys->psz_mime )
        {
            free( p_access->psz_demux );
            /* Shoutcast */
            p_access->psz_demux = strdup( "mp3" );
        }
        /* else probably Ogg Vorbis */
    }
    else if( !strcasecmp( psz_access, "unsv" ) &&
             p_sys->psz_mime &&
             !strcasecmp( p_sys->psz_mime, "misc/ultravox" ) )
    {
        free( p_access->psz_demux );
        /* Grrrr! detect ultravox server and force NSV demuxer */
        p_access->psz_demux = strdup( "nsv" );
    }
    else if( !strcmp( psz_access, "itpc" ) )
    {
        free( p_access->psz_demux );
        p_access->psz_demux = strdup( "podcast" );
    }
    else if( p_sys->psz_mime &&
             !strncasecmp( p_sys->psz_mime, "application/xspf+xml", 20 ) &&
             ( memchr( " ;\t", p_sys->psz_mime[20], 4 ) != NULL ) )
    {
        free( p_access->psz_demux );
        p_access->psz_demux = strdup( "xspf-open" );
    }

    if( p_sys->b_reconnect ) msg_Dbg( p_access, "auto re-connect enabled" );

    return VLC_SUCCESS;

error:
    vlc_UrlClean( &p_sys->url );
    vlc_UrlClean( &p_sys->proxy );
    free( p_sys->psz_proxy_passbuf );
    free( p_sys->psz_mime );
    free( p_sys->psz_pragma );
    free( p_sys->psz_location );
    free( p_sys->psz_user_agent );
    free( p_sys->psz_referrer );

    Disconnect( p_access );
    vlc_tls_Delete( p_sys->p_creds );

    if( p_sys->cookies )
    {
        int i;
        for( i = 0; i < vlc_array_count( p_sys->cookies ); i++ )
            free(vlc_array_item_at_index( p_sys->cookies, i ));
        vlc_array_destroy( p_sys->cookies );
    }

#ifdef HAVE_ZLIB_H
    inflateEnd( &p_sys->inflate.stream );
#endif
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    vlc_UrlClean( &p_sys->url );
    http_auth_Reset( &p_sys->auth );
    vlc_UrlClean( &p_sys->proxy );
    http_auth_Reset( &p_sys->proxy_auth );

    free( p_sys->psz_mime );
    free( p_sys->psz_pragma );
    free( p_sys->psz_location );

    free( p_sys->psz_icy_name );
    free( p_sys->psz_icy_genre );
    free( p_sys->psz_icy_title );

    free( p_sys->psz_user_agent );
    free( p_sys->psz_referrer );

    Disconnect( p_access );
    vlc_tls_Delete( p_sys->p_creds );

    if( p_sys->cookies )
    {
        int i;
        for( i = 0; i < vlc_array_count( p_sys->cookies ); i++ )
            free(vlc_array_item_at_index( p_sys->cookies, i ));
        vlc_array_destroy( p_sys->cookies );
    }

#ifdef HAVE_ZLIB_H
    inflateEnd( &p_sys->inflate.stream );
    free( p_sys->inflate.p_buffer );
#endif

    free( p_sys );
}

/* Read data from the socket taking care of chunked transfer if needed */
static int ReadData( access_t *p_access, int *pi_read,
                     uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    if( p_sys->b_chunked )
    {
        if( p_sys->i_chunk < 0 )
            return VLC_EGENERIC;

        if( p_sys->i_chunk <= 0 )
        {
            char *psz = net_Gets( p_access, p_sys->fd, p_sys->p_vs );
            /* read the chunk header */
            if( psz == NULL )
            {
                /* fatal error - end of file */
                msg_Dbg( p_access, "failed reading chunk-header line" );
                return VLC_EGENERIC;
            }
            p_sys->i_chunk = strtoll( psz, NULL, 16 );
            free( psz );

            if( p_sys->i_chunk <= 0 )   /* eof */
            {
                p_sys->i_chunk = -1;
                return VLC_EGENERIC;
            }
        }

        if( i_len > p_sys->i_chunk )
            i_len = p_sys->i_chunk;
    }
    *pi_read = net_Read( p_access, p_sys->fd, p_sys->p_vs, p_buffer, i_len, false );
    if( *pi_read <= 0 )
        return VLC_SUCCESS;

    if( p_sys->b_chunked )
    {
        p_sys->i_chunk -= *pi_read;
        if( p_sys->i_chunk <= 0 )
        {
            /* read the empty line */
            char *psz = net_Gets( p_access, p_sys->fd, p_sys->p_vs );
            free( psz );
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read: Read up to i_len bytes from the http connection and place in
 * p_buffer. Return the actual number of bytes read
 *****************************************************************************/
static int ReadICYMeta( access_t *p_access );
static ssize_t Read( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    if( p_sys->fd == -1 )
        goto fatal;

    if( p_sys->b_has_size )
    {
        /* Remaining bytes in the file */
        uint64_t remainder = p_access->info.i_size - p_access->info.i_pos;
        if( remainder < i_len )
            i_len = remainder;

        /* Remaining bytes in the response */
        if( p_sys->i_remaining < i_len )
            i_len = p_sys->i_remaining;
    }
    if( i_len == 0 )
        goto fatal;

    if( p_sys->i_icy_meta > 0 && p_access->info.i_pos - p_sys->i_icy_offset > 0 )
    {
        int64_t i_next = p_sys->i_icy_meta -
                                    (p_access->info.i_pos - p_sys->i_icy_offset ) % p_sys->i_icy_meta;

        if( i_next == p_sys->i_icy_meta )
        {
            if( ReadICYMeta( p_access ) )
                goto fatal;
        }
        if( i_len > i_next )
            i_len = i_next;
    }

    if( ReadData( p_access, &i_read, p_buffer, i_len ) )
        goto fatal;

    if( i_read <= 0 )
    {
        /*
         * I very much doubt that this will work.
         * If i_read == 0, the connection *IS* dead, so the only
         * sensible thing to do is Disconnect() and then retry.
         * Otherwise, I got recv() completely wrong. -- Courmisch
         */
        if( p_sys->b_continuous )
        {
            Request( p_access, 0 );
            p_sys->b_continuous = false;
            i_read = Read( p_access, p_buffer, i_len );
            p_sys->b_continuous = true;
        }
        Disconnect( p_access );
        if( p_sys->b_reconnect && vlc_object_alive( p_access ) )
        {
            msg_Dbg( p_access, "got disconnected, trying to reconnect" );
            if( Connect( p_access, p_access->info.i_pos ) )
            {
                msg_Dbg( p_access, "reconnection failed" );
            }
            else
            {
                p_sys->b_reconnect = false;
                i_read = Read( p_access, p_buffer, i_len );
                p_sys->b_reconnect = true;

                return i_read;
            }
        }

        if( i_read <= 0 )
        {
            if( i_read < 0 )
                p_sys->b_error = true;
            goto fatal;
        }
    }

    assert( i_read >= 0 );
    p_access->info.i_pos += i_read;
    if( p_sys->b_has_size )
    {
        assert( p_access->info.i_pos <= p_access->info.i_size );
        assert( (unsigned)i_read <= p_sys->i_remaining );
        p_sys->i_remaining -= i_read;
    }

    return i_read;

fatal:
    p_access->info.b_eof = true;
    return 0;
}

static int ReadICYMeta( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    uint8_t buffer;
    char *p, *psz_meta;
    int i_read;

    /* Read meta data length */
    if( ReadData( p_access, &i_read, &buffer, 1 ) )
        return VLC_EGENERIC;
    if( i_read != 1 )
        return VLC_EGENERIC;
    const int i_size = buffer << 4;
    /* msg_Dbg( p_access, "ICY meta size=%u", i_size); */

    psz_meta = malloc( i_size + 1 );
    for( i_read = 0; i_read < i_size; )
    {
        int i_tmp;
        if( ReadData( p_access, &i_tmp, (uint8_t *)&psz_meta[i_read], i_size - i_read ) || i_tmp <= 0 )
        {
            free( psz_meta );
            return VLC_EGENERIC;
        }
        i_read += i_tmp;
    }
    psz_meta[i_read] = '\0'; /* Just in case */

    /* msg_Dbg( p_access, "icy-meta=%s", psz_meta ); */

    /* Now parse the meta */
    /* Look for StreamTitle= */
    p = strcasestr( (char *)psz_meta, "StreamTitle=" );
    if( p )
    {
        p += strlen( "StreamTitle=" );
        if( *p == '\'' || *p == '"' )
        {
            char closing[] = { p[0], ';', '\0' };
            char *psz = strstr( &p[1], closing );
            if( !psz )
                psz = strchr( &p[1], ';' );

            if( psz ) *psz = '\0';
        }
        else
        {
            char *psz = strchr( &p[1], ';' );
            if( psz ) *psz = '\0';
        }

        if( !p_sys->psz_icy_title ||
            strcmp( p_sys->psz_icy_title, &p[1] ) )
        {
            free( p_sys->psz_icy_title );
            char *psz_tmp = strdup( &p[1] );
            p_sys->psz_icy_title = EnsureUTF8( psz_tmp );
            if( !p_sys->psz_icy_title )
                free( psz_tmp );
            p_access->info.i_update |= INPUT_UPDATE_META;

            msg_Dbg( p_access, "New Title=%s", p_sys->psz_icy_title );
        }
    }
    free( psz_meta );

    return VLC_SUCCESS;
}

#ifdef HAVE_ZLIB_H
static ssize_t ReadCompressed( access_t *p_access, uint8_t *p_buffer,
                               size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->b_compressed )
    {
        int i_ret;

        if( !p_sys->inflate.p_buffer )
            p_sys->inflate.p_buffer = malloc( 256 * 1024 );

        if( p_sys->inflate.stream.avail_in == 0 )
        {
            ssize_t i_read = Read( p_access, p_sys->inflate.p_buffer, 256 * 1024 );
            if( i_read <= 0 ) return i_read;
            p_sys->inflate.stream.next_in = p_sys->inflate.p_buffer;
            p_sys->inflate.stream.avail_in = i_read;
        }

        p_sys->inflate.stream.avail_out = i_len;
        p_sys->inflate.stream.next_out = p_buffer;

        i_ret = inflate( &p_sys->inflate.stream, Z_SYNC_FLUSH );
        if ( i_ret != Z_OK && i_ret != Z_STREAM_END )
            msg_Warn( p_access, "inflate return value: %d, %s", i_ret, p_sys->inflate.stream.msg );

        return i_len - p_sys->inflate.stream.avail_out;
    }
    else
    {
        return Read( p_access, p_buffer, i_len );
    }
}
#endif

/*****************************************************************************
 * Seek: close and re-open a connection at the right place
 *****************************************************************************/
static int Seek( access_t *p_access, uint64_t i_pos )
{
    msg_Dbg( p_access, "trying to seek to %"PRId64, i_pos );

    Disconnect( p_access );

    if( p_access->info.i_size
     && i_pos >= p_access->info.i_size ) {
        msg_Err( p_access, "seek too far" );
        int retval = Seek( p_access, p_access->info.i_size - 1 );
        if( retval == VLC_SUCCESS ) {
            uint8_t p_buffer[2];
            Read( p_access, p_buffer, 1);
            p_access->info.b_eof  = false;
        }
        return retval;
    }
    if( Connect( p_access, i_pos ) )
    {
        msg_Err( p_access, "seek failed" );
        p_access->info.b_eof = true;
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    bool       *pb_bool;
    int64_t    *pi_64;
    vlc_meta_t *p_meta;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = p_sys->b_seekable;
            break;
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );

#if 0       /* Disable for now until we have a clock synchro algo
             * which works with something else than MPEG over UDP */
            *pb_bool = p_sys->b_pace_control;
#endif
            *pb_bool = true;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = INT64_C(1000)
                * var_InheritInteger( p_access, "network-caching" );
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            break;

        case ACCESS_GET_META:
            p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t* );

            if( p_sys->psz_icy_name )
                vlc_meta_Set( p_meta, vlc_meta_Title, p_sys->psz_icy_name );
            if( p_sys->psz_icy_genre )
                vlc_meta_Set( p_meta, vlc_meta_Genre, p_sys->psz_icy_genre );
            if( p_sys->psz_icy_title )
                vlc_meta_Set( p_meta, vlc_meta_NowPlaying, p_sys->psz_icy_title );
            break;

        case ACCESS_GET_CONTENT_TYPE:
            *va_arg( args, char ** ) =
                p_sys->psz_mime ? strdup( p_sys->psz_mime ) : NULL;
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Connect:
 *****************************************************************************/
static int Connect( access_t *p_access, uint64_t i_tell )
{
    access_sys_t   *p_sys = p_access->p_sys;
    vlc_url_t      srv = p_sys->b_proxy ? p_sys->proxy : p_sys->url;

    /* Clean info */
    free( p_sys->psz_location );
    free( p_sys->psz_mime );
    free( p_sys->psz_pragma );

    free( p_sys->psz_icy_genre );
    free( p_sys->psz_icy_name );
    free( p_sys->psz_icy_title );


    p_sys->psz_location = NULL;
    p_sys->psz_mime = NULL;
    p_sys->psz_pragma = NULL;
    p_sys->b_mms = false;
    p_sys->b_chunked = false;
    p_sys->i_chunk = 0;
    p_sys->i_icy_meta = 0;
    p_sys->i_icy_offset = i_tell;
    p_sys->psz_icy_name = NULL;
    p_sys->psz_icy_genre = NULL;
    p_sys->psz_icy_title = NULL;
    p_sys->i_remaining = 0;
    p_sys->b_persist = false;
    p_sys->b_has_size = false;
    p_access->info.i_size = 0;
    p_access->info.i_pos  = i_tell;
    p_access->info.b_eof  = false;

    /* Open connection */
    assert( p_sys->fd == -1 ); /* No open sockets (leaking fds is BAD) */
    p_sys->fd = net_ConnectTCP( p_access, srv.psz_host, srv.i_port );
    if( p_sys->fd == -1 )
    {
        msg_Err( p_access, "cannot connect to %s:%d", srv.psz_host, srv.i_port );
        return -1;
    }
    setsockopt (p_sys->fd, SOL_SOCKET, SO_KEEPALIVE, &(int){ 1 }, sizeof (int));

    /* Initialize TLS/SSL session */
    if( p_sys->p_creds != NULL )
    {
        /* CONNECT to establish TLS tunnel through HTTP proxy */
        if( p_sys->b_proxy )
        {
            char *psz;
            unsigned i_status = 0;

            if( p_sys->i_version == 0 )
            {
                /* CONNECT is not in HTTP/1.0 */
                Disconnect( p_access );
                return -1;
            }

            net_Printf( p_access, p_sys->fd, NULL,
                        "CONNECT %s:%d HTTP/1.%d\r\nHost: %s:%d\r\n\r\n",
                        p_sys->url.psz_host, p_sys->url.i_port,
                        p_sys->i_version,
                        p_sys->url.psz_host, p_sys->url.i_port);

            psz = net_Gets( p_access, p_sys->fd, NULL );
            if( psz == NULL )
            {
                msg_Err( p_access, "cannot establish HTTP/TLS tunnel" );
                Disconnect( p_access );
                return -1;
            }

            sscanf( psz, "HTTP/%*u.%*u %3u", &i_status );
            free( psz );

            if( ( i_status / 100 ) != 2 )
            {
                msg_Err( p_access, "HTTP/TLS tunnel through proxy denied" );
                Disconnect( p_access );
                return -1;
            }

            do
            {
                psz = net_Gets( p_access, p_sys->fd, NULL );
                if( psz == NULL )
                {
                    msg_Err( p_access, "HTTP proxy connection failed" );
                    Disconnect( p_access );
                    return -1;
                }

                if( *psz == '\0' )
                    i_status = 0;

                free( psz );

                if( !vlc_object_alive (p_access) || p_sys->b_error )
                {
                    Disconnect( p_access );
                    return -1;
                }
            }
            while( i_status );
        }

        /* TLS/SSL handshake */
        p_sys->p_tls = vlc_tls_ClientSessionCreate( p_sys->p_creds, p_sys->fd,
                                                p_sys->url.psz_host, "https" );
        if( p_sys->p_tls == NULL )
        {
            msg_Err( p_access, "cannot establish HTTP/TLS session" );
            Disconnect( p_access );
            return -1;
        }
        p_sys->p_vs = &p_sys->p_tls->sock;
    }

    return Request( p_access, i_tell ) ? -2 : 0;
}


static int Request( access_t *p_access, uint64_t i_tell )
{
    access_sys_t   *p_sys = p_access->p_sys;
    char           *psz ;
    v_socket_t     *pvs = p_sys->p_vs;
    p_sys->b_persist = false;

    p_sys->i_remaining = 0;

    const char *psz_path = p_sys->url.psz_path;
    if( !psz_path || !*psz_path )
        psz_path = "/";
    if( p_sys->b_proxy && pvs == NULL )
        net_Printf( p_access, p_sys->fd, NULL,
                    "GET http://%s:%d%s HTTP/1.%d\r\n",
                    p_sys->url.psz_host, p_sys->url.i_port,
                    psz_path, p_sys->i_version );
    else
        net_Printf( p_access, p_sys->fd, pvs, "GET %s HTTP/1.%d\r\n",
                    psz_path, p_sys->i_version );
    if( p_sys->url.i_port != (pvs ? 443 : 80) )
        net_Printf( p_access, p_sys->fd, pvs, "Host: %s:%d\r\n",
                    p_sys->url.psz_host, p_sys->url.i_port );
    else
        net_Printf( p_access, p_sys->fd, pvs, "Host: %s\r\n",
                    p_sys->url.psz_host );
    /* User Agent */
    net_Printf( p_access, p_sys->fd, pvs, "User-Agent: %s\r\n",
                p_sys->psz_user_agent );
    /* Referrer */
    if (p_sys->psz_referrer)
    {
        net_Printf( p_access, p_sys->fd, pvs, "Referer: %s\r\n",
                    p_sys->psz_referrer);
    }
    /* Offset */
    if( p_sys->i_version == 1 && ! p_sys->b_continuous )
    {
        p_sys->b_persist = true;
        net_Printf( p_access, p_sys->fd, pvs,
                    "Range: bytes=%"PRIu64"-\r\n", i_tell );
        net_Printf( p_access, p_sys->fd, pvs, "Connection: close\r\n" );
    }

    /* Cookies */
    if( p_sys->cookies )
    {
        int i;
        for( i = 0; i < vlc_array_count( p_sys->cookies ); i++ )
        {
            const char * cookie = vlc_array_item_at_index( p_sys->cookies, i );
            char * psz_cookie_content = cookie_get_content( cookie );
            char * psz_cookie_domain = cookie_get_domain( cookie );

            assert( psz_cookie_content );

            /* FIXME: This is clearly not conforming to the rfc */
            bool is_in_right_domain = (!psz_cookie_domain || strstr( p_sys->url.psz_host, psz_cookie_domain ));

            if( is_in_right_domain )
            {
                msg_Dbg( p_access, "Sending Cookie %s", psz_cookie_content );
                if( net_Printf( p_access, p_sys->fd, pvs, "Cookie: %s\r\n", psz_cookie_content ) < 0 )
                    msg_Err( p_access, "failed to send Cookie" );
            }
            free( psz_cookie_content );
            free( psz_cookie_domain );
        }
    }

    /* Authentication */
    if( p_sys->url.psz_username || p_sys->url.psz_password )
        AuthReply( p_access, "", &p_sys->url, &p_sys->auth );

    /* Proxy Authentication */
    if( p_sys->proxy.psz_username || p_sys->proxy.psz_password )
        AuthReply( p_access, "Proxy-", &p_sys->proxy, &p_sys->proxy_auth );

    /* ICY meta data request */
    net_Printf( p_access, p_sys->fd, pvs, "Icy-MetaData: 1\r\n" );


    if( net_Printf( p_access, p_sys->fd, pvs, "\r\n" ) < 0 )
    {
        msg_Err( p_access, "failed to send request" );
        Disconnect( p_access );
        return VLC_EGENERIC;
    }

    /* Read Answer */
    if( ( psz = net_Gets( p_access, p_sys->fd, pvs ) ) == NULL )
    {
        msg_Err( p_access, "failed to read answer" );
        goto error;
    }
    if( !strncmp( psz, "HTTP/1.", 7 ) )
    {
        p_sys->psz_protocol = "HTTP";
        p_sys->i_code = atoi( &psz[9] );
    }
    else if( !strncmp( psz, "ICY", 3 ) )
    {
        p_sys->psz_protocol = "ICY";
        p_sys->i_code = atoi( &psz[4] );
        p_sys->b_reconnect = true;
    }
    else
    {
        msg_Err( p_access, "invalid HTTP reply '%s'", psz );
        free( psz );
        goto error;
    }
    msg_Dbg( p_access, "protocol '%s' answer code %d",
             p_sys->psz_protocol, p_sys->i_code );
    if( !strcmp( p_sys->psz_protocol, "ICY" ) )
    {
        p_sys->b_seekable = false;
    }
    if( p_sys->i_code != 206 && p_sys->i_code != 401 )
    {
        p_sys->b_seekable = false;
    }
    /* Authentication error - We'll have to display the dialog */
    if( p_sys->i_code == 401 )
    {

    }
    /* Other fatal error */
    else if( p_sys->i_code >= 400 )
    {
        msg_Err( p_access, "error: %s", psz );
        free( psz );
        goto error;
    }
    free( psz );

    for( ;; )
    {
        char *psz = net_Gets( p_access, p_sys->fd, pvs );
        char *p;
        char *p_trailing;

        if( psz == NULL )
        {
            msg_Err( p_access, "failed to read answer" );
            goto error;
        }

        if( !vlc_object_alive (p_access) || p_sys->b_error )
        {
            free( psz );
            goto error;
        }

        /* msg_Dbg( p_input, "Line=%s", psz ); */
        if( *psz == '\0' )
        {
            free( psz );
            break;
        }

        if( ( p = strchr( psz, ':' ) ) == NULL )
        {
            msg_Err( p_access, "malformed header line: %s", psz );
            free( psz );
            goto error;
        }
        *p++ = '\0';
        p += strspn( p, " \t" );

        /* trim trailing white space */
        p_trailing = p + strlen( p );
        if( p_trailing > p )
        {
            p_trailing--;
            while( ( *p_trailing == ' ' || *p_trailing == '\t' ) && p_trailing > p )
            {
                *p_trailing = '\0';
                p_trailing--;
            }
        }

        if( !strcasecmp( psz, "Content-Length" ) )
        {
            uint64_t i_size = i_tell + (p_sys->i_remaining = (uint64_t)atoll( p ));
            if(i_size > p_access->info.i_size) {
                p_sys->b_has_size = true;
                p_access->info.i_size = i_size;
            }
            msg_Dbg( p_access, "this frame size=%"PRIu64, p_sys->i_remaining );
        }
        else if( !strcasecmp( psz, "Content-Range" ) ) {
            uint64_t i_ntell = i_tell;
            uint64_t i_nend = (p_access->info.i_size > 0)?(p_access->info.i_size - 1):i_tell;
            uint64_t i_nsize = p_access->info.i_size;
            sscanf(p,"bytes %"SCNu64"-%"SCNu64"/%"SCNu64,&i_ntell,&i_nend,&i_nsize);
            if(i_nend > i_ntell ) {
                p_access->info.i_pos = i_ntell;
                p_sys->i_icy_offset  = i_ntell;
                p_sys->i_remaining = i_nend+1-i_ntell;
                uint64_t i_size = (i_nsize > i_nend) ? i_nsize : (i_nend + 1);
                if(i_size > p_access->info.i_size) {
                    p_sys->b_has_size = true;
                    p_access->info.i_size = i_size;
                }
                msg_Dbg( p_access, "stream size=%"PRIu64",pos=%"PRIu64",remaining=%"PRIu64,
                         i_nsize, i_ntell, p_sys->i_remaining);
            }
        }
        else if( !strcasecmp( psz, "Connection" ) ) {
            msg_Dbg( p_access, "Connection: %s",p );
            int i = -1;
            sscanf(p, "close%n",&i);
            if( i >= 0 ) {
                p_sys->b_persist = false;
            }
        }
        else if( !strcasecmp( psz, "Location" ) )
        {
            char * psz_new_loc;

            /* This does not follow RFC 2068, but yet if the url is not absolute,
             * handle it as everyone does. */
            if( p[0] == '/' )
            {
                const char *psz_http_ext = p_sys->p_tls ? "s" : "" ;

                if( p_sys->url.i_port == ( p_sys->p_tls ? 443 : 80 ) )
                {
                    if( asprintf(&psz_new_loc, "http%s://%s%s", psz_http_ext,
                                 p_sys->url.psz_host, p) < 0 )
                        goto error;
                }
                else
                {
                    if( asprintf(&psz_new_loc, "http%s://%s:%d%s", psz_http_ext,
                                 p_sys->url.psz_host, p_sys->url.i_port, p) < 0 )
                        goto error;
                }
            }
            else
            {
                psz_new_loc = strdup( p );
            }

            free( p_sys->psz_location );
            p_sys->psz_location = psz_new_loc;
        }
        else if( !strcasecmp( psz, "Content-Type" ) )
        {
            free( p_sys->psz_mime );
            p_sys->psz_mime = strdup( p );
            msg_Dbg( p_access, "Content-Type: %s", p_sys->psz_mime );
        }
        else if( !strcasecmp( psz, "Content-Encoding" ) )
        {
            msg_Dbg( p_access, "Content-Encoding: %s", p );
            if( !strcasecmp( p, "identity" ) )
                ;
#ifdef HAVE_ZLIB_H
            else if( !strcasecmp( p, "gzip" ) || !strcasecmp( p, "deflate" ) )
                p_sys->b_compressed = true;
#endif
            else
                msg_Warn( p_access, "Unknown content coding: %s", p );
        }
        else if( !strcasecmp( psz, "Pragma" ) )
        {
            if( !strcasecmp( psz, "Pragma: features" ) )
                p_sys->b_mms = true;
            free( p_sys->psz_pragma );
            p_sys->psz_pragma = strdup( p );
            msg_Dbg( p_access, "Pragma: %s", p_sys->psz_pragma );
        }
        else if( !strcasecmp( psz, "Server" ) )
        {
            msg_Dbg( p_access, "Server: %s", p );
            if( !strncasecmp( p, "Icecast", 7 ) ||
                !strncasecmp( p, "Nanocaster", 10 ) )
            {
                /* Remember if this is Icecast
                 * we need to force demux in this case without breaking
                 *  autodetection */

                /* Let live 365 streams (nanocaster) piggyback on the icecast
                 * routine. They look very similar */

                p_sys->b_reconnect = true;
                p_sys->b_pace_control = false;
                p_sys->b_icecast = true;
            }
        }
        else if( !strcasecmp( psz, "Transfer-Encoding" ) )
        {
            msg_Dbg( p_access, "Transfer-Encoding: %s", p );
            if( !strncasecmp( p, "chunked", 7 ) )
            {
                p_sys->b_chunked = true;
            }
        }
        else if( !strcasecmp( psz, "Icy-MetaInt" ) )
        {
            msg_Dbg( p_access, "Icy-MetaInt: %s", p );
            p_sys->i_icy_meta = atoi( p );
            if( p_sys->i_icy_meta < 0 )
                p_sys->i_icy_meta = 0;
            if( p_sys->i_icy_meta > 0 )
                p_sys->b_icecast = true;

            msg_Warn( p_access, "ICY metaint=%d", p_sys->i_icy_meta );
        }
        else if( !strcasecmp( psz, "Icy-Name" ) )
        {
            free( p_sys->psz_icy_name );
            char *psz_tmp = strdup( p );
            p_sys->psz_icy_name = EnsureUTF8( psz_tmp );
            if( !p_sys->psz_icy_name )
                free( psz_tmp );
            msg_Dbg( p_access, "Icy-Name: %s", p_sys->psz_icy_name );

            p_sys->b_icecast = true; /* be on the safeside. set it here as well. */
            p_sys->b_reconnect = true;
            p_sys->b_pace_control = false;
        }
        else if( !strcasecmp( psz, "Icy-Genre" ) )
        {
            free( p_sys->psz_icy_genre );
            char *psz_tmp = strdup( p );
            p_sys->psz_icy_genre = EnsureUTF8( psz_tmp );
            if( !p_sys->psz_icy_genre )
                free( psz_tmp );
            msg_Dbg( p_access, "Icy-Genre: %s", p_sys->psz_icy_genre );
        }
        else if( !strncasecmp( psz, "Icy-Notice", 10 ) )
        {
            msg_Dbg( p_access, "Icy-Notice: %s", p );
        }
        else if( !strncasecmp( psz, "icy-", 4 ) ||
                 !strncasecmp( psz, "ice-", 4 ) ||
                 !strncasecmp( psz, "x-audiocast", 11 ) )
        {
            msg_Dbg( p_access, "Meta-Info: %s: %s", psz, p );
        }
        else if( !strcasecmp( psz, "Set-Cookie" ) )
        {
            if( p_sys->cookies )
            {
                msg_Dbg( p_access, "Accepting Cookie: %s", p );
                cookie_append( p_sys->cookies, strdup(p) );
            }
            else
                msg_Dbg( p_access, "We have a Cookie we won't remember: %s", p );
        }
        else if( !strcasecmp( psz, "www-authenticate" ) )
        {
            msg_Dbg( p_access, "Authentication header: %s", p );
            http_auth_ParseWwwAuthenticateHeader( VLC_OBJECT(p_access),
                                                  &p_sys->auth, p );
        }
        else if( !strcasecmp( psz, "proxy-authenticate" ) )
        {
            msg_Dbg( p_access, "Proxy authentication header: %s", p );
            http_auth_ParseWwwAuthenticateHeader( VLC_OBJECT(p_access),
                                                  &p_sys->proxy_auth, p );
        }
        else if( !strcasecmp( psz, "authentication-info" ) )
        {
            msg_Dbg( p_access, "Authentication Info header: %s", p );
            if( AuthCheckReply( p_access, p, &p_sys->url, &p_sys->auth ) )
                goto error;
        }
        else if( !strcasecmp( psz, "proxy-authentication-info" ) )
        {
            msg_Dbg( p_access, "Proxy Authentication Info header: %s", p );
            if( AuthCheckReply( p_access, p, &p_sys->proxy, &p_sys->proxy_auth ) )
                goto error;
        }
        else if( !strcasecmp( psz, "Accept-Ranges" ) )
        {
            if( !strcasecmp( p, "bytes" ) )
                p_sys->b_seekable = true;
        }

        free( psz );
    }
    /* We close the stream for zero length data, unless of course the
     * server has already promised to do this for us.
     */
    if( p_sys->b_has_size && p_sys->i_remaining == 0 && p_sys->b_persist ) {
        Disconnect( p_access );
    }
    return VLC_SUCCESS;

error:
    Disconnect( p_access );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Disconnect:
 *****************************************************************************/
static void Disconnect( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_tls != NULL)
    {
        vlc_tls_SessionDelete( p_sys->p_tls );
        p_sys->p_tls = NULL;
        p_sys->p_vs = NULL;
    }
    if( p_sys->fd != -1)
    {
        net_Close(p_sys->fd);
        p_sys->fd = -1;
    }

}

/*****************************************************************************
 * Cookies (FIXME: we may want to rewrite that using a nice structure to hold
 * them) (FIXME: only support the "domain=" param)
 *****************************************************************************/

/* Get the NAME=VALUE part of the Cookie */
static char * cookie_get_content( const char * cookie )
{
    char * ret = strdup( cookie );
    if( !ret ) return NULL;
    char * str = ret;
    /* Look for a ';' */
    while( *str && *str != ';' ) str++;
    /* Replace it by a end-char */
    if( *str == ';' ) *str = 0;
    return ret;
}

/* Get the domain where the cookie is stored */
static char * cookie_get_domain( const char * cookie )
{
    const char * str = cookie;
    static const char domain[] = "domain=";
    if( !str )
        return NULL;
    /* Look for a ';' */
    while( *str )
    {
        if( !strncmp( str, domain, sizeof(domain) - 1 /* minus \0 */ ) )
        {
            str += sizeof(domain) - 1 /* minus \0 */;
            char * ret = strdup( str );
            /* Now remove the next ';' if present */
            char * ret_iter = ret;
            while( *ret_iter && *ret_iter != ';' ) ret_iter++;
            if( *ret_iter == ';' )
                *ret_iter = 0;
            return ret;
        }
        /* Go to next ';' field */
        while( *str && *str != ';' ) str++;
        if( *str == ';' ) str++;
        /* skip blank */
        while( *str && *str == ' ' ) str++;
    }
    return NULL;
}

/* Get NAME in the NAME=VALUE field */
static char * cookie_get_name( const char * cookie )
{
    char * ret = cookie_get_content( cookie ); /* NAME=VALUE */
    if( !ret ) return NULL;
    char * str = ret;
    while( *str && *str != '=' ) str++;
    *str = 0;
    return ret;
}

/* Add a cookie in cookies, checking to see how it should be added */
static void cookie_append( vlc_array_t * cookies, char * cookie )
{
    int i;

    if( !cookie )
        return;

    char * cookie_name = cookie_get_name( cookie );

    /* Don't send invalid cookies */
    if( !cookie_name )
        return;

    char * cookie_domain = cookie_get_domain( cookie );
    for( i = 0; i < vlc_array_count( cookies ); i++ )
    {
        char * current_cookie = vlc_array_item_at_index( cookies, i );
        char * current_cookie_name = cookie_get_name( current_cookie );
        char * current_cookie_domain = cookie_get_domain( current_cookie );

        assert( current_cookie_name );

        bool is_domain_matching = (
                      ( !cookie_domain && !current_cookie_domain ) ||
                      ( cookie_domain && current_cookie_domain &&
                        !strcmp( cookie_domain, current_cookie_domain ) ) );

        if( is_domain_matching && !strcmp( cookie_name, current_cookie_name )  )
        {
            /* Remove previous value for this cookie */
            free( current_cookie );
            vlc_array_remove( cookies, i );

            /* Clean */
            free( current_cookie_name );
            free( current_cookie_domain );
            break;
        }
        free( current_cookie_name );
        free( current_cookie_domain );
    }
    free( cookie_name );
    free( cookie_domain );
    vlc_array_append( cookies, cookie );
}


/*****************************************************************************
 * HTTP authentication
 *****************************************************************************/

static void AuthReply( access_t *p_access, const char *psz_prefix,
                       vlc_url_t *p_url, http_auth_t *p_auth )
{
    access_sys_t *p_sys = p_access->p_sys;
    char *psz_value;

    psz_value =
        http_auth_FormatAuthorizationHeader( VLC_OBJECT(p_access), p_auth,
                                             "GET", p_url->psz_path,
                                             p_url->psz_username,
                                             p_url->psz_password );
    if ( psz_value == NULL )
        return;

    net_Printf( p_access, p_sys->fd, p_sys->p_vs,
                "%sAuthorization: %s\r\n", psz_prefix, psz_value );
    free( psz_value );
}

static int AuthCheckReply( access_t *p_access, const char *psz_header,
                           vlc_url_t *p_url, http_auth_t *p_auth )
{
    return
        http_auth_ParseAuthenticationInfoHeader( VLC_OBJECT(p_access), p_auth,
                                                 psz_header, "",
                                                 p_url->psz_path,
                                                 p_url->psz_username,
                                                 p_url->psz_password );
}
