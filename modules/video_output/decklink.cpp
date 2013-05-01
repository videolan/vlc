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

#define __STDC_FORMAT_MACROS
#define __STDC_CONSTANT_MACROS

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_threads.h>

#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

#include <vlc_block.h>
#include <vlc_atomic.h>
#include <vlc_aout.h>
#include <arpa/inet.h>

#include <DeckLinkAPI.h>
#include <DeckLinkAPIDispatch.cpp>

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


struct vout_display_sys_t
{
    picture_pool_t *pool;
    bool tenbits;
};

/* Only one audio output module and one video output module
 * can be used per process.
 * We use a static mutex in audio/video submodules entry points.  */
struct decklink_sys_t
{
    IDeckLinkOutput *p_output;

    /*
     * Synchronizes aout and vout modules:
     * vout module waits until aout has been initialized.
     * That means video-only output is NOT supported.
     */
    vlc_mutex_t lock;
    vlc_cond_t cond;
    uint8_t users;
    BMDAudioConnection aconn;

    //int i_channels;
    int i_rate;

    int i_width;
    int i_height;

    BMDTimeScale timescale;
    BMDTimeValue frameduration;

    /* XXX: workaround card clock drift */
    mtime_t offset;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

static int  OpenVideo           (vlc_object_t *);
static void CloseVideo          (vlc_object_t *);
static int  OpenAudio           (vlc_object_t *);
static void CloseAudio          (vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin()
    set_shortname(N_("DecklinkOutput"))
    set_description(N_("output module to write to Blackmagic SDI card"))
    set_section(N_("Decklink General Options"), NULL)
    add_integer(CFG_PREFIX "card-index", 0,
                CARD_INDEX_TEXT, CARD_INDEX_LONGTEXT, true)

    add_submodule ()
    set_description (N_("Decklink Video Output module"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 0)
    set_callbacks (OpenVideo, CloseVideo)
    set_section(N_("Decklink Video Options"), NULL)
    add_string(VIDEO_CFG_PREFIX "video-connection", "sdi",
                VIDEO_CONNECTION_TEXT, VIDEO_CONNECTION_LONGTEXT, true)
                change_string_list(ppsz_videoconns, ppsz_videoconns_text)
    add_string(VIDEO_CFG_PREFIX "mode", "pal ",
                MODE_TEXT, MODE_LONGTEXT, true)
    add_bool(VIDEO_CFG_PREFIX "tenbits", false,
                VIDEO_TENBITS_TEXT, VIDEO_TENBITS_LONGTEXT, true)


    add_submodule ()
    set_description (N_("Decklink Audio Output module"))
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    set_capability("audio output", 0)
    set_callbacks (OpenAudio, CloseAudio)
    set_section(N_("Decklink Audio Options"), NULL)
    add_string(AUDIO_CFG_PREFIX "audio-connection", "embedded",
                AUDIO_CONNECTION_TEXT, AUDIO_CONNECTION_LONGTEXT, true)
                change_string_list(ppsz_audioconns, ppsz_audioconns_text)
    add_integer(AUDIO_CFG_PREFIX "audio-rate", 48000,
                RATE_TEXT, RATE_LONGTEXT, true)
    add_integer(AUDIO_CFG_PREFIX "audio-channels", 2,
                CHANNELS_TEXT, CHANNELS_LONGTEXT, true)
vlc_module_end ()

/* Protects decklink_sys_t creation/deletion */
static vlc_mutex_t sys_lock = VLC_STATIC_MUTEX;

static struct decklink_sys_t *GetDLSys(vlc_object_t *obj)
{
    vlc_object_t *libvlc = VLC_OBJECT(obj->p_libvlc);
    struct decklink_sys_t *sys;

    vlc_mutex_lock(&sys_lock);

    if (var_Type(libvlc, "decklink-sys") == VLC_VAR_ADDRESS)
        sys = (struct decklink_sys_t*)var_GetAddress(libvlc, "decklink-sys");
    else {
        sys = (struct decklink_sys_t*)malloc(sizeof(*sys));
        if (sys) {
            sys->p_output = NULL;
            sys->offset = 0;
            sys->users = 0;
            sys->aconn = 0;
            vlc_mutex_init(&sys->lock);
            vlc_cond_init(&sys->cond);
            var_Create(libvlc, "decklink-sys", VLC_VAR_ADDRESS);
            var_SetAddress(libvlc, "decklink-sys", (void*)sys);
        }
    }

    vlc_mutex_unlock(&sys_lock);
    return sys;
}

static void ReleaseDLSys(vlc_object_t *obj)
{
    vlc_object_t *libvlc = VLC_OBJECT(obj->p_libvlc);

    vlc_mutex_lock(&sys_lock);

    struct decklink_sys_t *sys = (struct decklink_sys_t*)var_GetAddress(libvlc, "decklink-sys");

    if (--sys->users == 0) {
        msg_Dbg(obj, "Destroying decklink data");
        vlc_mutex_destroy(&sys->lock);
        vlc_cond_destroy(&sys->cond);

        if (sys->p_output) {
            sys->p_output->StopScheduledPlayback(0, NULL, 0);
            sys->p_output->DisableVideoOutput();
            sys->p_output->DisableAudioOutput();
            sys->p_output->Release();
        }

        free(sys);
        var_Destroy(libvlc, "decklink-sys");
    }

    vlc_mutex_unlock(&sys_lock);
}

// Connection mode
static BMDAudioConnection getAConn(audio_output_t *aout)
{
    BMDAudioConnection conn = bmdAudioConnectionEmbedded;
    char *psz = var_InheritString(aout, AUDIO_CFG_PREFIX "audio-connection");
    if (!psz)
        return conn;

    if (!strcmp(psz, "embedded"))
        conn = bmdAudioConnectionEmbedded;
    else if (!strcmp(psz, "aesebu"))
        conn = bmdAudioConnectionAESEBU;
    else if (!strcmp(psz, "analog"))
        conn = bmdAudioConnectionAnalog;

    free(psz);
    return conn;
}

static BMDVideoConnection getVConn(vout_display_t *vd)
{
    BMDVideoConnection conn = bmdVideoConnectionSDI;
    char *psz = var_InheritString(vd, VIDEO_CFG_PREFIX "video-connection");
    if (!psz)
        goto end;

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

end:
    free(psz);
    return conn;
}

/*****************************************************************************
 *
 *****************************************************************************/

static struct decklink_sys_t *OpenDecklink(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
#define CHECK(message) do { \
    if (result != S_OK) \
    { \
        msg_Err(vd, message ": 0x%X", result); \
        goto error; \
    } \
} while(0)

    HRESULT result;
    IDeckLinkIterator *decklink_iterator = NULL;
    IDeckLinkDisplayMode *p_display_mode = NULL;
    IDeckLinkDisplayModeIterator *p_display_iterator = NULL;
    IDeckLinkConfiguration *p_config = NULL;
    IDeckLink *p_card = NULL;

    struct decklink_sys_t *decklink_sys = GetDLSys(VLC_OBJECT(vd));
    vlc_mutex_lock(&decklink_sys->lock);
    decklink_sys->users++;

    /* wait until aout is ready */
    while (decklink_sys->aconn == 0)
        vlc_cond_wait(&decklink_sys->cond, &decklink_sys->lock);

    int i_card_index = var_InheritInteger(vd, CFG_PREFIX "card-index");
    BMDVideoConnection vconn = getVConn(vd);
    char *mode = var_InheritString(vd, VIDEO_CFG_PREFIX "mode");
    size_t len = mode ? strlen(mode) : 0;
    if (!mode || len > 4)
    {
        free(mode);
        msg_Err(vd, "Missing or invalid mode");
        goto error;
    }

    BMDDisplayMode wanted_mode_id;
    memset(&wanted_mode_id, ' ', 4);
    strncpy((char*)&wanted_mode_id, mode, 4);
    free(mode);

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

    const char *psz_model_name;
    result = p_card->GetModelName(&psz_model_name);
    CHECK("Unknown model name");

    msg_Dbg(vd, "Opened DeckLink PCI card %s", psz_model_name);

    result = p_card->QueryInterface(IID_IDeckLinkOutput,
        (void**)&decklink_sys->p_output);
    CHECK("No outputs");

    result = p_card->QueryInterface(IID_IDeckLinkConfiguration,
        (void**)&p_config);
    CHECK("Could not get config interface");

    if (vconn)
    {
        result = p_config->SetInt(
            bmdDeckLinkConfigVideoOutputConnection, vconn);
        CHECK("Could not set video output connection");
    }

    result = decklink_sys->p_output->GetDisplayModeIterator(&p_display_iterator);
    CHECK("Could not enumerate display modes");

    for (; ; p_display_mode->Release())
    {
        int w, h;
        result = p_display_iterator->Next(&p_display_mode);
        if (result != S_OK)
            break;

        BMDDisplayMode mode_id = ntohl(p_display_mode->GetDisplayMode());

        const char *psz_mode_name;
        result = p_display_mode->GetName(&psz_mode_name);
        CHECK("Could not get display mode name");

        result = p_display_mode->GetFrameRate(&decklink_sys->frameduration,
            &decklink_sys->timescale);
        CHECK("Could not get frame rate");

        w = p_display_mode->GetWidth();
        h = p_display_mode->GetHeight();
        msg_Dbg(vd, "Found mode '%4.4s': %s (%dx%d, %.3f fps)",
                (char*)&mode_id, psz_mode_name, w, h,
                double(decklink_sys->timescale) / decklink_sys->frameduration);
        msg_Dbg(vd, "scale %d dur %d", (int)decklink_sys->timescale,
            (int)decklink_sys->frameduration);

        if (wanted_mode_id != mode_id)
            continue;

        decklink_sys->i_width = w;
        decklink_sys->i_height = h;

        mode_id = htonl(mode_id);

        BMDVideoOutputFlags flags = bmdVideoOutputVANC;
        if (mode_id == bmdModeNTSC ||
            mode_id == bmdModeNTSC2398 ||
            mode_id == bmdModePAL)
        {
            flags = bmdVideoOutputVITC;
        }

        BMDDisplayModeSupport support;
        IDeckLinkDisplayMode *resultMode;

        result = decklink_sys->p_output->DoesSupportVideoMode(mode_id,
            sys->tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV,
            flags, &support, &resultMode);
        CHECK("Does not support video mode");
        if (support == bmdDisplayModeNotSupported)
        {
            msg_Err(vd, "Video mode not supported");
                goto error;
        }

        result = decklink_sys->p_output->EnableVideoOutput(mode_id, flags);
        CHECK("Could not enable video output");

        break;
    }

    if (decklink_sys->i_width < 0 || decklink_sys->i_width & 1)
    {
        msg_Err(vd, "Unknown video mode specified.");
        goto error;
    }

    result = p_config->SetInt(
        bmdDeckLinkConfigAudioInputConnection, decklink_sys->aconn);
    CHECK("Could not set audio connection");

    if (/*decklink_sys->i_channels > 0 &&*/ decklink_sys->i_rate > 0)
    {
        result = decklink_sys->p_output->EnableAudioOutput(
            decklink_sys->i_rate,
            bmdAudioSampleType16bitInteger,
            /*decklink_sys->i_channels*/ 2,
            bmdAudioOutputStreamTimestamped);
    }
    CHECK("Could not start audio output");

    /* start */
    result = decklink_sys->p_output->StartScheduledPlayback(
        (mdate() * decklink_sys->timescale) / CLOCK_FREQ, decklink_sys->timescale, 1.0);
    CHECK("Could not start playback");

    p_config->Release();
    p_display_mode->Release();
    p_display_iterator->Release();
    p_card->Release();
    decklink_iterator->Release();

    vlc_mutex_unlock(&decklink_sys->lock);

    return decklink_sys;

error:
    if (decklink_sys->p_output) {
        decklink_sys->p_output->Release();
        decklink_sys->p_output = NULL;
    }
    if (p_card)
        p_card->Release();
    if (p_config)
        p_config->Release();
    if (p_display_iterator)
        p_display_iterator->Release();
    if (decklink_iterator)
        decklink_iterator->Release();
    if (p_display_mode)
        p_display_mode->Release();

    vlc_mutex_unlock(&decklink_sys->lock);
    ReleaseDLSys(VLC_OBJECT(vd));

    return NULL;
#undef CHECK
}

/*****************************************************************************
 * Video
 *****************************************************************************/

static picture_pool_t *PoolVideo(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;
    if (!sys->pool)
        sys->pool = picture_pool_NewFromFormat(&vd->fmt, requested_count);
    return sys->pool;
}

static inline void put_le32(uint8_t **p, uint32_t d)
{
    SetDWLE(*p, d);
    (*p) += 4;
}

static inline int clip(int a)
{
    if      (a < 4) return 4;
    else if (a > 1019) return 1019;
    else               return a;
}

static void v210_convert(void *frame_bytes, picture_t *pic, int dst_stride)
{
    int width = pic->format.i_width;
    int height = pic->format.i_height;
    int line_padding = dst_stride - ((width * 8 + 11) / 12) * 4;
    int h, w;
    uint8_t *data = (uint8_t*)frame_bytes;

    const uint16_t *y = (const uint16_t*)pic->p[0].p_pixels;
    const uint16_t *u = (const uint16_t*)pic->p[1].p_pixels;
    const uint16_t *v = (const uint16_t*)pic->p[2].p_pixels;

#define WRITE_PIXELS(a, b, c)           \
    do {                                \
        val =   clip(*a++);             \
        val |= (clip(*b++) << 10) |     \
               (clip(*c++) << 20);      \
        put_le32(&data, val);           \
    } while (0)

    for (h = 0; h < height; h++) {
        uint32_t val = 0;
        for (w = 0; w < width - 5; w += 6) {
            WRITE_PIXELS(u, y, v);
            WRITE_PIXELS(y, u, y);
            WRITE_PIXELS(v, y, u);
            WRITE_PIXELS(y, v, y);
        }
        if (w < width - 1) {
            WRITE_PIXELS(u, y, v);

            val = clip(*y++);
            if (w == width - 2)
                put_le32(&data, val);
#undef WRITE_PIXELS
        }
        if (w < width - 3) {
            val |= (clip(*u++) << 10) | (clip(*y++) << 20);
            put_le32(&data, val);

            val = clip(*v++) | (clip(*y++) << 10);
            put_le32(&data, val);
        }

        memset(data, 0, line_padding);
        data += line_padding;

        y += pic->p[0].i_pitch / 2 - width;
        u += pic->p[1].i_pitch / 2 - width / 2;
        v += pic->p[2].i_pitch / 2 - width / 2;
    }
}

static void DisplayVideo(vout_display_t *vd, picture_t *picture, subpicture_t *)
{
    vout_display_sys_t *sys = vd->sys;
    struct decklink_sys_t *decklink_sys = GetDLSys(VLC_OBJECT(vd));

    if (!picture)
        return;

    HRESULT result;
    int w, h, stride, length;
    mtime_t now;
    w = decklink_sys->i_width;
    h = decklink_sys->i_height;

    IDeckLinkMutableVideoFrame *pDLVideoFrame;
    result = decklink_sys->p_output->CreateVideoFrame(w, h, w*3,
        sys->tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV,
        bmdFrameFlagDefault, &pDLVideoFrame);

    if (result != S_OK) {
        msg_Err(vd, "Failed to create video frame: 0x%X", result);
        pDLVideoFrame = NULL;
        goto end;
    }

    void *frame_bytes;
    pDLVideoFrame->GetBytes((void**)&frame_bytes);
    stride = pDLVideoFrame->GetRowBytes();

    if (sys->tenbits)
        v210_convert(frame_bytes, picture, stride);
    else for(int y = 0; y < h; ++y) {
        uint8_t *dst = (uint8_t *)frame_bytes + stride * y;
        const uint8_t *src = (const uint8_t *)picture->p[0].p_pixels +
            picture->p[0].i_pitch * y;
        memcpy(dst, src, w * 2 /* bpp */);
    }


    // compute frame duration in CLOCK_FREQ units
    length = (decklink_sys->frameduration * CLOCK_FREQ) / decklink_sys->timescale;

    picture->date -= decklink_sys->offset;
    result = decklink_sys->p_output->ScheduleVideoFrame(pDLVideoFrame,
        picture->date, length, CLOCK_FREQ);

    if (result != S_OK) {
        msg_Err(vd, "Dropped Video frame %"PRId64 ": 0x%x",
            picture->date, result);
        goto end;
    }

    now = mdate() - decklink_sys->offset;

    BMDTimeValue decklink_now;
    double speed;
    decklink_sys->p_output->GetScheduledStreamTime (CLOCK_FREQ, &decklink_now, &speed);

    if ((now - decklink_now) > 400000) {
        /* XXX: workaround card clock drift */
        decklink_sys->offset += 50000;
        msg_Err(vd, "Delaying: offset now %"PRId64"", decklink_sys->offset);
    }

end:
    if (pDLVideoFrame)
        pDLVideoFrame->Release();
    picture_Release(picture);
}

static int ControlVideo(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(vd);
    const vout_display_cfg_t *cfg;

    switch (query) {
    case VOUT_DISPLAY_CHANGE_FULLSCREEN:
        cfg = va_arg(args, const vout_display_cfg_t *);
        return cfg->is_fullscreen ? VLC_EGENERIC : VLC_SUCCESS;
    default:
        return VLC_EGENERIC;
    }
}

static int OpenVideo(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys;
    struct decklink_sys_t *decklink_sys;

    vd->sys = sys = (vout_display_sys_t*)malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->tenbits = var_InheritBool(p_this, VIDEO_CFG_PREFIX "tenbits");

    decklink_sys = OpenDecklink(vd);
    if (!decklink_sys) {
        free(sys);
        return VLC_EGENERIC;
    }

    sys->pool = NULL;

    vd->fmt.i_chroma = sys->tenbits
        ? VLC_CODEC_I422_10L /* we will convert to v210 */
        : VLC_CODEC_UYVY;
    //video_format_FixRgb(&(vd->fmt));

    vd->fmt.i_width = decklink_sys->i_width;
    vd->fmt.i_height = decklink_sys->i_height;

    vd->info.has_hide_mouse = true;
    vd->pool    = PoolVideo;
    vd->prepare = NULL;
    vd->display = DisplayVideo;
    vd->control = ControlVideo;
    vd->manage  = NULL;
    vout_display_SendEventFullscreen(vd, false);

    return VLC_SUCCESS;
}

static void CloseVideo(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Delete(sys->pool);

    free(sys);

    ReleaseDLSys(p_this);
}

/*****************************************************************************
 * Audio
 *****************************************************************************/

static void Flush (audio_output_t *aout, bool drain)
{
    struct decklink_sys_t *decklink_sys = GetDLSys(VLC_OBJECT(aout));
    vlc_mutex_lock(&decklink_sys->lock);
    IDeckLinkOutput *p_output = decklink_sys->p_output;
    vlc_mutex_unlock(&decklink_sys->lock);
    if (!p_output)
        return;

    if (drain) {
        uint32_t samples;
        decklink_sys->p_output->GetBufferedAudioSampleFrameCount(&samples);
        msleep(CLOCK_FREQ * samples / decklink_sys->i_rate);
    } else if (decklink_sys->p_output->FlushBufferedAudioSamples() == E_FAIL)
        msg_Err(aout, "Flush failed");
}

static int TimeGet(audio_output_t *, mtime_t* restrict)
{
    /* synchronization is handled by the card */
    return -1;
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    struct decklink_sys_t *decklink_sys = GetDLSys(VLC_OBJECT(aout));

    if (decklink_sys->i_rate == 0)
        return VLC_EGENERIC;

    fmt->i_format = VLC_CODEC_S16N;
    fmt->i_channels = 2; //decklink_sys->i_channels;
    fmt->i_physical_channels = AOUT_CHANS_STEREO; //pi_channels_maps[fmt->i_channels];
    fmt->i_rate = decklink_sys->i_rate;
    fmt->i_bitspersample = 16;
    fmt->i_blockalign = fmt->i_channels * fmt->i_bitspersample /8 ;
    fmt->i_frame_length  = FRAME_SIZE;

    return VLC_SUCCESS;
}

static void PlayAudio(audio_output_t *aout, block_t *audio)
{
    struct decklink_sys_t *decklink_sys = GetDLSys(VLC_OBJECT(aout));
    vlc_mutex_lock(&decklink_sys->lock);
    IDeckLinkOutput *p_output = decklink_sys->p_output;
    vlc_mutex_unlock(&decklink_sys->lock);
    if (!p_output) {
        block_Release(audio);
        return;
    }

    audio->i_pts -= decklink_sys->offset;

    uint32_t sampleFrameCount = audio->i_buffer / (2 * 2 /*decklink_sys->i_channels*/);
    uint32_t written;
    HRESULT result = decklink_sys->p_output->ScheduleAudioSamples(
            audio->p_buffer, sampleFrameCount, audio->i_pts, CLOCK_FREQ, &written);

    if (result != S_OK)
        msg_Err(aout, "Failed to schedule audio sample: 0x%X", result);
    else if (sampleFrameCount != written)
        msg_Err(aout, "Written only %d samples out of %d", written, sampleFrameCount);

    block_Release(audio);
}

static int OpenAudio(vlc_object_t *p_this)
{
    audio_output_t *aout = (audio_output_t *)p_this;
    struct decklink_sys_t *decklink_sys = GetDLSys(VLC_OBJECT(aout));

    vlc_mutex_lock(&decklink_sys->lock);
    decklink_sys->aconn = getAConn(aout);
    //decklink_sys->i_channels = var_InheritInteger(vd, AUDIO_CFG_PREFIX "audio-channels");
    decklink_sys->i_rate = var_InheritInteger(aout, AUDIO_CFG_PREFIX "audio-rate");
    decklink_sys->users++;
    vlc_cond_signal(&decklink_sys->cond);
    vlc_mutex_unlock(&decklink_sys->lock);

    aout->play      = PlayAudio;
    aout->start     = Start;
    aout->flush     = Flush;
    aout->time_get  = TimeGet;

    aout->pause     = NULL;
    aout->stop      = NULL;
    aout->mute_set  = NULL;
    aout->volume_set= NULL;

    return VLC_SUCCESS;
}

static void CloseAudio(vlc_object_t *p_this)
{
    struct decklink_sys_t *decklink_sys = GetDLSys(p_this);
    vlc_mutex_lock(&decklink_sys->lock);
    decklink_sys->aconn = 0;
    vlc_mutex_unlock(&decklink_sys->lock);
    ReleaseDLSys(p_this);
}
