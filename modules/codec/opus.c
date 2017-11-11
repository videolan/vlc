/*****************************************************************************
 * opus.c: opus decoder/encoder module making use of libopus.
 *****************************************************************************
 * Copyright (C) 2003-2009, 2012 VLC authors and VideoLAN
 *
 * Authors: Gregory Maxwell <greg@xiph.org>
 * Based on speex.c by: Gildas Bazin <gbazin@videolan.org>
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
#ifdef ENABLE_SOUT
static int  OpenEncoder   ( vlc_object_t * );
static void CloseEncoder  ( vlc_object_t * );
#endif

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )

    set_description( N_("Opus audio decoder") )
    set_capability( "audio decoder", 100 )
    set_shortname( N_("Opus") )
    set_callbacks( OpenDecoder, CloseDecoder )

#ifdef ENABLE_SOUT
    add_submodule ()
    set_description( N_("Opus audio encoder") )
    set_capability( "encoder", 150 )
    set_shortname( N_("Opus") )
    set_callbacks( OpenEncoder, CloseEncoder )
#endif

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

static block_t *Packetize ( decoder_t *, block_t ** );
static int  DecodeAudio ( decoder_t *, block_t * );
static void Flush( decoder_t * );
static int  ProcessHeaders( decoder_t * );
static int  ProcessInitialHeader ( decoder_t *, ogg_packet * );
static block_t *ProcessPacket( decoder_t *, ogg_packet *, block_t * );

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
    p_dec->fmt_out.i_codec = VLC_CODEC_FL32;

    p_dec->pf_decode    = DecodeAudio;
    p_dec->pf_packetize = Packetize;
    p_dec->pf_flush     = Flush;

    p_sys->p_st = NULL;

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static block_t *DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;

    /* Block to Ogg packet */
    oggpacket.packet = p_block->p_buffer;
    oggpacket.bytes = p_block->i_buffer;

    oggpacket.granulepos = -1;
    oggpacket.b_o_s = 0;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Check for headers */
    if( !p_sys->b_has_headers )
    {
        if( ProcessHeaders( p_dec ) )
        {
            block_Release( p_block );
            return NULL;
        }
        p_sys->b_has_headers = true;
    }

    return ProcessPacket( p_dec, &oggpacket, p_block );
}

static int DecodeAudio( decoder_t *p_dec, block_t *p_block )
{
    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    p_block = DecodeBlock( p_dec, p_block );
    if( p_block != NULL )
        decoder_QueueAudio( p_dec, p_block );
    return VLCDEC_SUCCESS;
}

static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    if( pp_block == NULL ) /* No Drain */
        return NULL;
    block_t *p_block = *pp_block; *pp_block = NULL;
    if( p_block == NULL )
        return NULL;
    return DecodeBlock( p_dec, p_block );
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

    int i_extra = p_dec->fmt_in.i_extra;
    uint8_t *p_extra = p_dec->fmt_in.p_extra;

    /* If we have no header (e.g. from RTP), make one. */
    bool b_dummy_header = false;
    if( !i_extra ||
        (i_extra > 10 && memcmp( &p_extra[2], "OpusHead", 8 )) ) /* Borked muxers */
    {
        OpusHeader header;
        opus_prepare_header( p_dec->fmt_in.audio.i_channels,
                             p_dec->fmt_in.audio.i_rate, &header );
        if( opus_write_header( &p_extra, &i_extra, &header,
                               opus_get_version_string() ) )
            return VLC_ENOMEM;
        b_dummy_header = true;
    }

    if( xiph_SplitHeaders( pi_size, pp_data, &i_count,
                           i_extra, p_extra ) )
    {
        if( b_dummy_header )
            free( p_extra );
        return VLC_EGENERIC;
    }

    if( i_count < 2 )
    {
        if( b_dummy_header )
            free( p_extra );
        return VLC_EGENERIC;
    }

    oggpacket.granulepos = -1;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Take care of the initial Opus header */
    oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
    oggpacket.bytes  = pi_size[0];
    oggpacket.packet = pp_data[0];
    int ret = ProcessInitialHeader( p_dec, &oggpacket );

    if (ret != VLC_SUCCESS)
        msg_Err( p_dec, "initial Opus header is corrupted" );

    if( b_dummy_header )
        free( p_extra );

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
        p_header->channels>8 ||
        p_header->channel_mapping>1)
    {
        msg_Err( p_dec, "Unsupported channel mapping" );
        return VLC_EGENERIC;
    }

    /* Setup the format */
    p_dec->fmt_out.audio.i_physical_channels =
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
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set( &p_sys->end_date, 0 );
}

/*****************************************************************************
 * ProcessPacket: processes a Opus packet.
 *****************************************************************************/
static block_t *ProcessPacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                               block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_block->i_flags & (BLOCK_FLAG_CORRUPTED|BLOCK_FLAG_DISCONTINUITY) )
    {
        Flush( p_dec );
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            block_Release( p_block );
            return NULL;
        }
    }

    /* Date management */
    if( p_block->i_pts > VLC_TS_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    if( !date_Get( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return NULL;
    }

    block_t *p_aout_buffer = DecodePacket( p_dec, p_oggpacket,
                                           p_block->i_nb_samples,
                                           (int)p_block->i_length );

    block_Release( p_block );
    return p_aout_buffer;
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

    /* Since the information isn't always available at the demux level
     * use the packet's sample number */
    if(!i_nb_samples)
        i_nb_samples = spp;

    if( decoder_UpdateAudioFormat( p_dec ) )
        return NULL;
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

    p_aout_buffer->i_buffer = (i_nb_samples - i_end_trim) *
                              p_sys->header.channels * sizeof(float);

    if( spp > i_nb_samples )
    {
        memmove(p_aout_buffer->p_buffer,
            p_aout_buffer->p_buffer
            + (spp - i_nb_samples)*p_sys->header.channels*sizeof(float),
            p_aout_buffer->i_buffer);
    }
    i_nb_samples -= i_end_trim;

#ifndef OPUS_SET_GAIN
    if(p_sys->header.gain!=0)
    {
        float gain = pow(10., p_sys->header.gain/5120.);
        float *buf =(float *)p_aout_buffer->p_buffer;
        for( int i = 0; i < i_nb_samples*p_sys->header.channels; i++)
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

#ifdef ENABLE_SOUT

/* only ever encode 20 ms at a time, going longer doesn't yield much compression
   gain, shorter does have a compression loss, and doesn't matter so much in
   Ogg, unless you really need low latency, which would also require muxing one
   packet per page. */
static const unsigned OPUS_FRAME_SIZE = 960; /* 48000 * 20 / 1000 */

struct encoder_sys_t
{
    OpusMSEncoder *enc;
    float *buffer;
    unsigned i_nb_samples;
    int i_samples_delay;
    block_t *padding;
    int nb_streams;
};

static unsigned fill_buffer(encoder_t *enc, unsigned src_start, block_t *src,
                            unsigned samples)
{
    encoder_sys_t *p_sys = enc->p_sys;
    const unsigned channels = enc->fmt_out.audio.i_channels;
    const float *src_buf = ((const float *) src->p_buffer) + src_start;
    float *dest_buf = p_sys->buffer + (p_sys->i_nb_samples * channels);
    const unsigned len = samples * channels;

    memcpy(dest_buf, src_buf, len * sizeof(float));

    p_sys->i_nb_samples += samples;
    src_start += len;

    src->i_nb_samples -= samples;
    return src_start;
}

static block_t *Encode(encoder_t *enc, block_t *buf)
{
    encoder_sys_t *sys = enc->p_sys;

    if (!buf)
        return NULL;

    mtime_t i_pts = buf->i_pts -
                (mtime_t) CLOCK_FREQ * (mtime_t) sys->i_samples_delay /
                (mtime_t) enc->fmt_in.audio.i_rate;

    sys->i_samples_delay += buf->i_nb_samples;

    block_t *result = NULL;
    unsigned src_start = 0;
    unsigned padding_start = 0;
    /* The maximum Opus frame size is 1275 bytes + TOC sequence length. */
    const unsigned OPUS_MAX_ENCODED_BYTES = ((1275 + 3) * sys->nb_streams) - 2;

    while (sys->i_nb_samples + buf->i_nb_samples >= OPUS_FRAME_SIZE)
    {
        block_t *out_block = block_Alloc(OPUS_MAX_ENCODED_BYTES);

        /* add padding to beginning */
        if (sys->padding)
        {
            const size_t leftover_space = OPUS_FRAME_SIZE - sys->i_nb_samples;
            padding_start = fill_buffer(enc, padding_start, sys->padding,
                    __MIN(sys->padding->i_nb_samples, leftover_space));
            if (sys->padding->i_nb_samples <= 0)
            {
                block_Release(sys->padding);
                sys->padding = NULL;
            }
        }

        /* padding may have been freed either before or inside previous
         * if-statement */
        if (!sys->padding)
        {
            const size_t leftover_space = OPUS_FRAME_SIZE - sys->i_nb_samples;
            src_start = fill_buffer(enc, src_start, buf,
                    __MIN(buf->i_nb_samples, leftover_space));
        }

        opus_int32 bytes_encoded = opus_multistream_encode_float(sys->enc, sys->buffer,
                OPUS_FRAME_SIZE, out_block->p_buffer, out_block->i_buffer);

        if (bytes_encoded < 0)
        {
            block_Release(out_block);
        }
        else
        {
            out_block->i_length = (mtime_t) CLOCK_FREQ *
                (mtime_t) OPUS_FRAME_SIZE / (mtime_t) enc->fmt_in.audio.i_rate;

            out_block->i_dts = out_block->i_pts = i_pts;

            sys->i_samples_delay -= OPUS_FRAME_SIZE;

            i_pts += out_block->i_length;

            sys->i_nb_samples = 0;

            out_block->i_buffer = bytes_encoded;
            block_ChainAppend(&result, out_block);
        }
    }

    /* put leftover samples at beginning of buffer */
    if (buf->i_nb_samples > 0)
        fill_buffer(enc, src_start, buf, buf->i_nb_samples);

    return result;
}

static int OpenEncoder(vlc_object_t *p_this)
{
    encoder_t *enc = (encoder_t *)p_this;

    if (enc->fmt_out.i_codec != VLC_CODEC_OPUS)
        return VLC_EGENERIC;

    encoder_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    int status = VLC_SUCCESS;
    sys->buffer = NULL;
    sys->enc = NULL;

    enc->pf_encode_audio = Encode;
    enc->fmt_in.i_codec = VLC_CODEC_FL32;
    enc->fmt_in.audio.i_rate = /* Only 48kHz */
    enc->fmt_out.audio.i_rate = 48000;
    enc->fmt_out.audio.i_channels = enc->fmt_in.audio.i_channels;

    OpusHeader header;

    opus_prepare_header(enc->fmt_out.audio.i_channels,
            enc->fmt_out.audio.i_rate, &header);

    /* needed for max encoded size calculation */
    sys->nb_streams = header.nb_streams;

    int err;
    sys->enc =
        opus_multistream_surround_encoder_create(enc->fmt_in.audio.i_rate,
                enc->fmt_in.audio.i_channels, header.channel_mapping,
                &header.nb_streams, &header.nb_coupled, header.stream_map,
                OPUS_APPLICATION_AUDIO, &err);

    if (err != OPUS_OK)
    {
        msg_Err(enc, "Could not create encoder: error %d", err);
        sys->enc = NULL;
        status = VLC_EGENERIC;
        goto error;
    }

    /* TODO: vbr, fec */

    if( enc->fmt_out.i_bitrate )
        opus_multistream_encoder_ctl(sys->enc, OPUS_SET_BITRATE( enc->fmt_out.i_bitrate ));

    /* Buffer for incoming audio, since opus only accepts frame sizes that are
       multiples of 2.5ms */
    enc->p_sys = sys;
    sys->buffer = vlc_alloc(header.channels, sizeof(float) * OPUS_FRAME_SIZE);
    if (!sys->buffer) {
        status = VLC_ENOMEM;
        goto error;
    }

    sys->i_nb_samples = 0;

    sys->i_samples_delay = 0;
    int ret = opus_multistream_encoder_ctl(enc->p_sys->enc,
            OPUS_GET_LOOKAHEAD(&sys->i_samples_delay));
    if (ret != OPUS_OK)
        msg_Err(enc, "Unable to get number of lookahead samples: %s\n",
                opus_strerror(ret));

    header.preskip = sys->i_samples_delay;

    /* Now that we have preskip, we can write the header to extradata */
    if (opus_write_header((uint8_t **) &enc->fmt_out.p_extra,
                          &enc->fmt_out.i_extra, &header, opus_get_version_string()))
    {
        msg_Err(enc, "Failed to write header.");
        status = VLC_ENOMEM;
        goto error;
    }

    if (sys->i_samples_delay > 0)
    {
        const unsigned padding_samples = sys->i_samples_delay *
            enc->fmt_out.audio.i_channels;
        sys->padding = block_Alloc(padding_samples * sizeof(float));
        if (!sys->padding) {
            status = VLC_ENOMEM;
            goto error;
        }
        sys->padding->i_nb_samples = sys->i_samples_delay;
        float *pad_ptr = (float *) sys->padding->p_buffer;
        memset(pad_ptr, 0, padding_samples * sizeof(float));
    }
    else
    {
        sys->padding = NULL;
    }

    return status;

error:
    if (sys->enc)
        opus_multistream_encoder_destroy(sys->enc);
    free(sys->buffer);
    free(sys);
    return status;
}

static void CloseEncoder(vlc_object_t *p_this)
{
    encoder_t *enc = (encoder_t *)p_this;
    encoder_sys_t *sys = enc->p_sys;

    opus_multistream_encoder_destroy(sys->enc);
    if (sys->padding)
        block_Release(sys->padding);
    free(sys->buffer);
    free(sys);
}
#endif /* ENABLE_SOUT */
