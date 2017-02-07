/*****************************************************************************
 * inflate.c: zlib decompression module for VLC
 *****************************************************************************
 * Copyright © 2016 Rémi Denis-Courmont
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

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>

struct stream_sys_t
{
    z_stream zstream;
    bool eof;
    unsigned char buffer[16384];
};

static ssize_t Read(stream_t *stream, void *buf, size_t buflen)
{
    stream_sys_t *sys = stream->p_sys;
    ssize_t val;

    if (sys->eof || unlikely(buflen == 0))
        return 0;

    sys->zstream.next_out = buf;
    sys->zstream.avail_out = buflen;

    if (sys->zstream.avail_in == 0)
        sys->zstream.next_in = sys->buffer;

    val = (sys->buffer + sizeof (sys->buffer))
        - (sys->zstream.next_in + sys->zstream.avail_in);

    if (val > 0)
    {   /* Fill input buffer if there is space left */
        val = vlc_stream_Read(stream->p_source,
                          sys->zstream.next_in + sys->zstream.avail_in, val);
        if (val >= 0)
            sys->zstream.avail_in += val;
    }

    if (sys->zstream.avail_in == 0)
    {
        msg_Err(stream, "unexpected end of stream");
        return 0;
    }

    val = inflate(&sys->zstream, Z_SYNC_FLUSH);
    switch (val)
    {
        case Z_STREAM_END:
            msg_Dbg(stream, "end of stream");
            sys->eof = true;
            /* fall through */
        case Z_OK:
            return buflen - sys->zstream.avail_out;
        case Z_DATA_ERROR:
            msg_Err(stream, "corrupt stream");
            sys->eof = true;
            return -1;
        case Z_BUF_ERROR:
            if (sys->zstream.next_in == sys->buffer)
                break;

            memmove(sys->buffer, sys->zstream.next_in, sys->zstream.avail_in);
            sys->zstream.next_in = sys->buffer;
            return Read(stream, buf, buflen);
    }

    msg_Err(stream, "unhandled decompression error (%zd)", val);
    return -1;
}

static int ReadDir(stream_t *stream, input_item_node_t *node)
{
    (void) stream; (void) node;
    return VLC_EGENERIC;
}

static int Seek(stream_t *stream, uint64_t offset)
{
    (void) stream; (void) offset;
    return -1;
}

static int Control(stream_t *stream, int query, va_list args)
{
    switch (query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
            *va_arg(args, bool *) = false;
            break;
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
        case STREAM_GET_PTS_DELAY:
        case STREAM_GET_META:
        case STREAM_GET_CONTENT_TYPE:
        case STREAM_GET_SIGNAL:
        case STREAM_SET_PAUSE_STATE:
            return vlc_stream_vaControl(stream->p_source, query, args);
        case STREAM_IS_DIRECTORY:
        case STREAM_GET_SIZE:
        case STREAM_GET_TITLE_INFO:
        case STREAM_GET_TITLE:
        case STREAM_GET_SEEKPOINT:
        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
        case STREAM_SET_PRIVATE_ID_STATE:
        case STREAM_SET_PRIVATE_ID_CA:
        case STREAM_GET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;
        default:
            msg_Err(stream, "unimplemented query (%d) in control", query);
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    stream_t *stream = (stream_t *)obj;
    const uint8_t *peek;
    int bits;

    /* See IETF RFC6713 */
    if (vlc_stream_Peek(stream->p_source, &peek, 2) < 2)
        return VLC_EGENERIC;

    if ((peek[0] & 0xF) == 8 && (peek[0] >> 4) < 8 && (U16_AT(peek) % 31) == 0)
        bits = 15; /* zlib */
    else
    if (!memcmp(peek, "\x1F\x8B", 2))
        bits = 15 + 32; /* gzip */
    else
        return VLC_EGENERIC;

    stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->zstream.next_in = sys->buffer;
    sys->zstream.avail_in = 0;
    sys->zstream.zalloc = Z_NULL;
    sys->zstream.zfree = Z_NULL;
    sys->zstream.opaque = Z_NULL;
    sys->eof = false;

    int ret = inflateInit2(&sys->zstream, bits);
    if (ret != Z_OK)
    {
        free(sys);
        return (ret == Z_MEM_ERROR) ? VLC_ENOMEM : VLC_EGENERIC;
    }

    stream->p_sys = sys;
    stream->pf_read = Read;
    stream->pf_readdir = ReadDir;
    stream->pf_seek = Seek;
    stream->pf_control = Control;
    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    stream_t *stream = (stream_t *)obj;
    stream_sys_t *sys = stream->p_sys;

    inflateEnd(&sys->zstream);
    free(sys);
}

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_capability("stream_filter", 30)

    set_description(N_("Zlib decompression filter"))
    set_callbacks(Open, Close)
vlc_module_end()
