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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <vlc/vlc.h>

#include <vlc/decoder.h>
#include <vlc/vout.h>
#include <vlc/input.h>

#include "stream_output.h"
#include "input_internal.h"

static decoder_t * CreateDecoder( input_thread_t *, es_format_t *, int );
static void        DeleteDecoder( decoder_t * );

static int         DecoderThread( decoder_t * );
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

static es_format_t null_es_format = {0};

struct decoder_owner_sys_t
{
    vlc_bool_t      b_own_thread;

    int64_t         i_preroll_end;

    input_thread_t  *p_input;

    aout_instance_t *p_aout;
    aout_input_t    *p_aout_input;

    vout_thread_t   *p_vout;

    vout_thread_t   *p_spu_vout;
    int              i_spu_channel;

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
};


/**
 * Spawns a new decoder thread
 *
 * \param p_input the input thread
 * \param p_es the es descriptor
 * \return the spawned decoder object
 */
decoder_t *input_DecoderNew( input_thread_t *p_input,
                             es_format_t *fmt, vlc_bool_t b_force_decoder )
{
    decoder_t   *p_dec = NULL;
    vlc_value_t val;

    /* If we are in sout mode, search for packetizer module */
    if( p_input->p_sout && !b_force_decoder )
    {
        /* Create the decoder configuration structure */
        p_dec = CreateDecoder( p_input, fmt, VLC_OBJECT_PACKETIZER );
        if( p_dec == NULL )
        {
            msg_Err( p_input, "could not create packetizer" );
            return NULL;
        }
    }
    else
    {
        /* Create the decoder configuration structure */
        p_dec = CreateDecoder( p_input, fmt, VLC_OBJECT_DECODER );
        if( p_dec == NULL )
        {
            msg_Err( p_input, "could not create decoder" );
            return NULL;
        }
    }

    if( !p_dec->p_module )
    {
        msg_Err( p_dec, "no suitable decoder module for fourcc `%4.4s'.\n"
                 "VLC probably does not support this sound or video format.",
                 (char*)&p_dec->fmt_in.i_codec );

        DeleteDecoder( p_dec );
        vlc_object_destroy( p_dec );
        return NULL;
    }

    if( p_input->p_sout && p_input->input.b_can_pace_control &&
        !b_force_decoder )
    {
        msg_Dbg( p_input, "stream out mode -> no decoder thread" );
        p_dec->p_owner->b_own_thread = VLC_FALSE;
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
                               i_priority, VLC_FALSE ) )
        {
            msg_Err( p_dec, "cannot spawn decoder thread \"%s\"",
                             p_dec->p_module->psz_object_name );
            module_Unneed( p_dec, p_dec->p_module );
            DeleteDecoder( p_dec );
            vlc_object_destroy( p_dec );
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
    p_dec->b_die = VLC_TRUE;

    if( p_dec->p_owner->b_own_thread )
    {
        /* Make sure the thread leaves the function by
         * sending it an empty block. */
        block_t *p_block = block_New( p_dec, 0 );
        input_DecoderDecode( p_dec, p_block );

        vlc_thread_join( p_dec );

        /* Don't module_Unneed() here because of the dll loader that wants
         * close() in the same thread than open()/decode() */
    }
    else
    {
        /* Flush */
        input_DecoderDecode( p_dec, NULL );

        module_Unneed( p_dec, p_dec->p_module );
    }

    /* Delete decoder configuration */
    DeleteDecoder( p_dec );

    /* Delete the decoder */
    vlc_object_destroy( p_dec );
}

/**
 * Put a block_t in the decoder's fifo.
 *
 * \param p_dec the decoder object
 * \param p_block the data block
 */
void input_DecoderDecode( decoder_t * p_dec, block_t *p_block )
{
    if( p_dec->p_owner->b_own_thread )
    {
        if( p_dec->p_owner->p_input->b_out_pace_control )
        {
            /* FIXME !!!!! */
            while( !p_dec->b_die && !p_dec->b_error &&
                   p_dec->p_owner->p_fifo->i_depth > 10 )
            {
                msleep( 1000 );
            }
        }
        else if( p_dec->p_owner->p_fifo->i_size > 50000000 /* 50 MB */ )
        {
            /* FIXME: ideally we would check the time amount of data
             * in the fifo instead of its size. */
            msg_Warn( p_dec, "decoder/packetizer fifo full (data not "
                      "consummed quickly enough), resetting fifo!" );
            block_FifoEmpty( p_dec->p_owner->p_fifo );
        }

        block_FifoPut( p_dec->p_owner->p_fifo, p_block );
    }
    else
    {
        if( p_dec->b_error || (p_block && p_block->i_buffer <= 0) )
        {
            if( p_block ) block_Release( p_block );
        }
        else
        {
            DecoderDecode( p_dec, p_block );
        }
    }
}

void input_DecoderDiscontinuity( decoder_t * p_dec )
{
    block_t *p_null;

    /* Empty the fifo */
    if( p_dec->p_owner->b_own_thread )
    {
        block_FifoEmpty( p_dec->p_owner->p_fifo );
    }

    /* Send a special block */
    p_null = block_New( p_dec, 128 );
    p_null->i_flags |= BLOCK_FLAG_DISCONTINUITY;
    memset( p_null->p_buffer, 0, p_null->i_buffer );

    input_DecoderDecode( p_dec, p_null );
}

vlc_bool_t input_DecoderEmpty( decoder_t * p_dec )
{
    if( p_dec->p_owner->b_own_thread && p_dec->p_owner->p_fifo->i_depth > 0 )
    {
        return VLC_FALSE;
    }
    return VLC_TRUE;
}

void input_DecoderPreroll( decoder_t *p_dec, int64_t i_preroll_end )
{
    p_dec->p_owner->i_preroll_end = i_preroll_end;
}

#if 0
/**
 * Create a NULL packet for padding in case of a data loss
 *
 * \param p_input the input thread
 * \param p_es es descriptor
 * \return nothing
 */
static void input_NullPacket( input_thread_t * p_input,
                              es_descriptor_t * p_es )
{
#if 0
    block_t *p_block = block_New( p_input, PADDING_PACKET_SIZE );
    if( p_block )
    {
        memset( p_block->p_buffer, 0, PADDING_PACKET_SIZE );
        p_block->i_flags |= BLOCK_FLAG_DISCONTINUITY;

        block_FifoPut( p_es->p_dec->p_owner->p_fifo, p_block );
    }
#endif
}

/**
 * Send a NULL packet to the decoders
 *
 * \param p_input the input thread
 * \return nothing
 */
void input_EscapeDiscontinuity( input_thread_t * p_input )
{
#if 0
    unsigned int i_es, i;

    for( i_es = 0; i_es < p_input->stream.i_selected_es_number; i_es++ )
    {
        es_descriptor_t * p_es = p_input->stream.pp_selected_es[i_es];

        if( p_es->p_dec != NULL )
        {
            for( i = 0; i < PADDING_PACKET_NUMBER; i++ )
            {
                input_NullPacket( p_input, p_es );
            }
        }
    }
#endif
}

/**
 * Send a NULL packet to the audio decoders
 *
 * \param p_input the input thread
 * \return nothing
 */
void input_EscapeAudioDiscontinuity( input_thread_t * p_input )
{
#if 0
    unsigned int i_es, i;

    for( i_es = 0; i_es < p_input->stream.i_selected_es_number; i_es++ )
    {
        es_descriptor_t * p_es = p_input->stream.pp_selected_es[i_es];

        if( p_es->p_dec != NULL && p_es->i_cat == AUDIO_ES )
        {
            for( i = 0; i < PADDING_PACKET_NUMBER; i++ )
            {
                input_NullPacket( p_input, p_es );
            }
        }
    }
#endif
}
#endif

/**
 * Create a decoder object
 *
 * \param p_input the input thread
 * \param p_es the es descriptor
 * \param i_object_type Object type as define in include/vlc_objects.h
 * \return the decoder object
 */
static decoder_t * CreateDecoder( input_thread_t *p_input,
                                  es_format_t *fmt, int i_object_type )
{
    decoder_t *p_dec;

    p_dec = vlc_object_create( p_input, i_object_type );
    if( p_dec == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return NULL;
    }

    p_dec->pf_decode_audio = 0;
    p_dec->pf_decode_video = 0;
    p_dec->pf_decode_sub = 0;
    p_dec->pf_packetize = 0;

    /* Initialize the decoder fifo */
    p_dec->p_module = NULL;


    es_format_Copy( &p_dec->fmt_in, fmt );
    es_format_Copy( &p_dec->fmt_out, &null_es_format );

    /* Allocate our private structure for the decoder */
    p_dec->p_owner = malloc( sizeof( decoder_owner_sys_t ) );
    if( p_dec->p_owner == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return NULL;
    }
    p_dec->p_owner->b_own_thread = VLC_TRUE;
    p_dec->p_owner->i_preroll_end = -1;
    p_dec->p_owner->p_input = p_input;
    p_dec->p_owner->p_aout = NULL;
    p_dec->p_owner->p_aout_input = NULL;
    p_dec->p_owner->p_vout = NULL;
    p_dec->p_owner->p_spu_vout = NULL;
    p_dec->p_owner->i_spu_channel = 0;
    p_dec->p_owner->p_sout = p_input->p_sout;
    p_dec->p_owner->p_sout_input = NULL;
    p_dec->p_owner->p_packetizer = NULL;

    /* decoder fifo */
    if( ( p_dec->p_owner->p_fifo = block_FifoNew( p_dec ) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
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
        p_dec->p_module = module_Need( p_dec, "decoder", "$codec", 0 );
    else
        p_dec->p_module = module_Need( p_dec, "packetizer", "$packetizer", 0 );

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
                module_Need( p_dec->p_owner->p_packetizer,
                             "packetizer", "$packetizer", 0 );

            if( !p_dec->p_owner->p_packetizer->p_module )
            {
                es_format_Clean( &p_dec->p_owner->p_packetizer->fmt_in );
                vlc_object_detach( p_dec->p_owner->p_packetizer );
                vlc_object_destroy( p_dec->p_owner->p_packetizer );
            }
        }
    }

    return p_dec;
}

/**
 * The decoding main loop
 *
 * \param p_dec the decoder
 * \return 0
 */
static int DecoderThread( decoder_t * p_dec )
{
    block_t *p_block;

    /* The decoder's main loop */
    while( !p_dec->b_die && !p_dec->b_error )
    {
        if( ( p_block = block_FifoGet( p_dec->p_owner->p_fifo ) ) == NULL )
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
        p_block = block_FifoGet( p_dec->p_owner->p_fifo );
        if( p_block ) block_Release( p_block );
    }

    /* We do it here because of the dll loader that wants close() in the
     * same thread than open()/decode() */
    module_Unneed( p_dec, p_dec->p_module );

    return 0;
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
    int i_rate = p_block ? p_block->i_rate : 1000;

    if( p_block && p_block->i_buffer <= 0 )
    {
        block_Release( p_block );
        return VLC_SUCCESS;
    }

    if( p_dec->i_object_type == VLC_OBJECT_PACKETIZER )
    {
        block_t *p_sout_block;

        while( ( p_sout_block =
                     p_dec->pf_packetize( p_dec, p_block ? &p_block : 0 ) ) )
        {
            if( !p_dec->p_owner->p_sout_input )
            {
                es_format_Copy( &p_dec->p_owner->sout, &p_dec->fmt_out );

                p_dec->p_owner->sout.i_group = p_dec->fmt_in.i_group;
                p_dec->p_owner->sout.i_id = p_dec->fmt_in.i_id;
                if( p_dec->fmt_in.psz_language )
                {
                    if( p_dec->p_owner->sout.psz_language )
                        free( p_dec->p_owner->sout.psz_language );
                    p_dec->p_owner->sout.psz_language =
                        strdup( p_dec->fmt_in.psz_language );
                }

                p_dec->p_owner->p_sout_input =
                    sout_InputNew( p_dec->p_owner->p_sout,
                                   &p_dec->p_owner->sout );

                if( p_dec->p_owner->p_sout_input == NULL )
                {
                    msg_Err( p_dec, "cannot create packetizer output (%4.4s)",
                             (char *)&p_dec->p_owner->sout.i_codec );
                    p_dec->b_error = VLC_TRUE;

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
                p_sout_block->i_rate = i_rate;

                sout_InputSendBuffer( p_dec->p_owner->p_sout_input,
                                      p_sout_block );

                p_sout_block = p_next;
            }

            /* For now it's enough, as only sout inpact on this flag */
            if( p_dec->p_owner->p_sout->i_out_pace_nocontrol > 0 &&
                p_dec->p_owner->p_input->b_out_pace_control )
            {
                msg_Dbg( p_dec, "switching to synch mode" );
                p_dec->p_owner->p_input->b_out_pace_control = VLC_FALSE;
            }
            else if( p_dec->p_owner->p_sout->i_out_pace_nocontrol <= 0 &&
                     !p_dec->p_owner->p_input->b_out_pace_control )
            {
                msg_Dbg( p_dec, "switching to asynch mode" );
                p_dec->p_owner->p_input->b_out_pace_control = VLC_TRUE;
            }
        }
    }
    else if( p_dec->fmt_in.i_cat == AUDIO_ES )
    {
        aout_buffer_t *p_aout_buf;

        if( p_dec->p_owner->p_packetizer )
        {
            block_t *p_packetized_block;
            decoder_t *p_packetizer = p_dec->p_owner->p_packetizer;

            while( (p_packetized_block =
                    p_packetizer->pf_packetize( p_packetizer, &p_block )) )
            {
                if( p_packetizer->fmt_out.i_extra && !p_dec->fmt_in.i_extra )
                {
                    p_dec->fmt_in.i_extra = p_packetizer->fmt_out.i_extra;
                    p_dec->fmt_in.p_extra = malloc( p_dec->fmt_in.i_extra );
                    memcpy( p_dec->fmt_in.p_extra,
                            p_packetizer->fmt_out.p_extra,
                            p_dec->fmt_in.i_extra );
                }

                while( p_packetized_block )
                {
                    block_t *p_next = p_packetized_block->p_next;
                    p_packetized_block->p_next = NULL;
                    p_packetized_block->i_rate = i_rate;

                    while( (p_aout_buf = p_dec->pf_decode_audio( p_dec,
                                                       &p_packetized_block )) )
                    {
                        /* FIXME the best would be to handle the case start_date < preroll < end_date
                         * but that's not easy with non raw audio stream */
                        if( p_dec->p_owner->i_preroll_end > 0 &&
                            p_aout_buf->start_date < p_dec->p_owner->i_preroll_end )
                        {
                            aout_DecDeleteBuffer( p_dec->p_owner->p_aout,
                                                  p_dec->p_owner->p_aout_input, p_aout_buf );
                        }
                        else
                        {
                            p_dec->p_owner->i_preroll_end = -1;
                            aout_DecPlay( p_dec->p_owner->p_aout,
                                          p_dec->p_owner->p_aout_input,
                                          p_aout_buf );
                        }
                    }

                    p_packetized_block = p_next;
                }
            }
        }
        else while( (p_aout_buf = p_dec->pf_decode_audio( p_dec, &p_block )) )
        {
            if( p_dec->p_owner->i_preroll_end > 0 &&
                p_aout_buf->start_date < p_dec->p_owner->i_preroll_end )
            {
                aout_DecDeleteBuffer( p_dec->p_owner->p_aout,
                                      p_dec->p_owner->p_aout_input, p_aout_buf );
            }
            else
            {
                p_dec->p_owner->i_preroll_end = -1;
                aout_DecPlay( p_dec->p_owner->p_aout,
                              p_dec->p_owner->p_aout_input,
                              p_aout_buf );
            }
        }
    }
    else if( p_dec->fmt_in.i_cat == VIDEO_ES )
    {
        picture_t *p_pic;

        if( p_dec->p_owner->p_packetizer )
        {
            block_t *p_packetized_block;
            decoder_t *p_packetizer = p_dec->p_owner->p_packetizer;

            while( (p_packetized_block =
                    p_packetizer->pf_packetize( p_packetizer, &p_block )) )
            {
                if( p_packetizer->fmt_out.i_extra && !p_dec->fmt_in.i_extra )
                {
                    p_dec->fmt_in.i_extra = p_packetizer->fmt_out.i_extra;
                    p_dec->fmt_in.p_extra = malloc( p_dec->fmt_in.i_extra );
                    memcpy( p_dec->fmt_in.p_extra,
                            p_packetizer->fmt_out.p_extra,
                            p_dec->fmt_in.i_extra );
                }

                while( p_packetized_block )
                {
                    block_t *p_next = p_packetized_block->p_next;
                    p_packetized_block->p_next = NULL;
                    p_packetized_block->i_rate = i_rate;

                    while( (p_pic = p_dec->pf_decode_video( p_dec,
                                                       &p_packetized_block )) )
                    {
                        if( p_dec->p_owner->i_preroll_end > 0 &&
                            p_pic->date < p_dec->p_owner->i_preroll_end )
                        {
                            vout_DestroyPicture( p_dec->p_owner->p_vout, p_pic );
                        }
                        else
                        {
                            p_dec->p_owner->i_preroll_end = -1;
                            vout_DatePicture( p_dec->p_owner->p_vout, p_pic,
                                              p_pic->date );
                            vout_DisplayPicture( p_dec->p_owner->p_vout, p_pic );
                        }
                    }

                    p_packetized_block = p_next;
                }
            }
        }
        else while( (p_pic = p_dec->pf_decode_video( p_dec, &p_block )) )
        {
            if( p_dec->p_owner->i_preroll_end > 0 &&
                p_pic->date < p_dec->p_owner->i_preroll_end )
            {
                vout_DestroyPicture( p_dec->p_owner->p_vout, p_pic );
            }
            else
            {
                p_dec->p_owner->i_preroll_end = -1;
                vout_DatePicture( p_dec->p_owner->p_vout, p_pic, p_pic->date );
                vout_DisplayPicture( p_dec->p_owner->p_vout, p_pic );
            }
        }
    }
    else if( p_dec->fmt_in.i_cat == SPU_ES )
    {
        vout_thread_t *p_vout;
        subpicture_t *p_spu;
        while( (p_spu = p_dec->pf_decode_sub( p_dec, &p_block ) ) )
        {
            if( p_dec->p_owner->i_preroll_end > 0 &&
                p_spu->i_start < p_dec->p_owner->i_preroll_end &&
                ( p_spu->i_stop <= 0 || p_spu->i_stop <= p_dec->p_owner->i_preroll_end ) )
            {
                spu_DestroySubpicture( p_dec->p_owner->p_vout->p_spu, p_spu );
                continue;
            }

            p_dec->p_owner->i_preroll_end = -1;
            p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
            if( p_vout )
            {
                spu_DisplaySubpicture( p_vout->p_spu, p_spu );
                vlc_object_release( p_vout );
            }
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
    msg_Dbg( p_dec, "killing decoder fourcc `%4.4s', %d PES in FIFO",
             (char*)&p_dec->fmt_in.i_codec,
             p_dec->p_owner->p_fifo->i_depth );

    /* Free all packets still in the decoder fifo. */
    block_FifoEmpty( p_dec->p_owner->p_fifo );
    block_FifoRelease( p_dec->p_owner->p_fifo );

    /* Cleanup */
    if( p_dec->p_owner->p_aout_input )
        aout_DecDelete( p_dec->p_owner->p_aout, p_dec->p_owner->p_aout_input );

    if( p_dec->p_owner->p_vout )
    {
        int i_pic;

#define p_pic p_dec->p_owner->p_vout->render.pp_picture[i_pic]
        /* Hack to make sure all the the pictures are freed by the decoder */
        for( i_pic = 0; i_pic < p_dec->p_owner->p_vout->render.i_pictures;
             i_pic++ )
        {
            if( p_pic->i_status == RESERVED_PICTURE )
                vout_DestroyPicture( p_dec->p_owner->p_vout, p_pic );
            if( p_pic->i_refcount > 0 )
                vout_UnlinkPicture( p_dec->p_owner->p_vout, p_pic );
        }
#undef p_pic

        /* We are about to die. Reattach video output to p_vlc. */
        vout_Request( p_dec, p_dec->p_owner->p_vout, 0 );
    }

    if( p_dec->p_owner->p_sout_input )
    {
        sout_InputDelete( p_dec->p_owner->p_sout_input );
        es_format_Clean( &p_dec->p_owner->sout );
    }

    if( p_dec->fmt_in.i_cat == SPU_ES )
    {
        vout_thread_t *p_vout;

        p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
        if( p_vout )
        {
            spu_Control( p_vout->p_spu, SPU_CHANNEL_CLEAR,
                         p_dec->p_owner->i_spu_channel );
            vlc_object_release( p_vout );
        }
    }

    es_format_Clean( &p_dec->fmt_in );
    es_format_Clean( &p_dec->fmt_out );

    if( p_dec->p_owner->p_packetizer )
    {
        module_Unneed( p_dec->p_owner->p_packetizer,
                       p_dec->p_owner->p_packetizer->p_module );
        es_format_Clean( &p_dec->p_owner->p_packetizer->fmt_in );
        es_format_Clean( &p_dec->p_owner->p_packetizer->fmt_out );
        vlc_object_detach( p_dec->p_owner->p_packetizer );
        vlc_object_destroy( p_dec->p_owner->p_packetizer );
    }

    vlc_object_detach( p_dec );

    free( p_dec->p_owner );
}

/*****************************************************************************
 * Buffers allocation callbacks for the decoders
 *****************************************************************************/
static aout_buffer_t *aout_new_buffer( decoder_t *p_dec, int i_samples )
{
    decoder_owner_sys_t *p_sys = (decoder_owner_sys_t *)p_dec->p_owner;
    aout_buffer_t *p_buffer;

    if( p_sys->p_aout_input != NULL &&
        ( p_dec->fmt_out.audio.i_rate != p_sys->audio.i_rate ||
          p_dec->fmt_out.audio.i_original_channels !=
              p_sys->audio.i_original_channels ||
          p_dec->fmt_out.audio.i_bytes_per_frame !=
              p_sys->audio.i_bytes_per_frame ) )
    {
        /* Parameters changed, restart the aout */
        aout_DecDelete( p_sys->p_aout, p_sys->p_aout_input );
        p_sys->p_aout_input = NULL;
    }

    if( p_sys->p_aout_input == NULL )
    {
        audio_sample_format_t format;
        int i_force_dolby = config_GetInt( p_dec, "force-dolby-surround" );

        p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;
        p_sys->audio = p_dec->fmt_out.audio;

        memcpy( &format, &p_sys->audio, sizeof( audio_sample_format_t ) );
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

        p_sys->p_aout_input =
            aout_DecNew( p_dec, &p_sys->p_aout, &format );
        if( p_sys->p_aout_input == NULL )
        {
            msg_Err( p_dec, "failed to create audio output" );
            p_dec->b_error = VLC_TRUE;
            return NULL;
        }
        p_dec->fmt_out.audio.i_bytes_per_frame =
            p_sys->audio.i_bytes_per_frame;
    }

    p_buffer = aout_DecNewBuffer( p_sys->p_aout, p_sys->p_aout_input,
                                  i_samples );

    return p_buffer;
}

static void aout_del_buffer( decoder_t *p_dec, aout_buffer_t *p_buffer )
{
    aout_DecDeleteBuffer( p_dec->p_owner->p_aout,
                          p_dec->p_owner->p_aout_input, p_buffer );
}

static picture_t *vout_new_buffer( decoder_t *p_dec )
{
    decoder_owner_sys_t *p_sys = (decoder_owner_sys_t *)p_dec->p_owner;
    picture_t *p_pic;

    if( p_sys->p_vout == NULL ||
        p_dec->fmt_out.video.i_width != p_sys->video.i_width ||
        p_dec->fmt_out.video.i_height != p_sys->video.i_height ||
        p_dec->fmt_out.video.i_chroma != p_sys->video.i_chroma ||
        p_dec->fmt_out.video.i_aspect != p_sys->video.i_aspect )
    {
        if( !p_dec->fmt_out.video.i_width ||
            !p_dec->fmt_out.video.i_height )
        {
            /* Can't create a new vout without display size */
            return NULL;
        }

        if( !p_dec->fmt_out.video.i_sar_num ||
            !p_dec->fmt_out.video.i_sar_den )
        {
            p_dec->fmt_out.video.i_sar_num =
              p_dec->fmt_out.video.i_aspect * p_dec->fmt_out.video.i_height;

            p_dec->fmt_out.video.i_sar_den = VOUT_ASPECT_FACTOR *
              p_dec->fmt_out.video.i_width;
        }

        vlc_ureduce( &p_dec->fmt_out.video.i_sar_num,
                     &p_dec->fmt_out.video.i_sar_den,
                     p_dec->fmt_out.video.i_sar_num,
                     p_dec->fmt_out.video.i_sar_den, 0 );

        if( !p_dec->fmt_out.video.i_visible_width ||
            !p_dec->fmt_out.video.i_visible_height )
        {
            p_dec->fmt_out.video.i_visible_width =
                p_dec->fmt_out.video.i_width;
            p_dec->fmt_out.video.i_visible_height =
                p_dec->fmt_out.video.i_height;
        }

        p_dec->fmt_out.video.i_chroma = p_dec->fmt_out.i_codec;
        p_sys->video = p_dec->fmt_out.video;

        p_sys->p_vout = vout_Request( p_dec, p_sys->p_vout,
                                      &p_dec->fmt_out.video );
        if( p_sys->p_vout == NULL )
        {
            msg_Err( p_dec, "failed to create video output" );
            p_dec->b_error = VLC_TRUE;
            return NULL;
        }

        if( p_sys->video.i_rmask )
            p_sys->p_vout->render.i_rmask = p_sys->video.i_rmask;
        if( p_sys->video.i_gmask )
            p_sys->p_vout->render.i_gmask = p_sys->video.i_gmask;
        if( p_sys->video.i_bmask )
            p_sys->p_vout->render.i_bmask = p_sys->video.i_bmask;
    }

    /* Get a new picture */
    while( !(p_pic = vout_CreatePicture( p_sys->p_vout, 0, 0, 0 ) ) )
    {
        int i_pic, i_ready_pic = 0;

        if( p_dec->b_die || p_dec->b_error )
        {
            return NULL;
        }

#define p_pic p_dec->p_owner->p_vout->render.pp_picture[i_pic]
        /* Check the decoder doesn't leak pictures */
        for( i_pic = 0; i_pic < p_dec->p_owner->p_vout->render.i_pictures;
             i_pic++ )
        {
            if( p_pic->i_status == READY_PICTURE )
            {
                if( i_ready_pic++ > 0 ) break;
                else continue;
            }

            if( p_pic->i_status != DISPLAYED_PICTURE &&
                p_pic->i_status != RESERVED_PICTURE &&
                p_pic->i_status != READY_PICTURE ) break;

            if( !p_pic->i_refcount && p_pic->i_status != RESERVED_PICTURE )
                break;
        }
        if( i_pic == p_dec->p_owner->p_vout->render.i_pictures )
        {
            msg_Err( p_dec, "decoder is leaking pictures, resetting the heap" );

            /* Just free all the pictures */
            for( i_pic = 0; i_pic < p_dec->p_owner->p_vout->render.i_pictures;
                 i_pic++ )
            {
                if( p_pic->i_status == RESERVED_PICTURE )
                    vout_DestroyPicture( p_dec->p_owner->p_vout, p_pic );
                if( p_pic->i_refcount > 0 )
                vout_UnlinkPicture( p_dec->p_owner->p_vout, p_pic );
            }
        }
#undef p_pic

        msleep( VOUT_OUTMEM_SLEEP );
    }

    return p_pic;
}

static void vout_del_buffer( decoder_t *p_dec, picture_t *p_pic )
{
    vout_DestroyPicture( p_dec->p_owner->p_vout, p_pic );
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
    decoder_owner_sys_t *p_sys = (decoder_owner_sys_t *)p_dec->p_owner;
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

    if( p_sys->p_spu_vout != p_vout )
    {
        spu_Control( p_vout->p_spu, SPU_CHANNEL_REGISTER,
                     &p_sys->i_spu_channel );
        p_sys->p_spu_vout = p_vout;
    }

    p_subpic = spu_CreateSubpicture( p_vout->p_spu );
    if( p_subpic )
    {
        p_subpic->i_channel = p_sys->i_spu_channel;
    }

    vlc_object_release( p_vout );

    return p_subpic;
}

static void spu_del_buffer( decoder_t *p_dec, subpicture_t *p_subpic )
{
    decoder_owner_sys_t *p_sys = (decoder_owner_sys_t *)p_dec->p_owner;
    vout_thread_t *p_vout = NULL;

    p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( !p_vout || p_sys->p_spu_vout != p_vout )
    {
        if( p_vout )
            vlc_object_release( p_vout );
        msg_Warn( p_dec, "no vout found, leaking subpicture" );
        return;
    }

    spu_DestroySubpicture( p_vout->p_spu, p_subpic );

    vlc_object_release( p_vout );
}

