/**
 * @file amt.c
 * @brief Automatic Multicast Tunneling Protocol (AMT) file for VLC media player
 * Allows multicast streaming when not in a multicast-enabled network
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
 *          Nicolas Zunker                                    - Added IPv6 support
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
#define AMT_RELAY_ADDRESS N_("AMT relay (IP address or FQDN)")
#define AMT_RELAY_ADDR_LONG N_("AMT relay anycast address, or specify the relay you want by address or fully qualified domain name")
#define AMT_DEFAULT_RELAY "amt-relay.m2icast.net"

/*****************************************************************************
 * Various Lengths of Msgs or Hdrs
 *****************************************************************************/
#define MAC_LEN 6                /* length of generated MAC in bytes */
#define NONCE_LEN 4              /* length of nonce in bytes */

#define MSG_TYPE_LEN 1           /* length of msg type */
#define RELAY_QUERY_MSG_LEN 88   /* total length of relay query: 12 byte AMT header, + 36 bytes for IGMPv3 or + 76 for MLDv2 */
#define RELAY_ADV_MSG_LEN 24     /* length of relay advertisement message: 8 bytes + ip address (4 or 16 bytes) */
#define IGMP_QUERY_LEN 24        /* length of encapsulated IGMP query message */
#define IGMP_REPORT_LEN 20
#define AMT_IPV6_MAX_NUM_SOURCES 10 /* arbitrary maximum length on the number of sources we will read from an MLD Query's address records */
#define MLD_ADDRESS_RECORD_LEN 36 /* assuming no auxiliary data */
#define MLD_HEADER_LEN 8
#define MLD_REPORT_LEN (MLD_HEADER_LEN + MLD_ADDRESS_RECORD_LEN) /* 8 Byte Fixed MLD Header + the Address record (44 bytes)*/
#define AMT_HDR_LEN 2            /* length of AMT header on a packet */
#define IP_HDR_LEN 20            /* length of standard IP header */
#define IPv6_FIXED_HDR_LEN 40
#define IPv6_HOP_BY_HOP_OPTION_LEN 8 /* For MLD this header is always present */
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

#define AMT_MLD_MCAST_ADDRESS_RECORD_TYPE_ALLOW_NEW_SOURCES 5
#define AMT_MLD_MCAST_ADDRESS_RECORD_TYPE_BLOCK_OLD_SOURCES 6
#define AMT_MLD_REPORT_TYPE 143
#define AMT_MLD_QUERY_TYPE 130
#define AMT_MLD_DONE_TYPE 132

#define MCAST_ALLHOSTS "224.0.0.22"
#define MCAST_ALL_MLDv2_CAP_ROUTERS "FF02::16"
#define LINK_SCOPE_ALL_NODES_MCAST "FF02::1"
#define AMT_PORT 2268

#define DEFAULT_MTU (1500u - (IP_HDR_LEN + UDP_HDR_LEN))
#define DEFAULT_MTU_IPv6 (1500u - (IPv6_FIXED_HDR_LEN + UDP_HDR_LEN))

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

typedef struct {
    uint8_t next_header; /* 58  (ICMPv6) */
    uint8_t length;
    /* Router Alert*/
    uint8_t ra_type; /* 5 */
    uint8_t ra_length; /* 2 */
    uint16_t ra_router_alert; /* 0 for MLD */
    /* Padding Option */
    uint8_t po_type;
    uint8_t po_length; /* this should usually be 0 for our purposes */
} ipv6_hop_by_hop_option_t;

typedef struct {
    uint8_t version; /* 4 bits, = 6 for ipv6 */
    uint8_t traffic_class; /* 0 for now */
    uint32_t flow_label; /* 20 bits, 0 for now */
    uint16_t payload_len;
    uint8_t next_header; /* 0 (hop by hop option) */
    uint8_t hop_limit;
    struct in6_addr srcAddr;
    struct in6_addr dstAddr;
    ipv6_hop_by_hop_option_t hop_by_hop_option;
} amt_ipv6_t;

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

/* MLDv2 Multicast Address Record Format */
typedef struct {
    uint8_t record_type;
    uint8_t aux_data_len; /* length in 32 bit units */
    uint16_t num_srcs;
    struct in6_addr mcast_address;
    struct in6_addr *sources;
    uint32_t *auxiliary_data;
} amt_ipv6_multicast_address_record_t;

/* MLDv2 Multicast Listener Query Message */
typedef struct {
    uint8_t type; /* 130 */
    uint8_t code;
    uint16_t checksum;
    uint16_t max_resp_code;
    struct in6_addr mcast_address;
    bool s_flag;
    uint8_t qrv; /* 4 bits */
    uint8_t qqic;
    uint16_t num_srcs;
    struct in6_addr *srcs;
}  amt_mldv2_listener_query_t;

/* MLDv2 Multicast Listener Report Message */
typedef struct {
    uint8_t type; /* 143 */
    uint8_t code;
    uint16_t checksum;
    uint16_t num_records;
    amt_ipv6_multicast_address_record_t records[1];
}  amt_mldv2_listener_report_t;

/* MLDv1 Multicast Listener Done Message*/
typedef struct {
    uint8_t type; /* 132 */
    uint8_t code;
    uint16_t checksum;
    uint16_t max_resp_delay;
    struct in6_addr mcast_addr;
} amt_mldv1_listener_done_t;

/* ATM Membership Update Format (RFC7450) */
typedef struct _amt_membership_update_msg {
    amt_ip_alert_t ipHead;
    union {
        amt_mldv2_listener_report_t mld_memReport;
        amt_mldv1_listener_done_t mld_listener_done;
        amt_igmpv3_membership_report_t igmp_memReport;
    } encapsulated;
} amt_membership_update_msg_t;

/* AMT Functions */
static int amt_sockets_init( stream_t *p_access );
static void amt_send_relay_discovery_msg( stream_t *p_access, char *relay_ip );
static void amt_send_relay_request( stream_t *p_access, char *relay_ip );
static bool amt_rcv_relay_adv( stream_t *p_access );
static bool amt_rcv_relay_mem_query( stream_t *p_access );
static int amt_send_mem_update( stream_t *p_access, bool leave );
static bool open_amt_tunnel( stream_t *p_access );
static void amt_update_timer_cb( void *data );

/* Struct to hold AMT state */
typedef struct _access_sys_t
{
    char *relay;

    vlc_timer_t updateTimer;

    /* Mulicast group and source, differentiate between address family based on the sa_family which is the first field (sin_family or sin6_family) in both structs */
    union {
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } mcastGroupAddr;
    union {
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } mcastSrcAddr;
    /* AMT relay imformation */
    union {
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } relayDiscoAddr;

    /* AMT Relay Membership Query data (RFC7450) */
    struct relay_mem_query_msg_t {
        uint32_t ulRcvedNonce;
        uint8_t  type;
        uint8_t  uchaMAC[MAC_LEN];
        /* uint8_t  uchaIGMP[IGMP_QUERY_LEN]; */
    } relay_mem_query_msg;

    union {
        amt_igmpv3_membership_query_t igmp;
        amt_mldv2_listener_query_t mld;
    } relay_query;

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
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_integer( "amt-native-timeout", 5, TIMEOUT_TEXT, NULL )
    add_string( "amt-relay", AMT_DEFAULT_RELAY, AMT_RELAY_ADDRESS, AMT_RELAY_ADDR_LONG )

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
    char                *saveptr, *psz_strtok_r;
    char                 mcastSrc_buf[INET6_ADDRSTRLEN], mcastGroup_buf[INET6_ADDRSTRLEN];
    const char          *mcastSrc, *mcastGroup;
    int                  i_bind_port = 1234, i_server_port = 0, VLC_ret = VLC_SUCCESS, response;
    vlc_url_t            url = { 0 };

    if( p_access->b_preparsing )
    {
        return VLC_EGENERIC;
    }

    /* Set up p_access */
    ACCESS_SET_CALLBACKS( NULL, BlockAMT, Control, NULL );

    if( !p_access->psz_location )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the structure for holding AMT info and zeroize it */
    sys = vlc_obj_calloc( p_this, 1, sizeof( *sys ) );
    if( unlikely( sys == NULL ) )
    {
        return VLC_ENOMEM;
    }

    sys->mtu = 1500;

    p_access->p_sys = sys;

    sys->fd = sys->sAMT = sys->sQuery = -1;

    /* Parse psz_url syntax :
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
    {
        msg_Dbg( p_access, "Parsed AMT URL port %d",url.i_port);
        i_bind_port = url.i_port;
    }
    
    msg_Dbg( p_access, "Opening multicast: %s:%d local=%s:%d", url.psz_host, i_server_port, url.psz_path, i_bind_port );

    /* Initialize hints prior to call to vlc_getaddrinfo with either IP address or FQDN */
    memset( &hints, 0, sizeof( hints ));
    hints.ai_family = AF_UNSPEC;  /* Setting to AF_UNSPEC accepts both IPv4 and IPv6 */
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
    void *binary_address = 0;
    if (serverinfo->ai_family == AF_INET)
    {
        binary_address = &( (struct sockaddr_in*) serverinfo->ai_addr)->sin_addr;
    }
    else
    {
        binary_address = &( (struct sockaddr_in6*) serverinfo->ai_addr)->sin6_addr;
    }
    if( unlikely( inet_ntop(serverinfo->ai_family, binary_address, mcastGroup_buf, INET6_ADDRSTRLEN) == NULL ) )
    {
        int errConv = errno;
        msg_Err(p_access, "Could not convert binary socket address to string: %s", gai_strerror(errConv));
        goto cleanup;
    }
    mcastGroup = mcastGroup_buf;
    /* Store the binary socket representation of multicast group address */
    if ( serverinfo->ai_family == AF_INET )
    {
        sys->mcastGroupAddr.ipv4 = *(struct sockaddr_in*)serverinfo->ai_addr;
        hints.ai_family = AF_INET; /* now that we know family of the group we can improve the hints for resolving the source */
    }
    else
    {
        sys->mcastGroupAddr.ipv6 = *(struct sockaddr_in6*)serverinfo->ai_addr;
        hints.ai_family = AF_INET6;
    }
    /* Release the allocated memory */
    freeaddrinfo( serverinfo );
    serverinfo = NULL;

    /* Store string representation */

    msg_Dbg( p_access, "Setting multicast group address to %s", mcastGroup);

    /* Extract the source from the URL, or the multicast group when no source is provided */
    psz_strtok_r = strtok_r( (char *) p_access->psz_location, "@", &saveptr );
    if ( !psz_strtok_r )
    {
        msg_Err( p_access, "Could not parse location %s", p_access->psz_location);
        VLC_ret = VLC_EGENERIC;
        goto cleanup;
    }

    /* Store the string representation */
    mcastSrc = psz_strtok_r;

    /* If IP addresses in the strings are equal then no multicast source has been specified, so try anycast */
    /* The mcastSrc URL may include a port and may have the brackets of an IPv6 address whereas psz_host is just the IP address so we use strstr() instead of strcmp */
    if( strstr( mcastSrc, url.psz_host ) )
    {
        mcastSrc = 0; // for net_OpenDgram()
        memset(&sys->mcastSrcAddr,0,sizeof(sys->mcastSrcAddr));
        msg_Dbg( p_access, "No multicast source address specified, trying ASM...");
    }
    else
    {
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
        binary_address = 0;
        if (serverinfo->ai_family == AF_INET)
        {
            binary_address = &( (struct sockaddr_in*) serverinfo->ai_addr)->sin_addr;
        }
        else
        {
            binary_address = &( (struct sockaddr_in6*) serverinfo->ai_addr)->sin6_addr;
        }
        if( unlikely( inet_ntop(serverinfo->ai_family, binary_address, mcastSrc_buf, INET6_ADDRSTRLEN) == NULL ) )
        {
            int errConv = errno;
            msg_Err(p_access, "Could not convert binary socket address to string: %s", gai_strerror(errConv));
            goto cleanup;
        }
        mcastSrc = mcastSrc_buf;
        /* Store the binary socket representation of multicast source address */
        if ( serverinfo->ai_family == AF_INET )
        {
            sys->mcastSrcAddr.ipv4 = *(struct sockaddr_in*)serverinfo->ai_addr;
        } else {
            sys->mcastSrcAddr.ipv6 = *(struct sockaddr_in6*)serverinfo->ai_addr;
        }
        msg_Dbg( p_access, "Setting multicast source address to %s", mcastSrc);
    }


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
        /* Send IGMP leave message */
        amt_send_mem_update( p_access, true );
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
    ssize_t len = 0, shift = 0;
    int tunnel = UDP_HDR_LEN + AMT_HDR_LEN + (sys->mcastGroupAddr.ipv4.sin_family == AF_INET ? IP_HDR_LEN : IPv6_FIXED_HDR_LEN );

    /* Allocate anticipated MTU buffer for holding the UDP packet suitable for native or AMT tunneled multicast */
    block_t *pkt = block_Alloc( sys->mtu + tunnel );
    if ( unlikely( pkt == NULL ) )
    {
        return NULL;
    }

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
        if( len < tunnel || *(pkt->p_buffer) != AMT_MULT_DATA )
        {
            msg_Err(p_access, "Error pulling data from AMT socket. Length of data : %zd may be too short (should be minimum %d) - AMT header is %s",len, tunnel, (len > 0 && pkt->p_buffer[0] == AMT_MULT_DATA) ? "correct" : "incorrect" );
            goto error;
        }

        uint16_t payload_len = 0;
        bool b_is_fragmented = 0;
        static uint32_t fragment_id = 0;

        /* determine the payload length from the IP header(s), and handle fragmentation if it occurs */
        int i_ipv = pkt->p_buffer[AMT_HDR_LEN] >> 4;
        if( i_ipv == 4 )
        {
            payload_len = pkt->p_buffer[AMT_HDR_LEN + 2] << 8;
            payload_len += pkt->p_buffer[AMT_HDR_LEN + 3];
            /* in ipv4 headers its the total length so we ned to subtract the ip header's length (bottom 4 bits of the first byte) */
            payload_len -= 4 * (pkt->p_buffer[AMT_HDR_LEN] & 0x0F);

        }
        else if ( i_ipv == 6 )
        {
            payload_len = pkt->p_buffer[AMT_HDR_LEN + 4] << 8;
            payload_len += pkt->p_buffer[AMT_HDR_LEN + 5];

            int i_next_hdr = pkt->p_buffer[AMT_HDR_LEN + 6];
            b_is_fragmented = (i_next_hdr == 44);
            if ( !b_is_fragmented && i_next_hdr != 17 )
            {
                msg_Warn(p_access, "Received unexpected ip header in tunneled AMT data: next header = %d",i_next_hdr);
                /* we can probably survive this (provided there is no fragmentation) since we calculate the length as the difference between the length of the received data and the length listed in the ip header */
            }

            if ( b_is_fragmented && len < tunnel + 8 )
            {
                msg_Err(p_access, "Received length of data %zd is smaller than minimum length for an ip header + fragmentation header (%d)",len, tunnel + 8);
                goto error;
            }

            uint32_t tmp = 0;
            memcpy(&tmp,&pkt->p_buffer[AMT_HDR_LEN + IPv6_FIXED_HDR_LEN + 4],4);
            if ( b_is_fragmented && !fragment_id )
            {
                /* this is the start of a fragment, payload len in the ipv6 header will be wrong */
                payload_len = len - (tunnel + 8);

                fragment_id = tmp;

                i_next_hdr = pkt->p_buffer[AMT_HDR_LEN + IPv6_FIXED_HDR_LEN];
                if ( i_next_hdr != 17 )
                {
                    msg_Err(p_access, "Received unexpected next header in ipv6 fragment header: next header = %d",i_next_hdr);
                    goto error; /* here we actually need to be sure the next header is UDP othewrise we cant know the payload length */
                }

                msg_Dbg(p_access, "Start of new fragment, id is 0x%x (NETWORK BYTE ORDER) , first fragment's length is %u (%zd total - %d tunnel size) (mtu is %zd)",fragment_id, payload_len,len,tunnel,sys->mtu);
            }
            else if ( b_is_fragmented && fragment_id != tmp )
            {
                msg_Warn(p_access, "Received fragment id does not match last seen fragment id : 0x%x expected vs 0x%x received",fragment_id,tmp);
                payload_len -= 8; /* still try to receive it */
            }
            else if ( b_is_fragmented )
            {
                /* this is a subsequent fragment, the payload len in the ipv6 header is right, just need to subtract fragmentation header length */
                payload_len -= 8;
            }

            if ( b_is_fragmented && (pkt->p_buffer[AMT_HDR_LEN + IPv6_FIXED_HDR_LEN + 3] & 1) == 0 )
            {
                fragment_id = 0; /* no more fragments */
            }



        }
        else
        {
            msg_Err(p_access, "Received unexpected ip version in tunneled AMT data: %d",i_ipv);
            goto error;
        }

        /* if its fragmented, payload_len will be set correctly before we get here */
        if ( !b_is_fragmented )
        {
            payload_len -= UDP_HDR_LEN;
        }

        /* Set the offet to the first byte of the payload */
        shift = len - payload_len;

        if ( !payload_len || (shift < tunnel && !b_is_fragmented) )
        {
            msg_Err(p_access, "Length listed in the ip header is unrealistic: %zd bytes received but %u were listed in ip header. Tunnel overhead should be %d bytes",len,payload_len,tunnel);
            goto error;
        }


        /* If the length received is less than the AMT tunnel header then it's truncated */
        if( len < tunnel )
        {
            msg_Err(p_access, "%zd bytes packet truncated (MTU was %zd)", len, sys->mtu);
            pkt->i_flags |= BLOCK_FLAG_CORRUPTED;
        } else {
        /* Otherwise subtract the length of the AMT encapsulation from the packet received */
            len -= shift;
        }

    } else {
    /* Otherwise pull native multicast */
        struct sockaddr temp;
        socklen_t temp_size = sizeof( struct sockaddr );
        len = recvfrom( sys->fd, (char *)pkt->p_buffer, sys->mtu + tunnel, 0, (struct sockaddr*)&temp, &temp_size );
        if ( len <= 0 )
        {
            msg_Err(p_access, "recv() call failed: %zd was returned", len);
            goto error;
        }
    }

    /* Set the offset to payload start */
    pkt->p_buffer += shift;
    pkt->i_buffer = len;

    msg_Dbg(p_access, "Received mcast/amt packet of length %zd",len);
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
    hints.ai_family = AF_UNSPEC;  /* Setting to AF_UNSPEC accepts both IPv4 and IPv6 */
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
        struct sockaddr *server_addr = server->ai_addr; /* can be either an ipv4 struct sockaddr_in or a ipv6 struct sockaddr_in6 */
        char relay_ip[INET6_ADDRSTRLEN];

        /* Convert to binary representation */
        void *binary_address = 0;
        if (server->ai_family == AF_INET)
        {
            binary_address = &( (struct sockaddr_in*) server->ai_addr)->sin_addr;
        }
        else
        {
            binary_address = &( (struct sockaddr_in6*) server->ai_addr)->sin6_addr;
        }
        if( unlikely( inet_ntop(server->ai_family, binary_address, relay_ip,INET6_ADDRSTRLEN) == NULL ) )
        {
            int i_errConv = errno;
            msg_Err(p_access, "Could not convert relay ip to binary representation: %s", gai_strerror(i_errConv));
            goto error;
        }

        msg_Dbg( p_access, "Trying AMT Server: %s", relay_ip);

        /* Store the binary representation */
        if ( server->ai_family == AF_INET )
        {
            sys->relayDiscoAddr.ipv4 = *(struct sockaddr_in*)server_addr;
        } else {
            sys->relayDiscoAddr.ipv6 = *(struct sockaddr_in6*)server_addr;
        }

        if( amt_sockets_init( p_access ) != 0 )
        {
            continue; /* Try next server */
        }

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

        amt_send_mem_update( p_access, false );
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

            /* Arm IGMP timer once we've confirmed we are getting packets */
            vlc_timer_schedule( sys->updateTimer, false,
                        VLC_TICK_FROM_SEC( sys->relayDiscoAddr.ipv4.sin_family == AF_INET ? sys->relay_query.igmp.qqic : sys->relay_query.mld.qqic), 0 );
            msg_Err(p_access, "Arming timer. qqic is %d",sys->relayDiscoAddr.ipv4.sin_family == AF_INET ? sys->relay_query.igmp.qqic : sys->relay_query.mld.qqic);

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
    {
        freeaddrinfo( serverinfo );
    }
    return false;
}

/**
 * Calculate checksum
 * */
static unsigned short ipv6_checksum(unsigned short *buffer, int nLen)
{
    int i_nleft = nLen;
    int i_sum = 0;
    unsigned short *w = buffer;
    unsigned short answer = 0;

    uint8_t pseudo_header [40];
    memset(pseudo_header,0,40);

    pseudo_header[16] = 0xFF;
    pseudo_header[17] = 0x02;
    pseudo_header[31] = 0x16;

    uint32_t tmp = htonl(nLen); /* MLD report length */
    memcpy(&pseudo_header[32],&tmp,4);
    pseudo_header[39] = 58; /* next header = ICMPv6 */

    unsigned short *buf1 = (unsigned short*) pseudo_header;

    for( int i = 0; i < 20; i++ )
    {
        i_sum += *buf1++;
    }

    while ( i_nleft > 1)
    {
        i_sum += *w++;
        i_nleft -= 2;
    }
    if ( i_nleft == 1)
    {
        *(unsigned char*)(&answer) = *(unsigned char*)w;
        i_sum += answer;
    }

    while ( i_sum>>16 )
    {
        i_sum = (i_sum & 0xffff) + (i_sum >> 16);
    }

    answer = ~i_sum;
    return answer;
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

    while ( sum>>16 )
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    answer = ~sum;
    return answer;
}

/**
 * Make MLD Listener report
 * */
static int make_mld_report( stream_t *p_access, bool leave, amt_mldv2_listener_report_t *report, struct in6_addr *group, struct in6_addr *src)
{

    report->type = AMT_MLD_REPORT_TYPE;
    report->code = 0;
    report->checksum = 0;
    report->num_records = 1;
    report->records[0].record_type = leave ? AMT_MLD_MCAST_ADDRESS_RECORD_TYPE_BLOCK_OLD_SOURCES : AMT_MLD_MCAST_ADDRESS_RECORD_TYPE_ALLOW_NEW_SOURCES;
    report->records[0].aux_data_len = 0;
    report->records[0].num_srcs = 1;
    report->records[0].mcast_address = *group;
    report->records[0].sources = vlc_obj_calloc( p_access, 1, 16); /* ipv6 addr */
    if ( !report->records[0].sources )
    {
        return 1;
    }
    report->records[0].sources[0] = *src; /* ipv6 addr */
    return 0;
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
    p_ipHead->options = 0x0;
}

/**
 * Make IPv6 Packet structure preceding an MLD message
 * */
static void make_ipv6( amt_ipv6_t *p, uint16_t length, struct in6_addr *dst )
{
    p->version = 6;
    p->traffic_class = 0;
    p->flow_label = 0;
    p->payload_len = length;
    p->next_header = 0; /* hop by hop option */
    p->hop_limit = 1;
    p->srcAddr = (struct in6_addr) IN6ADDR_ANY_INIT;
    p->dstAddr = *dst;

    ipv6_hop_by_hop_option_t hbh;
    hbh.next_header = 58; /* ICMPv6 */
    hbh.length = 0;
    hbh.ra_type = 5;
    hbh.ra_length = 2;
    hbh.ra_router_alert = 0;
    hbh.po_type = 1; /* skip and continue, with low order bits set */
    hbh.po_length = 0;

    p->hop_by_hop_option = hbh;
}

/** Create relay discovery socket, query socket, UDP socket and
 * fills in relay anycast address for discovery
 * return 0 if successful, -1 if not
 */
static int amt_sockets_init( stream_t *p_access )
{
    struct sockaddr *rcvAddr;
    struct sockaddr_in rcvAddr4;
    struct sockaddr_in6 rcvAddr6;
    access_sys_t *sys = p_access->p_sys;
    memset( &rcvAddr4, 0, sizeof(struct sockaddr_in) );
    memset( &rcvAddr6, 0, sizeof(struct sockaddr_in6) );
    int enable = 0, res = 0;

    /* create UDP socket */
    sys->sAMT = vlc_socket( sys->relayDiscoAddr.ipv4.sin_family, SOCK_DGRAM, IPPROTO_UDP, true );
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

    rcvAddr4.sin_family      = AF_INET;
    rcvAddr4.sin_port        = htons( 0 );
    rcvAddr4.sin_addr.s_addr = INADDR_ANY;

    rcvAddr6.sin6_family      = AF_INET6;
    rcvAddr6.sin6_port        = htons( 0 );
    rcvAddr6.sin6_addr = (struct in6_addr) IN6ADDR_ANY_INIT;

    rcvAddr = sys->relayDiscoAddr.ipv4.sin_family == AF_INET ? (struct sockaddr*) &rcvAddr4 : (struct sockaddr*) &rcvAddr6;

    if( bind(sys->sAMT, rcvAddr, sys->relayDiscoAddr.ipv4.sin_family == AF_INET ? sizeof(rcvAddr4) : sizeof(rcvAddr6) ) != 0 )
    {
        msg_Err( p_access, "Failed to bind UDP socket error: %s", vlc_strerror(errno) );
        goto error;
    }

    sys->sQuery = vlc_socket( sys->relayDiscoAddr.ipv4.sin_family, SOCK_DGRAM, IPPROTO_UDP, true );
    if( sys->sQuery == -1 )
    {
        msg_Err( p_access, "Failed to create query socket" );
        goto error;
    }

    /* bind socket to local address */
    struct sockaddr_in stLocalAddr4 =
    {
        .sin_family      = AF_INET,
        .sin_port        = htons( 0 ),
        .sin_addr.s_addr = INADDR_ANY,
    };

    struct sockaddr_in6 stLocalAddr6;
    memset( &rcvAddr6, 0, sizeof(struct sockaddr_in6) );
    stLocalAddr6.sin6_family = AF_INET6;
    stLocalAddr6.sin6_port = htons( 0 );
    stLocalAddr6.sin6_addr = (struct in6_addr) IN6ADDR_ANY_INIT;

    struct sockaddr *stLocalAddr = sys->relayDiscoAddr.ipv4.sin_family == AF_INET ? (struct sockaddr*) &stLocalAddr4 : (struct sockaddr*) &stLocalAddr6;

    if( bind(sys->sQuery, stLocalAddr, sys->relayDiscoAddr.ipv4.sin_family == AF_INET ? sizeof(stLocalAddr4) : sizeof(stLocalAddr6) ) != 0 )
    {
        msg_Err( p_access, "Failed to bind query socket" );
        goto error;
    }

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
    ssize_t       nRet;
    access_sys_t *sys = p_access->p_sys;

    /* initialize variables */
    memset( chaSendBuffer, 0, sizeof(chaSendBuffer) );
    ulNonce = 0;

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
    nRet = sendto( sys->sAMT, chaSendBuffer, sizeof(chaSendBuffer), 0, (struct sockaddr*) &sys->relayDiscoAddr, sys->relayDiscoAddr.ipv4.sin_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));

    if( nRet < 0)
    {
        msg_Err( p_access, "Sendto failed to %s with error: %s.", relay_ip, vlc_strerror_c(errno));
    }
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
    chaSendBuffer[1] = sys->mcastGroupAddr.ipv4.sin_family == AF_INET6;
    chaSendBuffer[2] = 0;
    chaSendBuffer[3] = 0;

    ulNonce = sys->glob_ulNonce;
    memcpy( &chaSendBuffer[4], &ulNonce, sizeof(uint32_t) );

    nRet = send( sys->sAMT, chaSendBuffer, sizeof(chaSendBuffer), 0 );

    if( nRet < 0 )
    {
        msg_Err(p_access, "Error sending relay request to %s error: %s", relay_ip, vlc_strerror(errno) );
    }
}


/**
 * @brief Serialize an IPv6 Packet
 *
 * Params: the packet to serialize, a buffer to write it into and the maximum length of that buffer
 * Returns: the number of bytes written on success, else 0
 */

static int serialize_ipv6_pkt(amt_ipv6_t *ip, uint8_t *buf, int buflen){
    if ( buflen < IPv6_HOP_BY_HOP_OPTION_LEN + IPv6_FIXED_HDR_LEN )
    {
        return 0;
    }

    int i = 0;

    buf[i++] = (ip->version << 4) | (ip->traffic_class >> 4); /* first 4 bytes of tc */
    buf[i++] = (ip->traffic_class & 0xF) | ((ip->flow_label >> 16) & 0xF); /* bottom four bits of tc + "upper" 4 of flow_label (flow label is only 20 bits) */
    buf[i++] = ((ip->flow_label >> 8) & 0xFF);
    buf[i++] = (ip->flow_label & 0xFF);

    buf[i++] = ip->payload_len >> 16;
    buf[i++] = ip->payload_len & 0xFF;
    buf[i++] = ip->next_header;
    buf[i++] = ip->hop_limit;

    memcpy(&buf[i],&ip->srcAddr,16);
    i+= 16;
    memcpy(&buf[i],&ip->dstAddr,16);
    i+= 16;

    buf[i++] = ip->hop_by_hop_option.next_header;
    buf[i++] = ip->hop_by_hop_option.length;
    buf[i++] = ip->hop_by_hop_option.ra_type;
    buf[i++] = ip->hop_by_hop_option.ra_length;
    buf[i++] = ip->hop_by_hop_option.ra_router_alert >> 8;
    buf[i++] = ip->hop_by_hop_option.ra_router_alert & 0xFF;
    buf[i++] = ip->hop_by_hop_option.po_type;
    buf[i++] = ip->hop_by_hop_option.po_length;

    return i;
}

/**
 * @brief Serializes an MLD Report - computes and inserts checksum as well
 *
 * Params: report to serialize with an empty checksum, a buffer to write it into and the maximum length of that buffer
 * Returns: the number of bytes written on success, else 0
 */

static int serialize_mld_report(amt_mldv2_listener_report_t *report, uint8_t *buf, int buflen, stream_t *p_access){
    if ( buflen < MLD_REPORT_LEN )
    {
        return 0;
    }

    uint16_t tmp;
    int i = 0;
    buf[i++] = report->type;
    buf[i++] = report->code;
    buf[i++] = 0;/* checksum */
    buf[i++] = 0;/* skip checksum for now */
    buf[i++] = 0;/* reserved */
    buf[i++] = 0;/* reserved */
    buf[i++] = report->num_records >> 8; /* MSB first */
    buf[i++] = report->num_records & 0xFF;
    /* Address record */
    buf[i++] = report->records[0].record_type;
    buf[i++] = report->records[0].aux_data_len;
    buf[i++] = report->records[0].num_srcs >> 8;
    buf[i++] = report->records[0].num_srcs & 0xFF;
    memcpy(&buf[i],&report->records[0].mcast_address,16);
    i += 16;
    memcpy(&buf[i],&report->records[0].sources[0],16);
    i += 16;

    /* get checksum over buffer and write it back into the right place */
    tmp = htons(ipv6_checksum((unsigned short*)buf, buflen));
    msg_Dbg( p_access , "Ipv6 Checksum is 0x%x",tmp);
    buf[2] = tmp >> 8;
    buf[3] = tmp & 0xFF;

    return i;
}

/*
* create AMT request message format
* +----------------------------------------------------------------------------------+
* | Msg Type(1 byte)| Reserved (1 byte)| MAC (6 byte)| nonce (4 byte) | IGMP packet  |
* +----------------------------------------------------------------------------------+
*/
static int amt_send_mem_update( stream_t *p_access, bool leave)
{
    int           i_amt_hdr_len = MAC_LEN + NONCE_LEN + AMT_HDR_LEN;
    int           i_ipv6_hdr_len = IPv6_HOP_BY_HOP_OPTION_LEN + IPv6_FIXED_HDR_LEN;
    int           i_sendBufSize = i_amt_hdr_len + IP_HDR_IGMP_LEN;
    int           i_sendBufSizeIPv6 = i_amt_hdr_len + i_ipv6_hdr_len + MLD_REPORT_LEN;
    uint8_t       pSendBuffer[ i_sendBufSize > i_sendBufSizeIPv6 ? i_sendBufSize : i_sendBufSizeIPv6 ]; /* make the buffer as large as needed */
    uint32_t      ulNonce = 0;
    access_sys_t *sys = p_access->p_sys;

    memset( pSendBuffer, 0, sizeof(pSendBuffer) );

    pSendBuffer[0] = AMT_MEM_UPD;

    /* copy relay MAC response */
    memcpy( &pSendBuffer[2], sys->relay_mem_query_msg.uchaMAC, MAC_LEN );

    /* copy nonce */
    ulNonce = sys->glob_ulNonce;
    memcpy( &pSendBuffer[8], &ulNonce, NONCE_LEN );

    if ( sys->mcastGroupAddr.ipv4.sin_family == AF_INET )
    {
        /* make IP header for IGMP packet */
        amt_ip_alert_t p_ipHead;
        memset( &p_ipHead, 0, IP_HDR_IGMP_LEN );
        make_ip_header( &p_ipHead );

        struct sockaddr_in temp;
        int i_res = inet_pton( AF_INET, MCAST_ALLHOSTS, &(temp.sin_addr) );
        if( i_res != 1 )
        {
            msg_Err(p_access, "Could not convert all hosts multicast address: %s", gai_strerror(errno) );
            return -1;
        }
        p_ipHead.destAddr = temp.sin_addr.s_addr;
        p_ipHead.check = get_checksum( (unsigned short*)&p_ipHead, IP_HDR_IGMP_LEN );

        amt_igmpv3_groupRecord_t groupRcd;
        groupRcd.auxDatalen = 0;
        groupRcd.ssm = sys->mcastGroupAddr.ipv4.sin_addr.s_addr;

        if( sys->mcastSrcAddr.ipv4.sin_addr.s_addr )
        {
            groupRcd.type = leave ? AMT_IGMP_BLOCK:AMT_IGMP_INCLUDE;
            groupRcd.nSrc = htons(1);
            groupRcd.srcIP[0] = sys->mcastSrcAddr.ipv4.sin_addr.s_addr;

        }
        else
        {
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
        memcpy(&memUpdateMsg.encapsulated.igmp_memReport, &p_igmpMemRep, sizeof(p_igmpMemRep) );

        memcpy( &pSendBuffer[12], &memUpdateMsg, sizeof(memUpdateMsg.ipHead) + sizeof(memUpdateMsg.encapsulated.igmp_memReport) );
        send( sys->sAMT, pSendBuffer, i_sendBufSize + IGMP_REPORT_LEN , 0 );
    }
    else
    {

        amt_ipv6_t ip;
        memset( &ip, 0, sizeof(ip) );
        amt_mldv2_listener_report_t report;
        memset( &report, 0, sizeof(report) );

        struct in6_addr tmp;
        if( inet_pton( AF_INET6, MCAST_ALL_MLDv2_CAP_ROUTERS, &tmp ) != 1 )
        {
            msg_Err(p_access, "Could not convert all hosts multicast address: %s", gai_strerror(errno) );
            return -1;
        }

        make_ipv6(&ip, IPv6_HOP_BY_HOP_OPTION_LEN + MLD_REPORT_LEN, &tmp);

        if( make_mld_report(p_access,leave,&report,&sys->mcastGroupAddr.ipv6.sin6_addr,&sys->mcastSrcAddr.ipv6.sin6_addr) )
        {
            goto oom;
        }

        int i = serialize_ipv6_pkt(&ip,&pSendBuffer[i_amt_hdr_len],i_sendBufSizeIPv6 - MLD_REPORT_LEN - i_amt_hdr_len);
        if ( !i )
        {
            msg_Err( p_access, "Couldnt serialize ipv6 packet");
            goto fail;
        }

        if ( !serialize_mld_report(&report,&pSendBuffer[i_amt_hdr_len + i],MLD_REPORT_LEN,p_access) )
        {
            msg_Err( p_access, "Couldnt serialize mld report");
            goto fail;
        }

        send( sys->sAMT, pSendBuffer, i_sendBufSizeIPv6, 0 );

        vlc_obj_free( p_access, report.records[0].sources); /* allocd in the make_mld_report() function */
    }


    msg_Dbg( p_access, "AMT relay membership report sent");
    return 0;

    oom:
    msg_Err( p_access, "Out of Memory!");
    fail:
    return -1;
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

    ssize_t len = recvfrom( sys->sAMT, pkt, RELAY_ADV_MSG_LEN, 0, 0, 0 );

    if (len < 0)
    {
        msg_Err(p_access, "Received message length less than zero");
        return false;
    }

    /* AMT Relay Advertisement data (RFC7450) */
    uint32_t ulRcvNonce;
    uint8_t  type;

    memcpy( &type, &pkt[0], MSG_TYPE_LEN );
    if( type != AMT_RELAY_ADV )
    {
        msg_Err( p_access, "Received message not an AMT relay advertisement, ignoring. ");
        return false;
    }

    memcpy( &ulRcvNonce, &pkt[NONCE_LEN], NONCE_LEN );
    if( sys->glob_ulNonce != ulRcvNonce )
    {
        msg_Err( p_access, "Discovery nonces differ! currNonce: %x rcvd: %x", (uint32_t) htonl(sys->glob_ulNonce), (uint32_t) htonl(ulRcvNonce) );
        return false;
    }

    struct sockaddr_in relayAddr4;
    relayAddr4.sin_port = htons( AMT_PORT );
    relayAddr4.sin_family = AF_INET;

    struct sockaddr_in6 relayAddr6;
    relayAddr6.sin6_port = htons( AMT_PORT );
    relayAddr6.sin6_family = AF_INET6;

    void *bin_address_start = (sys->relayDiscoAddr.ipv4.sin_family == AF_INET) ? (void*) &relayAddr4.sin_addr : (void*) &relayAddr6.sin6_addr;
    memcpy( bin_address_start , &pkt[8], sys->relayDiscoAddr.ipv4.sin_family == AF_INET ? 4 : 16);

    int nRet = connect( sys->sAMT, sys->relayDiscoAddr.ipv4.sin_family == AF_INET ? (struct sockaddr*)&relayAddr4 : (struct sockaddr*)&relayAddr6, sys->relayDiscoAddr.ipv4.sin_family == AF_INET ? sizeof(relayAddr4) : sizeof(relayAddr6) );
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
    int i_buf_len = RELAY_QUERY_MSG_LEN + AMT_IPV6_MAX_NUM_SOURCES*16;
    char pkt[i_buf_len];
    memset( pkt, 0, i_buf_len);
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

    ssize_t len = recv( sys->sAMT, pkt, i_buf_len, 0 );

    if ( len <= 0 || (sys->mcastGroupAddr.ipv4.sin_family == AF_INET && len != (RELAY_QUERY_MSG_LEN - 40)) || (sys->mcastGroupAddr.ipv4.sin_family == AF_INET6 && len < RELAY_QUERY_MSG_LEN) ) /* subtract 40 for ipv4 case */
    {
        msg_Err(p_access, "length of relay query message invalid!");
        return false;
    }

    memcpy( &sys->relay_mem_query_msg.type, &pkt[0], MSG_TYPE_LEN );
    /* pkt[1] is reserved  */
    memcpy( &sys->relay_mem_query_msg.uchaMAC[0], &pkt[AMT_HDR_LEN], MAC_LEN );
    memcpy( &sys->relay_mem_query_msg.ulRcvedNonce, &pkt[AMT_HDR_LEN + MAC_LEN], NONCE_LEN );
    if( sys->relay_mem_query_msg.ulRcvedNonce != sys->glob_ulNonce )
    {
        msg_Warn( p_access, "Nonces are different rcvd: %x glob: %x", (uint32_t) htonl(sys->relay_mem_query_msg.ulRcvedNonce), (uint32_t) htonl(sys->glob_ulNonce) );
        return false;
    }

    if ( sys->mcastGroupAddr.ipv4.sin_family == AF_INET )
    {
        size_t shift = AMT_HDR_LEN + MAC_LEN + NONCE_LEN + IP_HDR_IGMP_LEN;
        sys->relay_query.igmp.type = pkt[shift];
        shift++; assert( shift < RELAY_QUERY_MSG_LEN);
        sys->relay_query.igmp.max_resp_code = pkt[shift];
        shift++; assert( shift < RELAY_QUERY_MSG_LEN);
        memcpy( &sys->relay_query.igmp.checksum, &pkt[shift], 2 );
        shift += 2; assert( shift < RELAY_QUERY_MSG_LEN);
        memcpy( &sys->relay_query.igmp.ssmIP, &pkt[shift], 4 );
        shift += 4; assert( shift < RELAY_QUERY_MSG_LEN);
        sys->relay_query.igmp.s_qrv = pkt[shift];
        shift++; assert( shift < RELAY_QUERY_MSG_LEN);
        if( pkt[shift] == 0 )
        {
            sys->relay_query.igmp.qqic = 125;
        }
        else
        {
            sys->relay_query.igmp.qqic = pkt[shift];
        }

        shift++; assert( shift < RELAY_QUERY_MSG_LEN);
        memcpy( &sys->relay_query.igmp.nSrc, &pkt[shift], 2 );
    }
    else
    {
        uint16_t temp_short;
        int offset = AMT_HDR_LEN + MAC_LEN + NONCE_LEN + IPv6_FIXED_HDR_LEN + IPv6_HOP_BY_HOP_OPTION_LEN;
        sys->relay_query.mld.type = pkt[offset++];
        sys->relay_query.mld.code = pkt[offset++];
        memcpy( &temp_short, &pkt[offset], 2 );
        sys->relay_query.mld.checksum = ntohs(temp_short);
        offset += 2;
        memcpy( &temp_short, &pkt[offset], 2 );
        sys->relay_query.mld.max_resp_code = ntohs(temp_short);
        offset += 4; /* += 2 for max resp code, += 2 more for reserved */
        memcpy( &sys->relay_query.mld.mcast_address, &pkt[offset], 16); /* ipv6 addr */
        offset += 16;
        sys->relay_query.mld.qrv = pkt[offset] & (7); /* bottom 3 bits */
        sys->relay_query.mld.s_flag = pkt[offset] & (1 << 3); /* 4th bit */
        offset++;
        sys->relay_query.mld.qqic = pkt[offset++];
        if ( !sys->relay_query.mld.qqic )
        {
            sys->relay_query.mld.qqic = 125;
        }
        memcpy( &temp_short, &pkt[offset], 2 );
        sys->relay_query.mld.num_srcs = ntohs(temp_short);
        offset += 2;
        if ( sys->relay_query.mld.num_srcs > AMT_IPV6_MAX_NUM_SOURCES )
        {
            msg_Err(p_access, "Too many source addresses in mld query - currently we only handle %d sources!",AMT_IPV6_MAX_NUM_SOURCES);
            return false;
        }
        sys->relay_query.mld.srcs = vlc_obj_calloc(p_access, sys->relay_query.mld.num_srcs, 16);
        if( unlikely( sys->relay_query.mld.srcs == NULL ) )
        {
            msg_Err(p_access, "AMT: Out of memory");
            return VLC_ENOMEM;
        }
        for ( int i = 0; i < sys->relay_query.mld.num_srcs; i++ )
        {
            struct in6_addr tmp;
            memcpy(&tmp,&pkt[offset],16);
            sys->relay_query.mld.srcs[i] = tmp;
            offset += 16;
            char src_buf [INET6_ADDRSTRLEN];
            msg_Dbg(p_access, "MLD Query- received multicast source address: %s",
            inet_ntop(AF_INET6,&tmp,src_buf,INET6_ADDRSTRLEN));
        }

    }

    return true;
}

/* A timer is spawned since IGMP membership updates need to issued periodically
 * in order to continue to receive multicast. */
static void amt_update_timer_cb( void *data )
{
    stream_t     *p_access = (stream_t*) data;
    access_sys_t *sys = p_access->p_sys;

    amt_send_mem_update( p_access, false );

    /* Arms the timer again for a single shot from this callback. That way, the
     * time spent in amt_send_mem_update() is taken into consideration. */
    vlc_timer_schedule( sys->updateTimer, false,
                        VLC_TICK_FROM_SEC( sys->mcastGroupAddr.ipv4.sin_family == AF_INET ? sys->relay_query.igmp.qqic : sys->relay_query.mld.qqic), 0 );
}
