/*****************************************************************************
 * vpar_headers.c : headers parsing
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: vpar_headers.c,v 1.12 2002/01/14 23:46:35 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
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
#include <stdlib.h>                                                /* free() */
#include <string.h>                                    /* memcpy(), memset() */

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "vdec_ext-plugins.h"
#include "vpar_pool.h"
#include "video_parser.h"
#include "video_decoder.h"

/*
 * Local prototypes
 */
static __inline__ void NextStartCode( bit_stream_t * );
static void SequenceHeader( vpar_thread_t * p_vpar );
static void GroupHeader( vpar_thread_t * p_vpar );
static void PictureHeader( vpar_thread_t * p_vpar );
static void ExtensionAndUserData( vpar_thread_t * p_vpar );
static void QuantMatrixExtension( vpar_thread_t * p_vpar );
static void SequenceScalableExtension( vpar_thread_t * p_vpar );
static void SequenceDisplayExtension( vpar_thread_t * p_vpar );
static void PictureDisplayExtension( vpar_thread_t * p_vpar );
static void PictureSpatialScalableExtension( vpar_thread_t * p_vpar );
static void PictureTemporalScalableExtension( vpar_thread_t * p_vpar );
static void CopyrightExtension( vpar_thread_t * p_vpar );

/*
 * Standard variables
 */

/*****************************************************************************
 * pi_default_intra_quant : default quantization matrix
 *****************************************************************************/
u8 pi_default_intra_quant[] ATTR_ALIGN(16) =
{
    8,  16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83
};

/*****************************************************************************
 * pi_default_nonintra_quant : default quantization matrix
 *****************************************************************************/
u8 pi_default_nonintra_quant[] ATTR_ALIGN(16) =
{
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16
};

/*****************************************************************************
 * pi_scan : zig-zag and alternate scan patterns
 *****************************************************************************/
u8 pi_scan[2][64] ATTR_ALIGN(16) =
{
    { /* Zig-Zag pattern */
        0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
        12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
        35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
        58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
    },
    { /* Alternate scan pattern */
        0,8,16,24,1,9,2,10,17,25,32,40,48,56,57,49,
        41,33,26,18,3,11,4,12,19,27,34,42,50,58,35,43,
        51,59,20,28,5,13,6,14,21,29,36,44,52,60,37,45,
        53,61,22,30,7,15,23,31,38,46,54,62,39,47,55,63
    }
};

/*
 * Local inline functions.
 */

/*****************************************************************************
 * ReferenceUpdate : Update the reference pointers when we have a new picture
 *****************************************************************************/
static void __inline__ ReferenceUpdate( vpar_thread_t * p_vpar,
                                        int i_coding_type,
                                        picture_t * p_newref )
{
    if( i_coding_type != B_CODING_TYPE )
    {
        if( p_vpar->sequence.p_forward != NULL )
        {
            vout_UnlinkPicture( p_vpar->p_vout, p_vpar->sequence.p_forward );
        }
        if( p_vpar->sequence.p_backward != NULL )
        {
            vout_DatePicture( p_vpar->p_vout, p_vpar->sequence.p_backward,
                              vpar_SynchroDate( p_vpar ) );
        }
        p_vpar->sequence.p_forward = p_vpar->sequence.p_backward;
        p_vpar->sequence.p_backward = p_newref;
        if( p_newref != NULL )
        {
            vout_LinkPicture( p_vpar->p_vout, p_newref );
        }
    }
    else if( p_newref != NULL )
    {
        /* Put date immediately. */
        vout_DatePicture( p_vpar->p_vout, p_newref, vpar_SynchroDate(p_vpar) );
    }
}

/*****************************************************************************
 * ReferenceReplace : Replace the last reference pointer when we destroy
 *                    a picture
 *****************************************************************************/
static void __inline__ ReferenceReplace( vpar_thread_t * p_vpar,
                                         int i_coding_type,
                                         picture_t * p_newref )
{
    if( i_coding_type != B_CODING_TYPE )
    {
        if( p_vpar->sequence.p_backward != NULL )
        {
            vout_UnlinkPicture( p_vpar->p_vout, p_vpar->sequence.p_backward );
        }
        p_vpar->sequence.p_backward = p_newref;
        if( p_newref != NULL )
        {
            vout_LinkPicture( p_vpar->p_vout, p_newref );
        }
    }
}

/*****************************************************************************
 * LoadMatrix : Load a quantization matrix
 *****************************************************************************/
static __inline__ void LoadMatrix( vpar_thread_t * p_vpar,
                                   quant_matrix_t * p_matrix )
{
    int i_dummy;

    if( !p_matrix->b_allocated )
    {
        /* Allocate a piece of memory to load the matrix. */
        if( (p_matrix->pi_matrix = (u8 *)malloc( 64*sizeof(u8) )) == NULL )
        {
            intf_ErrMsg( "vpar error: allocation error in LoadMatrix()" );
            p_vpar->p_fifo->b_error = 1;
            return;
        }
        p_matrix->b_allocated = 1;
    }

    for( i_dummy = 0; i_dummy < 64; i_dummy++ )
    {
        p_matrix->pi_matrix[p_vpar->ppi_scan[0][i_dummy]]
             = GetBits( &p_vpar->bit_stream, 8 );
    }
}

/*****************************************************************************
 * LinkMatrix : Link a quantization matrix to another
 *****************************************************************************/
static __inline__ void LinkMatrix( quant_matrix_t * p_matrix, u8 * pi_array )
{
    if( p_matrix->b_allocated )
    {
        /* Deallocate the piece of memory. */
        free( p_matrix->pi_matrix );
        p_matrix->b_allocated = 0;
    }

    p_matrix->pi_matrix = pi_array;
}

/*****************************************************************************
 * ChromaToFourCC: Return a FourCC value used by the video output.
 *****************************************************************************/
static __inline__ u64 ChromaToFourCC( int i_chroma )
{
    switch( i_chroma )
    {
        case CHROMA_420:
            return FOURCC_I420;

        case CHROMA_422:
            return FOURCC_I422;

        case CHROMA_444:
            return FOURCC_I444;

        default:
            /* This can't happen */
            return 0xdeadbeef;
    }
}

/*
 * Exported functions.
 */

/*****************************************************************************
 * vpar_NextSequenceHeader : Find the next sequence header
 *****************************************************************************/
int vpar_NextSequenceHeader( vpar_thread_t * p_vpar )
{
    while( !p_vpar->p_fifo->b_die )
    {
        NextStartCode( &p_vpar->bit_stream );
        if( ShowBits( &p_vpar->bit_stream, 32 ) == SEQUENCE_HEADER_CODE )
        {
            return 0;
        }
        RemoveBits( &p_vpar->bit_stream, 8 );
    }
    return 1;
}

/*****************************************************************************
 * vpar_ParseHeader : Parse the next header
 *****************************************************************************/
int vpar_ParseHeader( vpar_thread_t * p_vpar )
{
    NextStartCode( &p_vpar->bit_stream );
    switch( GetBits32( &p_vpar->bit_stream ) )
    {
    case SEQUENCE_HEADER_CODE:
        p_vpar->c_sequences++;
        SequenceHeader( p_vpar );
        return 0;
        break;

    case GROUP_START_CODE:
        GroupHeader( p_vpar );
        return 0;
        break;

    case PICTURE_START_CODE:
        PictureHeader( p_vpar );
        return 0;
        break;

    case SEQUENCE_END_CODE:
        intf_WarnMsg(3, "vpar warning: sequence end code received");
        return 1;
        break;

    default:
        break;
    }

    return 0;
}

/*
 * Following functions are local
 */

/*****************************************************************************
 * SequenceHeader : Parse the next sequence header
 *****************************************************************************/
static void SequenceHeader( vpar_thread_t * p_vpar )
{
#define RESERVED    -1
    static int i_frame_rate_table[16] =
    {
        0,
        23 * 1000,
        24 * 1001,
        25 * 1001,
        30 * 1000,
        30 * 1001,
        50 * 1001,
        60 * 1000,
        60 * 1001,
        RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED
    };
#undef RESERVED

    int i_height_save, i_width_save, i_aspect;

    i_height_save = p_vpar->sequence.i_height;
    i_width_save = p_vpar->sequence.i_width;

    p_vpar->sequence.i_width = GetBits( &p_vpar->bit_stream, 12 );
    p_vpar->sequence.i_height = GetBits( &p_vpar->bit_stream, 12 );
    i_aspect = GetBits( &p_vpar->bit_stream, 4 );
    p_vpar->sequence.i_frame_rate =
            i_frame_rate_table[ GetBits( &p_vpar->bit_stream, 4 ) ];

    /* We don't need bit_rate_value, marker_bit, vbv_buffer_size,
     * constrained_parameters_flag */
    RemoveBits( &p_vpar->bit_stream, 30 );

    /*
     * Quantization matrices
     */
    if( GetBits( &p_vpar->bit_stream, 1 ) ) /* load_intra_quantizer_matrix */
    {
        LoadMatrix( p_vpar, &p_vpar->sequence.intra_quant );
    }
    else
    {
        /* Use default matrix. */
        LinkMatrix( &p_vpar->sequence.intra_quant,
                    p_vpar->pi_default_intra_quant );
    }

    if( GetBits(&p_vpar->bit_stream, 1) ) /* load_non_intra_quantizer_matrix */
    {
        LoadMatrix( p_vpar, &p_vpar->sequence.nonintra_quant );
    }
    else
    {
        /* Use default matrix. */
        LinkMatrix( &p_vpar->sequence.nonintra_quant,
                    p_vpar->pi_default_nonintra_quant );
    }

    /* Unless later overwritten by a matrix extension, we have the same
     * matrices for luminance and chrominance. */
    LinkMatrix( &p_vpar->sequence.chroma_intra_quant,
                p_vpar->sequence.intra_quant.pi_matrix );
    LinkMatrix( &p_vpar->sequence.chroma_nonintra_quant,
                p_vpar->sequence.nonintra_quant.pi_matrix );

    /*
     * Sequence Extension
     */
    NextStartCode( &p_vpar->bit_stream );
    if( ShowBits( &p_vpar->bit_stream, 32 ) == EXTENSION_START_CODE )
    {
        int                         i_dummy;

        /* Turn the MPEG2 flag on */
        p_vpar->sequence.b_mpeg2 = 1;

        /* Parse sequence_extension */
        RemoveBits32( &p_vpar->bit_stream );
        /* extension_start_code_identifier, profile_and_level_indication */
        RemoveBits( &p_vpar->bit_stream, 12 );
        p_vpar->sequence.b_progressive = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->sequence.i_chroma_format = GetBits( &p_vpar->bit_stream, 2 );
        p_vpar->sequence.i_width |= GetBits( &p_vpar->bit_stream, 2 ) << 12;
        p_vpar->sequence.i_height |= GetBits( &p_vpar->bit_stream, 2 ) << 12;
        /* bit_rate_extension, marker_bit, vbv_buffer_size_extension,
         * low_delay */
        RemoveBits( &p_vpar->bit_stream, 22 );
        /* frame_rate_extension_n */
        i_dummy = GetBits( &p_vpar->bit_stream, 2 );
        /* frame_rate_extension_d */
        p_vpar->sequence.i_frame_rate *= (i_dummy + 1)
                                  / (GetBits( &p_vpar->bit_stream, 5 ) + 1);
    }
    else
    {
        /* It's an MPEG-1 stream. Put adequate parameters. */
        int i_xyratio;
        static int pi_mpeg1ratio[15] = {
            10000, 10000,  6735,  7031,  7615,  8055,  8437, 8935,
             9157,  9815, 10255, 10695, 10950, 11575, 12015
        };

        if( i_aspect > 1 )
        {
                i_xyratio = p_vpar->sequence.i_height *
                        pi_mpeg1ratio[i_aspect] / p_vpar->sequence.i_width;
                if( 7450 < i_xyratio && i_xyratio < 7550 )
                {
                        i_aspect = 2;
                }
                else if( 5575 < i_xyratio && i_xyratio < 5675 )
                {
                        i_aspect = 3;
                }
                else if( 4475 < i_xyratio && i_xyratio < 4575 )
                {
                        i_aspect = 4;
                }
        }

        p_vpar->sequence.b_mpeg2 = 0;
        p_vpar->sequence.b_progressive = 1;
        p_vpar->sequence.i_chroma_format = CHROMA_420;
    }

    /* Store calculated aspect ratio */
    switch( i_aspect )
    {
        case AR_3_4_PICTURE:
            p_vpar->sequence.i_aspect = VOUT_ASPECT_FACTOR * 4 / 3;
            break;

        case AR_16_9_PICTURE:
            p_vpar->sequence.i_aspect = VOUT_ASPECT_FACTOR * 16 / 9;
            break;

        case AR_221_1_PICTURE:
            p_vpar->sequence.i_aspect = VOUT_ASPECT_FACTOR * 221 / 100;
            break;

        case AR_SQUARE_PICTURE:
        default:
            p_vpar->sequence.i_aspect = VOUT_ASPECT_FACTOR
                    * p_vpar->sequence.i_width / p_vpar->sequence.i_height;
            break;
    }

    /* Update sizes */
    p_vpar->sequence.i_mb_width = (p_vpar->sequence.i_width + 15) / 16;
    p_vpar->sequence.i_mb_height = (p_vpar->sequence.b_progressive) ?
                                   (p_vpar->sequence.i_height + 15) / 16 :
                                   2 * ((p_vpar->sequence.i_height + 31) / 32);
    p_vpar->sequence.i_mb_size = p_vpar->sequence.i_mb_width
                                        * p_vpar->sequence.i_mb_height;
    p_vpar->sequence.i_width = (p_vpar->sequence.i_mb_width * 16);
    p_vpar->sequence.i_height = (p_vpar->sequence.i_mb_height * 16);
    p_vpar->sequence.i_size = p_vpar->sequence.i_width
                                        * p_vpar->sequence.i_height;
    switch( p_vpar->sequence.i_chroma_format )
    {
    case CHROMA_420:
        p_vpar->sequence.i_chroma_nb_blocks = 2;
        p_vpar->sequence.b_chroma_h_subsampled = 1;
        p_vpar->sequence.b_chroma_v_subsampled = 1;
        break;
    case CHROMA_422:
        p_vpar->sequence.i_chroma_nb_blocks = 4;
        p_vpar->sequence.b_chroma_h_subsampled = 1;
        p_vpar->sequence.b_chroma_v_subsampled = 0;
        break;
    case CHROMA_444:
        p_vpar->sequence.i_chroma_nb_blocks = 6;
        p_vpar->sequence.b_chroma_h_subsampled = 0;
        p_vpar->sequence.b_chroma_v_subsampled = 0;
        break;
    }

#if 0
    if(    p_vpar->sequence.i_width != i_width_save
        || p_vpar->sequence.i_height != i_height_save )
    {
         /* FIXME: Warn the video output */
    }
#endif

    /* Extension and User data */
    ExtensionAndUserData( p_vpar );

    /* XXX: The vout request and fifo opening will eventually be here */

    /* Spawn a video output if there is none */
    vlc_mutex_lock( &p_vout_bank->lock );
    
    if( p_vout_bank->i_count == 0 )
    {
        intf_WarnMsg( 1, "vpar: no vout present, spawning one" );

        p_vpar->p_vout = vout_CreateThread(
                           NULL, p_vpar->sequence.i_width,
                           p_vpar->sequence.i_height,
                           ChromaToFourCC( p_vpar->sequence.i_chroma_format ),
                           p_vpar->sequence.i_aspect );

        /* Everything failed */
        if( p_vpar->p_vout == NULL )
        {
            intf_ErrMsg( "vpar error: can't open vout, aborting" );
            vlc_mutex_unlock( &p_vout_bank->lock );

            p_vpar->p_fifo->b_error = 1;
            return;
        }
        
        p_vout_bank->pp_vout[ p_vout_bank->i_count ] = p_vpar->p_vout;
        p_vout_bank->i_count++;
    }
    else
    {
        /* Take the first video output FIXME: take the best one */
        p_vpar->p_vout = p_vout_bank->pp_vout[ 0 ];
    }

    vlc_mutex_unlock( &p_vout_bank->lock );
}

/*****************************************************************************
 * GroupHeader : Parse the next group of pictures header
 *****************************************************************************/
static void GroupHeader( vpar_thread_t * p_vpar )
{
    /* Nothing to do, we don't care. */
    RemoveBits( &p_vpar->bit_stream, 27 );
    ExtensionAndUserData( p_vpar );
}

/*****************************************************************************
 * PictureHeader : Parse the next picture header
 *****************************************************************************/
static void PictureHeader( vpar_thread_t * p_vpar )
{
    int                 i_structure, i_previous_coding_type;
    boolean_t           b_parsable = 0;

    /* Retrieve the PTS. */
    CurrentPTS( &p_vpar->bit_stream, &p_vpar->sequence.next_pts,
                &p_vpar->sequence.next_dts );

    /* Recover in case of stream discontinuity. */
    if( p_vpar->sequence.b_expect_discontinuity )
    {
        ReferenceUpdate( p_vpar, I_CODING_TYPE, NULL );
        ReferenceUpdate( p_vpar, I_CODING_TYPE, NULL );
        if( p_vpar->picture.p_picture != NULL )
        {
            vout_DestroyPicture( p_vpar->p_vout, p_vpar->picture.p_picture );
            p_vpar->picture.p_picture = NULL;
        }
        p_vpar->picture.i_current_structure = 0;
        p_vpar->sequence.b_expect_discontinuity = 0;
    }

    /* Parse the picture header. */
    RemoveBits( &p_vpar->bit_stream, 10 ); /* temporal_reference */
    i_previous_coding_type = p_vpar->picture.i_coding_type;
    p_vpar->picture.i_coding_type = GetBits( &p_vpar->bit_stream, 3 );
    RemoveBits( &p_vpar->bit_stream, 16 ); /* vbv_delay */

    if( p_vpar->picture.i_coding_type == P_CODING_TYPE
        || p_vpar->picture.i_coding_type == B_CODING_TYPE )
    {
        p_vpar->picture.ppi_f_code[0][1] = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.ppi_f_code[0][0] = GetBits( &p_vpar->bit_stream, 3 )
                                            - 1;
    }
    if( p_vpar->picture.i_coding_type == B_CODING_TYPE )
    {
        p_vpar->picture.ppi_f_code[1][1] = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.ppi_f_code[1][0] = GetBits( &p_vpar->bit_stream, 3 )
                                            - 1;
    }

    /* extra_information_picture */
    while( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        RemoveBits( &p_vpar->bit_stream, 8 );
    }

    /*
     * Picture Coding Extension
     */
    NextStartCode( &p_vpar->bit_stream );
    if( ShowBits( &p_vpar->bit_stream, 32 ) == EXTENSION_START_CODE )
    {
        /* Parse picture_coding_extension */
        RemoveBits32( &p_vpar->bit_stream );
        /* extension_start_code_identifier */
        RemoveBits( &p_vpar->bit_stream, 4 );

        /* Pre-substract 1 for later use in MotionDelta(). */
        p_vpar->picture.ppi_f_code[0][0] = GetBits( &p_vpar->bit_stream, 4 ) -1;
        p_vpar->picture.ppi_f_code[0][1] = GetBits( &p_vpar->bit_stream, 4 ) -1;
        p_vpar->picture.ppi_f_code[1][0] = GetBits( &p_vpar->bit_stream, 4 ) -1;
        p_vpar->picture.ppi_f_code[1][1] = GetBits( &p_vpar->bit_stream, 4 ) -1;
        p_vpar->picture.i_intra_dc_precision = GetBits( &p_vpar->bit_stream, 2 );
        i_structure = GetBits( &p_vpar->bit_stream, 2 );
        p_vpar->picture.b_top_field_first = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_frame_pred_frame_dct
             = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_concealment_mv = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_q_scale_type = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_intra_vlc_format = GetBits( &p_vpar->bit_stream, 1 );
        /* Alternate scan */
        p_vpar->picture.pi_scan =
            p_vpar->ppi_scan[ GetBits( &p_vpar->bit_stream, 1 ) ];
        p_vpar->picture.b_repeat_first_field = GetBits( &p_vpar->bit_stream, 1 );
        /* chroma_420_type (obsolete) */
        RemoveBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_progressive = GetBits( &p_vpar->bit_stream, 1 );

        /* composite_display_flag */
        if( GetBits( &p_vpar->bit_stream, 1 ) )
        {
            /* v_axis, field_sequence, sub_carrier, burst_amplitude,
             * sub_carrier_phase */
            RemoveBits( &p_vpar->bit_stream, 20 );
        }
    }
    else
    {
        /* MPEG-1 compatibility flags */
        p_vpar->picture.i_intra_dc_precision = 0; /* 8 bits */
        i_structure = FRAME_STRUCTURE;
        p_vpar->picture.b_top_field_first = 0;
        p_vpar->picture.b_frame_pred_frame_dct = 1;
        p_vpar->picture.b_concealment_mv = 0;
        p_vpar->picture.b_q_scale_type = 0;
        p_vpar->picture.b_intra_vlc_format = 0;
        p_vpar->picture.pi_scan = p_vpar->ppi_scan[0];
        p_vpar->picture.b_repeat_first_field = 0;
        p_vpar->picture.b_progressive = 1;
    }

    /* Extension and User data. */
    ExtensionAndUserData( p_vpar );

    p_vpar->pc_pictures[p_vpar->picture.i_coding_type]++;

    if( p_vpar->picture.i_current_structure )
    {
        if ( (i_structure == FRAME_STRUCTURE ||
              i_structure == p_vpar->picture.i_current_structure) )
        {
            /* We don't have the second field of the buffered frame. */
            if( p_vpar->picture.p_picture != NULL )
            {
                ReferenceReplace( p_vpar,
                                  p_vpar->picture.i_coding_type,
                                  NULL );
                vout_DestroyPicture( p_vpar->p_vout,
                                     p_vpar->picture.p_picture );
                p_vpar->picture.p_picture = NULL;
            }

            p_vpar->picture.i_current_structure = 0;

            intf_WarnMsg( 2, "Odd number of field pictures." );
        }
        else
        {
            /* Second field of a frame. We will decode it if, and only if we
             * have decoded the first field. */
            if( p_vpar->picture.p_picture == NULL )
            {
                if( (p_vpar->picture.i_coding_type == I_CODING_TYPE
                      && p_vpar->sequence.p_backward == NULL) )
                {
                    /* Exceptionnally, parse the picture if it is I. We need
                     * this in case of an odd number of field pictures, if the
                     * previous picture is not intra, we desperately need a
                     * new reference picture. OK, this is kind of kludgy. */
                    p_vpar->picture.i_current_structure = 0;
                }
                else
                {
                    b_parsable = 0;
                }
            }
            else
            {
                /* We suppose we have the reference pictures, since we already
                 * decoded the first field and the second field will not need
                 * any extra reference picture. There is a special case of
                 * P field being the second field of an I field, but ISO/IEC
                 * 13818-2 section 7.6.3.5 specifies that this P field will
                 * not need any reference picture besides the I field. So far
                 * so good. */
                b_parsable = 1;

                if( p_vpar->picture.i_coding_type == P_CODING_TYPE &&
                    i_previous_coding_type == I_CODING_TYPE &&
                    p_vpar->sequence.p_forward == NULL )
                {
                    /* This is the special case of section 7.6.3.5. Create
                     * a fake reference picture (which will not be used)
                     * but will prevent us from segfaulting in the slice
                     * parsing. */
                    static picture_t    fake_picture;
                    fake_picture.i_planes = 0; /* We will use it later */
                    p_vpar->sequence.p_forward = &fake_picture;
                }
            }
        }
    }

    if( !p_vpar->picture.i_current_structure )
    {
        /* First field of a frame, or new frame picture. */
        int     i_repeat_field;

        /* Do we have the reference pictures ? */
        b_parsable = !(((p_vpar->picture.i_coding_type == P_CODING_TYPE ||
                         p_vpar->picture.b_concealment_mv) &&
                        (p_vpar->sequence.p_backward == NULL)) ||
                         /* p_backward will become p_forward later */
                       ((p_vpar->picture.i_coding_type == B_CODING_TYPE) &&
                        (p_vpar->sequence.p_forward == NULL ||
                         p_vpar->sequence.p_backward == NULL)));

        /* Compute the number of times the frame will be emitted by the
         * decoder (number of half-periods). */
        if( p_vpar->sequence.b_progressive )
        {
            i_repeat_field = (1 + p_vpar->picture.b_repeat_first_field
                                + p_vpar->picture.b_top_field_first) * 2;
        }
        else
        {
            if( p_vpar->picture.b_progressive )
            {
                i_repeat_field = 2 + p_vpar->picture.b_repeat_first_field;
            }
            else
            {
                i_repeat_field = 2;
            }
        }

        /* Warn synchro we have a new picture (updates pictures index). */
        vpar_SynchroNewPicture( p_vpar, p_vpar->picture.i_coding_type,
                                i_repeat_field );

        if( b_parsable )
        {
            /* Does synchro say we have enough time to decode it ? */
            b_parsable = vpar_SynchroChoose( p_vpar,
                               p_vpar->picture.i_coding_type, i_structure );
        }
    }

    if( !b_parsable )
    {
        /* Update the reference pointers. */
        ReferenceUpdate( p_vpar, p_vpar->picture.i_coding_type, NULL );

        /* Update context. */
        if( i_structure != FRAME_STRUCTURE )
        {
            if( (p_vpar->picture.i_current_structure | i_structure)
                    == FRAME_STRUCTURE )
            {
                /* The frame is complete. */
                p_vpar->picture.i_current_structure = 0;

                vpar_SynchroTrash( p_vpar, p_vpar->picture.i_coding_type, i_structure );
            }
            else
            {
                p_vpar->picture.i_current_structure = i_structure;
            }
        }
        else
        {
            /* Warn Synchro we have trashed a picture. */
            vpar_SynchroTrash( p_vpar, p_vpar->picture.i_coding_type, i_structure );
        }
        p_vpar->picture.p_picture = NULL;

        return;
    }

    /* OK, now we are sure we will decode the picture. */
    p_vpar->pc_decoded_pictures[p_vpar->picture.i_coding_type]++;

#define P_picture p_vpar->picture.p_picture
    p_vpar->picture.b_error = 0;
    p_vpar->picture.b_frame_structure = (i_structure == FRAME_STRUCTURE);

    if( !p_vpar->picture.i_current_structure )
    {
        /* This is a new frame. Get a structure from the video_output. */
        while( ( P_picture = vout_CreatePicture( p_vpar->p_vout,
                                 p_vpar->picture.b_progressive,
                                 p_vpar->picture.b_top_field_first,
                                 p_vpar->picture.b_repeat_first_field ) )
                 == NULL )
        {
            intf_DbgMsg("vpar debug: vout_CreatePicture failed, delaying");
            if( p_vpar->p_fifo->b_die || p_vpar->p_fifo->b_error )
            {
                return;
            }
            msleep( VOUT_OUTMEM_SLEEP );
        }

        /* Initialize values. */
        vpar_SynchroDecode( p_vpar, p_vpar->picture.i_coding_type, i_structure );
        P_picture->i_matrix_coefficients = p_vpar->sequence.i_matrix_coefficients;
        p_vpar->picture.i_field_width = ( p_vpar->sequence.i_width
                    << ( 1 - p_vpar->picture.b_frame_structure ) );

        /* Update the reference pointers. */
        ReferenceUpdate( p_vpar, p_vpar->picture.i_coding_type, P_picture );
    }

    /* Initialize picture data for decoding. */
    p_vpar->picture.i_current_structure |= i_structure;
    p_vpar->picture.i_structure = i_structure;
    p_vpar->picture.b_second_field =
        (i_structure != p_vpar->picture.i_current_structure);
    p_vpar->picture.b_current_field =
        (i_structure == BOTTOM_FIELD );

    if( !p_vpar->p_config->p_stream_ctrl->b_grayscale )
    {
        switch( p_vpar->sequence.i_chroma_format )
        {
        case CHROMA_422:
            p_vpar->pool.pf_vdec_decode = vdec_DecodeMacroblock422;
            break;
        case CHROMA_444:
            p_vpar->pool.pf_vdec_decode = vdec_DecodeMacroblock444;
            break;
        case CHROMA_420:
        default:
            p_vpar->pool.pf_vdec_decode = vdec_DecodeMacroblock420;
            break;
        }
    }
    else
    {
        p_vpar->pool.pf_vdec_decode = vdec_DecodeMacroblockBW;
    }


    if( p_vpar->sequence.b_mpeg2 )
    {
        static f_picture_data_t ppf_picture_data[4][4] =
        {
            {
                NULL, NULL, NULL, NULL
            },
            {
                /* TOP_FIELD */
#if (VPAR_OPTIM_LEVEL > 1)
                NULL, vpar_PictureData2IT, vpar_PictureData2PT,
                vpar_PictureData2BT
#else
                NULL, vpar_PictureDataGENERIC, vpar_PictureDataGENERIC,
                vpar_PictureDataGENERIC
#endif
            },
            {
                /* BOTTOM_FIELD */
#if (VPAR_OPTIM_LEVEL > 1)
                NULL, vpar_PictureData2IB, vpar_PictureData2PB,
                vpar_PictureData2BB
#else
                NULL, vpar_PictureDataGENERIC, vpar_PictureDataGENERIC,
                vpar_PictureDataGENERIC
#endif
            },
            {
                /* FRAME_PICTURE */
#if (VPAR_OPTIM_LEVEL > 0)
                NULL, vpar_PictureData2IF, vpar_PictureData2PF,
                vpar_PictureData2BF
#else
                NULL, vpar_PictureDataGENERIC, vpar_PictureDataGENERIC,
                vpar_PictureDataGENERIC
#endif
            }
        };

        ppf_picture_data[p_vpar->picture.i_structure]
                        [p_vpar->picture.i_coding_type]( p_vpar );
    }
    else
    {
#if (VPAR_OPTIM_LEVEL > 1)
        static f_picture_data_t pf_picture_data[5] =
        { NULL, vpar_PictureData1I, vpar_PictureData1P, vpar_PictureData1B,
          vpar_PictureData1D };

        pf_picture_data[p_vpar->picture.i_coding_type]( p_vpar );
#else
        vpar_PictureDataGENERIC( p_vpar );
#endif
    }

    /* Wait for all the macroblocks to be decoded. */
    p_vpar->pool.pf_wait_pool( &p_vpar->pool );

    /* Re-spawn decoder threads if the user changed settings. */
    vpar_SpawnPool( p_vpar );

    if( p_vpar->p_fifo->b_die || p_vpar->p_fifo->b_error )
    {
        return;
    }

    if( p_vpar->sequence.p_forward != NULL &&
        p_vpar->sequence.p_forward->i_planes == 0 )
    {
        /* This can only happen with the fake picture created for section
         * 7.6.3.5. Clean up our mess. */
        p_vpar->sequence.p_forward = NULL;
    }

    if( p_vpar->picture.b_error )
    {
        /* Trash picture. */
        p_vpar->pc_malformed_pictures[p_vpar->picture.i_coding_type]++;

        vpar_SynchroEnd( p_vpar, p_vpar->picture.i_coding_type,
                         p_vpar->picture.i_structure, 1 );
        vout_DestroyPicture( p_vpar->p_vout, P_picture );

        ReferenceReplace( p_vpar, p_vpar->picture.i_coding_type, NULL );

        /* Prepare context for the next picture. */
        P_picture = NULL;
        if( p_vpar->picture.i_current_structure == FRAME_STRUCTURE )
            p_vpar->picture.i_current_structure = 0;
    }
    else if( p_vpar->picture.i_current_structure == FRAME_STRUCTURE )
    {
        /* Frame completely parsed. */
        vpar_SynchroEnd( p_vpar, p_vpar->picture.i_coding_type,
                         p_vpar->picture.i_structure, 0 );
        vout_DisplayPicture( p_vpar->p_vout, P_picture );

        /* Prepare context for the next picture. */
        P_picture = NULL;
        p_vpar->picture.i_current_structure = 0;
    }
#undef P_picture
}

/*****************************************************************************
 * ExtensionAndUserData : Parse the extension_and_user_data structure
 *****************************************************************************/
static void ExtensionAndUserData( vpar_thread_t * p_vpar )
{
    while( !p_vpar->p_fifo->b_die )
    {
        NextStartCode( &p_vpar->bit_stream );
        switch( ShowBits( &p_vpar->bit_stream, 32 ) )
        {
        case EXTENSION_START_CODE:
            RemoveBits32( &p_vpar->bit_stream );
            switch( GetBits( &p_vpar->bit_stream, 4 ) )
            {
            case SEQUENCE_DISPLAY_EXTENSION_ID:
                SequenceDisplayExtension( p_vpar );
                break;
            case QUANT_MATRIX_EXTENSION_ID:
                QuantMatrixExtension( p_vpar );
                break;
            case SEQUENCE_SCALABLE_EXTENSION_ID:
                SequenceScalableExtension( p_vpar );
                break;
            case PICTURE_DISPLAY_EXTENSION_ID:
                PictureDisplayExtension( p_vpar );
                break;
            case PICTURE_SPATIAL_SCALABLE_EXTENSION_ID:
                PictureSpatialScalableExtension( p_vpar );
                break;
            case PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID:
                PictureTemporalScalableExtension( p_vpar );
                break;
            case COPYRIGHT_EXTENSION_ID:
                CopyrightExtension( p_vpar );
                break;
            default:
                break;
            }
            break;

        case USER_DATA_START_CODE:
            RemoveBits32( &p_vpar->bit_stream );
            /* Wait for the next start code */
            break;

        default:
            return;
        }
    }
}


/*****************************************************************************
 * SequenceDisplayExtension : Parse the sequence_display_extension structure *
 *****************************************************************************/

static void SequenceDisplayExtension( vpar_thread_t * p_vpar )
{
    /* We don't care sequence_display_extension. */
    /* video_format */
    RemoveBits( &p_vpar->bit_stream, 3 );
    if( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        /* Two bytes for color_desciption */
        RemoveBits( &p_vpar->bit_stream, 16 );
        p_vpar->sequence.i_matrix_coefficients = GetBits( &p_vpar->bit_stream, 8 );
    }
    /* display_horizontal and vertical_size and a marker_bit */
    RemoveBits( &p_vpar->bit_stream, 29 );
}


/*****************************************************************************
 * QuantMatrixExtension : Load quantization matrices for luminance           *
 *                        and chrominance                                    *
 *****************************************************************************/

static void QuantMatrixExtension( vpar_thread_t * p_vpar )
{
    if( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        /* Load intra_quantiser_matrix for luminance. */
        LoadMatrix( p_vpar, &p_vpar->sequence.intra_quant );
    }
    else
    {
        /* Use the default matrix. */
        LinkMatrix( &p_vpar->sequence.intra_quant,
                    p_vpar->pi_default_intra_quant );
    }
    if( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        /* Load non_intra_quantiser_matrix for luminance. */
        LoadMatrix( p_vpar, &p_vpar->sequence.nonintra_quant );
    }
    else
    {
        /* Use the default matrix. */
        LinkMatrix( &p_vpar->sequence.nonintra_quant,
                    p_vpar->pi_default_nonintra_quant );
    }
    if( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        /* Load intra_quantiser_matrix for chrominance. */
        LoadMatrix( p_vpar, &p_vpar->sequence.chroma_intra_quant );
    }
    else
    {
        /* Link the chrominance intra matrix to the luminance one. */
        LinkMatrix( &p_vpar->sequence.chroma_intra_quant,
                    p_vpar->sequence.intra_quant.pi_matrix );
    }
    if( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        /* Load non_intra_quantiser_matrix for chrominance. */
        LoadMatrix( p_vpar, &p_vpar->sequence.chroma_nonintra_quant );
    }
    else
    {
        /* Link the chrominance nonintra matrix to the luminance one. */
        LinkMatrix( &p_vpar->sequence.chroma_nonintra_quant,
                    p_vpar->sequence.nonintra_quant.pi_matrix );
    }
}


/*****************************************************************************
 * SequenceScalableExtension : Parse the sequence_scalable_extension         *
 *                             structure to handle scalable coding           *
 *****************************************************************************/

static void SequenceScalableExtension( vpar_thread_t * p_vpar )
{
    /* We don't care about anything scalable except the scalable mode. */
    switch( p_vpar->sequence.i_scalable_mode = GetBits( &p_vpar->bit_stream, 2 ) )
    /* The length of the structure depends on the value of the scalable_mode */
    {
        case 1:
            RemoveBits32( &p_vpar->bit_stream );
            RemoveBits( &p_vpar->bit_stream, 21 );
            break;
        case 2:
            RemoveBits( &p_vpar->bit_stream, 12 );
            break;
        default:
            RemoveBits( &p_vpar->bit_stream, 4 );
    }

}
/*****************************************************************************
 * PictureDisplayExtension : Parse the picture_display_extension structure   *
 *****************************************************************************/

static void PictureDisplayExtension( vpar_thread_t * p_vpar )
{
    /* Number of frame center offset */
    int i_nb, i_dummy;
    /* I am not sure it works but it should
        (fewer tests than shown in §6.3.12) */
    i_nb = p_vpar->sequence.b_progressive ? p_vpar->sequence.b_progressive +
                                            p_vpar->picture.b_repeat_first_field +
                                            p_vpar->picture.b_top_field_first
                           : ( p_vpar->picture.b_frame_structure + 1 ) +
                             p_vpar->picture.b_repeat_first_field;
    for( i_dummy = 0; i_dummy < i_nb; i_dummy++ )
    {
        RemoveBits( &p_vpar->bit_stream, 17 );
        RemoveBits( &p_vpar->bit_stream, 17 );
    }
}


/*****************************************************************************
 * PictureSpatialScalableExtension                                           *
 *****************************************************************************/

static void PictureSpatialScalableExtension( vpar_thread_t * p_vpar )
{
    /* That's scalable, so we trash it */
    RemoveBits32( &p_vpar->bit_stream );
    RemoveBits( &p_vpar->bit_stream, 16 );
}


/*****************************************************************************
 * PictureTemporalScalableExtension                                          *
 *****************************************************************************/

static void PictureTemporalScalableExtension( vpar_thread_t * p_vpar )
{
    /* Scalable again, trashed again */
    RemoveBits( &p_vpar->bit_stream, 23 );
}


/*****************************************************************************
 * CopyrightExtension : Keeps some legal informations                        *
 *****************************************************************************/

static void CopyrightExtension( vpar_thread_t * p_vpar )
{
    u32     i_copyright_nb_1, i_copyright_nb_2; /* local integers */
    p_vpar->sequence.b_copyright_flag = GetBits( &p_vpar->bit_stream, 1 );
        /* A flag that says whether the copyright information is significant */
    p_vpar->sequence.i_copyright_id = GetBits( &p_vpar->bit_stream, 8 );
        /* An identifier compliant with ISO/CEI JTC 1/SC 29 */
    p_vpar->sequence.b_original = GetBits( &p_vpar->bit_stream, 1 );
        /* Reserved bits */
    RemoveBits( &p_vpar->bit_stream, 8 );
        /* The copyright_number is split in three parts */
        /* first part */
    i_copyright_nb_1 = GetBits( &p_vpar->bit_stream, 20 );
    RemoveBits( &p_vpar->bit_stream, 1 );
        /* second part */
    i_copyright_nb_2 = GetBits( &p_vpar->bit_stream, 22 );
    RemoveBits( &p_vpar->bit_stream, 1 );
        /* third part and sum */
    p_vpar->sequence.i_copyright_nb = ( (u64)i_copyright_nb_1 << 44 ) |
                                      ( (u64)i_copyright_nb_2 << 22 ) |
                                      ( (u64)GetBits( &p_vpar->bit_stream, 22 ) );
}
