/*****************************************************************************
 * webdav.c: WebDAV directory browsing access plug-in
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan.org>
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
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_block.h>
#include <vlc_input_item.h>
#include <vlc_keystore.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_strings.h>
#include <vlc_url.h>
#include <vlc_vector.h>
#include <vlc_xml.h>

#include "http/connmgr.h"
#include "http/message.h"

struct dav_entry
{
    char    *href;
    char    *name;
    bool     is_collection;
    bool     has_size;
    uint64_t size;
    bool     has_mtime;
    time_t   mtime;
};

typedef struct VLC_VECTOR(struct dav_entry) dav_entry_vector_t;

typedef struct
{
    struct vlc_http_mgr *manager;
    vlc_url_t            url;
    bool                 secure;
    unsigned             port;
    char                *host;
    char                *authority;
    char                *path;      /* absolute, includes any ?query */
    dav_entry_vector_t   entries;
} access_sys_t;

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname("WebDAV")
    set_description(N_("WebDAV input"))
    set_capability("access", 0)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    add_shortcut("dav", "davs", "webdav", "webdavs")
    set_callbacks(Open, Close)
vlc_module_end()

static char *BuildChildUrl(access_sys_t *sys, const char *href)
{
    vlc_url_t base = {
        .psz_protocol = sys->secure ? (char *)"webdavs" : (char *)"webdav",
        .psz_host     = sys->host,
        .i_port       = sys->port,
        .psz_path     = sys->path,
    };
    char *base_uri = vlc_uri_compose(&base);
    if (base_uri == NULL)
        return NULL;

    char *abs = vlc_uri_resolve(base_uri, href);
    free(base_uri);
    return abs;
}

static bool IsSelfHref(const access_sys_t *sys, const char *href)
{
    vlc_url_t u;
    if (vlc_UrlParse(&u, href) != 0 || u.psz_path == NULL)
    {
        vlc_UrlClean(&u);
        return false;
    }

    /* Compare the href path against our request path (without any query). */
    size_t hlen = strlen(u.psz_path);
    size_t slen = strcspn(sys->path, "?");
    bool match = hlen == slen && memcmp(u.psz_path, sys->path, hlen) == 0;
    vlc_UrlClean(&u);
    return match;
}

static char *LeafName(const char *href)
{
    size_t len = strcspn(href, "?");
    while (len > 0 && href[len - 1] == '/')
        len--;
    if (len == 0)
        return NULL;

    size_t start = len;
    while (start > 0 && href[start - 1] != '/')
        start--;

    char *name = strndup(href + start, len - start);
    if (name != NULL)
        vlc_uri_decode(name);
    return name;
}

/* Wrap an in-flight vlc_http_msg as a stream_t so the XML reader can
 * pull blocks lazily without buffering the full body. */
static void HttpRespDestroy(stream_t *s)
{
    vlc_http_msg_destroy(s->p_sys);
}

static block_t *HttpRespBlock(stream_t *s, bool *eof)
{
    struct vlc_http_msg *resp = s->p_sys;
    block_t *b = vlc_http_msg_read(resp);
    if (b == vlc_http_error)
        return NULL;
    if (b == NULL)
        *eof = true;
    return b;
}

static int HttpRespControl(stream_t *s, int query, va_list ap)
{
    (void)s;
    switch (query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg(ap, bool *) = false;
            return VLC_SUCCESS;
        case STREAM_GET_PTS_DELAY:
            *va_arg(ap, vlc_tick_t *) = DEFAULT_PTS_DELAY;
            return VLC_SUCCESS;
        default:
            return VLC_EGENERIC;
    }
}

static stream_t *HttpRespStream(vlc_object_t *parent, struct vlc_http_msg *resp)
{
    stream_t *s = vlc_stream_CommonNew(parent, HttpRespDestroy);
    if (s == NULL)
    {
        vlc_http_msg_destroy(resp);
        return NULL;
    }
    s->pf_block = HttpRespBlock;
    s->pf_control = HttpRespControl;
    s->p_sys = resp;
    return s;
}

static struct vlc_http_msg *DoPropfind(stream_t *access, const char *user,
                                       const char *pass, int *status)
{
    access_sys_t *sys = access->p_sys;

    struct vlc_http_msg *req = vlc_http_req_create("PROPFIND",
        sys->secure ? "https" : "http", sys->authority, sys->path);
    if (req == NULL)
        return NULL;

    vlc_http_msg_add_header(req, "Depth", "1");
    vlc_http_msg_add_header(req, "Content-Length", "0");
    vlc_http_msg_add_header(req, "Accept", "application/xml, text/xml");

    char *ua = var_InheritString(access, "http-user-agent");
    if (ua != NULL)
    {
        vlc_http_msg_add_agent(req, ua);
        free(ua);
    }

    if (user != NULL && pass != NULL)
        vlc_http_msg_add_creds_basic(req, false, user, pass);

    struct vlc_http_msg *resp = vlc_http_mgr_request(sys->manager, sys->secure,
                                                     sys->host, sys->port,
                                                     req, true, false);
    vlc_http_msg_destroy(req);

    resp = vlc_http_msg_get_final(resp);
    if (resp == NULL)
    {
        *status = -1;
        return NULL;
    }
    *status = vlc_http_msg_get_status(resp);
    return resp;
}

static bool IsDavNamespace(const char *ns)
{
    return ns != NULL && !strcmp(ns, "DAV:");
}

static char *DupXmlText(const char *s)
{
    static const char xml_ws[] = " \t\r\n";
    s += strspn(s, xml_ws);
    size_t n = strlen(s);
    while (n > 0 && strchr(xml_ws, s[n - 1]) != NULL)
        n--;
    return strndup(s, n);
}

static void EmitEntry(stream_t *access, struct vlc_readdir_helper *rdh,
                      const struct dav_entry *e)
{
    access_sys_t *sys = access->p_sys;

    char *url = BuildChildUrl(sys, e->href);
    if (url == NULL)
        return;

    const char *name = e->name;
    char *leaf = NULL;
    if (name == NULL || *name == '\0')
    {
        leaf = LeafName(e->href);
        name = leaf;
    }
    if (name == NULL)
    {
        free(url);
        return;
    }

    int type = e->is_collection ? ITEM_TYPE_DIRECTORY : ITEM_TYPE_FILE;
    input_item_t *item = NULL;
    vlc_readdir_helper_additem(rdh, url, NULL, name, type, ITEM_NET, &item);

    if (item != NULL && !e->is_collection)
    {
        if (e->has_size)
            input_item_AddStat(item, "size", e->size);
        if (e->has_mtime)
            input_item_AddStat(item, "mtime", e->mtime);
    }

    free(leaf);
    leaf = NULL;
    free(url);
    url = NULL;
}

static void ResetEntry(struct dav_entry *e)
{
    free(e->href);
    e->href = NULL;
    free(e->name);
    e->name = NULL;
    memset(e, 0, sizeof(*e));
}

/*
 * Parse a WebDAV PROPFIND multistatus response (RFC 4918 §14.16):
 *
 *   <D:multistatus xmlns:D="DAV:">
 *     <D:response>
 *       <D:href>/path/to/resource</D:href>
 *       <D:propstat>
 *         <D:prop>
 *           <D:resourcetype><D:collection/></D:resourcetype>     (optional)
 *           <D:getcontentlength>1234</D:getcontentlength>        (optional)
 *           <D:getlastmodified>RFC1123 date</D:getlastmodified>  (optional)
 *           <D:displayname>name</D:displayname>                  (optional)
 *         </D:prop>
 *       </D:propstat>
 *     </D:response>
 *     ... one <D:response> per child resource ...
 *   </D:multistatus>
 *
 * The DAV: namespace prefix varies between servers, so we match on the
 * resolved URI. With Depth: 1 the first <response> is the request target
 * itself, which lets us report self_is_collection_out from the same parse.
 */
static int ParseMultistatus(stream_t *access, stream_t *body,
                            bool *self_is_collection_out)
{
    access_sys_t *sys = access->p_sys;
    xml_reader_t *reader = xml_ReaderCreate(access, body);
    if (reader == NULL)
        return VLC_EGENERIC;

    struct dav_entry cur = { 0 };
    bool in_response = false;
    bool in_prop = false;
    bool in_resourcetype = false;
    enum { FIELD_NONE, FIELD_HREF, FIELD_NAME,
           FIELD_SIZE, FIELD_MTIME } field = FIELD_NONE;

    const char *node, *ns;
    int type;

    while ((type = xml_ReaderNextNodeNS(reader, &node, &ns)) > 0)
    {
        if (type == XML_READER_STARTELEM)
        {
            bool empty = xml_ReaderIsEmptyElement(reader) == 1;
            field = FIELD_NONE;

            if (!IsDavNamespace(ns))
                continue;

            /* we get the prefixed name, e.g. "D:href" */
            const char *local = strchr(node, ':');
            local = local ? local + 1 : node;

            if (!in_response)
            {
                if (!strcmp(local, "response"))
                {
                    in_response = true;
                    ResetEntry(&cur);
                }
                continue;
            }

            if (!in_prop)
            {
                if (!strcmp(local, "href"))
                    field = FIELD_HREF;
                else if (!strcmp(local, "prop"))
                    in_prop = !empty;
            }
            else if (in_resourcetype)
            {
                if (!strcmp(local, "collection"))
                    cur.is_collection = true;
            }
            else if (!strcmp(local, "resourcetype"))
                in_resourcetype = !empty;
            else if (!strcmp(local, "getcontentlength"))
                field = FIELD_SIZE;
            else if (!strcmp(local, "getlastmodified"))
                field = FIELD_MTIME;
            else if (!strcmp(local, "displayname"))
                field = FIELD_NAME;
        }
        else if (type == XML_READER_TEXT)
        {
            if (!in_response || field == FIELD_NONE)
                continue;

            char *value = DupXmlText(node);
            if (value == NULL)
                continue;

            switch (field)
            {
                case FIELD_HREF:
                    if (cur.href == NULL)
                        cur.href = value;
                    else
                        free(value);
                    break;
                case FIELD_NAME:
                    if (cur.name == NULL)
                        cur.name = value;
                    else
                        free(value);
                    break;
                case FIELD_SIZE:
                {
                    char *end;
                    unsigned long long v = strtoull(value, &end, 10);
                    if (end != value)
                    {
                        cur.size = v;
                        cur.has_size = true;
                    }
                    free(value);
                    break;
                }
                case FIELD_MTIME:
                {
                    time_t t = vlc_http_mktime(value);
                    if (t != (time_t)-1)
                    {
                        cur.mtime = t;
                        cur.has_mtime = true;
                    }
                    free(value);
                    break;
                }
                default:
                    free(value);
                    break;
            }
            field = FIELD_NONE;
        }
        else if (type == XML_READER_ENDELEM)
        {
            field = FIELD_NONE;
            if (!IsDavNamespace(ns))
                continue;

            const char *local = strchr(node, ':');
            local = local ? local + 1 : node;

            if (!strcmp(local, "resourcetype"))
                in_resourcetype = false;
            else if (!strcmp(local, "prop"))
            {
                in_prop = false;
                in_resourcetype = false;
            }
            else if (!strcmp(local, "response"))
            {
                /* memset on successful push transfers href/name ownership
                 * to the vector; ResetEntry frees them otherwise. */
                if (cur.href != NULL && IsSelfHref(sys, cur.href))
                {
                    if (self_is_collection_out != NULL)
                        *self_is_collection_out = cur.is_collection;
                    ResetEntry(&cur);
                }
                else if (cur.href == NULL
                      || !vlc_vector_push(&sys->entries, cur))
                    ResetEntry(&cur);
                else
                    memset(&cur, 0, sizeof(cur));
                in_response = false;
                in_prop = false;
                in_resourcetype = false;
            }
        }
    }

    ResetEntry(&cur);
    xml_ReaderDelete(reader);
    return VLC_SUCCESS;
}

static int DirRead(stream_t *access, input_item_node_t *node)
{
    access_sys_t *sys = access->p_sys;

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init(&rdh, access, node);
    const struct dav_entry *e;
    vlc_vector_foreach_ref(e, &sys->entries)
        EmitEntry(access, &rdh, e);
    vlc_readdir_helper_finish(&rdh, true);
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = vlc_obj_calloc(obj, 1, sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    access->p_sys = sys;
    int ret = VLC_EGENERIC;

    if (vlc_UrlParseFixup(&sys->url, access->psz_url) != 0
     || sys->url.psz_host == NULL || sys->url.psz_protocol == NULL)
        goto clean;

    const char *proto = sys->url.psz_protocol;
    if (!vlc_ascii_strcasecmp(proto, "dav")
     || !vlc_ascii_strcasecmp(proto, "webdav"))
        sys->secure = false;
    else if (!vlc_ascii_strcasecmp(proto, "davs")
          || !vlc_ascii_strcasecmp(proto, "webdavs"))
        sys->secure = true;
    else
        goto clean;

    sys->host = strdup(sys->url.psz_host);
    sys->port = sys->url.i_port;
    sys->authority = vlc_http_authority(sys->url.psz_host, sys->url.i_port);
    if (sys->host == NULL || sys->authority == NULL)
        goto clean;

    const char *path = sys->url.psz_path ? sys->url.psz_path : "/";
    if (sys->url.psz_option != NULL)
    {
        if (asprintf(&sys->path, "%s?%s", path, sys->url.psz_option) < 0)
            sys->path = NULL;
    }
    else
        sys->path = strdup(path);
    if (sys->path == NULL)
        goto clean;

    sys->manager = vlc_http_mgr_create(obj, NULL);
    if (sys->manager == NULL)
        goto clean;

    vlc_credential crd;
    vlc_credential_init(&crd, &sys->url);

    const char *user = sys->url.psz_username;
    const char *pass = sys->url.psz_password;
    if (vlc_credential_get(&crd, obj, NULL, NULL, NULL, NULL) == 0)
    {
        user = crd.psz_username;
        pass = crd.psz_password;
    }

    struct vlc_http_msg *resp = NULL;
    int status = -1;

    for (;;)
    {
        if (resp != NULL)
            vlc_http_msg_destroy(resp);
        resp = DoPropfind(access, user, pass, &status);
        if (resp == NULL)
        {
            vlc_credential_clean(&crd);
            goto clean;
        }

        if (status != 401)
            break;

        char *realm = vlc_http_msg_get_basic_realm(resp);
        if (realm == NULL)
            break;

        crd.psz_realm = realm;
        crd.psz_authtype = "Basic";
        int ok = vlc_credential_get(&crd, obj, NULL, NULL,
                                    _("HTTP authentication"),
                                    _("Please enter a valid login name and "
                                      "a password for realm %s."), realm);
        free(realm);
        if (ok != 0)
            break;
        user = crd.psz_username;
        pass = crd.psz_password;
    }

    if (status == 401 || status < 0)
    {
        msg_Err(access, "PROPFIND failed (status %d)", status);
        vlc_http_msg_destroy(resp);
        vlc_credential_clean(&crd);
        goto clean;
    }

    /* Follow Location on redirects. */
    if (status == 301 || status == 302 || status == 303
     || status == 307 || status == 308)
    {
        const char *loc = vlc_http_msg_get_header(resp, "Location");
        if (loc != NULL)
        {
            char *abs = vlc_uri_resolve(access->psz_url, loc);
            if (abs != NULL)
            {
                access->psz_url = abs;
                vlc_http_msg_destroy(resp);
                vlc_credential_clean(&crd);
                ret = VLC_ACCESS_REDIRECT;
                goto clean;
            }
        }
        msg_Err(access, "redirect without usable Location");
        vlc_http_msg_destroy(resp);
        vlc_credential_clean(&crd);
        goto clean;
    }

    if (status != 207)
    {
        /* Not a multistatus: redirect to http(s) so the regular HTTP access
         * can play the target as a single file. */
        char *abs;
        if (asprintf(&abs, "%s://%s%s", sys->secure ? "https" : "http",
                     sys->authority, sys->path) >= 0)
        {
            access->psz_url = abs;
            ret = VLC_ACCESS_REDIRECT;
        }
        vlc_http_msg_destroy(resp);
        vlc_credential_clean(&crd);
        goto clean;
    }

    vlc_credential_store(&crd, obj);
    vlc_credential_clean(&crd);

    bool self_is_collection = false;
    stream_t *body = HttpRespStream(obj, resp);
    if (body != NULL)
    {
        ParseMultistatus(access, body, &self_is_collection);
        vlc_stream_Delete(body);
    }

    if (!self_is_collection)
    {
        /* item is not a collection, so it's a file that we want to play
         * --> generate a http/s URL to allow playback */
        char *abs;
        if (asprintf(&abs, "%s://%s%s", sys->secure ? "https" : "http",
                     sys->authority, sys->path) >= 0)
        {
            access->psz_url = abs;
            ret = VLC_ACCESS_REDIRECT;
            goto clean;
        }
        msg_Err(access, "failed to build redirect URL");
        goto clean;
    }

    access->pf_readdir = DirRead;
    access->pf_control = access_vaDirectoryControlHelper;
    return VLC_SUCCESS;

clean:
    if (sys->manager != NULL)
        vlc_http_mgr_destroy(sys->manager);
    struct dav_entry *e;
    vlc_vector_foreach_ref(e, &sys->entries)
    {
        free(e->href);
        free(e->name);
    }
    vlc_vector_destroy(&sys->entries);
    free(sys->path);
    free(sys->authority);
    free(sys->host);
    vlc_UrlClean(&sys->url);
    return ret;
}

static void Close(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = access->p_sys;

    if (sys->manager != NULL)
        vlc_http_mgr_destroy(sys->manager);
    struct dav_entry *e;
    vlc_vector_foreach_ref(e, &sys->entries)
    {
        free(e->href);
        free(e->name);
    }
    vlc_vector_destroy(&sys->entries);
    free(sys->path);
    free(sys->authority);
    free(sys->host);
    vlc_UrlClean(&sys->url);
}
