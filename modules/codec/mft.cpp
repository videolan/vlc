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

#include "../video_chroma/d3d11_fmt.h"

#include <initguid.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mferror.h>
#include <mfobjects.h>
#include <codecapi.h>


#define _VIDEOINFOHEADER_
#include <vlc_codecs.h>

#include <algorithm>
#include <atomic>
#include <new>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

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

typedef HRESULT (WINAPI *pf_MFCreateDXGIDeviceManager)(UINT *, IMFDXGIDeviceManager **);

class mft_dec_sys_t
{
public:
    ComPtr<IMFTransform> mft;

    ~mft_dec_sys_t()
    {
        assert(!streamStarted);
    }

    const GUID* major_type = nullptr;
    const GUID* subtype = nullptr;
    /* Container for a dynamically constructed subtype */
    GUID custom_subtype;

    // Direct3D
    vlc_video_context  *vctx_out = nullptr;
    const d3d_format_t *cfg = nullptr;
    HRESULT (WINAPI *fptr_MFCreateDXGIDeviceManager)(UINT *resetToken, IMFDXGIDeviceManager **ppDeviceManager) = nullptr;
    UINT dxgi_token = 0;
    ComPtr<IMFDXGIDeviceManager> dxgi_manager;
    HANDLE d3d_handle = INVALID_HANDLE_VALUE;

    // D3D11
    ComPtr<ID3D11Texture2D> cached_tex;
    ID3D11ShaderResourceView *cachedSRV[32][DXGI_MAX_SHADER_VIEW] = {{nullptr}};

    /* For asynchronous MFT */
    bool is_async = false;
    ComPtr<IMFMediaEventGenerator> event_generator;
    int pending_input_events = 0;
    int pending_output_events = 0;

    /* Input stream */
    DWORD input_stream_id = 0;
    ComPtr<IMFMediaType> input_type;

    /* Output stream */
    DWORD output_stream_id = 0;
    ComPtr<IMFSample> output_sample;

    /* H264 only. */
    struct hxxx_helper hh = {};
    bool   b_xps_pushed = false; ///< (for xvcC) parameter sets pushed (SPS/PPS/VPS)

    std::atomic<size_t>  refcount{1};


    void AddRef()
    {
        refcount++;
    }

    bool Release()
    {
        if (--refcount == 0)
        {
            DoRelease();
            return true;
        }
        return false;
    }

    /// Required for Async MFTs
    HRESULT startStream()
    {
        assert(!streamStarted);
        HRESULT hr = mft->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, (ULONG_PTR)0);
        if (SUCCEEDED(hr))
            streamStarted = true;
        return hr;
    }
    /// Used for Async MFTs
    HRESULT endStream()
    {
        assert(streamStarted);
        HRESULT hr = mft->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, (ULONG_PTR)0);
        if (SUCCEEDED(hr))
            streamStarted = false;
        return hr;
    }

    HRESULT flushStream()
    {
        HRESULT hr = mft->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        if (SUCCEEDED(hr))
            streamStarted = false;
        return hr;
    }

private:

    void DoRelease()
    {
        if (output_sample.Get())
            output_sample->RemoveAllBuffers();

        if (mft.Get())
        {
            // mft->SetInputType(input_stream_id, nullptr, 0);
            // mft->SetOutputType(output_stream_id, nullptr, 0);

            if (vctx_out)
                mft->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)0);
        }

        for (size_t i=0; i < ARRAY_SIZE(cachedSRV); i++)
        {
            for (size_t j=0; j < ARRAY_SIZE(cachedSRV[i]); j++)
            {
                if (cachedSRV[i][j] != nullptr)
                    cachedSRV[i][j]->Release();
            }
        }

        if (vctx_out && dxgi_manager.Get())
        {
            if (d3d_handle != INVALID_HANDLE_VALUE)
                dxgi_manager->CloseDeviceHandle(d3d_handle);
        }

        if (vctx_out)
            vlc_video_context_Release(vctx_out);

        delete this;

        MFShutdown();
    }

    bool streamStarted = false;
};

struct mf_d3d11_pic_ctx
{
    struct d3d11_pic_context ctx;
    IMFMediaBuffer *out_media;
    mft_dec_sys_t  *mfdec;
};
#define MF_D3D11_PICCONTEXT_FROM_PICCTX(pic_ctx)  \
    container_of(pic_ctx, mf_d3d11_pic_ctx, ctx.s)

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

DEFINE_MEDIATYPE_GUID (vlc_MFVideoFormat_AV1, FCC('AV01'));

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
    { VLC_CODEC_AV1,  &vlc_MFVideoFormat_AV1 },
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

static int SetInputType(decoder_t *p_dec, DWORD stream_id, ComPtr<IMFMediaType> & result)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    result.Reset();

    ComPtr<IMFMediaType> input_media_type;

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

        input_media_type.Reset();
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

    hr = p_sys->mft->SetInputType(stream_id, input_media_type.Get(), 0);
    if (FAILED(hr))
        goto error;

    result.Swap(input_media_type);

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in SetInputType()");
    return VLC_EGENERIC;
}

static int SetOutputType(decoder_t *p_dec, DWORD stream_id)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    ComPtr<IMFMediaType> output_media_type;

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

        output_media_type.Reset();
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

    hr = p_sys->mft->SetOutputType(stream_id, output_media_type.Get(), 0);
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
    return VLC_EGENERIC;
}

static int AllocateInputSample(decoder_t *p_dec, DWORD stream_id, ComPtr<IMFSample> & result, DWORD size)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    result.Reset();

    ComPtr<IMFSample> input_sample;
    ComPtr<IMFMediaBuffer> input_media_buffer;
    DWORD allocation_size;

    MFT_INPUT_STREAM_INFO input_info;
    hr = p_sys->mft->GetInputStreamInfo(stream_id, &input_info);
    if (FAILED(hr))
        goto error;

    hr = MFCreateSample(&input_sample);
    if (FAILED(hr))
        goto error;

    allocation_size = std::max<DWORD>(input_info.cbSize, size);
    hr = MFCreateMemoryBuffer(allocation_size, &input_media_buffer);
    if (FAILED(hr))
        goto error;

    hr = input_sample->AddBuffer(input_media_buffer.Get());
    if (FAILED(hr))
        goto error;

    result.Swap(input_sample);

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in AllocateInputSample()");
    return VLC_EGENERIC;
}

static int AllocateOutputSample(decoder_t *p_dec, DWORD stream_id, ComPtr<IMFSample> & result)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    result.Reset();

    ComPtr<IMFSample> output_sample;

    MFT_OUTPUT_STREAM_INFO output_info;
    ComPtr<IMFMediaBuffer> output_media_buffer;
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

    hr = output_sample->AddBuffer(output_media_buffer.Get());
    if (FAILED(hr))
        goto error;

    result.Swap(output_sample);

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in AllocateOutputSample()");
    return VLC_EGENERIC;
}

static int ProcessInputStream(decoder_t *p_dec, DWORD stream_id, block_t *p_block)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
    HRESULT hr = S_OK;
    ComPtr<IMFSample> input_sample;

    block_t *p_xps_blocks = NULL;
    DWORD alloc_size = p_block->i_buffer;
    vlc_tick_t ts;
    ComPtr<IMFMediaBuffer> input_media_buffer;

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

    if (AllocateInputSample(p_dec, stream_id, input_sample, alloc_size))
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

    hr = p_sys->mft->ProcessInput(stream_id, input_sample.Get(), 0);
    if (FAILED(hr))
    {
        msg_Dbg(p_dec, "Failed to process input stream %lu (error 0x%lX)", stream_id, hr);
        goto error;
    }

    block_ChainRelease(p_xps_blocks);

    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in ProcessInputStream(). (hr=0x%lX)\n", hr);
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

static void d3d11mf_pic_context_destroy(picture_context_t *ctx)
{
    mf_d3d11_pic_ctx *pic_ctx = MF_D3D11_PICCONTEXT_FROM_PICCTX(ctx);
    mft_dec_sys_t *mfdec = pic_ctx->mfdec;
    pic_ctx->out_media->Release();
    static_assert(offsetof(mf_d3d11_pic_ctx, ctx.s) == 0, "Cast assumption failure");
    d3d11_pic_context_destroy(ctx);
    mfdec->Release();
}

static picture_context_t *d3d11mf_pic_context_copy(picture_context_t *ctx)
{
    mf_d3d11_pic_ctx *src_ctx = MF_D3D11_PICCONTEXT_FROM_PICCTX(ctx);
    mf_d3d11_pic_ctx *pic_ctx = static_cast<mf_d3d11_pic_ctx *>(malloc(sizeof(*pic_ctx)));
    if (unlikely(pic_ctx==nullptr))
        return nullptr;
    *pic_ctx = *src_ctx;
    vlc_video_context_Hold(pic_ctx->ctx.s.vctx);
    pic_ctx->out_media->AddRef();
    pic_ctx->mfdec->AddRef();
    for (int i=0;i<DXGI_MAX_SHADER_VIEW; i++)
    {
        pic_ctx->ctx.picsys.resource[i]  = src_ctx->ctx.picsys.resource[i];
        pic_ctx->ctx.picsys.renderSrc[i] = src_ctx->ctx.picsys.renderSrc[i];
    }
    AcquireD3D11PictureSys(&pic_ctx->ctx.picsys);
    return &pic_ctx->ctx.s;
}

static mf_d3d11_pic_ctx *CreatePicContext(ID3D11Texture2D *texture, UINT slice,
                                          ComPtr<IMFMediaBuffer> &media_buffer,
                                          mft_dec_sys_t *mfdec,
                                          ID3D11ShaderResourceView *renderSrc[DXGI_MAX_SHADER_VIEW],
                                          vlc_video_context *vctx)
{
    mf_d3d11_pic_ctx *pic_ctx = static_cast<mf_d3d11_pic_ctx *>(calloc(1, sizeof(*pic_ctx)));
    if (unlikely(pic_ctx==nullptr))
        return nullptr;

    media_buffer.CopyTo(&pic_ctx->out_media);
    pic_ctx->mfdec = mfdec;
    pic_ctx->mfdec->AddRef();

    pic_ctx->ctx.s.copy = d3d11mf_pic_context_copy;
    pic_ctx->ctx.s.destroy = d3d11mf_pic_context_destroy;
    pic_ctx->ctx.s.vctx = vlc_video_context_Hold(vctx);

    pic_ctx->ctx.picsys.slice_index = slice;
    for (int i=0;i<DXGI_MAX_SHADER_VIEW; i++)
    {
        pic_ctx->ctx.picsys.texture[i] = texture;
        pic_ctx->ctx.picsys.renderSrc[i] = renderSrc ? renderSrc[i] : NULL;
    }
    AcquireD3D11PictureSys(&pic_ctx->ctx.picsys);
    return pic_ctx;
}

static int ProcessOutputStream(decoder_t *p_dec, DWORD stream_id, bool & keep_reading)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    DWORD output_status = 0;
    MFT_OUTPUT_DATA_BUFFER output_buffer = { stream_id, p_sys->output_sample.Get(), 0, NULL };
    hr = p_sys->mft->ProcessOutput(0, 1, &output_buffer, &output_status);
    if (output_buffer.pEvents)
        output_buffer.pEvents->Release();

    keep_reading = false;
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
        return VLC_SUCCESS;

    if (hr == MF_E_TRANSFORM_STREAM_CHANGE || hr == MF_E_TRANSFORM_TYPE_NOT_SET)
    {
        if (SetOutputType(p_dec, p_sys->output_stream_id))
            return VLC_EGENERIC;

        /* Reallocate output sample. */
        if (AllocateOutputSample(p_dec, p_sys->output_stream_id, p_sys->output_sample))
            return VLC_EGENERIC;
        // there's an output ready, keep trying
        keep_reading = hr == MF_E_TRANSFORM_STREAM_CHANGE;
        return VLC_SUCCESS;
    }

    /* An error not listed above occurred */
    if (FAILED(hr))
    {
        msg_Dbg(p_dec, "Failed to process output stream %lu (error 0x%lX)", stream_id, hr);
        return VLC_EGENERIC;
    }

    if (output_buffer.pSample == nullptr)
        return VLC_SUCCESS;

    LONGLONG sample_time;
    hr = output_buffer.pSample->GetSampleTime(&sample_time);
    if (FAILED(hr))
        return VLC_EGENERIC;
    /* Convert from 100 nanoseconds unit to vlc ticks. */
    vlc_tick_t samp_time = VLC_TICK_FROM_MSFTIME(sample_time);

    DWORD output_count = 0;
    hr = output_buffer.pSample->GetBufferCount(&output_count);
    if (unlikely(FAILED(hr)))
        return VLC_EGENERIC;

    ComPtr<IMFSample> output_sample = output_buffer.pSample;

    for (DWORD buf_index = 0; buf_index < output_count; buf_index++)
    {
        picture_t *picture = NULL;
        ComPtr<IMFMediaBuffer> output_media_buffer;
        hr = output_sample->GetBufferByIndex(buf_index, &output_media_buffer);
        if (FAILED(hr))
            goto error;

        if (p_dec->fmt_in.i_cat == VIDEO_ES)
        {
            mf_d3d11_pic_ctx *pic_ctx = nullptr;
            UINT sliceIndex = 0;
            ComPtr<IMFDXGIBuffer> spDXGIBuffer;
            hr = output_media_buffer.As(&spDXGIBuffer);
            if (SUCCEEDED(hr))
            {
                ID3D11Texture2D *d3d11Res;
                hr = spDXGIBuffer->GetResource(IID_PPV_ARGS(&d3d11Res));
                if (SUCCEEDED(hr))
                {
                    D3D11_TEXTURE2D_DESC desc;
                    d3d11Res->GetDesc(&desc);

                    hr = spDXGIBuffer->GetSubresourceIndex(&sliceIndex);
                    if (!p_sys->vctx_out)
                    {
                        vlc_decoder_device *dec_dev = decoder_GetDecoderDevice(p_dec);
                        d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueDevice(dec_dev);
                        if (dev_sys != NULL)
                        {
                            p_sys->vctx_out = D3D11CreateVideoContext( dec_dev, desc.Format );
                            vlc_decoder_device_Release(dec_dev);
                            if (unlikely(p_sys->vctx_out == NULL))
                            {
                                msg_Err(p_dec, "failed to create a video context");
                                d3d11Res->Release();
                                return VLC_EGENERIC;
                            }
                            p_dec->fmt_out.video.i_width = desc.Width;
                            p_dec->fmt_out.video.i_height = desc.Height;

                            for (const d3d_format_t *output_format = DxgiGetRenderFormatList();
                                    output_format->name != NULL; ++output_format)
                            {
                                if (output_format->formatTexture == desc.Format &&
                                    is_d3d11_opaque(output_format->fourcc))
                                {
                                    p_sys->cfg = output_format;
                                    break;
                                }
                            }

                            p_dec->fmt_out.i_codec = p_sys->cfg->fourcc;
                            p_dec->fmt_out.video.i_chroma = p_sys->cfg->fourcc;

                            // pre allocate all the SRV for that texture
                            for (size_t slice=0; slice < desc.ArraySize; slice++)
                            {
                                ID3D11Texture2D *tex[DXGI_MAX_SHADER_VIEW] = {
                                    d3d11Res, d3d11Res, d3d11Res, d3d11Res
                                };

                                if (D3D11_AllocateResourceView(vlc_object_logger(p_dec), dev_sys->d3d_dev.d3ddevice, p_sys->cfg,
                                                                tex, slice, p_sys->cachedSRV[slice]) != VLC_SUCCESS)
                                {
                                    d3d11Res->Release();
                                    goto error;
                                }
                            }
                            p_sys->cached_tex = d3d11Res;
                        }
                    }
                    else if (desc.ArraySize == 1)
                    {
                        assert(sliceIndex == 0);

                        d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueContext(p_sys->vctx_out);

                        ID3D11Texture2D *tex[DXGI_MAX_SHADER_VIEW] = {
                            d3d11Res, d3d11Res, d3d11Res, d3d11Res
                        };

                        for (size_t j=0; j < ARRAY_SIZE(p_sys->cachedSRV[sliceIndex]); j++)
                        {
                            if (p_sys->cachedSRV[sliceIndex][j] != nullptr)
                                p_sys->cachedSRV[sliceIndex][j]->Release();
                        }

                        if (D3D11_AllocateResourceView(vlc_object_logger(p_dec), dev_sys->d3d_dev.d3ddevice, p_sys->cfg,
                                                       tex, sliceIndex, p_sys->cachedSRV[sliceIndex]) != VLC_SUCCESS)
                        {
                            d3d11Res->Release();
                            goto error;
                        }
                    }
                    else if (p_sys->cached_tex.Get() != d3d11Res)
                    {
                        msg_Err(p_dec, "separate texture not supported");
                        d3d11Res->Release();
                        goto error;
                    }

                    pic_ctx = CreatePicContext(d3d11Res, sliceIndex, output_media_buffer, p_sys, p_sys->cachedSRV[sliceIndex], p_sys->vctx_out);
                    d3d11Res->Release();

                    if (unlikely(pic_ctx == nullptr))
                        goto error;
                }
            }

            if (decoder_UpdateVideoOutput(p_dec, p_sys->vctx_out))
            {
                if (pic_ctx)
                    d3d11mf_pic_context_destroy(&pic_ctx->ctx.s);
                return VLC_EGENERIC;
            }

            picture = decoder_NewPicture(p_dec);
            if (!picture)
            {
                if (pic_ctx)
                    d3d11mf_pic_context_destroy(&pic_ctx->ctx.s);
                return VLC_EGENERIC;
            }

            UINT32 interlaced = FALSE;
            hr = output_sample->GetUINT32(MFSampleExtension_Interlaced, &interlaced);
            if (FAILED(hr))
                picture->b_progressive = true;
            else
                picture->b_progressive = !interlaced;

            picture->date = samp_time;

            if (pic_ctx)
            {
                picture->context = &pic_ctx->ctx.s;
            }
            else
            {
            BYTE *buffer_start;
            hr = output_media_buffer->Lock(&buffer_start, NULL, NULL);
            if (FAILED(hr))
            {
                picture_Release(picture);
                goto error;
            }

            CopyPackedBufferToPicture(picture, buffer_start);

            hr = output_media_buffer->Unlock();
            if (FAILED(hr))
            {
                picture_Release(picture);
                goto error;
            }
            }

            decoder_QueueVideo(p_dec, picture);
        }
        else
        {
            block_t *aout_buffer = NULL;
            if (decoder_UpdateAudioFormat(p_dec))
                goto error;
            if (p_dec->fmt_out.audio.i_bitspersample == 0 || p_dec->fmt_out.audio.i_channels == 0)
                goto error;

            DWORD total_length = 0;
            hr = output_media_buffer->GetCurrentLength(&total_length);
            if (FAILED(hr))
                goto error;

            int samples = total_length / (p_dec->fmt_out.audio.i_bitspersample * p_dec->fmt_out.audio.i_channels / 8);
            aout_buffer = decoder_NewAudioBuffer(p_dec, samples);
            if (!aout_buffer)
                return VLC_SUCCESS;
            if (aout_buffer->i_buffer < total_length)
            {
                block_Release(aout_buffer);
                goto error;
            }

            aout_buffer->i_pts = samp_time;

            BYTE *buffer_start;
            hr = output_media_buffer->Lock(&buffer_start, NULL, NULL);
            if (FAILED(hr))
            {
                block_Release(aout_buffer);
                goto error;
            }

            memcpy(aout_buffer->p_buffer, buffer_start, total_length);

            hr = output_media_buffer->Unlock();
            if (FAILED(hr))
            {
                block_Release(aout_buffer);
                goto error;
            }

            decoder_QueueAudio(p_dec, aout_buffer);
        }

        if (p_sys->output_sample.Get())
        {
            /* Sample is not provided by the MFT: clear its content. */
            hr = output_media_buffer->SetCurrentLength(0);
            if (FAILED(hr))
                goto error;
        }
    }

    if (p_sys->output_sample.Get() == nullptr)
    {
        /* Sample is provided by the MFT: decrease refcount. */
        output_sample->Release();
    }

    keep_reading = true;
    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in ProcessOutputStream()");
    return VLC_EGENERIC;
}

static void Flush(decoder_t *p_dec)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
    if (SUCCEEDED(p_sys->flushStream()))
        p_sys->startStream();
}

static int DecodeSync(decoder_t *p_dec, block_t *p_block)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);

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
        {
            msg_Warn(p_dec, "draining failed (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
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
    if (p_block)
        block_Release(p_block);
    return VLCDEC_SUCCESS;
}

static HRESULT DequeueMediaEvent(decoder_t *p_dec)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    ComPtr<IMFMediaEvent> event;
    hr = p_sys->event_generator->GetEvent(MF_EVENT_FLAG_NO_WAIT, &event);
    if (FAILED(hr))
        return hr;
    MediaEventType event_type;
    hr = event->GetType(&event_type);
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
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
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

static int EnableHardwareAcceleration(decoder_t *p_dec, ComPtr<IMFAttributes> & attributes)
{
    HRESULT hr = S_OK;
#if defined(STATIC_CODECAPI_AVDecVideoAcceleration_H264)
    switch (p_dec->fmt_in.i_codec)
    {
        case VLC_CODEC_H264:
            hr = attributes->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, TRUE);
            break;
        case VLC_CODEC_WMV1:
        case VLC_CODEC_WMV2:
        case VLC_CODEC_WMV3:
        case VLC_CODEC_VC1:
            hr = attributes->SetUINT32(CODECAPI_AVDecVideoAcceleration_VC1, TRUE);
            break;
        case VLC_CODEC_MPGV:
        case VLC_CODEC_MP2V:
            hr = attributes->SetUINT32(CODECAPI_AVDecVideoAcceleration_MPEG2, TRUE);
            break;
        default:
            hr = S_OK;
            break;
    }
#else
    VLC_UNUSED(p_dec);
    VLC_UNUSED(attributes);
#endif // STATIC_CODECAPI_AVDecVideoAcceleration_H264

    return SUCCEEDED(hr) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void DestroyMFT(decoder_t *p_dec);

static int SetD3D11(decoder_t *p_dec, d3d11_device_t *d3d_dev)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
    HRESULT hr;
    hr = p_sys->fptr_MFCreateDXGIDeviceManager(&p_sys->dxgi_token, &p_sys->dxgi_manager);
    if (FAILED(hr))
        return VLC_EGENERIC;

    hr = p_sys->dxgi_manager->ResetDevice(d3d_dev->d3ddevice, p_sys->dxgi_token);
    if (FAILED(hr))
        return VLC_EGENERIC;

    hr = p_sys->dxgi_manager->OpenDeviceHandle(&p_sys->d3d_handle);
    if (FAILED(hr))
        return VLC_EGENERIC;

    hr = p_sys->mft->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)p_sys->dxgi_manager.Get());
    if (FAILED(hr))
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int InitializeMFT(decoder_t *p_dec)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    ComPtr<IMFAttributes> attributes;
    hr = p_sys->mft->GetAttributes(&attributes);
    if (hr != E_NOTIMPL && FAILED(hr))
        goto error;
    if (SUCCEEDED(hr))
    {
        UINT32 is_async = FALSE;
        hr = attributes->GetUINT32(MF_TRANSFORM_ASYNC, &is_async);
        if (hr != MF_E_ATTRIBUTENOTFOUND && FAILED(hr))
            goto error;
        p_sys->is_async = is_async;
        if (p_sys->is_async)
        {
            hr = attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
            if (FAILED(hr))
                goto error;
            hr = p_sys->mft.As(&p_sys->event_generator);
            if (FAILED(hr))
                goto error;
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

    if (SetInputType(p_dec, p_sys->input_stream_id, p_sys->input_type))
        goto error;

    if (attributes.Get() && p_dec->fmt_in.i_cat == VIDEO_ES)
    {
        EnableHardwareAcceleration(p_dec, attributes);
        if (p_sys->fptr_MFCreateDXGIDeviceManager)
        {
            vlc_decoder_device *dec_dev = decoder_GetDecoderDevice(p_dec);
            if (dec_dev != nullptr)
            {
                d3d11_decoder_device_t *devsys11 = GetD3D11OpaqueDevice(dec_dev);
                if (devsys11 != nullptr)
                {
                    UINT32 can_d3d11;
                    hr = attributes->GetUINT32(MF_SA_D3D11_AWARE, &can_d3d11);
                    if (SUCCEEDED(hr) && can_d3d11)
                    {
                        SetD3D11(p_dec, &devsys11->d3d_dev);

                        IMFAttributes *outputAttr = NULL;
                        hr = p_sys->mft->GetOutputStreamAttributes(p_sys->output_stream_id, &outputAttr);
                        if (SUCCEEDED(hr))
                        {
                            hr = outputAttr->SetUINT32(MF_SA_D3D11_BINDFLAGS, D3D11_BIND_SHADER_RESOURCE);
                        }
                    }
                }
                vlc_decoder_device_Release(dec_dev);
            }
        }
    }

    if (SetOutputType(p_dec, p_sys->output_stream_id))
        goto error;

    /*
     * The input type was not set by the previous call to
     * SetInputType, try again after setting the output type.
     */
    if (p_sys->input_type.Get() == nullptr)
        if (SetInputType(p_dec, p_sys->input_stream_id, p_sys->input_type) || p_sys->input_type.Get() == nullptr)
            goto error;

    /* This event is required for asynchronous MFTs, optional otherwise. */
    hr = p_sys->startStream();
    if (FAILED(hr))
        goto error;

    if (attributes.Get() && p_dec->fmt_in.i_codec == VLC_CODEC_H264)
    {
        /* It's not an error if the following call fails. */
#if (_WIN32_WINNT < _WIN32_WINNT_WIN8)
        attributes->SetUINT32(CODECAPI_AVLowLatencyMode, TRUE);
#else
        attributes->SetUINT32(MF_LOW_LATENCY, TRUE);
#endif

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
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);

    if (p_sys->mft.Get())
    {
        p_sys->endStream();

        if (p_sys->output_sample.Get() == nullptr)
        {
            // the MFT produces the output and may still have some left, we need to drain them
            HRESULT hr;
            hr = p_sys->mft->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
            if (FAILED(hr))
            {
                msg_Warn(p_dec, "exit draining failed (hr=0x%lX)", hr);
            }
            else
            {
                for (;;)
                {
                    DWORD output_status = 0;
                    MFT_OUTPUT_DATA_BUFFER output_buffer = { p_sys->output_stream_id, p_sys->output_sample.Get(), 0, NULL };
                    hr = p_sys->mft->ProcessOutput(0, 1, &output_buffer, &output_status);
                    if (output_buffer.pEvents)
                        output_buffer.pEvents->Release();
                    if (output_buffer.pSample)
                    {
                        output_buffer.pSample->Release();
                    }
                    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
                        break;
                    if (hr == MF_E_TRANSFORM_TYPE_NOT_SET)
                        break;
                }
            }
        }

        // make sure don't have any input pending
        p_sys->flushStream();
    }

    if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
        hxxx_helper_clean(&p_sys->hh);
}

static int FindMFT(decoder_t *p_dec)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);
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
        hr = activate_objects[i]->ActivateObject(__uuidof(p_sys->mft.Get()), &pv);
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

static int LoadMFTLibrary(decoder_t *p_dec)
{
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr))
        return VLC_EGENERIC;

    if (p_dec->fmt_in.i_cat != VIDEO_ES) // nothing left to do
        return VLC_SUCCESS;

    if (p_dec->fmt_in.video.i_width == 0) // don't consume D3D resource for a fake decoder
    {
        msg_Dbg(p_dec, "skip D3D handling for dummy decoder");
        return VLC_SUCCESS;
    }

#if _WIN32_WINNT < _WIN32_WINNT_WIN8
    HINSTANCE mfplat_dll = LoadLibrary(TEXT("mfplat.dll"));
    if (mfplat_dll)
    {
        p_sys->fptr_MFCreateDXGIDeviceManager =  reinterpret_cast<pf_MFCreateDXGIDeviceManager>(
            GetProcAddress(mfplat_dll, "MFCreateDXGIDeviceManager") );
        // we still have the DLL automatically loaded after this
        FreeLibrary(mfplat_dll);
    }
#else // Win8+
    p_sys->fptr_MFCreateDXGIDeviceManager = &MFCreateDXGIDeviceManager;
#endif // Win8+

    return VLC_SUCCESS;
}

static int Open(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    mft_dec_sys_t *p_sys;

    p_sys = new (std::nothrow) mft_dec_sys_t();
    if (unlikely(p_sys == nullptr))
        return VLC_ENOMEM;
    p_dec->p_sys = p_sys;

    if( FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)) )
    {
        delete p_sys;
        return VLC_EGENERIC;
    }

    if (LoadMFTLibrary(p_dec))
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
    if (AllocateOutputSample(p_dec, p_sys->output_stream_id, p_sys->output_sample))
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
    mft_dec_sys_t *p_sys = static_cast<mft_dec_sys_t*>(p_dec->p_sys);

    DestroyMFT(p_dec);

    p_sys->Release();

    CoUninitialize();
}
