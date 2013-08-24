/*****************************************************************************
 * mpeg4audio.c: parse and packetize an MPEG 4 audio stream
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block.h>
#include <vlc_bits.h>

#include <vlc_block_helper.h>
#include "packetizer_helper.h"

#include <assert.h>

/* AAC Config in ES:
 *
 * AudioObjectType          5 bits
 * samplingFrequencyIndex   4 bits
 * if (samplingFrequencyIndex == 0xF)
 *  samplingFrequency   24 bits
 * channelConfiguration     4 bits
 * GA_SpecificConfig
 *  FrameLengthFlag         1 bit 1024 or 960
 *  DependsOnCoreCoder      1 bit (always 0)
 *  ExtensionFlag           1 bit (always 0)
 */

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
typedef struct
{
    int i_object_type;
    int i_samplerate;
    int i_channel;
    int i_sbr;          // 0: no sbr, 1: sbr, -1: unknown
    int i_ps;           // 0: no ps,  1: ps,  -1: unknown

    struct
    {
        int i_object_type;
        int i_samplerate;
    } extension;

    /* GASpecific */
    int i_frame_length;   // 1024 or 960

} mpeg4_cfg_t;

#define LATM_MAX_EXTRA_SIZE 64
typedef struct
{
    int i_program;
    int i_layer;

    int i_frame_length_type;
    int i_frame_length;         // type 1
    int i_frame_length_index;   // type 3 4 5 6 7

    mpeg4_cfg_t cfg;

    /* Raw configuration */
    int     i_extra;
    uint8_t extra[LATM_MAX_EXTRA_SIZE];

} latm_stream_t;

#define LATM_MAX_LAYER (8)
#define LATM_MAX_PROGRAM (16)
typedef struct
{
    int b_same_time_framing;
    int i_sub_frames;
    int i_programs;

    int pi_layers[LATM_MAX_PROGRAM];

    int pi_stream[LATM_MAX_PROGRAM][LATM_MAX_LAYER];

    int i_streams;
    latm_stream_t stream[LATM_MAX_PROGRAM*LATM_MAX_LAYER];

    int i_other_data;
    int i_crc;  /* -1 if not set */
} latm_mux_t;

struct decoder_sys_t
{
    /*
     * Input properties
     */
    int i_state;
    int i_type;

    block_bytestream_t bytestream;

    /*
     * Common properties
     */
    date_t  end_date;
    mtime_t i_pts;

    int i_frame_size;
    unsigned int i_channels;
    unsigned int i_rate, i_frame_length, i_header_size;

    int i_input_rate;

    /* LOAS */
    bool b_latm_cfg;
    latm_mux_t latm;
};

enum {
    TYPE_NONE,
    TYPE_RAW,
    TYPE_ADTS,
    TYPE_LOAS
};

static const int pi_sample_rates[16] =
{
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000,  7350,  0,     0,     0
};

#define ADTS_HEADER_SIZE 9
#define LOAS_HEADER_SIZE 3

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenPacketizer(vlc_object_t *);
static void ClosePacketizer(vlc_object_t *);

static block_t *PacketizeRawBlock    (decoder_t *, block_t **);
static block_t *PacketizeStreamBlock(decoder_t *, block_t **);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_PACKETIZER)
    set_description(N_("MPEG4 audio packetizer"))
    set_capability("packetizer", 50)
    set_callbacks(OpenPacketizer, ClosePacketizer)
vlc_module_end ()

/*****************************************************************************
 * OpenPacketizer: probe the packetizer and return score
 *****************************************************************************/
static int OpenPacketizer(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_MP4A)
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    if ((p_dec->p_sys = p_sys = (decoder_sys_t *)malloc(sizeof(decoder_sys_t))) == NULL)
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->i_state = STATE_NOSYNC;
    date_Set(&p_sys->end_date, 0);
    block_BytestreamInit(&p_sys->bytestream);
    p_sys->b_latm_cfg = false;

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_MP4A;

    msg_Dbg(p_dec, "running MPEG4 audio packetizer");

    if (p_dec->fmt_in.i_extra > 0) {
        uint8_t *p_config = (uint8_t*)p_dec->fmt_in.p_extra;
        int     i_index;

        i_index = ((p_config[0] << 1) | (p_config[1] >> 7)) & 0x0f;
        if (i_index != 0x0f) {
            p_dec->fmt_out.audio.i_rate = pi_sample_rates[i_index];
            p_dec->fmt_out.audio.i_frame_length =
                ((p_config[1] >> 2) & 0x01) ? 960 : 1024;
        } else {
            p_dec->fmt_out.audio.i_rate = ((p_config[1] & 0x7f) << 17) |
                (p_config[2] << 9) | (p_config[3] << 1) |
                (p_config[4] >> 7);
            p_dec->fmt_out.audio.i_frame_length =
                ((p_config[4] >> 2) & 0x01) ? 960 : 1024;
        }

        p_dec->fmt_out.audio.i_channels =
            (p_config[i_index == 0x0f ? 4 : 1] >> 3) & 0x0f;

        msg_Dbg(p_dec, "AAC %dHz %d samples/frame",
                 p_dec->fmt_out.audio.i_rate,
                 p_dec->fmt_out.audio.i_frame_length);

        date_Init(&p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1);

        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        p_dec->fmt_out.p_extra = malloc(p_dec->fmt_in.i_extra);
        if (!p_dec->fmt_out.p_extra) {
            p_dec->fmt_out.i_extra = 0;
            return VLC_ENOMEM;
        }
        memcpy(p_dec->fmt_out.p_extra, p_dec->fmt_in.p_extra,
                p_dec->fmt_in.i_extra);

        /* Set callback */
        p_dec->pf_packetize = PacketizeRawBlock;
        p_sys->i_type = TYPE_RAW;
    } else {
        msg_Dbg(p_dec, "no decoder specific info, must be an ADTS or LOAS stream");

        date_Init(&p_sys->end_date, p_dec->fmt_in.audio.i_rate, 1);

        /* We will try to create a AAC Config from adts/loas */
        p_dec->fmt_out.i_extra = 0;
        p_dec->fmt_out.p_extra = NULL;

        /* Set callback */
        p_dec->pf_packetize = PacketizeStreamBlock;
        p_sys->i_type = TYPE_NONE;
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 * PacketizeRawBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete frames.
 ****************************************************************************/
static block_t *PacketizeRawBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    if (!pp_block || !*pp_block)
        return NULL;

    if ((*pp_block)->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED)) {
        date_Set(&p_sys->end_date, 0);
        block_Release(*pp_block);
        return NULL;
    }

    p_block = *pp_block;
    *pp_block = NULL; /* Don't reuse this block */

    if (!date_Get(&p_sys->end_date) && p_block->i_pts <= VLC_TS_INVALID) {
        /* We've just started the stream, wait for the first PTS. */
        block_Release(p_block);
        return NULL;
    } else if (p_block->i_pts > VLC_TS_INVALID &&
             p_block->i_pts != date_Get(&p_sys->end_date)) {
        date_Set(&p_sys->end_date, p_block->i_pts);
    }

    p_block->i_pts = p_block->i_dts = date_Get(&p_sys->end_date);

    p_block->i_length = date_Increment(&p_sys->end_date,
        p_dec->fmt_out.audio.i_frame_length) - p_block->i_pts;

    return p_block;
}

/****************************************************************************
 * ADTS helpers
 ****************************************************************************/
static int ADTSSyncInfo(decoder_t * p_dec, const uint8_t * p_buf,
                         unsigned int * pi_channels,
                         unsigned int * pi_sample_rate,
                         unsigned int * pi_frame_length,
                         unsigned int * pi_header_size)
{
    int i_profile, i_sample_rate_idx, i_frame_size;
    bool b_crc;

    /* Fixed header between frames */
    //int i_id = ((p_buf[1] >> 3) & 0x01) ? 2 : 4; /* MPEG-2 or 4 */
    b_crc = !(p_buf[1] & 0x01);
    i_profile = p_buf[2] >> 6;
    i_sample_rate_idx = (p_buf[2] >> 2) & 0x0f;
    *pi_sample_rate = pi_sample_rates[i_sample_rate_idx];
    //private_bit = (p_buf[2] >> 1) & 0x01;
    *pi_channels = ((p_buf[2] & 0x01) << 2) | ((p_buf[3] >> 6) & 0x03);
    if (*pi_channels == 0) /* workaround broken streams */
        *pi_channels = 2;
    //original_copy = (p_buf[3] >> 5) & 0x01;
    //home = (p_buf[3] >> 4) & 0x01;

    /* Variable header */
    //copyright_id_bit = (p_buf[3] >> 3) & 0x01;
    //copyright_id_start = (p_buf[3] >> 2) & 0x01;
    i_frame_size = ((p_buf[3] & 0x03) << 11) | (p_buf[4] << 3) |
                   ((p_buf[5] >> 5) /*& 0x7*/);
    //uint16_t buffer_fullness = ((p_buf[5] & 0x1f) << 6) | (p_buf[6] >> 2);
    unsigned short i_raw_blocks_in_frame = p_buf[6] & 0x03;

    if (!*pi_sample_rate || !i_frame_size) {
        msg_Warn(p_dec, "Invalid ADTS header");
        return 0;
    }

    *pi_frame_length = 1024;

    if (i_raw_blocks_in_frame == 0) {
        if (b_crc) {
            msg_Warn(p_dec, "ADTS CRC not supported");
            //uint16_t crc = (p_buf[7] << 8) | p_buf[8];
        }
    } else {
        msg_Err(p_dec, "Multiple blocks per frame in ADTS not supported");
        return 0;
#if 0
        int i;
        const uint8_t *p_pos = p_buf + 7;
        uint16_t crc_block;
        uint16_t i_block_pos[3];
        if (b_crc) {
            for (i = 0 ; i < i_raw_blocks_in_frame ; i++) {
                /* the 1st block's position is known ... */
                i_block_pos[i] = (*p_pos << 8) | *(p_pos+1);
                p_pos += 2;
            }
            crc_block = (*p_pos << 8) | *(p_pos+1);
            p_pos += 2;
        }
        for (i = 0 ; i <= i_raw_blocks_in_frame ; i++) {
            //read 1 block
            if (b_crc) {
                msg_Err(p_dec, "ADTS CRC not supported");
                //uint16_t crc = (*p_pos << 8) | *(p_pos+1);
                //p_pos += 2;
            }
        }
#endif
    }


    /* Build the decoder specific info header */
    if (!p_dec->fmt_out.i_extra) {
        p_dec->fmt_out.p_extra = malloc(2);
        if (!p_dec->fmt_out.p_extra) {
            p_dec->fmt_out.i_extra = 0;
            return 0;
        }
        p_dec->fmt_out.i_extra = 2;
        ((uint8_t *)p_dec->fmt_out.p_extra)[0] =
            (i_profile + 1) << 3 | (i_sample_rate_idx >> 1);
        ((uint8_t *)p_dec->fmt_out.p_extra)[1] =
            ((i_sample_rate_idx & 0x01) << 7) | (*pi_channels <<3);
    }

    /* ADTS header length */
    *pi_header_size = b_crc ? 9 : 7;

    return i_frame_size - *pi_header_size;
}

/****************************************************************************
 * LOAS helpers
 ****************************************************************************/
static int LOASSyncInfo(uint8_t p_header[LOAS_HEADER_SIZE], unsigned int *pi_header_size)
{
    *pi_header_size = 3;
    return ((p_header[1] & 0x1f) << 8) + p_header[2];
}

static int Mpeg4GAProgramConfigElement(bs_t *s)
{
    /* TODO compute channels count ? */
    int i_tag = bs_read(s, 4);
    if (i_tag != 0x05)
        return -1;
    bs_skip(s, 2 + 4); // object type + sampling index
    int i_num_front = bs_read(s, 4);
    int i_num_side = bs_read(s, 4);
    int i_num_back = bs_read(s, 4);
    int i_num_lfe = bs_read(s, 2);
    int i_num_assoc_data = bs_read(s, 3);
    int i_num_valid_cc = bs_read(s, 4);

    if (bs_read1(s))
        bs_skip(s, 4); // mono downmix
    if (bs_read1(s))
        bs_skip(s, 4); // stereo downmix
    if (bs_read1(s))
        bs_skip(s, 2+1); // matrix downmix + pseudo_surround

    bs_skip(s, i_num_front * (1+4));
    bs_skip(s, i_num_side * (1+4));
    bs_skip(s, i_num_back * (1+4));
    bs_skip(s, i_num_lfe * (4));
    bs_skip(s, i_num_assoc_data * (4));
    bs_skip(s, i_num_valid_cc * (5));
    bs_align(s);
    int i_comment = bs_read(s, 8);
    bs_skip(s, i_comment * 8);
    return 0;
}

static int Mpeg4GASpecificConfig(mpeg4_cfg_t *p_cfg, bs_t *s)
{
    p_cfg->i_frame_length = bs_read1(s) ? 960 : 1024;

    if (bs_read1(s))     // depend on core coder
        bs_skip(s, 14);   // core coder delay

    int i_extension_flag = bs_read1(s);
    if (p_cfg->i_channel == 0)
        Mpeg4GAProgramConfigElement(s);
    if (p_cfg->i_object_type == 6 || p_cfg->i_object_type == 20)
        bs_skip(s, 3);    // layer

    if (i_extension_flag) {
        if (p_cfg->i_object_type == 22)
            bs_skip(s, 5 + 11);   // numOfSubFrame + layer length
        if (p_cfg->i_object_type == 17 || p_cfg->i_object_type == 19 ||
            p_cfg->i_object_type == 20 || p_cfg->i_object_type == 23)
            bs_skip(s, 1+1+1);    // ER data : section scale spectral */
        if (bs_read1(s))     // extension 3
            fprintf(stderr, "Mpeg4GASpecificConfig: error 1\n");
    }
    return 0;
}

static int Mpeg4ReadAudioObjectType(bs_t *s)
{
    int i_type = bs_read(s, 5);
    if (i_type == 31)
        i_type = 32 + bs_read(s, 6);
    return i_type;
}

static int Mpeg4ReadAudioSamplerate(bs_t *s)
{
    int i_index = bs_read(s, 4);
    if (i_index != 0x0f)
        return pi_sample_rates[i_index];
    return bs_read(s, 24);
}

static int Mpeg4ReadAudioSpecificInfo(mpeg4_cfg_t *p_cfg, int *pi_extra, uint8_t *p_extra, bs_t *s, int i_max_size)
{
#if 0
    static const char *ppsz_otype[] = {
        "NULL",
        "AAC Main", "AAC LC", "AAC SSR", "AAC LTP", "SBR", "AAC Scalable",
        "TwinVQ",
        "CELP", "HVXC",
        "Reserved", "Reserved",
        "TTSI",
        "Main Synthetic", "Wavetables Synthesis", "General MIDI",
        "Algorithmic Synthesis and Audio FX",
        "ER AAC LC",
        "Reserved",
        "ER AAC LTP", "ER AAC Scalable", "ER TwinVQ", "ER BSAC", "ER AAC LD",
        "ER CELP", "ER HVXC", "ER HILN", "ER Parametric",
        "SSC",
        "PS", "Reserved", "Escape",
        "Layer 1", "Layer 2", "Layer 3",
        "DST",
    };
#endif
    const int i_pos_start = bs_pos(s);
    bs_t s_sav = *s;
    int i_bits;
    int i;

    memset(p_cfg, 0, sizeof(*p_cfg));
    *pi_extra = 0;

    p_cfg->i_object_type = Mpeg4ReadAudioObjectType(s);
    p_cfg->i_samplerate = Mpeg4ReadAudioSamplerate(s);

    p_cfg->i_channel = bs_read(s, 4);
    if (p_cfg->i_channel == 7)
        p_cfg->i_channel = 8; // 7.1
    else if (p_cfg->i_channel >= 8)
        p_cfg->i_channel = -1;

    p_cfg->i_sbr = -1;
    p_cfg->i_ps  = -1;
    p_cfg->extension.i_object_type = 0;
    p_cfg->extension.i_samplerate = 0;
    if (p_cfg->i_object_type == 5 || p_cfg->i_object_type == 29) {
        p_cfg->i_sbr = 1;
        if (p_cfg->i_object_type == 29)
           p_cfg->i_ps = 1;
        p_cfg->extension.i_object_type = 5;
        p_cfg->extension.i_samplerate = Mpeg4ReadAudioSamplerate(s);

        p_cfg->i_object_type = Mpeg4ReadAudioObjectType(s);
    }

    switch(p_cfg->i_object_type)
    {
    case 1: case 2: case 3: case 4:
    case 6: case 7:
    case 17: case 19: case 20: case 21: case 22: case 23:
        Mpeg4GASpecificConfig(p_cfg, s);
        break;
    case 8:
        // CelpSpecificConfig();
        break;
    case 9:
        // HvxcSpecificConfig();
        break;
    case 12:
        // TTSSSpecificConfig();
        break;
    case 13: case 14: case 15: case 16:
        // StructuredAudioSpecificConfig();
        break;
    case 24:
        // ERCelpSpecificConfig();
        break;
    case 25:
        // ERHvxcSpecificConfig();
        break;
    case 26: case 27:
        // ParametricSpecificConfig();
        break;
    case 28:
        // SSCSpecificConfig();
        break;
    case 32: case 33: case 34:
        // MPEG_1_2_SpecificConfig();
        break;
    case 35:
        // DSTSpecificConfig();
        break;
    case 36:
        // ALSSpecificConfig();
        break;
    default:
        // error
        break;
    }
    switch(p_cfg->i_object_type)
    {
    case 17: case 19: case 20: case 21: case 22: case 23:
    case 24: case 25: case 26: case 27:
    {
        int epConfig = bs_read(s, 2);
        if (epConfig == 2 || epConfig == 3)
            //ErrorProtectionSpecificConfig();
        if (epConfig == 3)
            if (bs_read1(s)) {
                // TODO : directMapping
            }
        break;
    }
    default:
        break;
    }

    if (p_cfg->extension.i_object_type != 5 && i_max_size > 0 && i_max_size - (bs_pos(s) - i_pos_start) >= 16 &&
        bs_read(s, 11) == 0x2b7) {
        p_cfg->extension.i_object_type = Mpeg4ReadAudioObjectType(s);
        if (p_cfg->extension.i_object_type == 5) {
            p_cfg->i_sbr  = bs_read1(s);
            if (p_cfg->i_sbr == 1) {
                p_cfg->extension.i_samplerate = Mpeg4ReadAudioSamplerate(s);
                if (i_max_size > 0 && i_max_size - (bs_pos(s) - i_pos_start) >= 12 && bs_read(s, 11) == 0x548)
                   p_cfg->i_ps = bs_read1(s);
            }
        }
    }

    //fprintf(stderr, "Mpeg4ReadAudioSpecificInfo: t=%s(%d)f=%d c=%d sbr=%d\n",
    //         ppsz_otype[p_cfg->i_object_type], p_cfg->i_object_type, p_cfg->i_samplerate, p_cfg->i_channel, p_cfg->i_sbr);

    i_bits = bs_pos(s) - i_pos_start;

    *pi_extra = __MIN((i_bits + 7) / 8, LATM_MAX_EXTRA_SIZE);
    for (i = 0; i < *pi_extra; i++) {
        const int i_read = __MIN(8, i_bits - 8*i);
        p_extra[i] = bs_read(&s_sav, i_read) << (8-i_read);
    }
    return i_bits;
}

static int LatmGetValue(bs_t *s)
{
    int i_bytes = bs_read(s, 2);
    int v = 0;
    for (int i = 0; i < i_bytes; i++)
        v = (v << 8) + bs_read(s, 8);

    return v;
}

static int LatmReadStreamMuxConfiguration(latm_mux_t *m, bs_t *s)
{
    int i_mux_version;
    int i_mux_versionA;

    i_mux_version = bs_read(s, 1);
    i_mux_versionA = 0;
    if (i_mux_version)
        i_mux_versionA = bs_read(s, 1);

    if (i_mux_versionA != 0) /* support only A=0 */
        return -1;

    memset(m, 0, sizeof(*m));

    if (i_mux_versionA == 0)
        if (i_mux_version == 1)
            LatmGetValue(s); /* taraBufferFullness */

    m->b_same_time_framing = bs_read1(s);
    m->i_sub_frames = 1 + bs_read(s, 6);
    m->i_programs = 1 + bs_read(s, 4);

    for (int i_program = 0; i_program < m->i_programs; i_program++) {
        m->pi_layers[i_program] = 1+bs_read(s, 3);

        for (int i_layer = 0; i_layer < m->pi_layers[i_program]; i_layer++) {
            latm_stream_t *st = &m->stream[m->i_streams];
            bool b_previous_cfg;

            m->pi_stream[i_program][i_layer] = m->i_streams;
            st->i_program = i_program;
            st->i_layer = i_layer;

            b_previous_cfg = false;
            if (i_program != 0 || i_layer != 0)
                b_previous_cfg = bs_read1(s);

            if (b_previous_cfg) {
                assert(m->i_streams > 0);
                st->cfg = m->stream[m->i_streams-1].cfg;
            } else {
                int i_cfg_size = 0;
                if (i_mux_version == 1)
                    i_cfg_size = LatmGetValue(s);
                i_cfg_size -= Mpeg4ReadAudioSpecificInfo(&st->cfg, &st->i_extra, st->extra, s, i_cfg_size);
                if (i_cfg_size > 0)
                    bs_skip(s, i_cfg_size);
            }

            st->i_frame_length_type = bs_read(s, 3);
            switch(st->i_frame_length_type)
            {
            case 0:
            {
                bs_skip(s, 8); /* latmBufferFullnes */
                if (!m->b_same_time_framing)
                    if (st->cfg.i_object_type == 6 || st->cfg.i_object_type == 20 ||
                        st->cfg.i_object_type == 8 || st->cfg.i_object_type == 24)
                        bs_skip(s, 6); /* eFrameOffset */
                break;
            }
            case 1:
                st->i_frame_length = bs_read(s, 9);
                break;
            case 3: case 4: case 5:
                st->i_frame_length_index = bs_read(s, 6); // celp
                break;
            case 6: case 7:
                st->i_frame_length_index = bs_read(s, 1); // hvxc
            default:
                break;
            }
            /* Next stream */
            m->i_streams++;
        }
    }

    /* other data */
    if (bs_read1(s)) {
        if (i_mux_version == 1)
            m->i_other_data = LatmGetValue(s);
        else {
            int b_continue;
            do {
                b_continue = bs_read1(s);
                m->i_other_data = (m->i_other_data << 8) + bs_read(s, 8);
            } while (b_continue);
        }
    }

    /* crc */
    m->i_crc = -1;
    if (bs_read1(s))
        m->i_crc = bs_read(s, 8);

    return 0;
}

static int LOASParse(decoder_t *p_dec, uint8_t *p_buffer, int i_buffer)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    bs_t s;
    int i_accumulated = 0;

    bs_init(&s, p_buffer, i_buffer);

    /* Read the stream mux configuration if present */
    if (!bs_read1(&s) && !LatmReadStreamMuxConfiguration(&p_sys->latm, &s) &&
            p_sys->latm.i_streams > 0) {
        const latm_stream_t *st = &p_sys->latm.stream[0];

        p_sys->i_channels = st->cfg.i_channel;
        p_sys->i_rate = st->cfg.i_samplerate;
        p_sys->i_frame_length = st->cfg.i_frame_length;

        /* FIXME And if it changes ? */
        if (p_sys->i_channels && p_sys->i_rate && p_sys->i_frame_length > 0) {
            if (!p_dec->fmt_out.i_extra && st->i_extra > 0) {
                p_dec->fmt_out.i_extra = st->i_extra;
                p_dec->fmt_out.p_extra = malloc(st->i_extra);
                if (!p_dec->fmt_out.p_extra) {
                    p_dec->fmt_out.i_extra = 0;
                    return 0;
                }
                memcpy(p_dec->fmt_out.p_extra, st->extra, st->i_extra);
            }
            p_sys->b_latm_cfg = true;
        }
    }

    /* Wait for the configuration */
    if (!p_sys->b_latm_cfg)
        return 0;

    /* FIXME do we need to split the subframe into independent packet ? */
    if (p_sys->latm.i_sub_frames > 1)
        msg_Err(p_dec, "latm sub frames not yet supported, please send a sample");

    for (int i_sub = 0; i_sub < p_sys->latm.i_sub_frames; i_sub++) {
        int pi_payload[LATM_MAX_PROGRAM][LATM_MAX_LAYER];
        if (p_sys->latm.b_same_time_framing) {
            /* Payload length */
            for (int i_program = 0; i_program < p_sys->latm.i_programs; i_program++) {
                for (int i_layer = 0; i_layer < p_sys->latm.pi_layers[i_program]; i_layer++) {
                    latm_stream_t *st = &p_sys->latm.stream[p_sys->latm.pi_stream[i_program][i_layer]];
                    if (st->i_frame_length_type == 0) {
                        int i_payload = 0;
                        for (;;) {
                            int i_tmp = bs_read(&s, 8);
                            i_payload += i_tmp;
                            if (i_tmp != 255)
                                break;
                        }
                        pi_payload[i_program][i_layer] = i_payload;
                    } else if (st->i_frame_length_type == 1) {
                        pi_payload[i_program][i_layer] = st->i_frame_length / 8; /* XXX not correct */
                    } else if ((st->i_frame_length_type == 3) ||
                             (st->i_frame_length_type == 5) ||
                             (st->i_frame_length_type == 7)) {
                        bs_skip(&s, 2); // muxSlotLengthCoded
                        pi_payload[i_program][i_layer] = 0; /* TODO */
                    } else {
                        pi_payload[i_program][i_layer] = 0; /* TODO */
                    }
                }
            }

            /* Payload Data */
            for (int i_program = 0; i_program < p_sys->latm.i_programs; i_program++) {
                for (int i_layer = 0; i_layer < p_sys->latm.pi_layers[i_program]; i_layer++) {
                    /* XXX we only extract 1 stream */
                    if (i_program != 0 || i_layer != 0)
                        break;

                    if (pi_payload[i_program][i_layer] <= 0)
                        continue;

                    /* FIXME that's slow (and a bit ugly to write in place) */
                    for (int i = 0; i < pi_payload[i_program][i_layer]; i++) {
                        if (i_accumulated >= i_buffer)
                            return 0;
                        p_buffer[i_accumulated++] = bs_read(&s, 8);
                    }
                }
            }
        } else {
            const int i_chunks = bs_read(&s, 4);
            int pi_program[16];
            int pi_layer[16];

            msg_Err(p_dec, "latm without same time frameing not yet supported, please send a sample");

            for (int i_chunk = 0; i_chunk < i_chunks; i_chunk++) {
                const int streamIndex = bs_read(&s, 4);
                latm_stream_t *st = &p_sys->latm.stream[streamIndex];
                const int i_program = st->i_program;
                const int i_layer = st->i_layer;

                pi_program[i_chunk] = i_program;
                pi_layer[i_chunk] = i_layer;

                if (st->i_frame_length_type == 0) {
                    int i_payload = 0;
                    for (;;) {
                        int i_tmp = bs_read(&s, 8);
                        i_payload += i_tmp;
                        if (i_tmp != 255)
                            break;
                    }
                    pi_payload[i_program][i_layer] = i_payload;
                    bs_skip(&s, 1); // auEndFlag
                } else if (st->i_frame_length_type == 1) {
                    pi_payload[i_program][i_layer] = st->i_frame_length / 8; /* XXX not correct */
                } else if ((st->i_frame_length_type == 3) ||
                         (st->i_frame_length_type == 5) ||
                         (st->i_frame_length_type == 7)) {
                    bs_read(&s, 2); // muxSlotLengthCoded
                }
            }
            for (int i_chunk = 0; i_chunk < i_chunks; i_chunk++) {
                //const int i_program = pi_program[i_chunk];
                //const int i_layer = pi_layer[i_chunk];

                /* TODO ? Payload */
            }
        }
    }

#if 0
    if (p_sys->latm.i_other_data > 0)
        ; // TODO
#endif
    bs_align(&s);

    return i_accumulated;
}

/****************************************************************************
 * PacketizeStreamBlock: ADTS/LOAS packetizer
 ****************************************************************************/
static void SetupOutput(decoder_t *p_dec, block_t *p_block);
static block_t *PacketizeStreamBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[ADTS_HEADER_SIZE + LOAS_HEADER_SIZE];
    block_t *p_out_buffer;
    uint8_t *p_buf;

    if (!pp_block || !*pp_block)
        return NULL;

    if ((*pp_block)->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED)) {
        if ((*pp_block)->i_flags&BLOCK_FLAG_CORRUPTED) {
            p_sys->i_state = STATE_NOSYNC;
            block_BytestreamEmpty(&p_sys->bytestream);
        }
        date_Set(&p_sys->end_date, 0);
        block_Release(*pp_block);
        return NULL;
    }

    if (!date_Get(&p_sys->end_date) && (*pp_block)->i_pts <= VLC_TS_INVALID) {
        /* We've just started the stream, wait for the first PTS. */
        block_Release(*pp_block);
        return NULL;
    }

    block_BytestreamPush(&p_sys->bytestream, *pp_block);

    for (;;)
    {
        switch(p_sys->i_state)
        {

        case STATE_NOSYNC:
            while (block_PeekBytes(&p_sys->bytestream, p_header, 2)
                   == VLC_SUCCESS)
            {
                /* Look for sync word - should be 0xfff(adts) or 0x2b7(loas) */
                if (p_header[0] == 0xff && (p_header[1] & 0xf6) == 0xf0) {
                    if (p_sys->i_type != TYPE_ADTS)
                        msg_Dbg(p_dec, "detected ADTS format");

                    p_sys->i_state = STATE_SYNC;
                    p_sys->i_type = TYPE_ADTS;
                    break;
                } else if (p_header[0] == 0x56 && (p_header[1] & 0xe0) == 0xe0) {
                    if (p_sys->i_type != TYPE_LOAS)
                        msg_Dbg(p_dec, "detected LOAS format");

                    p_sys->i_state = STATE_SYNC;
                    p_sys->i_type = TYPE_LOAS;
                    break;
                }
                block_SkipByte(&p_sys->bytestream);
            }
            if (p_sys->i_state != STATE_SYNC) {
                block_BytestreamFlush(&p_sys->bytestream);

                /* Need more data */
                return NULL;
            }

        case STATE_SYNC:
            /* New frame, set the Presentation Time Stamp */
            p_sys->i_pts = p_sys->bytestream.p_block->i_pts;
            if (p_sys->i_pts > VLC_TS_INVALID &&
                p_sys->i_pts != date_Get(&p_sys->end_date))
                date_Set(&p_sys->end_date, p_sys->i_pts);
            p_sys->i_state = STATE_HEADER;
            break;

        case STATE_HEADER:
            if (p_sys->i_type == TYPE_ADTS) {
                /* Get ADTS frame header (ADTS_HEADER_SIZE bytes) */
                if (block_PeekBytes(&p_sys->bytestream, p_header,
                                     ADTS_HEADER_SIZE) != VLC_SUCCESS)
                    return NULL; /* Need more data */

                /* Check if frame is valid and get frame info */
                p_sys->i_frame_size = ADTSSyncInfo(p_dec, p_header,
                                                    &p_sys->i_channels,
                                                    &p_sys->i_rate,
                                                    &p_sys->i_frame_length,
                                                    &p_sys->i_header_size);
            } else {
                assert(p_sys->i_type == TYPE_LOAS);
                /* Get LOAS frame header (LOAS_HEADER_SIZE bytes) */
                if (block_PeekBytes(&p_sys->bytestream, p_header,
                                     LOAS_HEADER_SIZE) != VLC_SUCCESS)
                    return NULL; /* Need more data */

                /* Check if frame is valid and get frame info */
                p_sys->i_frame_size = LOASSyncInfo(p_header, &p_sys->i_header_size);
            }

            if (p_sys->i_frame_size <= 0) {
                msg_Dbg(p_dec, "emulated sync word");
                block_SkipByte(&p_sys->bytestream);
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            p_sys->i_state = STATE_NEXT_SYNC;

        case STATE_NEXT_SYNC:
            /* TODO: If p_block == NULL, flush the buffer without checking the
             * next sync word */
            if (p_sys->bytestream.p_block == NULL) {
                p_sys->i_state = STATE_NOSYNC;
                block_BytestreamFlush(&p_sys->bytestream);
                return NULL;
            }

            /* Check if next expected frame contains the sync word */
            if (block_PeekOffsetBytes(&p_sys->bytestream, p_sys->i_frame_size
                   + p_sys->i_header_size, p_header, 2) != VLC_SUCCESS)
                return NULL; /* Need more data */

            assert((p_sys->i_type == TYPE_ADTS) || (p_sys->i_type == TYPE_LOAS));
            if (((p_sys->i_type == TYPE_ADTS) &&
                  (p_header[0] != 0xff || (p_header[1] & 0xf6) != 0xf0)) ||
                ((p_sys->i_type == TYPE_LOAS) &&
                  (p_header[0] != 0x56 || (p_header[1] & 0xe0) != 0xe0))) {
                msg_Dbg(p_dec, "emulated sync word "
                         "(no sync on following frame)");
                p_sys->i_state = STATE_NOSYNC;
                block_SkipByte(&p_sys->bytestream);
                break;
            }

            p_sys->i_state = STATE_SEND_DATA;
            break;

        case STATE_GET_DATA:
            /* Make sure we have enough data.
             * (Not useful if we went through NEXT_SYNC) */
            if (block_WaitBytes(&p_sys->bytestream, p_sys->i_frame_size +
                                 p_sys->i_header_size) != VLC_SUCCESS)
                return NULL; /* Need more data */
            p_sys->i_state = STATE_SEND_DATA;

        case STATE_SEND_DATA:
            /* When we reach this point we already know we have enough
             * data available. */

            p_out_buffer = block_Alloc(p_sys->i_frame_size);
            if (!p_out_buffer) {
                //p_dec->b_error = true;
                return NULL;
            }
            p_buf = p_out_buffer->p_buffer;

            /* Skip the ADTS/LOAS header */
            block_SkipBytes(&p_sys->bytestream, p_sys->i_header_size);

            if (p_sys->i_type == TYPE_ADTS) {
                /* Copy the whole frame into the buffer */
                block_GetBytes(&p_sys->bytestream, p_buf, p_sys->i_frame_size);
            } else {
                assert(p_sys->i_type == TYPE_LOAS);
                /* Copy the whole frame into the buffer and parse/extract it */
                block_GetBytes(&p_sys->bytestream, p_buf, p_sys->i_frame_size);
                p_out_buffer->i_buffer = LOASParse(p_dec, p_buf, p_sys->i_frame_size);
                if (p_out_buffer->i_buffer <= 0) {
                    if (!p_sys->b_latm_cfg)
                        msg_Warn(p_dec, "waiting for header");

                    block_Release(p_out_buffer);
                    p_out_buffer = NULL;
                    p_sys->i_state = STATE_NOSYNC;
                    break;
                }
            }
            SetupOutput(p_dec, p_out_buffer);
            /* Make sure we don't reuse the same pts twice */
            if (p_sys->i_pts == p_sys->bytestream.p_block->i_pts)
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = VLC_TS_INVALID;

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop(&p_sys->bytestream);

            p_sys->i_state = STATE_NOSYNC;

            return p_out_buffer;
        }
    }

    return NULL;
}

/*****************************************************************************
 * SetupBuffer:
 *****************************************************************************/
static void SetupOutput(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_dec->fmt_out.audio.i_rate != p_sys->i_rate) {
        msg_Info(p_dec, "AAC channels: %d samplerate: %d",
                  p_sys->i_channels, p_sys->i_rate);

        const mtime_t i_end_date = date_Get(&p_sys->end_date);
        date_Init(&p_sys->end_date, p_sys->i_rate, 1);
        date_Set(&p_sys->end_date, i_end_date);
    }

    p_dec->fmt_out.audio.i_rate     = p_sys->i_rate;
    p_dec->fmt_out.audio.i_channels = p_sys->i_channels;
    p_dec->fmt_out.audio.i_bytes_per_frame = p_sys->i_frame_size;
    p_dec->fmt_out.audio.i_frame_length = p_sys->i_frame_length;

#if 0
    p_dec->fmt_out.audio.i_original_channels = p_sys->i_channels_conf;
    p_dec->fmt_out.audio.i_physical_channels = p_sys->i_channels_conf;
#endif

    p_block->i_pts = p_block->i_dts = date_Get(&p_sys->end_date);

    p_block->i_length =
        date_Increment(&p_sys->end_date, p_sys->i_frame_length) - p_block->i_pts;
}

/*****************************************************************************
 * ClosePacketizer: clean up the packetizer
 *****************************************************************************/
static void ClosePacketizer(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease(&p_sys->bytestream);

    free(p_dec->p_sys);
}
