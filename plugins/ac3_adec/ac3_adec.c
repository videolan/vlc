/*****************************************************************************
 * ac3_adec.c: ac3 decoder module main file
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ac3_adec.c,v 1.27 2002/04/23 14:16:20 sam Exp $
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

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                           /* getpid() */
#endif

#include "audio_output.h"

#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"                                /* MPEG?_AUDIO_ES */

#include "ac3_imdct.h"
#include "ac3_downmix.h"
#include "ac3_decoder.h"
#include "ac3_adec.h"

#define AC3DEC_FRAME_SIZE (2*1536) 

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  decoder_Probe     ( u8 * );
static int  decoder_Run       ( decoder_config_t * );
static int  InitThread        ( ac3dec_thread_t * p_adec );
static void EndThread         ( ac3dec_thread_t * p_adec );
static void BitstreamCallback ( bit_stream_t *p_bit_stream,
                                boolean_t b_new_pes );

/*****************************************************************************
 * Capabilities
 *****************************************************************************/
void _M( adec_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.dec.pf_probe = decoder_Probe;
    p_function_list->functions.dec.pf_run   = decoder_Run;
}

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
/* Variable containing the AC3 downmix method */
#define DOWNMIX_METHOD_VAR              "ac3-downmix"
/* Variable containing the AC3 IMDCT method */
#define IMDCT_METHOD_VAR                "ac3-imdct"

MODULE_CONFIG_START
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL)
ADD_MODULE  ( DOWNMIX_METHOD_VAR, MODULE_CAPABILITY_DOWNMIX, NULL, NULL,
              N_("AC3 downmix module"), NULL )
ADD_MODULE  ( IMDCT_METHOD_VAR, MODULE_CAPABILITY_IMDCT, NULL, NULL,
              N_("AC3 IMDCT module"), NULL )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("software AC3 decoder") )
    ADD_CAPABILITY( DECODER, 50 )
    ADD_SHORTCUT( "ac3_adec" )
    ADD_SHORTCUT( "ac3" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( adec_getfunctions )( &p_module->p_functions->dec );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP


/*****************************************************************************
 * decoder_Probe: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able 
 * to chose.
 *****************************************************************************/
static int decoder_Probe( u8 *pi_type )
{
    return ( *pi_type == AC3_AUDIO_ES ) ? 0 : -1;
}


/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/
static int InitThread( ac3dec_thread_t * p_ac3thread )
{
    char *psz_name;

    /*
     * Thread properties 
     */
    p_ac3thread->p_fifo = p_ac3thread->p_config->p_decoder_fifo;
    p_ac3thread->ac3_decoder =
        vlc_memalign( 16, sizeof(ac3dec_t), &p_ac3thread->ac3_decoder_orig );

    /*
     * Choose the best downmix module
     */
#define DOWNMIX p_ac3thread->ac3_decoder->downmix
    psz_name = config_GetPszVariable( DOWNMIX_METHOD_VAR );
    DOWNMIX.p_module = module_Need( MODULE_CAPABILITY_DOWNMIX, psz_name,
                                    NULL );
    if( psz_name ) free( psz_name );

    if( DOWNMIX.p_module == NULL )
    {
        intf_ErrMsg( "ac3dec error: no suitable downmix module" );
        free( p_ac3thread->ac3_decoder_orig );
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
    p_ac3thread->ac3_decoder->imdct =
    vlc_memalign( 16, sizeof(imdct_t), &p_ac3thread->ac3_decoder->imdct_orig );
    
#define IMDCT p_ac3thread->ac3_decoder->imdct
    psz_name = config_GetPszVariable( IMDCT_METHOD_VAR );
    IMDCT->p_module = module_Need( MODULE_CAPABILITY_IMDCT, psz_name,
                                   NULL );
    if( psz_name ) free( psz_name );

    if( IMDCT->p_module == NULL )
    {
        intf_ErrMsg( "ac3dec error: no suitable IMDCT module" );
        module_Unneed( p_ac3thread->ac3_decoder->downmix.p_module );
        free( p_ac3thread->ac3_decoder->imdct_orig );
        free( p_ac3thread->ac3_decoder_orig );
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
    p_ac3thread->ac3_decoder->samples =
        vlc_memalign( 16, 6 * 256 * sizeof(float),
                      &p_ac3thread->ac3_decoder->samples_orig );

    IMDCT->buf    = vlc_memalign( 16, N/4 * sizeof(complex_t),
                                  &IMDCT->buf_orig );
    IMDCT->delay  = vlc_memalign( 16, 6 * 256 * sizeof(float),
                                  &IMDCT->delay_orig );
    IMDCT->delay1 = vlc_memalign( 16, 6 * 256 * sizeof(float),
                                  &IMDCT->delay1_orig );
    IMDCT->xcos1  = vlc_memalign( 16, N/4 * sizeof(float),
                                  &IMDCT->xcos1_orig );
    IMDCT->xsin1  = vlc_memalign( 16, N/4 * sizeof(float),
                                  &IMDCT->xsin1_orig );
    IMDCT->xcos2  = vlc_memalign( 16, N/8 * sizeof(float),
                                  &IMDCT->xcos2_orig );
    IMDCT->xsin2  = vlc_memalign( 16, N/8 * sizeof(float),
                                  &IMDCT->xsin2_orig );
    IMDCT->xcos_sin_sse = vlc_memalign( 16, 128 * 4 * sizeof(float),
                                        &IMDCT->xcos_sin_sse_orig );
    IMDCT->w_1    = vlc_memalign( 16, 1  * sizeof(complex_t),
                                  &IMDCT->w_1_orig );
    IMDCT->w_2    = vlc_memalign( 16, 2  * sizeof(complex_t),
                                  &IMDCT->w_2_orig );
    IMDCT->w_4    = vlc_memalign( 16, 4  * sizeof(complex_t),
                                  &IMDCT->w_4_orig );
    IMDCT->w_8    = vlc_memalign( 16, 8  * sizeof(complex_t),
                                  &IMDCT->w_8_orig );
    IMDCT->w_16   = vlc_memalign( 16, 16 * sizeof(complex_t),
                                  &IMDCT->w_16_orig );
    IMDCT->w_32   = vlc_memalign( 16, 32 * sizeof(complex_t),
                                  &IMDCT->w_32_orig );
    IMDCT->w_64   = vlc_memalign( 16, 64 * sizeof(complex_t),
                                  &IMDCT->w_64_orig );

    ac3_init( p_ac3thread->ac3_decoder );

    /*
     * Initialize the output properties
     */
    p_ac3thread->p_aout_fifo = NULL;

    /*
     * Bit stream
     */
    InitBitstream(&p_ac3thread->ac3_decoder->bit_stream,
            p_ac3thread->p_config->p_decoder_fifo,
            BitstreamCallback, (void *) p_ac3thread );
    
    return( 0 );
}

/*****************************************************************************
 * decoder_Run: this function is called just after the thread is created
 *****************************************************************************/
static int decoder_Run ( decoder_config_t * p_config )
{
    ac3dec_thread_t *   p_ac3thread;
    void *              p_ac3thread_orig;         /* pointer before memalign */
    boolean_t           b_sync = 0;

    /* Allocate the memory needed to store the thread's structure */
    p_ac3thread = (ac3dec_thread_t *)vlc_memalign( 16,
                      sizeof(ac3dec_thread_t), &p_ac3thread_orig );

    if( p_ac3thread == NULL )
    {
        intf_ErrMsg ( "ac3_adec error: not enough memory "
                      "for decoder_Run() to allocate p_ac3thread" );
        DecoderError( p_config->p_decoder_fifo );
        return( -1 );
    }

    /*
     * Initialize the thread properties
     */
    p_ac3thread->p_config = p_config;
    if( InitThread( p_ac3thread ) )
    {
        intf_ErrMsg( "ac3_adec error: could not initialize thread" );
        DecoderError( p_config->p_decoder_fifo );
        free( p_ac3thread_orig );
        return( -1 );
    }

    /* ac3 decoder thread's main loop */
    /* FIXME : do we have enough room to store the decoded frames ?? */
    while ((!p_ac3thread->p_fifo->b_die) && (!p_ac3thread->p_fifo->b_error))
    {
        s16 * buffer;
        ac3_sync_info_t sync_info;

        if( !b_sync )
        {
             int i_sync_ptr;
#define p_bit_stream (&p_ac3thread->ac3_decoder->bit_stream)

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

        if (ac3_sync_frame (p_ac3thread->ac3_decoder, &sync_info))
        {
            b_sync = 0;
            continue;
        }

        if( ( p_ac3thread->p_aout_fifo != NULL ) &&
            ( p_ac3thread->p_aout_fifo->i_rate != sync_info.sample_rate ) )
        {
            /* Make sure the output thread leaves the NextFrame() function */
            vlc_mutex_lock (&(p_ac3thread->p_aout_fifo->data_lock));
            aout_DestroyFifo (p_ac3thread->p_aout_fifo);
            vlc_cond_signal (&(p_ac3thread->p_aout_fifo->data_wait));
            vlc_mutex_unlock (&(p_ac3thread->p_aout_fifo->data_lock));

            p_ac3thread->p_aout_fifo = NULL;
        }

        /* Creating the audio output fifo if not created yet */
        if (p_ac3thread->p_aout_fifo == NULL ) {
            p_ac3thread->p_aout_fifo = aout_CreateFifo( AOUT_FIFO_PCM, 2,
                           sync_info.sample_rate, AC3DEC_FRAME_SIZE, NULL  );
            if ( p_ac3thread->p_aout_fifo == NULL )
            {
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

                free( p_ac3thread->ac3_decoder->samples_orig );
        
                module_Unneed( p_ac3thread->ac3_decoder->imdct->p_module );
                module_Unneed( p_ac3thread->ac3_decoder->downmix.p_module );
        
                free( p_ac3thread->ac3_decoder->imdct_orig );
                free( p_ac3thread->ac3_decoder_orig );
        
                return( -1 );
            }
        }

        CurrentPTS( &p_ac3thread->ac3_decoder->bit_stream,
            &p_ac3thread->p_aout_fifo->date[p_ac3thread->p_aout_fifo->i_end_frame],
            NULL );
        if( !p_ac3thread->p_aout_fifo->date[p_ac3thread->p_aout_fifo->i_end_frame] )
        {
            p_ac3thread->p_aout_fifo->date[
                p_ac3thread->p_aout_fifo->i_end_frame] =
                LAST_MDATE;
        }
    
        buffer = ((s16 *)p_ac3thread->p_aout_fifo->buffer) + 
            (p_ac3thread->p_aout_fifo->i_end_frame * AC3DEC_FRAME_SIZE);

        if (ac3_decode_frame (p_ac3thread->ac3_decoder, buffer))
        {
            b_sync = 0;
            continue;
        }
        
        vlc_mutex_lock (&p_ac3thread->p_aout_fifo->data_lock);
        p_ac3thread->p_aout_fifo->i_end_frame = 
            (p_ac3thread->p_aout_fifo->i_end_frame + 1) & AOUT_FIFO_SIZE;
        vlc_cond_signal (&p_ac3thread->p_aout_fifo->data_wait);
        vlc_mutex_unlock (&p_ac3thread->p_aout_fifo->data_lock);

        RealignBits(&p_ac3thread->ac3_decoder->bit_stream);
    }

    /* If b_error is set, the ac3 decoder thread enters the error loop */
    if (p_ac3thread->p_fifo->b_error)
    {
        DecoderError( p_ac3thread->p_fifo );
    }

    /* End of the ac3 decoder thread */
    EndThread (p_ac3thread);

    free( p_ac3thread_orig );

    return( 0 );
}


/*****************************************************************************
 * EndThread : ac3 decoder thread destruction
 *****************************************************************************/
static void EndThread (ac3dec_thread_t * p_ac3thread)
{
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

    free( p_ac3thread->ac3_decoder->samples_orig );

    /* Unlock the modules */
    module_Unneed( p_ac3thread->ac3_decoder->downmix.p_module );
    module_Unneed( p_ac3thread->ac3_decoder->imdct->p_module );

    /* Free what's left of the decoder */
    free( p_ac3thread->ac3_decoder->imdct_orig );
    free( p_ac3thread->ac3_decoder_orig );
}

/*****************************************************************************
 * BitstreamCallback: Import parameters from the new data/PES packet
 *****************************************************************************
 * This function is called by input's NextDataPacket.
 *****************************************************************************/
static void BitstreamCallback ( bit_stream_t * p_bit_stream,
                                boolean_t b_new_pes )
{
    if( b_new_pes )
    {
        /* Drop special AC3 header */
        p_bit_stream->p_byte += 3;
    }
}

