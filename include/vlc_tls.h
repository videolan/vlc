/*****************************************************************************
 * tls.c
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: httpd.c 8263 2004-07-24 09:06:58Z courmisch $
 *
 * Authors: Remi Denis-Courmont <courmisch@via.ecp.fr>
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

#ifndef _VLC_TLS_H
# define _VLC_TLS_H

struct tls_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t  *p_module;
    void *p_sys;

    tls_server_t * (*pf_server_create) ( tls_t *, const char *, const char * );
};

struct tls_server_t
{
    tls_t *p_tls;
    void *p_sys;

    void (*pf_delete) ( tls_server_t * );
    
    int (*pf_add_CA) ( tls_server_t *, const char * );
    int (*pf_add_CRL) ( tls_server_t *, const char * );
    
    tls_session_t * (*pf_session_prepare) ( tls_server_t * );
};

struct tls_session_t
{
    tls_t *p_tls;
    tls_server_t *p_server;

    void *p_sys;

    tls_session_t * (*pf_handshake) ( tls_session_t *, int );
    void (*pf_close) ( tls_session_t * );
    int (*pf_send) ( tls_session_t *, const char *, int );
    int (*pf_recv) ( tls_session_t *, char *, int );
};


/*****************************************************************************
 * tls_ServerCreate:
 *****************************************************************************
 * Allocates a whole server's TLS credentials.
 * Returns NULL on error.
 *****************************************************************************/
# define __tls_ServerCreate( a, b, c ) (((tls_t *)a)->pf_server_create (a, b, c))
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


# define __tls_ServerDelete( a ) (((tls_server_t *)a)->pf_delete ( a ))
VLC_EXPORT( void, tls_ServerDelete, ( tls_server_t * ) );


# define tls_ServerSessionPrepare( a ) (((tls_server_t *)a)->pf_session_prepare (a))

# define tls_SessionHandshake( a, b ) (((tls_session_t *)a)->pf_handshake (a, b))

# define tls_SessionClose( a ) (((tls_session_t *)a)->pf_close (a))

# define tls_Send( a, b, c ) (((tls_session_t *)a)->pf_send (a, b, c ))

# define tls_Recv( a, b, c ) (((tls_session_t *)a)->pf_recv (a, b, c ))

#endif
