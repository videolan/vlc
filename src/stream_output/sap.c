/*****************************************************************************
 * sap.c : SAP announce handler
 *****************************************************************************
 * Copyright (C) 2002-2008 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>
#include <assert.h>

#include <vlc_sout.h>
#include <vlc_network.h>
#include <vlc_memstream.h>

#include "stream_output.h"
#include "libvlc.h"

/* SAP is always on that port */
#define IPPORT_SAP 9875

struct sap_address_t;

/* A SAP session descriptor, enqueued in the SAP handler queue */
struct session_descriptor_t
{
    struct vlc_list node;
    struct sap_address_t *addr;
    size_t length;
    char *data;
};

/* A SAP announce address. For each of these, we run the
 * control flow algorithm */
typedef struct sap_address_t
{
    struct vlc_list         node;

    vlc_thread_t            thread;
    vlc_cond_t              wait;

    char                    group[NI_MAXNUMERICHOST];
    struct sockaddr_storage orig;
    socklen_t               origlen;
    int                     fd;
    unsigned                interval;

    unsigned                session_count;
    struct vlc_list         sessions;
} sap_address_t;

static struct vlc_list sap_addrs = VLC_LIST_INITIALIZER(&sap_addrs);
static vlc_mutex_t sap_mutex = VLC_STATIC_MUTEX;

#define SAP_MAX_BUFFER 65534
#define MIN_INTERVAL 2
#define MAX_INTERVAL 300

static void *RunThread (void *);

static sap_address_t *AddressCreate (vlc_object_t *obj, const char *group)
{
    int fd = net_ConnectUDP (obj, group, IPPORT_SAP, 255);
    if (fd == -1)
        return NULL;

    sap_address_t *addr = malloc (sizeof (*addr));
    if (addr == NULL)
    {
        net_Close (fd);
        return NULL;
    }

    strlcpy (addr->group, group, sizeof (addr->group));
    addr->fd = fd;
    addr->origlen = sizeof (addr->orig);
    getsockname (fd, (struct sockaddr *)&addr->orig, &addr->origlen);

    addr->interval = var_CreateGetInteger (obj, "sap-interval");
    vlc_cond_init (&addr->wait);
    addr->session_count = 0;
    vlc_list_init(&addr->sessions);
    return addr;
}

static void AddressDestroy (sap_address_t *addr)
{
    net_Close (addr->fd);
    free (addr);
}

/**
 * main SAP handler thread
 * \param p_this the SAP Handler object
 * \return nothing
 */
static void *RunThread (void *self)
{
    sap_address_t *addr = self;

    vlc_mutex_lock(&sap_mutex);

    while (!vlc_list_is_empty(&addr->sessions))
    {
        session_descriptor_t *p_session;
        vlc_tick_t deadline = vlc_tick_now();

        assert (addr->session_count > 0);

        vlc_list_foreach (p_session, &addr->sessions, node)
        {
            send (addr->fd, p_session->data, p_session->length, 0);
            deadline += vlc_tick_from_samples(addr->interval, addr->session_count);

            if (vlc_cond_timedwait(&addr->wait, &sap_mutex, deadline) == 0)
                break; /* list may have changed! */
        }
    }

    vlc_mutex_unlock(&sap_mutex);
    return NULL;
}

#undef sout_AnnounceRegisterSDP
/**
 *  Registers a new session with the announce handler, using a pregenerated SDP
 *
 * \param obj a VLC object
 * \param sdp the SDP to register
 * \param dst session address (needed for SAP address auto detection)
 * \return the new session descriptor structure
 */
session_descriptor_t *
sout_AnnounceRegisterSDP (vlc_object_t *obj, const char *sdp,
                          const char *dst)
{
    int i;
    char psz_addr[NI_MAXNUMERICHOST];
    union
    {
        struct sockaddr     a;
        struct sockaddr_in  in;
        struct sockaddr_in6 in6;
    } addr;
    socklen_t addrlen = 0;
    struct addrinfo *res;

    msg_Dbg (obj, "adding SAP session");

    if (vlc_getaddrinfo (dst, 0, NULL, &res) == 0)
    {
        if ((size_t)res->ai_addrlen <= sizeof (addr))
            memcpy (&addr, res->ai_addr, res->ai_addrlen);
        addrlen = res->ai_addrlen;
        freeaddrinfo (res);
    }

    if (addrlen == 0 || (size_t)addrlen > sizeof (addr))
    {
        msg_Err (obj, "No/invalid address specified for SAP announce" );
        return NULL;
    }

    /* Determine SAP multicast address automatically */
    switch (addr.a.sa_family)
    {
#if defined (HAVE_INET_PTON) || defined (_WIN32)
        case AF_INET6:
        {
            /* See RFC3513 for list of valid IPv6 scopes */
            struct in6_addr *a6 = &addr.in6.sin6_addr;

            memcpy( a6->s6_addr + 2, "\x00\x00\x00\x00\x00\x00"
                   "\x00\x00\x00\x00\x00\x02\x7f\xfe", 14 );
            if( IN6_IS_ADDR_MULTICAST( a6 ) )
                /* force flags to zero, preserve scope */
                a6->s6_addr[1] &= 0xf;
            else
                /* Unicast IPv6 - assume global scope */
                memcpy( a6->s6_addr, "\xff\x0e", 2 );
            break;
        }
#endif

        case AF_INET:
        {
            /* See RFC2365 for IPv4 scopes */
            uint32_t ipv4 = addr.in.sin_addr.s_addr;

            /* 224.0.0.0/24 => 224.0.0.255 */
            if ((ipv4 & htonl (0xffffff00)) == htonl (0xe0000000))
                ipv4 =  htonl (0xe00000ff);
            else
            /* 239.255.0.0/16 => 239.255.255.255 */
            if ((ipv4 & htonl (0xffff0000)) == htonl (0xefff0000))
                ipv4 =  htonl (0xefffffff);
            else
            /* 239.192.0.0/14 => 239.195.255.255 */
            if ((ipv4 & htonl (0xfffc0000)) == htonl (0xefc00000))
                ipv4 =  htonl (0xefc3ffff);
            else
            if ((ipv4 & htonl (0xff000000)) == htonl (0xef000000))
                ipv4 = 0;
            else
            /* other addresses => 224.2.127.254 */
                ipv4 = htonl (0xe0027ffe);

            if( ipv4 == 0 )
            {
                msg_Err (obj, "Out-of-scope multicast address "
                         "not supported by SAP");
                return NULL;
            }

            addr.in.sin_addr.s_addr = ipv4;
            break;
        }

        default:
            msg_Err (obj, "Address family %u not supported by SAP",
                     (unsigned)addr.a.sa_family);
            return NULL;
    }

    i = vlc_getnameinfo( &addr.a, addrlen,
                         psz_addr, sizeof( psz_addr ), NULL, NI_NUMERICHOST );

    if( i )
    {
        msg_Err (obj, "%s", gai_strerror (i));
        return NULL;
    }

    /* Find/create SAP address thread */
    sap_address_t *sap_addr;

    msg_Dbg (obj, "using SAP address: %s", psz_addr);
    vlc_mutex_lock (&sap_mutex);
    vlc_list_foreach (sap_addr, &sap_addrs, node)
        if (!strcmp (psz_addr, sap_addr->group))
            goto matched;

    sap_addr = AddressCreate (obj, psz_addr);
    if (sap_addr == NULL)
    {
        vlc_mutex_unlock(&sap_mutex);
        return NULL;
    }
matched:
    vlc_list_append(&sap_addr->node, &sap_addrs);

    session_descriptor_t *session = malloc(sizeof (*session));
    if (unlikely(session == NULL))
        goto out; /* NOTE: we should destroy the thread if left unused */

    /* Build the SAP Headers */
    struct vlc_memstream stream;
    vlc_memstream_open(&stream);

    /* SAPv1, not encrypted, not compressed */
    uint8_t flags = 0x20;
#ifdef AF_INET6
    if (sap_addr->orig.ss_family == AF_INET6)
        flags |= 0x10;
#endif
    vlc_memstream_putc(&stream, flags);
    vlc_memstream_putc(&stream, 0x00); /* No authentication length */
    vlc_memstream_write(&stream, &(uint16_t){ vlc_tick_now() }, 2); /* ID hash */

    switch (sap_addr->orig.ss_family)
    {
#ifdef AF_INET6
        case AF_INET6:
        {
            const struct in6_addr *a6 =
                &((const struct sockaddr_in6 *)&sap_addr->orig)->sin6_addr;
            vlc_memstream_write(&stream, &a6, 16);
            break;
        }
#endif
        case AF_INET:
        {
            const struct in_addr *a4 =
                &((const struct sockaddr_in *)&sap_addr->orig)->sin_addr;
            vlc_memstream_write(&stream, &a4, 4);
            break;
        }
        default:
            vlc_assert_unreachable ();
    }

    vlc_memstream_puts(&stream, "application/sdp");
    vlc_memstream_putc(&stream, '\0');

    /* Build the final message */
    vlc_memstream_puts(&stream, sdp);

    if (vlc_memstream_close(&stream))
    {
        free(session);
        session = NULL;
        goto out;
    }

    session->addr = sap_addr;
    session->data = stream.ptr;
    session->length = stream.length;
    vlc_list_append(&session->node, &sap_addr->sessions);

    if (sap_addr->session_count++ == 0)
    {
        if (vlc_clone(&sap_addr->thread, RunThread, sap_addr,
                      VLC_THREAD_PRIORITY_LOW))
        {
            msg_Err(obj, "unable to spawn SAP announce thread");
            AddressDestroy(sap_addr);
            free(session->data);
            free(session);
            session = NULL;
            goto out;
        }
    }
    else
        vlc_cond_signal(&sap_addr->wait);
out:
    vlc_mutex_unlock(&sap_mutex);
    return session;
}

#undef sout_AnnounceUnRegister
/**
 *  Unregisters an existing session
 *
 * \param obj a VLC object
 * \param session the session descriptor
 */
void sout_AnnounceUnRegister (vlc_object_t *obj, session_descriptor_t *session)
{
    sap_address_t *addr = session->addr;

    msg_Dbg (obj, "removing SAP session");
    vlc_mutex_lock (&sap_mutex);
    vlc_list_remove(&session->node);

    if (vlc_list_is_empty(&addr->sessions))
        /* Last session for this address -> unlink the address */
        vlc_list_remove(&addr->node);

    addr->session_count--;
    vlc_cond_signal(&addr->wait);
    vlc_mutex_unlock(&sap_mutex);

    if (vlc_list_is_empty(&addr->sessions))
    {
        vlc_join(addr->thread, NULL);
        AddressDestroy(addr);
    }

    free(session->data);
    free(session);
}
