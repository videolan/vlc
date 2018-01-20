/*****************************************************************************
 * mft.c : Media Foundation Transform audio/video decoder
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

#undef WINVER
#define WINVER 0x0601

/* Needed for many mingw macros. */
#define COBJMACROS

/* Avoid having GUIDs being defined as "extern". */
#define INITGUID

#ifndef STDCALL
# define STDCALL __stdcall
#endif

#include <winapifamily.h>
#undef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include "../packetizer/h264_nal.h"
#define _VIDEOINFOHEADER_
#include <vlc_codecs.h>

#include <mfapi.h>
#include <mftransform.h>
#include <mferror.h>
#include <mfobjects.h>

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_description(N_("Media Foundation Transform decoder"))
    add_shortcut("mft")
    set_capability("video decoder", 1)
    set_callbacks(Open, Close)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)

    add_submodule()
    add_shortcut("mft")
    set_capability("audio decoder", 1)
    set_callbacks(Open, Close)
vlc_module_end()

typedef struct
{
    HINSTANCE mfplat_dll;
    HRESULT (STDCALL *fptr_MFTEnumEx)(GUID guidCategory, UINT32 Flags,
                                 const MFT_REGISTER_TYPE_INFO *pInputType,
                                 const MFT_REGISTER_TYPE_INFO *pOutputType,
                                 IMFActivate ***pppMFTActivate, UINT32 *pcMFTActivate);
    HRESULT (STDCALL *fptr_MFCreateSample)(IMFSample **ppIMFSample);
    HRESULT (STDCALL *fptr_MFCreateMemoryBuffer)(DWORD cbMaxLength, IMFMediaBuffer **ppBuffer);
    HRESULT (STDCALL *fptr_MFCreateAlignedMemoryBuffer)(DWORD cbMaxLength, DWORD fAlignmentFlags, IMFMediaBuffer **ppBuffer);
} MFHandle;

struct decoder_sys_t
{
    MFHandle mf_handle;

    IMFTransform *mft;

    const GUID* major_type;
    const GUID* subtype;

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
    IMFMediaType *output_type;

    /* H264 only. */
    uint8_t nal_length_size;
};

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
#ifndef MF_E_TRANSFORM_NEED_MORE_INPUT
# define MF_E_TRANSFORM_NEED_MORE_INPUT _HRESULT_TYPEDEF_(0xc00d6d72)
#endif

#ifndef MF_E_TRANSFORM_STREAM_CHANGE
# define MF_E_TRANSFORM_STREAM_CHANGE _HRESULT_TYPEDEF_(0xc00d6d61)
#endif

#ifndef MF_E_NO_EVENTS_AVAILABLE
# define MF_E_NO_EVENTS_AVAILABLE _HRESULT_TYPEDEF_(0xC00D3E80L)
#endif

#ifndef MF_EVENT_FLAG_NO_WAIT
# define MF_EVENT_FLAG_NO_WAIT 0x00000001
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
    { VLC_CODEC_MJPG, &MFVideoFormat_MJPG },
    { VLC_CODEC_WMV1, &MFVideoFormat_WMV1 },
    { VLC_CODEC_WMV2, &MFVideoFormat_WMV2 },
    { VLC_CODEC_WMV3, &MFVideoFormat_WMV3 },
    { VLC_CODEC_VC1,  &MFVideoFormat_WVC1 },
    { 0, NULL }
};

#if defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 4
DEFINE_GUID(MFAudioFormat_Dolby_AC3, 0xe06d802c, 0xdb46, 0x11cf, 0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea);
#endif
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

/*
 * Low latency mode for Windows 8. Without this option, the H264
 * decoder will fill *all* its internal buffers before returning a
 * frame. Because of this behavior, the decoder might return no frame
 * for more than 500 ms, making it unusable for playback.
 */
DEFINE_GUID(CODECAPI_AVLowLatencyMode, 0x9c27891a, 0xed7a, 0x40e1, 0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee);

static int SetInputType(decoder_t *p_dec, DWORD stream_id, IMFMediaType **result)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    HRESULT hr;

    *result = NULL;

    IMFMediaType *input_media_type = NULL;

    /* Search a suitable input type for the MFT. */
    int input_type_index = 0;
    bool found = false;
    for (int i = 0; !found; ++i)
    {
        hr = IMFTransform_GetInputAvailableType(p_sys->mft, stream_id, i, &input_media_type);
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
        hr = IMFMediaType_GetGUID(input_media_type, &MF_MT_SUBTYPE, &subtype);
        if (FAILED(hr))
            goto error;

        if (IsEqualGUID(&subtype, p_sys->subtype))
            found = true;

        if (found)
            input_type_index = i;

        IMFMediaType_Release(input_media_type);
        input_media_type = NULL;
    }
    if (!found)
        goto error;

    hr = IMFTransform_GetInputAvailableType(p_sys->mft, stream_id, input_type_index, &input_media_type);
    if (FAILED(hr))
        goto error;

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        UINT64 width = p_dec->fmt_in.video.i_width;
        UINT64 height = p_dec->fmt_in.video.i_height;
        UINT64 frame_size = (width << 32) | height;
        hr = IMFMediaType_SetUINT64(input_media_type, &MF_MT_FRAME_SIZE, frame_size);
        if (FAILED(hr))
            goto error;
    }
    else
    {
        hr = IMFMediaType_SetUINT32(input_media_type, &MF_MT_ORIGINAL_WAVE_FORMAT_TAG, p_sys->subtype->Data1);
        if (FAILED(hr))
            goto error;
        if (p_dec->fmt_in.audio.i_rate)
        {
            hr = IMFMediaType_SetUINT32(input_media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, p_dec->fmt_in.audio.i_rate);
            if (FAILED(hr))
                goto error;
        }
        if (p_dec->fmt_in.audio.i_channels)
        {
            hr = IMFMediaType_SetUINT32(input_media_type, &MF_MT_AUDIO_NUM_CHANNELS, p_dec->fmt_in.audio.i_channels);
            if (FAILED(hr))
                goto error;
        }
        if (p_dec->fmt_in.audio.i_bitspersample)
        {
            hr = IMFMediaType_SetUINT32(input_media_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, p_dec->fmt_in.audio.i_bitspersample);
            if (FAILED(hr))
                goto error;
        }
        if (p_dec->fmt_in.audio.i_blockalign)
        {
            hr = IMFMediaType_SetUINT32(input_media_type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, p_dec->fmt_in.audio.i_blockalign);
            if (FAILED(hr))
                goto error;
        }
        if (p_dec->fmt_in.i_bitrate)
        {
            hr = IMFMediaType_SetUINT32(input_media_type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, p_dec->fmt_in.i_bitrate / 8);
            if (FAILED(hr))
                goto error;
        }
    }

    if (p_dec->fmt_in.i_extra > 0)
    {
        UINT32 blob_size = 0;
        hr = IMFMediaType_GetBlobSize(input_media_type, &MF_MT_USER_DATA, &blob_size);
        /*
         * Do not overwrite existing user data in the input type, this
         * can cause the MFT to reject the type.
         */
        if (hr == MF_E_ATTRIBUTENOTFOUND)
        {
            hr = IMFMediaType_SetBlob(input_media_type, &MF_MT_USER_DATA,
                                      (const UINT8*)p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra);
            if (FAILED(hr))
                goto error;
        }
    }

    hr = IMFTransform_SetInputType(p_sys->mft, stream_id, input_media_type, 0);
    if (FAILED(hr))
        goto error;

    *result = input_media_type;

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in SetInputType()");
    if (input_media_type)
        IMFMediaType_Release(input_media_type);
    return VLC_EGENERIC;
}

static int SetOutputType(decoder_t *p_dec, DWORD stream_id, IMFMediaType **result)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    HRESULT hr;

    *result = NULL;

    IMFMediaType *output_media_type = NULL;

    /*
     * Enumerate available output types. The list is ordered by
     * preference thus we will use the first one unless YV12/I420 is
     * available for video or float32 for audio.
     */
    int output_type_index = 0;
    bool found = false;
    for (int i = 0; !found; ++i)
    {
        hr = IMFTransform_GetOutputAvailableType(p_sys->mft, stream_id, i, &output_media_type);
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
        hr = IMFMediaType_GetGUID(output_media_type, &MF_MT_SUBTYPE, &subtype);
        if (FAILED(hr))
            goto error;

        if (p_dec->fmt_in.i_cat == VIDEO_ES)
        {
            if (IsEqualGUID(&subtype, &MFVideoFormat_YV12) || IsEqualGUID(&subtype, &MFVideoFormat_I420))
                found = true;
        }
        else
        {
            UINT32 bits_per_sample;
            hr = IMFMediaType_GetUINT32(output_media_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &bits_per_sample);
            if (FAILED(hr))
                continue;
            if (bits_per_sample == 32 && IsEqualGUID(&subtype, &MFAudioFormat_Float))
                found = true;
        }

        if (found)
            output_type_index = i;

        IMFMediaType_Release(output_media_type);
        output_media_type = NULL;
    }
    /*
     * It's not an error if we don't find the output type we were
     * looking for, in this case we use the first available type which
     * is the "preferred" output type for this MFT.
     */

    hr = IMFTransform_GetOutputAvailableType(p_sys->mft, stream_id, output_type_index, &output_media_type);
    if (FAILED(hr))
        goto error;

    hr = IMFTransform_SetOutputType(p_sys->mft, stream_id, output_media_type, 0);
    if (FAILED(hr))
        goto error;

    GUID subtype;
    hr = IMFMediaType_GetGUID(output_media_type, &MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr))
        goto error;

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        video_format_Copy( &p_dec->fmt_out.video, &p_dec->fmt_in.video );
        p_dec->fmt_out.i_codec = vlc_fourcc_GetCodec(p_dec->fmt_in.i_cat, subtype.Data1);
    }
    else
    {
        p_dec->fmt_out.audio = p_dec->fmt_in.audio;

        UINT32 bitspersample = 0;
        hr = IMFMediaType_GetUINT32(output_media_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &bitspersample);
        if (SUCCEEDED(hr) && bitspersample)
            p_dec->fmt_out.audio.i_bitspersample = bitspersample;

        UINT32 channels = 0;
        hr = IMFMediaType_GetUINT32(output_media_type, &MF_MT_AUDIO_NUM_CHANNELS, &channels);
        if (SUCCEEDED(hr) && channels)
            p_dec->fmt_out.audio.i_channels = channels;

        UINT32 rate = 0;
        hr = IMFMediaType_GetUINT32(output_media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        if (SUCCEEDED(hr) && rate)
            p_dec->fmt_out.audio.i_rate = rate;

        vlc_fourcc_t fourcc;
        wf_tag_to_fourcc(subtype.Data1, &fourcc, NULL);
        p_dec->fmt_out.i_codec = vlc_fourcc_GetCodecAudio(fourcc, p_dec->fmt_out.audio.i_bitspersample);

        p_dec->fmt_out.audio.i_physical_channels = pi_channels_maps[p_dec->fmt_out.audio.i_channels];
    }

    *result = output_media_type;

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in SetOutputType()");
    if (output_media_type)
        IMFMediaType_Release(output_media_type);
    return VLC_EGENERIC;
}

static int AllocateInputSample(decoder_t *p_dec, DWORD stream_id, IMFSample** result, DWORD size)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    MFHandle *mf = &p_sys->mf_handle;
    HRESULT hr;

    *result = NULL;

    IMFSample *input_sample = NULL;

    MFT_INPUT_STREAM_INFO input_info;
    hr = IMFTransform_GetInputStreamInfo(p_sys->mft, stream_id, &input_info);
    if (FAILED(hr))
        goto error;

    hr = mf->fptr_MFCreateSample(&input_sample);
    if (FAILED(hr))
        goto error;

    IMFMediaBuffer *input_media_buffer = NULL;
    DWORD allocation_size = __MAX(input_info.cbSize, size);
    hr = mf->fptr_MFCreateMemoryBuffer(allocation_size, &input_media_buffer);
    if (FAILED(hr))
        goto error;

    hr = IMFSample_AddBuffer(input_sample, input_media_buffer);
    IMFMediaBuffer_Release(input_media_buffer);
    if (FAILED(hr))
        goto error;

    *result = input_sample;

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in AllocateInputSample()");
    if (input_sample)
        IMFSample_Release(input_sample);
    if (input_media_buffer)
        IMFMediaBuffer_Release(input_media_buffer);
    return VLC_EGENERIC;
}

static int AllocateOutputSample(decoder_t *p_dec, DWORD stream_id, IMFSample **result)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    MFHandle *mf = &p_sys->mf_handle;
    HRESULT hr;

    *result = NULL;

    IMFSample *output_sample = NULL;

    MFT_OUTPUT_STREAM_INFO output_info;
    hr = IMFTransform_GetOutputStreamInfo(p_sys->mft, stream_id, &output_info);
    if (FAILED(hr))
        goto error;

    if (output_info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES))
    {
        /* The MFT will provide an allocated sample. */
        return VLC_SUCCESS;
    }

    DWORD expected_flags = 0;
    if (p_dec->fmt_in.i_cat == VIDEO_ES)
        expected_flags |= MFT_OUTPUT_STREAM_WHOLE_SAMPLES
                        | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER
                        | MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;
    if ((output_info.dwFlags & expected_flags) != expected_flags)
        goto error;

    hr = mf->fptr_MFCreateSample(&output_sample);
    if (FAILED(hr))
        goto error;

    IMFMediaBuffer *output_media_buffer = NULL;
    DWORD allocation_size = output_info.cbSize;
    DWORD alignment = output_info.cbAlignment;
    if (alignment > 0)
        hr = mf->fptr_MFCreateAlignedMemoryBuffer(allocation_size, alignment - 1, &output_media_buffer);
    else
        hr = mf->fptr_MFCreateMemoryBuffer(allocation_size, &output_media_buffer);
    if (FAILED(hr))
        goto error;

    hr = IMFSample_AddBuffer(output_sample, output_media_buffer);
    if (FAILED(hr))
        goto error;

    *result = output_sample;

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in AllocateOutputSample()");
    if (output_sample)
        IMFSample_Release(output_sample);
    return VLC_EGENERIC;
}

static int ProcessInputStream(decoder_t *p_dec, DWORD stream_id, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    HRESULT hr;
    IMFSample *input_sample = NULL;

    if (AllocateInputSample(p_dec, stream_id, &input_sample, p_block->i_buffer))
        goto error;

    IMFMediaBuffer *input_media_buffer = NULL;
    hr = IMFSample_GetBufferByIndex(input_sample, 0, &input_media_buffer);
    if (FAILED(hr))
        goto error;

    BYTE *buffer_start;
    hr = IMFMediaBuffer_Lock(input_media_buffer, &buffer_start, NULL, NULL);
    if (FAILED(hr))
        goto error;

    memcpy(buffer_start, p_block->p_buffer, p_block->i_buffer);

    if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
    {
        /* in-place NAL to annex B conversion. */
        h264_AVC_to_AnnexB(buffer_start, p_block->i_buffer, p_sys->nal_length_size);
    }

    hr = IMFMediaBuffer_Unlock(input_media_buffer);
    if (FAILED(hr))
        goto error;

    hr = IMFMediaBuffer_SetCurrentLength(input_media_buffer, p_block->i_buffer);
    if (FAILED(hr))
        goto error;

    LONGLONG ts = p_block->i_pts;
    if (!ts && p_block->i_dts)
        ts = p_block->i_dts;

    /* Convert from microseconds to 100 nanoseconds unit. */
    hr = IMFSample_SetSampleTime(input_sample, ts * 10);
    if (FAILED(hr))
        goto error;

    hr = IMFTransform_ProcessInput(p_sys->mft, stream_id, input_sample, 0);
    if (FAILED(hr))
        goto error;

    IMFMediaBuffer_Release(input_media_buffer);
    IMFSample_Release(input_sample);

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in ProcessInputStream()");
    if (input_sample)
        IMFSample_Release(input_sample);
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

static int ProcessOutputStream(decoder_t *p_dec, DWORD stream_id)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    HRESULT hr;
    picture_t *picture = NULL;
    block_t *aout_buffer = NULL;

    DWORD output_status = 0;
    MFT_OUTPUT_DATA_BUFFER output_buffer = { stream_id, p_sys->output_sample, 0, NULL };
    hr = IMFTransform_ProcessOutput(p_sys->mft, 0, 1, &output_buffer, &output_status);
    if (output_buffer.pEvents)
        IMFCollection_Release(output_buffer.pEvents);
    /* Use the returned sample since it can be provided by the MFT. */
    IMFSample *output_sample = output_buffer.pSample;

    if (hr == S_OK)
    {
        if (!output_sample)
            return VLC_SUCCESS;

        LONGLONG sample_time;
        hr = IMFSample_GetSampleTime(output_sample, &sample_time);
        if (FAILED(hr))
            goto error;
        /* Convert from 100 nanoseconds unit to microseconds. */
        sample_time /= 10;

        DWORD total_length = 0;
        hr = IMFSample_GetTotalLength(output_sample, &total_length);
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
            hr = IMFSample_GetUINT32(output_sample, &MFSampleExtension_Interlaced, &interlaced);
            picture->b_progressive = !interlaced;

            picture->date = sample_time;
        }
        else
        {
            if (decoder_UpdateAudioFormat(p_dec))
                goto error;
            if (p_dec->fmt_out.audio.i_bitspersample == 0 || p_dec->fmt_out.audio.i_channels == 0)
                goto error;
            int samples = total_length / (p_dec->fmt_out.audio.i_bitspersample * p_dec->fmt_out.audio.i_channels / 8);
            aout_buffer = decoder_NewAudioBuffer(p_dec, samples);
            if (!aout_buffer)
                return VLC_SUCCESS;
            if (aout_buffer->i_buffer < total_length)
                goto error;

            aout_buffer->i_pts = sample_time;
        }

        IMFMediaBuffer *output_media_buffer = NULL;
        hr = IMFSample_GetBufferByIndex(output_sample, 0, &output_media_buffer);

        BYTE *buffer_start;
        hr = IMFMediaBuffer_Lock(output_media_buffer, &buffer_start, NULL, NULL);
        if (FAILED(hr))
            goto error;

        if (p_dec->fmt_in.i_cat == VIDEO_ES)
            CopyPackedBufferToPicture(picture, buffer_start);
        else
            memcpy(aout_buffer->p_buffer, buffer_start, total_length);

        hr = IMFMediaBuffer_Unlock(output_media_buffer);
        IMFSample_Release(output_media_buffer);
        if (FAILED(hr))
            goto error;

        if (p_sys->output_sample)
        {
            /* Sample is not provided by the MFT: clear its content. */
            hr = IMFMediaBuffer_SetCurrentLength(output_media_buffer, 0);
            if (FAILED(hr))
                goto error;
        }
        else
        {
            /* Sample is provided by the MFT: decrease refcount. */
            IMFSample_Release(output_sample);
        }
    }
    else if (hr == MF_E_TRANSFORM_STREAM_CHANGE || hr == MF_E_TRANSFORM_TYPE_NOT_SET)
    {
        if (p_sys->output_type)
            IMFMediaType_Release(p_sys->output_type);
        if (SetOutputType(p_dec, p_sys->output_stream_id, &p_sys->output_type))
            goto error;

        /* Reallocate output sample. */
        if (p_sys->output_sample)
            IMFSample_Release(p_sys->output_sample);
        p_sys->output_sample = NULL;
        if (AllocateOutputSample(p_dec, 0, &p_sys->output_sample))
            goto error;
        return VLC_SUCCESS;
    }
    else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
    {
        return VLC_SUCCESS;
    }
    else /* An error not listed above occurred */
    {
        msg_Err(p_dec, "Unexpected error in IMFTransform::ProcessOutput: %#lx",
                hr);
        goto error;
    }

    if (p_dec->fmt_in.i_cat == VIDEO_ES)
        decoder_QueueVideo(p_dec, picture);
    else
        decoder_QueueAudio(p_dec, aout_buffer);

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in ProcessOutputStream()");
    if (picture)
        picture_Release(picture);
    if (aout_buffer)
        block_Release(aout_buffer);
    return VLC_EGENERIC;
}

static int DecodeSync(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (!p_block) /* No Drain */
        return VLCDEC_SUCCESS;

    if (p_block->i_flags & (BLOCK_FLAG_CORRUPTED))
    {
        block_Release(p_block);
        return VLCDEC_SUCCESS;
    }

    /* Drain the output stream before sending the input packet. */
    if (ProcessOutputStream(p_dec, p_sys->output_stream_id))
        goto error;
    if (ProcessInputStream(p_dec, p_sys->input_stream_id, p_block))
        goto error;
    block_Release(p_block);

    return VLCDEC_SUCCESS;

error:
    msg_Err(p_dec, "Error in DecodeSync()");
    block_Release(p_block);
    return VLCDEC_SUCCESS;
}

static HRESULT DequeueMediaEvent(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    HRESULT hr;

    IMFMediaEvent *event = NULL;
    hr = IMFMediaEventGenerator_GetEvent(p_sys->event_generator, MF_EVENT_FLAG_NO_WAIT, &event);
    if (FAILED(hr))
        return hr;
    MediaEventType event_type;
    hr = IMFMediaEvent_GetType(event, &event_type);
    IMFMediaEvent_Release(event);
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
    decoder_sys_t *p_sys = p_dec->p_sys;
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
        if (ProcessOutputStream(p_dec, p_sys->output_stream_id))
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
            if (ProcessOutputStream(p_dec, p_sys->output_stream_id))
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
    decoder_sys_t *p_sys = p_dec->p_sys;
    HRESULT hr;

    IMFAttributes *attributes = NULL;
    hr = IMFTransform_GetAttributes(p_sys->mft, &attributes);
    if (hr != E_NOTIMPL && FAILED(hr))
        goto error;
    if (SUCCEEDED(hr))
    {
        UINT32 is_async = false;
        hr = IMFAttributes_GetUINT32(attributes, &MF_TRANSFORM_ASYNC, &is_async);
        if (hr != MF_E_ATTRIBUTENOTFOUND && FAILED(hr))
            goto error;
        p_sys->is_async = is_async;
        if (p_sys->is_async)
        {
            hr = IMFAttributes_SetUINT32(attributes, &MF_TRANSFORM_ASYNC_UNLOCK, true);
            if (FAILED(hr))
                goto error;
            hr = IMFTransform_QueryInterface(p_sys->mft, &IID_IMFMediaEventGenerator, (void**)&p_sys->event_generator);
            if (FAILED(hr))
                goto error;
        }
    }

    DWORD input_streams_count;
    DWORD output_streams_count;
    hr = IMFTransform_GetStreamCount(p_sys->mft, &input_streams_count, &output_streams_count);
    if (FAILED(hr))
        goto error;
    if (input_streams_count != 1 || output_streams_count != 1)
    {
        msg_Err(p_dec, "MFT decoder should have 1 input stream and 1 output stream.");
        goto error;
    }

    hr = IMFTransform_GetStreamIDs(p_sys->mft, 1, &p_sys->input_stream_id, 1, &p_sys->output_stream_id);
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

    if (SetOutputType(p_dec, p_sys->output_stream_id, &p_sys->output_type))
        goto error;

    /*
     * The input type was not set by the previous call to
     * SetInputType, try again after setting the output type.
     */
    if (!p_sys->input_type)
        if (SetInputType(p_dec, p_sys->input_stream_id, &p_sys->input_type) || !p_sys->input_type)
            goto error;

    /* This call can be a no-op for some MFT decoders, but it can potentially reduce starting time. */
    hr = IMFTransform_ProcessMessage(p_sys->mft, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, (ULONG_PTR)0);
    if (FAILED(hr))
        goto error;

    /* This event is required for asynchronous MFTs, optional otherwise. */
    hr = IMFTransform_ProcessMessage(p_sys->mft, MFT_MESSAGE_NOTIFY_START_OF_STREAM, (ULONG_PTR)0);
    if (FAILED(hr))
        goto error;

    if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
    {
        /* It's not an error if the following call fails. */
        IMFAttributes_SetUINT32(attributes, &CODECAPI_AVLowLatencyMode, true);

        if (p_dec->fmt_in.i_extra)
        {
            if (h264_isavcC((uint8_t*)p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra))
            {
                size_t i_buf;
                uint8_t *buf = h264_avcC_to_AnnexB_NAL(p_dec->fmt_in.p_extra,
                                                       p_dec->fmt_in.i_extra,
                                                      &i_buf, &p_sys->nal_length_size);
                if(buf)
                {
                    free(p_dec->fmt_in.p_extra);
                    p_dec->fmt_in.p_extra = buf;
                    p_dec->fmt_in.i_extra = i_buf;
                }
            }
        }
    }
    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in InitializeMFT()");
    DestroyMFT(p_dec);
    return VLC_EGENERIC;
}

static void DestroyMFT(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->event_generator)
        IMFMediaEventGenerator_Release(p_sys->event_generator);
    if (p_sys->input_type)
        IMFMediaType_Release(p_sys->input_type);
    if (p_sys->output_sample)
    {
        IMFMediaBuffer *output_media_buffer = NULL;
        HRESULT hr = IMFSample_GetBufferByIndex(p_sys->output_sample, 0, &output_media_buffer);
        if (SUCCEEDED(hr))
            IMFSample_Release(output_media_buffer);
        IMFSample_Release(p_sys->output_sample);
    }
    if (p_sys->output_type)
        IMFMediaType_Release(p_sys->output_type);
    if (p_sys->mft)
        IMFTransform_Release(p_sys->mft);

    p_sys->event_generator = NULL;
    p_sys->input_type = NULL;
    p_sys->output_sample = NULL;
    p_sys->output_type = NULL;
    p_sys->mft = NULL;
}

static int FindMFT(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    MFHandle *mf = &p_sys->mf_handle;
    HRESULT hr;

    /* Try to create a MFT using MFTEnumEx. */
    GUID category;
    if (p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        category = MFT_CATEGORY_VIDEO_DECODER;
        p_sys->major_type = &MFMediaType_Video;
        p_sys->subtype = FormatToGUID(video_format_table, p_dec->fmt_in.i_codec);
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
                 | MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_TRANSCODE_ONLY;
    MFT_REGISTER_TYPE_INFO input_type = { *p_sys->major_type, *p_sys->subtype };
    IMFActivate **activate_objects = NULL;
    UINT32 activate_objects_count = 0;
    hr = mf->fptr_MFTEnumEx(category, flags, &input_type, NULL, &activate_objects, &activate_objects_count);
    if (FAILED(hr))
        return VLC_EGENERIC;

    msg_Dbg(p_dec, "Found %d available MFT module(s)", activate_objects_count);
    if (activate_objects_count == 0)
        return VLC_EGENERIC;

    for (UINT32 i = 0; i < activate_objects_count; ++i)
    {
        hr = IMFActivate_ActivateObject(activate_objects[i], &IID_IMFTransform, (void**)&p_sys->mft);
        IMFActivate_Release(activate_objects[i]);
        if (FAILED(hr))
            continue;

        if (InitializeMFT(p_dec) == VLC_SUCCESS)
        {
            CoTaskMemFree(activate_objects);
            return VLC_SUCCESS;
        }
    }
    CoTaskMemFree(activate_objects);

    return VLC_EGENERIC;
}

static int LoadMFTLibrary(MFHandle *mf)
{
#if _WIN32_WINNT < _WIN32_WINNT_WIN7 || VLC_WINSTORE_APP
    mf->mfplat_dll = LoadLibrary(TEXT("mfplat.dll"));
    if (!mf->mfplat_dll)
        return VLC_EGENERIC;

    mf->fptr_MFTEnumEx = (void*)GetProcAddress(mf->mfplat_dll, "MFTEnumEx");
    mf->fptr_MFCreateSample = (void*)GetProcAddress(mf->mfplat_dll, "MFCreateSample");
    mf->fptr_MFCreateMemoryBuffer = (void*)GetProcAddress(mf->mfplat_dll, "MFCreateMemoryBuffer");
    mf->fptr_MFCreateAlignedMemoryBuffer = (void*)GetProcAddress(mf->mfplat_dll, "MFCreateAlignedMemoryBuffer");
    if (!mf->fptr_MFTEnumEx || !mf->fptr_MFCreateSample || !mf->fptr_MFCreateMemoryBuffer || !mf->fptr_MFCreateAlignedMemoryBuffer)
        return VLC_EGENERIC;
#else
    mf->fptr_MFTEnumEx = &MFTEnumEx;
    mf->fptr_MFCreateSample = &MFCreateSample;
    mf->fptr_MFCreateMemoryBuffer = &MFCreateMemoryBuffer;
    mf->fptr_MFCreateAlignedMemoryBuffer = &MFCreateAlignedMemoryBuffer;
#endif

    return VLC_SUCCESS;
}

static int Open(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys;

    p_sys = p_dec->p_sys = calloc(1, sizeof(*p_sys));
    if (!p_sys)
        return VLC_ENOMEM;

    if( FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)) )
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    if (LoadMFTLibrary(&p_sys->mf_handle))
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
    if (AllocateOutputSample(p_dec, 0, &p_sys->output_sample))
        goto error;

    p_dec->pf_decode = p_sys->is_async ? DecodeAsync : DecodeSync;

    return VLC_SUCCESS;

error:
    Close(p_this);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    MFHandle *mf = &p_sys->mf_handle;

    DestroyMFT(p_dec);

    if (mf->mfplat_dll)
        FreeLibrary(mf->mfplat_dll);

    free(p_sys);

    CoUninitialize();
}
