/*****************************************************************************
 * mpeg_adec.c: MPEG audio decoder thread
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: mpeg_adec.c,v 1.5.2.1 2001/12/30 06:06:00 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Michel Lespinasse <walken@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
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

#define MODULE_NAME mpeg_adec
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "audio_output.h"               /* aout_fifo_t (for audio_decoder.h) */

#include "modules.h"
#include "modules_export.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "mpeg_adec_generic.h"
#include "mpeg_adec.h"

#define ADEC_FRAME_SIZE (2*1152)

/*****************************************************************************
 * Local Prototypes
 *****************************************************************************/
static int   decoder_Probe ( probedata_t * );
static int   decoder_Run   ( decoder_config_t * );
static void  EndThread     ( adec_thread_t * );
static void  DecodeThread  ( adec_thread_t * );


/*****************************************************************************
 * Capabilities
 *****************************************************************************/
void _M( adec_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = decoder_Probe;
    p_function_list->functions.dec.pf_run = decoder_Run;
}

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for mpeg audio decoder module" )
    ADD_COMMENT( "Nothing to configure" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_DEC;
    p_module->psz_longname = "Mpeg I layer 1/2 audio decoder";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( adec_getfunctions )( &p_module->p_functions->dec );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * decoder_Probe: probe the decoder and return score
 *****************************************************************************/
static int decoder_Probe( probedata_t *p_data )
{
    if( p_data->i_type == MPEG1_AUDIO_ES || p_data->i_type == MPEG2_AUDIO_ES )
    {
        if( !TestCPU( CPU_CAPABILITY_FPU ) )
        {
            /* This can work but we'd really prefer libmad to take over. */
            return( 1 );
        }
        if( TestMethod( ADEC_MPEG_VAR, "builtin" ) )
        {
            return( 999 );
        }
        return( 100 );
    }
    return( 0 );
}

/*****************************************************************************
 * decoder_Run: initialize, go inside main loop, detroy
 *****************************************************************************/
static int decoder_Run ( decoder_config_t * p_config )
{
    adec_thread_t   * p_adec;
    
    intf_DbgMsg("mpeg_adec debug: thread launched, initializing.");
    
    /* Allocate the memory needed to store the thread's structure */
    if ( (p_adec = (adec_thread_t *)malloc (sizeof(adec_thread_t))) == NULL ) 
    {
        intf_ErrMsg ( "adec error: not enough memory for"
                      " adec_CreateThread() to create the new thread" );
        DecoderError( p_config->p_decoder_fifo );
        return 0;
    }
    
    /*
     * Initialize the thread properties
     */
    p_adec->p_config = p_config;
    p_adec->p_fifo = p_config->p_decoder_fifo;

    /* 
     * Initilize the banks
     */
    p_adec->bank_0.actual = p_adec->bank_0.v1;
    p_adec->bank_0.pos = 0;
    p_adec->bank_1.actual = p_adec->bank_1.v1;
    p_adec->bank_1.pos = 0;
    
    /*
     * Initialize bit stream 
     */
    p_adec->p_config->pf_init_bit_stream( &p_adec->bit_stream,
        p_adec->p_config->p_decoder_fifo, NULL, NULL );

    /* Create the audio output fifo */
    p_adec->p_aout_fifo = aout_CreateFifo( AOUT_ADEC_STEREO_FIFO, 2, 0, 0,
                                           ADEC_FRAME_SIZE, NULL );
    if ( p_adec->p_aout_fifo == NULL )
    {
        intf_ErrMsg("mpeg_adec error: cannot create audio output fifo");
        return -1;
    }

    intf_DbgMsg("mpeg_adec debug: thread initialized, decoding begins.");

    p_adec->i_sync = 0;

    /* Audio decoder thread's main loop */
    while( (!p_adec->p_fifo->b_die) && (!p_adec->p_fifo->b_error) )
    {
        DecodeThread( p_adec );
    }
    
    /* If b_error is set, the audio decoder thread enters the error loop */
    if( p_adec->p_fifo->b_error ) 
    {
        DecoderError( p_adec->p_fifo );
    }

    /* End of the audio decoder thread */
    EndThread( p_adec );

    return( 0 );
}

/*
 * Following finctions are local to this module
 */

/*****************************************************************************
 * DecodeThread: decodes a mpeg frame
 *****************************************************************************/
static void DecodeThread( adec_thread_t * p_adec )
{
    s16 * buffer;
    adec_sync_info_t sync_info;

    if( ! adec_SyncFrame (p_adec, &sync_info) )
    {
        p_adec->i_sync = 1;

        p_adec->p_aout_fifo->l_rate = sync_info.sample_rate;

        buffer = ((s16 *)p_adec->p_aout_fifo->buffer)
                    + (p_adec->p_aout_fifo->l_end_frame * ADEC_FRAME_SIZE);

        if( DECODER_FIFO_START( *p_adec->p_fifo)->i_pts )
        {
            p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] =
                DECODER_FIFO_START( *p_adec->p_fifo )->i_pts;
            DECODER_FIFO_START(*p_adec->p_fifo)->i_pts = 0;
        }
        else
        {
            p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] =
                LAST_MDATE;
        }

        if( adec_DecodeFrame (p_adec, buffer) )
        {
            /* Ouch, failed decoding... We'll have to resync */
            p_adec->i_sync = 0;
        }
        else
        {
            vlc_mutex_lock (&p_adec->p_aout_fifo->data_lock);

            p_adec->p_aout_fifo->l_end_frame =
                (p_adec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
            vlc_cond_signal (&p_adec->p_aout_fifo->data_wait);
            vlc_mutex_unlock (&p_adec->p_aout_fifo->data_lock);
        }
    }
}

/*****************************************************************************
 * EndThread : audio decoder thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread ( adec_thread_t *p_adec )
{
    intf_DbgMsg ( "adec debug: destroying audio decoder thread %p", p_adec );

    /* If the audio output fifo was created, we destroy it */
    if ( p_adec->p_aout_fifo != NULL ) 
    {
        aout_DestroyFifo ( p_adec->p_aout_fifo );

        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock (&(p_adec->p_aout_fifo->data_lock));
        vlc_cond_signal (&(p_adec->p_aout_fifo->data_wait));
        vlc_mutex_unlock (&(p_adec->p_aout_fifo->data_lock));
    }
    /* Destroy descriptor */
    free( p_adec );

    intf_DbgMsg ("adec debug: audio decoder thread %p destroyed", p_adec);
}

