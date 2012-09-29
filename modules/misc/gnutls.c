/*****************************************************************************
 * gnutls.c
 *****************************************************************************
 * Copyright (C) 2004-2011 Rémi Denis-Courmont
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

#include <errno.h>
#include <sys/types.h>
#include <errno.h>

#include <sys/stat.h>
#ifdef WIN32
# include <windows.h>
# include <io.h>
# include <wincrypt.h>
#else
# include <unistd.h>
#endif
#include <fcntl.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_tls.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_block.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "dhparams.h"

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenClient  (vlc_tls_t *, int, const char *);
static void CloseClient (vlc_tls_t *);
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
            change_string_list (priorities_values, priorities_text)
vlc_module_end ()

static vlc_mutex_t gnutls_mutex = VLC_STATIC_MUTEX;

/**
 * Initializes GnuTLS with proper locking.
 * @return VLC_SUCCESS on success, a VLC error code otherwise.
 */
static int gnutls_Init (vlc_object_t *p_this)
{
    int ret = VLC_EGENERIC;

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
#define gnutls_Error(o, val) gnutls_Error(VLC_OBJECT(o), val)


struct vlc_tls_sys
{
    gnutls_session_t session;
    gnutls_certificate_credentials_t x509_cred;
    char *hostname;
    bool handshaked;
};


/**
 * Sends data through a TLS session.
 */
static int gnutls_Send (void *opaque, const void *buf, size_t length)
{
    vlc_tls_t *session = opaque;
    vlc_tls_sys_t *sys = session->sys;

    int val = gnutls_record_send (sys->session, buf, length);
    return (val < 0) ? gnutls_Error (session, val) : val;
}


/**
 * Receives data through a TLS session.
 */
static int gnutls_Recv (void *opaque, void *buf, size_t length)
{
    vlc_tls_t *session = opaque;
    vlc_tls_sys_t *sys = session->sys;

    int val = gnutls_record_recv (sys->session, buf, length);
    return (val < 0) ? gnutls_Error (session, val) : val;
}


/**
 * Starts or continues the TLS handshake.
 *
 * @return -1 on fatal error, 0 on successful handshake completion,
 * 1 if more would-be blocking recv is needed,
 * 2 if more would-be blocking send is required.
 */
static int gnutls_ContinueHandshake (vlc_tls_t *session)
{
    vlc_tls_sys_t *sys = session->sys;
    int val;

#ifdef WIN32
    WSASetLastError (0);
#endif
    val = gnutls_handshake (sys->session);
    if ((val == GNUTLS_E_AGAIN) || (val == GNUTLS_E_INTERRUPTED))
        return 1 + gnutls_record_get_direction (sys->session);

    if (val < 0)
    {
#ifdef WIN32
        msg_Dbg (session, "Winsock error %d", WSAGetLastError ());
#endif
        msg_Err (session, "TLS handshake error: %s", gnutls_strerror (val));
        return -1;
    }

    sys->handshaked = true;
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
    { GNUTLS_CERT_NOT_ACTIVATED,
        "Certificate is not yet activated" },
    { GNUTLS_CERT_EXPIRED,
        "Certificate has expired" },
    { 0, NULL }
};


static int gnutls_HandshakeAndValidate (vlc_tls_t *session)
{
    vlc_tls_sys_t *sys = session->sys;

    int val = gnutls_ContinueHandshake (session);
    if (val)
        return val;

    /* certificates chain verification */
    unsigned status;

    val = gnutls_certificate_verify_peers2 (sys->session, &status);
    if (val)
    {
        msg_Err (session, "Certificate verification failed: %s",
                 gnutls_strerror (val));
        return -1;
    }

    if (status)
    {
        msg_Err (session, "TLS session: access denied (status 0x%X)", status);
        for (const error_msg_t *e = cert_errors; e->flag; e++)
        {
            if (status & e->flag)
            {
                msg_Err (session, "%s", e->msg);
                status &= ~e->flag;
            }
        }

        if (status)
            msg_Err (session,
                     "unknown certificate error (you found a bug in VLC)");
        return -1;
    }

    /* certificate (host)name verification */
    const gnutls_datum_t *data;
    data = gnutls_certificate_get_peers (sys->session, &(unsigned){0});
    if (data == NULL)
    {
        msg_Err (session, "Peer certificate not available");
        return -1;
    }

    gnutls_x509_crt_t cert;
    val = gnutls_x509_crt_init (&cert);
    if (val)
    {
        msg_Err (session, "X.509 fatal error: %s", gnutls_strerror (val));
        return -1;
    }

    val = gnutls_x509_crt_import (cert, data, GNUTLS_X509_FMT_DER);
    if (val)
    {
        msg_Err (session, "Certificate import error: %s",
                 gnutls_strerror (val));
        goto error;
    }

    if (sys->hostname != NULL
     && !gnutls_x509_crt_check_hostname (cert, sys->hostname))
    {
        msg_Err (session, "Certificate does not match \"%s\"", sys->hostname);
        goto error;
    }

    gnutls_x509_crt_deinit (cert);
    msg_Dbg (session, "TLS/X.509 certificate verified");
    return 0;

error:
    gnutls_x509_crt_deinit (cert);
    return -1;
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

#ifndef WIN32
/**
 * Loads x509 credentials from a file descriptor (directory or regular file)
 * and closes the descriptor.
 */
static void gnutls_x509_AddFD (vlc_object_t *obj,
                               gnutls_certificate_credentials_t cred,
                               int fd, bool priv, unsigned recursion)
{
    DIR *dir = fdopendir (fd);
    if (dir != NULL)
    {
        if (recursion == 0)
            goto skipdir;
        recursion--;

        for (;;)
        {
            char *ent = vlc_readdir (dir);
            if (ent == NULL)
                break;

            if ((strcmp (ent, ".") == 0) || (strcmp (ent, "..") == 0))
            {
                free (ent);
                continue;
            }

            int nfd = vlc_openat (fd, ent, O_RDONLY);
            if (nfd != -1)
            {
                msg_Dbg (obj, "loading x509 credentials from %s...", ent);
                gnutls_x509_AddFD (obj, cred, nfd, priv, recursion);
            }
            else
                msg_Dbg (obj, "cannot access x509 credentials in %s", ent);
            free (ent);
        }
    skipdir:
        closedir (dir);
        return;
    }

    block_t *block = block_File (fd);
    if (block != NULL)
    {
        gnutls_datum_t data = {
            .data = block->p_buffer,
            .size = block->i_buffer,
        };
        int res = priv
            ? gnutls_certificate_set_x509_key_mem (cred, &data, &data,
                                                   GNUTLS_X509_FMT_PEM)
            : gnutls_certificate_set_x509_trust_mem (cred, &data,
                                                     GNUTLS_X509_FMT_PEM);
        block_Release (block);

        if (res < 0)
            msg_Warn (obj, "cannot add x509 credentials: %s",
                      gnutls_strerror (res));
        else
            msg_Dbg (obj, "added %d %s(s)", res, priv ? "key" : "certificate");
    }
    else
        msg_Warn (obj, "cannot read x509 credentials: %m");
    close (fd);
}

static void gnutls_x509_AddPath (vlc_object_t *obj,
                                 gnutls_certificate_credentials_t cred,
                                 const char *path, bool priv)
{
    msg_Dbg (obj, "loading x509 credentials in %s...", path);
    int fd = vlc_open (path, O_RDONLY);
    if (fd == -1)
    {
        msg_Warn (obj, "cannot access x509 in %s: %m", path);
        return;
    }

    gnutls_x509_AddFD (obj, cred, fd, priv, 5);
}
#else /* WIN32 */
static int
gnutls_loadOSCAList (vlc_object_t *p_this,
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
#endif /* WIN32 */

/**
 * Initializes a client-side TLS session.
 */
static int OpenClient (vlc_tls_t *session, int fd, const char *hostname)
{
    if (gnutls_Init (VLC_OBJECT(session)))
        return VLC_EGENERIC;

    vlc_tls_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
    {
        gnutls_Deinit (VLC_OBJECT(session));
        return VLC_ENOMEM;
    }

    session->sys = sys;
    session->sock.p_sys = session;
    session->sock.pf_send = gnutls_Send;
    session->sock.pf_recv = gnutls_Recv;
    sys->handshaked = false;

    int val = gnutls_certificate_allocate_credentials (&sys->x509_cred);
    if (val != 0)
    {
        msg_Err (session, "cannot allocate credentials: %s",
                 gnutls_strerror (val));
        goto error;
    }

#ifndef WIN32
    char *userdir = config_GetUserDir (VLC_DATA_DIR);
    if (userdir != NULL)
    {
        char path[strlen (userdir) + sizeof ("/ssl/private/")];
        sprintf (path, "%s/ssl", userdir);
        vlc_mkdir (path, 0755);

        sprintf (path, "%s/ssl/certs/", userdir);
        gnutls_x509_AddPath (VLC_OBJECT(session), sys->x509_cred, path, false);
        sprintf (path, "%s/ssl/private/", userdir);
        gnutls_x509_AddPath (VLC_OBJECT(session), sys->x509_cred, path, true);
        free (userdir);
    }

    const char *confdir = config_GetConfDir ();
    {
        char path[strlen (confdir)
                   + sizeof ("/ssl/certs/ca-certificates.crt")];
        sprintf (path, "%s/ssl/certs/ca-certificates.crt", confdir);
        gnutls_x509_AddPath (VLC_OBJECT(session), sys->x509_cred, path, false);
    }
#else /* WIN32 */
    gnutls_loadOSCAList (VLC_OBJECT(session), sys->x509_cred);
#endif
    gnutls_certificate_set_verify_flags (sys->x509_cred,
                                         GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

    session->handshake = gnutls_HandshakeAndValidate;
    /*session->_handshake = gnutls_ContinueHandshake;*/

    val = gnutls_init (&sys->session, GNUTLS_CLIENT);
    if (val != 0)
    {
        msg_Err (session, "cannot initialize TLS session: %s",
                 gnutls_strerror (val));
        gnutls_certificate_free_credentials (sys->x509_cred);
        goto error;
    }

    if (gnutls_SessionPrioritize (VLC_OBJECT(session), sys->session))
        goto s_error;

    /* minimum DH prime bits */
    gnutls_dh_set_prime_bits (sys->session, 1024);

    val = gnutls_credentials_set (sys->session, GNUTLS_CRD_CERTIFICATE,
                                  sys->x509_cred);
    if (val < 0)
    {
        msg_Err (session, "cannot set TLS session credentials: %s",
                 gnutls_strerror (val));
        goto s_error;
    }

    /* server name */
    if (likely(hostname != NULL))
    {
        /* fill Server Name Indication */
        gnutls_server_name_set (sys->session, GNUTLS_NAME_DNS,
                                hostname, strlen (hostname));
        /* keep hostname to match CNAME after handshake */
        sys->hostname = strdup (hostname);
        if (unlikely(sys->hostname == NULL))
            goto s_error;
    }
    else
        sys->hostname = NULL;

    gnutls_transport_set_ptr (sys->session,
                              (gnutls_transport_ptr_t)(intptr_t)fd);
    return VLC_SUCCESS;

s_error:
    gnutls_deinit (sys->session);
    gnutls_certificate_free_credentials (sys->x509_cred);
error:
    gnutls_Deinit (VLC_OBJECT(session));
    free (sys);
    return VLC_EGENERIC;
}


static void CloseClient (vlc_tls_t *session)
{
    vlc_tls_sys_t *sys = session->sys;

    if (sys->handshaked)
        gnutls_bye (sys->session, GNUTLS_SHUT_WR);
    gnutls_deinit (sys->session);
    /* credentials must be free'd *after* gnutls_deinit() */
    gnutls_certificate_free_credentials (sys->x509_cred);

    gnutls_Deinit (VLC_OBJECT(session));
    free (sys->hostname);
    free (sys);
}


/**
 * Server-side TLS
 */
struct vlc_tls_creds_sys
{
    gnutls_certificate_credentials_t x509_cred;
    gnutls_dh_params_t               dh_params;
    int                            (*handshake) (vlc_tls_t *);
};


/**
 * Terminates TLS session and releases session data.
 * You still have to close the socket yourself.
 */
static void gnutls_SessionClose (vlc_tls_t *session)
{
    vlc_tls_sys_t *sys = session->sys;

    if (sys->handshaked)
        gnutls_bye (sys->session, GNUTLS_SHUT_WR);
    gnutls_deinit (sys->session);

    vlc_object_release (session);
    free (sys);
}


/**
 * Initializes a server-side TLS session.
 */
static vlc_tls_t *gnutls_SessionOpen (vlc_tls_creds_t *server, int fd)
{
    vlc_tls_creds_sys_t *ssys = server->sys;
    int val;

    vlc_tls_t *session = vlc_object_create (server, sizeof (*session));
    if (unlikely(session == NULL))
        return NULL;

    vlc_tls_sys_t *sys = malloc (sizeof (*session->sys));
    if (unlikely(sys == NULL))
    {
        vlc_object_release (session);
        return NULL;
    }

    session->sys = sys;
    session->sock.p_sys = session;
    session->sock.pf_send = gnutls_Send;
    session->sock.pf_recv = gnutls_Recv;
    session->handshake = ssys->handshake;
    session->u.close = gnutls_SessionClose;
    sys->handshaked = false;
    sys->hostname = NULL;

    val = gnutls_init (&sys->session, GNUTLS_SERVER);
    if (val != 0)
    {
        msg_Err (server, "cannot initialize TLS session: %s",
                 gnutls_strerror (val));
        free (sys);
        vlc_object_release (session);
        return NULL;
    }

    if (gnutls_SessionPrioritize (VLC_OBJECT (server), sys->session))
        goto error;

    val = gnutls_credentials_set (sys->session, GNUTLS_CRD_CERTIFICATE,
                                  ssys->x509_cred);
    if (val < 0)
    {
        msg_Err (server, "cannot set TLS session credentials: %s",
                 gnutls_strerror (val));
        goto error;
    }

    if (session->handshake == gnutls_HandshakeAndValidate)
        gnutls_certificate_server_set_request (sys->session,
                                               GNUTLS_CERT_REQUIRE);

    gnutls_transport_set_ptr (sys->session,
                              (gnutls_transport_ptr_t)(intptr_t)fd);
    return session;

error:
    gnutls_SessionClose (session);
    return NULL;
}


/**
 * Adds one or more certificate authorities.
 *
 * @param ca_path (Unicode) path to an x509 certificates list.
 *
 * @return -1 on error, 0 on success.
 */
static int gnutls_ServerAddCA (vlc_tls_creds_t *server, const char *ca_path)
{
    vlc_tls_creds_sys_t *sys = server->sys;
    const char *local_path = ToLocale (ca_path);

    int val = gnutls_certificate_set_x509_trust_file (sys->x509_cred,
                                                      local_path,
                                                      GNUTLS_X509_FMT_PEM );
    LocaleFree (local_path);
    if (val < 0)
    {
        msg_Err (server, "cannot add trusted CA (%s): %s", ca_path,
                 gnutls_strerror (val));
        return VLC_EGENERIC;
    }
    msg_Dbg (server, " %d trusted CA added (%s)", val, ca_path);

    /* enables peer's certificate verification */
    sys->handshake = gnutls_HandshakeAndValidate;

    return VLC_SUCCESS;
}


/**
 * Adds a certificates revocation list to be sent to TLS clients.
 *
 * @param crl_path (Unicode) path of the CRL file.
 *
 * @return -1 on error, 0 on success.
 */
static int gnutls_ServerAddCRL (vlc_tls_creds_t *server, const char *crl_path)
{
    vlc_tls_creds_sys_t *sys = server->sys;
    const char *local_path = ToLocale (crl_path);

    int val = gnutls_certificate_set_x509_crl_file (sys->x509_cred,
                                                    local_path,
                                                    GNUTLS_X509_FMT_PEM);
    LocaleFree (local_path);
    if (val < 0)
    {
        msg_Err (server, "cannot add CRL (%s): %s", crl_path,
                 gnutls_strerror (val));
        return VLC_EGENERIC;
    }
    msg_Dbg (server, "%d CRL added (%s)", val, crl_path);
    return VLC_SUCCESS;
}


/**
 * Allocates a whole server's TLS credentials.
 */
static int OpenServer (vlc_object_t *obj)
{
    vlc_tls_creds_t *server = (vlc_tls_creds_t *)obj;
    int val;

    if (gnutls_Init (obj))
        return VLC_EGENERIC;

    msg_Dbg (obj, "creating TLS server");

    vlc_tls_creds_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        goto error;

    server->sys     = sys;
    server->add_CA  = gnutls_ServerAddCA;
    server->add_CRL = gnutls_ServerAddCRL;
    server->open    = gnutls_SessionOpen;
    /* No certificate validation by default */
    sys->handshake  = gnutls_ContinueHandshake;

    /* Sets server's credentials */
    val = gnutls_certificate_allocate_credentials (&sys->x509_cred);
    if (val != 0)
    {
        msg_Err (server, "cannot allocate credentials: %s",
                 gnutls_strerror (val));
        goto error;
    }

    char *cert_path = var_GetNonEmptyString (obj, "tls-x509-cert");
    char *key_path = var_GetNonEmptyString (obj, "tls-x509-key");
    const char *lcert = ToLocale (cert_path);
    const char *lkey = ToLocale (key_path);
    val = gnutls_certificate_set_x509_key_file (sys->x509_cred, lcert, lkey,
                                                GNUTLS_X509_FMT_PEM);
    LocaleFree (lkey);
    LocaleFree (lcert);
    free (key_path);
    free (cert_path);

    if (val < 0)
    {
        msg_Err (server, "cannot set certificate chain or private key: %s",
                 gnutls_strerror (val));
        gnutls_certificate_free_credentials (sys->x509_cred);
        goto error;
    }

    /* FIXME:
     * - support other cipher suites
     */
    val = gnutls_dh_params_init (&sys->dh_params);
    if (val >= 0)
    {
        const gnutls_datum_t data = {
            .data = (unsigned char *)dh_params,
            .size = sizeof (dh_params) - 1,
        };

        val = gnutls_dh_params_import_pkcs3 (sys->dh_params, &data,
                                             GNUTLS_X509_FMT_PEM);
        if (val == 0)
            gnutls_certificate_set_dh_params (sys->x509_cred,
                                              sys->dh_params);
    }
    if (val < 0)
    {
        msg_Err (server, "cannot initialize DHE cipher suites: %s",
                 gnutls_strerror (val));
    }

    return VLC_SUCCESS;

error:
    free (sys);
    gnutls_Deinit (obj);
    return VLC_EGENERIC;
}

/**
 * Destroys a TLS server object.
 */
static void CloseServer (vlc_object_t *obj)
{
    vlc_tls_creds_t *server = (vlc_tls_creds_t *)obj;
    vlc_tls_creds_sys_t *sys = server->sys;

    /* all sessions depending on the server are now deinitialized */
    gnutls_certificate_free_credentials (sys->x509_cred);
    gnutls_dh_params_deinit (sys->dh_params);
    free (sys);

    gnutls_Deinit (obj);
}
