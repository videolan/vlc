/*****************************************************************************
 * http.c
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: http.c,v 1.1 2003/02/23 19:05:22 fenrir Exp $
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

#include "httpd.h"

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
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_access_out_sys_t
{
    httpd_t             *p_httpd;

    /* host */
    httpd_host_t        *p_httpd_host;

    /* stream */
    httpd_stream_t      *p_httpd_stream;
};

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

    if( !( p_sys = p_access->p_sys =
                malloc( sizeof( sout_access_out_sys_t ) ) ) )
    {
        msg_Err( p_access, "Not enough memory" );
        return( VLC_EGENERIC );
    }

    /* *** parse p_access->psz_name to extract bind address, port and file name *** */
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

    p_sys->p_httpd = httpd_Find( VLC_OBJECT(p_access), VLC_TRUE );
    if( !p_sys->p_httpd )
    {
        msg_Err( p_access, "cannot start httpd daemon" );

        free( psz_name );
        free( psz_file_name );
        free( p_access );
        return( VLC_EGENERIC );
    }

    p_sys->p_httpd_host =
        p_sys->p_httpd->pf_register_host( p_sys->p_httpd,
                                          psz_bind_addr, i_bind_port );

    if( !p_sys->p_httpd_host )
    {
        msg_Err( p_access, "cannot listen on %s:%d", psz_bind_addr, i_bind_port );
        httpd_Release( p_sys->p_httpd );

        free( psz_name );
        free( psz_file_name );
        free( p_access );
        return( VLC_EGENERIC );
    }

    p_sys->p_httpd_stream =
        p_sys->p_httpd->pf_register_stream( p_sys->p_httpd,
                                            psz_file_name, "application/x-octet_stream",
                                            NULL, NULL );

    if( !p_sys->p_httpd_stream )
    {
        msg_Err( p_access, "cannot add stream %s", psz_file_name );
        p_sys->p_httpd->pf_unregister_host( p_sys->p_httpd, p_sys->p_httpd_host );
        httpd_Release( p_sys->p_httpd );

        free( psz_name );
        free( psz_file_name );
        free( p_access );

        return( VLC_EGENERIC );
    }

    p_access->pf_write       = Write;
    p_access->pf_seek        = Seek;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys = p_access->p_sys;

    p_sys->p_httpd->pf_unregister_stream( p_sys->p_httpd, p_sys->p_httpd_stream );
    p_sys->p_httpd->pf_unregister_host( p_sys->p_httpd, p_sys->p_httpd_host );

    httpd_Release( p_sys->p_httpd );

    msg_Info( p_access, "Close" );

    free( p_sys );
}

/*****************************************************************************
 * Write:
 *****************************************************************************/
static int Write( sout_access_out_t *p_access, sout_buffer_t *p_buffer )
{
    sout_access_out_sys_t   *p_sys = p_access->p_sys;
    int i_err = 0;

    while( p_buffer )
    {
        sout_buffer_t *p_next;

        i_err = p_sys->p_httpd->pf_send_stream( p_sys->p_httpd, p_sys->p_httpd_stream,
                                                p_buffer->p_buffer, p_buffer->i_size );

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
    msg_Err( p_access, "http sout access cannot seek" );
    return( VLC_EGENERIC );
}

