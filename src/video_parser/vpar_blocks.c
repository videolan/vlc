/*****************************************************************************
 * vpar_blocks.c : blocks parsing
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
#include "vpar_synchro.h"
#include "video_parser.h"

/*
 * Local prototypes
 */
static __inline__ void InitMacroblock( vpar_thread_t * p_vpar,
                                       macroblock_t * p_mb, int i_mb_address );
static __inline__ int MacroblockAddressIncrement( vpar_thread_t * p_vpar );
static __inline__ void MacroblockModes( vpar_thread_t * p_vpar,
                                        macroblock_t * p_mb );
static void vpar_DecodeMPEG1Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG1Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG2Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG2Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );

typedef void      (*f_decode_block_t)( vpar_thread_t *, macroblock_t *, int );

/*****************************************************************************
 * vpar_ParseMacroblock : Parse the next macroblock
 *****************************************************************************/
void vpar_ParseMacroblock( vpar_thread_t * p_vpar, int * pi_mb_address,
                           int i_mb_previous, int i_mb_base )
{
    static f_addb_t ppf_addb_intra[2] = {vdec_AddBlock, vdec_CopyBlock};

    int             i_mb, i_b, i_mask, i_x, i_y, pi_pos[3], pi_width[3];
    macroblock_t *  p_mb;
    f_addb_t        pf_addb;

    *pi_mb_address += MacroblockAddressIncrement( p_vpar );

    for( i_mb = i_mb_previous; i_mb < *pi_mb_address; i_mb++ )
    {
        /* Skipped macroblock (ISO/IEC 13818-2 7.6.6). */
        static int          pi_dc_dct_reinit[4] = {128,256,512,1024};
        static f_motion_t   pf_motion_skipped[4] = {NULL, vdec_MotionField,
                                vdec_MotionField, vdec_MotionFrame};

        /* Reset DC predictors (7.2.1). */
        p_vpar->slice.pi_dc_dct_pred[0] = p_vpar->slice.pi_dc_dct_pred[1]
            = p_vpar->slice.pi_dc_dct_pred[2]
            = pi_dc_dct_reinit[p_vpar->picture.i_intra_dc_precision];

        if( p_vpar->picture.i_coding_type == P_CODING_TYPE )
        {
            /* Reset motion vector predictors (ISO/IEC 13818-2 7.6.3.4). */
            bzero( p_vpar->slice.pppi_pmv, 8*sizeof(int) );
        }

        if( (p_mb = p_vpar->picture.pp_mb[i_mb_base + i_mb] =
             vpar_NewMacroblock( &p_vpar->vfifo )) == NULL )
        {
            p_vpar->picture.b_error = 1;
            intf_ErrMsg("vpar error: macroblock list is empty !");
            return;
        }

        InitMacroblock( p_vpar, p_mb, i_mb );

        /* No IDCT nor AddBlock. */
        for( i_b = 0; i_b < 12; i_b++ )
        {
            p_mb->pf_idct[i_b] = vdec_DummyIDCT;
            p_mb->pf_addb[i_b] = vdec_DummyBlock;
        }

        /* Motion type is picture structure. */
        p_mb->pf_motion = pf_motion_skipped[p_vpar->picture.i_structure];

        /* Predict from field of same parity. */
        /* ??? */
    }

    /* Get a macroblock structure. */
    if( (p_mb = p_vpar->picture.pp_mb[i_mb_base + *pi_mb_address] =
         vpar_NewMacroblock( &p_vpar->vfifo )) == NULL )
    {
        p_vpar->picture.b_error = 1;
        intf_ErrMsg("vpar error: macroblock list is empty !");
        return;
    }

    InitMacroblock( p_vpar, p_mb, *pi_mb_address );

    /* Parse off macroblock_modes structure. */
    MacroblockModes( p_vpar, p_mb );

    if( p_vpar->mb.i_mb_type & MB_QUANT )
    {
        LoadQuantizerScale( p_vpar );
    }

    if( p_vpar->mb.i_mb_type & MB_MOTION_FORWARD )
    {
        (*p_vpar->sequence.pf_decode_mv)( p_vpar, 0 );
    }

    if( p_vpar->mb.i_mb_type & MB_MOTION_BACKWARD )
    {
        (*p_vpar->sequence.pf_decode_mv)( p_vpar, 1 );
    }

    if( p_vpar->picture.b_concealment_mv && (p_vpar->mb.i_mb_type & MB_INTRA) )
    {
        DumpBits( &p_vpar->bit_stream, 1 );
    }

    if( p_vpar->mb.i_mb_type & MB_PATTERN )
    {
        (*p_vpar->sequence.pf_decode_pattern)( p_vpar );
    }
    else
    {
        int     pi_coded_block_pattern[2] = {0,
                    (1 << 4+2*p_vpar->sequence.i_chroma_nb_blocks) - 1};
        p_vpar->mb.i_coded_block_pattern = pi_coded_block_pattern
                                    [p_vpar->mb.i_mb_type & MB_INTRA];
    }

    pf_addb = ppf_addb_intra[p_vpar->mb.i_mb_type & MB_INTRA];

    /* C'est de la merde, il faut recommencer */

    i_x = p_mb->i_mb_x << 4;
    i_y = p_mb->i_mb_y << 4;

    pi_pos[0] = i_y*(p_vpar->sequence.i_width
                        << (!p_vpar->picture.b_frame_structure))
                + (p_mb->i_structure == BOTTOM_FIELD)*p_vpar->sequence.i_width
                + i_x;
    pi_pos[1] = pi_pos[2] = i_y*(p_vpar->sequence.i_chroma_width
                        << (!p_vpar->picture.b_frame_structure))
                + (p_mb->i_structure == BOTTOM_FIELD)*p_vpar->sequence.i_chroma_width
                + (i_x >> (3-p_vpar->sequence.i_chroma_format));
    pi_width[0] = p_vpar->sequence.i_width << (!p_vpar->picture.b_frame_structure
                                               || p_vpar->mb.b_dct_type);
    pi_width[1] = pi_width[2] = p_vpar->sequence.i_chroma_width
                << (!p_vpar->picture.b_frame_structure || p_vpar->mb.b_dct_type);

    /* Effectively decode blocks. */
    for( i_b = 0, i_mask = 1 << (3 + 2*p_vpar->sequence.i_chroma_nb_blocks);
         i_b < 4 + 2*p_vpar->sequence.i_chroma_nb_blocks; i_b++, i_mask >>= 1 )
    {
        if( p_vpar->mb.i_coded_block_pattern & i_mask )
        {
            static f_decode_block_t pppf_decode_block[2][2] =
                { {vpar_DecodeMPEG1Non, vpar_DecodeMPEG1Intra},
                  {vpar_DecodeMPEG2Non, vpar_DecodeMPEG2Intra} };
            static int              pi_x[12] = {0,8,0,8,0,0,0,0,8,8,8,8};
            static int              pi_y[12] = {0,0,8,8,0,0,8,8,0,0,8,8};
            data_t                  pi_data[12] =
                {p_mb->p_picture->p_y, p_mb->p_picture->p_y,
                 p_mb->p_picture->p_y, p_mb->p_picture->p_y,
                 p_mb->p_picture->p_u, p_mb->p_picture->p_v,
                 p_mb->p_picture->p_u, p_mb->p_picture->p_v,
                 p_mb->p_picture->p_u, p_mb->p_picture->p_v,
                 p_mb->p_picture->p_u, p_mb->p_picture->p_v};

            bzero( p_mb->ppi_blocks[i_b], 64*sizeof(elem_t) );
            (*pppf_decode_block[p_vpar->sequence.b_mpeg2]
                               [p_vpar->mb.i_mb_type & MB_INTRA])
                ( p_vpar, p_mb, i_b );

            /* decode_block has already set pf_idct and pi_sparse_pos. */
            p_mb->pf_addb[i_b] = pf_addb;

            /* Calculate block coordinates. */
            p_mb->p_data[i_b] = pi_data[i_b] + pi_pos[i_b >> 2]
                    + pi_y[i_b]*pi_width[i_b >> 2]
                    + (p_vpar->mb.b_dct_type & ((i_b & 2) >> 1));
                    /* INACHEVÉ parce que trop pourri ! */
        }
        else
        {
            /* Block not coded, so no IDCT, nor AddBlock */
            p_mb->pf_addb[i_b] = vdec_DummyBlock;
            p_mb->pf_idct[i_b] = vdec_DummyIDCT;
        }
    }

    if( !( p_vpar->mb.i_mb_type & MB_INTRA ) )
    {
        static int          pi_dc_dct_reinit[4] = {128,256,512,1024};

        /* Reset DC predictors (7.2.1). */
        p_vpar->slice.pi_dc_dct_pred[0] = p_vpar->slice.pi_dc_dct_pred[1]
            = p_vpar->slice.pi_dc_dct_pred[2]
            = pi_dc_dct_reinit[p_vpar->picture.i_intra_dc_precision];
    }
    else if( !p_vpar->picture.b_concealment_mv )
    {
        /* Reset MV predictors. */
        bzero( p_vpar->slice.pppi_pmv, 8*sizeof(int) );
    }

    if( (p_vpar->picture.i_coding_type == P_CODING_TYPE) &&
        !(p_vpar->mb.i_mb_type & (MB_MOTION_FORWARD|MB_INTRA)) )
    {
        /* Special No-MC macroblock in P pictures (7.6.3.5). */
        p_vpar->slice.pppi_pmv[0][0][0] = p_vpar->slice.pppi_pmv[0][0][1] =
        p_vpar->slice.pppi_pmv[1][0][0] = p_vpar->slice.pppi_pmv[1][0][1] = 0;
        
        /* motion type ?????? */
        /* predict from field of same parity ????? */
    }
}

/*****************************************************************************
 * InitMacroblock : Initialize macroblock values
 *****************************************************************************/
static __inline__ void InitMacroblock( vpar_thread_t * p_vpar,
                                       macroblock_t * p_mb, int i_mb_address )
{
    p_mb->p_picture = p_vpar->picture.p_picture;
    p_mb->i_structure = p_vpar->picture.i_structure;
    p_mb->i_mb_x = i_mb_address % p_vpar->sequence.i_mb_width;
    p_mb->i_mb_y = i_mb_address / p_vpar->sequence.i_mb_width;
    p_mb->i_chroma_nb_blocks = p_vpar->sequence.i_chroma_nb_blocks;

    p_mb->i_lum_incr = p_vpar->picture.i_lum_incr;
    p_mb->i_chroma_incr = p_vpar->picture.i_chroma_incr;
}

/*****************************************************************************
 * MacroblockAddressIncrement : Get the macroblock_address_increment field
 *****************************************************************************/
static __inline__ int MacroblockAddressIncrement( vpar_thread_t * p_vpar )
{
    /* À pomper dans Berkeley */
}

/*****************************************************************************
 * MacroblockModes : Get the macroblock_modes structure
 *****************************************************************************/
static __inline__ void MacroblockModes( vpar_thread_t * p_vpar,
                                        macroblock_t * p_mb )
{
    static f_motion_t   pf_motion[2][4] =
        { {NULL, vdec_FieldRecon, vdec_16x8Recon, vdec_DMVRecon},
          {NULL, vdec_FieldRecon, vdec_FrameRecon, vdec_DMVRecon} };
    static int          ppi_mv_count[2][4] = { {0, 1, 2, 1}, {0, 2, 1, 1} };
    static int          ppi_mv_format[2][4] = { {0, 1, 1, 1}, {0, 1, 2, 1} };

    /* Get macroblock_type. */
    p_vpar->mb.i_mb_type = (p_vpar->picture.pf_macroblock_type)
                                  ( vpar_thread_t * p_vpar );

    /* SCALABILITY : warning, we don't know if spatial_temporal_weight_code
     * has to be dropped, take care if you use scalable streams. */
    /* DumpBits( &p_vpar->bit_stream, 2 ); */
    
    if( !(p_vpar->mb.i_mb_type & (MB_MOTION_FORWARD || MB_MOTION_BACKWARD))
        || p_vpar->picture.b_frame_pred_frame_dct )
    {
        /* If mb_type has neither MOTION_FORWARD nor MOTION_BACKWARD, this
         * is useless, but also harmless. */
        p_vpar->mb.i_motion_type = MOTION_FRAME;
    }
    else
    {
        p_vpar->mb.i_motion_type = GetBits( &p_vpar->bit_stream, 2 );
    }

    p_mb->pf_motion = pf_motion[p_vpar->picture.b_frame_structure]
                              [p_vpar->mb.i_motion_type];
    p_vpar->mb.i_mv_count = ppi_mv_count[p_vpar->picture.b_frame_structure]
                                        [p_vpar->mb.i_motion_type];
    p_vpar->mb.i_mv_format = ppi_mv_format[p_vpar->picture.b_frame_structure]
                                          [p_vpar->mb.i_motion_type];

    if( (p_vpar->picture.i_structure == FRAME_STRUCTURE) &&
        (!p_vpar->picture.b_frame_pred_frame_dct) &&
        (p_vpar->mb.i_mb_type & (MB_PATTERN|MB_INTRA)) )
    {
        p_vpar->mb.b_dct_type = GetBits( &p_vpar->bit_stream, 1 );
    }
    else
    {
        p_vpar->mb.b_dct_type = 0;
    }
}

/*****************************************************************************
 * vpar_IMBType : macroblock_type in I pictures
 *****************************************************************************/
int vpar_IMBType( vpar_thread_t * p_vpar )
{
    /* À pomper dans Berkeley */
}

/*****************************************************************************
 * vpar_PMBType : macroblock_type in P pictures
 *****************************************************************************/
int vpar_PMBType( vpar_thread_t * p_vpar )
{
    /* À pomper dans Berkeley */
}

/*****************************************************************************
 * vpar_BMBType : macroblock_type in B pictures
 *****************************************************************************/
int vpar_BMBType( vpar_thread_t * p_vpar )
{
    /* À pomper dans Berkeley */
}

/*****************************************************************************
 * vpar_DMBType : macroblock_type in D pictures
 *****************************************************************************/
int vpar_DMBType( vpar_thread_t * p_vpar )
{
    /* À pomper dans Berkeley */
}

/*****************************************************************************
 * vpar_CodedPattern420 : coded_block_pattern with 420 chroma
 *****************************************************************************/
int vpar_CodedPattern420( vpar_thread_t * p_vpar )
{
    /* À pomper dans Berkeley */
}

/*****************************************************************************
 * vpar_CodedPattern422 : coded_block_pattern with 422 chroma
 *****************************************************************************/
int vpar_CodedPattern422( vpar_thread_t * p_vpar )
{
    /* À pomper dans Berkeley + attention ! y'a 2 bits en plus en MPEG2 */
}

/*****************************************************************************
 * vpar_CodedPattern444 : coded_block_pattern with 444 chroma
 *****************************************************************************/
int vpar_CodedPattern444( vpar_thread_t * p_vpar )
{
    /* À pomper dans Berkeley + attention ! y'a 4 bits en plus en MPEG2 */
}

/*****************************************************************************
 * vpar_DecodeMPEG1Non : decode MPEG-1 non-intra blocks
 *****************************************************************************/
static void vpar_DecodeMPEG1Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b )
{
    /* À pomper dans Berkeley. Pour toutes ces fonctions, il faut mettre
       p_mb->pf_idct[i_b] à :
        - vdec_IDCT ou
        - vdec_SparseIDCT si la matrice n'a qu'un coefficient non nul.
       Dans le deuxième cas, p_mb->pi_sparse_pos[i_b] contient le numéro
       de ce coefficient. */

    if( p_vpar->picture.i_coding_type == D_CODING_TYPE )
    {
        /* Remove end_of_macroblock (always 1, prevents startcode emulation)
         * ISO/IEC 11172-2 section 2.4.2.7 and 2.4.3.6 */
        DumpBits( &p_vpar->bit_stream, 1 );
    }
}

/*****************************************************************************
 * vpar_DecodeMPEG1Intra : decode MPEG-1 intra blocks
 *****************************************************************************/
static void vpar_DecodeMPEG1Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b )
{
    /* À pomper dans Berkeley. */

    if( p_vpar->picture.i_coding_type == D_CODING_TYPE )
    {
        /* Remove end_of_macroblock (always 1, prevents startcode emulation)
         * ISO/IEC 11172-2 section 2.4.2.7 and 2.4.3.6 */
        DumpBits( &p_vpar->bit_stream, 1 );
    }
}

/*****************************************************************************
 * vpar_DecodeMPEG2Non : decode MPEG-2 non-intra blocks
 *****************************************************************************/
static void vpar_DecodeMPEG2Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b )
{
    /* À pomper dans Berkeley. Bien sûr les matrices seront différentes... */
}

/*****************************************************************************
 * vpar_DecodeMPEG2Intra : decode MPEG-2 intra blocks
 *****************************************************************************/
static void vpar_DecodeMPEG2Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b )
{
    /* À pomper dans Berkeley. */
}
