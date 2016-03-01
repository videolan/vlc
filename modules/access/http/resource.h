/*****************************************************************************
 * resource.h: HTTP resource common code
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
 * \defgroup http_res Resources
 * Remote HTTP resources identified by a URL
 * \ingroup http
 * @{
 */

struct vlc_http_msg;
struct vlc_http_mgr;

struct vlc_http_resource
{
    struct vlc_http_mgr *manager;
    char *host;
    unsigned port;
    bool secure;
    bool negotiate;
    char *authority;
    char *path;
    char *agent;
    char *referrer;
};

int vlc_http_res_init(struct vlc_http_resource *, struct vlc_http_mgr *mgr,
                      const char *uri,const char *ua, const char *ref);
void vlc_http_res_deinit(struct vlc_http_resource *);

struct vlc_http_msg *vlc_http_res_open(struct vlc_http_resource *res,
    int (*cb)(struct vlc_http_msg *req, const struct vlc_http_resource *,
              void *), void *);
char *vlc_http_res_get_redirect(const struct vlc_http_resource *,
                                const struct vlc_http_msg *resp);
char *vlc_http_res_get_type(const struct vlc_http_msg *resp);
struct block_t *vlc_http_res_read(struct vlc_http_msg *resp);

/** @} */
