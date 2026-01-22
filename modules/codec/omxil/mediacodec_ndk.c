/*****************************************************************************
 * mediacodec_ndk.c: mc_api implementation using NDK
 *****************************************************************************
 * Copyright © 2015 VLC authors and VideoLAN, VideoLabs
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

#include <jni.h>
#include <dlfcn.h>
#include <stdint.h>
#include <assert.h>

#include <vlc_common.h>

#include <media/NdkMediaCodec.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include "omxil_utils.h"

#include "mediacodec.h"

static_assert(MC_API_NO_QUIRKS == OMXCODEC_NO_QUIRKS
    && MC_API_QUIRKS_NEED_CSD == OMXCODEC_QUIRKS_NEED_CSD
    && MC_API_VIDEO_QUIRKS_IGNORE_PADDING == OMXCODEC_VIDEO_QUIRKS_IGNORE_PADDING
    && MC_API_VIDEO_QUIRKS_SUPPORT_INTERLACED == OMXCODEC_VIDEO_QUIRKS_SUPPORT_INTERLACED
    && MC_API_AUDIO_QUIRKS_NEED_CHANNELS == OMXCODEC_AUDIO_QUIRKS_NEED_CHANNELS,
    "mediacodec.h/omx_utils.h mismatch");

char* MediaCodec_GetName(vlc_object_t *p_obj, vlc_fourcc_t codec,
                         const char *psz_mime, int profile, int *p_quirks);

#define THREAD_NAME "mediacodec_ndk"

/* Not in NdkMedia API but we need it since we send config data via input
 * buffers and not via "csd-*" buffers from AMediaFormat */
#define AMEDIACODEC_FLAG_CODEC_CONFIG 2

/****************************************************************************
 * Local prototypes
 ****************************************************************************/

struct mc_api_sys
{
    AMediaCodec* p_codec;
    AMediaFormat* p_format;
    AMediaCodecBufferInfo info;
};

/*****************************************************************************
 * ConfigureDecoder
 *****************************************************************************/
static int ConfigureDecoder(mc_api *api, union mc_api_args *p_args)
{
    mc_api_sys *p_sys = api->p_sys;
    ANativeWindow *p_anw = NULL;

    assert(api->psz_mime && api->psz_name);

    p_sys->p_codec = AMediaCodec_createCodecByName(api->psz_name);
    if (!p_sys->p_codec)
    {
        msg_Err(api->p_obj, "AMediaCodec.createCodecByName for %s failed",
                api->psz_name);
        return MC_API_ERROR;
    }

    p_sys->p_format = AMediaFormat_new();
    if (!p_sys->p_format)
    {
        msg_Err(api->p_obj, "AMediaFormat.new failed");
        return MC_API_ERROR;
    }

    if (p_args->video.b_low_latency)
        AMediaFormat_setInt32(p_sys->p_format, "low-latency", 1);
    AMediaFormat_setInt32(p_sys->p_format, "encoder", 0);
    AMediaFormat_setString(p_sys->p_format, "mime", api->psz_mime);
    /* No limits for input size */
    AMediaFormat_setInt32(p_sys->p_format, "max-input-size", 0);
    if (api->i_cat == VIDEO_ES)
    {
        AMediaFormat_setInt32(p_sys->p_format, "width", p_args->video.i_width);
        AMediaFormat_setInt32(p_sys->p_format, "height", p_args->video.i_height);
        AMediaFormat_setInt32(p_sys->p_format, "rotation-degrees", p_args->video.i_angle);

        AMediaFormat_setInt32(p_sys->p_format, "color-range", p_args->video.color.range);
        AMediaFormat_setInt32(p_sys->p_format, "color-standard", p_args->video.color.standard);
        AMediaFormat_setInt32(p_sys->p_format, "color-transfer", p_args->video.color.transfer);

        if (p_args->video.p_surface)
        {
            p_anw = p_args->video.p_surface;
            if (p_args->video.b_tunneled_playback)
                AMediaFormat_setInt32(p_sys->p_format,
                                           "feature-tunneled-playback", 1);
            if (p_args->video.b_adaptive_playback)
                AMediaFormat_setInt32(p_sys->p_format,
                                           "feature-adaptive-playback", 1);
        }
    }
    else
    {
        AMediaFormat_setInt32(p_sys->p_format, "sample-rate", p_args->audio.i_sample_rate);
        AMediaFormat_setInt32(p_sys->p_format, "channel-count", p_args->audio.i_channel_count);
    }

    if (AMediaCodec_configure(p_sys->p_codec, p_sys->p_format,
                                   p_anw, NULL, 0) != AMEDIA_OK)
    {
        msg_Err(api->p_obj, "AMediaCodec.configure failed");
        return MC_API_ERROR;
    }

    api->b_direct_rendering = !!p_anw;

    return 0;
}

/*****************************************************************************
 * Stop
 *****************************************************************************/
static int Stop(mc_api *api)
{
    mc_api_sys *p_sys = api->p_sys;

    api->b_direct_rendering = false;

    if (p_sys->p_codec)
    {
        if (api->b_started)
        {
            AMediaCodec_stop(p_sys->p_codec);
            api->b_started = false;
        }
        AMediaCodec_delete(p_sys->p_codec);
        p_sys->p_codec = NULL;
    }
    if (p_sys->p_format)
    {
        AMediaFormat_delete(p_sys->p_format);
        p_sys->p_format = NULL;
    }

    msg_Dbg(api->p_obj, "MediaCodec via NDK closed");
    return 0;
}

/*****************************************************************************
 * Start
 *****************************************************************************/
static int Start(mc_api *api)
{
    mc_api_sys *p_sys = api->p_sys;
    int i_ret = MC_API_ERROR;

    if (AMediaCodec_start(p_sys->p_codec) != AMEDIA_OK)
    {
        msg_Err(api->p_obj, "AMediaCodec.start failed");
        goto error;
    }

    api->b_started = true;
    i_ret = 0;

    msg_Dbg(api->p_obj, "MediaCodec via NDK opened");
error:
    if (i_ret != 0)
        Stop(api);
    return i_ret;
}

/*****************************************************************************
 * Flush
 *****************************************************************************/
static int Flush(mc_api *api)
{
    mc_api_sys *p_sys = api->p_sys;

    if (AMediaCodec_flush(p_sys->p_codec) == AMEDIA_OK)
        return 0;
    else
        return MC_API_ERROR;
}

/*****************************************************************************
 * DequeueInput
 *****************************************************************************/
static int DequeueInput(mc_api *api, vlc_tick_t i_timeout)
{
    mc_api_sys *p_sys = api->p_sys;
    ssize_t i_index;

    i_index = AMediaCodec_dequeueInputBuffer(p_sys->p_codec, i_timeout);
    if (i_index >= 0)
        return i_index;
    else if (i_index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
        return MC_API_INFO_TRYAGAIN;
    else
    {
        msg_Err(api->p_obj, "AMediaCodec.dequeueInputBuffer failed");
        return MC_API_ERROR;
    }
}

/*****************************************************************************
 * QueueInput
 *****************************************************************************/
static int QueueInput(mc_api *api, int i_index, const void *p_buf,
                      size_t i_size, vlc_tick_t i_ts, bool b_config)
{
    mc_api_sys *p_sys = api->p_sys;
    uint8_t *p_mc_buf;
    size_t i_mc_size;
    int i_flags = (b_config ? AMEDIACODEC_FLAG_CODEC_CONFIG : 0)
                | (p_buf == NULL ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0);

    assert(i_index >= 0);

    p_mc_buf = AMediaCodec_getInputBuffer(p_sys->p_codec,
                                               i_index, &i_mc_size);
    if (!p_mc_buf)
        return MC_API_ERROR;

    if (i_mc_size > i_size)
        i_mc_size = i_size;
    memcpy(p_mc_buf, p_buf, i_mc_size);

    if (AMediaCodec_queueInputBuffer(p_sys->p_codec, i_index, 0, i_mc_size,
                                          i_ts, i_flags) == AMEDIA_OK)
        return 0;
    else
    {
        msg_Err(api->p_obj, "AMediaCodec.queueInputBuffer failed");
        return MC_API_ERROR;
    }
}

static int32_t GetFormatInteger(AMediaFormat *p_format, const char *psz_name)
{
    int32_t i_out = 0;
    AMediaFormat_getInt32(p_format, psz_name, &i_out);
    return i_out;
}

/*****************************************************************************
 * DequeueOutput
 *****************************************************************************/
static int DequeueOutput(mc_api *api, vlc_tick_t i_timeout)
{
    mc_api_sys *p_sys = api->p_sys;
    ssize_t i_index;

    i_index = AMediaCodec_dequeueOutputBuffer(p_sys->p_codec, &p_sys->info,
                                                   i_timeout);

    if (i_index >= 0)
        return i_index;
    else if (i_index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
        return MC_API_INFO_TRYAGAIN;
    else if (i_index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
        return MC_API_INFO_OUTPUT_BUFFERS_CHANGED;
    else if (i_index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
        return MC_API_INFO_OUTPUT_FORMAT_CHANGED;
    else
        return MC_API_ERROR;
}

/*****************************************************************************
 * GetOutput
 *****************************************************************************/
static int GetOutput(mc_api *api, int i_index, mc_api_out *p_out)
{
    mc_api_sys *p_sys = api->p_sys;

    if (i_index >= 0)
    {
        p_out->type = MC_OUT_TYPE_BUF;
        p_out->buf.i_index = i_index;

        p_out->buf.i_ts = p_sys->info.presentationTimeUs;
        p_out->b_eos = p_sys->info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;

        if (api->b_direct_rendering)
        {
            p_out->buf.p_ptr = NULL;
            p_out->buf.i_size = 0;
        }
        else
        {
            size_t i_mc_size;
            uint8_t *p_mc_buf = AMediaCodec_getOutputBuffer(p_sys->p_codec,
                                                                 i_index,
                                                                 &i_mc_size);
            /* p_mc_buf can be NULL in case of EOS */
            if (!p_mc_buf && !p_out->b_eos)
            {
                msg_Err(api->p_obj, "AMediaCodec.getOutputBuffer failed");
                return MC_API_ERROR;
            }
            p_out->buf.p_ptr = p_mc_buf + p_sys->info.offset;
            p_out->buf.i_size = p_sys->info.size;
        }
        return 1;
    }
    else if (i_index == MC_API_INFO_OUTPUT_FORMAT_CHANGED)
    {
        AMediaFormat *format = AMediaCodec_getOutputFormat(p_sys->p_codec);
        if (unlikely(format == NULL))
            return MC_API_ERROR;

        p_out->type = MC_OUT_TYPE_CONF;
        p_out->b_eos = false;
        if (api->i_cat == VIDEO_ES)
        {
            p_out->conf.video.width         = GetFormatInteger(format, "width");
            p_out->conf.video.height        = GetFormatInteger(format, "height");
            p_out->conf.video.stride        = GetFormatInteger(format, "stride");
            p_out->conf.video.slice_height  = GetFormatInteger(format, "slice-height");
            p_out->conf.video.pixel_format  = GetFormatInteger(format, "color-format");
            p_out->conf.video.crop_left     = GetFormatInteger(format, "crop-left");
            p_out->conf.video.crop_top      = GetFormatInteger(format, "crop-top");
            p_out->conf.video.crop_right    = GetFormatInteger(format, "crop-right");
            p_out->conf.video.crop_bottom   = GetFormatInteger(format, "crop-bottom");

            /* Extract color info from output format (API 28+) */
            p_out->conf.video.color.range = GetFormatInteger(format, "color-range");
            p_out->conf.video.color.standard = GetFormatInteger(format, "color-standard");
            p_out->conf.video.color.transfer = GetFormatInteger(format, "color-transfer");
        }
        else
        {
            p_out->conf.audio.channel_count = GetFormatInteger(format, "channel-count");
            p_out->conf.audio.channel_mask  = GetFormatInteger(format, "channel-mask");
            p_out->conf.audio.sample_rate   = GetFormatInteger(format, "sample-rate");
        }
        AMediaFormat_delete(format);
        return 1;
    }
    return 0;
}

/*****************************************************************************
 * ReleaseOutput
 *****************************************************************************/
static int ReleaseOutput(mc_api *api, int i_index, bool b_render)
{
    mc_api_sys *p_sys = api->p_sys;

    assert(i_index >= 0);
    if (AMediaCodec_releaseOutputBuffer(p_sys->p_codec, i_index, b_render)
                                             == AMEDIA_OK)
        return 0;
    else
        return MC_API_ERROR;
}

/*****************************************************************************
 * ReleaseOutputAtTime
 *****************************************************************************/
static int ReleaseOutputAtTime(mc_api *api, int i_index, int64_t i_ts_ns)
{
    mc_api_sys *p_sys = api->p_sys;

    assert(i_index >= 0);
    if (AMediaCodec_releaseOutputBufferAtTime(p_sys->p_codec, i_index, i_ts_ns)
                                                   == AMEDIA_OK)
        return 0;
    else
        return MC_API_ERROR;
}

/*****************************************************************************
 * Clean
 *****************************************************************************/
static void Clean(mc_api *api)
{
    free(api->psz_name);
    free(api->p_sys);
}

/*****************************************************************************
 * Prepare
 *****************************************************************************/
static int Prepare(mc_api * api, int i_profile)
{
    free(api->psz_name);

    api->i_quirks = 0;
    api->psz_name = MediaCodec_GetName(api->p_obj, api->i_codec, api->psz_mime,
                                       i_profile, &api->i_quirks);
    if (!api->psz_name)
        return MC_API_ERROR;
    api->i_quirks |= OMXCodec_GetQuirks(api->i_cat, api->i_codec, api->psz_name,
                                        strlen(api->psz_name));
    /* Allow interlaced picture after API 21 */
    api->i_quirks |= MC_API_VIDEO_QUIRKS_SUPPORT_INTERLACED;
    return 0;
}

/*****************************************************************************
 * MediaCodecNdk_Init
 *****************************************************************************/
int MediaCodecNdk_Init(mc_api *api)
{
    api->p_sys = calloc(1, sizeof(mc_api_sys));
    if (!api->p_sys)
        return MC_API_ERROR;

    api->clean = Clean;
    api->prepare = Prepare;
    api->configure_decoder = ConfigureDecoder;
    api->start = Start;
    api->stop = Stop;
    api->flush = Flush;
    api->dequeue_in = DequeueInput;
    api->queue_in = QueueInput;
    api->dequeue_out = DequeueOutput;
    api->get_out = GetOutput;
    api->release_out = ReleaseOutput;
    api->release_out_ts = ReleaseOutputAtTime;

    api->b_support_rotation = true;
    return 0;
}
