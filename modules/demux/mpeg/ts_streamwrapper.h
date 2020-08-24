/*****************************************************************************
 * ts_streamwrapper.c: Stream filter source wrapper
 *****************************************************************************
 * Copyright (C) 2020 VideoLabs, VLC authors and VideoLAN
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
#include <vlc_stream.h>

static int ts_stream_wrapper_Control(stream_t *s, int i_query, va_list va)
{
    stream_t *demuxstream = s->p_sys;
    return demuxstream->pf_control(demuxstream, i_query, va);
}

static ssize_t ts_stream_wrapper_Read(stream_t *s, void *buf, size_t len)
{
    stream_t *demuxstream = s->p_sys;
    return demuxstream->pf_read(demuxstream, buf, len);
}

static block_t * ts_stream_wrapper_ReadBlock(stream_t *s, bool *eof)
{
    stream_t *demuxstream = s->p_sys;
    return demuxstream->pf_block(demuxstream, eof);
}

static int ts_stream_wrapper_Seek(stream_t *s, uint64_t pos)
{
    stream_t *demuxstream = s->p_sys;
    return demuxstream->pf_seek(demuxstream, pos);
}

static void ts_stream_wrapper_Destroy(stream_t *s)
{
    VLC_UNUSED(s);
}

static stream_t * ts_stream_wrapper_New(stream_t *demuxstream)
{
    stream_t *s = vlc_stream_CommonNew(VLC_OBJECT(demuxstream),
                                       ts_stream_wrapper_Destroy);
    if(s)
    {
        s->p_sys = demuxstream;
        s->s = s;
        if(demuxstream->pf_read)
            s->pf_read = ts_stream_wrapper_Read;
        if(demuxstream->pf_control)
            s->pf_control = ts_stream_wrapper_Control;
        if(demuxstream->pf_seek)
            s->pf_seek = ts_stream_wrapper_Seek;
        if(demuxstream->pf_block)
            s->pf_block = ts_stream_wrapper_ReadBlock;
    }
    return s;
}
