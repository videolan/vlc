/*****************************************************************************
 * vlc_tls.h: Transport Layer Security API
 *****************************************************************************
 * Copyright (C) 2004-2011 Rémi Denis-Courmont
 * Copyright (C) 2005-2006 VLC authors and VideoLAN
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

#ifndef VLC_TLS_H
# define VLC_TLS_H

/**
 * \file
 * This file defines Transport Layer Security API (TLS) in vlc
 */

# include <vlc_network.h>

typedef struct vlc_tls_sys vlc_tls_sys_t;

typedef struct vlc_tls
{
    VLC_COMMON_MEMBERS

    union {
        module_t *module; /**< Plugin handle (client) */
        void    (*close) (struct vlc_tls *); /**< Close callback (server) */
    } u;
    vlc_tls_sys_t *sys;

    struct virtual_socket_t sock;
    int  (*handshake) (struct vlc_tls *);
} vlc_tls_t;

VLC_API vlc_tls_t *vlc_tls_ClientCreate (vlc_object_t *, int fd,
                                         const char *hostname);
VLC_API void vlc_tls_ClientDelete (vlc_tls_t *);

/* NOTE: It is assumed that a->sock.p_sys = a */
# define tls_Send( a, b, c ) (((vlc_tls_t *)a)->sock.pf_send (a, b, c))

# define tls_Recv( a, b, c ) (((vlc_tls_t *)a)->sock.pf_recv (a, b, c))


typedef struct vlc_tls_creds_sys vlc_tls_creds_sys_t;

/** TLS (server-side) credentials */
typedef struct vlc_tls_creds
{
    VLC_COMMON_MEMBERS

    module_t  *module;
    vlc_tls_creds_sys_t *sys;

    int (*add_CA) (struct vlc_tls_creds *, const char *path);
    int (*add_CRL) (struct vlc_tls_creds *, const char *path);

    vlc_tls_t *(*open) (struct vlc_tls_creds *, int fd);
} vlc_tls_creds_t;

vlc_tls_creds_t *vlc_tls_ServerCreate (vlc_object_t *,
                                       const char *cert, const char *key);
void vlc_tls_ServerDelete (vlc_tls_creds_t *);
int vlc_tls_ServerAddCA (vlc_tls_creds_t *srv, const char *path);
int vlc_tls_ServerAddCRL (vlc_tls_creds_t *srv, const char *path);

vlc_tls_t *vlc_tls_ServerSessionCreate (vlc_tls_creds_t *, int fd);
int vlc_tls_ServerSessionHandshake (vlc_tls_t *);
void vlc_tls_ServerSessionDelete (vlc_tls_t *);

#endif
