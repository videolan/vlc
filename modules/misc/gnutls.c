/*****************************************************************************
 * tls.c
 *****************************************************************************
 * Copyright (C) 2004-2005 VideoLAN
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

/*
 * TODO:
 * - fix FIXMEs
 */


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <errno.h>
#include <vlc/vlc.h>

#include <sys/types.h>
#include <errno.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
#endif


#include "vlc_tls.h"

#include <gcrypt.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

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

#define CHECK_CERT_TEXT N_("Check TLS/SSL server certificate validity")
#define CHECK_CERT_LONGTEXT N_( \
    "Ensures that server certificate is valid " \
    "(ie. signed by an approved Certificate Authority)." )

#define CHECK_HOSTNAME_TEXT N_("Check TLS/SSL server hostname in certificate")
#define CHECK_HOSTNAME_LONGTEXT N_( \
    "Ensures that server hostname in certificate match requested host name." )

vlc_module_begin();
    set_shortname( "GnuTLS" );
    set_description( _("GnuTLS TLS encryption layer") );
    set_capability( "tls", 1 );
    set_callbacks( Open, Close );
    set_category( CAT_ADVANCED );
    set_subcategory( SUBCAT_ADVANCED_MISC );

    add_bool( "tls-check-cert", VLC_FALSE, NULL, CHECK_CERT_TEXT,
              CHECK_CERT_LONGTEXT, VLC_FALSE );
    add_bool( "tls-check-hostname", VLC_FALSE, NULL, CHECK_HOSTNAME_TEXT,
              CHECK_HOSTNAME_LONGTEXT, VLC_FALSE );

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

    int                             (*pf_handshake2)( tls_session_t * );
} tls_server_sys_t;


typedef struct tls_session_sys_t
{
    gnutls_session  session;
    char            *psz_hostname;
    vlc_bool_t      b_handshaked;
} tls_session_sys_t;


typedef struct tls_client_sys_t
{
    struct tls_session_sys_t       session;
    gnutls_certificate_credentials x509_cred;
} tls_client_sys_t;


static int
_get_Int( vlc_object_t *p_this, const char *var )
{
    vlc_value_t value;

    if( var_Get( p_this, var, &value ) != VLC_SUCCESS )
    {
        var_Create( p_this, var, VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Get( p_this, var, &value );
    }

    return value.i_int;
}

static int
_get_Bool( vlc_object_t *p_this, const char *var )
{
    vlc_value_t value;

    if( var_Get( p_this, var, &value ) != VLC_SUCCESS )
    {
        var_Create( p_this, var, VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Get( p_this, var, &value );
    }

    return value.b_bool;
}

#define get_Int( a, b ) _get_Int( (vlc_object_t *)(a), (b) )
#define get_Bool( a, b ) _get_Bool( (vlc_object_t *)(a), (b) )


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
gnutls_ContinueHandshake( tls_session_t *p_session)
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
        msg_Err( p_session, "TLS handshake failed : %s",
                 gnutls_strerror( val ) );
        p_session->pf_close( p_session );
        return -1;
    }

    p_sys->b_handshaked = VLC_TRUE;
    return 0;
}

static int
gnutls_HandshakeAndValidate( tls_session_t *p_session )
{
    int val;

    val = gnutls_ContinueHandshake( p_session );
    if( val == 0 )
    {
        int status;
        gnutls_x509_crt cert;
        const gnutls_datum *p_data;
        tls_session_sys_t *p_sys;

        p_sys = (tls_session_sys_t *)(p_session->p_sys);
        /* certificates chain verification */
        val = gnutls_certificate_verify_peers2( p_sys->session, &status );

        if( val )
        {
            msg_Err( p_session, "TLS certificate verification failed : %s",
                     gnutls_strerror( val ) );
            p_session->pf_close( p_session );
            return -1;
        }

        if( status )
        {
            msg_Warn( p_session, "TLS session : access denied" );
            if( status & GNUTLS_CERT_INVALID )
                msg_Dbg( p_session, "certificate could not be verified" );
            if( status & GNUTLS_CERT_REVOKED )
                msg_Dbg( p_session, "certificate was revoked" );
            if( status & GNUTLS_CERT_SIGNER_NOT_FOUND )
                msg_Dbg( p_session, "certificate's signer was not found" );
            if( status & GNUTLS_CERT_SIGNER_NOT_CA )
                msg_Dbg( p_session, "certificate's signer is not a CA" );
            p_session->pf_close( p_session );
            return -1;
        }

        msg_Dbg( p_session, "TLS certificate verified" );
        if( p_sys->psz_hostname == NULL )
            return 0;

        /* certificate (host)name verification */
        p_data = gnutls_certificate_get_peers( p_sys->session, &val );
        if( p_data == NULL )
        {
            msg_Err( p_session, "TLS peer certificate not available" );
            p_session->pf_close( p_session );
            return -1;
        }

        val = gnutls_x509_crt_init( &cert );
        if( val )
        {
            msg_Err( p_session, "x509 fatal error : %s",
                     gnutls_strerror( val ) );
            p_session->pf_close( p_session );
            return -1;
        }

        val = gnutls_x509_crt_import( cert, p_data, GNUTLS_X509_FMT_DER );
        if( val )
        {
            msg_Err( p_session, "x509 certificate import error : %s",
                     gnutls_strerror( val ) );
            gnutls_x509_crt_deinit( cert );
            p_session->pf_close( p_session );
            return -1;
        }

        if( gnutls_x509_crt_check_hostname( cert, p_sys->psz_hostname ) == 0 )
        {
            msg_Err( p_session, "x509 certificate does not match \"%s\"",
                     p_sys->psz_hostname );
            gnutls_x509_crt_deinit( cert );
            p_session->pf_close( p_session );
            return -1;
        }

        gnutls_x509_crt_deinit( cert );
        
        msg_Dbg( p_session, "x509 hostname verified" );
        return 0;
    }

    return val;
}

static int
gnutls_BeginHandshake( tls_session_t *p_session, int fd,
                         const char *psz_hostname )
{
    tls_session_sys_t *p_sys;

    p_sys = (tls_session_sys_t *)(p_session->p_sys);

    gnutls_transport_set_ptr (p_sys->session, (gnutls_transport_ptr)fd);

    p_sys->psz_hostname = NULL;
    if( psz_hostname != NULL )
    {
        gnutls_server_name_set( p_sys->session, GNUTLS_NAME_DNS, psz_hostname,
                                strlen( psz_hostname ) );
        if( get_Bool( p_session, "tls-check-hostname" ) )
        {
            p_sys->psz_hostname = strdup( psz_hostname );
            if( p_sys->psz_hostname == NULL )
            {
                p_session->pf_close( p_session );
                return -1;
            }
        }
    }

    return p_session->pf_handshake2( p_session );
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

    if( p_sys->b_handshaked == VLC_TRUE )
        gnutls_bye( p_sys->session, GNUTLS_SHUT_WR );
    gnutls_deinit( p_sys->session );

    if( p_sys->psz_hostname != NULL )
        free( p_sys->psz_hostname );

    vlc_object_detach( p_session );
    vlc_object_destroy( p_session );

    free( p_sys );
}

static void
gnutls_ClientDelete( tls_session_t *p_session )
{
    /* On the client-side, credentials are re-allocated per session */
    gnutls_certificate_credentials x509_cred =
                        ((tls_client_sys_t *)(p_session->p_sys))->x509_cred;

    gnutls_SessionClose( p_session );

    /* credentials must be free'd *after* gnutls_deinit() */
    gnutls_certificate_free_credentials( x509_cred );
}


inline int
is_regular( const char *psz_filename )
{
#ifdef HAVE_SYS_STAT_H
    struct stat st;

    return ( stat( psz_filename, &st ) == 0 )
        && S_ISREG( st.st_mode );
#else
    return 1;
#endif
}


static int
gnutls_AddCADirectory( vlc_object_t *p_this,
                       gnutls_certificate_credentials cred,
                       const char *psz_dirname )
{
    DIR* dir;
    struct dirent *p_ent;
    int i_len;

    if( *psz_dirname == '\0' )
        psz_dirname = ".";

    dir = opendir( psz_dirname );
    if( dir == NULL )
    {
        msg_Warn( p_this, "Cannot open directory (%s) : %s", psz_dirname,
                  strerror( errno ) );
        return VLC_EGENERIC;
    }

    i_len = strlen( psz_dirname ) + 2;

    while( ( p_ent = readdir( dir ) ) != NULL )
    {
        char *psz_filename;

        psz_filename = (char *)malloc( i_len + strlen( p_ent->d_name ) );
        if( psz_filename == NULL )
            return VLC_ENOMEM;

        sprintf( psz_filename, "%s/%s", psz_dirname, p_ent->d_name );
        /* we neglect the race condition here - not security sensitive */
        if( is_regular( psz_filename ) )
        {
            int i;

            i = gnutls_certificate_set_x509_trust_file( cred, psz_filename,
                                                        GNUTLS_X509_FMT_PEM );
            if( i < 0 )
            {
                msg_Warn( p_this, "Cannot add trusted CA (%s) : %s",
                          psz_filename, gnutls_strerror( i ) );
            }
        }
        free( psz_filename );
    }

    closedir( dir );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * tls_ClientCreate:
 *****************************************************************************
 * Initializes client-side TLS session data.
 *****************************************************************************/
static tls_session_t *
gnutls_ClientCreate( tls_t *p_tls )
{
    tls_session_t *p_session = NULL;
    tls_client_sys_t *p_sys = NULL;
    int i_val;
    const int cert_type_priority[3] =
    {
        GNUTLS_CRT_X509,
        0
    };

    p_sys = (tls_client_sys_t *)malloc( sizeof(struct tls_client_sys_t) );
    if( p_sys == NULL )
        return NULL;
   
    p_session = (struct tls_session_t *)vlc_object_create ( p_tls, sizeof(struct tls_session_t) );
    if( p_session == NULL )
    {
        free( p_sys );
        return NULL;
    }

    p_session->p_sys = p_sys;
    p_session->sock.p_sys = p_session;
    p_session->sock.pf_send = gnutls_Send;
    p_session->sock.pf_recv = gnutls_Recv;
    p_session->pf_handshake = gnutls_BeginHandshake;
    p_session->pf_close = gnutls_ClientDelete;

    p_sys->session.b_handshaked = VLC_FALSE;

    vlc_object_attach( p_session, p_tls );

    i_val = gnutls_certificate_allocate_credentials( &p_sys->x509_cred );
    if( i_val != 0 )
    {
        msg_Err( p_tls, "Cannot allocate X509 credentials : %s",
                 gnutls_strerror( i_val ) );
        goto error;
    }

    if( get_Bool( p_tls, "tls-check-cert" ) )
    {
        /* FIXME: support for changing path/using multiple paths */
        char *psz_path;
        const char *psz_homedir;

        psz_homedir = p_tls->p_vlc->psz_homedir;
        psz_path = (char *)malloc( strlen( psz_homedir )
                                   + sizeof( CONFIG_DIR ) + 12 );
        if( psz_path == NULL )
        {
            gnutls_certificate_free_credentials( p_sys->x509_cred );
            goto error;
        }

        sprintf( psz_path, "%s/"CONFIG_DIR"/ssl/certs", psz_homedir );
        gnutls_AddCADirectory( (vlc_object_t *)p_session, p_sys->x509_cred,
                               psz_path );

        free( psz_path );
        p_session->pf_handshake2 = gnutls_HandshakeAndValidate;
    }
    else
        p_session->pf_handshake2 = gnutls_ContinueHandshake;

    i_val = gnutls_init( &p_sys->session.session, GNUTLS_CLIENT );
    if( i_val != 0 )
    {
        msg_Err( p_tls, "Cannot initialize TLS session : %s",
                 gnutls_strerror( i_val ) );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        goto error;
    }

    i_val = gnutls_set_default_priority( p_sys->session.session );
    if( i_val < 0 )
    {
        msg_Err( p_tls, "Cannot set ciphers priorities : %s",
                 gnutls_strerror( i_val ) );
        gnutls_deinit( p_sys->session.session );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        goto error;
    }

    i_val = gnutls_certificate_type_set_priority( p_sys->session.session,
                                                  cert_type_priority );
    if( i_val < 0 )
    {
        msg_Err( p_tls, "Cannot set certificate type priorities : %s",
                 gnutls_strerror( i_val ) );
        gnutls_deinit( p_sys->session.session );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        goto error;
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
        goto error;
    }

    return p_session;

error:
    vlc_object_detach( p_session );
    vlc_object_destroy( p_session );
    free( p_sys );

    return NULL;
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
    tls_server_sys_t *p_server_sys;
    gnutls_session session;
    int i_val;

    p_session = vlc_object_create( p_server, sizeof (struct tls_session_t) );
    if( p_session == NULL )
        return NULL;
    
    p_session->p_sys = malloc( sizeof(struct tls_session_sys_t) );
    if( p_session->p_sys == NULL )
    {
        vlc_object_destroy( p_session );
        return NULL;
    }

    vlc_object_attach( p_session, p_server );

    p_server_sys = (tls_server_sys_t *)p_server->p_sys;
    p_session->sock.p_sys = p_session;
    p_session->sock.pf_send = gnutls_Send;
    p_session->sock.pf_recv = gnutls_Recv;
    p_session->pf_handshake = gnutls_BeginHandshake;
    p_session->pf_handshake2 = p_server_sys->pf_handshake2;
    p_session->pf_close = gnutls_SessionClose;

    ((tls_session_sys_t *)p_session->p_sys)->b_handshaked = VLC_FALSE;

    i_val = gnutls_init( &session, GNUTLS_SERVER );
    if( i_val != 0 )
    {
        msg_Err( p_server, "Cannot initialize TLS session : %s",
                 gnutls_strerror( i_val ) );
        goto error;
    }

    ((tls_session_sys_t *)p_session->p_sys)->session = session;

    i_val = gnutls_set_default_priority( session );
    if( i_val < 0 )
    {
        msg_Err( p_server, "Cannot set ciphers priorities : %s",
                 gnutls_strerror( i_val ) );
        gnutls_deinit( session );
        goto error;
    }

    i_val = gnutls_credentials_set( session, GNUTLS_CRD_CERTIFICATE,
                                    p_server_sys->x509_cred );
    if( i_val < 0 )
    {
        msg_Err( p_server, "Cannot set TLS session credentials : %s",
                 gnutls_strerror( i_val ) );
        gnutls_deinit( session );
        goto error;
    }

    if( p_session->pf_handshake2 == gnutls_HandshakeAndValidate )
        gnutls_certificate_server_set_request( session, GNUTLS_CERT_REQUIRE );

    gnutls_dh_set_prime_bits( session, get_Int( p_server, "dh-bits" ) );

    /* Session resumption support */
    gnutls_db_set_cache_expiration( session, get_Int( p_server,
                                    "tls-cache-expiration" ) );
    gnutls_db_set_retrieve_function( session, cb_fetch );
    gnutls_db_set_remove_function( session, cb_delete );
    gnutls_db_set_store_function( session, cb_store );
    gnutls_db_set_ptr( session, p_server );

    return p_session;

error:
    free( p_session->p_sys );
    vlc_object_detach( p_session );
    vlc_object_destroy( p_session );
    return NULL;
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

    vlc_mutex_destroy( &p_sys->cache_lock );
    free( p_sys->p_cache );

    vlc_object_detach( p_server );
    vlc_object_destroy( p_server );

    /* all sessions depending on the server are now deinitialized */
    gnutls_certificate_free_credentials( p_sys->x509_cred );
    gnutls_dh_params_deinit( p_sys->dh_params );
    free( p_sys );
}


/*****************************************************************************
 * tls_ServerAddCA:
 *****************************************************************************
 * Adds one or more certificate authorities.
 * Returns -1 on error, 0 on success.
 *****************************************************************************/
static int
gnutls_ServerAddCA( tls_server_t *p_server, const char *psz_ca_path )
{
    int val;
    tls_server_sys_t *p_sys;

    p_sys = (tls_server_sys_t *)(p_server->p_sys);

    val = gnutls_certificate_set_x509_trust_file( p_sys->x509_cred,
                                                  psz_ca_path,
                                                  GNUTLS_X509_FMT_PEM );
    if( val < 0 )
    {
        msg_Err( p_server, "Cannot add trusted CA (%s) : %s", psz_ca_path,
                 gnutls_strerror( val ) );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_server, " %d trusted CA added (%s)", val, psz_ca_path );

    /* enables peer's certificate verification */
    p_sys->pf_handshake2 = gnutls_HandshakeAndValidate;

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
        msg_Err( p_server, "Cannot add CRL (%s) : %s", psz_crl_path,
                 gnutls_strerror( val ) );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_server, "%d CRL added (%s)", val, psz_crl_path );
    return VLC_SUCCESS;
}
    

/*****************************************************************************
 * tls_ServerCreate:
 *****************************************************************************
 * Allocates a whole server's TLS credentials.
 * Returns NULL on error.
 *****************************************************************************/
static tls_server_t *
gnutls_ServerCreate( tls_t *p_tls, const char *psz_cert_path,
                     const char *psz_key_path )
{
    tls_server_t *p_server;
    tls_server_sys_t *p_sys;
    int val;

    msg_Dbg( p_tls, "Creating TLS server" );

    p_sys = (tls_server_sys_t *)malloc( sizeof(struct tls_server_sys_t) );
    if( p_sys == NULL )
        return NULL;

    p_sys->i_cache_size = get_Int( p_tls, "tls-cache-size" );
    p_sys->p_cache = (struct saved_session_t *)calloc( p_sys->i_cache_size,
                                           sizeof( struct saved_session_t ) );
    if( p_sys->p_cache == NULL )
    {
        free( p_sys );
        return NULL;
    }
    p_sys->p_store = p_sys->p_cache;

    p_server = vlc_object_create( p_tls, sizeof(struct tls_server_t) );
    if( p_server == NULL )
    {
        free( p_sys->p_cache );
        free( p_sys );
        return NULL;
    }

    vlc_object_attach( p_server, p_tls );

    p_server->p_sys = p_sys;
    p_server->pf_delete = gnutls_ServerDelete;
    p_server->pf_add_CA = gnutls_ServerAddCA;
    p_server->pf_add_CRL = gnutls_ServerAddCRL;
    p_server->pf_session_prepare = gnutls_ServerSessionPrepare;

    /* No certificate validation by default */
    p_sys->pf_handshake2 = gnutls_ContinueHandshake;

    /* FIXME: check for errors */
    vlc_mutex_init( p_server, &p_sys->cache_lock );

    /* Sets server's credentials */
    val = gnutls_certificate_allocate_credentials( &p_sys->x509_cred );
    if( val != 0 )
    {
        msg_Err( p_server, "Cannot allocate X509 credentials : %s",
                 gnutls_strerror( val ) );
        goto error;
    }

    val = gnutls_certificate_set_x509_key_file( p_sys->x509_cred,
                                                psz_cert_path, psz_key_path,
                                                GNUTLS_X509_FMT_PEM );
    if( val < 0 )
    {
        msg_Err( p_server, "Cannot set certificate chain or private key : %s",
                 gnutls_strerror( val ) );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        goto error;
    }

    /* FIXME:
     * - regenerate these regularly
     * - support other ciper suites
     */
    val = gnutls_dh_params_init( &p_sys->dh_params );
    if( val >= 0 )
    {
        msg_Dbg( p_server, "Computing Diffie Hellman ciphers parameters" );
        val = gnutls_dh_params_generate2( p_sys->dh_params,
                                          get_Int( p_tls, "dh-bits" ) );
    }
    if( val < 0 )
    {
        msg_Err( p_server, "Cannot initialize DH cipher suites : %s",
                 gnutls_strerror( val ) );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        goto error;
    }
    msg_Dbg( p_server, "Ciphers parameters computed" );

    gnutls_certificate_set_dh_params( p_sys->x509_cred, p_sys->dh_params);

    return p_server;

error:
    vlc_mutex_destroy( &p_sys->cache_lock );
    vlc_object_detach( p_server );
    vlc_object_destroy( p_server );
    free( p_sys );
    return NULL;
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

    var_Create( p_this->p_libvlc, "gnutls_mutex", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "gnutls_mutex", &lock );
    vlc_mutex_lock( lock.p_address );

    /* Initialize GnuTLS only once */
    var_Create( p_this->p_libvlc, "gnutls_count", VLC_VAR_INTEGER );
    var_Get( p_this->p_libvlc, "gnutls_count", &count);

    if( count.i_int == 0)
    {
        const char *psz_version;

        __p_gcry_data = VLC_OBJECT( p_this->p_vlc );

        gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_vlc);
        if( gnutls_global_init( ) )
        {
            msg_Warn( p_this, "cannot initialize GnuTLS" );
            vlc_mutex_unlock( lock.p_address );
            return VLC_EGENERIC;
        }
        /*
         * FIXME: in fact, we currently depends on 1.0.17, but it breaks on
         * Debian which as a patched 1.0.16 (which we can use).
         */
        psz_version = gnutls_check_version( "1.0.16" );
        if( psz_version == NULL )
        {
            gnutls_global_deinit( );
            vlc_mutex_unlock( lock.p_address );
            msg_Err( p_this, "unsupported GnuTLS version" );
            return VLC_EGENERIC;
        }
        msg_Dbg( p_this, "GnuTLS v%s initialized", psz_version );
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
        msg_Dbg( p_this, "GnuTLS deinitialized" );
    }

    vlc_mutex_unlock( lock.p_address );
}
