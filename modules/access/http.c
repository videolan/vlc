/*****************************************************************************
 * http.c: HTTP access plug-in
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN
 * $Id: http.c,v 1.52 2004/01/09 12:23:47 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_playlist.h"
#include "network.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define PROXY_TEXT N_("Specify an HTTP proxy")
#define PROXY_LONGTEXT N_( \
    "Specify an HTTP proxy to use. It must be in the form " \
    "http://myproxy.mydomain:myport/. If none is specified, the HTTP_PROXY " \
    "environment variable will be tried." )

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for http streams. This " \
    "value should be set in millisecond units." )

vlc_module_begin();
    set_description( _("HTTP input") );
    set_capability( "access", 0 );
    add_category_hint( N_("http"), NULL, VLC_FALSE );
        add_string( "http-proxy", NULL, NULL, PROXY_TEXT, PROXY_LONGTEXT, VLC_FALSE );
        add_integer( "http-caching", 4 * DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
        add_string( "http-user", NULL, NULL, "HTTP user name", "HTTP user name for Basic Authentification", VLC_FALSE );
        add_string( "http-pwd", NULL , NULL, "HTTP password", "HTTP password for Basic Authentification", VLC_FALSE );
        add_string( "http-user-agent", COPYRIGHT_MESSAGE , NULL, "HTTP user agent", "HTTP user agent", VLC_FALSE );
    add_shortcut( "http" );
    add_shortcut( "http4" );
    add_shortcut( "http6" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct access_sys_t
{
    int fd;

    /* From uri */
    vlc_url_t url;
    char    *psz_user;
    char    *psz_passwd;
    char    *psz_user_agent;

    /* Proxy */
    vlc_bool_t b_proxy;
    vlc_url_t  proxy;

    /* */
    int        i_code;
    char      *psz_protocol;
    int        i_version;

    char       *psz_mime;
    char       *psz_location;

    int64_t    i_tell;
    int64_t    i_size;
};

static void    Seek( input_thread_t *, off_t );
static ssize_t Read( input_thread_t *, byte_t *, size_t );

static void    ParseURL( access_sys_t *, char *psz_url );
static int     Connect( input_thread_t *, vlc_bool_t *, off_t *, off_t );

static char *b64_encode( unsigned char *src );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    access_sys_t   *p_sys;
    vlc_value_t    val;

    /* Create private struct */
    p_sys = p_input->p_access_data = malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );
    p_sys->fd = -1;
    p_sys->b_proxy = VLC_FALSE;
    p_sys->i_version = 1;
    p_sys->psz_mime = NULL;
    p_sys->psz_location = NULL;
    p_sys->psz_user_agent = NULL;

    /* First set ipv4/ipv6 */
    var_Create( p_input, "ipv4", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_input, "ipv6", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    if( *p_input->psz_access )
    {
        /* Find out which shortcut was used */
        if( !strncmp( p_input->psz_access, "http4", 6 ) )
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_input, "ipv4", val );

            val.b_bool = VLC_FALSE;
            var_Set( p_input, "ipv6", val );
        }
        else if( !strncmp( p_input->psz_access, "http6", 6 ) )
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_input, "ipv6", val );

            val.b_bool = VLC_FALSE;
            var_Set( p_input, "ipv4", val );
        }
    }

    /* Parse URI */
    ParseURL( p_sys, p_input->psz_name );
    if( p_sys->url.psz_host == NULL || *p_sys->url.psz_host == '\0' )
    {
        msg_Warn( p_input, "invalid host" );
        goto error;
    }
    if( p_sys->url.i_port <= 0 )
    {
        p_sys->url.i_port = 80;
    }
    if( !p_sys->psz_user || *p_sys->psz_user == '\0' )
    {
        var_Create( p_input, "http-user", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_input, "http-user", &val );
        p_sys->psz_user = val.psz_string;

        var_Create( p_input, "http-pwd", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Get( p_input, "http-pwd", &val );
        p_sys->psz_passwd = val.psz_string;
    }

    /* Do user agent */
    var_Create( p_input, "http-user-agent", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_input, "http-user-agent", &val );
    p_sys->psz_user_agent = val.psz_string;

    /* Check proxy */
    var_Create( p_input, "http-proxy", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_input, "http-proxy", &val );
    if( val.psz_string && *val.psz_string )
    {
        p_sys->b_proxy = VLC_TRUE;
        vlc_UrlParse( &p_sys->proxy, val.psz_string, 0 );
    }
    else
    {
        char *psz_proxy = getenv( "http_proxy" );
        if( psz_proxy && *psz_proxy )
        {
            p_sys->b_proxy = VLC_TRUE;
            vlc_UrlParse( &p_sys->proxy, val.psz_string, 0 );
        }
        if( psz_proxy )
        {
            free( psz_proxy );
        }
    }
    if( val.psz_string )
    {
        free( val.psz_string );
    }

    if( p_sys->b_proxy )
    {
        if( p_sys->proxy.psz_host == NULL || *p_sys->proxy.psz_host == '\0' )
        {
            msg_Warn( p_input, "invalid proxy host" );
            goto error;
        }
        if( p_sys->proxy.i_port <= 0 )
        {
            p_sys->proxy.i_port = 80;
        }
    }

    msg_Dbg( p_input, "http: server='%s' port=%d file='%s", p_sys->url.psz_host, p_sys->url.i_port, p_sys->url.psz_path );
    if( p_sys->b_proxy )
    {
        msg_Dbg( p_input, "      proxy %s:%d", p_sys->proxy.psz_host, p_sys->proxy.i_port );
    }
    if( p_sys->psz_user && *p_sys->psz_user )
    {
        msg_Dbg( p_input, "      user='%s', pwd='%s'", p_sys->psz_user, p_sys->psz_passwd );
    }

    /* Connect */
    if( Connect( p_input, &p_input->stream.b_seekable, &p_input->stream.p_selected_area->i_size, 0 ) )
    {
        /* Retry with http 1.0 */
        p_sys->i_version = 0;

        if( Connect( p_input, &p_input->stream.b_seekable, &p_input->stream.p_selected_area->i_size, 0 ) )
        {
            goto error;
        }
    }

    if( ( p_sys->i_code == 301 || p_sys->i_code == 302 ||
          p_sys->i_code == 303 || p_sys->i_code == 307 ) &&
        p_sys->psz_location && *p_sys->psz_location )
    {
        playlist_t * p_playlist;

        msg_Dbg( p_input, "redirection to %s", p_sys->psz_location );

        p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST, FIND_PARENT );
        if( !p_playlist )
        {
            msg_Err( p_input, "redirection failed: can't find playlist" );
            goto error;
        }
        p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
        playlist_Add( p_playlist, p_sys->psz_location, p_sys->psz_location,
                      PLAYLIST_INSERT | PLAYLIST_GO,
                      p_playlist->i_index + 1 );
        vlc_object_release( p_playlist );

        p_sys->i_size = 0;  /* Force to stop reading */
    }

    /* Finish to set up p_input */
    p_input->pf_read = Read;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = Seek;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = VLC_TRUE;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    p_input->i_mtu = 0;
    if( !strcmp( p_sys->psz_protocol, "ICY" ) &&
        ( !p_input->psz_demux || !*p_input->psz_demux ) )
    {
        if( !strcasecmp( p_sys->psz_mime, "video/nsv" ) )
        {
            p_input->psz_demux = strdup( "nsv" );
        }
        else
        {
            p_input->psz_demux = strdup( "mp3" );
        }
        msg_Info( p_input, "ICY server found, %s demuxer selected", p_input->psz_demux );
    }

    /* Update default_pts to a suitable value for http access */
    var_Create( p_input, "http-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "http-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

    return VLC_SUCCESS;

error:
    vlc_UrlClean( &p_sys->url );
    vlc_UrlClean( &p_sys->proxy );
    if( p_sys->psz_mime ) free( p_sys->psz_mime );
    if( p_sys->psz_location ) free( p_sys->psz_location );
    if( p_sys->psz_user_agent ) free( p_sys->psz_user_agent );
    if( p_sys->psz_user ) free( p_sys->psz_user );
    if( p_sys->psz_passwd ) free( p_sys->psz_passwd );

    if( p_sys->fd > 0 )
    {
        net_Close( p_sys->fd );
    }
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    access_sys_t   *p_sys   = p_input->p_access_data;

    vlc_UrlClean( &p_sys->url );
    vlc_UrlClean( &p_sys->proxy );

    if( p_sys->psz_user ) free( p_sys->psz_user );
    if( p_sys->psz_passwd ) free( p_sys->psz_passwd );

    if( p_sys->psz_mime ) free( p_sys->psz_mime );
    if( p_sys->psz_location ) free( p_sys->psz_location );

    if( p_sys->psz_user_agent ) free( p_sys->psz_user_agent );

    if( p_sys->fd > 0 )
    {
        net_Close( p_sys->fd );
    }
    free( p_sys );
}

/*****************************************************************************
 * Seek: close and re-open a connection at the right place
 *****************************************************************************/
static void Seek( input_thread_t * p_input, off_t i_pos )
{
    access_sys_t   *p_sys   = p_input->p_access_data;

    msg_Dbg( p_input, "trying to seek to "I64Fd, i_pos );

    net_Close( p_sys->fd ); p_sys->fd = -1;

    if( Connect( p_input, &p_input->stream.b_seekable, &p_input->stream.p_selected_area->i_size, i_pos ) )
    {
        msg_Err( p_input, "seek failed" );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_tell = i_pos;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * Read: Read up to i_len bytes from the http connection and place in
 * p_buffer. Return the actual number of bytes read
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
    access_sys_t   *p_sys   = p_input->p_access_data;
    int            i_read;

    if( p_sys->fd < 0 )
    {
        return -1;
    }
    if( p_sys->i_size > 0 && i_len + p_sys->i_tell > p_sys->i_size )
    {
        if( ( i_len = p_sys->i_size - p_sys->i_tell ) == 0 )
        {
            return 0;
        }
    }

    i_read = net_Read( p_input, p_sys->fd, p_buffer, i_len, VLC_FALSE );
    if( i_read > 0 )
    {
        p_sys->i_tell += i_read;
    }
    return i_read;
}

/*****************************************************************************
 * ParseURL: extract user:password
 *****************************************************************************/
static void ParseURL( access_sys_t *p_sys, char *psz_url )
{
    char *psz_dup = strdup( psz_url );
    char *p = psz_dup;
    char *psz;

    /* Syntax //[user:password]@<hostname>[:<port>][/<path>] */
    while( *p == '/' )
    {
        p++;
    }
    psz = p;

    /* Parse auth */
    if( ( p = strchr( psz, '@' ) ) )
    {
        char *comma;

        *p++ = '\0';
        comma = strchr( psz, ':' );

        /* Retreive user:password */
        if( comma )
        {
            *comma++ = '\0';

            p_sys->psz_user = strdup( psz );
            p_sys->psz_passwd = strdup( comma );
        }
        else
        {
            p_sys->psz_user = strdup( psz );
        }
    }
    else
    {
        p = psz;
    }

    /* Parse uri */
    vlc_UrlParse( &p_sys->url, p, 0 );

    free( psz_dup );
}

/*****************************************************************************
 * Connect:
 *****************************************************************************/
static int Connect( input_thread_t *p_input, vlc_bool_t *pb_seekable, off_t *pi_size, off_t i_tell )
{
    access_sys_t   *p_sys   = p_input->p_access_data;
    vlc_url_t      srv = p_sys->b_proxy ? p_sys->proxy : p_sys->url;
    char           *psz;

    /* Clean info */
    if( p_sys->psz_location ) free( p_sys->psz_location );
    if( p_sys->psz_mime ) free( p_sys->psz_mime );

    p_sys->psz_location = NULL;
    p_sys->psz_mime = NULL;
    p_sys->i_size = -1;
    p_sys->i_tell = i_tell;


    /* Open connection */
    p_sys->fd = net_OpenTCP( p_input, srv.psz_host, srv.i_port );
    if( p_sys->fd < 0 )
    {
        msg_Err( p_input, "cannot connect to %s:%d", srv.psz_host, srv.i_port );
        return VLC_EGENERIC;
    }

    if( p_sys->b_proxy )
    {
        net_Printf( VLC_OBJECT(p_input), p_sys->fd,
                    "GET http://%s:%d/%s HTTP/1.%d\r\n",
                    p_sys->url.psz_host, p_sys->url.i_port, p_sys->url.psz_path,
                    p_sys->i_version );
    }
    else
    {
        net_Printf( VLC_OBJECT(p_input), p_sys->fd,
                    "GET %s HTTP/1.%d\r\n"
                    "Host: %s\r\n",
                    p_sys->url.psz_path, p_sys->i_version, p_sys->url.psz_host );
    }
    /* User Agent */
    net_Printf( VLC_OBJECT(p_input), p_sys->fd, "User-Agent: %s\r\n", p_sys->psz_user_agent );
    /* Offset */
    if( p_sys->i_version == 1 )
    {
        net_Printf( VLC_OBJECT(p_input), p_sys->fd,
                    "Range: bytes="I64Fd"-\r\n", i_tell );
    }
    /* Authentification */
    if( p_sys->psz_user && *p_sys->psz_user )
    {
        char *buf;
        char *b64;

        vasprintf( &buf, "%s:%s", p_sys->psz_user,
                   p_sys->psz_passwd ? p_sys->psz_passwd : "" );

        b64 = b64_encode( buf );

        net_Printf( VLC_OBJECT(p_input), p_sys->fd, "Authorization: Basic %s", b64 );
        free( b64 );
    }
    net_Printf( VLC_OBJECT(p_input), p_sys->fd, "Connection: Close\r\n" );

    if( net_Printf( VLC_OBJECT(p_input), p_sys->fd, "\r\n" ) < 0 )
    {
        msg_Err( p_input, "Failed to send request\n" );
        net_Close( p_sys->fd ); p_sys->fd = -1;
        return VLC_EGENERIC;
    }

    /* Set values */
    *pb_seekable = p_sys->i_version == 1 ? VLC_TRUE : VLC_FALSE;
    *pi_size = 0;

    /* Read Answer */
    if( ( psz = net_Gets( VLC_OBJECT(p_input), p_sys->fd ) ) == NULL )
    {
        msg_Err( p_input, "Failed to read answer\n" );
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
    }
    else
    {
        msg_Err( p_input, "invalid HTTP reply '%s'", psz );
        free( psz );
        goto error;
    }
    msg_Dbg( p_input, "Protocol '%s' answer code %d", p_sys->psz_protocol, p_sys->i_code );
    if( !strcmp( p_sys->psz_protocol, "ICY" ) )
    {
        *pb_seekable = VLC_FALSE;
    }
    if( p_sys->i_code != 206 )
    {
        *pb_seekable = VLC_FALSE;
    }
    if( p_sys->i_code >= 400 )
    {
        msg_Err( p_input, "error: %s", psz );
        free( psz );
        goto error;
    }
    free( psz );

    for( ;; )
    {
        char *psz = net_Gets( VLC_OBJECT(p_input), p_sys->fd );
        char *p;

        if( psz == NULL )
        {
            msg_Err( p_input, "Failed to read answer\n" );
            goto error;
        }

        msg_Dbg( p_input, "Line=%s", psz );
        if( *psz == '\0' )
        {
            free( psz );
            break;
        }


        if( ( p = strchr( psz, ':' ) ) == NULL )
        {
            msg_Err( p_input, "malformed header line: %s", psz );
            free( psz );
            goto error;
        }
        *p++ = '\0';

        if( !strcasecmp( psz, "Content-Length" ) )
        {
            *pi_size = p_sys->i_size = i_tell + atoll( p );
            msg_Dbg( p_input, "stream size="I64Fd, p_sys->i_size );
        }
        else if( !strcasecmp( psz, "Location" ) )
        {
            if( p_sys->psz_location ) free( p_sys->psz_location );
            p_sys->psz_location = strdup( p );
        }
        else if( !strcasecmp( psz, "Content-Type" ) )
        {
            if( p_sys->psz_mime ) free( p_sys->psz_mime );
            p_sys->psz_mime = strdup( p );
        }

        free( psz );
    }
    return VLC_SUCCESS;

error:
    net_Close( p_sys->fd ); p_sys->fd = -1;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * b64_encode:
 *****************************************************************************/
static char *b64_encode( unsigned char *src )
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    char *dst = malloc( strlen( src ) * 4 / 3 + 12 );
    char *ret = dst;
    unsigned i_bits = 0;
    unsigned i_shift = 0;

    for( ;; )
    {
        if( *src )
        {
            i_bits = ( i_bits << 8 )|( *src++ );
            i_shift += 8;
        }
        else if( i_shift > 0 )
        {
           i_bits <<= 6 - i_shift;
           i_shift = 6;
        }
        else
        {
            *dst++ = '=';
            break;
        }

        while( i_shift >= 6 )
        {
            i_shift -= 6;
            *dst++ = b64[(i_bits >> i_shift)&0x3f];
        }
    }

    *dst++ = '\0';

    return ret;
}
