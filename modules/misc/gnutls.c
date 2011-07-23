/*****************************************************************************
 * gnutls.c
 *****************************************************************************
 * Copyright (C) 2004-2006 Rémi Denis-Courmont
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <errno.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef WIN32
# include <io.h>
# include <wincrypt.h>
#else
# include <unistd.h>
#endif
# include <fcntl.h>


#include <vlc_tls.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_block.h>

#include <gcrypt.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include <vlc_gcrypt.h>
#include "dhparams.h"

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenClient  (vlc_object_t *);
static void CloseClient (vlc_object_t *);
static int  OpenServer  (vlc_object_t *);
static void CloseServer (vlc_object_t *);

#define PRIORITIES_TEXT N_("TLS cipher priorities")
#define PRIORITIES_LONGTEXT N_("Ciphers, key exchange methods, " \
    "hash functions and compression methods can be selected. " \
    "Refer to GNU TLS documentation for detailed syntax.")
static const char *const priorities_values[] = {
    "PERFORMANCE",
    "NORMAL",
    "SECURE128",
    "SECURE256",
    "EXPORT",
};
static const char *const priorities_text[] = {
    N_("Performance (prioritize faster ciphers)"),
    N_("Normal"),
    N_("Secure 128-bits (exclude 256-bits ciphers)"),
    N_("Secure 256-bits (prioritize 256-bits ciphers)"),
    N_("Export (include insecure ciphers)"),
};

vlc_module_begin ()
    set_shortname( "GNU TLS" )
    set_description( N_("GNU TLS transport layer security") )
    set_capability( "tls client", 1 )
    set_callbacks( OpenClient, CloseClient )
    set_category( CAT_ADVANCED )
    set_subcategory( SUBCAT_ADVANCED_MISC )

    add_submodule ()
        set_description( N_("GNU TLS server") )
        set_capability( "tls server", 1 )
        set_category( CAT_ADVANCED )
        set_subcategory( SUBCAT_ADVANCED_MISC )
        set_callbacks( OpenServer, CloseServer )

        add_string ("gnutls-priorities", "NORMAL", PRIORITIES_TEXT,
                    PRIORITIES_LONGTEXT, false)
            change_string_list (priorities_values, priorities_text, NULL)
vlc_module_end ()

static vlc_mutex_t gnutls_mutex = VLC_STATIC_MUTEX;

/**
 * Initializes GnuTLS with proper locking.
 * @return VLC_SUCCESS on success, a VLC error code otherwise.
 */
static int gnutls_Init (vlc_object_t *p_this)
{
    int ret = VLC_EGENERIC;

    vlc_gcrypt_init (); /* GnuTLS depends on gcrypt */

    vlc_mutex_lock (&gnutls_mutex);
    if (gnutls_global_init ())
    {
        msg_Err (p_this, "cannot initialize GnuTLS");
        goto error;
    }

    const char *psz_version = gnutls_check_version ("2.0.0");
    if (psz_version == NULL)
    {
        msg_Err (p_this, "unsupported GnuTLS version");
        gnutls_global_deinit ();
        goto error;
    }

    msg_Dbg (p_this, "GnuTLS v%s initialized", psz_version);
    ret = VLC_SUCCESS;

error:
    vlc_mutex_unlock (&gnutls_mutex);
    return ret;
}


/**
 * Deinitializes GnuTLS.
 */
static void gnutls_Deinit (vlc_object_t *p_this)
{
    vlc_mutex_lock (&gnutls_mutex);

    gnutls_global_deinit ();
    msg_Dbg (p_this, "GnuTLS deinitialized");
    vlc_mutex_unlock (&gnutls_mutex);
}


static int gnutls_Error (vlc_object_t *obj, int val)
{
    switch (val)
    {
        case GNUTLS_E_AGAIN:
#ifdef WIN32
            WSASetLastError (WSAEWOULDBLOCK);
#else
            errno = EAGAIN;
#endif
            break;

        case GNUTLS_E_INTERRUPTED:
#ifdef WIN32
            WSASetLastError (WSAEINTR);
#else
            errno = EINTR;
#endif
            break;

        default:
            msg_Err (obj, "%s", gnutls_strerror (val));
#ifndef NDEBUG
            if (!gnutls_error_is_fatal (val))
                msg_Err (obj, "Error above should be handled");
#endif
#ifdef WIN32
            WSASetLastError (WSAECONNRESET);
#else
            errno = ECONNRESET;
#endif
    }
    return -1;
}


struct tls_session_sys_t
{
    gnutls_session_t session;
    char            *psz_hostname;
    bool       b_handshaked;
};


/**
 * Sends data through a TLS session.
 */
static int
gnutls_Send( void *p_session, const void *buf, int i_length )
{
    int val;
    tls_session_sys_t *p_sys;

    p_sys = (tls_session_sys_t *)(((tls_session_t *)p_session)->p_sys);

    val = gnutls_record_send( p_sys->session, buf, i_length );
    return (val < 0) ? gnutls_Error ((vlc_object_t *)p_session, val) : val;
}


/**
 * Receives data through a TLS session.
 */
static int
gnutls_Recv( void *p_session, void *buf, int i_length )
{
    int val;
    tls_session_sys_t *p_sys;

    p_sys = (tls_session_sys_t *)(((tls_session_t *)p_session)->p_sys);

    val = gnutls_record_recv( p_sys->session, buf, i_length );
    return (val < 0) ? gnutls_Error ((vlc_object_t *)p_session, val) : val;
}


/**
 * Starts or continues the TLS handshake.
 *
 * @return -1 on fatal error, 0 on successful handshake completion,
 * 1 if more would-be blocking recv is needed,
 * 2 if more would-be blocking send is required.
 */
static int
gnutls_ContinueHandshake (tls_session_t *p_session)
{
    tls_session_sys_t *p_sys = p_session->p_sys;
    int val;

#ifdef WIN32
    WSASetLastError( 0 );
#endif
    val = gnutls_handshake( p_sys->session );
    if( ( val == GNUTLS_E_AGAIN ) || ( val == GNUTLS_E_INTERRUPTED ) )
        return 1 + gnutls_record_get_direction( p_sys->session );

    if( val < 0 )
    {
#ifdef WIN32
        msg_Dbg( p_session, "Winsock error %d", WSAGetLastError( ) );
#endif
        msg_Err( p_session, "TLS handshake error: %s",
                 gnutls_strerror( val ) );
        return -1;
    }

    p_sys->b_handshaked = true;
    return 0;
}


typedef struct
{
    int flag;
    const char *msg;
} error_msg_t;

static const error_msg_t cert_errors[] =
{
    { GNUTLS_CERT_INVALID,
        "Certificate could not be verified" },
    { GNUTLS_CERT_REVOKED,
        "Certificate was revoked" },
    { GNUTLS_CERT_SIGNER_NOT_FOUND,
        "Certificate's signer was not found" },
    { GNUTLS_CERT_SIGNER_NOT_CA,
        "Certificate's signer is not a CA" },
    { GNUTLS_CERT_INSECURE_ALGORITHM,
        "Insecure certificate signature algorithm" },
    { 0, NULL }
};


static int
gnutls_HandshakeAndValidate( tls_session_t *session )
{
    int val = gnutls_ContinueHandshake( session );
    if( val )
        return val;

    tls_session_sys_t *p_sys = (tls_session_sys_t *)(session->p_sys);

    /* certificates chain verification */
    unsigned status;
    val = gnutls_certificate_verify_peers2( p_sys->session, &status );

    if( val )
    {
        msg_Err( session, "Certificate verification failed: %s",
                 gnutls_strerror( val ) );
        return -1;
    }

    if( status )
    {
        msg_Err( session, "TLS session: access denied" );
        for( const error_msg_t *e = cert_errors; e->flag; e++ )
        {
            if( status & e->flag )
            {
                msg_Err( session, "%s", e->msg );
                status &= ~e->flag;
            }
        }

        if( status )
            msg_Err( session,
                     "unknown certificate error (you found a bug in VLC)" );

        return -1;
    }

    /* certificate (host)name verification */
    const gnutls_datum_t *data;
    data = gnutls_certificate_get_peers (p_sys->session, &(unsigned){0});
    if( data == NULL )
    {
        msg_Err( session, "Peer certificate not available" );
        return -1;
    }

    gnutls_x509_crt_t cert;
    val = gnutls_x509_crt_init( &cert );
    if( val )
    {
        msg_Err( session, "x509 fatal error: %s", gnutls_strerror( val ) );
        return -1;
    }

    val = gnutls_x509_crt_import( cert, data, GNUTLS_X509_FMT_DER );
    if( val )
    {
        msg_Err( session, "Certificate import error: %s",
                 gnutls_strerror( val ) );
        goto error;
    }

    if( p_sys->psz_hostname != NULL
     && !gnutls_x509_crt_check_hostname( cert, p_sys->psz_hostname ) )
    {
        msg_Err( session, "Certificate does not match \"%s\"",
                 p_sys->psz_hostname );
        goto error;
    }

    if( gnutls_x509_crt_get_expiration_time( cert ) < time( NULL ) )
    {
        msg_Err( session, "Certificate expired" );
        goto error;
    }

    if( gnutls_x509_crt_get_activation_time( cert ) > time ( NULL ) )
    {
        msg_Err( session, "Certificate not yet valid" );
        goto error;
    }

    gnutls_x509_crt_deinit( cert );
    msg_Dbg( session, "TLS/x509 certificate verified" );
    return 0;

error:
    gnutls_x509_crt_deinit( cert );
    return -1;
}

/**
 * Sets the operating system file descriptor backend for the TLS sesison.
 *
 * @param fd stream socket already connected with the peer.
 */
static void
gnutls_SetFD (tls_session_t *p_session, int fd)
{
    gnutls_transport_set_ptr (p_session->p_sys->session,
                              (gnutls_transport_ptr_t)(intptr_t)fd);
}

static int
gnutls_SessionPrioritize (vlc_object_t *obj, gnutls_session_t session)
{
    char *priorities = var_InheritString (obj, "gnutls-priorities");
    if (unlikely(priorities == NULL))
        return VLC_ENOMEM;

    const char *errp;
    int val = gnutls_priority_set_direct (session, priorities, &errp);
    if (val < 0)
    {
        msg_Err (obj, "cannot set TLS priorities \"%s\": %s", errp,
                 gnutls_strerror (val));
        val = VLC_EGENERIC;
    }
    else
        val = VLC_SUCCESS;
    free (priorities);
    return val;
}


static int
gnutls_Addx509File( vlc_object_t *p_this,
                    gnutls_certificate_credentials_t cred,
                    const char *psz_path, bool b_priv );
#ifdef WIN32
static int gnutls_loadOSCAList(vlc_object_t *p_this,
                               gnutls_certificate_credentials_t cred);
#endif

static int
gnutls_Addx509Directory( vlc_object_t *p_this,
                         gnutls_certificate_credentials_t cred,
                         const char *psz_dirname,
                         bool b_priv )
{
    DIR* dir;

    if( *psz_dirname == '\0' )
        psz_dirname = ".";

    dir = vlc_opendir( psz_dirname );
    if( dir == NULL )
    {
        if (errno != ENOENT)
        {
            msg_Err (p_this, "cannot open directory (%s): %m", psz_dirname);
            return VLC_EGENERIC;
        }

        msg_Dbg (p_this, "creating empty certificate directory: %s",
                 psz_dirname);
        vlc_mkdir (psz_dirname, b_priv ? 0700 : 0755);
        return VLC_SUCCESS;
    }
#ifdef S_ISLNK
    else
    {
        struct stat st1, st2;
        int fd = dirfd( dir );

        /*
         * Gets stats for the directory path, checks that it is not a
         * symbolic link (to avoid possibly infinite recursion), and verifies
         * that the inode is still the same, to avoid TOCTOU race condition.
         */
        if( ( fd == -1)
         || fstat( fd, &st1 ) || vlc_lstat( psz_dirname, &st2 )
         || S_ISLNK( st2.st_mode ) || ( st1.st_ino != st2.st_ino ) )
        {
            closedir( dir );
            return VLC_EGENERIC;
        }
    }
#endif

    for (;;)
    {
        char *ent = vlc_readdir (dir);
        if (ent == NULL)
            break;

        if ((strcmp (ent, ".") == 0) || (strcmp (ent, "..") == 0))
        {
            free( ent );
            continue;
        }

        char path[strlen (psz_dirname) + strlen (ent) + 2];
        sprintf (path, "%s"DIR_SEP"%s", psz_dirname, ent);
        free (ent);

        gnutls_Addx509File( p_this, cred, path, b_priv );
    }

    closedir( dir );
    return VLC_SUCCESS;
}


static int
gnutls_Addx509File( vlc_object_t *p_this,
                    gnutls_certificate_credentials cred,
                    const char *psz_path, bool b_priv )
{
    struct stat st;

    int fd = vlc_open (psz_path, O_RDONLY);
    if (fd == -1)
        goto error;

    block_t *block = block_File (fd);
    if (block != NULL)
    {
        close (fd);

        gnutls_datum data = {
            .data = block->p_buffer,
            .size = block->i_buffer,
        };
        int res = b_priv
            ? gnutls_certificate_set_x509_key_mem (cred, &data, &data,
                                                   GNUTLS_X509_FMT_PEM)
            : gnutls_certificate_set_x509_trust_mem (cred, &data,
                                                     GNUTLS_X509_FMT_PEM);
        block_Release (block);

        if (res < 0)
        {
            msg_Warn (p_this, "cannot add x509 credentials (%s): %s",
                      psz_path, gnutls_strerror (res));
            return VLC_EGENERIC;
        }
        msg_Dbg (p_this, "added x509 credentials (%s)", psz_path);
        return VLC_SUCCESS;
    }

    if (!fstat (fd, &st) && S_ISDIR (st.st_mode))
    {
        close (fd);
        msg_Dbg (p_this, "looking recursively for x509 credentials in %s",
                 psz_path);
        return gnutls_Addx509Directory (p_this, cred, psz_path, b_priv);
    }

error:
    msg_Warn (p_this, "cannot add x509 credentials (%s): %m", psz_path);
    if (fd != -1)
        close (fd);
    return VLC_EGENERIC;
}

#ifdef WIN32
static int
gnutls_loadOSCAList( vlc_object_t *p_this,
                     gnutls_certificate_credentials cred)
{
    HCERTSTORE hCertStore = CertOpenSystemStoreA((HCRYPTPROV)NULL, "ROOT");
    if (!hCertStore)
    {
        msg_Warn (p_this, "could not open the Cert SystemStore");
        return VLC_EGENERIC;
    }

    PCCERT_CONTEXT pCertContext = CertEnumCertificatesInStore(hCertStore, NULL);
    while( pCertContext )
    {
        gnutls_datum data = {
            .data = pCertContext->pbCertEncoded,
            .size = pCertContext->cbCertEncoded,
        };

        if(!gnutls_certificate_set_x509_trust_mem(cred, &data, GNUTLS_X509_FMT_DER))
        {
            msg_Warn (p_this, "cannot add x509 credential");
            return VLC_EGENERIC;
        }

        pCertContext = CertEnumCertificatesInStore(hCertStore, pCertContext);
    }
    return VLC_SUCCESS;
}
#endif

/** TLS client session data */
typedef struct tls_client_sys_t
{
    struct tls_session_sys_t         session;
    gnutls_certificate_credentials_t x509_cred;
} tls_client_sys_t;


/**
 * Initializes a client-side TLS session.
 */
static int OpenClient (vlc_object_t *obj)
{
    tls_session_t *p_session = (tls_session_t *)obj;
    int i_val;

    if (gnutls_Init (obj))
        return VLC_EGENERIC;

    tls_client_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
    {
        gnutls_Deinit (obj);
        return VLC_ENOMEM;
    }

    p_session->p_sys = &p_sys->session;
    p_session->sock.p_sys = p_session;
    p_session->sock.pf_send = gnutls_Send;
    p_session->sock.pf_recv = gnutls_Recv;
    p_session->pf_set_fd = gnutls_SetFD;

    p_sys->session.b_handshaked = false;

    i_val = gnutls_certificate_allocate_credentials (&p_sys->x509_cred);
    if (i_val != 0)
    {
        msg_Err (obj, "cannot allocate X509 credentials: %s",
                 gnutls_strerror (i_val));
        goto error;
    }

    char *userdir = config_GetUserDir ( VLC_DATA_DIR );
    if (userdir != NULL)
    {
        char path[strlen (userdir) + sizeof ("/ssl/private")];
        sprintf (path, "%s/ssl", userdir);
        vlc_mkdir (path, 0755);

        sprintf (path, "%s/ssl/certs", userdir);
        gnutls_Addx509Directory (VLC_OBJECT (p_session),
                                 p_sys->x509_cred, path, false);
        sprintf (path, "%s/ssl/private", userdir);
        gnutls_Addx509Directory (VLC_OBJECT (p_session), p_sys->x509_cred,
                                 path, true);
        free (userdir);
    }

    const char *confdir = config_GetConfDir ();
    {
        char path[strlen (confdir)
                   + sizeof ("/ssl/certs/ca-certificates.crt")];
        sprintf (path, "%s/ssl/certs/ca-certificates.crt", confdir);
#ifdef WIN32
        gnutls_loadOSCAList (VLC_OBJECT (p_session),
                             p_sys->x509_cred);
#else
        gnutls_Addx509File (VLC_OBJECT (p_session),
                            p_sys->x509_cred, path, false);
#endif
    }
    p_session->pf_handshake = gnutls_HandshakeAndValidate;
    /*p_session->pf_handshake = gnutls_ContinueHandshake;*/

    i_val = gnutls_init (&p_sys->session.session, GNUTLS_CLIENT);
    if (i_val != 0)
    {
        msg_Err (obj, "cannot initialize TLS session: %s",
                 gnutls_strerror (i_val));
        gnutls_certificate_free_credentials (p_sys->x509_cred);
        goto error;
    }

    if (gnutls_SessionPrioritize (VLC_OBJECT (p_session),
                                  p_sys->session.session))
        goto s_error;

    /* minimum DH prime bits */
    gnutls_dh_set_prime_bits (p_sys->session.session, 1024);

    i_val = gnutls_credentials_set (p_sys->session.session,
                                    GNUTLS_CRD_CERTIFICATE,
                                    p_sys->x509_cred);
    if (i_val < 0)
    {
        msg_Err (obj, "cannot set TLS session credentials: %s",
                 gnutls_strerror (i_val));
        goto s_error;
    }

    char *servername = var_GetNonEmptyString (p_session, "tls-server-name");
    if (servername == NULL )
        msg_Err (p_session, "server name missing for TLS session");
    else
        gnutls_server_name_set (p_sys->session.session, GNUTLS_NAME_DNS,
                                servername, strlen (servername));

    p_sys->session.psz_hostname = servername;

    return VLC_SUCCESS;

s_error:
    gnutls_deinit (p_sys->session.session);
    gnutls_certificate_free_credentials (p_sys->x509_cred);
error:
    gnutls_Deinit (obj);
    free (p_sys);
    return VLC_EGENERIC;
}


static void CloseClient (vlc_object_t *obj)
{
    tls_session_t *client = (tls_session_t *)obj;
    tls_client_sys_t *p_sys = (tls_client_sys_t *)(client->p_sys);

    if (p_sys->session.b_handshaked)
        gnutls_bye (p_sys->session.session, GNUTLS_SHUT_WR);
    gnutls_deinit (p_sys->session.session);
    /* credentials must be free'd *after* gnutls_deinit() */
    gnutls_certificate_free_credentials (p_sys->x509_cred);

    gnutls_Deinit (obj);
    free (p_sys->session.psz_hostname);
    free (p_sys);
}


/**
 * Server-side TLS
 */
struct tls_server_sys_t
{
    gnutls_certificate_credentials_t x509_cred;
    gnutls_dh_params_t               dh_params;
    int                            (*pf_handshake) (tls_session_t *);
};


/**
 * Terminates TLS session and releases session data.
 * You still have to close the socket yourself.
 */
static void
gnutls_SessionClose (tls_server_t *p_server, tls_session_t *p_session)
{
    tls_session_sys_t *p_sys = p_session->p_sys;
    (void)p_server;

    if( p_sys->b_handshaked )
        gnutls_bye( p_sys->session, GNUTLS_SHUT_WR );
    gnutls_deinit( p_sys->session );

    vlc_object_release( p_session );

    free( p_sys );
}


/**
 * Initializes a server-side TLS session.
 */
static tls_session_t *
gnutls_ServerSessionPrepare( tls_server_t *p_server )
{
    tls_session_t *p_session;
    tls_server_sys_t *p_server_sys;
    gnutls_session_t session;
    int i_val;

    p_session = vlc_object_create( p_server, sizeof (struct tls_session_t) );
    if( p_session == NULL )
        return NULL;

    p_session->p_sys = malloc( sizeof(struct tls_session_sys_t) );
    if( p_session->p_sys == NULL )
    {
        vlc_object_release( p_session );
        return NULL;
    }

    p_server_sys = p_server->p_sys;
    p_session->sock.p_sys = p_session;
    p_session->sock.pf_send = gnutls_Send;
    p_session->sock.pf_recv = gnutls_Recv;
    p_session->pf_set_fd = gnutls_SetFD;
    p_session->pf_handshake = p_server_sys->pf_handshake;

    p_session->p_sys->b_handshaked = false;
    p_session->p_sys->psz_hostname = NULL;

    i_val = gnutls_init( &session, GNUTLS_SERVER );
    if( i_val != 0 )
    {
        msg_Err( p_server, "cannot initialize TLS session: %s",
                 gnutls_strerror( i_val ) );
        goto error;
    }

    p_session->p_sys->session = session;

    if (gnutls_SessionPrioritize (VLC_OBJECT (p_session), session))
    {
        gnutls_deinit( session );
        goto error;
    }

    i_val = gnutls_credentials_set( session, GNUTLS_CRD_CERTIFICATE,
                                    p_server_sys->x509_cred );
    if( i_val < 0 )
    {
        msg_Err( p_server, "cannot set TLS session credentials: %s",
                 gnutls_strerror( i_val ) );
        gnutls_deinit( session );
        goto error;
    }

    if (p_session->pf_handshake == gnutls_HandshakeAndValidate)
        gnutls_certificate_server_set_request (session, GNUTLS_CERT_REQUIRE);

    return p_session;

error:
    free( p_session->p_sys );
    vlc_object_release( p_session );
    return NULL;
}


/**
 * Adds one or more certificate authorities.
 *
 * @param psz_ca_path (Unicode) path to an x509 certificates list.
 *
 * @return -1 on error, 0 on success.
 */
static int
gnutls_ServerAddCA( tls_server_t *p_server, const char *psz_ca_path )
{
    tls_server_sys_t *p_sys;
    char *psz_local_path;
    int val;

    p_sys = (tls_server_sys_t *)(p_server->p_sys);

    psz_local_path = ToLocale( psz_ca_path );
    val = gnutls_certificate_set_x509_trust_file( p_sys->x509_cred,
                                                  psz_local_path,
                                                  GNUTLS_X509_FMT_PEM );
    LocaleFree( psz_local_path );
    if( val < 0 )
    {
        msg_Err( p_server, "cannot add trusted CA (%s): %s", psz_ca_path,
                 gnutls_strerror( val ) );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_server, " %d trusted CA added (%s)", val, psz_ca_path );

    /* enables peer's certificate verification */
    p_sys->pf_handshake = gnutls_HandshakeAndValidate;

    return VLC_SUCCESS;
}


/**
 * Adds a certificates revocation list to be sent to TLS clients.
 *
 * @param psz_crl_path (Unicode) path of the CRL file.
 *
 * @return -1 on error, 0 on success.
 */
static int
gnutls_ServerAddCRL( tls_server_t *p_server, const char *psz_crl_path )
{
    int val;
    char *psz_local_path = ToLocale( psz_crl_path );

    val = gnutls_certificate_set_x509_crl_file( ((tls_server_sys_t *)
                                                (p_server->p_sys))->x509_cred,
                                                psz_local_path,
                                                GNUTLS_X509_FMT_PEM );
    LocaleFree( psz_local_path );
    if( val < 0 )
    {
        msg_Err( p_server, "cannot add CRL (%s): %s", psz_crl_path,
                 gnutls_strerror( val ) );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_server, "%d CRL added (%s)", val, psz_crl_path );
    return VLC_SUCCESS;
}


/**
 * Allocates a whole server's TLS credentials.
 */
static int OpenServer (vlc_object_t *obj)
{
    tls_server_t *p_server = (tls_server_t *)obj;
    tls_server_sys_t *p_sys;
    int val;

    if (gnutls_Init (obj))
        return VLC_EGENERIC;

    msg_Dbg (obj, "creating TLS server");

    p_sys = (tls_server_sys_t *)malloc( sizeof(struct tls_server_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_server->p_sys = p_sys;
    p_server->pf_add_CA  = gnutls_ServerAddCA;
    p_server->pf_add_CRL = gnutls_ServerAddCRL;
    p_server->pf_open    = gnutls_ServerSessionPrepare;
    p_server->pf_close   = gnutls_SessionClose;

    /* No certificate validation by default */
    p_sys->pf_handshake  = gnutls_ContinueHandshake;

    /* Sets server's credentials */
    val = gnutls_certificate_allocate_credentials( &p_sys->x509_cred );
    if( val != 0 )
    {
        msg_Err( p_server, "cannot allocate X509 credentials: %s",
                 gnutls_strerror( val ) );
        goto error;
    }

    char *psz_cert_path = var_GetNonEmptyString (obj, "tls-x509-cert");
    char *psz_key_path = var_GetNonEmptyString (obj, "tls-x509-key");
    const char *psz_local_cert = ToLocale (psz_cert_path);
    const char *psz_local_key = ToLocale (psz_key_path);
    val = gnutls_certificate_set_x509_key_file (p_sys->x509_cred,
                                                psz_local_cert, psz_local_key,
                                                GNUTLS_X509_FMT_PEM );
    LocaleFree (psz_local_key);
    free (psz_key_path);
    LocaleFree (psz_local_cert);
    free (psz_cert_path);

    if( val < 0 )
    {
        msg_Err( p_server, "cannot set certificate chain or private key: %s",
                 gnutls_strerror( val ) );
        gnutls_certificate_free_credentials( p_sys->x509_cred );
        goto error;
    }

    /* FIXME:
     * - support other ciper suites
     */
    val = gnutls_dh_params_init (&p_sys->dh_params);
    if (val >= 0)
    {
        const gnutls_datum_t data = {
            .data = (unsigned char *)dh_params,
            .size = sizeof (dh_params) - 1,
        };

        val = gnutls_dh_params_import_pkcs3 (p_sys->dh_params, &data,
                                             GNUTLS_X509_FMT_PEM);
        if (val == 0)
            gnutls_certificate_set_dh_params (p_sys->x509_cred,
                                              p_sys->dh_params);
    }
    if (val < 0)
    {
        msg_Err (p_server, "cannot initialize DHE cipher suites: %s",
                 gnutls_strerror (val));
    }

    return VLC_SUCCESS;

error:
    free (p_sys);
    return VLC_EGENERIC;
}

/**
 * Destroys a TLS server object.
 */
static void CloseServer (vlc_object_t *p_server)
{
    tls_server_sys_t *p_sys = ((tls_server_t *)p_server)->p_sys;

    /* all sessions depending on the server are now deinitialized */
    gnutls_certificate_free_credentials (p_sys->x509_cred);
    gnutls_dh_params_deinit (p_sys->dh_params);
    free (p_sys);

    gnutls_Deinit (p_server);
}
