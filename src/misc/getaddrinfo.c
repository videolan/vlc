/*****************************************************************************
 * getaddrinfo.c: getaddrinfo/getnameinfo replacement functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * Copyright (C) 2002-2004 Rémi Denis-Courmont
 * $Id$
 *
 * Author: Rémi Denis-Courmont <rem # videolan.org>
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

#include <vlc/vlc.h>

#include <stddef.h> /* size_t */
#include <string.h> /* strncpy(), strlen(), memcpy(), memset(), strchr() */
#include <stdlib.h> /* malloc(), free(), strtoul() */
#include <sys/types.h>
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#include <errno.h>

#if defined( WIN32 ) || defined( UNDER_CE )
#   if defined(UNDER_CE) && defined(sockaddr_storage)
#       undef sockaddr_storage
#   endif
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>
#   endif
#   include <netdb.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "network.h"

#ifdef SYS_BEOS
#   define NO_ADDRESS  NO_DATA
#   define INADDR_NONE 0xFFFFFFFF
#   define AF_UNSPEC   0
#endif

#define _NI_MASK (NI_NUMERICHOST|NI_NUMERICSERV|NI_NOFQDN|NI_NAMEREQD|\
                  NI_DGRAM)
#define _AI_MASK (AI_PASSIVE|AI_CANONNAME|AI_NUMERICHOST)


#ifndef HAVE_GAI_STRERROR
static struct
{
    int code;
    const char *msg;
} const __gai_errlist[] =
{
    { 0,              "Error 0" },
    { EAI_BADFLAGS,   "Invalid flag used" },
    { EAI_NONAME,     "Host or service not found" },
    { EAI_AGAIN,      "Temporary name service failure" },
    { EAI_FAIL,       "Non-recoverable name service failure" },
    { EAI_NODATA,     "No data for host name" },
    { EAI_FAMILY,     "Unsupported address family" },
    { EAI_SOCKTYPE,   "Unsupported socket type" },
    { EAI_SERVICE,    "Incompatible service for socket type" },
    { EAI_ADDRFAMILY, "Unavailable address family for host name" },
    { EAI_MEMORY,     "Memory allocation failure" },
    { EAI_SYSTEM,     "System error" },
    { 0,              NULL }
};

static const char *__gai_unknownerr = "Unrecognized error number";

/****************************************************************************
 * Converts an EAI_* error code into human readable english text.
 ****************************************************************************/
const char *vlc_gai_strerror( int errnum )
{
    int i;

    for (i = 0; __gai_errlist[i].msg != NULL; i++)
        if (errnum == __gai_errlist[i].code)
            return __gai_errlist[i].msg;

    return __gai_unknownerr;
}
# undef _EAI_POSITIVE_MAX
#else /* ifndef HAVE_GAI_STRERROR */
const char *vlc_gai_strerror( int errnum )
{
    return gai_strerror( errnum );
}
#endif

#if !(defined (HAVE_GETNAMEINFO) && defined (HAVE_GETADDRINFO))
/*
 * Converts the current herrno error value into an EAI_* error code.
 * That error code is normally returned by getnameinfo() or getaddrinfo().
 */
static int
gai_error_from_herrno( void )
{
    switch(h_errno)
    {
        case HOST_NOT_FOUND:
            return EAI_NONAME;

        case NO_ADDRESS:
# if (NO_ADDRESS != NO_DATA)
        case NO_DATA:
# endif
            return EAI_NODATA;

        case NO_RECOVERY:
            return EAI_FAIL;

        case TRY_AGAIN:
            return EAI_AGAIN;
    }
    return EAI_SYSTEM;
}
#endif /* if !(HAVE_GETNAMEINFO && HAVE_GETADDRINFO) */

#ifndef HAVE_GETNAMEINFO
/*
 * getnameinfo() non-thread-safe IPv4-only implementation,
 * Address-family-independant address to hostname translation
 * (reverse DNS lookup in case of IPv4).
 *
 * This is meant for use on old IP-enabled systems that are not IPv6-aware,
 * and probably do not have getnameinfo(), but have the old gethostbyaddr()
 * function.
 *
 * GNU C library 2.0.x is known to lack this function, even though it defines
 * getaddrinfo().
 */
static int
__getnameinfo( const struct sockaddr *sa, socklen_t salen,
               char *host, int hostlen, char *serv, int servlen, int flags )
{
    if (((unsigned)salen < sizeof (struct sockaddr_in))
     || (sa->sa_family != AF_INET))
        return EAI_FAMILY;
    else if (flags & (~_NI_MASK))
        return EAI_BADFLAGS;
    else
    {
        const struct sockaddr_in *addr;

        addr = (const struct sockaddr_in *)sa;

        if (host != NULL)
        {
            int solved = 0;

            /* host name resolution */
            if (!(flags & NI_NUMERICHOST))
            {
                struct hostent *hent;

                hent = gethostbyaddr ((const void*)&addr->sin_addr,
                                      4, AF_INET);

                if (hent != NULL)
                {
                    strncpy (host, hent->h_name, hostlen);
                    host[hostlen - 1] = '\0';

                    /*
                     * only keep first part of hostname
                     * if user don't want fully qualified
                     * domain name
                     */
                    if (flags & NI_NOFQDN)
                    {
                        char *ptr;

                        ptr = strchr (host, '.');
                        if (ptr != NULL)
                            *ptr = 0;
                    }

                    solved = 1;
                }
                else if (flags & NI_NAMEREQD)
                    return gai_error_from_herrno ();
            }

            if (!solved)
            {
                /* inet_ntoa() can't fail */
                strncpy (host, inet_ntoa (addr->sin_addr), hostlen);
                host[hostlen - 1] = '\0';
            }
        }

        if (serv != NULL)
        {
            struct servent *sent = NULL;

#ifndef SYS_BEOS /* No getservbyport() */
            int solved = 0;

            /* service name resolution */
            if (!(flags & NI_NUMERICSERV))
            {

                sent = getservbyport(addr->sin_port,
                                     (flags & NI_DGRAM)
                                     ? "udp" : "tcp");
                if (sent != NULL)
                {
                    strncpy (serv, sent->s_name, servlen);
                    serv[servlen - 1] = 0;
                    solved = 1;
                }
            }
#else
            sent = NULL;
#endif
            if (sent == NULL)
            {
                snprintf (serv, servlen, "%u",
                          (unsigned int)ntohs (addr->sin_port));
                serv[servlen - 1] = '\0';
            }
        }
    }
    return 0;
}

#endif /* if !HAVE_GETNAMEINFO */


#ifndef HAVE_GETADDRINFO
/*
 * This functions must be used to free the memory allocated by getaddrinfo().
 */
static void
__freeaddrinfo (struct addrinfo *res)
{
    if (res != NULL)
    {
        if (res->ai_canonname != NULL)
            free (res->ai_canonname);
        if (res->ai_addr != NULL)
            free (res->ai_addr);
        if (res->ai_next != NULL)
            free (res->ai_next);
        free (res);
    }
}


/*
 * Internal function that builds an addrinfo struct.
 */
static struct addrinfo *
makeaddrinfo (int af, int type, int proto,
              const struct sockaddr *addr, size_t addrlen,
              const char *canonname)
{
    struct addrinfo *res;

    res = (struct addrinfo *)malloc (sizeof (struct addrinfo));
    if (res != NULL)
    {
        res->ai_flags = 0;
        res->ai_family = af;
        res->ai_socktype = type;
        res->ai_protocol = proto;
        res->ai_addrlen = addrlen;
        res->ai_addr = malloc (addrlen);
        res->ai_canonname = NULL;
        res->ai_next = NULL;

        if (res->ai_addr != NULL)
        {
            memcpy (res->ai_addr, addr, addrlen);

            if (canonname != NULL)
            {
                res->ai_canonname = strdup (canonname);
                if (res->ai_canonname != NULL)
                    return res; /* success ! */
            }
            else
                return res;
        }
    }
    /* failsafe */
    vlc_freeaddrinfo (res);
    return NULL;
}


static struct addrinfo *
makeipv4info (int type, int proto, u_long ip, u_short port, const char *name)
{
    struct sockaddr_in addr;

    memset (&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
# ifdef HAVE_SA_LEN
    addr.sin_len = sizeof (addr);
# endif
    addr.sin_port = port;
    addr.sin_addr.s_addr = ip;

    return makeaddrinfo (AF_INET, type, proto,
                         (struct sockaddr*)&addr, sizeof (addr), name);
}


/*
 * getaddrinfo() non-thread-safe IPv4-only implementation
 * Address-family-independant hostname to address resolution.
 *
 * This is meant for IPv6-unaware systems that do probably not provide
 * getaddrinfo(), but still have old function gethostbyname().
 *
 * Only UDP and TCP over IPv4 are supported here.
 */
static int
__getaddrinfo (const char *node, const char *service,
               const struct addrinfo *hints, struct addrinfo **res)
{
    struct addrinfo *info;
    u_long ip;
    u_short port;
    int protocol = 0, flags = 0;
    const char *name = NULL;

    if (hints != NULL)
    {
        flags = hints->ai_flags;

        if (flags & ~_AI_MASK)
            return EAI_BADFLAGS;
        /* only accept AF_INET and AF_UNSPEC */
        if (hints->ai_family && (hints->ai_family != AF_INET))
            return EAI_FAMILY;

        /* protocol sanity check */
        switch (hints->ai_socktype)
        {
            case SOCK_STREAM:
                protocol = IPPROTO_TCP;
                break;

            case SOCK_DGRAM:
                protocol = IPPROTO_UDP;
                break;

#ifndef SYS_BEOS
            case SOCK_RAW:
#endif
            case 0:
                break;

            default:
                return EAI_SOCKTYPE;
        }
        if (hints->ai_protocol && protocol
         && (protocol != hints->ai_protocol))
            return EAI_SERVICE;
    }

    *res = NULL;

    /* default values */
    if (node == NULL)
    {
        if (flags & AI_PASSIVE)
            ip = htonl (INADDR_ANY);
        else
            ip = htonl (INADDR_LOOPBACK);
    }
    else
    if ((ip = inet_addr (node)) == INADDR_NONE)
    {
        struct hostent *entry = NULL;

        /* hostname resolution */
        if (!(flags & AI_NUMERICHOST))
            entry = gethostbyname (node);

        if (entry == NULL)
            return EAI_NONAME;

        if ((entry->h_length != 4) || (entry->h_addrtype != AF_INET))
            return EAI_FAMILY;

        ip = *((u_long *) entry->h_addr);
        if (flags & AI_CANONNAME)
            name = entry->h_name;
    }

    if ((flags & AI_CANONNAME) && (name == NULL))
        name = node;

    /* service resolution */
    if (service == NULL)
        port = 0;
    else
    {
        long d;
        char *end;

        d = strtoul (service, &end, 0);
        if (end[0] /* service is not a number */
         || (d > 65535))
        {
            struct servent *entry;
            const char *protoname;

            switch (protocol)
            {
                case IPPROTO_TCP:
                    protoname = "tcp";
                    break;

                case IPPROTO_UDP:
                    protoname = "udp";
                    break;

                default:
                    protoname = NULL;
            }

            entry = getservbyname (service, protoname);
            if (entry == NULL)
                return EAI_SERVICE;

            port = entry->s_port;
        }
        else
            port = htons ((u_short)d);
    }

    /* building results... */
    if ((!protocol) || (protocol == IPPROTO_UDP))
    {
        info = makeipv4info (SOCK_DGRAM, IPPROTO_UDP, ip, port, name);
        if (info == NULL)
        {
            errno = ENOMEM;
            return EAI_SYSTEM;
        }
        if (flags & AI_PASSIVE)
            info->ai_flags |= AI_PASSIVE;
        *res = info;
    }
    if ((!protocol) || (protocol == IPPROTO_TCP))
    {
        info = makeipv4info (SOCK_STREAM, IPPROTO_TCP, ip, port, name);
        if (info == NULL)
        {
            errno = ENOMEM;
            return EAI_SYSTEM;
        }
        info->ai_next = *res;
        if (flags & AI_PASSIVE)
            info->ai_flags |= AI_PASSIVE;
        *res = info;
    }

    return 0;
}
#endif /* if !HAVE_GETADDRINFO */


int vlc_getnameinfo( const struct sockaddr *sa, int salen,
                     char *host, int hostlen, int *portnum, int flags )
{
    char psz_servbuf[6], *psz_serv;
    int i_servlen, i_val;

    flags |= NI_NUMERICSERV;
    if( portnum != NULL )
    {
        psz_serv = psz_servbuf;
        i_servlen = sizeof( psz_servbuf );
    }
    else
    {
        psz_serv = NULL;
        i_servlen = 0;
    }
#ifdef WIN32
    /*
     * Here is the kind of kludge you need to keep binary compatibility among
     * varying OS versions...
     */
    typedef int (CALLBACK * GETNAMEINFO) ( const struct sockaddr*, socklen_t,
                                           char*, DWORD, char*, DWORD, int );
    HINSTANCE wship6_module;
    GETNAMEINFO ws2_getnameinfo;
     
    wship6_module = LoadLibrary( "wship6.dll" );
    if( wship6_module != NULL )
    {
        ws2_getnameinfo = (GETNAMEINFO)GetProcAddress( wship6_module,
                                                       "getnameinfo" );

        if( ws2_getnameinfo != NULL )
        {
            i_val = ws2_getnameinfo( sa, salen, host, hostlen, psz_serv,
                                     i_servlen, flags );
            FreeLibrary( wship6_module );

            if( portnum != NULL )
                *portnum = atoi( psz_serv );
            return i_val;
        }
            
        FreeLibrary( wship6_module );
    }
#endif
#if HAVE_GETNAMEINFO
    i_val = getnameinfo( sa, salen, host, hostlen, psz_serv, i_servlen,
                         flags );
#else
    {
# ifdef HAVE_USABLE_MUTEX_THAT_DONT_NEED_LIBVLC_POINTER
        static vlc_value_t lock;
    
        /* my getnameinfo implementation is not thread-safe as it uses
         * gethostbyaddr and the likes */
        vlc_mutex_lock( lock.p_address );
#else
# warning FIXME : This is not thread-safe!
#endif
        i_val = __getnameinfo( sa, salen, host, hostlen, psz_serv, i_servlen,
                               flags );
# ifdef HAVE_USABLE_MUTEX_THAT_DONT_NEED_LIBVLC_POINTER
        vlc_mutex_unlock( lock.p_address );
# endif
    }
#endif

    if( portnum != NULL )
        *portnum = atoi( psz_serv );

    return i_val;
}


/* TODO: support for setting sin6_scope_id */
int vlc_getaddrinfo( vlc_object_t *p_this, const char *node,
                     int i_port, const struct addrinfo *p_hints,
                     struct addrinfo **res )
{
    struct addrinfo hints;
    char psz_buf[NI_MAXHOST], *psz_node, psz_service[6];

    /*
     * In VLC, we always use port number as integer rather than strings
     * for historical reasons (and portability).
     */
    if( ( i_port > 65535 ) || ( i_port < 0 ) )
    {
        msg_Err( p_this, "invalid port number %d specified", i_port );
        return EAI_SERVICE;
    }

    /* cannot overflow */
    snprintf( psz_service, 6, "%d", i_port );

    /* Check if we have to force ipv4 or ipv6 */
    if( p_hints == NULL )
        memset( &hints, 0, sizeof( hints ) );
    else
        memcpy( &hints, p_hints, sizeof( hints ) );

    if( hints.ai_family == AF_UNSPEC )
    {
        vlc_value_t val;

        var_Create( p_this, "ipv4", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Get( p_this, "ipv4", &val );
        if( val.b_bool )
            hints.ai_family = AF_INET;

#ifdef AF_INET6
        var_Create( p_this, "ipv6", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Get( p_this, "ipv6", &val );
        if( val.b_bool )
            hints.ai_family = AF_INET6;
#endif
    }

    /* 
     * VLC extensions :
     * - accept "" as NULL
     * - ignore square brackets
     */
    if( ( node == NULL ) || (node[0] == '\0' ) )
    {
        psz_node = NULL;
    }
    else
    {
        strncpy( psz_buf, node, NI_MAXHOST );
        psz_buf[NI_MAXHOST - 1] = '\0';

        psz_node = psz_buf;

        if( psz_buf[0] == '[' )
        {
            char *ptr;

            ptr = strrchr( psz_buf, ']' );
            if( ( ptr != NULL ) && (ptr[1] == '\0' ) )
            {
                *ptr = '\0';
                psz_node++;
            }
        }
    }

#ifdef WIN32
    {
        typedef int (CALLBACK * GETADDRINFO) ( const char *, const char *,
                                            const struct addrinfo *,
                                            struct addrinfo ** );
        HINSTANCE wship6_module;
        GETADDRINFO ws2_getaddrinfo;
         
        wship6_module = LoadLibrary( "wship6.dll" );
        if( wship6_module != NULL )
        {
            ws2_getaddrinfo = (GETADDRINFO)GetProcAddress( wship6_module,
                                                        "getaddrinfo" );

            if( ws2_getaddrinfo != NULL )
            {
                int i_ret;

                i_ret = ws2_getaddrinfo( psz_node, psz_service, &hints, res );
                FreeLibrary( wship6_module ); /* is this wise ? */
                return i_ret;
            }

            FreeLibrary( wship6_module );
        }
    }
#endif
#if HAVE_GETADDRINFO
    return getaddrinfo( psz_node, psz_service, &hints, res );
#else
{
    int i_ret;

    vlc_value_t lock;

    var_Create( p_this->p_libvlc, "getaddrinfo_mutex", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "getaddrinfo_mutex", &lock );
    vlc_mutex_lock( lock.p_address );

    i_ret = __getaddrinfo( psz_node, psz_service, &hints, res );
    vlc_mutex_unlock( lock.p_address );
    return i_ret;
}
#endif
}


void vlc_freeaddrinfo( struct addrinfo *infos )
{
#ifdef WIN32
    typedef void (CALLBACK * FREEADDRINFO) ( struct addrinfo * );
    HINSTANCE wship6_module;
    FREEADDRINFO ws2_freeaddrinfo;
     
    wship6_module = LoadLibrary( "wship6.dll" );
    if( wship6_module != NULL )
    {
        ws2_freeaddrinfo = (FREEADDRINFO)GetProcAddress( wship6_module,
                                                         "freeaddrinfo" );

        /*
         * NOTE: it is assumed that wship6.dll defines either both
         * getaddrinfo and freeaddrinfo or none of them.
         */
        if( ws2_freeaddrinfo != NULL )
        {
            ws2_freeaddrinfo( infos );
            FreeLibrary( wship6_module );
            return;
        }

        FreeLibrary( wship6_module );
    }
#endif
#ifdef HAVE_GETADDRINFO
    freeaddrinfo( infos );
#else
    __freeaddrinfo( infos );
#endif
}
