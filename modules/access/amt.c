/**
 * @file amt.c
 * @brief Automatic Multicast Tunneling Protocol (AMT) file for VLC media player
 * Allows multicast streaming when not in a multicast-enabled network
 * Currently IPv4 is supported, but IPv6 is not yet.
 *
 * Copyright (C) 2018 VLC authors and VideoLAN
 * Copyright (c) Juniper Networks, Inc., 2018. All rights reserved.
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>           - original UDP code
 *          Tristan Leteurtre <tooney@via.ecp.fr>             - original UDP code
 *          Laurent Aimar <fenrir@via.ecp.fr>                 - original UDP code
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>       - original UDP code
 *          Remi Denis-Courmont                               - original UDP code
 *          Natalie Landsberg <natalie.landsberg97@gmail.com> - AMT support
 *          Wayne Brassem <wbrassem@rogers.com>               - Added FQDN support
 *
 * This code is licensed to you under the GNU Lesser General Public License
 * version 2.1 or later. You may not use this code except in compliance with
 * the GNU Lesser General Public License.
 * This code is not an official Juniper product.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <ctype.h>
#include <assert.h>
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_network.h>
#include <vlc_block.h>
#include <vlc_interrupt.h>
#include <vlc_url.h>

#ifdef HAVE_POLL_H
 #include <poll.h>
#endif
#ifdef HAVE_SYS_UIO_H
 #include <sys/uio.h>
#endif

#define BUFFER_TEXT N_("Receive buffer")
#define BUFFER_LONGTEXT N_("AMT receive buffer size (bytes)" )
#define TIMEOUT_TEXT N_("Native multicast timeout (sec)")
#define AMT_TIMEOUT_TEXT N_("AMT timeout (sec)")
#define AMT_RELAY_ADDRESS N_("AMT relay (IP address or FQDN)")
#define AMT_RELAY_ADDR_LONG N_("AMT relay anycast address, or specify the relay you want by address or fully qualified domain name")
#define AMT_DEFAULT_RELAY N_("amt-relay.m2icast.net")

/*****************************************************************************
 * Various Lengths of Msgs or Hdrs
 *****************************************************************************/
#define MAC_LEN 6                /* length of generated MAC in bytes */
#define NONCE_LEN 4              /* length of nonce in bytes */

#define MSG_TYPE_LEN 1           /* length of msg type */
#define RELAY_QUERY_MSG_LEN 48   /* total length of relay query */
#define RELAY_ADV_MSG_LEN 12     /* length of relay advertisement message */
#define IGMP_QUERY_LEN 24        /* length of encapsulated IGMP query message */
#define IGMP_REPORT_LEN 20
#define AMT_HDR_LEN 2            /* length of AMT header on a packet */
#define IP_HDR_LEN 20            /* length of standard IP header */
#define IP_HDR_IGMP_LEN 24       /* length of IP header with an IGMP report */
#define UDP_HDR_LEN 8            /* length of standard UDP header */
#define AMT_REQUEST_MSG_LEN 9
#define AMT_DISCO_MSG_LEN 8

/*****************************************************************************
 * Different AMT Message Types
 *****************************************************************************/
#define AMT_RELAY_DISCO 1       /* relay discovery */
#define AMT_RELAY_ADV 2         /* relay advertisement */
#define AMT_REQUEST 3           /* request */
#define AMT_MEM_QUERY 4         /* membership query */
#define AMT_MEM_UPD 5           /* membership update */
#define AMT_MULT_DATA 6         /* multicast data */
#define AMT_TEARDOWN 7          /* teardown (not currently supported) */

/*****************************************************************************
 * Different IGMP Message Types
 *****************************************************************************/
#define AMT_IGMPV3_MEMBERSHIP_QUERY_TYPEID 0x11
#define AMT_IGMPV3_MEMBERSHIP_REPORT_TYPEID 0x22
/* IGMPv2, interoperability  */
#define AMT_IGMPV1_MEMBERSHIP_REPORT_TYPEID 0x12
#define AMT_IGMPV2_MEMBERSHIP_REPORT_TYPEID 0x16
#define AMT_IGMPV2_MEMBERSHIP_LEAVE_TYPEID 0x17

#define AMT_IGMP_INCLUDE 0x01
#define AMT_IGMP_EXCLUDE 0x02
#define AMT_IGMP_INCLUDE_CHANGE 0x03
#define AMT_IGMP_EXCLUDE_CHANGE 0x04
#define AMT_IGMP_ALLOW 0x05
#define AMT_IGMP_BLOCK 0x06

#define MCAST_ANYCAST  "0.0.0.0"
#define MCAST_ALLHOSTS "224.0.0.22"
#define LOCAL_LOOPBACK "127.0.0.1"
#define AMT_PORT 2268

#define DEFAULT_MTU (1500u - (20 + 8))

/* IPv4 Header Format */
typedef struct _amt_ip {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t srcAddr;
    uint32_t destAddr;
} amt_ip_t;

/* IPv4 Header Format with options field */
typedef struct _amt_ip_alert {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t srcAddr;
    uint32_t destAddr;
    uint32_t options;
} amt_ip_alert_t;

/* IGMPv3 Group Record Format (RFC3376) */
typedef struct _amt_igmpv3_groupRecord {
    uint8_t  type;
    uint8_t  auxDatalen;
    uint16_t nSrc;
    uint32_t ssm;
    uint32_t srcIP[1];
} amt_igmpv3_groupRecord_t;

/* IGMPv3 Membership Report Format (RFC3376) */
typedef struct _amt_igmpv3_membership_report {
    uint8_t  type;
    uint8_t  resv;
    uint16_t checksum;
    uint16_t resv2;
    uint16_t nGroupRecord;
    amt_igmpv3_groupRecord_t grp[1];
} amt_igmpv3_membership_report_t;

/* IGMPv3 Membership Query Format (RFC3376) */
typedef struct _amt_igmpv3_membership_query {
    uint8_t  type;
    uint8_t  max_resp_code;  /* in 100ms, Max Resp Time = (mant | 0x10) << (exp + 3) */
    uint32_t checksum;
    uint32_t ssmIP;
    uint8_t  s_qrv;
    uint8_t  qqic;           /* in second, query Time = (mant | 0x10) << (exp + 3) */
    uint16_t nSrc;
    uint32_t srcIP[1];
} amt_igmpv3_membership_query_t;

/* ATM Membership Update Format (RFC7450) */
typedef struct _amt_membership_update_msg {
    amt_ip_alert_t ipHead;
    amt_igmpv3_membership_report_t memReport;
} amt_membership_update_msg_t;

/* AMT Functions */
static int amt_sockets_init( stream_t *p_access );
static void amt_send_relay_discovery_msg( stream_t *p_access, char *relay_ip );
static void amt_send_relay_request( stream_t *p_access, char *relay_ip );
static int amt_joinSSM_group( stream_t *p_access );
static int amt_joinASM_group( stream_t *p_access );
static int amt_leaveASM_group( stream_t *p_access );
static int amt_leaveSSM_group( stream_t *p_access );
static bool amt_rcv_relay_adv( stream_t *p_access );
static bool amt_rcv_relay_mem_query( stream_t *p_access );
static void amt_send_mem_update( stream_t *p_access, char *relay_ip, bool leave );
static bool open_amt_tunnel( stream_t *p_access );
static void amt_update_timer_cb( void *data );

/* Struct to hold AMT state */
typedef struct _access_sys_t
{
    char *relay;
    char relayDisco[INET_ADDRSTRLEN];

    vlc_timer_t updateTimer;

    /* Mulicast group and source */
    struct sockaddr_in mcastGroupAddr;
    struct sockaddr_in mcastSrcAddr;

    /* AMT relay imformation */
    struct sockaddr_in relayDiscoAddr;

    /* AMT Relay Membership Query data (RFC7450) */
    struct relay_mem_query_msg_t {
        uint32_t ulRcvedNonce;
        uint8_t  type;
        uint8_t  uchaMAC[MAC_LEN];
        uint8_t  uchaIGMP[IGMP_QUERY_LEN];
    } relay_mem_query_msg;

    amt_igmpv3_membership_query_t relay_igmp_query;
    size_t mtu;

    uint32_t glob_ulNonce;

    int fd;
    int sAMT;
    int sQuery;
    int timeout;

    bool tryAMT;
} access_sys_t;

/* Standard open/close functions */
static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/* Utility functions */
static unsigned short get_checksum( unsigned short *buffer, int nLen );
static void make_report( amt_igmpv3_membership_report_t *mr );
static void make_ip_header( amt_ip_alert_t *p_ipHead );

vlc_module_begin ()
    set_shortname( N_("AMT" ) )
    set_description( N_("AMT input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_integer( "amt-timeout", 5, AMT_TIMEOUT_TEXT, NULL, true )
    add_integer( "amt-native-timeout", 5, TIMEOUT_TEXT, NULL, true )
    add_string( "amt-relay", AMT_DEFAULT_RELAY, AMT_RELAY_ADDRESS, AMT_RELAY_ADDR_LONG, true )

    set_capability( "access", 0 )
    add_shortcut( "amt" )

    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *BlockAMT( stream_t *, bool * );
static int Control( stream_t *, int, va_list );

/*****************************************************************************
 * Open: Open a connection to the multicast feed
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    stream_t            *p_access = (stream_t*) p_this;
    access_sys_t        *sys = NULL;
    struct addrinfo      hints, *serverinfo = NULL;
    struct sockaddr_in  *server_addr;
    char                *psz_name = NULL, *saveptr, *psz_strtok_r;
    char                 mcastSrc_buf[INET_ADDRSTRLEN], mcastGroup_buf[INET_ADDRSTRLEN];
    const char          *mcastSrc, *mcastGroup;
    int                  i_bind_port = 1234, i_server_port = 0, VLC_ret = VLC_SUCCESS, response;
    vlc_url_t            url = { 0 };

    if( p_access->b_preparsing )
        return VLC_EGENERIC;

    /* Set up p_access */
    ACCESS_SET_CALLBACKS( NULL, BlockAMT, Control, NULL );

    if( !p_access->psz_location )
        return VLC_EGENERIC;

    /* Allocate the structure for holding AMT info and zeroize it */
    sys = vlc_obj_calloc( p_this, 1, sizeof( *sys ) );
    if( unlikely( sys == NULL ) )
        return VLC_ENOMEM;

    /* The standard MPEG-2 transport is 188 bytes.  7 packets fit into a standard 1500 byte Ethernet frame */
    sys->mtu = 7 * 188;

    p_access->p_sys = sys;

    sys->fd = sys->sAMT = sys->sQuery = -1;

    psz_name = strdup( p_access->psz_location );
    if ( unlikely( psz_name == NULL ) )
    {
        VLC_ret = VLC_EGENERIC;
        goto cleanup;
    }

    /* Parse psz_name syntax :
     * [serveraddr[:serverport]][@[bindaddr]:[bindport]] */
    if( vlc_UrlParse( &url, p_access->psz_url ) != 0 )
    {
        msg_Err( p_access, "Invalid URL: %s", p_access->psz_url );
        VLC_ret = VLC_EGENERIC;
        goto cleanup;
    }

    /* Determining the multicast source and group depends on the URL provided */
    /*                                                                        */
    /* The address(es) in the URL can be in the form of IP address or FQDN    */
    /* By calling vlc_getaaddrinfo() you get it in IP form either way         */
    /*                                                                        */
    /* Case 1: amt://<source-ip-address>@<multicast-group-ip-address>         */
    /*                                                                        */
    /*         mcastSrc = <source-ip-address>                            */
    /*         sys->mcastSrcAddr = inet_pton( sys->mcastSrc )                 */
    /*                                                                        */
    /*         mcastGroup = <multicast-group-ip-address>                 */
    /*         sys->mcastGroupAddr = inet_pton( sys->mcastGroup )             */
    /*                                                                        */
    /* Case 2: amt://<multicast-group-ip-address>                             */
    /*                                                                        */
    /*         mcastSrc = MCAST_ANYCAST = "0.0.0.0"                      */
    /*         sys->mcastSrcAddr = inet_pton( sys->mcastSrc ) = 0             */
    /*                                                                        */
    /*         mcastGroup = <multicast-group-ip-address>                 */
    /*         sys->mcastGroupAddr = inet_pton( sys->mcastGroup )             */
    /*                                                                        */

    /* If UDP port provided then assign port to stream */
    if( url.i_port > 0 )
        i_bind_port = url.i_port;

    msg_Dbg( p_access, "Opening multicast: %s:%d local=%s:%d", url.psz_host, i_server_port, url.psz_path, i_bind_port );

    /* Initialize hints prior to call to vlc_getaddrinfo with either IP address or FQDN */
    memset( &hints, 0, sizeof( hints ));
    hints.ai_family = AF_INET;  /* Setting to AF_UNSPEC accepts both IPv4 and IPv6 */
    hints.ai_socktype = SOCK_DGRAM;

    /* Retrieve list of multicast addresses matching the multicast group identifier */
    response = vlc_getaddrinfo( url.psz_host, AMT_PORT, &hints, &serverinfo );

    /* If an error returned print reason and exit */
    if( response != 0 )
    {
        msg_Err( p_access, "Could not find multicast group %s, reason: %s", url.psz_host, gai_strerror(response) );
        VLC_ret = VLC_EGENERIC;
        goto cleanup;
    }

    /* Convert binary socket address to string */
    server_addr = (struct sockaddr_in *) serverinfo->ai_addr;
    if( unlikely( inet_ntop(AF_INET, &(server_addr->sin_addr), mcastGroup_buf, INET_ADDRSTRLEN) == NULL ) )
    {
        int errConv = errno;
        msg_Err(p_access, "Could not convert binary socket address to string: %s", gai_strerror(errConv));
        goto cleanup;
    }
    mcastGroup = mcastGroup_buf;

    /* Store the binary socket representation of multicast group address */
    sys->mcastGroupAddr = *server_addr;

    /* Release the allocated memory */
    freeaddrinfo( serverinfo );
    serverinfo = NULL;

    /* Store string representation */

    msg_Dbg( p_access, "Setting multicast group address to %s", mcastGroup);

    /* Extract the source from the URL, or the multicast group when no source is provided */
    psz_strtok_r = strtok_r( psz_name, "@", &saveptr );
    if ( !psz_strtok_r )
    {
        msg_Err( p_access, "Could not parse location %s", psz_name);
        VLC_ret = VLC_EGENERIC;
        goto cleanup;
    }

    /* Store the string representation */
    mcastSrc = psz_strtok_r;

    /* If strings are equal then no multicast source has been specified, so try anycast */
    if( strcmp( url.psz_host, mcastSrc ) == 0 )
    {
        mcastSrc = MCAST_ANYCAST;
        sys->mcastSrcAddr.sin_addr.s_addr = 0;
        msg_Dbg( p_access, "No multicast source address specified, trying ASM...");
    }

    /* retrieve list of source addresses matching the multicast source identifier */
    response = vlc_getaddrinfo( mcastSrc, AMT_PORT, &hints, &serverinfo );

    /* If an error returned print reason and exit */
    if( response != 0 )
    {
        msg_Err( p_access, "Could not find multicast source %s, reason: %s", mcastSrc, gai_strerror(response) );
        VLC_ret = VLC_EGENERIC;
        goto cleanup;
    }

    /* Convert binary socket address to string */
    server_addr = (struct sockaddr_in *) serverinfo->ai_addr;
    if( unlikely( inet_ntop(AF_INET, &(server_addr->sin_addr), mcastSrc_buf, INET_ADDRSTRLEN) == NULL ) )
    {
        int errConv = errno;
        msg_Err(p_access, "Could not binary socket address to string: %s", gai_strerror(errConv));
        goto cleanup;
    }
    mcastSrc = mcastSrc_buf;

    /* Store the binary socket representation of multicast source address */
    sys->mcastSrcAddr = *server_addr;

    msg_Dbg( p_access, "Setting multicast source address to %s", mcastSrc);

    /* Pull the AMT relay address from the settings */
    sys->relay = var_InheritString( p_access, "amt-relay" );
    if( unlikely( sys->relay == NULL ) )
    {
        msg_Err( p_access, "No relay anycast or unicast address specified." );
        VLC_ret = VLC_EGENERIC;
        goto cleanup;
    }

    msg_Dbg( p_access, "Addresses: mcastGroup: %s mcastSrc: %s relay: %s", \
             mcastGroup, mcastSrc, sys->relay);

    /* Native multicast file descriptor */
    sys->fd = net_OpenDgram( p_access, mcastGroup, i_bind_port,
                             mcastSrc, i_server_port, IPPROTO_UDP );
    if( sys->fd == -1 )
    {
        VLC_ret = VLC_EGENERIC;
        goto cleanup;
    }

    int ret = vlc_timer_create( &sys->updateTimer, amt_update_timer_cb, p_access );
    if( ret != 0 )
    {
        VLC_ret = VLC_EGENERIC;
        goto cleanup;
    }

    sys->timeout = var_InheritInteger( p_access, "amt-native-timeout");
    if( sys->timeout > 0)
        sys->timeout *= 1000;

    sys->tryAMT = false;

cleanup: /* fall through */

    free( psz_name );
    vlc_UrlClean( &url );
    if( serverinfo )
        freeaddrinfo( serverinfo );

    if ( VLC_ret != VLC_SUCCESS )
    {
        free( sys->relay );
        if( sys->fd != -1 )
            net_Close( sys->fd );
    }

    return VLC_ret;
}

/*****************************************************************************
 * Close: Cancel thread and free data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    stream_t     *p_access = (stream_t*)p_this;
    access_sys_t *sys = p_access->p_sys;

    vlc_timer_destroy( sys->updateTimer );

    /* If using AMT tunneling send leave message and free the relay addresses */
    if ( sys->tryAMT )
    {
        /* Prepare socket options */
        if( sys->mcastSrcAddr.sin_addr.s_addr )
            amt_leaveSSM_group( p_access );
        else
            amt_leaveASM_group( p_access );

        /* Send IGMP leave message */
        amt_send_mem_update( p_access, sys->relayDisco, true );
    }
    free( sys->relay );

    net_Close( sys->fd );
    if( sys->sAMT != -1 )
        net_Close( sys->sAMT );
    if( sys->sQuery != -1 )
        net_Close( sys->sQuery );
}

/*****************************************************************************
 * Control: Define stream controls
 *****************************************************************************/
static int Control( stream_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS(var_InheritInteger( p_access, "network-caching" ));
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ReadAMT: Responsible for returning the multicast payload
 *
 * Default MTU based on number of MPEG-2 transports carried in a 1500 byte Ethernet frame
 * however the code is able to receive maximal IPv4 UDP frames and then adjusts the MTU
 *****************************************************************************/
static block_t *BlockAMT(stream_t *p_access, bool *restrict eof)
{
    access_sys_t *sys = p_access->p_sys;
    ssize_t len = 0, shift = 0, tunnel = IP_HDR_LEN + UDP_HDR_LEN + AMT_HDR_LEN;

    /* Allocate anticipated MTU buffer for holding the UDP packet suitable for native or AMT tunneled multicast */
    block_t *pkt = block_Alloc( sys->mtu + tunnel );
    if ( unlikely( pkt == NULL ) )
        return NULL;

    struct pollfd ufd[1];

    if( sys->tryAMT )
        ufd[0].fd = sys->sAMT; /* AMT tunneling file descriptor */
    else
        ufd[0].fd = sys->fd;   /* Native multicast file descriptor */
    ufd[0].events = POLLIN;

    switch (vlc_poll_i11e(ufd, 1, sys->timeout))
    {
        case 0:
            if( !sys->tryAMT )
            {
                msg_Err(p_access, "Native multicast receive time-out");
                if( !open_amt_tunnel( p_access ) )
                    goto error;
                break;
            }
            else
            {
                *eof = true;
            }
            /* fall through */
        case -1:
            goto error;
    }

    /* If using AMT tunneling perform basic checks and point to beginning of the payload */
    if( sys->tryAMT )
    {
        /* AMT is a wrapper for UDP streams, so recv is used. */
        len = recv( sys->sAMT, pkt->p_buffer, sys->mtu + tunnel, 0 );

        /* Check for the integrity of the received AMT packet */
        if( len < 0 || *(pkt->p_buffer) != AMT_MULT_DATA )
            goto error;

        /* Set the offet to the first byte of the payload */
        shift += tunnel;

        /* If the length received is less than the AMT tunnel header then it's truncated */
        if( len < tunnel )
        {
            msg_Err(p_access, "%zd bytes packet truncated (MTU was %zd)", len, sys->mtu);
            pkt->i_flags |= BLOCK_FLAG_CORRUPTED;
        }

        /* Otherwise subtract the length of the AMT encapsulation from the packet received */
        else
        {
            len -= tunnel;
        }
    }
    /* Otherwise pull native multicast */
    else
    {
        struct sockaddr temp;
        socklen_t temp_size = sizeof( struct sockaddr );
        len = recvfrom( sys->sAMT, (char *)pkt->p_buffer, sys->mtu + tunnel, 0, (struct sockaddr*)&temp, &temp_size );
    }

    /* Set the offset to payload start */
    pkt->p_buffer += shift;
    pkt->i_buffer -= shift;

    return pkt;

error:
    block_Release( pkt );
    return NULL;
}

/*****************************************************************************
 * open_amt_tunnel: Create an AMT tunnel to the AMT relay
 *****************************************************************************/
static bool open_amt_tunnel( stream_t *p_access )
{
    struct addrinfo hints, *serverinfo, *server;
    access_sys_t *sys = p_access->p_sys;

    memset( &hints, 0, sizeof( hints ));
    hints.ai_family = AF_INET;  /* Setting to AF_UNSPEC accepts both IPv4 and IPv6 */
    hints.ai_socktype = SOCK_DGRAM;

    msg_Dbg( p_access, "Attempting AMT to %s...", sys->relay);
    sys->tryAMT = true;

    /* Retrieve list of addresses matching the AMT relay */
    int response = vlc_getaddrinfo( sys->relay, AMT_PORT, &hints, &serverinfo );

    /* If an error returned print reason and exit */
    if( response != 0 )
    {
        msg_Err( p_access, "Could not find relay %s, reason: %s", sys->relay, gai_strerror(response) );
        goto error;
    }

    /* Iterate through the list of sockets to find one that works */
    for (server = serverinfo; server != NULL && !vlc_killed(); server = server->ai_next)
    {
        struct sockaddr_in *server_addr = (struct sockaddr_in *) server->ai_addr;
        char relay_ip[INET_ADDRSTRLEN];

        /* Convert to binary representation */
        if( unlikely( inet_ntop(AF_INET, &(server_addr->sin_addr), relay_ip, INET_ADDRSTRLEN) == NULL ) )
        {
            int errConv = errno;
            msg_Err(p_access, "Could not convert relay ip to binary representation: %s", gai_strerror(errConv));
            goto error;
        }

        /* Store string representation */
        memcpy(sys->relayDisco, relay_ip, INET_ADDRSTRLEN);
        if( unlikely( sys->relayDisco == NULL ) )
        {
            goto error;
        }

        msg_Dbg( p_access, "Trying AMT Server: %s", sys->relayDisco);

        /* Store the binary representation */
        sys->relayDiscoAddr.sin_addr = server_addr->sin_addr;

        if( amt_sockets_init( p_access ) != 0 )
            continue; /* Try next server */

        /* Negotiate with AMT relay and confirm you can pull a UDP packet  */
        amt_send_relay_discovery_msg( p_access, relay_ip );
        msg_Dbg( p_access, "Sent relay AMT discovery message to %s", relay_ip );

        if( !amt_rcv_relay_adv( p_access ) )
        {
            msg_Err( p_access, "Error receiving AMT relay advertisement msg from %s, skipping", relay_ip );
            goto error;
        }
        msg_Dbg( p_access, "Received AMT relay advertisement from %s", relay_ip );

        amt_send_relay_request( p_access, relay_ip );
        msg_Dbg( p_access, "Sent AMT relay request message to %s", relay_ip );

        if( !amt_rcv_relay_mem_query( p_access ) )
        {
            msg_Err( p_access, "Could not receive AMT relay membership query from %s, reason: %s", relay_ip, vlc_strerror(errno));
            goto error;
        }
        msg_Dbg( p_access, "Received AMT relay membership query from %s", relay_ip );

        /* If single source multicast send SSM join */
        if( sys->mcastSrcAddr.sin_addr.s_addr )
        {
            if( amt_joinSSM_group( p_access ) != 0 )
            {
                msg_Err( p_access, "Error joining SSM %s", vlc_strerror(errno) );
                goto error;
            }
            msg_Dbg( p_access, "Joined SSM" );
        }

        /* If any source multicast send ASM join */
        else {
            if( amt_joinASM_group( p_access ) != 0 )
            {
                msg_Err( p_access, "Error joining ASM %s", vlc_strerror(errno) );
                goto error;
            }
            msg_Dbg( p_access, "Joined ASM group" );
        }

        /* If started, the timer must be stopped before trying the next server
         * in order to avoid data-race with sys->sAMT. */
        vlc_timer_disarm( sys->updateTimer );

        amt_send_mem_update( p_access, sys->relayDisco, false );
        bool eof=false;
        block_t *pkt;

        /* Confirm that you can pull a UDP packet from the socket */
        if ( !(pkt = BlockAMT( p_access, &eof )) )
        {
            msg_Err( p_access, "Unable to receive UDP packet from AMT relay %s for multicast group", relay_ip );
            continue;
        }
        else
        {
            block_Release( pkt );
            msg_Dbg( p_access, "Got UDP packet from multicast group via AMT relay %s, continuing...", relay_ip );
            break;   /* found an active server sending UDP packets, so exit loop */
        }
    }

    /* if server is NULL then no AMT relay is responding */
    if (server == NULL)
    {
        msg_Err( p_access, "No AMT servers responding" );
        goto error;
    }

    /* release the allocated memory */
    freeaddrinfo( serverinfo );
    return true;

error:
    vlc_timer_disarm( sys->updateTimer );
    if( serverinfo )
        freeaddrinfo( serverinfo );
    return false;
}

/**
 * Calculate checksum
 * */
static unsigned short get_checksum( unsigned short *buffer, int nLen )
{
    int nleft = nLen;
    int sum = 0;
    unsigned short *w = buffer;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }
    if (nleft == 1)
    {
        *(unsigned char*)(&answer) = *(unsigned char*)w;
        sum += answer;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    answer = ~sum;
    return (answer);
}

/**
 * Make IGMP Membership report
 * */
static void make_report( amt_igmpv3_membership_report_t *mr )
{
    mr->type = AMT_IGMPV3_MEMBERSHIP_REPORT_TYPEID;
    mr->resv = 0;
    mr->checksum = 0;
    mr->resv2 = 0;
    mr->nGroupRecord = htons(1);
}

/**
 * Make IP header
 * */
static void make_ip_header( amt_ip_alert_t *p_ipHead )
{
    p_ipHead->ver_ihl = 0x46;
    p_ipHead->tos = 0xc0;
    p_ipHead->tot_len = htons( IP_HDR_IGMP_LEN + IGMP_REPORT_LEN );
    p_ipHead->id = 0x00;
    p_ipHead->frag_off = 0x0000;
    p_ipHead->ttl = 0x01;
    p_ipHead->protocol = 0x02;
    p_ipHead->check = 0;
    p_ipHead->srcAddr = INADDR_ANY;
    p_ipHead->options = 0x0000;
}

/** Create relay discovery socket, query socket, UDP socket and
 * fills in relay anycast address for discovery
 * return 0 if successful, -1 if not
 */
static int amt_sockets_init( stream_t *p_access )
{
    struct sockaddr_in rcvAddr;
    access_sys_t *sys = p_access->p_sys;
    memset( &rcvAddr, 0, sizeof(struct sockaddr_in) );
    int enable = 0, res = 0;

    /* Relay anycast address for discovery */
    sys->relayDiscoAddr.sin_family = AF_INET;
    sys->relayDiscoAddr.sin_port = htons( AMT_PORT );

    /* create UDP socket */
    sys->sAMT = vlc_socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP, true );
    if( sys->sAMT == -1 )
    {
        msg_Err( p_access, "Failed to create UDP socket" );
        goto error;
    }

    res = setsockopt(sys->sAMT, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if(res < 0)
    {
        msg_Err( p_access, "Couldn't make socket reusable");
        goto error;
    }

    rcvAddr.sin_family      = AF_INET;
    rcvAddr.sin_port        = htons( 0 );
    rcvAddr.sin_addr.s_addr = INADDR_ANY;

    if( bind(sys->sAMT, (struct sockaddr *)&rcvAddr, sizeof(rcvAddr) ) != 0 )
    {
        msg_Err( p_access, "Failed to bind UDP socket error: %s", vlc_strerror(errno) );
        goto error;
    }

    sys->sQuery = vlc_socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP, true );
    if( sys->sQuery == -1 )
    {
        msg_Err( p_access, "Failed to create query socket" );
        goto error;
    }

    /* bind socket to local address */
    struct sockaddr_in stLocalAddr =
    {
        .sin_family      = AF_INET,
        .sin_port        = htons( 0 ),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if( bind(sys->sQuery, (struct sockaddr *)&stLocalAddr, sizeof(struct sockaddr) ) != 0 )
    {
        msg_Err( p_access, "Failed to bind query socket" );
        goto error;
    }

    struct sockaddr_in stSvrAddr =
    {
        .sin_family = AF_INET,
        .sin_port = htons( 9124 ),
    };

    res = inet_pton( AF_INET, LOCAL_LOOPBACK, &stSvrAddr.sin_addr );
    if( res != 1 )
    {
        msg_Err( p_access, "Could not convert loopback address" );
        goto error;
    }
    /* TODO: stSvrAddr is unused ? */

    return 0;

error:
    if( sys->sAMT != -1 )
    {
        net_Close( sys->sAMT );
        sys->sAMT = -1;
    }

    if( sys->sQuery != -1 )
    {
        net_Close( sys->sQuery );
        sys->sQuery = -1;
    }
    return -1;
}

/**
 * Send a relay discovery message, before 3-way handshake
 * */
static void amt_send_relay_discovery_msg( stream_t *p_access, char *relay_ip )
{
    char          chaSendBuffer[AMT_DISCO_MSG_LEN];
    unsigned int  ulNonce;
    int           nRet;
    access_sys_t *sys = p_access->p_sys;

    /* initialize variables */
    memset( chaSendBuffer, 0, sizeof(chaSendBuffer) );
    ulNonce = 0;
    nRet = 0;

    /*
     * create AMT discovery message format
     * +---------------------------------------------------+
     * | Msg Type(1Byte)| Reserved (3 byte)| nonce (4byte) |
     * +---------------------------------------------------+
     */

    chaSendBuffer[0] = AMT_RELAY_DISCO;
    chaSendBuffer[1] = 0;
    chaSendBuffer[2] = 0;
    chaSendBuffer[3] = 0;

    /* create nonce and copy into send buffer */
    srand( (unsigned int)time(NULL) );
    ulNonce = htonl( rand() );
    memcpy( &chaSendBuffer[4], &ulNonce, sizeof(ulNonce) );
    sys->glob_ulNonce = ulNonce;

    /* send it */
    nRet = sendto( sys->sAMT, chaSendBuffer, sizeof(chaSendBuffer), 0,\
            (struct sockaddr *)&sys->relayDiscoAddr, sizeof(struct sockaddr) );

    if( nRet < 0)
        msg_Err( p_access, "Sendto failed to %s with error %d.", relay_ip, errno);
}

/**
 * Send relay request message, stage 2 of handshake
 * */
static void amt_send_relay_request( stream_t *p_access, char *relay_ip )
{
    char         chaSendBuffer[AMT_REQUEST_MSG_LEN];
    uint32_t     ulNonce;
    int          nRet;
    access_sys_t *sys = p_access->p_sys;

    memset( chaSendBuffer, 0, sizeof(chaSendBuffer) );

    ulNonce = 0;
    nRet = 0;

    /*
     * create AMT request message format
     * +-----------------------------------------------------------------+
     * | Msg Type(1Byte)| Reserved(1byte)|P flag(1byte)|Reserved (2 byte)|
     * +-----------------------------------------------------------------+
     * |             nonce (4byte)                                       |
     * +-----------------------------------------------------------------+
     *
     * The P flag is set to indicate which group membership protocol the
     * gateway wishes the relay to use in the Membership Query response:

     * Value Meaning

     *  0    The relay MUST respond with a Membership Query message that
     *       contains an IPv4 packet carrying an IGMPv3 General Query
     *       message.
     *  1    The relay MUST respond with a Membership Query message that
     *       contains an IPv6 packet carrying an MLDv2 General Query
     *       message.
     *
     */

    chaSendBuffer[0] = AMT_REQUEST;
    chaSendBuffer[1] = 0;
    chaSendBuffer[2] = 0;
    chaSendBuffer[3] = 0;

    ulNonce = sys->glob_ulNonce;
    memcpy( &chaSendBuffer[4], &ulNonce, sizeof(uint32_t) );

    nRet = send( sys->sAMT, chaSendBuffer, sizeof(chaSendBuffer), 0 );

    if( nRet < 0 )
        msg_Err(p_access, "Error sending relay request to %s error: %s", relay_ip, vlc_strerror(errno) );
}

/*
* create AMT request message format
* +----------------------------------------------------------------------------------+
* | Msg Type(1 byte)| Reserved (1 byte)| MAC (6 byte)| nonce (4 byte) | IGMP packet  |
* +----------------------------------------------------------------------------------+
*/
static void amt_send_mem_update( stream_t *p_access, char *relay_ip, bool leave)
{
    int           sendBufSize = IP_HDR_IGMP_LEN + MAC_LEN + NONCE_LEN + AMT_HDR_LEN;
    char          pSendBuffer[ sendBufSize + IGMP_REPORT_LEN ];
    uint32_t      ulNonce = 0;
    access_sys_t *sys = p_access->p_sys;

    memset( pSendBuffer, 0, sizeof(pSendBuffer) );

    pSendBuffer[0] = AMT_MEM_UPD;

    /* copy relay MAC response */
    memcpy( &pSendBuffer[2], sys->relay_mem_query_msg.uchaMAC, MAC_LEN );

    /* copy nonce */
    ulNonce = sys->glob_ulNonce;
    memcpy( &pSendBuffer[8], &ulNonce, NONCE_LEN );

    /* make IP header for IGMP packet */
    amt_ip_alert_t p_ipHead;
    memset( &p_ipHead, 0, IP_HDR_IGMP_LEN );
    make_ip_header( &p_ipHead );

    struct sockaddr_in temp;
    int res = inet_pton( AF_INET, MCAST_ALLHOSTS, &(temp.sin_addr) );
    if( res != 1 )
    {
        msg_Err(p_access, "Could not convert all hosts multicast address: %s", gai_strerror(errno) );
        return;
    }
    p_ipHead.destAddr = temp.sin_addr.s_addr;
    p_ipHead.check = get_checksum( (unsigned short*)&p_ipHead, IP_HDR_IGMP_LEN );

    amt_igmpv3_groupRecord_t groupRcd;
    groupRcd.auxDatalen = 0;
    groupRcd.ssm = sys->mcastGroupAddr.sin_addr.s_addr;

    if( sys->mcastSrcAddr.sin_addr.s_addr )
    {
        groupRcd.type = leave ? AMT_IGMP_BLOCK:AMT_IGMP_INCLUDE;
        groupRcd.nSrc = htons(1);
        groupRcd.srcIP[0] = sys->mcastSrcAddr.sin_addr.s_addr;

    } else {
        groupRcd.type = leave ? AMT_IGMP_INCLUDE_CHANGE:AMT_IGMP_EXCLUDE_CHANGE;
        groupRcd.nSrc = htons(0);
    }

    /* make IGMP membership report */
    amt_igmpv3_membership_report_t p_igmpMemRep;
    make_report( &p_igmpMemRep );

    memcpy(&p_igmpMemRep.grp[0], &groupRcd, (int)sizeof(groupRcd) );
    p_igmpMemRep.checksum = get_checksum( (unsigned short*)&p_igmpMemRep, IGMP_REPORT_LEN );

    amt_membership_update_msg_t memUpdateMsg;
    memset(&memUpdateMsg, 0, sizeof(memUpdateMsg));
    memcpy(&memUpdateMsg.ipHead, &p_ipHead, sizeof(p_ipHead) );
    memcpy(&memUpdateMsg.memReport, &p_igmpMemRep, sizeof(p_igmpMemRep) );

    memcpy( &pSendBuffer[12], &memUpdateMsg, sizeof(memUpdateMsg) );

    send( sys->sAMT, pSendBuffer, sizeof(pSendBuffer), 0 );

    msg_Dbg( p_access, "AMT relay membership report sent to %s", relay_ip );
}

/**
 * Receive relay advertisement message
 *
 *
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |  V=0  |Type=2 |                   Reserved                    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                        Discovery Nonce                        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                                                               |
 *  ~                  Relay Address (IPv4 or IPv6)                 ~
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * */
static bool amt_rcv_relay_adv( stream_t *p_access )
{
    char pkt[RELAY_ADV_MSG_LEN];
    access_sys_t *sys = p_access->p_sys;

    memset( pkt, 0, RELAY_ADV_MSG_LEN );

    struct pollfd ufd[1];

    ufd[0].fd = sys->sAMT;
    ufd[0].events = POLLIN;

    switch( vlc_poll_i11e(ufd, 1, sys->timeout) )
    {
        case 0:
            msg_Err(p_access, "AMT relay advertisement receive time-out");
            /* fall through */
        case -1:
            return false;
    }

    struct sockaddr temp;
    socklen_t temp_size = sizeof( struct sockaddr );
    ssize_t len = recvfrom( sys->sAMT, pkt, RELAY_ADV_MSG_LEN, 0, &temp, &temp_size );

    if (len < 0)
    {
        msg_Err(p_access, "Received message length less than zero");
        return false;
    }

    /* AMT Relay Advertisement data (RFC7450) */
    struct {
        uint32_t ulRcvNonce;
        uint32_t ipAddr;
        uint8_t  type;
    } relay_adv_msg;

    memcpy( &relay_adv_msg.type, &pkt[0], MSG_TYPE_LEN );
    if( relay_adv_msg.type != AMT_RELAY_ADV )
    {
        msg_Err( p_access, "Received message not an AMT relay advertisement, ignoring. ");
        return false;
    }

    memcpy( &relay_adv_msg.ulRcvNonce, &pkt[NONCE_LEN], NONCE_LEN );
    if( sys->glob_ulNonce != relay_adv_msg.ulRcvNonce )
    {
        msg_Err( p_access, "Discovery nonces differ! currNonce:%x rcvd%x", sys->glob_ulNonce, (uint32_t) ntohl(relay_adv_msg.ulRcvNonce) );
        return false;
    }

    memcpy( &relay_adv_msg.ipAddr, &pkt[8], 4 );

    struct sockaddr_in relayAddr =
    {
        .sin_family       = AF_INET,
        .sin_addr.s_addr  = relay_adv_msg.ipAddr,
        .sin_port         = htons( AMT_PORT ),
    };

    int nRet = connect( sys->sAMT, (struct sockaddr *)&relayAddr, sizeof(relayAddr) );
    if( nRet < 0 )
    {
        msg_Err( p_access, "Error connecting AMT UDP socket: %s", vlc_strerror(errno) );
        return false;
    }

    return true;
}

/**
 * Receive relay membership query message
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  V=0  |Type=4 | Reserved  |L|G|         Response MAC          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         Request Nonce                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |               Encapsulated General Query Message              |
   ~                 IPv4:IGMPv3(Membership Query)                 ~
   |                  IPv6:MLDv2(Listener Query)                   |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     Gateway Port Number       |                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
   |                                                               |
   +                                                               +
   |                Gateway IP Address (IPv4 or IPv6)              |
   +                                                               +
   |                                                               |
   +                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
static bool amt_rcv_relay_mem_query( stream_t *p_access )
{
    char pkt[RELAY_QUERY_MSG_LEN];
    memset( pkt, 0, RELAY_QUERY_MSG_LEN );
    struct pollfd ufd[1];
    access_sys_t *sys = p_access->p_sys;

    ufd[0].fd = sys->sAMT;
    ufd[0].events = POLLIN;

    switch( vlc_poll_i11e(ufd, 1, sys->timeout) )
    {
        case 0:
            msg_Err(p_access, "AMT relay membership query receive time-out");
            /* fall through */
        case -1:
            return false;
    }

    ssize_t len = recv( sys->sAMT, pkt, RELAY_QUERY_MSG_LEN, 0 );

    if (len < 0 || len != RELAY_QUERY_MSG_LEN)
    {
        msg_Err(p_access, "length less than zero");
        return false;
    }

    memcpy( &sys->relay_mem_query_msg.type, &pkt[0], MSG_TYPE_LEN );
    /* pkt[1] is reserved  */
    memcpy( &sys->relay_mem_query_msg.uchaMAC[0], &pkt[AMT_HDR_LEN], MAC_LEN );
    memcpy( &sys->relay_mem_query_msg.ulRcvedNonce, &pkt[AMT_HDR_LEN + MAC_LEN], NONCE_LEN );
    if( sys->relay_mem_query_msg.ulRcvedNonce != sys->glob_ulNonce )
    {
        msg_Warn( p_access, "Nonces are different rcvd: %x glob: %x", sys->relay_mem_query_msg.ulRcvedNonce, sys->glob_ulNonce );
        return false;
    }

    size_t shift = AMT_HDR_LEN + MAC_LEN + NONCE_LEN + IP_HDR_IGMP_LEN;
    sys->relay_igmp_query.type = pkt[shift];
    shift++; assert( shift < RELAY_QUERY_MSG_LEN);
    sys->relay_igmp_query.max_resp_code = pkt[shift];
    shift++; assert( shift < RELAY_QUERY_MSG_LEN);
    memcpy( &sys->relay_igmp_query.checksum, &pkt[shift], 2 );
    shift += 2; assert( shift < RELAY_QUERY_MSG_LEN);
    memcpy( &sys->relay_igmp_query.ssmIP, &pkt[shift], 4 );
    shift += 4; assert( shift < RELAY_QUERY_MSG_LEN);
    sys->relay_igmp_query.s_qrv = pkt[shift];
    shift++; assert( shift < RELAY_QUERY_MSG_LEN);
    if( pkt[shift] == 0 )
        sys->relay_igmp_query.qqic = 125;
    else
        sys->relay_igmp_query.qqic = pkt[shift];

    shift++; assert( shift < RELAY_QUERY_MSG_LEN);
    memcpy( &sys->relay_igmp_query.nSrc, &pkt[shift], 2 );

    /* Arms the timer for a single shot: cf. amt_update_timer_cb comment */
    vlc_timer_schedule( sys->updateTimer, false,
                        VLC_TICK_FROM_SEC( sys->relay_igmp_query.qqic ), 0 );

    return true;
}

/**
 * Join SSM group based on input addresses, or use the defaults
 * */
static int amt_joinSSM_group( stream_t *p_access )
{
#ifdef IP_ADD_SOURCE_MEMBERSHIP
    struct ip_mreq_source imr;
    access_sys_t *sys = p_access->p_sys;

    imr.imr_multiaddr.s_addr = sys->mcastGroupAddr.sin_addr.s_addr;
    imr.imr_sourceaddr.s_addr = sys->mcastSrcAddr.sin_addr.s_addr;
    imr.imr_interface.s_addr = INADDR_ANY;

    return setsockopt( sys->sAMT, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (char *)&imr, sizeof(imr) );
#else
    errno = EINVAL;
    return -1;
#endif
}

static int amt_joinASM_group( stream_t *p_access )
{
    struct ip_mreq imr;
    access_sys_t *sys = p_access->p_sys;

    imr.imr_multiaddr.s_addr = sys->mcastGroupAddr.sin_addr.s_addr;
    imr.imr_interface.s_addr = INADDR_ANY;

    return setsockopt( sys->sAMT, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&imr, sizeof(imr) );
}

/**
 * Leave SSM group that was joined earlier.
 * */
static int amt_leaveSSM_group( stream_t *p_access )
{
#ifdef IP_DROP_SOURCE_MEMBERSHIP
    struct ip_mreq_source imr;
    access_sys_t *sys = p_access->p_sys;

    imr.imr_multiaddr.s_addr = sys->mcastGroupAddr.sin_addr.s_addr;
    imr.imr_sourceaddr.s_addr = sys->mcastSrcAddr.sin_addr.s_addr;
    imr.imr_interface.s_addr = INADDR_ANY;

    return setsockopt( sys->sAMT, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, (char *)&imr, sizeof(imr) );
#else
    errno = EINVAL;
    return -1;
#endif
}

/**
 * Leave ASM group that was joined earlier.
 * */
static int amt_leaveASM_group( stream_t *p_access )
{
    struct ip_mreq imr;
    access_sys_t *sys = p_access->p_sys;

    imr.imr_multiaddr.s_addr = sys->mcastGroupAddr.sin_addr.s_addr;
    imr.imr_interface.s_addr = INADDR_ANY;

    return setsockopt( sys->sAMT, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&imr, sizeof(imr) );
}

/* A timer is spawned since IGMP membership updates need to issued periodically
 * in order to continue to receive multicast. */
static void amt_update_timer_cb( void *data )
{
    stream_t     *p_access = (stream_t*) data;
    access_sys_t *sys = p_access->p_sys;

    amt_send_mem_update( p_access, sys->relayDisco, false );

    /* Arms the timer again for a single shot from this callback. That way, the
     * time spent in amt_send_mem_update() is taken into consideration. */
    vlc_timer_schedule( sys->updateTimer, false,
                        VLC_TICK_FROM_SEC( sys->relay_igmp_query.qqic ), 0 );
}
