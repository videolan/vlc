/*****************************************************************************
 * live.h: HTTP read-only live stream declarations
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

/**
 * \defgroup http_live Live streams
 * Trivial HTTP-based live streams
 * \ingroup http_res
 * @{
 */

struct vlc_http_resource;
struct block_t;

struct vlc_http_resource *vlc_http_live_create(struct vlc_http_mgr *mgr,
                                               const char *uri, const char *ua,
                                               const char *ref);
struct block_t *vlc_http_live_read(struct vlc_http_resource *);

#define vlc_http_live_get_status vlc_http_res_get_status
#define vlc_http_live_get_redirect vlc_http_res_get_redirect
#define vlc_http_live_get_type vlc_http_res_get_type
#define vlc_http_live_destroye vlc_http_res_destroy

/** @} */
