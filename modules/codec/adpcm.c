/*****************************************************************************
 * adpcm.c : adpcm variant audio decoder
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Rémi Denis-Courmont
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
 *
 * Documentation: http://www.pcisys.net/~melanson/codecs/adpcm.txt
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder( vlc_object_t * );
static void CloseDecoder( vlc_object_t * );

static int DecodeAudio( decoder_t *, block_t * );
static void Flush( decoder_t * );

vlc_module_begin ()
    set_description( N_("ADPCM audio decoder") )
    set_capability( "audio decoder", 50 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_callbacks( OpenDecoder, CloseDecoder )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
enum adpcm_codec_e
{
    ADPCM_IMA_QT,
    ADPCM_IMA_WAV,
    ADPCM_MS,
    ADPCM_DK3,
    ADPCM_DK4,
    ADPCM_EA
};

typedef struct
{
    enum adpcm_codec_e codec;

    size_t              i_block;
    size_t              i_samplesperblock;

    date_t              end_date;
    int16_t            *prev;
} decoder_sys_t;

static void DecodeAdpcmMs    ( decoder_t *, int16_t *, uint8_t * );
static void DecodeAdpcmImaWav( decoder_t *, int16_t *, uint8_t * );
static void DecodeAdpcmImaQT ( decoder_t *, int16_t *, uint8_t * );
static void DecodeAdpcmDk4   ( decoder_t *, int16_t *, uint8_t * );
static void DecodeAdpcmDk3   ( decoder_t *, int16_t *, uint8_t * );
static void DecodeAdpcmEA    ( decoder_t *, int16_t *, uint8_t * );

/* Various table from http://www.pcisys.net/~melanson/codecs/adpcm.txt */
static const int i_index_table[16] =
{
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static const int i_step_table[89] =
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

static const int i_adaptation_table[16] =
{
    230, 230, 230, 230, 307, 409, 512, 614,
    768, 614, 512, 409, 307, 230, 230, 230
};

static const int i_adaptation_coeff1[7] =
{
    256, 512, 0, 192, 240, 460, 392
};

static const int i_adaptation_coeff2[7] =
{
    0, -256, 0, 64, 0, -208, -232
};

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    switch( p_dec->fmt_in.i_codec )
    {
        case VLC_CODEC_ADPCM_IMA_QT:
        case VLC_CODEC_ADPCM_IMA_WAV:
        case VLC_CODEC_ADPCM_MS:
        case VLC_CODEC_ADPCM_DK4:
        case VLC_CODEC_ADPCM_DK3:
        case VLC_CODEC_ADPCM_XA_EA:
            break;
        default:
            return VLC_EGENERIC;
    }

    if( p_dec->fmt_in.audio.i_rate <= 0 )
    {
        msg_Err( p_dec, "bad samplerate" );
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    p_sys = malloc(sizeof(*p_sys));
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    p_sys->prev = NULL;
    p_sys->i_samplesperblock = 0;

    unsigned i_channels = p_dec->fmt_in.audio.i_channels;
    uint8_t i_max_channels = 5;
    switch( p_dec->fmt_in.i_codec )
    {
        case VLC_CODEC_ADPCM_IMA_QT: /* IMA ADPCM */
            p_sys->codec = ADPCM_IMA_QT;
            i_max_channels = 2;
            break;
        case VLC_CODEC_ADPCM_IMA_WAV: /* IMA ADPCM */
            p_sys->codec = ADPCM_IMA_WAV;
            i_max_channels = 2;
            break;
        case VLC_CODEC_ADPCM_MS: /* MS ADPCM */
            p_sys->codec = ADPCM_MS;
            i_max_channels = 2;
            break;
        case VLC_CODEC_ADPCM_DK4: /* Duck DK4 ADPCM */
            p_sys->codec = ADPCM_DK4;
            i_max_channels = 2;
            break;
        case VLC_CODEC_ADPCM_DK3: /* Duck DK3 ADPCM */
            p_sys->codec = ADPCM_DK3;
            i_max_channels = 2;
            break;
        case VLC_CODEC_ADPCM_XA_EA: /* EA ADPCM */
            p_sys->codec = ADPCM_EA;
            p_sys->prev = calloc( 2 * p_dec->fmt_in.audio.i_channels,
                                  sizeof( int16_t ) );
            if( unlikely(p_sys->prev == NULL) )
            {
                free( p_sys );
                return VLC_ENOMEM;
            }
            break;
    }

    if (i_channels > i_max_channels || i_channels == 0)
    {
        free(p_sys->prev);
        free(p_sys);
        msg_Err( p_dec, "Invalid number of channels %i", p_dec->fmt_in.audio.i_channels );
        return VLC_EGENERIC;
    }

    if( p_dec->fmt_in.audio.i_blockalign <= 0 )
    {
        p_sys->i_block = (p_sys->codec == ADPCM_IMA_QT) ?
            34 * p_dec->fmt_in.audio.i_channels : 1024;
        msg_Warn( p_dec, "block size undefined, using %zu", p_sys->i_block );
    }
    else
    {
        p_sys->i_block = p_dec->fmt_in.audio.i_blockalign;
    }

    /* calculate samples per block */
    switch( p_sys->codec )
    {
    case ADPCM_IMA_QT:
        p_sys->i_samplesperblock = 64;
        break;
    case ADPCM_IMA_WAV:
        if( p_sys->i_block >= 4 * i_channels )
        {
            p_sys->i_samplesperblock = 2 * ( p_sys->i_block - 4 * i_channels )
                                     / i_channels;
        }
        break;
    case ADPCM_MS:
        if( p_sys->i_block >= 7 * i_channels )
        {
            p_sys->i_samplesperblock =
                2 * (p_sys->i_block - 7 * i_channels) / i_channels + 2;
        }
        break;
    case ADPCM_DK4:
        if( p_sys->i_block >= 4 * i_channels )
        {
            p_sys->i_samplesperblock =
                2 * (p_sys->i_block - 4 * i_channels) / i_channels + 1;
        }
        break;
    case ADPCM_DK3:
        i_channels = 2;
        if( p_sys->i_block >= 16 )
            p_sys->i_samplesperblock = ( 4 * ( p_sys->i_block - 16 ) + 2 )/ 3;
        break;
    case ADPCM_EA:
        if( p_sys->i_block >= i_channels )
        {
            p_sys->i_samplesperblock =
                2 * (p_sys->i_block - i_channels) / i_channels;
        }
    }

    msg_Dbg( p_dec, "format: samplerate:%d Hz channels:%d bits/sample:%d "
             "blockalign:%zu samplesperblock:%zu",
             p_dec->fmt_in.audio.i_rate, i_channels,
             p_dec->fmt_in.audio.i_bitspersample, p_sys->i_block,
             p_sys->i_samplesperblock );

    if (p_sys->i_samplesperblock == 0)
    {
        free(p_sys->prev);
        free(p_sys);
        msg_Err( p_dec, "Error computing number of samples per block");
        return VLC_EGENERIC;
    }

    p_dec->p_sys = p_sys;
    p_dec->fmt_out.i_codec = VLC_CODEC_S16N;
    p_dec->fmt_out.audio.i_rate = p_dec->fmt_in.audio.i_rate;
    p_dec->fmt_out.audio.i_channels = i_channels;
    p_dec->fmt_out.audio.i_physical_channels = vlc_chan_maps[i_channels];

    date_Init( &p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1 );

    p_dec->pf_decode = DecodeAudio;
    p_dec->pf_flush  = Flush;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set( &p_sys->end_date, VLC_TICK_INVALID );
}

/*****************************************************************************
 * DecodeBlock:
 *****************************************************************************/
static block_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys  = p_dec->p_sys;
    block_t *p_block;

    if( !*pp_block ) return NULL;

    p_block = *pp_block;

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        Flush( p_dec );
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
            goto drop;
    }

    if( p_block->i_pts != VLC_TICK_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }
    else if( date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
        /* We've just started the stream, wait for the first PTS. */
        goto drop;

    /* Don't re-use the same pts twice */
    p_block->i_pts = VLC_TICK_INVALID;

    if( p_block->i_buffer >= p_sys->i_block )
    {
        block_t *p_out;

        if( decoder_UpdateAudioFormat( p_dec ) )
            goto drop;
        p_out = decoder_NewAudioBuffer( p_dec, p_sys->i_samplesperblock );
        if( p_out == NULL )
            goto drop;

        p_out->i_pts = date_Get( &p_sys->end_date );
        p_out->i_length = date_Increment( &p_sys->end_date,
                                     p_sys->i_samplesperblock ) - p_out->i_pts;

        switch( p_sys->codec )
        {
        case ADPCM_IMA_QT:
            DecodeAdpcmImaQT( p_dec, (int16_t*)p_out->p_buffer,
                              p_block->p_buffer );
            break;
        case ADPCM_IMA_WAV:
            DecodeAdpcmImaWav( p_dec, (int16_t*)p_out->p_buffer,
                               p_block->p_buffer );
            break;
        case ADPCM_MS:
            DecodeAdpcmMs( p_dec, (int16_t*)p_out->p_buffer,
                           p_block->p_buffer );
            break;
        case ADPCM_DK4:
            DecodeAdpcmDk4( p_dec, (int16_t*)p_out->p_buffer,
                            p_block->p_buffer );
            break;
        case ADPCM_DK3:
            DecodeAdpcmDk3( p_dec, (int16_t*)p_out->p_buffer,
                            p_block->p_buffer );
            break;
        case ADPCM_EA:
            DecodeAdpcmEA( p_dec, (int16_t*)p_out->p_buffer,
                           p_block->p_buffer );
        default:
            break;
        }

        p_block->p_buffer += p_sys->i_block;
        p_block->i_buffer -= p_sys->i_block;
        return p_out;
    }

drop:
    block_Release( p_block );
    *pp_block = NULL;
    return NULL;
}

static int DecodeAudio( decoder_t *p_dec, block_t *p_block )
{
    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    block_t **pp_block = &p_block, *p_out;
    while( ( p_out = DecodeBlock( p_dec, pp_block ) ) != NULL )
        decoder_QueueAudio( p_dec, p_out );
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder:
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    free( p_sys->prev );
    free( p_sys );
}

/*****************************************************************************
 * Local functions
 *****************************************************************************/
#define CLAMP( v, min, max ) \
    if( (v) < (min) ) (v) = (min); \
    if( (v) > (max) ) (v) = (max)

#define GetByte( v ) \
    (v) = *p_buffer; p_buffer++;

#define GetWord( v ) \
    (v) = *p_buffer; p_buffer++; \
    (v) |= ( *p_buffer ) << 8; p_buffer++; \
    if( (v)&0x8000 ) (v) -= 0x010000;

/*
 * MS
 */
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

static void DecodeAdpcmMs( decoder_t *p_dec, int16_t *p_sample,
                           uint8_t *p_buffer )
{
    decoder_sys_t *p_sys  = p_dec->p_sys;
    adpcm_ms_channel_t channel[2];
    int b_stereo;
    int i_block_predictor;

    size_t i_total_samples = p_sys->i_samplesperblock;
    if(i_total_samples < 2)
        return;

    b_stereo = p_dec->fmt_out.audio.i_channels == 2 ? 1 : 0;

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

    if( b_stereo )
    {
        *p_sample++ = channel[0].i_sample2;
        *p_sample++ = channel[1].i_sample2;
        *p_sample++ = channel[0].i_sample1;
        *p_sample++ = channel[1].i_sample1;
    }
    else
    {
        *p_sample++ = channel[0].i_sample2;
        *p_sample++ = channel[0].i_sample1;
    }

    for( i_total_samples -= 2; i_total_samples >= 2; i_total_samples -= 2, p_buffer++ )
    {
        *p_sample++ = AdpcmMsExpandNibble( &channel[0], (*p_buffer) >> 4);
        *p_sample++ = AdpcmMsExpandNibble( &channel[b_stereo ? 1 : 0],
                                           (*p_buffer)&0x0f);
    }
}

/*
 * IMA-WAV
 */
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

static void DecodeAdpcmImaWav( decoder_t *p_dec, int16_t *p_sample,
                               uint8_t *p_buffer )
{
    decoder_sys_t *p_sys  = p_dec->p_sys;
    adpcm_ima_wav_channel_t channel[2];
    int                     i_nibbles;
    int                     b_stereo;

    b_stereo = p_dec->fmt_out.audio.i_channels == 2 ? 1 : 0;

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

    if( b_stereo )
    {
        for( i_nibbles = 2 * (p_sys->i_block - 8);
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
        for( i_nibbles = 2 * (p_sys->i_block - 4);
             i_nibbles > 0;
             i_nibbles -= 2, p_buffer++ )
        {
            *p_sample++ =AdpcmImaWavExpandNibble( &channel[0], (*p_buffer)&0x0f );
            *p_sample++ =AdpcmImaWavExpandNibble( &channel[0], (*p_buffer) >> 4 );
        }
    }
}

/*
 * Ima4 in QT file
 */
static void DecodeAdpcmImaQT( decoder_t *p_dec, int16_t *p_sample,
                              uint8_t *p_buffer )
{
    adpcm_ima_wav_channel_t channel[2];
    int                     i_nibbles;
    int                     i_ch;
    int                     i_step;

    i_step = p_dec->fmt_out.audio.i_channels;

    for( i_ch = 0; i_ch < p_dec->fmt_out.audio.i_channels; i_ch++ )
    {
        /* load preambule */
        channel[i_ch].i_predictor  = (int16_t)((( ( p_buffer[0] << 1 )|(  p_buffer[1] >> 7 ) ))<<7);
        channel[i_ch].i_step_index = p_buffer[1]&0x7f;

        CLAMP( channel[i_ch].i_step_index, 0, 88 );
        p_buffer += 2;

        for( i_nibbles = 0; i_nibbles < 64; i_nibbles +=2 )
        {
            *p_sample = AdpcmImaWavExpandNibble( &channel[i_ch], (*p_buffer)&0x0f);
            p_sample += i_step;

            *p_sample = AdpcmImaWavExpandNibble( &channel[i_ch], (*p_buffer >> 4)&0x0f);
            p_sample += i_step;

            p_buffer++;
        }

        /* Next channel */
        p_sample += 1 - 64 * i_step;
    }
}

/*
 * Dk4
 */
static void DecodeAdpcmDk4( decoder_t *p_dec, int16_t *p_sample,
                            uint8_t *p_buffer )
{
    decoder_sys_t *p_sys  = p_dec->p_sys;
    adpcm_ima_wav_channel_t channel[2];
    size_t                  i_nibbles;
    int                     b_stereo;

    b_stereo = p_dec->fmt_out.audio.i_channels == 2 ? 1 : 0;

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

    /* first output predictor */
    *p_sample++ = channel[0].i_predictor;
    if( b_stereo )
    {
        *p_sample++ = channel[1].i_predictor;
    }

    for( i_nibbles = 0;
         i_nibbles < p_sys->i_block - 4 * (b_stereo ? 2:1 );
         i_nibbles++ )
    {
        *p_sample++ = AdpcmImaWavExpandNibble( &channel[0],
                                              (*p_buffer) >> 4);
        *p_sample++ = AdpcmImaWavExpandNibble( &channel[b_stereo ? 1 : 0],
                                               (*p_buffer)&0x0f);

        p_buffer++;
    }
}

/*
 * Dk3
 */
static void DecodeAdpcmDk3( decoder_t *p_dec, int16_t *p_sample,
                            uint8_t *p_buffer )
{
    decoder_sys_t *p_sys  = p_dec->p_sys;
    uint8_t                 *p_end = &p_buffer[p_sys->i_block];
    adpcm_ima_wav_channel_t sum;
    adpcm_ima_wav_channel_t diff;
    int                     i_diff_value;

    p_buffer += 10;

    GetWord( sum.i_predictor );
    GetWord( diff.i_predictor );
    GetByte( sum.i_step_index );
    GetByte( diff.i_step_index );

    i_diff_value = diff.i_predictor;
    /* we process 6 nibbles at once */
    while( p_buffer + 1 <= p_end )
    {
        /* first 3 nibbles */
        AdpcmImaWavExpandNibble( &sum,
                                 (*p_buffer)&0x0f);

        AdpcmImaWavExpandNibble( &diff,
                                 (*p_buffer) >> 4 );

        i_diff_value = ( i_diff_value + diff.i_predictor ) / 2;

        *p_sample++ = sum.i_predictor + i_diff_value;
        *p_sample++ = sum.i_predictor - i_diff_value;

        p_buffer++;

        AdpcmImaWavExpandNibble( &sum,
                                 (*p_buffer)&0x0f);

        *p_sample++ = sum.i_predictor + i_diff_value;
        *p_sample++ = sum.i_predictor - i_diff_value;

        /* now last 3 nibbles */
        AdpcmImaWavExpandNibble( &sum,
                                 (*p_buffer)>>4);
        p_buffer++;
        if( p_buffer < p_end )
        {
            AdpcmImaWavExpandNibble( &diff,
                                     (*p_buffer)&0x0f );

            i_diff_value = ( i_diff_value + diff.i_predictor ) / 2;

            *p_sample++ = sum.i_predictor + i_diff_value;
            *p_sample++ = sum.i_predictor - i_diff_value;

            AdpcmImaWavExpandNibble( &sum,
                                     (*p_buffer)>>4);
            p_buffer++;

            *p_sample++ = sum.i_predictor + i_diff_value;
            *p_sample++ = sum.i_predictor - i_diff_value;
        }
    }
}


/*
 * EA ADPCM
 */
#define MAX_CHAN 5
static void DecodeAdpcmEA( decoder_t *p_dec, int16_t *p_sample,
                           uint8_t *p_buffer )
{
    static const int16_t EATable[]=
    {
        0x0000, 0x00F0, 0x01CC, 0x0188, 0x0000, 0x0000, 0xFF30, 0xFF24,
        0x0000, 0x0001, 0x0003, 0x0004, 0x0007, 0x0008, 0x000A, 0x000B,
        0x0000, 0xFFFF, 0xFFFD, 0xFFFC,
    };
    decoder_sys_t *p_sys  = p_dec->p_sys;
    int_fast32_t c1[MAX_CHAN], c2[MAX_CHAN];
    int_fast8_t d[MAX_CHAN];

    unsigned chans = p_dec->fmt_out.audio.i_channels;
    const uint8_t *p_end = &p_buffer[p_sys->i_block];
    int16_t *prev = p_sys->prev;
    int16_t *cur = prev + chans;

    for (unsigned c = 0; c < chans; c++)
    {
        uint8_t input = p_buffer[c];

        c1[c] = EATable[input >> 4];
        c2[c] = EATable[(input >> 4) + 4];
        d[c] = (input & 0xf) + 8;
    }

    for (p_buffer += chans; p_buffer < p_end; p_buffer += chans)
    {
        union { uint32_t u; int32_t i; } spl;

        for (unsigned c = 0; c < chans; c++)
        {
            spl.u = (p_buffer[c] & 0xf0u) << 24u;
            spl.i >>= d[c];
            spl.i = (spl.i + cur[c] * c1[c] + prev[c] * c2[c] + 0x80) >> 8;
            CLAMP(spl.i, -32768, 32767);
            prev[c] = cur[c];
            cur[c] = spl.i;

            *(p_sample++) = spl.i;
        }

        for (unsigned c = 0; c < chans; c++)
        {
            spl.u = (p_buffer[c] & 0x0fu) << 28u;
            spl.i >>= d[c];
            spl.i = (spl.i + cur[c] * c1[c] + prev[c] * c2[c] + 0x80) >> 8;
            CLAMP(spl.i, -32768, 32767);
            prev[c] = cur[c];
            cur[c] = spl.i;

            *(p_sample++) = spl.i;
        }
    }
}
