/*****************************************************************************
 * udp.c:
 *****************************************************************************
 * Copyright (C) 2004-2006 VLC authors and VideoLAN
 * Copyright © 2006-2007 Rémi Denis-Courmont
 *
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <errno.h>
#include <assert.h>

#include <vlc_network.h>

#ifdef _WIN32
#   undef EAFNOSUPPORT
#   define EAFNOSUPPORT WSAEAFNOSUPPORT
#   if defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 6
#    include <wincrypt.h>
#   endif
#   include <iphlpapi.h>
#else
#   include <unistd.h>
#   ifdef HAVE_NET_IF_H
#       include <net/if.h>
#   endif
#endif

#ifdef HAVE_LINUX_DCCP_H
# include <linux/dccp.h>
# ifndef SOCK_DCCP /* provisional API */
#  define SOCK_DCCP 6
# endif
#endif

#ifndef SOL_IP
# define SOL_IP IPPROTO_IP
#endif
#ifndef SOL_IPV6
# define SOL_IPV6 IPPROTO_IPV6
#endif
#ifndef IPPROTO_IPV6
# define IPPROTO_IPV6 41 /* IANA */
#endif
#ifndef SOL_DCCP
# define SOL_DCCP IPPROTO_DCCP
#endif
#ifndef IPPROTO_DCCP
# define IPPROTO_DCCP 33 /* IANA */
#endif
#ifndef SOL_UDPLITE
# define SOL_UDPLITE IPPROTO_UDPLITE
#endif
#ifndef IPPROTO_UDPLITE
# define IPPROTO_UDPLITE 136 /* IANA */
#endif

#if defined (HAVE_NETINET_UDPLITE_H)
# include <netinet/udplite.h>
#elif defined (__linux__)
/* still missing from glibc 2.6 */
# define UDPLITE_SEND_CSCOV     10
# define UDPLITE_RECV_CSCOV     11
#endif

/* */
static int net_SetupDgramSocket (vlc_object_t *p_obj, int fd,
                                 const struct addrinfo *ptr)
{
#if defined (SO_REUSEPORT) && !defined (__linux__)
    setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, &(int){ 1 }, sizeof (int));
#endif

#if defined (_WIN32)

    /* Check windows version so we know if we need to increase receive buffers
     * for Windows 7 and earlier

     * SetSocketMediaStreamingMode is present in win 8 and later, so we set
     * receive buffer if that isn't present
     */
#if (_WIN32_WINNT < _WIN32_WINNT_WIN8)
    HINSTANCE h_Network = LoadLibrary(TEXT("Windows.Networking.dll"));
    if( (h_Network == NULL) ||
        (GetProcAddress( h_Network, "SetSocketMediaStreamingMode" ) == NULL ) )
    {
        setsockopt (fd, SOL_SOCKET, SO_RCVBUF,
                         (void *)&(int){ 0x80000 }, sizeof (int));
    }
    if( h_Network )
        FreeLibrary( h_Network );
#endif

    if (net_SockAddrIsMulticast (ptr->ai_addr, ptr->ai_addrlen)
     && (sizeof (struct sockaddr_storage) >= ptr->ai_addrlen))
    {
        // This works for IPv4 too - don't worry!
        struct sockaddr_in6 dumb =
        {
            .sin6_family = ptr->ai_addr->sa_family,
            .sin6_port =  ((struct sockaddr_in *)(ptr->ai_addr))->sin_port
        };

        bind (fd, (struct sockaddr *)&dumb, ptr->ai_addrlen);
    }
    else
#endif
    if (bind (fd, ptr->ai_addr, ptr->ai_addrlen))
    {
        msg_Err( p_obj, "socket bind error: %s", vlc_strerror_c(net_errno) );
        net_Close (fd);
        return -1;
    }
    return fd;
}

static int net_Subscribe(vlc_object_t *obj, int fd,
                         const struct sockaddr *addr, socklen_t addrlen);

/* */
static int net_ListenSingle (vlc_object_t *obj, const char *host, unsigned port,
                             int protocol)
{
    struct addrinfo hints = {
        .ai_socktype = SOCK_DGRAM,
        .ai_protocol = protocol,
        .ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_IDN,
    }, *res;

    if (host && !*host)
        host = NULL;

    msg_Dbg (obj, "net: opening %s datagram port %u",
             host ? host : "any", port);

    int val = vlc_getaddrinfo (host, port, &hints, &res);
    if (val)
    {
        msg_Err (obj, "Cannot resolve %s port %u : %s", host, port,
                 gai_strerror (val));
        return -1;
    }

    val = -1;

    for (const struct addrinfo *ptr = res; ptr != NULL; ptr = ptr->ai_next)
    {
        int fd = net_Socket (obj, ptr->ai_family, ptr->ai_socktype,
                             ptr->ai_protocol);
        if (fd == -1)
        {
            msg_Dbg (obj, "socket error: %s", vlc_strerror_c(net_errno));
            continue;
        }

#ifdef IPV6_V6ONLY
        /* Try dual-mode IPv6 if available. */
        if (ptr->ai_family == AF_INET6)
            setsockopt (fd, SOL_IPV6, IPV6_V6ONLY, &(int){ 0 }, sizeof (int));
#endif
        fd = net_SetupDgramSocket( obj, fd, ptr );
        if( fd == -1 )
            continue;

        if (net_SockAddrIsMulticast (ptr->ai_addr, ptr->ai_addrlen)
         && net_Subscribe (obj, fd, ptr->ai_addr, ptr->ai_addrlen))
        {
            net_Close (fd);
            continue;
        }

        val = fd;
        break;
    }

    freeaddrinfo (res);
    return val;
}


static int net_SetMcastHopLimit( vlc_object_t *p_this,
                                 int fd, int family, int hlim )
{
    int proto, cmd;

    /* There is some confusion in the world whether IP_MULTICAST_TTL
     * takes a byte or an int as an argument.
     * BSD seems to indicate byte so we are going with that and use
     * int as a fallback to be safe */
    switch( family )
    {
#ifdef IP_MULTICAST_TTL
        case AF_INET:
            proto = SOL_IP;
            cmd = IP_MULTICAST_TTL;
            break;
#endif

#ifdef IPV6_MULTICAST_HOPS
        case AF_INET6:
            proto = SOL_IPV6;
            cmd = IPV6_MULTICAST_HOPS;
            break;
#endif

        default:
            errno = EAFNOSUPPORT;
            msg_Warn( p_this, "%s", vlc_strerror_c(EAFNOSUPPORT) );
            return VLC_EGENERIC;
    }

    if( setsockopt( fd, proto, cmd, &hlim, sizeof( hlim ) ) < 0 )
    {
        /* BSD compatibility */
        unsigned char buf;

        msg_Dbg( p_this, "cannot set hop limit (%d): %s", hlim,
                 vlc_strerror_c(net_errno) );
        buf = (unsigned char)(( hlim > 255 ) ? 255 : hlim);
        if( setsockopt( fd, proto, cmd, &buf, sizeof( buf ) ) )
        {
            msg_Err( p_this, "cannot set hop limit (%d): %s", hlim,
                     vlc_strerror_c(net_errno) );
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}


static int net_SetMcastOut (vlc_object_t *p_this, int fd, int family,
                            const char *iface)
{
    int scope = if_nametoindex (iface);
    if (scope == 0)
    {
        msg_Err (p_this, "invalid multicast interface: %s", iface);
        return -1;
    }

    switch (family)
    {
#ifdef IPV6_MULTICAST_IF
        case AF_INET6:
            if (setsockopt (fd, SOL_IPV6, IPV6_MULTICAST_IF,
                            &scope, sizeof (scope)) == 0)
                return 0;
            break;
#endif

#ifdef __linux__
        case AF_INET:
        {
            struct ip_mreqn req = { .imr_ifindex = scope };
            if (setsockopt (fd, SOL_IP, IP_MULTICAST_IF,
                            &req, sizeof (req)) == 0)
                return 0;
            break;
        }
#endif
        default:
            errno = EAFNOSUPPORT;
    }
    msg_Err (p_this, "cannot force multicast interface %s: %s", iface,
             vlc_strerror_c(errno));
    return -1;
}


#if defined(MCAST_JOIN_GROUP) && !defined (__APPLE__)
static unsigned var_GetIfIndex (vlc_object_t *obj)
{
    char *ifname = var_InheritString (obj, "miface");
    if (ifname == NULL)
        return 0;

    unsigned ifindex = if_nametoindex (ifname);
    if (ifindex == 0)
        msg_Err (obj, "invalid multicast interface: %s", ifname);
    free (ifname);
    return ifindex;
}
#endif


/**
 * IP-agnostic multicast join,
 * with fallback to old APIs, and fallback from SSM to ASM.
 */
static int
net_SourceSubscribe (vlc_object_t *obj, int fd,
                     const struct sockaddr *src, socklen_t srclen,
                     const struct sockaddr *grp, socklen_t grplen)
{
/* MCAST_JOIN_SOURCE_GROUP was introduced to OS X in v10.7, but it doesn't work,
 * so ignore it to use the same code path as on 10.5 or 10.6 */
#if defined (MCAST_JOIN_SOURCE_GROUP) && !defined (__APPLE__)
    /* Family-agnostic Source-Specific Multicast join */
    int level;
    struct group_source_req gsr;

    memset (&gsr, 0, sizeof (gsr));
    gsr.gsr_interface = var_GetIfIndex (obj);

    switch (grp->sa_family)
    {
#ifdef AF_INET6
        case AF_INET6:
        {
            const struct sockaddr_in6 *g6 = (const struct sockaddr_in6 *)grp;

            level = SOL_IPV6;
            assert(grplen >= (socklen_t)sizeof (struct sockaddr_in6));
            if (g6->sin6_scope_id != 0)
                gsr.gsr_interface = g6->sin6_scope_id;
            break;
        }
#endif
        case AF_INET:
            level = SOL_IP;
            break;
        default:
            errno = EAFNOSUPPORT;
            return -1;
    }

    assert(grplen <= (socklen_t)sizeof (gsr.gsr_group));
    memcpy (&gsr.gsr_source, src, srclen);
    assert(srclen <= (socklen_t)sizeof (gsr.gsr_source));
    memcpy (&gsr.gsr_group,  grp, grplen);
    if (setsockopt (fd, level, MCAST_JOIN_SOURCE_GROUP,
                    &gsr, sizeof (gsr)) == 0)
        return 0;

#else
    if (src->sa_family != grp->sa_family)
    {
        errno = EAFNOSUPPORT;
        return -1;
    }

    switch (grp->sa_family)
    {
# ifdef IP_ADD_SOURCE_MEMBERSHIP
        /* IPv4-specific API */
        case AF_INET:
        {
            struct ip_mreq_source imr;

            memset (&imr, 0, sizeof (imr));
            assert(grplen >= (socklen_t)sizeof (struct sockaddr_in));
            imr.imr_multiaddr = ((const struct sockaddr_in *)grp)->sin_addr;
            assert(srclen >= (socklen_t)sizeof (struct sockaddr_in));
            imr.imr_sourceaddr = ((const struct sockaddr_in *)src)->sin_addr;
            if (setsockopt (fd, SOL_IP, IP_ADD_SOURCE_MEMBERSHIP,
                            &imr, sizeof (imr)) == 0)
                return 0;
            break;
        }
# endif
        default:
            errno = EAFNOSUPPORT;
    }

#endif
    msg_Err (obj, "cannot join source multicast group: %s",
             vlc_strerror_c(net_errno));
    msg_Warn (obj, "trying ASM instead of SSM...");
    return net_Subscribe (obj, fd, grp, grplen);
}


static int net_Subscribe(vlc_object_t *obj, int fd,
                         const struct sockaddr *grp, socklen_t grplen)
{
/* MCAST_JOIN_GROUP was introduced to OS X in v10.7, but it doesn't work,
 * so ignore it to use the same code as on 10.5 or 10.6 */
#if defined (MCAST_JOIN_GROUP) && !defined (__APPLE__)
    /* Family-agnostic Any-Source Multicast join */
    int level;
    struct group_req gr;

    memset (&gr, 0, sizeof (gr));
    gr.gr_interface = var_GetIfIndex (obj);

    switch (grp->sa_family)
    {
#ifdef AF_INET6
        case AF_INET6:
        {
            const struct sockaddr_in6 *g6 = (const struct sockaddr_in6 *)grp;

            level = SOL_IPV6;
            assert(grplen >= (socklen_t)sizeof (struct sockaddr_in6));
            if (g6->sin6_scope_id != 0)
                gr.gr_interface = g6->sin6_scope_id;
            break;
        }
#endif
        case AF_INET:
            level = SOL_IP;
            break;
        default:
            errno = EAFNOSUPPORT;
            return -1;
    }

    assert(grplen <= (socklen_t)sizeof (gr.gr_group));
    memcpy (&gr.gr_group, grp, grplen);
    if (setsockopt (fd, level, MCAST_JOIN_GROUP, &gr, sizeof (gr)) == 0)
        return 0;

#else
    switch (grp->sa_family)
    {
# ifdef IPV6_JOIN_GROUP
        case AF_INET6:
        {
            struct ipv6_mreq ipv6mr;
            const struct sockaddr_in6 *g6 = (const struct sockaddr_in6 *)grp;

            memset (&ipv6mr, 0, sizeof (ipv6mr));
            assert(grplen >= (socklen_t)sizeof (struct sockaddr_in6));
            ipv6mr.ipv6mr_multiaddr = g6->sin6_addr;
            ipv6mr.ipv6mr_interface = g6->sin6_scope_id;
            if (!setsockopt (fd, SOL_IPV6, IPV6_JOIN_GROUP,
                             &ipv6mr, sizeof (ipv6mr)))
                return 0;
            break;
        }
# endif
# ifdef IP_ADD_MEMBERSHIP
        case AF_INET:
        {
            struct ip_mreq imr;

            memset (&imr, 0, sizeof (imr));
            assert(grplen >= (socklen_t)sizeof (struct sockaddr_in));
            imr.imr_multiaddr = ((const struct sockaddr_in *)grp)->sin_addr;
            if (setsockopt (fd, SOL_IP, IP_ADD_MEMBERSHIP,
                            &imr, sizeof (imr)) == 0)
                return 0;
            break;
        }
# endif
        default:
            errno = EAFNOSUPPORT;
    }

#endif
    msg_Err (obj, "cannot join multicast group: %s",
             vlc_strerror_c(net_errno));
    return -1;
}


static int net_SetDSCP( int fd, uint8_t dscp )
{
    struct sockaddr_storage addr;
    if( getsockname( fd, (struct sockaddr *)&addr, &(socklen_t){ sizeof (addr) }) )
        return -1;

    int level, cmd;

    switch( addr.ss_family )
    {
#ifdef IPV6_TCLASS
        case AF_INET6:
            level = SOL_IPV6;
            cmd = IPV6_TCLASS;
            break;
#endif

        case AF_INET:
            level = SOL_IP;
            cmd = IP_TOS;
            break;

        default:
#ifdef ENOPROTOOPT
            errno = ENOPROTOOPT;
#endif
            return -1;
    }

    return setsockopt( fd, level, cmd, &(int){ dscp }, sizeof (int));
}

#undef net_ConnectDgram
/*****************************************************************************
 * net_ConnectDgram:
 *****************************************************************************
 * Open a datagram socket to send data to a defined destination, with an
 * optional hop limit.
 *****************************************************************************/
int net_ConnectDgram( vlc_object_t *p_this, const char *psz_host, unsigned i_port,
                      int i_hlim, int proto )
{
    struct addrinfo hints = {
        .ai_socktype = SOCK_DGRAM,
        .ai_protocol = proto,
        .ai_flags = AI_NUMERICSERV | AI_IDN,
    }, *res;
    int       i_handle = -1;
    bool      b_unreach = false;

    if( i_hlim < 0 )
        i_hlim = var_InheritInteger( p_this, "ttl" );

    msg_Dbg( p_this, "net: connecting to [%s]:%u", psz_host, i_port );

    int val = vlc_getaddrinfo (psz_host, i_port, &hints, &res);
    if (val)
    {
        msg_Err (p_this, "cannot resolve [%s]:%u : %s", psz_host, i_port,
                 gai_strerror (val));
        return -1;
    }

    for (struct addrinfo *ptr = res; ptr != NULL; ptr = ptr->ai_next)
    {
        char *str;
        int fd = net_Socket (p_this, ptr->ai_family, ptr->ai_socktype,
                             ptr->ai_protocol);
        if (fd == -1)
            continue;

        /* Allow broadcast sending */
        setsockopt (fd, SOL_SOCKET, SO_BROADCAST, &(int){ 1 }, sizeof (int));

        if( i_hlim >= 0 )
            net_SetMcastHopLimit( p_this, fd, ptr->ai_family, i_hlim );

        str = var_InheritString (p_this, "miface");
        if (str != NULL)
        {
            net_SetMcastOut (p_this, fd, ptr->ai_family, str);
            free (str);
        }

        net_SetDSCP (fd, var_InheritInteger (p_this, "dscp"));

        if( connect( fd, ptr->ai_addr, ptr->ai_addrlen ) == 0 )
        {
            /* success */
            i_handle = fd;
            break;
        }

#if defined( _WIN32 )
        if( WSAGetLastError () == WSAENETUNREACH )
#else
        if( errno == ENETUNREACH )
#endif
            b_unreach = true;
        else
            msg_Warn( p_this, "%s port %u : %s", psz_host, i_port,
                      vlc_strerror_c(errno) );
        net_Close( fd );
    }

    freeaddrinfo( res );

    if( i_handle == -1 )
    {
        if( b_unreach )
            msg_Err( p_this, "Host %s port %u is unreachable", psz_host,
                     i_port );
        return -1;
    }

    return i_handle;
}

#undef net_OpenDgram
/*****************************************************************************
 * net_OpenDgram:
 *****************************************************************************
 * OpenDgram a datagram socket and return a handle
 *****************************************************************************/
int net_OpenDgram( vlc_object_t *obj, const char *psz_bind, unsigned i_bind,
                   const char *psz_server, unsigned i_server, int protocol )
{
    if ((psz_server == NULL) || (psz_server[0] == '\0'))
        return net_ListenSingle (obj, psz_bind, i_bind, protocol);

    msg_Dbg (obj, "net: connecting to [%s]:%u from [%s]:%u",
             psz_server, i_server, psz_bind, i_bind);

    struct addrinfo hints = {
        .ai_socktype = SOCK_DGRAM,
        .ai_protocol = protocol,
        .ai_flags = AI_NUMERICSERV | AI_IDN,
    }, *loc, *rem;

    int val = vlc_getaddrinfo (psz_server, i_server, &hints, &rem);
    if (val)
    {
        msg_Err (obj, "cannot resolve %s port %u : %s", psz_server, i_server,
                 gai_strerror (val));
        return -1;
    }

    hints.ai_flags |= AI_PASSIVE;
    val = vlc_getaddrinfo (psz_bind, i_bind, &hints, &loc);
    if (val)
    {
        msg_Err (obj, "cannot resolve %s port %u : %s", psz_bind, i_bind,
                 gai_strerror (val));
        freeaddrinfo (rem);
        return -1;
    }

    val = -1;
    for (struct addrinfo *ptr = loc; ptr != NULL; ptr = ptr->ai_next)
    {
        int fd = net_Socket (obj, ptr->ai_family, ptr->ai_socktype,
                             ptr->ai_protocol);
        if (fd == -1)
            continue; // usually, address family not supported

        fd = net_SetupDgramSocket( obj, fd, ptr );
        if( fd == -1 )
            continue;

        for (struct addrinfo *ptr2 = rem; ptr2 != NULL; ptr2 = ptr2->ai_next)
        {
            if ((ptr2->ai_family != ptr->ai_family)
             || (ptr2->ai_socktype != ptr->ai_socktype)
             || (ptr2->ai_protocol != ptr->ai_protocol))
                continue;

            if (net_SockAddrIsMulticast (ptr->ai_addr, ptr->ai_addrlen)
              ? net_SourceSubscribe (obj, fd,
                                     ptr2->ai_addr, ptr2->ai_addrlen,
                                     ptr->ai_addr, ptr->ai_addrlen)
              : connect (fd, ptr2->ai_addr, ptr2->ai_addrlen))
            {
                msg_Err (obj, "cannot connect to %s port %u: %s",
                         psz_server, i_server, vlc_strerror_c(net_errno));
                continue;
            }
            val = fd;
            break;
        }

        if (val != -1)
            break;

        net_Close (fd);
    }

    freeaddrinfo (rem);
    freeaddrinfo (loc);
    return val;
}


/**
 * net_SetCSCov:
 * Sets the send and receive checksum coverage of a socket:
 * @param fd socket
 * @param sendcov payload coverage of sent packets (bytes), -1 for full
 * @param recvcov minimum payload coverage of received packets, -1 for full
 */
int net_SetCSCov (int fd, int sendcov, int recvcov)
{
    int type;

    if (getsockopt (fd, SOL_SOCKET, SO_TYPE,
                    &type, &(socklen_t){ sizeof (type) }))
        return VLC_EGENERIC;

    switch (type)
    {
#ifdef UDPLITE_RECV_CSCOV
        case SOCK_DGRAM: /* UDP-Lite */
            if (sendcov == -1)
                sendcov = 0;
            else
                sendcov += 8; /* partial */
            if (setsockopt (fd, SOL_UDPLITE, UDPLITE_SEND_CSCOV, &sendcov,
                            sizeof (sendcov)))
                return VLC_EGENERIC;

            if (recvcov == -1)
                recvcov = 0;
            else
                recvcov += 8;
            if (setsockopt (fd, SOL_UDPLITE, UDPLITE_RECV_CSCOV,
                            &recvcov, sizeof (recvcov)))
                return VLC_EGENERIC;

            return VLC_SUCCESS;
#endif
#ifdef DCCP_SOCKOPT_SEND_CSCOV
        case SOCK_DCCP: /* DCCP and its ill-named socket type */
            if ((sendcov == -1) || (sendcov > 56))
                sendcov = 0;
            else
                sendcov = (sendcov + 3) / 4;
            if (setsockopt (fd, SOL_DCCP, DCCP_SOCKOPT_SEND_CSCOV,
                            &sendcov, sizeof (sendcov)))
                return VLC_EGENERIC;

            if ((recvcov == -1) || (recvcov > 56))
                recvcov = 0;
            else
                recvcov = (recvcov + 3) / 4;
            if (setsockopt (fd, SOL_DCCP, DCCP_SOCKOPT_RECV_CSCOV,
                            &recvcov, sizeof (recvcov)))
                return VLC_EGENERIC;

            return VLC_SUCCESS;
#endif
    }
#if !defined( UDPLITE_RECV_CSCOV ) && !defined( DCCP_SOCKOPT_SEND_CSCOV )
    VLC_UNUSED(sendcov);
    VLC_UNUSED(recvcov);
#endif

    return VLC_EGENERIC;
}
