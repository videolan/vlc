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
struct vlc_tls_creds;

ssize_t vlc_http_recv(int fd, void *buf, size_t len);

/**
 * Receives TLS data.
 *
 * Receives bytes from the peer through a TLS session.
 * @note This may be a cancellation point.
 * The caller is responsible for serializing reads on a given connection.
 */
ssize_t vlc_https_recv(struct vlc_tls *tls, void *buf, size_t len);

ssize_t vlc_http_send(int fd, const void *buf, size_t len);

/**
 * Sends bytes to a connection.
 * @note This may be a cancellation point.
 * The caller is responsible for serializing writes on a given connection.
 */
ssize_t vlc_https_send(struct vlc_tls *tls, const void *buf, size_t len);

struct vlc_tls *vlc_https_connect(struct vlc_tls_creds *creds,
                                  const char *name, unsigned port,
                                  bool *restrict two);
void vlc_http_disconnect(int fd);
void vlc_https_disconnect(struct vlc_tls *tls);

#endif
