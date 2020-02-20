/*****************************************************************************
 * decklink.cpp: BlackMagic DeckLink SDI input module
 *****************************************************************************
 * Copyright (C) 2010 Steinar H. Gunderson
 * Copyright (C) 2012-2014 Rafaël Carré
 *
 * Authors: Steinar H. Gunderson <steinar+vlc@gunderson.no>
            Rafaël Carré <funman@videolanorg>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "vlc_decklink.h"
#include <DeckLinkAPIDispatch.cpp>
#include <DeckLinkAPIVersion.h>
#if BLACKMAGIC_DECKLINK_API_VERSION < 0x0b010000
 #define IID_IDeckLinkProfileAttributes IID_IDeckLinkAttributes
 #define IDeckLinkProfileAttributes IDeckLinkAttributes
#endif

#include "sdi.h"

#include <atomic>

static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define CARD_INDEX_TEXT N_("Input card to use")
#define CARD_INDEX_LONGTEXT N_( \
    "DeckLink capture card to use, if multiple exist. " \
    "The cards are numbered from 0.")

#define MODE_TEXT N_("Desired input video mode. Leave empty for autodetection.")
#define MODE_LONGTEXT N_( \
    "Desired input video mode for DeckLink captures. " \
    "This value should be a FOURCC code in textual " \
    "form, e.g. \"ntsc\".")

#define AUDIO_CONNECTION_TEXT N_("Audio connection")
#define AUDIO_CONNECTION_LONGTEXT N_( \
    "Audio connection to use for DeckLink captures. " \
    "Valid choices: embedded, aesebu, analog. " \
    "Leave blank for card default.")

#define RATE_TEXT N_("Audio samplerate (Hz)")
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
    N_("SDI"), N_("HDMI"), N_("Optical SDI"), N_("Component"), N_("Composite"), N_("S-Video")
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
    add_string("decklink-mode", NULL,
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
    add_bool("decklink-tenbits", false, N_("10 bits"), N_("10 bits"), true)

    add_shortcut("decklink")
    set_capability("access", 0)
    set_callbacks(Open, Close)
vlc_module_end ()

static int Control(demux_t *, int, va_list);

namespace {

class DeckLinkCaptureDelegate;

struct demux_sys_t
{
    IDeckLink *card;
    IDeckLinkInput *input;
    DeckLinkCaptureDelegate *delegate;

    /* We need to hold onto the IDeckLinkConfiguration object, or our settings will not apply.
       See section 2.4.15 of the Blackmagic DeckLink SDK documentation. */
    IDeckLinkConfiguration *config;
    IDeckLinkProfileAttributes *attributes;

    bool autodetect;

    es_out_id_t *video_es;
    es_format_t video_fmt;
    es_out_id_t *audio_es[8];
    es_out_id_t *cc_es;

    vlc_mutex_t pts_lock;
    vlc_tick_t last_pts;  /* protected by <pts_lock> */

    uint32_t dominance_flags;
    int channels;
    int audio_streams;

    bool tenbits;
};

} // namespace

static const char *GetFieldDominance(BMDFieldDominance dom, uint32_t *flags)
{
    switch(dom)
    {
        case bmdProgressiveFrame:
            return "";
        case bmdProgressiveSegmentedFrame:
            return ", segmented";
        case bmdLowerFieldFirst:
            *flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            return ", interlaced [BFF]";
        case bmdUpperFieldFirst:
            *flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            return ", interlaced [TFF]";
        case bmdUnknownFieldDominance:
        default:
            return ", unknown field dominance";
    }
}

static es_format_t GetModeSettings(demux_t *demux, IDeckLinkDisplayMode *m,
        BMDDetectedVideoInputFormatFlags fmt_flags)
{
    demux_sys_t *sys = (demux_sys_t *)demux->p_sys;
    uint32_t flags = 0;
    (void)GetFieldDominance(m->GetFieldDominance(), &flags);

    BMDTimeValue frame_duration, time_scale;
    if (m->GetFrameRate(&frame_duration, &time_scale) != S_OK) {
        time_scale = 0;
        frame_duration = 1;
    }

    es_format_t video_fmt;
    vlc_fourcc_t chroma = 0;
    switch (fmt_flags) {
        case bmdDetectedVideoInputYCbCr422:
            chroma = sys->tenbits ? VLC_CODEC_I422_10L : VLC_CODEC_UYVY;
            break;
        case bmdDetectedVideoInputRGB444:
            chroma = VLC_CODEC_ARGB;
            break;
        default:
            msg_Err(demux, "Unsupported input format");
            break;
    }
    es_format_Init(&video_fmt, VIDEO_ES, chroma);

    video_fmt.video.i_chroma = chroma;
    video_fmt.video.i_width = m->GetWidth();
    video_fmt.video.i_height = m->GetHeight();
    video_fmt.video.i_sar_num = 1;
    video_fmt.video.i_sar_den = 1;
    video_fmt.video.i_frame_rate = time_scale;
    video_fmt.video.i_frame_rate_base = frame_duration;
    video_fmt.i_bitrate = video_fmt.video.i_width * video_fmt.video.i_height * video_fmt.video.i_frame_rate * 2 * 8;

    unsigned aspect_num, aspect_den;
    if (!var_InheritURational(demux, &aspect_num, &aspect_den, "decklink-aspect-ratio") &&
         aspect_num > 0 && aspect_den > 0) {
        video_fmt.video.i_sar_num = aspect_num * video_fmt.video.i_height;
        video_fmt.video.i_sar_den = aspect_den * video_fmt.video.i_width;
    }

    sys->dominance_flags = flags;

    return video_fmt;
}
namespace {

class DeckLinkCaptureDelegate : public IDeckLinkInputCallback
{
public:
    DeckLinkCaptureDelegate(demux_t *demux) : demux_(demux)
    {
        m_ref_.store(1);
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, LPVOID *) { return E_NOINTERFACE; }

    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return m_ref_.fetch_add(1);
    }

    virtual ULONG STDMETHODCALLTYPE Release(void)
    {
        uintptr_t new_ref = m_ref_.fetch_sub(1);
        if (new_ref == 0)
            delete this;
        return new_ref;
    }

    virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events, IDeckLinkDisplayMode *mode, BMDDetectedVideoInputFormatFlags flags)
    {
        demux_sys_t *sys = static_cast<demux_sys_t *>(demux_->p_sys);

        if( !(events & bmdVideoInputDisplayModeChanged ))
            return S_OK;

        decklink_str_t tmp_name;
        char *mode_name = NULL;
        if (mode->GetName(&tmp_name) == S_OK) {
            mode_name = DECKLINK_STRDUP(tmp_name);
            DECKLINK_FREE(tmp_name);
        }

        msg_Dbg(demux_, "Video input format changed to %s",
            (mode_name) ? mode_name : "unknown");
        free(mode_name);
        if (!sys->autodetect) {
            msg_Err(demux_, "Video format detection disabled");
            return S_OK;
        }

        BMDPixelFormat fmt = 0;
        switch (flags) {
            case bmdDetectedVideoInputYCbCr422:
                fmt = sys->tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV;
                break;
            case bmdDetectedVideoInputRGB444:
                fmt = bmdFormat8BitARGB;
                break;
            default:
                msg_Err(demux_, "Unsupported input format");
                return S_OK;
        }

        es_out_Del(demux_->out, sys->video_es);
        sys->video_fmt = GetModeSettings(demux_, mode, flags);
        sys->video_es = es_out_Add(demux_->out, &sys->video_fmt);

        sys->input->PauseStreams();
        sys->input->EnableVideoInput( mode->GetDisplayMode(), fmt, bmdVideoInputEnableFormatDetection );
        sys->input->FlushStreams();
        sys->input->StartStreams();

        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame*, IDeckLinkAudioInputPacket*);

private:
    std::atomic_uint m_ref_;
    demux_t *demux_;
};

} // namespace

HRESULT DeckLinkCaptureDelegate::VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioFrame)
{
    demux_sys_t *sys = static_cast<demux_sys_t *>(demux_->p_sys);

    if (videoFrame) {
        if (videoFrame->GetFlags() & bmdFrameHasNoInputSource) {
            msg_Warn(demux_, "No input signal detected (%ldx%ld)",
                     videoFrame->GetWidth(), videoFrame->GetHeight());
            return S_OK;
        }

        const int width = videoFrame->GetWidth();
        const int height = videoFrame->GetHeight();
        const int stride = videoFrame->GetRowBytes();

        int bpp = 0;
        switch (sys->video_fmt.i_codec) {
            case VLC_CODEC_I422_10L:
            case VLC_CODEC_ARGB:
                bpp = 4;
                break;
            case VLC_CODEC_UYVY:
                bpp = 2;
                break;
        };
        block_t *video_frame = block_Alloc(width * height * bpp);
        if (!video_frame)
            return S_OK;

        const uint32_t *frame_bytes;
        videoFrame->GetBytes((void**)&frame_bytes);

        BMDTimeValue stream_time, frame_duration;
        videoFrame->GetStreamTime(&stream_time, &frame_duration, CLOCK_FREQ);
        video_frame->i_flags = BLOCK_FLAG_TYPE_I | sys->dominance_flags;
        video_frame->i_pts = video_frame->i_dts = VLC_TICK_0 + stream_time;

        if (sys->video_fmt.i_codec == VLC_CODEC_I422_10L) {
            v210_convert((uint16_t*)video_frame->p_buffer, frame_bytes, width, height);
            IDeckLinkVideoFrameAncillary *vanc;
            if (videoFrame->GetAncillaryData(&vanc) == S_OK) {
                for (int i = 1; i < 21; i++) {
                    uint32_t *buf;
                    if (vanc->GetBufferForVerticalBlankingLine(i, (void**)&buf) != S_OK)
                        break;
                    uint16_t dec[width * 2];
                    v210_convert(&dec[0], buf, width, 1);
                    block_t *cc = vanc_to_cc(demux_, dec, width * 2);
                    if (!cc)
                        continue;
                    cc->i_pts = cc->i_dts = VLC_TICK_0 + stream_time;

                    if (!sys->cc_es) {
                        es_format_t fmt;

                        es_format_Init( &fmt, SPU_ES, VLC_CODEC_CEA608 );
                        fmt.psz_description = strdup(N_("Closed captions 1"));
                        if (fmt.psz_description) {
                            sys->cc_es = es_out_Add(demux_->out, &fmt);
                            msg_Dbg(demux_, "Adding Closed captions stream");
                        }
                    }
                    if (sys->cc_es)
                        es_out_Send(demux_->out, sys->cc_es, cc);
                    else
                        block_Release(cc);
                    break; // we found the line with Closed Caption data
                }
                vanc->Release();
            }
        } else if (sys->video_fmt.i_codec == VLC_CODEC_UYVY) {
            for (int y = 0; y < height; ++y) {
                const uint8_t *src = (const uint8_t *)frame_bytes + stride * y;
                uint8_t *dst = video_frame->p_buffer + width * 2 * y;
                memcpy(dst, src, width * 2);
            }
        } else {
                memcpy(video_frame->p_buffer, frame_bytes, width * height * bpp);
        }

        vlc_mutex_lock(&sys->pts_lock);
        if (video_frame->i_pts > sys->last_pts)
            sys->last_pts = video_frame->i_pts;
        vlc_mutex_unlock(&sys->pts_lock);

        es_out_SetPCR(demux_->out, video_frame->i_pts);
        es_out_Send(demux_->out, sys->video_es, video_frame);
    }

    if (audioFrame && audioFrame->GetSampleFrameCount())
    {
        const int bytes = audioFrame->GetSampleFrameCount() * sizeof(int16_t) * sys->channels;
        BMDTimeValue packet_time;
        void *frame_bytes;

        audioFrame->GetBytes(&frame_bytes);
        audioFrame->GetPacketTime(&packet_time, CLOCK_FREQ);

        if(sys->audio_streams > 1)
        {
            for(int i=0; i<sys->audio_streams; i++)
            {
                size_t i_samples = bytes / (sys->audio_streams * 4);
                block_t *p_frame = block_Alloc(i_samples * 4);
                if (!p_frame)
                    continue;

                for(size_t j=0; j<i_samples; j++) /* for each pair sample */
                {
                    memcpy(&p_frame->p_buffer[j * 4],
                           &reinterpret_cast<uint8_t *>(frame_bytes)[(j * sys->audio_streams + i) * 4],
                           4);
                }

                p_frame->i_pts = p_frame->i_dts = VLC_TICK_0 + packet_time;
                es_out_Send(demux_->out, sys->audio_es[i], p_frame);
            }
        }
        else
        {
            block_t *audio_frame = block_Alloc(bytes);
            if (!audio_frame)
                return S_OK;
            memcpy(audio_frame->p_buffer, frame_bytes, bytes);
            audio_frame->i_pts = audio_frame->i_dts = VLC_TICK_0 + packet_time;
            es_out_Send(demux_->out, sys->audio_es[0], audio_frame);
        }

        vlc_mutex_lock(&sys->pts_lock);
        if (VLC_TICK_0 + packet_time > sys->last_pts)
            sys->last_pts = VLC_TICK_0 + packet_time;
        vlc_mutex_unlock(&sys->pts_lock);

        es_out_SetPCR(demux_->out, VLC_TICK_0 + packet_time);
    }

    return S_OK;
}


static int GetAudioConn(demux_t *demux)
{
    demux_sys_t *sys = (demux_sys_t *)demux->p_sys;

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
    demux_sys_t *sys = (demux_sys_t *)demux->p_sys;

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
    int         card_index;
    int         physical_channels = 0;
    int         rate;
    BMDVideoInputFlags flags = bmdVideoInputFlagDefault;

    if (demux->out == NULL)
        return VLC_EGENERIC;

    /* Set up demux */
    demux->pf_demux = NULL;
    demux->pf_control = Control;
    demux->p_sys = sys = (demux_sys_t*)calloc(1, sizeof(demux_sys_t));
    if (!sys)
        return VLC_ENOMEM;

    vlc_mutex_init(&sys->pts_lock);

    sys->tenbits = var_InheritBool(p_this, "decklink-tenbits");

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

    decklink_str_t tmp_name;
    char *model_name;
    if (sys->card->GetModelName(&tmp_name) != S_OK) {
        model_name = strdup("unknown");
    } else {
        model_name = DECKLINK_STRDUP(tmp_name);
        DECKLINK_FREE(tmp_name);
    }

    msg_Dbg(demux, "Opened DeckLink PCI card %d (%s)", card_index, model_name);
    free(model_name);

    if (sys->card->QueryInterface(IID_IDeckLinkInput, (void**)&sys->input) != S_OK) {
        msg_Err(demux, "Card has no inputs");
        goto finish;
    }

    /* Set up the video and audio sources. */
    if (sys->card->QueryInterface(IID_IDeckLinkConfiguration, (void**)&sys->config) != S_OK) {
        msg_Err(demux, "Failed to get configuration interface");
        goto finish;
    }

    if (sys->card->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&sys->attributes) != S_OK) {
        msg_Err(demux, "Failed to get attributes interface");
        goto finish;
    }

    if (GetVideoConn(demux) || GetAudioConn(demux))
        goto finish;

    BMDPixelFormat fmt;
    fmt = sys->tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV;
    if (sys->attributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &sys->autodetect) != S_OK) {
        msg_Err(demux, "Failed to query card attribute");
        goto finish;
    }

    /* Get the list of display modes. */
    IDeckLinkDisplayModeIterator *mode_it;
    if (sys->input->GetDisplayModeIterator(&mode_it) != S_OK) {
        msg_Err(demux, "Failed to enumerate display modes");
        goto finish;
    }

    union {
        BMDDisplayMode id;
        char str[4];
    } u;

    u.id = 0;

    char *mode;
    mode = var_CreateGetNonEmptyString(demux, "decklink-mode");
    if (mode)
        sys->autodetect = false; // disable autodetection if mode was set

    if (sys->autodetect) {
        msg_Dbg(demux, "Card supports input format detection");
        flags |= bmdVideoInputEnableFormatDetection;
        /* Enable a random format, we will reconfigure on format detection */
        u.id = htonl(bmdModeHD1080p2997);
    } else {
        if (!mode || strlen(mode) < 3 || strlen(mode) > 4) {
            msg_Err(demux, "Invalid mode: \'%s\'", mode ? mode : "");
            free(mode);
            goto finish;
        }

        msg_Dbg(demux, "Looking for mode \'%s\'", mode);
        memcpy(u.str, mode, 4);
        if (u.str[3] == '\0')
            u.str[3] = ' '; /* 'pal'\0 -> 'pal ' */
        free(mode);
    }

    sys->video_fmt.video.i_width = 0;

    for (IDeckLinkDisplayMode *m;; m->Release()) {
        if ((mode_it->Next(&m) != S_OK) || !m)
            break;

        const char *mode_name;
        BMDTimeValue frame_duration, time_scale;
        uint32_t field_flags;
        const char *field = GetFieldDominance(m->GetFieldDominance(), &field_flags);
        BMDDisplayMode id = ntohl(m->GetDisplayMode());
        decklink_str_t tmp_name;

        if (m->GetName(&tmp_name) != S_OK) {
            mode_name = "unknown";
        } else {
            mode_name = DECKLINK_STRDUP(tmp_name);
            DECKLINK_FREE(tmp_name);
        }
        if (m->GetFrameRate(&frame_duration, &time_scale) != S_OK) {
            time_scale = 0;
            frame_duration = 1;
        }

        msg_Dbg(demux, "Found mode '%4.4s': %s (%dx%d, %.3f fps%s)",
                 (char*)&id, mode_name,
                 (int)m->GetWidth(), (int)m->GetHeight(),
                 double(time_scale) / frame_duration, field);

        if (u.id == id) {
            sys->video_fmt = GetModeSettings(demux, m, bmdDetectedVideoInputYCbCr422);
            msg_Dbg(demux, "Using that mode");
        }
    }

    mode_it->Release();

    if (sys->video_fmt.video.i_width == 0) {
        msg_Err(demux, "Unknown video mode `%4.4s\' specified.", (char*)&u.id);
        goto finish;
    }

    if (sys->input->EnableVideoInput(htonl(u.id), fmt, flags) != S_OK) {
        msg_Err(demux, "Failed to enable video input");
        goto finish;
    }

    /* Set up audio. */
    sys->channels = var_InheritInteger(demux, "decklink-audio-channels");
    switch (sys->channels) {
    case 0:
        break;
    case 2:
        physical_channels = AOUT_CHANS_STEREO;
        sys->audio_streams = 1;
        break;
    case 8:
        physical_channels = AOUT_CHANS_7_1;
        sys->audio_streams = 1;
        break;
    case 16:
        physical_channels = AOUT_CHANS_STEREO;
        sys->audio_streams = 8;
        break;
    default:
        msg_Err(demux, "Invalid number of channels (%d), disabling audio", sys->channels);
        sys->channels = 0;
        sys->audio_streams = 0;
    }
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

    msg_Dbg(demux, "added new video es %4.4s %dx%d",
             (char*)&sys->video_fmt.i_codec, sys->video_fmt.video.i_width, sys->video_fmt.video.i_height);
    sys->video_es = es_out_Add(demux->out, &sys->video_fmt);

    es_format_t audio_fmt;
    es_format_Init(&audio_fmt, AUDIO_ES, VLC_CODEC_S16N);
    audio_fmt.audio.i_channels = sys->channels / sys->audio_streams;
    audio_fmt.audio.i_physical_channels = physical_channels;
    audio_fmt.audio.i_rate = rate;
    audio_fmt.audio.i_bitspersample = 16;
    audio_fmt.audio.i_blockalign = audio_fmt.audio.i_channels * audio_fmt.audio.i_bitspersample / 8;
    audio_fmt.i_bitrate = audio_fmt.audio.i_channels * audio_fmt.audio.i_rate * audio_fmt.audio.i_bitspersample;

    for(int i=0; i<sys->audio_streams; i++)
    {
        msg_Dbg(demux, "added new audio es [%d] %4.4s %dHz %dbpp %dch", i,
                (char*)&audio_fmt.i_codec, audio_fmt.audio.i_rate,
                audio_fmt.audio.i_bitspersample, audio_fmt.audio.i_channels);
        sys->audio_es[i] = es_out_Add(demux->out, &audio_fmt);
    }

    ret = VLC_SUCCESS;

finish:
    if (decklink_iterator)
        decklink_iterator->Release();

    if (ret != VLC_SUCCESS)
        Close(p_this);

    return ret;
}

static void Close(vlc_object_t *p_this)
{
    demux_t     *demux = (demux_t *)p_this;
    demux_sys_t *sys = (demux_sys_t *)demux->p_sys;

    if (sys->attributes)
        sys->attributes->Release();

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

    free(sys);
}

static int Control(demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = (demux_sys_t *)demux->p_sys;
    bool *pb;

    switch(query)
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            pb = va_arg(args, bool *);
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t *) =
                VLC_TICK_FROM_MS(var_InheritInteger(demux, "live-caching"));
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            vlc_mutex_lock(&sys->pts_lock);
            *va_arg(args, vlc_tick_t *) = sys->last_pts;
            vlc_mutex_unlock(&sys->pts_lock);
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}
