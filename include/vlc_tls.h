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
VLC_EXPORT( int, tls_ServerAddCA, ( tls_server_t *, const char * ) );


/*****************************************************************************
 * tls_ServerAddCRL:
 *****************************************************************************
 * Adds a certificates revocation list to be sent to TLS clients.
 * Returns -1 on error, 0 on success.
 *****************************************************************************/
VLC_EXPORT( int, tls_ServerAddCRL, ( tls_server_t *, const char * ) );


VLC_EXPORT( void, tls_ServerDelete, ( tls_server_t * ) );


tls_session_t *
tls_ServerSessionPrepare( const tls_server_t *p_server );

tls_session_t *
tls_SessionHandshake( tls_session_t *p_session, int fd );

void
tls_SessionClose( tls_session_t *p_session );

int
tls_Send( tls_session_t *p_session, const char *buf, int i_length );

int
tls_Recv( tls_session_t *p_session, char *buf, int i_length );
