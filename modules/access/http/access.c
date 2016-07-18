/*****************************************************************************
 * access.c: HTTP/TLS VLC access plug-in
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_plugin.h>
#include <vlc_url.h>

#include "connmgr.h"
#include "resource.h"
#include "file.h"
#include "live.h"

struct access_sys_t
{
    struct vlc_http_mgr *manager;
    struct vlc_http_resource *resource;
};

static block_t *FileRead(access_t *access, bool *restrict eof)
{
    access_sys_t *sys = access->p_sys;

    block_t *b = vlc_http_file_read(sys->resource);
    if (b == NULL)
        *eof = true;
    return b;
}

static int FileSeek(access_t *access, uint64_t pos)
{
    access_sys_t *sys = access->p_sys;

    if (vlc_http_file_seek(sys->resource, pos))
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int FileControl(access_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;

    switch (query)
    {
        case STREAM_CAN_SEEK:
            *va_arg(args, bool *) = vlc_http_file_can_seek(sys->resource);
            break;

        case STREAM_CAN_FASTSEEK:
            *va_arg(args, bool *) = false;
            break;

        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = true;
            break;

        case STREAM_GET_SIZE:
        {
            uintmax_t val = vlc_http_file_get_size(sys->resource);
            if (val >= UINT64_MAX)
                return VLC_EGENERIC;

            *va_arg(args, uint64_t *) = val;
            break;
        }

        case STREAM_GET_PTS_DELAY:
            *va_arg(args, int64_t *) = INT64_C(1000) *
                var_InheritInteger(access, "network-caching");
            break;

        case STREAM_GET_CONTENT_TYPE:
            *va_arg(args, char **) = vlc_http_file_get_type(sys->resource);
            break;

        case STREAM_SET_PAUSE_STATE:
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static block_t *LiveRead(access_t *access, bool *restrict eof)
{
    access_sys_t *sys = access->p_sys;

    block_t *b = vlc_http_live_read(sys->resource);
    if (b == NULL) /* TODO: loop instead of EOF, see vlc_http_live_read() */
        *eof = true;
    return b;
}

static int NoSeek(access_t *access, uint64_t pos)
{
    (void) access;
    (void) pos;
    return VLC_EGENERIC;
}

static int LiveControl(access_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;

    switch (query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = false;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg(args, int64_t *) = INT64_C(1000) *
                var_InheritInteger(access, "network-caching");
            break;

        case STREAM_GET_CONTENT_TYPE:
            *va_arg(args, char **) = vlc_http_live_get_type(sys->resource);
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = malloc(sizeof (*sys));
    int ret = VLC_ENOMEM;

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->manager = NULL;
    sys->resource = NULL;

    void *jar = NULL;
    if (var_InheritBool(obj, "http-forward-cookies"))
        jar = var_InheritAddress(obj, "http-cookies");

    bool h2c = var_InheritBool(obj, "http2");

    sys->manager = vlc_http_mgr_create(obj, jar, h2c);
    if (sys->manager == NULL)
        goto error;

    char *ua = var_InheritString(obj, "http-user-agent");
    char *referer = var_InheritString(obj, "http-referrer");
    bool live = var_InheritBool(obj, "http-continuous");

    if (live)
        sys->resource = vlc_http_live_create(sys->manager, access->psz_url, ua,
                                             referer);
    else
        sys->resource = vlc_http_file_create(sys->manager, access->psz_url, ua,
                                             referer);
    free(referer);
    free(ua);

    if (sys->resource == NULL)
        goto error;

    char *redir = vlc_http_res_get_redirect(sys->resource);
    if (redir != NULL)
    {
        char *fixed = vlc_uri_fixup(redir);
        if (likely(fixed != NULL))
        {
            free(redir);
            redir = fixed;
        }

        access->psz_url = vlc_uri_resolve(access->psz_url, redir);
        free(redir);
        ret = VLC_ACCESS_REDIRECT;
        goto error;
    }

    ret = VLC_EGENERIC;

    int status = vlc_http_res_get_status(sys->resource);
    if (status < 0)
    {
        msg_Err(access, "HTTP connection failure");
        goto error;
    }
    if (status == 401) /* authentication */
        goto error; /* FIXME not implemented yet */
    if (status >= 300)
    {
        msg_Err(access, "HTTP %d error", status);
        goto error;
    }

    access->pf_read = NULL;
    if (live)
    {
        access->pf_block = LiveRead;
        access->pf_seek = NoSeek;
        access->pf_control = LiveControl;
    }
    else
    {
        access->pf_block = FileRead;
        access->pf_seek = FileSeek;
        access->pf_control = FileControl;
    }
    access->p_sys = sys;
    return VLC_SUCCESS;

error:
    if (sys->resource != NULL)
        vlc_http_res_destroy(sys->resource);
    if (sys->manager != NULL)
        vlc_http_mgr_destroy(sys->manager);
    free(sys);
    return ret;
}

static void Close(vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = access->p_sys;

    vlc_http_res_destroy(sys->resource);
    vlc_http_mgr_destroy(sys->manager);
    free(sys);
}

vlc_module_begin()
    set_description(N_("HTTPS input"))
    set_shortname(N_("HTTPS"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    set_capability("access", 2)
    add_shortcut("https", "http")
    set_callbacks(Open, Close)

    add_bool("http2", false, N_("Force HTTP/2"),
             N_("Force HTTP version 2.0 over TCP."), true)

    add_bool("http-continuous", false, N_("Continuous stream"),
             N_("Keep reading a resource that keeps being updated."), true)
        change_safe()
        change_volatile()
    add_bool("http-forward-cookies", true, N_("Cookies forwarding"),
             N_("Forward cookies across HTTP redirections."), true)
    add_string("http-referrer", NULL, N_("Referrer"),
               N_("Provide the referral URL, i.e. HTTP \"Referer\" (sic)."),
               true)
        change_safe()
        change_volatile()
    add_string("http-user-agent", NULL, N_("User agent"),
               N_("Override the name and version of the application as "
                  "provided to the HTTP server, i.e. the HTTP \"User-Agent\". "
                  "Name and version must be separated by a forward slash, "
                  "e.g. \"FooBar/1.2.3\"."), true)
        change_safe()
        change_private()
vlc_module_end()
