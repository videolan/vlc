/*****************************************************************************
 * sap.c : SAP announce handler
 *****************************************************************************
 * Copyright (C) 2002-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Rémi Denis-Courmont <rem # videolan.org>
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

#include <stdnoreturn.h>
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

/* A SAP session descriptor, enqueued in the SAP handler queue */
struct session_descriptor_t
{
    struct session_descriptor_t *next;
    size_t length;
    char *data;
};

/* A SAP announce address. For each of these, we run the
 * control flow algorithm */
typedef struct sap_address_t
{
    struct sap_address_t   *next;

    vlc_thread_t            thread;
    vlc_mutex_t             lock;
    vlc_cond_t              wait;

    char                    group[NI_MAXNUMERICHOST];
    struct sockaddr_storage orig;
    socklen_t               origlen;
    int                     fd;
    unsigned                interval;

    unsigned                session_count;
    session_descriptor_t   *first;
} sap_address_t;

static sap_address_t *sap_addrs = NULL;
static vlc_mutex_t sap_mutex = VLC_STATIC_MUTEX;

#define SAP_MAX_BUFFER 65534
#define MIN_INTERVAL 2
#define MAX_INTERVAL 300

noreturn static void *RunThread (void *);

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
    vlc_mutex_init (&addr->lock);
    vlc_cond_init (&addr->wait);
    addr->session_count = 0;
    addr->first = NULL;

    if (vlc_clone (&addr->thread, RunThread, addr, VLC_THREAD_PRIORITY_LOW))
    {
        msg_Err (obj, "unable to spawn SAP announce thread");
        net_Close (fd);
        free (addr);
        return NULL;
    }
    return addr;
}

static void AddressDestroy (sap_address_t *addr)
{
    assert (addr->first == NULL);

    vlc_cancel (addr->thread);
    vlc_join (addr->thread, NULL);
    vlc_cond_destroy (&addr->wait);
    vlc_mutex_destroy (&addr->lock);
    net_Close (addr->fd);
    free (addr);
}

/**
 * main SAP handler thread
 * \param p_this the SAP Handler object
 * \return nothing
 */
noreturn static void *RunThread (void *self)
{
    sap_address_t *addr = self;

    vlc_mutex_lock (&addr->lock);
    mutex_cleanup_push (&addr->lock);

    for (;;)
    {
        session_descriptor_t *p_session;
        vlc_tick_t deadline;

        while (addr->first == NULL)
            vlc_cond_wait (&addr->wait, &addr->lock);

        assert (addr->session_count > 0);

        deadline = vlc_tick_now ();
        for (p_session = addr->first; p_session; p_session = p_session->next)
        {
            send (addr->fd, p_session->data, p_session->length, 0);
            deadline += addr->interval * CLOCK_FREQ / addr->session_count;

            if (vlc_cond_timedwait (&addr->wait, &addr->lock, deadline) == 0)
                break; /* list may have changed! */
        }
    }

    vlc_cleanup_pop ();
    vlc_assert_unreachable ();
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
    for (sap_addr = sap_addrs; sap_addr; sap_addr = sap_addr->next)
        if (!strcmp (psz_addr, sap_addr->group))
            break;

    if (sap_addr == NULL)
    {
        sap_addr = AddressCreate (obj, psz_addr);
        if (sap_addr == NULL)
        {
            vlc_mutex_unlock (&sap_mutex);
            return NULL;
        }
        sap_addr->next = sap_addrs;
        sap_addrs = sap_addr;
    }
    /* Switch locks.
     * NEVER take the global SAP lock when holding a SAP thread lock! */
    vlc_mutex_lock (&sap_addr->lock);
    vlc_mutex_unlock (&sap_mutex);

    session_descriptor_t *session = malloc(sizeof (*session));
    if (unlikely(session == NULL))
        goto out; /* NOTE: we should destroy the thread if left unused */

    session->next = sap_addr->first;

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

    session->data = stream.ptr;
    session->length = stream.length;
    sap_addr->first = session;
    sap_addr->session_count++;
    vlc_cond_signal (&sap_addr->wait);
out:
    vlc_mutex_unlock (&sap_addr->lock);
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
    sap_address_t *addr, **paddr;
    session_descriptor_t **psession;

    msg_Dbg (obj, "removing SAP session");
    vlc_mutex_lock (&sap_mutex);
    paddr = &sap_addrs;
    for (;;)
    {
        addr = *paddr;
        assert (addr != NULL);

        psession = &addr->first;
        vlc_mutex_lock (&addr->lock);
        while (*psession != NULL)
        {
            if (*psession == session)
                goto found;
            psession = &(*psession)->next;
        }
        vlc_mutex_unlock (&addr->lock);
        paddr = &addr->next;
    }

found:
    *psession = session->next;

    if (addr->first == NULL)
        /* Last session for this address -> unlink the address */
        *paddr = addr->next;
    vlc_mutex_unlock (&sap_mutex);

    if (addr->first == NULL)
    {
        /* Last session for this address -> unlink the address */
        vlc_mutex_unlock (&addr->lock);
        AddressDestroy (addr);
    }
    else
    {
        addr->session_count--;
        vlc_cond_signal (&addr->wait);
        vlc_mutex_unlock (&addr->lock);
    }

    free(session->data);
    free(session);
}
