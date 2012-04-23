/*****************************************************************************
 * decoder.c: Functions for the management of decoders
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 * $Id$
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

#include "audio_output/aout_internal.h"
#include "stream_output/stream_output.h"
#include "input_internal.h"
#include "clock.h"
#include "decoder.h"
#include "event.h"
#include "resource.h"

#include "../video_output/vout_control.h"

static decoder_t *CreateDecoder( vlc_object_t *, input_thread_t *,
                                 es_format_t *, bool, input_resource_t *,
                                 sout_instance_t *p_sout );
static void       DeleteDecoder( decoder_t * );

static void      *DecoderThread( void * );
static void       DecoderProcess( decoder_t *, block_t * );
static void       DecoderError( decoder_t *p_dec, block_t *p_block );
static void       DecoderOutputChangePause( decoder_t *, bool b_paused, mtime_t i_date );
static void       DecoderFlush( decoder_t * );
static void       DecoderSignalBuffering( decoder_t *, bool );
static void       DecoderFlushBuffering( decoder_t * );

static void       DecoderUnsupportedCodec( decoder_t *, vlc_fourcc_t );

/* Buffers allocation callbacks for the decoders */
static aout_buffer_t *aout_new_buffer( decoder_t *, int );

static picture_t *vout_new_buffer( decoder_t * );
static void vout_del_buffer( decoder_t *, picture_t * );
static void vout_link_picture( decoder_t *, picture_t * );
static void vout_unlink_picture( decoder_t *, picture_t * );

static subpicture_t *spu_new_buffer( decoder_t *, const subpicture_updater_t * );
static void spu_del_buffer( decoder_t *, subpicture_t * );

struct decoder_owner_sys_t
{
    int64_t         i_preroll_end;

    input_thread_t  *p_input;
    input_resource_t*p_resource;
    input_clock_t   *p_clock;
    int             i_last_rate;

    vout_thread_t   *p_spu_vout;
    int              i_spu_channel;
    int64_t          i_spu_order;

    sout_instance_t         *p_sout;
    sout_packetizer_input_t *p_sout_input;

    vlc_thread_t     thread;

    /* Some decoders require already packetized data (ie. not truncated) */
    decoder_t *p_packetizer;
    bool b_packetizer;

    /* Current format in use by the output */
    video_format_t video;
    audio_format_t audio;
    es_format_t    sout;

    /* */
    bool           b_fmt_description;
    es_format_t    fmt_description;
    vlc_meta_t     *p_description;

    /* fifo */
    block_fifo_t *p_fifo;

    /* Lock for communication with decoder thread */
    vlc_mutex_t lock;
    vlc_cond_t  wait_request;
    vlc_cond_t  wait_acknowledge;

    /* -- These variables need locking on write(only) -- */
    audio_output_t *p_aout;

    vout_thread_t   *p_vout;

    /* -- Theses variables need locking on read *and* write -- */
    bool b_exit;

    /* Pause */
    bool b_paused;
    struct
    {
        mtime_t i_date;
        int     i_ignore;
    } pause;

    /* Buffering */
    bool b_buffering;
    struct
    {
        bool b_first;
        bool b_full;
        int  i_count;

        picture_t     *p_picture;
        picture_t     **pp_picture_next;

        subpicture_t  *p_subpic;
        subpicture_t  **pp_subpic_next;

        aout_buffer_t *p_audio;
        aout_buffer_t **pp_audio_next;

        block_t       *p_block;
        block_t       **pp_block_next;
    } buffer;

    /* Flushing */
    bool b_flushing;

    /* CC */
    struct
    {
        bool b_supported;
        bool pb_present[4];
        decoder_t *pp_decoder[4];
    } cc;

    /* Delay */
    mtime_t i_ts_delay;
};

#define DECODER_MAX_BUFFERING_COUNT (4)
#define DECODER_MAX_BUFFERING_AUDIO_DURATION (AOUT_MAX_PREPARE_TIME)
#define DECODER_MAX_BUFFERING_VIDEO_DURATION (1*CLOCK_FREQ)

/* Pictures which are DECODER_BOGUS_VIDEO_DELAY or more in advance probably have
 * a bogus PTS and won't be displayed */
#define DECODER_BOGUS_VIDEO_DELAY                ((mtime_t)(DEFAULT_PTS_DELAY * 30))

/* */
#define DECODER_SPU_VOUT_WAIT_DURATION ((int)(0.200*CLOCK_FREQ))


/*****************************************************************************
 * Public functions
 *****************************************************************************/
picture_t *decoder_NewPicture( decoder_t *p_decoder )
{
    picture_t *p_picture = p_decoder->pf_vout_buffer_new( p_decoder );
    if( !p_picture )
        msg_Warn( p_decoder, "can't get output picture" );
    return p_picture;
}
void decoder_DeletePicture( decoder_t *p_decoder, picture_t *p_picture )
{
    p_decoder->pf_vout_buffer_del( p_decoder, p_picture );
}
void decoder_LinkPicture( decoder_t *p_decoder, picture_t *p_picture )
{
    p_decoder->pf_picture_link( p_decoder, p_picture );
}
void decoder_UnlinkPicture( decoder_t *p_decoder, picture_t *p_picture )
{
    p_decoder->pf_picture_unlink( p_decoder, p_picture );
}

aout_buffer_t *decoder_NewAudioBuffer( decoder_t *p_decoder, int i_size )
{
    if( !p_decoder->pf_aout_buffer_new )
        return NULL;
    return p_decoder->pf_aout_buffer_new( p_decoder, i_size );
}

subpicture_t *decoder_NewSubpicture( decoder_t *p_decoder,
                                     const subpicture_updater_t *p_dyn )
{
    subpicture_t *p_subpicture = p_decoder->pf_spu_buffer_new( p_decoder, p_dyn );
    if( !p_subpicture )
        msg_Warn( p_decoder, "can't get output subpicture" );
    return p_subpicture;
}

void decoder_DeleteSubpicture( decoder_t *p_decoder, subpicture_t *p_subpicture )
{
    p_decoder->pf_spu_buffer_del( p_decoder, p_subpicture );
}

/* decoder_GetInputAttachments:
 */
int decoder_GetInputAttachments( decoder_t *p_dec,
                                 input_attachment_t ***ppp_attachment,
                                 int *pi_attachment )
{
    if( !p_dec->pf_get_attachments )
        return VLC_EGENERIC;

    return p_dec->pf_get_attachments( p_dec, ppp_attachment, pi_attachment );
}
/* decoder_GetDisplayDate:
 */
mtime_t decoder_GetDisplayDate( decoder_t *p_dec, mtime_t i_ts )
{
    if( !p_dec->pf_get_display_date )
        return VLC_TS_INVALID;

    return p_dec->pf_get_display_date( p_dec, i_ts );
}
/* decoder_GetDisplayRate:
 */
int decoder_GetDisplayRate( decoder_t *p_dec )
{
    if( !p_dec->pf_get_display_rate )
        return INPUT_RATE_DEFAULT;

    return p_dec->pf_get_display_rate( p_dec );
}

/* TODO: pass p_sout through p_resource? -- Courmisch */
static decoder_t *decoder_New( vlc_object_t *p_parent, input_thread_t *p_input,
                               es_format_t *fmt, input_clock_t *p_clock,
                               input_resource_t *p_resource,
                               sout_instance_t *p_sout  )
{
    decoder_t *p_dec = NULL;
    const char *psz_type = p_sout ? N_("packetizer") : N_("decoder");
    int i_priority;

    /* Create the decoder configuration structure */
    p_dec = CreateDecoder( p_parent, p_input, fmt,
                           p_sout != NULL, p_resource, p_sout );
    if( p_dec == NULL )
    {
        msg_Err( p_parent, "could not create %s", psz_type );
        dialog_Fatal( p_parent, _("Streaming / Transcoding failed"),
                      _("VLC could not open the %s module."),
                      vlc_gettext( psz_type ) );
        return NULL;
    }

    if( !p_dec->p_module )
    {
        DecoderUnsupportedCodec( p_dec, fmt->i_codec );

        DeleteDecoder( p_dec );
        return NULL;
    }

    p_dec->p_owner->p_clock = p_clock;
    assert( p_dec->fmt_out.i_cat != UNKNOWN_ES );

    if( p_dec->fmt_out.i_cat == AUDIO_ES )
        i_priority = VLC_THREAD_PRIORITY_AUDIO;
    else
        i_priority = VLC_THREAD_PRIORITY_VIDEO;

    /* Spawn the decoder thread */
    if( vlc_clone( &p_dec->p_owner->thread, DecoderThread, p_dec, i_priority ) )
    {
        msg_Err( p_dec, "cannot spawn decoder thread" );
        module_unneed( p_dec, p_dec->p_module );
        DeleteDecoder( p_dec );
        return NULL;
    }

    return p_dec;
}


/**
 * Spawns a new decoder thread from the input thread
 *
 * \param p_input the input thread
 * \param p_es the es descriptor
 * \return the spawned decoder object
 */
decoder_t *input_DecoderNew( input_thread_t *p_input,
                             es_format_t *fmt, input_clock_t *p_clock,
                             sout_instance_t *p_sout  )
{
    return decoder_New( VLC_OBJECT(p_input), p_input, fmt, p_clock,
                        p_input->p->p_resource, p_sout );
}

/**
 * Spawn a decoder thread outside of the input thread.
 */
decoder_t *input_DecoderCreate( vlc_object_t *p_parent, es_format_t *fmt,
                                input_resource_t *p_resource )
{
    return decoder_New( p_parent, NULL, fmt, NULL, p_resource, NULL );
}


/**
 * Kills a decoder thread and waits until it's finished
 *
 * \param p_input the input thread
 * \param p_es the es descriptor
 * \return nothing
 */
void input_DecoderDelete( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_cancel( p_owner->thread );

    /* Make sure we aren't paused/buffering/waiting/decoding anymore */
    vlc_mutex_lock( &p_owner->lock );
    const bool b_was_paused = p_owner->b_paused;
    p_owner->b_paused = false;
    p_owner->b_buffering = false;
    p_owner->b_flushing = true;
    p_owner->b_exit = true;
    vlc_cond_signal( &p_owner->wait_request );
    vlc_mutex_unlock( &p_owner->lock );

    vlc_join( p_owner->thread, NULL );
    p_owner->b_paused = b_was_paused;

    module_unneed( p_dec, p_dec->p_module );

    /* */
    if( p_dec->p_owner->cc.b_supported )
    {
        int i;
        for( i = 0; i < 4; i++ )
            input_DecoderSetCcState( p_dec, false, i );
    }

    /* Delete decoder */
    DeleteDecoder( p_dec );
}

/**
 * Put a block_t in the decoder's fifo.
 * Thread-safe w.r.t. the decoder. May be a cancellation point.
 *
 * \param p_dec the decoder object
 * \param p_block the data block
 */
void input_DecoderDecode( decoder_t *p_dec, block_t *p_block, bool b_do_pace )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    if( b_do_pace )
    {
        /* The fifo is not consummed when buffering and so will
         * deadlock vlc.
         * There is no need to lock as b_buffering is never modify
         * inside decoder thread. */
        if( !p_owner->b_buffering )
            block_FifoPace( p_owner->p_fifo, 10, SIZE_MAX );
    }
#ifdef __arm__
    else if( block_FifoSize( p_owner->p_fifo ) > 50*1024*1024 /* 50 MiB */ )
#else
    else if( block_FifoSize( p_owner->p_fifo ) > 400*1024*1024 /* 400 MiB, ie ~ 50mb/s for 60s */ )
#endif
    {
        /* FIXME: ideally we would check the time amount of data
         * in the FIFO instead of its size. */
        msg_Warn( p_dec, "decoder/packetizer fifo full (data not "
                  "consumed quickly enough), resetting fifo!" );
        block_FifoEmpty( p_owner->p_fifo );
    }

    block_FifoPut( p_owner->p_fifo, p_block );
}

bool input_DecoderIsEmpty( decoder_t * p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    assert( !p_owner->b_buffering );

    bool b_empty = block_FifoCount( p_dec->p_owner->p_fifo ) <= 0;
    if( b_empty )
    {
        vlc_mutex_lock( &p_owner->lock );
        /* TODO subtitles support */
        if( p_dec->fmt_out.i_cat == VIDEO_ES && p_owner->p_vout )
            b_empty = vout_IsEmpty( p_owner->p_vout );
        else if( p_dec->fmt_out.i_cat == AUDIO_ES && p_owner->p_aout )
            b_empty = aout_DecIsEmpty( p_owner->p_aout );
        vlc_mutex_unlock( &p_owner->lock );
    }
    return b_empty;
}

void input_DecoderIsCcPresent( decoder_t *p_dec, bool pb_present[4] )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    int i;

    vlc_mutex_lock( &p_owner->lock );
    for( i = 0; i < 4; i++ )
        pb_present[i] =  p_owner->cc.pb_present[i];
    vlc_mutex_unlock( &p_owner->lock );
}
int input_DecoderSetCcState( decoder_t *p_dec, bool b_decode, int i_channel )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    //msg_Warn( p_dec, "input_DecoderSetCcState: %d @%d", b_decode, i_channel );

    if( i_channel < 0 || i_channel >= 4 || !p_owner->cc.pb_present[i_channel] )
        return VLC_EGENERIC;

    if( b_decode )
    {
        static const vlc_fourcc_t fcc[4] = {
            VLC_FOURCC('c', 'c', '1', ' '),
            VLC_FOURCC('c', 'c', '2', ' '),
            VLC_FOURCC('c', 'c', '3', ' '),
            VLC_FOURCC('c', 'c', '4', ' '),
        };
        decoder_t *p_cc;
        es_format_t fmt;

        es_format_Init( &fmt, SPU_ES, fcc[i_channel] );
        p_cc = CreateDecoder( VLC_OBJECT(p_dec), p_owner->p_input, &fmt,
                              false, p_owner->p_resource, p_owner->p_sout );
        if( !p_cc )
        {
            msg_Err( p_dec, "could not create decoder" );
            dialog_Fatal( p_dec, _("Streaming / Transcoding failed"), "%s",
                          _("VLC could not open the decoder module.") );
            return VLC_EGENERIC;
        }
        else if( !p_cc->p_module )
        {
            DecoderUnsupportedCodec( p_dec, fcc[i_channel] );
            DeleteDecoder( p_cc );
            return VLC_EGENERIC;
        }
        p_cc->p_owner->p_clock = p_owner->p_clock;

        vlc_mutex_lock( &p_owner->lock );
        p_owner->cc.pp_decoder[i_channel] = p_cc;
        vlc_mutex_unlock( &p_owner->lock );
    }
    else
    {
        decoder_t *p_cc;

        vlc_mutex_lock( &p_owner->lock );
        p_cc = p_owner->cc.pp_decoder[i_channel];
        p_owner->cc.pp_decoder[i_channel] = NULL;
        vlc_mutex_unlock( &p_owner->lock );

        if( p_cc )
        {
            module_unneed( p_cc, p_cc->p_module );
            DeleteDecoder( p_cc );
        }
    }
    return VLC_SUCCESS;
}
int input_DecoderGetCcState( decoder_t *p_dec, bool *pb_decode, int i_channel )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    *pb_decode = false;
    if( i_channel < 0 || i_channel >= 4 || !p_owner->cc.pb_present[i_channel] )
        return VLC_EGENERIC;

    vlc_mutex_lock( &p_owner->lock );
    *pb_decode = p_owner->cc.pp_decoder[i_channel] != NULL;
    vlc_mutex_unlock( &p_owner->lock );
    return VLC_EGENERIC;
}

void input_DecoderChangePause( decoder_t *p_dec, bool b_paused, mtime_t i_date )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_mutex_lock( &p_owner->lock );

    assert( p_owner->b_paused != b_paused );

    p_owner->b_paused = b_paused;
    p_owner->pause.i_date = i_date;
    p_owner->pause.i_ignore = 0;
    vlc_cond_signal( &p_owner->wait_request );

    DecoderOutputChangePause( p_dec, b_paused, i_date );

    vlc_mutex_unlock( &p_owner->lock );
}

void input_DecoderChangeDelay( decoder_t *p_dec, mtime_t i_delay )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_mutex_lock( &p_owner->lock );
    p_owner->i_ts_delay = i_delay;
    vlc_mutex_unlock( &p_owner->lock );
}

void input_DecoderStartBuffering( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_mutex_lock( &p_owner->lock );

    DecoderFlush( p_dec );

    p_owner->buffer.b_first = true;
    p_owner->buffer.b_full = false;
    p_owner->buffer.i_count = 0;

    assert( !p_owner->buffer.p_picture && !p_owner->buffer.p_subpic &&
            !p_owner->buffer.p_audio && !p_owner->buffer.p_block );

    p_owner->buffer.p_picture = NULL;
    p_owner->buffer.pp_picture_next = &p_owner->buffer.p_picture;

    p_owner->buffer.p_subpic = NULL;
    p_owner->buffer.pp_subpic_next = &p_owner->buffer.p_subpic;

    p_owner->buffer.p_audio = NULL;
    p_owner->buffer.pp_audio_next = &p_owner->buffer.p_audio;

    p_owner->buffer.p_block = NULL;
    p_owner->buffer.pp_block_next = &p_owner->buffer.p_block;


    p_owner->b_buffering = true;

    vlc_cond_signal( &p_owner->wait_request );

    vlc_mutex_unlock( &p_owner->lock );
}

void input_DecoderStopBuffering( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_mutex_lock( &p_owner->lock );

    p_owner->b_buffering = false;

    vlc_cond_signal( &p_owner->wait_request );

    vlc_mutex_unlock( &p_owner->lock );
}

void input_DecoderWaitBuffering( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_mutex_lock( &p_owner->lock );

    while( p_owner->b_buffering && !p_owner->buffer.b_full )
    {
        block_FifoWake( p_owner->p_fifo );
        vlc_cond_wait( &p_owner->wait_acknowledge, &p_owner->lock );
    }

    vlc_mutex_unlock( &p_owner->lock );
}

void input_DecoderFrameNext( decoder_t *p_dec, mtime_t *pi_duration )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    *pi_duration = 0;

    vlc_mutex_lock( &p_owner->lock );
    if( p_dec->fmt_out.i_cat == VIDEO_ES )
    {
        if( p_owner->b_paused && p_owner->p_vout )
        {
            vout_NextPicture( p_owner->p_vout, pi_duration );
            p_owner->pause.i_ignore++;
            vlc_cond_signal( &p_owner->wait_request );
        }
    }
    else
    {
        /* TODO subtitle should not be flushed */
        DecoderFlush( p_dec );
    }
    vlc_mutex_unlock( &p_owner->lock );
}

bool input_DecoderHasFormatChanged( decoder_t *p_dec, es_format_t *p_fmt, vlc_meta_t **pp_meta )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    bool b_changed;

    vlc_mutex_lock( &p_owner->lock );
    b_changed = p_owner->b_fmt_description;
    if( b_changed )
    {
        if( p_fmt )
            es_format_Copy( p_fmt, &p_owner->fmt_description );

        if( pp_meta )
        {
            *pp_meta = NULL;
            if( p_owner->p_description )
            {
                *pp_meta = vlc_meta_New();
                if( *pp_meta )
                    vlc_meta_Merge( *pp_meta, p_owner->p_description );
            }
        }
        p_owner->b_fmt_description = false;
    }
    vlc_mutex_unlock( &p_owner->lock );
    return b_changed;
}

size_t input_DecoderGetFifoSize( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    return block_FifoSize( p_owner->p_fifo );
}

void input_DecoderGetObjects( decoder_t *p_dec,
                              vout_thread_t **pp_vout, audio_output_t **pp_aout )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_mutex_lock( &p_owner->lock );
    if( pp_vout )
        *pp_vout = p_owner->p_vout ? vlc_object_hold( p_owner->p_vout ) : NULL;
    if( pp_aout )
        *pp_aout = p_owner->p_aout ? vlc_object_hold( p_owner->p_aout ) : NULL;
    vlc_mutex_unlock( &p_owner->lock );
}

/*****************************************************************************
 * Internal functions
 *****************************************************************************/
static int DecoderGetInputAttachments( decoder_t *p_dec,
                                       input_attachment_t ***ppp_attachment,
                                       int *pi_attachment )
{
    input_thread_t *p_input = p_dec->p_owner->p_input;

    if( unlikely(p_input == NULL) )
        return VLC_ENOOBJ;
    return input_Control( p_input, INPUT_GET_ATTACHMENTS,
                          ppp_attachment, pi_attachment );
}
static mtime_t DecoderGetDisplayDate( decoder_t *p_dec, mtime_t i_ts )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_mutex_lock( &p_owner->lock );
    if( p_owner->b_buffering || p_owner->b_paused )
        i_ts = VLC_TS_INVALID;
    vlc_mutex_unlock( &p_owner->lock );

    if( !p_owner->p_clock || i_ts <= VLC_TS_INVALID )
        return i_ts;

    if( input_clock_ConvertTS( p_owner->p_clock, NULL, &i_ts, NULL, INT64_MAX ) )
        return VLC_TS_INVALID;

    return i_ts;
}
static int DecoderGetDisplayRate( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    if( !p_owner->p_clock )
        return INPUT_RATE_DEFAULT;
    return input_clock_GetRate( p_owner->p_clock );
}

/* */
static void DecoderUnsupportedCodec( decoder_t *p_dec, vlc_fourcc_t codec )
{
    msg_Err( p_dec, "no suitable decoder module for fourcc `%4.4s'. "
             "VLC probably does not support this sound or video format.",
             (char*)&codec );
    dialog_Fatal( p_dec, _("No suitable decoder module"),
                 _("VLC does not support the audio or video format \"%4.4s\". "
                  "Unfortunately there is no way for you to fix this."),
                  (char*)&codec );
}


/**
 * Create a decoder object
 *
 * \param p_input the input thread
 * \param p_es the es descriptor
 * \param b_packetizer instead of a decoder
 * \return the decoder object
 */
static decoder_t * CreateDecoder( vlc_object_t *p_parent,
                                  input_thread_t *p_input,
                                  es_format_t *fmt, bool b_packetizer,
                                  input_resource_t *p_resource,
                                  sout_instance_t *p_sout )
{
    decoder_t *p_dec;
    decoder_owner_sys_t *p_owner;
    es_format_t null_es_format;

    p_dec = vlc_custom_create( p_parent, sizeof( *p_dec ), "decoder" );
    if( p_dec == NULL )
        return NULL;

    p_dec->pf_decode_audio = NULL;
    p_dec->pf_decode_video = NULL;
    p_dec->pf_decode_sub = NULL;
    p_dec->pf_get_cc = NULL;
    p_dec->pf_packetize = NULL;

    /* Initialize the decoder */
    p_dec->p_module = NULL;

    memset( &null_es_format, 0, sizeof(es_format_t) );
    es_format_Copy( &p_dec->fmt_in, fmt );
    es_format_Copy( &p_dec->fmt_out, &null_es_format );

    p_dec->p_description = NULL;

    /* Allocate our private structure for the decoder */
    p_dec->p_owner = p_owner = malloc( sizeof( decoder_owner_sys_t ) );
    if( unlikely(p_owner == NULL) )
    {
        vlc_object_release( p_dec );
        return NULL;
    }
    p_owner->i_preroll_end = VLC_TS_INVALID;
    p_owner->i_last_rate = INPUT_RATE_DEFAULT;
    p_owner->p_input = p_input;
    p_owner->p_resource = p_resource;
    p_owner->p_aout = NULL;
    p_owner->p_vout = NULL;
    p_owner->p_spu_vout = NULL;
    p_owner->i_spu_channel = 0;
    p_owner->i_spu_order = 0;
    p_owner->p_sout = p_sout;
    p_owner->p_sout_input = NULL;
    p_owner->p_packetizer = NULL;
    p_owner->b_packetizer = b_packetizer;

    /* decoder fifo */
    p_owner->p_fifo = block_FifoNew();
    if( unlikely(p_owner->p_fifo == NULL) )
    {
        free( p_owner );
        vlc_object_release( p_dec );
        return NULL;
    }

    /* Set buffers allocation callbacks for the decoders */
    p_dec->pf_aout_buffer_new = aout_new_buffer;
    p_dec->pf_vout_buffer_new = vout_new_buffer;
    p_dec->pf_vout_buffer_del = vout_del_buffer;
    p_dec->pf_picture_link    = vout_link_picture;
    p_dec->pf_picture_unlink  = vout_unlink_picture;
    p_dec->pf_spu_buffer_new  = spu_new_buffer;
    p_dec->pf_spu_buffer_del  = spu_del_buffer;
    /* */
    p_dec->pf_get_attachments  = DecoderGetInputAttachments;
    p_dec->pf_get_display_date = DecoderGetDisplayDate;
    p_dec->pf_get_display_rate = DecoderGetDisplayRate;

    /* Find a suitable decoder/packetizer module */
    if( !b_packetizer )
        p_dec->p_module = module_need( p_dec, "decoder", "$codec", false );
    else
        p_dec->p_module = module_need( p_dec, "packetizer", "$packetizer", false );

    /* Check if decoder requires already packetized data */
    if( !b_packetizer &&
        p_dec->b_need_packetized && !p_dec->fmt_in.b_packetized )
    {
        p_owner->p_packetizer =
            vlc_custom_create( p_parent, sizeof( decoder_t ), "packetizer" );
        if( p_owner->p_packetizer )
        {
            es_format_Copy( &p_owner->p_packetizer->fmt_in,
                            &p_dec->fmt_in );

            es_format_Copy( &p_owner->p_packetizer->fmt_out,
                            &null_es_format );

            p_owner->p_packetizer->p_module =
                module_need( p_owner->p_packetizer,
                             "packetizer", "$packetizer", false );

            if( !p_owner->p_packetizer->p_module )
            {
                es_format_Clean( &p_owner->p_packetizer->fmt_in );
                vlc_object_release( p_owner->p_packetizer );
            }
        }
    }

    /* Copy ourself the input replay gain */
    if( fmt->i_cat == AUDIO_ES )
    {
        for( unsigned i = 0; i < AUDIO_REPLAY_GAIN_MAX; i++ )
        {
            if( !p_dec->fmt_out.audio_replay_gain.pb_peak[i] )
            {
                p_dec->fmt_out.audio_replay_gain.pb_peak[i] = fmt->audio_replay_gain.pb_peak[i];
                p_dec->fmt_out.audio_replay_gain.pf_peak[i] = fmt->audio_replay_gain.pf_peak[i];
            }
            if( !p_dec->fmt_out.audio_replay_gain.pb_gain[i] )
            {
                p_dec->fmt_out.audio_replay_gain.pb_gain[i] = fmt->audio_replay_gain.pb_gain[i];
                p_dec->fmt_out.audio_replay_gain.pf_gain[i] = fmt->audio_replay_gain.pf_gain[i];
            }
        }
    }

    /* */
    vlc_mutex_init( &p_owner->lock );
    vlc_cond_init( &p_owner->wait_request );
    vlc_cond_init( &p_owner->wait_acknowledge );

    p_owner->b_fmt_description = false;
    es_format_Init( &p_owner->fmt_description, UNKNOWN_ES, 0 );
    p_owner->p_description = NULL;

    p_owner->b_exit = false;

    p_owner->b_paused = false;
    p_owner->pause.i_date = VLC_TS_INVALID;
    p_owner->pause.i_ignore = 0;

    p_owner->b_buffering = false;
    p_owner->buffer.b_first = true;
    p_owner->buffer.b_full = false;
    p_owner->buffer.i_count = 0;
    p_owner->buffer.p_picture = NULL;
    p_owner->buffer.p_subpic = NULL;
    p_owner->buffer.p_audio = NULL;
    p_owner->buffer.p_block = NULL;

    p_owner->b_flushing = false;

    /* */
    p_owner->cc.b_supported = false;
    if( !b_packetizer )
    {
        if( p_owner->p_packetizer && p_owner->p_packetizer->pf_get_cc )
            p_owner->cc.b_supported = true;
        if( p_dec->pf_get_cc )
            p_owner->cc.b_supported = true;
    }

    for( unsigned i = 0; i < 4; i++ )
    {
        p_owner->cc.pb_present[i] = false;
        p_owner->cc.pp_decoder[i] = NULL;
    }
    p_owner->i_ts_delay = 0;
    return p_dec;
}

/**
 * The decoding main loop
 *
 * \param p_dec the decoder
 */
static void *DecoderThread( void *p_data )
{
    decoder_t *p_dec = (decoder_t *)p_data;
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    /* The decoder's main loop */
    for( ;; )
    {
        block_t *p_block = block_FifoGet( p_owner->p_fifo );

        /* Make sure there is no cancellation point other than this one^^.
         * If you need one, be sure to push cleanup of p_block. */
        DecoderSignalBuffering( p_dec, p_block == NULL );

        if( p_block )
        {
            int canc = vlc_savecancel();

            if( p_block->i_flags & BLOCK_FLAG_CORE_EOS )
            {
                /* calling DecoderProcess() with NULL block will make
                 * decoders/packetizers flush their buffers */
                block_Release( p_block );
                p_block = NULL;
            }

            if( p_dec->b_error )
                DecoderError( p_dec, p_block );
            else
                DecoderProcess( p_dec, p_block );

            vlc_restorecancel( canc );
        }
    }
    return NULL;
}

static block_t *DecoderBlockFlushNew()
{
    block_t *p_null = block_Alloc( 128 );
    if( !p_null )
        return NULL;

    p_null->i_flags |= BLOCK_FLAG_DISCONTINUITY |
                       BLOCK_FLAG_CORRUPTED |
                       BLOCK_FLAG_CORE_FLUSH;
    memset( p_null->p_buffer, 0, p_null->i_buffer );

    return p_null;
}

static void DecoderFlush( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_assert_locked( &p_owner->lock );

    /* Empty the fifo */
    block_FifoEmpty( p_owner->p_fifo );

    /* Monitor for flush end */
    p_owner->b_flushing = true;
    vlc_cond_signal( &p_owner->wait_request );

    /* Send a special block */
    block_t *p_null = DecoderBlockFlushNew();
    if( !p_null )
        return;
    input_DecoderDecode( p_dec, p_null, false );

    /* */
    while( p_owner->b_flushing )
        vlc_cond_wait( &p_owner->wait_acknowledge, &p_owner->lock );
}

static void DecoderSignalBuffering( decoder_t *p_dec, bool b_full )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_mutex_lock( &p_owner->lock );

    if( p_owner->b_buffering )
    {
        if( b_full )
            p_owner->buffer.b_full = true;
        vlc_cond_signal( &p_owner->wait_acknowledge );
    }

    vlc_mutex_unlock( &p_owner->lock );
}

static bool DecoderIsFlushing( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    bool b_flushing;

    vlc_mutex_lock( &p_owner->lock );

    b_flushing = p_owner->b_flushing;

    vlc_mutex_unlock( &p_owner->lock );

    return b_flushing;
}

static void DecoderWaitUnblock( decoder_t *p_dec, bool *pb_reject )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_assert_locked( &p_owner->lock );

    for( ;; )
    {
        if( p_owner->b_flushing )
            break;
        if( p_owner->b_paused )
        {
            if( p_owner->b_buffering && !p_owner->buffer.b_full )
                break;
            if( p_owner->pause.i_ignore > 0 )
            {
                p_owner->pause.i_ignore--;
                break;
            }
        }
        else
        {
            if( !p_owner->b_buffering || !p_owner->buffer.b_full )
                break;
        }
        vlc_cond_wait( &p_owner->wait_request, &p_owner->lock );
    }

    if( pb_reject )
        *pb_reject = p_owner->b_flushing;
}

static void DecoderOutputChangePause( decoder_t *p_dec, bool b_paused, mtime_t i_date )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_assert_locked( &p_owner->lock );

    /* XXX only audio and video output have to be paused.
     * - for sout it is useless
     * - for subs, it is done by the vout
     */
    if( p_dec->fmt_out.i_cat == AUDIO_ES )
    {
        if( p_owner->p_aout )
            aout_DecChangePause( p_owner->p_aout, b_paused, i_date );
    }
    else if( p_dec->fmt_out.i_cat == VIDEO_ES )
    {
        if( p_owner->p_vout )
            vout_ChangePause( p_owner->p_vout, b_paused, i_date );
    }
}
static inline void DecoderUpdatePreroll( int64_t *pi_preroll, const block_t *p )
{
    if( p->i_flags & (BLOCK_FLAG_PREROLL|BLOCK_FLAG_DISCONTINUITY) )
        *pi_preroll = INT64_MAX;
    else if( p->i_dts > VLC_TS_INVALID )
        *pi_preroll = __MIN( *pi_preroll, p->i_dts );
    else if( p->i_pts > VLC_TS_INVALID )
        *pi_preroll = __MIN( *pi_preroll, p->i_pts );
}

static void DecoderFixTs( decoder_t *p_dec, mtime_t *pi_ts0, mtime_t *pi_ts1,
                          mtime_t *pi_duration, int *pi_rate, mtime_t i_ts_bound )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    input_clock_t   *p_clock = p_owner->p_clock;

    vlc_assert_locked( &p_owner->lock );

    const mtime_t i_es_delay = p_owner->i_ts_delay;

    if( !p_clock )
        return;

    const bool b_ephemere = pi_ts1 && *pi_ts0 == *pi_ts1;
    int i_rate;

    if( *pi_ts0 > VLC_TS_INVALID )
    {
        *pi_ts0 += i_es_delay;
        if( pi_ts1 && *pi_ts1 > VLC_TS_INVALID )
            *pi_ts1 += i_es_delay;
        if( input_clock_ConvertTS( p_clock, &i_rate, pi_ts0, pi_ts1, i_ts_bound ) )
            *pi_ts0 = VLC_TS_INVALID;
    }
    else
    {
        i_rate = input_clock_GetRate( p_clock );
    }

    /* Do not create ephemere data because of rounding errors */
    if( !b_ephemere && pi_ts1 && *pi_ts0 == *pi_ts1 )
        *pi_ts1 += 1;

    if( pi_duration )
        *pi_duration = ( *pi_duration * i_rate + INPUT_RATE_DEFAULT-1 )
            / INPUT_RATE_DEFAULT;

    if( pi_rate )
        *pi_rate = i_rate;
}

static bool DecoderIsExitRequested( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_mutex_lock( &p_owner->lock );
    bool b_exit = p_owner->b_exit;
    vlc_mutex_unlock( &p_owner->lock );

    return b_exit;
}

/**
 * If *pb_reject, it does nothing, otherwise it waits for the given
 * deadline or a flush request (in which case it set *pi_reject to true.
 */
static void DecoderWaitDate( decoder_t *p_dec,
                             bool *pb_reject, mtime_t i_deadline )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_assert_locked( &p_owner->lock );

    if( *pb_reject || i_deadline < 0 )
        return;

    do
    {
        if( p_owner->b_flushing || p_owner->b_exit )
        {
            *pb_reject = true;
            break;
        }
    }
    while( vlc_cond_timedwait( &p_owner->wait_request, &p_owner->lock,
                               i_deadline ) == 0 );
}

static void DecoderPlayAudio( decoder_t *p_dec, aout_buffer_t *p_audio,
                              int *pi_played_sum, int *pi_lost_sum )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    audio_output_t *p_aout = p_owner->p_aout;

    /* */
    if( p_audio->i_pts <= VLC_TS_INVALID ) // FIXME --VLC_TS_INVALID verify audio_output/*
    {
        msg_Warn( p_dec, "non-dated audio buffer received" );
        *pi_lost_sum += 1;
        aout_BufferFree( p_audio );
        return;
    }

    /* */
    vlc_mutex_lock( &p_owner->lock );

    if( p_owner->b_buffering || p_owner->buffer.p_audio )
    {
        p_audio->p_next = NULL;

        *p_owner->buffer.pp_audio_next = p_audio;
        p_owner->buffer.pp_audio_next = &p_audio->p_next;

        p_owner->buffer.i_count++;
        if( p_owner->buffer.i_count > DECODER_MAX_BUFFERING_COUNT ||
            p_audio->i_pts - p_owner->buffer.p_audio->i_pts > DECODER_MAX_BUFFERING_AUDIO_DURATION )
        {
            p_owner->buffer.b_full = true;
            vlc_cond_signal( &p_owner->wait_acknowledge );
        }
    }

    for( ;; )
    {
        bool b_has_more = false;
        bool b_reject;
        DecoderWaitUnblock( p_dec, &b_reject );

        if( p_owner->b_buffering )
            break;

        /* */
        if( p_owner->buffer.p_audio )
        {
            p_audio = p_owner->buffer.p_audio;

            p_owner->buffer.p_audio = p_audio->p_next;
            p_owner->buffer.i_count--;

            b_has_more = p_owner->buffer.p_audio != NULL;
            if( !b_has_more )
                p_owner->buffer.pp_audio_next = &p_owner->buffer.p_audio;
        }

        /* */
        int i_rate = INPUT_RATE_DEFAULT;

        DecoderFixTs( p_dec, &p_audio->i_pts, NULL, &p_audio->i_length,
                      &i_rate, AOUT_MAX_ADVANCE_TIME );

        if( !p_aout ||
            p_audio->i_pts <= VLC_TS_INVALID ||
            i_rate < INPUT_RATE_DEFAULT/AOUT_MAX_INPUT_RATE ||
            i_rate > INPUT_RATE_DEFAULT*AOUT_MAX_INPUT_RATE )
            b_reject = true;

        DecoderWaitDate( p_dec, &b_reject,
                         p_audio->i_pts - AOUT_MAX_PREPARE_TIME );

        if( !b_reject )
        {
            assert( !p_owner->b_paused );
            if( !aout_DecPlay( p_aout, p_audio, i_rate ) )
                *pi_played_sum += 1;
            *pi_lost_sum += aout_DecGetResetLost( p_aout );
        }
        else
        {
            msg_Dbg( p_dec, "discarded audio buffer" );
            *pi_lost_sum += 1;
            aout_BufferFree( p_audio );
        }

        if( !b_has_more )
            break;
        if( !p_owner->buffer.p_audio )
            break;
    }
    vlc_mutex_unlock( &p_owner->lock );
}

static void DecoderDecodeAudio( decoder_t *p_dec, block_t *p_block )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    aout_buffer_t   *p_aout_buf;
    int i_decoded = 0;
    int i_lost = 0;
    int i_played = 0;

    while( (p_aout_buf = p_dec->pf_decode_audio( p_dec, &p_block )) )
    {
        audio_output_t *p_aout = p_owner->p_aout;

        if( DecoderIsExitRequested( p_dec ) )
        {
            /* It prevent freezing VLC in case of broken decoder */
            aout_DecDeleteBuffer( p_aout, p_aout_buf );
            if( p_block )
                block_Release( p_block );
            break;
        }
        i_decoded++;

        if( p_owner->i_preroll_end > VLC_TS_INVALID &&
            p_aout_buf->i_pts < p_owner->i_preroll_end )
        {
            aout_DecDeleteBuffer( p_aout, p_aout_buf );
            continue;
        }

        if( p_owner->i_preroll_end > VLC_TS_INVALID )
        {
            msg_Dbg( p_dec, "End of audio preroll" );
            if( p_owner->p_aout )
                aout_DecFlush( p_owner->p_aout );
            /* */
            p_owner->i_preroll_end = VLC_TS_INVALID;
        }

        DecoderPlayAudio( p_dec, p_aout_buf, &i_played, &i_lost );
    }

    /* Update ugly stat */
    input_thread_t  *p_input = p_owner->p_input;

    if( p_input != NULL && (i_decoded > 0 || i_lost > 0 || i_played > 0) )
    {
        vlc_mutex_lock( &p_input->p->counters.counters_lock);
        stats_Update( p_input->p->counters.p_lost_abuffers, i_lost, NULL );
        stats_Update( p_input->p->counters.p_played_abuffers, i_played, NULL );
        stats_Update( p_input->p->counters.p_decoded_audio, i_decoded, NULL );
        vlc_mutex_unlock( &p_input->p->counters.counters_lock);
    }
}
static void DecoderGetCc( decoder_t *p_dec, decoder_t *p_dec_cc )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    block_t *p_cc;
    bool pb_present[4];
    bool b_processed = false;
    int i;
    int i_cc_decoder;

    assert( p_dec_cc->pf_get_cc != NULL );

    /* Do not try retreiving CC if not wanted (sout) or cannot be retreived */
    if( !p_owner->cc.b_supported )
        return;

    p_cc = p_dec_cc->pf_get_cc( p_dec_cc, pb_present );
    if( !p_cc )
        return;

    vlc_mutex_lock( &p_owner->lock );
    for( i = 0, i_cc_decoder = 0; i < 4; i++ )
    {
        p_owner->cc.pb_present[i] |= pb_present[i];
        if( p_owner->cc.pp_decoder[i] )
            i_cc_decoder++;
    }

    for( i = 0; i < 4; i++ )
    {
        if( !p_owner->cc.pp_decoder[i] )
            continue;

        if( i_cc_decoder > 1 )
            DecoderProcess( p_owner->cc.pp_decoder[i], block_Duplicate( p_cc ) );
        else
            DecoderProcess( p_owner->cc.pp_decoder[i], p_cc );
        i_cc_decoder--;
        b_processed = true;
    }
    vlc_mutex_unlock( &p_owner->lock );

    if( !b_processed )
        block_Release( p_cc );
}

static void DecoderPlayVideo( decoder_t *p_dec, picture_t *p_picture,
                              int *pi_played_sum, int *pi_lost_sum )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    vout_thread_t  *p_vout = p_owner->p_vout;
    bool b_first_buffered;

    if( p_picture->date <= VLC_TS_INVALID )
    {
        msg_Warn( p_dec, "non-dated video buffer received" );
        *pi_lost_sum += 1;
        vout_ReleasePicture( p_vout, p_picture );
        return;
    }

    /* */
    vlc_mutex_lock( &p_owner->lock );

    if( ( p_owner->b_buffering && !p_owner->buffer.b_first ) || p_owner->buffer.p_picture )
    {
        p_picture->p_next = NULL;

        *p_owner->buffer.pp_picture_next = p_picture;
        p_owner->buffer.pp_picture_next = &p_picture->p_next;

        p_owner->buffer.i_count++;
        if( p_owner->buffer.i_count > DECODER_MAX_BUFFERING_COUNT ||
            p_picture->date - p_owner->buffer.p_picture->date > DECODER_MAX_BUFFERING_VIDEO_DURATION )
        {
            p_owner->buffer.b_full = true;
            vlc_cond_signal( &p_owner->wait_acknowledge );
        }
    }
    b_first_buffered = p_owner->buffer.p_picture != NULL;

    for( ;; b_first_buffered = false )
    {
        bool b_has_more = false;

        bool b_reject;

        DecoderWaitUnblock( p_dec, &b_reject );

        if( p_owner->b_buffering && !p_owner->buffer.b_first )
        {
            vlc_mutex_unlock( &p_owner->lock );
            return;
        }
        bool b_buffering_first = p_owner->b_buffering;

        /* */
        if( p_owner->buffer.p_picture )
        {
            p_picture = p_owner->buffer.p_picture;

            p_owner->buffer.p_picture = p_picture->p_next;
            p_owner->buffer.i_count--;

            b_has_more = p_owner->buffer.p_picture != NULL;
            if( !b_has_more )
                p_owner->buffer.pp_picture_next = &p_owner->buffer.p_picture;
        }

        /* */
        if( b_buffering_first )
        {
            assert( p_owner->buffer.b_first );
            assert( !p_owner->buffer.i_count );
            msg_Dbg( p_dec, "Received first picture" );
            p_owner->buffer.b_first = false;
            p_picture->b_force = true;
        }

        const bool b_dated = p_picture->date > VLC_TS_INVALID;
        int i_rate = INPUT_RATE_DEFAULT;
        DecoderFixTs( p_dec, &p_picture->date, NULL, NULL,
                      &i_rate, DECODER_BOGUS_VIDEO_DELAY );

        vlc_mutex_unlock( &p_owner->lock );

        /* */
        if( !p_picture->b_force && p_picture->date <= VLC_TS_INVALID ) // FIXME --VLC_TS_INVALID verify video_output/*
            b_reject = true;

        if( !b_reject )
        {
            if( i_rate != p_owner->i_last_rate || b_first_buffered )
            {
                /* Be sure to not display old picture after our own */
                vout_Flush( p_vout, p_picture->date );
                p_owner->i_last_rate = i_rate;
            }
            vout_PutPicture( p_vout, p_picture );
        }
        else
        {
            if( b_dated )
                msg_Warn( p_dec, "early picture skipped" );
            else
                msg_Warn( p_dec, "non-dated video buffer received" );

            *pi_lost_sum += 1;
            vout_ReleasePicture( p_vout, p_picture );
        }
        int i_tmp_display;
        int i_tmp_lost;
        vout_GetResetStatistic( p_vout, &i_tmp_display, &i_tmp_lost );

        *pi_played_sum += i_tmp_display;
        *pi_lost_sum += i_tmp_lost;

        if( !b_has_more || b_buffering_first )
            break;

        vlc_mutex_lock( &p_owner->lock );
        if( !p_owner->buffer.p_picture )
        {
            vlc_mutex_unlock( &p_owner->lock );
            break;
        }
    }
}

static void DecoderDecodeVideo( decoder_t *p_dec, block_t *p_block )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    picture_t      *p_pic;
    int i_lost = 0;
    int i_decoded = 0;
    int i_displayed = 0;

    while( (p_pic = p_dec->pf_decode_video( p_dec, &p_block )) )
    {
        vout_thread_t  *p_vout = p_owner->p_vout;
        if( DecoderIsExitRequested( p_dec ) )
        {
            /* It prevent freezing VLC in case of broken decoder */
            vout_ReleasePicture( p_vout, p_pic );
            if( p_block )
                block_Release( p_block );
            break;
        }

        i_decoded++;

        if( p_owner->i_preroll_end > VLC_TS_INVALID && p_pic->date < p_owner->i_preroll_end )
        {
            vout_ReleasePicture( p_vout, p_pic );
            continue;
        }

        if( p_owner->i_preroll_end > VLC_TS_INVALID )
        {
            msg_Dbg( p_dec, "End of video preroll" );
            if( p_vout )
                vout_Flush( p_vout, VLC_TS_INVALID+1 );
            /* */
            p_owner->i_preroll_end = VLC_TS_INVALID;
        }

        if( p_dec->pf_get_cc &&
            ( !p_owner->p_packetizer || !p_owner->p_packetizer->pf_get_cc ) )
            DecoderGetCc( p_dec, p_dec );

        DecoderPlayVideo( p_dec, p_pic, &i_displayed, &i_lost );
    }

    /* Update ugly stat */
    input_thread_t *p_input = p_owner->p_input;

    if( p_input != NULL && (i_decoded > 0 || i_lost > 0 || i_displayed > 0) )
    {
        vlc_mutex_lock( &p_input->p->counters.counters_lock );
        stats_Update( p_input->p->counters.p_decoded_video, i_decoded, NULL );
        stats_Update( p_input->p->counters.p_lost_pictures, i_lost , NULL);
        stats_Update( p_input->p->counters.p_displayed_pictures,
                      i_displayed, NULL);
        vlc_mutex_unlock( &p_input->p->counters.counters_lock );
    }
}

static void DecoderPlaySpu( decoder_t *p_dec, subpicture_t *p_subpic )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    vout_thread_t *p_vout = p_owner->p_spu_vout;

    /* */
    if( p_subpic->i_start <= VLC_TS_INVALID )
    {
        msg_Warn( p_dec, "non-dated spu buffer received" );
        subpicture_Delete( p_subpic );
        return;
    }

    /* */
    vlc_mutex_lock( &p_owner->lock );

    if( p_owner->b_buffering || p_owner->buffer.p_subpic )
    {
        p_subpic->p_next = NULL;

        *p_owner->buffer.pp_subpic_next = p_subpic;
        p_owner->buffer.pp_subpic_next = &p_subpic->p_next;

        p_owner->buffer.i_count++;
        /* XXX it is important to be full after the first one */
        if( p_owner->buffer.i_count > 0 )
        {
            p_owner->buffer.b_full = true;
            vlc_cond_signal( &p_owner->wait_acknowledge );
        }
    }

    for( ;; )
    {
        bool b_has_more = false;
        bool b_reject;
        DecoderWaitUnblock( p_dec, &b_reject );

        if( p_owner->b_buffering )
        {
            vlc_mutex_unlock( &p_owner->lock );
            return;
        }

        /* */
        if( p_owner->buffer.p_subpic )
        {
            p_subpic = p_owner->buffer.p_subpic;

            p_owner->buffer.p_subpic = p_subpic->p_next;
            p_owner->buffer.i_count--;

            b_has_more = p_owner->buffer.p_subpic != NULL;
            if( !b_has_more )
                p_owner->buffer.pp_subpic_next = &p_owner->buffer.p_subpic;
        }

        /* */
        DecoderFixTs( p_dec, &p_subpic->i_start, &p_subpic->i_stop, NULL,
                      NULL, INT64_MAX );

        if( p_subpic->i_start <= VLC_TS_INVALID )
            b_reject = true;

        DecoderWaitDate( p_dec, &b_reject,
                         p_subpic->i_start - SPU_MAX_PREPARE_TIME );
        vlc_mutex_unlock( &p_owner->lock );

        if( !b_reject )
            vout_PutSubpicture( p_vout, p_subpic );
        else
            subpicture_Delete( p_subpic );

        if( !b_has_more )
            break;
        vlc_mutex_lock( &p_owner->lock );
        if( !p_owner->buffer.p_subpic )
        {
            vlc_mutex_unlock( &p_owner->lock );
            break;
        }
    }
}

#ifdef ENABLE_SOUT
static void DecoderPlaySout( decoder_t *p_dec, block_t *p_sout_block )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    assert( p_owner->p_clock );
    assert( !p_sout_block->p_next );

    vlc_mutex_lock( &p_owner->lock );

    if( p_owner->b_buffering || p_owner->buffer.p_block )
    {
        block_ChainLastAppend( &p_owner->buffer.pp_block_next, p_sout_block );

        p_owner->buffer.i_count++;
        /* XXX it is important to be full after the first one */
        if( p_owner->buffer.i_count > 0 )
        {
            p_owner->buffer.b_full = true;
            vlc_cond_signal( &p_owner->wait_acknowledge );
        }
    }

    for( ;; )
    {
        bool b_has_more = false;
        bool b_reject;
        DecoderWaitUnblock( p_dec, &b_reject );

        if( p_owner->b_buffering )
        {
            vlc_mutex_unlock( &p_owner->lock );
            return;
        }

        /* */
        if( p_owner->buffer.p_block )
        {
            p_sout_block = p_owner->buffer.p_block;

            p_owner->buffer.p_block = p_sout_block->p_next;
            p_owner->buffer.i_count--;

            b_has_more = p_owner->buffer.p_block != NULL;
            if( !b_has_more )
                p_owner->buffer.pp_block_next = &p_owner->buffer.p_block;
        }
        p_sout_block->p_next = NULL;

        DecoderFixTs( p_dec, &p_sout_block->i_dts, &p_sout_block->i_pts,
                      &p_sout_block->i_length, NULL, INT64_MAX );

        vlc_mutex_unlock( &p_owner->lock );

        if( !b_reject )
            sout_InputSendBuffer( p_owner->p_sout_input, p_sout_block ); // FIXME --VLC_TS_INVALID inspect stream_output/*
        else
            block_Release( p_sout_block );

        if( !b_has_more )
            break;
        vlc_mutex_lock( &p_owner->lock );
        if( !p_owner->buffer.p_block )
        {
            vlc_mutex_unlock( &p_owner->lock );
            break;
        }
    }
}
#endif

/* */
static void DecoderFlushBuffering( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_assert_locked( &p_owner->lock );

    while( p_owner->buffer.p_picture )
    {
        picture_t *p_picture = p_owner->buffer.p_picture;

        p_owner->buffer.p_picture = p_picture->p_next;
        p_owner->buffer.i_count--;

        if( p_owner->p_vout )
        {
            vout_ReleasePicture( p_owner->p_vout, p_picture );
        }

        if( !p_owner->buffer.p_picture )
            p_owner->buffer.pp_picture_next = &p_owner->buffer.p_picture;
    }
    while( p_owner->buffer.p_audio )
    {
        aout_buffer_t *p_audio = p_owner->buffer.p_audio;

        p_owner->buffer.p_audio = p_audio->p_next;
        p_owner->buffer.i_count--;

        aout_BufferFree( p_audio );

        if( !p_owner->buffer.p_audio )
            p_owner->buffer.pp_audio_next = &p_owner->buffer.p_audio;
    }
    while( p_owner->buffer.p_subpic )
    {
        subpicture_t *p_subpic = p_owner->buffer.p_subpic;

        p_owner->buffer.p_subpic = p_subpic->p_next;
        p_owner->buffer.i_count--;

        subpicture_Delete( p_subpic );

        if( !p_owner->buffer.p_subpic )
            p_owner->buffer.pp_subpic_next = &p_owner->buffer.p_subpic;
    }
    if( p_owner->buffer.p_block )
    {
        block_ChainRelease( p_owner->buffer.p_block );

        p_owner->buffer.i_count = 0;
        p_owner->buffer.p_block = NULL;
        p_owner->buffer.pp_block_next = &p_owner->buffer.p_block;
    }
}

#ifdef ENABLE_SOUT
/* This function process a block for sout
 */
static void DecoderProcessSout( decoder_t *p_dec, block_t *p_block )
{
    decoder_owner_sys_t *p_owner = (decoder_owner_sys_t *)p_dec->p_owner;
    block_t *p_sout_block;

    while( ( p_sout_block =
                 p_dec->pf_packetize( p_dec, p_block ? &p_block : NULL ) ) )
    {
        if( !p_owner->p_sout_input )
        {
            es_format_Copy( &p_owner->sout, &p_dec->fmt_out );

            p_owner->sout.i_group = p_dec->fmt_in.i_group;
            p_owner->sout.i_id = p_dec->fmt_in.i_id;
            if( p_dec->fmt_in.psz_language )
            {
                free( p_owner->sout.psz_language );
                p_owner->sout.psz_language =
                    strdup( p_dec->fmt_in.psz_language );
            }

            p_owner->p_sout_input =
                sout_InputNew( p_owner->p_sout,
                               &p_owner->sout );

            if( p_owner->p_sout_input == NULL )
            {
                msg_Err( p_dec, "cannot create packetizer output (%4.4s)",
                         (char *)&p_owner->sout.i_codec );
                p_dec->b_error = true;

                while( p_sout_block )
                {
                    block_t *p_next = p_sout_block->p_next;
                    block_Release( p_sout_block );
                    p_sout_block = p_next;
                }
                break;
            }
        }

        while( p_sout_block )
        {
            block_t *p_next = p_sout_block->p_next;

            p_sout_block->p_next = NULL;

            DecoderPlaySout( p_dec, p_sout_block );

            p_sout_block = p_next;
        }
    }
}
#endif

/* This function process a video block
 */
static void DecoderProcessVideo( decoder_t *p_dec, block_t *p_block, bool b_flush )
{
    decoder_owner_sys_t *p_owner = (decoder_owner_sys_t *)p_dec->p_owner;

    if( p_owner->p_packetizer )
    {
        block_t *p_packetized_block;
        decoder_t *p_packetizer = p_owner->p_packetizer;

        while( (p_packetized_block =
                p_packetizer->pf_packetize( p_packetizer, p_block ? &p_block : NULL )) )
        {
            if( p_packetizer->fmt_out.i_extra && !p_dec->fmt_in.i_extra )
            {
                es_format_Clean( &p_dec->fmt_in );
                es_format_Copy( &p_dec->fmt_in, &p_packetizer->fmt_out );
            }
            if( p_packetizer->pf_get_cc )
                DecoderGetCc( p_dec, p_packetizer );

            while( p_packetized_block )
            {
                block_t *p_next = p_packetized_block->p_next;
                p_packetized_block->p_next = NULL;

                DecoderDecodeVideo( p_dec, p_packetized_block );

                p_packetized_block = p_next;
            }
        }
        /* The packetizer does not output a block that tell the decoder to flush
         * do it ourself */
        if( b_flush )
        {
            block_t *p_null = DecoderBlockFlushNew();
            if( p_null )
                DecoderDecodeVideo( p_dec, p_null );
        }
    }
    else if( p_block )
    {
        DecoderDecodeVideo( p_dec, p_block );
    }

    if( b_flush && p_owner->p_vout )
        vout_Flush( p_owner->p_vout, VLC_TS_INVALID+1 );
}

/* This function process a audio block
 */
static void DecoderProcessAudio( decoder_t *p_dec, block_t *p_block, bool b_flush )
{
    decoder_owner_sys_t *p_owner = (decoder_owner_sys_t *)p_dec->p_owner;

    if( p_owner->p_packetizer )
    {
        block_t *p_packetized_block;
        decoder_t *p_packetizer = p_owner->p_packetizer;

        while( (p_packetized_block =
                p_packetizer->pf_packetize( p_packetizer, p_block ? &p_block : NULL )) )
        {
            if( p_packetizer->fmt_out.i_extra && !p_dec->fmt_in.i_extra )
            {
                es_format_Clean( &p_dec->fmt_in );
                es_format_Copy( &p_dec->fmt_in, &p_packetizer->fmt_out );
            }

            while( p_packetized_block )
            {
                block_t *p_next = p_packetized_block->p_next;
                p_packetized_block->p_next = NULL;

                DecoderDecodeAudio( p_dec, p_packetized_block );

                p_packetized_block = p_next;
            }
        }
        /* The packetizer does not output a block that tell the decoder to flush
         * do it ourself */
        if( b_flush )
        {
            block_t *p_null = DecoderBlockFlushNew();
            if( p_null )
                DecoderDecodeAudio( p_dec, p_null );
        }
    }
    else if( p_block )
    {
        DecoderDecodeAudio( p_dec, p_block );
    }

    if( b_flush && p_owner->p_aout )
        aout_DecFlush( p_owner->p_aout );
}

/* This function process a subtitle block
 */
static void DecoderProcessSpu( decoder_t *p_dec, block_t *p_block, bool b_flush )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    input_thread_t *p_input = p_owner->p_input;
    vout_thread_t *p_vout;
    subpicture_t *p_spu;

    while( (p_spu = p_dec->pf_decode_sub( p_dec, p_block ? &p_block : NULL ) ) )
    {
        if( p_input != NULL )
        {
            vlc_mutex_lock( &p_input->p->counters.counters_lock );
            stats_Update( p_input->p->counters.p_decoded_sub, 1, NULL );
            vlc_mutex_unlock( &p_input->p->counters.counters_lock );
        }

        p_vout = input_resource_HoldVout( p_owner->p_resource );
        if( p_vout && p_owner->p_spu_vout == p_vout )
        {
            /* Preroll does not work very well with subtitle */
            if( p_spu->i_start > VLC_TS_INVALID &&
                p_spu->i_start < p_owner->i_preroll_end &&
                ( p_spu->i_stop <= VLC_TS_INVALID || p_spu->i_stop < p_owner->i_preroll_end ) )
            {
                subpicture_Delete( p_spu );
            }
            else
            {
                DecoderPlaySpu( p_dec, p_spu );
            }
        }
        else
        {
            subpicture_Delete( p_spu );
        }
        if( p_vout )
            vlc_object_release( p_vout );
    }

    if( b_flush && p_owner->p_spu_vout )
    {
        p_vout = input_resource_HoldVout( p_owner->p_resource );

        if( p_vout && p_owner->p_spu_vout == p_vout )
            vout_FlushSubpictureChannel( p_vout, p_owner->i_spu_channel );

        if( p_vout )
            vlc_object_release( p_vout );
    }
}

/* */
static void DecoderProcessOnFlush( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_mutex_lock( &p_owner->lock );
    DecoderFlushBuffering( p_dec );

    if( p_owner->b_flushing )
    {
        p_owner->b_flushing = false;
        vlc_cond_signal( &p_owner->wait_acknowledge );
    }
    vlc_mutex_unlock( &p_owner->lock );
}

/**
 * Decode a block
 *
 * \param p_dec the decoder object
 * \param p_block the block to decode
 * \return VLC_SUCCESS or an error code
 */
static void DecoderProcess( decoder_t *p_dec, block_t *p_block )
{
    decoder_owner_sys_t *p_owner = (decoder_owner_sys_t *)p_dec->p_owner;
    const bool b_flush_request = p_block && (p_block->i_flags & BLOCK_FLAG_CORE_FLUSH);

    if( p_block && p_block->i_buffer <= 0 )
    {
        assert( !b_flush_request );
        block_Release( p_block );
        return;
    }

#ifdef ENABLE_SOUT
    if( p_owner->b_packetizer )
    {
        if( p_block )
            p_block->i_flags &= ~BLOCK_FLAG_CORE_PRIVATE_MASK;

        DecoderProcessSout( p_dec, p_block );
    }
    else
#endif
    {
        bool b_flush = false;

        if( p_block )
        {
            const bool b_flushing = p_owner->i_preroll_end == INT64_MAX;
            DecoderUpdatePreroll( &p_owner->i_preroll_end, p_block );

            b_flush = !b_flushing && b_flush_request;

            p_block->i_flags &= ~BLOCK_FLAG_CORE_PRIVATE_MASK;
        }

        if( p_dec->fmt_out.i_cat == AUDIO_ES )
        {
            DecoderProcessAudio( p_dec, p_block, b_flush );
        }
        else if( p_dec->fmt_out.i_cat == VIDEO_ES )
        {
            DecoderProcessVideo( p_dec, p_block, b_flush );
        }
        else if( p_dec->fmt_out.i_cat == SPU_ES )
        {
            DecoderProcessSpu( p_dec, p_block, b_flush );
        }
        else
        {
            msg_Err( p_dec, "unknown ES format" );
            p_dec->b_error = true;
        }
    }

    /* */
    if( b_flush_request )
        DecoderProcessOnFlush( p_dec );
}

static void DecoderError( decoder_t *p_dec, block_t *p_block )
{
    const bool b_flush_request = p_block && (p_block->i_flags & BLOCK_FLAG_CORE_FLUSH);

    /* */
    if( p_block )
        block_Release( p_block );

    if( b_flush_request )
        DecoderProcessOnFlush( p_dec );
}


/**
 * Destroys a decoder object
 *
 * \param p_dec the decoder object
 * \return nothing
 */
static void DeleteDecoder( decoder_t * p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    msg_Dbg( p_dec, "killing decoder fourcc `%4.4s', %u PES in FIFO",
             (char*)&p_dec->fmt_in.i_codec,
             (unsigned)block_FifoCount( p_owner->p_fifo ) );

    /* Free all packets still in the decoder fifo. */
    block_FifoEmpty( p_owner->p_fifo );
    block_FifoRelease( p_owner->p_fifo );

    /* */
    vlc_mutex_lock( &p_owner->lock );
    DecoderFlushBuffering( p_dec );
    vlc_mutex_unlock( &p_owner->lock );

    /* Cleanup */
    if( p_owner->p_aout )
    {
        /* TODO: REVISIT gap-less audio */
        aout_DecFlush( p_owner->p_aout );
        aout_DecDelete( p_owner->p_aout );
        input_resource_RequestAout( p_owner->p_resource, p_owner->p_aout );
        if( p_owner->p_input != NULL )
            input_SendEventAout( p_owner->p_input );
    }
    if( p_owner->p_vout )
    {
        /* Hack to make sure all the the pictures are freed by the decoder
         * and that the vout is not paused anymore */
        vout_Reset( p_owner->p_vout );

        /* */
        input_resource_RequestVout( p_owner->p_resource, p_owner->p_vout, NULL,
                                    0, true );
        if( p_owner->p_input != NULL )
            input_SendEventVout( p_owner->p_input );
    }

#ifdef ENABLE_SOUT
    if( p_owner->p_sout_input )
    {
        sout_InputDelete( p_owner->p_sout_input );
        es_format_Clean( &p_owner->sout );
    }
#endif

    if( p_dec->fmt_out.i_cat == SPU_ES )
    {
        vout_thread_t *p_vout;

        p_vout = input_resource_HoldVout( p_owner->p_resource );
        if( p_vout )
        {
            if( p_owner->p_spu_vout == p_vout )
                vout_FlushSubpictureChannel( p_vout, p_owner->i_spu_channel );
            vlc_object_release( p_vout );
        }
    }

    es_format_Clean( &p_dec->fmt_in );
    es_format_Clean( &p_dec->fmt_out );
    if( p_dec->p_description )
        vlc_meta_Delete( p_dec->p_description );
    es_format_Clean( &p_owner->fmt_description );
    if( p_owner->p_description )
        vlc_meta_Delete( p_owner->p_description );

    if( p_owner->p_packetizer )
    {
        module_unneed( p_owner->p_packetizer,
                       p_owner->p_packetizer->p_module );
        es_format_Clean( &p_owner->p_packetizer->fmt_in );
        es_format_Clean( &p_owner->p_packetizer->fmt_out );
        if( p_owner->p_packetizer->p_description )
            vlc_meta_Delete( p_owner->p_packetizer->p_description );
        vlc_object_release( p_owner->p_packetizer );
    }

    vlc_cond_destroy( &p_owner->wait_acknowledge );
    vlc_cond_destroy( &p_owner->wait_request );
    vlc_mutex_destroy( &p_owner->lock );

    vlc_object_release( p_dec );

    free( p_owner );
}

/*****************************************************************************
 * Buffers allocation callbacks for the decoders
 *****************************************************************************/
static void DecoderUpdateFormatLocked( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    vlc_assert_locked( &p_owner->lock );

    p_owner->b_fmt_description = true;

    /* Copy es_format */
    es_format_Clean( &p_owner->fmt_description );
    es_format_Copy( &p_owner->fmt_description, &p_dec->fmt_out );

    /* Move p_description */
    if( p_owner->p_description && p_dec->p_description )
        vlc_meta_Delete( p_owner->p_description );
    p_owner->p_description = p_dec->p_description;
    p_dec->p_description = NULL;
}
static vout_thread_t *aout_request_vout( void *p_private,
                                         vout_thread_t *p_vout, video_format_t *p_fmt, bool b_recyle )
{
    decoder_t *p_dec = p_private;
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    input_thread_t *p_input = p_owner->p_input;

    p_vout = input_resource_RequestVout( p_owner->p_resource, p_vout, p_fmt, 1,
                                         b_recyle );
    if( p_input != NULL )
        input_SendEventVout( p_input );

    return p_vout;
}

static aout_buffer_t *aout_new_buffer( decoder_t *p_dec, int i_samples )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    aout_buffer_t *p_buffer;

    if( p_owner->p_aout
     && !AOUT_FMTS_IDENTICAL(&p_dec->fmt_out.audio, &p_owner->audio) )
    {
        audio_output_t *p_aout = p_owner->p_aout;

        /* Parameters changed, restart the aout */
        vlc_mutex_lock( &p_owner->lock );

        DecoderFlushBuffering( p_dec );

        aout_DecDelete( p_owner->p_aout );
        p_owner->p_aout = NULL;

        vlc_mutex_unlock( &p_owner->lock );
        input_resource_RequestAout( p_owner->p_resource, p_aout );
    }

    if( p_owner->p_aout == NULL )
    {
        const int i_force_dolby = var_InheritInteger( p_dec, "force-dolby-surround" );
        audio_sample_format_t format;
        audio_output_t *p_aout;
        aout_request_vout_t request_vout;

        p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;
        p_owner->audio = p_dec->fmt_out.audio;

        memcpy( &format, &p_owner->audio, sizeof( audio_sample_format_t ) );
        if( i_force_dolby &&
            (format.i_original_channels&AOUT_CHAN_PHYSMASK) ==
                (AOUT_CHAN_LEFT|AOUT_CHAN_RIGHT) )
        {
            if( i_force_dolby == 1 )
            {
                format.i_original_channels = format.i_original_channels |
                                             AOUT_CHAN_DOLBYSTEREO;
            }
            else /* i_force_dolby == 2 */
            {
                format.i_original_channels = format.i_original_channels &
                                             ~AOUT_CHAN_DOLBYSTEREO;
            }
        }

        request_vout.pf_request_vout = aout_request_vout;
        request_vout.p_private = p_dec;

        assert( p_owner->p_aout == NULL );
        p_aout = input_resource_RequestAout( p_owner->p_resource, NULL );
        if( p_aout )
        {
            aout_FormatPrepare( &format );
            if( aout_DecNew( p_aout, &format,
                             &p_dec->fmt_out.audio_replay_gain,
                             &request_vout ) )
            {
                input_resource_RequestAout( p_owner->p_resource, p_aout );
                p_aout = NULL;
            }
        }

        vlc_mutex_lock( &p_owner->lock );

        p_owner->p_aout = p_aout;
        DecoderUpdateFormatLocked( p_dec );
        if( unlikely(p_owner->b_paused) ) /* fake pause if needed */
            aout_DecChangePause( p_aout, true, mdate() );

        vlc_mutex_unlock( &p_owner->lock );

        if( p_owner->p_input != NULL )
            input_SendEventAout( p_owner->p_input );

        if( p_aout == NULL )
        {
            msg_Err( p_dec, "failed to create audio output" );
            p_dec->b_error = true;
            return NULL;
        }
        p_dec->fmt_out.audio.i_bytes_per_frame =
            p_owner->audio.i_bytes_per_frame;
    }

    p_buffer = aout_DecNewBuffer( p_owner->p_aout, i_samples );

    return p_buffer;
}

static picture_t *vout_new_buffer( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    if( p_owner->p_vout == NULL ||
        p_dec->fmt_out.video.i_width != p_owner->video.i_width ||
        p_dec->fmt_out.video.i_height != p_owner->video.i_height ||
        p_dec->fmt_out.video.i_visible_width != p_owner->video.i_visible_width ||
        p_dec->fmt_out.video.i_visible_height != p_owner->video.i_visible_height ||
        p_dec->fmt_out.video.i_x_offset != p_owner->video.i_x_offset  ||
        p_dec->fmt_out.video.i_y_offset != p_owner->video.i_y_offset  ||
        p_dec->fmt_out.i_codec != p_owner->video.i_chroma ||
        (int64_t)p_dec->fmt_out.video.i_sar_num * p_owner->video.i_sar_den !=
        (int64_t)p_dec->fmt_out.video.i_sar_den * p_owner->video.i_sar_num )
    {
        vout_thread_t *p_vout;

        if( !p_dec->fmt_out.video.i_width ||
            !p_dec->fmt_out.video.i_height )
        {
            /* Can't create a new vout without display size */
            return NULL;
        }

        video_format_t fmt = p_dec->fmt_out.video;
        fmt.i_chroma = p_dec->fmt_out.i_codec;
        p_owner->video = fmt;

        if( vlc_fourcc_IsYUV( fmt.i_chroma ) )
        {
            const vlc_chroma_description_t *dsc = vlc_fourcc_GetChromaDescription( fmt.i_chroma );
            for( unsigned int i = 0; dsc && i < dsc->plane_count; i++ )
            {
                while( fmt.i_width % dsc->p[i].w.den )
                    fmt.i_width++;
                while( fmt.i_height % dsc->p[i].h.den )
                    fmt.i_height++;
            }
        }

        if( !fmt.i_visible_width || !fmt.i_visible_height )
        {
            if( p_dec->fmt_in.video.i_visible_width &&
                p_dec->fmt_in.video.i_visible_height )
            {
                fmt.i_visible_width  = p_dec->fmt_in.video.i_visible_width;
                fmt.i_visible_height = p_dec->fmt_in.video.i_visible_height;
                fmt.i_x_offset       = p_dec->fmt_in.video.i_x_offset;
                fmt.i_y_offset       = p_dec->fmt_in.video.i_y_offset;
            }
            else
            {
                fmt.i_visible_width  = fmt.i_width;
                fmt.i_visible_height = fmt.i_height;
                fmt.i_x_offset       = 0;
                fmt.i_y_offset       = 0;
            }
        }

        if( fmt.i_visible_height == 1088 &&
            var_CreateGetBool( p_dec, "hdtv-fix" ) )
        {
            fmt.i_visible_height = 1080;
            if( !(fmt.i_sar_num % 136))
            {
                fmt.i_sar_num *= 135;
                fmt.i_sar_den *= 136;
            }
            msg_Warn( p_dec, "Fixing broken HDTV stream (display_height=1088)");
        }

        if( !fmt.i_sar_num || !fmt.i_sar_den )
        {
            fmt.i_sar_num = 1;
            fmt.i_sar_den = 1;
        }

        vlc_ureduce( &fmt.i_sar_num, &fmt.i_sar_den,
                     fmt.i_sar_num, fmt.i_sar_den, 50000 );

        vlc_mutex_lock( &p_owner->lock );

        DecoderFlushBuffering( p_dec );

        p_vout = p_owner->p_vout;
        p_owner->p_vout = NULL;
        vlc_mutex_unlock( &p_owner->lock );

        unsigned dpb_size;
        switch( p_dec->fmt_in.i_codec )
        {
        case VLC_CODEC_H264:
        case VLC_CODEC_DIRAC: /* FIXME valid ? */
            dpb_size = 18;
            break;
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
        p_vout = input_resource_RequestVout( p_owner->p_resource,
                                             p_vout, &fmt,
                                             dpb_size +
                                             p_dec->i_extra_picture_buffers +
                                             1 + DECODER_MAX_BUFFERING_COUNT,
                                             true );
        vlc_mutex_lock( &p_owner->lock );
        p_owner->p_vout = p_vout;

        DecoderUpdateFormatLocked( p_dec );

        vlc_mutex_unlock( &p_owner->lock );

        if( p_owner->p_input != NULL )
            input_SendEventVout( p_owner->p_input );
        if( p_vout == NULL )
        {
            msg_Err( p_dec, "failed to create video output" );
            p_dec->b_error = true;
            return NULL;
        }
    }

    /* Get a new picture
     */
    for( ;; )
    {
        if( DecoderIsExitRequested( p_dec ) || p_dec->b_error )
            return NULL;

        picture_t *p_picture = vout_GetPicture( p_owner->p_vout );
        if( p_picture )
            return p_picture;

        if( DecoderIsFlushing( p_dec ) )
            return NULL;

        /* */
        DecoderSignalBuffering( p_dec, true );

        /* Check the decoder doesn't leak pictures */
        vout_FixLeaks( p_owner->p_vout );

        /* FIXME add a vout_WaitPictureAvailable (timedwait) */
        msleep( VOUT_OUTMEM_SLEEP );
    }
}

static void vout_del_buffer( decoder_t *p_dec, picture_t *p_pic )
{
    vout_ReleasePicture( p_dec->p_owner->p_vout, p_pic );
}

static void vout_link_picture( decoder_t *p_dec, picture_t *p_pic )
{
    vout_HoldPicture( p_dec->p_owner->p_vout, p_pic );
}

static void vout_unlink_picture( decoder_t *p_dec, picture_t *p_pic )
{
    vout_ReleasePicture( p_dec->p_owner->p_vout, p_pic );
}

static subpicture_t *spu_new_buffer( decoder_t *p_dec,
                                     const subpicture_updater_t *p_updater )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    vout_thread_t *p_vout = NULL;
    subpicture_t *p_subpic;
    int i_attempts = 30;

    while( i_attempts-- )
    {
        if( DecoderIsExitRequested( p_dec ) || p_dec->b_error )
            break;

        p_vout = input_resource_HoldVout( p_owner->p_resource );
        if( p_vout )
            break;

        msleep( DECODER_SPU_VOUT_WAIT_DURATION );
    }

    if( !p_vout )
    {
        msg_Warn( p_dec, "no vout found, dropping subpicture" );
        return NULL;
    }

    if( p_owner->p_spu_vout != p_vout )
    {
        vlc_mutex_lock( &p_owner->lock );

        DecoderFlushBuffering( p_dec );

        vlc_mutex_unlock( &p_owner->lock );

        p_owner->i_spu_channel = vout_RegisterSubpictureChannel( p_vout );
        p_owner->i_spu_order = 0;
        p_owner->p_spu_vout = p_vout;
    }

    p_subpic = subpicture_New( p_updater );
    if( p_subpic )
    {
        p_subpic->i_channel = p_owner->i_spu_channel;
        p_subpic->i_order = p_owner->i_spu_order++;
        p_subpic->b_subtitle = true;
    }

    vlc_object_release( p_vout );

    return p_subpic;
}

static void spu_del_buffer( decoder_t *p_dec, subpicture_t *p_subpic )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    vout_thread_t *p_vout = NULL;

    p_vout = input_resource_HoldVout( p_owner->p_resource );
    if( !p_vout || p_owner->p_spu_vout != p_vout )
    {
        if( p_vout )
            vlc_object_release( p_vout );
        msg_Warn( p_dec, "no vout found, leaking subpicture" );
        return;
    }

    subpicture_Delete( p_subpic );

    vlc_object_release( p_vout );
}

