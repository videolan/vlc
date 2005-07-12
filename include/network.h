/*****************************************************************************
 * network.h: interface to communicate with network plug-ins
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef __VLC_NETWORK_H
# define __VLC_NETWORK_H

#if defined( UNDER_CE )
#   include <winsock.h>
#elif defined( WIN32 )
#   include <winsock2.h>
#elif HAVE_SYS_SOCKET_H
#   include <sys/socket.h>
#endif

/*****************************************************************************
 * network_socket_t: structure passed to a network plug-in to define the
 *                   kind of socket we want
 *****************************************************************************/
struct network_socket_t
{
    char * psz_bind_addr;
    int i_bind_port;

    char * psz_server_addr;
    int i_server_port;

    int i_ttl;

    /* Return values */
    int i_handle;
    size_t i_mtu;
};

typedef struct
{
    char *psz_protocol;
    char *psz_username;
    char *psz_password;
    char *psz_host;
    int  i_port;

    char *psz_path;

    char *psz_option;
    
    char *psz_buffer; /* to be freed */
} vlc_url_t;

/*****************************************************************************
 * vlc_UrlParse:
 *****************************************************************************
 * option : if != 0 then path is split at this char
 *
 * format [protocol://[login[:password]@]][host[:port]]/path[OPTIONoption]
 *****************************************************************************/
static inline void vlc_UrlParse( vlc_url_t *url, const char *psz_url,
                                 char option )
{
    char *psz_dup;
    char *psz_parse;
    char *p;

    url->psz_protocol = NULL;
    url->psz_username = NULL;
    url->psz_password = NULL;
    url->psz_host     = NULL;
    url->i_port       = 0;
    url->psz_path     = NULL;
    url->psz_option   = NULL;
    
    if( psz_url == NULL )
    {
        url->psz_buffer = NULL;
        return;
    }
    url->psz_buffer = psz_parse = psz_dup = strdup( psz_url );

    p  = strstr( psz_parse, ":/" );
    if( p != NULL )
    {
        /* we have a protocol */

        /* skip :// */
        *p++ = '\0';
        if( p[1] == '/' )
            p += 2;
        url->psz_protocol = psz_parse;

        psz_parse = p;
        p = strchr( psz_parse, '@' );
        if( p != NULL )
        {
            /* We have a login */
            url->psz_username = psz_parse;
            *p++ = '\0';

            psz_parse = strchr( psz_parse, ':' );
            if( psz_parse != NULL )
            {
                /* We have a password */
                *psz_parse++ = '\0';
                url->psz_password = psz_parse;
            }

            psz_parse = p;
        }
    }

    p = strchr( psz_parse, '/' );
    if( !p || psz_parse < p )
    {
        char *p2;

        /* We have a host[:port] */
        url->psz_host = strdup( psz_parse );
        if( p )
        {
            url->psz_host[p - psz_parse] = '\0';
        }

        if( *url->psz_host == '[' )
        {
            /* Ipv6 address */
            p2 = strchr( url->psz_host, ']' );
            if( p2 )
            {
                p2 = strchr( p2, ':' );
            }
        }
        else
        {
            p2 = strchr( url->psz_host, ':' );
        }
        if( p2 )
        {
            *p2++ = '\0';
            url->i_port = atoi( p2 );
        }
    }
    psz_parse = p;

    /* Now parse psz_path and psz_option */
    if( psz_parse )
    {
        url->psz_path = psz_parse;
        if( option != '\0' )
        {
            p = strchr( url->psz_path, option );
            if( p )
            {
                *p++ = '\0';
                url->psz_option = p;
            }
        }
    }
}

/*****************************************************************************
 * vlc_UrlClean:
 *****************************************************************************
 *
 *****************************************************************************/
static inline void vlc_UrlClean( vlc_url_t *url )
{
    if( url->psz_buffer ) free( url->psz_buffer );
    if( url->psz_host )   free( url->psz_host );

    url->psz_protocol = NULL;
    url->psz_username = NULL;
    url->psz_password = NULL;
    url->psz_host     = NULL;
    url->i_port       = 0;
    url->psz_path     = NULL;
    url->psz_option   = NULL;

    url->psz_buffer   = NULL;
}

/*****************************************************************************
 * vlc_UrlEncode: 
 *****************************************************************************
 * perform URL encoding
 * (you do NOT want to do URL decoding - it is not reversible - do NOT do it)
 *****************************************************************************/
static inline char *vlc_UrlEncode( const char *psz_url )
{
    char *psz_enc, *out;
    const char *in;

    psz_enc = (char *)malloc( 3 * strlen( psz_url ) + 1 );
    if( psz_enc == NULL )
        return NULL;

    out = psz_enc;
    for( in = psz_url; *in; in++ )
    {
        char c = *in;

        if( ( c <= 32 ) || ( c == '%' ) || ( c == '?' ) || ( c == '&' )
         || ( c == '+' ) )
        {
            *out++ = '%';   
            *out++ = ( ( c >> 4 ) >= 0xA ) ? 'A' + ( c >> 4 ) - 0xA
                                           : '0' + ( c >> 4 );
            *out++ = ( ( c & 0xf ) >= 0xA ) ? 'A' + ( c & 0xf ) - 0xA
                                           : '0' + ( c & 0xf );
        }
        else
            *out++ = c;
    }
    *out++ = '\0';

    return (char *)realloc( psz_enc, out - psz_enc );
}

/*****************************************************************************
 * vlc_UrlIsNotEncoded:
 *****************************************************************************
 * check if given string is not a valid URL and must hence be encoded
 *****************************************************************************/
#include <ctype.h>

static inline int vlc_UrlIsNotEncoded( const char *psz_url )
{
    const char *ptr;

    for( ptr = psz_url; *ptr; ptr++ )
    {
        char c = *ptr;

        if( c == '%' )
        {
            if( !isxdigit( ptr[1] ) || !isxdigit( ptr[2] ) )
                return 1; /* not encoded */
            ptr += 2;
        }
        else
        if( c == ' ' )
            return 1;
    }
    return 0; /* looks fine - but maybe it is not encoded */
}
                    
/*****************************************************************************
 * vlc_b64_encode:
 *****************************************************************************
 *
 *****************************************************************************/
static inline char *vlc_b64_encode( char *src )
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                                                                                
    char *dst = (char *)malloc( strlen( src ) * 4 / 3 + 12 );
    char *ret = dst;
    unsigned i_bits = 0;
    unsigned i_shift = 0;
                                                                                
    for( ;; )
    {
        if( *src )
        {
            i_bits = ( i_bits << 8 )|( *src++ );
            i_shift += 8;
        }
        else if( i_shift > 0 )
        {
           i_bits <<= 6 - i_shift;
           i_shift = 6;
        }
        else
        {
            *dst++ = '=';
            break;
        }
                                                                                
        while( i_shift >= 6 )
        {
            i_shift -= 6;
            *dst++ = b64[(i_bits >> i_shift)&0x3f];
        }
    }
                                                                                
    *dst++ = '\0';
                                                                                
    return ret;
}

/* Portable networking layer communication */
#define net_OpenTCP(a, b, c) __net_OpenTCP(VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __net_OpenTCP, ( vlc_object_t *p_this, const char *psz_host, int i_port ) );

#define net_ListenTCP(a, b, c) __net_ListenTCP(VLC_OBJECT(a), b, c)
VLC_EXPORT( int *, __net_ListenTCP, ( vlc_object_t *, const char *, int ) );

#define net_Accept(a, b, c) __net_Accept(VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __net_Accept, ( vlc_object_t *, int *, mtime_t ) );

#define net_OpenUDP(a, b, c, d, e ) __net_OpenUDP(VLC_OBJECT(a), b, c, d, e)
VLC_EXPORT( int, __net_OpenUDP, ( vlc_object_t *p_this, char *psz_bind, int i_bind, char *psz_server, int i_server ) );

VLC_EXPORT( void, net_Close, ( int fd ) );
VLC_EXPORT( void, net_ListenClose, ( int *fd ) );


/* Functions to read from or write to the networking layer */
struct virtual_socket_t
{
    void *p_sys;
    int (*pf_recv) ( void *, void *, int );
    int (*pf_send) ( void *, const void *, int );
};

#define net_Read(a,b,c,d,e,f) __net_Read(VLC_OBJECT(a),b,c,d,e,f)
VLC_EXPORT( int, __net_Read, ( vlc_object_t *p_this, int fd, v_socket_t *, uint8_t *p_data, int i_data, vlc_bool_t b_retry ) );

#define net_ReadNonBlock(a,b,c,d,e,f) __net_ReadNonBlock(VLC_OBJECT(a),b,c,d,e,f)
VLC_EXPORT( int, __net_ReadNonBlock, ( vlc_object_t *p_this, int fd, v_socket_t *, uint8_t *p_data, int i_data, mtime_t i_wait ) );

#define net_Select(a,b,c,d,e,f,g) __net_Select(VLC_OBJECT(a),b,c,d,e,f,g)
VLC_EXPORT( int, __net_Select, ( vlc_object_t *p_this, int *pi_fd, v_socket_t **, int i_fd, uint8_t *p_data, int i_data, mtime_t i_wait ) );

#define net_Write(a,b,c,d,e) __net_Write(VLC_OBJECT(a),b,c,d,e)
VLC_EXPORT( int, __net_Write, ( vlc_object_t *p_this, int fd, v_socket_t *, uint8_t *p_data, int i_data ) );

#define net_Gets(a,b,c) __net_Gets(VLC_OBJECT(a),b,c)
VLC_EXPORT( char *, __net_Gets, ( vlc_object_t *p_this, int fd, v_socket_t * ) );

VLC_EXPORT( int, net_Printf, ( vlc_object_t *p_this, int fd, v_socket_t *, const char *psz_fmt, ... ) );

#define net_vaPrintf(a,b,c,d,e) __net_vaPrintf(VLC_OBJECT(a),b,c,d,e)
VLC_EXPORT( int, __net_vaPrintf, ( vlc_object_t *p_this, int fd, v_socket_t *, const char *psz_fmt, va_list args ) );

/*****************************************************************************
 * net_StopRecv/Send
 *****************************************************************************
 * Wrappers for shutdown()
 *****************************************************************************/
#if defined (SHUT_WR)
/* the standard way */
# define net_StopSend( fd ) (void)shutdown( fd, SHUT_WR )
# define net_StopRecv( fd ) (void)shutdown( fd, SHUT_RD )
#elif defined (SD_SEND)
/* the Microsoft seemingly-purposedly-different-for-the-sake-of-it way */
# define net_StopSend( fd ) (void)shutdown( fd, SD_SEND )
# define net_StopRecv( fd ) (void)shutdown( fd, SD_RECEIVE )
#else
# warning FIXME: implement shutdown on your platform!
# define net_StopSend( fd ) (void)0
# define net_StopRecv( fd ) (void)0
#endif

/* Portable network names/addresses resolution layer */

/* GAI error codes */
# ifndef EAI_BADFLAGS
#  define EAI_BADFLAGS -1
# endif
# ifndef EAI_NONAME
#  define EAI_NONAME -2
# endif
# ifndef EAI_AGAIN
#  define EAI_AGAIN -3
# endif
# ifndef EAI_FAIL
#  define EAI_FAIL -4
# endif
# ifndef EAI_NODATA
#  define EAI_NODATA -5
# endif
# ifndef EAI_FAMILY
#  define EAI_FAMILY -6
# endif
# ifndef EAI_SOCKTYPE
#  define EAI_SOCKTYPE -7
# endif
# ifndef EAI_SERVICE
#  define EAI_SERVICE -8
# endif
# ifndef EAI_ADDRFAMILY
#  define EAI_ADDRFAMILY -9
# endif
# ifndef EAI_MEMORY
#  define EAI_MEMORY -10
# endif
# ifndef EAI_SYSTEM
#  define EAI_SYSTEM -11
# endif


# ifndef NI_MAXHOST
#  define NI_MAXHOST 1025
#  define NI_MAXSERV 32
# endif
# define NI_MAXNUMERICHOST 48

# ifndef NI_NUMERICHOST
#  define NI_NUMERICHOST 0x01
#  define NI_NUMERICSERV 0x02
#  define NI_NOFQDN      0x04
#  define NI_NAMEREQD    0x08
#  define NI_DGRAM       0x10
# endif

# ifndef HAVE_STRUCT_ADDRINFO
struct addrinfo
{
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};
#  define AI_PASSIVE     1
#  define AI_CANONNAME   2
#  define AI_NUMERICHOST 4
# endif /* if !HAVE_STRUCT_ADDRINFO */

/*** libidn support ***/
# ifndef AI_IDN
#  define AI_IDN      0
#  define AI_CANONIDN 0
# endif

VLC_EXPORT( const char *, vlc_gai_strerror, ( int ) );
VLC_EXPORT( int, vlc_getnameinfo, ( const struct sockaddr *, int, char *, int, int *, int ) );
VLC_EXPORT( int, vlc_getaddrinfo, ( vlc_object_t *, const char *, int, const struct addrinfo *, struct addrinfo ** ) );
VLC_EXPORT( void, vlc_freeaddrinfo, ( struct addrinfo * ) );

#endif
