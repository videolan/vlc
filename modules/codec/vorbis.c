/*****************************************************************************
 * vorbis.c: vorbis decoder module making use of libvorbis.
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: vorbis.c,v 1.19 2003/09/28 16:50:05 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>
#include <input_ext-dec.h>

#include <vlc/input.h>

#include <ogg/ogg.h>
#ifdef MODULE_NAME_IS_tremor
#include <tremor/ivorbiscodec.h>
#else
#include <vorbis/codec.h>
#endif

/*****************************************************************************
 * decoder_sys_t : vorbis decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    vlc_bool_t b_packetizer;

    /*
     * Input properties
     */
    int i_headers;

    /*
     * Vorbis properties
     */
    vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                            settings */
    vorbis_comment   vc; /* struct that stores all the bitstream user
                          * comments */
    vorbis_dsp_state vd; /* central working state for the packet->PCM
                          * decoder */
    vorbis_block     vb; /* local working space for packet->PCM decode */

    /*
     * Output properties
     */
    aout_instance_t        *p_aout;
    aout_input_t           *p_aout_input;
    audio_sample_format_t   aout_format;

    /*
     * Packetizer output properties
     */
    sout_packetizer_input_t *p_sout_input;
    sout_format_t           sout_format;

    /*
     * Common properties
     */
    audio_date_t          end_date;
    int                   i_last_block_size;

};

static int pi_channels_maps[6] =
{
    0,
    AOUT_CHAN_CENTER,   AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder   ( vlc_object_t * );
static int OpenPacketizer( vlc_object_t * );

static int InitDecoder   ( decoder_t * );
static int RunDecoder    ( decoder_t *, block_t * );
static int EndDecoder    ( decoder_t * );

static int ProcessPacket ( decoder_t *, ogg_packet *, mtime_t );
static int DecodePacket  ( decoder_t *, ogg_packet * );
static int SendPacket    ( decoder_t *, ogg_packet * );

static void ParseVorbisComments( decoder_t * );

#ifdef MODULE_NAME_IS_tremor
static void Interleave   ( int32_t *, const int32_t **, int, int );
#else
static void Interleave   ( float *, const float **, int, int );
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Vorbis audio decoder") );
#ifdef MODULE_NAME_IS_tremor
    set_capability( "decoder", 90 );
#else
    set_capability( "decoder", 100 );
#endif
    set_callbacks( OpenDecoder, NULL );

    add_submodule();
    set_description( _("Vorbis audio packetizer") );
    set_capability( "packetizer", 100 );
    set_callbacks( OpenPacketizer, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC('v','o','r','b') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_init = InitDecoder;
    p_dec->pf_decode = RunDecoder;
    p_dec->pf_end = EndDecoder;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }
    p_dec->p_sys->b_packetizer = VLC_FALSE;

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS ) p_dec->p_sys->b_packetizer = VLC_TRUE;

    return i_ret;
}

/*****************************************************************************
 * InitDecoder: Initalize the decoder
 *****************************************************************************/
static int InitDecoder( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    aout_DateSet( &p_sys->end_date, 0 );

    p_sys->p_aout = NULL;
    p_sys->p_aout_input = NULL;
    p_sys->aout_format.i_format = VLC_FOURCC('v','o','r','b');

    p_sys->p_sout_input = NULL;
    p_sys->sout_format.i_cat = AUDIO_ES;
    p_sys->sout_format.i_fourcc = VLC_FOURCC( 'v', 'o', 'r', 'b' );
    p_sys->sout_format.i_block_align = 0;
    p_sys->sout_format.i_bitrate     = 0;
    p_sys->sout_format.i_extra_data  = 0;
    p_sys->sout_format.p_extra_data  = NULL;

    /* Take care of vorbis init */
    vorbis_info_init( &p_sys->vi );
    vorbis_comment_init( &p_sys->vc );

    p_sys->i_headers = 0;

    return VLC_SUCCESS;
}

/****************************************************************************
 * RunDecoder: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static int RunDecoder( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;
    int i_ret;

    /* Block to Ogg packet */
    oggpacket.packet = p_block->p_buffer;
    oggpacket.bytes = p_block->i_buffer;
    oggpacket.granulepos = -1;
    oggpacket.b_o_s = 0;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    if( p_sys->i_headers == 0 )
    {
        /* Take care of the initial Vorbis header */

        oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
        if( vorbis_synthesis_headerin( &p_sys->vi, &p_sys->vc,
                                       &oggpacket ) < 0 )
        {
            msg_Err( p_dec->p_fifo, "This bitstream does not contain Vorbis "
                     "audio data");
            block_Release( p_block );
            return VLC_EGENERIC;
        }
        p_sys->i_headers++;

 
        if( p_sys->b_packetizer )
        {
            /* add a input for the stream ouput */
            p_sys->sout_format.i_sample_rate = p_sys->vi.rate;
            p_sys->sout_format.i_channels    = p_sys->vi.channels;
            p_sys->sout_format.i_block_align = 1;
            p_sys->sout_format.i_bitrate     = p_sys->vi.bitrate_nominal;

            p_sys->p_sout_input =
                sout_InputNew( p_dec, &p_sys->sout_format );

            if( !p_sys->p_sout_input )
            {
                msg_Err( p_dec, "cannot add a new stream" );
                block_Release( p_block );
                return VLC_EGENERIC;
            }
        }
        else
        {
#ifdef MODULE_NAME_IS_tremor
            p_sys->aout_format.i_format = VLC_FOURCC('f','i','3','2');
#else
            p_sys->aout_format.i_format = VLC_FOURCC('f','l','3','2');
#endif
            p_sys->aout_format.i_physical_channels =
                p_sys->aout_format.i_original_channels =
                    pi_channels_maps[p_sys->vi.channels];
            p_sys->aout_format.i_rate = p_sys->vi.rate;

            p_sys->p_aout = NULL;
            p_sys->p_aout_input = aout_DecNew( p_dec, &p_sys->p_aout,
                                               &p_sys->aout_format );

            if( p_sys->p_aout_input == NULL )
            {
                msg_Err( p_dec, "failed to create aout fifo" );
                block_Release( p_block );
                return VLC_EGENERIC;
            }
        }

        aout_DateInit( &p_sys->end_date, p_sys->vi.rate );

        msg_Dbg( p_dec, "channels:%d samplerate:%ld bitrate:%ld",
                 p_sys->vi.channels, p_sys->vi.rate,
                 p_sys->vi.bitrate_nominal );

        if( p_sys->b_packetizer )
        {
            i_ret = ProcessPacket( p_dec, &oggpacket, p_block->i_pts );
            block_Release( p_block );
            return i_ret;
        }
        else
        {
            block_Release( p_block );
            return VLC_SUCCESS;
        }
    }

    if( p_sys->i_headers == 1 )
    {
        /* The next packet in order is the comments header */
        if( vorbis_synthesis_headerin( &p_sys->vi, &p_sys->vc, &oggpacket )
            < 0 )
        {
            msg_Err( p_dec, "2nd Vorbis header is corrupted" );
            return VLC_EGENERIC;
        }
        p_sys->i_headers++;
    
        ParseVorbisComments( p_dec );

        if( p_sys->b_packetizer )
        {
            i_ret = ProcessPacket( p_dec, &oggpacket, p_block->i_pts );
            block_Release( p_block );
            return i_ret;
        }
        else
        {
            block_Release( p_block );
            return VLC_SUCCESS;
        }
    }

    if( p_sys->i_headers == 2 )
    {
        /* The next packet in order is the codebooks header
           We need to watch out that this packet is not missing as a
           missing or corrupted header is fatal. */
        if( vorbis_synthesis_headerin( &p_sys->vi, &p_sys->vc, &oggpacket )
            < 0 )
        {
            msg_Err( p_dec, "3rd Vorbis header is corrupted" );
            return VLC_EGENERIC;
        }
        p_sys->i_headers++;
    
        if( !p_sys->b_packetizer )
        {
            /* Initialize the Vorbis packet->PCM decoder */
            vorbis_synthesis_init( &p_sys->vd, &p_sys->vi );
            vorbis_block_init( &p_sys->vd, &p_sys->vb );
        }

        if( p_sys->b_packetizer )
        {
            i_ret = ProcessPacket( p_dec, &oggpacket, p_block->i_pts );
            block_Release( p_block );
            return i_ret;
        }
        else
        {
            block_Release( p_block );
            return VLC_SUCCESS;
        }
    }

    i_ret = ProcessPacket( p_dec, &oggpacket, p_block->i_pts );
    block_Release( p_block );
    return i_ret;
}

/*****************************************************************************
 * ProcessPacket: processes a Vorbis packet.
 *****************************************************************************/
static int ProcessPacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                          mtime_t i_pts )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Date management */
    if( i_pts > 0 && i_pts != aout_DateGet( &p_sys->end_date ) )
    {
        aout_DateSet( &p_sys->end_date, i_pts );
    }

    if( !aout_DateGet( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        return VLC_SUCCESS;
    }

    if( p_sys->b_packetizer )
    {
        return SendPacket( p_dec, p_oggpacket );
    }
    else
    {
        return DecodePacket( p_dec, p_oggpacket );
    }
}

/*****************************************************************************
 * DecodePacket: decodes a Vorbis packet.
 *****************************************************************************/
static int DecodePacket( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int           i_samples;

#ifdef MODULE_NAME_IS_tremor
    int32_t       **pp_pcm;
#else
    float         **pp_pcm;
#endif

    if( vorbis_synthesis( &p_sys->vb, p_oggpacket ) == 0 )
        vorbis_synthesis_blockin( &p_sys->vd, &p_sys->vb );

    /* **pp_pcm is a multichannel float vector. In stereo, for
     * example, pp_pcm[0] is left, and pp_pcm[1] is right. i_samples is
     * the size of each channel. Convert the float values
     * (-1.<=range<=1.) to whatever PCM format and write it out */

    while( ( i_samples = vorbis_synthesis_pcmout( &p_sys->vd, &pp_pcm ) ) > 0 )
    {

        aout_buffer_t *p_aout_buffer;
        p_aout_buffer = aout_DecNewBuffer( p_sys->p_aout, p_sys->p_aout_input,
                                           i_samples );
        if( !p_aout_buffer )
        {
            msg_Err( p_dec, "cannot get aout buffer" );
            return VLC_SUCCESS;
        }

        /* Interleave the samples */
#ifdef MODULE_NAME_IS_tremor
        Interleave( (int32_t *)p_aout_buffer->p_buffer,
                    (const int32_t **)pp_pcm, p_sys->vi.channels, i_samples );
#else
        Interleave( (float *)p_aout_buffer->p_buffer,
                    (const float **)pp_pcm, p_sys->vi.channels, i_samples );
#endif

        /* Tell libvorbis how many samples we actually consumed */
        vorbis_synthesis_read( &p_sys->vd, i_samples );

        /* Date management */
        p_aout_buffer->start_date = aout_DateGet( &p_sys->end_date );
        p_aout_buffer->end_date = aout_DateIncrement( &p_sys->end_date,
                                                      i_samples );

        aout_DecPlay( p_sys->p_aout, p_sys->p_aout_input, p_aout_buffer );
    }

}

/*****************************************************************************
 * SendPacket: send an ogg packet to the stream output.
 *****************************************************************************/
static int SendPacket( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int           i_block_size, i_samples;

    sout_buffer_t *p_sout_buffer =
        sout_BufferNew( p_sys->p_sout_input->p_sout, p_oggpacket->bytes );

    if( !p_sout_buffer ) return VLC_EGENERIC;

    i_block_size = vorbis_packet_blocksize( &p_sys->vi, p_oggpacket );
    if( i_block_size < 0 ) i_block_size = 0; /* non audio packet */
    i_samples = ( p_sys->i_last_block_size + i_block_size ) >> 2;
    p_sys->i_last_block_size = i_block_size;

    p_dec->p_vlc->pf_memcpy( p_sout_buffer->p_buffer,
                             p_oggpacket->packet,
                             p_oggpacket->bytes );

    p_sout_buffer->i_bitrate = p_sys->vi.bitrate_nominal;

    /* Date management */
    p_sout_buffer->i_dts = p_sout_buffer->i_pts =
        aout_DateGet( &p_sys->end_date );

    if( p_sys->i_headers >= 3 )
        p_sout_buffer->i_length =
            aout_DateIncrement( &p_sys->end_date, i_samples ) -
            p_sout_buffer->i_pts;
    else
        p_sout_buffer->i_length = 0;

    sout_InputSendBuffer( p_sys->p_sout_input, p_sout_buffer );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParseVorbisComments: FIXME should be done in demuxer
 *****************************************************************************/
static void ParseVorbisComments( decoder_t *p_dec )
{
    input_thread_t *p_input = (input_thread_t *)p_dec->p_parent;
    input_info_category_t *p_cat =
        input_InfoCategory( p_input, _("Vorbis Comment") );
    int i = 0;
    char *psz_name, *psz_value, *psz_comment;
    while ( i < p_dec->p_sys->vc.comments )
    {
        psz_comment = strdup( p_dec->p_sys->vc.user_comments[i] );
        if( !psz_comment )
        {
            msg_Warn( p_dec, "Out of memory" );
            break;
        }
        psz_name = psz_comment;
        psz_value = strchr( psz_comment, '=' );
        if( psz_value )
        {
            *psz_value = '\0';
            psz_value++;
            input_AddInfo( p_cat, psz_name, psz_value );
        }
        free( psz_comment );
        i++;
    }
}

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
#ifdef MODULE_NAME_IS_tremor
static void Interleave( int32_t *p_out, const int32_t **pp_in,
#else
static void Interleave( float *p_out, const float **pp_in,
#endif
                        int i_nb_channels, int i_samples )
{
    int i, j;

    for ( j = 0; j < i_samples; j++ )
    {
        for ( i = 0; i < i_nb_channels; i++ )
        {
            p_out[j * i_nb_channels + i] = pp_in[i][j];
        }
    }
}

/*****************************************************************************
 * EndDecoder: vorbis decoder destruction
 *****************************************************************************/
static int EndDecoder( decoder_t * p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_aout_input != NULL )
    {
        aout_DecDelete( p_sys->p_aout, p_sys->p_aout_input );
    }

    if( p_sys->p_sout_input != NULL )
    {
        sout_InputDelete( p_sys->p_sout_input );
    }

    if( !p_sys->b_packetizer && p_sys->i_headers >= 3 )
    {
        vorbis_block_clear( &p_sys->vb );
        vorbis_dsp_clear( &p_sys->vd );
    }

    vorbis_comment_clear( &p_sys->vc );
    vorbis_info_clear( &p_sys->vi );  /* must be called last */

    free( p_sys );

    return VLC_SUCCESS;
}
