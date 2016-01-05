/*****************************************************************************
 * file_test.c: HTTP file download test
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

#undef NDEBUG

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_http.h>
#include "file.h"
#include "message.h"

static const char url[] = "https://www.example.com:8443/dir/file.ext?a=b";
static const char ua[] = PACKAGE_NAME "/" PACKAGE_VERSION " (test suite)";

static const char *replies[2] = { NULL, NULL };
static uintmax_t offset = 0;
static bool etags = false;

static vlc_http_cookie_jar_t *jar;

int main(void)
{
    struct vlc_http_file *f;
    char *str;

    jar = vlc_http_cookies_new();

    /* Request failure test */
    f = vlc_http_file_create(NULL, url, ua, NULL);
    assert(f != NULL);
    vlc_http_file_seek(f, 0);
    assert(vlc_http_file_get_status(f) < 0);
    assert(vlc_http_file_get_redirect(f) == NULL);
    assert(vlc_http_file_get_size(f) == (uintmax_t)-1);
    assert(!vlc_http_file_can_seek(f));
    assert(vlc_http_file_get_type(f) == NULL);
    assert(vlc_http_file_read(f) == NULL);
    vlc_http_file_destroy(f);

    /* Non-seekable stream test */
    replies[0] = "HTTP/1.1 200 OK\r\n"
                 "ETag: \"foobar42\"\r\n"
                 "Content-Type: video/mpeg\r\n"
                 "\r\n";

    offset = 0;
    etags = true;
    f = vlc_http_file_create(NULL, url, ua, NULL);
    assert(f != NULL);
    assert(vlc_http_file_get_status(f) == 200);
    assert(!vlc_http_file_can_seek(f));
    assert(vlc_http_file_get_size(f) == (uintmax_t)-1);
    str = vlc_http_file_get_type(f);
    assert(str != NULL && !strcmp(str, "video/mpeg"));
    free(str);

    /* Seek failure */
    replies[0] = "HTTP/1.1 200 OK\r\nETag: \"foobar42\"\r\n\r\n";

    assert(vlc_http_file_seek(f, offset = 1234) < 0);
    vlc_http_file_destroy(f);

    /* Seekable file test */
    replies[0] = "HTTP/1.1 206 Partial Content\r\n"
                 "Content-Range: bytes 0-2344/2345\r\n"
                 "ETag: W/\"foobar42\"\r\n"
                 "Last-Modified: Mon, 21 Oct 2013 20:13:22 GMT\r\n"
                 "\r\n";

    offset = 0;
    f = vlc_http_file_create(NULL, url, ua, NULL);
    assert(f != NULL);
    assert(vlc_http_file_can_seek(f));
    assert(vlc_http_file_get_size(f) == 2345);
    assert(vlc_http_file_read(f) == NULL);

    /* Seek success */
    replies[0] = "HTTP/1.1 206 Partial Content\r\n"
                 "Content-Range: bytes 1234-3455/3456\r\n"
                 "ETag: W/\"foobar42\"\r\n"
                 "Last-Modified: Mon, 21 Oct 2013 20:13:22 GMT\r\n"
                 "\r\n";
    assert(vlc_http_file_seek(f, offset = 1234) == 0);
    assert(vlc_http_file_can_seek(f));
    assert(vlc_http_file_get_size(f) == 3456);
    assert(vlc_http_file_read(f) == NULL);

    /* Seek too far */
    replies[0] = "HTTP/1.1 416 Range Not Satisfiable\r\n"
                 "Content-Range: bytes */4567\r\n"
                 "ETag: W/\"foobar42\"\r\n"
                 "Last-Modified: Mon, 21 Oct 2013 20:13:22 GMT\r\n"
                 "\r\n";
    vlc_http_file_seek(f, offset = 5678);
    assert(vlc_http_file_can_seek(f));
    assert(vlc_http_file_get_size(f) == 4567);
    assert(vlc_http_file_read(f) == NULL);
    vlc_http_file_destroy(f);

    /* Redirect */
    replies[0] = "HTTP/1.1 301 Permanent Redirect\r\n"
                 "Location: /somewhere/else/#here\r\n"
                 "\r\n";

    offset = 0;
    f = vlc_http_file_create(NULL, url, ua, NULL);
    assert(f != NULL);
    assert(!vlc_http_file_can_seek(f));
    assert(vlc_http_file_get_size(f) == (uintmax_t)-1);
    str = vlc_http_file_get_redirect(f);
    assert(str != NULL
        && !strcmp(str, "https://www.example.com:8443/somewhere/else/"));
    free(str);
    vlc_http_file_destroy(f);

    /* Continuation */
    replies[0] = "HTTP/1.1 100 Standby\r\n"
                 "\r\n";
    replies[1] = "HTTP/1.1 200 OK\r\n"
                 "Content-Length: 9999\r\n"
                 "\r\n";
    offset = 0;
    f = vlc_http_file_create(NULL, url, ua, NULL);
    assert(f != NULL);
    assert(vlc_http_file_get_size(f) == 9999);
    assert(vlc_http_file_get_redirect(f) == NULL);
    vlc_http_file_destroy(f);

    /* No entity tags */
    replies[0] = "HTTP/1.1 206 Partial Content\r\n"
                 "Content-Range: bytes 0-2344/2345\r\n"
                 "Last-Modified: Mon, 21 Oct 2013 20:13:22 GMT\r\n"
                 "\r\n";

    offset = 0;
    etags = false;
    f = vlc_http_file_create(NULL, url, ua, NULL);
    assert(f != NULL);
    assert(vlc_http_file_can_seek(f));

    replies[0] = "HTTP/1.1 206 Partial Content\r\n"
                 "Content-Range: bytes 1234-3455/3456\r\n"
                 "Last-Modified: Mon, 21 Oct 2013 20:13:22 GMT\r\n"
                 "\r\n";
    assert(vlc_http_file_seek(f, offset = 1234) == 0);
    vlc_http_file_destroy(f);

    /* Invalid responses */
    replies[0] = "HTTP/1.1 206 Partial Content\r\n"
                 "Content-Type: multipart/byteranges\r\n"
                 "\r\n";
    offset = 0;

    f = vlc_http_file_create(NULL, url, ua, NULL);
    assert(f != NULL);
    assert(vlc_http_file_get_size(f) == (uintmax_t)-1);

    replies[0] = "HTTP/1.1 206 Partial Content\r\n"
                 "Content-Range: seconds 60-120/180\r\n"
                 "\r\n";
    assert(vlc_http_file_seek(f, 0) == -1);

    /* Incomplete range */
    replies[0] = "HTTP/1.1 206 Partial Content\r\n"
                 "Content-Range: bytes 0-1233/*\r\n"
                 "\r\n";
    assert(vlc_http_file_seek(f, 0) == 0);
    assert(vlc_http_file_get_size(f) == 1234);

    /* Extraneous range */
    replies[0] = "HTTP/1.1 200 OK\r\n"
                 "Content-Range: bytes 0-1233/1234\r\n"
                 "\r\n";
    assert(vlc_http_file_seek(f, 0) == 0);
    assert(vlc_http_file_get_size(f) == (uintmax_t)-1);

    vlc_http_file_destroy(f);

    /* Dummy API calls */
    f = vlc_http_file_create(NULL, "ftp://localhost/foo", NULL, NULL);
    assert(f == NULL);
    f = vlc_http_file_create(NULL, "/foo", NULL, NULL);
    assert(f == NULL);
    f = vlc_http_file_create(NULL, "http://www.example.com", NULL, NULL);
    assert(f != NULL);
    vlc_http_file_destroy(f);

    vlc_http_cookies_destroy(jar);
    return 0;
}

/* Callback for vlc_http_msg_h2_frame */
#include "h2frame.h"

struct vlc_h2_frame *
vlc_h2_frame_headers(uint_fast32_t id, uint_fast32_t mtu, bool eos,
                     unsigned count, const char *const tab[][2])
{
    (void) id; (void) mtu; (void) count, (void) tab;
    assert(!eos);
    return NULL;
}

/* Callback for the HTTP request */
#include "connmgr.h"

static struct vlc_http_stream stream;

static struct vlc_http_msg *stream_read_headers(struct vlc_http_stream *s)
{
    assert(s == &stream);

    /* return next reply */
    struct vlc_http_msg *m = NULL;
    const char *answer = replies[0];

    if (answer != NULL)
    {
        m = vlc_http_msg_headers(answer);
        assert(m != NULL);
        vlc_http_msg_attach(m, s);
    }

    memmove(replies, replies + 1, sizeof (replies) - sizeof (replies[0]));
    replies[(sizeof (replies) / sizeof (replies[0])) - 1] = NULL;

    return m;
}

static struct block_t *stream_read(struct vlc_http_stream *s)
{
    assert(s == &stream);
    return NULL;
}

static void stream_close(struct vlc_http_stream *s, bool abort)
{
    assert(s == &stream);
    assert(!abort);
}

static const struct vlc_http_stream_cbs stream_callbacks =
{
    stream_read_headers,
    stream_read,
    stream_close,
};

static struct vlc_http_stream stream = { &stream_callbacks };

struct vlc_http_msg *vlc_http_mgr_request(struct vlc_http_mgr *mgr, bool https,
                                          const char *host, unsigned port,
                                          const struct vlc_http_msg *req)
{
    const char *str;
    char *end;

    assert(https);
    assert(mgr == NULL);
    assert(!strcmp(host, "www.example.com"));
    assert(port == 8443);

    str = vlc_http_msg_get_method(req);
    assert(!strcmp(str, "GET"));
    str = vlc_http_msg_get_scheme(req);
    assert(!strcmp(str, "https"));
    str = vlc_http_msg_get_authority(req);
    assert(!strcmp(str, "www.example.com:8443"));
    str = vlc_http_msg_get_path(req);
    assert(!strcmp(str, "/dir/file.ext?a=b"));
    str = vlc_http_msg_get_agent(req);
    assert(!strcmp(str, ua));
    str = vlc_http_msg_get_header(req, "Referer");
    assert(str == NULL);
    str = vlc_http_msg_get_header(req, "Accept");
    assert(str == NULL || strstr(str, "*/*") != NULL);
    str = vlc_http_msg_get_header(req, "Accept-Language");
    assert(str == NULL || strstr(str, "*") != NULL);

    str = vlc_http_msg_get_header(req, "Range");
    assert(str != NULL && !strncmp(str, "bytes=", 6)
        && strtoul(str + 6, &end, 10) == offset && *end == '-');

    time_t mtime = vlc_http_msg_get_time(req, "If-Unmodified-Since");
    str = vlc_http_msg_get_header(req, "If-Match");

    if (etags)
    {
        if (offset != 0)
            assert(str != NULL && !strcmp(str, "\"foobar42\""));
        else
        if (str != NULL)
            assert(strcmp(str, "*") || strcmp(str, "\"foobar42\""));
    }
    else
    {
        if (offset != 0)
            assert(mtime == 1382386402);
    }

    return vlc_http_msg_get_initial(&stream);
}

int vlc_http_mgr_send_cookies(struct vlc_http_mgr *mgr,
                              struct vlc_http_msg *req)
{
    assert(mgr == NULL);
    return vlc_http_msg_add_cookies(req, jar);
}

void vlc_http_mgr_recv_cookies(struct vlc_http_mgr *mgr, bool https,
                               const char *host, const char *path,
                               const struct vlc_http_msg *resp)
{
    assert(mgr == NULL);
    assert(https);
    assert(!strcmp(host, "www.example.com"));
    assert(!strcmp(path, "/dir/file.ext?a=b"));
    vlc_http_msg_get_cookies(resp, jar, https, host, path);
}
