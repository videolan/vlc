/*****************************************************************************
 * input_dec.c: Functions for the management of decoders
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: input_dec.c,v 1.70 2003/11/18 22:08:07 gbazin Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/vout.h>

#include "stream_output.h"

#include "input_ext-intf.h"
#include "input_ext-plugins.h"

#include "codecs.h"

static decoder_t * CreateDecoder( input_thread_t *, es_descriptor_t *, int );
static int         DecoderThread( decoder_t * );
static void        DeleteDecoder( decoder_t * );

/* Buffers allocation callbacks for the decoders */
static aout_buffer_t *aout_new_buffer( decoder_t *, int );
static void aout_del_buffer( decoder_t *, aout_buffer_t * );

static picture_t *vout_new_buffer( decoder_t * );
static void vout_del_buffer( decoder_t *, picture_t * );

static es_format_t null_es_format = {0};

/*****************************************************************************
 * input_RunDecoder: spawns a new decoder thread
 *****************************************************************************/
decoder_fifo_t * input_RunDecoder( input_thread_t * p_input,
                                   es_descriptor_t * p_es )
{
    vlc_value_t    val;
    decoder_t      *p_dec = NULL;
    int            i_priority;

    /* If we are in sout mode, search for packetizer module */
    var_Get( p_input, "sout", &val );
    if( !p_es->b_force_decoder && val.psz_string && *val.psz_string )
    {
        free( val.psz_string );
        val.b_bool = VLC_TRUE;

        if( p_es->i_cat == AUDIO_ES )
        {
            var_Get( p_input, "sout-audio", &val );
        }
        else if( p_es->i_cat == VIDEO_ES )
        {
            var_Get( p_input, "sout-video", &val );
        }

        if( val.b_bool )
        {
            /* Create the decoder configuration structure */
            p_dec = CreateDecoder( p_input, p_es, VLC_OBJECT_PACKETIZER );
            if( p_dec == NULL )
            {
                msg_Err( p_input, "could not create packetizer" );
                return NULL;
            }

            p_dec->p_module =
                module_Need( p_dec, "packetizer", "$packetizer" );
        }
    }
    else
    {
        /* Create the decoder configuration structure */
        p_dec = CreateDecoder( p_input, p_es, VLC_OBJECT_DECODER );
        if( p_dec == NULL )
        {
            msg_Err( p_input, "could not create decoder" );
            return NULL;
        }

        /* default Get a suitable decoder module */
        p_dec->p_module = module_Need( p_dec, "decoder", "$codec" );

        if( val.psz_string ) free( val.psz_string );
    }

    if( !p_dec || !p_dec->p_module )
    {
        msg_Err( p_dec, "no suitable decoder module for fourcc `%4.4s'.\n"
                 "VLC probably does not support this sound or video format.",
                 (char*)&p_dec->p_fifo->i_fourcc );

        DeleteDecoder( p_dec );
        vlc_object_destroy( p_dec );
        return NULL;
    }

    if ( p_es->i_cat == AUDIO_ES )
    {
        i_priority = VLC_THREAD_PRIORITY_AUDIO;
    }
    else
    {
        i_priority = VLC_THREAD_PRIORITY_VIDEO;
    }

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

    p_input->stream.b_changed = 1;

    return p_dec->p_fifo;
}

/*****************************************************************************
 * input_EndDecoder: kills a decoder thread and waits until it's finished
 *****************************************************************************/
void input_EndDecoder( input_thread_t * p_input, es_descriptor_t * p_es )
{
    int i_dummy;
    decoder_t *p_dec = p_es->p_decoder_fifo->p_dec;

    p_es->p_decoder_fifo->b_die = 1;

    /* Make sure the thread leaves the NextDataPacket() function by
     * sending it a few null packets. */
    for( i_dummy = 0; i_dummy < PADDING_PACKET_NUMBER; i_dummy++ )
    {
        input_NullPacket( p_input, p_es );
    }

    if( p_es->p_pes != NULL )
    {
        input_DecodePES( p_es->p_decoder_fifo, p_es->p_pes );
    }

    /* Waiting for the thread to exit */
    /* I thought that unlocking was better since thread join can be long
     * but it actually creates late pictures and freezes --stef */
    /* vlc_mutex_unlock( &p_input->stream.stream_lock ); */
    vlc_thread_join( p_dec );
    /* vlc_mutex_lock( &p_input->stream.stream_lock ); */

    /* Unneed module */
    module_Unneed( p_dec, p_dec->p_module );

    /* Delete decoder configuration */
    DeleteDecoder( p_dec );

    /* Delete the decoder */
    vlc_object_destroy( p_dec );

    /* Tell the input there is no more decoder */
    p_es->p_decoder_fifo = NULL;

    p_input->stream.b_changed = 1;
}

/*****************************************************************************
 * input_DecodePES
 *****************************************************************************
 * Put a PES in the decoder's fifo.
 *****************************************************************************/
void input_DecodePES( decoder_fifo_t * p_decoder_fifo, pes_packet_t * p_pes )
{
    vlc_mutex_lock( &p_decoder_fifo->data_lock );

    p_pes->p_next = NULL;
    *p_decoder_fifo->pp_last = p_pes;
    p_decoder_fifo->pp_last = &p_pes->p_next;
    p_decoder_fifo->i_depth++;

    /* Warn the decoder that it's got work to do. */
    vlc_cond_signal( &p_decoder_fifo->data_wait );
    vlc_mutex_unlock( &p_decoder_fifo->data_lock );
}

/*****************************************************************************
 * Create a NULL packet for padding in case of a data loss
 *****************************************************************************/
void input_NullPacket( input_thread_t * p_input,
                       es_descriptor_t * p_es )
{
    data_packet_t *             p_pad_data;
    pes_packet_t *              p_pes;

    if( (p_pad_data = input_NewPacketForce( p_input->p_method_data,
                    PADDING_PACKET_SIZE)) == NULL )
    {
        msg_Err( p_input, "no new packet" );
        p_input->b_error = 1;
        return;
    }

    memset( p_pad_data->p_payload_start, 0, PADDING_PACKET_SIZE );
    p_pad_data->b_discard_payload = 1;
    p_pes = p_es->p_pes;

    if( p_pes != NULL )
    {
        p_pes->b_discontinuity = 1;
        p_pes->p_last->p_next = p_pad_data;
        p_pes->p_last = p_pad_data;
        p_pes->i_nb_data++;
    }
    else
    {
        if( (p_pes = input_NewPES( p_input->p_method_data )) == NULL )
        {
            msg_Err( p_input, "no PES packet" );
            p_input->b_error = 1;
            return;
        }

        p_pes->i_rate = p_input->stream.control.i_rate;
        p_pes->p_first = p_pes->p_last = p_pad_data;
        p_pes->i_nb_data = 1;
        p_pes->b_discontinuity = 1;
        input_DecodePES( p_es->p_decoder_fifo, p_pes );
    }
}

/*****************************************************************************
 * input_EscapeDiscontinuity: send a NULL packet to the decoders
 *****************************************************************************/
void input_EscapeDiscontinuity( input_thread_t * p_input )
{
    unsigned int i_es, i;

    for( i_es = 0; i_es < p_input->stream.i_selected_es_number; i_es++ )
    {
        es_descriptor_t * p_es = p_input->stream.pp_selected_es[i_es];

        if( p_es->p_decoder_fifo != NULL )
        {
            for( i = 0; i < PADDING_PACKET_NUMBER; i++ )
            {
                input_NullPacket( p_input, p_es );
            }
        }
    }
}

/*****************************************************************************
 * input_EscapeAudioDiscontinuity: send a NULL packet to the audio decoders
 *****************************************************************************/
void input_EscapeAudioDiscontinuity( input_thread_t * p_input )
{
    unsigned int i_es, i;

    for( i_es = 0; i_es < p_input->stream.i_selected_es_number; i_es++ )
    {
        es_descriptor_t * p_es = p_input->stream.pp_selected_es[i_es];

        if( p_es->p_decoder_fifo != NULL && p_es->i_cat == AUDIO_ES )
        {
            for( i = 0; i < PADDING_PACKET_NUMBER; i++ )
            {
                input_NullPacket( p_input, p_es );
            }
        }
    }
}

struct decoder_owner_sys_t
{
    aout_instance_t *p_aout;
    aout_input_t    *p_aout_input;

    vout_thread_t   *p_vout;

    sout_packetizer_input_t *p_sout;
    sout_format_t           sout_format;

    /* Current format in use by the output */
    video_format_t video;
    audio_format_t audio; 
};

/*****************************************************************************
 * CreateDecoder: create a decoder object
 *****************************************************************************/
static decoder_t * CreateDecoder( input_thread_t * p_input,
                                  es_descriptor_t * p_es, int i_object_type )
{
    decoder_t *p_dec;

    p_dec = vlc_object_create( p_input, i_object_type );
    if( p_dec == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return NULL;
    }

    p_dec->pf_decode = 0;
    p_dec->pf_decode_audio = 0;
    p_dec->pf_decode_video = 0;
    p_dec->pf_decode_sub = 0;
    p_dec->pf_packetize = 0;
    p_dec->pf_run = 0;

    /* Select a new ES */
    INSERT_ELEM( p_input->stream.pp_selected_es,
                 p_input->stream.i_selected_es_number,
                 p_input->stream.i_selected_es_number,
                 p_es );

    /* Allocate the memory needed to store the decoder's fifo */
    p_dec->p_fifo = vlc_object_create( p_input, VLC_OBJECT_DECODER_FIFO );
    if( p_dec->p_fifo == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return NULL;
    }

    /* Initialize the decoder fifo */
    p_dec->p_module = NULL;

    p_dec->fmt_in = p_es->fmt;

    if( p_es->p_waveformatex )
    {
#define p_wf ((WAVEFORMATEX *)p_es->p_waveformatex)
        p_dec->fmt_in.audio.i_channels = p_wf->nChannels;
        p_dec->fmt_in.audio.i_rate = p_wf->nSamplesPerSec;
        p_dec->fmt_in.i_bitrate = p_wf->nAvgBytesPerSec * 8;
        p_dec->fmt_in.audio.i_blockalign = p_wf->nBlockAlign;
        p_dec->fmt_in.audio.i_bitspersample = p_wf->wBitsPerSample;
        p_dec->fmt_in.i_extra = p_wf->cbSize;
        p_dec->fmt_in.p_extra = NULL;
        if( p_wf->cbSize )
        {
            p_dec->fmt_in.p_extra = malloc( p_wf->cbSize );
            memcpy( p_dec->fmt_in.p_extra, &p_wf[1], p_wf->cbSize );
        }
    }

    if( p_es->p_bitmapinfoheader )
    {
#define p_bih ((BITMAPINFOHEADER *) p_es->p_bitmapinfoheader)
        p_dec->fmt_in.i_extra = p_bih->biSize - sizeof(BITMAPINFOHEADER);
        p_dec->fmt_in.p_extra = NULL;
        if( p_dec->fmt_in.i_extra )
        {
            p_dec->fmt_in.p_extra = malloc( p_dec->fmt_in.i_extra );
            memcpy( p_dec->fmt_in.p_extra, &p_bih[1], p_dec->fmt_in.i_extra );
        }

        p_dec->fmt_in.video.i_width = p_bih->biWidth;
        p_dec->fmt_in.video.i_height = p_bih->biHeight;
    }

    p_dec->fmt_in.i_cat = p_es->i_cat;
    p_dec->fmt_in.i_codec = p_es->i_fourcc;

    p_dec->fmt_out = null_es_format;

    /* Allocate our private structure for the decoder */
    p_dec->p_owner = (decoder_owner_sys_t*)malloc(sizeof(decoder_owner_sys_t));
    if( p_dec->p_owner == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return NULL;
    }
    p_dec->p_owner->p_aout = NULL;
    p_dec->p_owner->p_aout_input = NULL;
    p_dec->p_owner->p_vout = NULL;
    p_dec->p_owner->p_sout = NULL;

    /* Set buffers allocation callbacks for the decoders */
    p_dec->pf_aout_buffer_new = aout_new_buffer;
    p_dec->pf_aout_buffer_del = aout_del_buffer;
    p_dec->pf_vout_buffer_new = vout_new_buffer;
    p_dec->pf_vout_buffer_del = vout_del_buffer;

    /* For old decoders only */
    vlc_mutex_init( p_input, &p_dec->p_fifo->data_lock );
    vlc_cond_init( p_input, &p_dec->p_fifo->data_wait );
    p_es->p_decoder_fifo = p_dec->p_fifo;
    p_dec->p_fifo->i_id = p_es->i_id;
    p_dec->p_fifo->i_fourcc = p_es->i_fourcc;
    p_dec->p_fifo->p_demux_data   = p_es->p_demux_data;
    p_dec->p_fifo->p_waveformatex = p_es->p_waveformatex;
    p_dec->p_fifo->p_bitmapinfoheader = p_es->p_bitmapinfoheader;
    p_dec->p_fifo->p_spuinfo = p_es->p_spuinfo;
    p_dec->p_fifo->p_stream_ctrl = &p_input->stream.control;
    p_dec->p_fifo->p_sout = p_input->stream.p_sout;
    p_dec->p_fifo->p_first = NULL;
    p_dec->p_fifo->pp_last = &p_dec->p_fifo->p_first;
    p_dec->p_fifo->i_depth = 0;
    p_dec->p_fifo->b_die = p_dec->p_fifo->b_error = 0;
    p_dec->p_fifo->p_packets_mgt = p_input->p_method_data;
    p_dec->p_fifo->p_dec = p_dec;
    vlc_object_attach( p_dec->p_fifo, p_input );

    vlc_object_attach( p_dec, p_input );

    return p_dec;
}

/*****************************************************************************
 * DecoderThread: the decoding main loop
 *****************************************************************************/
static int DecoderThread( decoder_t * p_dec )
{
    pes_packet_t  *p_pes;
    data_packet_t *p_data;
    block_t       *p_block;

    /* Temporary wrapper to keep old decoder api functional */
    if( p_dec->pf_run )
    {
        p_dec->pf_run( p_dec->p_fifo );
        return 0;
    }

    /* The decoder's main loop */
    while( !p_dec->p_fifo->b_die && !p_dec->p_fifo->b_error )
    {
        int i_size;

        input_ExtractPES( p_dec->p_fifo, &p_pes );
        if( !p_pes )
        {
            p_dec->p_fifo->b_error = 1;
            break;
        }

        for( i_size = 0, p_data = p_pes->p_first;
             p_data != NULL; p_data = p_data->p_next )
        {
            i_size += p_data->p_payload_end - p_data->p_payload_start;
        }
        p_block = block_New( p_dec, i_size );
        for( i_size = 0, p_data = p_pes->p_first;
             p_data != NULL; p_data = p_data->p_next )
        {
            if( p_data->p_payload_end == p_data->p_payload_start )
                continue;
            memcpy( p_block->p_buffer + i_size, p_data->p_payload_start,
                    p_data->p_payload_end - p_data->p_payload_start );
            i_size += p_data->p_payload_end - p_data->p_payload_start;
        }

        p_block->i_pts = p_pes->i_pts;
        p_block->i_dts = p_pes->i_dts;
        p_block->b_discontinuity = p_pes->b_discontinuity;

        if( p_dec->i_object_type == VLC_OBJECT_PACKETIZER )
        {
            block_t *p_sout_block;

            while( (p_sout_block = p_dec->pf_packetize( p_dec, &p_block )) )
            {
                if( !p_dec->p_owner->p_sout )
                {
                    sout_format_t *p_format = &p_dec->p_owner->sout_format;

                    p_format->i_cat = p_dec->fmt_out.i_cat;
                    p_format->i_fourcc = p_dec->fmt_out.i_codec;
                    p_format->i_sample_rate =
                        p_dec->fmt_out.audio.i_rate;
                    p_format->i_channels =
                        p_dec->fmt_out.audio.i_channels;
                    p_format->i_block_align =
                        p_dec->fmt_out.audio.i_blockalign;
                    p_format->i_width  =
                        p_dec->fmt_out.video.i_width;
                    p_format->i_height =
                        p_dec->fmt_out.video.i_height;
                    p_format->i_bitrate     = p_dec->fmt_out.i_bitrate;
                    p_format->i_extra_data  = p_dec->fmt_out.i_extra;
                    p_format->p_extra_data  = p_dec->fmt_out.p_extra;

                    p_dec->p_owner->p_sout =
                        sout_InputNew( p_dec, p_format );

                    if( p_dec->p_owner->p_sout == NULL )
                    {
                        msg_Err( p_dec, "cannot create packetizer output" );
                        break;
                    }
                }

                while( p_sout_block )
                {
                    block_t       *p_next = p_sout_block->p_next;
                    sout_buffer_t *p_sout_buffer;

                    p_sout_buffer =
                        sout_BufferNew( p_dec->p_owner->p_sout->p_sout,
                                        p_sout_block->i_buffer );
                    if( p_sout_buffer == NULL )
                    {
                        msg_Err( p_dec, "cannot get sout buffer" );
                        break;
                    }

                    memcpy( p_sout_buffer->p_buffer, p_sout_block->p_buffer,
                            p_sout_block->i_buffer );

                    p_sout_buffer->i_pts = p_sout_block->i_pts;
                    p_sout_buffer->i_dts = p_sout_block->i_dts;
                    p_sout_buffer->i_length = p_sout_block->i_length;

                    block_Release( p_sout_block );

                    sout_InputSendBuffer( p_dec->p_owner->p_sout, p_sout_buffer );

                    p_sout_block = p_next;
                }
            }
        }
        else if( p_dec->fmt_in.i_cat == AUDIO_ES )
        {
            aout_buffer_t *p_aout_buf;

            while( (p_aout_buf = p_dec->pf_decode_audio( p_dec, &p_block )) )
            {
                aout_DecPlay( p_dec->p_owner->p_aout,
                              p_dec->p_owner->p_aout_input, p_aout_buf );
            }
        }
        else
        {
            picture_t *p_pic;

            while( (p_pic = p_dec->pf_decode_video( p_dec, &p_block )) )
            {
                vout_DatePicture( p_dec->p_owner->p_vout, p_pic, p_pic->date );
                vout_DisplayPicture( p_dec->p_owner->p_vout, p_pic );
            }
        }

        input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );
    }

    return 0;
}

/*****************************************************************************
 * DeleteDecoder: destroys a decoder object
 *****************************************************************************/
static void DeleteDecoder( decoder_t * p_dec )
{
    vlc_object_detach( p_dec );
    vlc_object_detach( p_dec->p_fifo );

    msg_Dbg( p_dec,
             "killing decoder for 0x%x, fourcc `%4.4s', %d PES in FIFO",
             p_dec->p_fifo->i_id, (char*)&p_dec->p_fifo->i_fourcc,
             p_dec->p_fifo->i_depth );

    /* Free all packets still in the decoder fifo. */
    input_DeletePES( p_dec->p_fifo->p_packets_mgt,
                     p_dec->p_fifo->p_first );

    /* Destroy the lock and cond */
    vlc_cond_destroy( &p_dec->p_fifo->data_wait );
    vlc_mutex_destroy( &p_dec->p_fifo->data_lock );

    /* Free fifo */
    vlc_object_destroy( p_dec->p_fifo );

    /* Cleanup */
    if( p_dec->p_owner->p_aout_input )
        aout_DecDelete( p_dec->p_owner->p_aout, p_dec->p_owner->p_aout_input );

    if( p_dec->p_owner->p_vout )
    {
        int i_pic;

        /* Hack to make sure all the the pictures are freed by the decoder */
        for( i_pic = 0; i_pic < p_dec->p_owner->p_vout->render.i_pictures;
             i_pic++ )
        {
            if( p_dec->p_owner->p_vout->render.pp_picture[i_pic]->i_status ==
                RESERVED_PICTURE )
                vout_DestroyPicture( p_dec->p_owner->p_vout,
                    p_dec->p_owner->p_vout->render.pp_picture[i_pic] );
            if( p_dec->p_owner->p_vout->render.pp_picture[i_pic]->i_refcount
                > 0 )
                vout_UnlinkPicture( p_dec->p_owner->p_vout,
                    p_dec->p_owner->p_vout->render.pp_picture[i_pic] );
        }

        /* We are about to die. Reattach video output to p_vlc. */
        vout_Request( p_dec, p_dec->p_owner->p_vout, 0, 0, 0, 0 );
    }

    if( p_dec->p_owner->p_sout )
        sout_InputDelete( p_dec->p_owner->p_sout );

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
        p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;
        p_sys->audio = p_dec->fmt_out.audio;
        p_sys->p_aout_input =
            aout_DecNew( p_dec, &p_sys->p_aout, &p_sys->audio );
        if( p_sys->p_aout_input == NULL )
        {
            msg_Err( p_dec, "failed to create audio output" );
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

        p_dec->fmt_out.video.i_chroma = p_dec->fmt_out.i_codec;
        p_sys->video = p_dec->fmt_out.video;

        p_sys->p_vout = vout_Request( p_dec, p_sys->p_vout,
                                      p_sys->video.i_width,
                                      p_sys->video.i_height,
                                      p_sys->video.i_chroma,
                                      p_sys->video.i_aspect );

        if( p_sys->p_vout == NULL )
        {
            msg_Err( p_dec, "failed to create video output" );
            return NULL;
        }
    }

    /* Get a new picture */
    while( !(p_pic = vout_CreatePicture( p_sys->p_vout, 0, 0, 0 ) ) )
    {
        if( p_dec->b_die || p_dec->b_error )
        {
            return NULL;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    return p_pic;
}

static void vout_del_buffer( decoder_t *p_dec, picture_t *p_pic )
{
    vout_DestroyPicture( p_dec->p_owner->p_vout, p_pic );
}
