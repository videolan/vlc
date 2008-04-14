/*****************************************************************************
 * sap.c : SAP announce handler
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>
#include <ctype.h>                                  /* tolower(), isxdigit() */
#include <assert.h>

#include <vlc_sout.h>
#include <vlc_network.h>
#include <vlc_charset.h>

#include "stream_output.h"
#include "libvlc.h"

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
    struct sockaddr_storage orig;
    socklen_t origlen;
    int i_rfd; /* Read socket */
    int i_wfd; /* Write socket */

    /* Used for flow control */
    mtime_t t1;
    bool b_enabled;
    bool b_ready;
    int i_interval;
    int i_buff;
    int i_limit;
};

/* A SAP session descriptor, enqueued in the SAP handler queue */
struct sap_session_t {
    uint8_t       *psz_data;
    unsigned      i_length;
    sap_address_t *p_address;
    session_descriptor_t *p_sd;

    /* Last and next send */
    mtime_t        i_last;
    mtime_t        i_next;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RunThread( vlc_object_t *p_this);
static int ComputeRate( sap_address_t *p_address );

static int announce_SendSAPAnnounce( sap_handler_t *p_sap,
                                     sap_session_t *p_session );


static int announce_SAPAnnounceAdd( sap_handler_t *p_sap,
                             session_descriptor_t *p_session );

static int announce_SAPAnnounceDel( sap_handler_t *p_sap,
                             session_descriptor_t *p_session );

static void announce_SAPHandlerDestructor( vlc_object_t *p_this );


/**
 * Create the SAP handler
 *
 * \param p_announce the parent announce_handler
 * \return the newly created SAP handler or NULL on error
 */
sap_handler_t *announce_SAPHandlerCreate( announce_handler_t *p_announce )
{
    sap_handler_t *p_sap;

    p_sap = vlc_custom_create( VLC_OBJECT(p_announce), sizeof( sap_handler_t ),
                               VLC_OBJECT_ANNOUNCE, "announce" );
    if( !p_sap )
    {
        msg_Err( p_announce, "out of memory" );
        return NULL;
    }

    p_sap->psz_object_name = "sap announcer";

    vlc_mutex_init( p_sap, &p_sap->object_lock );

    p_sap->pf_add = announce_SAPAnnounceAdd;
    p_sap->pf_del = announce_SAPAnnounceDel;

    p_sap->i_sessions = 0;
    p_sap->i_addresses = 0;
    p_sap->i_current_session = 0;

    p_sap->b_control = config_GetInt( p_sap, "sap-flow-control");

    if( vlc_thread_create( p_sap, "sap handler", RunThread,
                       VLC_THREAD_PRIORITY_LOW, false ) )
    {
        msg_Dbg( p_announce, "unable to spawn SAP handler thread");
        vlc_object_release( p_sap );
        return NULL;
    }

    vlc_object_set_destructor( p_sap, announce_SAPHandlerDestructor );

    msg_Dbg( p_announce, "thread created, %i sessions", p_sap->i_sessions);

    return p_sap;
}

static void announce_SAPHandlerDestructor( vlc_object_t * p_this )
{
    sap_handler_t *p_sap = (sap_handler_t *)p_this;
    int i;

    /* Free the remaining sessions */
    for( i = 0 ; i< p_sap->i_sessions ; i++)
    {
        sap_session_t *p_session = p_sap->pp_sessions[i];
        FREENULL( p_session->psz_data );
        REMOVE_ELEM( p_sap->pp_sessions, p_sap->i_sessions , i );
        FREENULL( p_session );
    }

    /* Free the remaining addresses */
    for( i = 0 ; i< p_sap->i_addresses ; i++)
    {
        sap_address_t *p_address = p_sap->pp_addresses[i];
        FREENULL( p_address->psz_address );
        if( p_address->i_rfd > -1 )
        {
            net_Close( p_address->i_rfd );
        }
        if( p_address->i_wfd > -1 && p_sap->b_control )
        {
            net_Close( p_address->i_wfd );
        }
        REMOVE_ELEM( p_sap->pp_addresses, p_sap->i_addresses, i );
        FREENULL( p_address );
    }
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
        if( p_sap->b_control == true )
        {
            for( i = 0 ; i< p_sap->i_addresses ; i++)
            {
                if( p_sap->pp_addresses[i]->b_enabled == true )
                {
                    ComputeRate( p_sap->pp_addresses[i] );
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
        if( p_session->p_address->b_enabled == true &&
            p_session->p_address->b_ready == true )
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
    int i;
    char psz_addr[NI_MAXNUMERICHOST];
    bool b_ipv6 = false, b_ssm = false;
    sap_session_t *p_sap_session;
    mtime_t i_hash;
    struct sockaddr_storage addr;
    socklen_t addrlen;

    vlc_mutex_lock( &p_sap->object_lock );
    addrlen = p_session->addrlen;
    if ((addrlen == 0) || (addrlen > sizeof (addr)))
    {
        vlc_mutex_unlock( &p_sap->object_lock );
        msg_Err( p_sap, "No/invalid address specified for SAP announce" );
        return VLC_EGENERIC;
    }

    /* Determine SAP multicast address automatically */
    memcpy (&addr, &p_session->addr, addrlen);

    switch( p_session->addr.ss_family )
    {
#if defined (HAVE_INET_PTON) || defined (WIN32)
        case AF_INET6:
        {
            /* See RFC3513 for list of valid IPv6 scopes */
            struct in6_addr *a6 = &((struct sockaddr_in6 *)&addr)->sin6_addr;

            memcpy( a6->s6_addr + 2, "\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x02\x7f\xfe", 14 );
            if( IN6_IS_ADDR_MULTICAST( a6 ) )
            {
                /* SSM <=> ff3x::/32 */
                b_ssm = (U32_AT (a6->s6_addr) & 0xfff0ffff) == 0xff300000;

                /* force flags to zero, preserve scope */
                a6->s6_addr[1] &= 0xf;
            }
            else
                /* Unicast IPv6 - assume global scope */
                memcpy( a6->s6_addr, "\xff\x0e", 2 );

            b_ipv6 = true;
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
            if ((ipv4 & 0xff000000) == 0xef000000)
                ipv4 = 0;
            else
            /* other addresses => 224.2.127.254 */
            {
                /* SSM: 232.0.0.0/8 */
                b_ssm = (ipv4 >> 24) == 232;
                ipv4 = 0xe0027ffe;
            }

            if( ipv4 == 0 )
            {
                msg_Err( p_sap, "Out-of-scope multicast address "
                         "not supported by SAP" );
                vlc_mutex_unlock( &p_sap->object_lock );
                return VLC_EGENERIC;
            }

            ((struct sockaddr_in *)&addr)->sin_addr.s_addr = htonl( ipv4 );
            break;
        }

        default:
            vlc_mutex_unlock( &p_sap->object_lock );
            msg_Err( p_sap, "Address family %d not supported by SAP",
                     addr.ss_family );
            return VLC_EGENERIC;
    }

    i = vlc_getnameinfo( (struct sockaddr *)&addr, addrlen,
                         psz_addr, sizeof( psz_addr ), NULL, NI_NUMERICHOST );

    if( i )
    {
        vlc_mutex_unlock( &p_sap->object_lock );
        msg_Err( p_sap, "%s", vlc_gai_strerror( i ) );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_sap, "using SAP address: %s", psz_addr);

    /* XXX: Check for dupes */
    p_sap_session = (sap_session_t*)malloc(sizeof(sap_session_t));
    p_sap_session->p_sd = p_session;
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
            vlc_mutex_unlock( &p_sap->object_lock );
            return VLC_ENOMEM;
        }
        p_address->psz_address = strdup( psz_addr );
        p_address->i_wfd = net_ConnectUDP( VLC_OBJECT(p_sap), psz_addr, SAP_PORT, 255 );
        if( p_address->i_wfd != -1 )
        {
            shutdown( p_address->i_wfd, SHUT_RD );
            p_address->origlen = sizeof (p_address->orig);
            getsockname (p_address->i_wfd, (struct sockaddr *)&p_address->orig,
                         &p_address->origlen);
        }

        if( p_sap->b_control == true )
        {
            p_address->i_rfd = net_ListenUDP1( (vlc_object_t*)p_sap, psz_addr, SAP_PORT );
            if( p_address->i_rfd != -1 )
                shutdown( p_address->i_rfd, SHUT_WR );
            p_address->i_buff = 0;
            p_address->b_enabled = true;
            p_address->b_ready = false;
            p_address->i_limit = 10000; /* 10000 bps */
            p_address->t1 = 0;
        }
        else
        {
            p_address->b_enabled = true;
            p_address->b_ready = true;
            p_address->i_interval = config_GetInt( p_sap,"sap-interval");
            p_address->i_rfd = -1;
        }

        if( p_address->i_wfd == -1 || (p_address->i_rfd == -1
                                        && p_sap->b_control ) )
        {
            msg_Warn( p_sap, "disabling address" );
            p_address->b_enabled = false;
        }

        INSERT_ELEM( p_sap->pp_addresses,
                     p_sap->i_addresses,
                     p_sap->i_addresses,
                     p_address );
        p_sap_session->p_address = p_address;
    }

    memcpy (&p_session->orig, &p_sap_session->p_address->orig,
             p_session->origlen = p_sap_session->p_address->origlen);

    size_t headsize = 20;
    switch (p_session->orig.ss_family)
    {
#ifdef AF_INET6
        case AF_INET6:
            headsize += 16;
            break;
#endif
        case AF_INET:
            headsize += 4;
            break;
        default:
            msg_Err( p_sap, "Address family %d not supported by SAP",
                     addr.ss_family );
            vlc_mutex_unlock( &p_sap->object_lock );
            return VLC_EGENERIC;
    }

    /* If needed, build the SDP */
    assert( p_session->psz_sdp != NULL );

    p_sap_session->i_last = 0;
    p_sap_session->i_length = headsize + strlen (p_session->psz_sdp);
    p_sap_session->psz_data = malloc (p_sap_session->i_length + 1);
    if (p_sap_session->psz_data == NULL)
    {
        free (p_session->psz_sdp);
        vlc_mutex_unlock( &p_sap->object_lock );
        return VLC_ENOMEM;
    }

    /* Build the SAP Headers */
    uint8_t *psz_head = p_sap_session->psz_data;

    /* SAPv1, not encrypted, not compressed */
    psz_head[0] = 0x20;
    psz_head[1] = 0x00; /* No authentification length */

    i_hash = mdate();
    psz_head[2] = i_hash >> 8; /* Msg id hash */
    psz_head[3] = i_hash;      /* Msg id hash 2 */

    headsize = 4;
    switch (p_session->orig.ss_family)
    {
#ifdef AF_INET6
        case AF_INET6:
        {
            struct in6_addr *a6 =
                &((struct sockaddr_in6 *)&p_session->orig)->sin6_addr;
            memcpy (psz_head + headsize, a6, 16);
            psz_head[0] |= 0x10; /* IPv6 flag */
            headsize += 16;
            break;
        }
#endif
        case AF_INET:
        {
            uint32_t ipv4 =
                (((struct sockaddr_in *)&p_session->orig)->sin_addr.s_addr);
            memcpy (psz_head + headsize, &ipv4, 4);
            headsize += 4;
            break;
        }

    }

    memcpy (psz_head + headsize, "application/sdp", 16);
    headsize += 16;

    /* Build the final message */
    strcpy( (char *)psz_head + headsize, p_session->psz_sdp);

    /* Enqueue the announce */
    INSERT_ELEM( p_sap->pp_sessions,
                 p_sap->i_sessions,
                 p_sap->i_sessions,
                 p_sap_session );
    msg_Dbg( p_sap,"%i addresses, %i sessions",
                   p_sap->i_addresses,p_sap->i_sessions);

    vlc_mutex_unlock( &p_sap->object_lock );

    return VLC_SUCCESS;
}

/* Remove a SAP Announce */
static int announce_SAPAnnounceDel( sap_handler_t *p_sap,
                             session_descriptor_t *p_session )
{
    int i;
    vlc_mutex_lock( &p_sap->object_lock );

    msg_Dbg( p_sap, "removing session %p from SAP", p_session);

    /* Dequeue the announce */
    for( i = 0; i< p_sap->i_sessions; i++)
    {
        if( p_session == p_sap->pp_sessions[i]->p_sd )
        {
            sap_session_t *p_mysession = p_sap->pp_sessions[i];
            REMOVE_ELEM( p_sap->pp_sessions,
                         p_sap->i_sessions,
                         i );

            free( p_mysession->psz_data );
            free( p_mysession );
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
        msg_Dbg( p_sap, "sending announce");
#endif
        i_ret = net_Write( p_sap, p_session->p_address->i_wfd, NULL,
                           p_session->psz_data,
                           p_session->i_length );
        if( i_ret != (int)p_session->i_length )
        {
            msg_Warn( p_sap, "SAP send failed on address %s (%i %i)",
                      p_session->p_address->psz_address,
                      i_ret, p_session->i_length );
        }
        p_session->i_last = p_session->i_next;
        p_session->i_next = p_session->i_last
                            + p_session->p_address->i_interval*1000000;
    }
    return VLC_SUCCESS;
}

static int ComputeRate( sap_address_t *p_address )
{
    uint8_t buffer[SAP_MAX_BUFFER];
    ssize_t i_tot = 0;
    mtime_t i_temp;
    int i_rate;

    if( p_address->t1 == 0 )
    {
        p_address->t1 = mdate();
        return VLC_SUCCESS;
    }
    for (;;)
    {
        /* Might be too slow if we have huge data */
        ssize_t i_read = recv( p_address->i_rfd, buffer, SAP_MAX_BUFFER, 0 );
        if (i_read == -1)
            break;
        i_tot += i_read;
    }

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
    msg_Dbg( p_sap,"%s:%i: rate=%i, interval = %i s",
             p_address->psz_address,SAP_PORT, i_rate, p_address->i_interval );
#endif

    p_address->b_ready = true;

    p_address->t1 = i_temp;
    p_address->i_buff = 0;

    return VLC_SUCCESS;
}
