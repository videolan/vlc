/*****************************************************************************
 * wma.c: wma decoder using integer decoder from Rockbox, based on FFmpeg
 *****************************************************************************
 * Copyright (C) 2008-2009 M2X
 *
 * Authors: Rafaël Carré <rcarre@m2x.nl>
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
#include <vlc_codec.h>
#include <vlc_block_helper.h>
#include <vlc_bits.h>

#include <assert.h>

#include "wmadec.h"

/*****************************************************************************
 * decoder_sys_t : wma decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    date_t end_date; /* To set the PTS */
    WMADecodeContext wmadec; /* name is self explanative */

    int32_t *p_output; /* buffer where the frames are rendered */

    /* to not give too much samples at once to the audio output */
    int8_t *p_samples; /* point into p_output */
    unsigned int i_samples; /* number of buffered samples available */
};

/* FIXME : check supported configurations */
/* channel configuration */
static unsigned int pi_channels_maps[7] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static int DecodeFrame( decoder_t *, block_t * );
static void Flush( decoder_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );
    set_description( N_("WMA v1/v2 fixed point audio decoder") );
    set_capability( "audio decoder", 80 );
    add_shortcut( "wmafixed" )
    set_callbacks( OpenDecoder, CloseDecoder );
vlc_module_end();

/*****************************************************************************
 * SplitBuffer: Needed because aout really doesn't like big audio chunk and
 * wma produces easily > 30000 samples...
 *****************************************************************************/
static block_t *SplitBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    unsigned int i_samples = __MIN( p_sys->i_samples, 2048 );
    block_t *p_buffer;

    if( i_samples == 0 ) return NULL;

    if( decoder_UpdateAudioFormat( p_dec ) )
        return NULL;
    if( !( p_buffer = decoder_NewAudioBuffer( p_dec, i_samples ) ) )
        return NULL;

    p_buffer->i_pts = date_Get( &p_sys->end_date );
    p_buffer->i_length = date_Increment( &p_sys->end_date, i_samples )
                         - p_buffer->i_pts;

    memcpy( p_buffer->p_buffer, p_sys->p_samples, p_buffer->i_buffer );
    p_sys->p_samples += p_buffer->i_buffer;
    p_sys->i_samples -= i_samples;

    return p_buffer;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_WMA1 &&
        p_dec->fmt_in.i_codec != VLC_CODEC_WMA2 )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = (decoder_sys_t *)malloc(sizeof(decoder_sys_t));
    if( !p_sys )
        return VLC_ENOMEM;

    memset( p_sys, 0, sizeof( decoder_sys_t ) );

    /* Date */
    date_Init( &p_sys->end_date, p_dec->fmt_in.audio.i_rate, 1 );

    /* Set output properties */
    p_dec->fmt_out.i_codec = VLC_CODEC_S32N;
    p_dec->fmt_out.audio.i_bitspersample = p_dec->fmt_in.audio.i_bitspersample;
    p_dec->fmt_out.audio.i_rate = p_dec->fmt_in.audio.i_rate;

    p_dec->fmt_out.audio.i_channels = p_dec->fmt_in.audio.i_channels;

    assert( p_dec->fmt_out.audio.i_channels <
        ( sizeof( pi_channels_maps ) / sizeof( pi_channels_maps[0] ) ) );

    p_dec->fmt_out.audio.i_physical_channels =
        pi_channels_maps[p_dec->fmt_out.audio.i_channels];

    /* aout core assumes this number is not 0 and uses it in divisions */
    assert( p_dec->fmt_out.audio.i_physical_channels != 0 );

    asf_waveformatex_t wfx;
    wfx.rate = p_dec->fmt_in.audio.i_rate;
    wfx.bitrate = p_dec->fmt_in.i_bitrate;
    wfx.channels = p_dec->fmt_in.audio.i_channels;
    wfx.blockalign = p_dec->fmt_in.audio.i_blockalign;
    wfx.bitspersample = p_dec->fmt_in.audio.i_bitspersample;

    msg_Dbg( p_dec, "samplerate %d bitrate %d channels %d align %d bps %d",
        wfx.rate, wfx.bitrate, wfx.channels, wfx.blockalign,
        wfx.bitspersample );

    if( p_dec->fmt_in.i_codec == VLC_CODEC_WMA1 )
        wfx.codec_id = ASF_CODEC_ID_WMAV1;
    else if( p_dec->fmt_in.i_codec == VLC_CODEC_WMA2 )
        wfx.codec_id = ASF_CODEC_ID_WMAV2;

    wfx.datalen = p_dec->fmt_in.i_extra;
    if( wfx.datalen > 6 ) wfx.datalen = 6;
    if( wfx.datalen > 0 )
        memcpy( wfx.data, p_dec->fmt_in.p_extra, wfx.datalen );

    /* Init codec */
    if( wma_decode_init(&p_sys->wmadec, &wfx ) < 0 )
    {
        msg_Err( p_dec, "codec init failed" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Set callback */
    p_dec->pf_decode = DecodeFrame;
    p_dec->pf_flush  = Flush;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set( &p_sys->end_date, VLC_TS_INVALID );
}

/*****************************************************************************
 * DecodeFrame: decodes a wma frame.
 *****************************************************************************/
static int DecodeFrame( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_aout_buffer = NULL;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        Flush( p_dec );
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED)
        {
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }
    }

    /* Date management */
    if( p_block->i_pts > VLC_TS_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
        /* don't reuse the same pts */
        p_block->i_pts = VLC_TS_INVALID;
    }
    else if( !date_Get( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    if( wma_decode_superframe_init( &p_sys->wmadec, p_block->p_buffer,
            p_block->i_buffer ) == 0 )
    {
        msg_Err( p_dec, "failed initializing wmafixed decoder" );
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    if( p_sys->wmadec.nb_frames <= 0 )
    {
        msg_Err( p_dec, "can not decode, invalid ASF packet ?" );
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    /* worst case */
    size_t i_buffer = BLOCK_MAX_SIZE * MAX_CHANNELS * p_sys->wmadec.nb_frames;
    free( p_sys->p_output );
    p_sys->p_output = vlc_alloc(i_buffer, sizeof(int32_t));
    p_sys->p_samples = (int8_t*)p_sys->p_output;

    if( !p_sys->p_output )
    {
        /* OOM, will try a bit later if VLC hasn't been killed */
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    p_sys->i_samples = 0;

    for( int i = 0 ; i < p_sys->wmadec.nb_frames; i++ )
    {
        int i_samples = 0;

        i_samples = wma_decode_superframe_frame( &p_sys->wmadec,
                 p_sys->p_output + p_sys->i_samples * p_sys->wmadec.nb_channels,
                 p_block->p_buffer, p_block->i_buffer );

        if( i_samples < 0 )
        {
            msg_Warn( p_dec,
                "wma_decode_superframe_frame() failed for frame %d", i );
            free( p_sys->p_output );
            p_sys->p_output = NULL;
            return VLCDEC_SUCCESS;
        }
        p_sys->i_samples += i_samples; /* advance in the samples buffer */
    }

    block_Release( p_block ); /* this block has been decoded */

    for( size_t s = 0 ; s < i_buffer; s++ )
        p_sys->p_output[s] <<= 2; /* Q30 -> Q32 translation */

    while( ( p_aout_buffer = SplitBuffer( p_dec ) ) != NULL )
        decoder_QueueAudio( p_dec, p_aout_buffer );

    if( !p_sys->i_samples )
    {   /* we need to decode new samples now */
        free( p_sys->p_output );
        p_sys->p_output = NULL;
    }

    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder : wma decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_sys_t *p_sys = ((decoder_t*)p_this)->p_sys;

    free( p_sys->p_output );
    free( p_sys );
}
