/*****************************************************************************
 * udp.c: raw UDP access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: udp.c,v 1.7 2002/03/29 00:14:19 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "network.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void input_getfunctions( function_list_t * );
static int  UDPOpen       ( struct input_thread_s * );
static int  UDPSetProgram ( struct input_thread_s * , pgrm_descriptor_t * );  

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP
 
MODULE_INIT_START
    SET_DESCRIPTION( "Raw UDP access plug-in" )
    ADD_CAPABILITY( ACCESS, 0 )
    ADD_SHORTCUT( "udp" )
    ADD_SHORTCUT( "udpstream" )
    ADD_SHORTCUT( "udp4" )
    ADD_SHORTCUT( "udp6" )
MODULE_INIT_STOP
 
MODULE_ACTIVATE_START
    input_getfunctions( &p_module->p_functions->access );
MODULE_ACTIVATE_STOP
 
MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list )
{
#define input p_function_list->functions.access
    input.pf_open             = UDPOpen;
    input.pf_read             = input_FDNetworkRead;
    input.pf_close            = input_FDClose;
    input.pf_set_program      = UDPSetProgram;
    input.pf_set_area         = NULL;
    input.pf_seek             = NULL;
#undef input
}

/*****************************************************************************
 * UDPOpen: open the socket
 *****************************************************************************/
static int UDPOpen( input_thread_t * p_input )
{
    input_socket_t *    p_access_data;
    struct module_s *   p_network;
    char *              psz_network = "";
    char *              psz_name = strdup(p_input->psz_name);
    char *              psz_parser = psz_name;
    char *              psz_server_addr = "";
    char *              psz_server_port = "";
    char *              psz_bind_addr = "";
    char *              psz_bind_port = "";
    int                 i_bind_port = 0, i_server_port = 0;
    network_socket_t    socket_desc;

    if( config_GetIntVariable( "ipv4" ) )
    {
        psz_network = "ipv4";
    }
    if( config_GetIntVariable( "ipv6" ) )
    {
        psz_network = "ipv6";
    }

    if( *p_input->psz_access )
    {
        /* Find out which shortcut was used */
        if( !strncmp( p_input->psz_access, "udp6", 5 ) )
        {
            psz_network = "ipv6";
        }
        else if( !strncmp( p_input->psz_access, "udp4", 5 ) )
        {
            psz_network = "ipv4";
        }
    }

    /* Parse psz_name syntax :
     * [serveraddr[:serverport]][@[bindaddr]:[bindport]] */

    if( *psz_parser && *psz_parser != '@' )
    {
        /* Found server */
        psz_server_addr = psz_parser;

        while( *psz_parser && *psz_parser != ':' && *psz_parser != '@' )
        {
            if( *psz_parser == '[' )
            {
                /* IPv6 address */
                while( *psz_parser && *psz_parser != ']' )
                {
                    psz_parser++;
                }
            }
            psz_parser++;
        }

        if( *psz_parser == ':' )
        {
            /* Found server port */
            *psz_parser = '\0'; /* Terminate server name */
            psz_parser++;
            psz_server_port = psz_parser;

            while( *psz_parser && *psz_parser != '@' )
            {
                psz_parser++;
            }
        }
    }

    if( *psz_parser == '@' )
    {
        /* Found bind address or bind port */
        *psz_parser = '\0'; /* Terminate server port or name if necessary */
        psz_parser++;

        if( *psz_parser && *psz_parser != ':' )
        {
            /* Found bind address */
            psz_bind_addr = psz_parser;

            while( *psz_parser && *psz_parser != ':' )
            {
                if( *psz_parser == '[' )
                {
                    /* IPv6 address */
                    while( *psz_parser && *psz_parser != ']' )
                    {
                        psz_parser++;
                    }
                }
                psz_parser++;
            }
        }

        if( *psz_parser == ':' )
        {
            /* Found bind port */
            *psz_parser = '\0'; /* Terminate bind address if necessary */
            psz_parser++;

            psz_bind_port = psz_parser;
        }
    }

    /* Convert ports format */
    if( *psz_server_port )
    {
        i_server_port = strtol( psz_server_port, &psz_parser, 10 );
        if( *psz_parser )
        {
            intf_ErrMsg( "input error: cannot parse server port near %s",
                         psz_parser );
            free(psz_name);
            return( -1 );
        }
    }

    if( *psz_bind_port )
    {
        i_bind_port = strtol( psz_bind_port, &psz_parser, 10 );
        if( *psz_parser )
        {
            intf_ErrMsg( "input error: cannot parse bind port near %s",
                         psz_parser );
            free(psz_name);
            return( -1 );
        }
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( *psz_server_addr || i_server_port )
    {
        intf_ErrMsg("input warning: this UDP syntax is deprecated ; the server argument will be");
        intf_ErrMsg("ignored (%s:%d). If you wanted to enter a multicast address",
                    psz_server_addr, i_server_port);
        intf_ErrMsg("or local port, type : %s:@%s:%d",
                    *p_input->psz_access ? p_input->psz_access : "udp",
                    psz_server_addr, i_server_port );

        i_server_port = 0;
        psz_server_addr = "";
    }
 
    intf_WarnMsg( 2, "input: opening server=%s:%d local=%s:%d",
                  psz_server_addr, i_server_port, psz_bind_addr, i_bind_port );

    /* Prepare the network_socket_t structure */
    socket_desc.i_type = NETWORK_UDP;
    socket_desc.psz_bind_addr = psz_bind_addr;
    socket_desc.i_bind_port = i_bind_port;
    socket_desc.psz_server_addr = psz_server_addr;
    socket_desc.i_server_port = i_server_port;

    /* Find an appropriate network module */
    p_network = module_Need( MODULE_CAPABILITY_NETWORK, psz_network,
                             &socket_desc );
    free(psz_name);
    if( p_network == NULL )
    {
        return( -1 );
    }
    module_Unneed( p_network );
    
    p_access_data = p_input->p_access_data = malloc( sizeof(input_socket_t) );
    if( p_access_data == NULL )
    {
        intf_ErrMsg( "input error: Out of memory" );
        return( -1 );
    }

    p_access_data->i_handle = socket_desc.i_handle;
    p_input->i_mtu = socket_desc.i_mtu;

    return( 0 );
}

/*****************************************************************************
 * UDPSetProgram: Selects another program
 *****************************************************************************/
int UDPSetProgram( input_thread_t    * p_input,
                   pgrm_descriptor_t * p_new_prg )
{
    int                 i_es_index;

    if ( p_input->stream.p_selected_program )
    {
        for ( i_es_index = 1 ; /* 0 should be the PMT */
                i_es_index < p_input->stream.p_selected_program->
                    i_es_number ;
                i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if ( p_es->p_decoder_fifo )
            {
                input_UnselectES( p_input , p_es );
                p_es->p_pes = NULL; /* FIXME */
            }
#undef p_es
        }
    }

    for (i_es_index = 1 ; i_es_index < p_new_prg->i_es_number ; i_es_index ++ )
    {
#define p_es p_new_prg->pp_es[i_es_index]
        switch( p_es->i_cat )
        {
            case MPEG1_VIDEO_ES:
            case MPEG2_VIDEO_ES:
                if ( p_main->b_video )
                {
                    input_SelectES( p_input , p_es );
                }
                break;
            case MPEG1_AUDIO_ES:
            case MPEG2_AUDIO_ES:
                if ( p_main->b_audio )
                {
                    input_SelectES( p_input , p_es );
                }
                break;
            default:
                input_SelectES( p_input , p_es );
                break;
#undef p_es
        }
    }

    p_input->stream.p_selected_program = p_new_prg;

    return( 0 );
}
