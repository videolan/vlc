/*****************************************************************************
 * ac3_decoder_thread.c: ac3 decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: ac3_decoder_thread.c,v 1.38 2001/10/30 19:34:53 reno Exp $
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

/*
 * TODO :
 *
 * - vérifier l'état de la fifo de sortie avant d'y stocker les samples
 *   décodés ;
 * - vlc_cond_signal() / vlc_cond_wait()
 *
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* memset() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "modules.h"

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "stream_control.h"
#include "input_ext-dec.h"

#include "audio_output.h"

#include "ac3_imdct.h"
#include "ac3_downmix.h"
#include "ac3_decoder.h"
#include "ac3_decoder_thread.h"

#define AC3DEC_FRAME_SIZE (2*1536) 

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      InitThread              (ac3dec_thread_t * p_adec);
static void     RunThread               (ac3dec_thread_t * p_adec);
static void     ErrorThread             (ac3dec_thread_t * p_adec);
static void     EndThread               (ac3dec_thread_t * p_adec);
static void     BitstreamCallback       ( bit_stream_t *p_bit_stream,
                                              boolean_t b_new_pes );

/*****************************************************************************
 * ac3dec_CreateThread: creates an ac3 decoder thread
 *****************************************************************************/
vlc_thread_t ac3dec_CreateThread( adec_config_t * p_config )
{
    ac3dec_thread_t *   p_ac3thread;

    intf_DbgMsg( "ac3dec debug: creating ac3 decoder thread" );

    /* Allocate the memory needed to store the thread's structure */
    p_ac3thread = (ac3dec_thread_t *)memalign(16, sizeof(ac3dec_thread_t));

    if(p_ac3thread == NULL)
    {
        intf_ErrMsg ( "ac3dec error: not enough memory "
                      "for ac3dec_CreateThread() to create the new thread");
        return 0;
    }
   
    /*
     * Initialize the thread properties
     */
    p_ac3thread->p_config = p_config;
    p_ac3thread->p_fifo = p_config->decoder_config.p_decoder_fifo;
    p_ac3thread->ac3_decoder = memalign(16, sizeof(ac3dec_t));

    /*
     * Choose the best downmix module
     */
#define DOWNMIX p_ac3thread->ac3_decoder->downmix
    DOWNMIX.p_module = module_Need( MODULE_CAPABILITY_DOWNMIX, NULL );

    if( DOWNMIX.p_module == NULL )
    {
        intf_ErrMsg( "ac3dec error: no suitable downmix module" );
        free( p_ac3thread->ac3_decoder );
        free( p_ac3thread );
        return( 0 );
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
        free( p_ac3thread );
        return( 0 );
    }

#define F IMDCT->p_module->p_functions->imdct.functions.imdct
    IMDCT->pf_imdct_init    = F.pf_imdct_init;
    IMDCT->pf_imdct_256     = F.pf_imdct_256;
    IMDCT->pf_imdct_256_nol = F.pf_imdct_256_nol;
    IMDCT->pf_imdct_512     = F.pf_imdct_512;
    IMDCT->pf_imdct_512_nol = F.pf_imdct_512_nol;
#undef F
#undef IMDCT

    /* Initialize the ac3 decoder structures */
    p_ac3thread->ac3_decoder->samples = memalign(16, 6 * 256 * sizeof(float));
    p_ac3thread->ac3_decoder->imdct->buf = memalign(16, N/4 * sizeof(complex_t));
    p_ac3thread->ac3_decoder->imdct->delay = memalign(16, 6 * 256 * sizeof(float));
    p_ac3thread->ac3_decoder->imdct->delay1 = memalign(16, 6 * 256 * sizeof(float));
    p_ac3thread->ac3_decoder->imdct->xcos1 = memalign(16, N/4 * sizeof(float));
    p_ac3thread->ac3_decoder->imdct->xsin1 = memalign(16, N/4 * sizeof(float));
    p_ac3thread->ac3_decoder->imdct->xcos2 = memalign(16, N/8 * sizeof(float));
    p_ac3thread->ac3_decoder->imdct->xsin2 = memalign(16, N/8 * sizeof(float));
    p_ac3thread->ac3_decoder->imdct->xcos_sin_sse = memalign(16, 128 * 4 * sizeof(float));
    p_ac3thread->ac3_decoder->imdct->w_2 = memalign(16, 2 * sizeof(complex_t));
    p_ac3thread->ac3_decoder->imdct->w_4 = memalign(16, 4 * sizeof(complex_t));
    p_ac3thread->ac3_decoder->imdct->w_8 = memalign(16, 8 * sizeof(complex_t));
    p_ac3thread->ac3_decoder->imdct->w_16 = memalign(16, 16 * sizeof(complex_t));
    p_ac3thread->ac3_decoder->imdct->w_32 = memalign(16, 32 * sizeof(complex_t));
    p_ac3thread->ac3_decoder->imdct->w_64 = memalign(16, 64 * sizeof(complex_t));
    p_ac3thread->ac3_decoder->imdct->w_1 = memalign(16, sizeof(complex_t));

    ac3_init (p_ac3thread->ac3_decoder);

    /*
     * Initialize the output properties
     */
    p_ac3thread->p_aout_fifo = NULL;

    /* Spawn the ac3 decoder thread */
    if (vlc_thread_create(&p_ac3thread->thread_id, "ac3 decoder", 
                (vlc_thread_func_t)RunThread, (void *)p_ac3thread))
    {
        intf_ErrMsg( "ac3dec error: can't spawn ac3 decoder thread" );
        module_Unneed( p_ac3thread->ac3_decoder->downmix.p_module );
        module_Unneed( p_ac3thread->ac3_decoder->imdct->p_module );
        free( p_ac3thread->ac3_decoder->imdct->w_1 );
        free( p_ac3thread->ac3_decoder->imdct->w_64 );
        free( p_ac3thread->ac3_decoder->imdct->w_32 );
        free( p_ac3thread->ac3_decoder->imdct->w_16 );
        free( p_ac3thread->ac3_decoder->imdct->w_8 );
        free( p_ac3thread->ac3_decoder->imdct->w_4 );
        free( p_ac3thread->ac3_decoder->imdct->w_2 );
        free( p_ac3thread->ac3_decoder->imdct->xcos_sin_sse );
        free( p_ac3thread->ac3_decoder->imdct->xsin2 );
        free( p_ac3thread->ac3_decoder->imdct->xcos2 );
        free( p_ac3thread->ac3_decoder->imdct->xsin1 );
        free( p_ac3thread->ac3_decoder->imdct->xcos1 );
        free( p_ac3thread->ac3_decoder->imdct->delay1 );
        free( p_ac3thread->ac3_decoder->imdct->delay );
        free( p_ac3thread->ac3_decoder->imdct->buf );
        free( p_ac3thread->ac3_decoder->samples );
        free( p_ac3thread->ac3_decoder->imdct );
        free( p_ac3thread->ac3_decoder );
        free( p_ac3thread );
        return 0;
    }

    intf_DbgMsg ("ac3dec debug: ac3 decoder thread (%p) created", p_ac3thread);
    return p_ac3thread->thread_id;
}

/* Following functions are local */

/*****************************************************************************
 * InitThread : initialize an ac3 decoder thread
 *****************************************************************************/
static int InitThread (ac3dec_thread_t * p_ac3thread)
{
    intf_DbgMsg("ac3dec debug: initializing ac3 decoder thread %p",p_ac3thread);

    p_ac3thread->p_config->decoder_config.pf_init_bit_stream(
            &p_ac3thread->ac3_decoder->bit_stream,
            p_ac3thread->p_config->decoder_config.p_decoder_fifo,
            BitstreamCallback, (void *) p_ac3thread );

    /* Creating the audio output fifo */
    p_ac3thread->p_aout_fifo = aout_CreateFifo( AOUT_ADEC_STEREO_FIFO, 2, 0, 0,
                                               AC3DEC_FRAME_SIZE, NULL  );
    if ( p_ac3thread->p_aout_fifo == NULL )
    {
        return -1;
    }

    intf_DbgMsg("ac3dec debug: ac3 decoder thread %p initialized", p_ac3thread);
    return 0;
}

/*****************************************************************************
 * RunThread : ac3 decoder thread
 *****************************************************************************/
static void RunThread (ac3dec_thread_t * p_ac3thread)
{
    int sync;

    intf_DbgMsg ("ac3dec debug: running ac3 decoder thread (%p) (pid == %i)", p_ac3thread, getpid());

    /* Initializing the ac3 decoder thread */
    if (InitThread (p_ac3thread)) /* XXX?? */
    {
        p_ac3thread->p_fifo->b_error = 1;
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
            p_ac3thread->p_aout_fifo->date[p_ac3thread->p_aout_fifo->l_end_frame] =
                DECODER_FIFO_START(*p_ac3thread->p_fifo)->i_pts;
            DECODER_FIFO_START(*p_ac3thread->p_fifo)->i_pts = 0;
        } else {
            p_ac3thread->p_aout_fifo->date[p_ac3thread->p_aout_fifo->l_end_frame] =
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
        ErrorThread (p_ac3thread);
    }

    /* End of the ac3 decoder thread */
    EndThread (p_ac3thread);
}

/*****************************************************************************
 * ErrorThread : ac3 decoder's RunThread() error loop
 *****************************************************************************/
static void ErrorThread (ac3dec_thread_t * p_ac3thread)
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
            p_ac3thread->p_fifo->pf_delete_pes(p_ac3thread->p_fifo->p_packets_mgt,
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
 * EndThread : ac3 decoder thread destruction
 *****************************************************************************/
static void EndThread (ac3dec_thread_t * p_ac3thread)
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

    /* Unlock the modules */
    module_Unneed( p_ac3thread->ac3_decoder->downmix.p_module );
    module_Unneed( p_ac3thread->ac3_decoder->imdct->p_module );

    /* Destroy descriptor */
    free( p_ac3thread->ac3_decoder->imdct->w_1 );
    free( p_ac3thread->ac3_decoder->imdct->w_64 );
    free( p_ac3thread->ac3_decoder->imdct->w_32 );
    free( p_ac3thread->ac3_decoder->imdct->w_16 );
    free( p_ac3thread->ac3_decoder->imdct->w_8 );
    free( p_ac3thread->ac3_decoder->imdct->w_4 );
    free( p_ac3thread->ac3_decoder->imdct->w_2 );
    free( p_ac3thread->ac3_decoder->imdct->xcos_sin_sse );
    free( p_ac3thread->ac3_decoder->imdct->xsin2 );
    free( p_ac3thread->ac3_decoder->imdct->xcos2 );
    free( p_ac3thread->ac3_decoder->imdct->xsin1 );
    free( p_ac3thread->ac3_decoder->imdct->xcos1 );
    free( p_ac3thread->ac3_decoder->imdct->delay1 );
    free( p_ac3thread->ac3_decoder->imdct->delay );
    free( p_ac3thread->ac3_decoder->imdct->buf );
    free( p_ac3thread->ac3_decoder->samples );
    free( p_ac3thread->ac3_decoder->imdct );
    free( p_ac3thread->ac3_decoder );
    free( p_ac3thread->p_config );
    free( p_ac3thread );

    intf_DbgMsg ("ac3dec debug: ac3 decoder thread %p destroyed", p_ac3thread);
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

