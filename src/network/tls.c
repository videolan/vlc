/*****************************************************************************
 * tls.c
 *****************************************************************************
 * Copyright © 2004-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * @file
 * libvlc interface to the Transport Layer Security (TLS) plugins.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_POLL
# include <poll.h>
#endif
#include <assert.h>

#include <vlc_common.h>
#include "libvlc.h"

#include <vlc_tls.h>
#include <vlc_modules.h>

/*** TLS credentials ***/

static int tls_server_load(void *func, va_list ap)
{
    int (*activate) (vlc_tls_creds_t *, const char *, const char *) = func;
    vlc_tls_creds_t *crd = va_arg (ap, vlc_tls_creds_t *);
    const char *cert = va_arg (ap, const char *);
    const char *key = va_arg (ap, const char *);

    return activate (crd, cert, key);
}

static int tls_client_load(void *func, va_list ap)
{
    int (*activate) (vlc_tls_creds_t *) = func;
    vlc_tls_creds_t *crd = va_arg (ap, vlc_tls_creds_t *);

    return activate (crd);
}

static void tls_unload(void *func, va_list ap)
{
    void (*deactivate) (vlc_tls_creds_t *) = func;
    vlc_tls_creds_t *crd = va_arg (ap, vlc_tls_creds_t *);

    deactivate (crd);
}

/**
 * Allocates a whole server's TLS credentials.
 *
 * @param cert_path required (Unicode) path to an x509 certificate,
 *                  if NULL, anonymous key exchange will be used.
 * @param key_path (UTF-8) path to the PKCS private key for the certificate,
 *                 if NULL; cert_path will be used.
 *
 * @return NULL on error.
 */
vlc_tls_creds_t *
vlc_tls_ServerCreate (vlc_object_t *obj, const char *cert_path,
                      const char *key_path)
{
    vlc_tls_creds_t *srv = vlc_custom_create (obj, sizeof (*srv),
                                              "tls server");
    if (unlikely(srv == NULL))
        return NULL;

    if (key_path == NULL)
        key_path = cert_path;

    srv->module = vlc_module_load (srv, "tls server", NULL, false,
                                   tls_server_load, srv, cert_path, key_path);
    if (srv->module == NULL)
    {
        msg_Err (srv, "TLS server plugin not available");
        vlc_object_release (srv);
        return NULL;
    }

    return srv;
}

/**
 * Allocates TLS credentials for a client.
 * Credentials can be cached and reused across multiple TLS sessions.
 *
 * @return TLS credentials object, or NULL on error.
 **/
vlc_tls_creds_t *vlc_tls_ClientCreate (vlc_object_t *obj)
{
    vlc_tls_creds_t *crd = vlc_custom_create (obj, sizeof (*crd),
                                              "tls client");
    if (unlikely(crd == NULL))
        return NULL;

    crd->module = vlc_module_load (crd, "tls client", NULL, false,
                                   tls_client_load, crd);
    if (crd->module == NULL)
    {
        msg_Err (crd, "TLS client plugin not available");
        vlc_object_release (crd);
        return NULL;
    }

    return crd;
}

/**
 * Releases data allocated with vlc_tls_ClientCreate() or
 * vlc_tls_ServerCreate().
 * @param srv TLS server object to be destroyed, or NULL
 */
void vlc_tls_Delete (vlc_tls_creds_t *crd)
{
    if (crd == NULL)
        return;

    vlc_module_unload (crd->module, tls_unload, crd);
    vlc_object_release (crd);
}


/**
 * Adds one or more certificate authorities from a file.
 * @return -1 on error, 0 on success.
 */
int vlc_tls_ServerAddCA (vlc_tls_creds_t *srv, const char *path)
{
    return srv->add_CA (srv, path);
}


/**
 * Adds one or more certificate revocation list from a file.
 * @return -1 on error, 0 on success.
 */
int vlc_tls_ServerAddCRL (vlc_tls_creds_t *srv, const char *path)
{
    return srv->add_CRL (srv, path);
}


/*** TLS  session ***/

vlc_tls_t *vlc_tls_SessionCreate (vlc_tls_creds_t *crd, int fd,
                                  const char *host)
{
    vlc_tls_t *session = vlc_custom_create (crd, sizeof (*session),
                                            "tls session");
    int val = crd->open (crd, session, fd, host);
    if (val == VLC_SUCCESS)
        return session;
    vlc_object_release (session);
    return NULL;
}

void vlc_tls_SessionDelete (vlc_tls_t *session)
{
    vlc_tls_creds_t *crd = (vlc_tls_creds_t *)(session->p_parent);

    crd->close (crd, session);
    vlc_object_release (session);
}

int vlc_tls_SessionHandshake (vlc_tls_t *session, const char *host,
                              const char *service)
{
    return session->handshake (session, host, service);
}

/**
 * Performs client side of TLS handshake through a connected socket, and
 * establishes a secure channel. This is a blocking network operation.
 *
 * @param fd socket through which to establish the secure channel
 * @param hostname expected server name, used both as Server Name Indication
 *                 and as expected Common Name of the peer certificate
 *
 * @return NULL on error.
 **/
vlc_tls_t *vlc_tls_ClientSessionCreate (vlc_tls_creds_t *crd, int fd,
                                        const char *host, const char *service)
{
    vlc_tls_t *session = vlc_tls_SessionCreate (crd, fd, host);
    if (session == NULL)
        return NULL;

    mtime_t deadline = mdate ();
    deadline += var_InheritInteger (crd, "ipv4-timeout") * 1000;

    struct pollfd ufd[1];
    ufd[0].fd = fd;

    int val;
    while ((val = vlc_tls_SessionHandshake (session, host, service)) > 0)
    {
        mtime_t now = mdate ();
        if (now > deadline)
           now = deadline;

        assert (val <= 2);
        ufd[0] .events = (val == 1) ? POLLIN : POLLOUT;

        if (poll (ufd, 1, (deadline - now) / 1000) == 0)
        {
            msg_Err (session, "TLS client session handshake timeout");
            val = -1;
            break;
        }
    }

    if (val != 0)
    {
        msg_Err (session, "TLS client session handshake error");
        vlc_tls_SessionDelete (session);
        session = NULL;
    }
    return session;
}
