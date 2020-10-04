/*****************************************************************************
 * resource.c: HTTP resource common code
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

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include "message.h"
#include "connmgr.h"
#include "outfile.h"

struct vlc_http_outfile *vlc_http_outfile_create(struct vlc_http_mgr *mgr,
    const char *uri, const char *ua, const char *user, const char *pwd)
{
    struct vlc_http_msg *resp = NULL;
    vlc_url_t url;
    bool secure;

    if (vlc_UrlParse(&url, uri))
        goto error;
    if (url.psz_protocol == NULL || url.psz_host == NULL)
    {
        errno = EINVAL;
        goto error;
    }

    if (!vlc_ascii_strcasecmp(url.psz_protocol, "https"))
        secure = true;
    else if (!vlc_ascii_strcasecmp(url.psz_protocol, "http"))
        secure = false;
    else
    {
        errno = ENOTSUP;
        goto error;
    }

    char *authority = vlc_http_authority(url.psz_host, url.i_port);
    if (unlikely(authority == NULL))
        goto error;

    struct vlc_http_msg *req = vlc_http_req_create("PUT", url.psz_protocol,
                                                   authority, url.psz_path);
    free(authority);
    if (unlikely(req == NULL))
        goto error;

    vlc_http_msg_add_header(req, "Expect", "100-continue");

    if (user != NULL && pwd != NULL)
        vlc_http_msg_add_creds_basic(req, false, user, pwd);
    if (ua != NULL)
        vlc_http_msg_add_agent(req, ua);

    vlc_http_msg_add_cookies(req, vlc_http_mgr_get_jar(mgr));

    resp = vlc_http_mgr_request(mgr, secure, url.psz_host, url.i_port, req,
                                false, true);
    vlc_http_msg_destroy(req);
    if (resp == NULL)
        goto error;

    int status = vlc_http_msg_get_status(resp);

    /* FIXME: check that HTTP version >= 1.1 */

    if (status < 100 || status >= 200)
    {
        vlc_http_msg_destroy(resp);
        resp = NULL;
    }

error:
    vlc_UrlClean(&url);
    return (struct vlc_http_outfile *)resp;
}

ssize_t vlc_http_outfile_write(struct vlc_http_outfile *f, block_t *b)
{
    struct vlc_http_msg *msg = (struct vlc_http_msg *)f;

    return vlc_http_msg_write(msg, b, false);
}

int vlc_http_outfile_close(struct vlc_http_outfile *f)
{
    struct vlc_http_msg *msg = (struct vlc_http_msg *)f;
    int ret = vlc_http_msg_write(msg, NULL, true);

    if (ret < 0)
    {
        vlc_http_msg_destroy(msg);
        return -1;
    }

    msg = vlc_http_msg_iterate(msg);
    if (msg == NULL)
        return -1;

    int status = vlc_http_msg_get_status(msg);

    /* TODO: store cookies? */
    vlc_http_msg_destroy(msg);

    return (status >= 200 && status < 300) ? 0 : -1;
}
