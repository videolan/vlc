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

struct vlc_http_live
{
    struct vlc_http_resource resource;
    struct vlc_http_msg *resp;
    bool failed;
};

static int vlc_http_live_req(struct vlc_http_msg *req,
                             const struct vlc_http_resource *res, void *opaque)
{
    vlc_http_msg_add_header(req, "Accept-Encoding", "gzip, deflate");
    (void) res;
    (void) opaque;
    return 0;
}

static struct vlc_http_msg *vlc_http_live_open(struct vlc_http_live *live)
{
    return vlc_http_res_open(&live->resource, vlc_http_live_req, NULL);
}

void vlc_http_live_destroy(struct vlc_http_live *live)
{
    if (live->resp != NULL)
        vlc_http_msg_destroy(live->resp);
    vlc_http_res_deinit(&live->resource);
    free(live);
}

struct vlc_http_live *vlc_http_live_create(struct vlc_http_mgr *mgr,
                                           const char *uri, const char *ua,
                                           const char *ref)
{
    struct vlc_http_live *live = malloc(sizeof (*live));
    if (unlikely(live == NULL))
        return NULL;

    if (vlc_http_res_init(&live->resource, mgr, uri, ua, ref))
    {
        free(live);
        return NULL;
    }

    live->resp = NULL;
    live->failed = false;
    return live;
}

int vlc_http_live_get_status(struct vlc_http_live *live)
{
    if (live->resp == NULL)
    {
        if (live->failed)
            return -1;
        live->resp = vlc_http_live_open(live);
        if (live->resp == NULL)
        {
            live->failed = true;
            return -1;
        }
    }
    return vlc_http_msg_get_status(live->resp);
}

char *vlc_http_live_get_redirect(struct vlc_http_live *live)
{
    if (vlc_http_live_get_status(live) < 0)
        return NULL;
    return vlc_http_res_get_redirect(&live->resource, live->resp);
}

char *vlc_http_live_get_type(struct vlc_http_live *live)
{
    if (vlc_http_live_get_status(live) < 0)
        return NULL;
    return vlc_http_res_get_type(live->resp);
}

block_t *vlc_http_live_read(struct vlc_http_live *live)
{
    if (vlc_http_live_get_status(live) < 0)
        return NULL;

    struct block_t *block = vlc_http_res_read(live->resp);
    if (block != NULL)
        return block;

    /* Automatically try to reconnect */
    /* TODO: Retry-After parsing, loop and pacing timer */
    vlc_http_msg_destroy(live->resp);
    live->resp = NULL;
    if (vlc_http_live_get_status(live) < 0)
        return NULL;
    return vlc_http_res_read(live->resp);
}
