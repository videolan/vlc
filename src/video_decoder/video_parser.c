/*****************************************************************************
 * video_parser.c : video parser thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_parser.c,v 1.6 2001/10/01 16:18:49 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#include <errno.h>
#include <string.h>

#include <sys/times.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "modules.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "video.h"
#include "video_output.h"

#include "vdec_ext-plugins.h"
#include "vpar_pool.h"
#include "video_parser.h"

#include "main.h"

/*
 * Local prototypes
 */
static int      InitThread          ( vpar_thread_t * );
static void     RunThread           ( vpar_thread_t * );
static void     ErrorThread         ( vpar_thread_t * );
static void     EndThread           ( vpar_thread_t * );
static void     BitstreamCallback   ( bit_stream_t *, boolean_t );

/*****************************************************************************
 * vpar_CreateThread: create a generic parser thread
 *****************************************************************************
 * This function creates a new video parser thread, and returns a pointer
 * to its description. On error, it returns NULL.
 *****************************************************************************/
vlc_thread_t vpar_CreateThread( vdec_config_t * p_config )
{
    vpar_thread_t *     p_vpar;

    intf_DbgMsg( "vpar debug: creating video parser thread" );

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_vpar = (vpar_thread_t *)malloc( sizeof(vpar_thread_t) )) == NULL )
    {
        intf_ErrMsg( "vpar error: not enough memory "
                     "for vpar_CreateThread() to create the new thread");
        return( 0 );
    }

    /*
     * Initialize the thread properties
     */
    p_vpar->p_fifo = p_config->decoder_config.p_decoder_fifo;
    p_vpar->p_config = p_config;
    p_vpar->p_vout = NULL;

    /* Spawn the video parser thread */
    if ( vlc_thread_create( &p_vpar->thread_id, "video parser",
                            (vlc_thread_func_t)RunThread, (void *)p_vpar ) )
    {
        intf_ErrMsg("vpar error: can't spawn video parser thread");
        module_Unneed( p_vpar->p_idct_module );
        module_Unneed( p_vpar->p_motion_module );
        free( p_vpar );
        return( 0 );
    }

    intf_DbgMsg("vpar debug: video parser thread (%p) created", p_vpar);
    return( p_vpar->thread_id );
}

/* following functions are local */

/*****************************************************************************
 * InitThread: initialize vpar output thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
static int InitThread( vpar_thread_t *p_vpar )
{
    intf_DbgMsg("vpar debug: initializing video parser thread %p", p_vpar);

    /*
     * Choose the best motion compensation module
     */
    p_vpar->p_motion_module = module_Need( MODULE_CAPABILITY_MOTION, NULL );

    if( p_vpar->p_motion_module == NULL )
    {
        intf_ErrMsg( "vpar error: no suitable motion compensation module" );
        free( p_vpar );
        return( 0 );
    }

#define f ( p_vpar->p_motion_module->p_functions->motion.functions.motion )
    memcpy( p_vpar->pool.ppppf_motion, f.ppppf_motion, sizeof(void *) * 16 );
#undef f

    /*
     * Choose the best IDCT module
     */
    p_vpar->p_idct_module = module_Need( MODULE_CAPABILITY_IDCT, NULL );

    if( p_vpar->p_idct_module == NULL )
    {
        intf_ErrMsg( "vpar error: no suitable IDCT module" );
        module_Unneed( p_vpar->p_motion_module );
        free( p_vpar );
        return( 0 );
    }

#define f p_vpar->p_idct_module->p_functions->idct.functions.idct
    p_vpar->pool.pf_idct_init   = f.pf_idct_init;
    p_vpar->pf_sparse_idct_add  = f.pf_sparse_idct_add;
    p_vpar->pf_idct_add         = f.pf_idct_add;
    p_vpar->pf_sparse_idct_copy = f.pf_sparse_idct_copy;
    p_vpar->pf_idct_copy        = f.pf_idct_copy;
    p_vpar->pf_norm_scan        = f.pf_norm_scan;
#undef f

    /* Initialize input bitstream */
    p_vpar->p_config->decoder_config.pf_init_bit_stream( &p_vpar->bit_stream,
        p_vpar->p_config->decoder_config.p_decoder_fifo, BitstreamCallback,
        (void *)p_vpar );

    /* Initialize parsing data */
    p_vpar->sequence.p_forward = NULL;
    p_vpar->sequence.p_backward = NULL;
    p_vpar->sequence.intra_quant.b_allocated = 0;
    p_vpar->sequence.nonintra_quant.b_allocated = 0;
    p_vpar->sequence.chroma_intra_quant.b_allocated = 0;
    p_vpar->sequence.chroma_nonintra_quant.b_allocated = 0;
    p_vpar->sequence.i_matrix_coefficients = 1;
    p_vpar->sequence.next_pts = p_vpar->sequence.next_dts = 0;
    p_vpar->sequence.b_expect_discontinuity = 0;

    /* Initialize copyright information */
    p_vpar->sequence.b_copyright_flag = 0;
    p_vpar->sequence.b_original = 0;
    p_vpar->sequence.i_copyright_id = 0;
    p_vpar->sequence.i_copyright_nb = 0;

    p_vpar->picture.p_picture = NULL;
    p_vpar->picture.i_current_structure = 0;

    /* Initialize other properties */
    p_vpar->c_loops = 0;
    p_vpar->c_sequences = 0;
    memset(p_vpar->pc_pictures, 0, sizeof(p_vpar->pc_pictures));
    memset(p_vpar->pc_decoded_pictures, 0, sizeof(p_vpar->pc_decoded_pictures));
    memset(p_vpar->pc_malformed_pictures, 0,
           sizeof(p_vpar->pc_malformed_pictures));
    vpar_InitScanTable( p_vpar );

    /*
     * Initialize the synchro properties
     */
    vpar_SynchroInit( p_vpar );

    /* Spawn optional video decoder threads */
    vpar_InitPool( p_vpar );

    /* Mark thread as running and return */
    intf_DbgMsg("vpar debug: InitThread(%p) succeeded", p_vpar);
    return( 0 );
}

/*****************************************************************************
 * RunThread: generic parser thread
 *****************************************************************************
 * Video parser thread. This function only returns when the thread is
 * terminated.
 *****************************************************************************/
static void RunThread( vpar_thread_t *p_vpar )
{
    intf_DbgMsg("vpar debug: running video parser thread (%p) (pid == %i)", p_vpar, getpid());

    /*
     * Initialize thread
     */
    p_vpar->p_fifo->b_error = InitThread( p_vpar );

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_vpar->p_fifo->b_die) && (!p_vpar->p_fifo->b_error) )
    {
        /* Find the next sequence header in the stream */
        p_vpar->p_fifo->b_error = vpar_NextSequenceHeader( p_vpar );

        while( (!p_vpar->p_fifo->b_die) && (!p_vpar->p_fifo->b_error) )
        {
            p_vpar->c_loops++;

            /* Parse the next sequence, group or picture header */
            if( vpar_ParseHeader( p_vpar ) )
            {
                /* End of sequence */
                break;
            }
        }
    }

    /*
     * Error loop
     */
    if( p_vpar->p_fifo->b_error )
    {
        ErrorThread( p_vpar );
    }

    /* End of thread */
    EndThread( p_vpar );
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *****************************************************************************/
static void ErrorThread( vpar_thread_t *p_vpar )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock( &p_vpar->p_fifo->data_lock );

    /* Wait until a `die' order is sent */
    while( !p_vpar->p_fifo->b_die )
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY(*p_vpar->p_fifo) )
        {
            p_vpar->p_fifo->pf_delete_pes( p_vpar->p_fifo->p_packets_mgt,
                                  DECODER_FIFO_START(*p_vpar->p_fifo) );
            DECODER_FIFO_INCSTART( *p_vpar->p_fifo );
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait( &p_vpar->p_fifo->data_wait, &p_vpar->p_fifo->data_lock );
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock( &p_vpar->p_fifo->data_lock );
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( vpar_thread_t *p_vpar )
{
    intf_DbgMsg("vpar debug: destroying video parser thread %p", p_vpar);

    /* Release used video buffers. */
    if( p_vpar->sequence.p_forward != NULL )
    {
        vout_UnlinkPicture( p_vpar->p_vout, p_vpar->sequence.p_forward );
    }
    if( p_vpar->sequence.p_backward != NULL )
    {
        vout_DatePicture( p_vpar->p_vout, p_vpar->sequence.p_backward,
                          vpar_SynchroDate( p_vpar ) );
        vout_UnlinkPicture( p_vpar->p_vout, p_vpar->sequence.p_backward );
    }
    if( p_vpar->picture.p_picture != NULL )
    {
        vout_DestroyPicture( p_vpar->p_vout, p_vpar->picture.p_picture );
    }

    if( p_main->b_stats )
    {
        struct tms cpu_usage;
        times( &cpu_usage );

        intf_StatMsg( "vpar stats: %d loops among %d sequence(s)",
                      p_vpar->c_loops, p_vpar->c_sequences );

        intf_StatMsg( "vpar stats: cpu usage (user: %d, system: %d)",
                      cpu_usage.tms_utime, cpu_usage.tms_stime );

        intf_StatMsg( "vpar stats: Read %d frames/fields (I %d/P %d/B %d)",
                      p_vpar->pc_pictures[I_CODING_TYPE]
                      + p_vpar->pc_pictures[P_CODING_TYPE]
                      + p_vpar->pc_pictures[B_CODING_TYPE],
                      p_vpar->pc_pictures[I_CODING_TYPE],
                      p_vpar->pc_pictures[P_CODING_TYPE],
                      p_vpar->pc_pictures[B_CODING_TYPE] );
        intf_StatMsg( "vpar stats: Decoded %d frames/fields (I %d/P %d/B %d)",
                      p_vpar->pc_decoded_pictures[I_CODING_TYPE]
                      + p_vpar->pc_decoded_pictures[P_CODING_TYPE]
                      + p_vpar->pc_decoded_pictures[B_CODING_TYPE],
                      p_vpar->pc_decoded_pictures[I_CODING_TYPE],
                      p_vpar->pc_decoded_pictures[P_CODING_TYPE],
                      p_vpar->pc_decoded_pictures[B_CODING_TYPE] );
        intf_StatMsg( "vpar stats: Read %d malformed frames/fields (I %d/P %d/B %d)",
                      p_vpar->pc_malformed_pictures[I_CODING_TYPE]
                      + p_vpar->pc_malformed_pictures[P_CODING_TYPE]
                      + p_vpar->pc_malformed_pictures[B_CODING_TYPE],
                      p_vpar->pc_malformed_pictures[I_CODING_TYPE],
                      p_vpar->pc_malformed_pictures[P_CODING_TYPE],
                      p_vpar->pc_malformed_pictures[B_CODING_TYPE] );
#define S   p_vpar->sequence
        intf_StatMsg( "vpar info: %s stream (%dx%d), %d.%d pi/s",
                      S.b_mpeg2 ? "MPEG-2" : "MPEG-1",
                      S.i_width, S.i_height, S.i_frame_rate/1001,
                      S.i_frame_rate % 1001 );
        intf_StatMsg( "vpar info: %s, %s, matrix_coeff: %d",
                      S.b_progressive ? "Progressive" : "Non-progressive",
                      S.i_scalable_mode ? "scalable" : "non-scalable",
                      S.i_matrix_coefficients );
#undef S
    }

    /* Dispose of matrices if they have been allocated. */
    if( p_vpar->sequence.intra_quant.b_allocated )
    {
        free( p_vpar->sequence.intra_quant.pi_matrix );
    }
    if( p_vpar->sequence.nonintra_quant.b_allocated )
    {
        free( p_vpar->sequence.nonintra_quant.pi_matrix) ;
    }
    if( p_vpar->sequence.chroma_intra_quant.b_allocated )
    {
        free( p_vpar->sequence.chroma_intra_quant.pi_matrix );
    }
    if( p_vpar->sequence.chroma_nonintra_quant.b_allocated )
    {
        free( p_vpar->sequence.chroma_nonintra_quant.pi_matrix );
    }

    vpar_EndPool( p_vpar );

    free( p_vpar->p_config );

    module_Unneed( p_vpar->p_idct_module );
    module_Unneed( p_vpar->p_motion_module );

    free( p_vpar );

    intf_DbgMsg("vpar debug: EndThread(%p)", p_vpar);
}

/*****************************************************************************
 * BitstreamCallback: Import parameters from the new data/PES packet
 *****************************************************************************
 * This function is called by input's NextDataPacket.
 *****************************************************************************/
static void BitstreamCallback ( bit_stream_t * p_bit_stream,
                                boolean_t b_new_pes )
{
    vpar_thread_t * p_vpar = (vpar_thread_t *)p_bit_stream->p_callback_arg;

    if( b_new_pes )
    {
        p_vpar->sequence.next_pts =
            DECODER_FIFO_START( *p_bit_stream->p_decoder_fifo )->i_pts;
        p_vpar->sequence.next_dts =
            DECODER_FIFO_START( *p_bit_stream->p_decoder_fifo )->i_dts;
        p_vpar->sequence.i_current_rate =
            DECODER_FIFO_START( *p_bit_stream->p_decoder_fifo )->i_rate;

        if( DECODER_FIFO_START( *p_bit_stream->p_decoder_fifo )->b_discontinuity )
        {
#ifdef TRACE_VPAR
            intf_DbgMsg( "Discontinuity in BitstreamCallback" );
#endif
            /* Escape the current picture and reset the picture predictors. */
            p_vpar->sequence.b_expect_discontinuity = 1;
            p_vpar->picture.b_error = 1;
        }
    }

    if( p_bit_stream->p_data->b_discard_payload )
    {
#ifdef TRACE_VPAR
        intf_DbgMsg( "Discard payload in BitstreamCallback" );
#endif
        /* 1 packet messed up, trash the slice. */
        p_vpar->picture.b_error = 1;
    }
}
