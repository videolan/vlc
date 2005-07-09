/*****************************************************************************
 * tls.c
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Remi Denis-Courmont <rem # videolan.org>
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

#include <stdlib.h>
#include <vlc/vlc.h>

#include "vlc_tls.h"


/*****************************************************************************
 * tls_ServerCreate:
 *****************************************************************************
 * Allocates a whole server's TLS credentials.
 * Returns NULL on error.
 *****************************************************************************/
tls_server_t *
tls_ServerCreate( vlc_object_t *p_this, const char *psz_cert,
                  const char *psz_key )
{
    tls_t *p_tls;
    tls_server_t *p_server;

    p_tls = vlc_object_create( p_this, VLC_OBJECT_TLS );
    vlc_object_attach( p_tls, p_this );

    p_tls->p_module = module_Need( p_tls, "tls", 0, 0 );
    if( p_tls->p_module != NULL )
    {
        if( psz_key == NULL )
            psz_key = psz_cert;

        p_server = p_tls->pf_server_create( p_tls, psz_cert, psz_key );
        if( p_server != NULL )
        {
            msg_Dbg( p_tls, "TLS/SSL provider initialized" );
            return p_server;
        }
        else
            msg_Err( p_tls, "TLS/SSL provider error" );
        module_Unneed( p_tls, p_tls->p_module );
    }
    else
        msg_Err( p_tls, "TLS/SSL provider not found" );

    vlc_object_detach( p_tls );
    vlc_object_destroy( p_tls );
    return NULL;
}


/*****************************************************************************
 * tls_ServerDelete:
 *****************************************************************************
 * Releases data allocated with tls_ServerCreate.
 *****************************************************************************/
void
tls_ServerDelete( tls_server_t *p_server )
{
    tls_t *p_tls = (tls_t *)p_server->p_parent;

    p_server->pf_delete( p_server );

    module_Unneed( p_tls, p_tls->p_module );
    vlc_object_detach( p_tls );
    vlc_object_destroy( p_tls );
}


/*****************************************************************************
 * tls_ClientCreate:
 *****************************************************************************
 * Allocates a client's TLS credentials and shakes hands through the network.
 * Returns NULL on error. This is a blocking network operation.
 *****************************************************************************/
tls_session_t *
tls_ClientCreate( vlc_object_t *p_this, int fd, const char *psz_hostname )
{
    tls_t *p_tls;
    tls_session_t *p_session;

    p_tls = vlc_object_create( p_this, VLC_OBJECT_TLS );
    vlc_object_attach( p_tls, p_this );

    p_tls->p_module = module_Need( p_tls, "tls", 0, 0 );
    if( p_tls->p_module != NULL )
    {
        p_session = p_tls->pf_client_create( p_tls );
        if( p_session != NULL )
        {
            int i_val;

            for( i_val = tls_ClientSessionHandshake( p_session, fd,
                                                     psz_hostname );
                 i_val > 0;
                 i_val = tls_SessionContinueHandshake( p_session ) );
            
            if( i_val == 0 )
            {
                msg_Dbg( p_this, "TLS/SSL provider initialized" );
                return p_session;
            }
            msg_Err( p_this, "TLS/SSL session handshake error" );
        }
        else
            msg_Err( p_this, "TLS/SSL provider error" );
        module_Unneed( p_tls, p_tls->p_module );
    }
    else
        msg_Err( p_this, "TLS/SSL provider not found" );

    vlc_object_detach( p_tls );
    vlc_object_destroy( p_tls );
    return NULL;
}


/*****************************************************************************
 * tls_ClientDelete:
 *****************************************************************************
 * Releases data allocated with tls_ClientCreate.
 *****************************************************************************/
void
tls_ClientDelete( tls_session_t *p_session )
{
    tls_t *p_tls = (tls_t *)p_session->p_parent;

    p_session->pf_close( p_session );

    module_Unneed( p_tls, p_tls->p_module );
    vlc_object_detach( p_tls );
    vlc_object_destroy( p_tls );
}
