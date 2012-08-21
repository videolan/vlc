/*****************************************************************************
 * http.c
 *****************************************************************************
 * Copyright (C) 2001-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Jon Lech Johansen <jon@nanocrew.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>


#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_httpd.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-http-"

#define USER_TEXT N_("Username")
#define USER_LONGTEXT N_("User name that will be " \
                         "requested to access the stream." )
#define PASS_TEXT N_("Password")
#define PASS_LONGTEXT N_("Password that will be " \
                         "requested to access the stream." )
#define MIME_TEXT N_("Mime")
#define MIME_LONGTEXT N_("MIME returned by the server (autodetected " \
                        "if not specified)." )


vlc_module_begin ()
    set_description( N_("HTTP stream output") )
    set_capability( "sout access", 0 )
    set_shortname( "HTTP" )
    add_shortcut( "http", "https", "mmsh" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_ACO )
    add_string( SOUT_CFG_PREFIX "user", "",
                USER_TEXT, USER_LONGTEXT, true )
    add_password( SOUT_CFG_PREFIX "pwd", "",
                  PASS_TEXT, PASS_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "mime", "",
                MIME_TEXT, MIME_LONGTEXT, true )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "user", "pwd", "mime", NULL
};

static ssize_t Write( sout_access_out_t *, block_t * );
static int Seek ( sout_access_out_t *, off_t  );
static int Control( sout_access_out_t *, int, va_list );

struct sout_access_out_sys_t
{
    /* host */
    httpd_host_t        *p_httpd_host;

    /* stream */
    httpd_stream_t      *p_httpd_stream;

    /* gather header from stream */
    int                 i_header_allocated;
    int                 i_header_size;
    uint8_t             *p_header;
    bool          b_header_complete;
};

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys;

    char                *psz_user;
    char                *psz_pwd;
    char                *psz_mime;

    if( !( p_sys = p_access->p_sys =
                malloc( sizeof( sout_access_out_sys_t ) ) ) )
        return VLC_ENOMEM ;

    config_ChainParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

    const char *path = p_access->psz_path;
    path += strcspn( path, "/" );
    if( path > p_access->psz_path )
    {
        const char *port = strrchr( p_access->psz_path, ':' );
        if( port != NULL && strchr( port, ']' ) != NULL )
            port = NULL; /* IPv6 numeral */
        if( port != p_access->psz_path )
        {
            int len = (port ? port : path) - p_access->psz_path;
            msg_Warn( p_access, "\"%.*s\" HTTP host might be ignored in "
                      "multiple-host configurations, use at your own risks.",
                      len, p_access->psz_path );
            msg_Info( p_access, "Consider passing --http-host=IP on the "
                                "command line instead." );

            char host[len + 1];
            strncpy( host, p_access->psz_path, len );
            host[len] = '\0';

            var_Create( p_access, "http-host", VLC_VAR_STRING );
            var_SetString( p_access, "http-host", host );
        }
        if( port != NULL )
        {
            /* int len = path - ++port;
            msg_Info( p_access, "Consider passing --%s-port=%.*s on the "
                                "command line instead.",
                      strcasecmp( p_access->psz_access, "https" )
                      ? "http" : "https", len, port ); */
            port++;

            int bind_port = atoi( port );
            if( bind_port > 0 )
            {
                const char *var = strcasecmp( p_access->psz_access, "https" )
                                  ? "http-port" : "https-port";
                var_Create( p_access, var, VLC_VAR_INTEGER );
                var_SetInteger( p_access, var, bind_port );
            }
        }
    }
    if( !*path )
        path = "/";

    /* TLS support */
    if( p_access->psz_access && !strcmp( p_access->psz_access, "https" ) )
        p_sys->p_httpd_host = vlc_https_HostNew( VLC_OBJECT(p_access) );
    else
        p_sys->p_httpd_host = vlc_http_HostNew( VLC_OBJECT(p_access) );

    if( p_sys->p_httpd_host == NULL )
    {
        msg_Err( p_access, "cannot start HTTP server" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    psz_user = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "user" );
    psz_pwd = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "pwd" );
    if( p_access->psz_access && !strcmp( p_access->psz_access, "mmsh" ) )
    {
        psz_mime = strdup( "video/x-ms-asf-stream" );
    }
    else
    {
        psz_mime = var_GetNonEmptyString( p_access, SOUT_CFG_PREFIX "mime" );
    }

    p_sys->p_httpd_stream =
        httpd_StreamNew( p_sys->p_httpd_host, path, psz_mime,
                         psz_user, psz_pwd );
    free( psz_user );
    free( psz_pwd );
    free( psz_mime );

    if( p_sys->p_httpd_stream == NULL )
    {
        msg_Err( p_access, "cannot add stream %s", path );
        httpd_HostDelete( p_sys->p_httpd_host );

        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->i_header_allocated = 1024;
    p_sys->i_header_size      = 0;
    p_sys->p_header           = xmalloc( p_sys->i_header_allocated );
    p_sys->b_header_complete  = false;

    p_access->pf_write       = Write;
    p_access->pf_seek        = Seek;
    p_access->pf_control     = Control;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys = p_access->p_sys;

    httpd_StreamDelete( p_sys->p_httpd_stream );
    httpd_HostDelete( p_sys->p_httpd_host );

    free( p_sys->p_header );

    msg_Dbg( p_access, "Close" );

    free( p_sys );
}

static int Control( sout_access_out_t *p_access, int i_query, va_list args )
{
    (void)p_access;

    switch( i_query )
    {
        case ACCESS_OUT_CONTROLS_PACE:
            *va_arg( args, bool * ) = false;
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Write:
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i_err = 0;
    int i_len = 0;

    while( p_buffer )
    {
        block_t *p_next;

        if( p_buffer->i_flags & BLOCK_FLAG_HEADER )
        {
            /* gather header */
            if( p_sys->b_header_complete )
            {
                /* free previously gathered header */
                p_sys->i_header_size = 0;
                p_sys->b_header_complete = false;
            }
            if( (int)(p_buffer->i_buffer + p_sys->i_header_size) >
                p_sys->i_header_allocated )
            {
                p_sys->i_header_allocated =
                    p_buffer->i_buffer + p_sys->i_header_size + 1024;
                p_sys->p_header = xrealloc( p_sys->p_header,
                                                  p_sys->i_header_allocated );
            }
            memcpy( &p_sys->p_header[p_sys->i_header_size],
                    p_buffer->p_buffer,
                    p_buffer->i_buffer );
            p_sys->i_header_size += p_buffer->i_buffer;
        }
        else if( !p_sys->b_header_complete )
        {
            p_sys->b_header_complete = true;

            httpd_StreamHeader( p_sys->p_httpd_stream, p_sys->p_header,
                                p_sys->i_header_size );
        }

        i_len += p_buffer->i_buffer;
        /* send data */
        i_err = httpd_StreamSend( p_sys->p_httpd_stream, p_buffer->p_buffer,
                                  p_buffer->i_buffer );

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;

        if( i_err < 0 )
        {
            break;
        }
    }

    if( i_err < 0 )
    {
        block_ChainRelease( p_buffer );
    }

    return( i_err < 0 ? VLC_EGENERIC : i_len );
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    (void)i_pos;
    msg_Warn( p_access, "HTTP sout access cannot seek" );
    return VLC_EGENERIC;
}
