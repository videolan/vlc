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

/*
 * TODO:
 * - fix FIXMEs,
 * - client side stuff,
 * - server-side client cert validation,
 * - client-side server cert validation (?).
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <errno.h>
#include <vlc/vlc.h>

#include "vlc_tls.h"

#include <gcrypt.h>
#include <gnutls/gnutls.h>

#define DH_BITS 1024


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define DH_BITS_TEXT N_("Diffie-Hellman prime bits")
#define DH_BITS_LONGTEXT N_( \
    "Allows you to modify the Diffie-Hellman prime's number of bits " \
    "(used for TLS or SSL-based server-side encryption)." )

vlc_module_begin();
    set_description( _("GnuTLS TLS encryption layer") );
    set_capability( "tls", 1 );
    set_callbacks( Open, Close );

    add_integer( "dh-bits", DH_BITS, NULL, DH_BITS_TEXT,
                 DH_BITS_LONGTEXT, VLC_TRUE );
vlc_module_end();



typedef struct tls_server_sys_t
{
    gnutls_certificate_credentials x509_cred;
    gnutls_dh_params dh_params;
} tls_server_sys_t;


/*****************************************************************************
 * tls_Send:
 *****************************************************************************
 * Sends data through a TLS session.
 *****************************************************************************/
static int
gnutls_Send( tls_session_t *p_session, const char *buf, int i_length )
{
    int val;

    val = gnutls_record_send( *(gnutls_session *)(p_session->p_sys),
                              buf, i_length );
    return val < 0 ? -1 : val;
}


/*****************************************************************************
 * tls_Recv:
 *****************************************************************************
 * Receives data through a TLS session
 *****************************************************************************/
static int
gnutls_Recv( tls_session_t *p_session, char *buf, int i_length )
{
    int val;

    val = gnutls_record_recv( *(gnutls_session *)(p_session->p_sys),
                              buf, i_length );
    return val < 0 ? -1 : val;
}


/*****************************************************************************
 * tls_SessionHandshake:
 *****************************************************************************
 * Establishes TLS session with a peer through socket <fd>
 * Returns NULL on error (do NOT call tls_SessionClose in case of error or
 * re-use the session structure).
 *****************************************************************************/
static tls_session_t *
gnutls_SessionHandshake( tls_session_t *p_session, int fd )
{
    int val;
    gnutls_session *p_sys;

    p_sys = (gnutls_session *)(p_session->p_sys);

    gnutls_transport_set_ptr( *p_sys, (gnutls_transport_ptr)fd);
    val = gnutls_handshake( *p_sys );
    if( val < 0 )
    {
        gnutls_deinit( *p_sys );
        msg_Err( p_session->p_tls, "TLS handshake failed : %s",
                 gnutls_strerror( val ) );
        free( p_sys );
        free( p_session );
        return NULL;
    }
    return p_session;
}


/*****************************************************************************
 * tls_ServerCreate:
 *****************************************************************************
 * Terminates a TLS session and releases session data.
 *****************************************************************************/
static void
gnutls_SessionClose( tls_session_t *p_session )
{
    gnutls_session *p_sys;

    p_sys = (gnutls_session *)(p_session->p_sys);

    gnutls_bye( *p_sys, GNUTLS_SHUT_WR );
    gnutls_deinit ( *p_sys );
    free( p_sys );
    free( p_session );
}


/*****************************************************************************
 * tls_ServerSessionPrepare:
 *****************************************************************************
 * Initializes a server-side TLS session data
 *****************************************************************************/
static tls_session_t *
gnutls_ServerSessionPrepare( tls_server_t *p_server )
{
    tls_session_t *p_session;
    gnutls_session *p_sys;
    int val;
    vlc_value_t bits;

    p_sys = (gnutls_session *)malloc( sizeof(gnutls_session) );
    if( p_sys == NULL )
        return NULL;

    val = gnutls_init( p_sys, GNUTLS_SERVER );
    if( val != 0 )
    {
        msg_Err( p_server->p_tls, "Cannot initialize TLS session : %s",
                 gnutls_strerror( val ) );
        free( p_sys );
        return NULL;
    }
   
    val = gnutls_set_default_priority( *p_sys );
    if( val < 0 )
    {
        msg_Err( p_server->p_tls, "Cannot set ciphers priorities : %s",
                 gnutls_strerror( val ) );
        gnutls_deinit( *p_sys );
        free( p_sys );
        return NULL;
    }

    val = gnutls_credentials_set( *p_sys, GNUTLS_CRD_CERTIFICATE,
                                  ((tls_server_sys_t *)(p_server->p_sys))
                                  ->x509_cred );
    if( val < 0 )
    {
        msg_Err( p_server->p_tls, "Cannot set TLS session credentials : %s",
                 gnutls_strerror( val ) );
        gnutls_deinit( *p_sys );
        free( p_sys );
        return NULL;
    }

    /* TODO: support for client authentication */
    /*gnutls_certificate_server_set_request( p_session->session,
                                           GNUTLS_CERT_REQUEST ); */

    if( var_Get( p_server->p_tls, "dh-bits", &bits ) != VLC_SUCCESS )
    {
        var_Create( p_server->p_tls, "dh-bits",
                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Get( p_server->p_tls, "dh-bits", &bits );
    }

    gnutls_dh_set_prime_bits( *p_sys, bits.i_int );

    p_session = malloc( sizeof (struct tls_session_t) );
    if( p_session == NULL )
    {
        gnutls_deinit( *p_sys );
        free( p_sys );
        return NULL;
    }

    p_session->p_tls = p_server->p_tls;
    p_session->p_server = p_server;
    p_session->p_sys = p_sys;
    p_session->pf_handshake =gnutls_SessionHandshake;
    p_session->pf_close = gnutls_SessionClose;
    p_session->pf_send = gnutls_Send;
    p_session->pf_recv = gnutls_Recv;

    return p_session;
}


/*****************************************************************************
 * tls_ServerDelete:
 *****************************************************************************
 * Releases data allocated with tls_ServerCreate
 *****************************************************************************/
static void
gnutls_ServerDelete( tls_server_t *p_server )
{
    gnutls_certificate_free_credentials(
                                       ((tls_server_sys_t *)(p_server->p_sys))
                                         ->x509_cred );
    free( p_server->p_sys );
    free( p_server );
}


/*****************************************************************************
 * tls_ServerAddCA:
 *****************************************************************************
 * Adds one or more certificate authorities.
 * TODO: we are not able to check the client credentials yet, so this function
 * is pretty useless.
 * Returns -1 on error, 0 on success.
 *****************************************************************************/
static int
gnutls_ServerAddCA( tls_server_t *p_server, const char *psz_ca_path )
{
    int val;

    val = gnutls_certificate_set_x509_trust_file( ((tls_server_sys_t *)
                                                  (p_server->p_sys))
                                                    ->x509_cred,
                                                  psz_ca_path,
                                                  GNUTLS_X509_FMT_PEM );
    if( val < 0 )
    {
        msg_Err( p_server->p_tls, "Cannot add trusted CA (%s) : %s",
                 psz_ca_path, gnutls_strerror( val ) );
        gnutls_ServerDelete( p_server );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_server->p_tls, " %d trusted CA added (%s)", val,
             psz_ca_path );
    return VLC_SUCCESS;
}


/*****************************************************************************
 * tls_ServerAddCRL:
 *****************************************************************************
 * Adds a certificates revocation list to be sent to TLS clients.
 * Returns -1 on error, 0 on success.
 *****************************************************************************/
static int
gnutls_ServerAddCRL( tls_server_t *p_server, const char *psz_crl_path )
{
    int val;

    val = gnutls_certificate_set_x509_crl_file( ((tls_server_sys_t *)
                                                (p_server->p_sys))->x509_cred,
                                                psz_crl_path,
                                                GNUTLS_X509_FMT_PEM );
    if( val < 0 )
    {
        msg_Err( p_server->p_tls, "Cannot add CRL (%s) : %s",
                 psz_crl_path, gnutls_strerror( val ) );
        gnutls_ServerDelete( p_server );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_server->p_tls, "%d CRL added (%s)", val, psz_crl_path );
    return VLC_SUCCESS;
}
    

/*****************************************************************************
 * tls_ServerCreate:
 *****************************************************************************
 * Allocates a whole server's TLS credentials.
 * Returns NULL on error.
 *****************************************************************************/
static tls_server_t *
gnutls_ServerCreate( tls_t *p_this, const char *psz_cert_path,
                  const char *psz_key_path )
{
    tls_server_t *p_server;
    tls_server_sys_t *p_server_sys;
    int val;

    msg_Dbg( p_this, "Creating TLS server" );

    p_server_sys = (tls_server_sys_t *)malloc( sizeof(struct tls_server_sys_t) );
    if( p_server_sys == NULL )
        return NULL;

    /* Sets server's credentials */
    val = gnutls_certificate_allocate_credentials( &p_server_sys->x509_cred );
    if( val != 0 )
    {
        msg_Err( p_this, "Cannot allocate X509 credentials : %s",
                 gnutls_strerror( val ) );
        free( p_server_sys );
        return NULL;
    }

    val = gnutls_certificate_set_x509_key_file( p_server_sys->x509_cred,
                                                psz_cert_path, psz_key_path,
                                                GNUTLS_X509_FMT_PEM );
    if( val < 0 )
    {
        msg_Err( p_this, "Cannot set certificate chain or private key : %s",
                 gnutls_strerror( val ) );
        gnutls_certificate_free_credentials( p_server_sys->x509_cred );
        free( p_server_sys );
        return NULL;
    }

    /* FIXME: regenerate these regularly */
    val = gnutls_dh_params_init( &p_server_sys->dh_params );
    if( val >= 0 )
    {
        vlc_value_t bits;

        if( var_Get( p_this, "dh-bits", &bits ) != VLC_SUCCESS )
        {
            var_Create( p_this, "dh-bits",
                        VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
            var_Get( p_this, "dh-bits", &bits );
        }

        msg_Dbg( p_this, "Computing Diffie Hellman ciphers parameters" );
        val = gnutls_dh_params_generate2( p_server_sys->dh_params,
                                          bits.i_int );
    }
    if( val < 0 )
    {
        msg_Err( p_this, "Cannot initialize DH cipher suites : %s",
                 gnutls_strerror( val ) );
        gnutls_certificate_free_credentials( p_server_sys->x509_cred );
        free( p_server_sys );
        return NULL;
    }
    msg_Dbg( p_this, "Ciphers parameters computed" );

    gnutls_certificate_set_dh_params( p_server_sys->x509_cred,
                                      p_server_sys->dh_params);

    p_server = (tls_server_t *)malloc( sizeof(struct tls_server_t) );
    if( p_server == NULL )
    {
        free( p_server_sys );
        return NULL;
    }

    p_server->p_tls = p_this;
    p_server->p_sys = p_server_sys;
    p_server->pf_delete = gnutls_ServerDelete;
    p_server->pf_add_CA = gnutls_ServerAddCA;
    p_server->pf_add_CRL = gnutls_ServerAddCRL;
    p_server->pf_session_prepare = gnutls_ServerSessionPrepare;

    return p_server;
}


/*
 * gcrypt thread option VLC implementation
 */
vlc_object_t *__p_gcry_data;

static int gcry_vlc_mutex_init (void **p_sys)
{
    int i_val;
    vlc_mutex_t *p_lock = (vlc_mutex_t *)malloc (sizeof (vlc_mutex_t));

    if( p_lock == NULL)
        return ENOMEM;

    i_val = vlc_mutex_init( __p_gcry_data, p_lock);
    if (i_val)
        free (p_lock);
    else
        *p_sys = p_lock;
    return i_val;
}

static int gcry_vlc_mutex_destroy (void **p_sys)
{
    int i_val;
    vlc_mutex_t *p_lock = (vlc_mutex_t *)*p_sys;

    i_val = vlc_mutex_destroy (p_lock);
    free (p_lock);
    return i_val;
}

static int gcry_vlc_mutex_lock (void **p_sys)
{
    return vlc_mutex_lock ((vlc_mutex_t *)*p_sys);
}

static int gcry_vlc_mutex_unlock (void **lock)
{
    return vlc_mutex_unlock ((vlc_mutex_t *)*lock);
}

static struct gcry_thread_cbs gcry_threads_vlc =
{
    GCRY_THREAD_OPTION_USER,
    NULL,
    gcry_vlc_mutex_init,
    gcry_vlc_mutex_destroy,
    gcry_vlc_mutex_lock,
    gcry_vlc_mutex_unlock
};


static int
Open( vlc_object_t *p_this )
{
    tls_t *p_tls = (tls_t *)p_this;

    vlc_value_t lock, count;

    var_Create( p_this->p_libvlc, "tls_mutex", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "tls_mutex", &lock );
    vlc_mutex_lock( lock.p_address );

    /* Initialize GnuTLS only once */
    var_Create( p_this->p_libvlc, "gnutls_count", VLC_VAR_INTEGER );
    var_Get( p_this->p_libvlc, "gnutls_count", &count);

    if( count.i_int == 0)
    {
        __p_gcry_data = VLC_OBJECT( p_this->p_vlc );

        gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_vlc);
        if( gnutls_global_init( ) )
        {
            msg_Warn( p_this, "cannot initialize GNUTLS" );
            vlc_mutex_unlock( lock.p_address );
            return VLC_EGENERIC;
        }
        if( gnutls_check_version( "1.0.0" ) == NULL )
        {
            gnutls_global_deinit( );
            vlc_mutex_unlock( lock.p_address );
            msg_Err( p_this, "unsupported GNUTLS version" );
            return VLC_EGENERIC;
        }
        msg_Dbg( p_this, "GNUTLS initialized" );
    }

    count.i_int++;
    var_Set( p_this->p_libvlc, "gnutls_count", count);
    vlc_mutex_unlock( lock.p_address );

    p_tls->pf_server_create = gnutls_ServerCreate;
    return VLC_SUCCESS;
}


static void
Close( vlc_object_t *p_this )
{
    /*tls_t *p_tls = (tls_t *)p_this;
    tls_sys_t *p_sys = (tls_sys_t *)(p_this->p_sys);*/

    vlc_value_t lock, count;

    var_Create( p_this->p_libvlc, "gnutls_mutex", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "gnutls_mutex", &lock );
    vlc_mutex_lock( lock.p_address );

    var_Create( p_this->p_libvlc, "gnutls_count", VLC_VAR_INTEGER );
    var_Get( p_this->p_libvlc, "gnutls_count", &count);
    count.i_int--;
    var_Set( p_this->p_libvlc, "gnutls_count", count);

    if( count.i_int == 0 )
    {
        gnutls_global_deinit( );
        msg_Dbg( p_this, "GNUTLS deinitialized" );
    }

    vlc_mutex_unlock( lock.p_address);
}
