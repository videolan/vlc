/*****************************************************************************
 * access.c: Real rtsp input
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

#include <vlc_network.h>
#include "rtsp.h"
#include "real.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Real RTSP") )
    set_shortname( N_("Real RTSP") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    set_capability( "access", 10 )
    set_callbacks( Open, Close )
    add_shortcut( "realrtsp", "rtsp", "pnm" )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static block_t *BlockRead( access_t * );
static int     Seek( access_t *, uint64_t );
static int     Control( access_t *, int, va_list );

struct access_sys_t
{
    rtsp_client_t *p_rtsp;

    int fd;

    block_t *p_header;
};

/*****************************************************************************
 * Network wrappers
 *****************************************************************************/
static int RtspConnect( void *p_userdata, char *psz_server, int i_port )
{
    access_t *p_access = (access_t *)p_userdata;
    access_sys_t *p_sys = p_access->p_sys;

    /* Open connection */
    p_sys->fd = net_ConnectTCP( p_access, psz_server, i_port );
    if( p_sys->fd < 0 )
    {
        msg_Err( p_access, "cannot connect to %s:%d", psz_server, i_port );
        dialog_Fatal( p_access, _("Connection failed"),
                        _("VLC could not connect to \"%s:%d\"."), psz_server, i_port );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int RtspDisconnect( void *p_userdata )
{
    access_t *p_access = (access_t *)p_userdata;
    access_sys_t *p_sys = p_access->p_sys;

    net_Close( p_sys->fd );
    return VLC_SUCCESS;
}

static int RtspRead( void *p_userdata, uint8_t *p_buffer, int i_buffer )
{
    access_t *p_access = (access_t *)p_userdata;
    access_sys_t *p_sys = p_access->p_sys;

    return net_Read( p_access, p_sys->fd, 0, p_buffer, i_buffer, true );
}

static int RtspReadLine( void *p_userdata, uint8_t *p_buffer, int i_buffer )
{
    access_t *p_access = (access_t *)p_userdata;
    access_sys_t *p_sys = p_access->p_sys;

    char *psz = net_Gets( p_access, p_sys->fd, 0 );

    //fprintf(stderr, "ReadLine: %s\n", psz);

    if( psz ) strncpy( (char *)p_buffer, psz, i_buffer );
    else *p_buffer = 0;

    free( psz );
    return 0;
}

static int RtspWrite( void *p_userdata, uint8_t *p_buffer, int i_buffer )
{
    VLC_UNUSED(i_buffer);
    access_t *p_access = (access_t *)p_userdata;
    access_sys_t *p_sys = p_access->p_sys;

    //fprintf(stderr, "Write: %s", p_buffer);

    net_Printf( p_access, p_sys->fd, 0, "%s", p_buffer );

    return 0;
}

/*****************************************************************************
 * Open: open the rtsp connection
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t *p_access = (access_t *)p_this;
    access_sys_t *p_sys;
    char* psz_server = NULL;
    int i_result;

    if( !p_access->psz_access || (
        strncmp( p_access->psz_access, "rtsp", 4 ) &&
        strncmp( p_access->psz_access, "pnm", 3 )  &&
        strncmp( p_access->psz_access, "realrtsp", 8 ) ))
    {
            return VLC_EGENERIC;
    }

    p_access->pf_read = NULL;
    p_access->pf_block = BlockRead;
    p_access->pf_seek = Seek;
    p_access->pf_control = Control;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = false;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->p_rtsp = malloc( sizeof( rtsp_client_t) );
    if( !p_sys->p_rtsp )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_sys->p_header = NULL;
    p_sys->p_rtsp->p_userdata = p_access;
    p_sys->p_rtsp->pf_connect = RtspConnect;
    p_sys->p_rtsp->pf_disconnect = RtspDisconnect;
    p_sys->p_rtsp->pf_read = RtspRead;
    p_sys->p_rtsp->pf_read_line = RtspReadLine;
    p_sys->p_rtsp->pf_write = RtspWrite;

    i_result = rtsp_connect( p_sys->p_rtsp, p_access->psz_location, 0 );
    if( i_result )
    {
        msg_Dbg( p_access, "could not connect to: %s", p_access->psz_location );
        free( p_sys->p_rtsp );
        p_sys->p_rtsp = NULL;
        goto error;
    }

    msg_Dbg( p_access, "rtsp connected" );

    /* looking for server type */
    if( rtsp_search_answers( p_sys->p_rtsp, "Server" ) )
        psz_server = strdup( rtsp_search_answers( p_sys->p_rtsp, "Server" ) );
    else
    {
        if( rtsp_search_answers( p_sys->p_rtsp, "RealChallenge1" ) )
            psz_server = strdup("Real");
        else
            psz_server = strdup("unknown");
    }

    if( strstr( psz_server, "Real" ) || strstr( psz_server, "Helix" ) )
    {
        uint32_t bandwidth = 10485800;
        rmff_header_t *h;

        msg_Dbg( p_access, "found a real/helix rtsp server" );

        if( !(h = real_setup_and_get_header( p_sys->p_rtsp, bandwidth )) )
        {
            /* Check if we got a redirect */
            if( rtsp_search_answers( p_sys->p_rtsp, "Location" ) )
            {
                msg_Dbg( p_access, "redirect: %s",
                         rtsp_search_answers(p_sys->p_rtsp, "Location") );
                msg_Warn( p_access, "redirect not supported" );
                goto error;
            }


            msg_Err( p_access, "rtsp session can not be established" );
            dialog_Fatal( p_access, _("Session failed"), "%s",
                    _("The requested RTSP session could not be established.") );
            goto error;
        }

        p_sys->p_header = block_Alloc( 4096 );
        p_sys->p_header->i_buffer =
            rmff_dump_header( h, (char *)p_sys->p_header->p_buffer, 1024 );
        rmff_free_header( h );
    }
    else
    {
        msg_Warn( p_access, "only real/helix rtsp servers supported for now" );
        goto error;
    }

    free( psz_server );
    return VLC_SUCCESS;

 error:
    free( psz_server );
    Close( p_this );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->p_rtsp ) rtsp_close( p_sys->p_rtsp );
    free( p_sys->p_rtsp );
    free( p_sys );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static block_t *BlockRead( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    block_t *p_block;
    rmff_pheader_t pheader;
    int i_size;

    if( p_sys->p_header )
    {
        p_block = p_sys->p_header;
        p_sys->p_header = NULL;
        return p_block;
    }

    i_size = real_get_rdt_chunk_header( p_access->p_sys->p_rtsp, &pheader );
    if( i_size <= 0 ) return NULL;

    p_block = block_Alloc( i_size );
    p_block->i_buffer = real_get_rdt_chunk( p_access->p_sys->p_rtsp, &pheader,
                                            &p_block->p_buffer );

    return p_block;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( access_t *p_access, uint64_t i_pos )
{
    VLC_UNUSED(p_access);
    VLC_UNUSED(i_pos);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
            *va_arg( args, bool* ) = false;
            break;

        case ACCESS_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = true;
            break;

        case ACCESS_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = INT64_C(1000)
                * var_InheritInteger(p_access, "network-caching");
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_META:
        case ACCESS_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
