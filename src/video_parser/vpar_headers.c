/*****************************************************************************
 * vpar_headers.c : headers parsing
 * (c)1999 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/uio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "intf_msg.h"
#include "debug.h"                    /* ?? temporaire, requis par netlist.h */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"
#include "video.h"
#include "video_output.h"

#include "vdec_idct.h"
#include "video_decoder.h"
#include "vdec_motion.h"

#include "vpar_blocks.h"
#include "vpar_headers.h"
#include "video_fifo.h"
#include "video_parser.h"

/*
 * Local prototypes
 */
static __inline__ void NextStartCode( vpar_thread_t * p_vpar );
static void SequenceHeader( vpar_thread_t * p_vpar );
static void GroupHeader( vpar_thread_t * p_vpar );
static void PictureHeader( vpar_thread_t * p_vpar );
static void __inline__ ReferenceUpdate( vpar_thread_t * p_vpar,
                                        int i_coding_type,
                                        picture_t * p_newref );
static void __inline__ ReferenceReplace( vpar_thread_t * p_vpar,
                                         int i_coding_type,
                                         picture_t * p_newref );
static void SliceHeader00( vpar_thread_t * p_vpar,
                           int * pi_mb_address, int i_mb_base,
                           u32 i_vert_code );
static void SliceHeader01( vpar_thread_t * p_vpar,
                           int * pi_mb_address, int i_mb_base,
                           u32 i_vert_code );
static void SliceHeader10( vpar_thread_t * p_vpar,
                           int * pi_mb_address, int i_mb_base,
                           u32 i_vert_code );
static void SliceHeader11( vpar_thread_t * p_vpar,
                           int * pi_mb_address, int i_mb_base,
                           u32 i_vert_code );
static __inline__ void SliceHeader( vpar_thread_t * p_vpar,
                                    int * pi_mb_address, int i_mb_base,
                                    u32 i_vert_code );
static void ExtensionAndUserData( vpar_thread_t * p_vpar );
static void QuantMatrixExtension( vpar_thread_t * p_vpar );
static void SequenceScalableExtension( vpar_thread_t * p_vpar );
static void SequenceDisplayExtension( vpar_thread_t * p_vpar );
static void PictureDisplayExtension( vpar_thread_t * p_vpar );
static void PictureSpatialScalableExtension( vpar_thread_t * p_vpar );
static void PictureTemporalScalableExtension( vpar_thread_t * p_vpar );
static void CopyrightExtension( vpar_thread_t * p_vpar );
static __inline__ void LoadMatrix( vpar_thread_t * p_vpar, quant_matrix_t * p_matrix );
static __inline__ void LinkMatrix( quant_matrix_t * p_matrix, int * pi_array );

/*****************************************************************************
 * vpar_NextSequenceHeader : Find the next sequence header
 *****************************************************************************/
int vpar_NextSequenceHeader( vpar_thread_t * p_vpar )
{
    while( !p_vpar->b_die )
    {
        NextStartCode( p_vpar );
        if( ShowBits( &p_vpar->bit_stream, 32 ) == SEQUENCE_HEADER_CODE )
            return 0;
    }
    return 1;
}

/*****************************************************************************
 * vpar_ParseHeader : Parse the next header
 *****************************************************************************/
int vpar_ParseHeader( vpar_thread_t * p_vpar )
{
    while( !p_vpar->b_die )
    {
        NextStartCode( p_vpar );
        switch( GetBits32( &p_vpar->bit_stream ) )
        {
        case SEQUENCE_HEADER_CODE:
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
            return 1;
            break;

        default:
        }
    }

    return 0;
}

/*
 * Following functions are local
 */

/*****************************************************************************
 * NextStartCode : Find the next start code
 *****************************************************************************/
static __inline__ void NextStartCode( vpar_thread_t * p_vpar )
{
    /* Re-align the buffer on an 8-bit boundary */
    RealignBits( &p_vpar->bit_stream );

    while( ShowBits( &p_vpar->bit_stream, 24 ) != 0x01L && !p_vpar->b_die )
    {
        DumpBits( &p_vpar->bit_stream, 8 );
    }
}

/*****************************************************************************
 * SequenceHeader : Parse the next sequence header
 *****************************************************************************/
static void SequenceHeader( vpar_thread_t * p_vpar )
{
#define RESERVED    -1 
    static double d_frame_rate_table[16] =
    {
        0.0,
        ((23.0*1000.0)/1001.0),
        24.0,
        25.0,
        ((30.0*1000.0)/1001.0),
        30.0,
        50.0,
        ((60.0*1000.0)/1001.0),
        60.0,
        RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED, RESERVED
    };
#undef RESERVED

    int i_height_save, i_width_save;
    
    i_height_save = p_vpar->sequence.i_height;
    i_width_save = p_vpar->sequence.i_width;

    p_vpar->sequence.i_height = GetBits( &p_vpar->bit_stream, 12 );
    p_vpar->sequence.i_width = GetBits( &p_vpar->bit_stream, 12 );
    p_vpar->sequence.i_aspect_ratio = GetBits( &p_vpar->bit_stream, 4 );
    p_vpar->sequence.d_frame_rate =
            d_frame_rate_table[ GetBits( &p_vpar->bit_stream, 4 ) ];

    /* We don't need bit_rate_value, marker_bit, vbv_buffer_size,
     * constrained_parameters_flag */
    DumpBits( &p_vpar->bit_stream, 30 );
    
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
        LinkMatrix( &p_vpar->sequence.intra_quant, pi_default_intra_quant );
    }
    
    if( GetBits( &p_vpar->bit_stream, 1 ) ) /* load_non_intra_quantizer_matrix */
    {
        LoadMatrix( p_vpar, &p_vpar->sequence.nonintra_quant );
    }
    else
    {
        /* Use default matrix. */
        LinkMatrix( &p_vpar->sequence.nonintra_quant, pi_default_nonintra_quant );
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
    NextStartCode( p_vpar );
    if( ShowBits( &p_vpar->bit_stream, 32 ) == EXTENSION_START_CODE )
    {
        int                         i_dummy;
        static int                  pi_chroma_nb_blocks[4] = {0, 1, 2, 4};
        static f_chroma_pattern_t   ppf_chroma_pattern[4] =
                            {NULL, vpar_CodedPattern420,
                             vpar_CodedPattern422, vpar_CodedPattern444};
    
        /* Parse sequence_extension */
        DumpBits32( &p_vpar->bit_stream );
        /* extension_start_code_identifier, profile_and_level_indication */
        DumpBits( &p_vpar->bit_stream, 12 );
        p_vpar->sequence.b_progressive = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->sequence.i_chroma_format = GetBits( &p_vpar->bit_stream, 2 );
        p_vpar->sequence.i_chroma_width = p_vpar->sequence.i_width
                    >> (3-p_vpar->sequence.i_chroma_format);
        p_vpar->sequence.i_chroma_nb_blocks = pi_chroma_nb_blocks
                                    [p_vpar->sequence.i_chroma_format];
        p_vpar->sequence.pf_decode_pattern = ppf_chroma_pattern
                                    [p_vpar->sequence.i_chroma_format];
        p_vpar->sequence.i_width |= GetBits( &p_vpar->bit_stream, 2 ) << 12;
        p_vpar->sequence.i_height |= GetBits( &p_vpar->bit_stream, 2 ) << 12;
        /* bit_rate_extension, marker_bit, vbv_buffer_size_extension, low_delay */
        DumpBits( &p_vpar->bit_stream, 22 );
        /* frame_rate_extension_n */
        i_dummy = GetBits( &p_vpar->bit_stream, 2 );
        /* frame_rate_extension_d */
        p_vpar->sequence.d_frame_rate *= (i_dummy + 1)
                                  / (GetBits( &p_vpar->bit_stream, 5 ) + 1);

        p_vpar->sequence.pf_decode_mv = vpar_MPEG2MotionVector;
    }
    else
    {
        /* It's an MPEG-1 stream. Put adequate parameters. */
        p_vpar->sequence.b_progressive = 1;
        p_vpar->sequence.i_chroma_format = CHROMA_420;
        p_vpar->sequence.i_chroma_width = p_vpar->sequence.i_width >> 2;
        p_vpar->sequence.i_chroma_nb_blocks = 2;
        p_vpar->sequence.pf_decode_pattern = vpar_CodedPattern420;

        p_vpar->sequence.pf_decode_mv = vpar_MPEG1MotionVector;
    }

    p_vpar->sequence.i_mb_width = (p_vpar->sequence.i_width + 15) / 16;
    p_vpar->sequence.i_mb_height = (p_vpar->sequence.b_progressive) ?
                                   (p_vpar->sequence.i_height + 15) / 16 :
                                   2 * (p_vpar->sequence.i_height + 31) / 32;
    p_vpar->sequence.i_mb_size = p_vpar->sequence.i_mb_width
                                        * p_vpar->sequence.i_mb_height;
    p_vpar->sequence.i_width = (p_vpar->sequence.i_mb_width * 16);
    p_vpar->sequence.i_height = (p_vpar->sequence.i_mb_height * 16);
    p_vpar->sequence.i_size = p_vpar->sequence.i_width
                                        * p_vpar->sequence.i_height;

    /* Slice Header functions */
    if( p_vpar->sequence.i_height <= 2800 )
    {
        if( p_vpar->sequence.i_scalable_mode != SC_DP )
        {
            p_vpar->sequence.pf_slice_header = SliceHeader00;
        }
        else
        {
            p_vpar->sequence.pf_slice_header = SliceHeader01;
        }
    }
    else
    {
        if( p_vpar->sequence.i_scalable_mode != SC_DP )
        {
            p_vpar->sequence.pf_slice_header = SliceHeader10;
        }
        else
        {
            p_vpar->sequence.pf_slice_header = SliceHeader11;
        }
    }

    if(    p_vpar->sequence.i_width != i_width_save
        || p_vpar->sequence.i_height != i_height_save )
    {
         /* What do we do in case of a size change ??? */
    }

    /* Extension and User data */
    ExtensionAndUserData( p_vpar );
}

/*****************************************************************************
 * GroupHeader : Parse the next group of pictures header
 *****************************************************************************/
static void GroupHeader( vpar_thread_t * p_vpar )
{
    /* Nothing to do, we don't care. */
    DumpBits( &p_vpar->bit_stream, 27 );
    ExtensionAndUserData( p_vpar );
}

/*****************************************************************************
 * PictureHeader : Parse the next picture header
 *****************************************************************************/
static void PictureHeader( vpar_thread_t * p_vpar )
{
    static f_macroblock_type_t ppf_macroblock_type[4] =
                                                 {vpar_IMBType, vpar_PMBType,
                                                  vpar_BMBType, vpar_DMBType};

    int                 i_structure;
    int                 i_mb_address, i_mb_base, i_mb;
    elem_t *            p_y, p_u, p_v;
    boolean_t           b_parsable;
    u32                 i_dummy;
    
    DumpBits( &p_vpar->bit_stream, 10 ); /* temporal_reference */
    p_vpar->picture.i_coding_type = GetBits( &p_vpar->bit_stream, 3 );
    p_vpar->picture.pf_macroblock_type = ppf_macroblock_type
                                         [p_vpar->picture.i_coding_type];
    
    DumpBits( &p_vpar->bit_stream, 16 ); /* vbv_delay */
    
    p_vpar->picture.b_full_pel_forward_vector = GetBits( &p_vpar->bit_stream, 1 );
    p_vpar->picture.i_forward_f_code = GetBits( &p_vpar->bit_stream, 3 );
    p_vpar->picture.b_full_pel_backward_vector = GetBits( &p_vpar->bit_stream, 1 );
    p_vpar->picture.i_backward_f_code = GetBits( &p_vpar->bit_stream, 3 );

    /* extra_information_picture */
    while( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        DumpBits( &p_vpar->bit_stream, 8 );
    }

    /* 
     * Picture Coding Extension
     */
    NextStartCode( p_vpar );
    if( ShowBits( &p_vpar->bit_stream, 32 ) == EXTENSION_START_CODE )
    {
        /* Parse picture_coding_extension */
        DumpBits32( &p_vpar->bit_stream );
        /* extension_start_code_identifier */
        DumpBits( &p_vpar->bit_stream, 4 );
        
        p_vpar->picture.ppi_f_code[0][0] = GetBits( &p_vpar->bit_stream, 4 );
        p_vpar->picture.ppi_f_code[0][1] = GetBits( &p_vpar->bit_stream, 4 );
        p_vpar->picture.ppi_f_code[1][0] = GetBits( &p_vpar->bit_stream, 4 );
        p_vpar->picture.ppi_f_code[1][1] = GetBits( &p_vpar->bit_stream, 4 );
        p_vpar->picture.i_intra_dc_precision = GetBits( &p_vpar->bit_stream, 2 );
        i_structure = GetBits( &p_vpar->bit_stream, 2 );
        p_vpar->picture.b_top_field_first = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_frame_pred_frame_dct
             = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_concealment_mv = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_q_scale_type = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_intra_vlc_format = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_alternate_scan = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_repeat_first_field = GetBits( &p_vpar->bit_stream, 1 );
        /* repeat_first_field (ISO/IEC 13818-2 6.3.10 is necessary to know
         * the length of the picture_display_extension structure.
         * chroma_420_type (obsolete) */
        DumpBits( &p_vpar->bit_stream, 1 );
        p_vpar->picture.b_progressive_frame = GetBits( &p_vpar->bit_stream, 1 );
        
        /* composite_display_flag */
        if( GetBits( &p_vpar->bit_stream, 1 ) )
        {
            /* v_axis, field_sequence, sub_carrier, burst_amplitude,
             * sub_carrier_phase */
            DumpBits( &p_vpar->bit_stream, 20 );
        }
    }
    else
    {
        /* MPEG-1 compatibility flags */
        p_vpar->picture.i_intra_dc_precision = 0; /* 8 bits */
        i_structure = FRAME_STRUCTURE;
        p_vpar->picture.b_frame_pred_frame_dct = 1;
        p_vpar->picture.b_concealment_mv = 0;
        p_vpar->picture.b_q_scale_type = 0;
        p_vpar->picture.b_intra_vlc_format = 0;
        p_vpar->picture.b_alternate_scan = 0; /* zigzag */
        p_vpar->picture.b_repeat_first_field = 0;
        p_vpar->picture.b_progressive_frame = 1;
    }

    if( p_vpar->picture.i_current_structure &&
        (i_structure == FRAME_STRUCTURE ||
         i_structure == p_vpar->picture.i_current_structure) )
    {
        /* We don't have the second field of the buffered frame. */
        if( p_vpar->picture.p_picture != NULL )
        {
            ReferenceReplace( p_vpar,
                      p_vpar->picture.i_coding_type,
                      NULL );

            for( i_mb = 0; i_mb < p_vpar->sequence.i_mb_size >> 1; i_mb++ )
            {
                vpar_DestroyMacroblock( &p_vpar->vfifo,
                                        p_vpar->picture.pp_mb[i_mb] );
            }
            vout_DestroyPicture( p_vpar->p_vout, p_vpar->picture.p_picture );
        }
        
        p_vpar->picture.i_current_structure = 0;

        intf_DbgMsg("vpar debug: odd number of field picture.");
    }

    if( p_vpar->picture.i_current_structure )
    {
        /* Second field of a frame. We will decode it if, and only if we
         * have decoded the first frame. */
        b_parsable = (p_vpar->picture.p_picture != NULL);
    }
    else
    {
        /* Do we have the reference pictures ? */
        b_parsable = !((p_vpar->picture.i_coding_type == P_CODING_TYPE) &&
                       (p_vpar->sequence.p_forward == NULL)) ||
                      ((p_vpar->picture.i_coding_type == B_CODING_TYPE) &&
                       (p_vpar->sequence.p_forward == NULL ||
                        p_vpar->sequence.p_backward == NULL));

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
        
        /* Warn Synchro we have trashed a picture. */
        vpar_SynchroTrash( p_vpar, p_vpar->picture.i_coding_type, i_structure );

        /* Update context. */
        if( i_structure != FRAME_STRUCTURE )
            p_vpar->picture.i_current_structure = i_structure;
        p_vpar->picture.p_picture = NULL;

        return;
    }

    /* OK, now we are sure we will decode the picture. */
#define P_picture p_vpar->picture.p_picture
    p_vpar->picture.b_error = 0;

    if( !p_vpar->picture.i_current_structure )
    {
        /* This is a new frame. Get a structure from the video_output. */
        P_picture = vout_CreatePicture( p_vpar->p_vout,
                                        SPLITTED_YUV_PICTURE,
                                        p_vpar->sequence.i_width,
                                        p_vpar->sequence.i_height,
                                        p_vpar->sequence.i_chroma_format );

        /* Initialize values. */
        P_picture->date = vpar_SynchroDecode( p_vpar,
                                              p_vpar->picture.i_coding_type,
                                              i_structure );
        p_vpar->picture.i_lum_incr = - 8 + ( p_vpar->sequence.i_width
                    << ( i_structure != FRAME_STRUCTURE ) );
        p_vpar->picture.i_chroma_incr = -8 + ( p_vpar->sequence.i_width
                    << (( i_structure != FRAME_STRUCTURE ) +
                        ( 3 - p_vpar->sequence.i_chroma_format )) );

        /* Update the reference pointers. */
        ReferenceUpdate( p_vpar, p_vpar->picture.i_coding_type, P_picture );
    }
    p_vpar->picture.i_current_structure |= i_structure;
    p_vpar->picture.i_structure = i_structure;
    p_vpar->picture.b_frame_structure = (i_structure == FRAME_STRUCTURE);

    /* Initialize picture data for decoding. */
    if( i_structure == BOTTOM_FIELD )
    {
        i_mb_base = p_vpar->sequence.i_mb_size >> 1;
    }
    else
    {
        i_mb_base = 0;
    }
    i_mb_address = 0;

    /* Extension and User data. */
    ExtensionAndUserData( p_vpar );

    /* Picture data (ISO/IEC 13818-2 6.2.3.7). */
    NextStartCode( p_vpar );
    while( i_mb_address+i_mb_base < p_vpar->sequence.i_mb_size
           && !p_vpar->picture.b_error)
    {
        if( ((i_dummy = ShowBits( &p_vpar->bit_stream, 32 ))
                 < SLICE_START_CODE_MIN) ||
            (i_dummy > SLICE_START_CODE_MAX) )
        {
            intf_DbgMsg("vpar debug: premature end of picture");
            p_vpar->picture.b_error = 1;
            break;
        }
        DumpBits32( &p_vpar->bit_stream );
        
        /* Decode slice data. */
        SliceHeader( p_vpar, &i_mb_address, i_mb_base, i_dummy & 255 );
    }

    if( p_vpar->picture.b_error )
    {
        /* Trash picture. */
        for( i_mb = 0; p_vpar->picture.pp_mb[i_mb]; i_mb++ )
        {
            vpar_DestroyMacroblock( &p_vpar->vfifo, p_vpar->picture.pp_mb[i_mb] );
        }

        ReferenceReplace( p_vpar, p_vpar->picture.i_coding_type, NULL );
        vout_DestroyPicture( p_vpar->p_vout, P_picture );

        /* Prepare context for the next picture. */
        P_picture = NULL;
    }
    else if( p_vpar->picture.i_current_structure == FRAME_STRUCTURE )
    {
        /* Frame completely parsed. */
        for( i_mb = 0; i_mb < p_vpar->sequence.i_mb_size; i_mb++ )
        {
            vpar_DecodeMacroblock( &p_vpar->vfifo, p_vpar->picture.pp_mb[i_mb] );
        }

        /* Prepare context for the next picture. */
        P_picture = NULL;
    }
#undef P_picture
}

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
            vout_UnlinkPicture( p_vpar->p_vout, p_vpar->sequence.p_forward );
        p_vpar->sequence.p_forward = p_vpar->sequence.p_backward;
        p_vpar->sequence.p_backward = p_newref;
        if( p_newref != NULL )
            vout_LinkPicture( p_vpar->p_vout, p_newref );
    }
}

/*****************************************************************************
 * ReferenceReplace : Replace the last reference pointer when we destroy
 * a picture
 *****************************************************************************/
static void __inline__ ReferenceReplace( vpar_thread_t * p_vpar,
                                         int i_coding_type,
                                         picture_t * p_newref )
{
    if( i_coding_type != B_CODING_TYPE )
    {
        if( p_vpar->sequence.p_backward != NULL )
            vout_UnlinkPicture( p_vpar->p_vout, p_vpar->sequence.p_backward );
        p_vpar->sequence.p_backward = p_newref;
        if( p_newref != NULL )
            vout_LinkPicture( p_vpar->p_vout, p_newref );
    }
}

/*****************************************************************************
 * SliceHeaderXY : Parse the next slice structure
 *****************************************************************************
 * X = i_height > 2800 ?
 * Y = scalable_mode == SC_DP ?
 *****************************************************************************/
static void SliceHeader00( vpar_thread_t * p_vpar,
                           int * pi_mb_address, int i_mb_base,
                           u32 i_vert_code )
{
    SliceHeader( p_vpar, pi_mb_address, i_mb_base, i_vert_code );
}

static void SliceHeader01( vpar_thread_t * p_vpar,
                           int * pi_mb_address, int i_mb_base,
                           u32 i_vert_code )
{
    DumpBits( &p_vpar->bit_stream, 7 ); /* priority_breakpoint */
    SliceHeader( p_vpar, pi_mb_address, i_mb_base, i_vert_code );
}

static void SliceHeader10( vpar_thread_t * p_vpar,
                           int * pi_mb_address, int i_mb_base,
                           u32 i_vert_code )
{
    i_vert_code += GetBits( &p_vpar->bit_stream, 3 ) << 7;
    SliceHeader( p_vpar, pi_mb_address, i_mb_base, i_vert_code );
}

static void SliceHeader11( vpar_thread_t * p_vpar,
                           int * pi_mb_address, int i_mb_base,
                           u32 i_vert_code )
{
    i_vert_code += GetBits( &p_vpar->bit_stream, 3 ) << 7;
    DumpBits( &p_vpar->bit_stream, 7 ); /* priority_breakpoint */
    SliceHeader( p_vpar, pi_mb_address, i_mb_base, i_vert_code );
}

/*****************************************************************************
 * SliceHeader : Parse the next slice structure
 *****************************************************************************/
static __inline__ void SliceHeader( vpar_thread_t * p_vpar,
                                    int * pi_mb_address, int i_mb_base,
                                    u32 i_vert_code )
{
    /* DC predictors initialization table */
    static int              pi_dc_dct_reinit[4] = {128,256,512,1024};

    int                     i_mb_address_save = *pi_mb_address;

    /* slice_vertical_position_extension and priority_breakpoint already done */
    LoadQuantizerScale( p_vpar );

    if( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        /* intra_slice, slice_id */
        DumpBits( &p_vpar->bit_stream, 8 );
        /* extra_information_slice */
        while( GetBits( &p_vpar->bit_stream, 1 ) )
        {
            DumpBits( &p_vpar->bit_stream, 8 );
        }
    }

    *pi_mb_address = (i_vert_code - 1)*p_vpar->sequence.i_mb_width;

    /* Reset DC coefficients predictors (ISO/IEC 13818-2 7.2.1). Why
     * does the reference decoder put 0 instead of the normative values ? */
    p_vpar->slice.pi_dc_dct_pred[0] = p_vpar->slice.pi_dc_dct_pred[1]
        = p_vpar->slice.pi_dc_dct_pred[2]
        = pi_dc_dct_reinit[p_vpar->picture.i_intra_dc_precision];

    /* Reset motion vector predictors (ISO/IEC 13818-2 7.6.3.4). */
    bzero( p_vpar->slice.pppi_pmv, 8*sizeof(int) );

    do
    {
        vpar_ParseMacroblock( p_vpar, pi_mb_address, i_mb_address_save,
                              i_mb_base );
        i_mb_address_save = *pi_mb_address;
    }
    while( !ShowBits( &p_vpar->bit_stream, 23 ) );
}

/*****************************************************************************
 * ExtensionAndUserData : Parse the extension_and_user_data structure
 *****************************************************************************/
static void ExtensionAndUserData( vpar_thread_t * p_vpar )
{
    while( !p_vpar->b_die )
    {
        NextStartCode( p_vpar );
        switch( ShowBits( &p_vpar->bit_stream, 32 ) )
        {
        case EXTENSION_START_CODE:
            DumpBits32( &p_vpar->bit_stream );
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
            }
            break;

        case USER_DATA_START_CODE:
            DumpBits32( &p_vpar->bit_stream );
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
    DumpBits( &p_vpar->bit_stream, 3 );
    if( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        /* Three bytes for color_desciption */
        DumpBits( &p_vpar->bit_stream, 24 );
    }
    /* display_horizontal and vertical_size and a marker_bit */
    DumpBits( &p_vpar->bit_stream, 29 );
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
                    pi_default_intra_quant );
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
                    pi_default_nonintra_quant );
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
            DumpBits32( &p_vpar->bit_stream );
            DumpBits( &p_vpar->bit_stream, 21 );
            break;
        case 2:
            DumpBits( &p_vpar->bit_stream, 12 );
            break;
        default:
            DumpBits( &p_vpar->bit_stream, 4 );
    }

}
/*****************************************************************************
 * PictureDisplayExtension : Parse the picture_display_extension structure   *
 *****************************************************************************/

static void PictureDisplayExtension( vpar_thread_t * p_vpar )
{
    /* Number of frame center offset */
    int nb;
    /* I am not sure it works but it should
        (fewer tests than shown in §6.3.12) */
    nb = p_vpar->sequence.b_progressive ? p_vpar->sequence.b_progressive +
                                          p_vpar->picture.b_repeat_first_field +
                                          p_vpar->picture.b_top_field_first
                         : ( p_vpar->picture.b_frame_structure + 1 ) +
                           p_vpar->picture.b_repeat_first_field;
    DumpBits( &p_vpar->bit_stream, 34 * nb );
}


/*****************************************************************************
 * PictureSpatialScalableExtension                                           *
 *****************************************************************************/

static void PictureSpatialScalableExtension( vpar_thread_t * p_vpar )
{
    /* That's scalable, so we trash it */
    DumpBits32( &p_vpar->bit_stream );
    DumpBits( &p_vpar->bit_stream, 14 );
}


/*****************************************************************************
 * PictureTemporalScalableExtension                                          *
 *****************************************************************************/

static void PictureTemporalScalableExtension( vpar_thread_t * p_vpar )
{
    /* Scalable again, trashed again */
    DumpBits( &p_vpar->bit_stream, 23 );
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
    DumpBits( &p_vpar->bit_stream, 8 );
        /* The copyright_number is split in three parts */
        /* first part */
    i_copyright_nb_1 = GetBits( &p_vpar->bit_stream, 20 );
    DumpBits( &p_vpar->bit_stream, 1 );
        /* second part */
    i_copyright_nb_2 = GetBits( &p_vpar->bit_stream, 22 );
    DumpBits( &p_vpar->bit_stream, 1 );
        /* third part and sum */
    p_vpar->sequence.i_copyright_nb = ( (u64)i_copyright_nb_1 << 44 ) +
                                      ( (u64)i_copyright_nb_2 << 22 ) +
                                      ( (u64)GetBits( &p_vpar->bit_stream, 22 ) );
}


/*****************************************************************************
 * LoadMatrix : Load a quantization matrix
 *****************************************************************************/
static __inline__ void LoadMatrix( vpar_thread_t * p_vpar, quant_matrix_t * p_matrix )
{
    int i_dummy;
    
    if( !p_matrix->b_allocated )
    {
        /* Allocate a piece of memory to load the matrix. */
        p_matrix->pi_matrix = (int *)malloc( 64*sizeof(int) );
        p_matrix->b_allocated = 1;
    }
    
    for( i_dummy = 0; i_dummy < 64; i_dummy++ )
    {
        p_matrix->pi_matrix[pi_scan[SCAN_ZIGZAG][i_dummy]]
             = GetBits( &p_vpar->bit_stream, 8 );
    }

#ifdef FOURIER_IDCT
    /* Discrete Fourier Transform requires the quantization matrices to
     * be normalized before using them. */
    vdec_NormQuantMatrix( p_matrix->pi_matrix );
#endif
}

/*****************************************************************************
 * LinkMatrix : Link a quantization matrix to another
 *****************************************************************************/
static __inline__ void LinkMatrix( quant_matrix_t * p_matrix, int * pi_array )
{
    int i_dummy;
    
    if( p_matrix->b_allocated )
    {
        /* Deallocate the piece of memory. */
        free( p_matrix->pi_matrix );
        p_matrix->b_allocated = 0;
    }
    
    p_matrix->pi_matrix = pi_array;
}
