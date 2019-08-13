/*****************************************************************************
 * transport.h: HTTP/TLS TCP transport layer declarations
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

#ifndef VLC_HTTP_TRANSPORT_H
#define VLC_HTTP_TRANSPORT_H 1

#include <stddef.h>
#include <stdbool.h>

struct vlc_tls;
struct vlc_tls_client;

struct vlc_tls *vlc_https_connect(struct vlc_tls_client *creds,
                                  const char *name, unsigned port,
                                  bool *restrict two);
struct vlc_tls *vlc_https_connect_proxy(void *ctx,
                                        struct vlc_tls_client *creds,
                                        const char *name, unsigned port,
                                        bool *restrict two, const char *proxy);
bool vlc_http_port_blocked(unsigned port);

#endif
