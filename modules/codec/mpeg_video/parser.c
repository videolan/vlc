/*****************************************************************************
 * video_parser.c : video parser thread
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: parser.c,v 1.8 2002/11/28 17:35:00 sam Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "plugins.h"
#include "decoder.h"
#include "pool.h"
#include "parser.h"

/*
 * Local prototypes
 */
static int      OpenDecoder       ( vlc_object_t * );
static int      RunDecoder        ( decoder_fifo_t * );
static int      InitThread        ( vpar_thread_t * );
static void     EndThread         ( vpar_thread_t * );
static void     BitstreamCallback ( bit_stream_t *, vlc_bool_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define VDEC_IDCT_TEXT N_("IDCT module")
#define VDEC_IDCT_LONGTEXT N_( \
    "This option allows you to select the IDCT module used by this video " \
    "decoder. The default behavior is to automatically select the best " \
    "module available.")

#define VDEC_MOTION_TEXT N_("motion compensation module")
#define VDEC_MOTION_LONGTEXT N_( \
    "This option allows you to select the motion compensation module used by "\
    "this video decoder. The default behavior is to automatically select the "\
    "best module available.")

#define VDEC_SMP_TEXT N_("use additional processors")
#define VDEC_SMP_LONGTEXT N_( \
    "This video decoder can benefit from a multiprocessor computer. If you " \
    "have one, you can specify the number of processors here.")

#define VPAR_SYNCHRO_TEXT N_("force synchro algorithm {I|I+|IP|IP+|IPB}")
#define VPAR_SYNCHRO_LONGTEXT N_( \
    "This allows you to force the synchro algorithm, by directly selecting " \
    "the types of picture you want to decode. Please bear in mind that if " \
    "you select more pictures than what your CPU is capable to decode, " \
    "you won't get anything.")

vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_module  ( "mpeg-idct", "idct", NULL, NULL,
                  VDEC_IDCT_TEXT, VDEC_IDCT_LONGTEXT );
    add_module  ( "mpeg-motion", "motion compensation", NULL, NULL,
                  VDEC_MOTION_TEXT, VDEC_MOTION_LONGTEXT );
    add_integer ( "vdec-smp", 0, NULL, VDEC_SMP_TEXT, VDEC_SMP_LONGTEXT );
    add_string  ( "vpar-synchro", NULL, NULL, VPAR_SYNCHRO_TEXT,
                  VPAR_SYNCHRO_LONGTEXT );
    set_description( _("MPEG I/II video decoder module") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, NULL );
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

    if( p_fifo->i_fourcc == VLC_FOURCC('m','p','g','v') )
    {   
        p_fifo->pf_run = RunDecoder;
        return VLC_SUCCESS;
    }
    
    return VLC_EGENERIC;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder ( decoder_fifo_t * p_fifo )
{
    vpar_thread_t *     p_vpar;
    vlc_bool_t          b_error;

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_vpar = (vpar_thread_t *)malloc( sizeof(vpar_thread_t) )) == NULL )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }

    /*
     * Initialize the thread properties
     */
    p_vpar->p_fifo = p_fifo;
    p_vpar->p_vout = NULL;

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
    if( ( b_error = p_vpar->p_fifo->b_error ) )
    {
        DecoderError( p_vpar->p_fifo );
    }

    /* End of thread */
    EndThread( p_vpar );

    if( b_error )
    {
        return( -1 );
    }
   
    return( 0 );
    
} 

/*****************************************************************************
 * InitThread: initialize vpar output thread
 *****************************************************************************
 * This function is called from Run and performs the second step 
 * of the initialization. It returns 0 on success. Note that the thread's 
 * flag are not modified inside this function.
 *****************************************************************************/
static int InitThread( vpar_thread_t *p_vpar )
{
    /*
     * Choose the best motion compensation module
     */
    p_vpar->p_motion =
         module_Need( p_vpar->p_fifo, "motion compensation", "$mpeg-motion" );

    if( p_vpar->p_motion == NULL )
    {
        msg_Err( p_vpar->p_fifo, "no suitable motion compensation module" );
        free( p_vpar );
        return( -1 );
    }

    memcpy( p_vpar->pool.ppppf_motion,
            p_vpar->p_fifo->p_private, sizeof(void *) * 16 );

    /*
     * Choose the best IDCT module
     */
    p_vpar->p_idct = module_Need( p_vpar->p_fifo, "idct", "$mpeg-idct" );

    if( p_vpar->p_idct == NULL )
    {
        msg_Err( p_vpar->p_fifo, "no suitable IDCT module" );
        module_Unneed( p_vpar->p_fifo, p_vpar->p_motion );
        free( p_vpar );
        return( -1 );
    }

    p_vpar->pool.pf_idct_init   = ((void**)p_vpar->p_fifo->p_private)[0];
    p_vpar->pf_norm_scan        = ((void**)p_vpar->p_fifo->p_private)[1];
    p_vpar->pf_sparse_idct_add  = ((void**)p_vpar->p_fifo->p_private)[2];
    p_vpar->pf_sparse_idct_copy = ((void**)p_vpar->p_fifo->p_private)[3];
    p_vpar->pf_idct_add         = ((void**)p_vpar->p_fifo->p_private)[4];
    p_vpar->pf_idct_copy        = ((void**)p_vpar->p_fifo->p_private)[5];

    /* Initialize input bitstream */
    if( InitBitstream( &p_vpar->bit_stream, p_vpar->p_fifo,
                       BitstreamCallback, (void *)p_vpar ) != VLC_SUCCESS )
    {
        msg_Err( p_vpar->p_fifo, "cannot initialize bitstream" );
        module_Unneed( p_vpar->p_fifo, p_vpar->p_motion );
        free( p_vpar );
        return( -1 );
    }

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

    p_vpar->sequence.i_width = 0;
    p_vpar->sequence.i_height = 0;
    p_vpar->sequence.i_frame_rate = 0;
    p_vpar->sequence.i_scalable_mode = 0;
    p_vpar->sequence.i_matrix_coefficients = 0;
    p_vpar->sequence.b_mpeg2 = 0;
    p_vpar->sequence.b_progressive = 0;

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
    return( 0 );
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( vpar_thread_t *p_vpar )
{
#ifdef HAVE_SYS_TIMES_H
    struct tms cpu_usage;
    times( &cpu_usage );
#endif

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

    vout_Request( p_vpar->p_fifo, p_vpar->p_vout, 0, 0, 0, 0 );

    msg_Dbg( p_vpar->p_fifo, "%d loops among %d sequence(s)",
             p_vpar->c_loops, p_vpar->c_sequences );

#ifdef HAVE_SYS_TIMES_H
    msg_Dbg( p_vpar->p_fifo, "cpu usage (user: %d, system: %d)",
             cpu_usage.tms_utime, cpu_usage.tms_stime );
#endif

    msg_Dbg( p_vpar->p_fifo, "read %d frames/fields (I %d/P %d/B %d)",
             p_vpar->pc_pictures[I_CODING_TYPE]
             + p_vpar->pc_pictures[P_CODING_TYPE]
             + p_vpar->pc_pictures[B_CODING_TYPE],
             p_vpar->pc_pictures[I_CODING_TYPE],
             p_vpar->pc_pictures[P_CODING_TYPE],
             p_vpar->pc_pictures[B_CODING_TYPE] );
    msg_Dbg( p_vpar->p_fifo, "decoded %d frames/fields (I %d/P %d/B %d)",
             p_vpar->pc_decoded_pictures[I_CODING_TYPE]
             + p_vpar->pc_decoded_pictures[P_CODING_TYPE]
             + p_vpar->pc_decoded_pictures[B_CODING_TYPE],
             p_vpar->pc_decoded_pictures[I_CODING_TYPE],
             p_vpar->pc_decoded_pictures[P_CODING_TYPE],
             p_vpar->pc_decoded_pictures[B_CODING_TYPE] );
    msg_Dbg( p_vpar->p_fifo,
             "read %d malformed frames/fields (I %d/P %d/B %d)",
             p_vpar->pc_malformed_pictures[I_CODING_TYPE]
             + p_vpar->pc_malformed_pictures[P_CODING_TYPE]
             + p_vpar->pc_malformed_pictures[B_CODING_TYPE],
             p_vpar->pc_malformed_pictures[I_CODING_TYPE],
             p_vpar->pc_malformed_pictures[P_CODING_TYPE],
             p_vpar->pc_malformed_pictures[B_CODING_TYPE] );
#define S   p_vpar->sequence
    msg_Dbg( p_vpar->p_fifo, "%s stream (%dx%d), %d.%d pi/s",
             S.b_mpeg2 ? "MPEG-2" : "MPEG-1",
             S.i_width, S.i_height, S.i_frame_rate/1001,
             S.i_frame_rate % 1001 );
    msg_Dbg( p_vpar->p_fifo, "%s, %s, matrix_coeff: %d",
             S.b_progressive ? "Progressive" : "Non-progressive",
             S.i_scalable_mode ? "scalable" : "non-scalable",
             S.i_matrix_coefficients );
#undef S

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

    module_Unneed( p_vpar->p_fifo, p_vpar->p_idct );
    module_Unneed( p_vpar->p_fifo, p_vpar->p_motion );

    CloseBitstream( &p_vpar->bit_stream );
    free( p_vpar );
}

/*****************************************************************************
 * BitstreamCallback: Import parameters from the new data/PES packet
 *****************************************************************************
 * This function is called by input's NextDataPacket.
 *****************************************************************************/
static void BitstreamCallback ( bit_stream_t * p_bit_stream,
                                vlc_bool_t b_new_pes )
{
    vpar_thread_t * p_vpar = (vpar_thread_t *)p_bit_stream->p_callback_arg;

    if( b_new_pes )
    {
        p_vpar->sequence.i_current_rate =
            p_bit_stream->p_pes->i_rate;

        if( p_bit_stream->p_pes->b_discontinuity )
        {
            /* Escape the current picture and reset the picture predictors. */
            p_vpar->sequence.b_expect_discontinuity = 1;
            p_vpar->picture.b_error = 1;
        }
    }

    if( p_bit_stream->p_data->b_discard_payload )
    {
        /* 1 packet messed up, trash the slice. */
        p_vpar->picture.b_error = 1;
    }
}
