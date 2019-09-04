/*****************************************************************************
 * http.c: HTTP input module
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont
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

#undef MODULE_STRING
#define MODULE_STRING "oldhttp"

#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_meta.h>
#include <vlc_tls.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_charset.h>
#include <vlc_input_item.h>
#include <vlc_http.h>
#include <vlc_interrupt.h>
#include <vlc_keystore.h>
#include <vlc_memstream.h>

#include <assert.h>
#include <limits.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

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

    add_bool( "http-reconnect", false, RECONNECT_TEXT,
              RECONNECT_LONGTEXT, true )
    /* 'itpc' = iTunes Podcast */
    add_shortcut( "http", "unsv", "itpc", "icyx" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    vlc_tls_t *stream;

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

    int        i_icy_meta;
    uint64_t   i_icy_offset;
    char       *psz_icy_name;
    char       *psz_icy_genre;
    char       *psz_icy_title;

    uint64_t offset;
    uint64_t size;

    bool b_reconnect;
    bool b_has_size;
} access_sys_t;

/* */
static ssize_t Read( stream_t *, void *, size_t );
static int Seek( stream_t *, uint64_t );
static int Control( stream_t *, int, va_list );

/* */
static int Connect( stream_t * );
static void Disconnect( stream_t * );


static int AuthCheckReply( stream_t *p_access, const char *psz_header,
                           vlc_url_t *p_url, vlc_http_auth_t *p_auth );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    stream_t *p_access = (stream_t*)p_this;
    const char *psz_url = p_access->psz_url;
    char *psz;
    int ret = VLC_EGENERIC;
    vlc_credential credential;

    access_sys_t *p_sys = vlc_obj_malloc( p_this, sizeof(*p_sys) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    p_sys->stream = NULL;
    p_sys->b_proxy = false;
    p_sys->psz_proxy_passbuf = NULL;
    p_sys->psz_mime = NULL;
    p_sys->b_icecast = false;
    p_sys->psz_location = NULL;
    p_sys->psz_user_agent = NULL;
    p_sys->psz_referrer = NULL;
    p_sys->psz_username = NULL;
    p_sys->psz_password = NULL;
    p_sys->i_icy_meta = 0;
    p_sys->i_icy_offset = 0;
    p_sys->psz_icy_name = NULL;
    p_sys->psz_icy_genre = NULL;
    p_sys->psz_icy_title = NULL;
    p_sys->b_has_size = false;
    p_sys->offset = 0;
    p_sys->size = 0;
    p_access->p_sys = p_sys;

    if( vlc_UrlParse( &p_sys->url, psz_url ) || p_sys->url.psz_host == NULL )
    {
        msg_Err( p_access, "invalid URL" );
        vlc_UrlClean( &p_sys->url );
        return VLC_EGENERIC;
    }
    if( p_sys->url.i_port <= 0 )
        p_sys->url.i_port = 80;

    vlc_credential_init( &credential, &p_sys->url );

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

    if( vlc_credential_get( &credential, p_access, NULL, NULL, NULL, NULL ) )
    {
        p_sys->url.psz_username = (char *) credential.psz_username;
        p_sys->url.psz_password = (char *) credential.psz_password;
    }

connect:
    /* Connect */
    if( Connect( p_access ) )
        goto disconnect;

    if( p_sys->i_code == 401 )
    {
        if( p_sys->auth.psz_realm == NULL )
        {
            msg_Err( p_access, "authentication failed without realm" );
            goto disconnect;
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
                goto disconnect;
            msg_Err( p_access, "retrying with user=%s", p_sys->psz_username );
            p_sys->url.psz_username = p_sys->psz_username;
            p_sys->url.psz_password = p_sys->psz_password;
            Disconnect( p_access );
            goto connect;
        }
        else
            goto disconnect;
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
        goto disconnect;
    }

    if( p_sys->b_reconnect ) msg_Dbg( p_access, "auto re-connect enabled" );

    /* Set up p_access */
    p_access->pf_read = Read;
    p_access->pf_control = Control;
    p_access->pf_seek = Seek;

    vlc_credential_clean( &credential );

    return VLC_SUCCESS;

disconnect:
    Disconnect( p_access );

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

    return ret;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    stream_t     *p_access = (stream_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    vlc_UrlClean( &p_sys->url );
    if( p_sys->b_proxy )
        vlc_UrlClean( &p_sys->proxy );

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
}

/* Read data from the socket */
static int ReadData( stream_t *p_access, int *pi_read,
                     void *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;

    *pi_read = vlc_tls_Read(p_sys->stream, p_buffer, i_len, false);
    if( *pi_read < 0 && errno != EINTR && errno != EAGAIN )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read: Read up to i_len bytes from the http connection and place in
 * p_buffer. Return the actual number of bytes read
 *****************************************************************************/
static int ReadICYMeta( stream_t *p_access );

static ssize_t Read( stream_t *p_access, void *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;

    if (p_sys->stream == NULL)
        return 0;

    int i_chunk = i_len;

    if( p_sys->i_icy_meta > 0 )
    {
        if( UINT64_MAX - i_chunk < p_sys->offset )
            i_chunk = UINT64_MAX - p_sys->offset;

        if( p_sys->offset + i_chunk > p_sys->i_icy_offset )
            i_chunk = p_sys->i_icy_offset - p_sys->offset;
    }

    int i_read = 0;
    if( ReadData( p_access, &i_read, (uint8_t*)p_buffer, i_chunk ) )
        return 0;

    if( i_read < 0 )
        return -1; /* EINTR / EAGAIN */

    if( i_read == 0 )
    {
        Disconnect( p_access );
        if( p_sys->b_reconnect )
        {
            msg_Dbg( p_access, "got disconnected, trying to reconnect" );
            if( Connect( p_access ) )
                msg_Dbg( p_access, "reconnection failed" );
            else
                return -1;
        }
        return 0;
    }

    assert( i_read >= 0 );
    p_sys->offset += i_read;

    if( p_sys->i_icy_meta > 0 &&
        p_sys->offset == p_sys->i_icy_offset )
    {
        if( ReadICYMeta( p_access ) )
            return 0;
        p_sys->i_icy_offset = p_sys->offset + p_sys->i_icy_meta;
    }

    return i_read;
}

static int ReadICYMeta( stream_t *p_access )
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
            p++;
        }
        else
        {
            char *psz = strchr( p, ';' );
            if( psz ) *psz = '\0';
        }

        if( !p_sys->psz_icy_title ||
            strcmp( p_sys->psz_icy_title, p ) )
        {
            free( p_sys->psz_icy_title );
            char *psz_tmp = strdup( p );
            p_sys->psz_icy_title = EnsureUTF8( psz_tmp );
            if( !p_sys->psz_icy_title )
                free( psz_tmp );

            msg_Dbg( p_access, "New Icy-Title=%s", p_sys->psz_icy_title );
            if( p_access->p_input_item )
                input_item_SetMeta( p_access->p_input_item, vlc_meta_NowPlaying,
                                    p_sys->psz_icy_title );
        }
    }
    free( psz_meta );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Seek: close and re-open a connection at the right place
 *****************************************************************************/
static int Seek( stream_t *p_access, uint64_t i_pos )
{
    (void) p_access; (void) i_pos;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( stream_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    bool       *pb_bool;

    switch( i_query )
    {
        /* */
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
            pb_bool = va_arg( args, bool* );
            *pb_bool = false;
            break;
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            pb_bool = va_arg( args, bool* );
            *pb_bool = true;
            break;

        /* */
        case STREAM_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS(var_InheritInteger( p_access, "network-caching" ));
            break;

        case STREAM_GET_SIZE:
            if( !p_sys->b_has_size )
                return VLC_EGENERIC;
            *va_arg( args, uint64_t*) = p_sys->size;
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

/*****************************************************************************
 * Connect:
 *****************************************************************************/
static int Connect( stream_t *p_access )
{
    access_sys_t   *p_sys = p_access->p_sys;
    vlc_url_t      srv = p_sys->b_proxy ? p_sys->proxy : p_sys->url;
    ssize_t val;

    /* Clean info */
    free( p_sys->psz_location );
    free( p_sys->psz_mime );

    free( p_sys->psz_icy_genre );
    free( p_sys->psz_icy_name );
    free( p_sys->psz_icy_title );

    vlc_http_auth_Init( &p_sys->auth );
    vlc_http_auth_Init( &p_sys->proxy_auth );
    p_sys->psz_location = NULL;
    p_sys->psz_mime = NULL;
    p_sys->i_icy_meta = 0;
    p_sys->i_icy_offset = 0;
    p_sys->psz_icy_name = NULL;
    p_sys->psz_icy_genre = NULL;
    p_sys->psz_icy_title = NULL;
    p_sys->b_has_size = false;
    p_sys->offset = 0;
    p_sys->size = 0;

    struct vlc_memstream stream;

    vlc_memstream_open(&stream);

    vlc_memstream_puts(&stream, "GET ");
    if( p_sys->b_proxy )
        vlc_memstream_printf( &stream, "http://%s:%d",
                              p_sys->url.psz_host, p_sys->url.i_port );
    if( p_sys->url.psz_path == NULL || p_sys->url.psz_path[0] == '\0' )
        vlc_memstream_putc( &stream, '/' );
    else
        vlc_memstream_puts( &stream, p_sys->url.psz_path );
    if( p_sys->url.psz_option != NULL )
        vlc_memstream_printf( &stream, "?%s", p_sys->url.psz_option );
    vlc_memstream_puts( &stream, " HTTP/1.0\r\n" );

    vlc_memstream_printf( &stream, "Host: %s", p_sys->url.psz_host );
    if( p_sys->url.i_port != 80 )
        vlc_memstream_printf( &stream, ":%d", p_sys->url.i_port );
    vlc_memstream_puts( &stream, "\r\n" );

    /* User Agent */
    vlc_memstream_printf( &stream, "User-Agent: %s\r\n",
                          p_sys->psz_user_agent );
    /* Referrer */
    if (p_sys->psz_referrer)
        vlc_memstream_printf( &stream, "Referer: %s\r\n",
                              p_sys->psz_referrer );

    /* Authentication */
    if( p_sys->url.psz_username != NULL && p_sys->url.psz_password != NULL )
    {
        char *auth;

        auth = vlc_http_auth_FormatAuthorizationHeader( VLC_OBJECT(p_access),
                            &p_sys->auth, "GET", p_sys->url.psz_path,
                            p_sys->url.psz_username, p_sys->url.psz_password );
        if( auth != NULL )
             vlc_memstream_printf( &stream, "Authorization: %s\r\n", auth );
        free( auth );
    }

    /* Proxy Authentication */
    if( p_sys->b_proxy && p_sys->proxy.psz_username != NULL
     && p_sys->proxy.psz_password != NULL )
    {
        char *auth;

        auth = vlc_http_auth_FormatAuthorizationHeader( VLC_OBJECT(p_access),
                        &p_sys->proxy_auth, "GET", p_sys->url.psz_path,
                        p_sys->proxy.psz_username, p_sys->proxy.psz_password );
        if( auth != NULL )
             vlc_memstream_printf( &stream, "Proxy-Authorization: %s\r\n",
                                   auth );
        free( auth );
    }

    /* ICY meta data request */
    vlc_memstream_puts( &stream, "Icy-MetaData: 1\r\n" );

    vlc_memstream_puts( &stream, "\r\n" );

    if( vlc_memstream_close( &stream ) )
        return -1;

    /* Open connection */
    assert(p_sys->stream == NULL); /* No open sockets (leaking fds is BAD) */
    p_sys->stream = vlc_tls_SocketOpenTCP(VLC_OBJECT(p_access),
                                          srv.psz_host, srv.i_port);
    if (p_sys->stream == NULL)
    {
        msg_Err( p_access, "cannot connect to %s:%d", srv.psz_host, srv.i_port );
        free( stream.ptr );
        return -1;
    }

    msg_Dbg( p_access, "sending request:\n%s", stream.ptr );
    val = vlc_tls_Write(p_sys->stream, stream.ptr, stream.length);
    free( stream.ptr );

    if( val < (ssize_t)stream.length )
    {
        msg_Err( p_access, "failed to send request" );
        Disconnect( p_access );
        return -2;
    }

    /* Read Answer */
    char *psz = vlc_tls_GetLine(p_sys->stream);
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
    }
    else
    {
        msg_Err( p_access, "invalid HTTP reply '%s'", psz );
        free( psz );
        goto error;
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
        char *p, *p_trailing;

        psz = vlc_tls_GetLine(p_sys->stream);
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
            uint64_t i_size = (uint64_t)atoll( p );
            if(i_size > p_sys->size) {
                p_sys->b_has_size = true;
                p_sys->size = i_size;
            }
        }
        else if( !strcasecmp( psz, "Location" ) )
        {
            char * psz_new_loc;

            /* This does not follow RFC 2068, but yet if the url is not absolute,
             * handle it as everyone does. */
            if( p[0] == '/' )
            {
                if( p_sys->url.i_port == 80 )
                {
                    if( asprintf(&psz_new_loc, "http://%s%s",
                                 p_sys->url.psz_host, p) < 0 )
                        goto error;
                }
                else
                {
                    if( asprintf(&psz_new_loc, "http://%s:%d%s",
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
        else if( !strcasecmp( psz, "Icy-MetaInt" ) )
        {
            msg_Dbg( p_access, "Icy-MetaInt: %s", p );
            p_sys->i_icy_meta = atoi( p );
            if( p_sys->i_icy_meta < 0 )
                p_sys->i_icy_meta = 0;
            if( p_sys->i_icy_meta > 1 )
            {
                p_sys->i_icy_offset = p_sys->i_icy_meta;
                p_sys->b_icecast = true;
            }

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
            if ( p_access->p_input_item )
                input_item_SetMeta( p_access->p_input_item, vlc_meta_Title,
                                    p_sys->psz_icy_name );

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
            if( p_access->p_input_item )
                input_item_SetMeta( p_access->p_input_item, vlc_meta_Genre,
                                    p_sys->psz_icy_genre );
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
static void Disconnect( stream_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if (p_sys->stream != NULL)
        vlc_tls_Close(p_sys->stream);
    p_sys->stream = NULL;

    vlc_http_auth_Deinit( &p_sys->auth );
    vlc_http_auth_Deinit( &p_sys->proxy_auth );
}

/*****************************************************************************
 * HTTP authentication
 *****************************************************************************/

static int AuthCheckReply( stream_t *p_access, const char *psz_header,
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
