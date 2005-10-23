/*****************************************************************************
 * sap.c : SAP announce handler
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Rémi Denis-Courmont <rem # videolan.org>
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
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <ctype.h>                                  /* tolower(), isxdigit() */

#include <vlc/vlc.h>
#include <vlc/sout.h>

#include "network.h"
#include "charset.h"

/* SAP is always on that port */
#define SAP_PORT 9875

#define DEFAULT_PORT "1234"

#undef EXTRA_DEBUG

/* SAP Specific structures */

/* 100ms */
#define SAP_IDLE ((mtime_t)(0.100*CLOCK_FREQ))
#define SAP_MAX_BUFFER 65534
#define MIN_INTERVAL 2
#define MAX_INTERVAL 300

/* A SAP announce address. For each of these, we run the
 * control flow algorithm */
struct sap_address_t
{
    char *psz_address;
    char psz_machine[NI_MAXNUMERICHOST];
    int i_port;
    int i_rfd; /* Read socket */
    int i_wfd; /* Write socket */

    /* Used for flow control */
    mtime_t t1;
    vlc_bool_t b_enabled;
    vlc_bool_t b_ready;
    int i_interval;
    int i_buff;
    int i_limit;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RunThread( vlc_object_t *p_this);
static int CalculateRate( sap_handler_t *p_sap, sap_address_t *p_address );
static char *SDPGenerate( sap_handler_t *p_sap,
                          const session_descriptor_t *p_session,
                          const sap_address_t *p_addr );

static int announce_SendSAPAnnounce( sap_handler_t *p_sap,
                                     sap_session_t *p_session );


static int announce_SAPAnnounceAdd( sap_handler_t *p_sap,
                             session_descriptor_t *p_session );

static int announce_SAPAnnounceDel( sap_handler_t *p_sap,
                             session_descriptor_t *p_session );

#define FREE( p ) if( p ) { free( p ); (p) = NULL; }


/**
 * Create the SAP handler
 *
 * \param p_announce the parent announce_handler
 * \return the newly created SAP handler or NULL on error
 */
sap_handler_t *announce_SAPHandlerCreate( announce_handler_t *p_announce )
{
    sap_handler_t *p_sap;

    p_sap = vlc_object_create( p_announce, sizeof( sap_handler_t ) );

    if( !p_sap )
    {
        msg_Err( p_announce, "out of memory" );
        return NULL;
    }

    vlc_mutex_init( p_sap, &p_sap->object_lock );

    p_sap->pf_add = announce_SAPAnnounceAdd;
    p_sap->pf_del = announce_SAPAnnounceDel;

    p_sap->i_sessions = 0;
    p_sap->i_addresses = 0;
    p_sap->i_current_session = 0;

    p_sap->b_control = config_GetInt( p_sap, "sap-flow-control");

    if( vlc_thread_create( p_sap, "sap handler", RunThread,
                       VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Dbg( p_announce, "Unable to spawn SAP handler thread");
        free( p_sap );
        return NULL;
    };
    msg_Dbg( p_announce, "thread created, %i sessions", p_sap->i_sessions);
    return p_sap;
}

/**
 *  Destroy the SAP handler
 *  \param p_this the SAP Handler to destroy
 *  \return nothing
 */
void announce_SAPHandlerDestroy( sap_handler_t *p_sap )
{
    int i;

    vlc_mutex_destroy( &p_sap->object_lock );

    /* Free the remaining sessions */
    for( i = 0 ; i< p_sap->i_sessions ; i++)
    {
        sap_session_t *p_session = p_sap->pp_sessions[i];
        FREE( p_session->psz_sdp );
        FREE( p_session->psz_data );
        REMOVE_ELEM( p_sap->pp_sessions, p_sap->i_sessions , i );
        FREE( p_session );
    }

    /* Free the remaining addresses */
    for( i = 0 ; i< p_sap->i_addresses ; i++)
    {
        sap_address_t *p_address = p_sap->pp_addresses[i];
        FREE( p_address->psz_address );
        if( p_address->i_rfd > -1 )
        {
            net_Close( p_address->i_rfd );
        }
        if( p_address->i_wfd > -1 && p_sap->b_control )
        {
            net_Close( p_address->i_wfd );
        }
        REMOVE_ELEM( p_sap->pp_addresses, p_sap->i_addresses, i );
        FREE( p_address );
    }

    /* Free the structure */
    vlc_object_destroy( p_sap );
}

/**
 * main SAP handler thread
 * \param p_this the SAP Handler object
 * \return nothing
 */
static void RunThread( vlc_object_t *p_this)
{
    sap_handler_t *p_sap = (sap_handler_t*)p_this;
    sap_session_t *p_session;

    while( !p_sap->b_die )
    {
        int i;

        /* If needed, get the rate info */
        if( p_sap->b_control == VLC_TRUE )
        {
            for( i = 0 ; i< p_sap->i_addresses ; i++)
            {
                if( p_sap->pp_addresses[i]->b_enabled == VLC_TRUE )
                {
                    CalculateRate( p_sap, p_sap->pp_addresses[i] );
                }
            }
        }

        /* Find the session to announce */
        vlc_mutex_lock( &p_sap->object_lock );
        if( p_sap->i_sessions > p_sap->i_current_session + 1)
        {
            p_sap->i_current_session++;
        }
        else if( p_sap->i_sessions > 0)
        {
            p_sap->i_current_session = 0;
        }
        else
        {
            vlc_mutex_unlock( &p_sap->object_lock );
            msleep( SAP_IDLE );
            continue;
        }
        p_session = p_sap->pp_sessions[p_sap->i_current_session];
        vlc_mutex_unlock( &p_sap->object_lock );

        /* And announce it */
        if( p_session->p_address->b_enabled == VLC_TRUE &&
            p_session->p_address->b_ready == VLC_TRUE )
        {
            announce_SendSAPAnnounce( p_sap, p_session );
        }

        msleep( SAP_IDLE );
    }
}

/* Add a SAP announce */
static int announce_SAPAnnounceAdd( sap_handler_t *p_sap,
                             session_descriptor_t *p_session )
{
    int i_header_size, i;
    char *psz_head, psz_addr[NI_MAXNUMERICHOST];
    vlc_bool_t b_ipv6 = VLC_FALSE;
    sap_session_t *p_sap_session;
    mtime_t i_hash;
    struct addrinfo hints, *res;
    struct sockaddr_storage addr;

    vlc_mutex_lock( &p_sap->object_lock );

    if( p_session->psz_uri == NULL && p_session->psz_sdp == NULL )
    {
        vlc_mutex_unlock( &p_sap->object_lock );
        msg_Err( p_sap, "Trying to create a NULL SAP announce" );
        return VLC_EGENERIC;
    }

    /* Determine SAP multicast address automatically */
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICHOST;

    i = vlc_getaddrinfo( (vlc_object_t *)p_sap, p_session->psz_uri, 0,
                         &hints, &res );
    if( i )
    {
        vlc_mutex_unlock( &p_sap->object_lock );
        msg_Err( p_sap, "Invalid URI for SAP announce: %s: %s",
                 p_session->psz_uri, vlc_gai_strerror( i ) );
        return VLC_EGENERIC;
    }

    if( (unsigned)res->ai_addrlen > sizeof( addr ) )
    {
        vlc_mutex_unlock( &p_sap->object_lock );
        vlc_freeaddrinfo( res );
        msg_Err( p_sap, "Unsupported address family of size %d > %u",
                 res->ai_addrlen, (unsigned) sizeof( addr ) );
        return VLC_EGENERIC;
    }

    memcpy( &addr, res->ai_addr, res->ai_addrlen );

    switch( addr.ss_family )
    {
#if defined (HAVE_INET_PTON) || defined (WIN32)
        case AF_INET6:
        {
            /* See RFC3513 for list of valid IPv6 scopes */
            struct in6_addr *a6 = &((struct sockaddr_in6 *)&addr)->sin6_addr;

            memcpy( a6->s6_addr + 2, "\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x02\x7f\xfe", 14 );
            if( IN6_IS_ADDR_MULTICAST( a6 ) )
                 /* force flags to zero, preserve scope */
                a6->s6_addr[1] &= 0xf;
            else
                /* Unicast IPv6 - assume global scope */
                memcpy( a6->s6_addr, "\xff\x0e", 2 );

            b_ipv6 = VLC_TRUE;
            break;
        }
#endif

        case AF_INET:
        {
            /* See RFC2365 for IPv4 scopes */
            uint32_t ipv4;

            ipv4 = ntohl( ((struct sockaddr_in *)&addr)->sin_addr.s_addr );
            /* 224.0.0.0/24 => 224.0.0.255 */
            if ((ipv4 & 0xffffff00) == 0xe0000000)
                ipv4 =  0xe00000ff;
            else
            /* 239.255.0.0/16 => 239.255.255.255 */
            if ((ipv4 & 0xffff0000) == 0xefff0000)
                ipv4 =  0xefffffff;
            else
            /* 239.192.0.0/14 => 239.195.255.255 */
            if ((ipv4 & 0xfffc0000) == 0xefc00000)
                ipv4 =  0xefc3ffff;
            else
            /* other addresses => 224.2.127.254 */
                ipv4 = 0xe0027ffe;

            ((struct sockaddr_in *)&addr)->sin_addr.s_addr = htonl( ipv4 );
            break;
        }
        
        default:
            vlc_mutex_unlock( &p_sap->object_lock );
            vlc_freeaddrinfo( res );
            msg_Err( p_sap, "Address family %d not supported by SAP",
                     addr.ss_family );
            return VLC_EGENERIC;
    }

    i = vlc_getnameinfo( (struct sockaddr *)&addr, res->ai_addrlen,
                         psz_addr, sizeof( psz_addr ), NULL, NI_NUMERICHOST );
    vlc_freeaddrinfo( res );

    if( i )
    {
        vlc_mutex_unlock( &p_sap->object_lock );
        msg_Err( p_sap, "%s", vlc_gai_strerror( i ) );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_sap, "using SAP address: %s", psz_addr);

    /* XXX: Check for dupes */
    p_sap_session = (sap_session_t*)malloc(sizeof(sap_session_t));
    p_sap_session->p_address = NULL;

    /* Add the address to the buffer */
    for( i = 0; i < p_sap->i_addresses; i++)
    {
        if( !strcmp( psz_addr, p_sap->pp_addresses[i]->psz_address ) )
        {
            p_sap_session->p_address = p_sap->pp_addresses[i];
            break;
        }
    }

    if( p_sap_session->p_address == NULL )
    {
        sap_address_t *p_address = (sap_address_t *)
                                    malloc( sizeof(sap_address_t) );
        if( !p_address )
        {
            msg_Err( p_sap, "out of memory" );
            return VLC_ENOMEM;
        }
        p_address->psz_address = strdup( psz_addr );
        p_address->i_port  =  9875;
        p_address->i_wfd = net_OpenUDP( p_sap, "", 0, psz_addr,
                                        p_address->i_port );
        if( p_address->i_wfd != -1 )
        {
            char *ptr;

            net_StopRecv( p_address->i_wfd );
            net_GetSockAddress( p_address->i_wfd, p_address->psz_machine,
                                NULL );

            /* removes scope if present */
            ptr = strchr( p_address->psz_machine, '%' );
            if( ptr != NULL )
                *ptr = '\0';
        }

        if( p_sap->b_control == VLC_TRUE )
        {
            p_address->i_rfd = net_OpenUDP( p_sap, psz_addr,
                                            p_address->i_port, "", 0 );
            if( p_address->i_rfd != -1 )
                net_StopSend( p_address->i_rfd );
            p_address->i_buff = 0;
            p_address->b_enabled = VLC_TRUE;
            p_address->b_ready = VLC_FALSE;
            p_address->i_limit = 10000; /* 10000 bps */
            p_address->t1 = 0;
        }
        else
        {
            p_address->b_enabled = VLC_TRUE;
            p_address->b_ready = VLC_TRUE;
            p_address->i_interval = config_GetInt( p_sap,"sap-interval");
            p_address->i_rfd = -1;
        }

        if( p_address->i_wfd == -1 || (p_address->i_rfd == -1
                                        && p_sap->b_control ) )
        {
            msg_Warn( p_sap, "disabling address" );
            p_address->b_enabled = VLC_FALSE;
        }

        INSERT_ELEM( p_sap->pp_addresses,
                     p_sap->i_addresses,
                     p_sap->i_addresses,
                     p_address );
        p_sap_session->p_address = p_address;
    }


    /* Build the SAP Headers */
    i_header_size = ( b_ipv6 ? 16 : 4 ) + 20;
    psz_head = (char *) malloc( i_header_size * sizeof( char ) );
    if( psz_head == NULL )
    {
        msg_Err( p_sap, "out of memory" );
        return VLC_ENOMEM;
    }

    /* SAPv1, not encrypted, not compressed */
    psz_head[0] = b_ipv6 ? 0x30 : 0x20;
    psz_head[1] = 0x00; /* No authentification length */

    i_hash = mdate();
    psz_head[2] = (i_hash & 0xFF00) >> 8; /* Msg id hash */
    psz_head[3] = (i_hash & 0xFF);        /* Msg id hash 2 */

#if defined (HAVE_INET_PTON) || defined (WIN32)
    if( b_ipv6 )
    {
        inet_pton( AF_INET6, /* can't fail */
                   p_sap_session->p_address->psz_machine,
                   psz_head + 4 );
    }
    else
#else
    {
        inet_pton( AF_INET, /* can't fail */
                   p_sap_session->p_address->psz_machine,
                   psz_head + 4 );
    }
#endif

    memcpy( psz_head + (b_ipv6 ? 20 : 8), "application/sdp", 15 );

    /* If needed, build the SDP */
    if( p_session->psz_sdp == NULL )
    {
        p_session->psz_sdp = SDPGenerate( p_sap, p_session,
                                          p_sap_session->p_address );
        if( p_session->psz_sdp == NULL )
        {
            vlc_mutex_unlock( &p_sap->object_lock );
            return VLC_ENOMEM;
        }
    }

    p_sap_session->psz_sdp = strdup( p_session->psz_sdp );
    p_sap_session->i_last = 0;

    psz_head[ i_header_size-1 ] = '\0';
    p_sap_session->i_length = i_header_size + strlen( p_sap_session->psz_sdp);

    p_sap_session->psz_data = (uint8_t *)malloc( sizeof(char)*
                                                 p_sap_session->i_length );

    /* Build the final message */
    memcpy( p_sap_session->psz_data, psz_head, i_header_size );
    memcpy( p_sap_session->psz_data+i_header_size, p_sap_session->psz_sdp,
            strlen( p_sap_session->psz_sdp) );

    free( psz_head );

    /* Enqueue the announce */
    INSERT_ELEM( p_sap->pp_sessions,
                 p_sap->i_sessions,
                 p_sap->i_sessions,
                 p_sap_session );
    msg_Dbg( p_sap,"Addresses: %i  Sessions: %i",
                   p_sap->i_addresses,p_sap->i_sessions);

    /* Remember the SAP session for later deletion */
    p_session->p_sap = p_sap_session;

    vlc_mutex_unlock( &p_sap->object_lock );

    return VLC_SUCCESS;
}

/* Remove a SAP Announce */
static int announce_SAPAnnounceDel( sap_handler_t *p_sap,
                             session_descriptor_t *p_session )
{
    int i;
    vlc_mutex_lock( &p_sap->object_lock );

    msg_Dbg( p_sap,"removing SAP announce %p",p_session->p_sap);

    /* Dequeue the announce */
    for( i = 0; i< p_sap->i_sessions; i++)
    {
        if( p_session->p_sap == p_sap->pp_sessions[i] )
        {
            REMOVE_ELEM( p_sap->pp_sessions,
                         p_sap->i_sessions,
                         i );

            FREE( p_session->p_sap->psz_sdp );
            FREE( p_session->p_sap->psz_data );
            free( p_session->p_sap );
            break;
        }
    }

    /* XXX: Dequeue the address too if it is not used anymore
     * TODO: - address refcount
             - send a SAP deletion packet */

    msg_Dbg( p_sap,"%i announcements remaining", p_sap->i_sessions );

    vlc_mutex_unlock( &p_sap->object_lock );

    return VLC_SUCCESS;
}

static int announce_SendSAPAnnounce( sap_handler_t *p_sap,
                                     sap_session_t *p_session )
{
    int i_ret;

    /* This announce has never been sent yet */
    if( p_session->i_last == 0 )
    {
        p_session->i_next = mdate()+ p_session->p_address->i_interval*1000000;
        p_session->i_last = 1;
        return VLC_SUCCESS;
    }

    if( p_session->i_next < mdate() )
    {
#ifdef EXTRA_DEBUG
        msg_Dbg( p_sap, "Sending announce");
#endif
        i_ret = net_Write( p_sap, p_session->p_address->i_wfd, NULL,
                           p_session->psz_data,
                           p_session->i_length );
        if( i_ret != p_session->i_length )
        {
            msg_Warn( p_sap, "SAP send failed on address %s (%i %i)",
                      p_session->p_address->psz_address,
                      i_ret, p_session->i_length );
        }
        p_session->i_last = p_session->i_next;
        p_session->i_next = p_session->i_last
                            + p_session->p_address->i_interval*1000000;
    }
    else
    {
        return VLC_SUCCESS;
    }
    return VLC_SUCCESS;
}

static char *SDPGenerate( sap_handler_t *p_sap,
                          const session_descriptor_t *p_session,
                          const sap_address_t *p_addr )
{
    int64_t i_sdp_id = mdate();
    int     i_sdp_version = 1 + p_sap->i_sessions + (rand()&0xfff);
    char *psz_group, *psz_name, psz_uribuf[NI_MAXNUMERICHOST], *psz_uri,
         *psz_sdp;
    char ipv;

    psz_group = p_session->psz_group;
    psz_name = p_session->psz_name;

    /* FIXME: really check that psz_uri is a real IP address
     * FIXME: make a common function to obtain a canonical IP address */
    ipv = ( strchr( p_session->psz_uri, ':' )  != NULL) ? '6' : '4';
    if( *p_session->psz_uri == '[' )
    {
        char *ptr;

        strncpy( psz_uribuf, p_session->psz_uri + 1, sizeof( psz_uribuf ) );
        psz_uribuf[sizeof( psz_uribuf ) - 1] = '\0';
        ptr = strchr( psz_uribuf, '%' );
        if( ptr != NULL)
            *ptr = '\0';
        ptr = strchr( psz_uribuf, ']' );
        if( ptr != NULL)
            *ptr = '\0';
        psz_uri = psz_uribuf;
    }
    else
        psz_uri = p_session->psz_uri;

    /* see the lists in modules/stream_out/rtp.c for compliance stuff */
    if( asprintf( &psz_sdp,
                            "v=0\r\n"
                            "o=- "I64Fd" %d IN IP%c %s\r\n"
                            "s=%s\r\n"
                            "t=0 0\r\n"
                            "c=IN IP%c %s/%d\r\n"
                            "m=video %d %s %d\r\n"
                            "a=tool:"PACKAGE_STRING"\r\n"
                            "a=type:broadcast\r\n"
                            "%s%s%s",
                            i_sdp_id, i_sdp_version,
                            ipv, p_addr->psz_machine,
                            psz_name, ipv, psz_uri, p_session->i_ttl,
                            p_session->i_port, 
                            p_session->b_rtp ? "RTP/AVP" : "udp",
                            p_session->i_payload,
                            psz_group ? "a=x-plgroup:" : "",
                            psz_group ? psz_group : "", psz_group ? "\r\n" : "" ) == -1 )
        return NULL;
    
    msg_Dbg( p_sap, "Generated SDP (%i bytes):\n%s", strlen(psz_sdp),
             psz_sdp );
    return psz_sdp;
}

static int CalculateRate( sap_handler_t *p_sap, sap_address_t *p_address )
{
    int i_read;
    uint8_t buffer[SAP_MAX_BUFFER];
    int i_tot = 0;
    mtime_t i_temp;
    int i_rate;

    if( p_address->t1 == 0 )
    {
        p_address->t1 = mdate();
        return VLC_SUCCESS;
    }
    do
    {
        /* Might be too slow if we have huge data */
        i_read = net_ReadNonBlock( p_sap, p_address->i_rfd, NULL, buffer,
                                   SAP_MAX_BUFFER, 0 );
        i_tot += i_read;
    } while( i_read > 0 && i_tot < SAP_MAX_BUFFER );

    i_temp = mdate();

    /* We calculate the rate every 5 seconds */
    if( i_temp - p_address->t1 < 5000000 )
    {
        p_address->i_buff += i_tot;
        return VLC_SUCCESS;
    }

    /* Bits/second */
    i_rate = (int)(8*1000000*((mtime_t)p_address->i_buff + (mtime_t)i_tot ) /
                        (i_temp - p_address->t1 ));

    p_address->i_limit = 10000;

    p_address->i_interval = ((1000*i_rate / p_address->i_limit) *
                            (MAX_INTERVAL - MIN_INTERVAL))/1000 + MIN_INTERVAL;

    if( p_address->i_interval > MAX_INTERVAL || p_address->i_interval < 0 )
    {
        p_address->i_interval = MAX_INTERVAL;
    }
#ifdef EXTRA_DEBUG
    msg_Dbg( p_sap,"%s:%i : Rate=%i, Interval = %i s",
                    p_address->psz_address,p_address->i_port,
                    i_rate,
                    p_address->i_interval );
#endif

    p_address->b_ready = VLC_TRUE;

    p_address->t1 = i_temp;
    p_address->i_buff = 0;

    return VLC_SUCCESS;
}
