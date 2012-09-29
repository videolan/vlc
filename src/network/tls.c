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

#include <vlc_common.h>
#include "libvlc.h"

#include <vlc_tls.h>
#include <vlc_modules.h>

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
    vlc_tls_creds_t *srv = vlc_custom_create (obj, sizeof (*srv), "tls creds");
    if (unlikely(srv == NULL))
        return NULL;

    var_Create (srv, "tls-x509-cert", VLC_VAR_STRING);
    var_Create (srv, "tls-x509-key", VLC_VAR_STRING);

    if (cert_path != NULL)
    {
        var_SetString (srv, "tls-x509-cert", cert_path);

        if (key_path == NULL)
            key_path = cert_path;
        var_SetString (srv, "tls-x509-key", key_path);
    }

    srv->module = module_need (srv, "tls server", NULL, false );
    if (srv->module == NULL)
    {
        msg_Err (srv, "TLS server plugin not available");
        vlc_object_release (srv);
        return NULL;
    }

    msg_Dbg (srv, "TLS server plugin initialized");
    return srv;
}


/**
 * Releases data allocated with vlc_tls_ServerCreate().
 * @param srv TLS server object to be destroyed, or NULL
 */
void vlc_tls_ServerDelete (vlc_tls_creds_t *srv)
{
    if (srv == NULL)
        return;

    module_unneed (srv, srv->module);
    vlc_object_release (srv);
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


vlc_tls_t *vlc_tls_ServerSessionCreate (vlc_tls_creds_t *crd, int fd)
{
    vlc_tls_t *session = vlc_custom_create (crd, sizeof (*session),
                                            "tls server");
    int val = crd->open (crd, session, fd);
    if (val == VLC_SUCCESS)
        return session;
    vlc_object_release (session);
    return NULL;
}


void vlc_tls_ServerSessionDelete (vlc_tls_t *session)
{
    vlc_tls_creds_t *crd = (vlc_tls_creds_t *)(session->p_parent);

    crd->close (crd, session);
    vlc_object_release (session);
}


int vlc_tls_ServerSessionHandshake (vlc_tls_t *ses)
{
    int val = ses->handshake (ses);
    if (val < 0)
        vlc_tls_ServerSessionDelete (ses);
    return val;
}


/*** TLS client session ***/
/* TODO: cache certificates for the whole VLC instance lifetime */

static int tls_client_start(void *func, va_list ap)
{
    int (*activate) (vlc_tls_t *, int fd, const char *hostname) = func;
    vlc_tls_t *session = va_arg (ap, vlc_tls_t *);
    int fd = va_arg (ap, int);
    const char *hostname = va_arg (ap, const char *);

    return activate (session, fd, hostname);
}

static void tls_client_stop(void *func, va_list ap)
{
    void (*deactivate) (vlc_tls_t *) = func;
    vlc_tls_t *session = va_arg (ap, vlc_tls_t *);

    deactivate (session);
}

/**
 * Allocates a client's TLS credentials and shakes hands through the network.
 * This is a blocking network operation.
 *
 * @param fd stream socket through which to establish the secure communication
 * layer.
 * @param psz_hostname Server Name Indication to pass to the server, or NULL.
 *
 * @return NULL on error.
 **/
vlc_tls_t *
vlc_tls_ClientCreate (vlc_object_t *obj, int fd, const char *hostname)
{
    vlc_tls_t *cl = vlc_custom_create (obj, sizeof (*cl), "tls client");
    if (unlikely(cl == NULL))
        return NULL;

    cl->u.module = vlc_module_load (cl, "tls client", NULL, false,
                                    tls_client_start, cl, fd, hostname);
    if (cl->u.module == NULL)
    {
        msg_Err (cl, "TLS client plugin not available");
        vlc_object_release (cl);
        return NULL;
    }

    /* TODO: do this directly in the TLS plugin */
    int val;
    do
        val = cl->handshake (cl);
    while (val > 0);

    if (val != 0)
    {
        msg_Err (cl, "TLS client session handshake error");
        vlc_module_unload (cl->u.module, tls_client_stop, cl);
        vlc_object_release (cl);
        return NULL;
    }
    msg_Dbg (cl, "TLS client session initialized");
    return cl;
}


/**
 * Releases data allocated with vlc_tls_ClientCreate().
 * It is your job to close the underlying socket.
 */
void vlc_tls_ClientDelete (vlc_tls_t *cl)
{
    if (cl == NULL)
        return;

    vlc_module_unload (cl->u.module, tls_client_stop, cl);
    vlc_object_release (cl);
}
