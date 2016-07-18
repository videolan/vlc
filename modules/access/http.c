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

#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_meta.h>
#include <vlc_network.h>
#include <vlc_url.h>
#include <vlc_tls.h>
#include <vlc_strings.h>
#include <vlc_charset.h>
#include <vlc_input.h>
#include <vlc_http.h>
#include <vlc_interrupt.h>
#include <vlc_keystore.h>

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
    add_bool( "http-reconnect", false, RECONNECT_TEXT,
              RECONNECT_LONGTEXT, true )
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
    vlc_tls_creds_t *p_creds;
    vlc_tls_t *p_tls;

    /* From uri */
    vlc_url_t url;
    char    *psz_user_agent;
    char    *psz_referrer;
    char    *psz_username;
    char    *psz_password;
    vlc_http_auth_t auth;

    /* Proxy */
    bool b_proxy;
    vlc_url_t  proxy;
    vlc_http_auth_t proxy_auth;
    char       *psz_proxy_passbuf;

    /* */
    int        i_code;

    char       *psz_mime;
    char       *psz_location;
    bool b_icecast;

    bool b_chunked;
    int64_t    i_chunk;

    int        i_icy_meta;
    uint64_t   i_icy_offset;
    char       *psz_icy_name;
    char       *psz_icy_genre;
    char       *psz_icy_title;

    uint64_t offset;
    uint64_t size;

    /* cookie jar borrowed from playlist, do not free */
    vlc_http_cookie_jar_t * cookies;

    bool b_seekable;
    bool b_reconnect;
    bool b_continuous;
    bool b_has_size;
};

/* */
static ssize_t Read( access_t *, void *, size_t );
static int Seek( access_t *, uint64_t );
static int Control( access_t *, int, va_list );

/* */
static int Connect( access_t *, uint64_t );
static void Disconnect( access_t * );


static void AuthReply( access_t *p_acces, const char *psz_prefix,
                       vlc_url_t *p_url, vlc_http_auth_t *p_auth );
static int AuthCheckReply( access_t *p_access, const char *psz_header,
                           vlc_url_t *p_url, vlc_http_auth_t *p_auth );
static vlc_http_cookie_jar_t *GetCookieJar( vlc_object_t *p_this );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t *p_access = (access_t*)p_this;
    const char *psz_url = p_access->psz_url;
    char *psz;
    int ret = VLC_EGENERIC;
    vlc_credential credential;

    access_sys_t *p_sys = malloc( sizeof(*p_sys) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    p_sys->fd = -1;
    p_sys->b_proxy = false;
    p_sys->psz_proxy_passbuf = NULL;
    p_sys->b_seekable = true;
    p_sys->psz_mime = NULL;
    p_sys->b_icecast = false;
    p_sys->psz_location = NULL;
    p_sys->psz_user_agent = NULL;
    p_sys->psz_referrer = NULL;
    p_sys->psz_username = NULL;
    p_sys->psz_password = NULL;
    p_sys->p_creds = NULL;
    p_sys->p_tls = NULL;
    p_sys->i_icy_meta = 0;
    p_sys->i_icy_offset = 0;
    p_sys->psz_icy_name = NULL;
    p_sys->psz_icy_genre = NULL;
    p_sys->psz_icy_title = NULL;
    p_sys->b_has_size = false;
    p_sys->offset = 0;
    p_sys->size = 0;
    p_access->p_sys = p_sys;

    /* Only forward an store cookies if the corresponding option is activated */
    if( var_CreateGetBool( p_access, "http-forward-cookies" ) )
        p_sys->cookies = GetCookieJar( p_this );
    else
        p_sys->cookies = NULL;

    vlc_http_auth_Init( &p_sys->auth );
    vlc_http_auth_Init( &p_sys->proxy_auth );
    vlc_UrlParse( &p_sys->url, psz_url );
    vlc_credential_init( &credential, &p_sys->url );

    if( p_sys->url.psz_host == NULL || *p_sys->url.psz_host == '\0' )
    {
        msg_Warn( p_access, "invalid host" );
        goto error;
    }
    if( !strcmp( p_sys->url.psz_protocol, "https" ) )
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
        msg_Dbg(p_access, "querying proxy for %s", psz_url);
        psz = vlc_getProxyUrl(psz_url);

        if (psz != NULL)
            msg_Dbg(p_access, "proxy: %s", psz);
        else
            msg_Dbg(p_access, "no proxy");
    }
    if( psz != NULL )
    {
        p_sys->b_proxy = true;
        vlc_UrlParse( &p_sys->proxy, psz );
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

    if( vlc_credential_get( &credential, p_access, NULL, NULL, NULL, NULL ) )
    {
        p_sys->url.psz_username = (char *) credential.psz_username;
        p_sys->url.psz_password = (char *) credential.psz_password;
    }

connect:
    /* Connect */
    if( Connect( p_access, 0 ) )
        goto error;

    if( p_sys->i_code == 401 )
    {
        if( p_sys->auth.psz_realm == NULL )
        {
            msg_Err( p_access, "authentication failed without realm" );
            goto error;
        }
        /* FIXME ? */
        if( p_sys->url.psz_username && p_sys->url.psz_password &&
            p_sys->auth.psz_nonce && p_sys->auth.i_nonce == 0 )
        {
            Disconnect( p_access );
            goto connect;
        }
        free( p_sys->psz_username );
        free( p_sys->psz_password );
        p_sys->psz_username = p_sys->psz_password = NULL;

        msg_Dbg( p_access, "authentication failed for realm %s",
                 p_sys->auth.psz_realm );

        credential.psz_realm = p_sys->auth.psz_realm;
        credential.psz_authtype = p_sys->auth.psz_nonce  ? "Digest" : "Basic";

        if( vlc_credential_get( &credential, p_access, NULL, NULL,
                               _("HTTP authentication"),
                               _("Please enter a valid login name and a "
                               "password for realm %s."), p_sys->auth.psz_realm ) )
        {
            p_sys->psz_username = strdup(credential.psz_username);
            p_sys->psz_password = strdup(credential.psz_password);
            if (!p_sys->psz_username || !p_sys->psz_password)
                goto error;
            msg_Err( p_access, "retrying with user=%s", p_sys->psz_username );
            p_sys->url.psz_username = p_sys->psz_username;
            p_sys->url.psz_password = p_sys->psz_password;
            Disconnect( p_access );
            goto connect;
        }
        else
            goto error;
    }
    else
        vlc_credential_store( &credential, p_access );

    if( ( p_sys->i_code == 301 || p_sys->i_code == 302 ||
          p_sys->i_code == 303 || p_sys->i_code == 307 ) &&
        p_sys->psz_location != NULL )
    {
        p_access->psz_url = p_sys->psz_location;
        p_sys->psz_location = NULL;
        ret = VLC_ACCESS_REDIRECT;
        goto error;
    }

    if( p_sys->b_reconnect ) msg_Dbg( p_access, "auto re-connect enabled" );

    /* Set up p_access */
    p_access->pf_read = Read;
    p_access->pf_control = Control;
    p_access->pf_seek = Seek;

    vlc_credential_clean( &credential );

    return VLC_SUCCESS;

error:
    vlc_credential_clean( &credential );
    vlc_UrlClean( &p_sys->url );
    if( p_sys->b_proxy )
        vlc_UrlClean( &p_sys->proxy );
    free( p_sys->psz_proxy_passbuf );
    free( p_sys->psz_mime );
    free( p_sys->psz_location );
    free( p_sys->psz_user_agent );
    free( p_sys->psz_referrer );
    free( p_sys->psz_username );
    free( p_sys->psz_password );

    Disconnect( p_access );
    vlc_tls_Delete( p_sys->p_creds );

    free( p_sys );
    return ret;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    vlc_UrlClean( &p_sys->url );
    vlc_http_auth_Deinit( &p_sys->auth );
    if( p_sys->b_proxy )
        vlc_UrlClean( &p_sys->proxy );
    vlc_http_auth_Deinit( &p_sys->proxy_auth );

    free( p_sys->psz_mime );
    free( p_sys->psz_location );

    free( p_sys->psz_icy_name );
    free( p_sys->psz_icy_genre );
    free( p_sys->psz_icy_title );

    free( p_sys->psz_user_agent );
    free( p_sys->psz_referrer );
    free( p_sys->psz_username );
    free( p_sys->psz_password );

    Disconnect( p_access );
    vlc_tls_Delete( p_sys->p_creds );

    free( p_sys );
}

/* Read data from the socket taking care of chunked transfer if needed */
static int ReadData( access_t *p_access, int *pi_read,
                     void *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    if( p_sys->b_chunked )
    {
        if( p_sys->i_chunk < 0 )
            return VLC_EGENERIC;

        if( p_sys->i_chunk <= 0 )
        {
            char *psz;
            if( p_sys->p_tls != NULL )
                psz = vlc_tls_GetLine( p_sys->p_tls );
            else
                psz = net_Gets( p_access, p_sys->fd );
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

    if( p_sys->p_tls != NULL )
        *pi_read = vlc_tls_Read( p_sys->p_tls, p_buffer, i_len, false );
    else
        *pi_read = vlc_recv_i11e( p_sys->fd, p_buffer, i_len, 0 );
    if( *pi_read < 0 && errno != EINTR && errno != EAGAIN )
        return VLC_EGENERIC;
    if( *pi_read <= 0 )
        return VLC_SUCCESS;

    if( p_sys->b_chunked )
    {
        p_sys->i_chunk -= *pi_read;
        if( p_sys->i_chunk <= 0 )
        {   /* read the empty line */
            if( p_sys->p_tls != NULL )
                free( vlc_tls_GetLine( p_sys->p_tls ) );
            else
                free( net_Gets( p_access, p_sys->fd ) );
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read: Read up to i_len bytes from the http connection and place in
 * p_buffer. Return the actual number of bytes read
 *****************************************************************************/
static int ReadICYMeta( access_t *p_access );
static ssize_t Read( access_t *p_access, void *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    if( p_sys->fd == -1 )
        return 0;

    if( i_len == 0 )
        return 0;

    if( p_sys->i_icy_meta > 0 && p_sys->offset - p_sys->i_icy_offset > 0 )
    {
        int64_t i_next = p_sys->i_icy_meta -
                                    (p_sys->offset - p_sys->i_icy_offset ) % p_sys->i_icy_meta;

        if( i_next == p_sys->i_icy_meta )
        {
            if( ReadICYMeta( p_access ) )
                return 0;
        }
        if( i_len > i_next )
            i_len = i_next;
    }

    if( ReadData( p_access, &i_read, p_buffer, i_len ) )
        return 0;

    if( i_read < 0 )
        return -1; /* EINTR / EAGAIN */

    if( i_read == 0 )
    {
        Disconnect( p_access );
        if( p_sys->b_reconnect )
        {
            msg_Dbg( p_access, "got disconnected, trying to reconnect" );
            if( Connect( p_access, p_sys->offset ) )
                msg_Dbg( p_access, "reconnection failed" );
            else
                return -1;
        }
        return 0;
    }

    assert( i_read >= 0 );
    p_sys->offset += i_read;

    return i_read;
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

            msg_Dbg( p_access, "New Icy-Title=%s", p_sys->psz_icy_title );
            input_thread_t *p_input = p_access->p_input;
            if( p_input )
            {
                input_item_t *p_input_item = input_GetItem( p_access->p_input );
                if( p_input_item )
                    input_item_SetMeta( p_input_item, vlc_meta_NowPlaying, p_sys->psz_icy_title );
            }
        }
    }
    free( psz_meta );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Seek: close and re-open a connection at the right place
 *****************************************************************************/
static int Seek( access_t *p_access, uint64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;

    msg_Dbg( p_access, "trying to seek to %"PRId64, i_pos );
    Disconnect( p_access );

    if( p_sys->size && i_pos >= p_sys->size )
    {
        msg_Err( p_access, "seek too far" );
        int retval = Seek( p_access, p_sys->size - 1 );
        if( retval == VLC_SUCCESS ) {
            uint8_t p_buffer[2];
            Read( p_access, p_buffer, 1);
        }
        return retval;
    }
    if( Connect( p_access, i_pos ) )
    {
        msg_Err( p_access, "seek failed" );
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

    switch( i_query )
    {
        /* */
        case STREAM_CAN_SEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = p_sys->b_seekable;
            break;
        case STREAM_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = true;
            break;

        /* */
        case STREAM_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = INT64_C(1000)
                * var_InheritInteger( p_access, "network-caching" );
            break;

        case STREAM_GET_SIZE:
            if( !p_sys->b_has_size )
                return VLC_EGENERIC;
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = p_sys->size;
           break;

        /* */
        case STREAM_SET_PAUSE_STATE:
            break;

        case STREAM_GET_CONTENT_TYPE:
        {
            char **type = va_arg( args, char ** );

            if( p_sys->b_icecast && p_sys->psz_mime == NULL )
                *type = strdup( "audio/mpeg" );
            else if( !strcasecmp( p_access->psz_name, "itpc" ) )
                *type = strdup( "application/rss+xml" );
            else if( !strcasecmp( p_access->psz_name, "unsv" ) &&
                p_sys->psz_mime != NULL &&
                !strcasecmp( p_sys->psz_mime, "misc/ultravox" ) )
                /* Grrrr! detect ultravox server and force NSV demuxer */
                *type = strdup( "video/nsa" );
            else if( p_sys->psz_mime )
                *type = strdup( p_sys->psz_mime );
            else
                return VLC_EGENERIC;
            break;
        }

        default:
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

static int WriteHeaders( access_t *access, const char *fmt, ... )
{
    access_sys_t *sys = access->p_sys;
    char *str;
    va_list args;
    int len;

    va_start( args, fmt );
    len = vasprintf( &str, fmt, args );
    if( likely(len >= 0) )
    {
        if( ((sys->p_tls != NULL)
            ? vlc_tls_Write( sys->p_tls, str, len )
            : net_Write( access, sys->fd, str, len )) < len )
            len = -1;
        free( str );
    }
    va_end( args );
    return len;
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

    free( p_sys->psz_icy_genre );
    free( p_sys->psz_icy_name );
    free( p_sys->psz_icy_title );


    p_sys->psz_location = NULL;
    p_sys->psz_mime = NULL;
    p_sys->b_chunked = false;
    p_sys->i_chunk = 0;
    p_sys->i_icy_meta = 0;
    p_sys->i_icy_offset = i_tell;
    p_sys->psz_icy_name = NULL;
    p_sys->psz_icy_genre = NULL;
    p_sys->psz_icy_title = NULL;
    p_sys->b_has_size = false;
    p_sys->offset = i_tell;
    p_sys->size = 0;

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
            unsigned i_status;

            WriteHeaders( p_access,
                          "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n\r\n",
                          p_sys->url.psz_host, p_sys->url.i_port,
                          p_sys->url.psz_host, p_sys->url.i_port);

            psz = net_Gets( p_access, p_sys->fd );
            if( psz == NULL )
            {
                msg_Err( p_access, "cannot establish HTTP/TLS tunnel" );
                Disconnect( p_access );
                return -1;
            }

            if( sscanf( psz, "HTTP/1.%*u %3u", &i_status ) != 1 )
                i_status = 0;
            free( psz );

            if( ( i_status / 100 ) != 2 )
            {
                msg_Err( p_access, "HTTP/TLS tunnel through proxy denied" );
                Disconnect( p_access );
                return -1;
            }

            do
            {
                psz = net_Gets( p_access, p_sys->fd );
                if( psz == NULL )
                {
                    msg_Err( p_access, "HTTP proxy connection failed" );
                    Disconnect( p_access );
                    return -1;
                }

                if( *psz == '\0' )
                    i_status = 0;

                free( psz );
            }
            while( i_status );
        }

        /* TLS/SSL handshake */
        const char *alpn[] = { "http/1.1", NULL };

        p_sys->p_tls = vlc_tls_ClientSessionCreateFD( p_sys->p_creds, p_sys->fd,
                                                      p_sys->url.psz_host,
                                                      "https", alpn, NULL );
        if( p_sys->p_tls == NULL )
        {
            msg_Err( p_access, "cannot establish HTTP/TLS session" );
            Disconnect( p_access );
            return -1;
        }
    }

    const char *psz_path = p_sys->url.psz_path;
    if( !psz_path || !*psz_path )
        psz_path = "/";
    if( p_sys->b_proxy && p_sys->p_tls == NULL )
        WriteHeaders( p_access, "GET http://%s:%d%s%s%s HTTP/1.1\r\n",
                      p_sys->url.psz_host, p_sys->url.i_port,
                      psz_path, p_sys->url.psz_option ? "?" : "",
                      p_sys->url.psz_option ? p_sys->url.psz_option : "" );
    else
        WriteHeaders( p_access, "GET %s%s%s HTTP/1.1\r\n",
                      psz_path, p_sys->url.psz_option ? "?" : "",
                      p_sys->url.psz_option ? p_sys->url.psz_option : "" );
    if( p_sys->url.i_port != (p_sys->p_tls ? 443 : 80) )
        WriteHeaders( p_access, "Host: %s:%d\r\n",
                      p_sys->url.psz_host, p_sys->url.i_port );
    else
        WriteHeaders( p_access, "Host: %s\r\n", p_sys->url.psz_host );
    /* User Agent */
    WriteHeaders( p_access, "User-Agent: %s\r\n", p_sys->psz_user_agent );
    /* Referrer */
    if (p_sys->psz_referrer)
        WriteHeaders( p_access, "Referer: %s\r\n", p_sys->psz_referrer );
    /* Offset */
    if( !p_sys->b_continuous )
        WriteHeaders( p_access, "Range: bytes=%"PRIu64"-\r\n", i_tell );
    WriteHeaders( p_access, "Connection: close\r\n" );

    /* Cookies */
    if( p_sys->cookies )
    {
        char * psz_cookiestring = vlc_http_cookies_for_url( p_sys->cookies, &p_sys->url );
        if ( psz_cookiestring )
        {
            msg_Dbg( p_access, "Sending Cookie %s", psz_cookiestring );
            WriteHeaders( p_access, "Cookie: %s\r\n", psz_cookiestring );
            free( psz_cookiestring );
        }
    }

    /* Authentication */
    if( p_sys->url.psz_username && p_sys->url.psz_password )
        AuthReply( p_access, "", &p_sys->url, &p_sys->auth );

    /* Proxy Authentication */
    if( p_sys->b_proxy && p_sys->proxy.psz_username != NULL
     && p_sys->proxy.psz_password != NULL )
        AuthReply( p_access, "Proxy-", &p_sys->proxy, &p_sys->proxy_auth );

    /* ICY meta data request */
    WriteHeaders( p_access, "Icy-MetaData: 1\r\n" );

    if( WriteHeaders( p_access, "\r\n" ) < 0 )
    {
        msg_Err( p_access, "failed to send request" );
        Disconnect( p_access );
        return -2;
    }

    /* Read Answer */
    char *psz;
    if( p_sys->p_tls != NULL )
        psz = vlc_tls_GetLine( p_sys->p_tls );
    else
        psz = net_Gets( p_access, p_sys->fd );
    if( psz == NULL )
    {
        msg_Err( p_access, "failed to read answer" );
        goto error;
    }
    if( !strncmp( psz, "HTTP/1.", 7 ) )
    {
        p_sys->i_code = atoi( &psz[9] );
        msg_Dbg( p_access, "HTTP answer code %d", p_sys->i_code );
    }
    else if( !strncmp( psz, "ICY", 3 ) )
    {
        p_sys->i_code = atoi( &psz[4] );
        msg_Dbg( p_access, "ICY answer code %d", p_sys->i_code );
        p_sys->b_icecast = true;
        p_sys->b_reconnect = true;
        p_sys->b_seekable = false;
    }
    else
    {
        msg_Err( p_access, "invalid HTTP reply '%s'", psz );
        free( psz );
        goto error;
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
        char *psz, *p, *p_trailing;

        if( p_sys->p_tls != NULL )
            psz = vlc_tls_GetLine( p_sys->p_tls );
        else
            psz = net_Gets( p_access, p_sys->fd );
        if( psz == NULL )
        {
            msg_Err( p_access, "failed to read answer" );
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
            uint64_t i_size = i_tell + (uint64_t)atoll( p );
            if(i_size > p_sys->size) {
                p_sys->b_has_size = true;
                p_sys->size = i_size;
            }
        }
        else if( !strcasecmp( psz, "Content-Range" ) ) {
            uint64_t i_ntell = i_tell;
            uint64_t i_nend = (p_sys->size > 0) ? (p_sys->size - 1) : i_tell;
            uint64_t i_nsize = p_sys->size;
            sscanf(p,"bytes %"SCNu64"-%"SCNu64"/%"SCNu64,&i_ntell,&i_nend,&i_nsize);
            if(i_nend > i_ntell ) {
                p_sys->offset = i_ntell;
                p_sys->i_icy_offset  = i_ntell;
                uint64_t i_size = (i_nsize > i_nend) ? i_nsize : (i_nend + 1);
                if(i_size > p_sys->size) {
                    p_sys->b_has_size = true;
                    p_sys->size = i_size;
                }
                msg_Dbg( p_access, "stream size=%"PRIu64",pos=%"PRIu64,
                         i_nsize, i_ntell);
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
            else
                vlc_xml_decode( p_sys->psz_icy_name );
            msg_Dbg( p_access, "Icy-Name: %s", p_sys->psz_icy_name );
            input_thread_t *p_input = p_access->p_input;
            if ( p_input )
            {
                input_item_t *p_input_item = input_GetItem( p_access->p_input );
                if ( p_input_item )
                    input_item_SetMeta( p_input_item, vlc_meta_Title, p_sys->psz_icy_name );
            }

            p_sys->b_icecast = true; /* be on the safeside. set it here as well. */
            p_sys->b_reconnect = true;
        }
        else if( !strcasecmp( psz, "Icy-Genre" ) )
        {
            free( p_sys->psz_icy_genre );
            char *psz_tmp = strdup( p );
            p_sys->psz_icy_genre = EnsureUTF8( psz_tmp );
            if( !p_sys->psz_icy_genre )
                free( psz_tmp );
            else
                vlc_xml_decode( p_sys->psz_icy_genre );
            msg_Dbg( p_access, "Icy-Genre: %s", p_sys->psz_icy_genre );
            input_thread_t *p_input = p_access->p_input;
            if( p_input )
            {
                input_item_t *p_input_item = input_GetItem( p_access->p_input );
                if( p_input_item )
                    input_item_SetMeta( p_input_item, vlc_meta_Genre, p_sys->psz_icy_genre );
            }
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
                if ( vlc_http_cookies_append( p_sys->cookies, p, &p_sys->url ) )
                    msg_Dbg( p_access, "Accepting Cookie: %s", p );
                else
                    msg_Dbg( p_access, "Rejected Cookie: %s", p );
            }
            else
                msg_Dbg( p_access, "We have a Cookie we won't remember: %s", p );
        }
        else if( !strcasecmp( psz, "www-authenticate" ) )
        {
            msg_Dbg( p_access, "Authentication header: %s", p );
            vlc_http_auth_ParseWwwAuthenticateHeader( VLC_OBJECT(p_access),
                                                      &p_sys->auth, p );
        }
        else if( !strcasecmp( psz, "proxy-authenticate" ) )
        {
            msg_Dbg( p_access, "Proxy authentication header: %s", p );
            vlc_http_auth_ParseWwwAuthenticateHeader( VLC_OBJECT(p_access),
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
    return 0;

error:
    Disconnect( p_access );
    return -2;
}

/*****************************************************************************
 * Disconnect:
 *****************************************************************************/
static void Disconnect( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_tls != NULL)
        vlc_tls_Close( p_sys->p_tls );
    else if( p_sys->fd != -1)
        net_Close(p_sys->fd);
    p_sys->p_tls = NULL;
    p_sys->fd = -1;
}

/*****************************************************************************
 * HTTP authentication
 *****************************************************************************/

static void AuthReply( access_t *p_access, const char *psz_prefix,
                       vlc_url_t *p_url, vlc_http_auth_t *p_auth )
{
    char *psz_value;

    psz_value =
        vlc_http_auth_FormatAuthorizationHeader( VLC_OBJECT(p_access), p_auth,
                                                 "GET", p_url->psz_path,
                                                 p_url->psz_username,
                                                 p_url->psz_password );
    if ( psz_value == NULL )
        return;

    WriteHeaders( p_access, "%sAuthorization: %s\r\n", psz_prefix, psz_value );
    free( psz_value );
}

static int AuthCheckReply( access_t *p_access, const char *psz_header,
                           vlc_url_t *p_url, vlc_http_auth_t *p_auth )
{
    return
        vlc_http_auth_ParseAuthenticationInfoHeader( VLC_OBJECT(p_access),
                                                     p_auth,
                                                     psz_header, "",
                                                     p_url->psz_path,
                                                     p_url->psz_username,
                                                     p_url->psz_password );
}

/*****************************************************************************
 * HTTP cookies
 *****************************************************************************/

/**
 * Inherit the cookie jar from the playlist
 *
 * @param p_this: http access object
 * @return A borrowed reference to a vlc_http_cookie_jar_t, do not free
 */
static vlc_http_cookie_jar_t *GetCookieJar( vlc_object_t *p_this )
{
    vlc_value_t val;

    if ( var_Inherit( p_this, "http-cookies", VLC_VAR_ADDRESS, &val ) == VLC_SUCCESS )
        return val.p_address;
    else
        return NULL;
}
