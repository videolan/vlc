/*****************************************************************************
 * a52old.c: A52 decoder module main file
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: a52old.c,v 1.6 2002/08/30 22:22:24 massiot Exp $
 *
 * Authors: Michel Lespinasse <walken@zoy.org>
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
#include <string.h>                                              /* memset() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>

#include "aout_internal.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                           /* getpid() */
#endif

#include "imdct.h"
#include "downmix.h"
#include "adec.h"

#define A52DEC_FRAME_SIZE 1536

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder       ( vlc_object_t * );
static int  RunDecoder        ( decoder_fifo_t * );
static int  InitThread        ( a52dec_t * );
static void EndThread         ( a52dec_t * );
static void BitstreamCallback ( bit_stream_t *, vlc_bool_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_module  ( "a52-downmix", "downmix", NULL, NULL,
                  N_("A52 downmix module"), NULL );
    add_module  ( "a52-imdct", "imdct", NULL, NULL,
                  N_("A52 IMDCT module"), NULL );
    set_description( _("software A52 decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, NULL );
    add_shortcut( "a52" );
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
    
    if( p_fifo->i_fourcc != VLC_FOURCC('a','5','2',' ')
         && p_fifo->i_fourcc != VLC_FOURCC('a','5','2','b') )
    {   
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    a52dec_t *   p_a52dec;
    void *       p_orig;                          /* pointer before memalign */
    vlc_bool_t   b_sync = 0;

    mtime_t i_pts = 0;
    aout_buffer_t *p_aout_buffer;
    audio_sample_format_t output_format;
    audio_date_t end_date;

    /* Allocate the memory needed to store the thread's structure */
    p_a52dec = (a52dec_t *)vlc_memalign( &p_orig, 16, sizeof(a52dec_t) );
    memset( p_a52dec, 0, sizeof( a52dec_t ) );

    if( p_a52dec == NULL )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }

    /*
     * Initialize the thread properties
     */
    p_a52dec->p_fifo = p_fifo;
    if( InitThread( p_a52dec ) )
    {
        msg_Err( p_fifo, "could not initialize thread" );
        DecoderError( p_fifo );
        free( p_orig );
        return( -1 );
    }

    /* A52 decoder thread's main loop */
    /* FIXME : do we have enough room to store the decoded frames ?? */
    while ((!p_a52dec->p_fifo->b_die) && (!p_a52dec->p_fifo->b_error))
    {
        sync_info_t sync_info;

        if( !b_sync )
        {
             int i_sync_ptr;
#define p_bit_stream (&p_a52dec->bit_stream)

             /* Go to the next PES packet and jump to sync_ptr */
             do {
                BitstreamNextDataPacket( p_bit_stream );
             } while( !p_bit_stream->p_decoder_fifo->b_die
                       && !p_bit_stream->p_decoder_fifo->b_error
                       && p_bit_stream->p_data !=
                          p_bit_stream->p_decoder_fifo->p_first->p_first );
             i_sync_ptr = *(p_bit_stream->p_byte - 2) << 8
                            | *(p_bit_stream->p_byte - 1);
             p_bit_stream->p_byte += i_sync_ptr;

             /* Empty the bit FIFO and realign the bit stream */
             p_bit_stream->fifo.buffer = 0;
             p_bit_stream->fifo.i_available = 0;
             AlignWord( p_bit_stream );
             b_sync = 1;
#undef p_bit_stream
        }

        if (sync_frame (p_a52dec, &sync_info))
        {
            b_sync = 0;
            continue;
        }

        if( ( p_a52dec->p_aout_input == NULL )||
            ( output_format.i_rate != sync_info.sample_rate ) )
        {
            if( p_a52dec->p_aout_input )
            {
                /* Delete old output */
                msg_Warn( p_a52dec->p_fifo, "opening a new aout" );
                aout_InputDelete( p_a52dec->p_aout, p_a52dec->p_aout_input );
            }

            /* Set output configuration */
            output_format.i_format   = AOUT_FMT_S16_NE;
            output_format.i_channels = 2; /* FIXME ! */
            output_format.i_rate     = sync_info.sample_rate;
            aout_DateInit( &end_date, output_format.i_rate );
            p_a52dec->p_aout_input = aout_InputNew( p_a52dec->p_fifo,
                                                    &p_a52dec->p_aout,
                                                    &output_format );
        }

        if( p_a52dec->p_aout_input == NULL )
        {
            msg_Err( p_a52dec->p_fifo, "failed to create aout fifo" );
            p_a52dec->p_fifo->b_error = 1;
            continue;
        }

        NextPTS( &p_a52dec->bit_stream, &i_pts, NULL );
        if( i_pts != 0 && i_pts != aout_DateGet( &end_date ) )
        {
            aout_DateSet( &end_date, i_pts );
        }

        if( !aout_DateGet( &end_date ) )
        {
            continue;
        }

        p_aout_buffer = aout_BufferNew( p_a52dec->p_aout,
                                        p_a52dec->p_aout_input,
                                        A52DEC_FRAME_SIZE );
        if( !p_aout_buffer )
        {
            msg_Err( p_a52dec->p_fifo, "cannot get aout buffer" );
            p_a52dec->p_fifo->b_error = 1;
            continue;
        }

        p_aout_buffer->start_date = aout_DateGet( &end_date );
        p_aout_buffer->end_date = aout_DateIncrement( &end_date,
                                                      A52DEC_FRAME_SIZE );

        if (decode_frame (p_a52dec, (s16*)p_aout_buffer->p_buffer))
        {
            b_sync = 0;
            aout_BufferDelete( p_a52dec->p_aout, p_a52dec->p_aout_input,
                                                 p_aout_buffer );
            continue;
        }
        else
        {
            aout_BufferPlay( p_a52dec->p_aout, p_a52dec->p_aout_input,
                                               p_aout_buffer );
        }

        RealignBits(&p_a52dec->bit_stream);
    }

    /* If b_error is set, the A52 decoder thread enters the error loop */
    if (p_a52dec->p_fifo->b_error)
    {
        DecoderError( p_a52dec->p_fifo );
    }

    /* End of the A52 decoder thread */
    EndThread (p_a52dec);

    free( p_orig );

    return( 0 );
}

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/
static int InitThread( a52dec_t * p_a52dec )
{
    /*
     * Choose the best downmix module
     */
    p_a52dec->p_downmix = vlc_object_create( p_a52dec->p_fifo,
                                             sizeof( downmix_t ) );
    p_a52dec->p_downmix->psz_object_name = "downmix";

    p_a52dec->p_downmix->p_module =
                module_Need( p_a52dec->p_downmix, "downmix", "$a52-downmix" );

    if( p_a52dec->p_downmix->p_module == NULL )
    {
        msg_Err( p_a52dec->p_fifo, "no suitable downmix module" );
        vlc_object_destroy( p_a52dec->p_downmix );
        return( -1 );
    }

    /*
     * Choose the best IMDCT module
     */
    p_a52dec->p_imdct = vlc_object_create( p_a52dec->p_fifo,
                                           sizeof( imdct_t ) );
    
#define IMDCT p_a52dec->p_imdct
    p_a52dec->p_imdct->p_module =
                   module_Need( p_a52dec->p_imdct, "imdct", "$a52-imdct" );

    if( p_a52dec->p_imdct->p_module == NULL )
    {
        msg_Err( p_a52dec->p_fifo, "no suitable IMDCT module" );
        vlc_object_destroy( p_a52dec->p_imdct );
        module_Unneed( p_a52dec->p_downmix, p_a52dec->p_downmix->p_module );
        vlc_object_destroy( p_a52dec->p_downmix );
        return( -1 );
    }

    /* Initialize the A52 decoder structures */
    p_a52dec->samples = vlc_memalign( &p_a52dec->samples_orig,
                                      16, 6 * 256 * sizeof(float) );

    IMDCT->buf    = vlc_memalign( &IMDCT->buf_orig,
                                  16, N/4 * sizeof(complex_t) );
    IMDCT->delay  = vlc_memalign( &IMDCT->delay_orig,
                                  16, 6 * 256 * sizeof(float) );
    IMDCT->delay1 = vlc_memalign( &IMDCT->delay1_orig,
                                  16, 6 * 256 * sizeof(float) );
    IMDCT->xcos1  = vlc_memalign( &IMDCT->xcos1_orig,
                                  16, N/4 * sizeof(float) );
    IMDCT->xsin1  = vlc_memalign( &IMDCT->xsin1_orig,
                                  16, N/4 * sizeof(float) );
    IMDCT->xcos2  = vlc_memalign( &IMDCT->xcos2_orig,
                                  16, N/8 * sizeof(float) );
    IMDCT->xsin2  = vlc_memalign( &IMDCT->xsin2_orig,
                                  16, N/8 * sizeof(float) );
    IMDCT->xcos_sin_sse = vlc_memalign( &IMDCT->xcos_sin_sse_orig,
                                        16, 128 * 4 * sizeof(float) );
    IMDCT->w_1    = vlc_memalign( &IMDCT->w_1_orig,
                                  16, 1  * sizeof(complex_t) );
    IMDCT->w_2    = vlc_memalign( &IMDCT->w_2_orig,
                                  16, 2  * sizeof(complex_t) );
    IMDCT->w_4    = vlc_memalign( &IMDCT->w_4_orig,
                                  16, 4  * sizeof(complex_t) );
    IMDCT->w_8    = vlc_memalign( &IMDCT->w_8_orig,
                                  16, 8  * sizeof(complex_t) );
    IMDCT->w_16   = vlc_memalign( &IMDCT->w_16_orig,
                                  16, 16 * sizeof(complex_t) );
    IMDCT->w_32   = vlc_memalign( &IMDCT->w_32_orig,
                                  16, 32 * sizeof(complex_t) );
    IMDCT->w_64   = vlc_memalign( &IMDCT->w_64_orig,
                                  16, 64 * sizeof(complex_t) );
#undef IMDCT

    E_( a52_init )( p_a52dec );

    /*
     * Initialize the output properties
     */
    p_a52dec->p_aout = NULL;
    p_a52dec->p_aout_input = NULL;

    /*
     * Bit stream
     */
    InitBitstream( &p_a52dec->bit_stream, p_a52dec->p_fifo,
                   BitstreamCallback, (void *) p_a52dec );
    
    return( 0 );
}

/*****************************************************************************
 * EndThread : A52 decoder thread destruction
 *****************************************************************************/
static void EndThread (a52dec_t * p_a52dec)
{
    /* If the audio output fifo was created, we destroy it */
    if( p_a52dec->p_aout_input )
    {
        aout_InputDelete( p_a52dec->p_aout, p_a52dec->p_aout_input );
    }

    /* Free allocated structures */
#define IMDCT p_a52dec->p_imdct
    free( IMDCT->w_1_orig );
    free( IMDCT->w_64_orig );
    free( IMDCT->w_32_orig );
    free( IMDCT->w_16_orig );
    free( IMDCT->w_8_orig );
    free( IMDCT->w_4_orig );
    free( IMDCT->w_2_orig );
    free( IMDCT->xcos_sin_sse_orig );
    free( IMDCT->xsin2_orig );
    free( IMDCT->xcos2_orig );
    free( IMDCT->xsin1_orig );
    free( IMDCT->xcos1_orig );
    free( IMDCT->delay1_orig );
    free( IMDCT->delay_orig );
    free( IMDCT->buf_orig );
#undef IMDCT

    free( p_a52dec->samples_orig );

    /* Unlock the modules */
    module_Unneed( p_a52dec->p_downmix, p_a52dec->p_downmix->p_module );
    vlc_object_destroy( p_a52dec->p_downmix );

    module_Unneed( p_a52dec->p_imdct, p_a52dec->p_imdct->p_module );
    vlc_object_destroy( p_a52dec->p_imdct );

    /* Free what's left of the decoder */
    free( p_a52dec->imdct_orig );
}

/*****************************************************************************
 * BitstreamCallback: Import parameters from the new data/PES packet
 *****************************************************************************
 * This function is called by input's NextDataPacket.
 *****************************************************************************/
static void BitstreamCallback ( bit_stream_t * p_bit_stream,
                                vlc_bool_t b_new_pes )
{
    if( b_new_pes )
    {
        /* Drop special A52 header */
/*        p_bit_stream->p_byte += 3; */
    }
}

