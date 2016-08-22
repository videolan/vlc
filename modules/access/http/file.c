/*****************************************************************************
 * file.c: HTTP read-only file
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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_strings.h>
#include "message.h"
#include "resource.h"
#include "file.h"

#pragma GCC visibility push(default)

struct vlc_http_file
{
    struct vlc_http_resource resource;
    uintmax_t offset;
};

static int vlc_http_file_req(const struct vlc_http_resource *res,
                             struct vlc_http_msg *req, void *opaque)
{
    struct vlc_http_file *file = (struct vlc_http_file *)res;
    const uintmax_t *offset = opaque;

    if (file->resource.response != NULL)
    {
        const char *str = vlc_http_msg_get_header(file->resource.response,
                                                  "ETag");
        if (str != NULL)
        {
            if (!memcmp(str, "W/", 2))
                str += 2; /* skip weak mark */
            vlc_http_msg_add_header(req, "If-Match", "%s", str);
        }
        else
        {
            time_t mtime = vlc_http_msg_get_mtime(file->resource.response);
            if (mtime != -1)
                vlc_http_msg_add_time(req, "If-Unmodified-Since", &mtime);
        }
    }

    if (vlc_http_msg_add_header(req, "Range", "bytes=%ju-", *offset)
     && *offset != 0)
        return -1;
    return 0;
}

static int vlc_http_file_resp(const struct vlc_http_resource *res,
                              const struct vlc_http_msg *resp, void *opaque)
{
    const uintmax_t *offset = opaque;

    if (vlc_http_msg_get_status(resp) == 206)
    {
        const char *str = vlc_http_msg_get_header(resp, "Content-Range");
        if (str == NULL)
            /* A multipart/byteranges response. This is not what we asked for
             * and we do not support it. */
            goto fail;

        uintmax_t start, end;
        if (sscanf(str, "bytes %ju-%ju", &start, &end) != 2
         || start != *offset || start > end)
            /* A single range response is what we asked for, but not at that
             * start offset. */
            goto fail;
    }

    (void) res;
    return 0;

fail:
    errno = EIO;
    return -1;
}

static const struct vlc_http_resource_cbs vlc_http_file_callbacks =
{
    vlc_http_file_req,
    vlc_http_file_resp,
};

struct vlc_http_resource *vlc_http_file_create(struct vlc_http_mgr *mgr,
                                               const char *uri, const char *ua,
                                               const char *ref)
{
    struct vlc_http_file *file = malloc(sizeof (*file));
    if (unlikely(file == NULL))
        return NULL;

    if (vlc_http_res_init(&file->resource, &vlc_http_file_callbacks, mgr,
                          uri, ua, ref))
    {
        free(file);
        return NULL;
    }

    file->offset = 0;
    return &file->resource;
}

static uintmax_t vlc_http_msg_get_file_size(const struct vlc_http_msg *resp)
{
    int status = vlc_http_msg_get_status(resp);
    const char *range = vlc_http_msg_get_header(resp, "Content-Range");

    if (status == 206 /* Partial Content */)
    {   /* IETF RFC7233 §4.1 */
        assert(range != NULL); /* checked by vlc_http_file_resp() */

        uintmax_t end, total;

        switch (sscanf(range, "bytes %*u-%ju/%ju", &end, &total))
        {
            case 1:
                if (unlikely(end == UINTMAX_MAX))
                    return -1; /* avoid wrapping to zero */
                return end + 1;
            case 2:
                return total;
        }
        vlc_assert_unreachable(); /* checked by vlc_http_file_resp() */
    }

    if (status == 416 /* Range Not Satisfiable */)
    {   /* IETF RFC7233 §4.4 */
        uintmax_t total;

        if (range == NULL)
            return -1; /* valid but helpless response */

        if (sscanf(range, "bytes */%ju", &total) == 1)
            return total; /* this occurs when seeking beyond EOF */
    }

    return -1;
}

static bool vlc_http_msg_can_seek(const struct vlc_http_msg *resp)
{
    int status = vlc_http_msg_get_status(resp);
    if (status == 206 || status == 416)
        return true; /* Partial Content */

    return vlc_http_msg_get_token(resp, "Accept-Ranges", "bytes") != NULL;
}

uintmax_t vlc_http_file_get_size(struct vlc_http_resource *res)
{
    int status = vlc_http_res_get_status(res);
    if (status < 0)
        return -1;

    uintmax_t ret = vlc_http_msg_get_file_size(res->response);
    if (ret != (uintmax_t)-1)
        return ret;

    if (status >= 300 || status == 201)
        return -1; /* Error or redirection, size is unknown/irrelevant. */

    /* Content-Range is meaningless here (see RFC7233 B), so check if the size
     * of the response entity body is known. */
    return vlc_http_msg_get_size(res->response);
}

bool vlc_http_file_can_seek(struct vlc_http_resource *res)
{   /* See IETF RFC7233 */
    int status = vlc_http_res_get_status(res);
    if (status < 0)
        return false;
    return vlc_http_msg_can_seek(res->response);
}

int vlc_http_file_seek(struct vlc_http_resource *res, uintmax_t offset)
{
    struct vlc_http_msg *resp = vlc_http_res_open(res, &offset);
    if (resp == NULL)
        return -1;

    struct vlc_http_file *file = (struct vlc_http_file *)res;

    int status = vlc_http_msg_get_status(resp);
    if (res->response != NULL)
    {   /* Accept the new and ditch the old one if:
         * - requested succeeded and range was accepted (206),
         * - requested failed due to out-of-range (416),
         * - request succeeded and seek offset is zero (2xx).
         */
        if (status != 206 && status != 416 && (offset != 0 || status >= 300))
        {
            vlc_http_msg_destroy(resp);
            return -1;
        }
        vlc_http_msg_destroy(res->response);
    }

    res->response = resp;
    file->offset = offset;
    return 0;
}

block_t *vlc_http_file_read(struct vlc_http_resource *res)
{
    struct vlc_http_file *file = (struct vlc_http_file *)res;
    block_t *block = vlc_http_res_read(res);

    if (block == vlc_http_error)
    {   /* Automatically reconnect on error if server supports seek */
        if (res->response != NULL
         && vlc_http_msg_can_seek(res->response)
         && file->offset < vlc_http_msg_get_file_size(res->response)
         && vlc_http_file_seek(res, file->offset) == 0)
            block = vlc_http_res_read(res);

        if (block == vlc_http_error)
            return NULL;
    }

    if (block == NULL)
        return NULL; /* End of stream */

    file->offset += block->i_buffer;
    return block;
}
