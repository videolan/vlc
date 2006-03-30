/*****************************************************************************
 * udp.c
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
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

#define MAX_EMPTY_BLOCKS 200

#if defined(WIN32) || defined(UNDER_CE)
# define WINSOCK_STRERROR_SIZE 20
static const char *winsock_strerror( char *buf )
{
    snprintf( buf, WINSOCK_STRERROR_SIZE, "Winsock error %d",
              WSAGetLastError( ) );
    buf[WINSOCK_STRERROR_SIZE - 1] = '\0';
    return buf;
}
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-udp-"

#define CACHING_TEXT N_("Caching value (ms)")
#define CACHING_LONGTEXT N_( \
    "Default caching value for outbound UDP streams. This " \
    "value should be set in milliseconds." )

#define TTL_TEXT N_("Time-To-Live (TTL)")
#define TTL_LONGTEXT N_("Time-To-Live of the " \
                        "outgoing stream.")

#define GROUP_TEXT N_("Group packets")
#define GROUP_LONGTEXT N_("Packets can be sent one by one at the right time " \
                          "or by groups. You can choose the number " \
                          "of packets that will be sent at a time. It " \
                          "helps reducing the scheduling load on " \
                          "heavily-loaded systems." )
#define RAW_TEXT N_("Raw write")
#define RAW_LONGTEXT N_("Packets will be sent " \
                       "directly, without trying to fill the MTU (ie, " \
                       "without trying to make the biggest possible packets " \
                       "in order to improve streaming)." )

vlc_module_begin();
    set_description( _("UDP stream output") );
    set_shortname( N_( "UDP" ) );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_ACO );
    add_integer( SOUT_CFG_PREFIX "caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "ttl", 0, NULL,TTL_TEXT, TTL_LONGTEXT,
                                 VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "group", 1, NULL, GROUP_TEXT, GROUP_LONGTEXT,
                                 VLC_TRUE );
    add_suppressed_integer( SOUT_CFG_PREFIX "late" );
    add_bool( SOUT_CFG_PREFIX "raw",  0, NULL, RAW_TEXT, RAW_LONGTEXT,
                                 VLC_TRUE );

    set_capability( "sout access", 100 );
    add_shortcut( "udp" );
    add_shortcut( "rtp" ); // Will work only with ts muxer
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/

static const char *ppsz_sout_options[] = {
    "caching",
    "ttl",
    "group",
    "raw",
    NULL
};

static int  Write   ( sout_access_out_t *, block_t * );
static int  WriteRaw( sout_access_out_t *, block_t * );
static int  Seek    ( sout_access_out_t *, off_t  );

static void ThreadWrite( vlc_object_t * );
static block_t *NewUDPPacket( sout_access_out_t *, mtime_t );

typedef struct sout_access_thread_t
{
    VLC_COMMON_MEMBERS

    sout_instance_t *p_sout;

    block_fifo_t *p_fifo;

    int         i_handle;

    int64_t     i_caching;
    int         i_group;

    block_fifo_t *p_empty_blocks;

} sout_access_thread_t;

struct sout_access_out_sys_t
{
    int                 b_rtpts;  // 1 if add rtp/ts header
    uint16_t            i_sequence_number;
    uint32_t            i_ssrc;

    int                 i_mtu;

    block_t             *p_buffer;

    sout_access_thread_t *p_thread;

    vlc_bool_t          b_mtu_warning;
};

#define DEFAULT_PORT 1234

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys;

    char                *psz_parser;
    char                *psz_dst_addr;
    int                 i_dst_port;

    int                 i_handle;

    vlc_value_t         val;

    sout_CfgParse( p_access, SOUT_CFG_PREFIX,
                   ppsz_sout_options, p_access->p_cfg );

    if( !( p_sys = malloc( sizeof( sout_access_out_sys_t ) ) ) )
    {
        msg_Err( p_access, "not enough memory" );
        return VLC_EGENERIC;
    }
    memset( p_sys, 0, sizeof(sout_access_out_sys_t) );
    p_access->p_sys = p_sys;

    if( p_access->psz_access != NULL &&
        !strcmp( p_access->psz_access, "rtp" ) )
    {
        p_sys->b_rtpts = 1;
    }
    else
    {
        p_sys->b_rtpts = 0;
    }

    psz_parser = strdup( p_access->psz_name );

    psz_dst_addr = psz_parser;
    i_dst_port = 0;

    if ( *psz_parser == '[' )
    {
        while( *psz_parser && *psz_parser != ']' )
        {
            psz_parser++;
        }
    }
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

    p_sys->p_thread =
        vlc_object_create( p_access, sizeof( sout_access_thread_t ) );
    if( !p_sys->p_thread )
    {
        msg_Err( p_access, "out of memory" );
        return VLC_EGENERIC;
    }

    vlc_object_attach( p_sys->p_thread, p_access );
    p_sys->p_thread->p_sout = p_access->p_sout;
    p_sys->p_thread->b_die  = 0;
    p_sys->p_thread->b_error= 0;
    p_sys->p_thread->p_fifo = block_FifoNew( p_access );
    p_sys->p_thread->p_empty_blocks = block_FifoNew( p_access );

    var_Get( p_access, SOUT_CFG_PREFIX "ttl", &val );
    i_handle = net_ConnectUDP( p_this, psz_dst_addr, i_dst_port, val.i_int );
    if( i_handle == -1 )
    {
         msg_Err( p_access, "failed to open a connection (udp)" );
         return VLC_EGENERIC;
    }

    p_sys->p_thread->i_handle = i_handle;
    net_StopRecv( i_handle );

    var_Get( p_access, SOUT_CFG_PREFIX "caching", &val );
    p_sys->p_thread->i_caching = (int64_t)val.i_int * 1000;

    var_Get( p_access, SOUT_CFG_PREFIX "group", &val );
    p_sys->p_thread->i_group = val.i_int;

    p_sys->i_mtu = var_CreateGetInteger( p_this, "mtu" );

    if( vlc_thread_create( p_sys->p_thread, "sout write thread", ThreadWrite,
                           VLC_THREAD_PRIORITY_HIGHEST, VLC_FALSE ) )
    {
        msg_Err( p_access->p_sout, "cannot spawn sout access thread" );
        vlc_object_destroy( p_sys->p_thread );
        return VLC_EGENERIC;
    }

    srand( (uint32_t)mdate());
    p_sys->p_buffer          = NULL;
    p_sys->i_sequence_number = rand()&0xffff;
    p_sys->i_ssrc            = rand()&0xffffffff;

    var_Get( p_access, SOUT_CFG_PREFIX "raw", &val );
    if( val.b_bool )  p_access->pf_write = WriteRaw;
    else p_access->pf_write = Write;

    p_access->pf_seek = Seek;

    msg_Dbg( p_access, "udp access output opened(%s:%d)",
             psz_dst_addr, i_dst_port );

    free( psz_dst_addr );

    /* update p_sout->i_out_pace_nocontrol */
    p_access->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_access_out_t     *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i;

    p_sys->p_thread->b_die = 1;
    for( i = 0; i < 10; i++ )
    {
        block_t *p_dummy = block_New( p_access, p_sys->i_mtu );
        p_dummy->i_dts = 0;
        p_dummy->i_pts = 0;
        p_dummy->i_length = 0;
        memset( p_dummy->p_buffer, 0, p_dummy->i_buffer );
        block_FifoPut( p_sys->p_thread->p_fifo, p_dummy );
    }
    vlc_thread_join( p_sys->p_thread );

    block_FifoRelease( p_sys->p_thread->p_fifo );
    block_FifoRelease( p_sys->p_thread->p_empty_blocks );

    if( p_sys->p_buffer ) block_Release( p_sys->p_buffer );

    net_Close( p_sys->p_thread->i_handle );

    vlc_object_detach( p_sys->p_thread );
    vlc_object_destroy( p_sys->p_thread );
    /* update p_sout->i_out_pace_nocontrol */
    p_access->p_sout->i_out_pace_nocontrol--;

    msg_Dbg( p_access, "udp access output closed" );
    free( p_sys );
}

/*****************************************************************************
 * Write: standard write on a file descriptor.
 *****************************************************************************/
static int Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    while( p_buffer )
    {
        block_t *p_next;
        int i_packets = 0;

        if( !p_sys->b_mtu_warning && p_buffer->i_buffer > p_sys->i_mtu )
        {
            msg_Warn( p_access, "packet size > MTU, you should probably "
                      "increase the MTU" );
            p_sys->b_mtu_warning = VLC_TRUE;
        }

        /* Check if there is enough space in the buffer */
        if( p_sys->p_buffer &&
            p_sys->p_buffer->i_buffer + p_buffer->i_buffer > p_sys->i_mtu )
        {
            if( p_sys->p_buffer->i_dts + p_sys->p_thread->i_caching < mdate() )
            {
                msg_Dbg( p_access, "late packet for udp input (" I64Fd ")",
                         mdate() - p_sys->p_buffer->i_dts
                          - p_sys->p_thread->i_caching );
            }
            block_FifoPut( p_sys->p_thread->p_fifo, p_sys->p_buffer );
            p_sys->p_buffer = NULL;
        }

        while( p_buffer->i_buffer )
        {
            int i_write = __MIN( p_buffer->i_buffer, p_sys->i_mtu );

            i_packets++;

            if( !p_sys->p_buffer )
            {
                p_sys->p_buffer = NewUDPPacket( p_access, p_buffer->i_dts );
                if( !p_sys->p_buffer ) break;
            }

            memcpy( p_sys->p_buffer->p_buffer + p_sys->p_buffer->i_buffer,
                    p_buffer->p_buffer, i_write );

            p_sys->p_buffer->i_buffer += i_write;
            p_buffer->p_buffer += i_write;
            p_buffer->i_buffer -= i_write;
            if ( p_buffer->i_flags & BLOCK_FLAG_CLOCK )
            {
                if ( p_sys->p_buffer->i_flags & BLOCK_FLAG_CLOCK )
                    msg_Warn( p_access, "putting two PCRs at once" );
                p_sys->p_buffer->i_flags |= BLOCK_FLAG_CLOCK;
            }

            if( p_sys->p_buffer->i_buffer == p_sys->i_mtu || i_packets > 1 )
            {
                /* Flush */
                if( p_sys->p_buffer->i_dts + p_sys->p_thread->i_caching
                      < mdate() )
                {
                    msg_Dbg( p_access, "late packet for udp input (" I64Fd ")",
                             mdate() - p_sys->p_buffer->i_dts
                              - p_sys->p_thread->i_caching );
                }
                block_FifoPut( p_sys->p_thread->p_fifo, p_sys->p_buffer );
                p_sys->p_buffer = NULL;
            }
        }

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;
    }

    return( p_sys->p_thread->b_error ? -1 : 0 );
}

/*****************************************************************************
 * WriteRaw: write p_buffer without trying to fill mtu
 *****************************************************************************/
static int WriteRaw( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t   *p_sys = p_access->p_sys;
    block_t *p_buf;

    while ( p_sys->p_thread->p_empty_blocks->i_depth >= MAX_EMPTY_BLOCKS )
    {
        p_buf = block_FifoGet(p_sys->p_thread->p_empty_blocks);
        block_Release( p_buf );
    }

    block_FifoPut( p_sys->p_thread->p_fifo, p_buffer );

    return( p_sys->p_thread->b_error ? -1 : 0 );
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( sout_access_out_t *p_access, off_t i_pos )
{
    msg_Err( p_access, "UDP sout access cannot seek" );
    return -1;
}

/*****************************************************************************
 * NewUDPPacket: allocate a new UDP packet of size p_sys->i_mtu
 *****************************************************************************/
static block_t *NewUDPPacket( sout_access_out_t *p_access, mtime_t i_dts)
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    block_t *p_buffer;

    while ( p_sys->p_thread->p_empty_blocks->i_depth > MAX_EMPTY_BLOCKS )
    {
        p_buffer = block_FifoGet( p_sys->p_thread->p_empty_blocks );
        block_Release( p_buffer );
    }

    if( p_sys->p_thread->p_empty_blocks->i_depth == 0 )
    {
        p_buffer = block_New( p_access->p_sout, p_sys->i_mtu );
    }
    else
    {
        p_buffer = block_FifoGet(p_sys->p_thread->p_empty_blocks );       
        p_buffer->i_flags = 0;
        p_buffer = block_Realloc( p_buffer, 0, p_sys->i_mtu );
    }

    p_buffer->i_dts = i_dts;
    p_buffer->i_buffer = 0;

    if( p_sys->b_rtpts )
    {
        mtime_t i_timestamp = p_buffer->i_dts * 9 / 100;

        /* add rtp/ts header */
        p_buffer->p_buffer[0] = 0x80;
        p_buffer->p_buffer[1] = 0x21; // mpeg2-ts

        p_buffer->p_buffer[2] = ( p_sys->i_sequence_number >> 8 )&0xff;
        p_buffer->p_buffer[3] = p_sys->i_sequence_number&0xff;
        p_sys->i_sequence_number++;

        p_buffer->p_buffer[4] = ( i_timestamp >> 24 )&0xff;
        p_buffer->p_buffer[5] = ( i_timestamp >> 16 )&0xff;
        p_buffer->p_buffer[6] = ( i_timestamp >>  8 )&0xff;
        p_buffer->p_buffer[7] = i_timestamp&0xff;

        p_buffer->p_buffer[ 8] = ( p_sys->i_ssrc >> 24 )&0xff;
        p_buffer->p_buffer[ 9] = ( p_sys->i_ssrc >> 16 )&0xff;
        p_buffer->p_buffer[10] = ( p_sys->i_ssrc >>  8 )&0xff;
        p_buffer->p_buffer[11] = p_sys->i_ssrc&0xff;

        p_buffer->i_buffer = 12;
    }

    return p_buffer;
}

/*****************************************************************************
 * ThreadWrite: Write a packet on the network at the good time.
 *****************************************************************************/
static void ThreadWrite( vlc_object_t *p_this )
{
    sout_access_thread_t *p_thread = (sout_access_thread_t*)p_this;
    mtime_t              i_date_last = -1;
    mtime_t              i_to_send = p_thread->i_group;
    int                  i_dropped_packets = 0;
#if defined(WIN32) || defined(UNDER_CE)
    char strerror_buf[WINSOCK_STRERROR_SIZE];
# define strerror( x ) winsock_strerror( strerror_buf )
#endif

    while( !p_thread->b_die )
    {
        block_t *p_pk;
        mtime_t       i_date, i_sent;
#if 0
        if( (i++ % 1000)==0 ) {
          int i = 0;
          int j = 0;
          block_t *p_tmp = p_thread->p_empty_blocks->p_first;
          while( p_tmp ) { p_tmp = p_tmp->p_next; i++;}
          p_tmp = p_thread->p_fifo->p_first;
          while( p_tmp ) { p_tmp = p_tmp->p_next; j++;}
	  msg_Err( p_thread, "fifo depth: %d/%d, empty blocks: %d/%d",
                   p_thread->p_fifo->i_depth, j,p_thread->p_empty_blocks->i_depth,i );
	}
#endif
        p_pk = block_FifoGet( p_thread->p_fifo );

        i_date = p_thread->i_caching + p_pk->i_dts;
        if( i_date_last > 0 )
        {
            if( i_date - i_date_last > 2000000 )
            {
                if( !i_dropped_packets )
                    msg_Dbg( p_thread, "mmh, hole ("I64Fd" > 2s) -> drop",
                             i_date - i_date_last );

                block_FifoPut( p_thread->p_empty_blocks, p_pk );

                i_date_last = i_date;
                i_dropped_packets++;
                continue;
            }
            else if( i_date - i_date_last < -1000 )
            {
                if( !i_dropped_packets )
                    msg_Dbg( p_thread, "mmh, packets in the past ("I64Fd")",
                             i_date_last - i_date );
            }
        }

        i_to_send--;
        if( !i_to_send || (p_pk->i_flags & BLOCK_FLAG_CLOCK) )
        {
            mwait( i_date );
            i_to_send = p_thread->i_group;
        }
        if( send( p_thread->i_handle, p_pk->p_buffer, p_pk->i_buffer, 0 )
              == -1 )
        {
            msg_Warn( p_thread, "send error: %s", strerror(errno) );
        }

        if( i_dropped_packets )
        {
            msg_Dbg( p_thread, "dropped %i packets", i_dropped_packets );
            i_dropped_packets = 0;
        }

#if 1
        i_sent = mdate();
        if ( i_sent > i_date + 20000 )
        {
            msg_Dbg( p_thread, "packet has been sent too late (" I64Fd ")",
                     i_sent - i_date );
        }
#endif

        block_FifoPut( p_thread->p_empty_blocks, p_pk );

        i_date_last = i_date;
    }
}
