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

/**
 * \defgroup http_msg Messages
 * HTTP messages, header formatting and parsing
 * \ingroup http
 * @{
 * \file message.h
 */

struct vlc_http_msg;
struct block_t;
struct vlc_http_cookie_jar_t;

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
void vlc_http_msg_destroy(struct vlc_http_msg *);

/**
 * Formats a header field.
 *
 * Adds an HTTP message header to an HTTP request or response.
 * All headers must be formatted before the message is sent.
 *
 * @param name header field name
 * @param fmt printf-style format string
 * @return 0 on success, -1 on error (out of memory)
 */
int vlc_http_msg_add_header(struct vlc_http_msg *, const char *name,
                            const char *fmt, ...) VLC_FORMAT(3,4);

/**
 * Sets the agent field.
 *
 * Sets the User-Agent or Server header field.
 */
int vlc_http_msg_add_agent(struct vlc_http_msg *, const char *);

/**
 * Gets the agent field.
 *
 * Gets the User-Agent or Server header field.
 */
const char *vlc_http_msg_get_agent(const struct vlc_http_msg *);

/**
 * Parses a timestamp header field.
 *
 * @param name header field name
 * @return a timestamp value, or -1 on error.
 */
time_t vlc_http_msg_get_time(const struct vlc_http_msg *, const char *name);

/**
 * Adds a timestamp header field.
 *
 * @param name header field name
 * @param t pointer to timestamp
 * @return 0 on success, -1 on error (errno is set accordingly)
 */
int vlc_http_msg_add_time(struct vlc_http_msg *, const char *name,
                          const time_t *t);

/**
 * Adds a Date header field.
 */
int vlc_http_msg_add_atime(struct vlc_http_msg *);

/**
 * Gets message date.
 *
 * Extracts the original date of the message from the HTTP Date header.
 *
 * @return a time value on success, -1 on error.
 */
time_t vlc_http_msg_get_atime(const struct vlc_http_msg *);

/**
 * Gets resource date.
 *
 * Extracts the last modification date of the message content from the HTTP
 * Last-Modified header.
 *
 * @return a time value on success, -1 on error.
 */
time_t vlc_http_msg_get_mtime(const struct vlc_http_msg *);

/**
 * Gets retry timeout.
 *
 * Extracts the time (in seconds) until the expiration of the "retry-after"
 * time-out in the HTTP message. If the header value is an absolute date, it
 * is converted relative to the current time.
 *
 * @return the time in seconds, zero if the date is overdue or on error.
 */
unsigned vlc_http_msg_get_retry_after(const struct vlc_http_msg *);

void vlc_http_msg_get_cookies(const struct vlc_http_msg *,
                              struct vlc_http_cookie_jar_t *,
                              const char *host, const char *path);
int vlc_http_msg_add_cookies(struct vlc_http_msg *,
                             struct vlc_http_cookie_jar_t *);

char *vlc_http_msg_get_basic_realm(const struct vlc_http_msg *);

/**
 * Adds Basic credentials.
 *
 * Formats a plain username and password pair using HTTP Basic (RFC7617)
 * syntax.
 *
 * @param proxy true for proxy authentication,
 *              false for origin server authentication
 * @param username null-terminated username
 * @param password null-terminated password
 * @return 0 on success, -1 on out-of-memory (ENOMEM) or if username or
 * password are invalid (EINVAL).
 */
int vlc_http_msg_add_creds_basic(struct vlc_http_msg *, bool proxy,
                                 const char *username, const char *password);


/**
 * Looks up an header field.
 *
 * Finds an HTTP header field by (case-insensitive) name inside an HTTP
 * message header. If the message has more than one matching field, their value
 * are folded (as permitted by protocol specifications).
 *
 * @return header field value (valid until message is destroyed),
 *         or NULL if no fields matched
 */
const char *vlc_http_msg_get_header(const struct vlc_http_msg *,
                                    const char *name);

/**
 * Gets response status code.
 *
 * @return status code (e.g. 404), or negative if request
 */
int vlc_http_msg_get_status(const struct vlc_http_msg *m);

/**
 * Gets request method.
 *
 * @return request method (e.g. "GET"), or NULL if response
 */
const char *vlc_http_msg_get_method(const struct vlc_http_msg *);

/**
 * Gets request scheme.
 *
 * @return request scheme (e.g. "https"), or NULL if absent
 */
const char *vlc_http_msg_get_scheme(const struct vlc_http_msg *);

/**
 * Gets request authority.
 *
 * @return request authority (e.g. "www.example.com:8080"),
 *         or NULL if response
 */
const char *vlc_http_msg_get_authority(const struct vlc_http_msg *);

/**
 * Gets request absolute path.
 *
 * @return request absolute path (e.g. "/index.html"), or NULL if absent
 */
const char *vlc_http_msg_get_path(const struct vlc_http_msg *);

/**
 * Looks up a token in a header field.
 *
 * Finds the first occurence of a token within a HTTP field header.
 *
 * @param field HTTP header field name
 * @param token HTTP token name
 * @return the first byte of the token if found, NULL if not found.
 */
const char *vlc_http_msg_get_token(const struct vlc_http_msg *,
                                   const char *field, const char *token);

/**
 * Finds next token.
 *
 * Finds the following token in a HTTP header field value.
 *
 * @return First character of the following token,
 *         or NULL if there are no further tokens
 */
const char *vlc_http_next_token(const char *);

/**
 * Gets HTTP payload length.
 *
 * Determines the total length (in bytes) of the payload associated with the
 * HTTP message.
 *
 * @return byte length, or (uintmax_t)-1 if unknown.
 */
uintmax_t vlc_http_msg_get_size(const struct vlc_http_msg *);

/**
 * Gets next response headers.
 *
 * Discards the current response headers and gets the next set of response
 * headers for the same request. This is intended for HTTP 1xx continuation
 * responses and for message trailers.
 *
 * @param m current response headers (destroyed by the call)
 *
 * @return next response headers or NULL on error
 */
struct vlc_http_msg *vlc_http_msg_iterate(struct vlc_http_msg *) VLC_USED;

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
 *         NULL if the parameter was NULL, or NULL on error
 */
struct vlc_http_msg *vlc_http_msg_get_final(struct vlc_http_msg *) VLC_USED;

/**
 * Receives HTTP data.
 *
 * Dequeues the next block of data from an HTTP message. If no pending data has
 * been received, waits until data is received, the stream ends or the
 * underlying connection fails.
 *
 * @return data block
 * @retval NULL on end-of-stream
 * @retval vlc_http_error on fatal error
 */
struct block_t *vlc_http_msg_read(struct vlc_http_msg *) VLC_USED;

/** @} */

/**
 * \defgroup http_stream Streams
 * \ingroup http_connmgr
 *
 * HTTP request/response streams
 *
 * A stream is initiated by a client-side request header. It includes a
 * final response header, possibly preceded by one or more continuation
 * response headers. After the response header, a stream usually carries
 * a response payload.
 *
 * A stream may also carry a request payload (this is not supported so far).
 *
 * The HTTP stream constitutes the interface between an HTTP connection and
 * the higher-level HTTP messages layer.
 * @{
 */

struct vlc_http_stream;

/**
 * Error pointer value
 *
 * This is an error value for some HTTP functions that can return NULL in
 * non-error circumstances. Another return value is necessary to express
 * error/failure, which this is.
 * This compares different to NULL and to any valid pointer.
 *
 * @warning Dereferencing this pointer is undefined.
 */
extern void *const vlc_http_error;

void vlc_http_msg_attach(struct vlc_http_msg *m, struct vlc_http_stream *s);
struct vlc_http_msg *vlc_http_msg_get_initial(struct vlc_http_stream *s)
VLC_USED;

/** HTTP stream callbacks
 *
 * Connection-specific callbacks for stream manipulation
 */
struct vlc_http_stream_cbs
{
    struct vlc_http_msg *(*read_headers)(struct vlc_http_stream *);
    struct block_t *(*read)(struct vlc_http_stream *);
    void (*close)(struct vlc_http_stream *, bool abort);
};

/** HTTP stream */
struct vlc_http_stream
{
    const struct vlc_http_stream_cbs *cbs;
};

/**
 * Reads one message header.
 *
 * Reads the next message header of an HTTP stream from the network.
 * There is always exactly one request header per stream. There is usually
 * one response header per stream, except for continuation (1xx) headers.
 *
 * @warning The caller is responsible for reading headers at appropriate
 * times as intended by the protocol. Failure to do so may result in protocol
 * dead lock, and/or (HTTP 1.x) connection failure.
 */
static inline
struct vlc_http_msg *vlc_http_stream_read_headers(struct vlc_http_stream *s)
{
    return s->cbs->read_headers(s);
}

/**
 * Reads message payload data.
 *
 * Reads the next block of data from the message payload of an HTTP stream.
 *
 * @return a block of data (use block_Release() to free it)
 * @retval NULL The end of the stream was reached.
 * @retval vlc_http_error The stream encountered a fatal error.
 */
static inline struct block_t *vlc_http_stream_read(struct vlc_http_stream *s)
{
    return s->cbs->read(s);
}

/**
 * Closes an HTTP stream.
 *
 * Releases all resources associated or held by an HTTP stream. Any unread
 * header or data is discarded.
 */
static inline void vlc_http_stream_close(struct vlc_http_stream *s, bool abort)
{
    s->cbs->close(s, abort);
}

/** @} */

/**
 * Formats an HTTP 1.1 message header.
 *
 * Formats an message header in HTTP 1.x format, using HTTP version 1.1.
 *
 * @param m message to format/serialize
 * @param lenp location to write the length of the formatted message in bytes
 *             [OUT]
 * @param proxied whether the message is meant for sending to a proxy rather
 *                than an origin (only relevant for requests)
 * @return A heap-allocated nul-terminated string or *lenp bytes,
 *         or NULL on error
 */
char *vlc_http_msg_format(const struct vlc_http_msg *m, size_t *restrict lenp,
                          bool proxied) VLC_USED;

/**
 * Parses an HTTP 1.1 message header.
 */
struct vlc_http_msg *vlc_http_msg_headers(const char *msg) VLC_USED;

struct vlc_h2_frame;

/**
 * Formats an HTTP 2.0 HEADER frame.
 */
struct vlc_h2_frame *vlc_http_msg_h2_frame(const struct vlc_http_msg *m,
                                           uint_fast32_t stream_id, bool eos);

/**
 * Parses an HTTP 2.0 header table.
 */
struct vlc_http_msg *vlc_http_msg_h2_headers(unsigned count,
                                             const char *const headers[][2]);
