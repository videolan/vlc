/*****************************************************************************
 * tls.c: TLS wrapper
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
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

#ifndef _VLC_TLS_H
# define _VLC_TLS_H

# include "network.h"

struct tls_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t  *p_module;
    void *p_sys;

    tls_server_t * (*pf_server_create) ( tls_t *, const char *,
                                         const char * );
    tls_session_t * (*pf_client_create) ( tls_t * );
};

struct tls_server_t
{
    VLC_COMMON_MEMBERS

    void *p_sys;

    void (*pf_delete) ( tls_server_t * );

    int (*pf_add_CA) ( tls_server_t *, const char * );
    int (*pf_add_CRL) ( tls_server_t *, const char * );

    tls_session_t * (*pf_session_prepare) ( tls_server_t * );
};

struct tls_session_t
{
    VLC_COMMON_MEMBERS

    void *p_sys;

    struct virtual_socket_t sock;
    int (*pf_handshake) ( tls_session_t *, int, const char * );
    int (*pf_handshake2) ( tls_session_t * );
    void (*pf_close) ( tls_session_t * );
};


/*****************************************************************************
 * tls_ServerCreate:
 *****************************************************************************
 * Allocates a whole server's TLS credentials.
 * Returns NULL on error.
 *****************************************************************************/
VLC_EXPORT( tls_server_t *, tls_ServerCreate, ( vlc_object_t *, const char *, const char * ) );

/*****************************************************************************
 * tls_ServerAddCA:
 *****************************************************************************
 * Adds one or more certificate authorities.
 * Returns -1 on error, 0 on success.
 *****************************************************************************/
# define tls_ServerAddCA( a, b ) (((tls_server_t *)a)->pf_add_CA (a, b))


/*****************************************************************************
 * tls_ServerAddCRL:
 *****************************************************************************
 * Adds a certificates revocation list to be sent to TLS clients.
 * Returns -1 on error, 0 on success.
 *****************************************************************************/
# define tls_ServerAddCRL( a, b ) (((tls_server_t *)a)->pf_add_CRL (a, b))


VLC_EXPORT( void, tls_ServerDelete, ( tls_server_t * ) );


# define tls_ServerSessionPrepare( a ) (((tls_server_t *)a)->pf_session_prepare (a))
# define tls_ServerSessionHandshake( a, b ) (((tls_session_t *)a)->pf_handshake (a, b, NULL))
# define tls_ServerSessionClose( a ) (((tls_session_t *)a)->pf_close (a))

VLC_EXPORT( tls_session_t *, tls_ClientCreate, ( vlc_object_t *, int, const char * ) );
VLC_EXPORT( void, tls_ClientDelete, ( tls_session_t * ) );

# define tls_ClientSessionHandshake( a, b, c ) (((tls_session_t *)a)->pf_handshake (a, b, c))

# define tls_SessionContinueHandshake( a ) (((tls_session_t *)a)->pf_handshake2 (a))


/* NOTE: It is assumed that a->sock.p_sys = a */
# define tls_Send( a, b, c ) (((tls_session_t *)a)->sock.pf_send (a, b, c ))

# define tls_Recv( a, b, c ) (((tls_session_t *)a)->sock.pf_recv (a, b, c ))

#endif
