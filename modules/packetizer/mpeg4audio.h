/*****************************************************************************
 * mpeg4audio.h: ISO/IEC 14496-3 audio definitions
 *****************************************************************************
 * Copyright (C) 2001-2024 VLC authors, VideoLAN and VideoLabs
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
#include <vlc_bits.h>

enum MPEG4_audioObjectType /* ISO/IEC 14496-3:2009 1.5.1 */
{
    MPEG4_AOT_NULL            = 0,
    MPEG4_AOT_AAC_MAIN        = 1,
    MPEG4_AOT_AAC_LC          = 2,
    MPEG4_AOT_AAC_SSR         = 3,
    MPEG4_AOT_AAC_LTP         = 4,
    MPEG4_AOT_AAC_SBR         = 5,
    MPEG4_AOT_AAC_SC          = 6,
    MPEG4_AOT_TWINVQ          = 7,
    MPEG4_AOT_CELP            = 8,
    MPEG4_AOT_HVXC            = 9,
    MPEG4_AOT_RESERVED10      = 10,
    MPEG4_AOT_RESERVED11      = 11,
    MPEG4_AOT_TTSI            = 12,
    MPEG4_AOT_MAIN_SYNTHETIC  = 13,
    MPEG4_AOT_WAVETABLES      = 14,
    MPEG4_AOT_GENERAL_MIDI    = 15,
    MPEG4_AOT_ALGORITHMIC     = 16,
    MPEG4_AOT_ER_AAC_LC       = 17,
    MPEG4_AOT_RESERVED18      = 18,
    MPEG4_AOT_ER_AAC_LTP      = 19,
    MPEG4_AOT_ER_AAC_SC       = 20,
    MPEG4_AOT_ER_TWINVQ       = 21,
    MPEG4_AOT_ER_BSAC         = 22,
    MPEG4_AOT_ER_AAC_LD       = 23,
    MPEG4_AOT_ER_CELP         = 24,
    MPEG4_AOT_ER_HXVC         = 25,
    MPEG4_AOT_ER_HILN         = 26,
    MPEG4_AOT_ER_Parametric   = 27,
    MPEG4_AOT_SSC             = 28,
    MPEG4_AOT_AAC_PS          = 29,
    MPEG4_AOT_MPEG_SURROUND   = 30,
    MPEG4_AOT_ESCAPE          = 31,
    MPEG4_AOT_LAYER1          = 32,
    MPEG4_AOT_LAYER2          = 33,
    MPEG4_AOT_LAYER3          = 34,
    MPEG4_AOT_DST             = 35,
    MPEG4_AOT_ALS             = 36,
    MPEG4_AOT_SLS             = 37,
    MPEG4_AOT_SLS_NON_CORE    = 38,
    MPEG4_AOT_ER_AAC_ELD      = 39,
    MPEG4_AOT_SMR_SIMPLE      = 40,
    MPEG4_AOT_SMR_MAIN        = 41,
};

enum
{
    AAC_PROFILE_MAIN = MPEG4_AOT_AAC_MAIN - 1,
    AAC_PROFILE_LC,
    AAC_PROFILE_SSR,
    AAC_PROFILE_LTP,
    AAC_PROFILE_HE,
    AAC_PROFILE_LD   = MPEG4_AOT_ER_AAC_LD - 1,
    AAC_PROFILE_HEv2 = MPEG4_AOT_AAC_PS - 1,
    AAC_PROFILE_ELD  = MPEG4_AOT_ER_AAC_ELD - 1,
    /* Similar shift signaling as avcodec, as signaling should have been
       done in ADTS header. Values defaults to MPEG4 */
    AAC_PROFILE_MPEG2_LC = AAC_PROFILE_LC + 128,
    AAC_PROFILE_MPEG2_HE = AAC_PROFILE_HE + 128,
};

static const uint32_t MPEG4_asc_channelsbyindex[] =
{
    [0] = 0, /* Set later */

    [1] = AOUT_CHAN_CENTER, // Mono

    [2] = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT, // Stereo

    [3] = AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT, // 2.1ch 3.0

    [4] = AOUT_CHAN_CENTER | // 4ch 3.1
          AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
          AOUT_CHAN_REARCENTER,

    [5] = AOUT_CHAN_CENTER | // 5ch 3.2
          AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
          AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,

    [6] = AOUT_CHAN_CENTER | // 5.1ch 3.2.1
          AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
          AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
          AOUT_CHAN_LFE,

    [7] = AOUT_CHAN_CENTER | // 7.1ch 5.2.1
          AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
          AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT |
          AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
          AOUT_CHAN_LFE,

    [8] = 0,
};

#define MPEG4_ASC_MAX_INDEXEDPOS ARRAY_SIZE(MPEG4_asc_channelsbyindex)

typedef struct
{
    enum MPEG4_audioObjectType i_object_type;
    unsigned i_samplerate;
    uint8_t i_channel_configuration;
    int8_t i_sbr;          // 0: no sbr, 1: sbr, -1: unknown
    int8_t i_ps;           // 0: no ps,  1: ps,  -1: unknown

    struct
    {
        enum MPEG4_audioObjectType i_object_type;
        unsigned i_samplerate;
        uint8_t i_channel_configuration;
    } extension;

    /* GASpecific */
    unsigned i_frame_length;   // 1024 or 960

} MPEG4_asc_t;

static inline int MPEG4_read_GAProgramConfigElement(bs_t *s)
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

static inline int MPEG4_read_GASpecificConfig(MPEG4_asc_t *p_cfg, bs_t *s)
{
    p_cfg->i_frame_length = bs_read1(s) ? 960 : 1024;
    if(p_cfg->i_object_type == MPEG4_AOT_ER_AAC_LD) /* 14496-3 4.5.1.1 */
        p_cfg->i_frame_length >>= 1;
    else if(p_cfg->i_object_type == MPEG4_AOT_AAC_SSR)
        p_cfg->i_frame_length = 256;

    if (bs_read1(s))     // depend on core coder
        bs_skip(s, 14);   // core coder delay

    int i_extension_flag = bs_read1(s);
    if (p_cfg->i_channel_configuration == 0)
        MPEG4_read_GAProgramConfigElement(s);
    if (p_cfg->i_object_type == MPEG4_AOT_AAC_SC ||
        p_cfg->i_object_type == MPEG4_AOT_ER_AAC_SC)
        bs_skip(s, 3);    // layer

    if (i_extension_flag) {
        if (p_cfg->i_object_type == MPEG4_AOT_ER_BSAC)
            bs_skip(s, 5 + 11);   // numOfSubFrame + layer length
        if (p_cfg->i_object_type == MPEG4_AOT_ER_AAC_LC ||
            p_cfg->i_object_type == MPEG4_AOT_ER_AAC_LTP ||
            p_cfg->i_object_type == MPEG4_AOT_ER_AAC_SC ||
            p_cfg->i_object_type == MPEG4_AOT_ER_AAC_LD)
            bs_skip(s, 1+1+1);    // ER data : section scale spectral */
        if (bs_read1(s))     // extension 3
            fprintf(stderr, "MPEG4GASpecificConfig: error 1\n");
    }
    return 0;
}

static inline int MPEG4_read_ELDSpecificConfig(MPEG4_asc_t *p_cfg, bs_t *s)
{
    p_cfg->i_frame_length = bs_read1(s) ? 480 : 512;

    /* ELDSpecificConfig Table 4.180 */

    bs_skip(s, 3);
    if(bs_read1(s)) /* ldSbrPresentFlag */
    {
        bs_skip(s, 2);
        /* ld_sbr_header(channelConfiguration) Table 4.181 */
        unsigned numSbrHeader;
        switch(p_cfg->i_channel_configuration)
        {
        case 1: case 2:
            numSbrHeader = 1;
            break;
        case 3:
            numSbrHeader = 2;
            break;
        case 4: case 5: case 6:
            numSbrHeader = 3;
            break;
        case 7:
            numSbrHeader = 4;
            break;
        default:
            numSbrHeader = 0;
            break;
        }
        for( ; numSbrHeader; numSbrHeader-- )
        {
            /* sbr_header() Table 4.63 */
            bs_read(s, 14);
            bool header_extra_1 = bs_read1(s);
            bool header_extra_2 = bs_read1(s);
            if(header_extra_1)
                bs_read(s, 5);
            if(header_extra_2)
                bs_read(s, 6);
        }
    }

    for(unsigned eldExtType = bs_read(s, 4);
         eldExtType != 0x0 /* ELDEXT_TERM */;
         eldExtType = bs_read(s, 4))
    {
        unsigned eldExtLen = bs_read(s, 4);
        unsigned eldExtLenAdd = 0;
        if(eldExtLen == 15)
        {
            eldExtLenAdd = bs_read(s, 8);
            eldExtLen += eldExtLenAdd;
        }
        if(eldExtLenAdd == 255)
            eldExtLen += bs_read(s, 16);
        /* reserved extensions */
        for(; eldExtLen; eldExtLen--)
            bs_skip(s, 8);
    }

    return 0;
}

static inline enum MPEG4_audioObjectType MPEG4_read_AudioObjectType(bs_t *s)
{
    int i_type = bs_read(s, 5);
    if (i_type == 31)
        i_type = 32 + bs_read(s, 6);
    return (enum MPEG4_audioObjectType) i_type;
}

static const int pi_sample_rates[16] =
    {
        96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
        16000, 12000, 11025, 8000,  7350,  0,     0,     0
};

static inline unsigned MPEG4_read_AudioSamplerate(bs_t *s)
{
    int i_index = bs_read(s, 4);
    if (i_index != 0x0f)
        return pi_sample_rates[i_index];
    return bs_read(s, 24);
}

static inline int MPEG4_read_AudioSpecificConfig(bs_t *s, MPEG4_asc_t *p_cfg, bool b_withext)
{
    p_cfg->i_object_type = MPEG4_read_AudioObjectType(s);
    p_cfg->i_samplerate = MPEG4_read_AudioSamplerate(s);
    p_cfg->i_channel_configuration = bs_read(s, 4);

    p_cfg->i_sbr = -1;
    p_cfg->i_ps  = -1;
    p_cfg->extension.i_object_type = MPEG4_AOT_NULL;
    p_cfg->extension.i_samplerate = 0;
    p_cfg->extension.i_channel_configuration = 0;
    p_cfg->i_frame_length = 0;

    if (p_cfg->i_object_type == MPEG4_AOT_AAC_SBR ||
        p_cfg->i_object_type == MPEG4_AOT_AAC_PS) {
        p_cfg->i_sbr = 1;
        if (p_cfg->i_object_type == MPEG4_AOT_AAC_PS)
            p_cfg->i_ps = 1;
        p_cfg->extension.i_object_type = MPEG4_AOT_AAC_SBR;
        p_cfg->extension.i_samplerate = MPEG4_read_AudioSamplerate(s);

        p_cfg->i_object_type = MPEG4_read_AudioObjectType(s);
        if(p_cfg->i_object_type == MPEG4_AOT_ER_BSAC)
            p_cfg->extension.i_channel_configuration = bs_read(s, 4);
    }

    switch(p_cfg->i_object_type)
    {
    case MPEG4_AOT_AAC_MAIN:
    case MPEG4_AOT_AAC_LC:
    case MPEG4_AOT_AAC_SSR:
    case MPEG4_AOT_AAC_LTP:
    case MPEG4_AOT_AAC_SC:
    case MPEG4_AOT_TWINVQ:
    case MPEG4_AOT_ER_AAC_LC:
    case MPEG4_AOT_ER_AAC_LTP:
    case MPEG4_AOT_ER_AAC_SC:
    case MPEG4_AOT_ER_TWINVQ:
    case MPEG4_AOT_ER_BSAC:
    case MPEG4_AOT_ER_AAC_LD:
        MPEG4_read_GASpecificConfig(p_cfg, s);
        break;
    case MPEG4_AOT_CELP:
        // CelpSpecificConfig();
    case MPEG4_AOT_HVXC:
        // HvxcSpecificConfig();
    case MPEG4_AOT_TTSI:
        // TTSSSpecificConfig();
    case MPEG4_AOT_MAIN_SYNTHETIC:
    case MPEG4_AOT_WAVETABLES:
    case MPEG4_AOT_GENERAL_MIDI:
    case MPEG4_AOT_ALGORITHMIC:
        // StructuredAudioSpecificConfig();
    case MPEG4_AOT_ER_CELP:
        // ERCelpSpecificConfig();
    case MPEG4_AOT_ER_HXVC:
        // ERHvxcSpecificConfig();
    case MPEG4_AOT_ER_HILN:
    case MPEG4_AOT_ER_Parametric:
        // ParametricSpecificConfig();
    case MPEG4_AOT_SSC:
        // SSCSpecificConfig();
    case MPEG4_AOT_LAYER1:
    case MPEG4_AOT_LAYER2:
    case MPEG4_AOT_LAYER3:
        // MPEG_1_2_SpecificConfig();
    case MPEG4_AOT_DST:
        // DSTSpecificConfig();
    case MPEG4_AOT_ALS:
        // ALSSpecificConfig();
    case MPEG4_AOT_SLS:
    case MPEG4_AOT_SLS_NON_CORE:
        // SLSSpecificConfig();
    case MPEG4_AOT_ER_AAC_ELD:
        MPEG4_read_ELDSpecificConfig(p_cfg, s);
        break;
    case MPEG4_AOT_SMR_SIMPLE:
    case MPEG4_AOT_SMR_MAIN:
        // SymbolicMusicSpecificConfig();
    default:
        // error
        return VLC_EGENERIC;
    }

    switch(p_cfg->i_object_type)
    {
    case MPEG4_AOT_ER_AAC_LC:
    case MPEG4_AOT_ER_AAC_LTP:
    case MPEG4_AOT_ER_AAC_SC:
    case MPEG4_AOT_ER_TWINVQ:
    case MPEG4_AOT_ER_BSAC:
    case MPEG4_AOT_ER_AAC_LD:
    case MPEG4_AOT_ER_CELP:
    case MPEG4_AOT_ER_HXVC:
    case MPEG4_AOT_ER_HILN:
    case MPEG4_AOT_ER_Parametric:
    case MPEG4_AOT_ER_AAC_ELD:
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

    if (b_withext && p_cfg->extension.i_object_type != MPEG4_AOT_AAC_SBR &&
        !bs_eof(s) && bs_read(s, 11) == 0x2b7)
    {
        p_cfg->extension.i_object_type = MPEG4_read_AudioObjectType(s);
        if (p_cfg->extension.i_object_type == MPEG4_AOT_AAC_SBR)
        {
            p_cfg->i_sbr  = bs_read1(s);
            if (p_cfg->i_sbr == 1) {
                p_cfg->extension.i_samplerate = MPEG4_read_AudioSamplerate(s);
                if (bs_read(s, 11) == 0x548)
                    p_cfg->i_ps = bs_read1(s);
            }
        }
        else if (p_cfg->extension.i_object_type == MPEG4_AOT_ER_BSAC)
        {
            p_cfg->i_sbr  = bs_read1(s);
            if(p_cfg->i_sbr)
                p_cfg->extension.i_samplerate = MPEG4_read_AudioSamplerate(s);
            p_cfg->extension.i_channel_configuration = bs_read(s, 4);
        }
    }

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
        "PS", "MPEG Surround", "Escape",
        "Layer 1", "Layer 2", "Layer 3",
        "DST", "ALS", "SLS", "SLS non-core", "ELD",
        "SMR Simple", "SMR Main",
    };

    fprintf(stderr, "MPEG4ReadAudioSpecificInfo: t=%s(%d)f=%d c=%d sbr=%d\n",
            ppsz_otype[p_cfg->i_object_type], p_cfg->i_object_type,
            p_cfg->i_samplerate, p_cfg->i_channel, p_cfg->i_sbr);
#endif
    return bs_error(s) ? VLC_EGENERIC : VLC_SUCCESS;
}

#define MPEG4_STREAM_MAX_EXTRADATA 64
typedef struct
{
    uint8_t i_program;
    uint8_t i_layer;

    unsigned i_frame_length;         // type 1
    uint8_t i_frame_length_type;
    uint8_t i_frame_length_index;   // type 3 4 5 6 7

    MPEG4_asc_t cfg;

    /* Raw configuration */
    size_t i_extra;
    uint8_t extra[MPEG4_STREAM_MAX_EXTRADATA];

} MPEG4_audio_stream_t;

#define MPEG4_STREAMMUX_MAX_LAYER   8
#define MPEG4_STREAMMUX_MAX_PROGRAM 16
typedef struct
{
    bool b_same_time_framing;
    uint8_t i_sub_frames;
    uint8_t i_programs;

    uint8_t pi_layers[MPEG4_STREAMMUX_MAX_PROGRAM];

    uint8_t pi_stream[MPEG4_STREAMMUX_MAX_PROGRAM][MPEG4_STREAMMUX_MAX_LAYER];

    uint8_t i_streams;
    MPEG4_audio_stream_t stream[MPEG4_STREAMMUX_MAX_PROGRAM*MPEG4_STREAMMUX_MAX_LAYER];

    uint32_t i_other_data;
    int16_t  i_crc;  /* -1 if not set */
} MPEG4_streammux_config_t;

static inline uint32_t MPEG4_LatmGetValue(bs_t *s)
{
    uint32_t v = 0;
    for (int i = 1 + bs_read(s, 2); i > 0; i--)
        v = (v << 8) + bs_read(s, 8);
    return v;
}

static inline size_t AudioSpecificConfigBitsToBytes(bs_t *s, uint32_t i_bits, uint8_t *p_data)
{
    size_t i_extra = __MIN((i_bits + 7) / 8, MPEG4_STREAM_MAX_EXTRADATA);
    for (size_t i = 0; i < i_extra; i++) {
        const uint32_t i_read = __MIN(8, i_bits - 8*i);
        p_data[i] = bs_read(s, i_read) << (8-i_read);
    }
    return i_extra;
}

static inline int MPEG4_parse_StreamMuxConfig(bs_t *s, MPEG4_streammux_config_t *m)
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
            MPEG4_LatmGetValue(s); /* taraBufferFullness */

    if(bs_eof(s))
        return -1;

    m->b_same_time_framing = bs_read1(s);
    m->i_sub_frames = 1 + bs_read(s, 6);
    m->i_programs = 1 + bs_read(s, 4);

    for (uint8_t i_program = 0; i_program < m->i_programs; i_program++) {
        if(bs_eof(s))
            return -1;
        m->pi_layers[i_program] = 1+bs_read(s, 3);

        for (uint8_t i_layer = 0; i_layer < m->pi_layers[i_program]; i_layer++) {
            MPEG4_audio_stream_t *st = &m->stream[m->i_streams];
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
                uint32_t asc_size = 0;
                if(i_mux_version > 0)
                    asc_size = MPEG4_LatmGetValue(s);
                bs_t asc_bs = *s;
                MPEG4_read_AudioSpecificConfig(&asc_bs, &st->cfg, i_mux_version > 0);
                if (i_mux_version == 0)
                    asc_size = bs_pos(&asc_bs) - bs_pos(s);
                asc_bs = *s;
                st->i_extra = AudioSpecificConfigBitsToBytes(&asc_bs, asc_size, st->extra);
                bs_skip(s, asc_size);
            }

            st->i_frame_length_type = bs_read(s, 3);
            switch(st->i_frame_length_type)
            {
            case 0:
            {
                bs_skip(s, 8); /* latmBufferFullnes */
                if (!m->b_same_time_framing)
                    if (st->cfg.i_object_type == MPEG4_AOT_AAC_SC ||
                        st->cfg.i_object_type == MPEG4_AOT_CELP ||
                        st->cfg.i_object_type == MPEG4_AOT_ER_AAC_SC ||
                        st->cfg.i_object_type == MPEG4_AOT_ER_CELP)
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

    if(bs_error(s) || bs_eof(s))
        return -1;

    /* other data */
    if (bs_read1(s)) {
        if (i_mux_version == 1)
            m->i_other_data = MPEG4_LatmGetValue(s);
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

    return bs_error(s) ? -1 : 0;
}

