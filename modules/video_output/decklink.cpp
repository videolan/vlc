/*****************************************************************************
 * decklink.cpp: BlackMagic DeckLink SDI output module
 *****************************************************************************
 * Copyright (C) 2012-2013 Rafaël Carré
 * Copyright (C) 2009 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2009 Baptiste Coudurier <baptiste dot coudurier at gmail dot com>
 *
 * Authors: Rafaël Carré <funman@videolan.org>
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

/*
 * TODO: test non stereo audio
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_fixups.h>
#include <cinttypes>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_threads.h>

#include <vlc_vout_display.h>

#include <vlc_block.h>
#include <vlc_aout.h>
#include <vlc_cxx_helpers.hpp>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "../access/vlc_decklink.h"
#include "../stream_out/sdi/V210.hpp"
#include "../stream_out/sdi/Ancillary.hpp"
#include "../stream_out/sdi/DBMHelper.hpp"
#include "../stream_out/sdi/SDIGenerator.hpp"
#include <DeckLinkAPIDispatch.cpp>
#include <DeckLinkAPIVersion.h>
#if BLACKMAGIC_DECKLINK_API_VERSION < 0x0b010000
 #define IID_IDeckLinkProfileAttributes IID_IDeckLinkAttributes
 #define IDeckLinkProfileAttributes IDeckLinkAttributes
#endif

#define FRAME_SIZE 1920
#define CHANNELS_MAX 6

#if 0
static const int pi_channels_maps[CHANNELS_MAX+1] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHANS_STEREO,
    AOUT_CHANS_3_0,
    AOUT_CHANS_4_0,
    AOUT_CHANS_5_0,
    AOUT_CHANS_5_1,
};
#endif

#define NOSIGNAL_INDEX_TEXT N_("Timelength after which we assume there is no signal.")
#define NOSIGNAL_INDEX_LONGTEXT N_(\
    "Timelength after which we assume there is no signal.\n"\
    "After this delay we black out the video."\
    )

#define AFD_INDEX_TEXT N_("Active Format Descriptor value")

#define AR_INDEX_TEXT N_("Aspect Ratio")
#define AR_INDEX_LONGTEXT N_("Aspect Ratio of the source picture.")

#define AFDLINE_INDEX_TEXT N_("Active Format Descriptor line")
#define AFDLINE_INDEX_LONGTEXT N_("VBI line on which to output Active Format Descriptor.")

#define NOSIGNAL_IMAGE_TEXT N_("Picture to display on input signal loss")
#define NOSIGNAL_IMAGE_LONGTEXT NOSIGNAL_IMAGE_TEXT

#define CARD_INDEX_TEXT N_("Output card")
#define CARD_INDEX_LONGTEXT N_(\
    "DeckLink output card, if multiple exist. " \
    "The cards are numbered from 0.")

#define MODE_TEXT N_("Desired output mode")
#define MODE_LONGTEXT N_(\
    "Desired output mode for DeckLink output. " \
    "This value should be a FOURCC code in textual " \
    "form, e.g. \"ntsc\".")

#define AUDIO_CONNECTION_TEXT N_("Audio connection")
#define AUDIO_CONNECTION_LONGTEXT N_(\
    "Audio connection for DeckLink output.")


#define RATE_TEXT N_("Audio samplerate (Hz)")
#define RATE_LONGTEXT N_(\
    "Audio sampling rate (in hertz) for DeckLink output. " \
    "0 disables audio output.")

#define CHANNELS_TEXT N_("Number of audio channels")
#define CHANNELS_LONGTEXT N_(\
    "Number of output channels for DeckLink output. " \
    "Must be 2, 8 or 16. 0 disables audio output.")

#define VIDEO_CONNECTION_TEXT N_("Video connection")
#define VIDEO_CONNECTION_LONGTEXT N_(\
    "Video connection for DeckLink output.")

#define VIDEO_TENBITS_TEXT N_("10 bits")
#define VIDEO_TENBITS_LONGTEXT N_(\
    "Use 10 bits per pixel for video frames.")

#define CFG_PREFIX "decklink-output-"
#define VIDEO_CFG_PREFIX "decklink-vout-"
#define AUDIO_CFG_PREFIX "decklink-aout-"

/* Video Connections */
static const char *const ppsz_videoconns[] = {
    "sdi",
    "hdmi",
    "opticalsdi",
    "component",
    "composite",
    "svideo"
};
static const char *const ppsz_videoconns_text[] = {
    "SDI",
    "HDMI",
    "Optical SDI",
    "Component",
    "Composite",
    "S-video",
};
static const BMDVideoConnection rgbmd_videoconns[] =
{
    bmdVideoConnectionSDI,
    bmdVideoConnectionHDMI,
    bmdVideoConnectionOpticalSDI,
    bmdVideoConnectionComponent,
    bmdVideoConnectionComposite,
    bmdVideoConnectionSVideo,
};
static_assert(ARRAY_SIZE(rgbmd_videoconns) == ARRAY_SIZE(ppsz_videoconns), "videoconn arrays messed up");
static_assert(ARRAY_SIZE(rgbmd_videoconns) == ARRAY_SIZE(ppsz_videoconns_text), "videoconn arrays messed up");

static const int rgi_afd_values[] = {
    0, 2, 3, 4, 8, 9, 10, 11, 13, 14, 15,
};
static const char * const rgsz_afd_text[] = {
    "0:  Undefined",
    "2:  Box 16:9 (top aligned)",
    "3:  Box 14:9 (top aligned)",
    "4:  Box > 16:9 (centre aligned)",
    "8:  Same as coded frame (full frame)",
    "9:   4:3 (centre aligned)",
    "10: 16:9 (centre aligned)",
    "11: 14:9 (centre aligned)",
    "13:  4:3 (with shoot and protect 14:9 centre)",
    "14: 16:9 (with shoot and protect 14:9 centre)",
    "15: 16:9 (with shoot and protect  4:3 centre)",
};
static_assert(ARRAY_SIZE(rgi_afd_values) == ARRAY_SIZE(rgsz_afd_text), "afd arrays messed up");

static const int rgi_ar_values[] = {
    0, 1,
};
static const char * const rgsz_ar_text[] = {
    "0:   4:3",
    "1:  16:9",
};
static_assert(ARRAY_SIZE(rgi_ar_values) == ARRAY_SIZE(rgsz_ar_text), "afd arrays messed up");

namespace {

/* Only one audio output module and one video output module
 * can be used per process.
 * We use a static mutex in audio/video submodules entry points.  */
struct decklink_sys_t
{
    /* With LOCK */
    IDeckLinkOutput *p_output;

    /*
     * Synchronizes aout and vout modules:
     * vout module waits until aout has been initialized.
     * That means video-only output is NOT supported.
     */
    vlc_mutex_t lock;
    vlc_cond_t cond;
    uint8_t users;
    bool    b_videomodule;
    bool    b_recycling;

    //int i_channels;
    int i_rate;

    BMDTimeScale timescale;
    BMDTimeValue frameduration;

    /* XXX: workaround card clock drift */
    vlc_tick_t offset;

    /* !With LOCK */

    /* single video module exclusive */
    struct
    {
        bool tenbits;
        uint8_t afd, ar;
        int nosignal_delay;
        picture_t *pic_nosignal;
    } video;
};

} // namespace

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

static int  OpenVideo           (vout_display_t *, const vout_display_cfg_t *,
                                 video_format_t *, vlc_video_context *);
static void CloseVideo          (vout_display_t *);
static int  OpenAudio           (vlc_object_t *);
static void CloseAudio          (vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin()
    set_shortname(N_("DecklinkOutput"))
    set_description(N_("Output module to write to Blackmagic SDI card"))
    set_section(N_("DeckLink General Options"), NULL)
    add_integer(CFG_PREFIX "card-index", 0,
                CARD_INDEX_TEXT, CARD_INDEX_LONGTEXT, true)

    add_submodule ()
    set_description (N_("DeckLink Video Output module"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_callback_display(OpenVideo, 0)
    set_section(N_("DeckLink Video Options"), NULL)
    add_string(VIDEO_CFG_PREFIX "video-connection", "sdi",
                VIDEO_CONNECTION_TEXT, VIDEO_CONNECTION_LONGTEXT, true)
                change_string_list(ppsz_videoconns, ppsz_videoconns_text)
    add_string(VIDEO_CFG_PREFIX "mode", "",
                MODE_TEXT, MODE_LONGTEXT, true)
    add_bool(VIDEO_CFG_PREFIX "tenbits", true,
                VIDEO_TENBITS_TEXT, VIDEO_TENBITS_LONGTEXT, true)
    add_integer(VIDEO_CFG_PREFIX "nosignal-delay", 5,
                NOSIGNAL_INDEX_TEXT, NOSIGNAL_INDEX_LONGTEXT, true)
    add_integer(VIDEO_CFG_PREFIX "afd-line", 16,
                AFDLINE_INDEX_TEXT, AFDLINE_INDEX_LONGTEXT, true)
    add_integer_with_range(VIDEO_CFG_PREFIX "afd", 8, 0, 16,
                AFD_INDEX_TEXT, AFD_INDEX_TEXT, true)
                change_integer_list(rgi_afd_values, rgsz_afd_text)
    add_integer_with_range(VIDEO_CFG_PREFIX "ar", 1, 0, 1,
                AR_INDEX_TEXT, AR_INDEX_LONGTEXT, true)
                change_integer_list(rgi_ar_values, rgsz_ar_text)
    add_loadfile(VIDEO_CFG_PREFIX "nosignal-image", NULL,
                 NOSIGNAL_IMAGE_TEXT, NOSIGNAL_IMAGE_LONGTEXT)


    add_submodule ()
    set_description (N_("DeckLink Audio Output module"))
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    set_capability("audio output", 0)
    set_callbacks (OpenAudio, CloseAudio)
    set_section(N_("DeckLink Audio Options"), NULL)
    add_obsolete_string("audio-connection")
    add_integer(AUDIO_CFG_PREFIX "audio-rate", 48000,
                RATE_TEXT, RATE_LONGTEXT, true)
    add_integer(AUDIO_CFG_PREFIX "audio-channels", 2,
                CHANNELS_TEXT, CHANNELS_LONGTEXT, true)
vlc_module_end ()

/* Protects decklink_sys_t creation/deletion */
static vlc::threads::mutex sys_lock;

static decklink_sys_t *HoldDLSys(vlc_object_t *obj, int i_cat)
{
    vlc_object_t *libvlc = VLC_OBJECT(vlc_object_instance(obj));
    decklink_sys_t *sys;

    sys_lock.lock();

    if (var_Type(libvlc, "decklink-sys") == VLC_VAR_ADDRESS)
    {
        sys = (decklink_sys_t*)var_GetAddress(libvlc, "decklink-sys");
        sys->users++;

        if(i_cat == VIDEO_ES)
        {
            while(sys->b_videomodule)
            {
                sys_lock.unlock();
                msg_Info(obj, "Waiting for previous vout module to exit");
                vlc_tick_sleep(VLC_TICK_FROM_MS(100));
                sys_lock.lock();
            }
        }
    }
    else
    {
        sys = (decklink_sys_t*)malloc(sizeof(*sys));
        if (sys) {
            sys->p_output = NULL;
            sys->offset = 0;
            sys->users = 1;
            sys->b_videomodule = (i_cat == VIDEO_ES);
            sys->b_recycling = false;
            sys->i_rate = var_InheritInteger(obj, AUDIO_CFG_PREFIX "audio-rate");
            if(sys->i_rate > 0)
                sys->i_rate = -1;
            vlc_mutex_init(&sys->lock);
            vlc_cond_init(&sys->cond);
            var_Create(libvlc, "decklink-sys", VLC_VAR_ADDRESS);
            var_SetAddress(libvlc, "decklink-sys", (void*)sys);
        }
    }

    sys_lock.unlock();
    return sys;
}

static void ReleaseDLSys(vlc_object_t *obj, int i_cat)
{
    vlc_object_t *libvlc = VLC_OBJECT(vlc_object_instance(obj));

    sys_lock.lock();

    struct decklink_sys_t *sys = (struct decklink_sys_t*)var_GetAddress(libvlc, "decklink-sys");

    if (--sys->users == 0) {
        msg_Dbg(obj, "Destroying decklink data");

        if (sys->p_output) {
            sys->p_output->StopScheduledPlayback(0, NULL, 0);
            sys->p_output->DisableVideoOutput();
            sys->p_output->DisableAudioOutput();
            sys->p_output->Release();
        }

        /* Clean video specific */
        if (sys->video.pic_nosignal)
            picture_Release(sys->video.pic_nosignal);

        free(sys);
        var_Destroy(libvlc, "decklink-sys");
    }
    else if (i_cat == VIDEO_ES)
    {
        sys->b_videomodule = false;
        sys->b_recycling = true;
    }

    sys_lock.unlock();
}

static BMDVideoConnection getVConn(vout_display_t *vd, BMDVideoConnection mask)
{
    BMDVideoConnection conn = 0;
    char *psz = var_InheritString(vd, VIDEO_CFG_PREFIX "video-connection");
    if (psz)
    {
        for(size_t i=0; i<ARRAY_SIZE(rgbmd_videoconns); i++)
        {
            if (!strcmp(psz, ppsz_videoconns[i]) && (mask & rgbmd_videoconns[i]))
            {
                conn = rgbmd_videoconns[i];
                break;
            }
        }
        free(psz);
    }
    else /* Pick one as default connection */
    {
        conn = vlc_ctz(mask);
        conn = conn ? ( 1 << conn ) : bmdVideoConnectionSDI;
    }
    return conn;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int OpenDecklink(vout_display_t *vd, decklink_sys_t *sys, video_format_t *fmt)
{
#define CHECK(message) do { \
    if (result != S_OK) \
    { \
        const char *psz_err = Decklink::Helper::ErrorToString(result); \
        if(psz_err)\
            msg_Err(vd, message ": %s", psz_err); \
        else \
            msg_Err(vd, message ": 0x%X", result); \
        goto error; \
    } \
} while(0)

    HRESULT result;
    IDeckLinkIterator *decklink_iterator = NULL;
    IDeckLinkDisplayMode *p_display_mode = NULL;
    IDeckLinkConfiguration *p_config = NULL;
    IDeckLinkProfileAttributes *p_attributes = NULL;
    IDeckLink *p_card = NULL;
    BMDDisplayMode wanted_mode_id = bmdModeUnknown;

    vlc_mutex_lock(&sys->lock);

    /* wait until aout is ready */
    msg_Info(vd, "Waiting for DeckLink audio input module to start");
    while (sys->i_rate == -1)
        vlc_cond_wait(&sys->cond, &sys->lock);

    int i_card_index = var_InheritInteger(vd, CFG_PREFIX "card-index");
    char *mode = var_InheritString(vd, VIDEO_CFG_PREFIX "mode");

    if(mode)
    {
        size_t len = strlen(mode);
        if (len > 4)
        {
            free(mode);
            msg_Err(vd, "Invalid mode %s", mode);
            goto error;
        }
        memset(&wanted_mode_id, ' ', 4);
        strncpy((char*)&wanted_mode_id, mode, 4);
        wanted_mode_id = ntohl(wanted_mode_id);
        free(mode);
    }

    if (i_card_index < 0)
    {
        msg_Err(vd, "Invalid card index %d", i_card_index);
        goto error;
    }

    decklink_iterator = CreateDeckLinkIteratorInstance();
    if (!decklink_iterator)
    {
        msg_Err(vd, "DeckLink drivers not found.");
        goto error;
    }

    for(int i = 0; i <= i_card_index; ++i)
    {
        if (p_card)
            p_card->Release();
        result = decklink_iterator->Next(&p_card);
        CHECK("Card not found");
    }

    decklink_str_t tmp_name;
    char *psz_model_name;
    result = p_card->GetModelName(&tmp_name);
    CHECK("Unknown model name");
    psz_model_name = DECKLINK_STRDUP(tmp_name);
    DECKLINK_FREE(tmp_name);

    msg_Dbg(vd, "Opened DeckLink PCI card %s", psz_model_name);
    free(psz_model_name);

    /* Read attributes */

    result = p_card->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&p_attributes);
    CHECK("Could not get IDeckLinkAttributes");

    int64_t vconn;
    result = p_attributes->GetInt(BMDDeckLinkVideoOutputConnections, &vconn); /* reads mask */
    CHECK("Could not get BMDDeckLinkVideoOutputConnections");

    result = p_card->QueryInterface(IID_IDeckLinkOutput,
        (void**)&sys->p_output);
    CHECK("No outputs");

    result = p_card->QueryInterface(IID_IDeckLinkConfiguration,
        (void**)&p_config);
    CHECK("Could not get config interface");

    /* Now configure card */

    vconn = getVConn(vd, (BMDVideoConnection) vconn);
    if (vconn == 0)
    {
        msg_Err(vd, "Invalid video connection specified");
        goto error;
    }

    result = p_config->SetInt(bmdDeckLinkConfigVideoOutputConnection, (BMDVideoConnection) vconn);
    CHECK("Could not set video output connection");

    p_display_mode = Decklink::Helper::MatchDisplayMode(VLC_OBJECT(vd), sys->p_output,
                                          &vd->source, wanted_mode_id);
    if(p_display_mode == NULL)
    {
        msg_Err(vd, "Could not negociate a compatible display mode");
        goto error;
    }
    else
    {
        BMDDisplayMode mode_id = p_display_mode->GetDisplayMode();
        BMDDisplayMode modenl = htonl(mode_id);
        msg_Dbg(vd, "Selected mode '%4.4s'", (char *) &modenl);

        BMDPixelFormat pixelFormat = sys->video.tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV;
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
        result = sys->p_output->DoesSupportVideoMode(mode_id,
                                                pixelFormat,
                                                flags,
                                                &support,
                                                NULL);
        supported = (support != bmdDisplayModeNotSupported);
#else
        result = sys->p_output->DoesSupportVideoMode(vconn,
                                                mode_id,
                                                pixelFormat,
                                                bmdSupportedVideoModeDefault,
                                                NULL,
                                                &supported);
#endif
        CHECK("Does not support video mode");
        if (!supported)
        {
            msg_Err(vd, "Video mode not supported");
            goto error;
        }

        if (p_display_mode->GetWidth() <= 0 || p_display_mode->GetWidth() & 1)
        {
             msg_Err(vd, "Unknown video mode specified.");
             goto error;
        }

        result = p_display_mode->GetFrameRate(&sys->frameduration,
                                              &sys->timescale);
        CHECK("Could not read frame rate");

        result = sys->p_output->EnableVideoOutput(mode_id, flags);
        CHECK("Could not enable video output");

        video_format_Copy(fmt, &vd->source);
        fmt->i_width = fmt->i_visible_width = p_display_mode->GetWidth();
        fmt->i_height = fmt->i_visible_height = p_display_mode->GetHeight();
        fmt->i_x_offset = 0;
        fmt->i_y_offset = 0;
        fmt->i_sar_num = 0;
        fmt->i_sar_den = 0;
        fmt->i_chroma = !sys->video.tenbits ? VLC_CODEC_UYVY : VLC_CODEC_I422_10L; /* we will convert to v210 */
        fmt->i_frame_rate = (unsigned) sys->frameduration;
        fmt->i_frame_rate_base = (unsigned) sys->timescale;
    }

    if (/*decklink_sys->i_channels > 0 &&*/ sys->i_rate > 0)
    {
        result = sys->p_output->EnableAudioOutput(
            sys->i_rate,
            bmdAudioSampleType16bitInteger,
            /*decklink_sys->i_channels*/ 2,
            bmdAudioOutputStreamTimestamped);
        CHECK("Could not start audio output");
    }

    /* start */
    result = sys->p_output->StartScheduledPlayback(
        samples_from_vlc_tick(vlc_tick_now(), sys->timescale), sys->timescale, 1.0);
    CHECK("Could not start playback");

    p_config->Release();
    p_display_mode->Release();
    p_card->Release();
    p_attributes->Release();
    decklink_iterator->Release();

    vlc_mutex_unlock(&sys->lock);

    return VLC_SUCCESS;

error:
    if (sys->p_output) {
        sys->p_output->Release();
        sys->p_output = NULL;
    }
    if (p_card)
        p_card->Release();
    if (p_config)
        p_config->Release();
    if (p_attributes)
        p_attributes->Release();
    if (decklink_iterator)
        decklink_iterator->Release();
    if (p_display_mode)
        p_display_mode->Release();
    video_format_Clean(fmt);

    vlc_mutex_unlock(&sys->lock);

    return VLC_EGENERIC;
#undef CHECK
}

/*****************************************************************************
 * Video
 *****************************************************************************/
static void PrepareVideo(vout_display_t *vd, picture_t *picture, subpicture_t *,
                         vlc_tick_t date)
{
    decklink_sys_t *sys = (decklink_sys_t *) vd->sys;
    vlc_tick_t now = vlc_tick_now();

    if (!picture)
        return;

    if (now - date > vlc_tick_from_sec( sys->video.nosignal_delay )) {
        msg_Dbg(vd, "no signal");
        if (sys->video.pic_nosignal) {
            picture = sys->video.pic_nosignal;
        } else {
            if (sys->video.tenbits) { // I422_10L
                plane_t *y = &picture->p[0];
                memset(y->p_pixels, 0x0, y->i_lines * y->i_pitch);
                for (int i = 1; i < picture->i_planes; i++) {
                    plane_t *p = &picture->p[i];
                    size_t len = p->i_lines * p->i_pitch / 2;
                    int16_t *data = (int16_t*)p->p_pixels;
                    for (size_t j = 0; j < len; j++) // XXX: SIMD
                        data[j] = 0x200;
                }
            } else { // UYVY
                size_t len = picture->p[0].i_lines * picture->p[0].i_pitch;
                for (size_t i = 0; i < len; i+= 2) { // XXX: SIMD
                    picture->p[0].p_pixels[i+0] = 0x80;
                    picture->p[0].p_pixels[i+1] = 0;
                }
            }
        }
        date = now;
    }

    HRESULT result;
    int w, h, stride, length;
    w = vd->fmt.i_width;
    h = vd->fmt.i_height;

    IDeckLinkMutableVideoFrame *pDLVideoFrame;
    result = sys->p_output->CreateVideoFrame(w, h, w*3,
        sys->video.tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV,
        bmdFrameFlagDefault, &pDLVideoFrame);

    if (result != S_OK) {
        msg_Err(vd, "Failed to create video frame: 0x%X", result);
        pDLVideoFrame = NULL;
        goto end;
    }

    void *frame_bytes;
    pDLVideoFrame->GetBytes((void**)&frame_bytes);
    stride = pDLVideoFrame->GetRowBytes();

    if (sys->video.tenbits) {
        IDeckLinkVideoFrameAncillary *vanc;
        int line;
        void *buf;

        result = sys->p_output->CreateAncillaryData(
                sys->video.tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV, &vanc);
        if (result != S_OK) {
            msg_Err(vd, "Failed to create vanc: %d", result);
            goto end;
        }

        line = var_InheritInteger(vd, VIDEO_CFG_PREFIX "afd-line");
        result = vanc->GetBufferForVerticalBlankingLine(line, &buf);
        if (result != S_OK) {
            msg_Err(vd, "Failed to get VBI line %d: %d", line, result);
            goto end;
        }

        sdi::AFD afd(sys->video.afd, sys->video.ar);
        afd.FillBuffer(reinterpret_cast<uint8_t*>(buf), stride);

        sdi::V210::Convert(picture, stride, frame_bytes);

        result = pDLVideoFrame->SetAncillaryData(vanc);
        vanc->Release();
        if (result != S_OK) {
            msg_Err(vd, "Failed to set vanc: %d", result);
            goto end;
        }
    }
    else for(int y = 0; y < h; ++y) {
        uint8_t *dst = (uint8_t *)frame_bytes + stride * y;
        const uint8_t *src = (const uint8_t *)picture->p[0].p_pixels +
            picture->p[0].i_pitch * y;
        memcpy(dst, src, w * 2 /* bpp */);
    }


    // compute frame duration in CLOCK_FREQ units
    length = (sys->frameduration * CLOCK_FREQ) / sys->timescale;

    date -= sys->offset;
    result = sys->p_output->ScheduleVideoFrame(pDLVideoFrame,
        date, length, CLOCK_FREQ);

    if (result != S_OK) {
        msg_Err(vd, "Dropped Video frame %" PRId64 ": 0x%x", date, result);
        goto end;
    }

    now = vlc_tick_now() - sys->offset;

    BMDTimeValue decklink_now;
    double speed;
    sys->p_output->GetScheduledStreamTime (CLOCK_FREQ, &decklink_now, &speed);

    if ((now - decklink_now) > 400000) {
        /* XXX: workaround card clock drift */
        sys->offset += 50000;
        msg_Err(vd, "Delaying: offset now %" PRId64, sys->offset);
    }

end:
    if (pDLVideoFrame)
        pDLVideoFrame->Release();
}

static int ControlVideo(vout_display_t *vd, int query, va_list args)
{
    (void) vd; (void) args;

    switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static int OpenVideo(vout_display_t *vd, const vout_display_cfg_t *cfg,
                     video_format_t *fmtp, vlc_video_context *context)
{
    VLC_UNUSED(cfg); VLC_UNUSED(context);
    decklink_sys_t *sys = HoldDLSys(VLC_OBJECT(vd), VIDEO_ES);
    if(!sys)
        return VLC_ENOMEM;

    bool b_init;
    vlc_mutex_lock(&sys->lock);
    b_init = !sys->b_recycling;
    vlc_mutex_unlock(&sys->lock);

    if( b_init )
    {
        sys->video.tenbits = var_InheritBool(vd, VIDEO_CFG_PREFIX "tenbits");
        sys->video.nosignal_delay = var_InheritInteger(vd, VIDEO_CFG_PREFIX "nosignal-delay");
        sys->video.afd = var_InheritInteger(vd, VIDEO_CFG_PREFIX "afd");
        sys->video.ar = var_InheritInteger(vd, VIDEO_CFG_PREFIX "ar");
        sys->video.pic_nosignal = NULL;

        if (OpenDecklink(vd, sys, fmtp) != VLC_SUCCESS)
        {
            CloseVideo(vd);
            return VLC_EGENERIC;
        }

        char *pic_file = var_InheritString(vd, VIDEO_CFG_PREFIX "nosignal-image");
        if (pic_file)
        {
            sys->video.pic_nosignal = sdi::Generator::Picture(VLC_OBJECT(vd), pic_file, fmtp);
            if (!sys->video.pic_nosignal)
                msg_Err(vd, "Could not create no signal picture");
            free(pic_file);
        }
    }

    vd->prepare = PrepareVideo;
    vd->display = NULL;
    vd->control = ControlVideo;
    vd->close = CloseVideo;

    vd->sys = (vout_display_sys_t*) sys;

    return VLC_SUCCESS;
}

static void CloseVideo(vout_display_t *vd)
{
    ReleaseDLSys(VLC_OBJECT(vd), VIDEO_ES);
}

/*****************************************************************************
 * Audio
 *****************************************************************************/

static void Flush(audio_output_t *aout)
{
    decklink_sys_t *sys = (decklink_sys_t *) aout->sys;
    vlc_mutex_lock(&sys->lock);
    IDeckLinkOutput *p_output = sys->p_output;
    vlc_mutex_unlock(&sys->lock);
    if (!p_output)
        return;

    if (sys->p_output->FlushBufferedAudioSamples() == E_FAIL)
        msg_Err(aout, "Flush failed");
}

static void Drain(audio_output_t *aout)
{
    decklink_sys_t *sys = (decklink_sys_t *) aout->sys;
    vlc_mutex_lock(&sys->lock);
    IDeckLinkOutput *p_output = sys->p_output;
    vlc_mutex_unlock(&sys->lock);
    if (!p_output)
        return;

    uint32_t samples;
    sys->p_output->GetBufferedAudioSampleFrameCount(&samples);
    vlc_tick_sleep(vlc_tick_from_samples(samples, sys->i_rate));
}


static int TimeGet(audio_output_t *, vlc_tick_t* restrict)
{
    /* synchronization is handled by the card */
    return -1;
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    decklink_sys_t *sys = (decklink_sys_t *) aout->sys;

    if (sys->i_rate == 0)
        return VLC_EGENERIC;

    fmt->i_format = VLC_CODEC_S16N;
    fmt->i_channels = 2; //decklink_sys->i_channels;
    fmt->i_physical_channels = AOUT_CHANS_STEREO; //pi_channels_maps[fmt->i_channels];
    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
    fmt->i_rate = sys->i_rate;
    fmt->i_bitspersample = 16;
    fmt->i_blockalign = fmt->i_channels * fmt->i_bitspersample /8 ;
    fmt->i_frame_length  = FRAME_SIZE;

    return VLC_SUCCESS;
}

static void PlayAudio(audio_output_t *aout, block_t *audio, vlc_tick_t systempts)
{
    decklink_sys_t *sys = (decklink_sys_t *) aout->sys;
    vlc_mutex_lock(&sys->lock);
    IDeckLinkOutput *p_output = sys->p_output;
    audio->i_pts -= sys->offset;
    vlc_mutex_unlock(&sys->lock);
    if (!p_output) {
        block_Release(audio);
        return;
    }

    uint32_t sampleFrameCount = audio->i_buffer / (2 * 2 /*decklink_sys->i_channels*/);
    uint32_t written;
    HRESULT result = p_output->ScheduleAudioSamples(
            audio->p_buffer, sampleFrameCount, systempts, CLOCK_FREQ, &written);

    if (result != S_OK)
        msg_Err(aout, "Failed to schedule audio sample: 0x%X", result);
    else if (sampleFrameCount != written)
        msg_Err(aout, "Written only %d samples out of %d", written, sampleFrameCount);

    block_Release(audio);
}

static int OpenAudio(vlc_object_t *p_this)
{
    audio_output_t *aout = (audio_output_t *)p_this;
    decklink_sys_t *sys = HoldDLSys(p_this, AUDIO_ES);
    if(!sys)
        return VLC_ENOMEM;

    aout->sys = sys;

    vlc_mutex_lock(&sys->lock);
    //decklink_sys->i_channels = var_InheritInteger(vd, AUDIO_CFG_PREFIX "audio-channels");
    sys->i_rate = var_InheritInteger(aout, AUDIO_CFG_PREFIX "audio-rate");
    vlc_cond_signal(&sys->cond);
    vlc_mutex_unlock(&sys->lock);

    aout->play      = PlayAudio;
    aout->start     = Start;
    aout->flush     = Flush;
    aout->drain     = Drain;
    aout->time_get  = TimeGet;

    aout->pause     = aout_PauseDefault;
    aout->stop      = NULL;
    aout->mute_set  = NULL;
    aout->volume_set= NULL;

    return VLC_SUCCESS;
}

static void CloseAudio(vlc_object_t *p_this)
{
    decklink_sys_t *sys = (decklink_sys_t *) ((audio_output_t *)p_this)->sys;
    vlc_mutex_lock(&sys->lock);
    vlc_mutex_unlock(&sys->lock);
    ReleaseDLSys(p_this, AUDIO_ES);
}
