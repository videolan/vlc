/*****************************************************************************
 * video_parser.c : video parser thread
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: video_parser.c,v 1.22 2002/06/01 13:52:24 sam Exp $
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

#include <errno.h>
#include <string.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "vdec_ext-plugins.h"
#include "video_decoder.h"
#include "vpar_pool.h"
#include "video_parser.h"

/*
 * Local prototypes
 */
static int      decoder_Probe     ( u8 * );
static int      decoder_Run       ( decoder_fifo_t * );
static int      InitThread        ( vpar_thread_t * );
static void     EndThread         ( vpar_thread_t * );
static void     BitstreamCallback ( bit_stream_t *, vlc_bool_t );

/*****************************************************************************
 * Capabilities
 *****************************************************************************/
void _M( vdec_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.dec.pf_probe = decoder_Probe;
    p_function_list->functions.dec.pf_run   = decoder_Run;
}

/*****************************************************************************
 * Build configuration tree.
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

MODULE_CONFIG_START
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL)
ADD_MODULE  ( "mpeg-idct", MODULE_CAPABILITY_IDCT, NULL, NULL,
	      VDEC_IDCT_TEXT, VDEC_IDCT_LONGTEXT )
ADD_MODULE  ( "mpeg-motion", MODULE_CAPABILITY_MOTION, NULL, NULL,
	      VDEC_MOTION_TEXT, VDEC_IDCT_LONGTEXT )
ADD_INTEGER ( "vdec-smp", 0, NULL, VDEC_SMP_TEXT, VDEC_SMP_LONGTEXT )
ADD_STRING  ( "vpar-synchro", NULL, NULL, VPAR_SYNCHRO_TEXT,
              VPAR_SYNCHRO_LONGTEXT )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("MPEG I/II video decoder module") )
    ADD_CAPABILITY( DECODER, 50 )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( vdec_getfunctions )( &p_module->p_functions->dec );
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
    return( ( *pi_type == MPEG1_VIDEO_ES
               || *pi_type == MPEG2_VIDEO_ES ) ? 0 : -1 );
}

/*****************************************************************************
 * decoder_Run: this function is called just after the thread is created
 *****************************************************************************/
static int decoder_Run ( decoder_fifo_t * p_fifo )
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
 * This function is called from decoder_Run and performs the second step 
 * of the initialization. It returns 0 on success. Note that the thread's 
 * flag are not modified inside this function.
 *****************************************************************************/
static int InitThread( vpar_thread_t *p_vpar )
{
    char *psz_name;

    /*
     * Choose the best motion compensation module
     */
    psz_name = config_GetPsz( p_vpar->p_fifo, "mpeg-motion" );
    p_vpar->p_motion_module = module_Need( p_vpar->p_fifo,
                     MODULE_CAPABILITY_MOTION, psz_name, NULL );
    if( psz_name ) free( psz_name );

    if( p_vpar->p_motion_module == NULL )
    {
        msg_Err( p_vpar->p_fifo, "no suitable motion compensation module" );
        free( p_vpar );
        return( -1 );
    }

#define f ( p_vpar->p_motion_module->p_functions->motion.functions.motion )
    memcpy( p_vpar->pool.ppppf_motion, f.ppppf_motion, sizeof(void *) * 16 );
#undef f

    /*
     * Choose the best IDCT module
     */
    psz_name = config_GetPsz( p_vpar->p_fifo, "mpeg-idct" );
    p_vpar->p_idct_module = module_Need( p_vpar->p_fifo,
                                MODULE_CAPABILITY_IDCT, psz_name, NULL );
    if( psz_name ) free( psz_name );

    if( p_vpar->p_idct_module == NULL )
    {
        msg_Err( p_vpar->p_fifo, "no suitable IDCT module" );
        module_Unneed( p_vpar->p_motion_module );
        free( p_vpar );
        return( -1 );
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
    InitBitstream( &p_vpar->bit_stream, p_vpar->p_fifo,
                   BitstreamCallback, (void *)p_vpar );

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

    /* We are about to die. Reattach video output to p_vlc. */
    vlc_object_unlink( p_vpar->p_vout, p_vpar->p_fifo );
    vlc_object_attach( p_vpar->p_vout, p_vpar->p_fifo->p_vlc );

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

    module_Unneed( p_vpar->p_idct_module );
    module_Unneed( p_vpar->p_motion_module );

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
            p_bit_stream->p_decoder_fifo->p_first->i_rate;

        if( p_bit_stream->p_decoder_fifo->p_first->b_discontinuity )
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
