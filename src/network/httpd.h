/*****************************************************************************
 * httpd.h: builtin HTTP/RTSP server internals.
 *****************************************************************************
 * Copyright (C) 2004-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef _LIBVLC_HTTPD_H
#define _LIBVLC_HTTPD_H 1

struct httpd_t
{
    VLC_COMMON_MEMBERS

    int          i_host;
    httpd_host_t **host;
};


/* each host run in his own thread */
struct httpd_host_t
{
    VLC_COMMON_MEMBERS

    httpd_t     *httpd;

    /* ref count */
    int         i_ref;

    /* address/port and socket for listening at connections */
    char        *psz_hostname;
    int         i_port;
    int         *fd;

    /* Statistics */
    counter_t *p_active_counter;
    counter_t *p_total_counter;

    vlc_mutex_t lock;

    /* all registered url (becarefull that 2 httpd_url_t could point at the same url)
     * This will slow down the url research but make my live easier
     * All url will have their cb trigger, but only the first one can answer
     * */
    int         i_url;
    httpd_url_t **url;

    int            i_client;
    httpd_client_t **client;

    /* TLS data */
    tls_server_t *p_tls;
};
#endif /* _LIBVLC_HTTPD_H */
