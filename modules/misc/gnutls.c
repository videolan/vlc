/*****************************************************************************
 * tls.c
 *****************************************************************************
 * Copyright (C) 2004-2005 VideoLAN
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

#define DH_BITS             1024
#define CACHE_EXPIRATION    3600
#define CACHE_SIZE          64

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define DH_BITS_TEXT N_("Diffie-Hellman prime bits")
#define DH_BITS_LONGTEXT N_( \
    "Allows you to modify the Diffie-Hellman prime's number of bits " \
    "(used for TLS or SSL-based server-side encryption)." )

#define CACHE_EXPIRATION_TEXT N_("Expiration time for resumed TLS sessions")
#define CACHE_EXPIRATION_LONGTEXT N_( \
    "Defines the delay before resumed TLS sessions will be expired " \
    "(in seconds)." )

#define CACHE_SIZE_TEXT N_("Number of resumed TLS sessions")
#define CACHE_SIZE_LONGTEXT N_( \
    "Allows you to modify the maximum number of resumed TLS sessions that " \
    "the cache will hold." )


vlc_module_begin();
    set_description( _("GnuTLS TLS encryption layer") );
    set_capability( "tls", 1 );
    set_callbacks( Open, Close );
    set_category( CAT_ADVANCED );
    set_subcategory( SUBCAT_ADVANCED_MISC );

    add_integer( "dh-bits", DH_BITS, NULL, DH_BITS_TEXT,
                 DH_BITS_LONGTEXT, VLC_TRUE );
    add_integer( "tls-cache-expiration", CACHE_EXPIRATION, NULL,
                 CACHE_EXPIRATION_TEXT, CACHE_EXPIRATION_LONGTEXT, VLC_TRUE );
    add_integer( "tls-cache-size", CACHE_SIZE, NULL, CACHE_SIZE_TEXT,
                 CACHE_SIZE_LONGTEXT, VLC_TRUE );
vlc_module_end();


#define MAX_SESSION_ID    32
#define MAX_SESSION_DATA  1024

typedef struct saved_session_t
{
    char id[MAX_SESSION_ID];
    char data[MAX_SESSION_DATA];

    unsigned i_idlen;
    unsigned i_datalen;
} saved_session_t;


typedef struct tls_server_sys_t
{
    gnutls_certificate_credentials  x509_cred;
    gnutls_dh_params                dh_params;

    struct saved_session_t          *p_cache;
    struct saved_session_t          *p_store;
    int                             i_cache_size;
    vlc_mutex_t                     cache_lock;
} tls_server_sys_t;


typedef struct tls_session_sys_t
{
    gnutls_session  session;
} tls_session_sys_t;


typedef struct tls_client_sys_t
{
    struct tls_session_sys_t       session;
    gnutls_certificate_credentials x509_cred;
} tls_client_sys_t;


static int
_get_Int (vlc_object_t *p_this, const char *var)
{
    vlc_value_t value;

    if( var_Get( p_this, var, &value ) != VLC_SUCCESS )
    {
        var_Create( p_this, var, VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Get( p_this, var, &value );
    }

    return value.i_int;
}

#define get_Int( a, b ) _get_Int( (vlc_object_t *)(a), (b) )


/*****************************************************************************
 * tls_Send:
 *****************************************************************************
 * Sends data through a TLS session.
 *****************************************************************************/
static int
gnutls_Send( void *p_session, const void *buf, int i_length )
{
    int val;
    tls_session_sys_t *p_sys;

    p_sys = (tls_session_sys_t *)(((tls_session_t *)p_session)->p_sys);

    val = gnutls_record_send( p_sys->session, buf, i_length );
    /* TODO: handle fatal error */
    return val < 0 ? -1 : val;
}


/*****************************************************************************
 * tls_Recv:
 *****************************************************************************
 * Receives data through a TLS session.
 *****************************************************************************/
static int
gnutls_Recv( void *p_session, void *buf, int i_length )
{
    int val;
    tls_session_sys_t *p_sys;

    p_sys = (tls_session_sys_t *)(((tls_session_t *)p_session)->p_sys);

    val = gnutls_record_recv( p_sys->session, buf, i_length );
    /* TODO: handle fatal error */
    return val < 0 ? -1 : val;
}


/*****************************************************************************
 * tls_Session(Continue)?Handshake:
 *****************************************************************************
 * Establishes TLS session with a peer through socket <fd>.
 * Returns -1 on error (you need not and must not call tls_SessionClose)
 * 0 on succesful handshake completion, 1 if more would-be blocking recv is
 * needed, 2 if more would-be blocking send is required.
 *****************************************************************************/
static int
gnutls_SessionContinueHandshake( tls_session_t *p_session)
{
    tls_session_sys_t *p_sys;
    int val;

    p_sys = (tls_session_sys_t *)(p_session->p_sys);

     /* TODO: handle fatal error */
    val = gnutls_handshake( p_sys->session );
    if( ( val == GNUTLS_E_AGAIN ) || ( val == GNUTLS_E_INTERRUPTED ) )
        return 1 + gnutls_record_get_direction( p_sys->session );

    if( val < 0 )
    {
        gnutls_deinit( p_sys->session );
        msg_Err( p_session->p_tls, "TLS handshake failed : %s",
                 gnutls_strerror( val ) );
        free( p_sys );
        free( p_session );
        return -1;
    }

    return 0;
}

static int
gnutls_SessionHandshake( tls_session_t *p_session, int fd )
{
    tls_session_sys_t *p_sys;

    p_sys = (tls_session_sys_t *)(p_session->p_sys);

    gnutls_transport_set_ptr (p_sys->session, (gnutls_transport_ptr)fd);

    return gnutls_SessionContinueHandshake( p_session );
}


/*****************************************************************************
 * tls_SessionClose:
 *****************************************************************************
 * Terminates TLS session and releases session data.
 *****************************************************************************/
static void
gnutls_SessionClose( tls_session_t *p_session )
{
    tls_session_sys_t *p_sys;

    p_sys = (tls_session_sys_t *)(p_session->p_sys);

    /* On the client-side, credentials are re-allocated per session */
    if( p_session->p_server == NULL )
        gnutls_certificate_free_credentials( ((tls_client_sys_t *)p_sys)
                                                ->x509_cred );

    gnutls_bye( p_sys->session, GNUTLS_SHUT_WR );
    gnutls_deinit( p_sys->session );
    free( p_sys );
    free( p_session );
}


/*****************************************************************************
 * tls_ClientCreate:
 *****************************************************************************
 * Initializes client-side TLS session data.
 *****************************************************************************/
static tls_session_t *
gnutls_ClientCreate( tls_t *p_tls, const char *psz_ca_path )
{
    tls_session_t *p_session;
    tls_client_sys_t *p_sys;
    int i_val;
    const int cert_type_priority[3] =
    {
        GNUTLS_CRT_X509,
        0
    };

    p_sys = (tls_client_sys_t *)malloc( sizeof(struct tls_client_sys_t) );
    if( p_sys == NULL )
        return NULL;

    i_val = gnutls_certificate_allocate_credentials( &p_sys->x509_cred );
    if( i_val != 0 )
    {
        msg_Err( p_tls, "Cannot allocate X509 credentials : %s",
                 gnutls_strerror( i_val ) );
        free( p_sys );
        return NULL;
    }

    if( psz_ca_path != NULL )
    {
        i_val = gnutls_certificate_set_x509_trust_file( p_sys->x509_cred,
                                                        psz_ca_path,
                                                        GNUTLS_X509_FMT_PEM );
        if( i_val != 0 )
        {
            msg_Err( p_tls, "Cannot add trusted CA (%s) : %s", psz_ca_path,
                     gnutls_strerror( i_val ) );
            gnutls_certificate_free_credentials( p_sys->x509_cred );
            free( p_sys );
            return NULL;
        }
    }

    i_val = gnutls_init( &p_sys->session.session, GNUTLS_CLIENT );
    if( i_val != 0 )
    {
        msg_Err( p_tls, "Cannot initialize TLS session : %s",
                 gnutls_strerror( i_val ) );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        free( p_sys );
        return NULL;
    }

    i_val = gnutls_set_default_priority( p_sys->session.session );
    if( i_val < 0 )
    {
        msg_Err( p_tls, "Cannot set ciphers priorities : %s",
                 gnutls_strerror( i_val ) );
        gnutls_deinit( p_sys->session.session );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        free( p_sys );
        return NULL;
    }

    i_val = gnutls_certificate_type_set_priority( p_sys->session.session,
                                                  cert_type_priority );
    if( i_val < 0 )
    {
        msg_Err( p_tls, "Cannot set certificate type priorities : %s",
                 gnutls_strerror( i_val ) );
        gnutls_deinit( p_sys->session.session );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        free( p_sys );
        return NULL;
    }

    i_val = gnutls_credentials_set( p_sys->session.session,
                                    GNUTLS_CRD_CERTIFICATE,
                                    p_sys->x509_cred );
    if( i_val < 0 )
    {
        msg_Err( p_tls, "Cannot set TLS session credentials : %s",
                 gnutls_strerror( i_val ) );
        gnutls_deinit( p_sys->session.session );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        free( p_sys );
        return NULL;
    }

    p_session = malloc( sizeof (struct tls_session_t) );
    if( p_session == NULL )
    {
        gnutls_deinit( p_sys->session.session );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        free( p_sys );
        return NULL;
    }

    p_session->p_tls = p_tls;
    p_session->p_server = NULL;
    p_session->p_sys = p_sys;
    p_session->sock.p_sys = p_session;
    p_session->sock.pf_send = gnutls_Send;
    p_session->sock.pf_recv = gnutls_Recv;
    p_session->pf_handshake = gnutls_SessionHandshake;
    p_session->pf_handshake2 = gnutls_SessionContinueHandshake;
    p_session->pf_close = gnutls_SessionClose;

    return p_session;
}


/*****************************************************************************
 * TLS session resumption callbacks
 *****************************************************************************/
static int cb_store( void *p_server, gnutls_datum key, gnutls_datum data )
{
    tls_server_sys_t *p_sys = ((tls_server_t *)p_server)->p_sys;

    if( ( p_sys->i_cache_size == 0 )
     || ( key.size > MAX_SESSION_ID )
     || ( data.size > MAX_SESSION_DATA ) )
        return -1;

    vlc_mutex_lock( &p_sys->cache_lock );

    memcpy( p_sys->p_store->id, key.data, key.size);
    memcpy( p_sys->p_store->data, data.data, data.size );
    p_sys->p_store->i_idlen = key.size;
    p_sys->p_store->i_datalen = data.size;

    p_sys->p_store++;
    if( ( p_sys->p_store - p_sys->p_cache ) == p_sys->i_cache_size )
        p_sys->p_store = p_sys->p_cache;

    vlc_mutex_unlock( &p_sys->cache_lock );

    return 0;
}


static const gnutls_datum err_datum = { NULL, 0 };

static gnutls_datum cb_fetch( void *p_server, gnutls_datum key )
{
    tls_server_sys_t *p_sys = ((tls_server_t *)p_server)->p_sys;
    saved_session_t *p_session, *p_end;

    p_session = p_sys->p_cache;
    p_end = p_session + p_sys->i_cache_size;

    vlc_mutex_lock( &p_sys->cache_lock );

    while( p_session < p_end )
    {
        if( ( p_session->i_idlen == key.size )
         && !memcmp( p_session->id, key.data, key.size ) )
        {
            gnutls_datum data;

            data.size = p_session->i_datalen;

            data.data = gnutls_malloc( data.size );
            if( data.data == NULL )
            {
                vlc_mutex_unlock( &p_sys->cache_lock );
                return err_datum;
            }

            memcpy( data.data, p_session->data, data.size );
            vlc_mutex_unlock( &p_sys->cache_lock );
            return data;
        }
        p_session++;
    }

    vlc_mutex_unlock( &p_sys->cache_lock );

    return err_datum;
}


static int cb_delete( void *p_server, gnutls_datum key )
{
    tls_server_sys_t *p_sys = ((tls_server_t *)p_server)->p_sys;
    saved_session_t *p_session, *p_end;

    p_session = p_sys->p_cache;
    p_end = p_session + p_sys->i_cache_size;

    vlc_mutex_lock( &p_sys->cache_lock );

    while( p_session < p_end )
    {
        if( ( p_session->i_idlen == key.size )
         && !memcmp( p_session->id, key.data, key.size ) )
        {
            p_session->i_datalen = p_session->i_idlen = 0;
            vlc_mutex_unlock( &p_sys->cache_lock );
            return 0;
        }
        p_session++;
    }

    vlc_mutex_unlock( &p_sys->cache_lock );

    return -1;
}


/*****************************************************************************
 * tls_ServerSessionPrepare:
 *****************************************************************************
 * Initializes server-side TLS session data.
 *****************************************************************************/
static tls_session_t *
gnutls_ServerSessionPrepare( tls_server_t *p_server )
{
    tls_session_t *p_session;
    gnutls_session session;
    int i_val;

    i_val = gnutls_init( &session, GNUTLS_SERVER );
    if( i_val != 0 )
    {
        msg_Err( p_server->p_tls, "Cannot initialize TLS session : %s",
                 gnutls_strerror( i_val ) );
        return NULL;
    }
   
    i_val = gnutls_set_default_priority( session );
    if( i_val < 0 )
    {
        msg_Err( p_server->p_tls, "Cannot set ciphers priorities : %s",
                 gnutls_strerror( i_val ) );
        gnutls_deinit( session );
        return NULL;
    }

    i_val = gnutls_credentials_set( session, GNUTLS_CRD_CERTIFICATE,
                                    ((tls_server_sys_t *)(p_server->p_sys))
                                    ->x509_cred );
    if( i_val < 0 )
    {
        msg_Err( p_server->p_tls, "Cannot set TLS session credentials : %s",
                 gnutls_strerror( i_val ) );
        gnutls_deinit( session );
        return NULL;
    }

    /* TODO: support for client authentication */
    /*gnutls_certificate_server_set_request( p_session->session,
                                           GNUTLS_CERT_REQUEST ); */

    gnutls_dh_set_prime_bits( session, get_Int( p_server->p_tls, "dh-bits" ) );

    /* Session resumption support */
    gnutls_db_set_cache_expiration( session, get_Int( p_server->p_tls,
                                    "tls-cache-expiration" ) );
    gnutls_db_set_retrieve_function( session, cb_fetch );
    gnutls_db_set_remove_function( session, cb_delete );
    gnutls_db_set_store_function( session, cb_store );
    gnutls_db_set_ptr( session, p_server );

    p_session = malloc( sizeof (struct tls_session_t) );
    if( p_session == NULL )
    {
        gnutls_deinit( session );
        return NULL;
    }

    p_session->p_sys = (tls_session_sys_t *)malloc( sizeof(struct tls_session_sys_t) );
    if( p_session->p_sys == NULL )
    {
        gnutls_deinit( session );
        free( p_session );
        return NULL;
    }

    ((tls_session_sys_t *)p_session->p_sys)->session = session;

    p_session->p_tls = p_server->p_tls;
    p_session->p_server = p_server;
    p_session->sock.p_sys = p_session;
    p_session->sock.pf_send = gnutls_Send;
    p_session->sock.pf_recv = gnutls_Recv;
    p_session->pf_handshake = gnutls_SessionHandshake;
    p_session->pf_handshake2 = gnutls_SessionContinueHandshake;
    p_session->pf_close = gnutls_SessionClose;

    return p_session;
}


/*****************************************************************************
 * tls_ServerDelete:
 *****************************************************************************
 * Releases data allocated with tls_ServerCreate.
 *****************************************************************************/
static void
gnutls_ServerDelete( tls_server_t *p_server )
{
    tls_server_sys_t *p_sys;

    p_sys = (tls_server_sys_t *)p_server->p_sys;

    gnutls_certificate_free_credentials( p_sys->x509_cred );
    free( p_sys->p_cache );
    vlc_mutex_destroy( &p_sys->cache_lock );
    free( p_sys );
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

    p_server_sys->i_cache_size = get_Int( p_this, "tls-cache-size" );
    p_server_sys->p_cache = (struct saved_session_t *)
                            calloc( p_server_sys->i_cache_size,
                                    sizeof( struct saved_session_t ) );
    if( p_server_sys->p_cache == NULL )
    {
        free( p_server_sys );
        return NULL;
    }
    p_server_sys->p_store = p_server_sys->p_cache;
    /* FIXME: check for errors */
    vlc_mutex_init( p_this, &p_server_sys->cache_lock );

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

    /* FIXME:
     * - regenerate these regularly
     * - support other ciper suites
     */
    val = gnutls_dh_params_init( &p_server_sys->dh_params );
    if( val >= 0 )
    {
        msg_Dbg( p_this, "Computing Diffie Hellman ciphers parameters" );
        val = gnutls_dh_params_generate2( p_server_sys->dh_params,
                                          get_Int( p_this, "dh-bits" ) );
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


/*****************************************************************************
 * gcrypt thread option VLC implementation:
 *****************************************************************************/
vlc_object_t *__p_gcry_data;

static int gcry_vlc_mutex_init( void **p_sys )
{
    int i_val;
    vlc_mutex_t *p_lock = (vlc_mutex_t *)malloc( sizeof( vlc_mutex_t ) );

    if( p_lock == NULL)
        return ENOMEM;

    i_val = vlc_mutex_init( __p_gcry_data, p_lock );
    if( i_val )
        free( p_lock );
    else
        *p_sys = p_lock;
    return i_val;
}

static int gcry_vlc_mutex_destroy( void **p_sys )
{
    int i_val;
    vlc_mutex_t *p_lock = (vlc_mutex_t *)*p_sys;

    i_val = vlc_mutex_destroy( p_lock );
    free( p_lock );
    return i_val;
}

static int gcry_vlc_mutex_lock( void **p_sys )
{
    return vlc_mutex_lock( (vlc_mutex_t *)*p_sys );
}

static int gcry_vlc_mutex_unlock( void **lock )
{
    return vlc_mutex_unlock( (vlc_mutex_t *)*lock );
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


/*****************************************************************************
 * Module initialization
 *****************************************************************************/
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
    p_tls->pf_client_create = gnutls_ClientCreate;
    return VLC_SUCCESS;
}


/*****************************************************************************
 * Module deinitialization
 *****************************************************************************/
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
