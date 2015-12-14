/*****************************************************************************
 * h2conn.h: HTTP/2 connection handling
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
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

struct vlc_tls;
struct vlc_http_msg;

struct vlc_h2_conn;

struct vlc_h2_conn *vlc_h2_conn_create(struct vlc_tls *tls);
void vlc_h2_conn_release(struct vlc_h2_conn *conn);

struct vlc_http_stream *vlc_h2_stream_open(struct vlc_h2_conn *conn,
                                           const struct vlc_http_msg *msg);
