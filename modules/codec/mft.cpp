/*****************************************************************************
 * mft.cpp : Media Foundation Transform audio/video decoder
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Author: Felix Abecassis <felix.abecassis@gmail.com>
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

#include <winapifamily.h>
#undef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
extern "C" {
#include "hxxx_helper.h"
}

#include <initguid.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mferror.h>
#include <mfobjects.h>


#define _VIDEOINFOHEADER_
#include <vlc_codecs.h>

#include <new>

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_description(N_("Media Foundation Transform decoder"))
    add_shortcut("mft")
    set_capability("video decoder", 1)
    set_callbacks(Open, Close)
    set_subcategory(SUBCAT_INPUT_VCODEC)

    add_submodule()
    add_shortcut("mft")
    set_capability("audio decoder", 1)
    set_callbacks(Open, Close)
vlc_module_end()

typedef struct
{
    IMFTransform *mft;

    const GUID* major_type;
    const GUID* subtype;
    /* Container for a dynamically constructed subtype */
    GUID custom_subtype;

    /* For asynchronous MFT */
    bool is_async;
    IMFMediaEventGenerator *event_generator;
    int pending_input_events;
    int pending_output_events;

    /* Input stream */
    DWORD input_stream_id;
    IMFMediaType *input_type;

    /* Output stream */
    DWORD output_stream_id;
    IMFSample *output_sample;

    /* H264 only. */
    struct hxxx_helper hh;
    bool   b_xps_pushed; ///< (for xvcC) parameter sets pushed (SPS/PPS/VPS)
} decoder_sys_t;

static const int pi_channels_maps[9] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARCENTER | AOUT_CHAN_MIDDLELEFT
     | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
     | AOUT_CHAN_LFE,
};

/* Possibly missing from mingw headers */
#ifndef MF_E_NO_EVENTS_AVAILABLE
# define MF_E_NO_EVENTS_AVAILABLE _HRESULT_TYPEDEF_(0xC00D3E80L)
#endif

/*
 * The MFTransformXXX values might not be defined in mingw headers,
 * thus we use our own enum with the VLC prefix.
 */
enum
{
    VLC_METransformUnknown = 600,
    VLC_METransformNeedInput,
    VLC_METransformHaveOutput,
    VLC_METransformDrainComplete,
    VLC_METransformMarker,
};

typedef struct
{
    vlc_fourcc_t fourcc;
    const GUID   *guid;
} pair_format_guid;

/*
 * We need this table since the FOURCC used for GUID is not the same
 * as the FOURCC used by VLC, for instance h264 vs H264.
 */
static const pair_format_guid video_format_table[] =
{
    { VLC_CODEC_H264, &MFVideoFormat_H264 },
    { VLC_CODEC_MPGV, &MFVideoFormat_MPEG2 },
    { VLC_CODEC_MP2V, &MFVideoFormat_MPEG2 },
    { VLC_CODEC_MP1V, &MFVideoFormat_MPG1 },
    { VLC_CODEC_MJPG, &MFVideoFormat_MJPG },
    { VLC_CODEC_WMV1, &MFVideoFormat_WMV1 },
    { VLC_CODEC_WMV2, &MFVideoFormat_WMV2 },
    { VLC_CODEC_WMV3, &MFVideoFormat_WMV3 },
    { VLC_CODEC_VC1,  &MFVideoFormat_WVC1 },
    { 0, NULL }
};

// 8-bit luminance only

// Older versions of mingw-w64 lack this GUID, but it was added in mingw-w64
// git on 2021-07-11 (during __MINGW64_VERSION_MAJOR 10). Use a local
// redefinition of this GUID with a custom prefix, to let the same code build
// with both older and newer versions of mingw-w64 (and earlier git snapshots
// with __MINGW64_VERSION_MAJOR == 10).
DEFINE_MEDIATYPE_GUID (vlc_MFVideoFormat_L8, 50);

/*
 * Table to map MF Transform raw 3D3 output formats to native VLC FourCC
 */
static const pair_format_guid d3d_format_table[] = {
    { VLC_CODEC_RGB32, &MFVideoFormat_RGB32  },
    { VLC_CODEC_RGB24, &MFVideoFormat_RGB24  },
    { VLC_CODEC_RGBA,  &MFVideoFormat_ARGB32 },
    { VLC_CODEC_GREY,  &vlc_MFVideoFormat_L8 },
    { 0, NULL }
};

/*
 * We cannot use the FOURCC code for audio either since the
 * WAVE_FORMAT value is used to create the GUID.
 */
static const pair_format_guid audio_format_table[] =
{
    { VLC_CODEC_MPGA, &MFAudioFormat_MPEG      },
    { VLC_CODEC_MP3,  &MFAudioFormat_MP3       },
    { VLC_CODEC_DTS,  &MFAudioFormat_DTS       },
    { VLC_CODEC_MP4A, &MFAudioFormat_AAC       },
    { VLC_CODEC_WMA2, &MFAudioFormat_WMAudioV8 },
    { VLC_CODEC_A52,  &MFAudioFormat_Dolby_AC3 },
    { 0, NULL }
};

static const GUID *FormatToGUID(const pair_format_guid table[], vlc_fourcc_t fourcc)
{
    for (int i = 0; table[i].fourcc; ++i)
        if (table[i].fourcc == fourcc)
            return table[i].guid;

    return NULL;
}

static vlc_fourcc_t GUIDToFormat(const pair_format_guid table[], const GUID & guid)
{
    for (int i = 0; table[i].fourcc; ++i)
        if (IsEqualGUID(*table[i].guid, guid))
            return table[i].fourcc;

    return 0;
}

/*
 * Low latency mode for Windows 8. Without this option, the H264
 * decoder will fill *all* its internal buffers before returning a
 * frame. Because of this behavior, the decoder might return no frame
 * for more than 500 ms, making it unusable for playback.
 */
DEFINE_GUID(CODECAPI_AVLowLatencyMode, 0x9c27891a, 0xed7a, 0x40e1, 0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee);

static int SetInputType(decoder_t *p_dec, DWORD stream_id, IMFMediaType **result)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    *result = NULL;

    IMFMediaType *input_media_type = NULL;

    /* Search a suitable input type for the MFT. */
    int input_type_index = 0;
    bool found = false;
    for (int i = 0; !found; ++i)
    {
        hr = p_sys->mft->GetInputAvailableType(stream_id, i, &input_media_type);
        if (hr == MF_E_NO_MORE_TYPES)
            break;
        else if (hr == MF_E_TRANSFORM_TYPE_NOT_SET)
        {
            /* The output type must be set before setting the input type for this MFT. */
            return VLC_SUCCESS;
        }
        else if (FAILED(hr))
            goto error;

        GUID subtype;
        hr = input_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (FAILED(hr))
            goto error;

        if (IsEqualGUID(subtype, *p_sys->subtype))
            found = true;

        if (found)
            input_type_index = i;

        input_media_type->Release();
        input_media_type = NULL;
    }
    if (!found)
        goto error;

    hr = p_sys->mft->GetInputAvailableType(stream_id, input_type_index, &input_media_type);
    if (FAILED(hr))
        goto error;

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        UINT64 width = p_dec->fmt_in.video.i_width;
        UINT64 height = p_dec->fmt_in.video.i_height;
        UINT64 frame_size = (width << 32) | height;
        hr = input_media_type->SetUINT64(MF_MT_FRAME_SIZE, frame_size);
        if (FAILED(hr))
            goto error;

        /* Some transforms like to know the frame rate and may reject the input type otherwise. */
        UINT64 frame_ratio_num = p_dec->fmt_in.video.i_frame_rate;
        UINT64 frame_ratio_dem = p_dec->fmt_in.video.i_frame_rate_base;
        if(frame_ratio_num && frame_ratio_dem) {
            UINT64 frame_rate = (frame_ratio_num << 32) | frame_ratio_dem;
            hr = input_media_type->SetUINT64(MF_MT_FRAME_RATE, frame_rate);
            if(FAILED(hr))
                goto error;
        }
    }
    else
    {
        hr = input_media_type->SetUINT32(MF_MT_ORIGINAL_WAVE_FORMAT_TAG, p_sys->subtype->Data1);
        if (FAILED(hr))
            goto error;
        if (p_dec->fmt_in.audio.i_rate)
        {
            hr = input_media_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, p_dec->fmt_in.audio.i_rate);
            if (FAILED(hr))
                goto error;
        }
        if (p_dec->fmt_in.audio.i_channels)
        {
            hr = input_media_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, p_dec->fmt_in.audio.i_channels);
            if (FAILED(hr))
                goto error;
        }
        if (p_dec->fmt_in.audio.i_bitspersample)
        {
            hr = input_media_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, p_dec->fmt_in.audio.i_bitspersample);
            if (FAILED(hr))
                goto error;
        }
        if (p_dec->fmt_in.audio.i_blockalign)
        {
            hr = input_media_type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, p_dec->fmt_in.audio.i_blockalign);
            if (FAILED(hr))
                goto error;
        }
        if (p_dec->fmt_in.i_bitrate)
        {
            hr = input_media_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, p_dec->fmt_in.i_bitrate / 8);
            if (FAILED(hr))
                goto error;
        }
    }

    if (p_dec->fmt_in.i_extra > 0)
    {
        UINT32 blob_size = 0;
        hr = input_media_type->GetBlobSize(MF_MT_USER_DATA, &blob_size);
        /*
         * Do not overwrite existing user data in the input type, this
         * can cause the MFT to reject the type.
         */
        if (hr == MF_E_ATTRIBUTENOTFOUND)
        {
            hr = input_media_type->SetBlob(MF_MT_USER_DATA,
                                      static_cast<const UINT8*>(p_dec->fmt_in.p_extra), p_dec->fmt_in.i_extra);
            if (FAILED(hr))
                goto error;
        }
    }

    hr = p_sys->mft->SetInputType(stream_id, input_media_type, 0);
    if (FAILED(hr))
        goto error;

    *result = input_media_type;

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in SetInputType()");
    if (input_media_type)
        input_media_type->Release();
    return VLC_EGENERIC;
}

static int SetOutputType(decoder_t *p_dec, DWORD stream_id)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    IMFMediaType *output_media_type = NULL;

    /*
     * Enumerate available output types. The list is ordered by
     * preference thus we will use the first one unless YV12/I420 is
     * available for video or float32 for audio.
     */
    int output_type_index = -1;
    bool found = false;
    for (int i = 0; !found; ++i)
    {
        hr = p_sys->mft->GetOutputAvailableType(stream_id, i, &output_media_type);
        if (hr == MF_E_NO_MORE_TYPES)
            break;
        else if (hr == MF_E_TRANSFORM_TYPE_NOT_SET)
        {
            /* The input type must be set before setting the output type for this MFT. */
            return VLC_SUCCESS;
        }
        else if (FAILED(hr))
            goto error;

        GUID subtype;
        hr = output_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (FAILED(hr))
            goto error;

        if (p_dec->fmt_in.i_cat == VIDEO_ES)
        {
            if (IsEqualGUID(subtype, MFVideoFormat_NV12) || IsEqualGUID(subtype, MFVideoFormat_YV12) || IsEqualGUID(subtype, MFVideoFormat_I420))
                found = true;
            /* Transform might offer output in a D3DFMT proprietary FCC. If we can
             * use it, fall back to it in case we do not find YV12 or I420 */
            else if(output_type_index < 0 && GUIDToFormat(d3d_format_table, subtype) > 0)
                    output_type_index = i;
        }
        else
        {
            UINT32 bits_per_sample;
            hr = output_media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits_per_sample);
            if (FAILED(hr))
                continue;
            if (bits_per_sample == 32 && IsEqualGUID(subtype, MFAudioFormat_Float))
                found = true;
        }

        if (found)
            output_type_index = i;

        output_media_type->Release();
        output_media_type = NULL;
    }
    /*
     * It's not an error if we don't find the output type we were
     * looking for, in this case we use the first available type.
     */
    if(output_type_index < 0)
        /* No output format found we prefer, just pick the first one preferred
         * by the MFT */
        output_type_index = 0;

    hr = p_sys->mft->GetOutputAvailableType(stream_id, output_type_index, &output_media_type);
    if (FAILED(hr))
        goto error;

    hr = p_sys->mft->SetOutputType(stream_id, output_media_type, 0);
    if (FAILED(hr))
        goto error;

    GUID subtype;
    hr = output_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr))
        goto error;

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        video_format_Copy( &p_dec->fmt_out.video, &p_dec->fmt_in.video );

        /* Transform might offer output in a D3DFMT proprietary FCC */
        vlc_fourcc_t fcc = GUIDToFormat(d3d_format_table, subtype);
        if(fcc) {
            /* D3D formats are upside down */
            p_dec->fmt_out.video.orientation = ORIENT_BOTTOM_LEFT;
        } else {
            fcc = vlc_fourcc_GetCodec(p_dec->fmt_in.i_cat, subtype.Data1);
        }

        p_dec->fmt_out.i_codec = fcc;
    }
    else
    {
        p_dec->fmt_out.audio = p_dec->fmt_in.audio;

        UINT32 bitspersample = 0;
        hr = output_media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitspersample);
        if (SUCCEEDED(hr) && bitspersample)
            p_dec->fmt_out.audio.i_bitspersample = bitspersample;

        UINT32 channels = 0;
        hr = output_media_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        if (SUCCEEDED(hr) && channels)
            p_dec->fmt_out.audio.i_channels = channels;

        UINT32 rate = 0;
        hr = output_media_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        if (SUCCEEDED(hr) && rate)
            p_dec->fmt_out.audio.i_rate = rate;

        vlc_fourcc_t fourcc;
        wf_tag_to_fourcc(subtype.Data1, &fourcc, NULL);
        p_dec->fmt_out.i_codec = vlc_fourcc_GetCodecAudio(fourcc, p_dec->fmt_out.audio.i_bitspersample);

        p_dec->fmt_out.audio.i_physical_channels = pi_channels_maps[p_dec->fmt_out.audio.i_channels];
    }

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in SetOutputType()");
    if (output_media_type)
        output_media_type->Release();
    return VLC_EGENERIC;
}

static int AllocateInputSample(decoder_t *p_dec, DWORD stream_id, IMFSample** result, DWORD size)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    *result = NULL;

    IMFSample *input_sample = NULL;
    IMFMediaBuffer *input_media_buffer = NULL;
    DWORD allocation_size;

    MFT_INPUT_STREAM_INFO input_info;
    hr = p_sys->mft->GetInputStreamInfo(stream_id, &input_info);
    if (FAILED(hr))
        goto error;

    hr = MFCreateSample(&input_sample);
    if (FAILED(hr))
        goto error;

    allocation_size = __MAX(input_info.cbSize, size);
    hr = MFCreateMemoryBuffer(allocation_size, &input_media_buffer);
    if (FAILED(hr))
        goto error;

    hr = input_sample->AddBuffer(input_media_buffer);
    input_media_buffer->Release();
    if (FAILED(hr))
        goto error;

    *result = input_sample;

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in AllocateInputSample()");
    if (input_sample)
        input_sample->Release();
    if (input_media_buffer)
        input_media_buffer->Release();
    return VLC_EGENERIC;
}

static int AllocateOutputSample(decoder_t *p_dec, DWORD stream_id, IMFSample **result)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    *result = NULL;

    IMFSample *output_sample = NULL;

    MFT_OUTPUT_STREAM_INFO output_info;
    IMFMediaBuffer *output_media_buffer = NULL;
    DWORD allocation_size;
    DWORD alignment;

    hr = p_sys->mft->GetOutputStreamInfo(stream_id, &output_info);
    if (FAILED(hr))
        goto error;

    if (output_info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES))
    {
        /* The MFT will provide an allocated sample. */
        return VLC_SUCCESS;
    }

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        const DWORD expected_flags =
                          MFT_OUTPUT_STREAM_WHOLE_SAMPLES
                        | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER
                        | MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;
        if ((output_info.dwFlags & expected_flags) != expected_flags)
            goto error;
    }

    hr = MFCreateSample(&output_sample);
    if (FAILED(hr))
        goto error;

    allocation_size = output_info.cbSize;
    alignment = output_info.cbAlignment;
    if (alignment > 0)
        hr = MFCreateAlignedMemoryBuffer(allocation_size, alignment - 1, &output_media_buffer);
    else
        hr = MFCreateMemoryBuffer(allocation_size, &output_media_buffer);
    if (FAILED(hr))
        goto error;

    hr = output_sample->AddBuffer(output_media_buffer);
    if (FAILED(hr))
        goto error;

    *result = output_sample;

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in AllocateOutputSample()");
    if (output_sample)
        output_sample->Release();
    return VLC_EGENERIC;
}

static int ProcessInputStream(decoder_t *p_dec, DWORD stream_id, block_t *p_block)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr = S_OK;
    IMFSample *input_sample = NULL;

    block_t *p_xps_blocks = NULL;
    DWORD alloc_size = p_block->i_buffer;
    vlc_tick_t ts;
    IMFMediaBuffer *input_media_buffer = NULL;

    if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
    {
        /* in-place NAL to annex B conversion. */
        p_block = hxxx_helper_process_block(&p_sys->hh, p_block);

        if (p_sys->hh.i_input_nal_length_size && !p_sys->b_xps_pushed)
        {
            p_xps_blocks = hxxx_helper_get_extradata_block(&p_sys->hh);
            if (p_xps_blocks)
            {
                size_t extrasize;
                block_ChainProperties(p_xps_blocks, NULL, &extrasize, NULL);
                alloc_size += extrasize;
            }
        }
    }

    if (AllocateInputSample(p_dec, stream_id, &input_sample, alloc_size))
        goto error;

    hr = input_sample->GetBufferByIndex(0, &input_media_buffer);
    if (FAILED(hr))
        goto error;

    BYTE *buffer_start;
    hr = input_media_buffer->Lock(&buffer_start, NULL, NULL);
    if (FAILED(hr))
        goto error;

    if (p_xps_blocks) {
        buffer_start += block_ChainExtract(p_xps_blocks, buffer_start, alloc_size);
        p_sys->b_xps_pushed = true;
    }
    memcpy(buffer_start, p_block->p_buffer, p_block->i_buffer);

    hr = input_media_buffer->Unlock();
    if (FAILED(hr))
        goto error;

    hr = input_media_buffer->SetCurrentLength(p_block->i_buffer);
    if (FAILED(hr))
        goto error;

    ts = p_block->i_pts == VLC_TICK_INVALID ? p_block->i_dts : p_block->i_pts;

    /* Convert from microseconds to 100 nanoseconds unit. */
    hr = input_sample->SetSampleTime(MSFTIME_FROM_VLC_TICK(ts));
    if (FAILED(hr))
        goto error;

    hr = p_sys->mft->ProcessInput(stream_id, input_sample, 0);
    if (FAILED(hr))
    {
        msg_Dbg(p_dec, "Failed to process input stream %lu (error 0x%lX)", stream_id, hr);
        goto error;
    }

    input_media_buffer->Release();
    input_sample->Release();
    block_ChainRelease(p_xps_blocks);

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in ProcessInputStream(). (hr=0x%lX)\n", hr);
    if (input_sample)
        input_sample->Release();
    block_ChainRelease(p_xps_blocks);
    return VLC_EGENERIC;
}

/* Copy a packed buffer (no padding) to a picture_t */
static void CopyPackedBufferToPicture(picture_t *p_pic, const uint8_t *p_src)
{
    for (int i = 0; i < p_pic->i_planes; ++i)
    {
        uint8_t *p_dst = p_pic->p[i].p_pixels;

        if (p_pic->p[i].i_visible_pitch == p_pic->p[i].i_pitch)
        {
            /* Plane is packed, only one memcpy is needed. */
            uint32_t plane_size = p_pic->p[i].i_pitch * p_pic->p[i].i_visible_lines;
            memcpy(p_dst, p_src, plane_size);
            p_src += plane_size;
            continue;
        }

        for (int i_line = 0; i_line < p_pic->p[i].i_visible_lines; i_line++)
        {
            memcpy(p_dst, p_src, p_pic->p[i].i_visible_pitch);
            p_src += p_pic->p[i].i_visible_pitch;
            p_dst += p_pic->p[i].i_pitch;
        }
    }
}

static int ProcessOutputStream(decoder_t *p_dec, DWORD stream_id, bool & keep_reading)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr;
    picture_t *picture = NULL;
    block_t *aout_buffer = NULL;

    DWORD output_status = 0;
    MFT_OUTPUT_DATA_BUFFER output_buffer = { stream_id, p_sys->output_sample, 0, NULL };
    hr = p_sys->mft->ProcessOutput(0, 1, &output_buffer, &output_status);
    if (output_buffer.pEvents)
        output_buffer.pEvents->Release();
    /* Use the returned sample since it can be provided by the MFT. */
    IMFSample *output_sample = output_buffer.pSample;
    IMFMediaBuffer *output_media_buffer = NULL;

    keep_reading = false;
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
        return VLC_SUCCESS;

    if (hr == S_OK)
    {
        if (!output_sample)
            return VLC_SUCCESS;

        LONGLONG sample_time;
        hr = output_sample->GetSampleTime(&sample_time);
        if (FAILED(hr))
            goto error;
        /* Convert from 100 nanoseconds unit to microseconds. */
        vlc_tick_t samp_time = VLC_TICK_FROM_MSFTIME(sample_time);

        DWORD output_count = 0;
        hr = output_sample->GetBufferCount(&output_count);
        if (unlikely(FAILED(hr)))
            goto error;

        for (DWORD buf_index = 0; buf_index < output_count; buf_index++)
        {
            hr = output_sample->GetBufferByIndex(buf_index, &output_media_buffer);
            if (FAILED(hr))
                goto error;

            if (p_dec->fmt_in.i_cat == VIDEO_ES)
            {
                if (decoder_UpdateVideoFormat(p_dec))
                    return VLC_SUCCESS;
                picture = decoder_NewPicture(p_dec);
                if (!picture)
                    return VLC_SUCCESS;

                UINT32 interlaced = false;
                hr = output_sample->GetUINT32(MFSampleExtension_Interlaced, &interlaced);
                if (FAILED(hr))
                    picture->b_progressive = true;
                else
                    picture->b_progressive = !interlaced;

                picture->date = samp_time;

                BYTE *buffer_start;
                hr = output_media_buffer->Lock(&buffer_start, NULL, NULL);
                if (FAILED(hr))
                    goto error;

                CopyPackedBufferToPicture(picture, buffer_start);

                hr = output_media_buffer->Unlock();
                if (FAILED(hr))
                    goto error;

                decoder_QueueVideo(p_dec, picture);
            }
            else
            {
                if (decoder_UpdateAudioFormat(p_dec))
                    goto error;
                if (p_dec->fmt_out.audio.i_bitspersample == 0 || p_dec->fmt_out.audio.i_channels == 0)
                    goto error;

                DWORD total_length = 0;
                hr = output_sample->GetTotalLength(&total_length);
                if (FAILED(hr))
                    goto error;

                int samples = total_length / (p_dec->fmt_out.audio.i_bitspersample * p_dec->fmt_out.audio.i_channels / 8);
                aout_buffer = decoder_NewAudioBuffer(p_dec, samples);
                if (!aout_buffer)
                    return VLC_SUCCESS;
                if (aout_buffer->i_buffer < total_length)
                    goto error;

                aout_buffer->i_pts = samp_time;

                BYTE *buffer_start;
                hr = output_media_buffer->Lock(&buffer_start, NULL, NULL);
                if (FAILED(hr))
                    goto error;

                memcpy(aout_buffer->p_buffer, buffer_start, total_length);

                hr = output_media_buffer->Unlock();
                if (FAILED(hr))
                    goto error;

                decoder_QueueAudio(p_dec, aout_buffer);
            }

            if (p_sys->output_sample)
            {
                /* Sample is not provided by the MFT: clear its content. */
                hr = output_media_buffer->SetCurrentLength(0);
                if (FAILED(hr))
                    goto error;
            }

            output_media_buffer->Release();
            output_media_buffer = NULL;
        }

        if (!p_sys->output_sample)
        {
            /* Sample is provided by the MFT: decrease refcount. */
            output_sample->Release();
        }

        keep_reading = true;
        return VLC_SUCCESS;
    }

    if (hr == MF_E_TRANSFORM_STREAM_CHANGE || hr == MF_E_TRANSFORM_TYPE_NOT_SET)
    {
        if (SetOutputType(p_dec, p_sys->output_stream_id))
            goto error;

        /* Reallocate output sample. */
        if (p_sys->output_sample)
        {
            p_sys->output_sample->Release();
            p_sys->output_sample = NULL;
        }
        if (AllocateOutputSample(p_dec, p_sys->output_stream_id, &p_sys->output_sample))
            goto error;
        // there's an output ready, keep trying
        keep_reading = hr == MF_E_TRANSFORM_STREAM_CHANGE;
        return VLC_SUCCESS;
    }

    /* An error not listed above occurred */
    msg_Dbg(p_dec, "Failed to process output stream %lu (error 0x%lX)", stream_id, hr);

error:
    msg_Err(p_dec, "Error in ProcessOutputStream()");
    if (output_media_buffer)
        output_media_buffer->Release();
    if (picture)
        picture_Release(picture);
    if (aout_buffer)
        block_Release(aout_buffer);
    return VLC_EGENERIC;
}

static void Flush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    hr = p_sys->mft->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
}

static int DecodeSync(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);

    if (p_block && p_block->i_flags & (BLOCK_FLAG_CORRUPTED))
    {
        block_Release(p_block);
        return VLCDEC_SUCCESS;
    }

    if (p_block == NULL)
    {
        HRESULT hr;
        hr = p_sys->mft->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        if (FAILED(hr))
            return VLC_EGENERIC;
    }

    /* Drain the output stream before sending the input packet. */
    bool keep_reading;
    int err;
    do {
        err = ProcessOutputStream(p_dec, p_sys->output_stream_id, keep_reading);
    } while (err == VLC_SUCCESS && keep_reading);
    if (err != VLC_SUCCESS)
        goto error;

    if (p_block != NULL )
    {
        if (ProcessInputStream(p_dec, p_sys->input_stream_id, p_block))
            goto error;
        block_Release(p_block);
    }

    return VLCDEC_SUCCESS;

error:
    msg_Err(p_dec, "Error in DecodeSync()");
    block_Release(p_block);
    return VLCDEC_SUCCESS;
}

static HRESULT DequeueMediaEvent(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    IMFMediaEvent *event = NULL;
    hr = p_sys->event_generator->GetEvent(MF_EVENT_FLAG_NO_WAIT, &event);
    if (FAILED(hr))
        return hr;
    MediaEventType event_type;
    hr = event->GetType(&event_type);
    event->Release();
    if (FAILED(hr))
        return hr;

    if (event_type == VLC_METransformNeedInput)
        p_sys->pending_input_events += 1;
    else if (event_type == VLC_METransformHaveOutput)
        p_sys->pending_output_events += 1;
    else
        msg_Err(p_dec, "Unsupported asynchronous event.");

    return S_OK;
}

static int DecodeAsync(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    if (!p_block) /* No Drain */
        return VLCDEC_SUCCESS;

    if (p_block->i_flags & (BLOCK_FLAG_CORRUPTED))
    {
        block_Release(p_block);
        return VLCDEC_SUCCESS;
    }

    /* Dequeue all pending media events. */
    while ((hr = DequeueMediaEvent(p_dec)) == S_OK)
        continue;
    if (hr != MF_E_NO_EVENTS_AVAILABLE && FAILED(hr))
        goto error;

    /* Drain the output stream of the MFT before sending the input packet. */
    if (p_sys->pending_output_events > 0)
    {
        p_sys->pending_output_events -= 1;
        bool keep_reading;
        int err;
        do {
            err = ProcessOutputStream(p_dec, p_sys->output_stream_id, keep_reading);
        } while (err == VLC_SUCCESS && keep_reading);
        if (err != VLC_SUCCESS)
            goto error;
    }

    /* Poll the MFT and return decoded frames until the input stream is ready. */
    while (p_sys->pending_input_events == 0)
    {
        hr = DequeueMediaEvent(p_dec);
        if (hr == MF_E_NO_EVENTS_AVAILABLE)
        {
            /* Sleep for 1 ms to avoid excessive polling. */
            Sleep(1);
            continue;
        }
        if (FAILED(hr))
            goto error;

        if (p_sys->pending_output_events > 0)
        {
            p_sys->pending_output_events -= 1;
            bool keep_reading;
            int err;
            do {
                err = ProcessOutputStream(p_dec, p_sys->output_stream_id, keep_reading);
            } while (err == VLC_SUCCESS && keep_reading);
            if (err != VLC_SUCCESS)
                goto error;
            break;
        }
    }

    p_sys->pending_input_events -= 1;
    if (ProcessInputStream(p_dec, p_sys->input_stream_id, p_block))
        goto error;

    block_Release(p_block);

    return VLCDEC_SUCCESS;

error:
    msg_Err(p_dec, "Error in DecodeAsync()");
    block_Release(p_block);
    return VLCDEC_SUCCESS;
}

static void DestroyMFT(decoder_t *p_dec);

static int InitializeMFT(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    IMFAttributes *attributes = NULL;
    hr = p_sys->mft->GetAttributes(&attributes);
    if (hr != E_NOTIMPL && FAILED(hr))
        goto error;
    if (SUCCEEDED(hr))
    {
        UINT32 is_async = false;
        hr = attributes->GetUINT32(MF_TRANSFORM_ASYNC, &is_async);
        if (hr != MF_E_ATTRIBUTENOTFOUND && FAILED(hr))
            goto error;
        p_sys->is_async = is_async;
        if (p_sys->is_async)
        {
            hr = attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, true);
            if (FAILED(hr))
                goto error;
            void *pv;
            hr = p_sys->mft->QueryInterface(IID_IMFMediaEventGenerator, &pv);
            if (FAILED(hr))
                goto error;
            p_sys->event_generator = static_cast<IMFMediaEventGenerator *>(pv);
        }
    }

    DWORD input_streams_count;
    DWORD output_streams_count;
    hr = p_sys->mft->GetStreamCount(&input_streams_count, &output_streams_count);
    if (FAILED(hr))
        goto error;
    if (input_streams_count != 1 || output_streams_count != 1)
    {
        msg_Err(p_dec, "MFT decoder should have 1 input stream and 1 output stream.");
        goto error;
    }

    hr = p_sys->mft->GetStreamIDs(1, &p_sys->input_stream_id, 1, &p_sys->output_stream_id);
    if (hr == E_NOTIMPL)
    {
        /*
         * This is not an error, it happens if:
         * - there is a fixed number of streams.
         * AND
         * - streams are numbered consecutively from 0 to N-1.
         */
        p_sys->input_stream_id = 0;
        p_sys->output_stream_id = 0;
    }
    else if (FAILED(hr))
        goto error;

    if (SetInputType(p_dec, p_sys->input_stream_id, &p_sys->input_type))
        goto error;

    if (SetOutputType(p_dec, p_sys->output_stream_id))
        goto error;

    /*
     * The input type was not set by the previous call to
     * SetInputType, try again after setting the output type.
     */
    if (!p_sys->input_type)
        if (SetInputType(p_dec, p_sys->input_stream_id, &p_sys->input_type) || !p_sys->input_type)
            goto error;

    /* This call can be a no-op for some MFT decoders, but it can potentially reduce starting time. */
    hr = p_sys->mft->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, (ULONG_PTR)0);
    if (FAILED(hr))
        goto error;

    /* This event is required for asynchronous MFTs, optional otherwise. */
    hr = p_sys->mft->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, (ULONG_PTR)0);
    if (FAILED(hr))
        goto error;

    if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
    {
        /* It's not an error if the following call fails. */
        attributes->SetUINT32(CODECAPI_AVLowLatencyMode, true);

        hxxx_helper_init(&p_sys->hh, VLC_OBJECT(p_dec), p_dec->fmt_in.i_codec, 0, 0);
        hxxx_helper_set_extra(&p_sys->hh, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra);
    }
    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in InitializeMFT()");
    DestroyMFT(p_dec);
    return VLC_EGENERIC;
}

static void DestroyMFT(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);

    if (p_sys->event_generator)
        p_sys->event_generator->Release();
    if (p_sys->input_type)
        p_sys->input_type->Release();
    if (p_sys->output_sample)
    {
        p_sys->output_sample->RemoveAllBuffers();
    }

    if (p_sys->mft)
    {
        p_sys->mft->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, (ULONG_PTR)0);
        p_sys->mft->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, (ULONG_PTR)0);
        p_sys->mft->Release();
    }

    if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
        hxxx_helper_clean(&p_sys->hh);

    p_sys->event_generator = NULL;
    p_sys->input_type = NULL;
    p_sys->mft = NULL;

    MFShutdown();
}

static int FindMFT(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    /* Try to create a MFT using MFTEnumEx. */
    GUID category;
    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        category = MFT_CATEGORY_VIDEO_DECODER;
        p_sys->major_type = &MFMediaType_Video;
        p_sys->subtype = FormatToGUID(video_format_table, p_dec->fmt_in.i_codec);
        if(!p_sys->subtype) {
            /* Codec is not well known. Construct a MF transform subtype from the fourcc */
            p_sys->custom_subtype = MFVideoFormat_Base;
            p_sys->custom_subtype.Data1 = p_dec->fmt_in.i_codec;
            p_sys->subtype = &p_sys->custom_subtype;
        }
    }
    else
    {
        category = MFT_CATEGORY_AUDIO_DECODER;
        p_sys->major_type = &MFMediaType_Audio;
        p_sys->subtype = FormatToGUID(audio_format_table, p_dec->fmt_in.i_codec);
    }
    if (!p_sys->subtype)
        return VLC_EGENERIC;

    UINT32 flags = MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_LOCALMFT
                 | MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT
                 | MFT_ENUM_FLAG_HARDWARE;
    MFT_REGISTER_TYPE_INFO input_type = { *p_sys->major_type, *p_sys->subtype };
    IMFActivate **activate_objects = NULL;
    UINT32 activate_objects_count = 0;
    hr = MFTEnumEx(category, flags, &input_type, NULL, &activate_objects, &activate_objects_count);
    if (FAILED(hr))
        return VLC_EGENERIC;

    msg_Dbg(p_dec, "Found %d available MFT module(s) for %4.4s", activate_objects_count, (const char*)&p_dec->fmt_in.i_codec);
    if (activate_objects_count == 0)
        return VLC_EGENERIC;

    void *pv;
    for (UINT32 i = 0; i < activate_objects_count; ++i)
    {
        hr = activate_objects[i]->ActivateObject(IID_IMFTransform, &pv);
        activate_objects[i]->Release();
        if (FAILED(hr))
            continue;
        p_sys->mft = static_cast<IMFTransform *>(pv);

        if (InitializeMFT(p_dec) == VLC_SUCCESS)
        {
            for (++i; i < activate_objects_count; ++i)
                activate_objects[i]->Release();
            CoTaskMemFree(activate_objects);
            return VLC_SUCCESS;
        }
    }
    CoTaskMemFree(activate_objects);

    return VLC_EGENERIC;
}

static int LoadMFTLibrary()
{
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr))
        return VLC_EGENERIC;


    return VLC_SUCCESS;
}

static int Open(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys;

    p_sys = static_cast<decoder_sys_t*>(calloc(1, sizeof(*p_sys)));
    if (!p_sys)
        return VLC_ENOMEM;
    p_dec->p_sys = p_sys;

    if( FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)) )
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    if (LoadMFTLibrary())
    {
        msg_Err(p_dec, "Failed to load MFT library.");
        goto error;
    }

    if (FindMFT(p_dec))
    {
        msg_Err(p_dec, "Could not find suitable MFT decoder");
        goto error;
    }

    /* Only one output sample is needed, we can allocate one and reuse it. */
    if (AllocateOutputSample(p_dec, p_sys->output_stream_id, &p_sys->output_sample))
        goto error;

    p_dec->pf_decode = p_sys->is_async ? DecodeAsync : DecodeSync;
    p_dec->pf_flush = p_sys->is_async ? NULL : Flush;

    return VLC_SUCCESS;

error:
    Close(p_this);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = static_cast<decoder_sys_t*>(p_dec->p_sys);

    DestroyMFT(p_dec);

    free(p_sys);

    CoUninitialize();
}
