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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include "message.h"
#include "connmgr.h"
#include "file.h"

#pragma GCC visibility push(default)

struct vlc_http_file
{
    struct vlc_http_mgr *manager;
    struct vlc_http_msg *resp;
    char *host;
    unsigned port;
    char *authority;
    char *path;
    char *agent;
    char *referrer;

    uintmax_t offset;
};

static struct vlc_http_msg *vlc_http_file_req(const struct vlc_http_file *file,
                                              uintmax_t offset)
{
    struct vlc_http_msg *req;
    const char *str;

    req = vlc_http_req_create("GET", "https", file->authority, file->path);
    if (unlikely(req == NULL))
        return NULL;

    /* Content negotiation */
    vlc_http_msg_add_header(req, "Accept", "*/*");

    /* NOTE (TODO): Accept-Encoding should be used to allow compression.
     * Unforunately, some servers do not send byte ranges when compression
     * is enabled - so a separate HEAD request would be required first. */

    const char *lang = vlc_gettext("C");
    if (strcmp(lang, "C"))
        vlc_http_msg_add_header(req, "Accept-Language",
                                "%s, *;q=0.5", lang);

    /* Authentication */
    /* TODO: authentication */

    /* Request context */
    if (file->agent != NULL)
        vlc_http_msg_add_agent(req, file->agent);

    if (file->referrer != NULL) /* TODO: validate URL */
        vlc_http_msg_add_header(req, "Referer", "%s", file->referrer);

    if (file->resp != NULL)
    {
        str = vlc_http_msg_get_header(file->resp, "ETag");
        if (str != NULL)
        {
            if (!memcmp(str, "W/", 2))
                str += 2; /* skip weak mark */
            vlc_http_msg_add_header(req, "If-Match", "%s", str);
        }
        else
        {
            time_t mtime = vlc_http_msg_get_mtime(file->resp);
            if (mtime != -1)
                vlc_http_msg_add_time(req, "If-Unmodified-Since", &mtime);
        }
    }

    if (vlc_http_msg_add_header(req, "Range", "bytes=%ju-", offset)
     && offset != 0)
        goto error;
    /* TODO: vlc_http_msg_add_header(req, "TE", "deflate, gzip");*/
    /* TODO: Cookies */
    return req;
error:
    vlc_http_msg_destroy(req);
    return NULL;
}

static struct vlc_http_msg *vlc_http_file_open(struct vlc_http_file *file,
                                               uintmax_t offset)
{
    struct vlc_http_msg *req = vlc_http_file_req(file, offset);
    if (unlikely(req == NULL))
        return NULL;

    struct vlc_http_msg *resp = vlc_https_request(file->manager, file->host,
                                                  file->port, req);
    vlc_http_msg_destroy(req);

    resp = vlc_http_msg_get_final(resp);
    if (resp == NULL)
        return NULL;

    int status = vlc_http_msg_get_status(resp);
    if (status < 200 || status >= 599)
    {
        vlc_http_msg_destroy(resp);
        resp = NULL;
    }
    return resp;
}

void vlc_http_file_destroy(struct vlc_http_file *file)
{
    if (file->resp != NULL)
        vlc_http_msg_destroy(file->resp);

    free(file->referrer);
    free(file->agent);
    free(file->path);
    free(file->authority);
    free(file->host);
    free(file);
}

static char *vlc_http_authority(const char *host, unsigned port)
{
    static const char *const formats[4] = { "%s", "[%s]", "%s:%u", "[%s]:%u" };
    const bool brackets = strchr(host, ':') != NULL;
    const char *fmt = formats[brackets + 2 * (port != 0)];
    char *authority;

    if (unlikely(asprintf(&authority, fmt, host, port) == -1))
        return NULL;
    return authority;
}

struct vlc_http_file *vlc_http_file_create(struct vlc_http_mgr *mgr,
                                           const char *uri, const char *ua,
                                           const char *ref)
{
    vlc_url_t url;

    vlc_UrlParse(&url, uri);
    if (url.psz_protocol == NULL
     || vlc_ascii_strcasecmp(url.psz_protocol, "https")
     || url.psz_host == NULL)
    {
        vlc_UrlClean(&url);
        return NULL;
    }

    struct vlc_http_file *file = malloc(sizeof (*file));
    if (unlikely(file == NULL))
    {
        vlc_UrlClean(&url);
        return NULL;
    }

    file->host = strdup(url.psz_host);
    file->port = url.i_port;
    file->authority = vlc_http_authority(url.psz_host, url.i_port);
    file->agent = (ua != NULL) ? strdup(ua) : NULL;
    file->referrer = (ref != NULL) ? strdup(ref) : NULL;

    const char *path = url.psz_path;
    if (path == NULL)
        path = "/";

    if (url.psz_option != NULL)
    {
        if (asprintf(&file->path, "%s?%s", path, url.psz_option) == -1)
            file->path = NULL;
    }
    else
        file->path = strdup(path);

    vlc_UrlClean(&url);
    file->manager = mgr;
    file->resp = NULL;
    file->offset = 0;

    if (unlikely(file->host == NULL || file->authority == NULL
              || file->path == NULL))
    {
        vlc_http_file_destroy(file);
        file = NULL;
    }
    return file;
}

int vlc_http_file_get_status(struct vlc_http_file *file)
{
    if (file->resp == NULL)
    {
        file->resp = vlc_http_file_open(file, file->offset);
        if (file->resp == NULL)
            return -1;
    }
    return vlc_http_msg_get_status(file->resp);
}

char *vlc_http_file_get_redirect(struct vlc_http_file *file)
{
    int status = vlc_http_file_get_status(file);

    /* TODO: if (status == 426 Upgrade Required) */

    /* Location header is only meaningful for 201 and 3xx */
    if (status != 201 && (status / 100) != 3)
        return NULL;
    if (status == 304 /* Not Modified */
     || status == 305 /* Use Proxy (deprecated) */
     || status == 306 /* Switch Proxy (former) */)
        return NULL;

    const char *location = vlc_http_msg_get_header(file->resp, "Location");
    if (location == NULL)
        return NULL;

    /* TODO: if status is 3xx, check for Retry-After and wait */

    /* NOTE: The anchor is discard if it is present as VLC does not support
     * HTML anchors so far. */
    size_t len = strcspn(location, "#");

    /* FIXME: resolve relative URL _correctly_ */
    if (location[0] == '/')
    {
        char *url;

        if (unlikely(asprintf(&url, "https://%s%.*s", file->authority,
                              (int)len, location)) < 0)
            return NULL;
        return url;
    }
    return strndup(location, len);
}

uintmax_t vlc_http_file_get_size(struct vlc_http_file *file)
{
    int status = vlc_http_file_get_status(file);
    if (status < 0)
        return -1;

    const char *range = vlc_http_msg_get_header(file->resp, "Content-Range");

    if (status == 206 /* Partial Content */)
    {   /* IETF RFC7233 §4.1 */
        if (range == NULL)
            return -1; /* invalid response */

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
        return -1;
    }

    if (status == 416 /* Range Not Satisfiable */)
    {   /* IETF RFC7233 §4.4 */
        uintmax_t total;

        if (range == NULL)
            return -1; /* valid but helpless response */

        if (sscanf(range, "bytes */%ju", &total) == 1)
            return total; /* this occurs when seeking beyond EOF */
    }

    if (status >= 300 || status == 201)
        return -1; /* Error or redirection, size is unknown/irrelevant. */

    /* Content-Range is meaningless here (see RFC7233 B), so check if the size
     * of the response entity body is known. */
    return vlc_http_msg_get_size(file->resp);
}

bool vlc_http_file_can_seek(struct vlc_http_file *file)
{   /* See IETF RFC7233 */
    int status = vlc_http_file_get_status(file);
    if (status < 0)
        return false;
    if (status == 206 || status == 416)
        return true; /* Partial Content */

    const char *str = vlc_http_msg_get_header(file->resp, "Accept-Ranges");
    /* FIXME: tokenize */
    if (str != NULL && !vlc_ascii_strcasecmp(str, "bytes"))
        return true;

    return false;
}

char *vlc_http_file_get_type(struct vlc_http_file *file)
{
    int status = vlc_http_file_get_status(file);
    if (status < 200 || status >= 300)
        return NULL;

    const char *type = vlc_http_msg_get_header(file->resp, "Content-Type");
    return (type != NULL) ? strdup(type) : NULL;
}

int vlc_http_file_seek(struct vlc_http_file *file, uintmax_t offset)
{
    struct vlc_http_msg *resp = vlc_http_file_open(file, offset);
    if (resp == NULL)
        return -1;

    int status = vlc_http_msg_get_status(resp);
    if (file->resp != NULL)
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
        vlc_http_msg_destroy(file->resp);
    }

    file->resp = resp;
    file->offset = offset;
    return 0;
}

block_t *vlc_http_file_read(struct vlc_http_file *file)
{
    int status = vlc_http_file_get_status(file);
    if (status < 200 || status >= 300)
        return NULL; /* do not "read" redirect or error message */

    block_t *block = vlc_http_msg_read(file->resp);
    if (block == NULL)
    {   /* Automatically reconnect if server supports seek */
        if (vlc_http_file_can_seek(file)
         && vlc_http_file_seek(file, file->offset) == 0)
            block = vlc_http_msg_read(file->resp);

        if (block == NULL)
            return NULL;
    }

    file->offset += block->i_buffer;
    return block;
}
