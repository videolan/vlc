/***************************************************************************
              decoder.c  -  description
                -------------------
    Plugin Module definition for using libmad audio decoder in vlc. The
    libmad codec uses integer arithmic only. This makes it suitable for using
    it on architectures without a hardware FPU unit, such as the StrongArm
    CPU.

    begin                : Mon Nov 5 2001
    copyright            : (C) 2001 by Jean-Paul Saman
    email                : jpsaman@wxs.nl
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>

/*****************************************************************************
 * Libmad include files                                                      *
 *****************************************************************************/
#include <mad.h>
#include "decoder.h"
#include "libmad.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );
static int  RunDecoder     ( decoder_fifo_t * );
static int  InitThread     ( mad_adec_thread_t * );
static void EndThread      ( mad_adec_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( N_("Libmad"), NULL );
    set_description( _("libmad MPEG 1/2/3 audio decoder") );
    set_capability( "decoder", 100 );
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

    if( p_fifo->i_fourcc != VLC_FOURCC('m','p','g','a') )
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
    mad_adec_thread_t *   p_dec;
    int i_ret;

    /* Allocate the memory needed to store the thread's structure */
    p_dec = (mad_adec_thread_t *) malloc(sizeof(mad_adec_thread_t));

    if (p_dec == NULL)
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return VLC_ENOMEM;
    }

    /*
     * Initialize the thread properties
     */
    p_dec->p_fifo = p_fifo;
    if( InitThread( p_dec ) )
    {
        msg_Err( p_fifo, "could not initialize thread" );
        DecoderError( p_fifo );
        free( p_dec );
        return VLC_ETHREAD;
    }

    /* mad decoder thread's main loop */
    while( !p_dec->p_fifo->b_die && !p_dec->p_fifo->b_error )
    {
        msg_Dbg( p_dec->p_fifo, "starting libmad decoder" );
        i_ret = mad_decoder_run( &p_dec->libmad_decoder,
                                 MAD_DECODER_MODE_SYNC );
        if( i_ret == -1 )
        {
            msg_Err( p_dec->p_fifo, "libmad decoder returned abnormally" );
            DecoderError( p_dec->p_fifo );
            EndThread(p_dec);
            return VLC_EGENERIC;
        }
    }

    /* If b_error is set, the mad decoder thread enters the error loop */
    if (p_dec->p_fifo->b_error)
    {
        DecoderError( p_dec->p_fifo );
    }

    /* End of the mad decoder thread */
    EndThread (p_dec);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/
static int InitThread( mad_adec_thread_t * p_dec )
{
    decoder_fifo_t * p_fifo = p_dec->p_fifo;

    /* Initialize the thread properties */
    p_dec->p_aout = NULL;
    p_dec->p_aout_input = NULL;
    p_dec->output_format.i_format = VLC_FOURCC('f','i','3','2');

    /*
     * Properties of audio for libmad
     */

    /* Initialize the libmad decoder structures */
    p_dec->i_current_pts = p_dec->i_next_pts = 0;

    mad_decoder_init( &p_dec->libmad_decoder,
                      p_dec, /* vlc's thread structure and p_fifo playbuffer */
                      libmad_input,    /* input_func */
                      NULL,           /* header_func */
                      NULL,                /* filter */
                      libmad_output,  /* output_func */
                      NULL,                 /* error */
                      NULL );             /* message */

    mad_decoder_options( &p_dec->libmad_decoder, MAD_OPTION_IGNORECRC );

    /*
     * Initialize the input properties
     */

    /* Get the first data packet. */
    vlc_mutex_lock( &p_fifo->data_lock );
    while ( p_fifo->p_first == NULL )
    {
        if ( p_fifo->b_die )
        {
            vlc_mutex_unlock( &p_fifo->data_lock );
            return VLC_EGENERIC;
        }
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }
    vlc_mutex_unlock( &p_fifo->data_lock );
    p_dec->p_data = p_fifo->p_first->p_first;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EndThread : libmad decoder thread destruction
 *****************************************************************************/
static void EndThread (mad_adec_thread_t * p_dec)
{
    /* If the audio output fifo was created, we destroy it */
    if (p_dec->p_aout_input != NULL)
    {
        aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
    }

    /* mad_decoder_finish releases the memory allocated inside the struct */
    mad_decoder_finish( &p_dec->libmad_decoder );

    free( p_dec );
}

