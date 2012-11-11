/*****************************************************************************
 * opus.c: opus decoder/encoder module making use of libopus.
 *****************************************************************************
 * Copyright (C) 2003-2009, 2012 the VideoLAN team
 *
 * Authors: Gregory Maxwell <greg@xiph.org>
 * Based on speex.c by: Gildas Bazin <gbazin@videolan.org>
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

/*
 * TODO: preskip, trimming, file duration
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_codec.h>
#include <vlc_aout.h>
#include "../demux/xiph.h"

#include <ogg/ogg.h>
#include <opus.h>
#include <opus_multistream.h>

#include "opus_header.h"

#ifndef OPUS_SET_GAIN
#include <math.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )

    set_description( N_("Opus audio decoder") )
    set_capability( "decoder", 100 )
    set_shortname( N_("Opus") )
    set_callbacks( OpenDecoder, CloseDecoder )

vlc_module_end ()

/*****************************************************************************
 * decoder_sys_t : opus decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Input properties
     */
    bool b_has_headers;

    /*
     * Opus properties
     */
    OpusHeader header;
    OpusMSDecoder *p_st;

    /*
     * Common properties
     */
    date_t end_date;
};

static const int pi_channels_maps[9] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARCENTER | AOUT_CHAN_MIDDLELEFT
     | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
     | AOUT_CHAN_LFE,
};

/*
**  channel order as defined in http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9
*/

/* recommended vorbis channel order for 8 channels */
static const uint32_t pi_8channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT,
  AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
  AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,AOUT_CHAN_LFE, 0 };

/* recommended vorbis channel order for 7 channels */
static const uint32_t pi_7channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT,
  AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
  AOUT_CHAN_REARCENTER, AOUT_CHAN_LFE, 0 };

/* recommended vorbis channel order for 6 channels */
static const uint32_t pi_6channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT,
  AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_LFE, 0 };

/* recommended vorbis channel order for 5 channels */
static const uint32_t pi_5channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT,
  AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, 0 };

/* recommended vorbis channel order for 4 channels */
static const uint32_t pi_4channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, 0 };

/* recommended vorbis channel order for 3 channels */
static const uint32_t pi_3channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT, 0 };

/****************************************************************************
 * Local prototypes
 ****************************************************************************/

static block_t *DecodeBlock  ( decoder_t *, block_t ** );
static int  ProcessHeaders( decoder_t * );
static int  ProcessInitialHeader ( decoder_t *, ogg_packet * );
static void *ProcessPacket( decoder_t *, ogg_packet *, block_t ** );

static block_t *DecodePacket( decoder_t *, ogg_packet *, int, int );

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_OPUS )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_dec->p_sys->b_has_headers = false;

    date_Set( &p_sys->end_date, 0 );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_FL32;

    p_dec->pf_decode_audio = DecodeBlock;
    p_dec->pf_packetize    = DecodeBlock;

    p_sys->p_st = NULL;

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static block_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;

    if( !pp_block || !*pp_block)
        return NULL;

    /* Block to Ogg packet */
    oggpacket.packet = (*pp_block)->p_buffer;
    oggpacket.bytes = (*pp_block)->i_buffer;

    oggpacket.granulepos = -1;
    oggpacket.b_o_s = 0;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Check for headers */
    if( !p_sys->b_has_headers )
    {
        if( ProcessHeaders( p_dec ) )
        {
            block_Release( *pp_block );
            return NULL;
        }
        p_sys->b_has_headers = true;
    }

    return ProcessPacket( p_dec, &oggpacket, pp_block );
}

/*****************************************************************************
 * ProcessHeaders: process Opus headers.
 *****************************************************************************/
static int ProcessHeaders( decoder_t *p_dec )
{
    ogg_packet oggpacket;

    unsigned pi_size[XIPH_MAX_HEADER_COUNT];
    void     *pp_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_count;
    int ret = VLC_EGENERIC;

    if( xiph_SplitHeaders( pi_size, pp_data, &i_count,
                           p_dec->fmt_in.i_extra, p_dec->fmt_in.p_extra) )
        return VLC_EGENERIC;
    if( i_count < 2 )
        goto end;

    oggpacket.granulepos = -1;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Take care of the initial Opus header */
    oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
    oggpacket.bytes  = pi_size[0];
    oggpacket.packet = pp_data[0];
    ret = ProcessInitialHeader( p_dec, &oggpacket );

    if (ret != VLC_SUCCESS)
        msg_Err( p_dec, "initial Opus header is corrupted" );

end:
    for( unsigned i = 0; i < i_count; i++ )
        free( pp_data[i] );

    return ret;
}

/*****************************************************************************
 * ProcessInitialHeader: processes the inital Opus header packet.
 *****************************************************************************/
static int ProcessInitialHeader( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    int err;
    unsigned char new_stream_map[8];
    decoder_sys_t *p_sys = p_dec->p_sys;

    OpusHeader *p_header = &p_sys->header;

    if( !opus_header_parse((unsigned char *)p_oggpacket->packet,p_oggpacket->bytes,p_header) )
    {
        msg_Err( p_dec, "cannot read Opus header" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_dec, "Opus audio with %d channels", p_header->channels);

    if((p_header->channels>2 && p_header->channel_mapping==0) ||
       (p_header->channels>8 && p_header->channel_mapping==1) ||
        p_header->channel_mapping>1)
    {
        msg_Err( p_dec, "Unsupported channel mapping" );
        return VLC_EGENERIC;
    }

    /* Setup the format */
    p_dec->fmt_out.audio.i_physical_channels =
        p_dec->fmt_out.audio.i_original_channels =
            pi_channels_maps[p_header->channels];
    p_dec->fmt_out.audio.i_channels = p_header->channels;
    p_dec->fmt_out.audio.i_rate = 48000;

    if( p_header->channels>2 )
    {
        static const uint32_t *pi_ch[6] = { pi_3channels_in, pi_4channels_in,
                                            pi_5channels_in, pi_6channels_in,
                                            pi_7channels_in, pi_8channels_in };
        uint8_t pi_chan_table[AOUT_CHAN_MAX];

        aout_CheckChannelReorder( pi_ch[p_header->channels-3], NULL,
                                  p_dec->fmt_out.audio.i_physical_channels,
                                  pi_chan_table );
        for(int i=0;i<p_header->channels;i++)
            new_stream_map[pi_chan_table[i]]=p_header->stream_map[i];
    }
    /* Opus decoder init */
    p_sys->p_st = opus_multistream_decoder_create( 48000, p_header->channels,
                    p_header->nb_streams, p_header->nb_coupled,
                    p_header->channels>2?new_stream_map:p_header->stream_map,
                    &err );
    if( !p_sys->p_st || err!=OPUS_OK )
    {
        msg_Err( p_dec, "decoder initialization failed" );
        return VLC_EGENERIC;
    }

#ifdef OPUS_SET_GAIN
    if( opus_multistream_decoder_ctl( p_sys->p_st,OPUS_SET_GAIN(p_header->gain) ) != OPUS_OK )
    {
        msg_Err( p_dec, "OPUS_SET_GAIN failed" );
        opus_multistream_decoder_destroy( p_sys->p_st );
        return VLC_EGENERIC;
    }
#endif

    date_Init( &p_sys->end_date, 48000, 1 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ProcessPacket: processes a Opus packet.
 *****************************************************************************/
static void *ProcessPacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                            block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;

    /* Date management */
    if( p_block && p_block->i_pts > VLC_TS_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    if( !date_Get( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        if( p_block ) block_Release( p_block );
        return NULL;
    }

    *pp_block = NULL; /* To avoid being fed the same packet again */

    {
        block_t *p_aout_buffer = DecodePacket( p_dec, p_oggpacket,
                                               p_block->i_nb_samples,
                                               (int)p_block->i_length );

        block_Release( p_block );
        return p_aout_buffer;
    }
}

/*****************************************************************************
 * DecodePacket: decodes a Opus packet.
 *****************************************************************************/
static block_t *DecodePacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                              int i_nb_samples, int i_end_trim )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_oggpacket->bytes )
        return NULL;

    int spp;
    spp=opus_packet_get_nb_frames(p_oggpacket->packet,p_oggpacket->bytes);
    if(spp>0)spp*=opus_packet_get_samples_per_frame(p_oggpacket->packet,48000);
    if(spp<120||spp>120*48)return NULL;

    block_t *p_aout_buffer=decoder_NewAudioBuffer( p_dec, spp );
    if ( !p_aout_buffer )
    {
        msg_Err(p_dec, "Oops: No new buffer was returned!");
        return NULL;
    }

    spp=opus_multistream_decode_float(p_sys->p_st, p_oggpacket->packet,
         p_oggpacket->bytes, (float *)p_aout_buffer->p_buffer, spp, 0);
    if( spp < 0 || i_nb_samples <= 0 || i_end_trim >= i_nb_samples)
    {
        block_Release(p_aout_buffer);
        if( spp < 0 )
            msg_Err( p_dec, "Error: corrupted stream?" );
        return NULL;
    }
    if( spp > i_nb_samples )
    {
        memmove(p_aout_buffer->p_buffer,
            p_aout_buffer->p_buffer
            + (spp - i_nb_samples)*p_sys->header.channels*sizeof(float),
            (i_nb_samples - i_end_trim)*p_sys->header.channels*sizeof(float));
    }
    i_nb_samples -= i_end_trim;

#ifndef OPUS_SET_GAIN
    if(p_sys->header.gain!=0)
    {
        float gain = pow(10., p_sys->header.gain/5120.);
        float *buf =(float *)p_aout_buffer->p_buffer;
        int i;
        for( i = 0; i < i_nb_samples*p_sys->header.channels; i++)
            buf[i] *= gain;
    }
#endif
    p_aout_buffer->i_nb_samples = i_nb_samples;
    p_aout_buffer->i_pts = date_Get( &p_sys->end_date );
    p_aout_buffer->i_length = date_Increment( &p_sys->end_date, i_nb_samples )
        - p_aout_buffer->i_pts;
    return p_aout_buffer;
}

/*****************************************************************************
 * CloseDecoder: Opus decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t * p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_st ) opus_multistream_decoder_destroy(p_sys->p_st);

    free( p_sys );
}
