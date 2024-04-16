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

#ifndef _MSC_VER // including mfapi with mingw-w64 is not clean for UWP yet
#include <winapifamily.h>
#undef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <cassert>
#include <vector>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_aout.h>
extern "C" {
#include "hxxx_helper.h"
}

#include "mft_d3d.h"
#include "mft_d3d11.h"

#include <initguid.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mferror.h>
#include <mfobjects.h>
#include <codecapi.h>
#include <mfidl.h>


#if !defined(CODECAPI_AVDecVideoAcceleration_H264) // MINGW < 8.0.1
DEFINE_CODECAPI_GUID(AVDecVideoAcceleration_H264,  "f7db8a2f-4f48-4ee8-ae31-8b6ebe558ae2", 0xf7db8a2f, 0x4f48, 0x4ee8, 0xae, 0x31, 0x8b, 0x6e, 0xbe, 0x55, 0x8a, 0xe2)
DEFINE_CODECAPI_GUID(AVDecVideoAcceleration_VC1,   "f7db8a30-4f48-4ee8-ae31-8b6ebe558ae2", 0xf7db8a30, 0x4f48, 0x4ee8, 0xae, 0x31, 0x8b, 0x6e, 0xbe, 0x55, 0x8a, 0xe2)
DEFINE_CODECAPI_GUID(AVDecVideoAcceleration_MPEG2, "f7db8a2e-4f48-4ee8-ae31-8b6ebe558ae2", 0xf7db8a2e, 0x4f48, 0x4ee8, 0xae, 0x31, 0x8b, 0x6e, 0xbe, 0x55, 0x8a, 0xe2)
#define CODECAPI_AVDecVideoAcceleration_H264  DEFINE_CODECAPI_GUIDNAMED(AVDecVideoAcceleration_H264)
#define CODECAPI_AVDecVideoAcceleration_VC1   DEFINE_CODECAPI_GUIDNAMED(AVDecVideoAcceleration_VC1)
#define CODECAPI_AVDecVideoAcceleration_MPEG2 DEFINE_CODECAPI_GUIDNAMED(AVDecVideoAcceleration_MPEG2)
#endif


#include <vlc_codecs.h> // wf_tag_to_fourcc

#include <algorithm>
#include <atomic>
#include <new>
#include <memory>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);
static int OpenMFTAudioEncoder(vlc_object_t *);
static int OpenMFTVideoEncoder(vlc_object_t *);

#define MFT_DEBUG_TEXT N_("Extra MFT Debug")
#define MFT_DEBUG_LONGTEXT N_( "Show more MediaFoundation debug info, may be slower to load" )

vlc_module_begin()
    set_description(N_("Media Foundation Transform decoder"))
    add_shortcut("mft")
    set_capability("video decoder", 1)
    set_callbacks(Open, Close)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    add_bool("mft-debug", false, MFT_DEBUG_TEXT, MFT_DEBUG_LONGTEXT)

    add_submodule()
    add_shortcut("mft")
    set_capability("audio decoder", 1)
    set_callbacks(Open, Close)

#ifdef ENABLE_SOUT
    add_submodule()
    add_shortcut("mft")
    set_capability("audio encoder", 10) // less than DMO for now
    set_callback(OpenMFTAudioEncoder)

    add_submodule()
    add_shortcut("mft")
    set_capability("video encoder", 10) // less than DMO for now
    set_callback(OpenMFTVideoEncoder)
#endif
vlc_module_end()

class mft_sys_t : public vlc_mft_ref
{
public:
    ComPtr<IMFTransform> mft;

    virtual ~mft_sys_t()
    {
        assert(!streamStarted);

        event_generator.Reset();
        mft.Reset();
        input_type.Reset();
        output_sample.Reset();

        MFShutdown();

        CoUninitialize();
    }

    virtual bool IsEncoder() const = 0;

    /* For asynchronous MFT */
    bool is_async = false;
    ComPtr<IMFMediaEventGenerator> event_generator;
    int pending_input_events = 0;
    int pending_output_events = 0;
    int pending_drain_events = 0;
    HRESULT DequeueMediaEvent(vlc_logger *, bool wait = false);

    /* Input stream */
    DWORD input_stream_id = 0;
    ComPtr<IMFMediaType> input_type;

    /* Output stream */
    DWORD output_stream_id = 0;
    ComPtr<IMFSample> output_sample;

    HRESULT AllocateOutputSample(es_format_category_e cat, ComPtr<IMFSample> & result);
    HRESULT SetOutputType(vlc_logger *, const GUID & req_subtype, es_format_t & fmt_out);
    HRESULT SetInputType(const es_format_t & fmt_in, const MFT_REGISTER_TYPE_INFO &);

    virtual void DoRelease() = 0;

    void AddRef() final
    {
        refcount++;
    }

    bool Release() final
    {
        if (--refcount == 0)
        {
            if (output_sample.Get())
                output_sample->RemoveAllBuffers();

            DoRelease();
            delete this;
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

    /// Required for Async MFTs
    HRESULT shutdownStream()
    {
        ComPtr<IMFShutdown> shutdownObj;
        HRESULT hr = mft.As(&shutdownObj);
        if (FAILED(hr))
            return hr;
        return shutdownObj->Shutdown();
    }

private:
    bool streamStarted = false;
    std::atomic<size_t>  refcount{1};
};

class mft_dec_audio : public mft_sys_t
{
public:
    virtual ~mft_dec_audio() = default;

    bool IsEncoder() const final { return false; }

protected:
    void DoRelease() override
    {
    }
};

/**
 * Generic class to handle Direct3D with MediaFoundation
 */
class vlc_mft_d3d : public mft_sys_t
{
public:
    vlc_mft_d3d() = default;
    virtual ~vlc_mft_d3d() = default;

    std::unique_ptr<MFHW_d3d> hw_d3d;

    HRESULT SetD3D(vlc_logger *, vlc_decoder_device &);

protected:
    void DoRelease() override
    {
        if (hw_d3d)
            hw_d3d->Release(mft);
    }
};

class mft_dec_video : public vlc_mft_d3d
{
public:
    mft_dec_video() = default;
    virtual ~mft_dec_video() = default;

    bool IsEncoder() const final { return false; }

    /* H264 only. */
    struct hxxx_helper hh = {};
    bool   b_xps_pushed = false; ///< (for xvcC) parameter sets pushed (SPS/PPS/VPS)
};

class mft_enc_audio : public mft_sys_t
{
protected:
    void DoRelease() override
    {
    }

    bool IsEncoder() const final { return true; }
};

class mft_enc_video : public vlc_mft_d3d
{
public:

    HRESULT ProcessInputPicture(struct vlc_logger *, picture_t *);
    HRESULT ProcessOutput(vlc_logger *, block_t * & output);

    bool IsEncoder() const final { return true; }

private:
    bool is_first_picture = true;
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
#ifndef MF_E_NO_EVENTS_AVAILABLE
# define MF_E_NO_EVENTS_AVAILABLE _HRESULT_TYPEDEF_(0xC00D3E80L)
#endif

typedef struct
{
    vlc_fourcc_t fourcc;
    const GUID   guid;
} pair_format_guid;

#if defined(__MINGW64_VERSION_MAJOR)
# if __MINGW64_VERSION_MAJOR < 10 && !defined(CODECAPI_AVDecVideoAcceleration_H264)
// 8-bit luminance only
// Older versions of mingw-w64 lack this GUID, but it was added in mingw-w64
// git on 2021-07-11 (during __MINGW64_VERSION_MAJOR 10). Use a local
// redefinition of this GUID with a custom prefix, to let the same code build
// with both older and newer versions of mingw-w64 (and earlier git snapshots
// with __MINGW64_VERSION_MAJOR == 10).
DEFINE_MEDIATYPE_GUID (MFVideoFormat_L8, 50); // D3DFMT_L8

DEFINE_MEDIATYPE_GUID (MFVideoFormat_AV1, FCC('AV01'));
# endif  // __MINGW64_VERSION_MAJOR >= 0
#endif // __MINGW64_VERSION_MAJOR

/*
 * We need this table since the FOURCC used for GUID is not the same
 * as the FOURCC used by VLC, for instance h264 vs H264.
 */
static const pair_format_guid video_codec_table[] =
{
    { VLC_CODEC_H264, MFVideoFormat_H264 },
    { VLC_CODEC_MPGV, MFVideoFormat_MPEG2 },
    { VLC_CODEC_MP2V, MFVideoFormat_MPEG2 },
    { VLC_CODEC_MP1V, MFVideoFormat_MPG1 },
    { VLC_CODEC_MJPG, MFVideoFormat_MJPG },
    { VLC_CODEC_WMV1, MFVideoFormat_WMV1 },
    { VLC_CODEC_WMV2, MFVideoFormat_WMV2 },
    { VLC_CODEC_WMV3, MFVideoFormat_WMV3 },
    { VLC_CODEC_VC1,  MFVideoFormat_WVC1 },
    { VLC_CODEC_AV1,  MFVideoFormat_AV1 },

    { 0, GUID_NULL }
};

/*
 * Table to map MF Transform raw chroma output formats to native VLC FourCC
 */
static const pair_format_guid chroma_format_table[] = {
    { VLC_CODEC_NV12,  MFVideoFormat_NV12 },
    { VLC_CODEC_I420,  MFVideoFormat_I420 },
    { VLC_CODEC_YV12,  MFVideoFormat_YV12 },
    { VLC_CODEC_YV12,  MFVideoFormat_IYUV },
    { VLC_CODEC_YUYV,  MFVideoFormat_YUY2 },
    { VLC_CODEC_BGRX,  MFVideoFormat_RGB32  },
    { VLC_CODEC_BGR24, MFVideoFormat_RGB24  },
    { VLC_CODEC_BGRA,  MFVideoFormat_ARGB32 },
    { VLC_CODEC_GREY,  MFVideoFormat_L8 },

    { 0, GUID_NULL }
};

/*
 * We cannot use the FOURCC code for audio either since the
 * WAVE_FORMAT value is used to create the GUID.
 */
static const pair_format_guid audio_codec_table[] =
{
    { VLC_CODEC_MPGA, MFAudioFormat_MPEG      },
    { VLC_CODEC_MP3,  MFAudioFormat_MP3       },
    { VLC_CODEC_DTS,  MFAudioFormat_DTS       },
    { VLC_CODEC_MP4A, MFAudioFormat_AAC       },
    { VLC_CODEC_WMA2, MFAudioFormat_WMAudioV8 },
    { VLC_CODEC_A52,  MFAudioFormat_Dolby_AC3 },

    { 0, GUID_NULL }
};

static HRESULT MFTypeFromCodec(const GUID & type, vlc_fourcc_t codec, MFT_REGISTER_TYPE_INFO & info)
{
    const pair_format_guid *table = type == MFMediaType_Video ? video_codec_table : audio_codec_table;
    for (int i = 0; table[i].fourcc; ++i)
        if (table[i].fourcc == codec)
        {
            info.guidMajorType = type;
            info.guidSubtype = table[i].guid;
            return S_OK;
        }

    return E_INVALIDARG;
}

static vlc_fourcc_t MFFormatToChroma(const GUID & guid)
{
    for (int i = 0; chroma_format_table[i].fourcc; ++i)
        if (chroma_format_table[i].guid == guid)
            return chroma_format_table[i].fourcc;

    return 0;
}

static HRESULT MFTypeFromAudio(vlc_fourcc_t audio_format, MFT_REGISTER_TYPE_INFO & info)
{
    if (audio_format == VLC_CODEC_F32L)
    {
        info.guidMajorType = MFMediaType_Audio;
        info.guidSubtype = MFAudioFormat_Float;
        return S_OK;
    }
    if (audio_format == VLC_CODEC_S16L)
    {
        info.guidMajorType = MFMediaType_Audio;
        info.guidSubtype = MFAudioFormat_PCM;
        return S_OK;
    }

    return E_INVALIDARG;
}
static vlc_fourcc_t MFFormatToCodec(const pair_format_guid table[], const GUID & guid)
{
    for (int i = 0; table[i].fourcc; ++i)
        if (table[i].guid == guid)
            return table[i].fourcc;

    return 0;
}

static HRESULT MFTypeFromChroma(vlc_fourcc_t chroma, vlc_video_context *vctx, MFT_REGISTER_TYPE_INFO & info)
{
    if (vctx && vlc_video_context_GetType(vctx) == VLC_VIDEO_CONTEXT_D3D11VA)
    {
        assert(is_d3d11_opaque(chroma));
        auto *dev_sys = GetD3D11ContextPrivate(vctx);
        chroma = DxgiFormatFourcc(dev_sys->format);
        assert(chroma != 0);
    }
    for (int i = 0; chroma_format_table[i].fourcc; ++i)
        if (chroma_format_table[i].fourcc == chroma)
        {
            info.guidMajorType = MFMediaType_Video;
            info.guidSubtype = chroma_format_table[i].guid;
            return S_OK;
        }

    return E_INVALIDARG;
}

HRESULT mft_sys_t::SetInputType(const es_format_t & fmt_in, const MFT_REGISTER_TYPE_INFO & type)
{
    HRESULT hr;

    DWORD stream_id = input_stream_id;
    ComPtr<IMFMediaType> & result = input_type;

    result.Reset();

    ComPtr<IMFMediaType> input_media_type;

    /* Search a suitable input type for the MFT. */
    for (int i = 0;; ++i)
    {
        hr = mft->GetInputAvailableType(stream_id, i, input_media_type.ReleaseAndGetAddressOf());
        if (hr == MF_E_NO_MORE_TYPES)
            goto error;
        else if (hr == MF_E_TRANSFORM_TYPE_NOT_SET)
        {
            /* The output type must be set before setting the input type for this MFT. */
            return hr;
        }
        else if (FAILED(hr))
            goto error;

        GUID subtype;
        hr = input_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (FAILED(hr))
            goto error;

        if (subtype == type.guidSubtype)
            break;
    }

    if (fmt_in.i_cat == VIDEO_ES)
    {
        UINT32 width = fmt_in.video.i_visible_width;
        UINT32 height = fmt_in.video.i_visible_height;
        hr = MFSetAttributeSize(input_media_type.Get(), MF_MT_FRAME_SIZE, width, height);
        if (FAILED(hr))
            goto error;

        /* Some transforms like to know the frame rate and may reject the input type otherwise. */
        UINT32 frame_ratio_num = fmt_in.video.i_frame_rate;
        UINT32 frame_ratio_den = fmt_in.video.i_frame_rate_base;
        if(frame_ratio_num && frame_ratio_den) {
            hr = MFSetAttributeRatio(input_media_type.Get(), MF_MT_FRAME_RATE, frame_ratio_num, frame_ratio_den);
            if(FAILED(hr))
                goto error;
        }
    }
    else
    {
        hr = input_media_type->SetUINT32(MF_MT_ORIGINAL_WAVE_FORMAT_TAG, type.guidSubtype.Data1);
        if (FAILED(hr))
            goto error;
        if (fmt_in.audio.i_rate)
        {
            hr = input_media_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, fmt_in.audio.i_rate);
            if (FAILED(hr))
                goto error;
        }
        if (fmt_in.audio.i_channels)
        {
            hr = input_media_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, fmt_in.audio.i_channels);
            if (FAILED(hr))
                goto error;
        }
        if (fmt_in.audio.i_bitspersample)
        {
            hr = input_media_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, fmt_in.audio.i_bitspersample);
            if (FAILED(hr))
                goto error;
        }
        if (fmt_in.audio.i_blockalign)
        {
            hr = input_media_type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, fmt_in.audio.i_blockalign);
            if (FAILED(hr))
                goto error;
        }
        if (fmt_in.i_bitrate)
        {
            hr = input_media_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, fmt_in.i_bitrate / 8);
            if (FAILED(hr))
                goto error;
        }
    }

    if (fmt_in.i_extra > 0)
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
                                      static_cast<const UINT8*>(fmt_in.p_extra), fmt_in.i_extra);
            if (FAILED(hr))
                goto error;
        }
    }

    hr = mft->SetInputType(stream_id, input_media_type.Get(), 0);
    if (FAILED(hr))
        goto error;

    result.Swap(input_media_type);

error:
    return hr;
}

HRESULT mft_sys_t::SetOutputType(vlc_logger *logger,
                             const GUID & req_subtype, es_format_t & fmt_out)
{
    HRESULT hr;

    ComPtr<IMFMediaType> output_media_type;

    /*
     * Enumerate available output types. The list is ordered by
     * preference thus we will use the first one unless YV12/I420 is
     * available for video or float32 for audio.
     */
    GUID subtype;
    UINT32 best_bitrate = 0;
    int best_index = 0;
    int output_type_index = -1;
    for (int i = 0; output_type_index == -1; ++i)
    {
        hr = mft->GetOutputAvailableType(output_stream_id, i, output_media_type.ReleaseAndGetAddressOf());
        if (hr == MF_E_NO_MORE_TYPES)
        {
            /*
             * It's not an error if we don't find the output type we were
             * looking for, in this case we use the first available type.
             */
            /* No output format found we prefer, just pick the first one preferred
             * by the MFT */
            break;
        }
        else if (hr == MF_E_TRANSFORM_TYPE_NOT_SET)
        {
            /* The input type must be set before setting the output type for this MFT. */
            return hr;
        }
        else if (FAILED(hr))
            goto error;

        hr = output_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (FAILED(hr))
            goto error;

        if (req_subtype != GUID_NULL)
        {
            if (subtype != req_subtype)
                continue;
        }

        if (fmt_out.i_cat == VIDEO_ES)
        {
            if (IsEncoder())
            {
                if (MFFormatToCodec(video_codec_table, subtype) != 0)
                    output_type_index = i;
            }
            else
            {
                if(MFFormatToChroma(subtype) != 0)
                    output_type_index = i;
            }
        }
        else
        {
            if (req_subtype == GUID_NULL)
            {
                UINT32 bits_per_sample;
                hr = output_media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits_per_sample);
                if (FAILED(hr))
                    continue;
                if (bits_per_sample == 32 && subtype == MFAudioFormat_Float)
                    output_type_index = i;
                if (bits_per_sample == 16 && subtype == MFAudioFormat_PCM)
                    output_type_index = i;
                continue;
            }

            UINT32 rate = 0;
            hr = output_media_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
            if (FAILED(hr) || rate != fmt_out.audio.i_rate)
                continue;
            UINT32 channels = 0;
            hr = output_media_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
            if (FAILED(hr) || channels != fmt_out.audio.i_channels)
                continue;
            UINT32 abps = 0;
            hr = output_media_type->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &abps);
            if (FAILED(hr))
                continue;

            if (abps > best_bitrate && (fmt_out.i_bitrate == 0 || (8 * abps) <= fmt_out.i_bitrate))
            {
                best_bitrate = abps;
                best_index = i;
            }
        }
    }

    if (output_type_index == -1)
        output_type_index = best_index;

    hr = mft->GetOutputAvailableType(output_stream_id, output_type_index, output_media_type.ReleaseAndGetAddressOf());
    if (FAILED(hr))
        goto error;

    hr = output_media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr))
        goto error;

    if (fmt_out.i_cat == VIDEO_ES)
    {
        if (IsEncoder())
        {
            fmt_out.i_codec = MFFormatToCodec(video_codec_table, subtype);
        }
        else
        {
            /* Transform might offer output in a D3DFMT proprietary FCC */
            fmt_out.i_codec = MFFormatToChroma(subtype);
            if(!fmt_out.i_codec) {
                if (subtype == MFVideoFormat_IYUV)
                    subtype = MFVideoFormat_I420;
                fmt_out.i_codec = vlc_fourcc_GetCodec(VIDEO_ES, subtype.Data1);
            }
        }
    }

    if (fmt_out.i_cat == VIDEO_ES && IsEncoder())
    {
        if (fmt_out.i_bitrate != 0)
            fmt_out.i_bitrate = 1'000'000;
        output_media_type->SetUINT32(MF_MT_AVG_BITRATE, fmt_out.i_bitrate);

        if (fmt_out.video.i_frame_rate && fmt_out.video.i_frame_rate_base)
        {
            hr = MFSetAttributeRatio(output_media_type.Get(), MF_MT_FRAME_RATE,
                                     fmt_out.video.i_frame_rate,
                                     fmt_out.video.i_frame_rate_base);
        }

        hr = MFSetAttributeRatio(output_media_type.Get(), MF_MT_FRAME_SIZE,
                                    fmt_out.video.i_visible_width,
                                    fmt_out.video.i_visible_height);

        hr = MFSetAttributeRatio(output_media_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

        hr = output_media_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        hr = output_media_type->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, 0);
        hr = output_media_type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1);

        if (fmt_out.i_codec == VLC_CODEC_H264)
        {
            eAVEncH264VProfile profile;
#if (_WIN32_WINNT < _WIN32_WINNT_WIN8)
            bool isWin81OrGreater = false;
            HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
            if (likely(hKernel32 != NULL))
                isWin81OrGreater = GetProcAddress(hKernel32, "IsProcessCritical") != NULL;
            profile = isWin81OrGreater ? eAVEncH264VProfile_High : eAVEncH264VProfile_Main;
#else
            profile = eAVEncH264VProfile_High;
#endif
            hr = output_media_type->SetUINT32(MF_MT_MPEG2_PROFILE, profile);

            //hr = output_media_type->SetUINT32(MF_MT_MPEG2_LEVEL, eAVEncH264VLevel4);
        }

    }

    hr = mft->SetOutputType(output_stream_id, output_media_type.Get(), 0);
    if (FAILED(hr))
    {
        vlc_error(logger, "Failed to set the output. (hr=0x%lX)", hr);
        goto error;
    }

    mft->GetOutputCurrentType(output_stream_id, output_media_type.ReleaseAndGetAddressOf());

    if (fmt_out.i_cat == VIDEO_ES)
    {
        if (fmt_out.i_codec == VLC_CODEC_H264)
        {
            UINT32 blob_size = 0;
            hr = output_media_type->GetBlobSize(MF_MT_MPEG_SEQUENCE_HEADER, &blob_size);
            if (SUCCEEDED(hr) && blob_size != 0)
            {
                fmt_out.i_extra = blob_size;
                fmt_out.p_extra = malloc(fmt_out.i_extra);
                if (unlikely(fmt_out.p_extra == nullptr))
                    goto error;
                output_media_type->GetBlob(MF_MT_MPEG_SEQUENCE_HEADER,
                    static_cast<UINT8*>(fmt_out.p_extra), fmt_out.i_extra, nullptr);
            }
        }
    }
    else
    {
        UINT32 bitspersample = 0;
        hr = output_media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitspersample);
        if (SUCCEEDED(hr) && bitspersample)
            fmt_out.audio.i_bitspersample = bitspersample;

        UINT32 channels = 0;
        hr = output_media_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
        if (SUCCEEDED(hr) && channels)
            fmt_out.audio.i_channels = channels;

        UINT32 rate = 0;
        hr = output_media_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        if (SUCCEEDED(hr) && rate)
            fmt_out.audio.i_rate = rate;

        vlc_fourcc_t fourcc;
        wf_tag_to_fourcc(subtype.Data1, &fourcc, NULL);
        fmt_out.i_codec = vlc_fourcc_GetCodecAudio(fourcc, fmt_out.audio.i_bitspersample);

        fmt_out.audio.i_physical_channels = pi_channels_maps[fmt_out.audio.i_channels];

        if (fmt_out.i_codec == VLC_CODEC_MP4A)
        {
            UINT32 blob_size = 0;
            hr = output_media_type->GetBlobSize(MF_MT_USER_DATA, &blob_size);
            if (SUCCEEDED(hr))
            {
                size_t aac_info = sizeof(HEAACWAVEINFO) - sizeof(WAVEFORMATEX);
                if (blob_size > aac_info) // AudioSpecificConfig after HEAACWAVEINFO
                {
                    fmt_out.i_extra = blob_size - aac_info;
                    fmt_out.p_extra = malloc(fmt_out.i_extra);
                    if (unlikely(fmt_out.p_extra == nullptr))
                        goto error;
                    std::vector<UINT8> tmp(blob_size);
                    output_media_type->GetBlob(MF_MT_USER_DATA, tmp.data(), blob_size, nullptr);
                    memcpy(fmt_out.p_extra, &tmp[aac_info], fmt_out.i_extra);
                }
            }
        }
    }

    return S_OK;

error:
    vlc_error(logger, "Error in SetOutputType()");
    return hr;
}

static HRESULT AllocateInputSample(struct vlc_logger *logger, ComPtr<IMFTransform> & mft, DWORD stream_id, ComPtr<IMFSample> & result, DWORD size)
{
    HRESULT hr;

    result.Reset();

    ComPtr<IMFSample> input_sample;
    ComPtr<IMFMediaBuffer> input_media_buffer;
    DWORD allocation_size;

    MFT_INPUT_STREAM_INFO input_info;
    hr = mft->GetInputStreamInfo(stream_id, &input_info);
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

    return hr;

error:
    vlc_error(logger, "Error in AllocateInputSample(). (hr=0x%lX)", hr);
    return hr;
}

HRESULT mft_sys_t::AllocateOutputSample(es_format_category_e cat, ComPtr<IMFSample> & result)
{
    HRESULT hr;

    result.Reset();

    ComPtr<IMFSample> output_sample;

    MFT_OUTPUT_STREAM_INFO output_info;
    ComPtr<IMFMediaBuffer> output_media_buffer;
    DWORD allocation_size;
    DWORD alignment;

    hr = mft->GetOutputStreamInfo(output_stream_id, &output_info);
    if (FAILED(hr))
        return hr;

    if (output_info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES))
    {
        /* The MFT will provide an allocated sample. */
        return S_FALSE;
    }

    if (cat == VIDEO_ES && !IsEncoder())
    {
        const DWORD expected_flags =
                          MFT_OUTPUT_STREAM_WHOLE_SAMPLES
                        | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER
                        | MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;
        if ((output_info.dwFlags & expected_flags) != expected_flags)
            return E_UNEXPECTED;
    }

    hr = MFCreateSample(&output_sample);
    if (FAILED(hr))
        return hr;

    allocation_size = output_info.cbSize;
    alignment = output_info.cbAlignment;
    if (alignment > 0)
        hr = MFCreateAlignedMemoryBuffer(allocation_size, alignment - 1, &output_media_buffer);
    else
        hr = MFCreateMemoryBuffer(allocation_size, &output_media_buffer);
    if (FAILED(hr))
        return hr;

    hr = output_sample->AddBuffer(output_media_buffer.Get());
    if (FAILED(hr))
        return hr;

    result.Swap(output_sample);

    return S_OK;
}

HRESULT mft_enc_video::ProcessInputPicture(vlc_logger *logger, picture_t *p_pic)
{
    HRESULT hr;
    ComPtr<IMFSample> input_sample;

    const vlc_chroma_description_t *p_chroma_desc = vlc_fourcc_GetChromaDescription( p_pic->format.i_chroma );
    if( !p_chroma_desc )
        return E_INVALIDARG;

    plane_t dst_planes[PICTURE_PLANE_MAX];
    DWORD alloc_size = 0;
    for( unsigned i = 0; i < p_chroma_desc->plane_count; i++ )
    {
        plane_t *p = &dst_planes[i];

        p->i_lines         =
        p->i_visible_lines = p_pic->format.i_visible_height * p_chroma_desc->p[i].h.num / p_chroma_desc->p[i].h.den;
        p->i_pitch         =
        p->i_visible_pitch = p_pic->format.i_visible_width * p_chroma_desc->p[i].w.num / p_chroma_desc->p[i].w.den * p_chroma_desc->pixel_size;
        alloc_size += p->i_pitch * p->i_lines;
    }

    vlc_tick_t ts;
    ComPtr<IMFMediaBuffer> input_media_buffer;
    UINT64 frame_ratio_num = p_pic->format.i_frame_rate;
    UINT64 frame_ratio_den = p_pic->format.i_frame_rate_base;

    hr = AllocateInputSample(logger, mft, input_stream_id, input_sample, alloc_size);
    if (FAILED(hr))
        goto error;

    hr = input_sample->GetBufferByIndex(0, &input_media_buffer);
    if (FAILED(hr))
        goto error;

    BYTE *buffer_start;
    hr = input_media_buffer->Lock(&buffer_start, NULL, NULL);
    if (FAILED(hr))
        goto error;

    alloc_size = 0;
    for( unsigned i = 0; i < p_chroma_desc->plane_count; i++ )
    {
        plane_t *p = &dst_planes[i];
        p->p_pixels = &buffer_start[alloc_size];
        plane_CopyPixels(p, &p_pic->p[i]);
        alloc_size += p->i_pitch * p->i_lines;
    }

    hr = input_media_buffer->Unlock();
    if (FAILED(hr))
        goto error;

    hr = input_media_buffer->SetCurrentLength(alloc_size);
    if (FAILED(hr))
        goto error;

    ts = p_pic->date;

    /* Convert from microseconds to 100 nanoseconds unit. */
    hr = input_sample->SetSampleTime(MSFTIME_FROM_VLC_TICK(ts));
    if (FAILED(hr))
        goto error;

    hr = input_sample->SetSampleDuration(INT64_C(10000000) * frame_ratio_den / frame_ratio_num);

    if (is_first_picture)
    {
        input_sample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
        is_first_picture = false;
    }

    hr = mft->ProcessInput(input_stream_id, input_sample.Get(), 0);
    if (FAILED(hr))
    {
        vlc_debug(logger, "Failed to process input stream %lu (error 0x%lX)", input_stream_id, hr);
        goto error;
    }

    return hr;

error:
    vlc_error(logger, "Error in ProcessInputStream(). (hr=0x%lX)", hr);
    return hr;
}

static int ProcessInputStream(struct vlc_logger *logger, ComPtr<IMFTransform> & mft, DWORD stream_id, block_t *p_block)
{
    HRESULT hr;
    ComPtr<IMFSample> input_sample;

    DWORD alloc_size = p_block->i_buffer;
    vlc_tick_t ts;
    ComPtr<IMFMediaBuffer> input_media_buffer;

    hr = AllocateInputSample(logger, mft, stream_id, input_sample, alloc_size);
    if (FAILED(hr))
        goto error;

    hr = input_sample->GetBufferByIndex(0, &input_media_buffer);
    if (FAILED(hr))
        goto error;

    BYTE *buffer_start;
    hr = input_media_buffer->Lock(&buffer_start, NULL, NULL);
    if (FAILED(hr))
        goto error;

    memcpy(buffer_start, p_block->p_buffer, alloc_size);

    hr = input_media_buffer->Unlock();
    if (FAILED(hr))
        goto error;

    hr = input_media_buffer->SetCurrentLength(alloc_size);
    if (FAILED(hr))
        goto error;

    ts = p_block->i_pts == VLC_TICK_INVALID ? p_block->i_dts : p_block->i_pts;

    /* Convert from microseconds to 100 nanoseconds unit. */
    hr = input_sample->SetSampleTime(MSFTIME_FROM_VLC_TICK(ts));
    if (FAILED(hr))
        goto error;

    hr = mft->ProcessInput(stream_id, input_sample.Get(), 0);
    if (FAILED(hr))
    {
        vlc_debug(logger, "Failed to process input stream %lu (error 0x%lX)", stream_id, hr);
        goto error;
    }

    return VLC_SUCCESS;

error:
    vlc_error(logger, "Error in ProcessInputStream(). (hr=0x%lX)", hr);
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
    auto *p_sys = static_cast<mft_sys_t*>(p_dec->p_sys);
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
        // there's an output ready, keep trying
        keep_reading = hr == MF_E_TRANSFORM_STREAM_CHANGE;

        if (p_dec->fmt_in->i_cat == VIDEO_ES)
            video_format_Copy( &p_dec->fmt_out.video, &p_dec->fmt_in->video );
        else
            p_dec->fmt_out.audio = p_dec->fmt_in->audio;
        hr = p_sys->SetOutputType(vlc_object_logger(p_dec), GUID_NULL, p_dec->fmt_out);
        if (FAILED(hr))
            return VLC_EGENERIC;

        /* Reallocate output sample. */
        hr = p_sys->AllocateOutputSample(p_dec->fmt_in->i_cat, p_sys->output_sample);
        if (FAILED(hr))
        {
            msg_Err(p_dec, "Error in AllocateOutputSample(). (hr=0x%lX)", hr);
            return VLC_EGENERIC;
        }
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

        if (p_dec->fmt_in->i_cat == VIDEO_ES)
        {
            auto *vidsys = dynamic_cast<vlc_mft_d3d*>(p_sys);
            picture_context_t *pic_ctx = nullptr;
            if (vidsys->hw_d3d != nullptr)
            {
                ComPtr<IMFDXGIBuffer> spDXGIBuffer;
                hr = output_media_buffer.As(&spDXGIBuffer);
                if (vidsys->hw_d3d->vctx_out == nullptr)
                {
                    hr = vidsys->hw_d3d->SetupVideoContext(vlc_object_logger(p_dec), spDXGIBuffer, p_dec->fmt_out);
                    if (FAILED(hr))
                        goto error;
                }

                pic_ctx = vidsys->hw_d3d->CreatePicContext(vlc_object_logger(p_dec), spDXGIBuffer, p_sys);
                if (unlikely(pic_ctx == nullptr))
                    goto error;
            }

            if (decoder_UpdateVideoOutput(p_dec, vidsys->hw_d3d ? vidsys->hw_d3d->vctx_out : nullptr))
            {
                if (pic_ctx)
                    pic_ctx->destroy(pic_ctx);

                return VLC_EGENERIC;
            }

            picture = decoder_NewPicture(p_dec);
            if (!picture)
            {
                if (pic_ctx)
                    pic_ctx->destroy(pic_ctx);
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
                picture->context = pic_ctx;
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
    auto *p_sys = static_cast<mft_sys_t*>(p_dec->p_sys);
    if (SUCCEEDED(p_sys->flushStream()))
        p_sys->startStream();
}

static int DecodeSync(decoder_t *p_dec, block_t *p_block)
{
    auto *p_sys = static_cast<mft_sys_t*>(p_dec->p_sys);

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
        if (p_dec->fmt_in->i_codec == VLC_CODEC_H264)
        {
            /* in-place NAL to annex B conversion. */
            auto *vidsys = dynamic_cast<mft_dec_video*>(p_sys);
            p_block = hxxx_helper_process_block(&vidsys->hh, p_block);

            if (vidsys->hh.i_input_nal_length_size && !vidsys->b_xps_pushed)
            {
                block_t *p_xps_blocks = hxxx_helper_get_extradata_block(&vidsys->hh);
                if (p_xps_blocks)
                {
                    size_t extrasize;
                    block_ChainProperties(p_xps_blocks, NULL, &extrasize, NULL);
                    if (ProcessInputStream(vlc_object_logger(p_dec), p_sys->mft, p_sys->input_stream_id, p_xps_blocks))
                    {
                        block_ChainRelease(p_xps_blocks);
                        goto error;
                    }
                    vidsys->b_xps_pushed = true;
                    block_ChainRelease(p_xps_blocks);
                }
            }
        }

        if (ProcessInputStream(vlc_object_logger(p_dec), p_sys->mft, p_sys->input_stream_id, p_block))
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

HRESULT mft_sys_t::DequeueMediaEvent(vlc_logger *logger, bool wait)
{
    HRESULT hr;

    ComPtr<IMFMediaEvent> event;
    hr = event_generator->GetEvent(wait ? 0 : MF_EVENT_FLAG_NO_WAIT, &event);
    if (FAILED(hr))
        return hr;
    MediaEventType event_type;
    hr = event->GetType(&event_type);
    if (FAILED(hr))
        return hr;

    if (event_type == METransformNeedInput)
        pending_input_events += 1;
    else if (event_type == METransformHaveOutput)
        pending_output_events += 1;
    else if (event_type == METransformDrainComplete)
        pending_drain_events += 1;
    else
        vlc_error(logger, "Unsupported asynchronous event %lu.", event_type);

    return S_OK;
}

static int DecodeAsync(decoder_t *p_dec, block_t *p_block)
{
    auto *p_sys = static_cast<mft_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    if (!p_block) /* No Drain */
        return VLCDEC_SUCCESS;

    if (p_block->i_flags & (BLOCK_FLAG_CORRUPTED))
    {
        block_Release(p_block);
        return VLCDEC_SUCCESS;
    }

    /* Dequeue all pending media events. */
    while ((hr = p_sys->DequeueMediaEvent(vlc_object_logger(p_dec))) == S_OK)
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
        hr = p_sys->DequeueMediaEvent(vlc_object_logger(p_dec));
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
    if (ProcessInputStream(vlc_object_logger(p_dec), p_sys->mft, p_sys->input_stream_id, p_block))
        goto error;

    block_Release(p_block);

    return VLCDEC_SUCCESS;

error:
    msg_Err(p_dec, "Error in DecodeAsync()");
    block_Release(p_block);
    return VLCDEC_SUCCESS;
}

static HRESULT EnableHardwareAcceleration(const es_format_t & fmt_in, ComPtr<IMFAttributes> & attributes)
{
    HRESULT hr;
    switch (fmt_in.i_codec)
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

    return hr;
}

static void DestroyMFT(decoder_t *p_dec);

HRESULT vlc_mft_d3d::SetD3D(vlc_logger *logger, vlc_decoder_device & dec_dev)
{
    HRESULT hr;
    assert(hw_d3d == nullptr);
    if (dec_dev.type == VLC_DECODER_DEVICE_D3D11VA)
    {
        ComPtr<IMFAttributes> attributes;
        hr = mft->GetAttributes(&attributes);
        if (SUCCEEDED(hr))
        {
            UINT32 can_d3d11;
            hr = attributes->GetUINT32(MF_SA_D3D11_AWARE, &can_d3d11);
            if (SUCCEEDED(hr) && can_d3d11)
                hw_d3d = std::make_unique<MFHW_d3d11>();
        }
    }

    if (unlikely(hw_d3d == nullptr))
        return E_ABORT;

    hr = hw_d3d->SetD3D(logger, dec_dev, mft);
    if (FAILED(hr))
        hw_d3d.reset();

    return hr;
}

static int InitializeMFT(decoder_t *p_dec, const MFT_REGISTER_TYPE_INFO & type)
{
    auto *p_sys = static_cast<mft_sys_t*>(p_dec->p_sys);
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

    hr = p_sys->SetInputType(*p_dec->fmt_in, type);
    if (FAILED(hr))
    {
        msg_Err(p_dec, "Error in SetInputType(). (hr=0x%lX)", hr);
        goto error;
    }

    if (attributes.Get() && p_dec->fmt_in->i_cat == VIDEO_ES)
    {
        EnableHardwareAcceleration(*p_dec->fmt_in, attributes);
        auto *vidsys = dynamic_cast<vlc_mft_d3d*>(p_sys);
        if (p_dec->fmt_in->video.i_width != 0 /*&& vidsys->d3d.CanUseD3D()*/)
        {
            vlc_decoder_device *dec_dev = decoder_GetDecoderDevice(p_dec);
            if (dec_dev != nullptr)
            {
                hr = vidsys->SetD3D(vlc_object_logger(p_dec), *dec_dev);
                if (SUCCEEDED(hr) && dec_dev->type == VLC_DECODER_DEVICE_D3D11VA)
                {
                    IMFAttributes *outputAttr = NULL;
                    hr = p_sys->mft->GetOutputStreamAttributes(p_sys->output_stream_id, &outputAttr);
                    if (SUCCEEDED(hr))
                    {
                        hr = outputAttr->SetUINT32(MF_SA_D3D11_BINDFLAGS, D3D11_BIND_SHADER_RESOURCE);
                    }
                }
                vlc_decoder_device_Release(dec_dev);
            }
        }
    }

    if (p_dec->fmt_in->i_cat == VIDEO_ES)
        video_format_Copy( &p_dec->fmt_out.video, &p_dec->fmt_in->video );
    else
        p_dec->fmt_out.audio = p_dec->fmt_in->audio;
    hr = p_sys->SetOutputType(vlc_object_logger(p_dec), GUID_NULL, p_dec->fmt_out);
    if (FAILED(hr))
        goto error;

    /*
     * The input type was not set by the previous call to
     * SetInputType, try again after setting the output type.
     */
    if (p_sys->input_type.Get() == nullptr)
    {
        hr = p_sys->SetInputType(*p_dec->fmt_in, type);
        if (FAILED(hr))
        {
            msg_Err(p_dec, "Error in SetInputType(). (hr=0x%lX)", hr);
            goto error;
        }
        if (p_sys->input_type.Get() == nullptr)
            goto error;
    }

    /* This event is required for asynchronous MFTs, optional otherwise. */
    hr = p_sys->startStream();
    if (FAILED(hr))
    {
        msg_Err(p_dec, "Error in startStream(). (hr=0x%lX)", hr);
        goto error;
    }

    if (p_dec->fmt_in->i_codec == VLC_CODEC_H264)
    {
        auto *vidsys = dynamic_cast<mft_dec_video*>(p_sys);
        hxxx_helper_init(&vidsys->hh, VLC_OBJECT(p_dec), p_dec->fmt_in->i_codec, 0, 0);
        hxxx_helper_set_extra(&vidsys->hh, p_dec->fmt_in->p_extra, p_dec->fmt_in->i_extra);
    }
    return VLC_SUCCESS;

error:
    msg_Err(p_dec, "Error in InitializeMFT()");
    DestroyMFT(p_dec);
    return VLC_EGENERIC;
}

static void DestroyMFT(decoder_t *p_dec)
{
    auto *p_sys = static_cast<mft_sys_t*>(p_dec->p_sys);

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

        p_sys->shutdownStream();
    }

    auto *vidsys = dynamic_cast<mft_dec_video*>(p_sys);
    if (vidsys && vidsys->hh.p_obj)
        hxxx_helper_clean(&vidsys->hh);
}

static int ListTransforms(struct vlc_logger *logger, GUID category, const char *type, bool do_test)
{
    HRESULT hr;

    UINT32 flags = MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_LOCALMFT
                 | MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT
                 | MFT_ENUM_FLAG_HARDWARE;
    IMFActivate **activate_objects = NULL;
    UINT32 count = 0;
    hr = MFTEnumEx(category, flags, nullptr, nullptr, &activate_objects, &count);
    vlc_debug(logger, "Listing %u %s%s", count, type, (count?"s":""));
    if (FAILED(hr))
        return VLC_EGENERIC;

    for (UINT32 o=0; o<count; o++)
    {
        WCHAR Name[256];
        hr = activate_objects[o]->GetString(MFT_FRIENDLY_NAME_Attribute, Name, ARRAY_SIZE(Name), nullptr);
        if (FAILED(hr))
            wcscpy(Name,L"<unknown>");

        if (do_test)
        {
            ComPtr<IMFTransform> mft;
            hr = activate_objects[o]->ActivateObject(IID_PPV_ARGS(mft.GetAddressOf()));
            bool available = SUCCEEDED(hr);

            const char *async = "";
            if (available)
            {
                ComPtr<IMFAttributes> attributes;
                hr = mft->GetAttributes(&attributes);
                if (SUCCEEDED(hr))
                {
                    UINT32 is_async = FALSE;
                    hr = attributes->GetUINT32(MF_TRANSFORM_ASYNC, &is_async);
                    if (SUCCEEDED(hr))
                        async = is_async ? " (async)" : " (sync)";
                }
            }

            vlc_debug(logger, "%s '%ls'%s is%s available", type, Name, async, available?"":" not");
        }
        else
            vlc_debug(logger, "found %s '%ls'", type, Name);

        activate_objects[o]->ShutdownObject();
    }

    return VLC_SUCCESS;
}

static int FindMFT(decoder_t *p_dec)
{
    auto *p_sys = static_cast<mft_sys_t*>(p_dec->p_sys);
    HRESULT hr;

    /* Try to create a MFT using MFTEnumEx. */
    GUID category;
    MFT_REGISTER_TYPE_INFO input_type;
    if (p_dec->fmt_in->i_cat == VIDEO_ES)
    {
        category = MFT_CATEGORY_VIDEO_DECODER;
        hr = MFTypeFromCodec(MFMediaType_Video, p_dec->fmt_in->i_codec, input_type);
        if(FAILED(hr)) {
            /* Codec is not well known. Construct a MF transform subtype from the fourcc */
            input_type.guidSubtype = MFVideoFormat_Base;
            input_type.guidSubtype.Data1 = p_dec->fmt_in->i_codec;
        }
    }
    else
    {
        category = MFT_CATEGORY_AUDIO_DECODER;
        hr = MFTypeFromCodec(MFMediaType_Audio, p_dec->fmt_in->i_codec, input_type);
        if(FAILED(hr))
            // not a codec, maybe a raw format
            hr = MFTypeFromAudio(p_dec->fmt_in->i_codec, input_type);
    }
    if (FAILED(hr))
        return VLC_EGENERIC;

    const char *dec_str;
    if (p_dec->fmt_in->i_cat == VIDEO_ES)
        dec_str = "video decoder";
    else
        dec_str = "audio decoder";
    ListTransforms(vlc_object_logger(p_dec), category, dec_str, var_InheritBool(p_dec, "mft-debug"));

    UINT32 flags = MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_LOCALMFT
                 | MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT
                 | MFT_ENUM_FLAG_HARDWARE;
    IMFActivate **activate_objects = NULL;
    UINT32 activate_objects_count = 0;
    hr = MFTEnumEx(category, flags, &input_type, NULL, &activate_objects, &activate_objects_count);
    if (FAILED(hr))
        return VLC_EGENERIC;

    msg_Dbg(p_dec, "Found %d available MFT module(s) for %4.4s", activate_objects_count, (const char*)&p_dec->fmt_in->i_codec);
    if (activate_objects_count == 0)
        return VLC_EGENERIC;

    for (UINT32 i = 0; i < activate_objects_count; ++i)
    {
        hr = activate_objects[i]->ActivateObject(IID_PPV_ARGS(p_sys->mft.ReleaseAndGetAddressOf()));
        activate_objects[i]->Release();
        if (FAILED(hr))
            continue;

        if (InitializeMFT(p_dec, input_type) == VLC_SUCCESS)
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

HRESULT mft_enc_video::ProcessOutput(vlc_logger *logger, block_t * & output)
{
    output = nullptr;
    HRESULT hr;
    while (!is_async || pending_output_events > 0)
    {
        ComPtr<IMFSample> output_sample;
        hr = AllocateOutputSample(VIDEO_ES, output_sample);
        if (FAILED(hr))
        {
            vlc_error(logger, "Error in AllocateOutputSample(). (hr=0x%lX)", hr);
            break;
        }

        DWORD output_status = 0;
        MFT_OUTPUT_DATA_BUFFER output_buffer = { output_stream_id, output_sample.Get(), 0, NULL };
        hr = mft->ProcessOutput(0, 1, &output_buffer, &output_status);
        if (output_buffer.pEvents)
            output_buffer.pEvents->Release();
        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
            break;
        pending_output_events--;

        if (FAILED(hr))
        {
            vlc_debug(logger, "Failed to process video output stream %lu (error 0x%lX)", output_stream_id, hr);
            return hr;
        }

        if (output_buffer.pSample == nullptr)
            break;

        LONGLONG sample_time;
        hr = output_buffer.pSample->GetSampleTime(&sample_time);
        if (FAILED(hr))
        {
            vlc_debug(logger, "Failed to get output time. (hr=0x%lX)", hr);
            // if (output_sample.Get() == nullptr)
            //     output_buffer.pSample->Release();
            return hr;
        }

        DWORD output_count = 0;
        hr = output_buffer.pSample->GetBufferCount(&output_count);
        if (unlikely(FAILED(hr)))
        {
            vlc_debug(logger, "Failed to get output buffer count. (hr=0x%lX)", hr);
            // if (output_sample.Get() == nullptr)
            //     output_buffer.pSample->Release();
            return hr;
        }

        for (DWORD buf_index = 0; buf_index < output_count; buf_index++)
        {
            ComPtr<IMFMediaBuffer> output_media_buffer;
            hr = output_buffer.pSample->GetBufferByIndex(buf_index, &output_media_buffer);
            if (FAILED(hr))
            {
                vlc_debug(logger, "Failed to get output buffer %lu. (hr=0x%lX)", buf_index, hr);
                // if (output_sample.Get() == nullptr)
                //     output_buffer.pSample->Release();
                return hr;
            }

            DWORD total_length = 0;
            hr = output_media_buffer->GetCurrentLength(&total_length);
            if (FAILED(hr))
            {
                vlc_debug(logger, "Failed to get output buffer %lu length. (hr=0x%lX)", buf_index, hr);
                // if (output_sample.Get() == nullptr)
                //     output_buffer.pSample->Release();
                return hr;
            }

            block_t *vout_buffer = block_Alloc(total_length);
            if (unlikely(vout_buffer == nullptr))
            {
                // if (output_sample.Get() == nullptr)
                //     output_buffer.pSample->Release();
                hr = E_OUTOFMEMORY;
                return hr;
            }

            BYTE *buffer_start;
            hr = output_media_buffer->Lock(&buffer_start, NULL, NULL);
            if (FAILED(hr))
            {
                vlc_debug(logger, "Failed to lock output buffer %lu. (hr=0x%lX)", buf_index, hr);
                // if (output_sample.Get() == nullptr)
                //     output_buffer.pSample->Release();
                block_Release(vout_buffer);
                return hr;
            }

            memcpy(vout_buffer->p_buffer, buffer_start, total_length);

            hr = output_media_buffer->Unlock();

            if (output_sample.Get() == nullptr)
                /* Sample is not provided by the MFT: clear its content. */
                output_media_buffer->SetCurrentLength(0);

            if (FAILED(hr))
            {
                vlc_debug(logger, "Failed to unlock output buffer %lu. (hr=0x%lX)", buf_index, hr);
                // if (output_sample.Get() == nullptr)
                //     output_buffer.pSample->Release();
                block_Release(vout_buffer);
                return hr;
            }

            // Convert from 100 nanoseconds unit to vlc ticks.
            vout_buffer->i_dts = vout_buffer->i_pts = VLC_TICK_FROM_MSFTIME(sample_time);
            UINT64 decode_timestamp;
            hr = output_buffer.pSample->GetUINT64(MFSampleExtension_DecodeTimestamp, &decode_timestamp);
            if (SUCCEEDED(hr))
                vout_buffer->i_dts = VLC_TICK_FROM_MSFTIME(decode_timestamp);

            UINT32 is_keyframe = FALSE;
            hr = output_buffer.pSample->GetUINT32(MFSampleExtension_CleanPoint, &is_keyframe);
            if (SUCCEEDED(hr) && is_keyframe)
                vout_buffer->i_flags |= BLOCK_FLAG_TYPE_I;

            if (output == nullptr)
                output = vout_buffer;
            else
                block_ChainAppend(&output, vout_buffer);
        }
    }
    return S_OK;
}

#ifdef ENABLE_SOUT
static block_t *EncodeVideoAsync(encoder_t *p_enc, picture_t *p_pic)
{
    mft_enc_video *p_sys = static_cast<mft_enc_video*>(p_enc->p_sys);
    block_t *output = nullptr;

    if (p_pic == nullptr)
    {
        HRESULT hr;
        hr = p_sys->mft->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        if (FAILED(hr))
        {
            msg_Warn(p_enc, "draining failed (hr=0x%lX)", hr);
            return nullptr;
        }
        while (p_sys->pending_drain_events == 0)
        {
            hr = p_sys->DequeueMediaEvent(vlc_object_logger(p_enc), true);
            if (FAILED(hr))
                goto error;

            while (p_sys->pending_output_events > 0)
            {
                hr = p_sys->ProcessOutput(vlc_object_logger(p_enc), output);
                if (FAILED(hr))
                {
                    goto error;
                }
            }
        }
        p_sys->pending_drain_events = 0;
        return output;
    }

    HRESULT hr;
    /* Dequeue all pending media events. */
    while ((hr = p_sys->DequeueMediaEvent(vlc_object_logger(p_enc))) == S_OK)
        continue;
    if (hr != MF_E_NO_EVENTS_AVAILABLE && FAILED(hr))
        goto error;

    /* Drain the output stream of the MFT before sending the input packet. */
    while (p_sys->pending_output_events > 0)
    {
        hr = p_sys->ProcessOutput(vlc_object_logger(p_enc), output);
        if (FAILED(hr))
        {
            break;
        }
    }

    if (p_pic != nullptr)
    {
        /* Poll the MFT and return decoded frames until the input stream is ready. */
        while (p_sys->pending_input_events == 0)
        {
            hr = p_sys->DequeueMediaEvent(vlc_object_logger(p_enc), true);
            if (FAILED(hr))
                goto error;

            while (p_sys->pending_output_events > 0)
            {
                hr = p_sys->ProcessOutput(vlc_object_logger(p_enc), output);
                if (FAILED(hr))
                {
                    goto error;
                }
            }
        }

        p_sys->pending_input_events -= 1;
        hr = p_sys->ProcessInputPicture(vlc_object_logger(p_enc), p_pic);
        if (FAILED(hr))
            goto error;
    }

error:
    return output;
}

static block_t * EncodeVideo(encoder_t *p_enc, picture_t *p_pic)
{
    mft_enc_video *p_sys = static_cast<mft_enc_video*>(p_enc->p_sys);

    if (p_pic == nullptr)
    {
        HRESULT hr;
        hr = p_sys->mft->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        if (FAILED(hr))
        {
            msg_Warn(p_enc, "draining failed (hr=0x%lX)", hr);
            return nullptr;
        }
    }

    /* Drain the output stream before sending the input packet. */
    block_t *output = nullptr;
    HRESULT hr = p_sys->ProcessOutput(vlc_object_logger(p_enc), output);
    if (FAILED(hr))
    {
        if (output)
            block_Release(output);
        return nullptr;
    }

    if (p_pic != nullptr)
    {
        hr = p_sys->ProcessInputPicture(vlc_object_logger(p_enc), p_pic);
        if (FAILED(hr))
        {
            if (output)
                block_Release(output);
            return nullptr;
        }
    }

    return output;
}

static block_t * EncodeAudio(encoder_t *p_enc, block_t *p_aout_buffer)
{
    mft_enc_audio *p_sys = static_cast<mft_enc_audio*>(p_enc->p_sys);

    if (p_aout_buffer && p_aout_buffer->i_flags & (BLOCK_FLAG_CORRUPTED))
    {
        block_Release(p_aout_buffer);
        return nullptr;
    }

    if (p_aout_buffer == nullptr)
    {
        HRESULT hr;
        hr = p_sys->mft->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        if (FAILED(hr))
        {
            msg_Warn(p_enc, "draining failed (hr=0x%lX)", hr);
            return nullptr;
        }
    }

    /* Drain the output stream before sending the input packet. */
    block_t *output = nullptr;
    bool keep_reading = true;
    int err = VLC_SUCCESS;
    HRESULT hr;
    do {
        ComPtr<IMFSample> output_sample;
        hr = p_sys->AllocateOutputSample(p_enc->fmt_in.i_cat, output_sample);
        if (FAILED(hr))
        {
            msg_Err(p_enc, "Error in AllocateOutputSample(). (hr=0x%lX)", hr);
            break;
        }

        DWORD output_status = 0;
        MFT_OUTPUT_DATA_BUFFER output_buffer = { p_sys->output_stream_id, output_sample.Get(), 0, NULL };
        hr = p_sys->mft->ProcessOutput(0, 1, &output_buffer, &output_status);
        if (output_buffer.pEvents)
            output_buffer.pEvents->Release();
        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
            break;

        if (FAILED(hr))
        {
            msg_Dbg(p_enc, "Failed to process audio output stream %lu (error 0x%lX)", p_sys->output_stream_id, hr);
            return nullptr;
        }

        if (output_buffer.pSample == nullptr)
            break;

        LONGLONG sample_time;
        hr = output_buffer.pSample->GetSampleTime(&sample_time);
        if (FAILED(hr))
        {
            msg_Dbg(p_enc, "Failed to get output time. (hr=0x%lX)", hr);
            return nullptr;
        }
        /* Convert from 100 nanoseconds unit to vlc ticks. */
        vlc_tick_t samp_time = VLC_TICK_FROM_MSFTIME(sample_time);

        DWORD output_count = 0;
        hr = output_buffer.pSample->GetBufferCount(&output_count);
        if (unlikely(FAILED(hr)))
        {
            msg_Dbg(p_enc, "Failed to get output buffer count. (hr=0x%lX)", hr);
            return nullptr;
        }

        for (DWORD buf_index = 0; buf_index < output_count; buf_index++)
        {
            ComPtr<IMFMediaBuffer> output_media_buffer;
            hr = output_sample->GetBufferByIndex(buf_index, &output_media_buffer);
            if (FAILED(hr))
            {
                msg_Dbg(p_enc, "Failed to get output buffer %lu. (hr=0x%lX)", buf_index, hr);
                return nullptr;
            }

            DWORD total_length = 0;
            hr = output_media_buffer->GetCurrentLength(&total_length);
            if (FAILED(hr))
            {
                msg_Dbg(p_enc, "Failed to get output buffer %lu length. (hr=0x%lX)", buf_index, hr);
                return nullptr;
            }

            block_t *aout_buffer = block_Alloc(total_length);
            if (unlikely(aout_buffer == nullptr))
            {
                err = VLC_ENOMEM;
                break;
            }

            aout_buffer->i_pts = samp_time;

            BYTE *buffer_start;
            hr = output_media_buffer->Lock(&buffer_start, NULL, NULL);
            if (FAILED(hr))
            {
                msg_Dbg(p_enc, "Failed to lock output buffer %lu. (hr=0x%lX)", buf_index, hr);
                block_Release(aout_buffer);
                return nullptr;
            }

            memcpy(aout_buffer->p_buffer, buffer_start, total_length);

            hr = output_media_buffer->Unlock();
            if (FAILED(hr))
            {
                msg_Dbg(p_enc, "Failed to unlock output buffer %lu. (hr=0x%lX)", buf_index, hr);
                block_Release(aout_buffer);
                return nullptr;
            }

            if (output == nullptr)
                output = aout_buffer;
            else
                block_ChainAppend(&output, aout_buffer);
        }

    } while (err == VLC_SUCCESS && keep_reading);
    if (err != VLC_SUCCESS)
    {
        block_Release(output);
        return nullptr;
    }

    if (p_aout_buffer != nullptr &&
        ProcessInputStream(vlc_object_logger(p_enc), p_sys->mft,
                           p_sys->input_stream_id, p_aout_buffer))
    {
        block_Release(output);
        return nullptr;
    }

    return output;
}

static void EncoderClose(encoder_t *p_enc)
{
    mft_sys_t *p_sys = static_cast<mft_sys_t*>(p_enc->p_sys);

    if (p_sys->mft.Get())
    {
        p_sys->endStream();

        // if (p_sys->output_sample.Get() == nullptr)
        // {
        //     // the MFT produces the output and may still have some left, we need to drain them
        //     HRESULT hr;
        //     hr = p_sys->mft->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        //     if (FAILED(hr))
        //     {
        //         msg_Warn(p_enc, "exit draining failed (hr=0x%lX)", hr);
        //     }
        //     else
        //     {
        //         for (;;)
        //         {
        //             DWORD output_status = 0;
        //             MFT_OUTPUT_DATA_BUFFER output_buffer = { p_sys->output_stream_id, p_sys->output_sample.Get(), 0, NULL };
        //             hr = p_sys->mft->ProcessOutput(0, 1, &output_buffer, &output_status);
        //             if (output_buffer.pEvents)
        //                 output_buffer.pEvents->Release();
        //             if (output_buffer.pSample)
        //             {
        //                 output_buffer.pSample->Release();
        //             }
        //             if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
        //                 break;
        //             if (hr == MF_E_TRANSFORM_TYPE_NOT_SET)
        //                 break;
        //         }
        //     }
        // }

        // make sure don't have any input pending
        p_sys->flushStream();

        p_sys->shutdownStream();
    }

    p_sys->Release();
}

static int InitializeEncoder(struct vlc_logger *logger, mft_sys_t &mf_sys)
{
    HRESULT hr;
    ComPtr<IMFAttributes> attributes;
    hr = mf_sys.mft->GetAttributes(&attributes);
    if (FAILED(hr) && hr != E_NOTIMPL)
        goto error;
    if (SUCCEEDED(hr))
    {
        UINT32 is_async = FALSE;
        hr = attributes->GetUINT32(MF_TRANSFORM_ASYNC, &is_async);
        if (hr != MF_E_ATTRIBUTENOTFOUND && FAILED(hr))
            goto error;
        mf_sys.is_async = is_async;
        if (mf_sys.is_async)
        {
            hr = attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
            if (FAILED(hr))
                goto error;
            hr = mf_sys.mft.As(&mf_sys.event_generator);
            if (FAILED(hr))
                goto error;
        }
    }

    DWORD input_streams_count;
    DWORD output_streams_count;
    hr = mf_sys.mft->GetStreamCount(&input_streams_count, &output_streams_count);
    if (FAILED(hr))
        goto error;
    if (input_streams_count != 1 || output_streams_count != 1)
    {
        vlc_error(logger, "MFT encoder should have 1 input stream and 1 output stream.");
        goto error;
    }

    hr = mf_sys.mft->GetStreamIDs(1, &mf_sys.input_stream_id, 1, &mf_sys.output_stream_id);
    if (hr == E_NOTIMPL)
    {
        /*
         * This is not an error, it happens if:
         * - there is a fixed number of streams.
         * AND
         * - streams are numbered consecutively from 0 to N-1.
         */
        mf_sys.input_stream_id = 0;
        mf_sys.output_stream_id = 0;
    }
    else if (FAILED(hr))
        goto error;

    return VLC_SUCCESS;

error:
    return VLC_ENOTSUP;
}

static int OpenMFTVideoEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t*)p_this;

    if( FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)) )
        return VLC_EINVAL;

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr))
    {
        CoUninitialize();
        return VLC_ENOTSUP;
    }

    mft_enc_video *p_sys = new (std::nothrow) mft_enc_video();
    if (unlikely(p_sys == nullptr))
    {
        MFShutdown();
        CoUninitialize();
        return VLC_ENOMEM;
    }
    p_enc->p_sys = p_sys;

    GUID category = MFT_CATEGORY_VIDEO_ENCODER;
    MFT_REGISTER_TYPE_INFO input_type, output_type;
    IMFActivate **activate_objects = nullptr;
    UINT32 activate_objects_count = 0;
    UINT32 flags;
    bool found = false;

    hr = MFTypeFromChroma(p_enc->fmt_in.video.i_chroma, p_enc->vctx_in, input_type);
    if (FAILED(hr))
    {
        msg_Dbg(p_this, "Input chroma %4.4s not supported by MediaFoundation, forcing NV12",
                (char*)&p_enc->fmt_in.i_codec);
        p_enc->fmt_in.video.i_chroma = VLC_CODEC_NV12;
        p_enc->fmt_in.i_codec = p_enc->fmt_in.video.i_chroma;
        hr = MFTypeFromChroma(p_enc->fmt_in.video.i_chroma, nullptr, input_type);
        assert(SUCCEEDED(hr));
    }

    hr = MFTypeFromCodec(MFMediaType_Video, p_enc->fmt_out.i_codec, output_type);
    if (FAILED(hr))
    {
        msg_Dbg(p_this, "Output codec %4.4s not supported by MediaFoundation",
                (char*)&p_enc->fmt_out.i_codec);
        goto error;
    }

    ListTransforms(vlc_object_logger(p_this), category, "video encoder", var_InheritBool(p_this, "mft-debug"));

    flags = MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_LOCALMFT
            | MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT
            | MFT_ENUM_FLAG_HARDWARE;

    hr = MFTEnumEx(category, flags, &input_type, &output_type, &activate_objects, &activate_objects_count);
    if (FAILED(hr))
    {
        msg_Dbg(p_enc, "Failed to enumerate MFT modules for %4.4s", (const char*)&p_enc->fmt_out.i_codec);
        goto error;
    }
    if (activate_objects_count == 0)
    {
        // try the other raw input format
        input_type.guidSubtype = MFVideoFormat_NV12;
        hr = MFTEnumEx(category, flags, &input_type, &output_type, &activate_objects, &activate_objects_count);
        if (FAILED(hr))
        {
            msg_Dbg(p_enc, "Failed to enumerate MFT modules for %4.4s", (const char*)&p_enc->fmt_out.i_codec);
            goto error;
        }
    }
    msg_Dbg(p_enc, "Found %d available MFT module(s) for %4.4s to %4.4s", activate_objects_count,
            (const char*)&p_enc->fmt_in.i_codec, (const char*)&p_enc->fmt_out.i_codec);
    if (activate_objects_count == 0)
        goto error;

    for (UINT32 i = 0; i < activate_objects_count; ++i)
    {
        WCHAR Name[256];
        hr = activate_objects[i]->GetString(MFT_FRIENDLY_NAME_Attribute, Name, ARRAY_SIZE(Name), nullptr);
        if (FAILED(hr))
            wcsncpy(Name, L"<unknown>", ARRAY_SIZE(Name));
        hr = activate_objects[i]->ActivateObject(IID_PPV_ARGS(p_sys->mft.ReleaseAndGetAddressOf()));
        if (FAILED(hr))
        {
            msg_Warn(p_enc, "Failed to initialize %ls encoder", Name);
            continue;
        }

        if (InitializeEncoder(vlc_object_logger(p_this), *p_sys) == VLC_SUCCESS)
        {
            msg_Dbg(p_enc, "Using video encoder %ls", Name);
            found = true;
            break;
        }
    }
    for (UINT32 i = 0; i < activate_objects_count; ++i)
        activate_objects[i]->Release();
    CoTaskMemFree(activate_objects);
    if (!found)
        goto error;

    p_enc->fmt_in.i_codec = MFFormatToChroma(input_type.guidSubtype);
    p_enc->fmt_in.video.i_chroma = p_enc->fmt_in.i_codec;

    hr = p_sys->SetOutputType(vlc_object_logger(p_this),
                              output_type.guidSubtype, p_enc->fmt_out);
    if (FAILED(hr))
    {
        msg_Err(p_enc, "Error in SetOutputType(). (hr=0x%lX)", hr);
        goto error;
    }
    hr = p_sys->SetInputType(p_enc->fmt_in, input_type);
    if (FAILED(hr))
    {
        msg_Err(p_enc, "Error in SetInputType(). (hr=0x%lX)", hr);
        goto error;
    }

    /* This event is required for asynchronous MFTs, optional otherwise. */
    hr = p_sys->startStream();
    if (FAILED(hr))
    {
        msg_Err(p_enc, "Error in startStream(). (hr=0x%lX)", hr);
        goto error;
    }

    static const struct vlc_encoder_operations video_ops = []{
        struct vlc_encoder_operations cbs{};
        cbs.close = EncoderClose;
        cbs.encode_video = EncodeVideo;
        return cbs;
    }();
    static const struct vlc_encoder_operations video_async_ops = []{
        struct vlc_encoder_operations cbs{};
        cbs.close = EncoderClose;
        cbs.encode_video = EncodeVideoAsync;
        return cbs;
    }();
    p_enc->ops = p_sys->is_async ? &video_async_ops : &video_ops;

    return VLC_SUCCESS;

error:
    EncoderClose(p_enc);
    return VLC_ENOTSUP;
}

static int OpenMFTAudioEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t*)p_this;

    if( FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)) )
        return VLC_EINVAL;

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr))
    {
        CoUninitialize();
        return VLC_ENOTSUP;
    }

    mft_enc_audio *p_sys = new (std::nothrow) mft_enc_audio();
    if (unlikely(p_sys == nullptr))
    {
        MFShutdown();
        CoUninitialize();
        return VLC_ENOMEM;
    }
    p_enc->p_sys = p_sys;

    GUID category = MFT_CATEGORY_AUDIO_ENCODER;
    MFT_REGISTER_TYPE_INFO input_type, output_type;
    IMFActivate **activate_objects = nullptr;
    UINT32 activate_objects_count = 0;
    UINT32 flags;
    bool found = false;

    hr = MFTypeFromAudio(p_enc->fmt_in.audio.i_format, input_type);
    if (FAILED(hr))
    {
        msg_Dbg(p_this, "Input codec %4.4s not supported by MediaFoundation",
                (char*)&p_enc->fmt_in.i_codec);
        goto error;
    }

    hr = MFTypeFromCodec(MFMediaType_Audio, p_enc->fmt_out.i_codec, output_type);
    if (FAILED(hr))
        hr = MFTypeFromAudio(p_enc->fmt_out.i_codec, output_type);
    if (output_type.guidSubtype == GUID_NULL)
    {
        msg_Dbg(p_this, "Output codec %4.4s not supported by MediaFoundation",
                (char*)&p_enc->fmt_out.i_codec);
        goto error;
    }

    ListTransforms(vlc_object_logger(p_this), category, "audio encoder", var_InheritBool(p_this, "mft-debug"));

    flags = MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_LOCALMFT
            | MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT
            | MFT_ENUM_FLAG_HARDWARE;
    hr = MFTEnumEx(category, flags, &input_type, &output_type, &activate_objects, &activate_objects_count);
    if (FAILED(hr))
    {
        msg_Dbg(p_enc, "Failed to enumerate MFT modules for %4.4s", (const char*)&p_enc->fmt_out.i_codec);
        goto error;
    }
    if (activate_objects_count == 0)
    {
        // try the other raw input format
        if (p_enc->fmt_in.audio.i_format == VLC_CODEC_F32L)
            p_enc->fmt_in.audio.i_format = VLC_CODEC_S16L;
        else
            p_enc->fmt_in.audio.i_format = VLC_CODEC_F32L;
        MFTypeFromAudio(p_enc->fmt_in.audio.i_format, input_type);
        hr = MFTEnumEx(category, flags, &input_type, &output_type, &activate_objects, &activate_objects_count);
        if (FAILED(hr))
        {
            msg_Dbg(p_enc, "Failed to enumerate MFT modules for %4.4s", (const char*)&p_enc->fmt_out.i_codec);
            goto error;
        }
        p_enc->fmt_in.i_codec = p_enc->fmt_in.audio.i_format;
        p_enc->fmt_in.audio.i_bitspersample = aout_BitsPerSample(p_enc->fmt_in.i_codec);
        msg_Warn(p_enc, "Using %4.4s for audio source", (char*)&p_enc->fmt_in.i_codec);
    }
    msg_Dbg(p_enc, "Found %d available MFT module(s) for %4.4s to %4.4s", activate_objects_count,
            (const char*)&p_enc->fmt_in.i_codec, (const char*)&p_enc->fmt_out.i_codec);
    if (activate_objects_count == 0)
        goto error;

    for (UINT32 i = 0; i < activate_objects_count; ++i)
    {
        WCHAR Name[256];
        hr = activate_objects[i]->GetString(MFT_FRIENDLY_NAME_Attribute, Name, ARRAY_SIZE(Name), nullptr);
        if (FAILED(hr))
            wcsncpy(Name, L"<unknown>", ARRAY_SIZE(Name));
        hr = activate_objects[i]->ActivateObject(IID_PPV_ARGS(p_sys->mft.ReleaseAndGetAddressOf()));
        if (FAILED(hr))
            continue;

        if (InitializeEncoder(vlc_object_logger(p_this), *p_sys) == VLC_SUCCESS)
        {
            msg_Dbg(p_enc, "Using audio encoder %ls", Name);
            found = true;
            break;
        }
    }
    for (UINT32 i = 0; i < activate_objects_count; ++i)
        activate_objects[i]->Release();
    CoTaskMemFree(activate_objects);
    if (!found)
        goto error;

    if (p_sys->SetOutputType(vlc_object_logger(p_this),
                             output_type.guidSubtype, p_enc->fmt_out))
        goto error;

    hr = p_sys->SetInputType(p_enc->fmt_in, input_type);
    if (FAILED(hr))
    {
        msg_Err(p_enc, "Error in SetInputType(). (hr=0x%lX)", hr);
        goto error;
    }

    /* This event is required for asynchronous MFTs, optional otherwise. */
    hr = p_sys->startStream();
    if (FAILED(hr))
    {
        msg_Err(p_enc, "Error in startStream(). (hr=0x%lX)", hr);
        goto error;
    }

    static const struct vlc_encoder_operations audio_ops = []{
        struct vlc_encoder_operations cbs{};
        cbs.close = EncoderClose;
        cbs.encode_audio = EncodeAudio;
        return cbs;
    }();
    p_enc->ops = &audio_ops;

    return VLC_SUCCESS;

error:
    EncoderClose(p_enc);
    return VLC_ENOTSUP;
}
#endif // !ENABLE_SOUT

static int Open(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;

    if( FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)) )
        return VLC_EINVAL;

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr))
    {
        CoUninitialize();
        return VLC_ENOTSUP;
    }

    mft_sys_t *p_sys;
    if (p_dec->fmt_in->i_cat == VIDEO_ES)
        p_sys = new (std::nothrow) mft_dec_video();
    else
        p_sys = new (std::nothrow) mft_dec_audio();
    if (unlikely(p_sys == nullptr))
    {
        MFShutdown();
        CoUninitialize();
        return VLC_ENOMEM;
    }
    p_dec->p_sys = p_sys;

    if (FindMFT(p_dec))
    {
        msg_Err(p_dec, "Could not find suitable MFT decoder for %4.4s", (char*)&p_dec->fmt_in->i_codec);
        goto error;
    }

    /* Only one output sample is needed, we can allocate one and reuse it. */
    hr = p_sys->AllocateOutputSample(p_dec->fmt_in->i_cat, p_sys->output_sample);
    if (FAILED(hr))
    {
        msg_Err(p_dec, "Error in AllocateOutputSample(). (hr=0x%lX)", hr);
        goto error;
    }

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
    auto *p_sys = static_cast<mft_sys_t*>(p_dec->p_sys);

    DestroyMFT(p_dec);

    p_sys->Release();
}
