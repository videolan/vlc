/*****************************************************************************
 * decoder.c: Functions for the management of decoders
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
#include <vlc_osd.h>

#include <vlc_interface.h>
#include "audio_output/aout_internal.h"
#include "stream_output/stream_output.h"
#include "input_internal.h"

static decoder_t * CreateDecoder( input_thread_t *, es_format_t *, int, sout_instance_t *p_sout );
static void        DeleteDecoder( decoder_t * );

static void*        DecoderThread( vlc_object_t * );
static int         DecoderDecode( decoder_t * p_dec, block_t *p_block );

/* Buffers allocation callbacks for the decoders */
static aout_buffer_t *aout_new_buffer( decoder_t *, int );
static void aout_del_buffer( decoder_t *, aout_buffer_t * );

static picture_t *vout_new_buffer( decoder_t * );
static void vout_del_buffer( decoder_t *, picture_t * );
static void vout_link_picture( decoder_t *, picture_t * );
static void vout_unlink_picture( decoder_t *, picture_t * );

static subpicture_t *spu_new_buffer( decoder_t * );
static void spu_del_buffer( decoder_t *, subpicture_t * );

static es_format_t null_es_format;

struct decoder_owner_sys_t
{
    bool      b_own_thread;

    int64_t         i_preroll_end;

    input_thread_t  *p_input;
    input_clock_t   *p_clock;

    aout_instance_t *p_aout;
    aout_input_t    *p_aout_input;

    vout_thread_t   *p_vout;

    vout_thread_t   *p_spu_vout;
    int              i_spu_channel;
    int64_t          i_spu_order;

    sout_instance_t         *p_sout;
    sout_packetizer_input_t *p_sout_input;

    /* Some decoders require already packetized data (ie. not truncated) */
    decoder_t *p_packetizer;

    /* Current format in use by the output */
    video_format_t video;
    audio_format_t audio;
    es_format_t    sout;

    /* fifo */
    block_fifo_t *p_fifo;

    /* CC */
    bool b_cc_supported;
    vlc_mutex_t lock_cc;
    bool pb_cc_present[4];
    decoder_t *pp_cc[4];
};

/* */
static void DecoderUnsupportedCodec( decoder_t *p_dec, vlc_fourcc_t codec )
{
    msg_Err( p_dec, "no suitable decoder module for fourcc `%4.4s'.\n"
             "VLC probably does not support this sound or video format.",
             (char*)&codec );
    intf_UserFatal( p_dec, false, _("No suitable decoder module"), 
                    _("VLC does not support the audio or video format \"%4.4s\". "
                      "Unfortunately there is no way for you to fix this."), (char*)&codec );
}

/* decoder_GetInputAttachment:
 */
input_attachment_t *decoder_GetInputAttachment( decoder_t *p_dec,
                                                const char *psz_name )
{
    input_attachment_t *p_attachment;
    if( input_Control( p_dec->p_owner->p_input, INPUT_GET_ATTACHMENT, &p_attachment, psz_name ) )
        return NULL;
    return p_attachment;
}
/* decoder_GetInputAttachments:
 */
int decoder_GetInputAttachments( decoder_t *p_dec,
                                 input_attachment_t ***ppp_attachment,
                                 int *pi_attachment )
{
    return input_Control( p_dec->p_owner->p_input, INPUT_GET_ATTACHMENTS,
                          ppp_attachment, pi_attachment );
}
/* decoder_GetDisplayDate:
 */
mtime_t decoder_GetDisplayDate( decoder_t *p_dec, mtime_t i_ts )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    assert( p_owner->p_clock );
    return input_clock_GetTS( p_owner->p_clock, p_owner->p_input->i_pts_delay, i_ts );
}
/* decoder_GetDisplayRate:
 */
int decoder_GetDisplayRate( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    assert( p_owner->p_clock );
    return input_clock_GetRate( p_owner->p_clock );
}

/**
 * Spawns a new decoder thread
 *
 * \param p_input the input thread
 * \param p_es the es descriptor
 * \return the spawned decoder object
 */
decoder_t *input_DecoderNew( input_thread_t *p_input,
                             es_format_t *fmt, input_clock_t *p_clock, sout_instance_t *p_sout  )
{
    decoder_t   *p_dec = NULL;
    vlc_value_t val;

#ifndef ENABLE_SOUT
    (void)b_force_decoder;
#else
    /* If we are in sout mode, search for packetizer module */
    if( p_sout )
    {
        /* Create the decoder configuration structure */
        p_dec = CreateDecoder( p_input, fmt, VLC_OBJECT_PACKETIZER, p_sout );
        if( p_dec == NULL )
        {
            msg_Err( p_input, "could not create packetizer" );
            intf_UserFatal( p_input, false, _("Streaming / Transcoding failed"),
                            _("VLC could not open the packetizer module.") );
            return NULL;
        }
    }
    else
#endif
    {
        /* Create the decoder configuration structure */
        p_dec = CreateDecoder( p_input, fmt, VLC_OBJECT_DECODER, p_sout );
        if( p_dec == NULL )
        {
            msg_Err( p_input, "could not create decoder" );
            intf_UserFatal( p_input, false, _("Streaming / Transcoding failed"),
                            _("VLC could not open the decoder module.") );
            return NULL;
        }
    }

    if( !p_dec->p_module )
    {
        DecoderUnsupportedCodec( p_dec, fmt->i_codec );

        DeleteDecoder( p_dec );
        vlc_object_release( p_dec );
        return NULL;
    }

    p_dec->p_owner->p_clock = p_clock;

    if( p_sout && p_sout == p_input->p->p_sout && p_input->p->input.b_can_pace_control )
    {
        msg_Dbg( p_input, "stream out mode -> no decoder thread" );
        p_dec->p_owner->b_own_thread = false;
    }
    else
    {
        var_Get( p_input, "minimize-threads", &val );
        p_dec->p_owner->b_own_thread = !val.b_bool;
    }

    if( p_dec->p_owner->b_own_thread )
    {
        int i_priority;
        if( fmt->i_cat == AUDIO_ES )
            i_priority = VLC_THREAD_PRIORITY_AUDIO;
        else
            i_priority = VLC_THREAD_PRIORITY_VIDEO;

        /* Spawn the decoder thread */
        if( vlc_thread_create( p_dec, "decoder", DecoderThread,
                               i_priority, false ) )
        {
            msg_Err( p_dec, "cannot spawn decoder thread" );
            module_unneed( p_dec, p_dec->p_module );
            DeleteDecoder( p_dec );
            vlc_object_release( p_dec );
            return NULL;
        }
    }

    return p_dec;
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
    vlc_object_kill( p_dec );

    if( p_dec->p_owner->b_own_thread )
    {
        /* Make sure the thread leaves the function by
         * sending it an empty block. */
        block_t *p_block = block_New( p_dec, 0 );
        input_DecoderDecode( p_dec, p_block );

        vlc_thread_join( p_dec );

        /* Don't module_unneed() here because of the dll loader that wants
         * close() in the same thread than open()/decode() */
    }
    else
    {
        /* Flush */
        input_DecoderDecode( p_dec, NULL );

        module_unneed( p_dec, p_dec->p_module );
    }

    /* */
    if( p_dec->p_owner->b_cc_supported )
    {
        int i;
        for( i = 0; i < 4; i++ )
            input_DecoderSetCcState( p_dec, false, i );
    }

    /* Delete decoder configuration */
    DeleteDecoder( p_dec );

    /* Delete the decoder */
    vlc_object_release( p_dec );
}

/**
 * Put a block_t in the decoder's fifo.
 *
 * \param p_dec the decoder object
 * \param p_block the data block
 */
void input_DecoderDecode( decoder_t * p_dec, block_t *p_block )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    if( p_owner->b_own_thread )
    {
        if( p_owner->p_input->p->b_out_pace_control )
        {
            /* FIXME !!!!! */
            while( !p_dec->b_die && !p_dec->b_error &&
                   block_FifoCount( p_owner->p_fifo ) > 10 )
            {
                msleep( 1000 );
            }
        }
        else if( block_FifoSize( p_owner->p_fifo ) > 50000000 /* 50 MB */ )
        {
            /* FIXME: ideally we would check the time amount of data
             * in the fifo instead of its size. */
            msg_Warn( p_dec, "decoder/packetizer fifo full (data not "
                      "consumed quickly enough), resetting fifo!" );
            block_FifoEmpty( p_owner->p_fifo );
        }

        block_FifoPut( p_owner->p_fifo, p_block );
    }
    else
    {
        if( p_dec->b_error || ( p_block && p_block->i_buffer <= 0 ) )
        {
            if( p_block )
                block_Release( p_block );
        }
        else
        {
            DecoderDecode( p_dec, p_block );
        }
    }
}

void input_DecoderDiscontinuity( decoder_t * p_dec, bool b_flush )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    block_t *p_null;

    /* Empty the fifo */
    if( p_owner->b_own_thread && b_flush )
        block_FifoEmpty( p_owner->p_fifo );

    /* Send a special block */
    p_null = block_New( p_dec, 128 );
    p_null->i_flags |= BLOCK_FLAG_DISCONTINUITY;
    if( b_flush && p_dec->fmt_in.i_cat == SPU_ES )
        p_null->i_flags |= BLOCK_FLAG_CORE_FLUSH;
    /* FIXME check for p_packetizer or b_packitized from es_format_t of input ? */
    if( p_owner->p_packetizer && b_flush )
        p_null->i_flags |= BLOCK_FLAG_CORRUPTED;
    memset( p_null->p_buffer, 0, p_null->i_buffer );

    input_DecoderDecode( p_dec, p_null );
}

bool input_DecoderEmpty( decoder_t * p_dec )
{
    if( p_dec->p_owner->b_own_thread &&
        block_FifoCount( p_dec->p_owner->p_fifo ) > 0 )
    {
        return false;
    }
    return true;
}

void input_DecoderIsCcPresent( decoder_t *p_dec, bool pb_present[4] )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    int i;

    vlc_mutex_lock( &p_owner->lock_cc );
    for( i = 0; i < 4; i++ )
        pb_present[i] =  p_owner->pb_cc_present[i];
    vlc_mutex_unlock( &p_owner->lock_cc );
}
int input_DecoderSetCcState( decoder_t *p_dec, bool b_decode, int i_channel )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    //msg_Warn( p_dec, "input_DecoderSetCcState: %d @%d", b_decode, i_channel );

    if( i_channel < 0 || i_channel >= 4 || !p_owner->pb_cc_present[i_channel] )
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
        p_cc = CreateDecoder( p_owner->p_input, &fmt, VLC_OBJECT_DECODER, p_owner->p_sout );
        if( !p_cc )
        {
            msg_Err( p_dec, "could not create decoder" );
            intf_UserFatal( p_dec, false, _("Streaming / Transcoding failed"),
                            _("VLC could not open the decoder module.") );
            return VLC_EGENERIC;
        }
        else if( !p_cc->p_module )
        {
            DecoderUnsupportedCodec( p_dec, fcc[i_channel] );
            DeleteDecoder( p_cc );
            vlc_object_release( p_cc );
            return VLC_EGENERIC;
        }
        p_cc->p_owner->p_clock = p_owner->p_clock;

        vlc_mutex_lock( &p_owner->lock_cc );
        p_owner->pp_cc[i_channel] = p_cc;
        vlc_mutex_unlock( &p_owner->lock_cc );
    }
    else
    {
        decoder_t *p_cc;

        vlc_mutex_lock( &p_owner->lock_cc );
        p_cc = p_owner->pp_cc[i_channel];
        p_owner->pp_cc[i_channel] = NULL;
        vlc_mutex_unlock( &p_owner->lock_cc );

        if( p_cc )
        {
            vlc_object_kill( p_cc );
            module_unneed( p_cc, p_cc->p_module );
            DeleteDecoder( p_cc );
            vlc_object_release( p_cc );
        }
    }
    return VLC_SUCCESS;
}
int input_DecoderGetCcState( decoder_t *p_dec, bool *pb_decode, int i_channel )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    *pb_decode = false;
    if( i_channel < 0 || i_channel >= 4 || !p_owner->pb_cc_present[i_channel] )
        return VLC_EGENERIC;

    vlc_mutex_lock( &p_owner->lock_cc );
    *pb_decode = p_owner->pp_cc[i_channel] != NULL;
    vlc_mutex_unlock( &p_owner->lock_cc );
    return VLC_EGENERIC;
}

/**
 * Create a decoder object
 *
 * \param p_input the input thread
 * \param p_es the es descriptor
 * \param i_object_type Object type as define in include/vlc_objects.h
 * \return the decoder object
 */
static decoder_t * CreateDecoder( input_thread_t *p_input,
                                  es_format_t *fmt, int i_object_type, sout_instance_t *p_sout )
{
    decoder_t *p_dec;
    decoder_owner_sys_t *p_owner;
    int i;

    p_dec = vlc_object_create( p_input, i_object_type );
    if( p_dec == NULL )
        return NULL;

    p_dec->pf_decode_audio = NULL;
    p_dec->pf_decode_video = NULL;
    p_dec->pf_decode_sub = NULL;
    p_dec->pf_get_cc = NULL;
    p_dec->pf_packetize = NULL;

    /* Initialize the decoder fifo */
    p_dec->p_module = NULL;

    memset( &null_es_format, 0, sizeof(es_format_t) );
    es_format_Copy( &p_dec->fmt_in, fmt );
    es_format_Copy( &p_dec->fmt_out, &null_es_format );

    /* Allocate our private structure for the decoder */
    p_dec->p_owner = p_owner = malloc( sizeof( decoder_owner_sys_t ) );
    if( p_dec->p_owner == NULL )
    {
        vlc_object_release( p_dec );
        return NULL;
    }
    p_dec->p_owner->b_own_thread = true;
    p_dec->p_owner->i_preroll_end = -1;
    p_dec->p_owner->p_input = p_input;
    p_dec->p_owner->p_aout = NULL;
    p_dec->p_owner->p_aout_input = NULL;
    p_dec->p_owner->p_vout = NULL;
    p_dec->p_owner->p_spu_vout = NULL;
    p_dec->p_owner->i_spu_channel = 0;
    p_dec->p_owner->i_spu_order = 0;
    p_dec->p_owner->p_sout = p_sout;
    p_dec->p_owner->p_sout_input = NULL;
    p_dec->p_owner->p_packetizer = NULL;

    /* decoder fifo */
    if( ( p_dec->p_owner->p_fifo = block_FifoNew() ) == NULL )
    {
        free( p_dec->p_owner );
        vlc_object_release( p_dec );
        return NULL;
    }

    /* Set buffers allocation callbacks for the decoders */
    p_dec->pf_aout_buffer_new = aout_new_buffer;
    p_dec->pf_aout_buffer_del = aout_del_buffer;
    p_dec->pf_vout_buffer_new = vout_new_buffer;
    p_dec->pf_vout_buffer_del = vout_del_buffer;
    p_dec->pf_picture_link    = vout_link_picture;
    p_dec->pf_picture_unlink  = vout_unlink_picture;
    p_dec->pf_spu_buffer_new  = spu_new_buffer;
    p_dec->pf_spu_buffer_del  = spu_del_buffer;

    vlc_object_attach( p_dec, p_input );

    /* Find a suitable decoder/packetizer module */
    if( i_object_type == VLC_OBJECT_DECODER )
        p_dec->p_module = module_need( p_dec, "decoder", "$codec", 0 );
    else
        p_dec->p_module = module_need( p_dec, "packetizer", "$packetizer", 0 );

    /* Check if decoder requires already packetized data */
    if( i_object_type == VLC_OBJECT_DECODER &&
        p_dec->b_need_packetized && !p_dec->fmt_in.b_packetized )
    {
        p_dec->p_owner->p_packetizer =
            vlc_object_create( p_input, VLC_OBJECT_PACKETIZER );
        if( p_dec->p_owner->p_packetizer )
        {
            es_format_Copy( &p_dec->p_owner->p_packetizer->fmt_in,
                            &p_dec->fmt_in );

            es_format_Copy( &p_dec->p_owner->p_packetizer->fmt_out,
                            &null_es_format );

            vlc_object_attach( p_dec->p_owner->p_packetizer, p_input );

            p_dec->p_owner->p_packetizer->p_module =
                module_need( p_dec->p_owner->p_packetizer,
                             "packetizer", "$packetizer", 0 );

            if( !p_dec->p_owner->p_packetizer->p_module )
            {
                es_format_Clean( &p_dec->p_owner->p_packetizer->fmt_in );
                vlc_object_detach( p_dec->p_owner->p_packetizer );
                vlc_object_release( p_dec->p_owner->p_packetizer );
            }
        }
    }

    /* Copy ourself the input replay gain */
    if( fmt->i_cat == AUDIO_ES )
    {
        for( i = 0; i < AUDIO_REPLAY_GAIN_MAX; i++ )
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
    p_owner->b_cc_supported = false;
    if( i_object_type == VLC_OBJECT_DECODER )
    {
        if( p_owner->p_packetizer && p_owner->p_packetizer->pf_get_cc )
            p_owner->b_cc_supported = true;
        if( p_dec->pf_get_cc )
            p_owner->b_cc_supported = true;
    }

    vlc_mutex_init( &p_owner->lock_cc );
    for( i = 0; i < 4; i++ )
    {
        p_owner->pb_cc_present[i] = false;
        p_owner->pp_cc[i] = NULL;
    }
    return p_dec;
}

/**
 * The decoding main loop
 *
 * \param p_dec the decoder
 */
static void* DecoderThread( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    block_t *p_block;
    int canc = vlc_savecancel();

    /* The decoder's main loop */
    while( !p_dec->b_die && !p_dec->b_error )
    {
        if( ( p_block = block_FifoGet( p_owner->p_fifo ) ) == NULL )
        {
            p_dec->b_error = 1;
            break;
        }
        if( DecoderDecode( p_dec, p_block ) != VLC_SUCCESS )
        {
            break;
        }
    }

    while( !p_dec->b_die )
    {
        /* Trash all received PES packets */
        p_block = block_FifoGet( p_owner->p_fifo );
        if( p_block )
            block_Release( p_block );
    }

    /* We do it here because of the dll loader that wants close() in the
     * same thread than open()/decode() */
    module_unneed( p_dec, p_dec->p_module );
    vlc_restorecancel( canc );
    return NULL;
}

static inline void DecoderUpdatePreroll( int64_t *pi_preroll, const block_t *p )
{
    if( p->i_flags & (BLOCK_FLAG_PREROLL|BLOCK_FLAG_DISCONTINUITY) )
        *pi_preroll = INT64_MAX;
    else if( p->i_pts > 0 )
        *pi_preroll = __MIN( *pi_preroll, p->i_pts );
    else if( p->i_dts > 0 )
        *pi_preroll = __MIN( *pi_preroll, p->i_dts );
}

static mtime_t DecoderTeletextFixTs( mtime_t i_ts, mtime_t i_ts_delay )
{
    mtime_t current_date = mdate();

    /* FIXME I don't really like that, es_out SHOULD do it using the video pts */
    if( !i_ts || i_ts > current_date + 10000000 || i_ts < current_date )
    {
        /* ETSI EN 300 472 Annex A : do not take into account the PTS
         * for teletext streams. */
        return current_date + 400000 + i_ts_delay;
    }
    return i_ts;
}

static void DecoderSoutBufferFixTs( block_t *p_block,
                                    input_clock_t *p_clock, mtime_t i_ts_delay,
                                    bool b_teletext )
{
    assert( p_clock );

    p_block->i_rate = input_clock_GetRate( p_clock );

    if( p_block->i_dts > 0 )
        p_block->i_dts = input_clock_GetTS( p_clock, i_ts_delay, p_block->i_dts );

    if( p_block->i_pts > 0 )
        p_block->i_pts = input_clock_GetTS( p_clock, i_ts_delay, p_block->i_pts );

    if( p_block->i_length > 0 )
        p_block->i_length = ( p_block->i_length * p_block->i_rate +
                                INPUT_RATE_DEFAULT-1 ) / INPUT_RATE_DEFAULT;

    if( b_teletext )
        p_block->i_pts = DecoderTeletextFixTs( p_block->i_pts, i_ts_delay );
}
static void DecoderAoutBufferFixTs( aout_buffer_t *p_buffer,
                                    input_clock_t *p_clock, mtime_t i_ts_delay )
{
    /* sout display module does not set clock */
    if( !p_clock )
        return;

    if( p_buffer->start_date )
        p_buffer->start_date = input_clock_GetTS( p_clock, i_ts_delay, p_buffer->start_date );

    if( p_buffer->end_date )
        p_buffer->end_date = input_clock_GetTS( p_clock, i_ts_delay, p_buffer->end_date );
}
static void DecoderVoutBufferFixTs( picture_t *p_picture,
                                    input_clock_t *p_clock, mtime_t i_ts_delay )
{
    /* sout display module does not set clock */
    if( !p_clock )
        return;

    if( p_picture->date )
        p_picture->date = input_clock_GetTS( p_clock, i_ts_delay, p_picture->date );
}
static void DecoderSpuBufferFixTs( subpicture_t *p_subpic,
                                   input_clock_t *p_clock, mtime_t i_ts_delay,
                                   bool b_teletext )
{
    bool b_ephemere = p_subpic->i_start == p_subpic->i_stop;

    /* sout display module does not set clock */
    if( !p_clock )
        return;

    if( p_subpic->i_start )
        p_subpic->i_start = input_clock_GetTS( p_clock, i_ts_delay, p_subpic->i_start );

    if( p_subpic->i_stop )
        p_subpic->i_stop = input_clock_GetTS( p_clock, i_ts_delay, p_subpic->i_stop );

    /* Do not create ephemere picture because of rounding errors */
    if( !b_ephemere && p_subpic->i_start == p_subpic->i_stop )
        p_subpic->i_stop++;

    if( b_teletext )
        p_subpic->i_start = DecoderTeletextFixTs( p_subpic->i_start, i_ts_delay );
}

static void DecoderDecodeAudio( decoder_t *p_dec, block_t *p_block )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    input_thread_t  *p_input = p_owner->p_input;
    input_clock_t   *p_clock = p_owner->p_clock;
    aout_buffer_t   *p_aout_buf;

    while( (p_aout_buf = p_dec->pf_decode_audio( p_dec, &p_block )) )
    {
        aout_instance_t *p_aout = p_owner->p_aout;
        aout_input_t    *p_aout_input = p_owner->p_aout_input;

        if( p_dec->b_die )
        {
            /* It prevent freezing VLC in case of broken decoder */
            aout_DecDeleteBuffer( p_aout, p_aout_input, p_aout_buf );
            if( p_block )
                block_Release( p_block );
            break;
        }
        vlc_mutex_lock( &p_input->p->counters.counters_lock );
        stats_UpdateInteger( p_dec, p_input->p->counters.p_decoded_audio, 1, NULL );
        vlc_mutex_unlock( &p_input->p->counters.counters_lock );

        if( p_aout_buf->start_date < p_owner->i_preroll_end )
        {
            aout_DecDeleteBuffer( p_aout, p_aout_input, p_aout_buf );
            continue;
        }

        if( p_owner->i_preroll_end > 0 )
        {
            /* FIXME TODO flush audio output (don't know how to do that) */
            msg_Dbg( p_dec, "End of audio preroll" );
            p_owner->i_preroll_end = -1;
        }

        const int i_rate = p_clock ? input_clock_GetRate( p_clock ) : p_block->i_rate;

        DecoderAoutBufferFixTs( p_aout_buf, p_clock, p_input->i_pts_delay );
        if( i_rate >= INPUT_RATE_DEFAULT/AOUT_MAX_INPUT_RATE &&
            i_rate <= INPUT_RATE_DEFAULT*AOUT_MAX_INPUT_RATE )
            aout_DecPlay( p_aout, p_aout_input, p_aout_buf, i_rate );
        else
            aout_DecDeleteBuffer( p_aout, p_aout_input, p_aout_buf );
    }
}
static void DecoderGetCc( decoder_t *p_dec, decoder_t *p_dec_cc )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    block_t *p_cc;
    bool pb_present[4];
    int i;
    int i_cc_decoder;

    assert( p_dec_cc->pf_get_cc != NULL );

    /* Do not try retreiving CC if not wanted (sout) or cannot be retreived */
    if( !p_owner->b_cc_supported )
        return;

    p_cc = p_dec_cc->pf_get_cc( p_dec_cc, pb_present );
    if( !p_cc )
        return;

    vlc_mutex_lock( &p_owner->lock_cc );
    for( i = 0, i_cc_decoder = 0; i < 4; i++ )
    {
        p_owner->pb_cc_present[i] |= pb_present[i];
        if( p_owner->pp_cc[i] )
            i_cc_decoder++;
    }

    for( i = 0; i < 4; i++ )
    {
        if( !p_owner->pp_cc[i] )
            continue;

        if( i_cc_decoder > 1 )
            DecoderDecode( p_owner->pp_cc[i], block_Duplicate( p_cc ) );
        else
            DecoderDecode( p_owner->pp_cc[i], p_cc );
        i_cc_decoder--;
    }
    vlc_mutex_unlock( &p_owner->lock_cc );
}
static void VoutDisplayedPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );

    if( p_pic->i_status == READY_PICTURE )
    {
        /* Grr cannot destroy ready picture by myself so be sure vout won't like it */
        p_pic->date = 1;
    }
    else if( p_pic->i_refcount > 0 )
    {
        p_pic->i_status = DISPLAYED_PICTURE;
    }
    else
    {
        p_pic->i_status = DESTROYED_PICTURE;
        picture_CleanupQuant( p_pic );
        p_vout->i_heap_size--;
    }

    vlc_mutex_unlock( &p_vout->picture_lock );
}
static void VoutFlushPicture( vout_thread_t *p_vout )
{
    int i;
    vlc_mutex_lock( &p_vout->picture_lock );
    for( i = 0; i < p_vout->render.i_pictures; i++ )
    {
        picture_t *p_pic = p_vout->render.pp_picture[i];

        if( p_pic->i_status == READY_PICTURE ||
            p_pic->i_status == DISPLAYED_PICTURE )
        {
            /* We cannot change picture status if it is in READY_PICTURE state,
             * Just make sure they won't be displayed */
            p_pic->date = 1;
        }
    }
    vlc_mutex_unlock( &p_vout->picture_lock );
}

#if 0
static void DecoderOptimizePtsDelay( decoder_t *p_dec )
{
    input_thread_t *p_input = p_dec->p_owner->p_input;
    vout_thread_t *p_vout = p_dec->p_owner->p_vout;
    input_thread_private_t *p_priv = p_input->p;

    picture_t *p_old = NULL;
    picture_t *p_young = NULL;
    int i;

    /* Enable with --auto-adjust-pts-delay */
    if( !p_priv->pts_adjust.b_auto_adjust )
        return;

    for( i = 0; i < I_RENDERPICTURES; i++ )
    {
        picture_t *p_pic = PP_RENDERPICTURE[i];

        if( p_pic->i_status != READY_PICTURE )
            continue;

        if( !p_old || p_pic->date < p_old->date )
            p_old = p_pic;
        if( !p_young || p_pic->date > p_young->date )
            p_young = p_pic;
    }

    if( !p_young || !p_old )
        return;

    /* Try to find if we can reduce the pts
     * This first draft is way to simple, and we can't say if the
     * algo will converge. It's also full of constants.
     * But this simple algo allows to reduce the latency
     * to the minimum.
     * The whole point of this, is to bypass the pts_delay set
     * by the access but also the delay arbitraly set by
     * the remote server.
     * Actually the remote server's muxer may set up a
     * pts<->dts delay in the muxed stream. That is
     * why we may end up in having a negative pts_delay,
     * to compensate that artificial delay. */
    const mtime_t i_buffer_length = p_young->date - p_old->date;
    int64_t i_pts_slide = 0;
    if( i_buffer_length < 10000 )
    {
        if( p_priv->pts_adjust.i_num_faulty > 10 )
        {
            i_pts_slide = __MAX(p_input->i_pts_delay *3 / 2, 10000);
            p_priv->pts_adjust.i_num_faulty = 0;
        }
        if( p_priv->pts_adjust.b_to_high )
        {
            p_priv->pts_adjust.b_to_high = !p_priv->pts_adjust.b_to_high;
            p_priv->pts_adjust.i_num_faulty = 0;
        }
        p_priv->pts_adjust.i_num_faulty++;
    }
    else if( i_buffer_length > 100000 )
    {
        if( p_priv->pts_adjust.i_num_faulty > 25 )
        {
            i_pts_slide = -i_buffer_length/2;
            p_priv->pts_adjust.i_num_faulty = 0;
        }
        if( p_priv->pts_adjust.b_to_high )
        {
            p_priv->pts_adjust.b_to_high = !p_priv->pts_adjust.b_to_high;
            p_priv->pts_adjust.i_num_faulty = 0;
        }
        p_priv->pts_adjust.i_num_faulty++;
    }
    if( i_pts_slide != 0 )
    {
        const mtime_t i_pts_delay_org = p_input->i_pts_delay;

        p_input->i_pts_delay += i_pts_slide;

        /* Don't play with the pts delay for more than -2<->3sec */
        if( p_input->i_pts_delay < -2000000 )
            p_input->i_pts_delay = -2000000;
        else if( p_input->i_pts_delay > 3000000 )
            p_input->i_pts_delay = 3000000;
        i_pts_slide = p_input->i_pts_delay - i_pts_delay_org;

        msg_Dbg( p_input, "Sliding the pts by %dms pts delay at %dms picture buffer was %dms",
            (int)i_pts_slide/1000, (int)p_input->i_pts_delay/1000, (int)i_buffer_length/1000);

        vlc_mutex_lock( &p_vout->picture_lock );
        /* Slide all the picture */
        for( i = 0; i < I_RENDERPICTURES; i++ )
            PP_RENDERPICTURE[i]->date += i_pts_slide;
        /* FIXME: slide aout/spu */
        vlc_mutex_unlock( &p_vout->picture_lock );
    }
}
#endif

static void DecoderDecodeVideo( decoder_t *p_dec, block_t *p_block )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    input_thread_t *p_input = p_owner->p_input;
    picture_t      *p_pic;

    while( (p_pic = p_dec->pf_decode_video( p_dec, &p_block )) )
    {
        vout_thread_t  *p_vout = p_owner->p_vout;
        if( p_dec->b_die )
        {
            /* It prevent freezing VLC in case of broken decoder */
            VoutDisplayedPicture( p_vout, p_pic );
            if( p_block )
                block_Release( p_block );
            break;
        }

        vlc_mutex_lock( &p_input->p->counters.counters_lock );
        stats_UpdateInteger( p_dec, p_input->p->counters.p_decoded_video, 1, NULL );
        vlc_mutex_unlock( &p_input->p->counters.counters_lock );

        if( p_pic->date < p_owner->i_preroll_end )
        {
            VoutDisplayedPicture( p_vout, p_pic );
            continue;
        }

        if( p_owner->i_preroll_end > 0 )
        {
            msg_Dbg( p_dec, "End of video preroll" );
            if( p_vout )
                VoutFlushPicture( p_vout );
            /* */
            p_owner->i_preroll_end = -1;
        }

        if( p_dec->pf_get_cc &&
            ( !p_owner->p_packetizer || !p_owner->p_packetizer->pf_get_cc ) )
            DecoderGetCc( p_dec, p_dec );

        DecoderVoutBufferFixTs( p_pic, p_owner->p_clock, p_input->i_pts_delay );

        vout_DatePicture( p_vout, p_pic, p_pic->date );

        /* Re-enable it but do it right this time */
        //DecoderOptimizePtsDelay( p_dec );

        vout_DisplayPicture( p_vout, p_pic );
    }
}

/**
 * Decode a block
 *
 * \param p_dec the decoder object
 * \param p_block the block to decode
 * \return VLC_SUCCESS or an error code
 */
static int DecoderDecode( decoder_t *p_dec, block_t *p_block )
{
    decoder_owner_sys_t *p_owner = (decoder_owner_sys_t *)p_dec->p_owner;
    const bool b_telx = p_dec->fmt_in.i_codec == VLC_FOURCC('t','e','l','x');

    if( p_block && p_block->i_buffer <= 0 )
    {
        block_Release( p_block );
        return VLC_SUCCESS;
    }

#ifdef ENABLE_SOUT
    if( p_dec->i_object_type == VLC_OBJECT_PACKETIZER )
    {
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
                    if( p_owner->sout.psz_language )
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

                DecoderSoutBufferFixTs( p_sout_block,
                                        p_owner->p_clock, p_owner->p_input->i_pts_delay, b_telx );

                sout_InputSendBuffer( p_owner->p_sout_input,
                                      p_sout_block );

                p_sout_block = p_next;
            }

            /* For now it's enough, as only sout impact on this flag */
            if( p_owner->p_sout->i_out_pace_nocontrol > 0 &&
                p_owner->p_input->p->b_out_pace_control )
            {
                msg_Dbg( p_dec, "switching to sync mode" );
                p_owner->p_input->p->b_out_pace_control = false;
            }
            else if( p_owner->p_sout->i_out_pace_nocontrol <= 0 &&
                     !p_owner->p_input->p->b_out_pace_control )
            {
                msg_Dbg( p_dec, "switching to async mode" );
                p_owner->p_input->p->b_out_pace_control = true;
            }
        }
    }
    else
#endif
    if( p_dec->fmt_in.i_cat == AUDIO_ES )
    {
        if( p_block )
            DecoderUpdatePreroll( &p_owner->i_preroll_end, p_block );

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
        }
        else if( p_block )
        {
            DecoderDecodeAudio( p_dec, p_block );
        }
    }
    else if( p_dec->fmt_in.i_cat == VIDEO_ES )
    {
        if( p_block )
            DecoderUpdatePreroll( &p_owner->i_preroll_end, p_block );

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
        }
        else if( p_block )
        {
            DecoderDecodeVideo( p_dec, p_block );
        }
    }
    else if( p_dec->fmt_in.i_cat == SPU_ES )
    {
        input_thread_t *p_input = p_owner->p_input;
        vout_thread_t *p_vout;
        subpicture_t *p_spu;
        bool b_flushing = p_owner->i_preroll_end == INT64_MAX;
        bool b_flush = false;

        if( p_block )
        {
            DecoderUpdatePreroll( &p_owner->i_preroll_end, p_block );
            b_flush = (p_block->i_flags & BLOCK_FLAG_CORE_FLUSH) != 0;
        }

        if( !b_flushing && b_flush && p_owner->p_spu_vout )
        {
            p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );

            if( p_vout && p_owner->p_spu_vout == p_vout )
                spu_Control( p_vout->p_spu, SPU_CHANNEL_CLEAR,
                             p_owner->i_spu_channel );

            if( p_vout )
                vlc_object_release( p_vout );
        }

        while( (p_spu = p_dec->pf_decode_sub( p_dec, p_block ? &p_block : NULL ) ) )
        {
            vlc_mutex_lock( &p_input->p->counters.counters_lock );
            stats_UpdateInteger( p_dec, p_input->p->counters.p_decoded_sub, 1, NULL );
            vlc_mutex_unlock( &p_input->p->counters.counters_lock );

            p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
            if( p_vout && p_owner->p_spu_vout == p_vout )
            {
                /* Prerool does not work very well with subtitle */
                if( p_spu->i_start < p_owner->i_preroll_end &&
                    ( p_spu->i_stop <= 0 || p_spu->i_stop < p_owner->i_preroll_end ) )
                {
                    subpicture_Delete( p_spu );
                }
                else
                {
                    DecoderSpuBufferFixTs( p_spu, p_owner->p_clock, p_input->i_pts_delay, b_telx );
                    spu_DisplaySubpicture( p_vout->p_spu, p_spu );
                }
            }
            else
            {
                msg_Warn( p_dec, "no vout found, leaking subpicture" );
            }
            if( p_vout )
                vlc_object_release( p_vout );
        }
    }
    else
    {
        msg_Err( p_dec, "unknown ES format" );
        p_dec->b_error = 1;
    }

    return p_dec->b_error ? VLC_EGENERIC : VLC_SUCCESS;
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

    /* Cleanup */
    if( p_owner->p_aout_input )
        aout_DecDelete( p_owner->p_aout, p_owner->p_aout_input );
    if( p_owner->p_aout )
    {
        vlc_object_release( p_owner->p_aout );
        p_owner->p_aout = NULL;
    }
    if( p_owner->p_vout )
    {
        int i_pic;

#define p_pic p_owner->p_vout->render.pp_picture[i_pic]
        /* Hack to make sure all the the pictures are freed by the decoder */
        for( i_pic = 0; i_pic < p_owner->p_vout->render.i_pictures;
             i_pic++ )
        {
            if( p_pic->i_status == RESERVED_PICTURE )
                vout_DestroyPicture( p_owner->p_vout, p_pic );
            if( p_pic->i_refcount > 0 )
                vout_UnlinkPicture( p_owner->p_vout, p_pic );
        }
#undef p_pic

        /* We are about to die. Reattach video output to p_vlc. */
        vout_Request( p_dec, p_owner->p_vout, NULL );
        var_SetBool( p_owner->p_input, "intf-change-vout", true );
    }

#ifdef ENABLE_SOUT
    if( p_owner->p_sout_input )
    {
        sout_InputDelete( p_owner->p_sout_input );
        es_format_Clean( &p_owner->sout );
    }
#endif

    if( p_dec->fmt_in.i_cat == SPU_ES )
    {
        vout_thread_t *p_vout;

        p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
        if( p_vout )
        {
            spu_Control( p_vout->p_spu, SPU_CHANNEL_CLEAR,
                         p_owner->i_spu_channel );
            vlc_object_release( p_vout );
        }
    }

    es_format_Clean( &p_dec->fmt_in );
    es_format_Clean( &p_dec->fmt_out );

    if( p_owner->p_packetizer )
    {
        module_unneed( p_owner->p_packetizer,
                       p_owner->p_packetizer->p_module );
        es_format_Clean( &p_owner->p_packetizer->fmt_in );
        es_format_Clean( &p_owner->p_packetizer->fmt_out );
        vlc_object_detach( p_owner->p_packetizer );
        vlc_object_release( p_owner->p_packetizer );
    }

    vlc_mutex_destroy( &p_owner->lock_cc );

    vlc_object_detach( p_dec );

    free( p_owner );
}

/*****************************************************************************
 * Buffers allocation callbacks for the decoders
 *****************************************************************************/
static aout_buffer_t *aout_new_buffer( decoder_t *p_dec, int i_samples )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    aout_buffer_t *p_buffer;

    if( p_owner->p_aout_input != NULL &&
        ( p_dec->fmt_out.audio.i_rate != p_owner->audio.i_rate ||
          p_dec->fmt_out.audio.i_original_channels !=
              p_owner->audio.i_original_channels ||
          p_dec->fmt_out.audio.i_bytes_per_frame !=
              p_owner->audio.i_bytes_per_frame ) )
    {
        /* Parameters changed, restart the aout */
        aout_DecDelete( p_owner->p_aout, p_owner->p_aout_input );
        p_owner->p_aout_input = NULL;
    }

    if( p_owner->p_aout_input == NULL )
    {
        audio_sample_format_t format;
        int i_force_dolby = config_GetInt( p_dec, "force-dolby-surround" );

        p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;
        p_owner->audio = p_dec->fmt_out.audio;

        memcpy( &format, &p_owner->audio, sizeof( audio_sample_format_t ) );
        if ( i_force_dolby && (format.i_original_channels&AOUT_CHAN_PHYSMASK)
                                    == (AOUT_CHAN_LEFT|AOUT_CHAN_RIGHT) )
        {
            if ( i_force_dolby == 1 )
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

        p_owner->p_aout_input =
            aout_DecNew( p_dec, &p_owner->p_aout, &format, &p_dec->fmt_out.audio_replay_gain );
        if( p_owner->p_aout_input == NULL )
        {
            msg_Err( p_dec, "failed to create audio output" );
            p_dec->b_error = true;
            return NULL;
        }
        p_dec->fmt_out.audio.i_bytes_per_frame =
            p_owner->audio.i_bytes_per_frame;
    }

    p_buffer = aout_DecNewBuffer( p_owner->p_aout_input, i_samples );

    return p_buffer;
}

static void aout_del_buffer( decoder_t *p_dec, aout_buffer_t *p_buffer )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;

    aout_DecDeleteBuffer( p_owner->p_aout,
                          p_owner->p_aout_input, p_buffer );
}


int vout_CountPictureAvailable( vout_thread_t *p_vout );

static picture_t *vout_new_buffer( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    picture_t *p_pic;

    if( p_owner->p_vout == NULL ||
        p_dec->fmt_out.video.i_width != p_owner->video.i_width ||
        p_dec->fmt_out.video.i_height != p_owner->video.i_height ||
        p_dec->fmt_out.video.i_chroma != p_owner->video.i_chroma ||
        p_dec->fmt_out.video.i_aspect != p_owner->video.i_aspect )
    {
        if( !p_dec->fmt_out.video.i_width ||
            !p_dec->fmt_out.video.i_height )
        {
            /* Can't create a new vout without display size */
            return NULL;
        }

        if( !p_dec->fmt_out.video.i_visible_width ||
            !p_dec->fmt_out.video.i_visible_height )
        {
            if( p_dec->fmt_in.video.i_visible_width &&
                p_dec->fmt_in.video.i_visible_height )
            {
                p_dec->fmt_out.video.i_visible_width =
                    p_dec->fmt_in.video.i_visible_width;
                p_dec->fmt_out.video.i_visible_height =
                    p_dec->fmt_in.video.i_visible_height;
            }
            else
            {
                p_dec->fmt_out.video.i_visible_width =
                    p_dec->fmt_out.video.i_width;
                p_dec->fmt_out.video.i_visible_height =
                    p_dec->fmt_out.video.i_height;
            }
        }

        if( p_dec->fmt_out.video.i_visible_height == 1088 &&
            var_CreateGetBool( p_dec, "hdtv-fix" ) )
        {
            p_dec->fmt_out.video.i_visible_height = 1080;
            p_dec->fmt_out.video.i_sar_num *= 135;
            p_dec->fmt_out.video.i_sar_den *= 136;
            msg_Warn( p_dec, "Fixing broken HDTV stream (display_height=1088)");
        }

        if( !p_dec->fmt_out.video.i_sar_num ||
            !p_dec->fmt_out.video.i_sar_den )
        {
            p_dec->fmt_out.video.i_sar_num = p_dec->fmt_out.video.i_aspect *
              p_dec->fmt_out.video.i_visible_height;

            p_dec->fmt_out.video.i_sar_den = VOUT_ASPECT_FACTOR *
              p_dec->fmt_out.video.i_visible_width;
        }

        vlc_ureduce( &p_dec->fmt_out.video.i_sar_num,
                     &p_dec->fmt_out.video.i_sar_den,
                     p_dec->fmt_out.video.i_sar_num,
                     p_dec->fmt_out.video.i_sar_den, 50000 );

        p_dec->fmt_out.video.i_chroma = p_dec->fmt_out.i_codec;
        p_owner->video = p_dec->fmt_out.video;

        p_owner->p_vout = vout_Request( p_dec, p_owner->p_vout,
                                      &p_dec->fmt_out.video );
        var_SetBool( p_owner->p_input, "intf-change-vout", true );
        if( p_owner->p_vout == NULL )
        {
            msg_Err( p_dec, "failed to create video output" );
            p_dec->b_error = true;
            return NULL;
        }

        if( p_owner->video.i_rmask )
            p_owner->p_vout->render.i_rmask = p_owner->video.i_rmask;
        if( p_owner->video.i_gmask )
            p_owner->p_vout->render.i_gmask = p_owner->video.i_gmask;
        if( p_owner->video.i_bmask )
            p_owner->p_vout->render.i_bmask = p_owner->video.i_bmask;
    }

    /* Get a new picture
     */
    for( p_pic = NULL; ; )
    {
        int i_pic, i_ready_pic;

        if( p_dec->b_die || p_dec->b_error )
            return NULL;

        /* The video filter chain required that there is always 1 free buffer
         * that it will use as temporary one. It will release the temporary
         * buffer once its work is done, so this check is safe even if we don't
         * lock around both count() and create().
         */
        if( vout_CountPictureAvailable( p_owner->p_vout ) >= 2 )
        {
            p_pic = vout_CreatePicture( p_owner->p_vout, 0, 0, 0 );
            if( p_pic )
                break;
        }

#define p_pic p_owner->p_vout->render.pp_picture[i_pic]
        /* Check the decoder doesn't leak pictures */
        for( i_pic = 0, i_ready_pic = 0; i_pic < p_owner->p_vout->render.i_pictures; i_pic++ )
        {
            if( p_pic->i_status == READY_PICTURE )
            {
                i_ready_pic++;
                /* If we have at least 2 ready pictures, wait for the vout thread to
                 * process one */
                if( i_ready_pic >= 2 )
                    break;

                continue;
            }

            if( p_pic->i_status == DISPLAYED_PICTURE )
            {
                /* If at least one displayed picture is not referenced
                 * let vout free it */
                if( p_pic->i_refcount == 0 )
                    break;
            }
        }
        if( i_pic == p_owner->p_vout->render.i_pictures )
        {
            /* Too many pictures are still referenced, there is probably a bug
             * with the decoder */
            msg_Err( p_dec, "decoder is leaking pictures, resetting the heap" );

            /* Just free all the pictures */
            for( i_pic = 0; i_pic < p_owner->p_vout->render.i_pictures;
                 i_pic++ )
            {
                if( p_pic->i_status == RESERVED_PICTURE )
                    vout_DestroyPicture( p_owner->p_vout, p_pic );
                if( p_pic->i_refcount > 0 )
                vout_UnlinkPicture( p_owner->p_vout, p_pic );
            }
        }
#undef p_pic

        msleep( VOUT_OUTMEM_SLEEP );
    }

    return p_pic;
}

static void vout_del_buffer( decoder_t *p_dec, picture_t *p_pic )
{
    VoutDisplayedPicture( p_dec->p_owner->p_vout, p_pic );
}

static void vout_link_picture( decoder_t *p_dec, picture_t *p_pic )
{
    vout_LinkPicture( p_dec->p_owner->p_vout, p_pic );
}

static void vout_unlink_picture( decoder_t *p_dec, picture_t *p_pic )
{
    vout_UnlinkPicture( p_dec->p_owner->p_vout, p_pic );
}

static subpicture_t *spu_new_buffer( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_owner = p_dec->p_owner;
    vout_thread_t *p_vout = NULL;
    subpicture_t *p_subpic;
    int i_attempts = 30;

    while( i_attempts-- )
    {
        if( p_dec->b_die || p_dec->b_error ) break;

        p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
        if( p_vout ) break;

        msleep( VOUT_DISPLAY_DELAY );
    }

    if( !p_vout )
    {
        msg_Warn( p_dec, "no vout found, dropping subpicture" );
        return NULL;
    }

    if( p_owner->p_spu_vout != p_vout )
    {
        spu_Control( p_vout->p_spu, SPU_CHANNEL_REGISTER,
                     &p_owner->i_spu_channel );
        p_owner->i_spu_order = 0;
        p_owner->p_spu_vout = p_vout;
    }

    p_subpic = subpicture_New();
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

    p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
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

