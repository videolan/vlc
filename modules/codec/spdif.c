/*****************************************************************************
 * spdif.c: A52 pass-through to external decoder with enabled soundcard
 *****************************************************************************
 * Copyright (C) 2001-2002 VideoLAN
 * $Id: spdif.c,v 1.4 2002/08/12 22:12:51 massiot Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Juha Yrjola <jyrjola@cc.hut.fi>
 *          German Gomez Garcia <german@piraos.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>                                              /* memcpy() */
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/aout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#define A52_FRAME_SIZE 1536 

/*****************************************************************************
 * spdif_thread_t : A52 pass-through thread descriptor
 *****************************************************************************/
typedef struct spdif_thread_s
{
    /*
     * Thread properties
     */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Input properties
     */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */

    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;

    /*
     * Output properties
     */
    aout_instance_t *   p_aout; /* opaque */
    aout_input_t *      p_aout_input; /* opaque */
    audio_sample_format_t output_format;
} spdif_thread_t;

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );
static int  RunDecoder     ( decoder_fifo_t * );

static int  InitThread     ( spdif_thread_t *, decoder_fifo_t * );
static void EndThread      ( spdif_thread_t * );

static int  SyncInfo       ( const byte_t *, int *, int *, int * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("SPDIF pass-through A52 decoder") );
    set_capability( "decoder", 0 );
    set_callbacks( OpenDecoder, NULL );
    add_shortcut( "pass_through" );
    add_shortcut( "pass" );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able 
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this ) 
{   
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('a','5','2',' ') )
    {   
        return VLC_EGENERIC; 
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/****************************************************************************
 * RunDecoder: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    spdif_thread_t * p_dec;
    mtime_t last_date = 0;
    
    /* Allocate the memory needed to store the thread's structure */
    p_dec = malloc( sizeof(spdif_thread_t) );
    if( p_dec == NULL )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return -1;
    }
  
    if ( InitThread( p_dec, p_fifo ) )
    {
        
        msg_Err( p_fifo, "could not initialize thread" );
        DecoderError( p_fifo );
        free( p_dec );
        return -1;
    }

    /* liba52 decoder thread's main loop */
    while( !p_dec->p_fifo->b_die && !p_dec->p_fifo->b_error )
    {
        int i_frame_size, i_flags, i_rate, i_bit_rate;
        mtime_t pts;
        /* Temporary buffer to store the raw frame to be decoded */
        byte_t p_header[7];
        aout_buffer_t * p_buffer;

        /* Look for sync word - should be 0x0b77 */
        RealignBits( &p_dec->bit_stream );
        while( (ShowBits( &p_dec->bit_stream, 16 ) ) != 0x0b77 && 
               (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error))
        {
            RemoveBits( &p_dec->bit_stream, 8 );
        }

        /* Get A/52 frame header */
        GetChunk( &p_dec->bit_stream, p_header, 7 );
        if( p_dec->p_fifo->b_die ) break;

        /* Check if frame is valid and get frame info */
        i_frame_size = SyncInfo( p_header, &i_flags, &i_rate,
                                 &i_bit_rate );

        if( !i_frame_size )
        {
            msg_Warn( p_dec->p_fifo, "a52_syncinfo failed" );
            continue;
        }

        if( (p_dec->p_aout_input != NULL) &&
            ( (p_dec->output_format.i_rate != i_rate) ) )
        {
            /* Parameters changed - this should not happen. */
            aout_InputDelete( p_dec->p_aout, p_dec->p_aout_input );
            p_dec->p_aout_input = NULL;
        }

        /* Creating the audio input if not created yet. */
        if( p_dec->p_aout_input == NULL )
        {
            p_dec->output_format.i_rate = i_rate;
            /* p_dec->output_format.i_channels = i_channels; */
            p_dec->p_aout_input = aout_InputNew( p_dec->p_fifo,
                                                 &p_dec->p_aout,
                                                 &p_dec->output_format );

            if ( p_dec->p_aout_input == NULL )
            {
                p_dec->p_fifo->b_error = 1;
                break;
            }
        }

        /* Set the Presentation Time Stamp */
        CurrentPTS( &p_dec->bit_stream, &pts, NULL );
        if ( pts != 0 )
        {
            last_date = pts;
        }

        if ( !last_date )
        {
            byte_t p_junk[3840];

            /* We've just started the stream, wait for the first PTS. */
            GetChunk( &p_dec->bit_stream, p_junk, i_frame_size - 7 );
            continue;
        }

        p_buffer = aout_BufferNew( p_dec->p_aout, p_dec->p_aout_input,
                                   i_frame_size );
        if ( p_buffer == NULL ) return -1;
        p_buffer->start_date = last_date;
        last_date += (mtime_t)(A52_FRAME_SIZE * 1000000)
                       / p_dec->output_format.i_rate;
        p_buffer->end_date = last_date;

        /* Get the whole frame. */
        memcpy( p_buffer->p_buffer, p_header, 7 );
        GetChunk( &p_dec->bit_stream, p_buffer->p_buffer + 7,
                  i_frame_size - 7 );
        if( p_dec->p_fifo->b_die ) break;

        /* Send the buffer to the mixer. */
        aout_BufferPlay( p_dec->p_aout, p_dec->p_aout_input, p_buffer );
    }

    /* If b_error is set, the spdif thread enters the error loop */
    if( p_dec->p_fifo->b_error )
    {
        DecoderError( p_dec->p_fifo );
    }

    /* End of the spdif decoder thread */
    EndThread( p_dec );
    
    return 0;
}

/****************************************************************************
 * InitThread: initialize thread data and create output fifo
 ****************************************************************************/
static int InitThread( spdif_thread_t * p_dec, decoder_fifo_t * p_fifo )
{
    /* Initialize the thread properties */
    p_dec->p_aout = NULL;
    p_dec->p_aout_input = NULL;
    p_dec->p_fifo = p_fifo;
    p_dec->output_format.i_format = AOUT_FMT_A52;
    p_dec->output_format.i_channels = -1;

    /* Init the Bitstream */
    InitBitstream( &p_dec->bit_stream, p_dec->p_fifo,
                   NULL, NULL );

    return 0;
}

/*****************************************************************************
 * EndThread : spdif thread destruction
 *****************************************************************************/
static void EndThread( spdif_thread_t * p_dec )
{
    if ( p_dec->p_aout_input != NULL )
    {
        aout_InputDelete( p_dec->p_aout, p_dec->p_aout_input );
    }

    free( p_dec );
}

/****************************************************************************
 * Local structures and tables
 ****************************************************************************/
typedef struct sync_frame_s
{
    struct syncinfo
    {
        u8      syncword[2];
        u8      crc1[2];
        u8      code;
    } syncinfo;

    struct bsi
    {
        u8      bsidmod;
        u8      acmod;
    } bsi;
} sync_frame_t;

typedef struct frame_size_s
{
    u16     i_bit_rate;
    u16     i_frame_size[3];
} frame_size_t;                

typedef struct info_s
{
    int i_bit_rate;
    int i_frame_size;
    int i_sample_rate;
    int i_bs_mod;
} info_t;

static const frame_size_t p_frame_size_code[64] =
{
        { 32  ,{64   ,69   ,96   } },
        { 32  ,{64   ,70   ,96   } },
        { 40  ,{80   ,87   ,120  } },
        { 40  ,{80   ,88   ,120  } },
        { 48  ,{96   ,104  ,144  } },
        { 48  ,{96   ,105  ,144  } },
        { 56  ,{112  ,121  ,168  } },
        { 56  ,{112  ,122  ,168  } },
        { 64  ,{128  ,139  ,192  } },
        { 64  ,{128  ,140  ,192  } },
        { 80  ,{160  ,174  ,240  } },
        { 80  ,{160  ,175  ,240  } },
        { 96  ,{192  ,208  ,288  } },
        { 96  ,{192  ,209  ,288  } },
        { 112 ,{224  ,243  ,336  } },
        { 112 ,{224  ,244  ,336  } },
        { 128 ,{256  ,278  ,384  } },
        { 128 ,{256  ,279  ,384  } },
        { 160 ,{320  ,348  ,480  } },
        { 160 ,{320  ,349  ,480  } },
        { 192 ,{384  ,417  ,576  } },
        { 192 ,{384  ,418  ,576  } },
        { 224 ,{448  ,487  ,672  } },
        { 224 ,{448  ,488  ,672  } },
        { 256 ,{512  ,557  ,768  } },
        { 256 ,{512  ,558  ,768  } },
        { 320 ,{640  ,696  ,960  } },
        { 320 ,{640  ,697  ,960  } },
        { 384 ,{768  ,835  ,1152 } },
        { 384 ,{768  ,836  ,1152 } },
        { 448 ,{896  ,975  ,1344 } },
        { 448 ,{896  ,976  ,1344 } },
        { 512 ,{1024 ,1114 ,1536 } },
        { 512 ,{1024 ,1115 ,1536 } },
        { 576 ,{1152 ,1253 ,1728 } },
        { 576 ,{1152 ,1254 ,1728 } },
        { 640 ,{1280 ,1393 ,1920 } },
        { 640 ,{1280 ,1394 ,1920 } }
};

/****************************************************************************
 * SyncInfo: parse A52 sync info
 ****************************************************************************
 * NB : i_flags is unused, this is just to mimick liba52's a52_syncinfo
 * Returns the frame size
 ****************************************************************************/
static int SyncInfo( const byte_t * p_buffer, int * pi_flags, int * pi_rate,
                     int * pi_bitrate )
{
    static const int p_sample_rates[4] = { 48000, 44100, 32000, -1 };
    int             i_frame_rate_code;
    int             i_frame_size_code;
    const sync_frame_t * p_sync_frame;

    p_sync_frame = (const sync_frame_t *)p_buffer;

    /* Compute frame rate */
    i_frame_rate_code = (p_sync_frame->syncinfo.code >> 6) & 0x03;
    *pi_rate = p_sample_rates[i_frame_rate_code];
    if ( *pi_rate == -1 )
    {
        return 0;
    }

    if ( ( ( p_sync_frame->bsi.bsidmod >> 3 ) & 0x1f ) != 0x08 )
    {
        return 0;
    }

    /* Compute frame size */
    i_frame_size_code = p_sync_frame->syncinfo.code & 0x3f;        
    *pi_bitrate = p_frame_size_code[i_frame_size_code].i_bit_rate;

    return ( 2 * p_frame_size_code[i_frame_size_code]
                   .i_frame_size[i_frame_rate_code] );
}
