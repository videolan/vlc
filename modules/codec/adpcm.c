/*****************************************************************************
 * adpcm.c : adpcm variant audio decoder
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: adpcm.c,v 1.3 2002/12/30 17:28:31 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 *
 * Documentation: http://www.pcisys.net/~melanson/codecs/adpcm.txt
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include "codecs.h"
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

#define ADPCM_IMA_QT    1
#define ADPCM_IMA_WAV   2
#define ADPCM_MS        3

typedef struct adec_thread_s
{
    int i_codec;

    WAVEFORMATEX    *p_wf;
    
    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;
    int                 i_block;
    uint8_t             *p_block;
    int                 i_samplesperblock;
    
    /* Input properties */
    decoder_fifo_t *p_fifo;
    
    /* Output properties */
    aout_instance_t *   p_aout;       /* opaque */
    aout_input_t *      p_aout_input; /* opaque */
    audio_sample_format_t output_format;

    audio_date_t        date;
    mtime_t             pts;

} adec_thread_t;

static int  OpenDecoder    ( vlc_object_t * );

static int  RunDecoder     ( decoder_fifo_t * );
static int  InitThread     ( adec_thread_t * );
static void DecodeThread   ( adec_thread_t * );
static void EndThread      ( adec_thread_t * );


static void DecodeAdpcmMs( adec_thread_t *, aout_buffer_t * );
static void DecodeAdpcmImaWav( adec_thread_t *, aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("ADPCM audio deocder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();


static int pi_channels_maps[6] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARLEFT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARLEFT 
};

/* Various table from http://www.pcisys.net/~melanson/codecs/adpcm.txt */
static int i_index_table[16] =
{
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static int i_step_table[89] =
{
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static int i_adaptation_table[16] =
{
    230, 230, 230, 230, 307, 409, 512, 614,
    768, 614, 512, 409, 307, 230, 230, 230
};

static int i_adaptation_coeff1[7] =
{
    256, 512, 0, 192, 240, 460, 392
};

static int i_adaptation_coeff2[7] =
{
    0, -256, 0, 64, 0, -208, -232
};


/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;
    
    switch( p_fifo->i_fourcc )
    {   
//        case VLC_FOURCC('i','m','a', '4'): /* IMA ADPCM */
        case VLC_FOURCC('m','s',0x00,0x02): /* MS ADPCM */
        case VLC_FOURCC('m','s',0x00,0x11): /* IMA ADPCM */
//        case VLC_FOURCC('m','s',0x00,0x61): /* Duck DK4 ADPCM */
//        case VLC_FOURCC('m','s',0x00,0x62): /* Duck DK3 ADPCM */

            p_fifo->pf_run = RunDecoder;
            return VLC_SUCCESS;
            
        default:
            return VLC_EGENERIC;
    }

}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    adec_thread_t *p_adec;
    int b_error;

    if( !( p_adec = malloc( sizeof( adec_thread_t ) ) ) )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    memset( p_adec, 0, sizeof( adec_thread_t ) );
    
    p_adec->p_fifo = p_fifo;

    if( InitThread( p_adec ) != 0 )
    {
        DecoderError( p_fifo );
        return( -1 );
    }

    while( ( !p_adec->p_fifo->b_die )&&( !p_adec->p_fifo->b_error ) )
    {
        DecodeThread( p_adec );
    }


    if( ( b_error = p_adec->p_fifo->b_error ) )
    {
        DecoderError( p_adec->p_fifo );
    }

    EndThread( p_adec );
    if( b_error )
    {
        return( -1 );
    }

    return( 0 );
}


#define FREE( p ) if( p ) free( p ); p = NULL
#define GetWLE( p ) \
    ( *(u8*)(p) + ( *((u8*)(p)+1) << 8 ) )

#define GetDWLE( p ) \
    (  *(u8*)(p) + ( *((u8*)(p)+1) << 8 ) + \
        ( *((u8*)(p)+2) << 16 ) + ( *((u8*)(p)+3) << 24 ) )

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/

static int InitThread( adec_thread_t * p_adec )
{
    if( !( p_adec->p_wf = (WAVEFORMATEX*)p_adec->p_fifo->p_demux_data ) )
    {
        msg_Err( p_adec->p_fifo, "missing format" );
        return( -1 );
    }
    /* fourcc to codec */
    switch( p_adec->p_fifo->i_fourcc )
    {
        case VLC_FOURCC('i','m','a', '4'): /* IMA ADPCM */
            p_adec->i_codec = ADPCM_IMA_QT;
            break;
        case VLC_FOURCC('m','s',0x00,0x11): /* IMA ADPCM */
            p_adec->i_codec = ADPCM_IMA_WAV;
            break;
        case VLC_FOURCC('m','s',0x00,0x02): /* MS ADPCM */
            p_adec->i_codec = ADPCM_MS;
            break;
        case VLC_FOURCC('m','s',0x00,0x61): /* Duck DK4 ADPCM */
        case VLC_FOURCC('m','s',0x00,0x62): /* Duck DK3 ADPCM */
            p_adec->i_codec = 0;
            break;
    }

    if( p_adec->p_wf->nChannels < 1 || 
            p_adec->p_wf->nChannels > 2 )
    {
        msg_Err( p_adec->p_fifo, "bad channels count(1-2)" );
        return( -1 );
    }
    if( !( p_adec->i_block = p_adec->p_wf->nBlockAlign ) )
    {
        if( p_adec->i_codec == ADPCM_IMA_QT )
        {
            p_adec->i_block = 34;
        }
        else
        {
            p_adec->i_block = 1024; // XXX FIXME
        }
        msg_Err( p_adec->p_fifo,
                 "block size undefined, using %d default", 
                 p_adec->i_block );
    }
    p_adec->p_block = malloc( p_adec->i_block );

    /* calculate samples per block */
    switch( p_adec->i_codec )
    {
        case ADPCM_IMA_QT:
            p_adec->i_samplesperblock = 64;
            break;
        case ADPCM_IMA_WAV:
            p_adec->i_samplesperblock = 
                 2 * ( p_adec->i_block - 4 * p_adec->p_wf->nChannels )/
                 p_adec->p_wf->nChannels;
                 break;
        case ADPCM_MS:
            p_adec->i_samplesperblock = 
                2 * ( p_adec->i_block - 7 * p_adec->p_wf->nChannels ) / 
                p_adec->p_wf->nChannels + 2;
            break;
        default:
            p_adec->i_samplesperblock = 0;
    }
   
    msg_Dbg( p_adec->p_fifo,
             "format: samplerate:%dHz channels:%d bits/sample:%d blockalign:%d samplesperblock %d",
             p_adec->p_wf->nSamplesPerSec,
             p_adec->p_wf->nChannels,
             p_adec->p_wf->wBitsPerSample, 
             p_adec->p_wf->nBlockAlign,
             p_adec->i_samplesperblock );
    
    //p_adec->output_format.i_format = VLC_FOURCC('s','1','6','l');
    /* FIXME good way ? */
    p_adec->output_format.i_format = AOUT_FMT_S16_NE;
    p_adec->output_format.i_rate = p_adec->p_wf->nSamplesPerSec;


    p_adec->output_format.i_physical_channels = 
        p_adec->output_format.i_original_channels =
            pi_channels_maps[p_adec->p_wf->nChannels];
    p_adec->output_format.i_bytes_per_frame =
        p_adec->p_wf->nChannels * 2;
    p_adec->output_format.i_frame_length = 1;
    p_adec->p_aout = NULL;
    p_adec->p_aout_input = NULL;

    /* **** Create a new audio output **** */
    aout_DateInit( &p_adec->date, p_adec->output_format.i_rate );
    p_adec->p_aout_input = aout_DecNew( p_adec->p_fifo,
                                        &p_adec->p_aout,
                                        &p_adec->output_format );
    if( !p_adec->p_aout_input )
    {
        msg_Err( p_adec->p_fifo, "cannot create aout" );
        return( -1 );
    }

    /* Init the BitStream */
    InitBitstream( &p_adec->bit_stream, p_adec->p_fifo,
                   NULL, NULL );

    return( 0 );
}


/*****************************************************************************
 * DecodeThread: decodes a frame
 *****************************************************************************/
static void DecodeThread( adec_thread_t *p_adec )
{
    aout_buffer_t   *p_aout_buffer;

    /* get pts */
    CurrentPTS( &p_adec->bit_stream, &p_adec->pts, NULL );
    /* gather block */
    GetChunk( &p_adec->bit_stream,
              p_adec->p_block,
              p_adec->i_block );

    /* get output buffer */
    if( p_adec->pts != 0 && p_adec->pts != aout_DateGet( &p_adec->date ) )
    {
        aout_DateSet( &p_adec->date, p_adec->pts );
    }
    else if( !aout_DateGet( &p_adec->date ) )
    {
        return;
    }

    p_aout_buffer = aout_DecNewBuffer( p_adec->p_aout, 
                                       p_adec->p_aout_input,
                                       p_adec->i_samplesperblock );
    if( !p_aout_buffer )
    {
        msg_Err( p_adec->p_fifo, "cannot get aout buffer" );
        p_adec->p_fifo->b_error = 1;
        return;
    }
    
    p_aout_buffer->start_date = aout_DateGet( &p_adec->date );
    p_aout_buffer->end_date = aout_DateIncrement( &p_adec->date,
                                                  p_adec->i_samplesperblock );

    /* decode */
    
    switch( p_adec->i_codec )
    {
        case ADPCM_IMA_QT:
            break;
        case ADPCM_IMA_WAV:
            DecodeAdpcmImaWav( p_adec, p_aout_buffer );
            break;
        case ADPCM_MS:
            DecodeAdpcmMs( p_adec, p_aout_buffer );
            break;
        default:
            break;
    }


    /* **** Now we can output these samples **** */
    aout_DecPlay( p_adec->p_aout, p_adec->p_aout_input, p_aout_buffer );
}


/*****************************************************************************
 * EndThread : faad decoder thread destruction
 *****************************************************************************/
static void EndThread (adec_thread_t *p_adec)
{
    if( p_adec->p_aout_input )
    {
        aout_DecDelete( p_adec->p_aout, p_adec->p_aout_input );
    }

    msg_Dbg( p_adec->p_fifo, "adpcm audio decoder closed" );
        
    free( p_adec->p_block );
    free( p_adec );
}
#define CLAMP( v, min, max ) \
    if( (v) < (min) ) (v) = (min); \
    if( (v) > (max) ) (v) = (max)

#define GetByte( v ) \
    (v) = *p_buffer; p_buffer++;

#define GetWord( v ) \
    (v) = *p_buffer; p_buffer++; \
    (v) |= ( *p_buffer ) << 8; p_buffer++; \
    if( (v)&0x8000 ) (v) -= 0x010000;

typedef struct adpcm_ms_channel_s
{
    int i_idelta; 
    int i_sample1, i_sample2;
    int i_coeff1, i_coeff2;

} adpcm_ms_channel_t;


static int AdpcmMsExpandNibble(adpcm_ms_channel_t *p_channel,
                               int i_nibble )
{
    int i_predictor;
    int i_snibble;
    /* expand sign */

    i_snibble = i_nibble - ( i_nibble&0x08 ? 0x10 : 0 );
    
    i_predictor = ( p_channel->i_sample1 * p_channel->i_coeff1 + 
                    p_channel->i_sample2 * p_channel->i_coeff2 ) / 256 +
                  i_snibble * p_channel->i_idelta;

    CLAMP( i_predictor, -32768, 32767 );

    p_channel->i_sample2 = p_channel->i_sample1;
    p_channel->i_sample1 = i_predictor;

    p_channel->i_idelta = ( i_adaptation_table[i_nibble] * 
                            p_channel->i_idelta ) / 256;
    if( p_channel->i_idelta < 16 )
    {
        p_channel->i_idelta = 16;
    }
    return( i_predictor );
}
    
static void DecodeAdpcmMs( adec_thread_t *p_adec,  
                           aout_buffer_t *p_aout_buffer)
{
    uint8_t            *p_buffer;
    adpcm_ms_channel_t channel[2];
    int i_nibbles;
    uint16_t           *p_sample;
    int b_stereo;
    int i_block_predictor;
    
    p_buffer = p_adec->p_block;
    b_stereo = p_adec->p_wf->nChannels == 2 ? 1 : 0;

    GetByte( i_block_predictor );
    CLAMP( i_block_predictor, 0, 6 );
    channel[0].i_coeff1 = i_adaptation_coeff1[i_block_predictor];
    channel[0].i_coeff2 = i_adaptation_coeff2[i_block_predictor];

    if( b_stereo )
    {
        GetByte( i_block_predictor );
        CLAMP( i_block_predictor, 0, 6 );
        channel[1].i_coeff1 = i_adaptation_coeff1[i_block_predictor];
        channel[1].i_coeff2 = i_adaptation_coeff2[i_block_predictor];
    }
    GetWord( channel[0].i_idelta );
    if( b_stereo )
    {
        GetWord( channel[1].i_idelta );
    }
    
    GetWord( channel[0].i_sample1 );
    if( b_stereo )
    {
        GetWord( channel[1].i_sample1 );
    }

    GetWord( channel[0].i_sample2 );
    if( b_stereo )
    {
        GetWord( channel[1].i_sample2 );
    }

    p_sample = (int16_t*)p_aout_buffer->p_buffer;

    if( b_stereo )
    {
        *p_sample = channel[0].i_sample2; p_sample++;
        *p_sample = channel[1].i_sample2; p_sample++;
        *p_sample = channel[0].i_sample1; p_sample++;
        *p_sample = channel[1].i_sample1; p_sample++;
    }
    else
    {
        *p_sample = channel[0].i_sample2; p_sample++;
        *p_sample = channel[0].i_sample1; p_sample++;
    }

    for( i_nibbles =  2 *( p_adec->i_block - 7 * p_adec->p_wf->nChannels );
         i_nibbles > 0; i_nibbles -= 2,p_buffer++ )
    {
        *p_sample = AdpcmMsExpandNibble( &channel[0], (*p_buffer) >> 4);
        p_sample++;
        
        *p_sample = AdpcmMsExpandNibble( &channel[b_stereo ? 1 : 0], 
                                         (*p_buffer)&0x0f);
        p_sample++;
    }

    
}

typedef struct adpcm_ima_wav_channel_s
{
    int i_predictor;
    int i_step_index;

} adpcm_ima_wav_channel_t;

static int AdpcmImaWavExpandNibble(adpcm_ima_wav_channel_t *p_channel,
                                   int i_nibble )
{
    int i_diff;

    i_diff = i_step_table[p_channel->i_step_index] >> 3;
    if( i_nibble&0x04 ) i_diff += i_step_table[p_channel->i_step_index];
    if( i_nibble&0x02 ) i_diff += i_step_table[p_channel->i_step_index]>>1;
    if( i_nibble&0x01 ) i_diff += i_step_table[p_channel->i_step_index]>>2;
    if( i_nibble&0x08 )
        p_channel->i_predictor -= i_diff;
    else
        p_channel->i_predictor += i_diff;

    CLAMP( p_channel->i_predictor, -32768, 32767 );

    p_channel->i_step_index += i_index_table[i_nibble];

    CLAMP( p_channel->i_step_index, 0, 88 );

    return( p_channel->i_predictor );
}

static void DecodeAdpcmImaWav( adec_thread_t *p_adec,  
                               aout_buffer_t *p_aout_buffer)
{
    uint8_t                 *p_buffer;
    adpcm_ima_wav_channel_t channel[2];
    int                     i_nibbles;
    uint16_t                *p_sample;
    int                     b_stereo;
    
    p_buffer = p_adec->p_block;
    b_stereo = p_adec->p_wf->nChannels == 2 ? 1 : 0;

    GetWord( channel[0].i_predictor );
    GetByte( channel[0].i_step_index );
    CLAMP( channel[0].i_step_index, 0, 88 );
    p_buffer++;

    if( b_stereo )
    {
        GetWord( channel[1].i_predictor );
        GetByte( channel[1].i_step_index );
        CLAMP( channel[1].i_step_index, 0, 88 );
        p_buffer++;
    }
    
    p_sample = (int16_t*)p_aout_buffer->p_buffer;
    if( b_stereo )
    {
        for( i_nibbles = 2 * (p_adec->i_block - 8); 
             i_nibbles > 0; 
             i_nibbles -= 16 )
        {
            int i;

            for( i = 0; i < 4; i++ )
            {
                p_sample[i * 4] = 
                    AdpcmImaWavExpandNibble(&channel[0],p_buffer[i]&0x0f);
                p_sample[i * 4 + 2] =
                    AdpcmImaWavExpandNibble(&channel[0],p_buffer[i] >> 4);
            }
            p_buffer += 4;
            
            for( i = 0; i < 4; i++ )
            {
                p_sample[i * 4 + 1] = 
                    AdpcmImaWavExpandNibble(&channel[1],p_buffer[i]&0x0f);
                p_sample[i * 4 + 3] =
                    AdpcmImaWavExpandNibble(&channel[1],p_buffer[i] >> 4);
            }
            p_buffer += 4;
            p_sample += 16;

        }


    }
    else
    {
        for( i_nibbles = 2 * (p_adec->i_block - 4); 
             i_nibbles > 0; 
             i_nibbles -= 2, p_buffer++ )
        {
            *p_sample =AdpcmImaWavExpandNibble( &channel[0], (*p_buffer)&0x0f );
            p_sample++;
            *p_sample =AdpcmImaWavExpandNibble( &channel[0], (*p_buffer) >> 4 );
            p_sample++;
        }
    }
}




