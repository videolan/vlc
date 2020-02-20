/*****************************************************************************
 * DBMSDIOutput.cpp: Decklink SDI Output
 *****************************************************************************
 * Copyright © 2014-2016 VideoLAN and VideoLAN Authors
 *                  2018 VideoLabs
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
#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_interrupt.h>
#include <vlc_decklink.h>

#include "DBMHelper.hpp"
#include "DBMSDIOutput.hpp"
#include "SDIStream.hpp"
#include "SDIAudioMultiplex.hpp"
#include "SDIGenerator.hpp"
#include "Ancillary.hpp"
#include "V210.hpp"

#include <DeckLinkAPIDispatch.cpp>
#include <DeckLinkAPIVersion.h>
#if BLACKMAGIC_DECKLINK_API_VERSION < 0x0b010000
 #define IID_IDeckLinkProfileAttributes IID_IDeckLinkAttributes
 #define IDeckLinkProfileAttributes IDeckLinkAttributes
#endif

#include "sdiout.hpp"

#include <arpa/inet.h>

#define DECKLINK_CARD_BUFFER (CLOCK_FREQ)
#define DECKLINK_PREROLL (CLOCK_FREQ*3/4)
#define DECKLINK_SCHED_OFFSET (CLOCK_FREQ/20)

static_assert(DECKLINK_CARD_BUFFER > DECKLINK_PREROLL + DECKLINK_SCHED_OFFSET, "not in card buffer limits");

using namespace sdi_sout;

DBMSDIOutput::DBMSDIOutput(sout_stream_t *p_stream) :
    SDIOutput(p_stream)
{
    p_card = NULL;
    p_output = NULL;
    clock.system_reference = VLC_TICK_INVALID;
    clock.offset = 0;
    lasttimestamp = 0;
    b_running = false;
    streamStartTime = VLC_TICK_INVALID;
    vlc_mutex_init(&feeder.lock);
    vlc_cond_init(&feeder.cond);
}

DBMSDIOutput::~DBMSDIOutput()
{
    if(p_output)
    {
        while(!isDrained())
            vlc_tick_wait(vlc_tick_now() + CLOCK_FREQ/60);
        vlc_cancel(feeder.thread);
        vlc_join(feeder.thread, NULL);
    }

    es_format_Clean(&video.configuredfmt);
    if(p_output)
    {
        BMDTimeValue out;
        p_output->StopScheduledPlayback(lasttimestamp, &out, timescale);
        p_output->DisableVideoOutput();
        p_output->DisableAudioOutput();
        p_output->Release();
    }
    if(p_card)
        p_card->Release();
}

AbstractStream *DBMSDIOutput::Add(const es_format_t *fmt)
{
    AbstractStream *s = SDIOutput::Add(fmt);
    if(s)
    {
        msg_Dbg(p_stream, "accepted %s %4.4s",
                          s->getID().toString().c_str(), (const char *) &fmt->i_codec);
    }
    else
    {
        msg_Err(p_stream, "rejected es id %d %4.4s",
                          fmt->i_id, (const char *) &fmt->i_codec);
    }

    return s;
}

#define CHECK(message) do { \
    if (result != S_OK) \
    { \
    const char *psz_err = Decklink::Helper::ErrorToString(result); \
    if(psz_err)\
    msg_Err(p_stream, message ": %s", psz_err); \
    else \
    msg_Err(p_stream, message ": 0x%X", result); \
    goto error; \
} \
} while(0)

HRESULT STDMETHODCALLTYPE DBMSDIOutput::QueryInterface( REFIID, LPVOID * )
{
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DBMSDIOutput::AddRef()
{
    return 1;
}

ULONG STDMETHODCALLTYPE DBMSDIOutput::Release()
{
    return 1;
}

HRESULT STDMETHODCALLTYPE DBMSDIOutput::ScheduledFrameCompleted
    (IDeckLinkVideoFrame *, BMDOutputFrameCompletionResult result)
{
    if(result == bmdOutputFrameDropped)
        msg_Warn(p_stream, "dropped frame");
    else if(result == bmdOutputFrameDisplayedLate)
        msg_Warn(p_stream, "late frame");

    bool b_active;
    vlc_mutex_lock(&feeder.lock);
    if((S_OK == p_output->IsScheduledPlaybackRunning(&b_active)) && b_active)
        vlc_cond_signal(&feeder.cond);
    vlc_mutex_unlock(&feeder.lock);

    return S_OK;
}

HRESULT DBMSDIOutput::ScheduledPlaybackHasStopped (void)
{
    return S_OK;
}

int DBMSDIOutput::Open()
{
    HRESULT result;
    IDeckLinkIterator *decklink_iterator = NULL;

    int i_card_index = var_InheritInteger(p_stream, CFG_PREFIX "card-index");

    if (i_card_index < 0)
    {
        msg_Err(p_stream, "Invalid card index %d", i_card_index);
        goto error;
    }

    decklink_iterator = CreateDeckLinkIteratorInstance();
    if (!decklink_iterator)
    {
        msg_Err(p_stream, "DeckLink drivers not found.");
        goto error;
    }

    for(int i = 0; i <= i_card_index; ++i)
    {
        if (p_card)
        {
            p_card->Release();
            p_card = NULL;
        }
        result = decklink_iterator->Next(&p_card);
        CHECK("Card not found");
    }

    decklink_str_t tmp_name;
    const char *psz_model_name;
    result = p_card->GetModelName(&tmp_name);
    CHECK("Unknown model name");
    psz_model_name = DECKLINK_STRDUP(tmp_name);
    DECKLINK_FREE(tmp_name);

    msg_Dbg(p_stream, "Opened DeckLink PCI card %s", psz_model_name);

    result = p_card->QueryInterface(IID_IDeckLinkOutput, (void**)&p_output);
    CHECK("No outputs");

    decklink_iterator->Release();

    if(vlc_clone(&feeder.thread, feederThreadCallback, this, VLC_THREAD_PRIORITY_INPUT))
        goto error;

    return VLC_SUCCESS;

error:
    if (p_output)
    {
        p_output->Release();
        p_output = NULL;
    }
    if (p_card)
    {
        p_card->Release();
        p_output = NULL;
    }
    if (decklink_iterator)
        decklink_iterator->Release();

    return VLC_EGENERIC;
}

int DBMSDIOutput::ConfigureAudio(const audio_format_t *)
{
    HRESULT result;
    IDeckLinkProfileAttributes *p_attributes = NULL;

    if(FAKE_DRIVER)
        return VLC_SUCCESS;

    if(!p_output)
        return VLC_EGENERIC;

    if(!video.configuredfmt.i_codec && b_running)
        return VLC_EGENERIC;

    if (audio.i_channels > 0)
    {
        uint8_t maxchannels = audioMultiplex->config.getMultiplexedFramesCount() * 2;

        result = p_card->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&p_attributes);
        CHECK("Could not get IDeckLinkAttributes");

        int64_t i64;
        result = p_attributes->GetInt(BMDDeckLinkMaximumAudioChannels, &i64);
        CHECK("Could not get BMDDeckLinkMaximumAudioChannels");
        if(i64 < maxchannels)
        {
            msg_Err(p_stream, "requested channels %" PRIu8 " exceeds supported maximum: %" PRId64,
                    maxchannels, i64);
            goto error;
        }

        msg_Dbg(p_stream, "configuring audio output with %d", maxchannels);
        result = p_output->EnableAudioOutput(
                    bmdAudioSampleRate48kHz,
                    bmdAudioSampleType16bitInteger,
                    maxchannels,
                    bmdAudioOutputStreamTimestamped);
        CHECK("Could not start audio output");

        if(S_OK != p_output->BeginAudioPreroll())
        {
            p_output->DisableAudioOutput();
            goto error;
        }

        audio.b_configured = true;

        p_attributes->Release();
    }
    return VLC_SUCCESS;

error:
    if(p_attributes)
        p_attributes->Release();
    return VLC_EGENERIC;
}

static BMDVideoConnection getVConn(const char *psz)
{
    BMDVideoConnection conn = bmdVideoConnectionSDI;

    if(!psz)
        return conn;

    if (!strcmp(psz, "sdi"))
        conn = bmdVideoConnectionSDI;
    else if (!strcmp(psz, "hdmi"))
        conn = bmdVideoConnectionHDMI;
    else if (!strcmp(psz, "opticalsdi"))
        conn = bmdVideoConnectionOpticalSDI;
    else if (!strcmp(psz, "component"))
        conn = bmdVideoConnectionComponent;
    else if (!strcmp(psz, "composite"))
        conn = bmdVideoConnectionComposite;
    else if (!strcmp(psz, "svideo"))
        conn = bmdVideoConnectionSVideo;

    return conn;
}

int DBMSDIOutput::ConfigureVideo(const video_format_t *vfmt)
{
    HRESULT result;
    BMDDisplayMode wanted_mode_id = bmdModeUnknown;
    IDeckLinkConfiguration *p_config = NULL;
    IDeckLinkProfileAttributes *p_attributes = NULL;
    IDeckLinkDisplayMode *p_display_mode = NULL;
    char *psz_string = NULL;
    video_format_t *fmt = &video.configuredfmt.video;

    if(FAKE_DRIVER)
    {
        video_format_Copy(fmt, vfmt);
        fmt->i_chroma = !video.tenbits ? VLC_CODEC_UYVY : VLC_CODEC_I422_10L;
        fmt->i_frame_rate = (unsigned) frameduration;
        fmt->i_frame_rate_base = (unsigned) timescale;
        video.configuredfmt.i_codec = fmt->i_chroma;
        return VLC_SUCCESS;
    }

    if(!p_output)
        return VLC_EGENERIC;

    if(!video.configuredfmt.i_codec && b_running)
        return VLC_EGENERIC;

    /* Now configure card */
    if(!p_output)
        return VLC_EGENERIC;

    result = p_card->QueryInterface(IID_IDeckLinkConfiguration, (void**)&p_config);
    CHECK("Could not get config interface");

    psz_string = var_InheritString(p_stream, CFG_PREFIX "mode");
    if(psz_string)
    {
        size_t len = strlen(psz_string);
        if (len > 4)
        {
            free(psz_string);
            msg_Err(p_stream, "Invalid mode %s", psz_string);
            goto error;
        }
        memset(&wanted_mode_id, ' ', 4);
        strncpy((char*)&wanted_mode_id, psz_string, 4);
        wanted_mode_id = ntohl(wanted_mode_id);
        free(psz_string);
    }

    /* Read attributes */
    result = p_card->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&p_attributes);
    CHECK("Could not get IDeckLinkAttributes");

    int64_t vconn;
    result = p_attributes->GetInt(BMDDeckLinkVideoOutputConnections, &vconn); /* reads mask */
    CHECK("Could not get BMDDeckLinkVideoOutputConnections");

    psz_string = var_InheritString(p_stream, CFG_PREFIX "video-connection");
    vconn = getVConn(psz_string);
    free(psz_string);
    if (vconn == 0)
    {
        msg_Err(p_stream, "Invalid video connection specified");
        goto error;
    }

    result = p_config->SetInt(bmdDeckLinkConfigVideoOutputConnection, vconn);
    CHECK("Could not set video output connection");

    p_display_mode = Decklink::Helper::MatchDisplayMode(VLC_OBJECT(p_stream),
                                                        p_output, vfmt, wanted_mode_id);
    if(p_display_mode == NULL)
    {
        msg_Err(p_stream, "Could not negociate a compatible display mode");
        goto error;
    }
    else
    {
        BMDDisplayMode mode_id = p_display_mode->GetDisplayMode();
        BMDDisplayMode modenl = htonl(mode_id);
        msg_Dbg(p_stream, "Selected mode '%4.4s'", (char *) &modenl);

        BMDPixelFormat pixelFormat = video.tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV;
        BMDVideoOutputFlags flags = bmdVideoOutputVANC;
        if (mode_id == bmdModeNTSC ||
            mode_id == bmdModeNTSC2398 ||
            mode_id == bmdModePAL)
        {
            flags = bmdVideoOutputVITC;
        }
        bool supported;
#if BLACKMAGIC_DECKLINK_API_VERSION < 0x0b010000
        BMDDisplayModeSupport support = bmdDisplayModeNotSupported;
        result = p_output->DoesSupportVideoMode(mode_id,
                                                pixelFormat,
                                                flags,
                                                &support,
                                                NULL);
        supported = (support != bmdDisplayModeNotSupported);
#else
        result = p_output->DoesSupportVideoMode(vconn,
                                                mode_id,
                                                pixelFormat,
                                                bmdSupportedVideoModeDefault,
                                                NULL,
                                                &supported);
#endif
        CHECK("Does not support video mode");
        if (!supported)
        {
            msg_Err(p_stream, "Video mode not supported");
            goto error;
        }

        if (p_display_mode->GetWidth() <= 0 || p_display_mode->GetWidth() & 1)
        {
            msg_Err(p_stream, "Unknown video mode specified.");
            goto error;
        }

        result = p_display_mode->GetFrameRate(&frameduration,
                                              &timescale);
        CHECK("Could not read frame rate");

        result = p_output->EnableVideoOutput(mode_id, flags);
        CHECK("Could not enable video output");

        p_output->SetScheduledFrameCompletionCallback(this);

        video_format_Copy(fmt, vfmt);
        fmt->i_width = fmt->i_visible_width = p_display_mode->GetWidth();
        fmt->i_height = fmt->i_visible_height = p_display_mode->GetHeight();
        fmt->i_x_offset = 0;
        fmt->i_y_offset = 0;
        fmt->i_sar_num = 0;
        fmt->i_sar_den = 0;
        fmt->i_chroma = !video.tenbits ? VLC_CODEC_UYVY : VLC_CODEC_I422_10L; /* we will convert to v210 */
        fmt->i_frame_rate = (unsigned) frameduration;
        fmt->i_frame_rate_base = (unsigned) timescale;
        video.configuredfmt.i_codec = fmt->i_chroma;

        char *psz_file = var_InheritString(p_stream, CFG_PREFIX "nosignal-image");
        if(psz_file)
        {
            video.pic_nosignal = sdi::Generator::Picture(VLC_OBJECT(p_stream), psz_file, fmt);
            if (!video.pic_nosignal)
                msg_Err(p_stream, "Could not create no signal picture");
            free(psz_file);
        }
    }

    p_display_mode->Release();
    p_attributes->Release();
    p_config->Release();

    return VLC_SUCCESS;

error:
    if (p_display_mode)
        p_display_mode->Release();
    if(p_attributes)
        p_attributes->Release();
    if (p_config)
        p_config->Release();
    return VLC_EGENERIC;
}

int DBMSDIOutput::StartPlayback()
{
    HRESULT result;
    if(FAKE_DRIVER && !b_running)
    {
        b_running = true;
        return VLC_SUCCESS;
    }
    if(b_running)
        return VLC_EGENERIC;

    if(audio.b_configured)
        p_output->EndAudioPreroll();

    result = p_output->StartScheduledPlayback(streamStartTime, CLOCK_FREQ, 1.0);
    CHECK("Could not start playback");
    b_running = true;
    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}

int DBMSDIOutput::FeedOneFrame()
{
    picture_t *p = reinterpret_cast<picture_t *>(videoBuffer.Dequeue());
    if(p)
        return ProcessVideo(p, reinterpret_cast<block_t *>(captionsBuffer.Dequeue()));

    return VLC_SUCCESS;
}

int DBMSDIOutput::FeedAudio(vlc_tick_t start, vlc_tick_t preroll, bool b_truncate)
{
#ifdef SDI_MULTIPLEX_DEBUG
        audioMultiplex->Debug();
#endif
    vlc_tick_t bufferStart = audioMultiplex->bufferStart();
    unsigned i_samples_per_frame =
            audioMultiplex->alignedInterleaveInSamples(bufferStart, SAMPLES_PER_FRAME);

    unsigned buffered = 0;
    while(bufferStart <= (start+preroll) &&
          audioMultiplex->availableVirtualSamples(bufferStart) >= i_samples_per_frame)
    {
        block_t *out = audioMultiplex->Extract(i_samples_per_frame);
        if(out)
        {
#ifdef SDI_MULTIPLEX_DEBUG
              msg_Dbg(p_stream, "extracted %u samples pts %ld i_samples_per_frame %u",
                      out->i_nb_samples, out->i_dts, i_samples_per_frame);
#endif
              if(b_truncate && out->i_pts < start)
              {
#ifdef SDI_MULTIPLEX_DEBUG
                  msg_Err(p_stream,"dropping %u samples at %ld (-%ld)",
                          i_samples_per_frame, out->i_pts, (start - out->i_pts));
#endif
                  block_Release(out);
              }
              else
              {
                  ProcessAudio(out);
              }
        }
        else break;
        bufferStart = audioMultiplex->bufferStart();
        i_samples_per_frame = audioMultiplex->alignedInterleaveInSamples(bufferStart, SAMPLES_PER_FRAME);
        buffered += i_samples_per_frame;
    }

    return buffered > 0 ? VLC_SUCCESS : VLC_EGENERIC;
}

void * DBMSDIOutput::feederThreadCallback(void *me)
{
    reinterpret_cast<DBMSDIOutput *>(me)->feederThread();
    return NULL;
}

void DBMSDIOutput::feederThread()
{
    vlc_tick_t maxdelay = CLOCK_FREQ/60;
    for(;;)
    {
        vlc_mutex_lock(&feeder.lock);
        mutex_cleanup_push(&feeder.lock);
        vlc_cond_timedwait(&feeder.cond, &feeder.lock, vlc_tick_now() + maxdelay);
        vlc_cleanup_pop();
        int cancel = vlc_savecancel();
        doSchedule();
        if(timescale)
            maxdelay = CLOCK_FREQ * frameduration / timescale;
        vlc_restorecancel(cancel);
        vlc_mutex_unlock(&feeder.lock);
    }
}

int DBMSDIOutput::Process()
{
    if((!p_output && !FAKE_DRIVER))
        return VLC_SUCCESS;

    vlc_mutex_lock(&feeder.lock);
    vlc_cond_signal(&feeder.cond);
    vlc_mutex_unlock(&feeder.lock);

    return VLC_SUCCESS;
}

int DBMSDIOutput::doSchedule()
{
    const vlc_tick_t preroll = DECKLINK_PREROLL;
    vlc_tick_t next = videoBuffer.NextPictureTime();
    if(next == VLC_TICK_INVALID ||
       (!b_running && !ReachedPlaybackTime(next + preroll + SAMPLES_PER_FRAME*CLOCK_FREQ/48000)))
        return VLC_SUCCESS;

    if(FAKE_DRIVER)
    {
        FeedOneFrame();
        FeedAudio(next, preroll, false);
        b_running = true;
        return VLC_SUCCESS;
    }

    uint32_t bufferedFramesCount;
    uint32_t bufferedAudioCount = 0;
    if(S_OK != p_output->GetBufferedVideoFrameCount(&bufferedFramesCount))
        return VLC_EGENERIC;

    uint32_t bufferedFramesTarget = (uint64_t)timescale*preroll/frameduration/CLOCK_FREQ;
    if( bufferedFramesTarget > bufferedFramesCount )
    {
        for(size_t i=0; i<bufferedFramesTarget - bufferedFramesCount; i++)
        {
            FeedOneFrame();
            if(b_running)
                break;
        }
        p_output->GetBufferedVideoFrameCount(&bufferedFramesCount);
    }

    /* no frames got in ?? */
    if(streamStartTime == VLC_TICK_INVALID)
    {
        assert(bufferedFramesTarget);
        assert(!b_running);
        return VLC_EGENERIC;
    }

    if(audio.b_configured)
    {
        if(S_OK != p_output->GetBufferedAudioSampleFrameCount(&bufferedAudioCount))
            return VLC_EGENERIC;

        uint32_t bufferedAudioTarget = 48000*preroll/CLOCK_FREQ;
        if(bufferedAudioCount < bufferedAudioTarget)
        {
            vlc_tick_t audioSamplesDuration = (bufferedAudioTarget - bufferedAudioCount)
                                            * CLOCK_FREQ / 48000;
            if(b_running)
                FeedAudio(next, audioSamplesDuration, false);
            else
                FeedAudio(streamStartTime, audioSamplesDuration, true);

            p_output->GetBufferedAudioSampleFrameCount(&bufferedAudioCount);
        }
    }

    if(!b_running && bufferedFramesCount >= bufferedFramesTarget)
    {
        msg_Dbg(p_stream, "Preroll end with %d frames/%d samples",
                          bufferedFramesCount, bufferedAudioCount);
        StartPlayback();
    }

    return (bufferedFramesCount < bufferedFramesTarget) ? VLC_ENOITEM : VLC_SUCCESS;
}

int DBMSDIOutput::ProcessAudio(block_t *p_block)
{
    if(FAKE_DRIVER)
    {
        block_Release(p_block);
        return VLC_SUCCESS;
    }

    if (!p_output)
    {
        block_Release(p_block);
        return VLC_EGENERIC;
    }

    uint32_t sampleFrameCount = p_block->i_nb_samples;
    uint32_t written;
    p_block->i_pts -= clock.offset;
    const BMDTimeValue scheduleTime = p_block->i_pts + DECKLINK_SCHED_OFFSET;
    HRESULT result = p_output->ScheduleAudioSamples(
                p_block->p_buffer, p_block->i_nb_samples,
                scheduleTime, CLOCK_FREQ, &written);

    if (result != S_OK)
        msg_Err(p_stream, "Failed to schedule audio sample: 0x%X", result);
    else
    {
        lasttimestamp = __MAX(p_block->i_pts, lasttimestamp);
        if (sampleFrameCount != written)
            msg_Err(p_stream, "Written only %d samples out of %d", written, sampleFrameCount);
    }

    block_Release(p_block);

    return result != S_OK ? VLC_EGENERIC : VLC_SUCCESS;
}

int DBMSDIOutput::ProcessVideo(picture_t *picture, block_t *p_cc)
{
    if (!picture)
        return VLC_EGENERIC;

    checkClockDrift();

    if(video.pic_nosignal && !FAKE_DRIVER)
    {
        BMDTimeValue streamTime;
        double playbackSpeed;
        if(S_OK == p_output->GetScheduledStreamTime(CLOCK_FREQ, &streamTime, &playbackSpeed))
        {
            if(picture->date - streamTime >
               VLC_TICK_FROM_SEC(video.nosignal_delay))
            {
                msg_Info(p_stream, "no signal");
                picture_Hold(video.pic_nosignal);
                video.pic_nosignal->date = streamTime + VLC_TICK_FROM_MS(30);
                doProcessVideo(video.pic_nosignal, NULL);
            }
        }
    }

    return doProcessVideo(picture, p_cc);
}

int DBMSDIOutput::doProcessVideo(picture_t *picture, block_t *p_cc)
{
    HRESULT result;
    int w, h, stride, length, ret = VLC_EGENERIC;
    BMDTimeValue scheduleTime;
    IDeckLinkMutableVideoFrame *pDLVideoFrame = NULL;
    w = video.configuredfmt.video.i_visible_width;
    h = video.configuredfmt.video.i_visible_height;

    if(FAKE_DRIVER)
        goto end;

    result = p_output->CreateVideoFrame(w, h, w*3,
                                        video.tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV,
                                        bmdFrameFlagDefault, &pDLVideoFrame);
    if(result != S_OK) {
        msg_Err(p_stream, "Failed to create video frame: 0x%X", result);
        goto error;
    }

    void *frame_bytes;
    pDLVideoFrame->GetBytes((void**)&frame_bytes);
    stride = pDLVideoFrame->GetRowBytes();

    if (video.tenbits)
    {
        IDeckLinkVideoFrameAncillary *vanc;
        void *buf;

        result = p_output->CreateAncillaryData(bmdFormat10BitYUV, &vanc);
        if (result != S_OK) {
            msg_Err(p_stream, "Failed to create vanc: %d", result);
            goto error;
        }

        result = vanc->GetBufferForVerticalBlankingLine(ancillary.afd_line, &buf);
        if (result != S_OK) {
            msg_Err(p_stream, "Failed to get VBI line %u: %d", ancillary.afd_line, result);
            goto error;
        }

        sdi::AFD afd(ancillary.afd, ancillary.ar);
        afd.FillBuffer(reinterpret_cast<uint8_t*>(buf), stride);

        if(p_cc)
        {
            result = vanc->GetBufferForVerticalBlankingLine(ancillary.captions_line, &buf);
            if (result != S_OK) {
                msg_Err(p_stream, "Failed to get VBI line %u: %d", ancillary.captions_line, result);
                goto error;
            }
            sdi::Captions captions(p_cc->p_buffer, p_cc->i_buffer, timescale, frameduration);
            captions.FillBuffer(reinterpret_cast<uint8_t*>(buf), stride);
        }

        sdi::V210::Convert(picture, stride, frame_bytes);

        result = pDLVideoFrame->SetAncillaryData(vanc);
        vanc->Release();
        if (result != S_OK) {
            msg_Err(p_stream, "Failed to set vanc: %d", result);
            goto error;
        }
    }
    else for(int y = 0; y < h; ++y) {
        uint8_t *dst = (uint8_t *)frame_bytes + stride * y;
        const uint8_t *src = (const uint8_t *)picture->p[0].p_pixels +
                picture->p[0].i_pitch * y;
        memcpy(dst, src, w * 2 /* bpp */);
    }


    // compute frame duration in CLOCK_FREQ units
    length = (frameduration * CLOCK_FREQ) / timescale;
    picture->date -= clock.offset;
    scheduleTime = picture->date + DECKLINK_SCHED_OFFSET;
    result = p_output->ScheduleVideoFrame(pDLVideoFrame, scheduleTime, length, CLOCK_FREQ);
    if (result != S_OK) {
        msg_Err(p_stream, "Dropped Video frame %" PRId64 ": 0x%x",
                picture->date, result);
        goto error;
    }
    lasttimestamp = __MAX(scheduleTime, lasttimestamp);

    if(!b_running) /* preroll */
    {
        if(streamStartTime == VLC_TICK_INVALID ||
           picture->date < streamStartTime)
            streamStartTime = picture->date;
    }

end:
    ret = VLC_SUCCESS;

error:
    if(p_cc)
        block_Release(p_cc);

    picture_Release(picture);
    if (pDLVideoFrame)
        pDLVideoFrame->Release();

    return ret;
}

void DBMSDIOutput::checkClockDrift()
{
    BMDTimeValue hardwareTime, timeInFrame, ticksPerFrame;
    if(!b_running || FAKE_DRIVER)
        return;
    if(S_OK == p_output->GetHardwareReferenceClock(CLOCK_FREQ,
                                                   &hardwareTime,
                                                   &timeInFrame,
                                                   &ticksPerFrame))
    {
        if(clock.system_reference == VLC_TICK_INVALID)
        {
            clock.system_reference = vlc_tick_now();
            clock.hardware_reference = hardwareTime;
        }
        else
        {
            vlc_tick_t elapsed_system = vlc_tick_now() - clock.system_reference;
            BMDTimeValue elapsed_hardware = hardwareTime - clock.hardware_reference;
            if(std::abs(elapsed_system - elapsed_hardware) >
                    std::abs(clock.offset) + VLC_TICK_FROM_MS(15))
            {
                clock.offset = elapsed_system - elapsed_hardware;
                msg_Info(p_stream, "offset now %" PRId64 " ms", clock.offset / 1000);
            }
        }
    }
}

bool DBMSDIOutput::isDrained()
{
    if(b_running)
    {
        if(!videoStream->isEOS() || !videoBuffer.isEOS())
            return false;
    }

    return true;
}
