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
 * TODO:
 *  - test non stereo audio
 *  - inherit aout/vout settings from corresponding module
 *  (allow to change settings between successive runs per instance)
 *  - allow several instances per process
 *  - get rid of process-wide destructor
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


#define RATE_TEXT N_("Audio sampling rate in Hz")
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
static struct
{
    IDeckLink *p_card;
    IDeckLinkOutput *p_output;
    IDeckLinkConfiguration *p_config;
    IDeckLinkDisplayModeIterator *p_display_iterator;
    IDeckLinkIterator *decklink_iterator;

    //int i_channels;
    int i_rate;

    int i_width;
    int i_height;

    BMDTimeScale timescale;
    BMDTimeValue frameduration;

    /* XXX: workaround card clock drift */
    mtime_t offset;
} decklink_sys = {
    NULL, NULL, NULL, NULL, NULL,
    0, 0,
    -1, -1,
    0, 0,
    0,
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

// Connection mode
static BMDAudioConnection getAConn(vlc_object_t *p_this)
{
    BMDAudioConnection conn = bmdAudioConnectionEmbedded;
    char *psz = var_InheritString(p_this, AUDIO_CFG_PREFIX "audio-connection");
    if (!psz)
        goto end;

    if (!strcmp(psz, "embedded"))
        conn = bmdAudioConnectionEmbedded;
    else if (!strcmp(psz, "aesebu"))
        conn = bmdAudioConnectionAESEBU;
    else if (!strcmp(psz, "analog"))
        conn = bmdAudioConnectionAnalog;

end:
    free(psz);
    return conn;
}

static BMDVideoConnection getVConn(vlc_object_t *p_this)
{
    BMDVideoConnection conn = bmdVideoConnectionSDI;
    char *psz = var_InheritString(p_this, VIDEO_CFG_PREFIX "video-connection");
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

static atomic_uint initialized = ATOMIC_VAR_INIT(0);

static void CloseDecklink(void) __attribute__((destructor));
static void CloseDecklink(void)
{
    if (!atomic_load(&initialized))
        return;

    decklink_sys.p_output->StopScheduledPlayback(0, NULL, 0);
    decklink_sys.p_output->DisableVideoOutput();
    decklink_sys.p_output->DisableAudioOutput();

    if (decklink_sys.decklink_iterator)
        decklink_sys.decklink_iterator->Release();

    if (decklink_sys.p_display_iterator)
        decklink_sys.p_display_iterator->Release();

    if (decklink_sys.p_config)
        decklink_sys.p_config->Release();

    if (decklink_sys.p_output)
        decklink_sys.p_output->Release();

    if (decklink_sys.p_card)
        decklink_sys.p_card->Release();
}

static int OpenDecklink(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys = vd->sys;
#define CHECK(message) do { \
    if (result != S_OK) \
    { \
        msg_Err(p_this, message ": 0x%X", result); \
        goto error; \
    } \
} while(0)

    HRESULT result;
    IDeckLinkDisplayMode *p_display_mode = NULL;
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;

    vlc_mutex_lock(&lock);

    if (atomic_load(&initialized)) {
        /* already initialized */
        vlc_mutex_unlock(&lock);
        return VLC_SUCCESS;
    }

    //decklink_sys.i_channels = var_InheritInteger(p_this, AUDIO_CFG_PREFIX "audio-channels");
    decklink_sys.i_rate = var_InheritInteger(p_this, AUDIO_CFG_PREFIX "audio-rate");
    int i_card_index = var_InheritInteger(p_this, CFG_PREFIX "card-index");
    BMDVideoConnection vconn = getVConn(p_this);
    BMDAudioConnection aconn = getAConn(p_this);
    char *mode = var_InheritString(p_this, VIDEO_CFG_PREFIX "mode");
    size_t len = mode ? strlen(mode) : 0;
    if (!mode || len > 4)
    {
        free(mode);
        msg_Err(p_this, "Missing or invalid mode");
        goto error;
    }

    BMDDisplayMode wanted_mode_id;
    memset(&wanted_mode_id, ' ', 4);
    strncpy((char*)&wanted_mode_id, mode, 4);
    free(mode);

    if (i_card_index < 0)
    {
        msg_Err(p_this, "Invalid card index %d", i_card_index);
        goto error;
    }

    decklink_sys.decklink_iterator = CreateDeckLinkIteratorInstance();
    if (!decklink_sys.decklink_iterator)
    {
        msg_Err(p_this, "DeckLink drivers not found.");
        goto error;
    }

    for(int i = 0; i <= i_card_index; ++i)
    {
        if (decklink_sys.p_card)
            decklink_sys.p_card->Release();
        result = decklink_sys.decklink_iterator->Next(&decklink_sys.p_card);
        CHECK("Card not found");
    }

    const char *psz_model_name;
    result = decklink_sys.p_card->GetModelName(&psz_model_name);
    CHECK("Unknown model name");

    msg_Dbg(p_this, "Opened DeckLink PCI card %s", psz_model_name);

    result = decklink_sys.p_card->QueryInterface(IID_IDeckLinkOutput,
        (void**)&decklink_sys.p_output);
    CHECK("No outputs");

    result = decklink_sys.p_card->QueryInterface(IID_IDeckLinkConfiguration,
        (void**)&decklink_sys.p_config);
    CHECK("Could not get config interface");

    if (vconn)
    {
        result = decklink_sys.p_config->SetInt(
            bmdDeckLinkConfigVideoOutputConnection, vconn);
        CHECK("Could not set video output connection");
    }

    if (aconn)
    {
        result = decklink_sys.p_config->SetInt(
            bmdDeckLinkConfigAudioInputConnection, aconn);
        CHECK("Could not set audio output connection");
    }

    result = decklink_sys.p_output->GetDisplayModeIterator(&decklink_sys.p_display_iterator);
    CHECK("Could not enumerate display modes");

    for (; ; p_display_mode->Release())
    {
        int w, h;
        result = decklink_sys.p_display_iterator->Next(&p_display_mode);
        if (result != S_OK)
            break;

        BMDDisplayMode mode_id = ntohl(p_display_mode->GetDisplayMode());

        const char *psz_mode_name;
        result = p_display_mode->GetName(&psz_mode_name);
        CHECK("Could not get display mode name");

        result = p_display_mode->GetFrameRate(&decklink_sys.frameduration,
            &decklink_sys.timescale);
        CHECK("Could not get frame rate");

        w = p_display_mode->GetWidth();
        h = p_display_mode->GetHeight();
        msg_Dbg(p_this, "Found mode '%4.4s': %s (%dx%d, %.3f fps)",
                (char*)&mode_id, psz_mode_name, w, h,
                double(decklink_sys.timescale) / decklink_sys.frameduration);
        msg_Dbg(p_this, "scale %d dur %d", (int)decklink_sys.timescale,
            (int)decklink_sys.frameduration);

        if (wanted_mode_id != mode_id)
            continue;

        decklink_sys.i_width = w;
        decklink_sys.i_height = h;

        p_display_mode->Release();
        p_display_mode = NULL;

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

        result = decklink_sys.p_output->DoesSupportVideoMode(mode_id,
            sys->tenbits ? bmdFormat10BitYUV : bmdFormat8BitYUV,
            flags, &support, &resultMode);
        CHECK("Does not support video mode");
        if (support == bmdDisplayModeNotSupported)
        {
            msg_Err(p_this, "Video mode not supported");
                goto error;
        }

        result = decklink_sys.p_output->EnableVideoOutput(mode_id, flags);
        CHECK("Could not enable video output");

        break;
    }

    if (decklink_sys.i_width < 0)
    {
        msg_Err(p_this, "Unknown video mode specified.");
        goto error;
    }

    /* audio */
    if (/*decklink_sys.i_channels > 0 &&*/ decklink_sys.i_rate > 0)
    {
        result = decklink_sys.p_output->EnableAudioOutput(
            decklink_sys.i_rate,
            bmdAudioSampleType16bitInteger,
            /*decklink_sys.i_channels*/ 2,
            bmdAudioOutputStreamTimestamped);
    }
    else
    {
        result = decklink_sys.p_output->DisableAudioOutput();
    }
    CHECK("Could not enable audio output");


    /* start */
    result = decklink_sys.p_output->StartScheduledPlayback(
        (mdate() * decklink_sys.timescale) / CLOCK_FREQ, decklink_sys.timescale, 1.0);
    CHECK("Could not start playback");

    atomic_store(&initialized, 1);

    vlc_mutex_unlock(&lock);
    return VLC_SUCCESS;


error:
    if (decklink_sys.decklink_iterator)
        decklink_sys.decklink_iterator->Release();
    if (decklink_sys.p_display_iterator)
        decklink_sys.p_display_iterator->Release();
    if (p_display_mode)
        p_display_mode->Release();

    vlc_mutex_unlock(&lock);
    return VLC_EGENERIC;
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

    if (!picture)
        return;

    HRESULT result;
    int w, h, stride, length;
    mtime_t now;
    w = decklink_sys.i_width;
    h = decklink_sys.i_height;

    IDeckLinkMutableVideoFrame *pDLVideoFrame;
    result = decklink_sys.p_output->CreateVideoFrame(w, h, w*3,
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
    length = (decklink_sys.frameduration * CLOCK_FREQ) / decklink_sys.timescale;

    picture->date -= decklink_sys.offset;
    result = decklink_sys.p_output->ScheduleVideoFrame(pDLVideoFrame,
        picture->date, length, CLOCK_FREQ);

    if (result != S_OK) {
        msg_Err(vd, "Dropped Video frame %"PRId64 ": 0x%x",
            picture->date, result);
        goto end;
    }

    now = mdate() - decklink_sys.offset;

    BMDTimeValue decklink_now;
    double speed;
    decklink_sys.p_output->GetScheduledStreamTime (CLOCK_FREQ, &decklink_now, &speed);

    if ((now - decklink_now) > 400000) {
        /* XXX: workaround card clock drift */
        decklink_sys.offset += 50000;
        msg_Err(vd, "Delaying: offset now %"PRId64"", decklink_sys.offset);
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

static atomic_uint video_lock = ATOMIC_VAR_INIT(0);
static int OpenVideo(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys;

    if (atomic_exchange(&video_lock, 1)) {
        msg_Err(vd, "Decklink video module already busy");
        return VLC_EGENERIC;
    }

    vd->sys = sys = (vout_display_sys_t*)malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    if (OpenDecklink(p_this) != VLC_SUCCESS)
        goto error;

    if (decklink_sys.i_width & 1) {
        msg_Err(vd, "Invalid width %d", decklink_sys.i_width);
        goto error;
    }

    sys->pool = NULL;

    sys->tenbits = var_InheritBool(p_this, VIDEO_CFG_PREFIX "tenbits");
    vd->fmt.i_chroma = sys->tenbits
        ? VLC_CODEC_I422_10L /* we will convert to v210 */
        : VLC_CODEC_UYVY;
    //video_format_FixRgb(&(vd->fmt));

    vd->fmt.i_width = decklink_sys.i_width;
    vd->fmt.i_height = decklink_sys.i_height;

    vd->info.has_hide_mouse = true;
    vd->pool    = PoolVideo;
    vd->prepare = NULL;
    vd->display = DisplayVideo;
    vd->control = ControlVideo;
    vd->manage  = NULL;
    vout_display_SendEventFullscreen(vd, false);

    return VLC_SUCCESS;

error:
    free(sys);
    return VLC_EGENERIC;
}

static void CloseVideo(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Delete(sys->pool);

    free(sys);

    atomic_fetch_sub(&video_lock, 1);
}

/*****************************************************************************
 * Audio
 *****************************************************************************/

static void Flush (audio_output_t *aout, bool drain)
{
    if (!atomic_load(&initialized))
        return;

    if (drain) {
        uint32_t samples;
        decklink_sys.p_output->GetBufferedAudioSampleFrameCount(&samples);
        msleep(CLOCK_FREQ * samples / decklink_sys.i_rate);
    } else if (decklink_sys.p_output->FlushBufferedAudioSamples() == E_FAIL)
        msg_Err(aout, "Flush failed");
}

static int TimeGet(audio_output_t *, mtime_t* restrict)
{
    /* synchronization is handled by the card */
    return -1;
}

static int Start(audio_output_t *, audio_sample_format_t *restrict fmt)
{
    fmt->i_format = VLC_CODEC_S16N;
    fmt->i_channels = 2; //decklink_sys.i_channels;
    fmt->i_physical_channels = AOUT_CHANS_STEREO; //pi_channels_maps[fmt->i_channels];
    fmt->i_rate = decklink_sys.i_rate;
    fmt->i_bitspersample = 16;
    fmt->i_blockalign = fmt->i_channels * fmt->i_bitspersample /8 ;
    fmt->i_frame_length  = FRAME_SIZE;

    return VLC_SUCCESS;
}

static void PlayAudio(audio_output_t *aout, block_t *audio)
{
    if (!atomic_load(&initialized))
        return;

    audio->i_pts -= decklink_sys.offset;

    uint32_t sampleFrameCount = audio->i_buffer / (2 * 2 /*decklink_sys.i_channels*/);
    uint32_t written;
    HRESULT result = decklink_sys.p_output->ScheduleAudioSamples(
            audio->p_buffer, sampleFrameCount, audio->i_pts, CLOCK_FREQ, &written);

    if (result != S_OK)
        msg_Err(aout, "Failed to schedule audio sample: 0x%X", result);
    else if (sampleFrameCount != written)
        msg_Err(aout, "Written only %d samples out of %d", written, sampleFrameCount);

    block_Release(audio);
}

static atomic_uint audio_lock = ATOMIC_VAR_INIT(0);
static int OpenAudio(vlc_object_t *p_this)
{
    audio_output_t *aout = (audio_output_t *)p_this;

    if (atomic_exchange(&audio_lock, 1)) {
        msg_Err(aout, "Decklink audio module already busy");
        return VLC_EGENERIC;
    }

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

static void CloseAudio(vlc_object_t *)
{
    atomic_fetch_sub(&audio_lock, 1);
}
