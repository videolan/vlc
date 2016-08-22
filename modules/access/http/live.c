/*****************************************************************************
 * live.c: HTTP read-only live stream
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>

#include <vlc_common.h>
#include "message.h"
#include "resource.h"
#include "live.h"

#pragma GCC visibility push(default)

static int vlc_http_live_req(const struct vlc_http_resource *res,
                             struct vlc_http_msg *req, void *opaque)
{
    vlc_http_msg_add_header(req, "Accept-Encoding", "gzip, deflate");
    (void) res;
    (void) opaque;
    return 0;
}

static int vlc_http_live_resp(const struct vlc_http_resource *res,
                              const struct vlc_http_msg *resp, void *opaque)
{
    (void) res;
    (void) resp;
    (void) opaque;
    return 0;
}

static const struct vlc_http_resource_cbs vlc_http_live_callbacks =
{
    vlc_http_live_req,
    vlc_http_live_resp,
};

struct vlc_http_resource *vlc_http_live_create(struct vlc_http_mgr *mgr,
                                               const char *uri, const char *ua,
                                               const char *ref)
{
    struct vlc_http_resource *res = malloc(sizeof (*res));
    if (unlikely(res == NULL))
        return NULL;

    if (vlc_http_res_init(res, &vlc_http_live_callbacks, mgr, uri, ua, ref))
    {
        free(res);
        res = NULL;
    }

    return res;
}

block_t *vlc_http_live_read(struct vlc_http_resource *res)
{
    struct block_t *block = vlc_http_res_read(res);
    if (block != NULL && block != vlc_http_error)
        return block;

    /* Automatically try to reconnect */
    /* TODO: Retry-After parsing, loop and pacing timer */
    vlc_http_msg_destroy(res->response);
    res->response = NULL;
    return vlc_http_res_read(res);
}
