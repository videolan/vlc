/*****************************************************************************
 * decoder.c: Functions for the management of decoders
 *****************************************************************************
 * Copyright (C) 1999-2019 VLC authors, VideoLAN and Videolabs SAS
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>
#include <stdatomic.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_sout.h>
#include <vlc_codec.h>
#include <vlc_spu.h>
#include <vlc_meta.h>
#include <vlc_dialog.h>
#include <vlc_modules.h>
#include <vlc_picture_pool.h>
#include <vlc_tracer.h>
#include <vlc_list.h>
#include <vlc_replay_gain.h>

#include "audio_output/aout_internal.h"
#include "stream_output/stream_output.h"
#include "../clock/clock.h"
#include "decoder.h"
#include "resource.h"
#include "decoder_prevframe.h"

#include "../libvlc.h"

#include "../video_output/vout_internal.h"


/**
 * \file src/input/decoder.c
 *
 * The input decoder connects the input client pushing data to the
 * decoder implementation (through the matching elementary stream)
 * and the following output for audio, video and subtitles.
 *
 * It follows the locking rules below:
 *
 *  - The fifo cannot be locked when calling function from the
 *    decoder module implementation.
 *
 *  - However, the decoder module implementation might indirectly
 *    lock the fifo when calling the owner methods, in particular
 *    to send a frame or update the output status.
 *
 *  - The input code can lock the fifo to modify the global state
 *    of the input decoder.
 *
 * Backpressure preventing starvation is done by the pacing of the
 * decoder, the calls into the decoder implementation, and the
 * limits of the fifo queue.
 *
 * Basically a very fast decoder will often wait since the fifo will be
 * consumed really quickly and thus almost never stay under the lock.
 * Likewise, when the decoder is slower and the fifo can grow, it also
 * means that the decoder thread will wait more often on the
 * `decoder_t::pf_decode` call, which is done without the fifo lock as
 * per above rules.
 *
 * In addition with the standard input/output cycle from the decoder,
 * the video decoders can create sub-decoders for the closed captions
 * support embedded in the supplementary information from the codecs.
 *
 * To do so, they need to create a `decoder_cc_desc_t` matching with the
 * format that needs to be described (number of channels, type of
 * channels) and they then create them along with the closed-captions
 * content with `decoder_QueueCc`.
 *
 * In the `input/decoder.c` code, the access to the sub-decoders in the
 * subdecs.list table is protected through the `subdecs.lock` mutex.
 * Taking this lock ensures that the sub-decoder won't get
 * asynchronously removed while using it, and any mutex from the
 * sub-decoder can then be taken under this lock.
 **/

/*
 * Possibles values set in p_owner->reload atomic
 */
enum reload
{
    RELOAD_NO_REQUEST,
    RELOAD_DECODER,     /* Reload the decoder module */
    RELOAD_DECODER_AOUT /* Stop the aout and reload the decoder module */
};

struct decoder_video
{
    vout_thread_t *vout;
    enum vlc_vout_order vout_order;
    bool started;
    bool drained;

    /* pool to use when the decoder doesn't use its own */
    struct picture_pool_t *out_pool;
    vlc_video_context *vctx;

    /* Mouse event */
    vlc_mutex_t mouse_lock;
    vlc_mouse_event mouse_event;
    void *mouse_opaque;

    /* previous-frame */
    struct decoder_prevframe pf;
    vlc_tick_t pf_pts;
};

struct decoder_audio
{
    /* If aout is valid, then astream is valid too */
    audio_output_t *aout;
    vlc_aout_stream *stream;
};

struct decoder_spu
{
    vout_thread_t *vout;
    ssize_t channel;
    int64_t order;
};

struct vlc_input_decoder_t
{
    decoder_t        dec;
    es_format_t      dec_fmt_in;
    input_resource_t*p_resource;
    vlc_clock_t     *p_clock;
    const char *psz_id;

    bool hw_dec;

    const struct vlc_input_decoder_callbacks *cbs;
    void *cbs_userdata;

    vlc_thread_t     thread;

    /* Some decoders require already packetized data (ie. not truncated) */
    decoder_t *p_packetizer;
    es_format_t pktz_fmt_in;
    bool b_packetizer;

    /* initial and immutable category */
    enum es_format_category_e cat;
    /* Current format in use by the output */
    es_format_t    fmt;

    /* */
    bool           b_fmt_description;
    vlc_meta_t     *p_description;
    atomic_int     reload;

    /* fifo */
    block_fifo_t *p_fifo;

    /* Lock for communication with decoder thread */
    vlc_cond_t  wait_request;
    vlc_cond_t  wait_acknowledge;
    vlc_cond_t  wait_fifo; /* TODO: merge with wait_acknowledge */

    /*
     * 3 threads can read/write these output variables, the DecoderThread, the
     * input thread, and the ModuleThread. The ModuleThread is either the
     * DecoderThread for synchronous modules or any thread for asynchronous
     * modules.
     *
     * Asynchronous modules are responsible for serializing/locking every
     * output calls in any thread as long as the decoder_UpdateVideoFormat() or
     * decoder_NewPicture() calls are not concurrent, cf.
     * decoder_UpdateVideoFormat() and decoder_NewPicture() notes.
     *
     * The ModuleThread is the owner of these variables, it should hold
     * the lock when writing them but doesn't have to hold it when using them.
     *
     * The DecoderThread should always hold the lock when reading/using
     * aout/vouts.
     *
     * The input thread can read these variables in order to stop outputs, when
     * both ModuleThread and DecoderThread are stopped (from DecoderDelete()).
     */
     union {
        struct decoder_video video;
        struct decoder_audio audio;
        struct decoder_spu spu;
    };

    sout_stream_t *p_sout;
    sout_packetizer_input_t *p_sout_input;

    /* -- Theses variables need locking on read *and* write -- */
    /* Preroll */
    vlc_tick_t i_preroll_end;

#define PREROLL_NONE   VLC_TICK_MIN
#define PREROLL_FORCED VLC_TICK_MAX

    /* Pause & Rate */
    vlc_tick_t pause_date;
    vlc_tick_t delay, output_delay;
    float rate, output_rate;
    int frames_countdown;
    bool paused, output_paused;

    bool error;

    /* Waiting */
    bool b_waiting;
    bool b_first;
    bool b_has_data;
    bool out_started;

    /* Flushing */
    bool flushing;
    bool b_draining;
    bool b_idle;
    bool aborting;

    /* Sub decs */
    struct
    {
        vlc_mutex_t lock;
        struct vlc_list list;
    } subdecs;

    /* CC */
    struct
    {
        /* All members guarded by subdecs.lock */
        size_t count;
        vlc_fourcc_t selected_codec;
        bool b_supported;
        decoder_cc_desc_t desc;
        bool desc_changed;
        bool b_sout_created;
        sout_packetizer_input_t *p_sout_input;
        char *sout_es_id;
    } cc;

    vlc_input_decoder_t *master_dec;

    struct vlc_list node;
};

/* Pictures which are DECODER_BOGUS_VIDEO_DELAY or more in advance probably have
 * a bogus PTS and won't be displayed */
#define DECODER_BOGUS_VIDEO_DELAY                ((vlc_tick_t)(DEFAULT_PTS_DELAY * 30))

/* */
#define DECODER_SPU_VOUT_WAIT_DURATION   VLC_TICK_FROM_MS(200)
#define BLOCK_FLAG_CORE_PRIVATE_RELOADED (1 << BLOCK_FLAG_CORE_PRIVATE_SHIFT)

#define decoder_Notify(decoder_priv, event, ...) \
    if (decoder_priv->cbs && decoder_priv->cbs->event) \
        decoder_priv->cbs->event(decoder_priv, __VA_ARGS__, \
                                 decoder_priv->cbs_userdata);

static inline vlc_input_decoder_t *dec_get_owner( decoder_t *p_dec )
{
    return container_of( p_dec, vlc_input_decoder_t, dec );
}

/**
 * When the input decoder is being used only for packetizing (happen in stream output
 * configuration.), there's no need to spawn a decoder thread. The input_decoder is then considered
 * *synchronous*.
 *
 * @retval true When no decoder thread will be spawned.
 * @retval false When a decoder thread will be spawned.
 */
static inline bool vlc_input_decoder_IsSynchronous( const vlc_input_decoder_t *dec )
{
    return dec->p_sout != NULL;
}

static void Decoder_SeekPreviousFrame(vlc_input_decoder_t *owner, int steps,
                                      bool failed)
{
    vlc_fifo_Assert(owner->p_fifo);
    assert(steps != DEC_PF_SEEK_STEPS_NONE);

    if (steps > DEC_PF_SEEK_STEPS_MAX)
    {
        /* Too much failing attempt */
        decoder_Notify(owner, frame_previous_status, -ERANGE);
    }
    else
    {
        vlc_tick_t pts = owner->video.pf_pts;
        unsigned frame_rate = owner->fmt.video.i_frame_rate;
        unsigned frame_rate_base = owner->fmt.video.i_frame_rate_base;
        /* Request the input to seek back */
        decoder_Notify(owner, frame_previous_seek, pts,
                       frame_rate, frame_rate_base, steps, failed);
    }
}

static void Decoder_PausedForNextFrame(vlc_input_decoder_t *owner)
{
    vlc_fifo_Assert(owner->p_fifo);
    assert(owner->cat == VIDEO_ES);
    assert(owner->output_paused);

    if (owner->video.vout == NULL)
        return;

    if (likely(owner->frames_countdown <= 0))
        return;

    /* Handle all next-frame requests that were sent while the video was
    *  pausing */
    int next_request_count = owner->frames_countdown;
    owner->frames_countdown = vout_NextPicture(owner->video.vout, next_request_count);

    assert(next_request_count >= owner->frames_countdown);
    /* Notify for pictures that are processes by the vout */
    for (int i = 0; i < next_request_count - owner->frames_countdown; ++i)
        decoder_Notify(owner, frame_next_status, 0);
}

static void Decoder_DisplayPreviousFrame(vlc_input_decoder_t *owner, picture_t *pic)
{
    vlc_fifo_Assert(owner->p_fifo);

    if (owner->video.vout == NULL)
    {
        picture_Release(pic);
        return;
    }

    vout_PutPicture(owner->video.vout, pic);
    vout_NextPicture(owner->video.vout, 1);

    decoder_Notify(owner, frame_previous_status, 0);
}

static picture_t *Decoder_HandlePreviousFrame(vlc_input_decoder_t *owner,
                                              picture_t *pic)
{
    int seek_steps;
    pic = decoder_prevframe_AddPic(&owner->video.pf, pic,
                                   &owner->video.pf_pts, &seek_steps);
    if (pic != NULL)
    {
        owner->b_first = false;
        picture_t *resume_pic = pic->p_next;
        assert(resume_pic->p_next == NULL);
        pic->p_next = NULL;
        Decoder_DisplayPreviousFrame(owner, pic);
        /* Keep picture for normal playback or next-frame (if resumed) */
        pic = resume_pic;
    }

    if (seek_steps != DEC_PF_SEEK_STEPS_NONE)
        Decoder_SeekPreviousFrame(owner, seek_steps, pic == NULL);

    if (pic == NULL)
        return NULL;

    /* Wait for a new prev-frame request. If we don't wait, we will fill the
     * vout with frames following the prev-frame, they won't be displayed but
     * this will make next flush quite difficult to handle. When both
     * prev-frame and resumed frames are in the fifo, it's ambiguous which
     * frames should be flushed vs. retained during a flush operation. */
    while (owner->frames_countdown == -1
        && !decoder_prevframe_IsActive(&owner->video.pf))
        vlc_fifo_Wait(owner->p_fifo);

    if (decoder_prevframe_IsActive(&owner->video.pf))
    {
        picture_Release(pic);
        return NULL;
    }
    return pic;
}

static void Decoder_RequestFramePrevious(vlc_input_decoder_t *owner)
{
    vlc_fifo_Assert(owner->p_fifo);
    assert(owner->video.pf_pts != VLC_TICK_INVALID);

    int seek_steps;
    decoder_prevframe_Request(&owner->video.pf, &seek_steps);

    if (seek_steps != DEC_PF_SEEK_STEPS_NONE)
        Decoder_SeekPreviousFrame(owner, seek_steps, false);
    vlc_fifo_Signal(owner->p_fifo);
}

static void Decoder_ChangeOutputPause( vlc_input_decoder_t *p_owner, bool paused, vlc_tick_t date )
{
    vlc_fifo_Assert(p_owner->p_fifo);

    decoder_t *p_dec = &p_owner->dec;

    switch( p_dec->fmt_in->i_cat )
    {
        case VIDEO_ES:
            msg_Dbg( p_dec, "%s video", paused ? "pausing" : "resuming" );
            if( p_owner->video.vout != NULL && p_owner->video.started )
                vout_ChangePause( p_owner->video.vout, paused, date );
            break;
        case AUDIO_ES:
            msg_Dbg( p_dec, "%s audio", paused ? "pausing" : "resuming" );
            if( p_owner->audio.stream != NULL )
                vlc_aout_stream_ChangePause( p_owner->audio.stream, paused, date );
            break;
        case SPU_ES:
            msg_Dbg( p_dec, "not really %s SPU", paused ? "pausing" : "resuming" );
            break;
        default:
            vlc_assert_unreachable();
    }
    p_owner->output_paused = paused;
}

static void Decoder_ChangeOutputRate( vlc_input_decoder_t *p_owner, float rate )
{
    vlc_fifo_Assert(p_owner->p_fifo);

    decoder_t *p_dec = &p_owner->dec;

    msg_Dbg( p_dec, "changing rate: %f", rate );
    switch( p_dec->fmt_in->i_cat )
    {
        case VIDEO_ES:
            if( p_owner->video.vout != NULL && p_owner->video.started )
                vout_ChangeRate( p_owner->video.vout, rate );
            break;
        case AUDIO_ES:
            if( p_owner->audio.stream != NULL )
                vlc_aout_stream_ChangeRate( p_owner->audio.stream, rate );
            break;
        case SPU_ES:
            if( p_owner->spu.vout != NULL )
            {
                assert(p_owner->spu.channel != VOUT_SPU_CHANNEL_INVALID);
                vout_ChangeSpuRate(p_owner->spu.vout, p_owner->spu.channel,
                                   rate );
            }
            break;
        default:
            vlc_assert_unreachable();
    }
    p_owner->output_rate = rate;
}

static void Decoder_ChangeOutputDelay( vlc_input_decoder_t *p_owner, vlc_tick_t delay )
{
    vlc_fifo_Assert(p_owner->p_fifo);

    decoder_t *p_dec = &p_owner->dec;

    msg_Dbg( p_dec, "changing delay: %"PRId64, delay );
    switch( p_dec->fmt_in->i_cat )
    {
        case VIDEO_ES:
            if( p_owner->video.vout != NULL && p_owner->video.started )
                vout_ChangeDelay( p_owner->video.vout, delay );
            break;
        case AUDIO_ES:
            if( p_owner->audio.stream != NULL )
                vlc_aout_stream_ChangeDelay( p_owner->audio.stream, delay );
            break;
        case SPU_ES:
            if( p_owner->spu.vout != NULL )
            {
                assert(p_owner->spu.channel != VOUT_SPU_CHANNEL_INVALID);
                vout_ChangeSpuDelay(p_owner->spu.vout, p_owner->spu.channel,
                                    delay);
            }
            break;
        default:
            vlc_assert_unreachable();
    }
    p_owner->output_delay = delay;
}

static void Decoder_UpdateOutState(vlc_input_decoder_t *owner)
{
    if (owner->paused)
        Decoder_ChangeOutputPause(owner, owner->paused, owner->pause_date);

    if (owner->rate != 1.f)
        Decoder_ChangeOutputRate(owner, owner->rate);

    if (owner->delay != 0)
        Decoder_ChangeOutputDelay(owner, owner->delay);
}

/**
 * Load a decoder module
 */
static int LoadDecoder(decoder_t *p_dec, bool b_packetizer, es_format_t *fmt_in)
{
    p_dec->b_frame_drop_allowed = true;

    decoder_LoadModule( p_dec, b_packetizer, true );
    if( !p_dec->p_module )
    {
        es_format_Clean(fmt_in);
        decoder_Clean( p_dec );
        return -1;
    }
    return 0;
}

static int DecoderThread_Reload( vlc_input_decoder_t *p_owner,
                                 const es_format_t *restrict p_fmt,
                                 enum reload reload )
{
    /* Copy p_fmt since it can be destroyed by decoder_Clean */
    decoder_t *p_dec = &p_owner->dec;
    es_format_t fmt_in;
    if( es_format_Copy( &fmt_in, p_fmt ) != VLC_SUCCESS )
    {
        p_owner->error = true;
        return VLC_EGENERIC;
    }

    /* Restart the decoder module */
    vlc_fifo_Unlock(p_owner->p_fifo);
    decoder_Clean( p_dec );
    vlc_fifo_Lock(p_owner->p_fifo);
    es_format_Clean( &p_owner->dec_fmt_in );
    p_owner->error = false;

    if( reload == RELOAD_DECODER_AOUT )
    {
        assert( p_owner->cat == AUDIO_ES );
        audio_output_t *p_aout = p_owner->audio.aout;
        vlc_aout_stream *p_astream = p_owner->audio.stream;
        // no need to lock, the decoder and ModuleThread are dead
        p_owner->audio.aout = NULL;
        p_owner->audio.stream = NULL;
        if( p_aout )
        {
            assert( p_astream );
            vlc_aout_stream_Delete( p_astream );
            input_resource_PutAout( p_owner->p_resource, p_aout );
        }
    }

    decoder_Init(p_dec, &p_owner->dec_fmt_in, &fmt_in);
    vlc_fifo_Unlock(p_owner->p_fifo);
    if (LoadDecoder(p_dec, false, &p_owner->dec_fmt_in))
    {
        vlc_fifo_Lock(p_owner->p_fifo);
        p_owner->error = true;
        es_format_Clean( &fmt_in );
        return VLC_EGENERIC;
    }
    vlc_fifo_Lock(p_owner->p_fifo);
    es_format_Clean( &fmt_in );
    return VLC_SUCCESS;
}

static void DecoderUpdateFormatLocked( vlc_input_decoder_t *p_owner )
{
    decoder_t *p_dec = &p_owner->dec;

    vlc_fifo_Assert(p_owner->p_fifo);

    es_format_Clean( &p_owner->fmt );
    es_format_Copy( &p_owner->fmt, &p_dec->fmt_out );

    assert( p_owner->fmt.i_cat == p_dec->fmt_in->i_cat );
    assert( p_owner->cat == p_dec->fmt_in->i_cat );

    /* Move p_description */
    if( p_dec->p_description != NULL )
    {
        if( p_owner->p_description != NULL )
            vlc_meta_Delete( p_owner->p_description );
        p_owner->p_description = p_dec->p_description;
        p_dec->p_description = NULL;
    }

    p_owner->b_fmt_description = true;
}

static void MouseEvent( const vlc_mouse_t *newmouse, void *user_data )
{
    decoder_t *dec = user_data;
    vlc_input_decoder_t *owner = dec_get_owner( dec );
    assert( owner->cat == VIDEO_ES );

    vlc_mutex_lock( &owner->video.mouse_lock );
    if( owner->video.mouse_event )
        owner->video.mouse_event( newmouse, owner->video.mouse_opaque);
    vlc_mutex_unlock( &owner->video.mouse_lock );
}

/*****************************************************************************
 * Buffers allocation callbacks for the decoders
 *****************************************************************************/
static int ModuleThread_UpdateAudioFormat( decoder_t *p_dec )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );
    assert( p_owner->cat == AUDIO_ES );

    if( p_owner->audio.aout &&
       ( !AOUT_FMTS_IDENTICAL(&p_dec->fmt_out.audio, &p_owner->fmt.audio) ||
         p_dec->fmt_out.i_codec != p_dec->fmt_out.audio.i_format ||
         p_dec->fmt_out.i_profile != p_owner->fmt.i_profile ) )
    {
        audio_output_t *p_aout = p_owner->audio.aout;
        vlc_aout_stream *p_astream = p_owner->audio.stream;

        /* Parameters changed, restart the aout */
        vlc_fifo_Lock(p_owner->p_fifo);
        p_owner->audio.stream = NULL;
        p_owner->audio.aout = NULL; // the DecoderThread should not use the old aout anymore
        vlc_fifo_Unlock(p_owner->p_fifo);
        vlc_aout_stream_Delete( p_astream );

        input_resource_PutAout( p_owner->p_resource, p_aout );
    }

    /* Check if only replay gain has changed */
    if( replay_gain_Compare( &p_dec->fmt_in->audio_replay_gain,
                             &p_owner->fmt.audio_replay_gain ) )
    {
        p_dec->fmt_out.audio_replay_gain = p_dec->fmt_in->audio_replay_gain;
        if( p_owner->audio.aout )
        {
            p_owner->fmt.audio_replay_gain = p_dec->fmt_in->audio_replay_gain;
            var_TriggerCallback( p_owner->audio.aout, "audio-replay-gain-mode" );
        }
    }

    if( p_owner->audio.aout == NULL )
    {
        p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;

        audio_sample_format_t format = p_dec->fmt_out.audio;
        aout_FormatPrepare( &format );

        const int i_force_dolby = var_InheritInteger( p_dec, "force-dolby-surround" );
        if( i_force_dolby &&
            format.i_physical_channels == (AOUT_CHAN_LEFT|AOUT_CHAN_RIGHT) )
        {
            if( i_force_dolby == 1 )
                format.i_chan_mode |= AOUT_CHANMODE_DOLBYSTEREO;
            else /* i_force_dolby == 2 */
                format.i_chan_mode &= ~AOUT_CHANMODE_DOLBYSTEREO;
        }

        audio_output_t *p_aout;
        vlc_aout_stream *p_astream;

        p_aout = input_resource_GetAout( p_owner->p_resource );
        if( p_aout )
        {
            const struct vlc_aout_stream_cfg cfg = {
                .fmt = &format,
                .profile = p_dec->fmt_out.i_profile,
                .clock = p_owner->p_clock,
                .str_id = p_owner->psz_id,
                .replay_gain = &p_dec->fmt_out.audio_replay_gain,
            };
            p_astream = vlc_aout_stream_New( p_aout, &cfg );
            if( p_astream == NULL )
            {
                input_resource_PutAout( p_owner->p_resource, p_aout );
                p_aout = NULL;
            }
        }
        else
            p_astream = NULL;

        vlc_fifo_Lock(p_owner->p_fifo);
        p_owner->audio.aout = p_aout;
        p_owner->audio.stream = p_astream;

        DecoderUpdateFormatLocked( p_owner );
        aout_FormatPrepare( &p_owner->fmt.audio );

        if( p_aout == NULL )
        {
            vlc_fifo_Unlock(p_owner->p_fifo);
            return -1;
        }

        p_dec->fmt_out.audio.i_bytes_per_frame =
            p_owner->fmt.audio.i_bytes_per_frame;
        p_dec->fmt_out.audio.i_bitspersample =
            p_owner->fmt.audio.i_bitspersample;
        p_dec->fmt_out.audio.i_frame_length =
            p_owner->fmt.audio.i_frame_length;

        Decoder_UpdateOutState( p_owner );
        vlc_fifo_Unlock( p_owner->p_fifo );
    }
    return 0;
}

static int CreateVoutIfNeeded(vlc_input_decoder_t *);


static int ModuleThread_UpdateVideoFormat( decoder_t *p_dec, vlc_video_context *vctx )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );
    assert( p_owner->cat == VIDEO_ES );

    int created_vout = CreateVoutIfNeeded(p_owner);
    if (created_vout == -1)
        return -1; // error
    if (created_vout == 0)
    {
        // video context didn't change
        if (p_owner->video.vctx == vctx)
            return 0;
    }
    assert(p_owner->video.vout);

    if (p_owner->video.vctx)
        vlc_video_context_Release(p_owner->video.vctx);
    p_owner->video.vctx = vctx ? vlc_video_context_Hold(vctx) : NULL;

    // configure the new vout
    vlc_fifo_Lock(p_owner->p_fifo);
    if ( p_owner->video.out_pool == NULL )
    {
        unsigned dpb_size;
        switch( p_dec->fmt_in->i_codec )
        {
        case VLC_CODEC_HEVC:
        case VLC_CODEC_H264:
        case VLC_CODEC_DIRAC: /* FIXME valid ? */
        case VLC_CODEC_VVC:
            dpb_size = 18;
            break;
        case VLC_CODEC_AV1:
            dpb_size = 8; /* NUM_REF_FRAMES from the AV1 spec */
            break;
        case VLC_CODEC_MP4V:
        case VLC_CODEC_VP5:
        case VLC_CODEC_VP6:
        case VLC_CODEC_VP6F:
        case VLC_CODEC_VP8:
            dpb_size = 3;
            break;
        default:
            dpb_size = 2;
            break;
        }
        size_t pic_count = dpb_size + p_dec->i_extra_picture_buffers;
        pic_count ++; /* Held by the vout */
        pic_count ++; /* Held by previous-frame handling or filters */
        picture_pool_t *pool = picture_pool_NewFromFormat( &p_dec->fmt_out.video,
                                                           pic_count );

        if( pool == NULL)
        {
            msg_Err(p_dec, "Failed to create a pool of %d %4.4s pictures",
                           dpb_size + p_dec->i_extra_picture_buffers + 1,
                           (char*)&p_dec->fmt_out.video.i_chroma);
            vlc_fifo_Unlock(p_owner->p_fifo);
            goto error;
        }

        p_owner->video.out_pool = pool;
    }

    vout_configuration_t cfg = {
        .vout = p_owner->video.vout, .clock = p_owner->p_clock,
        .str_id = p_owner->psz_id,
        .fmt = &p_dec->fmt_out.video,
        .mouse_event = MouseEvent, .mouse_opaque = p_dec,
    };
    vlc_fifo_Unlock(p_owner->p_fifo);

    enum input_resource_vout_state vout_state;
    vout_thread_t *p_vout =
        input_resource_RequestVout(p_owner->p_resource, vctx, &cfg, NULL,
                                   &vout_state);
    if (p_vout != NULL)
    {
        assert(vout_state == INPUT_RESOURCE_VOUT_NOTCHANGED ||
               vout_state == INPUT_RESOURCE_VOUT_STARTED);

        vlc_fifo_Lock(p_owner->p_fifo);
        p_owner->video.started = true;

        Decoder_UpdateOutState( p_owner );

        if (vout_state == INPUT_RESOURCE_VOUT_STARTED)
            decoder_Notify(p_owner, on_vout_started, p_vout,
                           p_owner->video.vout_order);
        vlc_fifo_Unlock(p_owner->p_fifo);
        return 0;
    }
    else
    {
        assert(vout_state == INPUT_RESOURCE_VOUT_NOTCHANGED ||
               vout_state == INPUT_RESOURCE_VOUT_STOPPED);

        if (vout_state == INPUT_RESOURCE_VOUT_STOPPED)
            decoder_Notify(p_owner, on_vout_stopped, cfg.vout);
    }

error:
    /* Clean fmt and vctx to trigger a new vout creation on the next update
     * call */
    vlc_fifo_Lock(p_owner->p_fifo);
    es_format_Clean( &p_owner->fmt );
    vlc_fifo_Unlock(p_owner->p_fifo);

    if (p_owner->video.vctx != NULL)
    {
        vlc_video_context_Release(p_owner->video.vctx);
        p_owner->video.vctx = NULL;
    }
    return -1;
}

static int CreateVoutIfNeeded(vlc_input_decoder_t *p_owner)
{
    decoder_t *p_dec = &p_owner->dec;
    assert( p_owner->cat == VIDEO_ES );
    bool need_vout = false;

    vlc_fifo_Lock(p_owner->p_fifo);
    if( p_owner->video.vout == NULL )
    {
        msg_Dbg(p_dec, "vout: none found");
        need_vout = true;
    }
    if( p_dec->fmt_out.video.i_width != p_owner->fmt.video.i_width
             || p_dec->fmt_out.video.i_height != p_owner->fmt.video.i_height )
    {
        msg_Dbg(p_dec, "vout change: decoder size");
        need_vout = true;
    }
    if( p_dec->fmt_out.video.i_visible_width != p_owner->fmt.video.i_visible_width
             || p_dec->fmt_out.video.i_visible_height != p_owner->fmt.video.i_visible_height
             || p_dec->fmt_out.video.i_x_offset != p_owner->fmt.video.i_x_offset
             || p_dec->fmt_out.video.i_y_offset != p_owner->fmt.video.i_y_offset )
    {
        msg_Dbg(p_dec, "vout change: visible size");
        need_vout = true;
    }
    if( p_dec->fmt_out.i_codec != p_owner->fmt.video.i_chroma )
    {
        msg_Dbg(p_dec, "vout change: chroma");
        need_vout = true;
    }
    if( (int64_t)p_dec->fmt_out.video.i_sar_num * p_owner->fmt.video.i_sar_den !=
             (int64_t)p_dec->fmt_out.video.i_sar_den * p_owner->fmt.video.i_sar_num )
    {
        msg_Dbg(p_dec, "vout change: SAR");
        need_vout = true;
    }
    if( p_dec->fmt_out.video.orientation != p_owner->fmt.video.orientation )
    {
        msg_Dbg(p_dec, "vout change: orientation");
        need_vout = true;
    }
    if( p_dec->fmt_out.video.multiview_mode != p_owner->fmt.video.multiview_mode )
    {
        msg_Dbg(p_dec, "vout change: multiview");
        need_vout = true;
    }

    if( !need_vout )
    {
        vlc_fifo_Unlock(p_owner->p_fifo);
        return 0; // vout unchanged
    }

    vout_thread_t *p_vout = p_owner->video.vout;
    p_owner->video.vout = NULL; // the DecoderThread should not use the old vout anymore
    p_owner->video.started = false;
    vlc_fifo_Unlock( p_owner->p_fifo );

    enum vlc_vout_order order;
    const vout_configuration_t cfg = { .vout = p_vout, .fmt = NULL };
    p_vout = input_resource_RequestVout( p_owner->p_resource, NULL, &cfg, &order, NULL );

    vlc_fifo_Lock( p_owner->p_fifo );
    p_owner->video.vout = p_vout;
    p_owner->video.vout_order = order;

    DecoderUpdateFormatLocked( p_owner );
    p_owner->fmt.video.i_chroma = p_dec->fmt_out.i_codec;
    picture_pool_t *pool = p_owner->video.out_pool;
    p_owner->video.out_pool = NULL;
    vlc_fifo_Unlock( p_owner->p_fifo );

     if ( pool != NULL )
         picture_pool_Release( pool );

    if( p_vout == NULL )
    {
        msg_Err( p_dec, "failed to create video output" );
        return -1;
    }

    return 1; // new vout was created
}

static vlc_decoder_device * ModuleThread_GetDecoderDevice( decoder_t *p_dec )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );
    assert( p_owner->cat == VIDEO_ES );

    /* Requesting a decoder device will automatically enable hw decoding */
    if (!p_owner->hw_dec)
        return NULL;

    int created_vout = CreateVoutIfNeeded(p_owner);
    if (created_vout == -1)
        return NULL;  // error

    assert(p_owner->video.vout);
    vlc_decoder_device *dec_device = vout_GetDevice(p_owner->video.vout);
    if (created_vout == 1)
        return dec_device; // new vout was created with a decoder device

    bool need_format_update = false;
    if ( memcmp( &p_dec->fmt_out.video.mastering,
                 &p_owner->fmt.video.mastering,
                 sizeof(p_owner->fmt.video.mastering)) )
    {
        msg_Dbg(p_dec, "vout update: mastering data");
        need_format_update = true;
    }
    if ( p_dec->fmt_out.video.lighting.MaxCLL !=
         p_owner->fmt.video.lighting.MaxCLL ||
         p_dec->fmt_out.video.lighting.MaxFALL !=
         p_owner->fmt.video.lighting.MaxFALL )
    {
        msg_Dbg(p_dec, "vout update: lighting data");
        need_format_update = true;
    }

    if ( need_format_update )
    {
        /* the format has changed but we don't need a new vout */
        vlc_fifo_Lock(p_owner->p_fifo);
        DecoderUpdateFormatLocked( p_owner );
        vlc_fifo_Unlock(p_owner->p_fifo);
    }
    return dec_device;
}

static picture_t *ModuleThread_NewVideoBuffer( decoder_t *p_dec )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );
    assert( p_owner->cat == VIDEO_ES );
    assert( p_owner->video.vout );
    assert( p_owner->video.out_pool );

    picture_t *pic = picture_pool_Wait( p_owner->video.out_pool );

    if (pic)
        picture_Reset( pic );
    return pic;
}

static subpicture_t *ModuleThread_NewSpuBuffer( decoder_t *p_dec,
                                     const subpicture_updater_t *p_updater )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );
    assert( p_owner->cat == SPU_ES );
    vout_thread_t *p_vout = NULL;
    subpicture_t *p_subpic;
    int i_attempts = 30;

    while( i_attempts-- )
    {
        if( p_owner->error )
            break;

        p_vout = input_resource_HoldVout( p_owner->p_resource );
        if( p_vout )
            break;

        vlc_tick_sleep( DECODER_SPU_VOUT_WAIT_DURATION );
    }

    if( !p_vout )
    {
        msg_Warn( p_dec, "no vout found, dropping subpicture" );
        if( p_owner->spu.vout )
        {
            vlc_fifo_Lock( p_owner->p_fifo );
            assert(p_owner->spu.channel != VOUT_SPU_CHANNEL_INVALID);
            decoder_Notify(p_owner, on_vout_stopped, p_owner->spu.vout);

            vout_UnregisterSubpictureChannel(p_owner->spu.vout,
                                             p_owner->spu.channel);
            p_owner->spu.channel = VOUT_SPU_CHANNEL_INVALID;

            vout_Release(p_owner->spu.vout);
            p_owner->spu.vout = NULL; // the DecoderThread should not use the old vout anymore
            vlc_fifo_Unlock( p_owner->p_fifo );
        }
        return NULL;
    }

    if( p_owner->spu.vout != p_vout )
    {
        vlc_fifo_Lock(p_owner->p_fifo);

        if (p_owner->spu.vout) /* notify the previous vout deletion unlocked */
            decoder_Notify(p_owner, on_vout_stopped, p_owner->spu.vout);

        if (p_owner->spu.vout)
        {
            /* Unregister the SPU channel of the previous vout */
            assert(p_owner->spu.channel != VOUT_SPU_CHANNEL_INVALID);
            vout_UnregisterSubpictureChannel(p_owner->spu.vout,
                                             p_owner->spu.channel);
            vout_Release(p_owner->spu.vout);
            p_owner->spu.vout = NULL; // the DecoderThread should not use the old vout anymore
        }

        enum vlc_vout_order channel_order;
        p_owner->spu.channel =
            vout_RegisterSubpictureChannelInternal(p_vout, p_owner->p_clock,
                                                   &channel_order);
        p_owner->spu.order = 0;

        if (p_owner->spu.channel == VOUT_SPU_CHANNEL_INVALID)
        {
            /* The new vout doesn't support SPU, aborting... */
            vlc_fifo_Unlock(p_owner->p_fifo);
            vout_Release(p_vout);
            return NULL;
        }

        p_owner->spu.vout = p_vout;
        Decoder_UpdateOutState(p_owner);

        assert(channel_order != VLC_VOUT_ORDER_NONE);
        decoder_Notify(p_owner, on_vout_started, p_vout, channel_order);
        vlc_fifo_Unlock(p_owner->p_fifo);
    }
    else
        vout_Release(p_vout);

    p_subpic = subpicture_New( p_updater );
    if( p_subpic )
    {
        p_subpic->i_channel = p_owner->spu.channel;
        p_subpic->i_order = p_owner->spu.order++;
    }

    return p_subpic;
}

static int InputThread_GetInputAttachments( decoder_t *p_dec,
                                       input_attachment_t ***ppp_attachment,
                                       int *pi_attachment )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );
    if (!p_owner->cbs || !p_owner->cbs->get_attachments)
        return VLC_EINVAL;

    int ret = p_owner->cbs->get_attachments(p_owner, ppp_attachment,
                                            p_owner->cbs_userdata);
    if (ret < 0)
        return VLC_EGENERIC;
    *pi_attachment = ret;
    return VLC_SUCCESS;
}

static vlc_tick_t ModuleThread_GetDisplayDate( decoder_t *p_dec,
                                       vlc_tick_t system_now, vlc_tick_t i_ts )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );

    vlc_fifo_Lock(p_owner->p_fifo);
    if( p_owner->b_waiting || p_owner->paused )
        i_ts = VLC_TICK_INVALID;
    float rate = p_owner->output_rate;
    vlc_fifo_Unlock(p_owner->p_fifo);

    if( !p_owner->p_clock || i_ts == VLC_TICK_INVALID )
        return i_ts;

    vlc_clock_Lock( p_owner->p_clock );
    vlc_tick_t conv_ts =
        vlc_clock_ConvertToSystem( p_owner->p_clock, system_now, i_ts, rate, NULL );
    vlc_clock_Unlock( p_owner->p_clock );
    return conv_ts;
}

static float ModuleThread_GetDisplayRate( decoder_t *p_dec )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );

    if( !p_owner->p_clock )
        return 1.f;
    vlc_fifo_Lock(p_owner->p_fifo);
    float rate = p_owner->output_rate;
    vlc_fifo_Unlock(p_owner->p_fifo);
    return rate;
}

/*****************************************************************************
 * Public functions
 *****************************************************************************/
vlc_frame_t *decoder_NewAudioBuffer( decoder_t *dec, int samples )
{
    assert( dec->fmt_out.audio.i_frame_length > 0
         && dec->fmt_out.audio.i_bytes_per_frame  > 0 );

    size_t length = samples * dec->fmt_out.audio.i_bytes_per_frame
                            / dec->fmt_out.audio.i_frame_length;
    vlc_frame_t *frame = block_Alloc( length );
    if( likely(frame != NULL) )
    {
        frame->i_nb_samples = samples;
        frame->i_pts = frame->i_length = 0;
    }
    return frame;
}

static void RequestReload( vlc_input_decoder_t *p_owner )
{
    /* Don't override reload if it's RELOAD_DECODER_AOUT */
    int expected = RELOAD_NO_REQUEST;
    atomic_compare_exchange_strong( &p_owner->reload, &expected, RELOAD_DECODER );
}

static int DecoderWaitUnblock(vlc_input_decoder_t *p_owner, vlc_tick_t date)
{
    struct vlc_tracer *tracer = vlc_object_get_tracer(VLC_OBJECT(&p_owner->dec));
    vlc_fifo_Assert(p_owner->p_fifo);

    if( p_owner->b_waiting )
    {
        if (tracer != NULL)
            vlc_tracer_TraceEvent(tracer, "DEC", p_owner->psz_id, "start wait");
        p_owner->b_has_data = true;
        vlc_cond_signal( &p_owner->wait_acknowledge );
    }

    while (p_owner->b_waiting && p_owner->b_has_data && !p_owner->flushing)
        vlc_fifo_WaitCond(p_owner->p_fifo, &p_owner->wait_request);

    if (!p_owner->out_started)
    {
        p_owner->out_started = true;
        if (tracer != NULL)
            vlc_tracer_TraceEvent(tracer, "DEC", p_owner->psz_id, "stop wait");

        vlc_clock_Lock(p_owner->p_clock);
        vlc_clock_Start(p_owner->p_clock, vlc_tick_now(), date);
        vlc_clock_Unlock(p_owner->p_clock);
    }

    if (p_owner->flushing)
    {
        p_owner->b_has_data = false;
        vlc_cond_signal(&p_owner->wait_acknowledge);
        return VLC_ENOENT;
    }

    return VLC_SUCCESS;
}

static inline void DecoderUpdatePreroll( vlc_tick_t *pi_preroll, const vlc_frame_t *p )
{
    if( p->i_flags & BLOCK_FLAG_PREROLL )
        *pi_preroll = PREROLL_FORCED;
    /* Check if we can use the packet for end of preroll */
    else if( (p->i_flags & BLOCK_FLAG_DISCONTINUITY) &&
             (p->i_buffer == 0 || (p->i_flags & BLOCK_FLAG_CORRUPTED)) )
        *pi_preroll = PREROLL_FORCED;
    else if( p->i_dts != VLC_TICK_INVALID )
        *pi_preroll = __MIN( *pi_preroll, p->i_dts );
    else if( p->i_pts != VLC_TICK_INVALID )
        *pi_preroll = __MIN( *pi_preroll, p->i_pts );
}

static void DecoderSendSubstream(vlc_input_decoder_t *p_owner)
{
    decoder_t *p_dec = &p_owner->dec;
    if (p_dec->pf_get_cc == NULL)
        return;

    bool b_wants_substreams;
    int ret = sout_StreamControl(p_owner->p_sout,
                                 SOUT_STREAM_WANTS_SUBSTREAMS,
                                 &b_wants_substreams);

    if (ret != VLC_SUCCESS || !b_wants_substreams)
        return;

    if (p_owner->cc.p_sout_input == NULL && p_owner->cc.b_sout_created)
        return;

    decoder_cc_desc_t desc;
    vlc_frame_t *p_cc = p_dec->pf_get_cc( p_dec, &desc );
    if (p_cc == NULL)
        return;

    if (!p_owner->cc.b_sout_created)
    {
        es_format_t ccfmt;
        es_format_Init(&ccfmt, SPU_ES, VLC_CODEC_CEA608);
        ccfmt.i_group = p_owner->fmt.i_group;
        ccfmt.subs.cc.i_reorder_depth = desc.i_reorder_depth;

        /* Create only one es ID since we simply send the CC whole side data
         * un-decoded to the stream output.
         * Specifying the owner's ID is important for the uniqueness for the ID.
         */
        if (asprintf(&p_owner->cc.sout_es_id, "%s/cc", p_owner->psz_id) == -1)
        {
            es_format_Clean(&ccfmt);
            block_Release(p_cc);
            return;
        }

        p_owner->cc.p_sout_input =
            sout_InputNew(p_owner->p_sout, &ccfmt, p_owner->cc.sout_es_id);

        es_format_Clean(&ccfmt);
        p_owner->cc.b_sout_created = true;
    }

    if (!p_owner->cc.p_sout_input ||
        sout_InputSendBuffer(p_owner->p_sout, p_owner->cc.p_sout_input, p_cc))
    {
        block_Release(p_cc);
    }
}

/* This function process a frame for sout
 */
static void DecoderThread_ProcessSout( vlc_input_decoder_t *p_owner, vlc_frame_t *frame )
{
    decoder_t *p_dec = &p_owner->dec;
    vlc_frame_t *sout_frame;
    vlc_frame_t **ppframe = frame ? &frame : NULL;

    while( ( sout_frame =
                 p_dec->pf_packetize( p_dec, ppframe ) ) )
    {
        if( p_owner->p_sout_input == NULL )
        {
            vlc_fifo_Lock(p_owner->p_fifo);
            DecoderUpdateFormatLocked( p_owner );

            p_owner->fmt.i_group = p_dec->fmt_in->i_group;
            p_owner->fmt.i_id = p_dec->fmt_in->i_id;
            if( p_dec->fmt_in->psz_language )
            {
                free( p_owner->fmt.psz_language );
                p_owner->fmt.psz_language =
                    strdup( p_dec->fmt_in->psz_language );
            }
            vlc_fifo_Unlock(p_owner->p_fifo);

            p_owner->p_sout_input =
                sout_InputNew( p_owner->p_sout, &p_owner->fmt, p_owner->psz_id );

            if( p_owner->p_sout_input == NULL )
            {
                msg_Err( p_dec, "cannot create packetized sout output (%4.4s)",
                         (char *)&p_owner->fmt.i_codec );
                p_owner->error = true;

                if(frame)
                    block_Release(frame);

                block_ChainRelease(sout_frame);
                break;
            }
        }

        while( sout_frame )
        {
            vlc_frame_t *p_next = sout_frame->p_next;

            sout_frame->p_next = NULL;

            DecoderSendSubstream( p_owner );

            /* FIXME --VLC_TICK_INVALID inspect stream_output*/
            if ( sout_InputSendBuffer( p_owner->p_sout, p_owner->p_sout_input, sout_frame ) ==
                 VLC_EGENERIC )
            {
                msg_Err( p_dec, "cannot continue streaming due to errors with codec %4.4s",
                                (char *)&p_owner->fmt.i_codec );

                p_owner->error = true;

                /* Cleanup */

                if( frame )
                    block_Release( frame );

                block_ChainRelease( p_next );
                return;
            }

            sout_frame = p_next;
        }
    }
}

static void GetCcChannels(vlc_input_decoder_t *owner, size_t *max_channels,
                          uint64_t *bitmap)
{
    switch (owner->cc.selected_codec)
    {
        case VLC_CODEC_CEA608:
            if (max_channels != NULL)
                *max_channels = 4;
            if (bitmap != NULL)
                *bitmap = owner->cc.desc.i_608_channels;
            break;
        case VLC_CODEC_CEA708:
            if (max_channels != NULL)
                *max_channels = 64;
            if (bitmap != NULL)
                *bitmap = owner->cc.desc.i_708_channels;
            break;
        default: vlc_assert_unreachable();
    }
}

static bool SubDecoderIsCc(vlc_input_decoder_t *subdec)
{
    return subdec->cat == SPU_ES &&
            (subdec->dec.fmt_in->i_codec == VLC_CODEC_CEA608 ||
             subdec->dec.fmt_in->i_codec == VLC_CODEC_CEA708);
}

/* */
static void DecoderPlayCcLocked( vlc_input_decoder_t *p_owner, vlc_frame_t *p_cc,
                                 const decoder_cc_desc_t *p_desc )
{
    vlc_fifo_Assert(p_owner->p_fifo);
    if (p_owner->flushing || p_owner->aborting)
    {
        vlc_frame_Release(p_cc);
        return;
    }

    vlc_mutex_lock(&p_owner->subdecs.lock);
    if (p_owner->cc.desc.i_608_channels != p_desc->i_608_channels ||
        p_owner->cc.desc.i_708_channels != p_desc->i_708_channels ||
        p_owner->cc.desc.i_reorder_depth != p_desc->i_reorder_depth)
    {
        p_owner->cc.desc = *p_desc;
        p_owner->cc.desc_changed = true;
    }

    if (p_owner->cc.count == 0)
        goto end;

    size_t cc_idx = 0;
    vlc_input_decoder_t *it;

    vlc_list_foreach(it, &p_owner->subdecs.list, node)
    {
        if (!SubDecoderIsCc(it))
            continue;

        if (++cc_idx == p_owner->cc.count)
        {
            block_FifoPut(it->p_fifo, p_cc);
            p_cc = NULL;
        }
        else
        {
            block_t *dup = block_Duplicate(p_cc);
            if (dup == NULL)
                break;
            block_FifoPut(it->p_fifo, dup);
        }
    }

end:
    vlc_mutex_unlock(&p_owner->subdecs.lock);

    if (p_cc != NULL) /* can have bitmap set but no created decs */
        block_Release( p_cc );
}

static void PacketizerGetCc( vlc_input_decoder_t *p_owner, decoder_t *p_dec_cc )
{
    vlc_frame_t *p_cc;
    decoder_cc_desc_t desc;

    /* Do not try retrieving CC if not wanted (sout) or cannot be retrieved */
    if( !p_owner->cc.b_supported )
        return;

    assert( p_dec_cc->pf_get_cc != NULL );

    p_cc = p_dec_cc->pf_get_cc( p_dec_cc, &desc );
    if( !p_cc )
        return;

    DecoderPlayCcLocked( p_owner, p_cc, &desc );
}

static void ModuleThread_QueueCc( decoder_t *p_videodec, vlc_frame_t *p_cc,
                                  const decoder_cc_desc_t *p_desc )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_videodec );

    if (unlikely(p_cc == NULL))
        return;

    if (!p_owner->cc.b_supported ||
       (p_owner->p_packetizer != NULL && p_owner->p_packetizer->pf_get_cc != NULL))
    {
        block_Release(p_cc);
        return;
    }

    vlc_fifo_Lock(p_owner->p_fifo);
    DecoderPlayCcLocked(p_owner, p_cc, p_desc);
    vlc_fifo_Unlock(p_owner->p_fifo);
}

static int ModuleThread_PlayVideo( vlc_input_decoder_t *p_owner, picture_t *p_picture )
{
    decoder_t *p_dec = &p_owner->dec;
    assert( p_owner->cat == VIDEO_ES );

    if( p_picture->date == VLC_TICK_INVALID )
        /* FIXME: VLC_TICK_INVALID -- verify video_output */
    {
        msg_Warn( p_dec, "non-dated video buffer received" );
        picture_Release( p_picture );
        return VLC_EGENERIC;
    }

    if (p_owner->flushing || p_owner->aborting)
    {
        picture_Release(p_picture);
        return VLC_SUCCESS;
    }

    vout_thread_t  *p_vout = p_owner->video.vout;

    assert( p_owner->video.started );

    bool prerolled = p_owner->i_preroll_end != PREROLL_NONE;
    if( prerolled && p_owner->i_preroll_end > p_picture->date )
    {
        picture_Release( p_picture );
        return VLC_SUCCESS;
    }

    p_owner->i_preroll_end = PREROLL_NONE;

    if( unlikely(prerolled) )
    {
        msg_Dbg( p_dec, "end of video preroll" );

        if( p_vout )
            vout_FlushAll( p_vout );
    }

    /* previous-frame handling */
    if (unlikely(p_owner->paused) && p_vout != NULL &&
        decoder_prevframe_IsActive(&p_owner->video.pf))
    {
        p_picture = Decoder_HandlePreviousFrame(p_owner, p_picture);
        if (p_picture == NULL)
            return VLC_ENOENT;
    }

    if( p_owner->b_first && p_owner->b_waiting )
    {
        msg_Dbg( p_dec, "Received first picture" );
        p_owner->b_first = false;
        p_picture->b_force = true;
    }
    else
    {
        int ret = DecoderWaitUnblock(p_owner, p_picture->date);
        if (ret != VLC_SUCCESS)
        {
            picture_Release(p_picture);
            return ret;
        }
    }

    /* */
    if( p_vout == NULL )
    {
        picture_Release( p_picture );
        return VLC_EGENERIC;
    }

    if (unlikely(p_owner->output_paused && p_owner->frames_countdown > 0))
    {
        p_owner->frames_countdown--;
        vout_PutPicture(p_vout, p_picture);
        decoder_Notify(p_owner, frame_next_status, 0);
        return VLC_SUCCESS;
    }

    if( p_picture->b_still )
    {
        /* Ensure no earlier higher pts breaks still state */
        vout_Flush( p_vout, p_picture->date );
    }
    vout_PutPicture( p_vout, p_picture );

    return VLC_SUCCESS;
}

static void ModuleThread_QueueVideo( decoder_t *p_dec, picture_t *p_pic )
{
    assert( p_pic );
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );
    assert( p_owner->cat == VIDEO_ES );
    struct vlc_tracer *tracer = vlc_object_get_tracer( &p_dec->obj );

    if ( tracer != NULL )
    {
        vlc_tracer_TraceStreamPTS( tracer, "DEC", p_owner->psz_id,
                            "OUT", p_pic->date );
    }

    vlc_fifo_Lock( p_owner->p_fifo );

    int success = ModuleThread_PlayVideo( p_owner, p_pic );

    unsigned displayed = 0;
    unsigned vout_lost = 0;
    unsigned vout_late = 0;
    if( p_owner->video.vout != NULL )
    {
        vout_GetResetStatistic( p_owner->video.vout, &displayed, &vout_lost,
                                &vout_late );
    }
    if (success != VLC_SUCCESS)
        vout_lost++;

    decoder_Notify(p_owner, on_new_video_stats, 1, vout_lost, displayed, vout_late);
    vlc_fifo_Unlock(p_owner->p_fifo);
}

static vlc_decoder_device * thumbnailer_get_device( decoder_t *p_dec )
{
    VLC_UNUSED(p_dec);
    // no hardware decoder on purpose
    // we don't want to load many DLLs and allocate many pictures
    // just to decode one picture
    return NULL;
}

static picture_t *thumbnailer_buffer_new( decoder_t *p_dec )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );
    /* Avoid decoding more than one frame when a thumbnail was
     * already generated */
    vlc_fifo_Lock(p_owner->p_fifo);
    if( !p_owner->b_first )
    {
        vlc_fifo_Unlock(p_owner->p_fifo);
        return NULL;
    }
    vlc_fifo_Unlock(p_owner->p_fifo);
    return picture_NewFromFormat( &p_dec->fmt_out.video );
}

static void ModuleThread_QueueThumbnail( decoder_t *p_dec, picture_t *p_pic )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );

    vlc_fifo_Lock(p_owner->p_fifo);
    if( p_owner->b_first )
        decoder_Notify(p_owner, on_thumbnail_ready, p_pic);
    p_owner->b_first = false;
    vlc_fifo_Unlock(p_owner->p_fifo);

    picture_Release( p_pic );
}

static int ModuleThread_PlayAudio( vlc_input_decoder_t *p_owner, vlc_frame_t *p_audio )
{
    decoder_t *p_dec = &p_owner->dec;
    assert( p_owner->cat == AUDIO_ES );

    assert( p_audio != NULL );

    if( p_audio->i_pts == VLC_TICK_INVALID ) // FIXME --VLC_TICK_INVALID verify audio_output/*
    {
        msg_Warn( p_dec, "non-dated audio buffer received" );
        block_Release( p_audio );
        return VLC_EGENERIC;
    }

    vlc_aout_stream *p_astream = p_owner->audio.stream;
    if( p_astream == NULL )
    {
        msg_Dbg( p_dec, "discarded audio buffer" );
        block_Release( p_audio );
        return VLC_EGENERIC;
    }

    if (p_owner->flushing || p_owner->aborting)
    {
        block_Release(p_audio);
        return VLC_SUCCESS;
    }

    bool prerolled = p_owner->i_preroll_end != PREROLL_NONE;
    if( prerolled && p_owner->i_preroll_end > p_audio->i_pts )
    {
        block_Release( p_audio );
        return VLC_SUCCESS;
    }

    p_owner->i_preroll_end = PREROLL_NONE;

    if( unlikely(prerolled) )
    {
        msg_Dbg( p_dec, "end of audio preroll" );

        vlc_aout_stream_Flush( p_astream );
    }

    int ret = DecoderWaitUnblock(p_owner, p_audio->i_pts);
    if (ret != VLC_SUCCESS)
    {
        block_Release(p_audio);
        return ret;
    }

    int status = vlc_aout_stream_Play( p_astream, p_audio );
    if( status == AOUT_DEC_CHANGED )
    {
        /* Only reload the decoder */
        RequestReload( p_owner );
    }
    else if( status == AOUT_DEC_FAILED )
    {
        /* If we reload because the aout failed, we should release it. That
            * way, a next call to ModuleThread_UpdateAudioFormat() won't re-use the
            * previous (failing) aout but will try to create a new one. */
        atomic_store( &p_owner->reload, RELOAD_DECODER_AOUT );
    }

    return VLC_SUCCESS;
}

static void ModuleThread_QueueAudio( decoder_t *p_dec, vlc_frame_t *p_aout_buf )
{
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );
    assert( p_owner->cat == AUDIO_ES );
    struct vlc_tracer *tracer = vlc_object_get_tracer( &p_dec->obj );

    if ( tracer != NULL && p_aout_buf != NULL )
    {
        vlc_tracer_TraceStreamDTS( tracer, "DEC", p_owner->psz_id, "OUT",
                            p_aout_buf->i_pts, p_aout_buf->i_dts );
    }

    vlc_fifo_Lock(p_owner->p_fifo);

    int success = ModuleThread_PlayAudio( p_owner, p_aout_buf );

    unsigned played = 0;
    unsigned aout_lost = 0;
    if( p_owner->audio.stream != NULL )
    {
        vlc_aout_stream_GetResetStats( p_owner->audio.stream, &aout_lost, &played );
    }
    if (success != VLC_SUCCESS)
        aout_lost++;

    decoder_Notify(p_owner, on_new_audio_stats, 1, aout_lost, played);
    vlc_fifo_Unlock(p_owner->p_fifo);
}

static void ModuleThread_PlaySpu( vlc_input_decoder_t *p_owner, subpicture_t *p_subpic )
{
    decoder_t *p_dec = &p_owner->dec;
    assert( p_owner->cat == SPU_ES );
    vout_thread_t *p_vout = p_owner->spu.vout;

    /* */
    if( p_subpic->i_start == VLC_TICK_INVALID )
    {
        msg_Warn( p_dec, "non-dated spu buffer received" );
        subpicture_Delete( p_subpic );
        return;
    }

    /* */
    int ret = DecoderWaitUnblock(p_owner, p_subpic->i_start);

    if (ret != VLC_SUCCESS || p_subpic->i_start == VLC_TICK_INVALID)
    {
        subpicture_Delete( p_subpic );
        return;
    }

    vout_PutSubpicture( p_vout, p_subpic );
}

static void ModuleThread_QueueSpu( decoder_t *p_dec, subpicture_t *p_spu )
{
    assert( p_spu );
    vlc_input_decoder_t *p_owner = dec_get_owner( p_dec );
    assert( p_owner->cat == SPU_ES );
    struct vlc_tracer *tracer = vlc_object_get_tracer( &p_dec->obj );

    if ( tracer != NULL && p_spu != NULL )
    {
        vlc_tracer_TraceStreamPTS( tracer, "DEC", p_owner->psz_id,
                            "OUT", p_spu->i_start );
    }

    /* The vout must be created from a previous decoder_NewSubpicture call. */
    assert( p_owner->spu.vout );

    /* Preroll does not work very well with subtitle */
    vlc_fifo_Lock(p_owner->p_fifo);
    if( p_spu->i_start != VLC_TICK_INVALID &&
        p_spu->i_start < p_owner->i_preroll_end &&
        ( p_spu->i_stop == VLC_TICK_INVALID || p_spu->i_stop < p_owner->i_preroll_end ) )
        subpicture_Delete( p_spu );
    else
        ModuleThread_PlaySpu( p_owner, p_spu );
    vlc_fifo_Unlock(p_owner->p_fifo);
}

static void DecoderThread_ProcessInput( vlc_input_decoder_t *p_owner, vlc_frame_t *frame );
static void DecoderThread_DecodeBlock( vlc_input_decoder_t *p_owner, vlc_frame_t *frame )
{
    decoder_t *p_dec = &p_owner->dec;
    struct vlc_tracer *tracer = vlc_object_get_tracer( &p_dec->obj );

    vlc_fifo_Unlock(p_owner->p_fifo);

    if ( tracer != NULL && frame != NULL )
    {
        vlc_tracer_TraceStreamDTS( tracer, "DEC", p_owner->psz_id, "IN",
                            frame->i_pts, frame->i_dts );
    }

    int ret = p_dec->pf_decode( p_dec, frame );

    vlc_fifo_Lock(p_owner->p_fifo);
    switch( ret )
    {
        case VLCDEC_SUCCESS:
            break;
        case VLCDEC_ECRITICAL:
            p_owner->error = true;
            break;
        case VLCDEC_RELOAD:
            RequestReload( p_owner );
            if( unlikely( frame == NULL ) )
                break;
            if( !( frame->i_flags & BLOCK_FLAG_CORE_PRIVATE_RELOADED ) )
            {
                frame->i_flags |= BLOCK_FLAG_CORE_PRIVATE_RELOADED;
                DecoderThread_ProcessInput( p_owner, frame );
            }
            else /* We prefer loosing this frame than an infinite recursion */
                block_Release( frame );
            break;
        default:
            vlc_assert_unreachable();
    }
}

/**
 * Decode a frame
 *
 * \param p_owner the input decoder object
 * \param frame the block to decode
 */
static void DecoderThread_ProcessInput( vlc_input_decoder_t *p_owner, vlc_frame_t *frame )
{
    decoder_t *p_dec = &p_owner->dec;

    if( p_owner->error )
        goto error;

    /* Here, the atomic doesn't prevent to miss a reload request.
     * DecoderThread_ProcessInput() can still be called after the decoder module or the
     * audio output requested a reload. This will only result in a drop of an
     * input frame or an output buffer. */
    enum reload reload;
    if( ( reload = atomic_exchange( &p_owner->reload, RELOAD_NO_REQUEST ) ) )
    {
        msg_Warn( p_dec, "Reloading the decoder module%s",
                  reload == RELOAD_DECODER_AOUT ? " and the audio output" : "" );

        if( DecoderThread_Reload( p_owner, p_dec->fmt_in, reload ) != VLC_SUCCESS )
            goto error;
    }

    bool packetize = p_owner->p_packetizer != NULL;
    if( frame )
    {
        if( frame->i_buffer <= 0 )
            goto error;

        DecoderUpdatePreroll( &p_owner->i_preroll_end, frame );
        if( unlikely( frame->i_flags & BLOCK_FLAG_CORE_PRIVATE_RELOADED ) )
        {
            /* This frame has already been packetized */
            packetize = false;
        }
    }

    if( p_owner->p_sout != NULL )
    {
        vlc_fifo_Unlock(p_owner->p_fifo);
        DecoderThread_ProcessSout( p_owner, frame );
        vlc_fifo_Lock(p_owner->p_fifo);
        return;
    }
    if( packetize )
    {
        vlc_frame_t *packetized_frame;
        vlc_frame_t **ppframe = frame ? &frame : NULL;
        decoder_t *p_packetizer = p_owner->p_packetizer;

        while( (packetized_frame =
                p_packetizer->pf_packetize( p_packetizer, ppframe ) ) )
        {
            if( !es_format_IsSimilar( p_dec->fmt_in, &p_packetizer->fmt_out ) )
            {
                msg_Dbg( p_dec, "restarting module due to input format change");
                es_format_LogDifferences( vlc_object_logger(p_dec),
                                          "decoder in", p_dec->fmt_in,
                                          "packetizer out", &p_packetizer->fmt_out );

                /* Drain the decoder module */
                DecoderThread_DecodeBlock( p_owner, NULL );

                if( DecoderThread_Reload( p_owner, &p_packetizer->fmt_out,
                                          RELOAD_DECODER ) != VLC_SUCCESS )
                {
                    block_ChainRelease( packetized_frame );
                    return;
                }
            }

            if( p_packetizer->pf_get_cc )
                PacketizerGetCc( p_owner, p_packetizer );

            while( packetized_frame )
            {
                vlc_frame_t *p_next = packetized_frame->p_next;
                packetized_frame->p_next = NULL;

                DecoderThread_DecodeBlock( p_owner, packetized_frame );

                if( p_owner->error )
                {
                    block_ChainRelease( p_next );
                    return;
                }

                packetized_frame = p_next;
            }
        }
        /* Drain the decoder after the packetizer is drained */
        if( !ppframe )
            DecoderThread_DecodeBlock( p_owner, NULL );
    }
    else
        DecoderThread_DecodeBlock( p_owner, frame );
    return;

error:
    if( frame )
        block_Release( frame );
}

static void DecoderThread_Flush( vlc_input_decoder_t *p_owner )
{
    decoder_t *p_dec = &p_owner->dec;
    decoder_t *p_packetizer = p_owner->p_packetizer;

    if( p_packetizer != NULL && p_packetizer->pf_flush != NULL )
        p_packetizer->pf_flush( p_packetizer );

    if ( p_dec->pf_flush != NULL )
        p_dec->pf_flush( p_dec );

    p_owner->error = false;
}

static void Decoder_VideoDrained(vlc_input_decoder_t *owner)
{
    owner->video.drained = true;
    if (owner->frames_countdown > 0)
    {
        if (unlikely(vout_IsEmpty(owner->video.vout)))
        {
            /* Unlikely case where all pictures are already sent to the vout
             * next queue while draining. This could happen with a burst of
             * next-frame request near EOF. */
            owner->frames_countdown = 0;
            decoder_Notify(owner, frame_next_status, -EAGAIN);
        }
    }
    else if(owner->frames_countdown == -1)
    {
        /* Also check if we need to increase seek steps near EOF */
        int seek_steps;
        picture_t *nullpic =
            decoder_prevframe_AddPic(&owner->video.pf, NULL,
                                     &owner->video.pf_pts, &seek_steps);
        assert(nullpic == NULL); (void) nullpic;
        if (seek_steps != DEC_PF_SEEK_STEPS_NONE)
            Decoder_SeekPreviousFrame(owner, seek_steps, true);
    }
}

/**
 * The decoding main loop
 *
 * \param p_data the input decoder object
 */
static void *DecoderThread( void *p_data )
{
    vlc_input_decoder_t *p_owner = (vlc_input_decoder_t *)p_data;

    const char *thread_name;
    switch (p_owner->cat)
    {
        case VIDEO_ES: thread_name = "vlc-dec-video"; break;
        case AUDIO_ES: thread_name = "vlc-dec-audio"; break;
        case SPU_ES:   thread_name = "vlc-dec-spu"; break;
        case DATA_ES:  thread_name = "vlc-dec-data"; break;
        default:       thread_name = "vlc-decoder"; break;
    }

    vlc_thread_set_name(thread_name);

    /* The decoder's main loop */
    vlc_fifo_Lock( p_owner->p_fifo );

    while( !p_owner->aborting || p_owner->flushing )
    {
        if( p_owner->flushing )
        {   /* Flush before/regardless of pause. We do not want to resume just
             * for the sake of flushing (glitches could otherwise happen). */
            vlc_fifo_Unlock( p_owner->p_fifo );

            /* Flush the decoder (and the output) */
            DecoderThread_Flush( p_owner );

            vlc_fifo_Lock( p_owner->p_fifo );

            /* Reset flushing after DecoderThread_ProcessInput in case vlc_input_decoder_Flush
             * is called again. This will avoid a second useless flush (but
             * harmless). */
            p_owner->flushing = false;
            p_owner->out_started = false;
            p_owner->i_preroll_end = PREROLL_NONE;
            continue;
        }

        if( p_owner->paused != p_owner->output_paused )
        {   /* Update playing/paused status of the output */
            Decoder_ChangeOutputPause( p_owner, p_owner->paused, p_owner->pause_date );
            decoder_Notify(p_owner, on_output_paused, p_owner->paused,
                           p_owner->pause_date);
            if (unlikely(p_owner->paused && p_owner->cat == VIDEO_ES
                      && p_owner->frames_countdown != 0))
                Decoder_PausedForNextFrame(p_owner);
            continue;
        }

        if( p_owner->rate != p_owner->output_rate )
        {
            Decoder_ChangeOutputRate( p_owner, p_owner->rate );
            continue;
        }

        if( p_owner->delay != p_owner->output_delay )
        {
            Decoder_ChangeOutputDelay( p_owner, p_owner->delay );
            continue;
        }

        if( p_owner->paused && p_owner->frames_countdown == 0 )
        {   /* Wait for resumption from pause */
            p_owner->b_idle = true;
            vlc_cond_signal( &p_owner->wait_acknowledge );
            vlc_fifo_Wait( p_owner->p_fifo );
            p_owner->b_idle = false;
            continue;
        }

        vlc_cond_signal( &p_owner->wait_fifo );

        vlc_frame_t *frame = vlc_fifo_DequeueUnlocked( p_owner->p_fifo );
        if( frame == NULL )
        {
            if( likely(!p_owner->b_draining) )
            {   /* Wait for a block to decode (or a request to drain) */
                p_owner->b_idle = true;
                vlc_cond_signal( &p_owner->wait_acknowledge );

                if (p_owner->frames_countdown > 0)
                {
                    /* next-frames are requested but the FIFO is empty, ask for
                     * more buffering */
                    decoder_Notify( p_owner, frame_next_need_data, true );
                }
                vlc_fifo_Wait( p_owner->p_fifo );
                p_owner->b_idle = false;
                continue;
            }
            /* We have emptied the FIFO and there is a pending request to
             * drain. Pass frame = NULL to decoder just once. */
        }

        /* DecoderThread_ProcessInput will unlock when playing to the decoders
         * but will ensure it re-locks in the end. This is necessary to handle
         * reloading, CC and packetizing. */
        DecoderThread_ProcessInput( p_owner, frame );

        if( p_owner->b_draining && frame == NULL )
        {
            p_owner->b_draining = false;

            switch (p_owner->cat)
            {
                case AUDIO_ES:
                    if( p_owner->audio.stream != NULL )
                    {
                        /* Draining: the decoder is drained and all decoded
                         * buffers are queued to the output at this point.
                         * Now drain the output. */
                        vlc_aout_stream_Drain( p_owner->audio.stream );
                    }
                    break;
                case VIDEO_ES:
                    Decoder_VideoDrained(p_owner);
                    break;
                default:
                    break;
            }
        }

        vlc_cond_signal( &p_owner->wait_acknowledge );
    }

    vlc_fifo_Unlock( p_owner->p_fifo );
    return NULL;
}

static const struct decoder_owner_callbacks dec_video_cbs =
{
    .video = {
        .get_device = ModuleThread_GetDecoderDevice,
        .format_update = ModuleThread_UpdateVideoFormat,
        .buffer_new = ModuleThread_NewVideoBuffer,
        .queue = ModuleThread_QueueVideo,
        .queue_cc = ModuleThread_QueueCc,
        .get_display_date = ModuleThread_GetDisplayDate,
        .get_display_rate = ModuleThread_GetDisplayRate,
    },
    .get_attachments = InputThread_GetInputAttachments,
};
static const struct decoder_owner_callbacks dec_thumbnailer_cbs =
{
    .video = {
        .get_device = thumbnailer_get_device,
        .buffer_new = thumbnailer_buffer_new,
        .queue = ModuleThread_QueueThumbnail,
    },
    .get_attachments = InputThread_GetInputAttachments,
};
static const struct decoder_owner_callbacks dec_audio_cbs =
{
    .audio = {
        .format_update = ModuleThread_UpdateAudioFormat,
        .queue = ModuleThread_QueueAudio,
    },
    .get_attachments = InputThread_GetInputAttachments,
};
static const struct decoder_owner_callbacks dec_spu_cbs =
{
    .spu = {
        .buffer_new = ModuleThread_NewSpuBuffer,
        .queue = ModuleThread_QueueSpu,
    },
    .get_attachments = InputThread_GetInputAttachments,
};

/**
 * Create a decoder object
 *
 * \param p_parent a VLC parent object to inherit variable from
 * \param cfg the input decoder configuration
 * \return a new input decoder object
 */
static vlc_input_decoder_t *
CreateDecoder( vlc_object_t *p_parent, const struct vlc_input_decoder_cfg *cfg )
{
    decoder_t *p_dec;
    vlc_input_decoder_t *p_owner;
    static_assert(offsetof(vlc_input_decoder_t, dec) == 0,
                  "the decoder must be first in the owner structure");

    assert(cfg->input_type != INPUT_TYPE_PREPARSING);

    const es_format_t *fmt = cfg->fmt;

    p_owner = vlc_custom_create( p_parent, sizeof( *p_owner ), "decoder" );
    if( p_owner == NULL )
        return NULL;
    p_dec = &p_owner->dec;

    p_owner->psz_id = cfg->str_id;
    p_owner->p_clock = cfg->clock;
    p_owner->i_preroll_end = PREROLL_NONE;
    p_owner->p_resource = cfg->resource;
    p_owner->hw_dec = cfg->hw_dec;
    p_owner->cbs = cfg->cbs;
    p_owner->cbs_userdata = cfg->cbs_data;
    p_owner->p_sout = cfg->sout;
    p_owner->p_sout_input = NULL;
    p_owner->p_packetizer = NULL;

    p_owner->b_fmt_description = false;
    p_owner->p_description = NULL;

    p_owner->output_delay = p_owner->delay = 0;
    p_owner->output_rate = p_owner->rate = 1.f;
    p_owner->output_paused = p_owner->paused = false;
    p_owner->pause_date = VLC_TICK_INVALID;
    p_owner->frames_countdown = 0;

    p_owner->b_waiting = false;
    p_owner->b_first = true;
    p_owner->b_has_data = false;
    p_owner->out_started = false;

    p_owner->error = false;

    p_owner->flushing = false;
    p_owner->b_draining = false;
    atomic_init( &p_owner->reload, RELOAD_NO_REQUEST );
    p_owner->b_idle = false;
    p_owner->cat = fmt->i_cat;

    es_format_Init( &p_owner->fmt, p_owner->cat, 0 );

    /* decoder fifo */
    p_owner->p_fifo = block_FifoNew();
    if( unlikely(p_owner->p_fifo == NULL) )
    {
        vlc_object_delete(p_dec);
        return NULL;
    }

    vlc_cond_init( &p_owner->wait_request );
    vlc_cond_init( &p_owner->wait_acknowledge );
    vlc_cond_init( &p_owner->wait_fifo );

    /* Load a packetizer module if the input is not already packetized */
    if( cfg->sout == NULL && !fmt->b_packetized )
    {
        p_owner->p_packetizer =
            vlc_custom_create( p_parent, sizeof( decoder_t ), "packetizer" );
        if( p_owner->p_packetizer )
        {
            decoder_Init(p_owner->p_packetizer, &p_owner->pktz_fmt_in, fmt);
            if (LoadDecoder(p_owner->p_packetizer, true, &p_owner->pktz_fmt_in))
            {
                vlc_object_delete(p_owner->p_packetizer);
                p_owner->p_packetizer = NULL;
            }
            else
            {
                p_owner->p_packetizer->fmt_out.b_packetized = true;
                fmt = &p_owner->p_packetizer->fmt_out;
            }
        }
    }

    switch( fmt->i_cat )
    {
        case VIDEO_ES:
            p_owner->video.vout = NULL;
            p_owner->video.started = false;
            p_owner->video.drained = false;
            vlc_mutex_init( &p_owner->video.mouse_lock );
            p_owner->video.mouse_event = NULL;
            p_owner->video.mouse_opaque = NULL;

            decoder_prevframe_Init( &p_owner->video.pf );
            p_owner->video.pf_pts = VLC_TICK_INVALID;

            if( cfg->input_type == INPUT_TYPE_THUMBNAILING )
                p_dec->cbs = &dec_thumbnailer_cbs;
            else
                p_dec->cbs = &dec_video_cbs;
            break;
        case AUDIO_ES:
            p_owner->audio.aout = NULL;
            p_owner->audio.stream = NULL;
            p_dec->cbs = &dec_audio_cbs;
            break;
        case SPU_ES:
            p_owner->spu.vout = NULL;
            p_owner->spu.channel = VOUT_SPU_CHANNEL_INVALID;
            p_owner->spu.order = 0;
            p_dec->cbs = &dec_spu_cbs;
            break;
        default:
            msg_Err( p_dec, "unknown ES format" );
            return p_owner;
    }

    /* Find a suitable decoder/packetizer module */
    decoder_Init(p_dec, &p_owner->dec_fmt_in, fmt);
    if (LoadDecoder(p_dec, cfg->sout != NULL, &p_owner->dec_fmt_in))
        return p_owner;

    assert( p_dec->fmt_in->i_cat == p_dec->fmt_out.i_cat && fmt->i_cat == p_dec->fmt_in->i_cat);

    /* Copy ourself the input replay gain */
    if( fmt->i_cat == AUDIO_ES )
    {
        replay_gain_Merge( &p_dec->fmt_out.audio_replay_gain, &fmt->audio_replay_gain );
    }

    /* */
    vlc_mutex_init(&p_owner->subdecs.lock);
    p_owner->cc.selected_codec = cfg->cc_decoder == 708 ?
                                 VLC_CODEC_CEA708 : VLC_CODEC_CEA608;
    p_owner->cc.b_supported = ( cfg->sout == NULL );

    p_owner->cc.desc.i_608_channels = 0;
    p_owner->cc.desc.i_708_channels = 0;
    vlc_list_init(&p_owner->subdecs.list);
    p_owner->cc.count = 0;
    p_owner->cc.p_sout_input = NULL;
    p_owner->cc.sout_es_id = NULL;
    p_owner->cc.b_sout_created = false;
    p_owner->master_dec = NULL;
    return p_owner;
}

/**
 * Destroys a decoder object
 *
 * \param p_owner the input decoder object
 * \param i_cat the elementary stream format category for the decoder
 */
static void DeleteDecoder( vlc_input_decoder_t *p_owner, enum es_format_category_e i_cat )
{
    decoder_t *p_dec = &p_owner->dec;
    msg_Dbg( p_dec, "killing decoder fourcc `%4.4s'",
             (char*)&p_dec->fmt_in->i_codec );

    decoder_Clean( p_dec );

    /* Free all packets still in the decoder fifo. */
    block_FifoEmpty( p_owner->p_fifo );

    /* Cleanup */
    if( p_owner->p_sout_input )
    {
        sout_InputDelete( p_owner->p_sout, p_owner->p_sout_input );
        if( p_owner->cc.p_sout_input )
            sout_InputDelete( p_owner->p_sout, p_owner->cc.p_sout_input );
        free( p_owner->cc.sout_es_id );
    }

    switch( i_cat )
    {
        case AUDIO_ES:
            if( p_owner->audio.aout )
            {
                /* TODO: REVISIT gap-less audio */
                assert( p_owner->audio.stream );
                vlc_aout_stream_Delete( p_owner->audio.stream );
                input_resource_PutAout( p_owner->p_resource, p_owner->audio.aout );
            }
            break;
        case VIDEO_ES: {
            vout_thread_t *vout = p_owner->video.vout;

            if ( p_owner->video.out_pool )
                picture_pool_Release( p_owner->video.out_pool );

            if (p_owner->video.vctx)
                vlc_video_context_Release( p_owner->video.vctx );

            if (vout != NULL)
            {
                /* Hold the vout since PutVout will likely release it and a
                 * last reference is needed for notify callbacks */
                vout_Hold(vout);

                enum input_resource_vout_state vout_state;
                input_resource_PutVout(p_owner->p_resource, vout, &vout_state);
                if (vout_state == INPUT_RESOURCE_VOUT_STOPPED)
                    decoder_Notify(p_owner, on_vout_stopped, vout);

                vout_Release(vout);
            }
            break;
        }
        case SPU_ES:
        {
            if( p_owner->spu.vout )
            {
                assert( p_owner->spu.channel != VOUT_SPU_CHANNEL_INVALID );
                decoder_Notify(p_owner, on_vout_stopped, p_owner->spu.vout);

                vout_UnregisterSubpictureChannel( p_owner->spu.vout,
                                                  p_owner->spu.channel );
                vout_Release(p_owner->spu.vout);
            }
            break;
        }
        case DATA_ES:
        case UNKNOWN_ES:
            break;
        default:
            vlc_assert_unreachable();
    }

    es_format_Clean( &p_owner->dec_fmt_in );
    es_format_Clean( &p_owner->pktz_fmt_in );
    es_format_Clean( &p_owner->fmt );

    if( p_owner->p_description )
        vlc_meta_Delete( p_owner->p_description );

    block_FifoRelease( p_owner->p_fifo );
    decoder_Destroy( p_owner->p_packetizer );
    decoder_Destroy( &p_owner->dec );
}

/* */
static void DecoderUnsupportedCodec( decoder_t *p_dec, const es_format_t *fmt, bool b_decoding )
{
    if (fmt->i_codec != VLC_CODEC_UNKNOWN && fmt->i_codec) {
        const char *desc = vlc_fourcc_GetDescription(fmt->i_cat, fmt->i_codec);
        if (!desc || !*desc)
            desc = N_("No description for this codec");
        msg_Err( p_dec, "Codec `%4.4s' (%s) is not supported.", (char*)&fmt->i_codec, desc );
        vlc_dialog_display_error( p_dec, _("Codec not supported"),
            _("VLC could not decode the format \"%4.4s\" (%s)"),
            (char*)&fmt->i_codec, desc );
    } else if( b_decoding ){
        msg_Err( p_dec, "could not identify codec" );
        vlc_dialog_display_error( p_dec, _("Unidentified codec"),
            _("VLC could not identify the audio or video codec" ) );
    }
}

/* TODO: pass p_sout through p_resource? -- Courmisch */
static vlc_input_decoder_t *
decoder_New( vlc_object_t *p_parent, const struct vlc_input_decoder_cfg *cfg )
{
    const char *psz_type = cfg->sout ? N_("packetizer") : N_("decoder");

    /* Create the decoder configuration structure */
    vlc_input_decoder_t *p_owner = CreateDecoder( p_parent, cfg );
    if( p_owner == NULL )
    {
        msg_Err( p_parent, "could not create %s", cfg->str_id );
        vlc_dialog_display_error( p_parent, _("Streaming / Transcoding failed"),
            _("VLC could not open the %s module."), vlc_gettext( psz_type ) );
        return NULL;
    }

    decoder_t *p_dec = &p_owner->dec;
    if( !p_dec->p_module )
    {
        DecoderUnsupportedCodec( p_dec, cfg->fmt, !cfg->sout );

        /* Don't use dec->fmt_in->i_cat since it may not be initialized here. */
        DeleteDecoder( p_owner, cfg->fmt->i_cat );
        return NULL;
    }

    assert( p_dec->fmt_in->i_cat != UNKNOWN_ES );

    /* Do not delay sout creation for SPU or DATA. */
    if( cfg->sout && cfg->fmt->b_packetized &&
        (cfg->fmt->i_cat != VIDEO_ES && cfg->fmt->i_cat != AUDIO_ES) )
    {
        p_owner->p_sout_input = sout_InputNew( p_owner->p_sout, cfg->fmt, p_owner->psz_id );
        if( p_owner->p_sout_input == NULL )
        {
            msg_Err( p_dec, "cannot create sout input (%4.4s)",
                     (char *)&cfg->fmt->i_codec );
            p_owner->error = true;
        }
    }

    if( !vlc_input_decoder_IsSynchronous( p_owner ) )
    {
        /* Spawn the decoder thread in asynchronous scenario. */
        if( vlc_clone( &p_owner->thread, DecoderThread, p_owner ) )
        {
            msg_Err( p_dec, "cannot spawn decoder thread" );
            DeleteDecoder( p_owner, p_dec->fmt_in->i_cat );
            return NULL;
        }
    }

    return p_owner;
}


/**
 * Spawns a new decoder thread from the input thread
 *
 * \param parent the VLC object to inherit variable from
 * \param cfg the input decoder configuration
 * \return the spawned decoder object
 */
vlc_input_decoder_t *
vlc_input_decoder_New( vlc_object_t *parent, const struct vlc_input_decoder_cfg *cfg )
{
    return decoder_New( parent, cfg );
}

vlc_input_decoder_t *
vlc_input_decoder_Create( vlc_object_t *p_parent, const es_format_t *fmt, const char *es_id,
                          struct vlc_clock_t *clock, input_resource_t *p_resource )
{
    const struct vlc_input_decoder_cfg cfg = {
        .fmt = fmt,
        .str_id = es_id,
        .clock = clock,
        .resource = p_resource,
        .sout = NULL,
        .input_type = INPUT_TYPE_PLAYBACK,
        .hw_dec = var_InheritBool( p_parent, "hw-dec" ),
        .cbs = NULL, .cbs_data = NULL,
    };
    return decoder_New( p_parent, &cfg );
}

static void RemoveCcDecoder(vlc_input_decoder_t *owner,
                            vlc_input_decoder_t *subdec)
{
    vlc_mutex_lock(&owner->subdecs.lock);

    vlc_input_decoder_t *it;
    vlc_list_foreach(it, &owner->subdecs.list, node)
        if (it == subdec)
        {
            vlc_list_remove(&it->node);
            owner->cc.count--;
            vlc_mutex_unlock(&owner->subdecs.lock);
            return;
        }

    vlc_assert_unreachable();
}

void vlc_input_decoder_Delete( vlc_input_decoder_t *p_owner )
{
    if (p_owner->master_dec != NULL)
        RemoveCcDecoder(p_owner->master_dec, p_owner);

    vlc_fifo_Lock( p_owner->p_fifo );
    p_owner->aborting = true;
    p_owner->b_waiting = false;
    vlc_fifo_Signal( p_owner->p_fifo );

    /* Make sure we aren't waiting/decoding anymore */
    vlc_cond_signal( &p_owner->wait_request );
    vlc_fifo_Unlock( p_owner->p_fifo );

    if( !vlc_input_decoder_IsSynchronous( p_owner ) )
        vlc_join( p_owner->thread, NULL );

#ifndef NDEBUG
    vlc_mutex_lock(&p_owner->subdecs.lock);
    assert(vlc_list_is_empty(&p_owner->subdecs.list));
    assert(p_owner->cc.count == 0);
    vlc_mutex_unlock(&p_owner->subdecs.lock);
#endif

    /* Delete decoder */
    DeleteDecoder(p_owner, p_owner->dec_fmt_in.i_cat);
}

void vlc_subdec_desc_Clean(struct vlc_subdec_desc *desc)
{
    for (size_t i = 0; i < desc->fmt_count; ++i)
        es_format_Clean(&desc->fmt_array[i]);
    free(desc->fmt_array);
}

static void GetCCDescLocked(vlc_input_decoder_t *owner,
                            struct vlc_subdec_desc *desc)
{
    vlc_fifo_Assert(owner->p_fifo);

    if (!owner->cc.desc_changed)
    {
        desc->fmt_array = NULL;
        desc->fmt_count = 0;
        return;
    }
    owner->cc.desc_changed = false;

    size_t max_channels;
    uint64_t bitmap;
    GetCcChannels(owner, &max_channels, &bitmap);

    int count = vlc_popcount(bitmap);
    if (count == 0)
    {
        desc->fmt_array = NULL;
        desc->fmt_count = 0;
        return;
    }

    desc->fmt_array = vlc_alloc(count, sizeof(es_format_t));
    if (unlikely(desc->fmt_array == NULL))
    {
        desc->fmt_count = 0;
        return;
    }
    desc->fmt_count = count;

    static const char CEA608_fmtdesc[] = N_("Closed captions %u");
    static const char CEA708_fmtdesc[] = N_("DTVCC Closed captions %u");
    const char *fmtdesc = owner->cc.selected_codec == VLC_CODEC_CEA608 ?
                          CEA608_fmtdesc : CEA708_fmtdesc;

    size_t array_idx = 0;
    for (int i = 0; bitmap > 0; bitmap >>= 1, i++)
    {
        if ((bitmap & 1) == 0)
            continue;
        es_format_t *fmt = &desc->fmt_array[array_idx++];
        es_format_Init(fmt, SPU_ES, owner->cc.selected_codec);
        fmt->i_id = i + 1;
        fmt->subs.cc.i_channel = i;
        fmt->subs.cc.i_reorder_depth = owner->cc.desc.i_reorder_depth;
        if (asprintf(&fmt->psz_description, fmtdesc, fmt->i_id) == -1)
            fmt->psz_description = NULL;
    }
}

static void GetStatusLocked(vlc_input_decoder_t *p_owner,
                            struct vlc_input_decoder_status *status)
{
    vlc_fifo_Assert(p_owner->p_fifo);

    status->format.changed = p_owner->b_fmt_description;
    p_owner->b_fmt_description = false;

    if( status->format.changed && p_owner->fmt.i_cat == UNKNOWN_ES )
    {
        /* The format changed but the output creation failed */
        status->format.changed = false;
    }

    if( status->format.changed )
    {
        es_format_Copy( &status->format.fmt, &p_owner->fmt );
        status->format.meta = NULL;

        if( p_owner->p_description )
        {
            status->format.meta  = vlc_meta_New();
            if( status->format.meta  )
                vlc_meta_Merge( status->format.meta, p_owner->p_description );
        }
    }
    GetCCDescLocked(p_owner, &status->subdec_desc);
}

void vlc_input_decoder_DecodeWithStatus(vlc_input_decoder_t *p_owner, vlc_frame_t *frame,
                                        bool b_do_pace,
                                        struct vlc_input_decoder_status *status)
{
    if( vlc_input_decoder_IsSynchronous( p_owner ) )
    {
        /* DecoderThread's fifo should be empty as no decoder thread is running. */
        assert( vlc_fifo_IsEmpty( p_owner->p_fifo ) );
        vlc_fifo_Lock(p_owner->p_fifo);
        DecoderThread_ProcessInput( p_owner, frame );
        if (status != NULL)
            GetStatusLocked(p_owner, status);
        vlc_fifo_Unlock(p_owner->p_fifo);
        return;
    }

    vlc_fifo_Lock( p_owner->p_fifo );
    if( !b_do_pace )
    {
        /* FIXME: ideally we would check the time amount of data
         * in the FIFO instead of its size. */
        /* 400 MiB, i.e. ~ 50mb/s for 60s */
        if( vlc_fifo_GetBytes( p_owner->p_fifo ) > 400*1024*1024 )
        {
            msg_Warn( &p_owner->dec, "decoder/packetizer fifo full (data not "
                      "consumed quickly enough), resetting fifo!" );
            block_ChainRelease( vlc_fifo_DequeueAllUnlocked( p_owner->p_fifo ) );
            frame->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        }
    }
    else
    if( !p_owner->b_waiting )
    {   /* The FIFO is not consumed when waiting, so pacing would deadlock VLC.
         * Locking is not necessary as b_waiting is only read, not written by
         * the decoder thread. */
        while( vlc_fifo_GetCount( p_owner->p_fifo ) >= 10 )
            vlc_fifo_WaitCond( p_owner->p_fifo, &p_owner->wait_fifo );
    }

    if (vlc_fifo_IsEmpty(p_owner->p_fifo) && p_owner->frames_countdown > 0)
        decoder_Notify(p_owner, frame_next_need_data, false);

    vlc_fifo_QueueUnlocked( p_owner->p_fifo, frame );
    if (status != NULL)
        GetStatusLocked(p_owner, status);

    struct vlc_tracer *tracer = vlc_object_get_tracer(&p_owner->dec.obj);
    if (tracer != NULL)
    {
        size_t fifo_size = vlc_fifo_GetBytes(p_owner->p_fifo);
        size_t fifo_count = vlc_fifo_GetCount(p_owner->p_fifo);
        vlc_tracer_Trace(tracer,
                         VLC_TRACE("id", p_owner->psz_id),
                         VLC_TRACE("fifo_size", (uint64_t)fifo_size),
                         VLC_TRACE("fifo_count", (uint64_t)fifo_count),
                         VLC_TRACE_END);
    }


    vlc_fifo_Unlock( p_owner->p_fifo );
}

void vlc_input_decoder_Decode(vlc_input_decoder_t *p_owner, vlc_frame_t *frame,
                              bool b_do_pace)
{
    vlc_input_decoder_DecodeWithStatus(p_owner, frame, b_do_pace, NULL);
}

static bool vlc_input_decoder_IsDrainedLocked(vlc_input_decoder_t *owner)
{
    vlc_fifo_Assert(owner->p_fifo);

    if (owner->b_draining)
        return false;
    else if (owner->p_sout_input != NULL)
        return true;
    else if (owner->cat == VIDEO_ES && owner->video.vout != NULL)
        return vout_IsEmpty(owner->video.vout);
    else if(owner->cat == AUDIO_ES && owner->audio.stream != NULL)
        return vlc_aout_stream_IsDrained( owner->audio.stream);
    else
        return true; /* TODO subtitles support */
}

bool vlc_input_decoder_IsDrained(vlc_input_decoder_t *owner)
{
    vlc_fifo_Lock(owner->p_fifo);
    bool drained = vlc_input_decoder_IsDrainedLocked(owner);
    vlc_fifo_Unlock(owner->p_fifo);
    return drained;
}

bool vlc_input_decoder_IsEmpty( vlc_input_decoder_t * p_owner )
{
    assert( !p_owner->b_waiting );

    vlc_fifo_Lock( p_owner->p_fifo );
    if( !vlc_fifo_IsEmpty( p_owner->p_fifo ) )
    {
        vlc_fifo_Unlock( p_owner->p_fifo );
        return false;
    }

    bool b_empty = vlc_input_decoder_IsDrainedLocked( p_owner );

    vlc_fifo_Unlock( p_owner->p_fifo );

    return b_empty;
}

void vlc_input_decoder_Drain( vlc_input_decoder_t *p_owner )
{
    if ( vlc_input_decoder_IsSynchronous( p_owner ) )
    {
        /* Process a NULL frame synchronously to signal draining to packetizer/decoder. */
        vlc_fifo_Lock(p_owner->p_fifo);
        DecoderThread_ProcessInput( p_owner, NULL );
        vlc_fifo_Unlock(p_owner->p_fifo);
        return;
    }

    vlc_fifo_Lock( p_owner->p_fifo );
    p_owner->b_draining = true;
    vlc_fifo_Signal( p_owner->p_fifo );
    vlc_fifo_Unlock( p_owner->p_fifo );
}

void vlc_input_decoder_Flush( vlc_input_decoder_t *p_owner )
{
    vlc_fifo_Lock( p_owner->p_fifo );
    enum es_format_category_e cat = p_owner->cat;

    /* Empty the fifo */
    block_ChainRelease( vlc_fifo_DequeueAllUnlocked( p_owner->p_fifo ) );

    /* Don't need to wait for the DecoderThread to flush. Indeed, if called a
     * second time, this function will clear the FIFO again before anything was
     * dequeued by DecoderThread and there is no need to flush a second time in
     * a row. */
    p_owner->flushing = true;
    p_owner->b_draining = false;

    /* Flush video/spu decoder when paused: increment frames_countdown in order
     * to display one frame/subtitle */
    if( p_owner->paused && ( cat == VIDEO_ES || cat == SPU_ES )
     && p_owner->frames_countdown == 0 )
        p_owner->frames_countdown = 1;

    if ( p_owner->p_sout_input != NULL )
    {
        sout_InputFlush( p_owner->p_sout, p_owner->p_sout_input );
    }
    else if( cat == AUDIO_ES )
    {
        if( p_owner->audio.stream )
            vlc_aout_stream_Flush( p_owner->audio.stream );
    }
    else if( cat == VIDEO_ES )
    {
        p_owner->video.drained = false;
        if( p_owner->video.vout && p_owner->video.started
         && p_owner->frames_countdown != -1 )
        {
            /* prev-frame: don't flush if frames_countdown == -1. If requests
             * are sent in a burst, we want to avoid flushing the previous
             * frame that is being displayed. */
            vout_FlushAll( p_owner->video.vout );
        }
        decoder_prevframe_Flush( &p_owner->video.pf );
    }
    else if( cat == SPU_ES )
    {
        if( p_owner->spu.vout )
        {
            assert( p_owner->spu.channel != VOUT_SPU_CHANNEL_INVALID );
            vout_FlushSubpictureChannel( p_owner->spu.vout, p_owner->spu.channel );
        }
    }
    vlc_fifo_Signal( p_owner->p_fifo );

    if (unlikely(p_owner->b_waiting && p_owner->b_has_data))
    {
        /* Signal the output thread to stop waiting from DecoderWaitUnblock()
         * and to discard the current frame (via 'flushing' = true). */
        vlc_cond_signal(&p_owner->wait_request);

        /* Flushing is fully asynchronous, but we need to wait for the output
         * thread to unblock in DecoderWaitUnblock() otherwise there are no
         * ways to know if the frame referenced when waiting comes from before
         * or after the flush. Waiting here is almost instantaneous since we
         * are sure that the output thread is waiting in DecoderWaitUnblock().
         */
        while (p_owner->b_has_data)
            vlc_fifo_WaitCond(p_owner->p_fifo, &p_owner->wait_acknowledge);
    }

    vlc_fifo_Unlock( p_owner->p_fifo );

    if (vlc_input_decoder_IsSynchronous(p_owner))
    {
        /* With a synchronous decoder,there is no decoder thread which
         * can process the flush request. We flush synchronously from
         * here and reset the flushing state. */
        DecoderThread_Flush(p_owner);
        vlc_fifo_Lock(p_owner->p_fifo);
        p_owner->flushing = false;
        p_owner->i_preroll_end = PREROLL_NONE;
        vlc_fifo_Unlock(p_owner->p_fifo);
    }
}

vlc_input_decoder_t *
vlc_input_decoder_CreateSubDec(vlc_input_decoder_t *p_owner,
                               const struct vlc_input_decoder_cfg *cfg)
{
    decoder_t *p_dec = &p_owner->dec;

    vlc_mutex_lock(&p_owner->subdecs.lock);

    vlc_input_decoder_t *p_ccowner;

    p_ccowner = vlc_input_decoder_New( VLC_OBJECT(p_dec), cfg );
    if( !p_ccowner )
    {
        msg_Err( p_dec, "could not create decoder" );
        vlc_dialog_display_error( p_dec,
            _("Streaming / Transcoding failed"), "%s",
            _("VLC could not open the decoder module.") );
        vlc_mutex_unlock(&p_owner->subdecs.lock);
        return NULL;
    }
    assert(p_ccowner->dec.p_module != NULL);

    vlc_list_append(&p_ccowner->node, &p_owner->subdecs.list);
    p_owner->cc.count++;
    p_ccowner->master_dec = p_owner;

    vlc_mutex_unlock(&p_owner->subdecs.lock);
    return p_ccowner;
}

void vlc_input_decoder_ChangePause( vlc_input_decoder_t *p_owner,
                                    bool b_paused, vlc_tick_t i_date )
{
    /* Normally, p_owner->b_paused != b_paused here. But if a track is added
     * while the input is paused (e.g. add sub file), then b_paused is
     * (incorrectly) false. FIXME: This is a bug in the decoder owner. */
    vlc_fifo_Lock( p_owner->p_fifo );

    p_owner->paused = b_paused;
    p_owner->pause_date = i_date;
    p_owner->frames_countdown = 0;
    vlc_fifo_Signal( p_owner->p_fifo );
    vlc_fifo_Unlock( p_owner->p_fifo );
}

void vlc_input_decoder_ChangeRate( vlc_input_decoder_t *owner, float rate )
{
    vlc_fifo_Lock( owner->p_fifo );
    owner->rate = rate;
    vlc_fifo_Unlock( owner->p_fifo );
}

void vlc_input_decoder_ChangeDelay( vlc_input_decoder_t *owner, vlc_tick_t delay )
{
    vlc_fifo_Lock( owner->p_fifo );
    owner->delay = delay;
    vlc_fifo_Unlock( owner->p_fifo );
}

void vlc_input_decoder_StartWait( vlc_input_decoder_t *p_owner )
{
    if ( vlc_input_decoder_IsSynchronous( p_owner ) )
        return;

    assert( !p_owner->b_waiting );

    vlc_fifo_Lock(p_owner->p_fifo);
    p_owner->b_has_data = false;
    p_owner->b_first = true;
    p_owner->b_waiting = true;
    vlc_cond_signal(&p_owner->wait_request);
    vlc_fifo_Unlock(p_owner->p_fifo);
}

void vlc_input_decoder_StopWait( vlc_input_decoder_t *p_owner )
{
    if ( vlc_input_decoder_IsSynchronous( p_owner ) )
        return;

    vlc_fifo_Lock(p_owner->p_fifo);
    assert( p_owner->b_waiting );
    p_owner->b_waiting = false;
    vlc_cond_signal( &p_owner->wait_request );
    vlc_fifo_Unlock(p_owner->p_fifo);
}

void vlc_input_decoder_Wait( vlc_input_decoder_t *p_owner )
{
    if ( vlc_input_decoder_IsSynchronous( p_owner ) )
        return;
    assert( p_owner->b_waiting );

    vlc_fifo_Lock(p_owner->p_fifo);
    while( !p_owner->b_has_data )
    {
        /* Don't need to lock p_owner->paused since it's only modified by the
         * owner */
        if( p_owner->paused )
            break;
        if( p_owner->b_idle && vlc_fifo_IsEmpty( p_owner->p_fifo ) )
        {
            msg_Err( &p_owner->dec, "buffer deadlock prevented" );
            break;
        }
        vlc_fifo_WaitCond(p_owner->p_fifo, &p_owner->wait_acknowledge);
    }
    vlc_fifo_Unlock(p_owner->p_fifo);
}

static void StopFrameNextLocked(vlc_input_decoder_t *owner)
{
    decoder_prevframe_Reset(&owner->video.pf);
    owner->video.pf_pts = VLC_TICK_INVALID;
    if (owner->frames_countdown == -1)
        owner->frames_countdown = 0;
    vlc_fifo_Signal(owner->p_fifo);
}

void vlc_input_decoder_StopFrameNext(vlc_input_decoder_t *owner)
{
    vlc_fifo_Lock(owner->p_fifo);
    StopFrameNextLocked(owner);
    vlc_fifo_Unlock(owner->p_fifo);
}

void vlc_input_decoder_FrameNext( vlc_input_decoder_t *p_owner )
{
    assert( p_owner->paused );
    assert( p_owner->cat == VIDEO_ES );

    vlc_fifo_Lock( p_owner->p_fifo );

    if( p_owner->video.vout == NULL )
    {
        decoder_Notify( p_owner, frame_next_status, -EBUSY );
        vlc_fifo_Unlock( p_owner->p_fifo );
        return;
    }

    if( unlikely( p_owner->frames_countdown == INT_MAX ) )
    {
        decoder_Notify( p_owner, frame_next_status, -EINVAL );
        vlc_fifo_Unlock( p_owner->p_fifo );
        return;
    }

    if( p_owner->video.drained && vout_IsEmpty( p_owner->video.vout ) )
    {
        p_owner->frames_countdown = 0;
        decoder_Notify( p_owner, frame_next_status, -EAGAIN );
        vlc_fifo_Unlock( p_owner->p_fifo );
        return;
    }

    StopFrameNextLocked( p_owner );

    if (!p_owner->output_paused)
    {
        /* Request will be handled when paused, from
         * Decoder_PausedForNextFrame() */
        p_owner->frames_countdown++;
        vlc_fifo_Unlock( p_owner->p_fifo );
        return;
    }

    size_t needed_count = vout_NextPicture(p_owner->video.vout, 1);
    assert(p_owner->frames_countdown >= 0);
    if (needed_count > (unsigned) p_owner->frames_countdown)
        p_owner->frames_countdown++;
    else
        decoder_Notify(p_owner, frame_next_status, 0);

    vlc_fifo_Unlock( p_owner->p_fifo );
}

void vlc_input_decoder_FramePrevious(vlc_input_decoder_t *owner)
{
    assert(owner->paused);
    assert(owner->cat == VIDEO_ES);

    vlc_fifo_Lock(owner->p_fifo);

    if (!owner->video.started)
    {
        decoder_Notify(owner, frame_previous_status, -EBUSY);
        vlc_fifo_Unlock(owner->p_fifo);
        return;
    }

    if (owner->frames_countdown != -1)
    {
        owner->frames_countdown = -1;

        owner->video.pf_pts = vout_FlushAll(owner->video.vout);
    }

    if (owner->video.pf_pts == VLC_TICK_INVALID)
    {
        /* No frame displayed yet (that's a success) */
        decoder_Notify(owner, frame_previous_status, 0);
        vlc_fifo_Unlock(owner->p_fifo);
        return;
    }

    Decoder_RequestFramePrevious(owner);

    vlc_fifo_Unlock(owner->p_fifo);
}

size_t vlc_input_decoder_GetFifoSize( vlc_input_decoder_t *p_owner )
{
    return block_FifoSize( p_owner->p_fifo );
}

static bool DecoderHasVbi( decoder_t *dec )
{
    return dec->fmt_in->i_cat == SPU_ES && dec->fmt_in->i_codec == VLC_CODEC_TELETEXT
        && var_Type( dec, "vbi-page" ) == VLC_VAR_INTEGER;
}

int vlc_input_decoder_GetVbiPage( vlc_input_decoder_t *owner, bool *opaque )
{
    decoder_t *dec = &owner->dec;
    if( !DecoderHasVbi( dec ) )
        return -1;
    *opaque = var_GetBool( dec, "vbi-opaque" );
    return var_GetInteger( dec, "vbi-page" );
}

int vlc_input_decoder_SetVbiPage( vlc_input_decoder_t *owner, unsigned page )
{
    decoder_t *dec = &owner->dec;
    if( !DecoderHasVbi( dec ) )
        return VLC_EGENERIC;
    return var_SetInteger( dec, "vbi-page", page );
}

int vlc_input_decoder_SetVbiOpaque( vlc_input_decoder_t *owner, bool opaque )
{
    decoder_t *dec = &owner->dec;
    if( !DecoderHasVbi( dec ) )
        return VLC_EGENERIC;
    return var_SetBool( dec, "vbi-opaque", opaque );
}

void vlc_input_decoder_SetVoutMouseEvent( vlc_input_decoder_t *owner,
                                          vlc_mouse_event mouse_event,
                                          void *user_data )
{
    assert( owner->cat == VIDEO_ES );

    vlc_mutex_lock( &owner->video.mouse_lock );

    owner->video.mouse_event = mouse_event;
    owner->video.mouse_opaque = user_data;

    vlc_mutex_unlock( &owner->video.mouse_lock );
}

int vlc_input_decoder_AddVoutOverlay( vlc_input_decoder_t *owner, subpicture_t *sub,
                                      size_t *channel )
{
    assert( owner->cat == VIDEO_ES );
    assert( sub && channel );

    vlc_fifo_Lock(owner->p_fifo);

    if( !owner->video.vout )
    {
        vlc_fifo_Unlock(owner->p_fifo);
        return VLC_EGENERIC;
    }
    ssize_t channel_id =
        vout_RegisterSubpictureChannel( owner->video.vout );
    if (channel_id == -1)
    {
        vlc_fifo_Unlock(owner->p_fifo);
        return VLC_EGENERIC;
    }
    sub->i_start = sub->i_stop = vlc_tick_now();
    sub->i_channel = *channel = channel_id;
    sub->i_order = 0;
    sub->b_ephemer = true;
    vout_PutSubpicture( owner->video.vout, sub );

    vlc_fifo_Unlock(owner->p_fifo);
    return VLC_SUCCESS;
}

int vlc_input_decoder_DelVoutOverlay( vlc_input_decoder_t *owner, size_t channel )
{
    assert( owner->cat == VIDEO_ES );

    vlc_fifo_Lock(owner->p_fifo);

    if( !owner->video.vout )
    {
        vlc_fifo_Unlock(owner->p_fifo);
        return VLC_EGENERIC;
    }
    vout_UnregisterSubpictureChannel( owner->video.vout, channel );

    vlc_fifo_Unlock(owner->p_fifo);
    return VLC_SUCCESS;
}

int vlc_input_decoder_SetSpuHighlight( vlc_input_decoder_t *p_owner,
                                       const vlc_spu_highlight_t *spu_hl )
{
    assert( p_owner->cat == SPU_ES );

    if( p_owner->p_sout_input )
        sout_InputControl( p_owner->p_sout, p_owner->p_sout_input,
                           SOUT_INPUT_SET_SPU_HIGHLIGHT, spu_hl );

    vlc_fifo_Lock(p_owner->p_fifo);
    if( !p_owner->spu.vout )
    {
        vlc_fifo_Unlock(p_owner->p_fifo);
        return VLC_EGENERIC;
    }

    vout_SetSpuHighlight( p_owner->spu.vout, spu_hl );

    vlc_fifo_Unlock(p_owner->p_fifo);
    return VLC_SUCCESS;
}
