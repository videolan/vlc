/*****************************************************************************
 * network.h: interface to communicate with network plug-ins
 *****************************************************************************
 * Copyright (C) 2002-2005 VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Rémi Denis-Courmont <courmisch # via.ecp.fr>
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
/*****************************************************************************
 * network_socket_t: structure passed to a network plug-in to define the
 *                   kind of socket we want
 *****************************************************************************/
struct network_socket_t
{
    unsigned int i_type;

    char * psz_bind_addr;
    int i_bind_port;

    char * psz_server_addr;
    int i_server_port;

    int i_ttl;

    /* Return values */
    int i_handle;
    size_t i_mtu;
};

/* Socket types */
#define NETWORK_UDP 1
#define NETWORK_TCP 2
#define NETWORK_TCP_PASSIVE 3


typedef struct
{
    char *psz_protocol;
    char *psz_host;
    int  i_port;

    char *psz_path;

    char *psz_option;
} vlc_url_t;

/*****************************************************************************
 * vlc_UrlParse:
 *****************************************************************************
 * option : if != 0 then path is split at this char
 *
 * format [protocol://][host[:port]]/path[OPTIONoption]
 *****************************************************************************/
static inline void vlc_UrlParse( vlc_url_t *url, char *psz_url, char option )
{
    char *psz_dup = psz_url ? strdup( psz_url ) : 0;
    char *psz_parse = psz_dup;
    char *p;

    url->psz_protocol = NULL;
    url->psz_host     = NULL;
    url->i_port       = 0;
    url->psz_path     = NULL;
    url->psz_option   = NULL;

    if( !psz_url ) return;

    if( ( p  = strstr( psz_parse, ":/" ) ) )
    {
        /* we have a protocol */

        /* skip :// */
        *p++ = '\0';
        if( p[0] == '/' && p[1] == '/' )
        {
            p += 2;
        }
        url->psz_protocol = strdup( psz_dup );

        psz_parse = p;
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
        url->psz_path = strdup( psz_parse );
        if( option != '\0' )
        {
            p = strchr( url->psz_path, option );
            if( p )
            {
                *p++ = '\0';
                url->psz_option = strdup( p );
            }
        }
    }
    free( psz_dup );
}

/*****************************************************************************
 * vlc_UrlClean:
 *****************************************************************************
 *
 *****************************************************************************/
static inline void vlc_UrlClean( vlc_url_t *url )
{
    if( url->psz_protocol ) free( url->psz_protocol );
    if( url->psz_host )     free( url->psz_host );
    if( url->psz_path )     free( url->psz_path );
    if( url->psz_option )   free( url->psz_option );

    url->psz_protocol = NULL;
    url->psz_host     = NULL;
    url->i_port       = 0;
    url->psz_path     = NULL;
    url->psz_option   = NULL;
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
static inline char *vlc_b64_encode( unsigned char *src )
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                                                                                
    char *dst = malloc( strlen( src ) * 4 / 3 + 12 );
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

VLC_EXPORT( int, net_ConvertIPv4, ( uint32_t *p_addr, const char * psz_address ) );

/* Portable networking layer communication */
#define net_OpenTCP(a, b, c) __net_OpenTCP(VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __net_OpenTCP, ( vlc_object_t *p_this, const char *psz_host, int i_port ) );

#define net_ListenTCP(a, b, c) __net_ListenTCP(VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __net_ListenTCP, ( vlc_object_t *p_this, char *psz_localaddr, int i_port ) );

#define net_Accept(a, b, c) __net_Accept(VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __net_Accept, ( vlc_object_t *p_this, int fd_listen, mtime_t i_wait ) );

#define net_OpenUDP(a, b, c, d, e ) __net_OpenUDP(VLC_OBJECT(a), b, c, d, e)
VLC_EXPORT( int, __net_OpenUDP, ( vlc_object_t *p_this, char *psz_bind, int i_bind, char *psz_server, int i_server ) );

VLC_EXPORT( void, net_Close, ( int fd ) );


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

#endif
