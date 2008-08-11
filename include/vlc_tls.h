/*****************************************************************************
 * tls.c: Transport Layer Security API
 *****************************************************************************
 * Copyright (C) 2004-2007 the VideoLAN team
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

#ifndef VLC_TLS_H
# define VLC_TLS_H

/**
 * \file
 * This file defines Transport Layer Security API (TLS) in vlc
 */

# include <vlc_network.h>

typedef struct tls_server_sys_t tls_server_sys_t;

struct tls_server_t
{
    VLC_COMMON_MEMBERS

    module_t  *p_module;
    tls_server_sys_t *p_sys;

    int (*pf_add_CA) ( tls_server_t *, const char * );
    int (*pf_add_CRL) ( tls_server_t *, const char * );

    tls_session_t * (*pf_open)  ( tls_server_t * );
    void            (*pf_close) ( tls_server_t *, tls_session_t * );
};

typedef struct tls_session_sys_t tls_session_sys_t;

struct tls_session_t
{
    VLC_COMMON_MEMBERS

    module_t  *p_module;
    tls_session_sys_t *p_sys;

    struct virtual_socket_t sock;
    void (*pf_set_fd) ( tls_session_t *, int );
    int  (*pf_handshake) ( tls_session_t * );
};


tls_server_t *tls_ServerCreate (vlc_object_t *, const char *, const char *);
void tls_ServerDelete (tls_server_t *);
int tls_ServerAddCA (tls_server_t *srv, const char *path);
int tls_ServerAddCRL (tls_server_t *srv, const char *path);

tls_session_t *tls_ServerSessionPrepare (tls_server_t *);
int tls_ServerSessionHandshake (tls_session_t *, int fd);
int tls_SessionContinueHandshake (tls_session_t *);
void tls_ServerSessionClose (tls_session_t *);

VLC_EXPORT( tls_session_t *, tls_ClientCreate, ( vlc_object_t *, int, const char * ) );
VLC_EXPORT( void, tls_ClientDelete, ( tls_session_t * ) );

/* NOTE: It is assumed that a->sock.p_sys = a */
# define tls_Send( a, b, c ) (((tls_session_t *)a)->sock.pf_send (a, b, c ))

# define tls_Recv( a, b, c ) (((tls_session_t *)a)->sock.pf_recv (a, b, c ))

#endif
