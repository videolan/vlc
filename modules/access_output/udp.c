/*****************************************************************************
 * udp.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: udp.c,v 1.2 2003/01/23 15:52:04 sam Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <sys/socket.h>
#endif

#include "network.h"

#define DEFAULT_PORT 1234
#define LATENCY     100000
#define MAX_ERROR    500000
/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static int     Write( sout_instance_t *, sout_buffer_t * );
static int     Seek( sout_instance_t *, off_t  );

static void    ThreadWrite( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("UDP stream ouput") );
    set_capability( "sout access", 100 );
    add_shortcut( "udp" );
    add_shortcut( "rtp" ); // Will work only with ts muxer
    set_callbacks( Open, Close );
vlc_module_end();

typedef struct sout_access_thread_s
{
    VLC_COMMON_MEMBERS

    sout_instance_t *p_sout;

    sout_fifo_t *p_fifo;

    int         i_handle;

} sout_access_thread_t;

typedef struct sout_access_data_s
{
    int                 b_rtpts;  // 1 if add rtp/ts header
    uint16_t            i_sequence_number;
    uint32_t            i_ssrc;

    unsigned int        i_mtu;

    sout_buffer_t       *p_buffer;

    sout_access_thread_t *p_thread;

} sout_access_data_t;

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_access_data_t  *p_access;

    char                *psz_parser;
    char                *psz_dst_addr;
    int                 i_dst_port;

    module_t            *p_network;
    network_socket_t    socket_desc;

    if( !( p_access = malloc( sizeof( sout_access_data_t ) ) ) )
    {
        msg_Err( p_sout, "Not enough memory" );
        return( -1 );
    }

    if( p_sout->psz_access != NULL &&
        !strcmp( p_sout->psz_access, "rtp" ) )
    {
         if( p_sout->psz_mux != NULL &&
            strcmp( p_sout->psz_mux, "ts" ) )
        {
            msg_Err( p_sout, "rtp ouput work only with ts payload" );
            free( p_access );
            return( -1 );
        }
        p_access->b_rtpts = 1;
    }
    else
    {
        p_access->b_rtpts = 0;
    }

    psz_parser = strdup( p_sout->psz_name );

    psz_dst_addr = psz_parser;
    i_dst_port = 0;

    while( *psz_parser && *psz_parser != ':' )
    {
        psz_parser++;
    }
    if( *psz_parser == ':' )
    {
        *psz_parser = '\0';
        psz_parser++;
        i_dst_port = atoi( psz_parser );
    }
    if( i_dst_port <= 0 )
    {
        i_dst_port = DEFAULT_PORT;
    }

    p_access->p_thread = vlc_object_create( p_sout,
                                            sizeof( sout_access_thread_t ) );
    if( !p_access->p_thread )
    {
        msg_Err( p_sout, "out of memory" );
        return( -1 );
    }

    p_access->p_thread->p_sout = p_sout;
    p_access->p_thread->b_die  = 0;
    p_access->p_thread->b_error= 0;
    p_access->p_thread->p_fifo = sout_FifoCreate( p_sout );

    socket_desc.i_type = NETWORK_UDP;
    socket_desc.psz_server_addr = psz_dst_addr;
    socket_desc.i_server_port   = i_dst_port;
    socket_desc.psz_bind_addr   = "";
    socket_desc.i_bind_port     = 0;
    p_access->p_thread->p_private = (void*)&socket_desc;
    if( !( p_network = module_Need( p_access->p_thread, 
                                    "network", "" ) ) )
    {
        msg_Err( p_sout, "failed to open a connection (udp)" );
        return( -1 );
    }
    module_Unneed( p_access->p_thread, p_network );

    p_access->p_thread->i_handle = socket_desc.i_handle;
    p_access->i_mtu     = socket_desc.i_mtu;

    if( vlc_thread_create( p_access->p_thread, "sout write thread",
                           ThreadWrite, VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Err( p_sout, "cannot spawn sout access thread" );
        vlc_object_destroy( p_access->p_thread );
        return( -1 );
    }

    p_access->p_buffer  = NULL;
    p_access->i_sequence_number = 12;   // FIXME should be random, used by rtp
    p_access->i_ssrc            = 4212; // FIXME   "    "    "       "  "   "

    p_sout->i_method        = SOUT_METHOD_NETWORK;
    p_sout->p_access_data   = (void*)p_access;
    p_sout->pf_write        = Write;
    p_sout->pf_seek         = Seek;

    msg_Info( p_sout,
              "Open: addr:`%s' port:`%d'",
              psz_dst_addr, i_dst_port );

    free( psz_dst_addr );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_access_data_t  *p_access = (sout_access_data_t*)p_sout->p_access_data;
    int                 i;

    p_access->p_thread->b_die = 1;
    for( i = 0; i < 10; i++ )
    {
        sout_buffer_t       *p_dummy;

        p_dummy = sout_BufferNew( p_sout, p_access->i_mtu );
        p_dummy->i_dts = 0;
        p_dummy->i_pts = 0;
        p_dummy->i_length = 0;
        sout_FifoPut( p_access->p_thread->p_fifo, p_dummy );
    }
    vlc_thread_join( p_access->p_thread );

    sout_FifoDestroy( p_sout, p_access->p_thread->p_fifo );

    if( p_access->p_buffer )
    {
        sout_BufferDelete( p_sout, p_access->p_buffer );
    }

#if defined( UNDER_CE )
    CloseHandle( (HANDLE)p_access->p_thread->i_handle );
#elif defined( WIN32 )
    closesocket( p_access->p_thread->i_handle );
#else
    close( p_access->p_thread->i_handle );
#endif
    msg_Info( p_sout, "Close" );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static int Write( sout_instance_t *p_sout, sout_buffer_t *p_buffer )
{
    sout_access_data_t  *p_access = (sout_access_data_t*)p_sout->p_access_data;
    unsigned int i_write;

    while( p_buffer )
    {
        sout_buffer_t *p_next;
        if( p_buffer->i_size > p_access->i_mtu )
        {
            msg_Warn( p_sout, "arggggggggggggg packet size > mtu" );
            i_write = p_access->i_mtu;
        }
        else
        {
            i_write = p_buffer->i_size;
        }

        /* if we have enough data, enque the buffer */
        if( p_access->p_buffer &&
            p_access->p_buffer->i_size + i_write > p_access->i_mtu )
        {
            sout_FifoPut( p_access->p_thread->p_fifo, p_access->p_buffer );
            p_access->p_buffer = NULL;
        }

        if( !p_access->p_buffer )
        {
            p_access->p_buffer = sout_BufferNew( p_sout, p_access->i_mtu );
            p_access->p_buffer->i_dts = p_buffer->i_dts;
            p_access->p_buffer->i_size = 0;
            if( p_access->b_rtpts )
            {
                mtime_t i_timestamp = p_access->p_buffer->i_dts * 9 / 100;

                /* add rtp/ts header */
                p_access->p_buffer->p_buffer[0] = 0x80;
                p_access->p_buffer->p_buffer[1] = 0x21; // mpeg2-ts
                p_access->p_buffer->p_buffer[2] =
                    ( p_access->i_sequence_number >> 8 )&0xff;
                p_access->p_buffer->p_buffer[3] =
                    p_access->i_sequence_number&0xff;

                p_access->p_buffer->p_buffer[4] = ( i_timestamp >> 24 )&0xff;
                p_access->p_buffer->p_buffer[5] = ( i_timestamp >> 16 )&0xff;
                p_access->p_buffer->p_buffer[6] = ( i_timestamp >>  8 )&0xff;
                p_access->p_buffer->p_buffer[7] = i_timestamp&0xff;

                p_access->p_buffer->p_buffer[ 8] =
                    ( p_access->i_ssrc >> 24 )&0xff;
                p_access->p_buffer->p_buffer[ 9] =
                    ( p_access->i_ssrc >> 16 )&0xff;
                p_access->p_buffer->p_buffer[10] =
                    ( p_access->i_ssrc >>  8 )&0xff;
                p_access->p_buffer->p_buffer[11] = p_access->i_ssrc&0xff;

                p_access->p_buffer->i_size = 12;
            }
        }


        if( p_buffer->i_size > 0 )
        {
            memcpy( p_access->p_buffer->p_buffer + p_access->p_buffer->i_size,
                    p_buffer->p_buffer,
                    i_write );
            p_access->p_buffer->i_size += i_write;
        }
        p_next = p_buffer->p_next;
        sout_BufferDelete( p_sout, p_buffer );
        p_buffer = p_next;
    }

    return( p_access->p_thread->b_error ? -1 : 0 );
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_instance_t *p_sout, off_t i_pos )
{

    msg_Err( p_sout, "udp sout access cannot seek" );
    return( -1 );
}

/*****************************************************************************
 * ThreadWrite: Write a packet on the network at the good time.
 *****************************************************************************/
static void ThreadWrite( vlc_object_t *p_this )
{
    sout_access_thread_t *p_thread = (sout_access_thread_t*)p_this;
    sout_instance_t      *p_sout = p_thread->p_sout;
    sout_buffer_t *p_buffer;
    mtime_t       i_date;

    while( !p_thread->b_die && p_thread->p_fifo->i_depth < 5 )
    {
        msleep( 10000 );
    }
    if( p_thread->b_die )
    {
        return;
    }
    p_buffer = sout_FifoShow( p_thread->p_fifo );
    i_date = mdate() - p_buffer->i_dts;

    for( ;; )
    {
        mtime_t i_wait;

        p_buffer = sout_FifoGet( p_thread->p_fifo );

        if( p_thread->b_die )
        {
            return;
        }
        i_wait = i_date + p_buffer->i_dts;
        mwait( i_wait );

        if( i_wait - mdate() > MAX_ERROR ||
            i_wait - mdate() < -MAX_ERROR )
        {
            msg_Warn( p_sout, "resetting clock" );
            i_date = mdate() - p_buffer->i_dts;
        }
        send( p_thread->i_handle,
              p_buffer->p_buffer,
              p_buffer->i_size,
              0 );

        sout_BufferDelete( p_sout, p_buffer );
    }
}


