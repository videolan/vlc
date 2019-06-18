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

#undef MODULE_STRING
#define MODULE_STRING "http"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_keystore.h>
#include <vlc_plugin.h>
#include <vlc_url.h>

#include "connmgr.h"
#include "resource.h"
#include "file.h"
#include "live.h"

typedef struct
{
    struct vlc_http_mgr *manager;
    struct vlc_http_resource *resource;
} access_sys_t;

static block_t *FileRead(stream_t *access, bool *restrict eof)
{
    access_sys_t *sys = access->p_sys;

    block_t *b = vlc_http_file_read(sys->resource);
    if (b == NULL)
        *eof = true;
    return b;
}

static int FileSeek(stream_t *access, uint64_t pos)
{
    access_sys_t *sys = access->p_sys;

    if (vlc_http_file_seek(sys->resource, pos))
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int FileControl(stream_t *access, int query, va_list args)
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
            *va_arg(args, vlc_tick_t *) = VLC_TICK_FROM_MS(
                var_InheritInteger(access, "network-caching") );
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

static block_t *LiveRead(stream_t *access, bool *restrict eof)
{
    access_sys_t *sys = access->p_sys;

    block_t *b = vlc_http_live_read(sys->resource);
    if (b == NULL) /* TODO: loop instead of EOF, see vlc_http_live_read() */
        *eof = true;
    return b;
}

static int LiveControl(stream_t *access, int query, va_list args)
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
            *va_arg(args, vlc_tick_t *) = VLC_TICK_FROM_MS(
                var_InheritInteger(access, "network-caching") );
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
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = malloc(sizeof (*sys));
    int ret = VLC_ENOMEM;

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->manager = NULL;
    sys->resource = NULL;

    void *jar = NULL;
    if (var_InheritBool(obj, "http-forward-cookies"))
        jar = var_InheritAddress(obj, "http-cookies");

    struct vlc_credential crd;
    struct vlc_url_t crd_url;
    char *psz_realm = NULL;

    vlc_UrlParse(&crd_url, access->psz_url);
    vlc_credential_init(&crd, &crd_url);

    sys->manager = vlc_http_mgr_create(obj, jar);
    if (sys->manager == NULL)
        goto error;

    char *ua = var_InheritString(obj, "http-user-agent");
    char *referer = var_InheritString(obj, "http-referrer");
    bool live = var_InheritBool(obj, "http-continuous");

    sys->resource = (live ? vlc_http_live_create : vlc_http_file_create)(
        sys->manager, access->psz_url, ua, referer);
    free(referer);
    free(ua);

    if (sys->resource == NULL)
        goto error;

    if (vlc_credential_get(&crd, obj, NULL, NULL, NULL, NULL))
        vlc_http_res_set_login(sys->resource,
                               crd.psz_username, crd.psz_password);

    ret = VLC_EGENERIC;

    int status = vlc_http_res_get_status(sys->resource);

    while (status == 401) /* authentication */
    {
        crd.psz_authtype = "Basic";
        free(psz_realm);
        psz_realm = vlc_http_res_get_basic_realm(sys->resource);

        if (psz_realm == NULL)
            break;
        crd.psz_realm = psz_realm;
        if (!vlc_credential_get(&crd, obj, NULL, NULL, _("HTTP authentication"),
                                _("Please enter a valid login name and "
                                  "a password for realm %s."), crd.psz_realm))
            break;

        vlc_http_res_set_login(sys->resource,
                               crd.psz_username, crd.psz_password);
        status = vlc_http_res_get_status(sys->resource);
    }

    if (status < 0)
    {
        msg_Err(access, "HTTP connection failure");
        goto error;
    }

    char *redir = vlc_http_res_get_redirect(sys->resource);
    if (redir != NULL)
    {
        access->psz_url = redir;
        ret = VLC_ACCESS_REDIRECT;
        goto error;
    }

    if (status >= 300)
    {
        msg_Err(access, "HTTP %d error", status);
        goto error;
    }

    vlc_credential_store(&crd, obj);
    free(psz_realm);
    vlc_credential_clean(&crd);
    vlc_UrlClean(&crd_url);

    access->pf_read = NULL;
    if (live)
    {
        access->pf_block = LiveRead;
        access->pf_seek = NULL;
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
    free(psz_realm);
    vlc_credential_clean(&crd);
    vlc_UrlClean(&crd_url);
    free(sys);
    return ret;
}

static void Close(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
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

    add_bool("http-continuous", false, N_("Continuous stream"),
             N_("Keep reading a resource that keeps being updated."), true)
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
