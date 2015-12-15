/*****************************************************************************
 * message.h: HTTP request/response
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

#include <stdint.h>

struct vlc_http_msg;
struct block_t;

/**
 * Creates an HTTP request.
 *
 * Allocates an HTTP request message.
 *
 * @param method request method (e.g. "GET")
 * @param scheme protocol scheme (e.g. "https")
 * @param authority target host (e.g. "www.example.com:8080")
 * @param path request path (e.g. "/dir/page.html")
 * @return an HTTP stream or NULL on allocation failure
 */
struct vlc_http_msg *
vlc_http_req_create(const char *method, const char *scheme,
                    const char *authority, const char *path) VLC_USED;

/**
 * Creates an HTTP response.
 *
 * Allocates an HTTP response message.
 *
 * @param status HTTP status code
 * @return an HTTP stream or NULL on allocation failure
 */
struct vlc_http_msg *vlc_http_resp_create(unsigned status) VLC_USED;

/**
 * Destroys an HTTP message.
 */
void vlc_http_msg_destroy(struct vlc_http_msg *m);

/**
 * Formats a header.
 *
 * Adds an HTTP message header to an HTTP request or response.
 * All headers must be formatted before the message is sent.
 *
 * @param name header name
 * @return 0 on success, -1 on error (out of memory)
 */
int vlc_http_msg_add_header(struct vlc_http_msg *m, const char *name,
                            const char *fmt, ...) VLC_FORMAT(3,4);

/**
 * Sets the agent header.
 *
 * Sets the User-Agent or Server header.
 */
int vlc_http_msg_add_agent(struct vlc_http_msg *m, const char *str);

const char *vlc_http_msg_get_agent(const struct vlc_http_msg *m);

/**
 * Parses a timestamp header.
 *
 * @param name header field name
 * @return a timestamp value, or -1 on error.
 */
time_t vlc_http_msg_get_time(const struct vlc_http_msg *m, const char *name);

/**
 * Adds a timestamp header.
 *
 * @param name header field name
 * @param t pointer to timestamp
 * @return 0 on success, -1 on error (errno is set accordingly)
 */
int vlc_http_msg_add_time(struct vlc_http_msg *m, const char *name,
                          const time_t *t);

/**
 * Adds a Date header.
 */
int vlc_http_msg_add_atime(struct vlc_http_msg *m);

/**
 * Gets message date.
 *
 * Extracts the original date of the message from the HTTP Date header.
 *
 * @return a time value on success, -1 on error.
 */
time_t vlc_http_msg_get_atime(const struct vlc_http_msg *m);

/**
 * Gets resource date.
 *
 * Extracts the last modification date of the message content from the HTTP
 * Last-Modified header.
 *
 * @return a time value on success, -1 on error.
 */
time_t vlc_http_msg_get_mtime(const struct vlc_http_msg *m);

/**
 * Gets retry timeout.
 *
 * Exrtacts the time (in seconds) until the expiration of the "retry-after"
 * time-out in the HTTP message. If the header value is an absolute date, it
 * is converted relative to the current time.
 *
 * @return the time in seconds, zero if the date is overdue or on error.
 */
unsigned vlc_http_msg_get_retry_after(const struct vlc_http_msg *m);

/**
 * Looks up an HTTP header.
 *
 * Finds an HTTP header by (case-insensitive) name inside an HTTP message.
 * If the message has more than one matching header, their value are folded
 * (as permitted by specifications).
 *
 * @return header value (valid until message is destroyed),
 *         or NULL if no headers matched
 */
const char *vlc_http_msg_get_header(const struct vlc_http_msg *m,
                                    const char *name);

int vlc_http_msg_get_status(const struct vlc_http_msg *m);
const char *vlc_http_msg_get_method(const struct vlc_http_msg *m);
const char *vlc_http_msg_get_scheme(const struct vlc_http_msg *m);
const char *vlc_http_msg_get_authority(const struct vlc_http_msg *m);
const char *vlc_http_msg_get_path(const struct vlc_http_msg *m);

/**
 * Gets HTTP payload length.
 *
 * @return byte length, or (uintmax_t)-1 if unknown.
 */
uintmax_t vlc_http_msg_get_size(const struct vlc_http_msg *m);

/**
 * Gets next response headers.
 *
 * Discards the current response headers and gets the next set of response
 * headers for the same request. This is intended for HTTP 1xx continuation
 * responses and for message trailers.
 *
 * @param m current response headers (destroyed by the call)
 *
 * @return next response headers or NULL on error.
 */
struct vlc_http_msg *vlc_http_msg_iterate(struct vlc_http_msg *m) VLC_USED;

/**
 * Gets final response headers.
 *
 * Skips HTTP 1xx continue headers until a final set of response headers is
 * received. This is a convenience wrapper around vlc_http_msg_iterate() for
 * use when continuation headers are not useful (e.g. GET or CONNECT).
 *
 * @param m current response headers or NULL
 *
 * @return the final response headers (m if it was already final),
 *         NULL if the parameter was NULL, or NULL on error.
 */
struct vlc_http_msg *vlc_http_msg_get_final(struct vlc_http_msg *m) VLC_USED;

/**
 * Receives HTTP data.
 *
 * Dequeues the next block of data from an HTTP message. If no pending data has
 * been received, waits until data is received, the stream ends or the
 * underlying connection fails.
 *
 * @return data block, or NULL on end-of-stream or error
 */
struct block_t *vlc_http_msg_read(struct vlc_http_msg *m) VLC_USED;

/* Interfaces to lower layers */
struct vlc_http_stream;

void vlc_http_msg_attach(struct vlc_http_msg *m, struct vlc_http_stream *s);

struct vlc_http_stream_cbs
{
    struct vlc_http_msg *(*read_headers)(struct vlc_http_stream *);
    struct block_t *(*read)(struct vlc_http_stream *);
    void (*close)(struct vlc_http_stream *, bool abort);
};

struct vlc_http_stream
{
    const struct vlc_http_stream_cbs *cbs;
};

static inline
struct vlc_http_msg *vlc_http_stream_read_headers(struct vlc_http_stream *s)
{
    return s->cbs->read_headers(s);
}

static inline struct block_t *vlc_http_stream_read(struct vlc_http_stream *s)
{
    return s->cbs->read(s);
}

static inline void vlc_http_stream_close(struct vlc_http_stream *s, bool abort)
{
    s->cbs->close(s, abort);
}

char *vlc_http_msg_format(const struct vlc_http_msg *m, size_t *) VLC_USED;
struct vlc_http_msg *vlc_http_msg_headers(const char *msg) VLC_USED;

struct vlc_h2_frame;

struct vlc_h2_frame *vlc_http_msg_h2_frame(const struct vlc_http_msg *m,
                                           uint_fast32_t stream_id, bool eos);
struct vlc_http_msg *vlc_http_msg_h2_headers(unsigned n, char *hdrs[][2]);
