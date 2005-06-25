/*****************************************************************************
 * http.c: HTTP input module
 *****************************************************************************
 * Copyright (C) 2001-2005 VideoLAN
 * $Id$
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
#include "vlc_meta.h"
#include "network.h"
#include "vlc_tls.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define PROXY_TEXT N_("HTTP proxy")
#define PROXY_LONGTEXT N_( \
    "You can specify an HTTP proxy to use. It must be of the form " \
    "http://myproxy.mydomain:myport/. If none is specified, the HTTP_PROXY " \
    "environment variable will be tried." )

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for http streams. This " \
    "value should be set in millisecond units." )

#define USER_TEXT N_("HTTP user name")
#define USER_LONGTEXT N_("Allows you to modify the user name that will " \
    "be used for the connection (Basic authentication only).")

#define PASS_TEXT N_("HTTP password")
#define PASS_LONGTEXT N_("Allows you to modify the password that will be " \
    "used for the connection.")

#define AGENT_TEXT N_("HTTP user agent")
#define AGENT_LONGTEXT N_("Allows you to modify the user agent that will be " \
    "used for the connection.")

#define RECONNECT_TEXT N_("Auto re-connect")
#define RECONNECT_LONGTEXT N_("Will automatically attempt a re-connection " \
    "in case it was untimely closed.")

#define CONTINUOUS_TEXT N_("Continuous stream")
#define CONTINUOUS_LONGTEXT N_("Enable this option to read a file that is " \
    "being constantly updated (for example, a JPG file on a server)")

vlc_module_begin();
    set_description( _("HTTP input") );
    set_capability( "access2", 0 );
    set_shortname( _( "HTTP/HTTPS" ) );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_string( "http-proxy", NULL, NULL, PROXY_TEXT, PROXY_LONGTEXT,
                VLC_FALSE );
    add_integer( "http-caching", 4 * DEFAULT_PTS_DELAY / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_string( "http-user", NULL, NULL, USER_TEXT, USER_LONGTEXT, VLC_FALSE );
    add_string( "http-pwd", NULL , NULL, PASS_TEXT, PASS_LONGTEXT, VLC_FALSE );
    add_string( "http-user-agent", COPYRIGHT_MESSAGE , NULL, AGENT_TEXT,
                AGENT_LONGTEXT, VLC_FALSE );
    add_bool( "http-reconnect", 0, NULL, RECONNECT_TEXT,
              RECONNECT_LONGTEXT, VLC_TRUE );
    add_bool( "http-continuous", 0, NULL, CONTINUOUS_TEXT,
              CONTINUOUS_LONGTEXT, VLC_TRUE );

    add_shortcut( "http" );
    add_shortcut( "http4" );
    add_shortcut( "http6" );
    add_shortcut( "https" );
    add_shortcut( "unsv" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct access_sys_t
{
    int fd;
    tls_session_t *p_tls;
    v_socket_t    *p_vs;

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
    char       *psz_protocol;
    int        i_version;

    char       *psz_mime;
    char       *psz_pragma;
    char       *psz_location;
    vlc_bool_t b_mms;
    vlc_bool_t b_icecast;
    vlc_bool_t b_ssl;

    vlc_bool_t b_chunked;
    int64_t    i_chunk;

    int        i_icy_meta;
    char       *psz_icy_name;
    char       *psz_icy_genre;
    char       *psz_icy_title;

    int i_remaining;

    vlc_bool_t b_seekable;
    vlc_bool_t b_reconnect;
    vlc_bool_t b_continuous;
    vlc_bool_t b_pace_control;
};

/* */
static int Read( access_t *, uint8_t *, int );
static int Seek( access_t *, int64_t );
static int Control( access_t *, int, va_list );

/* */
static void ParseURL( access_sys_t *, char *psz_url );
static int  Connect( access_t *, int64_t );
static int Request( access_t *p_access, int64_t i_tell );
static void Disconnect( access_t * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    char         *psz;

    /* First set ipv4/ipv6 */
    var_Create( p_access, "ipv4", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_access, "ipv6", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    if( *p_access->psz_access )
    {
        vlc_value_t val;
        /* Find out which shortcut was used */
        if( !strncmp( p_access->psz_access, "http4", 6 ) )
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_access, "ipv4", val );

            val.b_bool = VLC_FALSE;
            var_Set( p_access, "ipv6", val );
        }
        else if( !strncmp( p_access->psz_access, "http6", 6 ) )
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_access, "ipv6", val );

            val.b_bool = VLC_FALSE;
            var_Set( p_access, "ipv4", val );
        }
    }

    /* Set up p_access */
    p_access->pf_read = Read;
    p_access->pf_block = NULL;
    p_access->pf_control = Control;
    p_access->pf_seek = Seek;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );
    p_sys->fd = -1;
    p_sys->b_proxy = VLC_FALSE;
    p_sys->i_version = 1;
    p_sys->b_seekable = VLC_TRUE;
    p_sys->psz_mime = NULL;
    p_sys->psz_pragma = NULL;
    p_sys->b_mms = VLC_FALSE;
    p_sys->b_icecast = VLC_FALSE;
    p_sys->psz_location = NULL;
    p_sys->psz_user_agent = NULL;
    p_sys->b_pace_control = VLC_TRUE;
    p_sys->b_ssl = VLC_FALSE;
    p_sys->p_tls = NULL;
    p_sys->p_vs = NULL;
    p_sys->i_icy_meta = 0;
    p_sys->psz_icy_name = NULL;
    p_sys->psz_icy_genre = NULL;
    p_sys->psz_icy_title = NULL;
    p_sys->i_remaining = 0;

    /* Parse URI */
    if( vlc_UrlIsNotEncoded( p_access->psz_path ) )
    {
        psz = vlc_UrlEncode( p_access->psz_path );
        if( psz == NULL )
        {
            free( p_sys );
            return VLC_ENOMEM;
        }

        ParseURL( p_sys, psz );
        free( psz );
    }
    else
        ParseURL( p_sys, p_access->psz_path );

    if( p_sys->url.psz_host == NULL || *p_sys->url.psz_host == '\0' )
    {
        msg_Warn( p_access, "invalid host" );
        goto error;
    }
    if( !strncmp( p_access->psz_access, "https", 5 ) )
    {
        /* SSL over HTTP */
        p_sys->b_ssl = VLC_TRUE;
        if( p_sys->url.i_port <= 0 )
            p_sys->url.i_port = 443;
    }
    else
    {
        if( p_sys->url.i_port <= 0 )
            p_sys->url.i_port = 80;
    }
    if( !p_sys->psz_user || *p_sys->psz_user == '\0' )
    {
        p_sys->psz_user = var_CreateGetString( p_access, "http-user" );
        p_sys->psz_passwd = var_CreateGetString( p_access, "http-pwd" );
    }

    /* Do user agent */
    p_sys->psz_user_agent = var_CreateGetString( p_access, "http-user-agent" );

    /* Check proxy */
    psz = var_CreateGetString( p_access, "http-proxy" );
    if( *psz )
    {
        p_sys->b_proxy = VLC_TRUE;
        vlc_UrlParse( &p_sys->proxy, psz, 0 );
    }
#ifdef HAVE_GETENV
    else
    {
        char *psz_proxy = getenv( "HTTP_PROXY" );
        if( psz_proxy && *psz_proxy )
        {
            p_sys->b_proxy = VLC_TRUE;
            vlc_UrlParse( &p_sys->proxy, psz_proxy, 0 );
        }
    }
#endif
    free( psz );

    if( p_sys->b_proxy )
    {
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

    msg_Dbg( p_access, "http: server='%s' port=%d file='%s",
             p_sys->url.psz_host, p_sys->url.i_port, p_sys->url.psz_path );
    if( p_sys->b_proxy )
    {
        msg_Dbg( p_access, "      proxy %s:%d", p_sys->proxy.psz_host,
                 p_sys->proxy.i_port );
    }
    if( p_sys->psz_user && *p_sys->psz_user )
    {
        msg_Dbg( p_access, "      user='%s', pwd='%s'",
                 p_sys->psz_user, p_sys->psz_passwd );
    }

    p_sys->b_reconnect = var_CreateGetBool( p_access, "http-reconnect" );
    p_sys->b_continuous = var_CreateGetBool( p_access, "http-continuous" );

    /* Connect */
    if( Connect( p_access, 0 ) )
    {
        /* Retry with http 1.0 */
        p_sys->i_version = 0;

        if( p_access->b_die ||
            Connect( p_access, 0 ) )
        {
            goto error;
        }
    }

    if( ( p_sys->i_code == 301 || p_sys->i_code == 302 ||
          p_sys->i_code == 303 || p_sys->i_code == 307 ) &&
        p_sys->psz_location && *p_sys->psz_location )
    {
        playlist_t * p_playlist;
        input_item_t *p_input_item;

        msg_Dbg( p_access, "redirection to %s", p_sys->psz_location );

        p_playlist = vlc_object_find( p_access, VLC_OBJECT_PLAYLIST,
                                      FIND_ANYWHERE );
        if( !p_playlist )
        {
            msg_Err( p_access, "redirection failed: can't find playlist" );
            goto error;
        }

        /* Change the uri */
        vlc_mutex_lock( &p_playlist->object_lock );
        p_input_item = &p_playlist->status.p_item->input;
        vlc_mutex_lock( &p_input_item->lock );
        free( p_input_item->psz_uri );
        free( p_access->psz_path );
        p_input_item->psz_uri = strdup( p_sys->psz_location );
        p_access->psz_path = strdup( p_sys->psz_location );
        vlc_mutex_unlock( &p_input_item->lock );
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );

        /* Clean up current Open() run */
        vlc_UrlClean( &p_sys->url );
        vlc_UrlClean( &p_sys->proxy );
        if( p_sys->psz_mime ) free( p_sys->psz_mime );
        if( p_sys->psz_pragma ) free( p_sys->psz_pragma );
        if( p_sys->psz_location ) free( p_sys->psz_location );
        if( p_sys->psz_user_agent ) free( p_sys->psz_user_agent );
        if( p_sys->psz_user ) free( p_sys->psz_user );
        if( p_sys->psz_passwd ) free( p_sys->psz_passwd );

        Disconnect( p_access );
        free( p_sys );

        /* Do new Open() run with new data */
        return Open( p_this );
    }

    if( p_sys->b_mms )
    {
        msg_Dbg( p_access, "This is actually a live mms server, BAIL" );
        goto error;
    }

    if( !strcmp( p_sys->psz_protocol, "ICY" ) || p_sys->b_icecast )
    {
        if( p_sys->psz_mime && strcasecmp( p_sys->psz_mime, "application/ogg" ) )
        {
            if( !strcasecmp( p_sys->psz_mime, "video/nsv" ) ||
                !strcasecmp( p_sys->psz_mime, "video/nsa" ) )
                p_access->psz_demux = strdup( "nsv" );
            else if( !strcasecmp( p_sys->psz_mime, "audio/aac" ) ||
                     !strcasecmp( p_sys->psz_mime, "audio/aacp" ) )
                p_access->psz_demux = strdup( "m4a" );
            else if( !strcasecmp( p_sys->psz_mime, "audio/mpeg" ) )
                p_access->psz_demux = strdup( "mp3" );

            msg_Info( p_access, "Raw-audio server found, %s demuxer selected",
                      p_access->psz_demux );

#if 0       /* Doesn't work really well because of the pre-buffering in
             * shoutcast servers (the buffer content will be sent as fast as
             * possible). */
            p_sys->b_pace_control = VLC_FALSE;
#endif
        }
        else if( !p_sys->psz_mime )
        {
             /* Shoutcast */
             p_access->psz_demux = strdup( "mp3" );
        }
        /* else probably Ogg Vorbis */
    }
    else if( !strcasecmp( p_access->psz_access, "unsv" ) &&
             p_sys->psz_mime &&
             !strcasecmp( p_sys->psz_mime, "misc/ultravox" ) )
    {
        /* Grrrr! detect ultravox server and force NSV demuxer */
        p_access->psz_demux = strdup( "nsv" );
    }

    if( p_sys->b_reconnect ) msg_Dbg( p_access, "auto re-connect enabled" );

    /* PTS delay */
    var_Create( p_access, "http-caching", VLC_VAR_INTEGER |VLC_VAR_DOINHERIT );

    return VLC_SUCCESS;

error:
    vlc_UrlClean( &p_sys->url );
    vlc_UrlClean( &p_sys->proxy );
    if( p_sys->psz_mime ) free( p_sys->psz_mime );
    if( p_sys->psz_pragma ) free( p_sys->psz_pragma );
    if( p_sys->psz_location ) free( p_sys->psz_location );
    if( p_sys->psz_user_agent ) free( p_sys->psz_user_agent );
    if( p_sys->psz_user ) free( p_sys->psz_user );
    if( p_sys->psz_passwd ) free( p_sys->psz_passwd );

    Disconnect( p_access );
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
    vlc_UrlClean( &p_sys->proxy );

    if( p_sys->psz_user ) free( p_sys->psz_user );
    if( p_sys->psz_passwd ) free( p_sys->psz_passwd );

    if( p_sys->psz_mime ) free( p_sys->psz_mime );
    if( p_sys->psz_pragma ) free( p_sys->psz_pragma );
    if( p_sys->psz_location ) free( p_sys->psz_location );

    if( p_sys->psz_icy_name ) free( p_sys->psz_icy_name );
    if( p_sys->psz_icy_genre ) free( p_sys->psz_icy_genre );
    if( p_sys->psz_icy_title ) free( p_sys->psz_icy_title );

    if( p_sys->psz_user_agent ) free( p_sys->psz_user_agent );

    Disconnect( p_access );
    free( p_sys );
}

/*****************************************************************************
 * Read: Read up to i_len bytes from the http connection and place in
 * p_buffer. Return the actual number of bytes read
 *****************************************************************************/
static int ReadICYMeta( access_t *p_access );
static int Read( access_t *p_access, uint8_t *p_buffer, int i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    if( p_sys->fd < 0 )
    {
        p_access->info.b_eof = VLC_TRUE;
        return 0;
    }

    if( p_access->info.i_size > 0 &&
        i_len + p_access->info.i_pos > p_access->info.i_size )
    {
        if( ( i_len = p_access->info.i_size - p_access->info.i_pos ) == 0 )
        {
            p_access->info.b_eof = VLC_TRUE;
            return 0;
        }
    }

    if( p_sys->b_chunked )
    {
        if( p_sys->i_chunk < 0 )
        {
            p_access->info.b_eof = VLC_TRUE;
            return 0;
        }

        if( p_sys->i_chunk <= 0 )
        {
            char *psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, p_sys->p_vs );
            /* read the chunk header */
            if( psz == NULL )
            {
                msg_Dbg( p_access, "failed reading chunk-header line" );
                return -1;
            }
            p_sys->i_chunk = strtoll( psz, NULL, 16 );
            free( psz );

            if( p_sys->i_chunk <= 0 )   /* eof */
            {
                p_sys->i_chunk = -1;
                p_access->info.b_eof = VLC_TRUE;
                return 0;
            }
        }

        if( i_len > p_sys->i_chunk )
        {
            i_len = p_sys->i_chunk;
        }
    }

    if( p_sys->b_continuous && i_len > p_sys->i_remaining )
    {
        /* Only ask for the remaining length */
        int i_new_len = p_sys->i_remaining;
        if( i_new_len == 0 )
        {
            Request( p_access, 0 );
            i_read = Read( p_access, p_buffer, i_len );
            return i_read;
        }
        i_len = i_new_len;
    }

    if( p_sys->i_icy_meta > 0 && p_access->info.i_pos > 0 )
    {
        int64_t i_next = p_sys->i_icy_meta -
                                    p_access->info.i_pos % p_sys->i_icy_meta;

        if( i_next == p_sys->i_icy_meta )
        {
            if( ReadICYMeta( p_access ) )
            {
                p_access->info.b_eof = VLC_TRUE;
                return -1;
            }
        }
        if( i_len > i_next )
            i_len = i_next;
    }

    i_read = net_Read( p_access, p_sys->fd, p_sys->p_vs, p_buffer, i_len, VLC_FALSE );

    if( i_read > 0 )
    {
        p_access->info.i_pos += i_read;

        if( p_sys->b_chunked )
        {
            p_sys->i_chunk -= i_read;
            if( p_sys->i_chunk <= 0 )
            {
                /* read the empty line */
                char *psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, p_sys->p_vs );
                if( psz ) free( psz );
            }
        }
    }
    else if( i_read == 0 )
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
            p_sys->b_continuous = VLC_FALSE;
            i_read = Read( p_access, p_buffer, i_len );
            p_sys->b_continuous = VLC_TRUE;
        }
        Disconnect( p_access );
        if( p_sys->b_reconnect )
        {
            msg_Dbg( p_access, "got disconnected, trying to reconnect" );
            if( Connect( p_access, p_access->info.i_pos ) )
            {
                msg_Dbg( p_access, "reconnection failed" );
            }
            else
            {
                p_sys->b_reconnect = VLC_FALSE;
                i_read = Read( p_access, p_buffer, i_len );
                p_sys->b_reconnect = VLC_TRUE;
            }
        }

        if( i_read == 0 ) p_access->info.b_eof = VLC_TRUE;
    }

    if( p_sys->b_continuous )
    {
        p_sys->i_remaining -= i_read;
    }

    return i_read;
}

static int ReadICYMeta( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    uint8_t buffer[1];
    char *psz_meta;
    int i_read;
    char *p;

    /* Read meta data length */
    i_read = net_Read( p_access, p_sys->fd, p_sys->p_vs, buffer, 1,
                       VLC_TRUE );
    if( i_read <= 0 )
        return VLC_EGENERIC;


    if( buffer[0] <= 0 )
        return VLC_SUCCESS;

    msg_Dbg( p_access, "ICY meta size=%d", buffer[0] * 16);

    psz_meta = malloc( buffer[0] * 16 + 1 );
    i_read = net_Read( p_access, p_sys->fd, p_sys->p_vs,
                       psz_meta, buffer[0] * 16, VLC_TRUE );

    if( i_read != buffer[0] * 16 )
        return VLC_EGENERIC;

    psz_meta[buffer[0]*16] = '\0'; /* Just in case */

    msg_Dbg( p_access, "icy-meta=%s", psz_meta );

    /* Now parse the meta */
    /* Look for StreamTitle= */
    p = strcasestr( psz_meta, "StreamTitle=" );
    if( p )
    {
        p += strlen( "StreamTitle=" );
        if( *p == '\'' || *p == '"' )
        {
            char *psz = strchr( &p[1], p[0] );
            if( !psz )
                psz = strchr( &p[1], ';' );

            if( psz ) *psz = '\0';
        }
        else
        {
            char *psz = strchr( &p[1], ';' );
            if( psz ) *psz = '\0';
        }

        if( p_sys->psz_icy_title ) free( p_sys->psz_icy_title );

        p_sys->psz_icy_title = strdup( &p[1] );

        p_access->info.i_update |= INPUT_UPDATE_META;
    }

    free( psz_meta );

    msg_Dbg( p_access, "New Title=%s", p_sys->psz_icy_title );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Seek: close and re-open a connection at the right place
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    msg_Dbg( p_access, "trying to seek to "I64Fd, i_pos );

    Disconnect( p_access );

    if( Connect( p_access, i_pos ) )
    {
        msg_Err( p_access, "seek failed" );
        p_access->info.b_eof = VLC_TRUE;
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
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;
    vlc_meta_t **pp_meta;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = p_sys->b_seekable;
            break;
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );

#if 0       /* Disable for now until we have a clock synchro algo
             * which works with something else than MPEG over UDP */
            *pb_bool = p_sys->b_pace_control;
#endif
            *pb_bool = VLC_TRUE;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = (int64_t)var_GetInteger( p_access, "http-caching" ) * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            break;

        case ACCESS_GET_META:
            pp_meta = (vlc_meta_t**)va_arg( args, vlc_meta_t** );
            *pp_meta = vlc_meta_New();
            msg_Dbg( p_access, "GET META %s %s %s",
                     p_sys->psz_icy_name, p_sys->psz_icy_genre, p_sys->psz_icy_title );
            if( p_sys->psz_icy_name )
                vlc_meta_Add( *pp_meta, VLC_META_TITLE,
                              p_sys->psz_icy_name );
            if( p_sys->psz_icy_genre )
                vlc_meta_Add( *pp_meta, VLC_META_GENRE,
                              p_sys->psz_icy_genre );
            if( p_sys->psz_icy_title )
                vlc_meta_Add( *pp_meta, VLC_META_NOW_PLAYING,
                              p_sys->psz_icy_title );
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
static int Connect( access_t *p_access, int64_t i_tell )
{
    access_sys_t   *p_sys = p_access->p_sys;
    vlc_url_t      srv = p_sys->b_proxy ? p_sys->proxy : p_sys->url;

    /* Clean info */
    if( p_sys->psz_location ) free( p_sys->psz_location );
    if( p_sys->psz_mime ) free( p_sys->psz_mime );
    if( p_sys->psz_pragma ) free( p_sys->psz_pragma );

    if( p_sys->psz_icy_genre ) free( p_sys->psz_icy_genre );
    if( p_sys->psz_icy_name ) free( p_sys->psz_icy_name );
    if( p_sys->psz_icy_title ) free( p_sys->psz_icy_title );


    p_sys->psz_location = NULL;
    p_sys->psz_mime = NULL;
    p_sys->psz_pragma = NULL;
    p_sys->b_mms = VLC_FALSE;
    p_sys->b_chunked = VLC_FALSE;
    p_sys->i_chunk = 0;
    p_sys->i_icy_meta = 0;
    p_sys->psz_icy_name = NULL;
    p_sys->psz_icy_genre = NULL;
    p_sys->psz_icy_title = NULL;

    p_access->info.i_size = 0;
    p_access->info.i_pos  = i_tell;
    p_access->info.b_eof  = VLC_FALSE;


    /* Open connection */
    p_sys->fd = net_OpenTCP( p_access, srv.psz_host, srv.i_port );
    if( p_sys->fd < 0 )
    {
        msg_Err( p_access, "cannot connect to %s:%d", srv.psz_host, srv.i_port );
        return VLC_EGENERIC;
    }

    /* Initialize TLS/SSL session */
    if( p_sys->b_ssl == VLC_TRUE )
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
                return VLC_EGENERIC;
            }

            net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                        "CONNECT %s:%d HTTP/1.%d\r\nHost: %s:%d\r\n\r\n",
                        p_sys->url.psz_host, p_sys->url.i_port,
                        p_sys->i_version,
                        p_sys->url.psz_host, p_sys->url.i_port);

            psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, NULL );
            if( psz == NULL )
            {
                msg_Err( p_access, "cannot establish HTTP/TLS tunnel" );
                Disconnect( p_access );
                return VLC_EGENERIC;
            }

            sscanf( psz, "HTTP/%*u.%*u %3u", &i_status );
            free( psz );

            if( ( i_status / 100 ) != 2 )
            {
                msg_Err( p_access, "HTTP/TLS tunnel through proxy denied" );
                Disconnect( p_access );
                return VLC_EGENERIC;
            }

            do
            {
                psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, NULL );
                if( psz == NULL )
                {
                    msg_Err( p_access, "HTTP proxy connection failed" );
                    Disconnect( p_access );
                    return VLC_EGENERIC;
                }

                if( *psz == '\0' )
                    i_status = 0;

                free( psz );
            }
            while( i_status );
        }

        /* TLS/SSL handshake */
        p_sys->p_tls = tls_ClientCreate( VLC_OBJECT(p_access), p_sys->fd,
                                         srv.psz_host );
        if( p_sys->p_tls == NULL )
        {
            msg_Err( p_access, "cannot establish HTTP/TLS session" );
            Disconnect( p_access );
            return VLC_EGENERIC;
        }
        p_sys->p_vs = &p_sys->p_tls->sock;
    }

    return Request( p_access, i_tell );
}


static int Request( access_t *p_access, int64_t i_tell )
{
    access_sys_t   *p_sys = p_access->p_sys;
    char           *psz ;
    v_socket_t     *pvs = p_sys->p_vs;

    if( p_sys->b_proxy )
    {
        /* FIXME: support SSL proxies */
        if( p_sys->url.psz_path )
        {
            net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                        "GET http://%s:%d%s HTTP/1.%d\r\n",
                        p_sys->url.psz_host, p_sys->url.i_port,
                        p_sys->url.psz_path, p_sys->i_version );
        }
        else
        {
            net_Printf( VLC_OBJECT(p_access), p_sys->fd, NULL,
                        "GET http://%s:%d/ HTTP/1.%d\r\n",
                        p_sys->url.psz_host, p_sys->url.i_port,
                        p_sys->i_version );
        }
    }
    else
    {
        char *psz_path = p_sys->url.psz_path;
        if( !psz_path || !*psz_path )
        {
            psz_path = "/";
        }
        if( p_sys->url.i_port != 80)
        {
            net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs,
                        "GET %s HTTP/1.%d\r\nHost: %s:%d\r\n",
                        psz_path, p_sys->i_version, p_sys->url.psz_host,
                        p_sys->url.i_port );
        }
        else
        {
            net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs,
                        "GET %s HTTP/1.%d\r\nHost: %s\r\n",
                        psz_path, p_sys->i_version, p_sys->url.psz_host );
        }
    }
    /* User Agent */
    net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs, "User-Agent: %s\r\n",
                p_sys->psz_user_agent );
    /* Offset */
    if( p_sys->i_version == 1 )
    {
        net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs,
                    "Range: bytes="I64Fd"-\r\n", i_tell );
    }

    /* Authentification */
    if( p_sys->psz_user && *p_sys->psz_user )
    {
        char *buf;
        char *b64;

        asprintf( &buf, "%s:%s", p_sys->psz_user,
                   p_sys->psz_passwd ? p_sys->psz_passwd : "" );

        b64 = vlc_b64_encode( buf );
        free( buf );

        net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs,
                    "Authorization: Basic %s\r\n", b64 );
        free( b64 );
    }

    /* ICY meta data request */
    net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs, "Icy-MetaData: 1\r\n" );


    if( p_sys->b_continuous && p_sys->i_version == 1 )
    {
        net_Printf( VLC_OBJECT( p_access ), p_sys->fd, pvs,
                    "Connection: keep-alive\r\n" );
    }
    else
    {
        net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs,
                    "Connection: Close\r\n");
        p_sys->b_continuous = VLC_FALSE;
    }

    if( net_Printf( VLC_OBJECT(p_access), p_sys->fd, pvs, "\r\n" ) < 0 )
    {
        msg_Err( p_access, "failed to send request" );
        Disconnect( p_access );
        return VLC_EGENERIC;
    }

    /* Read Answer */
    if( ( psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, pvs ) ) == NULL )
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
        p_sys->b_reconnect = VLC_TRUE;
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
        p_sys->b_seekable = VLC_FALSE;
    }
    if( p_sys->i_code != 206 )
    {
        p_sys->b_seekable = VLC_FALSE;
    }
    if( p_sys->i_code >= 400 )
    {
        msg_Err( p_access, "error: %s", psz );
        free( psz );
        goto error;
    }
    free( psz );

    for( ;; )
    {
        char *psz = net_Gets( VLC_OBJECT(p_access), p_sys->fd, pvs );
        char *p;

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
        while( *p == ' ' ) p++;

        if( !strcasecmp( psz, "Content-Length" ) )
        {
            if( p_sys->b_continuous )
            {
                p_access->info.i_size = -1;
                msg_Dbg( p_access, "this frame size="I64Fd, atoll(p ) );
                p_sys->i_remaining = atoll( p );
            }
            else
            {
                p_access->info.i_size = i_tell + atoll( p );
                msg_Dbg( p_access, "stream size="I64Fd, p_access->info.i_size );
            }
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
            msg_Dbg( p_access, "Content-Type: %s", p_sys->psz_mime );
        }
        else if( !strcasecmp( psz, "Pragma" ) )
        {
            if( !strcasecmp( psz, "Pragma: features" ) )
                p_sys->b_mms = VLC_TRUE;
            if( p_sys->psz_pragma ) free( p_sys->psz_pragma );
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

                p_sys->b_reconnect = VLC_TRUE;
                p_sys->b_pace_control = VLC_FALSE;
                p_sys->b_icecast = VLC_TRUE;
            }
        }
        else if( !strcasecmp( psz, "Transfer-Encoding" ) )
        {
            msg_Dbg( p_access, "Transfer-Encoding: %s", p );
            if( !strncasecmp( p, "chunked", 7 ) )
            {
                p_sys->b_chunked = VLC_TRUE;
            }
        }
        else if( !strcasecmp( psz, "Icy-MetaInt" ) )
        {
            msg_Dbg( p_access, "Icy-MetaInt: %s", p );
            p_sys->i_icy_meta = atoi( p );
            if( p_sys->i_icy_meta < 0 )
                p_sys->i_icy_meta = 0;

            msg_Warn( p_access, "ICY metaint=%d", p_sys->i_icy_meta );
        }
        else if( !strcasecmp( psz, "Icy-Name" ) )
        {
            if( p_sys->psz_icy_name ) free( p_sys->psz_icy_name );
            p_sys->psz_icy_name = strdup( p );
            msg_Dbg( p_access, "Icy-Name: %s", p_sys->psz_icy_name );

            p_sys->b_icecast = VLC_TRUE; /* be on the safeside. set it here as well. */
            p_sys->b_reconnect = VLC_TRUE;
            p_sys->b_pace_control = VLC_FALSE;
        }
        else if( !strcasecmp( psz, "Icy-Genre" ) )
        {
            if( p_sys->psz_icy_genre ) free( p_sys->psz_icy_genre );
            p_sys->psz_icy_genre = strdup( p );
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

        free( psz );
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
        tls_ClientDelete( p_sys->p_tls );
        p_sys->p_tls = NULL;
        p_sys->p_vs = NULL;
    }
    if( p_sys->fd != -1)
    {
        net_Close(p_sys->fd);
        p_sys->fd = -1;
    }
    
}
