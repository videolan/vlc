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

#include <stdlib.h>
#include <vlc/vlc.h>
#include <assert.h>

#include "vlc_tls.h"


#define HAVE_GNUTLS 1
/*
 * TODO:
 * - libgcrypt thread-safety !!!
 * - fix FIXMEs,
 * - gnutls version check,
 * - client side stuff,
 * - server-side client cert validation,
 * - client-side server cert validation (?).
 */

/* FIXME: proper configure check */
//#define HAVE_GNUTLS 1

#ifdef HAVE_GNUTLS
#   include <gnutls/gnutls.h>

#   define DH_BITS 1024


struct tls_server_t
{
    gnutls_certificate_credentials x509_cred;
    gnutls_dh_params dh_params;
    vlc_object_t *p_this;
};

struct tls_session_t
{
    gnutls_session session;
    vlc_object_t *p_this;
};

/* FIXME: is this legal in the VLC? */
unsigned i_servernum = 0;


static int
tls_Init( vlc_object_t *p_this )
{
    vlc_value_t lock;

    var_Create( p_this->p_libvlc, "tls_mutex", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "tls_mutex", &lock );
    vlc_mutex_lock( lock.p_address );

    /* Initialize GnuTLS only once */
    /* FIXME: should check version number */
    if( i_servernum == 0)
    {
        if( gnutls_global_init( ) )
        {
            msg_Warn( p_this, "cannot initialize GNUTLS" );
            vlc_mutex_unlock( lock.p_address);
            return -1;
        }
        msg_Dbg( p_this, "GNUTLS initialized" );
    }

    i_servernum++;
    vlc_mutex_unlock( lock.p_address );

    return 0;
}


static void
tls_CleanUp( vlc_object_t *p_this )
{
    vlc_value_t lock;

    var_Create( p_this->p_libvlc, "tls_mutex", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "tls_mutex", &lock );
    vlc_mutex_lock( lock.p_address );

    i_servernum--;
    if( i_servernum == 0 )
    {
        gnutls_global_deinit( );
        msg_Dbg( p_this, "GNUTLS deinitialized" );
    }

    vlc_mutex_unlock( lock.p_address);
}

#endif


/*****************************************************************************
 * tls_ServerCreate:
 *****************************************************************************
 * Allocates a whole server's TLS credentials.
 * Returns NULL on error.
 *****************************************************************************/
tls_server_t *
tls_ServerCreate( vlc_object_t *p_this, const char *psz_cert_path,
                  const char *psz_key_path )
{
#if HAVE_GNUTLS
    tls_server_t *p_server;
    int val;

    msg_Dbg( p_this, "Creating TLS server" );
    if( tls_Init( p_this ) )
        return NULL;

    p_server = (tls_server_t *)malloc( sizeof(struct tls_server_t) );

    /* FIXME: do not hard-code PEM file paths */
    /* Sets server's credentials */
    val = gnutls_certificate_allocate_credentials( &p_server->x509_cred );
    if( val != 0 )
    {
        msg_Err( p_this, "Cannot allocate X509 credentials : %s",
                 gnutls_strerror( val ) );
        free( p_server );
        return NULL;
    }

    val = gnutls_certificate_set_x509_key_file( p_server->x509_cred,
                                                psz_cert_path, psz_key_path,
                                                GNUTLS_X509_FMT_PEM );
    if( val < 0 )
    {
        msg_Err( p_this, "Cannot set certificate chain or private key : %s",
                 gnutls_strerror( val ) );
        gnutls_certificate_free_credentials( p_server->x509_cred );
        free( p_server );
        return NULL;
    }

    /* FIXME: regenerate these regularly */
    val = gnutls_dh_params_init( &p_server->dh_params );
    if( val >= 0 )
    {
        msg_Dbg( p_this, "Computing Diffie Hellman ciphers parameters" );
        val = gnutls_dh_params_generate2( p_server->dh_params, DH_BITS );
    }
    if( val < 0 )
    {
        msg_Err( p_this, "Cannot initialize DH cipher suites : %s",
                 gnutls_strerror( val ) );
        gnutls_certificate_free_credentials( p_server->x509_cred );
        free( p_server );
        return NULL;
    }
    msg_Dbg( p_this, "Ciphers parameters computed" );

    gnutls_certificate_set_dh_params( p_server->x509_cred,
                                      p_server->dh_params);
   
    p_server->p_this = p_this;
    return p_server;
#else
    return NULL;
#endif
}


/*****************************************************************************
 * tls_ServerAddCA:
 *****************************************************************************
 * Adds one or more certificate authorities.
 * TODO: we are not able to check the client credentials yet, so this function
 * is pretty useless.
 * Returns -1 on error, 0 on success.
 *****************************************************************************/
int
tls_ServerAddCA( tls_server_t *p_server, const char *psz_ca_path )
{
#if HAVE_GNUTLS
    int val;

    assert( p_server != NULL);
    val = gnutls_certificate_set_x509_trust_file( p_server->x509_cred,
                                                  psz_ca_path,
                                                  GNUTLS_X509_FMT_PEM );
    if( val < 0 )
    {
        msg_Err( p_server->p_this, "Cannot add trusted CA (%s) : %s",
                 psz_ca_path, gnutls_strerror( val ) );
        free( p_server );
        return -1;
    }
    msg_Dbg( p_server->p_this, " %d trusted CA added (%s)", val,
             psz_ca_path );
    return 0;
#else
    return -1;
#endif
}


/*****************************************************************************
 * tls_ServerAddCRL:
 *****************************************************************************
 * Adds a certificates revocation list to be sent to TLS clients.
 * Returns -1 on error, 0 on success.
 *****************************************************************************/
int
tls_ServerAddCRL( tls_server_t *p_server, const char *psz_crl_path )
{
#if HAVE_GNUTLS
    int val;

    val = gnutls_certificate_set_x509_crl_file( p_server->x509_cred,
                                                psz_crl_path,
                                                GNUTLS_X509_FMT_PEM );
    if( val < 0 )
    {
        msg_Err( p_server->p_this, "Cannot add CRL (%s) : %s",
                 psz_crl_path, gnutls_strerror( val ) );
        free( p_server );
        return -1;
    }
    msg_Dbg( p_server->p_this, "%d CRL added (%s)", val, psz_crl_path );
    return 0;
#else
    return -1;
#endif
}
    


/*****************************************************************************
 * tls_ServerDelete:
 *****************************************************************************
 * Releases data allocated with tls_ServerCreate
 *****************************************************************************/
void
tls_ServerDelete( tls_server_t *p_server )
{
    assert( p_server != NULL );

#if HAVE_GNUTLS
    gnutls_certificate_free_credentials( p_server->x509_cred );
    tls_CleanUp( p_server->p_this );
    free( p_server );
#endif
}


/*****************************************************************************
 * tls_ServerSessionPrepare:
 *****************************************************************************
 * Initializes a server-side TLS session data
 *****************************************************************************/
tls_session_t *
tls_ServerSessionPrepare( const tls_server_t *p_server )
{
#if HAVE_GNUTLS
    tls_session_t *p_session;
    gnutls_session session;
    int val;

    assert( p_server != NULL );

    val = gnutls_init( &session, GNUTLS_SERVER );
    if( val != 0 )
    {
        msg_Err( p_server->p_this, "Cannot initialize TLS session : %s",
                 gnutls_strerror( val ) );
        return NULL;
    }
   
    val = gnutls_set_default_priority( session );
    if( val < 0 )
    {
        msg_Err( p_server->p_this, "Cannot set ciphers priorities : %s",
                 gnutls_strerror( val ) );
        gnutls_deinit( session );
        return NULL;
    }

    val = gnutls_credentials_set( session, GNUTLS_CRD_CERTIFICATE,
                                  p_server->x509_cred );
    if( val < 0 )
    {
        msg_Err( p_server->p_this, "Cannot set TLS session credentials : %s",
                 gnutls_strerror( val ) );
        gnutls_deinit( session );
        return NULL;
    }

    /* TODO: support for client authentication */
    /*gnutls_certificate_server_set_request( p_session->session,
                                           GNUTLS_CERT_REQUEST ); */

    gnutls_dh_set_prime_bits( session, DH_BITS );

    p_session = malloc( sizeof (struct tls_session_t) );
    p_session->session = session;
    p_session->p_this = p_server->p_this;

    return p_session;
#else
    return NULL;
#endif
}


/*****************************************************************************
 * tls_SessionHandshake:
 *****************************************************************************
 * Establishes TLS session with a peer through socket <fd>
 * Returns NULL on error (do NOT call tls_SessionClose in case of error or
 * re-use the session structure).
 *****************************************************************************/
tls_session_t *
tls_SessionHandshake( tls_session_t *p_session, int fd )
{
#if HAVE_GNUTLS
    int val;

    assert( p_session != NULL );

    gnutls_transport_set_ptr( p_session->session, (gnutls_transport_ptr)fd);
    val = gnutls_handshake( p_session->session);
    if( val < 0 )
    {
        gnutls_deinit( p_session->session );
        msg_Err( p_session->p_this, "TLS handshake failed : %s",
                 gnutls_strerror( val ) );
        free( p_session );
        return NULL;
    }
    return p_session;
#else
    return NULL;
#endif
}


/*****************************************************************************
 * tls_ServerCreate:
 *****************************************************************************
 * Terminates a TLS session and releases session data.
 *****************************************************************************/
void
tls_SessionClose( tls_session_t *p_session )
{
    assert( p_session != NULL );

#if HAVE_GNUTLS
    gnutls_bye( p_session->session, GNUTLS_SHUT_WR );
    gnutls_deinit (p_session->session );
    free( p_session );
#endif
}


/*****************************************************************************
 * tls_Send:
 *****************************************************************************
 * Sends data through a TLS session.
 *****************************************************************************/
int
tls_Send( tls_session_t *p_session, const char *buf, int i_length )
{
#if HAVE_GNUTLS
    int val;

    assert( p_session != NULL );

    val = gnutls_record_send( p_session->session, buf, i_length );
    if( val < 0 )
    {
        /*msg_Warn( p_session->p_this, "TLS problem : %s",
                  gnutls_strerror( val ) );*/
        return -1;
    }
    return val;
#else
    return -1;
#endif
}


/*****************************************************************************
 * tls_Recv:
 *****************************************************************************
 * Receives data through a TLS session
 *****************************************************************************/
int
tls_Recv( tls_session_t *p_session, char *buf, int i_length )
{
#if HAVE_GNUTLS
    int val;

    assert( p_session != NULL );

    val = gnutls_record_recv( p_session->session, buf, i_length );
    if( val < 0 )
    {
        /*msg_Warn( p_session->p_this, "TLS problem : %s",
                  gnutls_strerror( val ) );*/
        return -1;
    }
    return val;
#else
    return -1;
#endif
}
