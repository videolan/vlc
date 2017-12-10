/*****************************************************************************
 * aac.c: FDK-AAC Encoder plugin for vlc.
 *****************************************************************************
 * Copyright (C) 2012 Sergio Ammirata
 *
 * Authors: Sergio Ammirata <sergio@ammirata.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Alternatively you can redistribute this file under the terms of the
 *  BSD license as stated below:
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fdk-aac/aacenc_lib.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

static int OpenEncoder(vlc_object_t *);
static void CloseEncoder(vlc_object_t *);

#define ENC_CFG_PREFIX "sout-fdkaac-"

#define AOT_TEXT N_("Encoder Profile")
#define AOT_LONGTEXT N_("Encoder Algorithm to use.")

#define SIDEBAND_TEXT N_("Enable spectral band replication")
#define SIDEBAND_LONGTEXT N_("This is an optional feature only for the AAC-ELD profile.")

#define VBR_QUALITY_TEXT N_("VBR Quality")
#define VBR_QUALITY_LONGTEXT N_("Quality of the VBR Encoding (0=cbr, 1-5 constant vbr quality, 5 is the best).")

#define AFTERBURNER_TEXT N_("Enable afterburner library")
#define AFTERBURNER_LONGTEXT N_("This library will produce higher quality audio at the expense of additional CPU usage (default is enabled).")

#define SIGNALING_TEXT N_("Signaling mode of the extension AOT")
#define SIGNALING_LONGTEXT N_("1 is explicit for SBR and implicit for PS (default), 2 is explicit hierarchical.")

#define  CH_ORDER_MPEG 0  /*!< MPEG channel ordering (e. g. 5.1: C, L, R, SL, SR, LFE)           */
#define  CH_ORDER_WAV 1   /*!< WAV fileformat channel ordering (e. g. 5.1: L, R, C, LFE, SL, SR) */
#define  CH_ORDER_WG4 2   /*!< WG4 fileformat channel ordering (e. g. 5.1: L, R, SL, SR, C, LFE) */

#define PROFILE_AAC_LC 2
#define PROFILE_AAC_HE 5
#define PROFILE_AAC_HE_v2 29
#define PROFILE_AAC_LD 23
#define PROFILE_AAC_ELD 39

#define SIGNALING_COMPATIBLE 1
#define SIGNALING_HIERARCHICAL 2

static const int pi_aot_values[] = { PROFILE_AAC_LC, PROFILE_AAC_HE, PROFILE_AAC_HE_v2, PROFILE_AAC_LD, PROFILE_AAC_ELD };
static const char *const ppsz_aot_descriptions[] =
{ N_("AAC-LC"), N_("HE-AAC"), N_("HE-AAC-v2"), N_("AAC-LD"), N_("AAC-ELD") };

vlc_module_begin ()
    set_shortname(N_("FDKAAC"))
    set_description(N_("FDK-AAC Audio encoder"))
    set_capability("encoder", 150)
    set_callbacks(OpenEncoder, CloseEncoder)
    add_shortcut("fdkaac")
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACODEC)
    add_integer(ENC_CFG_PREFIX "profile", PROFILE_AAC_LC, AOT_TEXT,
             AOT_LONGTEXT, false)
    change_integer_list(pi_aot_values, ppsz_aot_descriptions);
    add_bool(ENC_CFG_PREFIX "sbr", false, SIDEBAND_TEXT,
              SIDEBAND_LONGTEXT, false)
    add_integer(ENC_CFG_PREFIX "vbr", 0, VBR_QUALITY_TEXT,
              VBR_QUALITY_LONGTEXT, false)
    change_integer_range (0, 5)
    add_bool(ENC_CFG_PREFIX "afterburner", true, AFTERBURNER_TEXT,
              AFTERBURNER_LONGTEXT, true)
    add_integer(ENC_CFG_PREFIX "signaling", SIGNALING_COMPATIBLE, SIGNALING_TEXT,
             SIGNALING_LONGTEXT, true)
    change_integer_range (0, 2)
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *EncodeAudio(encoder_t *p_enc, block_t *p_buf);

static const char *const ppsz_enc_options[] = {
    "profile", "sbr", "vbr", "afterburner", "signaling", NULL
};

/*****************************************************************************
 * encoder_sys_t : aac encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    double d_compression_ratio;
    mtime_t i_pts_last;
    int i_encoderdelay; /* Samples delay introduced by the profile */
    int i_frame_size;
    int i_maxoutputsize; /* Maximum buffer size for encoded output */
    HANDLE_AACENCODER handle;
};

static const char *fdkaac_error(AACENC_ERROR erraac)
{
    switch (erraac) {
    case AACENC_OK: return "No error";
    case AACENC_INVALID_HANDLE: return "Invalid handle";
    case AACENC_MEMORY_ERROR: return "Memory allocation error";
    case AACENC_UNSUPPORTED_PARAMETER: return "Unsupported parameter";
    case AACENC_INVALID_CONFIG: return "Invalid config";
    case AACENC_INIT_ERROR: return "Initialization error";
    case AACENC_INIT_AAC_ERROR: return "AAC library initialization error";
    case AACENC_INIT_SBR_ERROR: return "SBR library initialization error";
    case AACENC_INIT_TP_ERROR: return "Transport library initialization error";
    case AACENC_INIT_META_ERROR: return "Metadata library initialization error";
    case AACENC_ENCODE_ERROR: return "Encoding error";
    case AACENC_ENCODE_EOF: return "End of file";
    default: return "Unknown error";
    }
}

/*****************************************************************************
 * OpenDecoder: open the encoder.
 *****************************************************************************/
static int OpenEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;

    config_ChainParse(p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg);

    int i_aot;
    switch (p_enc->fmt_out.i_codec) {
    case VLC_CODEC_MP4A:
        i_aot = var_InheritInteger(p_enc, ENC_CFG_PREFIX "profile");
        break;
    case VLC_FOURCC('l', 'a', 'a', 'c'):
        i_aot = PROFILE_AAC_LC;
        break;
    case VLC_FOURCC('h', 'a', 'a', 'c'):
        i_aot = PROFILE_AAC_HE;
        break;
    case VLC_FOURCC('s', 'a', 'a', 'c'):
        i_aot = PROFILE_AAC_HE_v2;
        break;
    default:
        return VLC_EGENERIC;
    }

    if (p_enc->fmt_in.audio.i_channels != 2)
        if (i_aot == PROFILE_AAC_HE_v2 || i_aot == PROFILE_AAC_ELD) {
            msg_Err(p_enc, "Selected profile %d can only be used with stereo", i_aot);
            return VLC_EGENERIC;
        }

    uint16_t channel_config;
    CHANNEL_MODE mode;
    switch (p_enc->fmt_in.audio.i_channels) {
    case 1: mode = MODE_1; channel_config = AOUT_CHAN_CENTER; break;
    case 2: mode = MODE_2; channel_config = AOUT_CHANS_STEREO; break;
    case 3: mode = MODE_1_2; channel_config = AOUT_CHANS_3_0; break;
    case 4: mode = MODE_1_2_1; channel_config = AOUT_CHANS_4_CENTER_REAR; break;
    case 5: mode = MODE_1_2_2; channel_config = AOUT_CHANS_5_0; break;
    case 6: mode = MODE_1_2_2_1; channel_config = AOUT_CHANS_5_1; break;
    case 8: mode = MODE_1_2_2_2_1; channel_config = AOUT_CHANS_7_1; break;
    default:
        msg_Err(p_enc, "we do not support > 8 input channels, this input has %i",
                        p_enc->fmt_in.audio.i_channels);
        return VLC_EGENERIC;
    }

    p_enc->fmt_in.audio.i_physical_channels = channel_config;

    msg_Info(p_enc, "Initializing AAC Encoder, %i channels", p_enc->fmt_in.audio.i_channels);

    /* Allocate the memory needed to store the encoder's structure */
    encoder_sys_t *p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t));
    if (unlikely(!p_sys))
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;
    p_enc->fmt_in.i_codec = VLC_CODEC_S16N;
    p_enc->fmt_out.i_cat = AUDIO_ES;
    p_enc->fmt_out.i_codec = VLC_CODEC_MP4A;

    p_sys->i_pts_last = 0;

    AACENC_ERROR erraac;
    erraac = aacEncOpen(&p_sys->handle, 0, p_enc->fmt_in.audio.i_channels);
    if (erraac != AACENC_OK) {
        msg_Err(p_enc, "Unable to open encoder: %s", fdkaac_error(erraac));
        free(p_sys);
        return VLC_EGENERIC;
    }

#define SET_PARAM(P, V) do { \
        AACENC_ERROR err = aacEncoder_SetParam(p_sys->handle, AACENC_ ## P, V); \
        if (err != AACENC_OK) { \
            msg_Err(p_enc, "Couldn't set " #P " to value %d: %s", V, fdkaac_error(err)); \
            goto error; \
        } \
    } while(0)

    SET_PARAM(AOT, i_aot);
    bool b_eld_sbr = var_InheritBool(p_enc, ENC_CFG_PREFIX "sbr");
    if (i_aot == PROFILE_AAC_ELD && b_eld_sbr)
        SET_PARAM(SBR_MODE, 1);
    SET_PARAM(SAMPLERATE, p_enc->fmt_out.audio.i_rate);
    SET_PARAM(CHANNELMODE, mode);
    SET_PARAM(CHANNELORDER, CH_ORDER_WG4);

    int i_vbr = var_InheritInteger(p_enc, ENC_CFG_PREFIX "vbr");
    if (i_vbr != 0) {
        if ((i_aot == PROFILE_AAC_HE || i_aot == PROFILE_AAC_HE_v2) && i_vbr > 3) {
            msg_Warn(p_enc, "Maximum VBR quality for this profile is 3, setting vbr=3");
            i_vbr = 3;
        }
        SET_PARAM(BITRATEMODE, i_vbr);
    } else {
        int i_bitrate = p_enc->fmt_out.i_bitrate;
        if (i_bitrate == 0) {
            i_bitrate = 96 * p_enc->fmt_in.audio.i_channels * p_enc->fmt_out.audio.i_rate / 44;
            if (i_aot == PROFILE_AAC_HE || i_aot == PROFILE_AAC_HE_v2 || b_eld_sbr)
                i_bitrate /= 2;
            p_enc->fmt_out.i_bitrate = i_bitrate;
            msg_Info(p_enc, "Setting optimal bitrate of %i", i_bitrate);
        }
        SET_PARAM(BITRATE, i_bitrate);
    }
    SET_PARAM(TRANSMUX, 0);
    SET_PARAM(SIGNALING_MODE, (int)var_InheritInteger(p_enc, ENC_CFG_PREFIX "signaling"));
    SET_PARAM(AFTERBURNER, !!var_InheritBool(p_enc, ENC_CFG_PREFIX "afterburner"));
#undef SET_PARAM

    erraac = aacEncEncode(p_sys->handle, NULL, NULL, NULL, NULL);
    if (erraac != AACENC_OK) {
        msg_Err(p_enc, "Unable to initialize the encoder: %s", fdkaac_error(erraac));
        goto error;
    }

    AACENC_InfoStruct info = { 0 };
    erraac = aacEncInfo(p_sys->handle, &info);
    if (erraac != AACENC_OK) {
        msg_Err(p_enc, "Unable to get the encoder info: %s", fdkaac_error(erraac));
        goto error;
    }

    /* The maximum packet size is 6144 bits aka 768 bytes per channel. */
    p_sys->i_maxoutputsize = 768*p_enc->fmt_in.audio.i_channels;
    p_enc->fmt_in.audio.i_bitspersample = 16;
    p_sys->i_frame_size = info.frameLength;
    p_sys->i_encoderdelay = info.encoderDelay;

    p_enc->fmt_out.i_extra = info.confSize;
    if (p_enc->fmt_out.i_extra) {
        p_enc->fmt_out.p_extra = malloc(p_enc->fmt_out.i_extra);
        if (p_enc->fmt_out.p_extra == NULL) {
            msg_Err(p_enc, "Unable to allocate fmt_out.p_extra");
            goto error;
        }
        memcpy(p_enc->fmt_out.p_extra, info.confBuf, p_enc->fmt_out.i_extra);
    }

    p_enc->pf_encode_audio = EncodeAudio;

#ifndef NDEBUG
    // TODO: Add more debug info to this config printout
    msg_Dbg(p_enc, "fmt_out.p_extra = %i", p_enc->fmt_out.i_extra);
#endif

    return VLC_SUCCESS;

error:
    CloseEncoder(p_this);
    return VLC_EGENERIC;
}

/****************************************************************************
 * EncodeAudio: the whole thing
 ****************************************************************************/
static block_t *EncodeAudio(encoder_t *p_enc, block_t *p_aout_buf)
{
    int16_t *p_buffer;
    int i_samples;
    mtime_t i_pts_out;

    encoder_sys_t *p_sys = p_enc->p_sys;

    if (likely(p_aout_buf)) {
        p_buffer = (int16_t *)p_aout_buf->p_buffer;
        i_samples = p_aout_buf->i_nb_samples;
        i_pts_out = p_aout_buf->i_pts - (mtime_t)((double)CLOCK_FREQ *
               (double)p_sys->i_encoderdelay /
               (double)p_enc->fmt_out.audio.i_rate);
        if (p_sys->i_pts_last == 0)
            p_sys->i_pts_last = i_pts_out - (mtime_t)((double)CLOCK_FREQ *
               (double)(p_sys->i_frame_size) /
               (double)p_enc->fmt_out.audio.i_rate);
    } else {
        i_samples = 0;
        i_pts_out = p_sys->i_pts_last;
    }

    int i_samples_left = i_samples;
    int i_loop_count = 0;

    block_t *p_chain = NULL;
    while (i_samples_left >= 0) {
        AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
        AACENC_InArgs in_args = { 0 };
        AACENC_OutArgs out_args = { 0 };
        int in_identifier = IN_AUDIO_DATA;
        int in_size, in_elem_size;
        int out_identifier = OUT_BITSTREAM_DATA;
        int out_size, out_elem_size;
        void *in_ptr, *out_ptr;

        if (unlikely(i_samples == 0)) {
            // this forces the encoder to purge whatever is left in the internal buffer
            in_args.numInSamples = -1;
        } else {
            in_ptr = p_buffer + (i_samples - i_samples_left)*p_enc->fmt_in.audio.i_channels;
            in_size = 2*p_enc->fmt_in.audio.i_channels*i_samples_left;
            in_elem_size = 2;
            in_args.numInSamples = p_enc->fmt_in.audio.i_channels*i_samples_left;
            in_buf.numBufs = 1;
            in_buf.bufs = &in_ptr;
            in_buf.bufferIdentifiers = &in_identifier;
            in_buf.bufSizes = &in_size;
            in_buf.bufElSizes = &in_elem_size;
        }
        block_t *p_block;
        p_block = block_Alloc(p_sys->i_maxoutputsize);
        p_block->i_buffer = p_sys->i_maxoutputsize;
        out_ptr = p_block->p_buffer;
        out_size = p_block->i_buffer;
        out_elem_size = 1;
        out_buf.numBufs = 1;
        out_buf.bufs = &out_ptr;
        out_buf.bufferIdentifiers = &out_identifier;
        out_buf.bufSizes = &out_size;
        out_buf.bufElSizes = &out_elem_size;

        AACENC_ERROR erraac;
        if ((erraac = aacEncEncode(p_sys->handle, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
            if (erraac == AACENC_ENCODE_EOF) {
                msg_Info(p_enc, "Encoding final bytes (EOF)");
            } else {
                msg_Err(p_enc, "Encoding failed: %s", fdkaac_error(erraac));
                block_Release(p_block);
                break;
            }
        }
        if (out_args.numOutBytes > 0) {
            p_block->i_buffer = out_args.numOutBytes;
            if (unlikely(i_samples == 0)) {
                // I only have the numOutBytes so approximate based on compression factor
                double d_samples_forward = p_sys->d_compression_ratio*(double)out_args.numOutBytes;
                i_pts_out += (mtime_t)d_samples_forward;
                p_block->i_length = (mtime_t)d_samples_forward;
                // TODO: It would be more precise (a few microseconds) to use d_samples_forward =
                // (mtime_t)CLOCK_FREQ * (mtime_t)p_sys->i_frame_size/(mtime_t)p_enc->fmt_out.audio.i_rate
                // but I am not sure if the lib always outputs a full frame when
                // emptying the internal buffer in the EOF scenario
            } else {
                if (i_loop_count == 0) {
                    // There can be an implicit delay in the first loop cycle because leftover bytes
                    // in the library buffer from the prior block
                    double d_samples_delay = (double)p_sys->i_frame_size - (double)out_args.numInSamples /
                                             (double)p_enc->fmt_in.audio.i_channels;
                    i_pts_out -= (mtime_t)((double)CLOCK_FREQ * d_samples_delay /
                                           (double)p_enc->fmt_out.audio.i_rate);
                    p_block->i_length = (mtime_t)((double)CLOCK_FREQ * (double)p_sys->i_frame_size /
                        (double)p_enc->fmt_out.audio.i_rate);
                    p_block->i_nb_samples = d_samples_delay;
                    //p_block->i_length = i_pts_out - p_sys->i_pts_last;
                } else {
                    double d_samples_forward = (double)out_args.numInSamples/(double)p_enc->fmt_in.audio.i_channels;
                    double d_length = ((double)CLOCK_FREQ * d_samples_forward /
                                            (double)p_enc->fmt_out.audio.i_rate);
                    i_pts_out += (mtime_t) d_length;
                    p_block->i_length = (mtime_t) d_length;
                    p_block->i_nb_samples = d_samples_forward;
                }
            }
            p_block->i_dts = p_block->i_pts = i_pts_out;
            block_ChainAppend(&p_chain, p_block);
#if 0
            msg_Dbg(p_enc, "dts %"PRId64", length %"PRId64", " "pts_last "
                            "%"PRId64" numOutBytes = %i, numInSamples = %i, "
                            "i_samples %i, i_loop_count %i",
                              p_block->i_dts, p_block->i_length,
                              p_sys->i_pts_last, out_args.numOutBytes,
                              out_args.numInSamples, i_samples, i_loop_count);
#endif
            if (likely(i_samples > 0)) {
                p_sys->d_compression_ratio = (double)p_block->i_length / (double)out_args.numOutBytes;
                i_samples_left -= out_args.numInSamples/p_enc->fmt_in.audio.i_channels;
                p_sys->i_pts_last = i_pts_out;
            }
        } else {
            block_Release(p_block);
            //msg_Dbg(p_enc, "aac_encode_audio: not enough data yet");
            break;
        }
        if (unlikely(i_loop_count++ > 100)) {
            msg_Err(p_enc, "Loop count greater than 100!!!, something must be wrong with the encoder library");
            break;
        }
    }

    return p_chain;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    aacEncClose(&p_sys->handle);
    free(p_sys);
}
