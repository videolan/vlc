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
                                       macroblock_t * p_mb );
static __inline__ int MacroblockAddressIncrement( vpar_thread_t * p_vpar );
static __inline__ void MacroblockModes( vpar_thread_t * p_vpar,
                                        macroblock_t * p_mb );
typedef (void *)    f_decode_block_t( vpar_thread_t *, macroblock_t *, int );
static void vpar_DecodeMPEG1Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG1Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG2Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG2Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );

/*
 * Standard variables
 */

/*****************************************************************************
 * pi_default_intra_quant : default quantization matrix
 *****************************************************************************/
#ifndef VDEC_DFT
extern int pi_default_intra_quant =
{
    8,  16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83
}:	
#else
extern int pi_default_intra_quant =
{
    2048,   5681,   6355,   6623,   6656,   5431,   4018,   2401,
    5681,   7880,   10207,  10021,  9587,   8091,   6534,   3625,
    6355,   10207,  11363,  10619,  9700,   8935,   6155,   3507,
    6623,   9186,   10226,  9557,   8730,   8041,   6028,   3322,
    5632,   9232,   9031,   8730,   8192,   7040,   5542,   3390,
    5230,   7533,   7621,   7568,   7040,   6321,   5225,   3219,
    3602,   5189,   5250,   5539,   5265,   5007,   4199,   2638,
    1907,   2841,   3230,   3156,   3249,   3108,   2638,   1617	
};
#endif

/*****************************************************************************
 * pi_default_nonintra_quant : default quantization matrix
 *****************************************************************************/
#ifndef VDEC_DFT
extern int pi_default_nonintra_quant =
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
#else
extern int pi_default_nonintra_quant =
{
    4096,   5680,   5344,   4816,   4096,   3216,   2224,   1136,
    5680,   7888,   7424,   6688,   5680,   4464,   3072,   1568,
    5344,   7424,   6992,   6288,   5344,   4208,   2896,   1472,
    4816,   6688,   6288,   5664,   4816,   3792,   2608,   1328,
    4096,   5680,   5344,   4816,   4096,   3216,   2224,   1136,
    3216,   4464,   4208,   3792,   3216,   2528,   1744,   880,
    2224,   3072,   2896,   2608,   2224,   1744,   1200,   608,
    1136,   1568,   1472,   1328,   1136,   880,    608,    304	
};
#endif

/*****************************************************************************
 * pi_scan : zig-zag and alternate scan patterns
 *****************************************************************************/
extern int pi_scan[2][64] =
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
 * Initialization of lookup tables
 */

/*****************************************************************************
 * vpar_InitCrop : Initialize the crop table for saturation
 *                 (ISO/IEC 13818-2 section 7.4.3)
 *****************************************************************************/
#ifdef MPEG2_COMPLIANT
void vpar_InitCrop( vpar_thread_t * p_vpar )
{
    int i_dummy;

    p_vpar->pi_crop = p_vpar->pi_crop_buf + 32768;

    for( i_dummy = -32768; i_dummy < -2048; i_dummy++ )
    {
        p_vpar->pi_crop[i_dummy] = -2048;
    }
    for( ; i_dummy < 2047; i_dummy++ )
    {
        p_vpar->pi_crop[i_dummy] = i_dummy;
    }
    for( ; i_dummy < 32767; i_dummy++ )
    {
        p_vpar->pi_crop[i_dummy] = 2047;
    }
}
#endif

/*****************************************************************************
 * InitMbAddrInc : Initialize the lookup table for mb_addr_inc               *
 *****************************************************************************/

void InitMbAddrInc( vpar_thread_t * p_vpar )
{
    bzero( &p_vpar->mb_addr_inc, 4096*sizeof( int ) );
    p_vpar->mb_addr_inc[8].i_value = MACROBLOCK_ESCAPE;
    p_vpar->mb_addr_inc[15].i_value = MACROBLOCK_STUFFING;
    p_vpar->mb_addr_inc[24].i_value = 33;
    p_vpar->mb_addr_inc[25].i_value = 32;
    p_vpar->mb_addr_inc[26].i_value = 31;
    p_vpar->mb_addr_inc[27].i_value = 30;
    p_vpar->mb_addr_inc[28].i_value = 29;
    p_vpar->mb_addr_inc[29].i_value = 28;
    p_vpar->mb_addr_inc[30].i_value = 27;
    p_vpar->mb_addr_inc[31].i_value = 26;
    p_vpar->mb_addr_inc[32].i_value = 25;
    p_vpar->mb_addr_inc[33].i_value = 24;
    p_vpar->mb_addr_inc[34].i_value = 23;
    p_vpar->mb_addr_inc[35].i_value = 22;
    p_vpar->mb_addr_inc[36].i_value = 21;
    p_vpar->mb_addr_inc[38].i_value = 20;
    p_vpar->mb_addr_inc[40].i_value = 19;
    p_vpar->mb_addr_inc[42].i_value = 18;
    p_vpar->mb_addr_inc[44].i_value = 17;
    p_vpar->mb_addr_inc[46].i_value = 16;
    p_vpar->mb_addr_inc[48].i_value = 15;
    p_vpar->mb_addr_inc[56].i_value = 14;
    p_vpar->mb_addr_inc[64].i_value = 13;
    p_vpar->mb_addr_inc[72].i_value = 12;
    p_vpar->mb_addr_inc[80].i_value = 11;
    p_vpar->mb_addr_inc[88].i_value = 10;
    p_vpar->mb_addr_inc[96].i_value = 9;
    p_vpar->mb_addr_inc[112].i_value = 8;
    p_vpar->mb_addr_inc[128].i_value = 7;
    p_vpar->mb_addr_inc[192].i_value = 6;
    p_vpar->mb_addr_inc[256].i_value = 5;
    p_vpar->mb_addr_inc[384].i_value = 4;
    p_vpar->mb_addr_inc[512].i_value = 3;
    p_vpar->mb_addr_inc[768].i_value = 2;
    p_vpar->mb_addr_inc[1024].i_value = 1;
    /* Length of the variable length code */
    p_vpar->mb_addr_inc[8].i_length = 11;
    p_vpar->mb_addr_inc[15].i_length = 11;
    p_vpar->mb_addr_inc[24].i_length = 11;
    p_vpar->mb_addr_inc[25].i_length = 11;
    p_vpar->mb_addr_inc[26].i_length = 11;
    p_vpar->mb_addr_inc[27].i_length = 11;
    p_vpar->mb_addr_inc[28].i_length = 11;
    p_vpar->mb_addr_inc[29].i_length = 11;
    p_vpar->mb_addr_inc[30].i_length = 11;
    p_vpar->mb_addr_inc[31].i_length = 11;
    p_vpar->mb_addr_inc[32].i_length = 11;
    p_vpar->mb_addr_inc[33].i_length = 11;
    p_vpar->mb_addr_inc[34].i_length = 11;
    p_vpar->mb_addr_inc[35].i_length = 11;
    p_vpar->mb_addr_inc[36].i_length = 10;
    p_vpar->mb_addr_inc[38].i_length = 10;
    p_vpar->mb_addr_inc[40].i_length = 10;
    p_vpar->mb_addr_inc[42].i_length = 10;
    p_vpar->mb_addr_inc[44].i_length = 10;
    p_vpar->mb_addr_inc[46].i_length = 10;
    p_vpar->mb_addr_inc[48].i_length = 8;
    p_vpar->mb_addr_inc[56].i_length = 8;
    p_vpar->mb_addr_inc[64].i_length = 8;
    p_vpar->mb_addr_inc[72].i_length = 8;
    p_vpar->mb_addr_inc[80].i_length = 8;
    p_vpar->mb_addr_inc[88].i_length = 8;
    p_vpar->mb_addr_inc[96].i_length = 7;   
    p_vpar->mb_addr_inc[112].i_length = 7;
    p_vpar->mb_addr_inc[128].i_length = 5;
    p_vpar->mb_addr_inc[192].i_length = 5;
    p_vpar->mb_addr_inc[256].i_length = 4;
    p_vpar->mb_addr_inc[384].i_length = 4;
    p_vpar->mb_addr_inc[512].i_length = 3;
    p_vpar->mb_addr_inc[768].i_length = 3;   
    p_vpar->mb_addr_inc[1024].i_length = 1;
}

/*
 * Macroblock parsing functions
 */

/*****************************************************************************
 * vpar_ParseMacroblock : Parse the next macroblock
 *****************************************************************************/
void vpar_ParseMacroblock( vpar_thread_t * p_vpar, int * pi_mb_address,
                           int i_mb_previous, int i_mb_base )
{
    static f_addb_t ppf_addb_intra[2] = {vdec_AddBlock, vdec_CopyBlock};
    static f_decode_block_t pppf_decode_block[2][2] =
                { {vpar_DecodeMPEG1Non, vpar_DecodeMPEG1Intra},
                  {vpar_DecodeMPEG2Non, vpar_DecodeMPEG2Intra} };
    static int      pi_x[12] = {0,8,0,8,0,0,0,0,8,8,8,8};
    static int      pi_y[2][12] = { {0,0,8,8,0,0,8,8,0,0,8,8},
                                    {0,0,1,1,0,0,1,1,0,0,1,1} };
    static int      pi_chroma_hor[4] = { 0, 1, 1, 0 };
    static int      pi_chroma_ver[4] = { 0, 1, 0, 0 };

    int             i_mb, i_b, i_mask;
    macroblock_t *  p_mb;
    f_addb_t        pf_addb;
    elem_t *        p_data1;
    elem_t *        p_data2;

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

        InitMacroblock( p_vpar, p_mb );

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

    InitMacroblock( p_vpar, p_mb );

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

    /*
     * Effectively decode blocks.
     */

    i_mask = 1 << (3 + 2*p_vpar->sequence.i_chroma_nb_blocks);

    /* luminance */
    p_data1 = p_mb->p_picture->p_y
              + p_mb->i_l_x + p_mb->i_l_y*(p_vpar->sequence.i_width);

    for( i_b = 0; i_b < 4; i_b++, i_mask >>= 1 )
    {
        if( p_vpar->mb.i_coded_block_pattern & i_mask )
        {
            memset( p_mb->ppi_blocks[i_b], 0, 64*sizeof(elem_t) );
            (*pppf_decode_block[p_vpar->sequence.b_mpeg2]
                               [p_vpar->mb.i_mb_type & MB_INTRA])
                ( p_vpar, p_mb, i_b );
        
            /* decode_block has already set pf_idct and pi_sparse_pos. */
            p_mb->pf_addb[i_b] = pf_addb;
     
            /* Calculate block coordinates. */
            p_mb->p_data[i_b] = p_data1
                                 + pi_y[p_vpar->mb.b_dct_type][i_b]
                                   * p_vpar->sequence.i_chroma_width;
        }
        else
        {
            /* Block not coded, so no IDCT, nor AddBlock */
            p_mb->pf_addb[i_b] = vdec_DummyBlock;
            p_mb->pf_idct[i_b] = vdec_DummyIDCT;
        }
    }

    /* chrominance U */
    p_data1 = p_mb->p_picture->p_u
              + p_mb->i_c_x >> pi_chroma_hor[p_vpar->sequence.i_chroma_format]
              + (p_mb->i_c_y >> pi_chroma_ver[p_vpar->sequence.i_chroma_format])
                * (p_vpar->sequence.i_chroma_width);
    p_data2 = p_mb->p_picture->p_v
              + p_mb->i_c_x >> pi_chroma_hor[p_vpar->sequence.i_chroma_format]
              + (p_mb->i_c_y >> pi_chroma_ver[p_vpar->sequence.i_chroma_format])
                * (p_vpar->sequence.i_chroma_width);

    for( i_b = 4; i_b < 4 + 2*p_vpar->sequence.i_chroma_nb_blocks;
         i_b++, i_mask >>= 1 )
    {
        elem_t *    pp_data[2] = {p_data1, p_data2};

        if( p_vpar->mb.i_coded_block_pattern & i_mask )
        {
            memset( p_mb->ppi_blocks[i_b], 0, 64*sizeof(elem_t) );
            (*pppf_decode_block[p_vpar->sequence.b_mpeg2]
                               [p_vpar->mb.i_mb_type & MB_INTRA])
                ( p_vpar, p_mb, i_b );

            /* decode_block has already set pf_idct and pi_sparse_pos. */
            p_mb->pf_addb[i_b] = pf_addb;

            /* Calculate block coordinates. */
            p_mb->p_data[i_b] = pp_data[i_b & 1]
                                 + pi_y[p_vpar->mb.b_dct_type][i_b]
                                   * p_vpar->sequence.i_chroma_width;
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
                                       macroblock_t * p_mb )
{
    p_mb->p_picture = p_vpar->picture.p_picture;
    p_mb->i_structure = p_vpar->picture.i_structure;
    p_mb->i_l_x = p_vpar->mb.i_l_x;
    p_mb->i_l_y = p_vpar->mb.i_l_y;
    p_mb->i_c_x = p_vpar->mb.i_c_x;
    p_mb->i_c_y = p_vpar->mb.i_c_y;
    p_mb->i_chroma_nb_blocks = p_vpar->sequence.i_chroma_nb_blocks;

    p_mb->i_addb_l_stride = p_mb->i_l_stride = p_vpar->picture.i_l_stride;
    p_mb->i_addb_c_stride = p_mb->i_c_stride = p_vpar->picture.i_c_stride;

    /* Update macroblock real position. */
    p_vpar->mb.i_l_x += 16;
    p_vpar->mb.i_l_y += (p_vpar->mb.i_l_x / p_vpar->sequence.i_width)
                        * (2 - p_vpar->picture.b_frame_structure) * 16;
    p_vpar->mb.i_l_x %= p_vpar->sequence.i_width;

    p_vpar->mb.i_c_x += p_vpar->sequence.i_chroma_mb_width;
    p_vpar->mb.i_c_y += (p_vpar->mb.i_c_x / p_vpar->sequence.i_chroma_width)
                        * (2 - p_vpar->picture.b_frame_structure)
                        * p_vpar->sequence.i_chroma_mb_height;
    p_vpar->mb.i_c_x %= p_vpar->sequence.i_chroma_width;
}

/*****************************************************************************
 * MacroblockAddressIncrement : Get the macroblock_address_increment field
 *****************************************************************************/
static __inline__ int MacroblockAddressIncrement( vpar_thread_t * p_vpar )
{
    /* Index in the lookup table mb_addr_inc */
    int    i_index = ShowBits( &p_vpar->bit_stream, 11 );
    p_vpar->mb.i_addr_inc = 0;
    /* Test the presence of the escape character */
    while( i_index == 8 )
    {
        DumpBits( &p_vpar->bit_stream, 11 );
        p_vpar->mb.i_addr_inc += 33;
        i_index = ShowBits( &p_vpar->bit_stream, 11 );
    }
    /* Affect the value from the lookup table */
    p_vpar->mb.i_addr_inc += p_vpar->mb_addr_inc[i_index].i_value;
    /* Dump the good number of bits */
    DumpBits( &p_vpar->bit_stream, p_vpar->mb_addr_inc[i_index].i_length );
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
    
    if( !(p_vpar->mb.i_mb_type & (MB_MOTION_FORWARD | MB_MOTION_BACKWARD))
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

    p_vpar->mb.b_dct_type = 0;
    if( (p_vpar->picture.i_structure == FRAME_STRUCTURE) &&
        (!p_vpar->picture.b_frame_pred_frame_dct) &&
        (p_vpar->mb.i_mb_type & (MB_PATTERN|MB_INTRA)) )
    {
        if( p_vpar->mb.b_dct_type = GetBits( &p_vpar->bit_stream, 1 ) )
        {
            /* The DCT is coded on fields. Jump one line between each
             * sample. */
            p_mb->i_addb_l_stride <<= 1;
	    p_mb->i_addb_l_stride += 8;
            /* With CHROMA_420, the DCT is necessarily frame-coded. */
            if( p_vpar->picture.sequence.i_chroma_format != CHROMA_420 )
            {
	        p_mb->i_addb_c_stride <<= 1;
	        p_mb->i_addb_c_stride += 8;
            }
        }
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
