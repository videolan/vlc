/*****************************************************************************
 * gnutls.c
 *****************************************************************************
 * Copyright (C) 2004-2012 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Öesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <time.h>
#include <errno.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_tls.h>
#include <vlc_block.h>
#include <vlc_dialog.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include "dhparams.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenClient  (vlc_tls_creds_t *);
static void CloseClient (vlc_tls_creds_t *);
static int  OpenServer  (vlc_tls_creds_t *, const char *, const char *);
static void CloseServer (vlc_tls_creds_t *);

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
    set_subcategory( SUBCAT_ADVANCED_NETWORK )

    add_submodule ()
        set_description( N_("GNU TLS server") )
        set_capability( "tls server", 1 )
        set_category( CAT_ADVANCED )
        set_subcategory( SUBCAT_ADVANCED_NETWORK )
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

    const char *psz_version = gnutls_check_version ("3.0.20");
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
#ifdef _WIN32
            WSASetLastError (WSAEWOULDBLOCK);
#else
            errno = EAGAIN;
#endif
            break;

        case GNUTLS_E_INTERRUPTED:
#ifdef _WIN32
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
#ifdef _WIN32
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
static int gnutls_ContinueHandshake (vlc_tls_t *session, const char *host,
                                     const char *service)
{
    vlc_tls_sys_t *sys = session->sys;
    int val;

#ifdef _WIN32
    WSASetLastError (0);
#endif
    do
    {
        val = gnutls_handshake (sys->session);
        msg_Dbg (session, "TLS handshake: %s", gnutls_strerror (val));

        if ((val == GNUTLS_E_AGAIN) || (val == GNUTLS_E_INTERRUPTED))
            /* I/O event: return to caller's poll() loop */
            return 1 + gnutls_record_get_direction (sys->session);
    }
    while (val < 0 && !gnutls_error_is_fatal (val));

    if (val < 0)
    {
#ifdef _WIN32
        msg_Dbg (session, "Winsock error %d", WSAGetLastError ());
#endif
        msg_Err (session, "TLS handshake error: %s", gnutls_strerror (val));
        return -1;
    }

    sys->handshaked = true;
    (void) host; (void) service;
    return 0;
}


/**
 * Looks up certificate in known hosts data base.
 * @return 0 on success, -1 on failure.
 */
static int gnutls_CertSearch (vlc_tls_t *obj, const char *host,
                              const char *service,
                              const gnutls_datum_t *restrict datum)
{
    assert (host != NULL);

    /* Look up mismatching certificate in store */
    int val = gnutls_verify_stored_pubkey (NULL, NULL, host, service,
                                           GNUTLS_CRT_X509, datum, 0);
    const char *msg;
    switch (val)
    {
        case 0:
            msg_Dbg (obj, "certificate key match for %s", host);
            return 0;
        case GNUTLS_E_NO_CERTIFICATE_FOUND:
            msg_Dbg (obj, "no known certificates for %s", host);
            msg = N_("You attempted to reach %s. "
                "However the security certificate presented by the server "
                "is unknown and could not be authenticated by any trusted "
                "Certification Authority. "
                "This problem may be caused by a configuration error "
                "or an attempt to breach your security or your privacy.\n\n"
                "If in doubt, abort now.\n");
            break;
        case GNUTLS_E_CERTIFICATE_KEY_MISMATCH:
            msg_Dbg (obj, "certificate keys mismatch for %s", host);
            msg = N_("You attempted to reach %s. "
                "However the security certificate presented by the server "
                "changed since the previous visit "
                "and was not authenticated by any trusted "
                "Certification Authority. "
                "This problem may be caused by a configuration error "
                "or an attempt to breach your security or your privacy.\n\n"
                "If in doubt, abort now.\n");
            break;
        default:
            msg_Err (obj, "certificate key match error for %s: %s", host,
                     gnutls_strerror (val));
            return -1;
    }

    if (dialog_Question (obj, _("Insecure site"), vlc_gettext (msg),
                         _("Abort"), _("View certificate"), NULL, host) != 2)
        return -1;

    gnutls_x509_crt_t cert;
    gnutls_datum_t desc;

    if (gnutls_x509_crt_init (&cert))
        return -1;
    if (gnutls_x509_crt_import (cert, datum, GNUTLS_X509_FMT_DER)
     || gnutls_x509_crt_print (cert, GNUTLS_CRT_PRINT_ONELINE, &desc))
    {
        gnutls_x509_crt_deinit (cert);
        return -1;
    }
    gnutls_x509_crt_deinit (cert);

    val = dialog_Question (obj, _("Insecure site"),
         _("This is the certificate presented by %s:\n%s\n\n"
           "If in doubt, abort now.\n"),
                           _("Abort"), _("Accept 24 hours"),
                           _("Accept permanently"), host, desc.data);
    gnutls_free (desc.data);

    time_t expiry = 0;
    switch (val)
    {
        case 2:
            time (&expiry);
            expiry += 24 * 60 * 60;
        case 3:
            val = gnutls_store_pubkey (NULL, NULL, host, service,
                                       GNUTLS_CRT_X509, datum, expiry, 0);
            if (val)
                msg_Err (obj, "cannot store X.509 certificate: %s",
                         gnutls_strerror (val));
            return 0;
    }
    return -1;
}


static struct
{
    unsigned flag;
    const char msg[29];
} cert_errs[] =
{
    { GNUTLS_CERT_INVALID,            "Certificate not verified"     },
    { GNUTLS_CERT_REVOKED,            "Certificate revoked"          },
    { GNUTLS_CERT_SIGNER_NOT_FOUND,   "Signer not found"             },
    { GNUTLS_CERT_SIGNER_NOT_CA,      "Signer not a CA"              },
    { GNUTLS_CERT_INSECURE_ALGORITHM, "Signature algorithm insecure" },
    { GNUTLS_CERT_NOT_ACTIVATED,      "Certificate not activated"    },
    { GNUTLS_CERT_EXPIRED,            "Certificate expired"          },
};


static int gnutls_HandshakeAndValidate (vlc_tls_t *session, const char *host,
                                        const char *service)
{
    vlc_tls_sys_t *sys = session->sys;

    int val = gnutls_ContinueHandshake (session, host, service);
    if (val)
        return val;

    /* certificates chain verification */
    unsigned status;

    val = gnutls_certificate_verify_peers2 (sys->session, &status);
    if (val)
    {
        msg_Err (session, "Certificate verification error: %s",
                 gnutls_strerror (val));
        return -1;
    }
    if (status)
    {
        msg_Err (session, "Certificate verification failure (0x%04X)", status);
        for (size_t i = 0; i < sizeof (cert_errs) / sizeof (cert_errs[0]); i++)
            if (status & cert_errs[i].flag)
                msg_Err (session, " * %s", cert_errs[i].msg);
        if (status & ~(GNUTLS_CERT_INVALID|GNUTLS_CERT_SIGNER_NOT_FOUND))
            return -1;
    }

    /* certificate (host)name verification */
    const gnutls_datum_t *data;
    unsigned count;
    data = gnutls_certificate_get_peers (sys->session, &count);
    if (data == NULL || count == 0)
    {
        msg_Err (session, "Peer certificate not available");
        return -1;
    }
    msg_Dbg (session, "%u certificate(s) in the list", count);

    if (val || host == NULL)
        return val;
    if (status && gnutls_CertSearch (session, host, service, data))
        return -1;

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

    val = !gnutls_x509_crt_check_hostname (cert, host);
    if (val)
    {
        msg_Err (session, "Certificate does not match \"%s\"", host);
        val = gnutls_CertSearch (session, host, service, data);
    }
error:
    gnutls_x509_crt_deinit (cert);
    return val;
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


/**
 * TLS credentials private data
 */
struct vlc_tls_creds_sys
{
    gnutls_certificate_credentials_t x509_cred;
    gnutls_dh_params_t dh_params; /* XXX: used for server only */
    int (*handshake) (vlc_tls_t *, const char *, const char *);
        /* ^^ XXX: useful for server only */
};


/**
 * Terminates TLS session and releases session data.
 * You still have to close the socket yourself.
 */
static void gnutls_SessionClose (vlc_tls_creds_t *crd, vlc_tls_t *session)
{
    vlc_tls_sys_t *sys = session->sys;

    if (sys->handshaked)
        gnutls_bye (sys->session, GNUTLS_SHUT_WR);
    gnutls_deinit (sys->session);

    free (sys);
    (void) crd;
}


/**
 * Initializes a server-side TLS session.
 */
static int gnutls_SessionOpen (vlc_tls_creds_t *crd, vlc_tls_t *session,
                               int type, int fd)
{
    vlc_tls_sys_t *sys = malloc (sizeof (*session->sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    session->sys = sys;
    session->sock.p_sys = session;
    session->sock.pf_send = gnutls_Send;
    session->sock.pf_recv = gnutls_Recv;
    session->handshake = crd->sys->handshake;
    sys->handshaked = false;

    int val = gnutls_init (&sys->session, type);
    if (val != 0)
    {
        msg_Err (session, "cannot initialize TLS session: %s",
                 gnutls_strerror (val));
        free (sys);
        return VLC_EGENERIC;
    }

    if (gnutls_SessionPrioritize (VLC_OBJECT (crd), sys->session))
        goto error;

    val = gnutls_credentials_set (sys->session, GNUTLS_CRD_CERTIFICATE,
                                  crd->sys->x509_cred);
    if (val < 0)
    {
        msg_Err (session, "cannot set TLS session credentials: %s",
                 gnutls_strerror (val));
        goto error;
    }

    gnutls_transport_set_ptr (sys->session,
                              (gnutls_transport_ptr_t)(intptr_t)fd);
    return VLC_SUCCESS;

error:
    gnutls_SessionClose (crd, session);
    return VLC_EGENERIC;
}

static int gnutls_ServerSessionOpen (vlc_tls_creds_t *crd, vlc_tls_t *session,
                                     int fd, const char *hostname)
{
    int val = gnutls_SessionOpen (crd, session, GNUTLS_SERVER, fd);
    if (val != VLC_SUCCESS)
        return val;

    if (session->handshake == gnutls_HandshakeAndValidate)
        gnutls_certificate_server_set_request (session->sys->session,
                                               GNUTLS_CERT_REQUIRE);
    assert (hostname == NULL);
    return VLC_SUCCESS;
}

static int gnutls_ClientSessionOpen (vlc_tls_creds_t *crd, vlc_tls_t *session,
                                     int fd, const char *hostname)
{
    int val = gnutls_SessionOpen (crd, session, GNUTLS_CLIENT, fd);
    if (val != VLC_SUCCESS)
        return val;

    vlc_tls_sys_t *sys = session->sys;

    /* minimum DH prime bits */
    gnutls_dh_set_prime_bits (sys->session, 1024);

    if (likely(hostname != NULL))
        /* fill Server Name Indication */
        gnutls_server_name_set (sys->session, GNUTLS_NAME_DNS,
                                hostname, strlen (hostname));

    return VLC_SUCCESS;
}


/**
 * Adds one or more Certificate Authorities to the trusted set.
 *
 * @param path (UTF-8) path to an X.509 certificates list.
 *
 * @return -1 on error, 0 on success.
 */
static int gnutls_AddCA (vlc_tls_creds_t *crd, const char *path)
{
    block_t *block = block_FilePath (path);
    if (block == NULL)
    {
        msg_Err (crd, "cannot read trusted CA from %s: %m", path);
        return VLC_EGENERIC;
    }

    gnutls_datum_t d = {
       .data = block->p_buffer,
       .size = block->i_buffer,
    };

    int val = gnutls_certificate_set_x509_trust_mem (crd->sys->x509_cred, &d,
                                                     GNUTLS_X509_FMT_PEM);
    block_Release (block);
    if (val < 0)
    {
        msg_Err (crd, "cannot load trusted CA from %s: %s", path,
                 gnutls_strerror (val));
        return VLC_EGENERIC;
    }
    msg_Dbg (crd, " %d trusted CA%s added from %s", val, (val != 1) ? "s" : "",
             path);

    /* enables peer's certificate verification */
    crd->sys->handshake = gnutls_HandshakeAndValidate;
    return VLC_SUCCESS;
}


/**
 * Adds a Certificates Revocation List to be sent to TLS clients.
 *
 * @param path (UTF-8) path of the CRL file.
 *
 * @return -1 on error, 0 on success.
 */
static int gnutls_AddCRL (vlc_tls_creds_t *crd, const char *path)
{
    block_t *block = block_FilePath (path);
    if (block == NULL)
    {
        msg_Err (crd, "cannot read CRL from %s: %m", path);
        return VLC_EGENERIC;
    }

    gnutls_datum_t d = {
       .data = block->p_buffer,
       .size = block->i_buffer,
    };

    int val = gnutls_certificate_set_x509_crl_mem (crd->sys->x509_cred, &d,
                                                   GNUTLS_X509_FMT_PEM);
    block_Release (block);
    if (val < 0)
    {
        msg_Err (crd, "cannot add CRL (%s): %s", path, gnutls_strerror (val));
        return VLC_EGENERIC;
    }
    msg_Dbg (crd, "%d CRL%s added from %s", val, (val != 1) ? "s" : "", path);
    return VLC_SUCCESS;
}


/**
 * Allocates a whole server's TLS credentials.
 */
static int OpenServer (vlc_tls_creds_t *crd, const char *cert, const char *key)
{
    int val;

    if (gnutls_Init (VLC_OBJECT(crd)))
        return VLC_EGENERIC;

    vlc_tls_creds_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        goto error;

    crd->sys     = sys;
    crd->add_CA  = gnutls_AddCA;
    crd->add_CRL = gnutls_AddCRL;
    crd->open    = gnutls_ServerSessionOpen;
    crd->close   = gnutls_SessionClose;
    /* No certificate validation by default */
    sys->handshake  = gnutls_ContinueHandshake;

    /* Sets server's credentials */
    val = gnutls_certificate_allocate_credentials (&sys->x509_cred);
    if (val != 0)
    {
        msg_Err (crd, "cannot allocate credentials: %s",
                 gnutls_strerror (val));
        goto error;
    }

    block_t *certblock = block_FilePath (cert);
    if (certblock == NULL)
    {
        msg_Err (crd, "cannot read certificate chain from %s: %m", cert);
        return VLC_EGENERIC;
    }

    block_t *keyblock = block_FilePath (key);
    if (keyblock == NULL)
    {
        msg_Err (crd, "cannot read private key from %s: %m", key);
        block_Release (certblock);
        return VLC_EGENERIC;
    }

    gnutls_datum_t pub = {
       .data = certblock->p_buffer,
       .size = certblock->i_buffer,
    }, priv = {
       .data = keyblock->p_buffer,
       .size = keyblock->i_buffer,
    };

    val = gnutls_certificate_set_x509_key_mem (sys->x509_cred, &pub, &priv,
                                                GNUTLS_X509_FMT_PEM);
    block_Release (keyblock);
    block_Release (certblock);
    if (val < 0)
    {
        msg_Err (crd, "cannot load X.509 key: %s", gnutls_strerror (val));
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
        msg_Err (crd, "cannot initialize DHE cipher suites: %s",
                 gnutls_strerror (val));
    }

    return VLC_SUCCESS;

error:
    free (sys);
    gnutls_Deinit (VLC_OBJECT(crd));
    return VLC_EGENERIC;
}

/**
 * Destroys a TLS server object.
 */
static void CloseServer (vlc_tls_creds_t *crd)
{
    vlc_tls_creds_sys_t *sys = crd->sys;

    /* all sessions depending on the server are now deinitialized */
    gnutls_certificate_free_credentials (sys->x509_cred);
    gnutls_dh_params_deinit (sys->dh_params);
    free (sys);

    gnutls_Deinit (VLC_OBJECT(crd));
}

/**
 * Initializes a client-side TLS credentials.
 */
static int OpenClient (vlc_tls_creds_t *crd)
{
    if (gnutls_Init (VLC_OBJECT(crd)))
        return VLC_EGENERIC;

    vlc_tls_creds_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        goto error;

    crd->sys = sys;
    //crd->add_CA = gnutls_AddCA;
    //crd->add_CRL = gnutls_AddCRL;
    crd->open = gnutls_ClientSessionOpen;
    crd->close = gnutls_SessionClose;
    sys->handshake = gnutls_HandshakeAndValidate;

    int val = gnutls_certificate_allocate_credentials (&sys->x509_cred);
    if (val != 0)
    {
        msg_Err (crd, "cannot allocate credentials: %s",
                 gnutls_strerror (val));
        goto error;
    }

    val = gnutls_certificate_set_x509_system_trust (sys->x509_cred);
    if (val < 0)
        msg_Err (crd, "cannot load trusted Certificate Authorities: %s",
                 gnutls_strerror (val));
    else
        msg_Dbg (crd, "loaded %d trusted CAs", val);

    gnutls_certificate_set_verify_flags (sys->x509_cred,
                                         GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

    return VLC_SUCCESS;
error:
    free (sys);
    gnutls_Deinit (VLC_OBJECT(crd));
    return VLC_EGENERIC;
}

static void CloseClient (vlc_tls_creds_t *crd)
{
    vlc_tls_creds_sys_t *sys = crd->sys;

    gnutls_certificate_free_credentials (sys->x509_cred);
    free (sys);

    gnutls_Deinit (VLC_OBJECT(crd));
}
