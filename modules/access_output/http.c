/*****************************************************************************
 * http.c
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <string.h>
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "vlc_httpd.h"

#define FREE( p ) if( p ) { free( p); (p) = NULL; }

#define DEFAULT_PORT 8080

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static int     Write( sout_access_out_t *, sout_buffer_t * );
static int     Seek ( sout_access_out_t *, off_t  );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("HTTP stream ouput") );
    set_capability( "sout access", 0 );
    add_shortcut( "http" );
    add_shortcut( "mmsh" );
    set_callbacks( Open, Close );
vlc_module_end();

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
    vlc_bool_t          b_header_complete;
};

#if 0
static struct
{
    char *psz_ext;
    char *psz_mime;
} http_mime[] =
{
    { ".avi",   "video/avi" },
    { ".asf",   "video/x-ms-asf" },
    { ".m1a",   "audio/mpeg" },
    { ".m2a",   "audio/mpeg" },
    { ".m1v",   "video/mpeg" },
    { ".m2v",   "video/mpeg" },
    { ".mp2",   "audio/mpeg" },
    { ".mp3",   "audio/mpeg" },
    { ".mpa",   "audio/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".mpeg",  "video/mpeg" },
    { ".mpe",   "video/mpeg" },
    { ".mov",   "video/quicktime" },
    { ".moov",  "video/quicktime" },
    { ".ogg",   "application/ogg" },
    { ".ogm",   "application/ogg" },
    { ".wma",   "audio/x-ms-wma" },
    { ".wmv",   "video/x-ms-wmv" },
    { ".wav",   "audio/wav" },
    { NULL,     NULL }
};

static char *GetMime( char *psz_name )
{
    char *psz_ext;

    psz_ext = strrchr( psz_name, '.' );
    if( psz_ext )
    {
        int i;

        for( i = 0; http_mime[i].psz_ext != NULL ; i++ )
        {
            if( !strcmp( http_mime[i].psz_ext, psz_ext ) )
            {
                return( http_mime[i].psz_mime );
            }
        }
    }
    return( "application/octet-stream" );
}
#endif

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys;

    char                *psz_parser, *psz_name;

    char                *psz_bind_addr;
    int                 i_bind_port;
    char                *psz_file_name;

    char                *psz_mime = NULL;

    if( !( p_sys = p_access->p_sys =
                malloc( sizeof( sout_access_out_sys_t ) ) ) )
    {
        msg_Err( p_access, "Not enough memory" );
        return( VLC_EGENERIC );
    }

    /* p_access->psz_name host.name:port/filename */
    psz_name = psz_parser = strdup( p_access->psz_name );

    psz_bind_addr = psz_parser;
    i_bind_port = 0;
    psz_file_name = "";

    while( *psz_parser && *psz_parser != ':' && *psz_parser != '/' )
    {
        psz_parser++;
    }
    if( *psz_parser == ':' )
    {
        *psz_parser = '\0';
        psz_parser++;
        i_bind_port = atoi( psz_parser );

        while( *psz_parser && *psz_parser != '/' )
        {
            psz_parser++;
        }
    }
    if( *psz_parser == '/' )
    {
        *psz_parser = '\0';
        psz_parser++;
        psz_file_name = psz_parser;
    }

    if( i_bind_port <= 0 )
    {
        i_bind_port = DEFAULT_PORT;
    }

    if( !*psz_file_name )
    {
        psz_file_name = strdup( "/" );
    }
    else if( *psz_file_name != '/' )
    {
        char *p = psz_file_name;

        psz_file_name = malloc( strlen( p ) + 2 );
        strcpy( psz_file_name, "/" );
        strcat( psz_file_name, p );
    }
    else
    {
        psz_file_name = strdup( psz_file_name );
    }

    p_sys->p_httpd_host = httpd_HostNew( VLC_OBJECT(p_access), psz_bind_addr,
                                         i_bind_port );
    if( p_sys->p_httpd_host == NULL )
    {
        msg_Err( p_access, "cannot listen on %s:%d",
                 psz_bind_addr, i_bind_port );

        free( psz_name );
        free( psz_file_name );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_access->psz_access && !strcmp( p_access->psz_access, "mmsh" ) )
    {
        psz_mime = "video/x-ms-asf-stream";
    }

    p_sys->p_httpd_stream =
        httpd_StreamNew( p_sys->p_httpd_host, psz_file_name, psz_mime,
                         sout_cfg_find_value( p_access->p_cfg, "user" ),
                         sout_cfg_find_value( p_access->p_cfg, "pwd" ) );
    if( p_sys->p_httpd_stream == NULL )
    {
        msg_Err( p_access, "cannot add stream %s", psz_file_name );
        httpd_HostDelete( p_sys->p_httpd_host );

        free( psz_name );
        free( psz_file_name );
        free( p_sys );
        return VLC_EGENERIC;
    }

    free( psz_file_name );
    free( psz_name );

    p_sys->i_header_allocated = 1024;
    p_sys->i_header_size      = 0;
    p_sys->p_header           = malloc( p_sys->i_header_allocated );
    p_sys->b_header_complete  = VLC_FALSE;

    p_access->pf_write       = Write;
    p_access->pf_seek        = Seek;


    /* update p_sout->i_out_pace_nocontrol */
    p_access->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys = p_access->p_sys;

    /* update p_sout->i_out_pace_nocontrol */
    p_access->p_sout->i_out_pace_nocontrol--;

    httpd_StreamDelete( p_sys->p_httpd_stream );
    httpd_HostDelete( p_sys->p_httpd_host );

    FREE( p_sys->p_header );

    msg_Dbg( p_access, "Close" );

    free( p_sys );
}

/*****************************************************************************
 * Write:
 *****************************************************************************/
static int Write( sout_access_out_t *p_access, sout_buffer_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i_err = 0;

    while( p_buffer )
    {
        sout_buffer_t *p_next;

        if( p_buffer->i_flags & SOUT_BUFFER_FLAGS_HEADER )
        {
            /* gather header */
            if( p_sys->b_header_complete )
            {
                /* free previously gathered header */
                p_sys->i_header_size = 0;
                p_sys->b_header_complete = VLC_FALSE;
            }
            if( (int)(p_buffer->i_size + p_sys->i_header_size) >
                p_sys->i_header_allocated )
            {
                p_sys->i_header_allocated =
                    p_buffer->i_size + p_sys->i_header_size + 1024;
                p_sys->p_header =
                    realloc( p_sys->p_header, p_sys->i_header_allocated );
            }
            memcpy( &p_sys->p_header[p_sys->i_header_size],
                    p_buffer->p_buffer,
                    p_buffer->i_size );
            p_sys->i_header_size += p_buffer->i_size;
        }
        else if( !p_sys->b_header_complete )
        {
            p_sys->b_header_complete = VLC_TRUE;

            httpd_StreamHeader( p_sys->p_httpd_stream, p_sys->p_header,
                                p_sys->i_header_size );
        }

        /* send data */
        i_err = httpd_StreamSend( p_sys->p_httpd_stream, p_buffer->p_buffer,
                                  p_buffer->i_size );

        p_next = p_buffer->p_next;
        sout_BufferDelete( p_access->p_sout, p_buffer );
        p_buffer = p_next;

        if( i_err < 0 )
        {
            break;
        }
    }

    if( i_err < 0 )
    {
        sout_buffer_t *p_next;
        while( p_buffer )
        {
            p_next = p_buffer->p_next;
            sout_BufferDelete( p_access->p_sout, p_buffer );
            p_buffer = p_next;
        }
    }

    return( i_err < 0 ? VLC_EGENERIC : VLC_SUCCESS );
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    msg_Warn( p_access, "HTTP sout access cannot seek" );
    return VLC_EGENERIC;
}
