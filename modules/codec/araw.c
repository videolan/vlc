/*****************************************************************************
 * araw.c: Pseudo audio decoder; for raw pcm data
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: araw.c,v 1.15 2003/05/17 20:30:31 gbazin Exp $
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

typedef struct adec_thread_s
{
    WAVEFORMATEX    *p_wf;

    /* Input properties */
    decoder_fifo_t *p_fifo;
    int16_t        *p_logtos16;  // used with m/alaw to s16

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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("Pseudo Raw/Log Audio decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();


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
        case VLC_FOURCC('a','r','a','w'): /* from wav/avi/asf file */
        case VLC_FOURCC('t','w','o','s'): /* _signed_ big endian samples (mov)*/
        case VLC_FOURCC('s','o','w','t'): /* _signed_ little endian samples (mov)*/

        case VLC_FOURCC('a','l','a','w'):
        case VLC_FOURCC('u','l','a','w'):
            p_fifo->pf_run = RunDecoder;
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
    if( ( p_adec->p_wf = (WAVEFORMATEX*)p_adec->p_fifo->p_waveformatex )
        == NULL )
    {
        msg_Err( p_adec->p_fifo, "unknown raw format" );
        return( -1 );
    }

    /* fixing some values */
    if( ( p_adec->p_wf->wFormatTag  == WAVE_FORMAT_PCM ||
          p_adec->p_wf->wFormatTag  == WAVE_FORMAT_IEEE_FLOAT )&&
        !p_adec->p_wf->nBlockAlign )
    {
        p_adec->p_wf->nBlockAlign =
            p_adec->p_wf->nChannels *
                ( ( p_adec->p_wf->wBitsPerSample + 7 ) / 8 );
    }

    msg_Dbg( p_adec->p_fifo,
             "raw format: samplerate:%dHz channels:%d bits/sample:%d "
             "blockalign:%d",
             p_adec->p_wf->nSamplesPerSec,
             p_adec->p_wf->nChannels,
             p_adec->p_wf->wBitsPerSample,
             p_adec->p_wf->nBlockAlign );

    /* Initialize the thread properties */
    p_adec->p_logtos16 = NULL;
    if( p_adec->p_wf->wFormatTag  == WAVE_FORMAT_IEEE_FLOAT )
    {
        switch( ( p_adec->p_wf->wBitsPerSample + 7 ) / 8 )
        {
            case( 4 ):
                p_adec->output_format.i_format = VLC_FOURCC('f','l','3','2');
                break;
            case( 8 ):
                p_adec->output_format.i_format = VLC_FOURCC('f','l','6','4');
                break;
            default:
                msg_Err( p_adec->p_fifo, "bad parameters(bits/sample)" );
                return( -1 );
        }
    }
    else
    {
        if( p_adec->p_fifo->i_fourcc == VLC_FOURCC( 't', 'w', 'o', 's' ) )
        {
            switch( ( p_adec->p_wf->wBitsPerSample + 7 ) / 8 )
            {
                case( 1 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','8',' ',' ');
                    break;
                case( 2 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','1','6','b');
                    break;
                case( 3 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','2','4','b');
                    break;
                case( 4 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','3','2','b');
                    break;
                default:
                    msg_Err( p_adec->p_fifo, "bad parameters(bits/sample)" );
                    return( -1 );
            }
        }
        else if( p_adec->p_fifo->i_fourcc == VLC_FOURCC( 's', 'o', 'w', 't' ) )
        {
            switch( ( p_adec->p_wf->wBitsPerSample + 7 ) / 8 )
            {
                case( 1 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','8',' ',' ');
                    break;
                case( 2 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','1','6','l');
                    break;
                case( 3 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','2','4','l');
                    break;
                case( 4 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','3','2','l');
                    break;
                default:
                    msg_Err( p_adec->p_fifo, "bad parameters(bits/sample)" );
                    return( -1 );
            }
        }
        else if( p_adec->p_fifo->i_fourcc == VLC_FOURCC( 'a', 'r', 'a', 'w' ) )
        {
            switch( ( p_adec->p_wf->wBitsPerSample + 7 ) / 8 )
            {
                case( 1 ):
                    p_adec->output_format.i_format = VLC_FOURCC('u','8',' ',' ');
                    break;
                case( 2 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','1','6','l');
                    break;
                case( 3 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','2','4','l');
                    break;
                case( 4 ):
                    p_adec->output_format.i_format = VLC_FOURCC('s','3','2','l');
                    break;
                default:
                    msg_Err( p_adec->p_fifo, "bad parameters(bits/sample)" );
                    return( -1 );
            }
        }
        else if( p_adec->p_fifo->i_fourcc == VLC_FOURCC( 'a', 'l', 'a', 'w' ) )
        {
            p_adec->output_format.i_format = AOUT_FMT_S16_NE;
            p_adec->p_logtos16  = alawtos16;
        }
        else if( p_adec->p_fifo->i_fourcc == VLC_FOURCC( 'u', 'l', 'a', 'w' ) )
        {
            p_adec->output_format.i_format = AOUT_FMT_S16_NE;
            p_adec->p_logtos16  = ulawtos16;
        }
    }
    p_adec->output_format.i_rate = p_adec->p_wf->nSamplesPerSec;

    if( p_adec->p_wf->nChannels <= 0 || p_adec->p_wf->nChannels > 5 )
    {
        msg_Err( p_adec->p_fifo, "bad channels count(1-5)" );
        return( -1 );
    }

    p_adec->output_format.i_physical_channels =
            p_adec->output_format.i_original_channels =
            pi_channels_maps[p_adec->p_wf->nChannels];
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

    return( 0 );
}

static void GetPESData( u8 *p_buf, int i_max, pes_packet_t *p_pes )
{
    int i_copy;
    int i_count;

    data_packet_t   *p_data;

    i_count = 0;
    p_data = p_pes->p_first;
    while( p_data != NULL && i_count < i_max )
    {

        i_copy = __MIN( p_data->p_payload_end - p_data->p_payload_start,
                        i_max - i_count );

        if( i_copy > 0 )
        {
            memcpy( p_buf,
                    p_data->p_payload_start,
                    i_copy );
        }

        p_data = p_data->p_next;
        i_count += i_copy;
        p_buf   += i_copy;
    }

    if( i_count < i_max )
    {
        memset( p_buf, 0, i_max - i_count );
    }
}

/*****************************************************************************
 * DecodeThread: decodes a frame
 *****************************************************************************/
static void DecodeThread( adec_thread_t *p_adec )
{
    aout_buffer_t   *p_aout_buffer;
    int             i_samples; // per channels
    int             i_size;
    uint8_t         *p_data, *p;
    pes_packet_t    *p_pes;

    /* **** get samples count **** */
    input_ExtractPES( p_adec->p_fifo, &p_pes );
    if( !p_pes )
    {
        p_adec->p_fifo->b_error = 1;
        return;
    }
    i_size = p_pes->i_pes_size;

    if( p_adec->p_wf->nBlockAlign > 0 )
    {
        i_size -= i_size % p_adec->p_wf->nBlockAlign;
    }
    if( i_size <= 0 || i_size < p_adec->p_wf->nBlockAlign )
    {
        input_DeletePES( p_adec->p_fifo->p_packets_mgt, p_pes );
        return;
    }

    i_samples = i_size / ( ( p_adec->p_wf->wBitsPerSample + 7 ) / 8 ) /
                p_adec->p_wf->nChannels;

    p_adec->pts = p_pes->i_pts;

    /* **** Now we can output these samples **** */

    if( p_adec->pts != 0 && p_adec->pts != aout_DateGet( &p_adec->date ) )
    {
        aout_DateSet( &p_adec->date, p_adec->pts );
    }
    else if( !aout_DateGet( &p_adec->date ) )
    {
        return;
    }

    /* gather data */
    p = p_data = malloc( i_size );
    GetPESData( p_data, i_size, p_pes );

    while( i_samples > 0 )
    {
        int i_copy;

        i_copy = __MIN( i_samples, 1024 );
        p_aout_buffer = aout_DecNewBuffer( p_adec->p_aout,
                                           p_adec->p_aout_input,
                                           i_copy );
        if( !p_aout_buffer )
        {
            msg_Err( p_adec->p_fifo, "cannot get aout buffer" );
            p_adec->p_fifo->b_error = 1;

            free( p_data );
            return;
        }

        p_aout_buffer->start_date = aout_DateGet( &p_adec->date );
        p_aout_buffer->end_date = aout_DateIncrement( &p_adec->date,
                                                      i_copy );

        if( p_adec->p_logtos16 )
        {
            int16_t *s = (int16_t*)p_aout_buffer->p_buffer;

            unsigned int     i;

            for( i = 0; i < p_aout_buffer->i_nb_bytes; i++ )
            {
                *s++ = p_adec->p_logtos16[*p++];
            }
        }
        else
        {
            memcpy( p_aout_buffer->p_buffer, p,
                    p_aout_buffer->i_nb_bytes );

            p += p_aout_buffer->i_nb_bytes;
        }

        aout_DecPlay( p_adec->p_aout, p_adec->p_aout_input, p_aout_buffer );

        i_samples -= i_copy;
    }

    free( p_data );
    input_DeletePES( p_adec->p_fifo->p_packets_mgt, p_pes );
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

    msg_Dbg( p_adec->p_fifo, "raw audio decoder closed" );

    free( p_adec );
}
