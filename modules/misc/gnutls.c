/*****************************************************************************
 * gnutls.c
 *****************************************************************************
 * Copyright (C) 2004-2017 Rémi Denis-Courmont
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
 * You should have received a copy of the GNU Lesser General Public License
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
#include <unistd.h>
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_tls.h>
#include <vlc_block.h>
#include <vlc_dialog.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

typedef struct vlc_tls_gnutls
{
    vlc_tls_t tls;
    gnutls_session_t session;
    vlc_object_t *obj;
} vlc_tls_gnutls_t;

static void gnutls_Banner(vlc_object_t *obj)
{
    msg_Dbg(obj, "using GnuTLS v%s (built with v"GNUTLS_VERSION")",
            gnutls_check_version(NULL));
}

static int gnutls_Error(vlc_tls_gnutls_t *priv, int val)
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
            msg_Err(priv->obj, "%s", gnutls_strerror (val));
#ifndef NDEBUG
            if (!gnutls_error_is_fatal (val))
                msg_Err(priv->obj, "Error above should be handled");
#endif
#ifdef _WIN32
            WSASetLastError (WSAECONNRESET);
#endif
            errno = ECONNRESET;
    }
    return -1;
}

static ssize_t vlc_gnutls_read(gnutls_transport_ptr_t ptr, void *buf,
                               size_t length)
{
    vlc_tls_t *sock = ptr;
    struct iovec iov = {
        .iov_base = buf,
        .iov_len = length,
    };

    return sock->ops->readv(sock, &iov, 1);
}

static ssize_t vlc_gnutls_writev(gnutls_transport_ptr_t ptr,
                                 const giovec_t *giov, int iovcnt)
{
#ifdef IOV_MAX
    const long iovmax = IOV_MAX;
#else
    const long iovmax = sysconf(_SC_IOV_MAX);
#endif
    if (unlikely(iovcnt > iovmax))
    {
        errno = EINVAL;
        return -1;
    }
    if (unlikely(iovcnt == 0))
        return 0;

    vlc_tls_t *sock = ptr;
    struct iovec iov[iovcnt];

    for (int i = 0; i < iovcnt; i++)
    {
        iov[i].iov_base = giov[i].iov_base;
        iov[i].iov_len = giov[i].iov_len;
    }

    return sock->ops->writev(sock, iov, iovcnt);
}

static int gnutls_GetFD(vlc_tls_t *tls, short *restrict events)
{
    vlc_tls_gnutls_t *priv = (vlc_tls_gnutls_t *)tls;
    vlc_tls_t *sock = gnutls_transport_get_ptr(priv->session);

    return vlc_tls_GetPollFD(sock, events);
}

static ssize_t gnutls_Recv(vlc_tls_t *tls, struct iovec *iov, unsigned count)
{
    vlc_tls_gnutls_t *priv = (vlc_tls_gnutls_t *)tls;
    gnutls_session_t session = priv->session;
    size_t rcvd = 0;

    while (count > 0)
    {
        ssize_t val = gnutls_record_recv(session, iov->iov_base, iov->iov_len);
        if (val < 0)
            return rcvd ? (ssize_t)rcvd : gnutls_Error(priv, val);

        rcvd += val;

        if ((size_t)val < iov->iov_len)
            break;

        iov++;
        count--;
    }

    return rcvd;
}

static ssize_t gnutls_Send (vlc_tls_t *tls, const struct iovec *iov,
                            unsigned count)
{
    vlc_tls_gnutls_t *priv = (vlc_tls_gnutls_t *)tls;
    gnutls_session_t session = priv->session;
    ssize_t val;

    if (!gnutls_record_check_corked(session))
    {
        gnutls_record_cork(session);

        while (count > 0)
        {
            val = gnutls_record_send(session, iov->iov_base, iov->iov_len);
            if (val < (ssize_t)iov->iov_len)
                break;

            iov++;
            count--;
        }
    }

    val = gnutls_record_uncork(session, 0);
    return (val < 0) ? gnutls_Error(priv, val) : val;
}

static int gnutls_Shutdown(vlc_tls_t *tls, bool duplex)
{
    vlc_tls_gnutls_t *priv = (vlc_tls_gnutls_t *)tls;
    gnutls_session_t session = priv->session;
    ssize_t val;

    /* Flush any pending data */
    val = gnutls_record_uncork(session, 0);
    if (val < 0)
        return gnutls_Error(priv, val);

    val = gnutls_bye(session, duplex ? GNUTLS_SHUT_RDWR : GNUTLS_SHUT_WR);
    if (val < 0)
        return gnutls_Error(priv, val);

    return 0;
}

static void gnutls_Close (vlc_tls_t *tls)
{
    vlc_tls_gnutls_t *priv = (vlc_tls_gnutls_t *)tls;

    gnutls_deinit(priv->session);
    free(priv);
}

static const struct vlc_tls_operations gnutls_ops =
{
    gnutls_GetFD,
    gnutls_Recv,
    gnutls_Send,
    gnutls_Shutdown,
    gnutls_Close,
};

static vlc_tls_gnutls_t *gnutls_SessionOpen(vlc_object_t *obj, int type,
                                         gnutls_certificate_credentials_t x509,
                                           vlc_tls_t *sock,
                                           const char *const *alpn)
{
    vlc_tls_gnutls_t *priv = malloc(sizeof (*priv));
    if (unlikely(priv == NULL))
        return NULL;

    gnutls_session_t session;
    const char *errp;
    int val;

    type |= GNUTLS_NONBLOCK | GNUTLS_ENABLE_FALSE_START;

    val = gnutls_init(&session, type);
    if (val != 0)
    {
        msg_Err(obj, "cannot initialize TLS session: %s",
                gnutls_strerror(val));
        free(priv);
        return NULL;
    }

    char *priorities = var_InheritString(obj, "gnutls-priorities");
    if (unlikely(priorities == NULL))
        goto error;

    val = gnutls_priority_set_direct (session, priorities, &errp);
    if (val < 0)
        msg_Err(obj, "cannot set TLS priorities \"%s\": %s", errp,
                gnutls_strerror(val));
    free (priorities);
    if (val < 0)
        goto error;

    val = gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, x509);
    if (val < 0)
    {
        msg_Err(obj, "cannot set TLS session credentials: %s",
                gnutls_strerror(val));
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

    gnutls_transport_set_ptr(session, sock);
    gnutls_transport_set_vec_push_function(session, vlc_gnutls_writev);
    gnutls_transport_set_pull_function(session, vlc_gnutls_read);

    priv->session = session;
    priv->obj = obj;

    vlc_tls_t *tls = &priv->tls;

    tls->ops = &gnutls_ops;
    return priv;

error:
    gnutls_deinit (session);
    free(priv);
    return NULL;
}

/**
 * Starts or continues the TLS handshake.
 *
 * @return -1 on fatal error, 0 on successful handshake completion,
 * 1 if more would-be blocking recv is needed,
 * 2 if more would-be blocking send is required.
 */
static int gnutls_Handshake(vlc_tls_t *tls, char **restrict alp)
{
    vlc_tls_gnutls_t *priv = (vlc_tls_gnutls_t *)tls;
    vlc_object_t *obj = priv->obj;
    gnutls_session_t session = priv->session;
    int val = gnutls_handshake(session);

    if (gnutls_error_is_fatal(val))
    {
        msg_Err(obj, "TLS handshake error: %s", gnutls_strerror(val));
        return -1;
    }

    switch (val)
    {
        case GNUTLS_E_SUCCESS:
            break;
        case GNUTLS_E_AGAIN:
        case GNUTLS_E_INTERRUPTED:
            /* I/O event: return to caller's poll() loop */
            return 1 + gnutls_record_get_direction(session);
        default:
            msg_Warn(obj, "TLS handshake error: %s", gnutls_strerror(val));
            return 1;
    }

    msg_Dbg(obj, "TLS handshake complete");

    unsigned flags = gnutls_session_get_flags(session);

    if (flags & GNUTLS_SFLAGS_SAFE_RENEGOTIATION)
        msg_Dbg(obj, " - safe renegotiation (RFC5746) enabled");
    if (flags & GNUTLS_SFLAGS_EXT_MASTER_SECRET)
        msg_Dbg(obj, " - extended master secret (RFC7627) enabled");
    if (flags & GNUTLS_SFLAGS_ETM)
        msg_Dbg(obj, " - encrypt then MAC (RFC7366) enabled");
    if (flags & GNUTLS_SFLAGS_FALSE_START)
        msg_Dbg(obj, " - false start (RFC7918) enabled");

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

static vlc_tls_t *gnutls_ClientSessionOpen(vlc_tls_client_t *crd,
                                           vlc_tls_t *sk, const char *hostname,
                                           const char *const *alpn)
{
    vlc_tls_gnutls_t *priv = gnutls_SessionOpen(VLC_OBJECT(crd), GNUTLS_CLIENT,
                                                crd->sys, sk, alpn);
    if (priv == NULL)
        return NULL;

    gnutls_session_t session = priv->session;

    /* minimum DH prime bits */
    gnutls_dh_set_prime_bits (session, 1024);

    if (likely(hostname != NULL))
        /* fill Server Name Indication */
        gnutls_server_name_set (session, GNUTLS_NAME_DNS,
                                hostname, strlen (hostname));

    return &priv->tls;
}

static int gnutls_ClientHandshake(vlc_tls_t *tls,
                                  const char *host, const char *service,
                                  char **restrict alp)
{
    vlc_tls_gnutls_t *priv = (vlc_tls_gnutls_t *)tls;
    vlc_object_t *obj = priv->obj;

    int val = gnutls_Handshake(tls, alp);
    if (val)
        return val;

    /* certificates chain verification */
    gnutls_session_t session = priv->session;
    unsigned status;

    val = gnutls_certificate_verify_peers3 (session, host, &status);
    if (val)
    {
        msg_Err(obj, "Certificate verification error: %s",
                gnutls_strerror(val));
        goto error;
    }

    if (status == 0) /* Good certificate */
        return 0;

    /* Bad certificate */
    gnutls_datum_t desc;

    if (gnutls_certificate_verification_status_print(status,
                         gnutls_certificate_type_get (session), &desc, 0) == 0)
    {
        msg_Err(obj, "Certificate verification failure: %s", desc.data);
        gnutls_free (desc.data);
    }

    status &= ~GNUTLS_CERT_INVALID; /* always set / catch-all error */
    status &= ~GNUTLS_CERT_SIGNER_NOT_FOUND; /* unknown CA */
    status &= ~GNUTLS_CERT_UNEXPECTED_OWNER; /* mismatched hostname */

    if (status != 0 || host == NULL)
        goto error; /* Really bad certificate */

    /* Look up mismatching certificate in store */
    const gnutls_datum_t *datum;
    unsigned count;

    datum = gnutls_certificate_get_peers (session, &count);
    if (datum == NULL || count == 0)
    {
        msg_Err(obj, "Peer certificate not available");
        goto error;
    }

    msg_Dbg(obj, "%u certificate(s) in the list", count);
    val = gnutls_verify_stored_pubkey (NULL, NULL, host, service,
                                       GNUTLS_CRT_X509, datum, 0);
    const char *msg;
    switch (val)
    {
        case 0:
            msg_Dbg(obj, "certificate key match for %s", host);
            return 0;
        case GNUTLS_E_NO_CERTIFICATE_FOUND:
            msg_Dbg(obj, "no known certificates for %s", host);
            msg = N_("However, the security certificate presented by the "
                "server is unknown and could not be authenticated by any "
                "trusted Certificate Authority.");
            break;
        case GNUTLS_E_CERTIFICATE_KEY_MISMATCH:
            msg_Dbg(obj, "certificate keys mismatch for %s", host);
            msg = N_("However, the security certificate presented by the "
                "server changed since the previous visit and was not "
                "authenticated by any trusted Certificate Authority.");
            break;
        default:
            msg_Err(obj, "certificate key match error for %s: %s", host,
                    gnutls_strerror(val));
            goto error;
    }

    if (vlc_dialog_wait_question(obj, VLC_DIALOG_QUESTION_WARNING,
            _("Abort"), _("View certificate"), NULL,
            _("Insecure site"),
            _("You attempted to reach %s. %s\n"
            "This problem may be stem from an attempt to breach your security, "
            "compromise your privacy, or a configuration error.\n\n"
            "If in doubt, abort now.\n"), host, vlc_gettext(msg)) != 1)
        goto error;

    gnutls_x509_crt_t cert;

    if (gnutls_x509_crt_init (&cert))
        goto error;
    if (gnutls_x509_crt_import (cert, datum, GNUTLS_X509_FMT_DER)
     || gnutls_x509_crt_print (cert, GNUTLS_CRT_PRINT_ONELINE, &desc))
    {
        gnutls_x509_crt_deinit (cert);
        goto error;
    }
    gnutls_x509_crt_deinit (cert);

    val = vlc_dialog_wait_question(obj, VLC_DIALOG_QUESTION_WARNING,
            _("Abort"), _("Accept 24 hours"), _("Accept permanently"),
            _("Insecure site"),
            _("This is the certificate presented by %s:\n%s\n\n"
            "If in doubt, abort now.\n"), host, desc.data);
    gnutls_free (desc.data);

    time_t expiry = 0;
    switch (val)
    {
        case 1:
            time (&expiry);
            expiry += 24 * 60 * 60;
            /* fall through */
        case 2:
            val = gnutls_store_pubkey (NULL, NULL, host, service,
                                       GNUTLS_CRT_X509, datum, expiry, 0);
            if (val)
                msg_Err(obj, "cannot store X.509 certificate: %s",
                         gnutls_strerror (val));
            break;
        default:
            goto error;
    }
    return 0;

error:
    if (alp != NULL)
        free(*alp);
    return -1;
}

static void gnutls_ClientDestroy(vlc_tls_client_t *crd)
{
    gnutls_certificate_credentials_t x509 = crd->sys;

    gnutls_certificate_free_credentials(x509);
}

static const struct vlc_tls_client_operations gnutls_ClientOps =
{
    .open = gnutls_ClientSessionOpen,
    .handshake = gnutls_ClientHandshake,
    .destroy = gnutls_ClientDestroy,
};

/**
 * Initializes a client-side TLS credentials.
 */
static int OpenClient(vlc_tls_client_t *crd)
{
    gnutls_certificate_credentials_t x509;

    gnutls_Banner(VLC_OBJECT(crd));

    int val = gnutls_certificate_allocate_credentials (&x509);
    if (val != 0)
    {
        msg_Err (crd, "cannot allocate credentials: %s",
                 gnutls_strerror (val));
        return VLC_EGENERIC;
    }

    if (var_InheritBool(crd, "gnutls-system-trust"))
    {
        val = gnutls_certificate_set_x509_system_trust(x509);
        if (val < 0)
            msg_Err(crd, "cannot load trusted Certificate Authorities "
                    "from %s: %s", "system", gnutls_strerror(val));
        else
            msg_Dbg(crd, "loaded %d trusted CAs from %s", val, "system");
    }

    char *dir = var_InheritString(crd, "gnutls-dir-trust");
    if (dir != NULL)
    {
        val = gnutls_certificate_set_x509_trust_dir(x509, dir,
                                                    GNUTLS_X509_FMT_PEM);
        if (val < 0)
            msg_Err(crd, "cannot load trusted Certificate Authorities "
                    "from %s: %s", dir, gnutls_strerror(val));
        else
            msg_Dbg(crd, "loaded %d trusted CAs from %s", val, dir);
        free(dir);
    }

    gnutls_certificate_set_verify_flags (x509,
                                         GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

    crd->ops = &gnutls_ClientOps;
    crd->sys = x509;
    return VLC_SUCCESS;
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
static vlc_tls_t *gnutls_ServerSessionOpen(vlc_tls_server_t *crd,
                                           vlc_tls_t *sk,
                                           const char *const *alpn)
{
    vlc_tls_creds_sys_t *sys = crd->sys;
    vlc_tls_gnutls_t *priv = gnutls_SessionOpen(VLC_OBJECT(crd), GNUTLS_SERVER,
                                                sys->x509_cred, sk, alpn);
    return (priv != NULL) ? &priv->tls : NULL;
}

static void gnutls_ServerDestroy(vlc_tls_server_t *crd)
{
    vlc_tls_creds_sys_t *sys = crd->sys;

    /* all sessions depending on the server are now deinitialized */
    gnutls_certificate_free_credentials(sys->x509_cred);
    gnutls_dh_params_deinit(sys->dh_params);
    free(sys);
}

static const struct vlc_tls_server_operations gnutls_ServerOps =
{
    .open = gnutls_ServerSessionOpen,
    .handshake = gnutls_Handshake,
    .destroy = gnutls_ServerDestroy,
};

/**
 * Allocates a whole server's TLS credentials.
 */
static int OpenServer(vlc_tls_server_t *crd, const char *cert, const char *key)
{
    gnutls_Banner(VLC_OBJECT(crd));

    vlc_tls_creds_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* Sets server's credentials */
    int val = gnutls_certificate_allocate_credentials (&sys->x509_cred);
    if (val != 0)
    {
        msg_Err (crd, "cannot allocate credentials: %s",
                 gnutls_strerror (val));
        free (sys);
        return VLC_ENOMEM;
    }

    block_t *certblock = block_FilePath(cert, false);
    if (certblock == NULL)
    {
        msg_Err (crd, "cannot read certificate chain from %s: %s", cert,
                 vlc_strerror_c(errno));
        goto error;
    }

    block_t *keyblock = block_FilePath(key, false);
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

    crd->ops = &gnutls_ServerOps;
    crd->sys = sys;
    return VLC_SUCCESS;

error:
    gnutls_certificate_free_credentials (sys->x509_cred);
    free (sys);
    return VLC_EGENERIC;
}
#endif

#define SYSTEM_TRUST_TEXT N_("Use system trust database")
#define SYSTEM_TRUST_LONGTEXT N_( \
    "Trust the root certificates of Certificate Authorities stored in " \
    "the operating system trust database to authenticate TLS sessions.")

#define DIR_TRUST_TEXT N_("Trust directory")
#define DIR_TRUST_LONGTEXT N_( \
    "Trust the root certificates of Certificate Authorities stored in " \
    "the specified directory to authenticate TLS sessions.")

#define PRIORITIES_TEXT N_("TLS cipher priorities")
#define PRIORITIES_LONGTEXT N_("Ciphers, key exchange methods, " \
    "hash functions and compression methods can be selected. " \
    "Refer to GNU TLS documentation for detailed syntax.")
static const char *const priorities_values[] = {
    "PERFORMANCE",
    "NORMAL",
    "SECURE128",
    "SECURE256",
};
static const char *const priorities_text[] = {
    N_("Performance (prioritize faster ciphers)"),
    N_("Normal"),
    N_("Secure 128-bits (exclude 256-bits ciphers)"),
    N_("Secure 256-bits (prioritize 256-bits ciphers)"),
};

vlc_module_begin ()
    set_shortname( "GNU TLS" )
    set_description( N_("GNU TLS transport layer security") )
    set_capability( "tls client", 1 )
    set_callback(OpenClient)
    set_category( CAT_ADVANCED )
    set_subcategory( SUBCAT_ADVANCED_NETWORK )
    add_bool("gnutls-system-trust", true, SYSTEM_TRUST_TEXT,
             SYSTEM_TRUST_LONGTEXT, true)
    add_string("gnutls-dir-trust", NULL, DIR_TRUST_TEXT,
               DIR_TRUST_TEXT, true)
    add_string ("gnutls-priorities", "NORMAL", PRIORITIES_TEXT,
                PRIORITIES_LONGTEXT, false)
        change_string_list (priorities_values, priorities_text)
#ifdef ENABLE_SOUT
    add_submodule ()
        set_description( N_("GNU TLS server") )
        set_capability( "tls server", 1 )
        set_category( CAT_ADVANCED )
        set_subcategory( SUBCAT_ADVANCED_NETWORK )
        set_callback(OpenServer)
#endif
vlc_module_end ()
