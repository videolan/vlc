/*****************************************************************************
 * httpd.c
 *****************************************************************************
 * Copyright (C) 2004-2006 VLC authors and VideoLAN
 * Copyright © 2004-2007 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Rémi Denis-Courmont
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

#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_httpd.h>

#include <assert.h>

#include <vlc_list.h>
#include <vlc_network.h>
#include <vlc_tls.h>
#include <vlc_strings.h>
#include <vlc_rand.h>
#include <vlc_charset.h>
#include <vlc_url.h>
#include <vlc_mime.h>
#include <vlc_block.h>
#include "../libvlc.h"

#include <string.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#ifdef HAVE_POLL_H
# include <poll.h>
#endif

#if defined(_WIN32)
#   include <winsock2.h>
#elif defined(HAVE_SYS_SOCKET_H)
#   include <sys/socket.h>
#endif

#if defined(_WIN32)
/* We need HUGE buffer otherwise TCP throughput is very limited */
#define HTTPD_CL_BUFSIZE 1000000
#else
#define HTTPD_CL_BUFSIZE 10000
#endif

static void httpd_ClientDestroy(httpd_client_t *cl);
static void httpd_AppendData(httpd_stream_t *stream, uint8_t *p_data, int i_data);

/* each host run in his own thread */
struct httpd_host_t
{
    struct vlc_object_t obj;
    struct vlc_list node;

    /* ref count */
    atomic_uint ref;

    /* address/port and socket for listening at connections */
    int         *fds;
    unsigned     nfd;
    unsigned     port;

    vlc_thread_t thread;
    vlc_mutex_t lock;

    /* all registered url (becarefull that 2 httpd_url_t could point at the same url)
     * This will slow down the url research but make my live easier
     * All url will have their cb trigger, but only the first one can answer
     * */
    struct vlc_list urls;

    size_t client_count;
    struct vlc_list clients;

    /* TLS data */
    vlc_tls_server_t *p_tls;
};


struct httpd_url_t
{
    httpd_host_t *host;
    struct vlc_list node;
    vlc_mutex_t lock;

    char      *psz_url;
    char      *psz_user;
    char      *psz_password;

    struct
    {
        httpd_callback_t     cb;
        httpd_callback_sys_t *p_sys;
    } catch[HTTPD_MSG_MAX];
};

/* status */
enum
{
    HTTPD_CLIENT_RECEIVING,
    HTTPD_CLIENT_RECEIVE_DONE,

    HTTPD_CLIENT_SENDING,
    HTTPD_CLIENT_SEND_DONE,

    HTTPD_CLIENT_WAITING,

    HTTPD_CLIENT_DEAD,

    HTTPD_CLIENT_TLS_HS_IN,
    HTTPD_CLIENT_TLS_HS_OUT
};

struct httpd_client_t
{
    httpd_url_t *url;
    vlc_tls_t   *sock;

    struct vlc_list node;

    bool    b_stream_mode;
    uint8_t i_state;

    vlc_tick_t i_activity_date;
    vlc_tick_t i_activity_timeout;

    /* buffer for reading header */
    int     i_buffer_size;
    int     i_buffer;
    uint8_t *p_buffer;

    /*
     * If waiting for a keyframe, this is the position (in bytes) of the
     * last keyframe the stream saw before this client connected.
     * Otherwise, -1.
     */
    int64_t i_keyframe_wait_to_pass;

    /* */
    httpd_message_t query;  /* client -> httpd */
    httpd_message_t answer; /* httpd -> client */
};


/*****************************************************************************
 * Various functions
 *****************************************************************************/
static const char *httpd_ReasonFromCode(unsigned i_code)
{
    typedef struct
    {
        unsigned   i_code;
        const char psz_reason[36];
    } http_status_info;

    static const http_status_info http_reason[] =
    {
        /*{ 100, "Continue" },
          { 101, "Switching Protocols" },*/
        { 200, "OK" },
        /*{ 201, "Created" },
          { 202, "Accepted" },
          { 203, "Non-authoritative information" },
          { 204, "No content" },
          { 205, "Reset content" },
          { 206, "Partial content" },
          { 250, "Low on storage space" },
          { 300, "Multiple choices" },*/
        { 301, "Moved permanently" },
        /*{ 302, "Moved temporarily" },
          { 303, "See other" },
          { 304, "Not modified" },
          { 305, "Use proxy" },
          { 307, "Temporary redirect" },
          { 400, "Bad request" },*/
        { 401, "Unauthorized" },
        /*{ 402, "Payment Required" },*/
        { 403, "Forbidden" },
        { 404, "Not found" },
        { 405, "Method not allowed" },
        /*{ 406, "Not acceptable" },
          { 407, "Proxy authentication required" },
          { 408, "Request time-out" },
          { 409, "Conflict" },
          { 410, "Gone" },
          { 411, "Length required" },
          { 412, "Precondition failed" },
          { 413, "Request entity too large" },
          { 414, "Request-URI too large" },
          { 415, "Unsupported media Type" },
          { 416, "Requested range not satisfiable" },
          { 417, "Expectation failed" },
          { 451, "Parameter not understood" },
          { 452, "Conference not found" },
          { 453, "Not enough bandwidth" },*/
        { 454, "Session not found" },
        { 455, "Method not valid in this State" },
        { 456, "Header field not valid for resource" },
        { 457, "Invalid range" },
        /*{ 458, "Read-only parameter" },*/
        { 459, "Aggregate operation not allowed" },
        { 460, "Non-aggregate operation not allowed" },
        { 461, "Unsupported transport" },
        /*{ 462, "Destination unreachable" },*/
        { 500, "Internal server error" },
        { 501, "Not implemented" },
        /*{ 502, "Bad gateway" },*/
        { 503, "Service unavailable" },
        /*{ 504, "Gateway time-out" },*/
        { 505, "Protocol version not supported" },
        { 551, "Option not supported" },
        { 999, "" }
    };

    static const char psz_fallback_reason[5][16] = {
        "Continue", "OK", "Found", "Client error", "Server error"
    };

    assert((i_code >= 100) && (i_code <= 599));

    const http_status_info *p = http_reason;
    while (i_code < p->i_code)
        p++;

    if (p->i_code == i_code)
        return p->psz_reason;

    return psz_fallback_reason[(i_code / 100) - 1];
}

static size_t httpd_HtmlError (char **body, int code, const char *url)
{
    const char *errname = httpd_ReasonFromCode (code);
    assert (errname);

    char *url_Encoded = vlc_xml_encode (url ? url : "");

    int res = asprintf (body,
        "<?xml version=\"1.0\" encoding=\"ascii\" ?>\n"
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\""
        " \"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<title>%s</title>\n"
        "</head>\n"
        "<body>\n"
        "<h1>%d %s%s%s%s</h1>\n"
        "<hr />\n"
        "<a href=\"http://www.videolan.org\">VideoLAN</a>\n"
        "</body>\n"
        "</html>\n", errname, code, errname,
        (url_Encoded ? " (" : ""), (url_Encoded ? url_Encoded : ""), (url_Encoded ? ")" : ""));

    free (url_Encoded);

    if (res == -1) {
        *body = NULL;
        return 0;
    }

    return (size_t)res;
}


/*****************************************************************************
 * High Level Functions: httpd_file_t
 *****************************************************************************/
struct httpd_file_t
{
    httpd_url_t *url;
    httpd_file_callback_t pf_fill;
    httpd_file_sys_t      *p_sys;
    char mime[1];
};

static int
httpd_FileCallBack(httpd_callback_sys_t *p_sys, httpd_client_t *cl,
                    httpd_message_t *answer, const httpd_message_t *query)
{
    httpd_file_t *file = (httpd_file_t*)p_sys;
    uint8_t **pp_body, *p_body;
    int *pi_body, i_body;

    if (!answer || !query )
        return VLC_SUCCESS;

    answer->i_proto  = HTTPD_PROTO_HTTP;
    answer->i_version= 1;
    answer->i_type   = HTTPD_MSG_ANSWER;

    answer->i_status = 200;

    httpd_MsgAdd(answer, "Content-type",  "%s", file->mime);
    httpd_MsgAdd(answer, "Cache-Control", "%s", "no-cache");

    if (query->i_type != HTTPD_MSG_HEAD) {
        pp_body = &answer->p_body;
        pi_body = &answer->i_body;
    } else {
        /* The file still needs to be executed. */
        p_body = NULL;
        i_body = 0;
        pp_body = &p_body;
        pi_body = &i_body;
    }

    if (query->i_type == HTTPD_MSG_POST) {
        /* msg_Warn not supported */
    }

    uint8_t *psz_args = query->psz_args;
    file->pf_fill(file->p_sys, file, psz_args, pp_body, pi_body);

    if (query->i_type == HTTPD_MSG_HEAD)
        free(p_body);

    /* We respect client request */
    if (httpd_MsgGet(&cl->query, "Connection") != NULL)
        httpd_MsgAdd(answer, "Connection", "close");

    httpd_MsgAdd(answer, "Content-Length", "%d", answer->i_body);

    return VLC_SUCCESS;
}

httpd_file_t *httpd_FileNew(httpd_host_t *host,
                             const char *psz_url, const char *psz_mime,
                             const char *psz_user, const char *psz_password,
                             httpd_file_callback_t pf_fill,
                             httpd_file_sys_t *p_sys)
{
    const char *mime = psz_mime;
    if (mime == NULL || mime[0] == '\0')
        mime = vlc_mime_Ext2Mime(psz_url);

    size_t mimelen = strlen(mime);
    httpd_file_t *file = malloc(sizeof(*file) + mimelen);
    if (unlikely(file == NULL))
        return NULL;

    file->url = httpd_UrlNew(host, psz_url, psz_user, psz_password);
    if (!file->url) {
        free(file);
        return NULL;
    }

    file->pf_fill = pf_fill;
    file->p_sys   = p_sys;
    memcpy(file->mime, mime, mimelen + 1);

    httpd_UrlCatch(file->url, HTTPD_MSG_HEAD, httpd_FileCallBack,
                    (httpd_callback_sys_t*)file);
    httpd_UrlCatch(file->url, HTTPD_MSG_GET,  httpd_FileCallBack,
                    (httpd_callback_sys_t*)file);
    httpd_UrlCatch(file->url, HTTPD_MSG_POST, httpd_FileCallBack,
                    (httpd_callback_sys_t*)file);

    return file;
}

httpd_file_sys_t *httpd_FileDelete(httpd_file_t *file)
{
    httpd_file_sys_t *p_sys = file->p_sys;

    httpd_UrlDelete(file->url);
    free(file);
    return p_sys;
}

/*****************************************************************************
 * High Level Functions: httpd_handler_t (for CGIs)
 *****************************************************************************/
struct httpd_handler_t
{
    httpd_url_t *url;

    httpd_handler_callback_t pf_fill;
    void                    *p_sys;

};

static int
httpd_HandlerCallBack(httpd_callback_sys_t *p_sys, httpd_client_t *cl,
                       httpd_message_t *answer, const httpd_message_t *query)
{
    httpd_handler_t *handler = (httpd_handler_t*)p_sys;
    char psz_remote_addr[NI_MAXNUMERICHOST];

    if (!answer || !query)
        return VLC_SUCCESS;

    answer->i_proto  = HTTPD_PROTO_NONE;
    answer->i_type   = HTTPD_MSG_ANSWER;

    /* We do it ourselves, thanks */
    answer->i_status = 0;

    if (!httpd_ClientIP(cl, psz_remote_addr, NULL))
        *psz_remote_addr = '\0';

    uint8_t *psz_args = query->psz_args;
    handler->pf_fill(handler->p_sys, handler, query->psz_url, psz_args,
                      query->i_type, query->p_body, query->i_body,
                      psz_remote_addr, NULL,
                      &answer->p_body, &answer->i_body);

    if (query->i_type == HTTPD_MSG_HEAD) {
        char *p = (char *)answer->p_body;

        /* Looks for end of header (i.e. one empty line) */
        while ((p = strchr(p, '\r')))
            if (p[1] == '\n' && p[2] == '\r' && p[3] == '\n')
                break;

        if (p) {
            p[4] = '\0';
            answer->i_body = strlen((char*)answer->p_body) + 1;
            answer->p_body = xrealloc(answer->p_body, answer->i_body);
        }
    }

    if (strncmp((char *)answer->p_body, "HTTP/1.", 7)) {
        int i_status, i_headers;
        char *psz_headers, *psz_new;
        const char *psz_status;

        if (!strncmp((char *)answer->p_body, "Status: ", 8)) {
            /* Apache-style */
            i_status = strtol((char *)&answer->p_body[8], &psz_headers, 0);
            if (*psz_headers == '\r' || *psz_headers == '\n') psz_headers++;
            if (*psz_headers == '\n') psz_headers++;
            i_headers = answer->i_body - (psz_headers - (char *)answer->p_body);
        } else {
            i_status = 200;
            psz_headers = (char *)answer->p_body;
            i_headers = answer->i_body;
        }

        psz_status = httpd_ReasonFromCode(i_status);
        answer->i_body = sizeof("HTTP/1.0 xxx \r\n")
                        + strlen(psz_status) + i_headers - 1;
        psz_new = (char *)xmalloc(answer->i_body + 1);
        sprintf(psz_new, "HTTP/1.0 %03d %s\r\n", i_status, psz_status);
        memcpy(&psz_new[strlen(psz_new)], psz_headers, i_headers);
        free(answer->p_body);
        answer->p_body = (uint8_t *)psz_new;
    }

    return VLC_SUCCESS;
}

httpd_handler_t *httpd_HandlerNew(httpd_host_t *host, const char *psz_url,
                                  const char *psz_user,
                                  const char *psz_password,
                                  httpd_handler_callback_t pf_fill,
                                  void *p_sys)
{
    httpd_handler_t *handler = malloc(sizeof(*handler));
    if (!handler)
        return NULL;

    handler->url = httpd_UrlNew(host, psz_url, psz_user, psz_password);
    if (!handler->url) {
        free(handler);
        return NULL;
    }

    handler->pf_fill = pf_fill;
    handler->p_sys   = p_sys;

    httpd_UrlCatch(handler->url, HTTPD_MSG_HEAD, httpd_HandlerCallBack,
                    (httpd_callback_sys_t*)handler);
    httpd_UrlCatch(handler->url, HTTPD_MSG_GET,  httpd_HandlerCallBack,
                    (httpd_callback_sys_t*)handler);
    httpd_UrlCatch(handler->url, HTTPD_MSG_POST, httpd_HandlerCallBack,
                    (httpd_callback_sys_t*)handler);

    return handler;
}

void *httpd_HandlerDelete(httpd_handler_t *handler)
{
    void *p_sys = handler->p_sys;
    httpd_UrlDelete(handler->url);
    free(handler);
    return p_sys;
}

/*****************************************************************************
 * High Level Functions: httpd_redirect_t
 *****************************************************************************/
struct httpd_redirect_t
{
    httpd_url_t *url;
    char         dst[1];
};

static int httpd_RedirectCallBack(httpd_callback_sys_t *p_sys,
                                   httpd_client_t *cl, httpd_message_t *answer,
                                   const httpd_message_t *query)
{
    httpd_redirect_t *rdir = (httpd_redirect_t*)p_sys;
    char *p_body;
    (void)cl;

    if (!answer || !query)
        return VLC_SUCCESS;

    answer->i_proto  = HTTPD_PROTO_HTTP;
    answer->i_version= 1;
    answer->i_type   = HTTPD_MSG_ANSWER;
    answer->i_status = 301;

    answer->i_body = httpd_HtmlError (&p_body, 301, rdir->dst);
    answer->p_body = (unsigned char *)p_body;

    /* XXX check if it's ok or we need to set an absolute url */
    httpd_MsgAdd(answer, "Location",  "%s", rdir->dst);

    httpd_MsgAdd(answer, "Content-Length", "%d", answer->i_body);

    if (httpd_MsgGet(&cl->query, "Connection") != NULL)
        httpd_MsgAdd(answer, "Connection", "close");

    return VLC_SUCCESS;
}

httpd_redirect_t *httpd_RedirectNew(httpd_host_t *host, const char *psz_url_dst,
                                     const char *psz_url_src)
{
    size_t dstlen = strlen(psz_url_dst);

    httpd_redirect_t *rdir = malloc(sizeof(*rdir) + dstlen);
    if (unlikely(rdir == NULL))
        return NULL;

    rdir->url = httpd_UrlNew(host, psz_url_src, NULL, NULL);
    if (!rdir->url) {
        free(rdir);
        return NULL;
    }
    memcpy(rdir->dst, psz_url_dst, dstlen + 1);

    /* Redirect apply for all HTTP request and RTSP DESCRIBE resquest */
    httpd_UrlCatch(rdir->url, HTTPD_MSG_HEAD, httpd_RedirectCallBack,
                    (httpd_callback_sys_t*)rdir);
    httpd_UrlCatch(rdir->url, HTTPD_MSG_GET, httpd_RedirectCallBack,
                    (httpd_callback_sys_t*)rdir);
    httpd_UrlCatch(rdir->url, HTTPD_MSG_POST, httpd_RedirectCallBack,
                    (httpd_callback_sys_t*)rdir);
    httpd_UrlCatch(rdir->url, HTTPD_MSG_DESCRIBE, httpd_RedirectCallBack,
                    (httpd_callback_sys_t*)rdir);

    return rdir;
}
void httpd_RedirectDelete(httpd_redirect_t *rdir)
{
    httpd_UrlDelete(rdir->url);
    free(rdir);
}

/*****************************************************************************
 * High Level Funtions: httpd_stream_t
 *****************************************************************************/
struct httpd_stream_t
{
    vlc_mutex_t lock;
    httpd_url_t *url;

    char    *psz_mime;

    /* Header to send as first packet */
    uint8_t *p_header;
    int     i_header;

    /* Some muxes, in particular the avformat mux, can mark given blocks
     * as keyframes, to ensure that the stream starts with one.
     * (This is particularly important for WebM streaming to certain
     * browsers.) Store if we've ever seen any such keyframe blocks,
     * and if so, the byte position of the start of the last one. */
    bool        b_has_keyframes;
    int64_t     i_last_keyframe_seen_pos;

    /* circular buffer */
    int         i_buffer_size;      /* buffer size, can't be reallocated smaller */
    uint8_t     *p_buffer;          /* buffer */
    int64_t     i_buffer_pos;       /* absolute position from beginning */
    int64_t     i_buffer_last_pos;  /* a new connection will start with that */

    /* custom headers */
    size_t        i_http_headers;
    httpd_header * p_http_headers;
};

static int httpd_StreamCallBack(httpd_callback_sys_t *p_sys,
                                 httpd_client_t *cl, httpd_message_t *answer,
                                 const httpd_message_t *query)
{
    httpd_stream_t *stream = (httpd_stream_t*)p_sys;

    if (!answer || !query || !cl)
        return VLC_SUCCESS;

    if (answer->i_body_offset > 0) {
        int     i_pos;

        if (answer->i_body_offset >= stream->i_buffer_pos)
            return VLC_EGENERIC;    /* wait, no data available */

        if (cl->i_keyframe_wait_to_pass >= 0) {
            if (stream->i_last_keyframe_seen_pos <= cl->i_keyframe_wait_to_pass)
                /* still waiting for the next keyframe */
                return VLC_EGENERIC;

            /* seek to the new keyframe */
            answer->i_body_offset = stream->i_last_keyframe_seen_pos;
            cl->i_keyframe_wait_to_pass = -1;
        }

        if (answer->i_body_offset + stream->i_buffer_size < stream->i_buffer_pos)
            answer->i_body_offset = stream->i_buffer_last_pos; /* this client isn't fast enough */

        i_pos   = answer->i_body_offset % stream->i_buffer_size;
        int64_t i_write = stream->i_buffer_pos - answer->i_body_offset;

        if (i_write > HTTPD_CL_BUFSIZE)
            i_write = HTTPD_CL_BUFSIZE;
        else if (i_write <= 0)
            return VLC_EGENERIC;    /* wait, no data available */

        /* Don't go past the end of the circular buffer */
        i_write = __MIN(i_write, stream->i_buffer_size - i_pos);

        /* using HTTPD_MSG_ANSWER -> data available */
        answer->i_proto  = HTTPD_PROTO_HTTP;
        answer->i_version= 0;
        answer->i_type   = HTTPD_MSG_ANSWER;

        answer->i_body = i_write;
        answer->p_body = xmalloc(i_write);
        memcpy(answer->p_body, &stream->p_buffer[i_pos], i_write);

        answer->i_body_offset += i_write;

        return VLC_SUCCESS;
    } else {
        answer->i_proto  = HTTPD_PROTO_HTTP;
        answer->i_version= 0;
        answer->i_type   = HTTPD_MSG_ANSWER;

        answer->i_status = 200;

        bool b_has_content_type = false;
        bool b_has_cache_control = false;

        vlc_mutex_lock(&stream->lock);
        for (size_t i = 0; i < stream->i_http_headers; i++)
            if (strncasecmp(stream->p_http_headers[i].name, "Content-Length", 14)) {
                httpd_MsgAdd(answer, stream->p_http_headers[i].name, "%s",
                              stream->p_http_headers[i].value);

                if (!strncasecmp(stream->p_http_headers[i].name, "Content-Type", 12))
                    b_has_content_type = true;
                else if (!strncasecmp(stream->p_http_headers[i].name, "Cache-Control", 13))
                    b_has_cache_control = true;
            }
        vlc_mutex_unlock(&stream->lock);

        if (query->i_type != HTTPD_MSG_HEAD) {
            cl->b_stream_mode = true;
            vlc_mutex_lock(&stream->lock);
            /* Send the header */
            if (stream->i_header > 0) {
                answer->i_body = stream->i_header;
                answer->p_body = xmalloc(stream->i_header);
                memcpy(answer->p_body, stream->p_header, stream->i_header);
            }
            answer->i_body_offset = stream->i_buffer_last_pos;
            if (stream->b_has_keyframes)
                cl->i_keyframe_wait_to_pass = stream->i_last_keyframe_seen_pos;
            else
                cl->i_keyframe_wait_to_pass = -1;
            vlc_mutex_unlock(&stream->lock);
        } else {
            httpd_MsgAdd(answer, "Content-Length", "0");
            answer->i_body_offset = 0;
        }

        /* FIXME: move to http access_output */
        if (!strcmp(stream->psz_mime, "video/x-ms-asf-stream")) {
            bool b_xplaystream = false;

            httpd_MsgAdd(answer, "Content-type", "application/octet-stream");
            httpd_MsgAdd(answer, "Server", "Cougar 4.1.0.3921");
            httpd_MsgAdd(answer, "Pragma", "no-cache");
            httpd_MsgAdd(answer, "Pragma", "client-id=%lu",
                          vlc_mrand48()&0x7fff);
            httpd_MsgAdd(answer, "Pragma", "features=\"broadcast\"");

            /* Check if there is a xPlayStrm=1 */
            for (size_t i = 0; i < query->i_headers; i++)
                if (!strcasecmp(query->p_headers[i].name,  "Pragma") &&
                    strstr(query->p_headers[i].value, "xPlayStrm=1"))
                    b_xplaystream = true;

            if (!b_xplaystream)
                answer->i_body_offset = 0;
        } else if (!b_has_content_type)
            httpd_MsgAdd(answer, "Content-type", "%s", stream->psz_mime);

        if (!b_has_cache_control)
            httpd_MsgAdd(answer, "Cache-Control", "no-cache");

        httpd_MsgAdd(answer, "Connection", "close");

        return VLC_SUCCESS;
    }
}

httpd_stream_t *httpd_StreamNew(httpd_host_t *host,
                                 const char *psz_url, const char *psz_mime,
                                 const char *psz_user, const char *psz_password)
{
    httpd_stream_t *stream = malloc(sizeof(*stream));
    if (!stream)
        return NULL;

    stream->psz_mime = NULL;
    stream->p_buffer = NULL;

    stream->url = httpd_UrlNew(host, psz_url, psz_user, psz_password);
    if (!stream->url)
        goto error;

    vlc_mutex_init(&stream->lock);
    if (psz_mime == NULL || psz_mime[0] == '\0')
        psz_mime = vlc_mime_Ext2Mime(psz_url);

    stream->psz_mime = strdup(psz_mime);
    if (stream->psz_mime == NULL)
        goto error;

    stream->i_header = 0;
    stream->p_header = NULL;
    stream->i_buffer_size = 5000000;    /* 5 Mo per stream */

    stream->p_buffer = malloc(stream->i_buffer_size);
    if (stream->p_buffer == NULL)
        goto error;

    /* We set to 1 to make life simpler
     * (this way i_body_offset can never be 0) */
    stream->i_buffer_pos = 1;
    stream->i_buffer_last_pos = 1;
    stream->b_has_keyframes = false;
    stream->i_last_keyframe_seen_pos = 0;
    stream->i_http_headers = 0;
    stream->p_http_headers = NULL;

    httpd_UrlCatch(stream->url, HTTPD_MSG_HEAD, httpd_StreamCallBack,
                    (httpd_callback_sys_t*)stream);
    httpd_UrlCatch(stream->url, HTTPD_MSG_GET, httpd_StreamCallBack,
                    (httpd_callback_sys_t*)stream);
    httpd_UrlCatch(stream->url, HTTPD_MSG_POST, httpd_StreamCallBack,
                    (httpd_callback_sys_t*)stream);

    return stream;

error:
    free(stream->psz_mime);

    if (stream->url)
        httpd_UrlDelete(stream->url);

    free(stream);

    return NULL;
}

int httpd_StreamHeader(httpd_stream_t *stream, uint8_t *p_data, int i_data)
{
    vlc_mutex_lock(&stream->lock);
    free(stream->p_header);
    stream->p_header = NULL;

    stream->i_header = i_data;
    if (i_data > 0) {
        stream->p_header = xmalloc(i_data);
        memcpy(stream->p_header, p_data, i_data);
    }
    vlc_mutex_unlock(&stream->lock);

    return VLC_SUCCESS;
}

static void httpd_AppendData(httpd_stream_t *stream, uint8_t *p_data, int i_data)
{
    int i_pos = stream->i_buffer_pos % stream->i_buffer_size;
    int i_count = i_data;
    while (i_count > 0) {
        int i_copy = __MIN(i_count, stream->i_buffer_size - i_pos);

        /* Ok, we can't go past the end of our buffer */
        memcpy(&stream->p_buffer[i_pos], p_data, i_copy);

        i_pos = (i_pos + i_copy) % stream->i_buffer_size;
        i_count -= i_copy;
        p_data  += i_copy;
    }

    stream->i_buffer_pos += i_data;
}

int httpd_StreamSend(httpd_stream_t *stream, const block_t *p_block)
{
    if (!p_block || !p_block->p_buffer)
        return VLC_SUCCESS;

    vlc_mutex_lock(&stream->lock);

    /* save this pointer (to be used by new connection) */
    stream->i_buffer_last_pos = stream->i_buffer_pos;

    if (p_block->i_flags & BLOCK_FLAG_TYPE_I) {
        stream->b_has_keyframes = true;
        stream->i_last_keyframe_seen_pos = stream->i_buffer_pos;
    }

    httpd_AppendData(stream, p_block->p_buffer, p_block->i_buffer);

    vlc_mutex_unlock(&stream->lock);
    return VLC_SUCCESS;
}

void httpd_StreamDelete(httpd_stream_t *stream)
{
    httpd_UrlDelete(stream->url);
    for (size_t i = 0; i < stream->i_http_headers; i++) {
        free(stream->p_http_headers[i].name);
        free(stream->p_http_headers[i].value);
    }
    free(stream->p_http_headers);
    free(stream->psz_mime);
    free(stream->p_header);
    free(stream->p_buffer);
    free(stream);
}

/*****************************************************************************
 * Low level
 *****************************************************************************/
static void* httpd_HostThread(void *);
static httpd_host_t *httpd_HostCreate(vlc_object_t *, const char *,
                                       const char *, vlc_tls_server_t *);

/* create a new host */
httpd_host_t *vlc_http_HostNew(vlc_object_t *p_this)
{
    return httpd_HostCreate(p_this, "http-host", "http-port", NULL);
}

httpd_host_t *vlc_https_HostNew(vlc_object_t *obj)
{
    char *cert = var_InheritString(obj, "http-cert");
    if (!cert) {
        msg_Err(obj, "HTTP/TLS certificate not specified!");
        return NULL;
    }

    char *key = var_InheritString(obj, "http-key");
    vlc_tls_server_t *tls = vlc_tls_ServerCreate(obj, cert, key);

    if (!tls) {
        msg_Err(obj, "HTTP/TLS certificate error (%s and %s)",
                 cert, key ? key : cert);
        free(key);
        free(cert);
        return NULL;
    }
    free(key);
    free(cert);

    return httpd_HostCreate(obj, "http-host", "https-port", tls);
}

httpd_host_t *vlc_rtsp_HostNew(vlc_object_t *p_this)
{
    return httpd_HostCreate(p_this, "rtsp-host", "rtsp-port", NULL);
}

static struct httpd
{
    vlc_mutex_t  mutex;
    struct vlc_list hosts;
} httpd = { VLC_STATIC_MUTEX, VLC_LIST_INITIALIZER(&httpd.hosts) };

static httpd_host_t *httpd_HostCreate(vlc_object_t *p_this,
                                       const char *hostvar,
                                       const char *portvar,
                                       vlc_tls_server_t *p_tls)
{
    httpd_host_t *host;
    unsigned port = var_InheritInteger(p_this, portvar);

    /* to be sure to avoid multiple creation */
    vlc_mutex_lock(&httpd.mutex);

    /* verify if it already exist */
    vlc_list_foreach(host, &httpd.hosts, node) {
        /* cannot mix TLS and non-TLS hosts */
        if (host->port != port
         || (host->p_tls != NULL) != (p_tls != NULL))
            continue;

        /* Increase existing matching host reference count. */
        atomic_fetch_add_explicit(&host->ref, 1, memory_order_relaxed);

        vlc_mutex_unlock(&httpd.mutex);
        vlc_tls_ServerDelete(p_tls);
        return host;
    }

    /* create the new host */
    host = (httpd_host_t *)vlc_custom_create(p_this, sizeof (*host),
                                              "http host");
    if (!host)
        goto error;

    vlc_mutex_init(&host->lock);
    atomic_init(&host->ref, 1);

    char *hostname = var_InheritString(p_this, hostvar);

    host->fds = net_ListenTCP(p_this, hostname, port);
    free(hostname);

    if (!host->fds) {
        msg_Err(p_this, "cannot create socket(s) for HTTP host");
        goto error;
    }
    for (host->nfd = 0; host->fds[host->nfd] != -1; host->nfd++);

    host->port     = port;
    vlc_list_init(&host->urls);
    host->client_count = 0;
    vlc_list_init(&host->clients);
    host->p_tls    = p_tls;

    /* create the thread */
    if (vlc_clone(&host->thread, httpd_HostThread, host,
                   VLC_THREAD_PRIORITY_LOW)) {
        msg_Err(p_this, "cannot spawn http host thread");
        goto error;
    }

    /* now add it to httpd */
    vlc_list_append(&host->node, &httpd.hosts);
    vlc_mutex_unlock(&httpd.mutex);

    return host;

error:
    vlc_mutex_unlock(&httpd.mutex);

    if (host) {
        net_ListenClose(host->fds);
        vlc_object_delete(host);
    }

    vlc_tls_ServerDelete(p_tls);
    return NULL;
}

/* delete a host */
void httpd_HostDelete(httpd_host_t *host)
{
    httpd_client_t *client;

    vlc_mutex_lock(&httpd.mutex);

    if (atomic_fetch_sub_explicit(&host->ref, 1, memory_order_relaxed) > 1) {
        /* still used */
        vlc_mutex_unlock(&httpd.mutex);
        msg_Dbg(host, "httpd_HostDelete: host still in use");
        return;
    }

    vlc_list_remove(&host->node);
    vlc_cancel(host->thread);
    vlc_join(host->thread, NULL);

    msg_Dbg(host, "HTTP host removed");

    vlc_list_foreach(client, &host->clients, node) {
        msg_Warn(host, "client still connected");
        httpd_ClientDestroy(client);
    }

    assert(vlc_list_is_empty(&host->urls));
    vlc_tls_ServerDelete(host->p_tls);
    net_ListenClose(host->fds);
    vlc_object_delete(host);
    vlc_mutex_unlock(&httpd.mutex);
}

/* register a new url */
httpd_url_t *httpd_UrlNew(httpd_host_t *host, const char *psz_url,
                           const char *psz_user, const char *psz_password)
{
    httpd_url_t *url;

    assert(psz_url);

    vlc_mutex_lock(&host->lock);
    vlc_list_foreach(url, &host->urls, node)
        if (!strcmp(psz_url, url->psz_url)) {
            msg_Warn(host, "cannot add '%s' (url already defined)", psz_url);
            vlc_mutex_unlock(&host->lock);
            return NULL;
        }

    url = malloc(sizeof (*url));
    if (unlikely(url == NULL)) {
        vlc_mutex_unlock(&host->lock);
        return NULL;
    }
    url->psz_url = NULL;
    url->psz_user = NULL;
    url->psz_password = NULL;

    url->host = host;

    vlc_mutex_init(&url->lock);

    url->psz_url = strdup(psz_url);
    if (url->psz_url == NULL)
        goto error;

    url->psz_user = strdup(psz_user ? psz_user : "");
    if (url->psz_user == NULL)
        goto error;

    url->psz_password = strdup(psz_password ? psz_password : "");
    if (url->psz_password == NULL)
        goto error;

    for (int i = 0; i < HTTPD_MSG_MAX; i++) {
        url->catch[i].cb = NULL;
        url->catch[i].p_sys = NULL;
    }

    vlc_list_append(&url->node, &host->urls);
    vlc_mutex_unlock(&host->lock);

    return url;

error:
    free(url->psz_password);
    free(url->psz_user);
    free(url->psz_url);

    free(url);
    return NULL;
}

/* register callback on a url */
int httpd_UrlCatch(httpd_url_t *url, int i_msg, httpd_callback_t cb,
                    httpd_callback_sys_t *p_sys)
{
    vlc_mutex_lock(&url->lock);
    url->catch[i_msg].cb   = cb;
    url->catch[i_msg].p_sys= p_sys;
    vlc_mutex_unlock(&url->lock);

    return VLC_SUCCESS;
}

/* delete a url */
void httpd_UrlDelete(httpd_url_t *url)
{
    httpd_host_t *host = url->host;
    httpd_client_t *client;

    vlc_mutex_lock(&host->lock);
    vlc_list_remove(&url->node);

    free(url->psz_url);
    free(url->psz_user);
    free(url->psz_password);

    vlc_list_foreach(client, &host->clients, node) {
        if (client->url != url)
            continue;

        /* TODO complete it */
        msg_Warn(host, "force closing connections");
        host->client_count--;
        httpd_ClientDestroy(client);
    }
    free(url);
    vlc_mutex_unlock(&host->lock);
}

static void httpd_MsgInit(httpd_message_t *msg)
{
    msg->cl         = NULL;
    msg->i_type     = HTTPD_MSG_NONE;
    msg->i_proto    = HTTPD_PROTO_NONE;
    msg->i_version  = -1; /* FIXME */

    msg->i_status   = 0;

    msg->psz_url    = NULL;
    msg->psz_args   = NULL;

    msg->i_headers  = 0;
    msg->p_headers  = NULL;

    msg->i_body_offset = 0;
    msg->i_body        = 0;
    msg->p_body        = NULL;
}

static void httpd_MsgClean(httpd_message_t *msg)
{
    free(msg->psz_url);
    free(msg->psz_args);
    for (size_t i = 0; i < msg->i_headers; i++) {
        free(msg->p_headers[i].name);
        free(msg->p_headers[i].value);
    }
    free(msg->p_headers);
    free(msg->p_body);
    httpd_MsgInit(msg);
}

const char *httpd_MsgGet(const httpd_message_t *msg, const char *name)
{
    for (size_t i = 0; i < msg->i_headers; i++)
        if (!strcasecmp(msg->p_headers[i].name, name))
            return msg->p_headers[i].value;
    return NULL;
}

void httpd_MsgAdd(httpd_message_t *msg, const char *name, const char *psz_value, ...)
{
    httpd_header *p_tmp = realloc(msg->p_headers, sizeof(httpd_header) * (msg->i_headers + 1));
    if (!p_tmp)
        return;

    msg->p_headers = p_tmp;

    httpd_header *h = &msg->p_headers[msg->i_headers];
    h->name = strdup(name);
    if (!h->name)
        return;

    h->value = NULL;

    va_list args;
    va_start(args, psz_value);
    int ret = us_vasprintf(&h->value, psz_value, args);
    va_end(args);

    if (ret == -1) {
        free(h->name);
        return;
    }

    msg->i_headers++;
}

static void httpd_ClientInit(httpd_client_t *cl, vlc_tick_t now)
{
    cl->i_state = HTTPD_CLIENT_RECEIVING;
    cl->i_activity_date = now;
    cl->i_activity_timeout = VLC_TICK_FROM_SEC(10);
    cl->i_buffer_size = HTTPD_CL_BUFSIZE;
    cl->i_buffer = 0;
    cl->p_buffer = xmalloc(cl->i_buffer_size);
    cl->i_keyframe_wait_to_pass = -1;
    cl->b_stream_mode = false;

    httpd_MsgInit(&cl->query);
    httpd_MsgInit(&cl->answer);
}

char* httpd_ClientIP(const httpd_client_t *cl, char *ip, int *port)
{
    return net_GetPeerAddress(vlc_tls_GetFD(cl->sock), ip, port) ? NULL : ip;
}

char* httpd_ServerIP(const httpd_client_t *cl, char *ip, int *port)
{
    return net_GetSockAddress(vlc_tls_GetFD(cl->sock), ip, port) ? NULL : ip;
}

static void httpd_ClientDestroy(httpd_client_t *cl)
{
    vlc_list_remove(&cl->node);
    vlc_tls_Close(cl->sock);
    httpd_MsgClean(&cl->answer);
    httpd_MsgClean(&cl->query);

    free(cl->p_buffer);
    free(cl);
}

static httpd_client_t *httpd_ClientNew(vlc_tls_t *sock, vlc_tick_t now)
{
    httpd_client_t *cl = malloc(sizeof(httpd_client_t));

    if (!cl) return NULL;

    cl->sock    = sock;
    cl->url     = NULL;

    httpd_ClientInit(cl, now);
    return cl;
}

static
ssize_t httpd_NetRecv (httpd_client_t *cl, uint8_t *p, size_t i_len)
{
    vlc_tls_t *sock = cl->sock;
    struct iovec iov = { .iov_base = p, .iov_len = i_len };
    return sock->ops->readv(sock, &iov, 1);
}

static
ssize_t httpd_NetSend (httpd_client_t *cl, const uint8_t *p, size_t i_len)
{
    vlc_tls_t *sock = cl->sock;
    const struct iovec iov = { .iov_base = (void *)p, .iov_len = i_len };
    return sock->ops->writev(sock, &iov, 1);
}


static const struct
{
    const char name[16];
    int  i_type;
    int  i_proto;
}
msg_type[] =
{
    { "OPTIONS",       HTTPD_MSG_OPTIONS,      HTTPD_PROTO_RTSP },
    { "DESCRIBE",      HTTPD_MSG_DESCRIBE,     HTTPD_PROTO_RTSP },
    { "SETUP",         HTTPD_MSG_SETUP,        HTTPD_PROTO_RTSP },
    { "PLAY",          HTTPD_MSG_PLAY,         HTTPD_PROTO_RTSP },
    { "PAUSE",         HTTPD_MSG_PAUSE,        HTTPD_PROTO_RTSP },
    { "GET_PARAMETER", HTTPD_MSG_GETPARAMETER, HTTPD_PROTO_RTSP },
    { "TEARDOWN",      HTTPD_MSG_TEARDOWN,     HTTPD_PROTO_RTSP },
    { "GET",           HTTPD_MSG_GET,          HTTPD_PROTO_HTTP },
    { "HEAD",          HTTPD_MSG_HEAD,         HTTPD_PROTO_HTTP },
    { "POST",          HTTPD_MSG_POST,         HTTPD_PROTO_HTTP },
};


static int httpd_ClientRecv(httpd_client_t *cl)
{
    int i_len;

    /* ignore leading whites */
    if (cl->query.i_proto == HTTPD_PROTO_NONE && cl->i_buffer == 0) {
        unsigned char c;

        i_len = httpd_NetRecv(cl, &c, 1);

        if (i_len > 0 && !strchr("\r\n\t ", c)) {
            cl->p_buffer[0] = c;
            cl->i_buffer++;
        }
    } else if (cl->query.i_proto == HTTPD_PROTO_NONE) {
        /* enough to see if it's Interleaved RTP over RTSP or RTSP/HTTP */
        i_len = httpd_NetRecv(cl, &cl->p_buffer[cl->i_buffer],
                               7 - cl->i_buffer);
        if (i_len > 0)
            cl->i_buffer += i_len;

        /* The smallest legal request is 7 bytes ("GET /\r\n"),
         * this is the maximum we can ask at this point. */
        if (cl->i_buffer >= 7) {
            if (!memcmp(cl->p_buffer, "HTTP/1.", 7)) {
                cl->query.i_proto = HTTPD_PROTO_HTTP;
                cl->query.i_type  = HTTPD_MSG_ANSWER;
            } else if (!memcmp(cl->p_buffer, "RTSP/1.", 7)) {
                cl->query.i_proto = HTTPD_PROTO_RTSP;
                cl->query.i_type  = HTTPD_MSG_ANSWER;
            } else {
                /* We need the full request line to determine the protocol. */
                cl->query.i_proto = HTTPD_PROTO_HTTP0;
                cl->query.i_type  = HTTPD_MSG_NONE;
            }
        }
    } else if (cl->query.i_body > 0) {
        /* we are reading the body of a request or a channel */
        assert (cl->query.p_body != NULL);
        i_len = httpd_NetRecv(cl, &cl->query.p_body[cl->i_buffer],
                               cl->query.i_body - cl->i_buffer);
        if (i_len > 0)
            cl->i_buffer += i_len;

        if (cl->i_buffer >= cl->query.i_body)
            cl->i_state = HTTPD_CLIENT_RECEIVE_DONE;
    } else for (;;) { /* we are reading a header -> char by char */
        if (cl->i_buffer == cl->i_buffer_size) {
            // Allocate an extra byte for the termination null byte
            uint8_t *newbuf = realloc(cl->p_buffer, cl->i_buffer_size + 1025);
            if (!newbuf) {
                i_len = 0;
                break;
            }

            cl->p_buffer = newbuf;
            cl->i_buffer_size += 1024;
        }

        i_len = httpd_NetRecv (cl, &cl->p_buffer[cl->i_buffer], 1);
        if (i_len <= 0)
            break;

        cl->i_buffer++;

        if ((cl->query.i_proto == HTTPD_PROTO_HTTP0)
                && (cl->p_buffer[cl->i_buffer - 1] == '\n'))
        {
            /* Request line is now complete */
            const char *p = memchr(cl->p_buffer, ' ', cl->i_buffer);
            size_t len;

            assert(cl->query.i_type == HTTPD_MSG_NONE);

            if (!p) { /* no URI: evil guy */
                i_len = 0; /* drop connection */
                break;
            }

            do
                p++; /* skips extra spaces */
            while (*p == ' ');

            p = memchr(p, ' ', ((char *)cl->p_buffer) + cl->i_buffer - p);
            if (!p) { /* no explicit protocol: HTTP/0.9 */
                i_len = 0; /* not supported currently -> drop */
                break;
            }

            do
                p++; /* skips extra spaces ever again */
            while (*p == ' ');

            len = ((char *)cl->p_buffer) + cl->i_buffer - p;
            if (len < 7) /* foreign protocol */
                i_len = 0; /* I don't understand -> drop */
            else if (!memcmp(p, "HTTP/1.", 7)) {
                cl->query.i_proto = HTTPD_PROTO_HTTP;
                cl->query.i_version = atoi(p + 7);
            } else if (!memcmp(p, "RTSP/1.", 7)) {
                cl->query.i_proto = HTTPD_PROTO_RTSP;
                cl->query.i_version = atoi(p + 7);
            } else if (!memcmp(p, "HTTP/", 5)) {
                const uint8_t sorry[] =
                    "HTTP/1.1 505 Unknown HTTP version\r\n\r\n";
                httpd_NetSend(cl, sorry, sizeof(sorry) - 1);
                i_len = 0; /* drop */
            } else if (!memcmp(p, "RTSP/", 5)) {
                const uint8_t sorry[] =
                    "RTSP/1.0 505 Unknown RTSP version\r\n\r\n";
                httpd_NetSend(cl, sorry, sizeof(sorry) - 1);
                i_len = 0; /* drop */
            } else /* yet another foreign protocol */
                i_len = 0;

            if (i_len == 0)
                break;
        }

        if ((cl->i_buffer >= 2 && !memcmp(&cl->p_buffer[cl->i_buffer-2], "\n\n", 2))||
                (cl->i_buffer >= 4 && !memcmp(&cl->p_buffer[cl->i_buffer-4], "\r\n\r\n", 4)))
        {
            char *p;

            /* we have finished the header so parse it and set i_body */
            cl->p_buffer[cl->i_buffer] = '\0';

            if (cl->query.i_type == HTTPD_MSG_ANSWER) {
                /* FIXME:
                 * assume strlen("HTTP/1.x") = 8
                 */
                cl->query.i_status =
                    strtol((char *)&cl->p_buffer[8],
                            &p, 0);
                while (*p == ' ')
                    p++;
            } else {
                p = NULL;
                cl->query.i_type = HTTPD_MSG_NONE;

                for (unsigned i = 0; i < ARRAY_SIZE(msg_type); i++)
                    if (cl->query.i_proto == msg_type[i].i_proto
                     && strncmp((char *)cl->p_buffer, msg_type[i].name,
                                strlen(msg_type[i].name)) == 0) {
                        p = (char *)&cl->p_buffer[strlen(msg_type[i].name) + 1 ];
                        cl->query.i_type = msg_type[i].i_type;
                        break;
                    }

                if (!p) {
                    if (strstr((char *)cl->p_buffer, "HTTP/1."))
                        cl->query.i_proto = HTTPD_PROTO_HTTP;
                    else if (strstr((char *)cl->p_buffer, "RTSP/1."))
                        cl->query.i_proto = HTTPD_PROTO_RTSP;
                } else {
                    char *p2;
                    char *p3;

                    while (*p == ' ')
                        p++;

                    p2 = strchr(p, ' ');
                    if (p2)
                        *p2++ = '\0';

                    if (!strncasecmp(p, (cl->query.i_proto == HTTPD_PROTO_HTTP) ? "http:" : "rtsp:", 5)) {
                        /* Skip hier-part of URL (if present) */
                        p += 5;
                        if (!strncmp(p, "//", 2)) { /* skip authority */
                            /* see RFC3986 §3.2 */
                            p += 2;
                            p += strcspn(p, "/?#");
                        }
                    }
                    else if (!strncasecmp(p, (cl->query.i_proto == HTTPD_PROTO_HTTP) ? "https:" : "rtsps:", 6)) {
                        /* Skip hier-part of URL (if present) */
                        p += 6;
                        if (!strncmp(p, "//", 2)) { /* skip authority */
                            /* see RFC3986 §3.2 */
                            p += 2;
                            p += strcspn(p, "/?#");
                        }
                    }

                    if(cl->query.psz_url == NULL) {
                        cl->query.psz_url = strdup(p);
                        if ((p3 = strchr(cl->query.psz_url, '?')) ) {
                            *p3++ = '\0';
                            cl->query.psz_args = (uint8_t *)strdup(p3);
                        }
                    }
                    p = p2;
                }
            }
            if (p)
                p = strchr(p, '\n');

            if (p) {
                while (*p == '\n' || *p == '\r')
                    p++;

                while (p && *p) {
                    char *line = p;
                    char *eol = p = strchr(p, '\n');
                    char *colon;

                    while (eol && eol >= line && (*eol == '\n' || *eol == '\r'))
                        *eol-- = '\0';

                    if ((colon = strchr(line, ':'))) {
                        *colon++ = '\0';
                        while (*colon == ' ')
                            colon++;
                        httpd_MsgAdd(&cl->query, line, "%s", colon);

                        if (!strcasecmp(line, "Content-Length"))
                            cl->query.i_body = atol(colon);
                    }

                    if (p) {
                        p++;
                        while (*p == '\n' || *p == '\r')
                            p++;
                    }
                }
            }
            if (cl->query.i_body > 0) {
                /* TODO Mhh, handle the case where the client only
                 * sends a request and closes the connection to
                 * mark the end of the body (probably only RTSP) */
                if (cl->query.i_body < 65536)
                    cl->query.p_body = malloc(cl->query.i_body);
                else
                    cl->query.p_body = NULL;
                cl->i_buffer = 0;
                if (!cl->query.p_body) {
                    switch (cl->query.i_proto) {
                        case HTTPD_PROTO_HTTP: {
                            const uint8_t sorry[] = "HTTP/1.1 413 Request Entity Too Large\r\n\r\n";
                            httpd_NetSend(cl, sorry, sizeof(sorry) - 1);
                            break;
                        }
                        case HTTPD_PROTO_RTSP: {
                            const uint8_t sorry[] = "RTSP/1.0 413 Request Entity Too Large\r\n\r\n";
                            httpd_NetSend(cl, sorry, sizeof(sorry) - 1);
                            break;
                        }
                        default:
                            vlc_assert_unreachable();
                    }
                    i_len = 0; /* drop */
                }
                break;
            } else
                cl->i_state = HTTPD_CLIENT_RECEIVE_DONE;
        }
    }

    if (i_len == 0) {
        if (cl->query.i_proto != HTTPD_PROTO_NONE && cl->query.i_type != HTTPD_MSG_NONE) {
            /* connection closed -> end of data */
            if (cl->query.i_body > 0)
                cl->query.i_body = cl->i_buffer;
            cl->i_state = HTTPD_CLIENT_RECEIVE_DONE;
        }
        else
            cl->i_state = HTTPD_CLIENT_DEAD; /* connection failed */
        return 0;
    }

    /* check if the client is to be set to dead */
    if (i_len < 0) {
#if defined(_WIN32)
        if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
        if (errno == EAGAIN)
#endif
            return -1;

        cl->i_state = HTTPD_CLIENT_DEAD; /* connection failed */
        return 0;
    }

    /* XXX: for QT I have to disable timeout. Try to find why */
    if (cl->query.i_proto == HTTPD_PROTO_RTSP)
        cl->i_activity_timeout = 0;

    return 0;
}

static int httpd_ClientSend(httpd_client_t *cl)
{
    int i_len;

    if (cl->i_buffer < 0) {
        /* We need to create the header */
        int i_size = 0;
        char *p;
        const char *psz_status = httpd_ReasonFromCode(cl->answer.i_status);

        i_size = strlen("HTTP/1.") + 10 + 10 + strlen(psz_status) + 5;
        for (size_t i = 0; i < cl->answer.i_headers; i++)
            i_size += strlen(cl->answer.p_headers[i].name) + 2 +
                      strlen(cl->answer.p_headers[i].value) + 2;

        if (cl->i_buffer_size < i_size) {
            cl->i_buffer_size = i_size;
            free(cl->p_buffer);
            cl->p_buffer = xmalloc(i_size);
        }
        p = (char *)cl->p_buffer;

        p += sprintf(p, "%s.%" PRIu8 " %d %s\r\n",
                      cl->answer.i_proto ==  HTTPD_PROTO_HTTP ? "HTTP/1" : "RTSP/1",
                      cl->answer.i_version,
                      cl->answer.i_status, psz_status);
        for (size_t i = 0; i < cl->answer.i_headers; i++)
            p += sprintf(p, "%s: %s\r\n", cl->answer.p_headers[i].name,
                          cl->answer.p_headers[i].value);
        p += sprintf(p, "\r\n");

        cl->i_buffer = 0;
        cl->i_buffer_size = (uint8_t*)p - cl->p_buffer;
    }

    i_len = httpd_NetSend(cl, &cl->p_buffer[cl->i_buffer],
                           cl->i_buffer_size - cl->i_buffer);

    if (i_len < 0) {
#if defined(_WIN32)
        if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
        if (errno == EAGAIN)
#endif
            return -1;

        /* Connection failed, or hung up (EPIPE) */
        cl->i_state = HTTPD_CLIENT_DEAD;
        return 0;
    }

    cl->i_buffer += i_len;

    if (cl->i_buffer >= cl->i_buffer_size) {
        if (cl->answer.i_body == 0  && cl->answer.i_body_offset > 0) {
            /* catch more body data */
            int     i_msg = cl->query.i_type;
            int64_t i_offset = cl->answer.i_body_offset;

            httpd_MsgClean(&cl->answer);
            cl->answer.i_body_offset = i_offset;

            cl->url->catch[i_msg].cb(cl->url->catch[i_msg].p_sys, cl,
                                     &cl->answer, &cl->query);
        }

        if (cl->answer.i_body > 0) {
            /* send the body data */
            free(cl->p_buffer);
            cl->p_buffer = cl->answer.p_body;
            cl->i_buffer_size = cl->answer.i_body;
            cl->i_buffer = 0;

            cl->answer.i_body = 0;
            cl->answer.p_body = NULL;
        } else /* send finished */
            cl->i_state = HTTPD_CLIENT_SEND_DONE;
    }
    return 0;
}

static void httpd_ClientTlsHandshake(httpd_host_t *host, httpd_client_t *cl)
{
    switch (vlc_tls_SessionHandshake(host->p_tls, cl->sock))
    {
        case -1: cl->i_state = HTTPD_CLIENT_DEAD;       break;
        case 0:  cl->i_state = HTTPD_CLIENT_RECEIVING;  break;
        case 1:  cl->i_state = HTTPD_CLIENT_TLS_HS_IN;  break;
        case 2:  cl->i_state = HTTPD_CLIENT_TLS_HS_OUT; break;
    }
}

static bool httpdAuthOk(const char *user, const char *pass, const char *b64)
{
    if (!*user && !*pass)
        return true;

    if (!b64)
        return false;

    if (strncasecmp(b64, "BASIC", 5))
        return false;

    b64 += 5;
    while (*b64 == ' ')
        b64++;

    char *given_user = vlc_b64_decode(b64);
    if (!given_user)
        return false;

    char *given_pass = NULL;
    given_pass = strchr (given_user, ':');
    if (!given_pass)
        goto auth_failed;

    *given_pass++ = '\0';

    if (strcmp (given_user, user))
        goto auth_failed;

    if (strcmp (given_pass, pass))
        goto auth_failed;

    free(given_user);
    return true;

auth_failed:
    free(given_user);
    return false;
}

static void httpdLoop(httpd_host_t *host)
{
    struct pollfd ufd[host->nfd + host->client_count];
    unsigned nfd;
    for (nfd = 0; nfd < host->nfd; nfd++) {
        ufd[nfd].fd = host->fds[nfd];
        ufd[nfd].events = POLLIN;
        ufd[nfd].revents = 0;
    }

    vlc_mutex_lock(&host->lock);
    /* add all socket that should be read/write and close dead connection */
    vlc_tick_t now = vlc_tick_now();
    int delay = -1;
    httpd_client_t *cl;

    int canc = vlc_savecancel();
    vlc_list_foreach(cl, &host->clients, node) {
        int val = -1;

        switch (cl->i_state) {
            case HTTPD_CLIENT_RECEIVING:
                val = httpd_ClientRecv(cl);
                break;
            case HTTPD_CLIENT_SENDING:
                val = httpd_ClientSend(cl);
                break;
            case HTTPD_CLIENT_TLS_HS_IN:
            case HTTPD_CLIENT_TLS_HS_OUT:
                httpd_ClientTlsHandshake(host, cl);
                break;
        }

        if (cl->i_state == HTTPD_CLIENT_DEAD
         || (cl->i_activity_timeout > 0
          && cl->i_activity_date + cl->i_activity_timeout < now)) {
            host->client_count--;
            httpd_ClientDestroy(cl);
            continue;
        }

        if (val == 0) {
            cl->i_activity_date = now;
            delay = 0;
        }

        struct pollfd *pufd = ufd + nfd;
        assert (pufd < ufd + ARRAY_SIZE (ufd));

        pufd->events = pufd->revents = 0;

        switch (cl->i_state) {
            case HTTPD_CLIENT_RECEIVING:
            case HTTPD_CLIENT_TLS_HS_IN:
                pufd->events = POLLIN;
                break;

            case HTTPD_CLIENT_SENDING:
            case HTTPD_CLIENT_TLS_HS_OUT:
                pufd->events = POLLOUT;
                break;

            case HTTPD_CLIENT_RECEIVE_DONE: {
                httpd_message_t *answer = &cl->answer;
                httpd_message_t *query  = &cl->query;

                httpd_MsgInit(answer);

                /* Handle what we received */
                switch (query->i_type) {
                    case HTTPD_MSG_ANSWER:
                        cl->url     = NULL;
                        cl->i_state = HTTPD_CLIENT_DEAD;
                        break;

                    case HTTPD_MSG_OPTIONS:
                        answer->i_type   = HTTPD_MSG_ANSWER;
                        answer->i_proto  = query->i_proto;
                        answer->i_status = 200;
                        answer->i_body = 0;
                        answer->p_body = NULL;

                        httpd_MsgAdd(answer, "Server", "VLC/%s", VERSION);
                        httpd_MsgAdd(answer, "Content-Length", "0");

                        switch(query->i_proto) {
                        case HTTPD_PROTO_HTTP:
                            answer->i_version = 1;
                            httpd_MsgAdd(answer, "Allow", "GET,HEAD,POST,OPTIONS");
                            break;

                        case HTTPD_PROTO_RTSP:
                            answer->i_version = 0;

                            const char *p = httpd_MsgGet(query, "Cseq");
                            if (p)
                                httpd_MsgAdd(answer, "Cseq", "%s", p);
                            p = httpd_MsgGet(query, "Timestamp");
                            if (p)
                                httpd_MsgAdd(answer, "Timestamp", "%s", p);

                            p = httpd_MsgGet(query, "Require");
                            if (p) {
                                answer->i_status = 551;
                                httpd_MsgAdd(query, "Unsupported", "%s", p);
                            }

                            httpd_MsgAdd(answer, "Public", "DESCRIBE,SETUP,"
                                    "TEARDOWN,PLAY,PAUSE,GET_PARAMETER");
                            break;
                        }

                        if (httpd_MsgGet(&cl->query, "Connection") != NULL)
                            httpd_MsgAdd(answer, "Connection", "close");

                        cl->i_buffer = -1;  /* Force the creation of the answer in
                                             * httpd_ClientSend */
                        cl->i_state = HTTPD_CLIENT_SENDING;
                        break;

                    case HTTPD_MSG_NONE:
                        if (query->i_proto == HTTPD_PROTO_NONE) {
                            cl->url = NULL;
                            cl->i_state = HTTPD_CLIENT_DEAD;
                        } else {
                            /* unimplemented */
                            answer->i_proto  = query->i_proto ;
                            answer->i_type   = HTTPD_MSG_ANSWER;
                            answer->i_version= 0;
                            answer->i_status = 501;

                            char *p;
                            answer->i_body = httpd_HtmlError (&p, 501, NULL);
                            answer->p_body = (uint8_t *)p;
                            httpd_MsgAdd(answer, "Content-Length", "%d", answer->i_body);
                            httpd_MsgAdd(answer, "Connection", "close");

                            cl->i_buffer = -1;  /* Force the creation of the answer in httpd_ClientSend */
                            cl->i_state = HTTPD_CLIENT_SENDING;
                        }
                        break;

                    default: {
                        httpd_url_t *url;
                        int i_msg = query->i_type;
                        bool b_auth_failed = false;

                        /* Search the url and trigger callbacks */
                        vlc_list_foreach(url, &host->urls, node) {
                            if (strcmp(url->psz_url, query->psz_url))
                                continue;
                            if (!url->catch[i_msg].cb)
                                continue;

                            if (answer) {
                                b_auth_failed = !httpdAuthOk(url->psz_user,
                                   url->psz_password,
                                   httpd_MsgGet(query, "Authorization")); /* BASIC id */
                                if (b_auth_failed)
                                   break;
                            }

                            if (url->catch[i_msg].cb(url->catch[i_msg].p_sys, cl, answer, query))
                                continue;

                            if (answer->i_proto == HTTPD_PROTO_NONE)
                                cl->i_buffer = cl->i_buffer_size; /* Raw answer from a CGI */
                            else
                                cl->i_buffer = -1;

                            /* only one url can answer */
                            answer = NULL;
                            if (!cl->url)
                                cl->url = url;
                        }

                        if (answer) {
                            answer->i_proto  = query->i_proto;
                            answer->i_type   = HTTPD_MSG_ANSWER;
                            answer->i_version= 0;

                           if (b_auth_failed) {
                                httpd_MsgAdd(answer, "WWW-Authenticate",
                                        "Basic realm=\"VLC stream\"");
                                answer->i_status = 401;
                            } else
                                answer->i_status = 404; /* no url registered */

                            char *p;
                            answer->i_body = httpd_HtmlError (&p, answer->i_status,
                                    query->psz_url);
                            answer->p_body = (uint8_t *)p;

                            cl->i_buffer = -1;  /* Force the creation of the answer in httpd_ClientSend */
                            httpd_MsgAdd(answer, "Content-Length", "%d", answer->i_body);
                            httpd_MsgAdd(answer, "Content-Type", "%s", "text/html");
                            if (httpd_MsgGet(&cl->query, "Connection") != NULL)
                                httpd_MsgAdd(answer, "Connection", "close");
                        }

                        cl->i_state = HTTPD_CLIENT_SENDING;
                    }
                }
                break;
            }

            case HTTPD_CLIENT_SEND_DONE:
                if (!cl->b_stream_mode || cl->answer.i_body_offset == 0) {
                    bool do_close = false;

                    cl->url = NULL;

                    if (cl->query.i_proto != HTTPD_PROTO_HTTP
                     || cl->query.i_version > 0)
                    {
                        const char *psz_connection = httpd_MsgGet(&cl->answer,
                                                                 "Connection");
                        if (psz_connection != NULL)
                            do_close = !strcasecmp(psz_connection, "close");
                    }
                    else
                        do_close = true;

                    if (!do_close) {
                        httpd_MsgClean(&cl->query);
                        httpd_MsgInit(&cl->query);

                        cl->i_buffer = 0;
                        cl->i_buffer_size = 1000;
                        free(cl->p_buffer);
                        // Allocate an extra byte for the null terminating byte
                        cl->p_buffer = xmalloc(cl->i_buffer_size + 1);
                        cl->i_state = HTTPD_CLIENT_RECEIVING;
                    } else
                        cl->i_state = HTTPD_CLIENT_DEAD;
                    httpd_MsgClean(&cl->answer);
                } else {
                    int64_t i_offset = cl->answer.i_body_offset;
                    httpd_MsgClean(&cl->answer);

                    cl->answer.i_body_offset = i_offset;
                    free(cl->p_buffer);
                    cl->p_buffer = NULL;
                    cl->i_buffer = 0;
                    cl->i_buffer_size = 0;

                    cl->i_state = HTTPD_CLIENT_WAITING;
                }
                break;

            case HTTPD_CLIENT_WAITING: {
                int64_t i_offset = cl->answer.i_body_offset;
                int i_msg = cl->query.i_type;

                httpd_MsgInit(&cl->answer);
                cl->answer.i_body_offset = i_offset;

                cl->url->catch[i_msg].cb(cl->url->catch[i_msg].p_sys, cl,
                        &cl->answer, &cl->query);
                if (cl->answer.i_type != HTTPD_MSG_NONE) {
                    /* we have new data, so re-enter send mode */
                    cl->i_buffer      = 0;
                    cl->p_buffer      = cl->answer.p_body;
                    cl->i_buffer_size = cl->answer.i_body;
                    cl->answer.p_body = NULL;
                    cl->answer.i_body = 0;
                    cl->i_state = HTTPD_CLIENT_SENDING;
                }
            }
        }

        pufd->fd = vlc_tls_GetPollFD(cl->sock, &pufd->events);

        if (pufd->events != 0)
            nfd++;
        /* we will wait 20ms (not too big) if HTTPD_CLIENT_WAITING */
        else if (delay != 0)
            delay = 20;
    }
    vlc_mutex_unlock(&host->lock);
    vlc_restorecancel(canc);

    while (poll(ufd, nfd, delay) < 0)
    {
        if (errno != EINTR)
            msg_Err(host, "polling error: %s", vlc_strerror_c(errno));
    }

    canc = vlc_savecancel();
    vlc_mutex_lock(&host->lock);

    /* Handle client sockets */
    now = vlc_tick_now();
    nfd = host->nfd;

    /* Handle server sockets (accept new connections) */
    for (nfd = 0; nfd < host->nfd; nfd++) {
        int fd = ufd[nfd].fd;

        assert (fd == host->fds[nfd]);

        if (ufd[nfd].revents == 0)
            continue;

        /* */
        fd = vlc_accept (fd, NULL, NULL, true);
        if (fd == -1)
            continue;
        setsockopt (fd, SOL_SOCKET, SO_REUSEADDR,
                &(int){ 1 }, sizeof(int));

        vlc_tls_t *sk = vlc_tls_SocketOpen(fd);
        if (unlikely(sk == NULL))
        {
            vlc_close(fd);
            continue;
        }

        if (host->p_tls != NULL)
        {
            const char *alpn[] = { "http/1.1", NULL };
            vlc_tls_t *tls;

            tls = vlc_tls_ServerSessionCreate(host->p_tls, sk, alpn);
            if (tls == NULL)
            {
                vlc_tls_SessionDelete(sk);
                continue;
            }
            sk = tls;
        }

        cl = httpd_ClientNew(sk, now);

        if (host->p_tls != NULL)
            cl->i_state = HTTPD_CLIENT_TLS_HS_OUT;

        host->client_count++;
        vlc_list_append(&cl->node, &host->clients);
    }

    vlc_mutex_unlock(&host->lock);
    vlc_restorecancel(canc);
}

static void* httpd_HostThread(void *data)
{
    httpd_host_t *host = data;

    while (atomic_load_explicit(&host->ref, memory_order_relaxed) > 0)
        httpdLoop(host);
    return NULL;
}

int httpd_StreamSetHTTPHeaders(httpd_stream_t * p_stream,
                               const httpd_header *p_headers, size_t i_headers)
{
    if (!p_stream)
        return VLC_EGENERIC;

    vlc_mutex_lock(&p_stream->lock);
    if (p_stream->p_http_headers) {
        for (size_t i = 0; i < p_stream->i_http_headers; i++) {
            free(p_stream->p_http_headers[i].name);
            free(p_stream->p_http_headers[i].value);
        }
        free(p_stream->p_http_headers);
        p_stream->p_http_headers = NULL;
        p_stream->i_http_headers = 0;
    }

    if (!p_headers || !i_headers) {
        vlc_mutex_unlock(&p_stream->lock);
        return VLC_SUCCESS;
    }

    p_stream->p_http_headers = vlc_alloc(i_headers, sizeof(httpd_header));
    if (!p_stream->p_http_headers) {
        vlc_mutex_unlock(&p_stream->lock);
        return VLC_ENOMEM;
    }

    size_t j = 0;
    for (size_t i = 0; i < i_headers; i++) {
        if (unlikely(!p_headers[i].name || !p_headers[i].value))
            continue;

        p_stream->p_http_headers[j].name = strdup(p_headers[i].name);
        p_stream->p_http_headers[j].value = strdup(p_headers[i].value);

        if (unlikely(!p_stream->p_http_headers[j].name ||
                      !p_stream->p_http_headers[j].value)) {
            free(p_stream->p_http_headers[j].name);
            free(p_stream->p_http_headers[j].value);
            break;
        }
        j++;
    }
    p_stream->i_http_headers = j;
    vlc_mutex_unlock(&p_stream->lock);
    return VLC_SUCCESS;
}
