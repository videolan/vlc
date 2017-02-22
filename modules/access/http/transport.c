/*****************************************************************************
 * transport.c: HTTP/TLS TCP transport layer
 *****************************************************************************
 * Copyright © 2015 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <vlc_common.h>
#include <vlc_tls.h>

#include "transport.h"

vlc_tls_t *vlc_https_connect(vlc_tls_creds_t *creds, const char *name,
                             unsigned port, bool *restrict two)
{
    if (port == 0)
        port = 443;

    /* TODO: implement fast open. This requires merging vlc_tls_SocketOpenTCP()
     * and vlc_tls_ClientSessionCreate() though. */
    vlc_tls_t *sock = vlc_tls_SocketOpenTCP(creds->obj.parent, name, port);
    if (sock == NULL)
        return NULL;

    /* TLS with ALPN */
    const char *alpn[] = { "h2", "http/1.1", NULL };
    char *alp;

    vlc_tls_t *tls = vlc_tls_ClientSessionCreate(creds, sock, name, "https",
                                                 alpn + !*two, &alp);
    if (tls == NULL)
    {
        vlc_tls_Close(sock);
        return NULL;
    }

    *two = (alp != NULL) && !strcmp(alp, "h2");
    free(alp);
    return tls;
}
