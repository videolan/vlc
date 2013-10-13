/*****************************************************************************
 * decklink.cpp: BlackMagic DeckLink SDI input module
 *****************************************************************************
 * Copyright (C) 2010 Steinar H. Gunderson
 * Copyright (C) 2009 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2009 Baptiste Coudurier <baptiste dot coudurier at gmail dot com>
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
    add_bool("decklink-tenbits", false, N_("10 bits"), N_("10 bits"), true)

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
    es_out_id_t *cc_es;

    vlc_mutex_t pts_lock;
    int last_pts;  /* protected by <pts_lock> */

    uint32_t dominance_flags;
    int channels;

    bool tenbits;
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

static inline uint32_t av_le2ne32(uint32_t val)
{
    union {
        uint32_t v;
        uint8_t b[4];
    } u;
    u.v = val;
    return (u.b[0] << 0) | (u.b[1] << 8) | (u.b[2] << 16) | (u.b[3] << 24);
}

static void v210_convert(uint16_t *dst, const uint32_t *bytes, const int width, const int height)
{
    const int stride = ((width + 47) / 48) * 48 * 8 / 3 / 4;
    uint16_t *y = &dst[0];
    uint16_t *u = &dst[width * height * 2 / 2];
    uint16_t *v = &dst[width * height * 3 / 2];

#define READ_PIXELS(a, b, c)         \
    do {                             \
        val  = av_le2ne32(*src++);   \
        *a++ =  val & 0x3FF;         \
        *b++ = (val >> 10) & 0x3FF;  \
        *c++ = (val >> 20) & 0x3FF;  \
    } while (0)

    for (int h = 0; h < height; h++) {
        const uint32_t *src = bytes;
        uint32_t val = 0;
        int w;
        for (w = 0; w < width - 5; w += 6) {
            READ_PIXELS(u, y, v);
            READ_PIXELS(y, u, y);
            READ_PIXELS(v, y, u);
            READ_PIXELS(y, v, y);
        }
        if (w < width - 1) {
            READ_PIXELS(u, y, v);

            val  = av_le2ne32(*src++);
            *y++ =  val & 0x3FF;
        }
        if (w < width - 3) {
            *u++ = (val >> 10) & 0x3FF;
            *y++ = (val >> 20) & 0x3FF;

            val  = av_le2ne32(*src++);
            *v++ =  val & 0x3FF;
            *y++ = (val >> 10) & 0x3FF;
        }

        bytes += stride;
    }
}

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

        int bpp = sys->tenbits ? 4 : 2;
        block_t *video_frame = block_Alloc(width * height * bpp);
        if (!video_frame)
            return S_OK;

        const uint32_t *frame_bytes;
        videoFrame->GetBytes((void**)&frame_bytes);

        BMDTimeValue stream_time, frame_duration;
        videoFrame->GetStreamTime(&stream_time, &frame_duration, CLOCK_FREQ);
        video_frame->i_flags = BLOCK_FLAG_TYPE_I | sys->dominance_flags;
        video_frame->i_pts = video_frame->i_dts = VLC_TS_0 + stream_time;

        if (sys->tenbits) {
            v210_convert((uint16_t*)video_frame->p_buffer, frame_bytes, width, height);
            IDeckLinkVideoFrameAncillary *vanc;
            if (videoFrame->GetAncillaryData(&vanc) == S_OK) {
                for (int i = 1; i < 21; i++) {
                    uint32_t *buf;
                    if (vanc->GetBufferForVerticalBlankingLine(i, (void**)&buf) != S_OK)
                        break;
                    uint16_t dec[width * 2];
                    v210_convert(&dec[0], buf, width, 1);
                    static const uint16_t vanc_header[3] = { 0, 0x3ff, 0x3ff };
                    if (!memcmp(vanc_header, dec, sizeof(vanc_header))) {
                        int len = (dec[5] & 0xff) + 6 + 1;
                        uint16_t vanc_sum = 0;
                        bool parity_ok = true;
                        for (int i = 3; i < len - 1; i++) {
                            uint16_t v = dec[i];
                            int np = v >> 8;
                            int p = parity(v & 0xff);
                            if ((!!p ^ !!(v & 0x100)) || (np != 1 && np != 2)) {
                                parity_ok = false;
                                break;
                            }
                            vanc_sum += v;
                            vanc_sum &= 0x1ff;
                            dec[i] &= 0xff;
                        }

                        if (!parity_ok)
                            continue;

                        vanc_sum |= ((~vanc_sum & 0x100) << 1);
                        if (dec[len - 1] != vanc_sum)
                            continue;

                        if (dec[3] != 0x61 /* DID */ ||
                            dec[4] != 0x01 /* SDID = CEA-708 */)
                            continue;

                        /* CDP follows */
                        uint16_t *cdp = &dec[6];
                        if (cdp[0] != 0x96 || cdp[1] != 0x69)
                            continue;

                        len -= 7; // remove VANC header and checksum

                        if (cdp[2] != len)
                            continue;

                        uint8_t cdp_sum = 0;
                        for (int i = 0; i < len - 1; i++)
                            cdp_sum += cdp[i];
                        cdp_sum = cdp_sum ? 256 - cdp_sum : 0;
                        if (cdp[len - 1] != cdp_sum)
                            continue;

                        uint8_t rate = cdp[3];
                        if (!(rate & 0x0f))
                            continue;
                        rate >>= 4;
                        if (rate > 8)
                            continue;

                        if (!(cdp[4] & 0x43)) /* ccdata_present | caption_service_active | reserved */
                            continue;

                        uint16_t hdr = (cdp[5] << 8) | cdp[6];
                        if (cdp[7] != 0x72) /* ccdata_id */
                            continue;

                        int cc_count = cdp[8];
                        if (!(cc_count & 0xe0))
                            continue;
                        cc_count &= 0x1f;

                        /* FIXME: parse additional data (CC language?) */
                        if ((len - 13) < cc_count * 3)
                            continue;

                        if (cdp[len - 4] != 0x74) /* footer id */
                            continue;

                        uint16_t ftr = (cdp[len - 3] << 8) | cdp[len - 2];
                        if (ftr != hdr)
                            continue;

                        block_t *cc = block_Alloc(cc_count * 3);

                        for (int i = 0; i < cc_count; i++) {
                            cc->p_buffer[3*i+0] = cdp[9 + 3*i+0] & 3;
                            cc->p_buffer[3*i+1] = cdp[9 + 3*i+1];
                            cc->p_buffer[3*i+2] = cdp[9 + 3*i+2];
                        }

                        cc->i_pts = cc->i_dts = VLC_TS_0 + stream_time;

                        if (!sys->cc_es) {
                            es_format_t fmt;

                            es_format_Init( &fmt, SPU_ES, VLC_FOURCC('c', 'c', '1' , ' ') );
                            fmt.psz_description = strdup("Closed captions 1");
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
                }
                vanc->Release();
            }
        } else {
            for (int y = 0; y < height; ++y) {
                const uint8_t *src = (const uint8_t *)frame_bytes + stride * y;
                uint8_t *dst = video_frame->p_buffer + width * 2 * y;
                memcpy(dst, src, width * 2);
            }
        }

        vlc_mutex_lock(&sys->pts_lock);
        if (video_frame->i_pts > sys->last_pts)
            sys->last_pts = video_frame->i_pts;
        vlc_mutex_unlock(&sys->pts_lock);

        es_out_Control(demux_->out, ES_OUT_SET_PCR, video_frame->i_pts);
        es_out_Send(demux_->out, sys->video_es, video_frame);
    }

    if (audioFrame) {
        const int bytes = audioFrame->GetSampleFrameCount() * sizeof(int16_t) * sys->channels;

        block_t *audio_frame = block_Alloc(bytes);
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

static int Open(vlc_object_t *p_this)
{
    demux_t     *demux = (demux_t*)p_this;
    demux_sys_t *sys;
    int         ret = VLC_EGENERIC;
    int         card_index;
    int         width = 0, height, fps_num, fps_den;
    int         physical_channels = 0;
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

    const char *model_name;
    if (sys->card->GetModelName(&model_name) != S_OK)
        model_name = "unknown";

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

    char *mode;
    mode = var_CreateGetNonEmptyString(demux, "decklink-mode");
    if (!mode || strlen(mode) < 3 || strlen(mode) > 4) {
        msg_Err(demux, "Invalid mode: `%s\'", mode ? mode : "");
        goto finish;
    }

    /* Get the list of display modes. */
    IDeckLinkDisplayModeIterator *mode_it;
    if (sys->input->GetDisplayModeIterator(&mode_it) != S_OK) {
        msg_Err(demux, "Failed to enumerate display modes");
        free(mode);
        goto finish;
    }

    union {
        BMDDisplayMode id;
        char str[4];
    } u;
    memcpy(u.str, mode, 4);
    if (u.str[3] == '\0')
        u.str[3] = ' '; /* 'pal'\0 -> 'pal ' */
    free(mode);

    for (IDeckLinkDisplayMode *m;; m->Release()) {
        if ((mode_it->Next(&m) != S_OK) || !m)
            break;

        const char *mode_name;
        BMDTimeValue frame_duration, time_scale;
        uint32_t flags = 0;
        const char *field = GetFieldDominance(m->GetFieldDominance(), &flags);
        BMDDisplayMode id = ntohl(m->GetDisplayMode());

        if (m->GetName(&mode_name) != S_OK)
            mode_name = "unknown";
        if (m->GetFrameRate(&frame_duration, &time_scale) != S_OK) {
            time_scale = 0;
            frame_duration = 1;
        }

        msg_Dbg(demux, "Found mode '%4.4s': %s (%dx%d, %.3f fps%s)",
                 (char*)&id, mode_name,
                 (int)m->GetWidth(), (int)m->GetHeight(),
                 double(time_scale) / frame_duration, field);

        if (u.id == id) {
            width = m->GetWidth();
            height = m->GetHeight();
            fps_num = time_scale;
            fps_den = frame_duration;
            sys->dominance_flags = flags;
        }
    }

    mode_it->Release();

    if (width == 0) {
        msg_Err(demux, "Unknown video mode `%4.4s\' specified.", (char*)&u.id);
        goto finish;
    }

    BMDPixelFormat fmt; fmt = sys->tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV;
    if (sys->input->EnableVideoInput(htonl(u.id), fmt, 0) != S_OK) {
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
        break;
    case 8:
        physical_channels = AOUT_CHANS_7_1;
        break;
    //case 16:
    default:
        msg_Err(demux, "Invalid number of channels (%d), disabling audio", sys->channels);
        sys->channels = 0;
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

    /* Declare elementary streams */
    es_format_t video_fmt;
    vlc_fourcc_t chroma; chroma = sys->tenbits ? VLC_CODEC_I422_10L : VLC_CODEC_UYVY;
    es_format_Init(&video_fmt, VIDEO_ES, chroma);
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
    audio_fmt.audio.i_physical_channels = physical_channels;
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
