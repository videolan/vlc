/*****************************************************************************
 * vpar_headers.c : headers parsing
 * (c)1999 VideoLAN
 *****************************************************************************/

/* ?? passer en terminate/destroy avec les signaux supplémentaires */

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
#include "video_parser.h"

#include "undec_picture.h"
#include "video_fifo.h"
#include "video_decoder.h"

/*
 * Local prototypes
 */

/*****************************************************************************
 * vpar_NextSequenceHeader : Find the next sequence header
 *****************************************************************************/
void vpar_NextSequenceHeader( vpar_thread_t * p_vpar )
{
    while( !p_vpar->b_die )
    {
        NextStartCode( p_vpar );
        if( ShowBits( &p_vpar->bit_stream, 32 ) == SEQUENCE_START_CODE )
            return;
    }
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
    /* Re-align the buffer to an 8-bit boundary */
    DumpBits( &p_vpar->bit_stream, p_vpar->bit_stream.fifo.i_available & 7 );

    while( ShowBits( &p_vpar->bit_stream, 24 ) != 0x01L && !p_vpar->b_die )
    {
        DumpBits( &p_vpar->bit_stream, 8 );
    }
}

/*****************************************************************************
 * SequenceHeader : Parse the next sequence header
 *****************************************************************************/
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
 
  RESERVED,
  RESERVED,
  RESERVED,
  RESERVED,
  RESERVED,
  RESERVED,
  RESERVED
};

static void SequenceHeader( vpar_thread_t * p_vpar )
{
    int i_height_save, i_width_save;
    
    i_height_save = p_vpar->sequence.i_height;
    i_width_save = p_vpar->sequence.i_width;

    p_vpar->sequence.i_height = ntohl( GetBits( p_vpar->bit_stream, 12 ) );
    p_vpar->sequence.i_width = ntohl( GetBits( p_vpar->bit_stream, 12 ) );
    p_vpar->sequence.i_ratio = GetBits( p_vpar->bit_stream, 4 );
    p_vpar->sequence.d_frame_rate =
            d_frame_rate_table( GetBits( p_vpar->bit_stream, 4 ) );

    /* We don't need bit_rate_value, marker_bit, vbv_buffer_size,
     * constrained_parameters_flag */
    DumpBits( p_vpar->bit_stream, 30 );
    
    /*
     * Quantization matrices
     */
    if( GetBits( p_vpar->bit_stream, 1 ) ) /* load_intra_quantizer_matrix */
    {
        LoadMatrix( p_vpar, &p_vpar->sequence.intra_quant );
    }
    else
    {
        /* Use default matrix. */
        LinkMatrix( &p_vpar->sequence.intra_quant, pi_default_intra_quant );
    }
    
    if( GetBits( p_vpar->bit_stream, 1 ) ) /* load_non_intra_quantizer_matrix */
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
    if( ShowBits( p_vpar->bit_stream, 32 ) == EXTENSION_START_CODE )
    {
        int             i_dummy;
    
        /* Parse sequence_extension */
        DumpBits32( &p_vpar->bit_stream );
        /* extension_start_code_identifier, profile_and_level_indication */
        DumpBits( &p_vpar->bit_stream, 12 );
        p_vpar->sequence.b_progressive = GetBits( &p_vpar->bit_stream, 1 );
        p_vpar->sequence.i_chroma_format = GetBits( &p_vpar->bit_stream, 2 );
        p_vpar->sequence.i_width |= GetBits( &p_vpar->bit_stream, 2 ) << 12;
        p_vpar->sequence.i_height |= GetBits( &p_vpar->bit_stream, 2 ) << 12;
        /* bit_rate_extension, marker_bit, vbv_buffer_size_extension, low_delay */
        DumpBits( &p_vpar->bit_stream, 22 );
        /* frame_rate_extension_n */
        i_dummy = GetBits( &p_vpar->bit_stream, 2 );
        /* frame_rate_extension_d */
        p_vpar->sequence.d_frame_rate *= (i_dummy + 1)
                                  / (GetBits( &p_vpar->bit_stream, 5 ) + 1);

        /* Extension and User data */
        ExtensionAndUserData( p_vpar );
    }
    else
    {
        /* It's an MPEG-1 stream. Put adequate parameters. */
        p_vpar->sequence.b_progressive = 1;
        p_vpar->i_chroma_format = CHROMA_420;
    }

    p_vpar->sequence.i_mb_width = (p_vpar->sequence.i_width + 15) / 16;
    p_vpar->sequence.i_mb_height = (p_vpar->sequence.b_progressive) ?
                                   (p_vpar->sequence.i_height + 15) / 16 :
                                   2 * (p_vpar->sequence.i_height + 31) / 32;
    p_vpar->sequence.i_width = (p_vpar->sequence.i_mb_width * 16);
    p_vpar->sequence.i_height = (p_vpar->sequence.i_mb_height * 16);

    if(    p_vpar->sequence.i_width != i_width_save
        || p_vpar->sequence.i_height != i_height_save
        || p_vpar->sequence.p_frame_lum_lookup == NULL )
    {
        int                 i_x, i_y;
        pel_lookup_table *  p_fr, p_fl;
        int                 i_fr, i_fl;
        static int          pi_chroma_size[4] = {0, 2, 1, 0}
#define Sequence p_vpar->sequence

        /* The size of the pictures has changed. Probably a new sequence.
         * We must recalculate the lookup matrices. */

        /* First unlink the previous lookup matrices so that they can
         * be freed in the future. */
        if( Sequence.p_frame_lum_lookup != NULL )
        {
            UNLINK_LOOKUP( Sequence.p_frame_lum_lookup );
            UNLINK_LOOKUP( Sequence.p_field_lum_lookup );
            UNLINK_LOOKUP( Sequence.p_frame_chroma_lookup );
            UNLINK_LOOKUP( Sequence.p_field_chroma_lookup );
        }

        /* Allocate the new lookup tables. */
        Sequence.p_frame_lum_lookup
            = (pel_lookup_table_t *)malloc( sizeof( pel_lookup_table_t ) *
                        Sequence.i_width * Sequence.i_height );
        Sequence.p_field_lum_lookup
            = (pel_lookup_table_t *)malloc( sizeof( pel_lookup_table_t ) *
                        Sequence.i_width * Sequence.i_height );
        Sequence.p_frame_chroma_lookup
            = (pel_lookup_table_t *)malloc( sizeof( pel_lookup_table_t ) *
                        Sequence.i_width * Sequence.i_height
                         >> pi_chroma_size[Sequence.i_chroma_format] );
        Sequence.p_field_chroma_lookup
            = (pel_lookup_table_t *)malloc( sizeof( pel_lookup_table_t ) *
                        Sequence.i_width * Sequence.i_height
                         >> pi_chroma_size[Sequence.i_chroma_format] );

        if( !Sequence.p_frame_lum_lookup || !Sequence.p_field_lum_lookup
            || !Sequence.p_frame_chroma_lookup
            || !Sequence.p_field_chroma_lookup )
        {
            intf_DbgMsg("vpar error: not enough memory for lookup tables");
            p_vpar->b_error = 1;
            return;
        }

        /* Fill in the luminance lookup tables */
        p_fr = &Sequence.p_frame_lum_lookup->pi_pel;
        p_fl = &Sequence.p_field_lum_lookup->pi_pel;
        i_fr = i_fl = 0;

        for( i_y = 0; i_y < Sequence.i_height; i_y++ )
        {
            int i_mb_y, i_b_y, i_pos_y;
            i_mb_y = i_y >> 4;
            i_b_y = (i_y & 15) >> 3;
            i_pos_y = (i_y & 7);

            for( i_x = 0; i_x < Sequence.i_width; i_x++ )
            {
                int i_mb_x, i_b_x, i_pos_x;
                i_mb_x = i_x >> 4;
                i_b_x = (i_x & 15) >> 3;
                i_pos_x = (i_x & 7);

                p_fl[i_fr + i_x] = p_fr[i_fr + i_x]
                                 = (i_mb_y*Sequence.i_mb_width + i_mb_y)*256
                                        + ((i_b_y << 1) + i_b_x)*64
                                        + i_pos_y*8 + i_pos_x;
            }
            i_fr += Sequence.i_width;
            i_fl += Sequence.i_width << 1;
            if( i_fl == Sequence.i_width*Sequence.i_height )
            {
                i_fl = Sequence.i_width;
            }
        }
        
        /* Fill in the chrominance lookup tables */
        p_fr = &Sequence.p_frame_chroma_lookup->pi_pel;
        p_fl = &Sequence.p_field_chroma_lookup->pi_pel;
        i_fr = i_fl = 0;

        switch( p_vpar->i_chroma_format )
        {
        case CHROMA_444:
            /* That's the same as luminance */
            memcopy( &Sequence.p_frame_croma_lookup->pi_pel,
                     &Sequence.p_frame_lum_lookup->pi_pel,
                     sizeof(PEL_P)*Sequence.i_height*Sequence.i_width );
            memcopy( &Sequence.p_field_croma_lookup->pi_pel,
                     &Sequence.p_field_lum_lookup->pi_pel,
                     sizeof(PEL_P)*Sequence.i_height*Sequence.i_width );

        case CHROMA_422:
            for( i_y = 0; i_y < Sequence.i_height; i_y++ )
            {
                int i_mb_y, i_b_y, i_pos_y;
                i_mb_y = i_y >> 4;
                i_b_y = (i_y & 15) >> 3;
                i_pos_y = (i_y & 7);
    
                for( i_x = 0; i_x < (Sequence.i_width >> 1); i_x++ )
                {
                    int i_mb_x, i_pos_x;
                    i_mb_x = i_x >> 3;
                    i_pos_x = (i_x & 7);
    
                    p_fl[i_fr + i_x] = p_fr[i_fr + i_x]
                                     = (i_mb_y*Sequence.i_mb_width + i_mb_y)*128
                                            + i_b_y*64
                                            + i_pos_y*8 + i_pos_x;
                }
                i_fr += Sequence.i_width >> 1;
                i_fl += Sequence.i_width;
                if( i_fl == (Sequence.i_width*Sequence.i_height >> 1) )
                {
                    i_fl = (Sequence.i_width >> 1);
                }
            }

        case CHROMA_420:
            for( i_y = 0; i_y < (Sequence.i_height >> 1); i_y++ )
            {
                int i_mb_y, i_pos_y;
                i_mb_y = i_y >> 3;
                i_pos_y = (i_y & 7);
    
                for( i_x = 0; i_x < (Sequence.i_width >> 1); i_x++ )
                {
                    int i_mb_x, i_pos_x;
                    i_mb_x = i_x >> 3;
                    i_pos_x = (i_x & 7);
    
                    p_fl[i_fr + i_x] = p_fr[i_fr + i_x]
                                     = (i_mb_y*Sequence.i_mb_width + i_mb_y)*64
                                            + i_pos_y*8 + i_pos_x;
                }
                i_fr += Sequence.i_width >> 1;
                i_fl += Sequence.i_width;
                if( i_fl == (Sequence.i_width*Sequence.i_height >> 2) )
                {
                    i_fl = Sequence.i_width >> 1;
                }
            }
        } /* switch */
        
        /* Link the new lookup tables so that they don't get freed all
         * the time. */
        LINK_LOOKUP( Sequence.p_frame_lum_lookup );
        LINK_LOOKUP( Sequence.p_field_lum_lookup );
        LINK_LOOKUP( Sequence.p_frame_chroma_lookup );
        LINK_LOOKUP( Sequence.p_field_chroma_lookup );
#undef Sequence
    }
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
    int                 i_coding_type;
    mtime_t             i_pts;
    undec_picture_t *   p_undec_p;
    
    DumpBits( &p_vpar->bit_stream, 10 ); /* temporal_reference */
    i_coding_type = GetBits( &p_vpar->bit_stream, 3 );
    
    if( ((i_coding_type == P_CODING_TYPE) &&
         (p_vpar->sequence.p_forward == NULL)) ||
        ((i_coding_type == B_CODING_TYPE) &&
         (p_vpar->sequence.p_forward == NULL ||
          p_vpar->sequence.p_backward == NULL)) )
    {
        /* The picture cannot be decoded because we lack one of the
         * reference frames */
        
        /* Update the reference pointers */
        ReferenceUpdate( p_vpar, i_coding_type, NULL );
        
        /* Warn Synchro we have trashed a picture */
        vpar_SynchroTrash( p_vpar, i_coding_type );

        return;
    }

    if( !(i_pts = vpar_SynchroChoose( p_vpar, i_coding_type )) )
    {
        /* Synchro has decided not to decode the picture */
        
        /* Update the reference pointers */
        ReferenceUpdate( p_vpar, i_coding_type, NULL );
        
        return;
    }

    /* OK, now we are sure we will decode the picture. Get a structure. */
    p_undec_p = vpar_NewPicture( &p_vpar->vfifo );
    
    /* Request a buffer from the video_output. */
    p_undec_p->p_picture = vout_CreatePicture( p_vpar->p_vout,
                                               SPLITTED_YUV_PICTURE,
                                               p_vpar->sequence.i_width,
                                               p_vpar->sequence.i_height,
                                               p_vpar->sequence.i_chroma_format );

    /* Initialize values */
    p_undec_p->i_coding_type = i_conding_type;
    p_undec_p->b_mpeg2 = p_vpar->sequence.b_mpeg2;
    p_undec_p->i_mb_height = p_vpar->sequence.i_mb_height;
    p_undec_p->i_mb_width = p_vpar->sequence.i_mb_width;
    p_undec_p->i_pts = i_pts;
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
        /* The P picture would have become the new p_backward reference. */
        vout_UnlinkPicture( p_vpar->p_vout, p_vpar->sequence.p_forward );
        p_vpar->sequence.p_forward = p_vpar->sequence.p_backward;
        p_vpar->sequence.p_backward = p_newref;
    }
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
 * SequenceDisplayExtension : Parse the sequence_display_extension structure
 *****************************************************************************/
static void SequenceDisplayExtension( vpar_thread_t * p_vpar )
{

}

/*****************************************************************************
 * LoadMatrix : Load a quantization matrix
 *****************************************************************************/
static void LoadMatrix( vpar_thread_t * p_vpar, quant_matrix_t * p_matrix )
{
    int i_dummy;
    
    if( !p_matrix->b_allocated )
    {
        /* Allocate a piece of memory to load the matrix. */
        p_matrix->pi_matrix = (int *)malloc( 64*sizeof(int) );
        p_matrix->b_allocated = TRUE;
    }
    
    for( i_dummy = 0; i_dummy < 64; i_dummy++ )
    {
        p_matrix->pi_matrix[pi_scan[SCAN_ZIGZAG][i_dummy]]
             = GetBits( p_vpar->bit_stream, 8 );
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
static void LinkMatrix( quant_matrix_t * p_matrix, int * pi_array )
{
    int i_dummy;
    
    if( p_matrix->b_allocated )
    {
        /* Deallocate the piece of memory. */
        free( p_matrix->pi_matrix );
        p_matrix->b_allocated = FALSE;
    }
    
    p_matrix->pi_matrix = pi_array;
}

