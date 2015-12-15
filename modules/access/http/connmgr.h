/*****************************************************************************
 * connmgr.h: HTTP/TLS VLC connection manager declaration
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

struct vlc_http_mgr;
struct vlc_http_msg;

struct vlc_http_msg *vlc_https_request(struct vlc_http_mgr *mgr,
                                       const char *host, unsigned port,
                                       const struct vlc_http_msg *req);

struct vlc_http_mgr *vlc_http_mgr_create(vlc_object_t *obj);
void vlc_http_mgr_destroy(struct vlc_http_mgr *mgr);
