/*****************************************************************************
 * chunked.c: HTTP 1.1 chunked encoding
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
# include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_tls.h> /* TODO: remove this */

#include "message.h"
#include "conn.h"

struct vlc_chunked_stream
{
    struct vlc_http_stream stream;
    struct vlc_http_stream *parent;
    struct vlc_tls *tls;
    uintmax_t chunk_length;
    bool eof;
    bool error;
};

static void *vlc_chunked_fatal(struct vlc_chunked_stream *s)
{
    s->error = true;
    return vlc_http_error;
}

static struct vlc_http_msg *vlc_chunked_wait(struct vlc_http_stream *stream)
{
    /* Request trailers are not supported so far.
     * There cannot be headers during chunked encoding. */
    (void) stream;
    return NULL;
}

static block_t *vlc_chunked_read(struct vlc_http_stream *stream)
{
    struct vlc_chunked_stream *s =
        container_of(stream, struct vlc_chunked_stream, stream);
    block_t *block = NULL;

    if (s->eof)
        return NULL;
    if (s->error)
        return vlc_http_error;

    /* Read chunk size (hexadecimal length) */
    if (s->chunk_length == 0)
    {   /* NOTE: This accepts LF in addition to CRLF. No big deal. */
        char *line = vlc_tls_GetLine(s->tls);
        if (line == NULL)
        {
            errno = EPROTO;
            return vlc_chunked_fatal(s);
        }

        int end;

        if (sscanf(line, "%jx%n", &s->chunk_length, &end) < 1
         || (line[end] != '\0' && line[end] != ';' /* ignore extension(s) */))
            s->chunk_length = UINTMAX_MAX;

        free(line);

        if (s->chunk_length == UINTMAX_MAX)
        {
            errno = EPROTO;
            return vlc_chunked_fatal(s);
        }
    }

    /* Read chunk data */
    if (s->chunk_length > 0)
    {
        size_t size = 1536; /* arbitrary */
        if (size > s->chunk_length)
            size = s->chunk_length;

        block = block_Alloc(size);
        if (unlikely(block == NULL))
            return NULL;

        ssize_t val = vlc_tls_Read(s->tls, block->p_buffer, size, false);
        if (val <= 0)
        {   /* Connection error (-) or unexpected end of connection (0) */
            block_Release(block);
            return vlc_chunked_fatal(s);
        }

        block->i_buffer = val;
        s->chunk_length -= val;
    }
    else
        s->eof = true;

    /* Read chunk end (CRLF) */
    if (s->chunk_length == 0)
    {
        char crlf[2];

        if (vlc_tls_Read(s->tls, crlf, 2, true) < 2 || memcmp(crlf, "\r\n", 2))
            vlc_chunked_fatal(s);
    }
    return block;
}

static void vlc_chunked_close(struct vlc_http_stream *stream, bool abort)
{
    struct vlc_chunked_stream *s =
        container_of(stream, struct vlc_chunked_stream, stream);

    if (!s->eof) /* Abort connection if stream is closed before end */
        vlc_chunked_fatal(s);

    vlc_http_stream_close(s->parent, abort || s->error);
    free(s);
}

static struct vlc_http_stream_cbs vlc_chunked_callbacks =
{
    vlc_chunked_wait,
    vlc_chunked_read,
    vlc_chunked_close,
};

struct vlc_http_stream *vlc_chunked_open(struct vlc_http_stream *parent,
                                         struct vlc_tls *tls)
{
    struct vlc_chunked_stream *s = malloc(sizeof (*s));
    if (unlikely(s == NULL))
        return NULL;

    s->stream.cbs = &vlc_chunked_callbacks;
    s->parent = parent;
    s->tls = tls;
    s->chunk_length = 0;
    s->eof = false;
    s->error = false;
    return &s->stream;
}
