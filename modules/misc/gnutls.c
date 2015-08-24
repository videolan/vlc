/*****************************************************************************
 * gnutls.c
 *****************************************************************************
 * Copyright (C) 2004-2015 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>
#include <stdlib.h>
#include <string.h>
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

#if (GNUTLS_VERSION_NUMBER >= 0x030300)
static int gnutls_Init (vlc_object_t *obj)
{
    const char *version = gnutls_check_version ("3.3.0");
    if (version == NULL)
    {
        msg_Err (obj, "unsupported GnuTLS version");
        return -1;
    }
    msg_Dbg (obj, "using GnuTLS version %s", version);
    return 0;
}

# define gnutls_Deinit() (void)0
#else
#define GNUTLS_SEC_PARAM_MEDIUM GNUTLS_SEC_PARAM_NORMAL
static vlc_mutex_t gnutls_mutex = VLC_STATIC_MUTEX;

/**
 * Initializes GnuTLS with proper locking.
 * @return VLC_SUCCESS on success, a VLC error code otherwise.
 */
static int gnutls_Init (vlc_object_t *obj)
{
    const char *version = gnutls_check_version ("3.1.11");
    if (version == NULL)
    {
        msg_Err (obj, "unsupported GnuTLS version");
        return -1;
    }
    msg_Dbg (obj, "using GnuTLS version %s", version);

    if (gnutls_check_version ("3.3.0") == NULL)
    {
         int val;

         vlc_mutex_lock (&gnutls_mutex);
         val = gnutls_global_init ();
         vlc_mutex_unlock (&gnutls_mutex);

         if (val)
         {
             msg_Err (obj, "cannot initialize GnuTLS");
             return -1;
         }
    }
    return 0;
}

/**
 * Deinitializes GnuTLS.
 */
static void gnutls_Deinit (void)
{
    vlc_mutex_lock (&gnutls_mutex);
    gnutls_global_deinit ();
    vlc_mutex_unlock (&gnutls_mutex);
}
#endif

static int gnutls_Error (vlc_object_t *obj, int val)
{
    switch (val)
    {
        case GNUTLS_E_AGAIN:
#ifdef _WIN32
            WSASetLastError (WSAEWOULDBLOCK);
#endif
            errno = EAGAIN;
            break;

        case GNUTLS_E_INTERRUPTED:
#ifdef _WIN32
            WSASetLastError (WSAEINTR);
#endif
            errno = EINTR;
            break;

        default:
            msg_Err (obj, "%s", gnutls_strerror (val));
#ifndef NDEBUG
            if (!gnutls_error_is_fatal (val))
                msg_Err (obj, "Error above should be handled");
#endif
#ifdef _WIN32
            WSASetLastError (WSAECONNRESET);
#endif
            errno = ECONNRESET;
    }
    return -1;
}
#define gnutls_Error(o, val) gnutls_Error(VLC_OBJECT(o), val)

#ifdef IOV_MAX
static ssize_t vlc_gnutls_writev (gnutls_transport_ptr_t ptr,
                                  const giovec_t *giov, int iovcnt)
{
    if (unlikely((unsigned)iovcnt > IOV_MAX))
    {
        errno = EINVAL;
        return -1;
    }
    if (unlikely(iovcnt == 0))
        return 0;

    struct iovec iov[iovcnt];
    struct msghdr msg = {
        .msg_iov = iov,
        .msg_iovlen = iovcnt,
    };
    int fd = (intptr_t)ptr;

    for (int i = 0; i < iovcnt; i++)
    {
        iov[i].iov_base = giov[i].iov_base;
        iov[i].iov_len = giov[i].iov_len;
    }

    return sendmsg (fd, &msg, MSG_NOSIGNAL);
}
#endif

/**
 * Sends data through a TLS session.
 */
static int gnutls_Send (void *opaque, const void *buf, size_t length)
{
    assert (opaque != NULL);

    vlc_tls_t *tls = opaque;
    gnutls_session_t session = tls->sys;

    int val = gnutls_record_send (session, buf, length);
    return (val < 0) ? gnutls_Error (tls, val) : val;
}


/**
 * Receives data through a TLS session.
 */
static int gnutls_Recv (void *opaque, void *buf, size_t length)
{
    assert (opaque != NULL);

    vlc_tls_t *tls = opaque;
    gnutls_session_t session = tls->sys;

    int val = gnutls_record_recv (session, buf, length);
    return (val < 0) ? gnutls_Error (tls, val) : val;
}

static int gnutls_SessionOpen (vlc_tls_t *tls, int type,
                               gnutls_certificate_credentials_t x509, int fd,
                               const char *const *alpn)
{
    gnutls_session_t session;
    const char *errp;
    int val;

    val = gnutls_init (&session, type);
    if (val != 0)
    {
        msg_Err (tls, "cannot initialize TLS session: %s",
                 gnutls_strerror (val));
        return VLC_EGENERIC;
    }

    char *priorities = var_InheritString (tls, "gnutls-priorities");
    if (unlikely(priorities == NULL))
        goto error;

    val = gnutls_priority_set_direct (session, priorities, &errp);
    if (val < 0)
        msg_Err (tls, "cannot set TLS priorities \"%s\": %s", errp,
                 gnutls_strerror (val));
    free (priorities);
    if (val < 0)
        goto error;

    val = gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, x509);
    if (val < 0)
    {
        msg_Err (tls, "cannot set TLS session credentials: %s",
                 gnutls_strerror (val));
        goto error;
    }

    if (alpn != NULL)
    {
        gnutls_datum_t *protv = NULL;
        unsigned protc = 0;

        while (*alpn != NULL)
        {
            gnutls_datum_t *n = realloc(protv, sizeof (*protv) * (protc + 1));
            if (unlikely(n == NULL))
            {
                free(protv);
                goto error;
            }
            protv = n;

            protv[protc].data = (void *)*alpn;
            protv[protc].size = strlen(*alpn);
            protc++;
            alpn++;
        }

        val = gnutls_alpn_set_protocols (session, protv, protc, 0);
        free (protv);
    }

    gnutls_transport_set_int (session, fd);
#ifdef IOV_MAX
    gnutls_transport_set_vec_push_function (session, vlc_gnutls_writev);
#endif
    tls->sys = session;
    tls->sock.p_sys = NULL;
    tls->sock.pf_send = gnutls_Send;
    tls->sock.pf_recv = gnutls_Recv;
    return VLC_SUCCESS;

error:
    gnutls_deinit (session);
    return VLC_EGENERIC;
}

/**
 * Starts or continues the TLS handshake.
 *
 * @return -1 on fatal error, 0 on successful handshake completion,
 * 1 if more would-be blocking recv is needed,
 * 2 if more would-be blocking send is required.
 */
static int gnutls_ContinueHandshake (vlc_tls_t *tls, char **restrict alp)
{
    gnutls_session_t session = tls->sys;
    int val;

#ifdef _WIN32
    WSASetLastError (0);
#endif
    do
    {
        val = gnutls_handshake (session);
        msg_Dbg (tls, "TLS handshake: %s", gnutls_strerror (val));

        switch (val)
        {
            case GNUTLS_E_SUCCESS:
                goto done;
            case GNUTLS_E_AGAIN:
            case GNUTLS_E_INTERRUPTED:
                /* I/O event: return to caller's poll() loop */
                return 1 + gnutls_record_get_direction (session);
        }
    }
    while (!gnutls_error_is_fatal (val));

#ifdef _WIN32
    msg_Dbg (tls, "Winsock error %d", WSAGetLastError ());
#endif
    msg_Err (tls, "TLS handshake error: %s", gnutls_strerror (val));
    return -1;

done:
    if (alp != NULL)
    {
        gnutls_datum_t datum;

        val = gnutls_alpn_get_selected_protocol (session, &datum);
        if (val == 0)
        {
            if (memchr (datum.data, 0, datum.size) != NULL)
                return -1; /* Other end is doing something fishy?! */

            *alp = strndup ((char *)datum.data, datum.size);
            if (unlikely(*alp == NULL))
                return -1;
        }
        else
            *alp = NULL;
    }
    return 0;
}

/**
 * Terminates TLS session and releases session data.
 * You still have to close the socket yourself.
 */
static void gnutls_SessionClose (vlc_tls_t *tls)
{
    gnutls_session_t session = tls->sys;

    if (tls->sock.p_sys != NULL)
        gnutls_bye (session, GNUTLS_SHUT_WR);

    gnutls_deinit (session);
}

static int gnutls_ClientSessionOpen (vlc_tls_creds_t *crd, vlc_tls_t *tls,
                                     int fd, const char *hostname,
                                     const char *const *alpn)
{
    int val = gnutls_SessionOpen (tls, GNUTLS_CLIENT, crd->sys, fd, alpn);
    if (val != VLC_SUCCESS)
        return val;

    gnutls_session_t session = tls->sys;

    /* minimum DH prime bits */
    gnutls_dh_set_prime_bits (session, 1024);

    if (likely(hostname != NULL))
        /* fill Server Name Indication */
        gnutls_server_name_set (session, GNUTLS_NAME_DNS,
                                hostname, strlen (hostname));

    return VLC_SUCCESS;
}

static int gnutls_ClientHandshake (vlc_tls_t *tls, const char *host,
                                   const char *service, char **restrict alp)
{
    int val = gnutls_ContinueHandshake (tls, alp);
    if (val)
        return val;

    /* certificates chain verification */
    gnutls_session_t session = tls->sys;
    unsigned status;

    val = gnutls_certificate_verify_peers3 (session, host, &status);
    if (val)
    {
        msg_Err (tls, "Certificate verification error: %s",
                 gnutls_strerror (val));
failure:
        gnutls_bye (session, GNUTLS_SHUT_RDWR);
        return -1;
    }

    if (status == 0)
    {   /* Good certificate */
success:
        tls->sock.p_sys = tls;
        return 0;
    }

    /* Bad certificate */
    gnutls_datum_t desc;

    if (gnutls_certificate_verification_status_print(status,
                         gnutls_certificate_type_get (session), &desc, 0) == 0)
    {
        msg_Err (tls, "Certificate verification failure: %s", desc.data);
        gnutls_free (desc.data);
    }

    status &= ~GNUTLS_CERT_INVALID; /* always set / catch-all error */
    status &= ~GNUTLS_CERT_SIGNER_NOT_FOUND; /* unknown CA */
    status &= ~GNUTLS_CERT_UNEXPECTED_OWNER; /* mismatched hostname */

    if (status != 0 || host == NULL)
        goto failure; /* Really bad certificate */

    /* Look up mismatching certificate in store */
    const gnutls_datum_t *datum;
    unsigned count;

    datum = gnutls_certificate_get_peers (session, &count);
    if (datum == NULL || count == 0)
    {
        msg_Err (tls, "Peer certificate not available");
        goto failure;
    }

    msg_Dbg (tls, "%u certificate(s) in the list", count);
    val = gnutls_verify_stored_pubkey (NULL, NULL, host, service,
                                       GNUTLS_CRT_X509, datum, 0);
    const char *msg;
    switch (val)
    {
        case 0:
            msg_Dbg (tls, "certificate key match for %s", host);
            goto success;
        case GNUTLS_E_NO_CERTIFICATE_FOUND:
            msg_Dbg (tls, "no known certificates for %s", host);
            msg = N_("However the security certificate presented by the "
                "server is unknown and could not be authenticated by any "
                "trusted Certificate Authority.");
            break;
        case GNUTLS_E_CERTIFICATE_KEY_MISMATCH:
            msg_Dbg (tls, "certificate keys mismatch for %s", host);
            msg = N_("However the security certificate presented by the "
                "server changed since the previous visit and was not "
                "authenticated by any trusted Certificate Authority. ");
            break;
        default:
            msg_Err (tls, "certificate key match error for %s: %s", host,
                     gnutls_strerror (val));
            goto failure;
    }

    if (dialog_Question (tls, _("Insecure site"),
        _("You attempted to reach %s. %s\n"
          "This problem may be stem from an attempt to breach your security, "
          "compromise your privacy, or a configuration error.\n\n"
          "If in doubt, abort now.\n"),
                         _("Abort"), _("View certificate"), NULL,
                         vlc_gettext (msg), host) != 2)
        goto failure;

    gnutls_x509_crt_t cert;

    if (gnutls_x509_crt_init (&cert))
        goto failure;
    if (gnutls_x509_crt_import (cert, datum, GNUTLS_X509_FMT_DER)
     || gnutls_x509_crt_print (cert, GNUTLS_CRT_PRINT_ONELINE, &desc))
    {
        gnutls_x509_crt_deinit (cert);
        goto failure;
    }
    gnutls_x509_crt_deinit (cert);

    val = dialog_Question (tls, _("Insecure site"),
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
                msg_Err (tls, "cannot store X.509 certificate: %s",
                         gnutls_strerror (val));
            goto success;
    }
    goto failure;
}

/**
 * Initializes a client-side TLS credentials.
 */
static int OpenClient (vlc_tls_creds_t *crd)
{
    gnutls_certificate_credentials_t x509;

    if (gnutls_Init (VLC_OBJECT(crd)))
        return VLC_EGENERIC;

    int val = gnutls_certificate_allocate_credentials (&x509);
    if (val != 0)
    {
        msg_Err (crd, "cannot allocate credentials: %s",
                 gnutls_strerror (val));
        gnutls_Deinit ();
        return VLC_EGENERIC;
    }

    val = gnutls_certificate_set_x509_system_trust (x509);
    if (val < 0)
        msg_Err (crd, "cannot load trusted Certificate Authorities: %s",
                 gnutls_strerror (val));
    else
        msg_Dbg (crd, "loaded %d trusted CAs", val);

    gnutls_certificate_set_verify_flags (x509,
                                         GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

    crd->sys = x509;
    crd->open = gnutls_ClientSessionOpen;
    crd->handshake = gnutls_ClientHandshake;
    crd->close = gnutls_SessionClose;

    return VLC_SUCCESS;
}

static void CloseClient (vlc_tls_creds_t *crd)
{
    gnutls_certificate_credentials_t x509 = crd->sys;

    gnutls_certificate_free_credentials (x509);
    gnutls_Deinit ();
}

#ifdef ENABLE_SOUT
/**
 * Server-side TLS credentials private data
 */
typedef struct vlc_tls_creds_sys
{
    gnutls_certificate_credentials_t x509_cred;
    gnutls_dh_params_t dh_params;
} vlc_tls_creds_sys_t;

/**
 * Initializes a server-side TLS session.
 */
static int gnutls_ServerSessionOpen (vlc_tls_creds_t *crd, vlc_tls_t *tls,
                                     int fd, const char *hostname,
                                     const char *const *alpn)
{
    vlc_tls_creds_sys_t *sys = crd->sys;

    assert (hostname == NULL);
    return gnutls_SessionOpen (tls, GNUTLS_SERVER, sys->x509_cred, fd, alpn);
}

static int gnutls_ServerHandshake (vlc_tls_t *tls, const char *host,
                                   const char *service, char **restrict alp)
{
    int val = gnutls_ContinueHandshake (tls, alp);
    if (val == 0)
        tls->sock.p_sys = tls;

    (void) host; (void) service;
    return val;
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
    {
        gnutls_Deinit ();
        return VLC_ENOMEM;
    }

    /* Sets server's credentials */
    val = gnutls_certificate_allocate_credentials (&sys->x509_cred);
    if (val != 0)
    {
        msg_Err (crd, "cannot allocate credentials: %s",
                 gnutls_strerror (val));
        free (sys);
        gnutls_Deinit ();
        return VLC_ENOMEM;
    }

    block_t *certblock = block_FilePath (cert);
    if (certblock == NULL)
    {
        msg_Err (crd, "cannot read certificate chain from %s: %s", cert,
                 vlc_strerror_c(errno));
        goto error;
    }

    block_t *keyblock = block_FilePath (key);
    if (keyblock == NULL)
    {
        msg_Err (crd, "cannot read private key from %s: %s", key,
                 vlc_strerror_c(errno));
        block_Release (certblock);
        goto error;
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
     * - regenerate these regularly
     * - support other cipher suites
     */
    val = gnutls_dh_params_init (&sys->dh_params);
    if (val >= 0)
    {
        gnutls_sec_param_t sec = GNUTLS_SEC_PARAM_MEDIUM;
        unsigned bits = gnutls_sec_param_to_pk_bits (GNUTLS_PK_DH, sec);

        msg_Dbg (crd, "generating Diffie-Hellman %u-bits parameters...", bits);
        val = gnutls_dh_params_generate2 (sys->dh_params, bits);
        if (val == 0)
            gnutls_certificate_set_dh_params (sys->x509_cred,
                                              sys->dh_params);
    }
    if (val < 0)
    {
        msg_Err (crd, "cannot initialize DHE cipher suites: %s",
                 gnutls_strerror (val));
    }

    msg_Dbg (crd, "ciphers parameters loaded");

    crd->sys = sys;
    crd->open = gnutls_ServerSessionOpen;
    crd->handshake = gnutls_ServerHandshake;
    crd->close = gnutls_SessionClose;

    return VLC_SUCCESS;

error:
    gnutls_certificate_free_credentials (sys->x509_cred);
    free (sys);
    gnutls_Deinit ();
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
    gnutls_Deinit ();
}
#endif

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
    add_string ("gnutls-priorities", "NORMAL", PRIORITIES_TEXT,
                PRIORITIES_LONGTEXT, false)
        change_string_list (priorities_values, priorities_text)
#ifdef ENABLE_SOUT
    add_submodule ()
        set_description( N_("GNU TLS server") )
        set_capability( "tls server", 1 )
        set_category( CAT_ADVANCED )
        set_subcategory( SUBCAT_ADVANCED_NETWORK )
        set_callbacks( OpenServer, CloseServer )
#endif
vlc_module_end ()
