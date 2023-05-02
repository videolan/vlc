/*****************************************************************************
 * hx.c : raw video / audio IP cam demuxer
 *****************************************************************************
 * Copyright (C) 2022 - VideoLabs, VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_arrays.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

/* All Marker formats:
 * [tag.4][size.4][tag dependant.8] */
static const char HXVS[4] = { 'H', 'X', 'V', 'S' }; /* H264 file tag */
static const char HXVT[4] = { 'H', 'X', 'V', 'T' }; /* HEVC */
static const char HXVF[4] = { 'H', 'X', 'V', 'F' }; /* Video samples */
static const char HXAF[4] = { 'H', 'X', 'A', 'F' }; /* Audio samples */
static const char HXFI[4] = { 'H', 'X', 'F', 'I' }; /* RAP Index */

#define HX_HEADER_SIZE 16
#define HX_INDEX_SIZE 200000

struct hxfi_index
{
    uint32_t offset;
    uint32_t time;
};

typedef struct
{
    es_out_id_t *p_es_video;
    es_out_id_t *p_es_audio;
    vlc_tick_t video_pts;
    vlc_tick_t audio_pts;
    vlc_tick_t pcr;
    uint32_t video_pts_offset;
    uint32_t audio_pts_offset;
    uint32_t duration;
    DECL_ARRAY(struct hxfi_index) index;
} hx_sys_t;

static int LoadIndex(demux_t *p_demux)
{
    hx_sys_t *p_sys  = p_demux->p_sys;

    /* From what I understand:
     * Header HXFI[size.4][duration.4][?.4]
     * Size is always 200K, predictable file offset
     * All indexes entries are 4/4 bytes
     * zero filled empty entries
     */

    uint64_t size;
    if(vlc_stream_GetSize(p_demux->s, &size) != VLC_SUCCESS ||
       size < HX_INDEX_SIZE + HX_HEADER_SIZE)
        return VLC_EGENERIC;

    if(vlc_stream_Seek(p_demux->s, size - HX_INDEX_SIZE - HX_HEADER_SIZE) != VLC_SUCCESS)
        return VLC_EGENERIC;

    uint8_t temp[HX_HEADER_SIZE];
    if(vlc_stream_Read(p_demux->s, temp, HX_HEADER_SIZE) != HX_HEADER_SIZE ||
       memcmp(temp, HXFI, 4))
    {
        msg_Warn(p_demux, "Could not find index at expected location");
        return VLC_EGENERIC;
    }

    p_sys->duration = GetDWLE(temp + 8);
    msg_Dbg(p_demux, "Reading Index Length @%" PRIu32 "ms", p_sys->duration);

    uint32_t prevtime = UINT32_MAX;
    for(;;)
    {
        if(vlc_stream_Read(p_demux->s, temp, 8) != 8)
            break;
        struct hxfi_index entry;
        entry.offset = GetDWLE(temp);
        entry.time = GetDWLE(temp + 4);
        if(entry.offset == 0)
            break;
        if(entry.time != prevtime)
        {
            prevtime = entry.time;
            ARRAY_APPEND(p_sys->index, entry);
        }
    }
    msg_Dbg(p_demux, "Using %d entries from HXFI index", p_sys->index.i_size);
    return VLC_SUCCESS;
}

static struct hxfi_index LookupIndex(hx_sys_t *p_sys, uint32_t time)
{
    struct hxfi_index entry = { 0, 0 };
    if(p_sys->index.i_size)
    {
        unsigned l = 0, h = p_sys->index.i_size - 1;
        while(l <= h)
        {
            unsigned m = (l + h) >> 1;
            if(p_sys->index.p_elems[m].time > time)
            {
                if(m == 0)
                    break;
                h = m -1;
            }
            else
            {
                /* store lowest as temp result */
                entry = p_sys->index.p_elems[m];
                if(entry.time == time)
                    break;
                l = m + 1;
            }
        };
    }
    return entry;
}

static int Demux(demux_t *p_demux)
{
    hx_sys_t *p_sys  = p_demux->p_sys;

    uint8_t header[HX_HEADER_SIZE];
    if(vlc_stream_Read(p_demux->s, header, HX_HEADER_SIZE) != HX_HEADER_SIZE)
        return VLC_DEMUXER_EOF;

    es_out_id_t *es = NULL;
    vlc_tick_t *ppts;
    uint32_t *ppts_offset;

    if(!memcmp(header, HXVF, 4))
    {
        es = p_sys->p_es_video;
        ppts = &p_sys->video_pts;
        ppts_offset = &p_sys->video_pts_offset;
    }
    else if(!memcmp(header, HXAF, 4))
    {
        es = p_sys->p_es_audio;
        ppts = &p_sys->audio_pts;
        ppts_offset = &p_sys->audio_pts_offset;
    }
    else if(!memcmp(header, HXVS, 4) || !memcmp(header, HXVT, 4))
    {
        return VLC_DEMUXER_SUCCESS;
    }
    else
    {
        msg_Dbg(p_demux, "EOF on %4.4s", (const char *) header);
        return VLC_DEMUXER_EOF;
    }

    uint32_t sz = GetDWLE(header + 4);
    int i_ret = VLC_DEMUXER_SUCCESS;

    if(es)
    {
        /* [HXAF.4][size.4][pts.4][?.4] */
        /* [HXVF.4][size.4][pts.4][?.4] */
        *ppts = VLC_TICK_0;
        if(*ppts_offset != UINT32_MAX)
            *ppts += VLC_TICK_FROM_MS(GetDWLE(&header[8]) - *ppts_offset);
        else
            *ppts_offset = GetDWLE(&header[8]);

        block_t *p_block = vlc_stream_Block(p_demux->s, sz);
        if(p_block == NULL)
            return VLC_DEMUXER_EOF;

        if(p_block->i_buffer < sz)
            i_ret = VLC_DEMUXER_EOF;

        if(p_sys->p_es_audio == es)
        {
            /* HXAF sample format prefix [?.1/channels.1/rate.1/?.1] */
            const uint8_t audioprefix[4] = {0x00, 0x01, 0x50, 0x00};
            if(p_block->i_buffer >= 4 &&
               !memcmp(p_block->p_buffer, audioprefix, 4))
            {
                p_block->i_buffer -= 4;
                p_block->p_buffer += 4;
            }
            else
            {
                msg_Warn(p_demux,"Unsupported audio format, dropping");
                block_Release(p_block);
                p_block = NULL;
            }
        }

        if(p_sys->pcr == VLC_TICK_INVALID)
        {
            p_sys->pcr = *ppts;
            es_out_SetPCR(p_demux->out, p_sys->pcr);
        }

        if(p_block)
        {
            p_block->i_dts = p_block->i_pts = *ppts;
            es_out_Send(p_demux->out, es, p_block);
        }

        if( p_sys->audio_pts > p_sys->pcr )
        {
            p_sys->pcr = p_sys->audio_pts;
            es_out_SetPCR(p_demux->out, p_sys->pcr);
        }
    }
    else
    {
        if(vlc_stream_Read(p_demux->s, NULL, sz) != sz)
            i_ret = VLC_DEMUXER_EOF;
    }

    return i_ret;
}

static int SeekUsingIndex(demux_t *p_demux, uint32_t time, bool b_precise)
{
    hx_sys_t *p_sys  = p_demux->p_sys;
    struct hxfi_index idx = LookupIndex(p_sys, time);
    if(idx.offset == 0)
        return VLC_EGENERIC;
    int ret = vlc_stream_Seek(p_demux->s, idx.offset);
    if(ret == VLC_SUCCESS)
    {
        p_sys->pcr = VLC_TICK_0 + VLC_TICK_FROM_MS(idx.time);
        p_sys->audio_pts = VLC_TICK_INVALID;
        p_sys->video_pts = VLC_TICK_INVALID;
        if(b_precise)
            es_out_Control(p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                           VLC_TICK_0 + VLC_TICK_FROM_MS(time));
    }
    return ret;
}

static int Control(demux_t *p_demux, int i_query, va_list args)
{
    hx_sys_t *p_sys  = p_demux->p_sys;
    switch(i_query)
    {
        case DEMUX_GET_TIME:
        {
            *va_arg(args, vlc_tick_t *) = p_sys->pcr;
            return VLC_SUCCESS;
        }
        case DEMUX_GET_LENGTH:
        {
            *va_arg(args, vlc_tick_t *) = VLC_TICK_FROM_MS(p_sys->duration);
            return VLC_SUCCESS;
        }
        case DEMUX_GET_POSITION:
        {
            if(!p_sys->duration)
                return VLC_EGENERIC;
            *va_arg(args, double *) = MS_FROM_VLC_TICK(p_sys->pcr) / p_sys->duration;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_TIME:
        {
            vlc_tick_t time = va_arg(args, vlc_tick_t);
            bool b_precise = va_arg(args, int);
            return SeekUsingIndex(p_demux, MS_FROM_VLC_TICK(time), b_precise);
        }
        case DEMUX_SET_POSITION:
        {
            double pos = va_arg(args, double);
            bool b_precise = va_arg(args, int);
            if(p_sys->index.i_size == 0)
                return VLC_EGENERIC;
            return SeekUsingIndex(p_demux, pos * p_sys->duration, b_precise);
        }
        case DEMUX_CAN_SEEK:
        {
            if(p_sys->index.i_size == 0)
                return VLC_EGENERIC;
            /* fallthrough */
        }
        default:
            break;
    }

    return demux_vaControlHelper(p_demux->s, 0, -1, 0, 1, i_query, args);
}

static void Close(vlc_object_t *p_this)
{
    demux_t     *p_demux = (demux_t*)p_this;
    hx_sys_t *p_sys  = p_demux->p_sys;
    ARRAY_RESET(p_sys->index);
    free(p_sys);
}

static int Open(vlc_object_t * p_this)
{
    demux_t     *p_demux = (demux_t*)p_this;
    hx_sys_t *p_sys;
    const uint8_t *p_peek;

    if(vlc_stream_Peek(p_demux->s, &p_peek, 20) != 20 ||
       (memcmp(p_peek, HXVS, 4) && memcmp(p_peek, HXVT, 4)) ||
        memcmp(p_peek + 16, HXVF, 4))
        return VLC_EGENERIC;

    p_demux->p_sys = p_sys = malloc(sizeof(hx_sys_t));
    if(!p_sys)
        return VLC_ENOMEM;
    p_sys->audio_pts = VLC_TICK_INVALID;
    p_sys->video_pts = VLC_TICK_INVALID;
    p_sys->pcr = VLC_TICK_INVALID;
    p_sys->audio_pts_offset = UINT32_MAX;
    p_sys->video_pts_offset = UINT32_MAX;
    p_sys->p_es_audio = NULL;
    p_sys->p_es_video = NULL;
    p_sys->duration = 0;
    ARRAY_INIT(p_sys->index);

    bool b_seekable;
    if(vlc_stream_Control(p_demux->s, STREAM_CAN_SEEK, &b_seekable) != VLC_SUCCESS)
        b_seekable = false;

    vlc_fourcc_t vcodec = memcmp(p_peek, HXVT, 4) ? VLC_CODEC_H264 : VLC_CODEC_HEVC;
    unsigned width = GetDWLE(p_peek + 4);
    unsigned height = GetDWLE(p_peek + 8);

    if(b_seekable)
    {
        uint64_t origin = vlc_stream_Tell(p_demux->s);
        if(LoadIndex(p_demux) != VLC_SUCCESS)
            msg_Err(p_demux, "Failed to load index");
        if(vlc_stream_Seek(p_demux->s, origin) != VLC_SUCCESS)
        {
            msg_Err(p_demux, "Failed to seek after loading index, giving up");
            Close(p_this);
            return VLC_EGENERIC;
        }
    }

    /* Create ES from parameters or defaults */
    es_format_t fmt;

    es_format_Init(&fmt, VIDEO_ES, vcodec);
    fmt.video.i_visible_width = width;
    fmt.video.i_visible_height = height;
    fmt.b_packetized = false;
    p_sys->p_es_video = es_out_Add(p_demux->out, &fmt);
    es_format_Clean(&fmt);

    es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_ALAW);
    fmt.audio.i_rate = 8000;
    fmt.audio.i_channels = 1;
    p_sys->p_es_audio = es_out_Add(p_demux->out, &fmt);
    es_format_Clean(&fmt);

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname("HX")
    set_description(N_("HX video demuxer"))
    set_capability("demux", 10)
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_callbacks(Open, Close)
    add_shortcut("hx")
vlc_module_end ()
