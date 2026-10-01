/*****************************************************************************
 * psmf.c: Sorry PayStation Movie demux
 *****************************************************************************
 * Copyright (C) 2026 VideoLabs, VLC authors and VideoLAN
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
#include <vlc_demux.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_charset.h>
#include <vlc_arrays.h>

#include <limits.h>
#include <stdckdint.h>

#define PSMF_MIN_HEADER_SIZE          0x82
#define PSMF_STREAM_DESC_SIZE         16
#define PSMF_ENTRYPOINTS_ENTRY_SIZE   10
#define PSMF_MARK_ENTRY_SIZE          40
#define PSMF_MARK_TYPE_CHAPTER        0x05
#define PSMF_MARK_TYPE_EVENT          0x10
#define PSMF_PS_PACKET_SIZE           2048
#define PSMF_MAX_STREAMS              128
#define PSMF_MAX_HEADER_SIZE          (16 * 1024 * 1024)

typedef struct
{
    unsigned rate;
    unsigned channel_id;
    unsigned frame_size;
    unsigned samples_per_frame;
    unsigned bitrate;
} atrac3p_header_t;

static bool atrac3p_parse_header(const uint8_t *p, size_t len, atrac3p_header_t *h)
{
    static const unsigned rates[8] = {32000, 44100, 48000, 88200, 96000, 0, 0, 0};
    if (len < 4 || GetWBE(p) != 0x0fd0)
        return false;

    const uint16_t cfg = GetWBE(p + 2);
    const unsigned rate_idx = (cfg >> 13) & 7;

    h->rate = rates[rate_idx];
    h->channel_id = (cfg >> 10) & 7;
    h->frame_size = ((cfg & 0x3ff) << 3) + 8;
    h->samples_per_frame = 2048;

    if (!h->rate || !h->channel_id || !h->frame_size)
        return false;

    h->bitrate = (uint64_t)h->frame_size * 8 * h->rate / h->samples_per_frame;

    return true;
}

typedef struct
{
    uint32_t pts;
    uint32_t packet;
    uint8_t index;
    uint8_t pic_offset;
} psmf_ep_t;

typedef struct
{
    stream_t *source;
    uint64_t base;
    uint64_t size;
} psmf_stream_sys_t;

typedef struct
{
    es_out_t *es_out;
    demux_t *psdemux;

    uint32_t data_offset;
    uint32_t data_size;

    uint64_t pts_start;
    uint64_t pts_end;
    vlc_tick_t duration;

    psmf_ep_t *ep;
    size_t ep_count;

    input_title_t *title;
    int current_seekpoint;
    unsigned updates;

} demux_sys_t;

/*
    Fake EsOut to intercept and handle ES specific changes
*/
typedef struct psmf_atrac_es_t
{
    es_out_id_t *id;
    es_format_t fmt;

    atrac3p_header_t header;
    bool configured;

    uint8_t frame_header[8];
    size_t frame_header_size;

    block_t *frame;

    struct psmf_atrac_es_t *next;
} psmf_atrac_es_t;

#define PSMF_PES_PRIVATE_STREAM_1 0xBD
#define PSMF_PS_PRIVATE_ID_BASE   0xBD00 // depends on the PS demux id

typedef struct
{
    uint16_t ps_id;
    uint8_t data[PSMF_STREAM_DESC_SIZE];
} psmf_stream_descriptor_t;

typedef struct
{
    es_out_t *p_dst_out;
    es_out_t es_out;

    psmf_stream_descriptor_t *descriptors;
    size_t descriptor_count;

    psmf_atrac_es_t *atrac;
} psmf_esout_priv_t;

static uint16_t psmf_DescriptorId(const uint8_t *desc)
{
    const uint8_t stream_id = desc[0];

    if (stream_id == PSMF_PES_PRIVATE_STREAM_1)
        return PSMF_PS_PRIVATE_ID_BASE | desc[1];

    return stream_id;
}

static const psmf_stream_descriptor_t *
psmf_FindDescriptor(const psmf_esout_priv_t *priv, int id)
{
    if (id < 0 || id > UINT16_MAX)
        return NULL;

    for (size_t i = 0; i < priv->descriptor_count; ++i)
    {
        if (priv->descriptors[i].ps_id == (uint16_t)id)
            return &priv->descriptors[i];
    }

    return NULL;
}

static psmf_atrac_es_t * psmf_atrac_Find(psmf_esout_priv_t *priv, es_out_id_t *id)
{
    for (psmf_atrac_es_t *state = priv->atrac; state != NULL; state = state->next)
        if (state->id == id)
            return state;
    return NULL;
}

static void psmf_atrac_Reset(psmf_atrac_es_t *state)
{
    if (state->frame != NULL)
    {
        block_Release(state->frame);
        state->frame = NULL;
    }

    state->frame_header_size = 0;
}

static void psmf_atrac_DeleteState(psmf_atrac_es_t *state)
{
    psmf_atrac_Reset(state);
    es_format_Clean(&state->fmt);
    free(state);
}

static const struct
{
    uint8_t channels;
    uint16_t physical_channels;
} atrac3p_channel_layouts[8] = {
    [0] = { 0 }, /* invalid */
    [1] = { 1, AOUT_CHAN_CENTER },
    [2] = { 2, AOUT_CHANS_STEREO },
    [3] = { 3, AOUT_CHANS_3_0 },
    [4] = { 4, AOUT_CHANS_4_CENTER_REAR },
    [5] = { 6, AOUT_CHANS_5_1 },
    [6] = { 7, AOUT_CHANS_5_1 | AOUT_CHAN_REARCENTER },
    [7] = { 8, AOUT_CHANS_7_1 },
};

static int psmf_atrac_UpdateFormat(psmf_esout_priv_t *priv, psmf_atrac_es_t *state,
                                   const atrac3p_header_t *header)
{
    if (header->channel_id == 0 || header->channel_id >= ARRAY_SIZE(atrac3p_channel_layouts))
        return VLC_EGENERIC;

    if (state->configured &&
        state->header.rate == header->rate &&
        state->header.channel_id == header->channel_id &&
        state->header.frame_size == header->frame_size)
        return VLC_SUCCESS;

    state->fmt.i_bitrate = header->bitrate;
    state->fmt.audio.i_rate = header->rate;
    state->fmt.audio.i_channels =
            atrac3p_channel_layouts[header->channel_id & 7].channels;
    state->fmt.audio.i_physical_channels =
            atrac3p_channel_layouts[header->channel_id & 7].physical_channels;
    state->fmt.audio.i_blockalign = header->frame_size;
    state->fmt.audio.i_bytes_per_frame = header->frame_size;

    int ret = es_out_Control(priv->p_dst_out, ES_OUT_SET_ES_FMT, state->id, &state->fmt);
    if (ret != VLC_SUCCESS)
        return ret;

    state->header = *header;
    state->configured = true;
    return VLC_SUCCESS;
}

static int psmf_atrac_StartFrame(psmf_esout_priv_t *priv, psmf_atrac_es_t *state,
                                 const uint8_t header_data[8], const block_t *source,
                                 bool *ts_used)
{
    atrac3p_header_t header;
    if (!atrac3p_parse_header(header_data, 8, &header))
        return VLC_EGENERIC;

    int ret = psmf_atrac_UpdateFormat(priv, state, &header);
    if (ret != VLC_SUCCESS)
        return ret;

    block_t *frame = block_Alloc(header.frame_size);
    if (unlikely(frame == NULL))
        return VLC_ENOMEM;

    block_CopyProperties(frame, source);

    if (*ts_used)
    {
        frame->i_pts = VLC_TICK_INVALID;
        frame->i_dts = VLC_TICK_INVALID;
        frame->i_length = 0;
        frame->i_flags &= ~BLOCK_FLAG_DISCONTINUITY;
    }
    else
        *ts_used = true;

    frame->i_buffer = 0;
    frame->i_nb_samples = header.samples_per_frame;
    state->frame = frame;

    return VLC_SUCCESS;
}

static int psmf_atrac_Append(psmf_esout_priv_t *priv, psmf_atrac_es_t *state,
                             const uint8_t *data, size_t amount)
{
    block_t *frame = state->frame;
    if (frame == NULL || !state->configured)
        return VLC_EGENERIC;

    const size_t missing = state->header.frame_size - frame->i_buffer;
    if (amount > missing)
        return VLC_EGENERIC;

    memcpy(frame->p_buffer + frame->i_buffer, data, amount);
    frame->i_buffer += amount;

    if (frame->i_buffer != state->header.frame_size)
        return VLC_SUCCESS;

    state->frame = NULL;
    return es_out_Send(priv->p_dst_out, state->id, frame);
}

static int psmf_atrac_ConsumeHeader(psmf_esout_priv_t *priv, psmf_atrac_es_t *state,
                                    const uint8_t **data, size_t *size,
                                    const block_t *source, bool *ts_used)
{
    const size_t needed = sizeof(state->frame_header) - state->frame_header_size;
    const size_t amount = *size < needed ? *size : needed;

    memcpy(state->frame_header + state->frame_header_size, *data, amount);
    state->frame_header_size += amount;
    *data += amount;
    *size -= amount;

    if (state->frame_header_size < sizeof(state->frame_header))
        return VLC_SUCCESS;

    int ret = psmf_atrac_StartFrame(priv, state, state->frame_header,
                                    source, ts_used);
    state->frame_header_size = 0;
    return ret;
}

static int psmf_atrac_ConsumeContinuation(psmf_esout_priv_t *priv, psmf_atrac_es_t *state,
                                          const uint8_t **data, size_t *size, uint16_t continuation)
{
    if (continuation > *size)
    {
        psmf_atrac_Reset(state);
        return VLC_EGENERIC;
    }

    if (state->frame == NULL)
    {
        // can't continue without existing data. skip to next packet
        *data += continuation;
        *size -= continuation;
        return VLC_SUCCESS;
    }

    const size_t missing =
        state->header.frame_size - state->frame->i_buffer;

    if (continuation != missing)
    {
        // implicit vs sum accounting disagreed. corruption ?
        block_Release(state->frame);
        state->frame = NULL;

        *data += continuation;
        *size -= continuation;
        return VLC_SUCCESS;
    }

    int ret = psmf_atrac_Append(priv, state, *data, continuation);
    *data += continuation;
    *size -= continuation;
    return ret;
}

static int psmf_es_out_callback_Send(es_out_t *out, es_out_id_t *id, block_t *block)
{
    psmf_esout_priv_t *priv = container_of(out, psmf_esout_priv_t, es_out);
    psmf_atrac_es_t *state = psmf_atrac_Find(priv, id);

    if (state == NULL)
        return es_out_Send(priv->p_dst_out, id, block);

    if (block->i_flags & BLOCK_FLAG_DISCONTINUITY)
        psmf_atrac_Reset(state);

    if (block->i_buffer < 2)
    {
        block_Release(block);
        return VLC_SUCCESS;
    }

    const uint8_t *data = block->p_buffer;
    size_t size = block->i_buffer;
    bool ts_used = false;
    int ret = VLC_SUCCESS;

    /* stream_id data[0]
     * reserved  data[1] */
    data += 2;
    size -= 2;

    /* Tricky fragmentation header: Either
     * 0x0FD0 ATRAC frame sync word on new frame, or,
     * uint16_t continuation_size if continuing */
    uint16_t continuation = 0;
    if (size >= 2 && GetWBE(data) != 0x0FD0)
    {
        continuation = GetWBE(data);
        data += 2;
        size -= 2;
    }

    if (state->frame_header_size != 0)
    {
        // handle incomplete header
        ret = psmf_atrac_ConsumeHeader(priv, state, &data, &size, block, &ts_used);
        if (ret != VLC_SUCCESS || state->frame_header_size != 0)
            goto out;
    }
    else if (continuation != 0)
    {
        ret = psmf_atrac_ConsumeContinuation(priv, state, &data, &size, continuation);
        if (ret != VLC_SUCCESS)
            goto out;
    }
    else if (state->frame != NULL)
    {
        // discontinuity/loss ? discard incomplete data
        block_Release(state->frame);
        state->frame = NULL;
    }

    while (size > 0)
    {
        if (state->frame == NULL)
        {
            ret = psmf_atrac_ConsumeHeader(priv, state, &data, &size, block, &ts_used);
            if (ret != VLC_SUCCESS)
                goto out;

            if (state->frame == NULL)
                break;
        }

        const size_t missing = state->header.frame_size - state->frame->i_buffer;
        const size_t amount = size < missing ? size : missing;

        ret = psmf_atrac_Append(priv, state, data, amount);
        data += amount;
        size -= amount;

        if (ret != VLC_SUCCESS)
            goto out;
    }

out:
    block_Release(block);

    if (ret != VLC_SUCCESS)
        psmf_atrac_Reset(state);

    return ret;
}

static es_out_id_t * psmf_es_out_callback_Add(es_out_t *out, input_source_t *in,
                                              const es_format_t *source_fmt)
{
    VLC_UNUSED(in);

    psmf_esout_priv_t *priv = container_of(out, psmf_esout_priv_t, es_out);

    const psmf_stream_descriptor_t *descriptor = psmf_FindDescriptor(priv, source_fmt->i_id);
    if (descriptor == NULL)
        return NULL;

    const uint8_t stream_id = descriptor->data[0];
    const uint8_t private_id = descriptor->data[1];

    es_out_id_t *id = NULL;

    if ((stream_id & 0xf0) == 0xe0)
    {
        es_format_t fmt;
        es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_H264);
        fmt.b_packetized = false;
        id = es_out_Add(priv->p_dst_out, &fmt);
        es_format_Clean(&fmt);
    }
    else if (stream_id == PSMF_PES_PRIVATE_STREAM_1)
    {
        if ((private_id & 0xf0) == 0x00)
        {
            psmf_atrac_es_t *state = calloc(1, sizeof(*state));
            if (unlikely(state == NULL))
                return NULL;
            es_format_Init(&state->fmt, AUDIO_ES, VLC_CODEC_ATRAC3P);
            state->fmt.b_packetized = true;
            /* We set common values for decoder initialization
               But *it will be updated* after reading the in-band config */
            state->fmt.audio.i_blockalign = 376;
            state->fmt.audio.i_bytes_per_frame = 376;
            state->fmt.audio.i_channels = 2;
            state->fmt.audio.i_physical_channels = AOUT_CHANS_STEREO;
            state->fmt.audio.i_rate = 44100;
            state->fmt.audio.i_frame_length = 2048;
            id = es_out_Add(priv->p_dst_out, &state->fmt);
            if (id == NULL)
                psmf_atrac_DeleteState(state);
            else
            {
                state->id = id;
                state->next = priv->atrac;
                priv->atrac = state;
            }
        }
        else if ((private_id & 0xf0) == 0x10)
        {
            const unsigned channels = descriptor->data[14];
            if (channels != 1 && channels != 2)
                return NULL;

            if (descriptor->data[15] != 2) // 44.1 kHz
                return NULL;

            es_format_t fmt;
            es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_S16L);
            fmt.b_packetized = true;
            fmt.i_bitrate = 44100 * channels * 16;
            fmt.audio.i_rate = 44100;
            fmt.audio.i_channels = channels;
            fmt.audio.i_physical_channels = channels == 1 ? AOUT_CHAN_CENTER
                                                          : AOUT_CHANS_STEREO;
            fmt.audio.i_bitspersample = 16;
            fmt.audio.i_blockalign = channels * 2;
            fmt.audio.i_bytes_per_frame = channels * 2;
            fmt.audio.i_frame_length = 1;
            id = es_out_Add(priv->p_dst_out, &fmt);
            es_format_Clean(&fmt);
        }
    }

    return id;
}

static void psmf_es_out_callback_Del(es_out_t *out, es_out_id_t *id)
{
    psmf_esout_priv_t *priv = container_of(out, psmf_esout_priv_t, es_out);
    psmf_atrac_es_t **link = &priv->atrac;

    while (*link != NULL)
    {
        psmf_atrac_es_t *state = *link;

        if (state->id == id)
        {
            *link = state->next;
            psmf_atrac_DeleteState(state);
            break;
        }

        link = &state->next;
    }

    es_out_Del(priv->p_dst_out, id);
}

static int psmf_es_out_callback_Control(es_out_t *out, input_source_t *in,
                                        int query, va_list args)
{
    VLC_UNUSED(in);

    psmf_esout_priv_t *priv = container_of(out, psmf_esout_priv_t, es_out);
    return es_out_vaControl(priv->p_dst_out, query, args);
}

static void psmf_es_out_callback_Destroy(es_out_t *out)
{
    psmf_esout_priv_t *priv = container_of(out, psmf_esout_priv_t, es_out);

    while (priv->atrac != NULL)
    {
        psmf_atrac_es_t *state = priv->atrac;
        priv->atrac = state->next;
        psmf_atrac_DeleteState(state);
    }

    free(priv->descriptors);
    free(priv);
}

static const struct es_out_callbacks psmf_es_out_Callbacks = {
    .add = psmf_es_out_callback_Add,
    .send = psmf_es_out_callback_Send,
    .del = psmf_es_out_callback_Del,
    .control = psmf_es_out_callback_Control,
    .destroy = psmf_es_out_callback_Destroy,
};

static es_out_t * psmf_es_out_New(es_out_t *dst, const uint8_t *header,
                                  size_t header_size, uint16_t stream_count)
{
    if (stream_count > PSMF_MAX_STREAMS)
        return NULL;

    const size_t descriptors_size = (size_t)stream_count * PSMF_STREAM_DESC_SIZE;

    if (PSMF_MIN_HEADER_SIZE > header_size ||
        descriptors_size > header_size - PSMF_MIN_HEADER_SIZE)
        return NULL;

    psmf_esout_priv_t *priv = calloc(1, sizeof(*priv));
    if (unlikely(priv == NULL))
        return NULL;

    if (stream_count > 0)
    {
        priv->descriptors = vlc_alloc(stream_count, sizeof(*priv->descriptors));
        if (unlikely(priv->descriptors == NULL))
        {
            free(priv);
            return NULL;
        }

        for (uint16_t i = 0; i < stream_count; i++)
        {
            const uint8_t *desc = header + PSMF_MIN_HEADER_SIZE +
                                  (size_t)i * PSMF_STREAM_DESC_SIZE;

            memcpy(priv->descriptors[i].data, desc, PSMF_STREAM_DESC_SIZE);
            priv->descriptors[i].ps_id = psmf_DescriptorId(desc);
        }
    }

    priv->descriptor_count = stream_count;
    priv->p_dst_out = dst;
    priv->es_out.cbs = &psmf_es_out_Callbacks;

    return &priv->es_out;
}
/*
    !Fake EsOut
*/

static uint64_t GetQW48BE(const uint8_t *p)
{
    return ((uint64_t)p[0] << 40) |
           ((uint64_t)p[1] << 32) |
           ((uint64_t)p[2] << 24) |
           ((uint64_t)p[3] << 16) |
           ((uint64_t)p[4] <<  8) |
           ((uint64_t)p[5] <<  0);
}

static bool psmf_stream_CanSeek(stream_t *s)
{
    psmf_stream_sys_t *sys = s->p_sys;
    return vlc_stream_CanSeek(sys->source);
}

static bool psmf_stream_CanPause(stream_t *s)
{
    psmf_stream_sys_t *sys = s->p_sys;
    return vlc_stream_CanPause(sys->source);
}

static bool psmf_stream_CanControlPace(stream_t *s)
{
    psmf_stream_sys_t *sys = s->p_sys;
    return vlc_stream_CanPace(sys->source);
}

static bool psmf_stream_CanFastSeek(stream_t *s)
{
    psmf_stream_sys_t *sys = s->p_sys;
    return vlc_stream_CanFastSeek(sys->source);
}

static ssize_t psmf_stream_Read(stream_t *s, void *buf, size_t len)
{
    psmf_stream_sys_t *sys = s->p_sys;
    const uint64_t pos = vlc_stream_Tell(sys->source) - sys->base;

    if (pos >= sys->size)
        return 0;

    const uint64_t left = sys->size - pos;
    if (left < len)
        len = (size_t)left;

    return vlc_stream_ReadPartial(sys->source, buf, len);
}

static int psmf_stream_Seek(stream_t *s, uint64_t pos)
{
    psmf_stream_sys_t *sys = s->p_sys;

    if (pos > sys->size || pos > UINT64_MAX - sys->base)
        return VLC_EGENERIC;

    return vlc_stream_Seek(sys->source, sys->base + pos);
}

static int psmf_stream_GetSize(stream_t *s, uint64_t *size)
{
    psmf_stream_sys_t *sys = s->p_sys;
    *size = sys->size;
    return VLC_SUCCESS;
}

static vlc_tick_t psmf_stream_GetPtsDelay(stream_t *s)
{
    psmf_stream_sys_t *sys = s->p_sys;
    return vlc_stream_GetPtsDelay(sys->source);
}

static int psmf_stream_SetPauseState(stream_t *s, bool paused)
{
    psmf_stream_sys_t *sys = s->p_sys;
    return vlc_stream_Control(sys->source, STREAM_SET_PAUSE_STATE, paused);
}

static void psmf_stream_Destroy(stream_t *s)
{
    free(s->p_sys);
}

static const struct vlc_stream_operations stream_ops =
{
    .can_seek = psmf_stream_CanSeek,
    .can_pause = psmf_stream_CanPause,
    .can_control_pace = psmf_stream_CanControlPace,
    .set_pause_state = psmf_stream_SetPauseState,
    .stream =
    {
        .can_fastseek = psmf_stream_CanFastSeek,
        .read = psmf_stream_Read,
        .seek = psmf_stream_Seek,
        .get_size = psmf_stream_GetSize,
        .get_pts_delay = psmf_stream_GetPtsDelay,
    },
};

static stream_t *psmf_stream_New(demux_t *demux, uint64_t base, uint64_t size)
{
    stream_t *s = vlc_stream_CommonNew(VLC_OBJECT(demux), psmf_stream_Destroy);
    if (s == NULL)
        return NULL;

    psmf_stream_sys_t *sys = malloc(sizeof(*sys));
    if (sys == NULL)
    {
        vlc_stream_Delete(s);
        return NULL;
    }

    sys->source = demux->s;
    sys->base = base;
    sys->size = size;

    s->p_sys = sys;
    s->ops = &stream_ops;

    if (vlc_stream_Seek(demux->s, base) != VLC_SUCCESS)
    {
        vlc_stream_Delete(s);
        return NULL;
    }

    return s;
}

static int ParseEntryPointMap(demux_t *demux, demux_sys_t *sys,
                              const uint8_t *header, size_t header_size)
{
    const uint16_t stream_count = GetWBE(header + 0x80);
    if (stream_count == 0 || stream_count > PSMF_MAX_STREAMS)
        return VLC_SUCCESS;

    const size_t descriptors_end = PSMF_MIN_HEADER_SIZE +
                                   (size_t)stream_count * PSMF_STREAM_DESC_SIZE;

    if (descriptors_end > header_size)
        return VLC_EGENERIC;

    for(uint16_t i = 0; i < stream_count; ++i)
    {
        const uint8_t *desc = header + PSMF_MIN_HEADER_SIZE +
                              (size_t)i * PSMF_STREAM_DESC_SIZE;
        const uint8_t stream_id = desc[0];

        if ((stream_id & 0xf0) != 0xe0)
            continue;

        const uint32_t ep_offset = GetDWBE(desc + 4);
        const uint32_t ep_count = GetDWBE(desc + 8);
        if (ep_offset == 0 || ep_count == 0)
            continue;

        size_t ep_bytes;
        if(ckd_mul(&ep_bytes, ep_count, PSMF_ENTRYPOINTS_ENTRY_SIZE))
            continue;

        if (ep_offset > header_size || ep_bytes > header_size - ep_offset)
            continue;

        psmf_ep_t *ep = vlc_alloc(ep_count, sizeof(*ep));
        if (ep == NULL)
            return VLC_ENOMEM;

        size_t valid = 0;
        uint32_t last_pts = 0;
        uint32_t last_packet = 0;

        for(uint32_t j = 0; j < ep_count; ++j)
        {
            const uint8_t *entry = header + ep_offset + (size_t)j * PSMF_ENTRYPOINTS_ENTRY_SIZE;
            const uint32_t pts = GetDWBE(entry + 2);
            const uint32_t packet = GetDWBE(entry + 6);

            if ((uint64_t)packet * PSMF_PS_PACKET_SIZE >= sys->data_size)
                continue;

            if (valid > 0 && (pts < last_pts || packet < last_packet))
                break;

            ep[valid].index = entry[0];
            ep[valid].pic_offset = entry[1];
            ep[valid].pts = pts;
            ep[valid].packet = packet;
            last_pts = pts;
            last_packet = packet;
            valid++;
        }

        if (valid == 0)
        {
            free(ep);
            continue;
        }

        sys->ep = ep;
        sys->ep_count = valid;

        msg_Dbg(demux, "using PSMF EntryPoint map: %zu entries, offset 0x%"PRIx32, valid, ep_offset);
        return VLC_SUCCESS;
    }

    return VLC_SUCCESS;
}

static int ParseMarks(demux_t *demux, demux_sys_t *sys,
                       const uint8_t *header, size_t header_size)
{
    const uint32_t mark_offset = GetDWBE(header + 0x10);
    const uint32_t mark_size = GetDWBE(header + 0x14);

    if (mark_offset == 0 || mark_size == 0)
        return VLC_SUCCESS;

    if (mark_size < 6 ||
        mark_offset > header_size ||
        mark_size > header_size - mark_offset)
        return VLC_SUCCESS; // ignore

    const uint8_t *section = header + mark_offset;
    const uint32_t section_size = GetDWBE(section);
    const uint16_t mark_count = GetWBE(section + 4);

    if (section_size < 6 ||
        section_size > mark_size ||
        section_size > header_size - mark_offset ||
        mark_count > (section_size - 6) / PSMF_MARK_ENTRY_SIZE)
        return VLC_SUCCESS;

    input_title_t *title = vlc_input_title_New();
    if (title == NULL)
        return VLC_ENOMEM;

    title->i_length = sys->duration;

    for(uint16_t i = 0; i < mark_count; ++i)
    {
        const uint8_t *entry = section + 6 + (size_t)i * PSMF_MARK_ENTRY_SIZE;
        const uint8_t type = entry[0];
        const uint8_t name_length = entry[1];

        if (type != PSMF_MARK_TYPE_CHAPTER && type != PSMF_MARK_TYPE_EVENT)
            continue;

        const uint64_t pts = GetQW48BE(entry + 4);
        if (pts < sys->pts_start)
            continue;

        const uint64_t delta = pts - sys->pts_start;
        if (delta > INT64_MAX)
            continue;

        const vlc_tick_t time = vlc_tick_from_samples(delta, 90000);

        if (sys->duration != VLC_TICK_INVALID && time > sys->duration)
            continue;

        seekpoint_t *seekpoint = vlc_seekpoint_New();
        if (seekpoint == NULL)
        {
            vlc_input_title_Delete(title);
            return VLC_ENOMEM;
        }

        seekpoint->i_time_offset = time;

        const size_t len = __MIN(name_length, 24);
        if (len > 0)
        {
            seekpoint->psz_name = strndup((const char *)entry + 16, len);
            if (seekpoint->psz_name == NULL)
            {
                vlc_seekpoint_Delete(seekpoint);
                vlc_input_title_Delete(title);
                return VLC_ENOMEM;
            }
            EnsureUTF8(seekpoint->psz_name);
        }
        else if (type == PSMF_MARK_TYPE_EVENT)
        {
            if (asprintf(&seekpoint->psz_name, "Event %u", i + 1) < 0)
                seekpoint->psz_name = NULL;
        }

        TAB_APPEND(title->i_seekpoint, title->seekpoint, seekpoint);

        msg_Dbg(demux, "PSMF mark %u: type 0x%02x at %"PRId64" ms%s%s",
                 i, type, MS_FROM_VLC_TICK(time),
                 seekpoint->psz_name != NULL ? ", " : "",
                 seekpoint->psz_name != NULL ? seekpoint->psz_name : "");
    }

    if (title->i_seekpoint == 0)
    {
        vlc_input_title_Delete(title);
        return VLC_SUCCESS;
    }

    sys->title = title;
    sys->current_seekpoint = 0;
    return VLC_SUCCESS;
}

static int FindEntryPoint(const demux_sys_t *sys, uint32_t pts)
{
    if (sys->ep_count == 0)
        return -1;

    size_t lo = 0;
    size_t hi = sys->ep_count;

    while(lo < hi)
    {
        const size_t mid = lo + (hi - lo) / 2;
        if (sys->ep[mid].pts <= pts)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo == 0 ? -1 : (int)(lo - 1);
}

static void UpdateCurrentSeekpoint(demux_sys_t *sys, vlc_tick_t time)
{
    if (sys->title == NULL || sys->title->i_seekpoint == 0)
        return;

    int current = 0;
    for(int i = 1; i < sys->title->i_seekpoint; ++i)
    {
        if (sys->title->seekpoint[i]->i_time_offset > time)
            break;
        current = i;
    }

    if (sys->current_seekpoint != current)
    {
        sys->current_seekpoint = current;
        sys->updates |= INPUT_UPDATE_SEEKPOINT;
    }
}

static int SeekTime(demux_t *demux, vlc_tick_t time, bool precise)
{
    demux_sys_t *sys = demux->p_sys;

    if (time < 0)
        time = 0;
    if (sys->duration > 0 && time > sys->duration)
        time = sys->duration;

    if (sys->ep_count > 0)
    {
        const int64_t relpts = samples_from_vlc_tick(time, 90000);
        uint64_t target = sys->pts_start;

        if (relpts > 0 && (uint64_t)relpts <= UINT64_MAX - target)
            target += (uint64_t)relpts;

        if (target > UINT32_MAX)
            target = UINT32_MAX;

        const int index = FindEntryPoint(sys, (uint32_t)target);
        uint64_t byte_offset = 0;

        if (index >= 0)
            byte_offset = (uint64_t)sys->ep[index].packet * PSMF_PS_PACKET_SIZE;

        const double pos = sys->data_size > 0 ? (double)byte_offset / (double)sys->data_size
                                              : 0.0;

        return demux_SetPosition(sys->psdemux, pos, precise);
    }

    if (sys->duration > 0)
        return demux_SetPosition(sys->psdemux, (double)time / (double)sys->duration, precise);

    return demux_SetTime(sys->psdemux, time, precise);
}

static int Demux(demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;
    return demux_Demux(sys->psdemux);
}

static int Control(demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    switch(query)
    {
        case DEMUX_GET_LENGTH:
        {
            vlc_tick_t *length = va_arg(args, vlc_tick_t *);
            if (sys->duration != VLC_TICK_INVALID)
            {
                *length = sys->duration;
                return VLC_SUCCESS;
            }
            return demux_Control(sys->psdemux, DEMUX_GET_LENGTH, length);
        }

        case DEMUX_SET_TIME:
        {
            const vlc_tick_t time = va_arg(args, vlc_tick_t);
            const bool precise = (bool)va_arg(args, int);
            const int ret = SeekTime(demux, time, precise);
            if (ret == VLC_SUCCESS)
                UpdateCurrentSeekpoint(sys, time);
            return ret;
        }

        case DEMUX_SET_POSITION:
        {
            double pos = va_arg(args, double);
            const bool precise = (bool)va_arg(args, int);

            pos = VLC_CLIP(pos, 0.0, 1.0);
            if (sys->duration > 0)
            {
                const vlc_tick_t time = (vlc_tick_t)(pos * sys->duration);
                const int ret = SeekTime(demux, time, precise);
                if (ret == VLC_SUCCESS)
                    UpdateCurrentSeekpoint(sys, time);
                return ret;
            }

            return demux_SetPosition(sys->psdemux, pos, precise);
        }

        case DEMUX_GET_POSITION:
        {
            double *pos = va_arg(args, double *);
            vlc_tick_t time;

            if (sys->duration > 0 &&
                demux_Control(sys->psdemux, DEMUX_GET_TIME, &time) == VLC_SUCCESS)
            {
                *pos = VLC_CLIP((double)time / (double)sys->duration, 0.0, 1.0);
                return VLC_SUCCESS;
            }
            return demux_Control(sys->psdemux, DEMUX_GET_POSITION, pos);
        }

        case DEMUX_GET_TITLE_INFO:
        {
            input_title_t ***titles = va_arg(args, input_title_t ***);
            int *count = va_arg(args, int *);
            int *title_offset = va_arg(args, int *);
            int *chapter_offset = va_arg(args, int *);

            *title_offset = 0;
            *chapter_offset = 0;

            if (sys->title == NULL)
                return VLC_EGENERIC;

            *titles = malloc(sizeof(**titles));
            if (*titles == NULL)
                return VLC_ENOMEM;

            (*titles)[0] = vlc_input_title_Duplicate(sys->title);
            if ((*titles)[0] == NULL)
            {
                free(*titles);
                *titles = NULL;
                return VLC_ENOMEM;
            }

            *count = 1;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE:
            if (sys->title == NULL)
                return VLC_EGENERIC;
            *va_arg(args, int *) = 0;
            return VLC_SUCCESS;

        case DEMUX_SET_TITLE:
            return (va_arg(args, int) == 0 && sys->title != NULL) ? VLC_SUCCESS : VLC_EGENERIC;

        case DEMUX_GET_SEEKPOINT:
            if (sys->title == NULL)
                return VLC_EGENERIC;
            *va_arg(args, int *) = sys->current_seekpoint;
            return VLC_SUCCESS;

        case DEMUX_SET_SEEKPOINT:
        {
            const int seekpoint = va_arg(args, int);
            if (sys->title == NULL || seekpoint < 0 || seekpoint >= sys->title->i_seekpoint)
                return VLC_EGENERIC;

            const vlc_tick_t time = sys->title->seekpoint[seekpoint]->i_time_offset;
            const int ret = SeekTime(demux, time, true);
            if (ret == VLC_SUCCESS)
            {
                sys->current_seekpoint = seekpoint;
                sys->updates |= INPUT_UPDATE_SEEKPOINT;
            }
            return ret;
        }

        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *flags = va_arg(args, unsigned *);
            *flags &= sys->updates;
            sys->updates &= ~*flags;
            return VLC_SUCCESS;
        }

        default:
            return demux_vaControl(sys->psdemux, query, args);
    }
}

static void Close(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    if (sys->psdemux != NULL)
        demux_Delete(sys->psdemux);

    vlc_input_title_Delete(sys->title);
    free(sys->ep);
    if (sys->es_out)
        es_out_Delete(sys->es_out);
    free(sys);
}

static int Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    const uint8_t *header;

    if (vlc_stream_Peek(demux->s, &header, PSMF_MIN_HEADER_SIZE) < PSMF_MIN_HEADER_SIZE)
        return VLC_EGENERIC;

    if (memcmp(header, "PSMF", 4) != 0)
        return VLC_EGENERIC;

    if (header[4] != '0' || header[5] != '0' ||
        header[6] != '1' || header[7] < '0' || header[7] > '9')
        return VLC_EGENERIC;

    const uint32_t data_offset = GetDWBE(header + 8);
    const uint32_t data_size = GetDWBE(header + 12);
    const uint16_t stream_count = GetWBE(header + 0x80);

    if (data_offset < PSMF_MIN_HEADER_SIZE ||
        data_offset > PSMF_MAX_HEADER_SIZE ||
        (data_offset & (PSMF_PS_PACKET_SIZE - 1)) != 0 ||
        data_size == 0 ||
        stream_count > PSMF_MAX_STREAMS)
        return VLC_EGENERIC;

    size_t need = PSMF_MIN_HEADER_SIZE + (size_t)stream_count * PSMF_STREAM_DESC_SIZE;
    if (need > data_offset)
        return VLC_EGENERIC;

    if (vlc_stream_Peek(demux->s, &header, need) < (ssize_t)need)
        return VLC_EGENERIC;

    size_t metadata_need = need;

    const uint32_t mark_offset = GetDWBE(header + 0x10);
    const uint32_t mark_size = GetDWBE(header + 0x14);
    if (mark_offset != 0 && mark_size >= 6 &&
        mark_offset <= data_offset && mark_size <= data_offset - mark_offset)
    {
        metadata_need = __MAX(metadata_need, (size_t)mark_offset + mark_size);
    }

    for(uint16_t i = 0; i < stream_count; ++i)
    {
        const uint8_t *desc = header + PSMF_MIN_HEADER_SIZE +
                              (size_t)i * PSMF_STREAM_DESC_SIZE;
        if ((desc[0] & 0xf0) != 0xe0)
            continue;

        const uint32_t ep_offset = GetDWBE(desc + 4);
        const uint32_t ep_count = GetDWBE(desc + 8);
        if (ep_offset == 0 || ep_count == 0)
            continue;

        size_t ep_bytes;
        if(ckd_mul(&ep_bytes, ep_count, PSMF_ENTRYPOINTS_ENTRY_SIZE))
            continue;

        if (ep_offset > data_offset || ep_bytes > data_offset - ep_offset)
            continue;

        metadata_need = __MAX(metadata_need, (size_t)ep_offset + ep_bytes);
    }

    if (metadata_need > data_offset)
        return VLC_EGENERIC;

    if (vlc_stream_Peek(demux->s, &header, metadata_need) < (ssize_t)metadata_need)
        return VLC_EGENERIC;

    demux_sys_t *sys = calloc(1, sizeof(*sys));
    if (sys == NULL)
        return VLC_ENOMEM;
    demux->p_sys = sys;

    sys->es_out = psmf_es_out_New(demux->out, header, metadata_need, stream_count);
    if (unlikely(!sys->es_out))
        goto error;

    sys->data_offset = data_offset;
    sys->data_size = data_size;
    sys->duration = VLC_TICK_INVALID;

    sys->pts_start = GetQW48BE(header + 0x54);
    sys->pts_end = GetQW48BE(header + 0x5a);

    if (sys->pts_end >= sys->pts_start &&
        sys->pts_end - sys->pts_start <= INT64_MAX)
        sys->duration = vlc_tick_from_samples(sys->pts_end - sys->pts_start, 90000);

    int ret = ParseEntryPointMap(demux, sys, header, metadata_need);
    if (ret != VLC_SUCCESS)
        goto error;

    ret = ParseMarks(demux, sys, header, metadata_need);
    if (ret != VLC_SUCCESS)
        goto error;

    stream_t *p_stream = psmf_stream_New(demux, data_offset, data_size);
    if (p_stream == NULL)
        goto error;

    sys->psdemux = demux_New(VLC_OBJECT(demux), "ps", "psmf://", p_stream, sys->es_out);
    if (sys->psdemux == NULL)
    {
        vlc_stream_Delete(p_stream);
        goto error;
    }

    demux->pf_demux = Demux;
    demux->pf_control = Control;

    msg_Dbg(demux,
            "PSMF version %.4s, PS offset 0x%"PRIx32", size 0x%"PRIx32
            ", duration %"PRId64" ms, EntryPoint entries %zu, marks %d",
            header + 4, data_offset, data_size,
            sys->duration != VLC_TICK_INVALID ? MS_FROM_VLC_TICK(sys->duration) : -1,
            sys->ep_count,
            sys->title ? sys->title->i_seekpoint : 0);

    return VLC_SUCCESS;

error:
    vlc_input_title_Delete(sys->title);
    free(sys->ep);
    if (sys->es_out)
        es_out_Delete(sys->es_out);
    free(sys);
    demux->p_sys = NULL;
    return VLC_EGENERIC;
}

vlc_module_begin()
    set_shortname(N_("PSMF"))
    set_description(N_("PlayStation Movie Format demuxer"))
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_capability("demux", 10)
    set_callbacks(Open, Close)
    add_shortcut("psmf")
    add_file_extension("pmf")
vlc_module_end()
