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
static int  InitThread     ( mad_adec_thread_t * p_mad_adec );
static void EndThread      ( mad_adec_thread_t * p_mad_adec );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DOWNSCALE_TEXT N_("Mad audio downscale routine (fast,mp321)")
#define DOWNSCALE_LONGTEXT N_( \
    "Specify the mad audio downscale routine you want to use. By default " \
    "the mad plugin will use the fastest routine.")

vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_string( "downscale", "fast", NULL, DOWNSCALE_TEXT, DOWNSCALE_LONGTEXT );
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
    mad_adec_thread_t *   p_mad_adec;

    /* Allocate the memory needed to store the thread's structure */
    p_mad_adec = (mad_adec_thread_t *) malloc(sizeof(mad_adec_thread_t));

    if (p_mad_adec == NULL)
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }

    /*
     * Initialize the thread properties
     */
    p_mad_adec->p_fifo = p_fifo;
    if( InitThread( p_mad_adec ) )
    {
        msg_Err( p_fifo, "could not initialize thread" );
        DecoderError( p_fifo );
        free( p_mad_adec );
        return( -1 );
    }

    /* mad decoder thread's main loop */
    while ((!p_mad_adec->p_fifo->b_die) && (!p_mad_adec->p_fifo->b_error))
    {
        msg_Dbg( p_mad_adec->p_fifo, "starting libmad decoder" );
        if (mad_decoder_run(p_mad_adec->libmad_decoder, MAD_DECODER_MODE_SYNC)==-1)
        {
            msg_Err( p_mad_adec->p_fifo, "libmad decoder returned abnormally" );
            DecoderError( p_mad_adec->p_fifo );
            EndThread(p_mad_adec);
            return( -1 );
        }
    }

    /* If b_error is set, the mad decoder thread enters the error loop */
    if (p_mad_adec->p_fifo->b_error)
    {
        DecoderError( p_mad_adec->p_fifo );
    }

    /* End of the mad decoder thread */
    EndThread (p_mad_adec);

    return( 0 );
}

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/
static int InitThread( mad_adec_thread_t * p_mad_adec )
{
    decoder_fifo_t * p_fifo = p_mad_adec->p_fifo;
    char *psz_downscale = NULL;

    /*
     * Properties of audio for libmad
     */
		
		/* Look what scaling method was requested by the user */
    psz_downscale = config_GetPsz( p_fifo, "downscale" );

    if ( strncmp(psz_downscale,"fast",4)==0 )
    {
        p_mad_adec->audio_scaling = FAST_SCALING;
        msg_Dbg( p_fifo, "downscale fast selected" );
    }
    else if ( strncmp(psz_downscale,"mpg321",7)==0 )
    {
        p_mad_adec->audio_scaling = MPG321_SCALING;
        msg_Dbg( p_fifo, "downscale mpg321 selected" );
    }
    else
    {
        p_mad_adec->audio_scaling = FAST_SCALING;
        msg_Dbg( p_fifo, "downscale default fast selected" );
    }

		if (psz_downscale) free(psz_downscale);

    /* Initialize the libmad decoder structures */
    p_mad_adec->libmad_decoder = (struct mad_decoder*) malloc(sizeof(struct mad_decoder));
    if (p_mad_adec->libmad_decoder == NULL)
    {
        msg_Err( p_mad_adec->p_fifo, "out of memory" );
        return -1;
    }
    p_mad_adec->i_current_pts = p_mad_adec->i_next_pts = 0;

    mad_decoder_init( p_mad_adec->libmad_decoder,
                      p_mad_adec,         /* vlc's thread structure and p_fifo playbuffer */
                      libmad_input,          /* input_func */
                      0,                /* header_func */
                      0,                /* filter */
                      libmad_output,         /* output_func */
                      0,                  /* error */
                      0);                    /* message */

    mad_decoder_options(p_mad_adec->libmad_decoder, MAD_OPTION_IGNORECRC);
//    mad_timer_reset(&p_mad_adec->libmad_timer);

    /*
     * Initialize the output properties
     */
    p_mad_adec->p_aout_fifo = NULL;

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
            return( -1 );
        }
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }
    vlc_mutex_unlock( &p_fifo->data_lock );
    p_mad_adec->p_data = p_fifo->p_first->p_first;

    return( 0 );
}

/*****************************************************************************
 * EndThread : libmad decoder thread destruction
 *****************************************************************************/
static void EndThread (mad_adec_thread_t * p_mad_adec)
{
    /* If the audio output fifo was created, we destroy it */
    if (p_mad_adec->p_aout_fifo != NULL)
    {
        aout_DestroyFifo (p_mad_adec->p_aout_fifo);

        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock (&(p_mad_adec->p_aout_fifo->data_lock));
        vlc_cond_signal (&(p_mad_adec->p_aout_fifo->data_wait));
        vlc_mutex_unlock (&(p_mad_adec->p_aout_fifo->data_lock));
    }

    /* mad_decoder_finish releases the memory allocated inside the struct */
    mad_decoder_finish( p_mad_adec->libmad_decoder );

    /* Unlock the modules, p_mad_adec->p_fifo is released by the decoder subsystem  */
    free( p_mad_adec->libmad_decoder );
    free( p_mad_adec );
}

