/*****************************************************************************
 * ac3_adec.c: ac3 decoder module main file
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ac3_adec.c,v 1.7 2001/12/10 04:53:10 sam Exp $
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

#define MODULE_NAME ac3_adec
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* memset() */

#include "common.h"
#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */
#include "threads.h"
#include "mtime.h"

#include "audio_output.h"

#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"                                /* MPEG?_AUDIO_ES */

#include "modules.h"
#include "modules_export.h"

#include "ac3_imdct.h"
#include "ac3_downmix.h"
#include "ac3_decoder.h"
#include "ac3_adec.h"

#define AC3DEC_FRAME_SIZE (2*1536) 

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      ac3_adec_Probe      ( probedata_t * );
static int      ac3_adec_Run         ( decoder_config_t * );
static int      ac3_adec_Init        (ac3dec_thread_t * p_adec);
static void     ac3_adec_ErrorThread (ac3dec_thread_t * p_adec);
static void     ac3_adec_EndThread   (ac3dec_thread_t * p_adec);
static void     BitstreamCallback    ( bit_stream_t *p_bit_stream,
                                              boolean_t b_new_pes );

/*****************************************************************************
 * Capabilities
 *****************************************************************************/
void _M( adec_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = ac3_adec_Probe;
    p_function_list->functions.dec.pf_run = ac3_adec_Run;
}

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for ac3 decoder module" )
    ADD_COMMENT( "Nothing to configure" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_DEC;
    p_module->psz_longname = "Ac3 sofware decoder";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( adec_getfunctions )( &p_module->p_functions->dec );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP


/*****************************************************************************
 * ac3_adec_Probe: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able 
 * to chose.
 *****************************************************************************/
static int ac3_adec_Probe( probedata_t *p_data )
{
    return ( p_data->i_type == AC3_AUDIO_ES ) ? 50 : 0;
}

/*****************************************************************************
 * ac3_adec_Run: this function is called just after the thread is created
 *****************************************************************************/
static int ac3_adec_Run ( decoder_config_t * p_config )
{
    ac3dec_thread_t *   p_ac3thread;
    int sync;

    intf_DbgMsg( "ac3_adec debug: ac3_adec thread launched, initializing" );

    /* Allocate the memory needed to store the thread's structure */
    p_ac3thread = (ac3dec_thread_t *)memalign(16, sizeof(ac3dec_thread_t));

    if( p_ac3thread == NULL )
    {
        intf_ErrMsg ( "ac3_adec error: not enough memory "
                      "for ac3_adec_Run() to allocate p_ac3thread" );
        return( -1 );
    }
   
    /*
     * Initialize the thread properties
     */
    p_ac3thread->p_config = p_config;
    if( ac3_adec_Init( p_ac3thread ) )
    {
        intf_ErrMsg( "ac3_adec error: could not initialize thread" );
        free( p_ac3thread );
        return( -1 );
    }

    sync = 0;
    p_ac3thread->sync_ptr = 0;

    /* ac3 decoder thread's main loop */
    /* FIXME : do we have enough room to store the decoded frames ?? */
    while ((!p_ac3thread->p_fifo->b_die) && (!p_ac3thread->p_fifo->b_error))
    {
        s16 * buffer;
        ac3_sync_info_t sync_info;
        int ptr;

        if (!sync) {
            do {
                GetBits(&p_ac3thread->ac3_decoder->bit_stream,8);
            } while ((!p_ac3thread->sync_ptr) && (!p_ac3thread->p_fifo->b_die)
                    && (!p_ac3thread->p_fifo->b_error));
            
            ptr = p_ac3thread->sync_ptr;

            while(ptr-- && (!p_ac3thread->p_fifo->b_die)
                && (!p_ac3thread->p_fifo->b_error))
            {
                p_ac3thread->ac3_decoder->bit_stream.p_byte++;
            }
                        
            /* we are in sync now */
            sync = 1;
        }

        if (DECODER_FIFO_START(*p_ac3thread->p_fifo)->i_pts)
        {
            p_ac3thread->p_aout_fifo->date[
                p_ac3thread->p_aout_fifo->l_end_frame] =
                DECODER_FIFO_START(*p_ac3thread->p_fifo)->i_pts;
            DECODER_FIFO_START(*p_ac3thread->p_fifo)->i_pts = 0;
        } else {
            p_ac3thread->p_aout_fifo->date[
                p_ac3thread->p_aout_fifo->l_end_frame] =
                LAST_MDATE;
        }
    
        if (ac3_sync_frame (p_ac3thread->ac3_decoder, &sync_info))
        {
            sync = 0;
            goto bad_frame;
        }

        p_ac3thread->p_aout_fifo->l_rate = sync_info.sample_rate;

        buffer = ((s16 *)p_ac3thread->p_aout_fifo->buffer) + 
            (p_ac3thread->p_aout_fifo->l_end_frame * AC3DEC_FRAME_SIZE);

        if (ac3_decode_frame (p_ac3thread->ac3_decoder, buffer))
        {
            sync = 0;
            goto bad_frame;
        }
        
        vlc_mutex_lock (&p_ac3thread->p_aout_fifo->data_lock);
        p_ac3thread->p_aout_fifo->l_end_frame = 
            (p_ac3thread->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
        vlc_cond_signal (&p_ac3thread->p_aout_fifo->data_wait);
        vlc_mutex_unlock (&p_ac3thread->p_aout_fifo->data_lock);

        bad_frame:
            RealignBits(&p_ac3thread->ac3_decoder->bit_stream);
    }

    /* If b_error is set, the ac3 decoder thread enters the error loop */
    if (p_ac3thread->p_fifo->b_error)
    {
        ac3_adec_ErrorThread (p_ac3thread);
    }

    /* End of the ac3 decoder thread */
    ac3_adec_EndThread (p_ac3thread);

    free( p_ac3thread );

    return( 0 );
}


/*****************************************************************************
 * ac3_adec_Init: initialize data before entering main loop
 *****************************************************************************/
static int ac3_adec_Init( ac3dec_thread_t * p_ac3thread )
{
    /*
     * Thread properties 
     */
    p_ac3thread->p_fifo = p_ac3thread->p_config->p_decoder_fifo;
    p_ac3thread->ac3_decoder = memalign( 16, sizeof(ac3dec_t) );

    /*
     * Choose the best downmix module
     */
#define DOWNMIX p_ac3thread->ac3_decoder->downmix
    DOWNMIX.p_module = module_Need( MODULE_CAPABILITY_DOWNMIX, NULL );

    if( DOWNMIX.p_module == NULL )
    {
        intf_ErrMsg( "ac3dec error: no suitable downmix module" );
        free( p_ac3thread->ac3_decoder );
        return( -1 );
    }

#define F DOWNMIX.p_module->p_functions->downmix.functions.downmix
    DOWNMIX.pf_downmix_3f_2r_to_2ch     = F.pf_downmix_3f_2r_to_2ch;
    DOWNMIX.pf_downmix_2f_2r_to_2ch     = F.pf_downmix_2f_2r_to_2ch;
    DOWNMIX.pf_downmix_3f_1r_to_2ch     = F.pf_downmix_3f_1r_to_2ch;
    DOWNMIX.pf_downmix_2f_1r_to_2ch     = F.pf_downmix_2f_1r_to_2ch;
    DOWNMIX.pf_downmix_3f_0r_to_2ch     = F.pf_downmix_3f_0r_to_2ch;
    DOWNMIX.pf_stream_sample_2ch_to_s16 = F.pf_stream_sample_2ch_to_s16;
    DOWNMIX.pf_stream_sample_1ch_to_s16 = F.pf_stream_sample_1ch_to_s16;
#undef F
#undef DOWNMIX

    /*
     * Choose the best IMDCT module
     */
    p_ac3thread->ac3_decoder->imdct = memalign(16, sizeof(imdct_t));
    
#define IMDCT p_ac3thread->ac3_decoder->imdct
    IMDCT->p_module = module_Need( MODULE_CAPABILITY_IMDCT, NULL );

    if( IMDCT->p_module == NULL )
    {
        intf_ErrMsg( "ac3dec error: no suitable IMDCT module" );
        module_Unneed( p_ac3thread->ac3_decoder->downmix.p_module );
        free( p_ac3thread->ac3_decoder->imdct );
        free( p_ac3thread->ac3_decoder );
        return( -1 );
    }

#define F IMDCT->p_module->p_functions->imdct.functions.imdct
    IMDCT->pf_imdct_init    = F.pf_imdct_init;
    IMDCT->pf_imdct_256     = F.pf_imdct_256;
    IMDCT->pf_imdct_256_nol = F.pf_imdct_256_nol;
    IMDCT->pf_imdct_512     = F.pf_imdct_512;
    IMDCT->pf_imdct_512_nol = F.pf_imdct_512_nol;
#undef F

    /* Initialize the ac3 decoder structures */
#define p_dec p_ac3thread->ac3_decoder
#if defined( __MINGW32__ )
    p_dec->samples_back = memalign( 16, 6 * 256 * sizeof(float) + 15 );
    p_dec->samples = (float *)
                     (((unsigned long) p_dec->samples_back + 15 ) & ~0xFUL);
#else
    p_dec->samples = memalign( 16, 6 * 256 * sizeof(float) );
#endif
#undef p_dec

    IMDCT->buf    = memalign( 16, N/4 * sizeof(complex_t) );
    IMDCT->delay  = memalign( 16, 6 * 256 * sizeof(float) );
    IMDCT->delay1 = memalign( 16, 6 * 256 * sizeof(float) );
    IMDCT->xcos1  = memalign( 16, N/4 * sizeof(float) );
    IMDCT->xsin1  = memalign( 16, N/4 * sizeof(float) );
    IMDCT->xcos2  = memalign( 16, N/8 * sizeof(float) );
    IMDCT->xsin2  = memalign( 16, N/8 * sizeof(float) );
    IMDCT->xcos_sin_sse = memalign( 16, 128 * 4 * sizeof(float) );
    IMDCT->w_1    = memalign( 16, 1  * sizeof(complex_t) );
    IMDCT->w_2    = memalign( 16, 2  * sizeof(complex_t) );
    IMDCT->w_4    = memalign( 16, 4  * sizeof(complex_t) );
    IMDCT->w_8    = memalign( 16, 8  * sizeof(complex_t) );
    IMDCT->w_16   = memalign( 16, 16 * sizeof(complex_t) );
    IMDCT->w_32   = memalign( 16, 32 * sizeof(complex_t) );
    IMDCT->w_64   = memalign( 16, 64 * sizeof(complex_t) );

    ac3_init( p_ac3thread->ac3_decoder );

    /*
     * Initialize the output properties
     */
    p_ac3thread->p_aout_fifo = NULL;

    intf_DbgMsg ( "ac3_adec debug: ac3_adec thread (%p) initialized", 
                  p_ac3thread );

    /*
     * Bit stream
     */
    p_ac3thread->p_config->pf_init_bit_stream(
            &p_ac3thread->ac3_decoder->bit_stream,
            p_ac3thread->p_config->p_decoder_fifo,
            BitstreamCallback, (void *) p_ac3thread );
    
    /* Creating the audio output fifo */
    p_ac3thread->p_aout_fifo = aout_CreateFifo( AOUT_ADEC_STEREO_FIFO, 2, 0, 0,
                                               AC3DEC_FRAME_SIZE, NULL  );
    if ( p_ac3thread->p_aout_fifo == NULL )
    {
        free( IMDCT->w_1 );
        free( IMDCT->w_64 );
        free( IMDCT->w_32 );
        free( IMDCT->w_16 );
        free( IMDCT->w_8 );
        free( IMDCT->w_4 );
        free( IMDCT->w_2 );
        free( IMDCT->xcos_sin_sse );
        free( IMDCT->xsin2 );
        free( IMDCT->xcos2 );
        free( IMDCT->xsin1 );
        free( IMDCT->xcos1 );
        free( IMDCT->delay1 );
        free( IMDCT->delay );
        free( IMDCT->buf );
#undef IMDCT

#if defined( __MINGW32__ )
        free( p_ac3thread->ac3_decoder->samples_back );
#else
        free( p_ac3thread->ac3_decoder->samples );
#endif

        module_Unneed( p_ac3thread->ac3_decoder->imdct->p_module );
        module_Unneed( p_ac3thread->ac3_decoder->downmix.p_module );

        free( p_ac3thread->ac3_decoder->imdct );
        free( p_ac3thread->ac3_decoder );

        return( -1 );
    }

    intf_DbgMsg("ac3dec debug: ac3 decoder thread %p initialized", p_ac3thread);
    
    return( 0 );
}


/*****************************************************************************
 * ac3_adec_ErrorThread : ac3 decoder's RunThread() error loop
 *****************************************************************************/
static void ac3_adec_ErrorThread (ac3dec_thread_t * p_ac3thread)
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock (&p_ac3thread->p_fifo->data_lock);

    /* Wait until a `die' order is sent */
    while (!p_ac3thread->p_fifo->b_die)
    {
        /* Trash all received PES packets */
        while (!DECODER_FIFO_ISEMPTY(*p_ac3thread->p_fifo))
        {
            p_ac3thread->p_fifo->pf_delete_pes(
                    p_ac3thread->p_fifo->p_packets_mgt,
                    DECODER_FIFO_START(*p_ac3thread->p_fifo));
            DECODER_FIFO_INCSTART (*p_ac3thread->p_fifo);
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait (&p_ac3thread->p_fifo->data_wait,
                       &p_ac3thread->p_fifo->data_lock);
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock (&p_ac3thread->p_fifo->data_lock);
}

/*****************************************************************************
 * ac3_adec_EndThread : ac3 decoder thread destruction
 *****************************************************************************/
static void ac3_adec_EndThread (ac3dec_thread_t * p_ac3thread)
{
    intf_DbgMsg ("ac3dec debug: destroying ac3 decoder thread %p", p_ac3thread);

    /* If the audio output fifo was created, we destroy it */
    if (p_ac3thread->p_aout_fifo != NULL)
    {
        aout_DestroyFifo (p_ac3thread->p_aout_fifo);

        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock (&(p_ac3thread->p_aout_fifo->data_lock));
        vlc_cond_signal (&(p_ac3thread->p_aout_fifo->data_wait));
        vlc_mutex_unlock (&(p_ac3thread->p_aout_fifo->data_lock));
    }

    /* Free allocated structures */
#define IMDCT p_ac3thread->ac3_decoder->imdct
    free( IMDCT->w_1 );
    free( IMDCT->w_64 );
    free( IMDCT->w_32 );
    free( IMDCT->w_16 );
    free( IMDCT->w_8 );
    free( IMDCT->w_4 );
    free( IMDCT->w_2 );
    free( IMDCT->xcos_sin_sse );
    free( IMDCT->xsin2 );
    free( IMDCT->xcos2 );
    free( IMDCT->xsin1 );
    free( IMDCT->xcos1 );
    free( IMDCT->delay1 );
    free( IMDCT->delay );
    free( IMDCT->buf );
#undef IMDCT

#if defined( __MINGW32__ )
    free( p_ac3thread->ac3_decoder->samples_back );
#else
    free( p_ac3thread->ac3_decoder->samples );
#endif

    /* Unlock the modules */
    module_Unneed( p_ac3thread->ac3_decoder->downmix.p_module );
    module_Unneed( p_ac3thread->ac3_decoder->imdct->p_module );

    /* Free what's left of the decoder */
    free( p_ac3thread->ac3_decoder->imdct );
    free( p_ac3thread->ac3_decoder );

    intf_DbgMsg( "ac3dec debug: ac3 decoder thread %p destroyed", p_ac3thread );
}

/*****************************************************************************
 * BitstreamCallback: Import parameters from the new data/PES packet
 *****************************************************************************
 * This function is called by input's NextDataPacket.
 *****************************************************************************/
static void BitstreamCallback ( bit_stream_t * p_bit_stream,
                                        boolean_t b_new_pes)
{

    ac3dec_thread_t *p_ac3thread=(ac3dec_thread_t *)p_bit_stream->p_callback_arg;

    if( b_new_pes )
    {
        int ptr;
        
        ptr = *(p_bit_stream->p_byte + 1);
        ptr <<= 8;
        ptr |= *(p_bit_stream->p_byte + 2);
        p_ac3thread->sync_ptr = ptr;
        p_bit_stream->p_byte += 3;                                                            
    }
}

