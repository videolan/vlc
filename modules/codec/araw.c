/*****************************************************************************
 * araw.c: Pseudo audio decoder; for raw pcm data
 *****************************************************************************
 * Copyright (C) 2001, 2003 VideoLAN
 * $Id: araw.c,v 1.21 2003/11/04 01:27:33 fenrir Exp $
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
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <vlc/vlc.h>

#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include <vlc/sout.h>

#include "codecs.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  DecoderOpen    ( vlc_object_t * );

static int  EncoderOpen  ( vlc_object_t *p_this );
static void EncoderClose ( vlc_object_t *p_this );

vlc_module_begin();
    /* audio decoder module */
    set_description( _("Raw/Log Audio decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( DecoderOpen, NULL );

    /* audio encoder submodule */
    add_submodule();
    set_description( _("Raw audio encoder") );
    set_capability( "audio encoder", 10 );
    set_callbacks( EncoderOpen, EncoderClose );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int DecoderInit  ( decoder_t * );
static int DecoderDecode( decoder_t *, block_t * );
static int DecoderEnd   ( decoder_t * );

struct decoder_sys_t
{
    WAVEFORMATEX    *p_wf;

    int16_t        *p_logtos16;  // used with m/alaw to int16_t

    /* Output properties */
    aout_instance_t *   p_aout;       /* opaque */
    aout_input_t *      p_aout_input; /* opaque */
    audio_sample_format_t output_format;

    audio_date_t        date;
};


static block_t *EncoderEncode( encoder_t *, aout_buffer_t *p_aout_buf );

/*****************************************************************************
 * DecoderOpen: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int DecoderOpen( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    switch( p_dec->p_fifo->i_fourcc )
    {
        case VLC_FOURCC('a','r','a','w'): /* from wav/avi/asf file */
        case VLC_FOURCC('t','w','o','s'): /* _signed_ big endian samples (mov)*/
        case VLC_FOURCC('s','o','w','t'): /* _signed_ little endian samples (mov)*/

        case VLC_FOURCC('a','l','a','w'):
        case VLC_FOURCC('u','l','a','w'):
            p_dec->pf_init   = DecoderInit;
            p_dec->pf_decode = DecoderDecode;
            p_dec->pf_end    = DecoderEnd;

            p_dec->p_sys     = malloc( sizeof( decoder_sys_t ) );
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}

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

static int16_t ulawtos16[256] =
{
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
    -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
    -11900, -11388, -10876, -10364,  -9852,  -9340,  -8828,  -8316,
     -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
     -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
     -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
     -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
     -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
     -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
      -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
      -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
      -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
      -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
      -120,   -112,   -104,    -96,    -88,    -80,    -72,    -64,
       -56,    -48,    -40,    -32,    -24,    -16,     -8,      0,
     32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
     23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
     15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
     11900,  11388,  10876,  10364,   9852,   9340,   8828,   8316,
      7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
      5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
      3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
      2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
      1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
      1372,   1308,   1244,   1180,   1116,   1052,    988,    924,
       876,    844,    812,    780,    748,    716,    684,    652,
       620,    588,    556,    524,    492,    460,    428,    396,
       372,    356,    340,    324,    308,    292,    276,    260,
       244,    228,    212,    196,    180,    164,    148,    132,
       120,    112,    104,     96,     88,     80,     72,     64,
        56,     48,     40,     32,     24,     16,      8,      0
};

static int16_t alawtos16[256] =
{
     -5504,  -5248,  -6016,  -5760,  -4480,  -4224,  -4992,  -4736,
     -7552,  -7296,  -8064,  -7808,  -6528,  -6272,  -7040,  -6784,
     -2752,  -2624,  -3008,  -2880,  -2240,  -2112,  -2496,  -2368,
     -3776,  -3648,  -4032,  -3904,  -3264,  -3136,  -3520,  -3392,
    -22016, -20992, -24064, -23040, -17920, -16896, -19968, -18944,
    -30208, -29184, -32256, -31232, -26112, -25088, -28160, -27136,
    -11008, -10496, -12032, -11520,  -8960,  -8448,  -9984,  -9472,
    -15104, -14592, -16128, -15616, -13056, -12544, -14080, -13568,
      -344,   -328,   -376,   -360,   -280,   -264,   -312,   -296,
      -472,   -456,   -504,   -488,   -408,   -392,   -440,   -424,
       -88,    -72,   -120,   -104,    -24,     -8,    -56,    -40,
      -216,   -200,   -248,   -232,   -152,   -136,   -184,   -168,
     -1376,  -1312,  -1504,  -1440,  -1120,  -1056,  -1248,  -1184,
     -1888,  -1824,  -2016,  -1952,  -1632,  -1568,  -1760,  -1696,
      -688,   -656,   -752,   -720,   -560,   -528,   -624,   -592,
      -944,   -912,  -1008,   -976,   -816,   -784,   -880,   -848,
      5504,   5248,   6016,   5760,   4480,   4224,   4992,   4736,
      7552,   7296,   8064,   7808,   6528,   6272,   7040,   6784,
      2752,   2624,   3008,   2880,   2240,   2112,   2496,   2368,
      3776,   3648,   4032,   3904,   3264,   3136,   3520,   3392,
     22016,  20992,  24064,  23040,  17920,  16896,  19968,  18944,
     30208,  29184,  32256,  31232,  26112,  25088,  28160,  27136,
     11008,  10496,  12032,  11520,   8960,   8448,   9984,   9472,
     15104,  14592,  16128,  15616,  13056,  12544,  14080,  13568,
       344,    328,    376,    360,    280,    264,    312,    296,
       472,    456,    504,    488,    408,    392,    440,    424,
        88,     72,    120,    104,     24,      8,     56,     40,
       216,    200,    248,    232,    152,    136,    184,    168,
      1376,   1312,   1504,   1440,   1120,   1056,   1248,   1184,
      1888,   1824,   2016,   1952,   1632,   1568,   1760,   1696,
       688,    656,    752,    720,    560,    528,    624,    592,
       944,    912,   1008,    976,    816,    784,    880,    848
};

/*****************************************************************************
 * DecoderInit: initialize data before entering main loop
 *****************************************************************************/
static int DecoderInit( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    WAVEFORMATEX *p_wf;
    if( ( p_wf = (WAVEFORMATEX*)p_dec->p_fifo->p_waveformatex ) == NULL )
    {
        msg_Err( p_dec, "unknown raw format" );
        return VLC_EGENERIC;
    }

    if( p_wf->nChannels <= 0 || p_wf->nChannels > 5 )
    {
        msg_Err( p_dec, "bad channels count(1-5)" );
        return VLC_EGENERIC;
    }
    if( p_wf->nSamplesPerSec <= 0 )
    {
        msg_Err( p_dec, "bad samplerate" );
        return VLC_EGENERIC;

    }

    p_sys->p_wf = p_wf;
    p_sys->p_logtos16 = NULL;

    /* fixing some values */
    if( p_wf->nBlockAlign == 0 &&
        ( p_wf->wFormatTag == WAVE_FORMAT_PCM ||
          p_wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ) )
    {
        p_wf->nBlockAlign = ( p_wf->wBitsPerSample + 7 ) / 8 * p_wf->nChannels;
    }

    msg_Dbg( p_dec,
             "samplerate:%dHz channels:%d bits/sample:%d blockalign:%d",
             p_wf->nSamplesPerSec,  p_wf->nChannels,
             p_wf->wBitsPerSample,  p_wf->nBlockAlign );

    if( p_wf->wFormatTag  == WAVE_FORMAT_IEEE_FLOAT )
    {
        switch( ( p_wf->wBitsPerSample + 7 ) / 8 )
        {
            case( 4 ):
                p_sys->output_format.i_format = VLC_FOURCC('f','l','3','2');
                break;
            case( 8 ):
                p_sys->output_format.i_format = VLC_FOURCC('f','l','6','4');
                break;
            default:
                msg_Err( p_dec, "bad parameters(bits/sample)" );
                return VLC_EGENERIC;
        }
    }
    else
    {
        if( p_dec->p_fifo->i_fourcc == VLC_FOURCC( 't', 'w', 'o', 's' ) )
        {
            switch( ( p_wf->wBitsPerSample + 7 ) / 8 )
            {
                case( 1 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','8',' ',' ');
                    break;
                case( 2 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','1','6','b');
                    break;
                case( 3 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','2','4','b');
                    break;
                case( 4 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','3','2','b');
                    break;
                default:
                    msg_Err( p_dec, "bad parameters(bits/sample)" );
                    return VLC_EGENERIC;
            }
        }
        else if( p_dec->p_fifo->i_fourcc == VLC_FOURCC( 's', 'o', 'w', 't' ) )
        {
            switch( ( p_wf->wBitsPerSample + 7 ) / 8 )
            {
                case( 1 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','8',' ',' ');
                    break;
                case( 2 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','1','6','l');
                    break;
                case( 3 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','2','4','l');
                    break;
                case( 4 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','3','2','l');
                    break;
                default:
                    msg_Err( p_dec, "bad parameters(bits/sample)" );
                    return VLC_EGENERIC;
            }
        }
        else if( p_dec->p_fifo->i_fourcc == VLC_FOURCC( 'a', 'r', 'a', 'w' ) )
        {
            switch( ( p_wf->wBitsPerSample + 7 ) / 8 )
            {
                case( 1 ):
                    p_sys->output_format.i_format = VLC_FOURCC('u','8',' ',' ');
                    break;
                case( 2 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','1','6','l');
                    break;
                case( 3 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','2','4','l');
                    break;
                case( 4 ):
                    p_sys->output_format.i_format = VLC_FOURCC('s','3','2','l');
                    break;
                default:
                    msg_Err( p_dec, "bad parameters(bits/sample)" );
                    return VLC_EGENERIC;
            }
        }
        else if( p_dec->p_fifo->i_fourcc == VLC_FOURCC( 'a', 'l', 'a', 'w' ) )
        {
            p_sys->output_format.i_format = AOUT_FMT_S16_NE;
            p_sys->p_logtos16  = alawtos16;
        }
        else if( p_dec->p_fifo->i_fourcc == VLC_FOURCC( 'u', 'l', 'a', 'w' ) )
        {
            p_sys->output_format.i_format = AOUT_FMT_S16_NE;
            p_sys->p_logtos16  = ulawtos16;
        }
    }
    p_sys->output_format.i_rate = p_wf->nSamplesPerSec;

    p_sys->output_format.i_physical_channels =
    p_sys->output_format.i_original_channels = pi_channels_maps[p_wf->nChannels];

    p_sys->p_aout       = NULL;
    p_sys->p_aout_input = aout_DecNew( p_dec, &p_sys->p_aout,
                                       &p_sys->output_format );
    if( p_sys->p_aout_input == NULL )
    {
        msg_Err( p_dec, "cannot create aout" );
        return VLC_EGENERIC;
    }

    aout_DateInit( &p_sys->date, p_sys->output_format.i_rate );
    aout_DateSet( &p_sys->date, 0 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecoderDecode: decodes a frame
 *****************************************************************************/
static int DecoderDecode( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int            i_size = p_block->i_buffer;
    uint8_t        *p = p_block->p_buffer;
    int            i_samples; // per channels

    if( p_sys->p_wf->nBlockAlign > 0 )
    {
        /* Align it (it should already be aligned by demuxer) */
        i_size -= i_size % p_sys->p_wf->nBlockAlign;
    }
    if( i_size < p_sys->p_wf->nBlockAlign )
    {
        return VLC_SUCCESS;
    }
    i_samples = i_size / ( ( p_sys->p_wf->wBitsPerSample + 7 ) / 8 ) /
                p_sys->p_wf->nChannels;

    if( p_block->i_pts != 0 && p_block->i_pts != aout_DateGet( &p_sys->date ) )
    {
        aout_DateSet( &p_sys->date, p_block->i_pts );
    }
    else if( !aout_DateGet( &p_sys->date ) )
    {
        return VLC_SUCCESS;
    }

    while( i_samples > 0 )
    {
        int           i_copy = __MIN( i_samples, 1024 );
        aout_buffer_t *out;

        out = aout_DecNewBuffer( p_sys->p_aout, p_sys->p_aout_input, i_copy );
        if( out == NULL )
        {
            msg_Err( p_dec, "cannot get aout buffer" );
            return VLC_EGENERIC;
        }

        out->start_date = aout_DateGet( &p_sys->date );
        out->end_date   = aout_DateIncrement( &p_sys->date, i_copy );

        if( p_sys->p_logtos16 )
        {
            int16_t      *s = (int16_t*)out->p_buffer;
            unsigned int i;

            for( i = 0; i < out->i_nb_bytes / 2; i++ )
            {
                *s++ = p_sys->p_logtos16[*p++];
            }
        }
        else
        {
            memcpy( out->p_buffer, p, out->i_nb_bytes );

            p += out->i_nb_bytes;
        }

        aout_DecPlay( p_sys->p_aout, p_sys->p_aout_input, out );

        i_samples -= i_copy;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecoderEnd : faad decoder thread destruction
 *****************************************************************************/
static int DecoderEnd( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_aout_input )
    {
        aout_DecDelete( p_sys->p_aout, p_sys->p_aout_input );
    }
    free( p_sys );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EncoderOpen:
 *****************************************************************************/
static int  EncoderOpen  ( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;

    if( p_enc->format.audio.i_format != VLC_FOURCC( 's', '1', '6', 'b' ) &&
        p_enc->format.audio.i_format != VLC_FOURCC( 's', '1', '6', 'l' ) )
    {
        msg_Warn( p_enc, "unhandled input format" );
        return VLC_EGENERIC;
    }

    switch( p_enc->i_fourcc )
    {
        case VLC_FOURCC( 's', '1', '6', 'b' ):
        case VLC_FOURCC( 's', '1', '6', 'l' ):
        case VLC_FOURCC( 'u', '8', ' ', ' ' ):
        case VLC_FOURCC( 's', '8', ' ', ' ' ):
#if 0
        -> could be easyly done with table look up
        case VLC_FOURCC( 'a', 'l', 'a', 'w' ):
        case VLC_FOURCC( 'u', 'l', 'a', 'w' ):
#endif
            break;
        default:
            return VLC_EGENERIC;
    }

    p_enc->p_sys = NULL;
    p_enc->pf_header = NULL;
    p_enc->pf_encode_audio = EncoderEncode;
    p_enc->pf_encode_video = NULL;
    p_enc->i_extra_data = 0;
    p_enc->p_extra_data = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EncoderClose:
 *****************************************************************************/
static void EncoderClose ( vlc_object_t *p_this )
{
    return;
}

/*****************************************************************************
 * EncoderEncode:
 *****************************************************************************/
static block_t *EncoderEncode( encoder_t *p_enc, aout_buffer_t *p_aout_buf )
{
    block_t *p_block = NULL;
    unsigned int i;

    if( p_enc->i_fourcc == p_enc->format.audio.i_format )
    {
        if( ( p_block = block_New( p_enc, p_aout_buf->i_nb_bytes ) ) )
        {
            memcpy( p_block->p_buffer, p_aout_buf->p_buffer, p_aout_buf->i_nb_bytes );
        }
    }
    else if( p_enc->i_fourcc == VLC_FOURCC( 'u', '8', ' ', ' ' ) )
    {
        if( ( p_block = block_New( p_enc, p_aout_buf->i_nb_bytes / 2 ) ) )
        {
            uint8_t *p_dst = (uint8_t*)p_block->p_buffer;
            int8_t  *p_src = (int8_t*) p_aout_buf->p_buffer;

            if( p_enc->format.audio.i_format == VLC_FOURCC( 's', '1', '6', 'l' ) )
            {
                p_src++;
            }

            for( i = 0; i < p_aout_buf->i_nb_bytes / 2; i++ )
            {
                *p_dst++ = *p_src + 128; p_src += 2;
            }
        }
    }
    else if( p_enc->i_fourcc == VLC_FOURCC( 's', '8', ' ', ' ' ) )
    {
        if( ( p_block = block_New( p_enc, p_aout_buf->i_nb_bytes / 2 ) ) )
        {
            int8_t *p_dst = (int8_t*)p_block->p_buffer;
            int8_t *p_src = (int8_t*)p_aout_buf->p_buffer;

            if( p_enc->format.audio.i_format == VLC_FOURCC( 's', '1', '6', 'l' ) )
            {
                p_src++;
            }

            for( i = 0; i < p_aout_buf->i_nb_bytes / 2; i++ )
            {
                *p_dst++ = *p_src; p_src += 2;
            }
        }
    }
    else
    {
        /* endian swapping */
        if( ( p_block = block_New( p_enc, p_aout_buf->i_nb_bytes ) ) )
        {
            uint8_t *p_dst = (uint8_t*)p_block->p_buffer;
            uint8_t *p_src = (uint8_t*)p_aout_buf->p_buffer;

            for( i = 0; i < p_aout_buf->i_nb_bytes / 2; i++ )
            {
                p_dst[0] = p_src[1];
                p_dst[1] = p_src[0];

                p_dst += 2;
                p_src += 2;
            }
        }
    }

    if( p_block )
    {
        p_block->i_dts = p_block->i_pts = p_aout_buf->start_date;
        p_block->i_length = (int64_t)p_aout_buf->i_nb_samples * (int64_t)1000000 /
                            p_enc->format.audio.i_rate;
    }

    return p_block;
}


