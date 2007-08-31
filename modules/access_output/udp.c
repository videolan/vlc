/*****************************************************************************
 * udp.c
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
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
#include <vlc/vlc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include <vlc_sout.h>
#include <vlc_block.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <sys/socket.h>
#endif

#include <vlc_network.h>

#if defined (HAVE_NETINET_UDPLITE_H)
# include <netinet/udplite.h>
#elif defined (__linux__)
# define UDPLITE_SEND_CSCOV     10
# define UDPLITE_RECV_CSCOV     11
#endif

#ifndef IPPROTO_UDPLITE
# define IPPROTO_UDPLITE 136 /* from IANA */
#endif
#ifndef SOL_UDPLITE
# define SOL_UDPLITE IPPROTO_UDPLITE
#endif

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
#define RTCP_TEXT N_("RTCP Sender Report")
#define RTCP_LONGTEXT N_("Send RTCP Sender Report packets")
#define RTCP_PORT_TEXT N_("RTCP destination port number")
#define RTCP_PORT_LONGTEXT N_("Sends RTCP packets to this port (0 = auto)")
#define AUTO_MCAST_TEXT N_("Automatic multicast streaming")
#define AUTO_MCAST_LONGTEXT N_("Allocates an outbound multicast address " \
                               "automatically.")
#define UDPLITE_TEXT N_("UDP-Lite")
#define UDPLITE_LONGTEXT N_("Use UDP-Lite/IP instead of normal UDP/IP")
#define CSCOV_TEXT N_("Checksum coverage")
#define CSCOV_LONGTEXT N_("Payload bytes covered by layer-4 checksum")

vlc_module_begin();
    set_description( _("UDP stream output") );
    set_shortname( "UDP" );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_ACO );
    add_integer( SOUT_CFG_PREFIX "caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "group", 1, NULL, GROUP_TEXT, GROUP_LONGTEXT,
                                 VLC_TRUE );
    add_obsolete_integer( SOUT_CFG_PREFIX "late" );
    add_bool( SOUT_CFG_PREFIX "raw",  VLC_FALSE, NULL, RAW_TEXT, RAW_LONGTEXT,
                                 VLC_TRUE );
    add_bool( SOUT_CFG_PREFIX "rtcp",  VLC_FALSE, NULL, RAW_TEXT, RAW_LONGTEXT,
                                 VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "rtcp-port",  0, NULL, RTCP_PORT_TEXT,
                 RTCP_PORT_LONGTEXT, VLC_TRUE );
    add_bool( SOUT_CFG_PREFIX "auto-mcast", VLC_FALSE, NULL, AUTO_MCAST_TEXT,
              AUTO_MCAST_LONGTEXT, VLC_TRUE );
    add_bool( SOUT_CFG_PREFIX "udplite", VLC_FALSE, NULL, UDPLITE_TEXT, UDPLITE_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "cscov", 12, NULL, CSCOV_TEXT, CSCOV_LONGTEXT, VLC_TRUE );

    set_capability( "sout access", 100 );
    add_shortcut( "udp" );
    add_shortcut( "rtp" ); // Will work only with ts muxer
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/

static const char *const ppsz_sout_options[] = {
    "auto-mcast",
    "caching",
    "group",
    "raw",
    "rtcp",
    "rtcp-port",
    "lite",
    "cscov",
    NULL
};

/* Options handled by the libvlc network core */
static const char *const ppsz_core_options[] = {
    "dscp",
    "ttl",
    "miface",
    "miface-addr",
    NULL
};

static int  Write   ( sout_access_out_t *, block_t * );
static int  WriteRaw( sout_access_out_t *, block_t * );
static int  Seek    ( sout_access_out_t *, off_t  );

static void ThreadWrite( vlc_object_t * );
static block_t *NewUDPPacket( sout_access_out_t *, mtime_t );
static const char *MakeRandMulticast (int family, char *buf, size_t buflen);

typedef struct rtcp_sender_t
{
    size_t   length;  /* RTCP packet length */
    uint8_t  payload[28 + 8 + (2 * 257)];
    int      handle;  /* RTCP socket handler */

    uint32_t packets; /* RTP packets sent */
    uint32_t bytes;   /* RTP bytes sent */
    unsigned counter; /* RTP packets sent since last RTCP packet */
} rtcp_sender_t;

typedef struct sout_access_thread_t
{
    VLC_COMMON_MEMBERS

    sout_instance_t *p_sout;

    block_fifo_t *p_fifo;

    int         i_handle;

    int64_t     i_caching;
    int         i_group;

    block_fifo_t *p_empty_blocks;

    rtcp_sender_t rtcp;
} sout_access_thread_t;

struct sout_access_out_sys_t
{
    int                 i_mtu;

    vlc_bool_t          b_rtpts; // true for RTP/MP2 encapsulation
    vlc_bool_t          b_mtu_warning;
    uint16_t            i_sequence_number;
    uint32_t            i_ssrc;

    block_t             *p_buffer;

    sout_access_thread_t *p_thread;

};

#define DEFAULT_PORT 1234
#define RTP_HEADER_LENGTH 12

static int OpenRTCP (vlc_object_t *obj, rtcp_sender_t *rtcp, int rtp_fd,
                     int proto, uint16_t dport);
static void SendRTCP (rtcp_sender_t *obj, const block_t *rtp);
static void CloseRTCP (rtcp_sender_t *obj);

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys;

    char                *psz_dst_addr = NULL;
    int                 i_dst_port, i_rtcp_port = 0, proto = IPPROTO_UDP;
    const char          *protoname = "UDP";

    int                 i_handle;

    config_ChainParse( p_access, SOUT_CFG_PREFIX,
                       ppsz_sout_options, p_access->p_cfg );
    config_ChainParse( p_access, "",
                       ppsz_core_options, p_access->p_cfg );

    if (var_Create (p_access, "dst-port", VLC_VAR_INTEGER)
     || var_Create (p_access, "src-port", VLC_VAR_INTEGER)
     || var_Create (p_access, "dst-addr", VLC_VAR_STRING)
     || var_Create (p_access, "src-addr", VLC_VAR_STRING))
    {
        return VLC_ENOMEM;
    }

    if( !( p_sys = calloc ( 1, sizeof( sout_access_out_sys_t ) ) ) )
    {
        msg_Err( p_access, "not enough memory" );
        return VLC_ENOMEM;
    }
    p_access->p_sys = p_sys;

    if( p_access->psz_access != NULL )
    {
        if (strcmp (p_access->psz_access, "rtp") == 0)
            p_sys->b_rtpts = VLC_TRUE;
    }

    if (var_GetBool (p_access, SOUT_CFG_PREFIX"lite"))
    {
        protoname = "UDP-Lite";
        proto = IPPROTO_UDPLITE;
    }

    i_dst_port = DEFAULT_PORT;
    if (var_GetBool (p_access, SOUT_CFG_PREFIX"auto-mcast"))
    {
        char buf[INET6_ADDRSTRLEN];
        if (MakeRandMulticast (AF_INET, buf, sizeof (buf)) != NULL)
            psz_dst_addr = strdup (buf);
    }
    else
    {
        char *psz_parser = psz_dst_addr = strdup( p_access->psz_path );

        if (psz_parser[0] == '[')
            psz_parser = strchr (psz_parser, ']');

        psz_parser = strchr (psz_parser ?: psz_dst_addr, ':');
        if (psz_parser != NULL)
        {
            *psz_parser++ = '\0';
            i_dst_port = atoi (psz_parser);
        }
    }

    if (var_GetBool (p_access, SOUT_CFG_PREFIX"rtcp"))
    {
        /* This is really only for the RTP sout plugin.
         * Doing RTCP for non RTP packet is NOT a good idea. */
        i_rtcp_port = var_GetInteger (p_access, SOUT_CFG_PREFIX"rtcp-port");
        if (i_rtcp_port == 0)
            i_rtcp_port = i_dst_port + 1;
    }

    p_sys->p_thread =
        vlc_object_create( p_access, sizeof( sout_access_thread_t ) );
    if( !p_sys->p_thread )
    {
        msg_Err( p_access, "out of memory" );
        free (p_sys);
        free (psz_dst_addr);
        return VLC_ENOMEM;
    }

    vlc_object_attach( p_sys->p_thread, p_access );
    p_sys->p_thread->p_sout = p_access->p_sout;
    p_sys->p_thread->b_die  = 0;
    p_sys->p_thread->b_error= 0;
    p_sys->p_thread->p_fifo = block_FifoNew( p_access );
    p_sys->p_thread->p_empty_blocks = block_FifoNew( p_access );

    i_handle = net_ConnectDgram( p_this, psz_dst_addr, i_dst_port, -1, proto );
    free (psz_dst_addr);

    if( i_handle == -1 )
    {
         msg_Err( p_access, "failed to create %s socket", protoname );
         vlc_object_destroy (p_sys->p_thread);
         free (p_sys);
         return VLC_EGENERIC;
    }
    else
    {
        char addr[NI_MAXNUMERICHOST];
        int port;

        if (net_GetSockAddress (i_handle, addr, &port) == 0)
        {
            msg_Dbg (p_access, "source: %s port %d", addr, port);
            var_SetString (p_access, "src-addr", addr);
            var_SetInteger (p_access, "src-port", port);
        }

        if (net_GetPeerAddress (i_handle, addr, &port) == 0)
        {
            msg_Dbg (p_access, "destination: %s port %d", addr, port);
            var_SetString (p_access, "dst-addr", addr);
            var_SetInteger (p_access, "dst-port", port);
        }
    }
    p_sys->p_thread->i_handle = i_handle;
    shutdown( i_handle, SHUT_RD );

    int cscov = var_GetInteger (p_access, SOUT_CFG_PREFIX"cscov");
    if (cscov)
    {
        switch (proto)
        {
#ifdef UDPLITE_SEND_CSCOV
            case IPPROTO_UDPLITE:
                cscov += 8;
                setsockopt (i_handle, SOL_UDPLITE, UDPLITE_SEND_CSCOV,
                            &(int){ cscov }, sizeof (cscov));
                break;
#endif
#ifdef DCCP_SOCKOPT_RECV_CSCOV
            /* FIXME: ^^is this the right name ? */
            /* FIXME: is this inherited by accept() ? */
            case IPPROTO_DCCP:
                cscov = ((cscov + 3) >> 2) + 1;
                if (cscov > 15)
                    break; /* out of DCCP cscov range */
                setsockopt (i_handle, SOL_DCCP, DCCP_SOCKOPT_RECV_CSCOV,
                            &(int){ cscov }, sizeof (cscov));
                break;
#endif
        }
    }

    p_sys->p_thread->i_caching =
        (int64_t)1000 * var_GetInteger( p_access, SOUT_CFG_PREFIX "caching");
    p_sys->p_thread->i_group =
        var_GetInteger( p_access, SOUT_CFG_PREFIX "group" );

    p_sys->i_mtu = var_CreateGetInteger( p_this, "mtu" );
    if( p_sys->b_rtpts && ( p_sys->i_mtu < RTP_HEADER_LENGTH ) )
        p_sys->i_mtu = 576 - 20 - 8;

    srand( (uint32_t)mdate());
    p_sys->p_buffer          = NULL;
    p_sys->i_sequence_number = rand()&0xffff;
    p_sys->i_ssrc            = rand()&0xffffffff;

    if (i_rtcp_port && OpenRTCP (VLC_OBJECT (p_access), &p_sys->p_thread->rtcp,
                                 i_handle, proto, i_rtcp_port))
    {
        msg_Err (p_access, "cannot initialize RTCP sender");
        net_Close (i_handle);
        vlc_object_destroy (p_sys->p_thread);
        free (p_sys);
        return VLC_EGENERIC;
    }

    if( vlc_thread_create( p_sys->p_thread, "sout write thread", ThreadWrite,
                           VLC_THREAD_PRIORITY_HIGHEST, VLC_FALSE ) )
    {
        msg_Err( p_access->p_sout, "cannot spawn sout access thread" );
        net_Close (i_handle);
        vlc_object_destroy( p_sys->p_thread );
        free (p_sys);
        return VLC_EGENERIC;
    }

    if (var_GetBool (p_access, SOUT_CFG_PREFIX"raw"))
        p_access->pf_write = WriteRaw;
    else
        p_access->pf_write = Write;

    p_access->pf_seek = Seek;

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

    vlc_object_kill( p_sys->p_thread );
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
    CloseRTCP (&p_sys->p_thread->rtcp);

    vlc_object_detach( p_sys->p_thread );
    vlc_object_destroy( p_sys->p_thread );
    /* update p_sout->i_out_pace_nocontrol */
    p_access->p_sout->i_out_pace_nocontrol--;

    msg_Dbg( p_access, "UDP access output closed" );
    free( p_sys );
}

/*****************************************************************************
 * Write: standard write on a file descriptor.
 *****************************************************************************/
static int Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i_len = 0;

    while( p_buffer )
    {
        block_t *p_next;
        int i_packets = 0;
        mtime_t now = mdate();

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
            if( p_sys->p_buffer->i_dts + p_sys->p_thread->i_caching < now )
            {
                msg_Dbg( p_access, "late packet for UDP input (" I64Fd ")",
                         now - p_sys->p_buffer->i_dts
                          - p_sys->p_thread->i_caching );
            }
            block_FifoPut( p_sys->p_thread->p_fifo, p_sys->p_buffer );
            p_sys->p_buffer = NULL;
        }

        i_len += p_buffer->i_buffer;
        while( p_buffer->i_buffer )
        {
            int i_payload_size = p_sys->i_mtu;
            if( p_sys->b_rtpts )
                i_payload_size -= RTP_HEADER_LENGTH;

            int i_write = __MIN( p_buffer->i_buffer, i_payload_size );

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
                if( p_sys->p_buffer->i_dts + p_sys->p_thread->i_caching < now )
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

    return( p_sys->p_thread->b_error ? -1 : i_len );
}

/*****************************************************************************
 * WriteRaw: write p_buffer without trying to fill mtu
 *****************************************************************************/
static int WriteRaw( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t   *p_sys = p_access->p_sys;
    block_t *p_buf;
    int i_len;

    while ( p_sys->p_thread->p_empty_blocks->i_depth >= MAX_EMPTY_BLOCKS )
    {
        p_buf = block_FifoGet(p_sys->p_thread->p_empty_blocks);
        block_Release( p_buf );
    }

    i_len = p_buffer->i_buffer;
    block_FifoPut( p_sys->p_thread->p_fifo, p_buffer );

    return( p_sys->p_thread->b_error ? -1 : i_len );
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
        p_buffer->p_buffer[1] = 33; // mpeg2-ts

        SetWBE( p_buffer->p_buffer + 2, p_sys->i_sequence_number );
        p_sys->i_sequence_number++;
        SetDWBE( p_buffer->p_buffer + 4, i_timestamp );
        SetDWBE( p_buffer->p_buffer + 8, p_sys->i_ssrc );

        p_buffer->i_buffer = RTP_HEADER_LENGTH;
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
          msg_Dbg( p_thread, "fifo depth: %d/%d, empty blocks: %d/%d",
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
        ssize_t val = send( p_thread->i_handle, p_pk->p_buffer,
                            p_pk->i_buffer, 0 );
        if (val == -1)
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

        SendRTCP (&p_thread->rtcp, p_pk);

        block_FifoPut( p_thread->p_empty_blocks, p_pk );

        i_date_last = i_date;
    }
}


static const char *MakeRandMulticast (int family, char *buf, size_t buflen)
{
    uint32_t rand = (getpid() & 0xffff)
                  | (uint32_t)(((mdate () >> 10) & 0xffff) << 16);

    switch (family)
    {
#ifdef AF_INET6
        case AF_INET6:
        {
            struct in6_addr addr;
            memcpy (&addr, "\xff\x38\x00\x00" "\x00\x00\x00\x00"
                           "\x00\x00\x00\x00", 12);
            rand |= 0x80000000;
            memcpy (addr.s6_addr + 12, &(uint32_t){ htonl (rand) }, 4);
            return inet_ntop (family, &addr, buf, buflen);
        }
#endif

        case AF_INET:
        {
            struct in_addr addr;
            addr.s_addr = htonl ((rand & 0xffffff) | 0xe8000000);
            return inet_ntop (family, &addr, buf, buflen);
        }
    }
#ifdef EAFNOSUPPORT
    errno = EAFNOSUPPORT;
#endif
    return NULL;
}


/*
 * NOTE on RTCP implementation:
 * - there is a single sender (us), no conferencing here! => n = sender = 1,
 * - as such we need not bother to include Receiver Reports,
 * - in unicast case, there is a single receiver => members = 1 + 1 = 2,
 *   and obviously n > 25% of members,
 * - in multicast case, we do not want to maintain the number of receivers
 *   and we assume it is big (i.e. than 3) because that's what broadcasting is
 *   all about,
 * - it is assumed we_sent = true (could be wrong), since we are THE sender,
 * - we always send SR + SDES, while running,
 * - FIXME: we do not implement separate rate limiting for SDES,
 * - we do not implement any profile-specific extensions for the time being.
 */
static int OpenRTCP (vlc_object_t *obj, rtcp_sender_t *rtcp, int rtp_fd,
                     int proto, uint16_t dport)
{
    uint8_t *ptr;
    int fd;

    char src[NI_MAXNUMERICHOST], dst[NI_MAXNUMERICHOST];
    int sport;

    rtcp->bytes = rtcp->packets = rtcp->counter = 0;

    if (net_GetSockAddress (rtp_fd, src, &sport)
     || net_GetPeerAddress (rtp_fd, dst, NULL))
        return VLC_EGENERIC;

    sport++;
    fd = net_OpenDgram (obj, src, sport, dst, dport, AF_UNSPEC, proto);
    if (fd == -1)
        return VLC_EGENERIC;

    rtcp->handle = fd;

    ptr = (uint8_t *)strchr (src, '%');
    if (ptr != NULL)
        *ptr = '\0'; /* remove scope ID frop IPv6 addresses */

    ptr = rtcp->payload;

    /* Sender report */
    ptr[0] = 2 << 6; /* V = 2, P = RC = 0 */
    ptr[1] = 200; /* payload type: Sender Report */
    SetWBE (ptr + 2, 6); /* length = 6 (7 double words) */
    memset (ptr + 4, 0, 4); /* SSRC unknown yet */
    SetQWBE (ptr + 8, NTPtime64 ());
    memset (ptr + 16, 0, 12); /* timestamp and counters */
    ptr += 28;

    /* Source description */
    uint8_t *sdes = ptr;
    ptr[0] = (2 << 6) | 1; /* V = 2, P = 0, SC = 1 */
    ptr[1] = 202; /* payload type: Source Description */
    uint8_t *lenptr = ptr + 2;
    memset (ptr + 4, 0, 4); /* SSRC unknown yet */
    ptr += 8;

    ptr[0] = 1; /* CNAME - mandatory */
    assert (NI_MAXNUMERICHOST <= 256);
    ptr[1] = strlen (src);
    memcpy (ptr + 2, src, ptr[1]);
    ptr += ptr[1] + 2;

    static const char tool[] = PACKAGE_STRING;
    ptr[0] = 6; /* TOOL */
    ptr[1] = (sizeof (tool) > 256) ? 255 : (sizeof (tool) - 1);
    memcpy (ptr + 2, tool, ptr[1]);
    ptr += ptr[1] + 2;

    while ((ptr - sdes) & 3) /* 32-bits padding */
        *ptr++ = 0;
    SetWBE (lenptr, ptr - sdes);

    rtcp->length = ptr - rtcp->payload;
    return VLC_SUCCESS;
}

static void CloseRTCP (rtcp_sender_t *rtcp)
{
    if (rtcp->handle == -1)
        return;

    uint8_t *ptr = rtcp->payload;
    /* Bye */
    ptr[0] = (2 << 6) | 1; /* V = 2, P = 0, SC = 1 */
    ptr[1] = 203; /* payload type: Bye */
    SetWBE (ptr + 2, 1);
    /* SSRC is already there :) */

    /* We are THE sender, so we are more important than anybody else, so
     * we can afford not to check bandwidth constraints here. */
    send (rtcp->handle, rtcp->payload, 8, 0);
    net_Close (rtcp->handle);
}

static void SendRTCP (rtcp_sender_t *rtcp, const block_t *rtp)
{
    uint8_t *ptr = rtcp->payload;

    if ((rtcp->handle == -1) /* RTCP sender off */
     || (rtp->i_buffer < 12)) /* too short RTP packet */
        return;

    /* Updates statistics */
    rtcp->packets++;
    rtcp->bytes += rtp->i_buffer;
    rtcp->counter += rtp->i_buffer;

    /* 1.25% rate limit */
    if ((rtcp->counter / 80) < rtcp->length)
        return;

    uint32_t last = GetDWBE (ptr + 8); // last RTCP SR send time
    uint64_t now64 = NTPtime64 ();
    if ((now64 >> 32) < (last + 5))
        return; // no more than one SR every 5 seconds

    memcpy (ptr + 4, rtp->p_buffer + 8, 4); /* SR SSRC */
    SetQWBE (ptr + 8, now64);
    memcpy (ptr + 16, rtp->p_buffer + 4, 4); /* RTP timestamp */
    SetDWBE (ptr + 20, rtcp->packets);
    SetDWBE (ptr + 24, rtcp->bytes);
    memcpy (ptr + 28 + 4, rtp->p_buffer + 8, 4); /* SDES SSRC */

    if (send (rtcp->handle, ptr, rtcp->length, 0) == (ssize_t)rtcp->length)
        rtcp->counter = 0;
}
