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

#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>
#include <assert.h>

#include <vlc_sout.h>
#include <vlc_network.h>

#include "stream_output.h"
#include "libvlc.h"

/* SAP is always on that port */
#define IPPORT_SAP 9875

struct session_descriptor_t
{
    bool b_ssm;
};

/* A SAP session descriptor, enqueued in the SAP handler queue */
typedef struct sap_session_t
{
    struct sap_session_t *next;
    const session_descriptor_t *p_sd;
    size_t                length;
    uint8_t               data[];
} sap_session_t;

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
    sap_session_t          *first;
} sap_address_t;

/* The SAP handler, running in a separate thread */
struct sap_handler_t
{
    VLC_COMMON_MEMBERS

    vlc_mutex_t    lock;
    sap_address_t *first;
};

#define SAP_MAX_BUFFER 65534
#define MIN_INTERVAL 2
#define MAX_INTERVAL 300

static void *RunThread (void *);

/**
 * Create the SAP handler
 *
 * \param p_announce a VLC object
 * \return the newly created SAP handler or NULL on error
 */
static sap_handler_t *SAP_Create (vlc_object_t *p_announce)
{
    sap_handler_t *p_sap;

    p_sap = vlc_custom_create (p_announce, sizeof (*p_sap), "sap sender");
    if (p_sap == NULL)
        return NULL;

    vlc_mutex_init (&p_sap->lock);
    p_sap->first = NULL;
    return p_sap;
}

static void SAP_Destroy (sap_handler_t *p_sap)
{
    assert (p_sap->first == NULL);
    vlc_mutex_destroy (&p_sap->lock);
    vlc_object_release (p_sap);
}

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
VLC_NORETURN
static void *RunThread (void *self)
{
    sap_address_t *addr = self;

    vlc_mutex_lock (&addr->lock);
    mutex_cleanup_push (&addr->lock);

    for (;;)
    {
        sap_session_t *p_session;
        mtime_t deadline;

        while (addr->first == NULL)
            vlc_cond_wait (&addr->wait, &addr->lock);

        assert (addr->session_count > 0);

        deadline = mdate ();
        for (p_session = addr->first; p_session; p_session = p_session->next)
        {
            send (addr->fd, p_session->data, p_session->length, 0);
            deadline += addr->interval * CLOCK_FREQ / addr->session_count;

            if (vlc_cond_timedwait (&addr->wait, &addr->lock, deadline) == 0)
                break; /* list may have changed! */
        }
    }

    vlc_cleanup_pop ();
    assert (0);
}

/**
 * Add a SAP announce
 */
static int SAP_Add (sap_handler_t *p_sap, session_descriptor_t *p_session,
                    const char *sdp, const char *dst)
{
    int i;
    char psz_addr[NI_MAXNUMERICHOST];
    sap_session_t *p_sap_session;
    mtime_t i_hash;
    union
    {
        struct sockaddr     a;
        struct sockaddr_in  in;
        struct sockaddr_in6 in6;
    } addr;
    socklen_t addrlen = 0;
    struct addrinfo *res;

    if (vlc_getaddrinfo (dst, 0, NULL, &res) == 0)
    {
        if (res->ai_addrlen <= sizeof (addr))
            memcpy (&addr, res->ai_addr, res->ai_addrlen);
        addrlen = res->ai_addrlen;
        freeaddrinfo (res);
    }

    if (addrlen == 0 || addrlen > sizeof (addr))
    {
        msg_Err( p_sap, "No/invalid address specified for SAP announce" );
        return VLC_EGENERIC;
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
                msg_Err( p_sap, "Out-of-scope multicast address "
                         "not supported by SAP" );
                return VLC_EGENERIC;
            }

            addr.in.sin_addr.s_addr = ipv4;
            break;
        }

        default:
            msg_Err( p_sap, "Address family %d not supported by SAP",
                     addr.a.sa_family );
            return VLC_EGENERIC;
    }

    i = vlc_getnameinfo( &addr.a, addrlen,
                         psz_addr, sizeof( psz_addr ), NULL, NI_NUMERICHOST );

    if( i )
    {
        msg_Err( p_sap, "%s", gai_strerror( i ) );
        return VLC_EGENERIC;
    }

    /* Find/create SAP address thread */
    msg_Dbg( p_sap, "using SAP address: %s", psz_addr);

    vlc_mutex_lock (&p_sap->lock);
    sap_address_t *sap_addr;
    for (sap_addr = p_sap->first; sap_addr; sap_addr = sap_addr->next)
        if (!strcmp (psz_addr, sap_addr->group))
            break;

    if (sap_addr == NULL)
    {
        sap_addr = AddressCreate (VLC_OBJECT(p_sap), psz_addr);
        if (sap_addr == NULL)
        {
            vlc_mutex_unlock (&p_sap->lock);
            return VLC_EGENERIC;
        }
        sap_addr->next = p_sap->first;
        p_sap->first = sap_addr;
    }
    /* Switch locks.
     * NEVER take the global SAP lock when holding a SAP thread lock! */
    vlc_mutex_lock (&sap_addr->lock);
    vlc_mutex_unlock (&p_sap->lock);

    size_t headsize = 20, length;
    switch (sap_addr->orig.ss_family)
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
            assert (0);
    }

    /* XXX: Check for dupes */
    length = headsize + strlen (sdp);
    p_sap_session = malloc (sizeof (*p_sap_session) + length + 1);
    if (p_sap_session == NULL)
    {
        vlc_mutex_unlock (&sap_addr->lock);
        return VLC_EGENERIC; /* NOTE: we should destroy the thread if left unused */
    }
    p_sap_session->next = sap_addr->first;
    sap_addr->first = p_sap_session;
    p_sap_session->p_sd = p_session;
    p_sap_session->length = length;

    /* Build the SAP Headers */
    uint8_t *psz_head = p_sap_session->data;

    /* SAPv1, not encrypted, not compressed */
    psz_head[0] = 0x20;
    psz_head[1] = 0x00; /* No authentication length */

    i_hash = mdate();
    psz_head[2] = i_hash >> 8; /* Msg id hash */
    psz_head[3] = i_hash;      /* Msg id hash 2 */

    headsize = 4;
    switch (sap_addr->orig.ss_family)
    {
#ifdef AF_INET6
        case AF_INET6:
        {
            const struct in6_addr *a6 =
                &((const struct sockaddr_in6 *)&sap_addr->orig)->sin6_addr;
            memcpy (psz_head + headsize, a6, 16);
            psz_head[0] |= 0x10; /* IPv6 flag */
            headsize += 16;
            break;
        }
#endif
        case AF_INET:
        {
            const struct in_addr *a4 =
                &((const struct sockaddr_in *)&sap_addr->orig)->sin_addr;
            memcpy (psz_head + headsize, a4, 4);
            headsize += 4;
            break;
        }

    }

    memcpy (psz_head + headsize, "application/sdp", 16);
    headsize += 16;

    /* Build the final message */
    strcpy((char *)psz_head + headsize, sdp);

    sap_addr->session_count++;
    vlc_cond_signal (&sap_addr->wait);
    vlc_mutex_unlock (&sap_addr->lock);
    return VLC_SUCCESS;
}

/**
 * Remove a SAP Announce
 */
static void SAP_Del (sap_handler_t *p_sap,
                     const session_descriptor_t *p_session)
{
    vlc_mutex_lock (&p_sap->lock);

    /* TODO: give a handle back in SAP_Add, and use that... */
    sap_address_t *addr, **paddr;
    sap_session_t *session, **psession;

    paddr = &p_sap->first;
    for (addr = p_sap->first; addr; addr = addr->next)
    {
        psession = &addr->first;
        vlc_mutex_lock (&addr->lock);
        for (session = addr->first; session; session = session->next)
        {
            if (session->p_sd == p_session)
                goto found;
            psession = &session->next;
        }
        vlc_mutex_unlock (&addr->lock);
        paddr = &addr->next;
    }
    assert (0);

found:
    *psession = session->next;

    if (addr->first == NULL)
        /* Last session for this address -> unlink the address */
        *paddr = addr->next;
    vlc_mutex_unlock (&p_sap->lock);

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

    free (session);
}

/****************************************************************************
 * Sout-side functions
 ****************************************************************************/

static void sap_destroy (vlc_object_t *p_this)
{
    libvlc_priv (p_this->p_libvlc)->p_sap = NULL;
}

#undef sout_AnnounceRegisterSDP

static vlc_mutex_t sap_mutex = VLC_STATIC_MUTEX;

/**
 *  Registers a new session with the announce handler, using a pregenerated SDP
 *
 * \param obj a VLC object
 * \param psz_sdp the SDP to register
 * \param psz_dst session address (needed for SAP address auto detection)
 * \return the new session descriptor structure
 */
session_descriptor_t *
sout_AnnounceRegisterSDP( vlc_object_t *obj, const char *psz_sdp,
                          const char *psz_dst )
{
    session_descriptor_t *p_session = calloc( 1, sizeof (*p_session) );
    if( !p_session )
        return NULL;

    vlc_mutex_lock (&sap_mutex);
    sap_handler_t *p_sap = libvlc_priv (obj->p_libvlc)->p_sap;
    if (p_sap == NULL)
    {
        p_sap = SAP_Create (VLC_OBJECT (obj->p_libvlc));
        libvlc_priv (obj->p_libvlc)->p_sap = p_sap;
        vlc_object_set_destructor ((vlc_object_t *)p_sap, sap_destroy);
    }
    else
        vlc_object_hold ((vlc_object_t *)p_sap);
    vlc_mutex_unlock (&sap_mutex);

    if (p_sap == NULL)
        goto error;

    msg_Dbg (obj, "adding SAP session");
    if (SAP_Add (p_sap, p_session, psz_sdp, psz_dst))
    {
        vlc_mutex_lock (&sap_mutex);
        vlc_object_release ((vlc_object_t *)p_sap);
        vlc_mutex_unlock (&sap_mutex);
        goto error;
    }

    return p_session;

error:
    free (p_session);
    return NULL;
}

#undef sout_AnnounceUnRegister
/**
 *  Unregisters an existing session
 *
 * \param obj a VLC object
 * \param p_session the session descriptor
 * \return VLC_SUCCESS or an error
 */
int sout_AnnounceUnRegister( vlc_object_t *obj,
                             session_descriptor_t *p_session )
{
    sap_handler_t *p_sap = libvlc_priv (obj->p_libvlc)->p_sap;

    msg_Dbg (obj, "removing SAP session");
    SAP_Del (p_sap, p_session);

    vlc_mutex_lock (&sap_mutex);
    vlc_object_release ((vlc_object_t *)p_sap);
    vlc_mutex_unlock (&sap_mutex);

    free (p_session);

    return 0;
}
