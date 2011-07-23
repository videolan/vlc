/*****************************************************************************
 * tls.c
 *****************************************************************************
 * Copyright © 2004-2007 Rémi Denis-Courmont
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
tls_server_t *
tls_ServerCreate (vlc_object_t *obj, const char *cert_path,
                  const char *key_path)
{
    tls_server_t *srv;

    srv = (tls_server_t *)vlc_custom_create (obj, sizeof (*srv), "tls server");
    if (srv == NULL)
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

    srv->p_module = module_need (srv, "tls server", NULL, false );
    if (srv->p_module == NULL)
    {
        msg_Err (srv, "TLS server plugin not available");
        vlc_object_release (srv);
        return NULL;
    }

    msg_Dbg (srv, "TLS server plugin initialized");
    return srv;
}


/**
 * Releases data allocated with tls_ServerCreate.
 * @param srv TLS server object to be destroyed, or NULL
 */
void tls_ServerDelete (tls_server_t *srv)
{
    if (srv == NULL)
        return;

    module_unneed (srv, srv->p_module);
    vlc_object_release (srv);
}


/**
 * Adds one or more certificate authorities from a file.
 * @return -1 on error, 0 on success.
 */
int tls_ServerAddCA (tls_server_t *srv, const char *path)
{
    return srv->pf_add_CA (srv, path);
}


/**
 * Adds one or more certificate revocation list from a file.
 * @return -1 on error, 0 on success.
 */
int tls_ServerAddCRL (tls_server_t *srv, const char *path)
{
    return srv->pf_add_CRL (srv, path);
}


tls_session_t *tls_ServerSessionCreate (tls_server_t *srv, int fd)
{
    tls_session_t *ses = srv->pf_open (srv);
    if (ses != NULL)
        ses->pf_set_fd (ses, fd);
    return ses;
}


void tls_ServerSessionDelete (tls_session_t *ses)
{
    tls_server_t *srv = (tls_server_t *)(ses->p_parent);
    srv->pf_close (srv, ses);
}


int tls_ServerSessionHandshake (tls_session_t *ses)
{
    int val = ses->pf_handshake (ses);
    if (val < 0)
        tls_ServerSessionDelete (ses);
    return val;
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
tls_session_t *
tls_ClientCreate (vlc_object_t *obj, int fd, const char *psz_hostname)
{
    tls_session_t *cl;
    int val;

    cl = (tls_session_t *)vlc_custom_create (obj, sizeof (*cl), "tls client");
    if (cl == NULL)
        return NULL;

    var_Create (cl, "tls-server-name", VLC_VAR_STRING);
    if (psz_hostname != NULL)
    {
        msg_Dbg (cl, "requested server name: %s", psz_hostname);
        var_SetString (cl, "tls-server-name", psz_hostname);
    }
    else
        msg_Dbg (cl, "requested anonymous server");

    cl->p_module = module_need (cl, "tls client", NULL, false );
    if (cl->p_module == NULL)
    {
        msg_Err (cl, "TLS client plugin not available");
        vlc_object_release (cl);
        return NULL;
    }

    cl->pf_set_fd (cl, fd);

    do
        val = cl->pf_handshake (cl);
    while (val > 0);

    if (val == 0)
    {
        msg_Dbg (cl, "TLS client session initialized");
        return cl;
    }
    msg_Err (cl, "TLS client session handshake error");

    module_unneed (cl, cl->p_module);
    vlc_object_release (cl);
    return NULL;
}


/**
 * Releases data allocated with tls_ClientCreate.
 * It is your job to close the underlying socket.
 */
void tls_ClientDelete (tls_session_t *cl)
{
    if (cl == NULL)
        return;

    module_unneed (cl, cl->p_module);
    vlc_object_release (cl);
}
