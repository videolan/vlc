/*****************************************************************************
 * decklink.cpp: BlackMagic DeckLink SDI input module
 *****************************************************************************
 * Copyright (C) 2010 Steinar H. Gunderson
 *
 * Authors: Steinar H. Gunderson <steinar+vlc@gunderson.no>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#define __STDC_CONSTANT_MACROS 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_atomic.h>

#include <arpa/inet.h>

#include <DeckLinkAPI.h>
#include <DeckLinkAPIDispatch.cpp>

static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define CARD_INDEX_TEXT N_("Input card to use")
#define CARD_INDEX_LONGTEXT N_( \
    "DeckLink capture card to use, if multiple exist. " \
    "The cards are numbered from 0.")

#define MODE_TEXT N_("Desired input video mode")
#define MODE_LONGTEXT N_( \
    "Desired input video mode for DeckLink captures. " \
    "This value should be a FOURCC code in textual " \
    "form, e.g. \"ntsc\".")

#define AUDIO_CONNECTION_TEXT N_("Audio connection")
#define AUDIO_CONNECTION_LONGTEXT N_( \
    "Audio connection to use for DeckLink captures. " \
    "Valid choices: embedded, aesebu, analog. " \
    "Leave blank for card default.")

#define RATE_TEXT N_("Audio sampling rate in Hz")
#define RATE_LONGTEXT N_( \
    "Audio sampling rate (in hertz) for DeckLink captures. " \
    "0 disables audio input.")

#define CHANNELS_TEXT N_("Number of audio channels")
#define CHANNELS_LONGTEXT N_( \
    "Number of input audio channels for DeckLink captures. " \
    "Must be 2, 8 or 16. 0 disables audio input.")

#define VIDEO_CONNECTION_TEXT N_("Video connection")
#define VIDEO_CONNECTION_LONGTEXT N_( \
    "Video connection to use for DeckLink captures. " \
    "Valid choices: sdi, hdmi, opticalsdi, component, " \
    "composite, svideo. " \
    "Leave blank for card default.")

static const char *const ppsz_videoconns[] = {
    "sdi", "hdmi", "opticalsdi", "component", "composite", "svideo"
};
static const char *const ppsz_videoconns_text[] = {
    N_("SDI"), N_("HDMI"), N_("Optical SDI"), N_("Component"), N_("Composite"), N_("S-video")
};

static const char *const ppsz_audioconns[] = {
    "embedded", "aesebu", "analog"
};
static const char *const ppsz_audioconns_text[] = {
    N_("Embedded"), N_("AES/EBU"), N_("Analog")
};

#define ASPECT_RATIO_TEXT N_("Aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_(\
    "Aspect ratio (4:3, 16:9). Default assumes square pixels.")

vlc_module_begin ()
    set_shortname(N_("DeckLink"))
    set_description(N_("Blackmagic DeckLink SDI input"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)

    add_integer("decklink-card-index", 0,
                 CARD_INDEX_TEXT, CARD_INDEX_LONGTEXT, true)
    add_string("decklink-mode", "pal ",
                 MODE_TEXT, MODE_LONGTEXT, true)
    add_string("decklink-audio-connection", 0,
                 AUDIO_CONNECTION_TEXT, AUDIO_CONNECTION_LONGTEXT, true)
        change_string_list(ppsz_audioconns, ppsz_audioconns_text)
    add_integer("decklink-audio-rate", 48000,
                 RATE_TEXT, RATE_LONGTEXT, true)
    add_integer("decklink-audio-channels", 2,
                 CHANNELS_TEXT, CHANNELS_LONGTEXT, true)
    add_string("decklink-video-connection", 0,
                 VIDEO_CONNECTION_TEXT, VIDEO_CONNECTION_LONGTEXT, true)
        change_string_list(ppsz_videoconns, ppsz_videoconns_text)
    add_string("decklink-aspect-ratio", NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_LONGTEXT, true)

    add_shortcut("decklink")
    set_capability("access_demux", 10)
    set_callbacks(Open, Close)
vlc_module_end ()

static int Control(demux_t *, int, va_list);

class DeckLinkCaptureDelegate;

struct demux_sys_t
{
    IDeckLink *card;
    IDeckLinkInput *input;
    DeckLinkCaptureDelegate *delegate;

    /* We need to hold onto the IDeckLinkConfiguration object, or our settings will not apply.
       See section 2.4.15 of the Blackmagic Decklink SDK documentation. */
    IDeckLinkConfiguration *config;

    es_out_id_t *video_es;
    es_out_id_t *audio_es;

    vlc_mutex_t pts_lock;
    int last_pts;  /* protected by <pts_lock> */

    uint32_t dominance_flags;
    int channels;
};

class DeckLinkCaptureDelegate : public IDeckLinkInputCallback
{
public:
    DeckLinkCaptureDelegate(demux_t *demux) : demux_(demux)
    {
        vlc_atomic_set(&m_ref_, 1);
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, LPVOID *) { return E_NOINTERFACE; }

    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return vlc_atomic_inc(&m_ref_);
    }

    virtual ULONG STDMETHODCALLTYPE Release(void)
    {
        uintptr_t new_ref = vlc_atomic_dec(&m_ref_);
        if (new_ref == 0)
            delete this;
        return new_ref;
    }

    virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode*, BMDDetectedVideoInputFormatFlags)
    {
        msg_Dbg(demux_, "Video input format changed");
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame*, IDeckLinkAudioInputPacket*);

private:
    vlc_atomic_t m_ref_;
    demux_t *demux_;
};

HRESULT DeckLinkCaptureDelegate::VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioFrame)
{
    demux_sys_t *sys = demux_->p_sys;

    if (videoFrame) {
        if (videoFrame->GetFlags() & bmdFrameHasNoInputSource) {
            msg_Warn(demux_, "No input signal detected");
            return S_OK;
        }

        const int width = videoFrame->GetWidth();
        const int height = videoFrame->GetHeight();
        const int stride = videoFrame->GetRowBytes();
        const int bpp = 2;

        block_t *video_frame = block_New(demux_, width * height * bpp);
        if (!video_frame)
            return S_OK;

        void *frame_bytes;
        videoFrame->GetBytes(&frame_bytes);
        for (int y = 0; y < height; ++y) {
            const uint8_t *src = (const uint8_t *)frame_bytes + stride * y;
            uint8_t *dst = video_frame->p_buffer + width * bpp * y;
            memcpy(dst, src, width * bpp);
        }

        BMDTimeValue stream_time, frame_duration;
        videoFrame->GetStreamTime(&stream_time, &frame_duration, CLOCK_FREQ);
        video_frame->i_flags = BLOCK_FLAG_TYPE_I | sys->dominance_flags;
        video_frame->i_pts = video_frame->i_dts = VLC_TS_0 + stream_time;

        vlc_mutex_lock(&sys->pts_lock);
        if (video_frame->i_pts > sys->last_pts)
            sys->last_pts = video_frame->i_pts;
        vlc_mutex_unlock(&sys->pts_lock);

        es_out_Control(demux_->out, ES_OUT_SET_PCR, video_frame->i_pts);
        es_out_Send(demux_->out, sys->video_es, video_frame);
    }

    if (audioFrame) {
        const int bytes = audioFrame->GetSampleFrameCount() * sizeof(int16_t) * sys->channels;

        block_t *audio_frame = block_New(demux_, bytes);
        if (!audio_frame)
            return S_OK;

        void *frame_bytes;
        audioFrame->GetBytes(&frame_bytes);
        memcpy(audio_frame->p_buffer, frame_bytes, bytes);

        BMDTimeValue packet_time;
        audioFrame->GetPacketTime(&packet_time, CLOCK_FREQ);
        audio_frame->i_pts = audio_frame->i_dts = VLC_TS_0 + packet_time;

        vlc_mutex_lock(&sys->pts_lock);
        if (audio_frame->i_pts > sys->last_pts)
            sys->last_pts = audio_frame->i_pts;
        vlc_mutex_unlock(&sys->pts_lock);
        if (audio_frame->i_pts > sys->last_pts)

        es_out_Control(demux_->out, ES_OUT_SET_PCR, audio_frame->i_pts);
        es_out_Send(demux_->out, sys->audio_es, audio_frame);
    }

    return S_OK;
}


static int GetAudioConn(demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;

    char *opt = var_CreateGetNonEmptyString(demux, "decklink-audio-connection");
    if (!opt)
        return VLC_SUCCESS;

    BMDAudioConnection c;
    if (!strcmp(opt, "embedded"))
        c = bmdAudioConnectionEmbedded;
    else if (!strcmp(opt, "aesebu"))
        c = bmdAudioConnectionAESEBU;
    else if (!strcmp(opt, "analog"))
        c = bmdAudioConnectionAnalog;
    else {
        msg_Err(demux, "Invalid audio-connection: `%s\' specified", opt);
        free(opt);
        return VLC_EGENERIC;
    }

    if (sys->config->SetInt(bmdDeckLinkConfigAudioInputConnection, c) != S_OK) {
        msg_Err(demux, "Failed to set audio input connection");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int GetVideoConn(demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;

    char *opt = var_InheritString(demux, "decklink-video-connection");
    if (!opt)
        return VLC_SUCCESS;

    BMDVideoConnection c;
    if (!strcmp(opt, "sdi"))
        c = bmdVideoConnectionSDI;
    else if (!strcmp(opt, "hdmi"))
        c = bmdVideoConnectionHDMI;
    else if (!strcmp(opt, "opticalsdi"))
        c = bmdVideoConnectionOpticalSDI;
    else if (!strcmp(opt, "component"))
        c = bmdVideoConnectionComponent;
    else if (!strcmp(opt, "composite"))
        c = bmdVideoConnectionComposite;
    else if (!strcmp(opt, "svideo"))
        c = bmdVideoConnectionSVideo;
    else {
        msg_Err(demux, "Invalid video-connection: `%s\' specified", opt);
        free(opt);
        return VLC_EGENERIC;
    }

    free(opt);
    if (sys->config->SetInt(bmdDeckLinkConfigVideoInputConnection, c) != S_OK) {
        msg_Err(demux, "Failed to set video input connection");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


static int Open(vlc_object_t *p_this)
{
    demux_t     *demux = (demux_t*)p_this;
    demux_sys_t *sys;
    int         ret = VLC_EGENERIC;
    char        *display_mode = NULL;
    bool        b_found_mode;
    int         card_index;
    int         width, height, fps_num, fps_den;
    int         rate;
    unsigned    aspect_num, aspect_den;

    /* Only when selected */
    if (*demux->psz_access == '\0')
        return VLC_EGENERIC;

    /* Set up demux */
    demux->pf_demux = NULL;
    demux->pf_control = Control;
    demux->info.i_update = 0;
    demux->info.i_title = 0;
    demux->info.i_seekpoint = 0;
    demux->p_sys = sys = (demux_sys_t*)calloc(1, sizeof(demux_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    vlc_mutex_init(&sys->pts_lock);

    IDeckLinkDisplayModeIterator *display_iterator = NULL;

    IDeckLinkIterator *decklink_iterator = CreateDeckLinkIteratorInstance();
    if (!decklink_iterator) {
        msg_Err(demux, "DeckLink drivers not found.");
        goto finish;
    }

    card_index = var_InheritInteger(demux, "decklink-card-index");
    if (card_index < 0) {
        msg_Err(demux, "Invalid card index %d", card_index);
        goto finish;
    }

    for (int i = 0; i <= card_index; i++) {
        if (sys->card)
            sys->card->Release();
        if (decklink_iterator->Next(&sys->card) != S_OK) {
            msg_Err(demux, "DeckLink PCI card %d not found", card_index);
            goto finish;
        }
    }

    const char *model_name;
    if (sys->card->GetModelName(&model_name) != S_OK) {
        msg_Err(demux, "Could not get model name");
        goto finish;
    }

    msg_Dbg(demux, "Opened DeckLink PCI card %d (%s)", card_index, model_name);

    if (sys->card->QueryInterface(IID_IDeckLinkInput, (void**)&sys->input) != S_OK) {
        msg_Err(demux, "Card has no inputs");
        goto finish;
    }

    /* Set up the video and audio sources. */
    if (sys->card->QueryInterface(IID_IDeckLinkConfiguration, (void**)&sys->config) != S_OK) {
        msg_Err(demux, "Failed to get configuration interface");
        goto finish;
    }

    if (GetVideoConn(demux) || GetAudioConn(demux))
        goto finish;

    /* Get the list of display modes. */
    if (sys->input->GetDisplayModeIterator(&display_iterator) != S_OK) {
        msg_Err(demux, "Failed to enumerate display modes");
        goto finish;
    }

    display_mode = var_CreateGetNonEmptyString(demux, "decklink-mode");
    if (!display_mode || strlen(display_mode) > 4) {
        msg_Err(demux, "Missing or invalid --decklink-mode string");
        goto finish;
    }

    /*
     * Pad the --decklink-mode string to four characters, so the user can specify e.g. "pal"
     * without having to add the trailing space.
     */
    char sz_display_mode_padded[5];
    strcpy(sz_display_mode_padded, "    ");
    for (unsigned i = 0; i < strlen(display_mode); ++i)
        sz_display_mode_padded[i] = display_mode[i];

    BMDDisplayMode wanted_mode_id;
    memcpy(&wanted_mode_id, &sz_display_mode_padded, sizeof(wanted_mode_id));

    b_found_mode = false;

    for (;;) {
        IDeckLinkDisplayMode *display_mode;
        if ((display_iterator->Next(&display_mode) != S_OK) || !display_mode)
            break;

        char sz_mode_id_text[5] = {0};
        BMDDisplayMode mode_id = ntohl(display_mode->GetDisplayMode());
        memcpy(sz_mode_id_text, &mode_id, sizeof(mode_id));

        const char *mode_name;
        if (display_mode->GetName(&mode_name) != S_OK) {
            msg_Err(demux, "Failed to get display mode name");
            display_mode->Release();
            goto finish;
        }

        BMDTimeValue frame_duration, time_scale;
        if (display_mode->GetFrameRate(&frame_duration, &time_scale) != S_OK) {
            msg_Err(demux, "Failed to get frame rate");
            display_mode->Release();
            goto finish;
        }

        const char *field_dominance;
        uint32_t dominance_flags = 0;
        switch(display_mode->GetFieldDominance())
        {
        case bmdProgressiveFrame:
            field_dominance = "";
            break;
        case bmdProgressiveSegmentedFrame:
            field_dominance = ", segmented";
            break;
        case bmdLowerFieldFirst:
            field_dominance = ", interlaced [BFF]";
            dominance_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            break;
        case bmdUpperFieldFirst:
            field_dominance = ", interlaced [TFF]";
            dominance_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case bmdUnknownFieldDominance:
        default:
            field_dominance = ", unknown field dominance";
            break;
        }

        msg_Dbg(demux, "Found mode '%s': %s (%dx%d, %.3f fps%s)",
                 sz_mode_id_text, mode_name,
                 (int)display_mode->GetWidth(), (int)display_mode->GetHeight(),
                 double(time_scale) / frame_duration, field_dominance);

        if (wanted_mode_id == mode_id) {
            b_found_mode = true;
            width = display_mode->GetWidth();
            height = display_mode->GetHeight();
            fps_num = time_scale;
            fps_den = frame_duration;
            sys->dominance_flags = dominance_flags;
        }

        display_mode->Release();
    }

    if (!b_found_mode) {
        msg_Err(demux, "Unknown video mode specified. "
                 "Run VLC with -vv to get a list of supported modes.");
        goto finish;
    }

    if (sys->input->EnableVideoInput(htonl(wanted_mode_id), bmdFormat8BitYUV, 0) != S_OK) {
        msg_Err(demux, "Failed to enable video input");
        goto finish;
    }

    /* Set up audio. */
    sys->channels = var_InheritInteger(demux, "decklink-audio-channels");
    rate = var_InheritInteger(demux, "decklink-audio-rate");
    if (rate > 0 && sys->channels > 0) {
        if (sys->input->EnableAudioInput(rate, bmdAudioSampleType16bitInteger, sys->channels) != S_OK) {
            msg_Err(demux, "Failed to enable audio input");
            goto finish;
        }
    }

    sys->delegate = new DeckLinkCaptureDelegate(demux);
    sys->input->SetCallback(sys->delegate);

    if (sys->input->StartStreams() != S_OK) {
        msg_Err(demux, "Could not start streaming from SDI card. This could be caused "
                          "by invalid video mode or flags, access denied, or card already in use.");
        goto finish;
    }

    /* Declare elementary streams */
    es_format_t video_fmt;
    es_format_Init(&video_fmt, VIDEO_ES, VLC_CODEC_UYVY);
    video_fmt.video.i_width = width;
    video_fmt.video.i_height = height;
    video_fmt.video.i_sar_num = 1;
    video_fmt.video.i_sar_den = 1;
    video_fmt.video.i_frame_rate = fps_num;
    video_fmt.video.i_frame_rate_base = fps_den;
    video_fmt.i_bitrate = video_fmt.video.i_width * video_fmt.video.i_height * video_fmt.video.i_frame_rate * 2 * 8;

    if (!var_InheritURational(demux, &aspect_num, &aspect_den, "decklink-aspect-ratio") &&
         aspect_num > 0 && aspect_den > 0) {
        video_fmt.video.i_sar_num = aspect_num * video_fmt.video.i_height;
        video_fmt.video.i_sar_den = aspect_den * video_fmt.video.i_width;
    }

    msg_Dbg(demux, "added new video es %4.4s %dx%d",
             (char*)&video_fmt.i_codec, video_fmt.video.i_width, video_fmt.video.i_height);
    sys->video_es = es_out_Add(demux->out, &video_fmt);

    es_format_t audio_fmt;
    es_format_Init(&audio_fmt, AUDIO_ES, VLC_CODEC_S16N);
    audio_fmt.audio.i_channels = sys->channels;
    audio_fmt.audio.i_rate = rate;
    audio_fmt.audio.i_bitspersample = 16;
    audio_fmt.audio.i_blockalign = audio_fmt.audio.i_channels * audio_fmt.audio.i_bitspersample / 8;
    audio_fmt.i_bitrate = audio_fmt.audio.i_channels * audio_fmt.audio.i_rate * audio_fmt.audio.i_bitspersample;

    msg_Dbg(demux, "added new audio es %4.4s %dHz %dbpp %dch",
             (char*)&audio_fmt.i_codec, audio_fmt.audio.i_rate, audio_fmt.audio.i_bitspersample, audio_fmt.audio.i_channels);
    sys->audio_es = es_out_Add(demux->out, &audio_fmt);

    ret = VLC_SUCCESS;

finish:
    if (decklink_iterator)
        decklink_iterator->Release();

    free(display_mode);

    if (display_iterator)
        display_iterator->Release();

    if (ret != VLC_SUCCESS)
        Close(p_this);

    return ret;
}

static void Close(vlc_object_t *p_this)
{
    demux_t     *demux = (demux_t *)p_this;
    demux_sys_t *sys   = demux->p_sys;

    if (sys->config)
        sys->config->Release();

    if (sys->input) {
        sys->input->StopStreams();
        sys->input->Release();
    }

    if (sys->card)
        sys->card->Release();

    if (sys->delegate)
        sys->delegate->Release();

    vlc_mutex_destroy(&sys->pts_lock);
    free(sys);
}

static int Control(demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;
    bool *pb;
    int64_t *pi64;

    switch(query)
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            pb = (bool*)va_arg(args, bool *);
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg(args, int64_t *);
            *pi64 = INT64_C(1000) * var_InheritInteger(demux, "live-caching");
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg(args, int64_t *);
            vlc_mutex_lock(&sys->pts_lock);
            *pi64 = sys->last_pts;
            vlc_mutex_unlock(&sys->pts_lock);
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}
