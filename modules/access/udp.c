/*****************************************************************************
 * udp.c: raw UDP & RTP access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: udp.c,v 1.27 2004/01/21 10:22:31 fenrir Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Tristan Leteurtre <tooney@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *
 * Reviewed: 23 October 2003, Jean-Paul Saman <jpsaman@wxs.nl>
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

#include "network.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for udp streams. This " \
    "value should be set in miliseconds units." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("UDP/RTP input") );
    add_category_hint( N_("UDP"), NULL , VLC_TRUE );
    add_integer( "udp-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    set_capability( "access", 0 );
    add_shortcut( "udp" );
    add_shortcut( "udpstream" );
    add_shortcut( "udp4" );
    add_shortcut( "udp6" );
    add_shortcut( "rtp" );
    add_shortcut( "rtp4" );
    add_shortcut( "rtp6" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define RTP_HEADER_LEN 12

static ssize_t Read    ( input_thread_t *, byte_t *, size_t );
static ssize_t RTPRead ( input_thread_t *, byte_t *, size_t );
static ssize_t RTPChoose( input_thread_t *, byte_t *, size_t );

struct access_sys_t
{
    int fd;
};

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t     *p_input = (input_thread_t *)p_this;
    access_sys_t       *p_sys;

    char *              psz_name = strdup(p_input->psz_name);
    char *              psz_parser = psz_name;
    char *              psz_server_addr = "";
    char *              psz_server_port = "";
    char *              psz_bind_addr = "";
    char *              psz_bind_port = "";
    int                 i_bind_port = 0, i_server_port = 0;
    vlc_value_t         val;


    /* First set ipv4/ipv6 */
    var_Create( p_input, "ipv4", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_input, "ipv6", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    if( *p_input->psz_access )
    {
        /* Find out which shortcut was used */
        if( !strncmp( p_input->psz_access, "udp4", 6 ) || !strncmp( p_input->psz_access, "rtp4", 6 ))
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_input, "ipv4", val );

            val.b_bool = VLC_FALSE;
            var_Set( p_input, "ipv6", val );
        }
        else if( !strncmp( p_input->psz_access, "udp6", 6 ) || !strncmp( p_input->psz_access, "rtp6", 6 ) )
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_input, "ipv6", val );

            val.b_bool = VLC_FALSE;
            var_Set( p_input, "ipv4", val );
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
            *psz_parser++ = '\0'; /* Terminate server name */
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
        *psz_parser++ = '\0'; /* Terminate server port or name if necessary */

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
            *psz_parser++ = '\0'; /* Terminate bind address if necessary */
            psz_bind_port = psz_parser;
        }
    }

    i_server_port = strtol( psz_server_port, NULL, 10 );
    if( ( i_bind_port   = strtol( psz_bind_port,   NULL, 10 ) ) == 0 )
    {
        i_bind_port = config_GetInt( p_this, "server-port" );
    }

    if( *psz_server_addr || i_server_port )
    {
        msg_Err( p_input, "this UDP syntax is deprecated; the server argument will be");
        msg_Err( p_input, "ignored (%s:%d). If you wanted to enter a multicast address",
                          psz_server_addr, i_server_port);
        msg_Err( p_input, "or local port, type : %s:@%s:%d",
                          *p_input->psz_access ? p_input->psz_access : "udp",
                          psz_server_addr, i_server_port );

        i_server_port = 0;
        psz_server_addr = "";
    }

    msg_Dbg( p_input, "opening server=%s:%d local=%s:%d",
             psz_server_addr, i_server_port, psz_bind_addr, i_bind_port );

    p_sys = p_input->p_access_data = malloc( sizeof( access_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return VLC_EGENERIC;
    }

    p_sys->fd = net_OpenUDP( p_input, psz_bind_addr, i_bind_port,
                                      psz_server_addr, i_server_port );
    if( p_sys->fd < 0 )
    {
        msg_Err( p_input, "cannot open socket" );
        free( psz_name );
        free( p_sys );
        return VLC_EGENERIC;
    }
    free( psz_name );

    /* FIXME */
    p_input->i_mtu = config_GetInt( p_this, "mtu" );

    /* fill p_input fields */
    p_input->pf_read = RTPChoose;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;
    p_input->pf_seek = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = VLC_FALSE;
    p_input->stream.b_seekable = VLC_FALSE;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update default_pts to a suitable value for udp access */
    var_Create( p_input, "udp-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "udp-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys = p_input->p_access_data;

    msg_Info( p_input, "closing UDP target `%s'", p_input->psz_source );

    net_Close( p_sys->fd );

    free( p_sys );
}

/*****************************************************************************
 * Read: read on a file descriptor, checking b_die periodically
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
    access_sys_t   *p_sys = p_input->p_access_data;

    return net_Read( p_input, p_sys->fd, p_buffer, i_len, VLC_FALSE );
}

/*****************************************************************************
 * RTPRead : read from the network, and parse the RTP header
 *****************************************************************************/
static ssize_t RTPRead( input_thread_t * p_input, byte_t * p_buffer,
                        size_t i_len )
{
    int         i_rtp_version;
    int         i_CSRC_count;
    int         i_payload_type;
    int         i_skip = 0;

    byte_t *    p_tmp_buffer = alloca( p_input->i_mtu );

    /* Get the raw data from the socket.
     * We first assume that RTP header size is the classic RTP_HEADER_LEN. */
    ssize_t i_ret = Read( p_input, p_tmp_buffer, p_input->i_mtu );

    if ( i_ret <= 0 ) return 0; /* i_ret is at least 1 */

    /* Parse the header and make some verifications.
     * See RFC 1889 & RFC 2250. */

    i_rtp_version  = ( p_tmp_buffer[0] & 0xC0 ) >> 6;
    i_CSRC_count   = ( p_tmp_buffer[0] & 0x0F );
    i_payload_type = ( p_tmp_buffer[1] & 0x7F );

    if ( i_rtp_version != 2 )
        msg_Dbg( p_input, "RTP version is %u, should be 2", i_rtp_version );

    if( i_payload_type == 14 )
    {
        i_skip = 4;
    }
    else if( i_payload_type !=  33 && i_payload_type != 32 )
    {
        msg_Dbg( p_input, "unsupported RTP payload type (%u)", i_payload_type );
    }
    i_skip += RTP_HEADER_LEN + 4*i_CSRC_count;

    /* A CSRC extension field is 32 bits in size (4 bytes) */
    if ( i_ret < i_skip )
    {
        /* Packet is not big enough to hold the complete RTP_HEADER with
         * CSRC extensions.
         */
        msg_Warn( p_input, "RTP input trashing %d bytes", i_ret - i_len );
        return 0;
    }

    /* Return the packet without the RTP header. */
    i_ret -= i_skip;

    if ( (size_t)i_ret > i_len )
    {
        /* This should NOT happen. */
        msg_Warn( p_input, "RTP input trashing %d bytes", i_ret - i_len );
        i_ret = i_len;
    }

    p_input->p_vlc->pf_memcpy( p_buffer, &p_tmp_buffer[i_skip], i_ret );

    return i_ret;
}

/*****************************************************************************
 * RTPChoose : read from the network, and decide whether it's UDP or RTP
 *****************************************************************************/
static ssize_t RTPChoose( input_thread_t * p_input, byte_t * p_buffer,
                          size_t i_len )
{
    int         i_rtp_version;
    int         i_CSRC_count;
    int         i_payload_type;

    byte_t *    p_tmp_buffer = alloca( p_input->i_mtu );

    /* Get the raw data from the socket.
     * We first assume that RTP header size is the classic RTP_HEADER_LEN. */
    ssize_t i_ret = Read( p_input, p_tmp_buffer, p_input->i_mtu );

    if ( i_ret <= 0 ) return 0; /* i_ret is at least 1 */

    /* Check that it's not TS. */
    if ( p_tmp_buffer[0] == 0x47 )
    {
        msg_Dbg( p_input, "detected TS over raw UDP" );
        p_input->pf_read = Read;
        p_input->p_vlc->pf_memcpy( p_buffer, p_tmp_buffer, i_ret );
        return i_ret;
    }

    /* Parse the header and make some verifications.
     * See RFC 1889 & RFC 2250. */

    i_rtp_version  = ( p_tmp_buffer[0] & 0xC0 ) >> 6;
    i_CSRC_count   = ( p_tmp_buffer[0] & 0x0F );
    i_payload_type = ( p_tmp_buffer[1] & 0x7F );

    if ( i_rtp_version != 2 )
    {
        msg_Dbg( p_input, "no supported RTP header detected" );
        p_input->pf_read = Read;
        p_input->p_vlc->pf_memcpy( p_buffer, p_tmp_buffer, i_ret );
        return i_ret;
    }

    switch ( i_payload_type )
    {
    case 33:
        msg_Dbg( p_input, "detected TS over RTP" );
        break;

    case 14:
        msg_Dbg( p_input, "detected MPEG audio over RTP" );
        if( !p_input->psz_demux || *p_input->psz_demux == '\0' )
        {
            p_input->psz_demux = "mp3";
        }
        break;

    case 32:
        msg_Dbg( p_input, "detected MPEG video over RTP" );
        break;

    default:
        msg_Dbg( p_input, "no RTP header detected" );
        p_input->pf_read = Read;
        p_input->p_vlc->pf_memcpy( p_buffer, p_tmp_buffer, i_ret );
        return i_ret;
    }

    p_input->pf_read = RTPRead;

    /* A CSRC extension field is 32 bits in size (4 bytes) */
    if( i_ret < RTP_HEADER_LEN + 4*i_CSRC_count )
    {
        /* Packet is not big enough to hold the complete RTP_HEADER with
         * CSRC extensions.
         */
        msg_Warn( p_input, "RTP input trashing %d bytes", i_ret - i_len );
        return 0;
    }

    /* Return the packet without the RTP header. */
    i_ret -= RTP_HEADER_LEN + 4*i_CSRC_count;

    if ( (size_t)i_ret > i_len )
    {
        /* This should NOT happen. */
        msg_Warn( p_input, "RTP input trashing %d bytes", i_ret - i_len );
        i_ret = i_len;
    }

    p_input->p_vlc->pf_memcpy( p_buffer, &p_tmp_buffer[RTP_HEADER_LEN + 4*i_CSRC_count], i_ret );

    return i_ret;
}
