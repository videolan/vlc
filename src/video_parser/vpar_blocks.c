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
typedef void (*f_decode_block_t)( vpar_thread_t *, macroblock_t *, int );
static void vpar_DecodeMPEG1Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG1Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG2Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG2Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );

/*
 * Initialisation tables
 */
    /* Table for coded_block_pattern resolution */
static lookup_t     pl_coded_pattern_init_table[512] = 
    { {MB_ERROR, 0}, {0, 9}, {39, 9}, {27, 9}, {59, 9}, {55, 9}, {47, 9}, {31, 9},
    {58, 8}, {58, 8}, {54, 8}, {54, 8}, {46, 8}, {46, 8}, {30, 8}, {30, 8},
    {57, 8}, {57, 8}, {53, 8}, {53, 8}, {45, 8}, {45, 8}, {29, 8}, {29, 8},
    {38, 8}, {38, 8}, {26, 8}, {26, 8}, {37, 8}, {37, 8}, {25, 8}, {25, 8},
    {43, 8}, {43, 8}, {23, 8}, {23, 8}, {51, 8}, {51, 8}, {15, 8}, {15, 8},
    {42, 8}, {42, 8}, {22, 8}, {22, 8}, {50, 8}, {50, 8}, {14, 8}, {14, 8},
    {41, 8}, {41, 8}, {21, 8}, {21, 8}, {49, 8}, {49, 8}, {13, 8}, {13, 8},
    {35, 8}, {35, 8}, {19, 8}, {19, 8}, {11, 8}, {11, 8}, {7, 8}, {7, 8},
    {34, 7}, {34, 7}, {34, 7}, {34, 7}, {18, 7}, {18, 7}, {18, 7}, {18, 7},
    {10, 7}, {10, 7}, {10, 7}, {10, 7}, {6, 7}, {6, 7}, {6, 7}, {6, 7}, 
    {33, 7}, {33, 7}, {33, 7}, {33, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, 
    {9, 7}, {9, 7}, {9, 7}, {9, 7}, {5, 7}, {5, 7}, {5, 7}, {5, 7}, 
    {63, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6}, 
    {3, 6}, {3, 6}, {3, 6}, {3, 6}, {3, 6}, {3, 6}, {3, 6}, {3, 6}, 
    {36, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6}, 
    {24, 6}, {24, 6}, {24, 6}, {24, 6}, {24, 6}, {24, 6}, {24, 6}, {24, 6}, 
    {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5},
    {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5},
    {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, 
    {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, 
    {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, 
    {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, 
    {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, 
    {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, 
    {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, 
    {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, 
    {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, 
    {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, 
    {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, 
    {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, 
    {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, 
    {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, 
    {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, 
    {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, 
    {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, 
    {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, 
    {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, 
    {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, 
    {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, 
    {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, 
    {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, 
    {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, 
    {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, 
    {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, 
    {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, 
    {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, 
    {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, 
    {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, 
    {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, 
    {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, 
    {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, 
    {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4},
    {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4},
    {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, 
    {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, 
    {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4},
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3} };

    /* Tables for dc DCT coefficients 
     * Tables are cut in two parts to reduce memory occupation
     */
    
    /* Table B-12, dct_dc_size_luminance, codes 00xxx ... 11110 */
static lookup_t     pl_dct_dc_lum_init_table_1[32] =
    { {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
      {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
      {0, 3}, {0, 3}, {0, 3}, {0, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3},
      {4, 3}, {4, 3}, {4, 3}, {4, 3}, {5, 4}, {5, 4}, {6, 5}, {MB_ERROR, 0}
    };

    /* Table B-12, dct_dc_size_luminance, codes 111110xxx ... 111111111 */
static lookup_t     pl_dct_dc_lum_init_table_2[32] =
    { {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6},
      {8, 7}, {8, 7}, {8, 7}, {8, 7}, {9, 8}, {9, 8}, {10,9}, {11,9},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}
    };

    /* Table B-13, dct_dc_size_chrominance, codes 00xxx ... 11110 */
static lookup_t     pl_dct_dc_chrom_init_table_1[32] =
    { {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
      {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
      {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
      {3, 3}, {3, 3}, {3, 3}, {3, 3}, {4, 4}, {4, 4}, {5, 5}, {MB_ERROR, 0}
    };

    /* Table B-13, dct_dc_size_chrominance, codes 111110xxxx ... 1111111111 */
static lookup_t     pl_dct_dc_chrom_init_table_2[32] =
    { {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
      {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
      {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7},
      {8, 8}, {8, 8}, {8, 8}, {8, 8}, {9, 9}, {9, 9}, {10,10}, {11,10}
    };
    
    /* 
     * Structure to store the tables B14 & B15 
     * Is constructed from the tables below 
     */
    dct_lookup_t            ppl_dct_coef[2][16384];


    /* Tables for ac DCT coefficients. There are cut in many parts to save space */
    /* Table B-14, DCT coefficients table zero,
     * codes 0100 ... 1xxx (used for first (DC) coefficient)
     */
static dct_lookup_t pl_DCT_tab_dc[12] =
    {
        {0,2,4}, {2,1,4}, {1,1,3}, {1,1,3},
        {0,1,1}, {0,1,1}, {0,1,1}, {0,1,1},
        {0,1,1}, {0,1,1}, {0,1,1}, {0,1,1}
    };

    /* Table B-14, DCT coefficients table zero,
     * codes 0100 ... 1xxx (used for all other coefficients)
     */
static dct_lookup_t pl_DCT_tab_ac[12] =
    {
        {0,2,4},  {2,1,4},  {1,1,3},  {1,1,3},
        {DCT_EOB,0,2}, {DCT_EOB,0,2}, {DCT_EOB,0,2}, {DCT_EOB,0,2}, /* EOB */
        {0,1,2},  {0,1,2},  {0,1,2},  {0,1,2}
    };

    /* Table B-14, DCT coefficients table zero,
     * codes 000001xx ... 00111xxx
     */
static dct_lookup_t pl_DCT_tab0[60] =
    {
        {DCT_ESCAPE,0,6}, {DCT_ESCAPE,0,6}, {DCT_ESCAPE,0,6}, {DCT_ESCAPE,0,6},
        /* Escape */
        {2,2,7}, {2,2,7}, {9,1,7}, {9,1,7},
        {0,4,7}, {0,4,7}, {8,1,7}, {8,1,7},
        {7,1,6}, {7,1,6}, {7,1,6}, {7,1,6},
        {6,1,6}, {6,1,6}, {6,1,6}, {6,1,6},
        {1,2,6}, {1,2,6}, {1,2,6}, {1,2,6},
        {5,1,6}, {5,1,6}, {5,1,6}, {5,1,6},
        {13,1,8}, {0,6,8}, {12,1,8}, {11,1,8},
        {3,2,8}, {1,3,8}, {0,5,8}, {10,1,8},
        {0,3,5}, {0,3,5}, {0,3,5}, {0,3,5},
        {0,3,5}, {0,3,5}, {0,3,5}, {0,3,5},
        {4,1,5}, {4,1,5}, {4,1,5}, {4,1,5},
        {4,1,5}, {4,1,5}, {4,1,5}, {4,1,5},
        {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
        {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5}
    };

    /* Table B-15, DCT coefficients table one,
     * codes 000001xx ... 11111111
     */
static dct_lookup_t pl_DCT_tab0a[252] =
    {
        {65,0,6}, {65,0,6}, {65,0,6}, {65,0,6}, /* Escape */
        {7,1,7}, {7,1,7}, {8,1,7}, {8,1,7},
        {6,1,7}, {6,1,7}, {2,2,7}, {2,2,7},
        {0,7,6}, {0,7,6}, {0,7,6}, {0,7,6},
        {0,6,6}, {0,6,6}, {0,6,6}, {0,6,6},
        {4,1,6}, {4,1,6}, {4,1,6}, {4,1,6},
        {5,1,6}, {5,1,6}, {5,1,6}, {5,1,6},
        {1,5,8}, {11,1,8}, {0,11,8}, {0,10,8},
        {13,1,8}, {12,1,8}, {3,2,8}, {1,4,8},
        {2,1,5}, {2,1,5}, {2,1,5}, {2,1,5},
        {2,1,5}, {2,1,5}, {2,1,5}, {2,1,5},
        {1,2,5}, {1,2,5}, {1,2,5}, {1,2,5},
        {1,2,5}, {1,2,5}, {1,2,5}, {1,2,5},
        {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
        {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
        {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
        {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
        {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
        {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
        {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
        {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
        {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
        {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
        {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4}, /* EOB */
        {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
        {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
        {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
        {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
        {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
        {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
        {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
        {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
        {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
        {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
        {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
        {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
        {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
        {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
        {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
        {0,4,5}, {0,4,5}, {0,4,5}, {0,4,5},
        {0,4,5}, {0,4,5}, {0,4,5}, {0,4,5},
        {0,5,5}, {0,5,5}, {0,5,5}, {0,5,5},
        {0,5,5}, {0,5,5}, {0,5,5}, {0,5,5},
        {9,1,7}, {9,1,7}, {1,3,7}, {1,3,7},
        {10,1,7}, {10,1,7}, {0,8,7}, {0,8,7},
        {0,9,7}, {0,9,7}, {0,12,8}, {0,13,8},
        {2,3,8}, {4,2,8}, {0,14,8}, {0,15,8}
    };

    /* Table B-14, DCT coefficients table zero,
     * codes 0000001000 ... 0000001111
     */
static dct_lookup_t pl_DCT_tab1[8] =
    {
        {16,1,10}, {5,2,10}, {0,7,10}, {2,3,10},
        {1,4,10}, {15,1,10}, {14,1,10}, {4,2,10}
    };

    /* Table B-15, DCT coefficients table one,
     * codes 000000100x ... 000000111x
     */
static dct_lookup_t pl_DCT_tab1a[8] =
    {
        {5,2,9}, {5,2,9}, {14,1,9}, {14,1,9},
        {2,4,10}, {16,1,10}, {15,1,9}, {15,1,9}
    };

    /* Table B-14/15, DCT coefficients table zero / one,
     * codes 000000010000 ... 000000011111
     */
static dct_lookup_t pl_DCT_tab2[16] =
    {
        {0,11,12}, {8,2,12}, {4,3,12}, {0,10,12},
        {2,4,12}, {7,2,12}, {21,1,12}, {20,1,12},
        {0,9,12}, {19,1,12}, {18,1,12}, {1,5,12},
        {3,3,12}, {0,8,12}, {6,2,12}, {17,1,12}
    };

    /* Table B-14/15, DCT coefficients table zero / one,
     * codes 0000000010000 ... 0000000011111
     */
static dct_lookup_t pl_DCT_tab3[16] =
    {
        {10,2,13}, {9,2,13}, {5,3,13}, {3,4,13},
        {2,5,13}, {1,7,13}, {1,6,13}, {0,15,13},
        {0,14,13}, {0,13,13}, {0,12,13}, {26,1,13},
        {25,1,13}, {24,1,13}, {23,1,13}, {22,1,13}
    };

    /* Table B-14/15, DCT coefficients table zero / one,
     * codes 00000000010000 ... 00000000011111
     */
static dct_lookup_t pl_DCT_tab4[16] =
    {
        {0,31,14}, {0,30,14}, {0,29,14}, {0,28,14},
        {0,27,14}, {0,26,14}, {0,25,14}, {0,24,14},
        {0,23,14}, {0,22,14}, {0,21,14}, {0,20,14},
        {0,19,14}, {0,18,14}, {0,17,14}, {0,16,14}
    };

    /* Table B-14/15, DCT coefficients table zero / one,
     *   codes 000000000010000 ... 000000000011111
     */
static dct_lookup_t pl_DCT_tab5[16] =
    {
    {0,40,15}, {0,39,15}, {0,38,15}, {0,37,15},
    {0,36,15}, {0,35,15}, {0,34,15}, {0,33,15},
    {0,32,15}, {1,14,15}, {1,13,15}, {1,12,15},
    {1,11,15}, {1,10,15}, {1,9,15}, {1,8,15}
    };

    /* Table B-14/15, DCT coefficients table zero / one,
     * codes 0000000000010000 ... 0000000000011111
     */
static dct_lookup_t pl_DCT_tab6[16] =
    {
        {1,18,16}, {1,17,16}, {1,16,16}, {1,15,16},
        {6,3,16}, {16,2,16}, {15,2,16}, {14,2,16},
        {13,2,16}, {12,2,16}, {11,2,16}, {31,1,16},
        {30,1,16}, {29,1,16}, {28,1,16}, {27,1,16}
    };
/*
 * Initialization of lookup tables
 */

/*****************************************************************************
 * vpar_InitCrop : Initialize the crop table for saturation
 *                 (ISO/IEC 13818-2 section 7.4.3)
 *****************************************************************************/
#if defined(MPEG2_COMPLIANT) && !defined(VDEC_DFT)
void vpar_InitCrop( vpar_thread_t * p_vpar )
{
    int i_dummy;

    p_vpar->pi_crop = p_vpar->pi_crop_buf + 4096;

    for( i_dummy = -4096; i_dummy < -2048; i_dummy++ )
    {
        p_vpar->pi_crop[i_dummy] = -2048;
    }
    for( ; i_dummy < 2047; i_dummy++ )
    {
        p_vpar->pi_crop[i_dummy] = i_dummy;
    }
    for( ; i_dummy < 4095; i_dummy++ )
    {
        p_vpar->pi_crop[i_dummy] = 2047;
    }
}
#endif

/*****************************************************************************
 * InitMbAddrInc : Initialize the lookup table for mb_addr_inc
 *****************************************************************************/

/* Function for filling up the lookup table for mb_addr_inc */
static void __inline__ FillMbAddrIncTable( vpar_thread_t * p_vpar,
                                    int i_start, int i_end, int i_step, 
                                    int * pi_value, int i_length )
{
    int i_pos, i_offset;
    for( i_pos = i_start ; i_pos < i_end ; i_pos += i_step )
    {
        for( i_offset = 0 ; i_offset < i_step ; i_offset ++ )
        {
            p_vpar->pl_mb_addr_inc[i_pos + i_offset].i_value = * pi_value;
            p_vpar->pl_mb_addr_inc[i_pos + i_offset].i_length = i_length;
        }
        (*pi_value)--;
    }
}
    
/* Function that initialize the table using the last one */
void vpar_InitMbAddrInc( vpar_thread_t * p_vpar )
{
    int i_dummy;
    int i_value;
    
    for( i_dummy = 0 ; i_dummy < 8 ; i_dummy++ )
    {
        p_vpar->pl_mb_addr_inc[i_dummy].i_value = MB_ERROR;
        p_vpar->pl_mb_addr_inc[i_dummy].i_length = 0;
    }

    p_vpar->pl_mb_addr_inc[8].i_value = MB_ADDRINC_ESCAPE;
    p_vpar->pl_mb_addr_inc[8].i_length = 11;

    for( i_dummy = 9 ; i_dummy < 15 ; i_dummy ++ ) 
    {
        p_vpar->pl_mb_addr_inc[i_dummy].i_value =  MB_ERROR;
        p_vpar->pl_mb_addr_inc[i_dummy].i_length = 0;
    }

    p_vpar->pl_mb_addr_inc[15].i_value = MB_ADDRINC_STUFFING;
    p_vpar->pl_mb_addr_inc[15].i_length = 11;

    for( i_dummy = 16; i_dummy < 24; i_dummy++ ) 
    {
        p_vpar->pl_mb_addr_inc[i_dummy].i_value =  MB_ERROR;
        p_vpar->pl_mb_addr_inc[i_dummy].i_length = 0;
    }
    
    i_value = 33;
    
    FillMbAddrIncTable( p_vpar, 24, 36, 1, &i_value, 11 );
    FillMbAddrIncTable( p_vpar, 36, 48, 2, &i_value, 10 );
    FillMbAddrIncTable( p_vpar, 48, 96, 8, &i_value, 8 );
    FillMbAddrIncTable( p_vpar, 96, 128, 16, &i_value, 7 );
    FillMbAddrIncTable( p_vpar, 128, 256, 64, &i_value, 5 );
    FillMbAddrIncTable( p_vpar, 256, 512, 128, &i_value, 4 );
    FillMbAddrIncTable( p_vpar, 512, 1024, 256, &i_value, 3 );
    FillMbAddrIncTable( p_vpar, 1024, 2048, 1024, &i_value, 1 );
}

/*****************************************************************************
 * Init*MBType : Initialize lookup table for the Macroblock type
 *****************************************************************************/

/* Fonction for filling up the tables */
static void __inline__ FillMBType( vpar_thread_t * p_vpar,
                                   int           i_mb_type,
                                   int           i_start,
                                   int           i_end,
                                   int           i_value,
                                   int           i_length )
{
    int i_dummy;
    
    for( i_dummy = i_start ; i_dummy < i_end ; i_dummy++ )
    {
        p_vpar->ppl_mb_type[i_mb_type][i_dummy].i_value = i_value;
        p_vpar->ppl_mb_type[i_mb_type][i_dummy].i_length = i_length;
    }
}

/* Fonction that fills the table for P MB_Type */
void vpar_InitPMBType( vpar_thread_t * p_vpar )
{
    FillMBType( p_vpar, 0, 32, 64, MB_MOTION_FORWARD|MB_PATTERN, 1 );
    FillMBType( p_vpar, 0, 16, 32, MB_PATTERN, 2 );
    FillMBType( p_vpar, 0, 8, 16, MB_MOTION_FORWARD, 3 );
    FillMBType( p_vpar, 0, 6, 8, MB_INTRA, 5 );
    FillMBType( p_vpar, 0, 4, 6, MB_QUANT|MB_MOTION_FORWARD|MB_PATTERN, 5 );
    FillMBType( p_vpar, 0, 2, 4, MB_QUANT|MB_PATTERN, 5 );
    p_vpar->ppl_mb_type[0][1].i_value = MB_QUANT|MB_INTRA;
    p_vpar->ppl_mb_type[0][1].i_length = 6;
    p_vpar->ppl_mb_type[0][0].i_value = MB_ERROR;
    p_vpar->ppl_mb_type[0][0].i_length = 0;
}

/* Fonction that fills the table for B MB_Type */
void vpar_InitBMBType( vpar_thread_t * p_vpar )
{
    FillMBType( p_vpar, 1, 48, 64, MB_MOTION_FORWARD
                                  |MB_MOTION_BACKWARD|MB_PATTERN, 2 );
    FillMBType( p_vpar, 1, 32, 48, MB_MOTION_FORWARD|MB_MOTION_BACKWARD, 2 );
    FillMBType( p_vpar, 1, 24, 32, MB_MOTION_BACKWARD|MB_PATTERN, 3 );
    FillMBType( p_vpar, 1, 16, 24, MB_MOTION_BACKWARD, 3 );
    FillMBType( p_vpar, 1, 12, 16, MB_MOTION_FORWARD|MB_PATTERN, 4 );
    FillMBType( p_vpar, 1, 8, 12, MB_MOTION_FORWARD, 4 );
    FillMBType( p_vpar, 1, 6, 8, MB_INTRA, 5 );
    FillMBType( p_vpar, 1, 4, 6, MB_QUANT|MB_MOTION_FORWARD
                                |MB_MOTION_BACKWARD|MB_PATTERN, 5 );
    p_vpar->ppl_mb_type[1][3].i_value = MB_QUANT|MB_MOTION_FORWARD|MB_PATTERN;
    p_vpar->ppl_mb_type[1][3].i_length = 6;
    p_vpar->ppl_mb_type[1][2].i_value = MB_QUANT|MB_MOTION_BACKWARD|MB_PATTERN;
    p_vpar->ppl_mb_type[1][2].i_length = 6;
    p_vpar->ppl_mb_type[1][1].i_value = MB_QUANT|MB_INTRA;
    p_vpar->ppl_mb_type[1][1].i_length = 6;
    p_vpar->ppl_mb_type[1][0].i_value =MB_ERROR;
    p_vpar->ppl_mb_type[1][0].i_length = 0;
}

/*****************************************************************************
 * InitCodedPattern : Initialize the lookup table for decoding 
 *                    coded block pattern
 *****************************************************************************/
void vpar_InitCodedPattern( vpar_thread_t * p_vpar )
{
    p_vpar->pl_coded_pattern = (lookup_t*) pl_coded_pattern_init_table;
}

/*****************************************************************************
 * InitDCT : Initialize tables giving the length of the dct coefficient
 *           from the vlc code
 *****************************************************************************/

/* First fonction for filling the table */
static void __inline__ FillDCTTable( dct_lookup_t * p_tab_dest, dct_lookup_t * p_tab_src,
                                     int i_step, int i_nb_elem, int i_offset )
{
    int i_dummy, i_dummy2;

    for( i_dummy=0 ; i_dummy < i_nb_elem ; i_dummy++ )
    {
        for( i_dummy2=0 ; i_dummy2 < i_step ; i_dummy2++ )
        {
            p_tab_dest[(i_dummy+i_offset)*i_step+i_dummy2] = p_tab_src[i_dummy];
        }
    }
}


/* Fonction that actually fills the table or create the pointers */
void vpar_InitDCTTables( vpar_thread_t * p_vpar )
{
    /* Tables are cut in two parts to reduce memory occupation */    
    p_vpar->pppl_dct_dc_size[0][0] = pl_dct_dc_lum_init_table_1;
    p_vpar->pppl_dct_dc_size[0][1] = pl_dct_dc_lum_init_table_2;
    p_vpar->pppl_dct_dc_size[1][0] = pl_dct_dc_chrom_init_table_1;
    p_vpar->pppl_dct_dc_size[1][1] = pl_dct_dc_chrom_init_table_2;

    memset( ppl_dct_coef[0], MB_ERROR, 16 );
    memset( ppl_dct_coef[1], MB_ERROR, 16 );
    
    /* For table B14 & B15, we have a pointer to tables */
    /* We fill the table thanks to the fonction defined above */
    FillDCTTable( ppl_dct_coef[0], pl_DCT_tab0, 256, 60,  4 );
    FillDCTTable( ppl_dct_coef[0], pl_DCT_tab1,  64,  8,  8 );
    FillDCTTable( ppl_dct_coef[0], pl_DCT_tab2,  16, 16, 16 );
    FillDCTTable( ppl_dct_coef[0], pl_DCT_tab3,   8, 16, 16 );
    FillDCTTable( ppl_dct_coef[0], pl_DCT_tab4,   4, 16, 16 );
    FillDCTTable( ppl_dct_coef[0], pl_DCT_tab5,   2, 16, 16 );
    FillDCTTable( ppl_dct_coef[0], pl_DCT_tab6,   1, 16, 16 );

    FillDCTTable( ppl_dct_coef[1], pl_DCT_tab0a, 256, 60, 4 );
    FillDCTTable( ppl_dct_coef[1], pl_DCT_tab1a,  64,  8,  8 );
    FillDCTTable( ppl_dct_coef[1], pl_DCT_tab2,   16, 16, 16 );
    FillDCTTable( ppl_dct_coef[1], pl_DCT_tab3,    8, 16, 16 );
    FillDCTTable( ppl_dct_coef[1], pl_DCT_tab4,    4, 16, 16 );
    FillDCTTable( ppl_dct_coef[1], pl_DCT_tab5,    2, 16, 16 );
    FillDCTTable( ppl_dct_coef[1], pl_DCT_tab6,    1, 16, 16 );
}

/*
 * Macroblock parsing functions
 */

/*****************************************************************************
 * InitMacroblock : Initialize macroblock values
 *****************************************************************************/
static __inline__ void InitMacroblock( vpar_thread_t * p_vpar,
                                       macroblock_t * p_mb )
{
    static f_chroma_motion_t pf_chroma_motion[4] =
    { NULL, vdec_Motion420, vdec_Motion422, vdec_Motion444 };

    p_mb->p_picture = p_vpar->picture.p_picture;
    p_mb->i_structure = p_vpar->picture.i_structure;
    p_mb->i_current_structure = p_vpar->picture.i_current_structure;
    p_mb->i_l_x = p_vpar->mb.i_l_x;
    p_mb->i_l_y = p_vpar->mb.i_l_y;
    p_mb->i_c_x = p_vpar->mb.i_c_x;
    p_mb->i_c_y = p_vpar->mb.i_c_y;
    p_mb->i_chroma_nb_blocks = p_vpar->sequence.i_chroma_nb_blocks;
    p_mb->pf_chroma_motion = pf_chroma_motion[p_vpar->sequence.i_chroma_format];
    p_mb->b_P_coding_type = ( p_vpar->picture.i_coding_type == P_CODING_TYPE );

    if( (p_vpar->picture.i_coding_type == P_CODING_TYPE) ||
        (p_vpar->picture.i_coding_type == B_CODING_TYPE) )
       p_mb->p_forward = p_vpar->sequence.p_forward;
    if( p_vpar->picture.i_coding_type == B_CODING_TYPE )
        p_mb->p_backward = p_vpar->sequence.p_backward;

    p_mb->i_addb_l_stride = (p_mb->i_l_stride = p_vpar->picture.i_l_stride) - 8;
    p_mb->i_addb_c_stride = (p_mb->i_c_stride = p_vpar->picture.i_c_stride) - 8;

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
    int i_addr_inc = 0;
    /* Index in the lookup table mb_addr_inc */
    int    i_index = ShowBits( &p_vpar->bit_stream, 11 );
        
    /* Test the presence of the escape character */
    while( i_index == 8 )
    {
        RemoveBits( &p_vpar->bit_stream, 11 );
        i_addr_inc += 33;
        i_index = ShowBits( &p_vpar->bit_stream, 11 );
    }
     
    /* Affect the value from the lookup table */
    i_addr_inc += p_vpar->pl_mb_addr_inc[i_index].i_value;
    
    /* Dump the good number of bits */
    RemoveBits( &p_vpar->bit_stream, p_vpar->pl_mb_addr_inc[i_index].i_length );
    
    return i_addr_inc;
}

/*****************************************************************************
 * MacroblockModes : Get the macroblock_modes structure
 *****************************************************************************/
static __inline__ void MacroblockModes( vpar_thread_t * p_vpar,
                                        macroblock_t * p_mb )
{
    static f_motion_t   pf_motion[2][4] =
        { {NULL, vdec_MotionFieldField, vdec_MotionField16x8, vdec_MotionFieldDMV},
          {NULL, vdec_MotionFrameField, vdec_MotionFrameFrame, vdec_MotionFrameDMV} };
    static int          ppi_mv_count[2][4] = { {0, 1, 2, 1}, {0, 2, 1, 1} };
    static int          ppi_mv_format[2][4] = { {0, 1, 1, 1}, {0, 1, 2, 1} };

    /* Get macroblock_type. */
    p_vpar->mb.i_mb_type = (p_vpar->picture.pf_macroblock_type)( p_vpar );
    p_mb->i_mb_type = p_vpar->mb.i_mb_type;
    
    /* SCALABILITY : warning, we don't know if spatial_temporal_weight_code
     * has to be dropped, take care if you use scalable streams. */
    /* RemoveBits( &p_vpar->bit_stream, 2 ); */
    
    if( !(p_vpar->mb.i_mb_type & (MB_MOTION_FORWARD | MB_MOTION_BACKWARD)) )
    {
        /* If mb_type has neither MOTION_FORWARD nor MOTION_BACKWARD, this
         * is useless, but also harmless. */
        p_vpar->mb.i_motion_type = MOTION_FRAME;
    }
    else
    {
        if( p_vpar->picture.i_structure == FRAME_STRUCTURE
            && p_vpar->picture.b_frame_pred_frame_dct )
        {
            p_vpar->mb.i_motion_type = MOTION_FRAME;
        }
        else
        {
            p_vpar->mb.i_motion_type = GetBits( &p_vpar->bit_stream, 2 );
        }
    }
   
    if( p_mb->b_P_coding_type && !(p_vpar->mb.i_mb_type & (MB_MOTION_FORWARD|MB_INTRA)) )
    {
        /* Special No-MC macroblock in P pictures (7.6.3.5). */
        memset( p_vpar->slice.pppi_pmv, 0, 8*sizeof(int) );
        memset( p_mb->pppi_motion_vectors, 0, 8*sizeof(int) );
        
        p_vpar->mb.i_motion_type = MOTION_FRAME;
        p_mb->ppi_field_select[0][0] = ( p_vpar->picture.i_current_structure == BOTTOM_FIELD );
    }

     if( p_vpar->mb.i_mb_type & MB_INTRA )
    {
        /* For the intra macroblocks, we use an empty motion
         * compensation function */
        p_mb->pf_motion = vdec_MotionDummy;
    }
    else
    {
        p_mb->pf_motion = pf_motion[p_vpar->picture.b_frame_structure]
                                   [p_vpar->mb.i_motion_type];
    }
    p_vpar->mb.i_mv_count = ppi_mv_count[p_vpar->picture.b_frame_structure]
                                        [p_vpar->mb.i_motion_type];
    p_vpar->mb.i_mv_format = ppi_mv_format[p_vpar->picture.b_frame_structure]
                                          [p_vpar->mb.i_motion_type];

    p_vpar->mb.b_dct_type = 0;
    if( (p_vpar->picture.i_structure == FRAME_STRUCTURE) &&
        (!p_vpar->picture.b_frame_pred_frame_dct) &&
        (p_vpar->mb.i_mb_type & (MB_PATTERN|MB_INTRA)) )
    {
        if( (p_vpar->mb.b_dct_type = GetBits( &p_vpar->bit_stream, 1 )) )
        {
            /* The DCT is coded on fields. Jump one line between each
             * sample. */
            p_mb->i_addb_l_stride <<= 1;
            p_mb->i_addb_l_stride += 8;
            /* With CHROMA_420, the DCT is necessarily frame-coded. */
            if( p_vpar->sequence.i_chroma_format != CHROMA_420 )
            {
                p_mb->i_addb_c_stride <<= 1;
                p_mb->i_addb_c_stride += 8;
            }
        }
    }
    p_vpar->mb.b_dmv = p_vpar->mb.i_motion_type == MOTION_DMV;
}
     
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

    int             i_mb, i_b, i_mask;
    macroblock_t *  p_mb;
    f_addb_t        pf_addb;
    yuv_data_t *    p_data1;
    yuv_data_t *    p_data2;

    /************* DEBUG *************/
    int i_inc;
    static int i_count;
i_count++;

    i_inc = MacroblockAddressIncrement( p_vpar );
    *pi_mb_address += i_inc;
    //*pi_mb_address += MacroblockAddressIncrement( p_vpar );

    for( i_mb = i_mb_previous + 1; i_mb < *pi_mb_address; i_mb++ )
    {
        /* Skipped macroblock (ISO/IEC 13818-2 7.6.6). */
        static int          pi_dc_dct_reinit[4] = {128,256,512,1024};
        static f_motion_t   pf_motion_skipped[4] = {NULL, vdec_MotionFieldField,
                                vdec_MotionFieldField, vdec_MotionFrameFrame};
fprintf(stderr, "On sauuuute !\n");
        /* Reset DC predictors (7.2.1). */
        p_vpar->slice.pi_dc_dct_pred[0] = p_vpar->slice.pi_dc_dct_pred[1]
            = p_vpar->slice.pi_dc_dct_pred[2]
            = pi_dc_dct_reinit[p_vpar->picture.i_intra_dc_precision];

        if( p_vpar->picture.i_coding_type == P_CODING_TYPE )
        {
            /* Reset motion vector predictors (ISO/IEC 13818-2 7.6.3.4). */
            memset( p_vpar->slice.pppi_pmv, 0, 8*sizeof(int) );
        }

        if( (p_mb = p_vpar->picture.pp_mb[i_mb_base + i_mb] =
             vpar_NewMacroblock( &p_vpar->vfifo )) == NULL )
        {
            p_vpar->picture.b_error = 1;
            intf_ErrMsg("vpar error: macroblock list is empty !\n");
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
        p_mb->i_mb_type = MB_MOTION_FORWARD;

        /* Set the field we use for motion compensation */
        p_mb->ppi_field_select[0][0] = p_mb->ppi_field_select[0][1]
                                     = ( p_vpar->picture.i_current_structure == BOTTOM_FIELD );
    }

    /* Get a macroblock structure. */
    if( (p_mb = p_vpar->picture.pp_mb[i_mb_base + *pi_mb_address] =
         vpar_NewMacroblock( &p_vpar->vfifo )) == NULL )
    {
        p_vpar->picture.b_error = 1;
        intf_ErrMsg("vpar error: macroblock list is empty !\n");
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
//fprintf( stderr, "motion !\n" );
        (*p_vpar->sequence.pf_decode_mv)( p_vpar, p_mb, 0 );
    }

    if( p_vpar->mb.i_mb_type & MB_MOTION_BACKWARD )
    {
//fprintf( stderr, "motion2 !\n" );    
        (*p_vpar->sequence.pf_decode_mv)( p_vpar, p_mb, 1 );
    }

    if( p_vpar->picture.b_concealment_mv && (p_vpar->mb.i_mb_type & MB_INTRA) )
    {
        RemoveBits( &p_vpar->bit_stream, 1 );
    }
if( 0 )        
    //i_count == 1231 &&
    // i_count != *pi_mb_address)
    //p_vpar->picture.i_coding_type == P_CODING_TYPE )
{
    fprintf( stderr, "i_count = %d (%d)\n", i_count, p_vpar->mb.i_mb_type );
     fprintf( stderr, "%x", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x ", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x\n", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x ", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x\n", GetBits( &p_vpar->bit_stream, 16 ) );
     exit(0);
}

    if( p_vpar->mb.i_mb_type & MB_PATTERN )
    {
        p_vpar->mb.i_coded_block_pattern = (*p_vpar->sequence.pf_decode_pattern)( p_vpar );
//fprintf( stderr, "pattern : %d\n", p_vpar->mb.i_coded_block_pattern );
    }
    else
    {
        int     pi_coded_block_pattern[2] = {0,
                    (1 << (4+p_vpar->sequence.i_chroma_nb_blocks)) - 1};
        p_vpar->mb.i_coded_block_pattern = pi_coded_block_pattern
                                    [p_vpar->mb.i_mb_type & MB_INTRA];
    }
    pf_addb = ppf_addb_intra[p_vpar->mb.i_mb_type & MB_INTRA];

    /*
     * Effectively decode blocks.
     */

    i_mask = 1 << (3 + p_vpar->sequence.i_chroma_nb_blocks);

    /* luminance */
    p_data1 = p_mb->p_picture->p_y
              + p_mb->i_l_x + p_mb->i_l_y*(p_vpar->sequence.i_width);

    for( i_b = 0 ; i_b < 4 ; i_b++, i_mask >>= 1 )
    {
        if( p_vpar->mb.i_coded_block_pattern & i_mask )
        {
            memset( p_mb->ppi_blocks[i_b], 0, 64*sizeof(dctelem_t) );
            (*pppf_decode_block[p_vpar->sequence.b_mpeg2]
                               [p_vpar->mb.i_mb_type & MB_INTRA])
                ( p_vpar, p_mb, i_b );
        
            /* decode_block has already set pf_idct and pi_sparse_pos. */
            p_mb->pf_addb[i_b] = pf_addb;
     
            /* Calculate block coordinates. */
            p_mb->p_data[i_b] = p_data1
                                + pi_y[p_vpar->mb.b_dct_type][i_b]
                                * p_vpar->sequence.i_width
                                + pi_x[i_b];
        }
        else
        {
            /* Block not coded, so no IDCT, nor AddBlock */
            p_mb->pf_addb[i_b] = vdec_DummyBlock;
            p_mb->pf_idct[i_b] = vdec_DummyIDCT;
        }
    }

    /* chrominance */
    p_data1 = p_mb->p_picture->p_u
              + p_mb->i_c_x
              + p_mb->i_c_y
                * (p_vpar->sequence.i_chroma_width);
    p_data2 = p_mb->p_picture->p_v
               + p_mb->i_c_x
               + p_mb->i_c_y
                * (p_vpar->sequence.i_chroma_width);
    
    for( i_b = 4; i_b < 4 + p_vpar->sequence.i_chroma_nb_blocks;
         i_b++, i_mask >>= 1 )
    {
        yuv_data_t *    pp_data[2] = {p_data1, p_data2};

        if( p_vpar->mb.i_coded_block_pattern & i_mask )
        {
            memset( p_mb->ppi_blocks[i_b], 0, 64*sizeof(dctelem_t) );
            (*pppf_decode_block[p_vpar->sequence.b_mpeg2]
                               [p_vpar->mb.i_mb_type & MB_INTRA])
                ( p_vpar, p_mb, i_b );

            /* decode_block has already set pf_idct and pi_sparse_pos. */
            p_mb->pf_addb[i_b] = pf_addb;

            /* Calculate block coordinates. */
            p_mb->p_data[i_b] = pp_data[i_b & 1]
                                 + pi_y[p_vpar->mb.b_dct_type][i_b]
                                   * p_vpar->sequence.i_chroma_width
                                 + pi_x[i_b];
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
        memset( p_vpar->slice.pppi_pmv, 0, 8*sizeof(int) );
    }

    if( p_mb->b_P_coding_type && !(p_vpar->mb.i_mb_type & (MB_MOTION_FORWARD|MB_INTRA)) )
    {
        p_mb->i_mb_type |= MB_MOTION_FORWARD;
    }
    
 if( 0 )        
    //i_count == 249)
    // i_count != *pi_mb_address)
    //b_stop )
{
    fprintf( stderr, "i_count = %d (%d)\n", i_count, i_inc );
     fprintf( stderr, "%x", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x ", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x\n", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x ", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x", GetBits( &p_vpar->bit_stream, 16 ) );
     fprintf( stderr, "%x\n", GetBits( &p_vpar->bit_stream, 16 ) );
     exit(0);
}
}

/*****************************************************************************
 * vpar_IMBType : macroblock_type in I pictures
 *****************************************************************************/
int vpar_IMBType( vpar_thread_t * p_vpar )
{
    /* Take two bits for testing */
    int                 i_type = ShowBits( &p_vpar->bit_stream, 2 );
    
    /* Lookup table for macroblock_type */
    static lookup_t     pl_mb_Itype[4] = { {MB_ERROR, 0},
                                           {MB_QUANT|MB_INTRA, 2},
                                           {MB_INTRA, 1},
                                           {MB_INTRA, 1} };
    /* Dump the good number of bits */
    RemoveBits( &p_vpar->bit_stream, pl_mb_Itype[i_type].i_length );
    return pl_mb_Itype[i_type].i_value;
}

/*****************************************************************************
 * vpar_PMBType : macroblock_type in P pictures
 *****************************************************************************/
int vpar_PMBType( vpar_thread_t * p_vpar )
{
    /* Testing on 6 bits */
    int                i_type = ShowBits( &p_vpar->bit_stream, 6 );
    
#if 0   
/* Table B-3, macroblock_type in P-pictures, codes 001..1xx */
static lookup_t PMBtab0[8] = {
  {-1,0},
  {MB_MOTION_FORWARD,3},
  {MB_PATTERN,2}, {MB_PATTERN,2},
  {MB_MOTION_FORWARD|MB_PATTERN,1}, 
  {MB_MOTION_FORWARD|MB_PATTERN,1},
  {MB_MOTION_FORWARD|MB_PATTERN,1}, 
  {MB_MOTION_FORWARD|MB_PATTERN,1}
};

/* Table B-3, macroblock_type in P-pictures, codes 000001..00011x */
static lookup_t PMBtab1[8] = {
  {-1,0},
  {MB_QUANT|MB_INTRA,6},
  {MB_QUANT|MB_PATTERN,5}, {MB_QUANT|MB_PATTERN,5},
  {MB_QUANT|MB_MOTION_FORWARD|MB_PATTERN,5}, {MB_QUANT|MB_MOTION_FORWARD|MB_PATTERN,5},
  {MB_INTRA,5}, {MB_INTRA,5}
};


  if(i_type >= 8)      
  {
    i_type >>= 3;
    RemoveBits( &p_vpar->bit_stream,PMBtab0[i_type].i_length );
    return PMBtab0[i_type].i_value;
  }

  if (i_type==0)
  {
      printf("Invalid P macroblock_type code\n");
    return -1;
  }
    RemoveBits( &p_vpar->bit_stream,PMBtab1[i_type].i_length );
 return PMBtab1[i_type].i_value;
#endif
    /* Dump the good number of bits */
    RemoveBits( &p_vpar->bit_stream, p_vpar->ppl_mb_type[0][i_type].i_length );
    /* return the value from the lookup table for P type */
    return p_vpar->ppl_mb_type[0][i_type].i_value;
}

/*****************************************************************************
 * vpar_BMBType : macroblock_type in B pictures
 *****************************************************************************/
int vpar_BMBType( vpar_thread_t * p_vpar )
{
     /* Testing on 6 bits */
    int                i_type = ShowBits( &p_vpar->bit_stream, 6 );
    
    /* Dump the good number of bits */
    RemoveBits( &p_vpar->bit_stream, p_vpar->ppl_mb_type[1][i_type].i_length );
    
    /* return the value from the lookup table for B type */
    return p_vpar->ppl_mb_type[1][i_type].i_value;
}

/*****************************************************************************
 * vpar_DMBType : macroblock_type in D pictures
 *****************************************************************************/
int vpar_DMBType( vpar_thread_t * p_vpar )
{
    /* Taking 1 bit */
    int               i_type = GetBits( &p_vpar->bit_stream, 1 );
    
    /* Lookup table */
    static int        pi_mb_Dtype[2] = { MB_ERROR, 1 };
    
    return pi_mb_Dtype[i_type];
}

/*****************************************************************************
 * vpar_CodedPattern420 : coded_block_pattern with 420 chroma
 *****************************************************************************/
int vpar_CodedPattern420( vpar_thread_t * p_vpar )
{
    /* Take the max 9 bits length vlc code for testing */
    int      i_vlc = ShowBits( &p_vpar->bit_stream, 9 );
    
    /* Trash the good number of bits read in  the lookup table */
    RemoveBits( &p_vpar->bit_stream, p_vpar->pl_coded_pattern[i_vlc].i_length );
    
    /* return the value from the vlc table */
    return p_vpar->pl_coded_pattern[i_vlc].i_value;
}

/*****************************************************************************
 * vpar_CodedPattern422 : coded_block_pattern with 422 chroma
 *****************************************************************************/
int vpar_CodedPattern422( vpar_thread_t * p_vpar )
{
    int      i_vlc = ShowBits( &p_vpar->bit_stream, 9 );
    
    /* Supplementary 2 bits long code for 422 format */
    int      i_coded_block_pattern_1;
    
    RemoveBits( &p_vpar->bit_stream, p_vpar->pl_coded_pattern[i_vlc].i_length );
    i_coded_block_pattern_1 = GetBits( &p_vpar->bit_stream, 2 );
    
    /* the code is just to be added to the value found in the table */
    return p_vpar->pl_coded_pattern[i_vlc].i_value |
           (i_coded_block_pattern_1 << 6);
}

/*****************************************************************************
 * vpar_CodedPattern444 : coded_block_pattern with 444 chroma
 *****************************************************************************/
int vpar_CodedPattern444( vpar_thread_t * p_vpar )
{
    int      i_vlc = ShowBits( &p_vpar->bit_stream, 9 );
    int      i_coded_block_pattern_2;
    
    RemoveBits( &p_vpar->bit_stream, p_vpar->pl_coded_pattern[i_vlc].i_length );
    i_coded_block_pattern_2 = GetBits( &p_vpar->bit_stream, 6 );
    
    return p_vpar->pl_coded_pattern[i_vlc].i_value |
           ( i_coded_block_pattern_2 << 6 );
}

/*****************************************************************************
 * vpar_DecodeMPEG1Non : decode MPEG-1 non-intra blocks
 *****************************************************************************/
static void vpar_DecodeMPEG1Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b )
{

    if( p_vpar->picture.i_coding_type == D_CODING_TYPE )
    {
        /* Remove end_of_macroblock (always 1, prevents startcode emulation)
         * ISO/IEC 11172-2 section 2.4.2.7 and 2.4.3.6 */
        RemoveBits( &p_vpar->bit_stream, 1 );
    }
}

/*****************************************************************************
 * vpar_DecodeMPEG1Intra : decode MPEG-1 intra blocks
 *****************************************************************************/
static void vpar_DecodeMPEG1Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b )
{

    if( p_vpar->picture.i_coding_type == D_CODING_TYPE )
    {
        /* Remove end_of_macroblock (always 1, prevents startcode emulation)
         * ISO/IEC 11172-2 section 2.4.2.7 and 2.4.3.6 */
        RemoveBits( &p_vpar->bit_stream, 1 );
    }
}

/*****************************************************************************
 * vpar_DecodeMPEG2Non : decode MPEG-2 non-intra blocks
 *****************************************************************************/
static void vpar_DecodeMPEG2Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b )
{
    int         i_parse;
    int         i_nc;
    int         i_cc;
    int         i_coef;
    int         i_type;
    int         i_code;
    int         i_length;
    int         i_pos;
    int         i_run;
    int         i_level;
    boolean_t   b_sign;
    int *       ppi_quant[2];
    
    /* Lookup Table for the chromatic component */
    static int pi_cc_index[12] = { 0, 0, 0, 0, 1, 2, 1, 2, 1, 2 };

    i_cc = pi_cc_index[i_b];

    /* Determine whether it is luminance or not (chrominance) */
    i_type = ( i_cc + 1 ) >> 1;

    /* Give a pointer to the quantization matrices for intra blocks */
    ppi_quant[0] = p_vpar->sequence.nonintra_quant.pi_matrix;
    ppi_quant[1] = p_vpar->sequence.chroma_nonintra_quant.pi_matrix;

    /* Decoding of the AC coefficients */
    
    i_nc = 0;
    i_coef = 0;
    for( i_parse = 0; ; i_parse++ )
    {
        i_code = ShowBits( &p_vpar->bit_stream, 16 );
        if( i_code >= 16384 )
        {
            if( i_parse == 0 )
            {
                i_run =     pl_DCT_tab_dc[(i_code>>12)-4].i_run;
                i_level =   pl_DCT_tab_dc[(i_code>>12)-4].i_level;
                i_length =  pl_DCT_tab_dc[(i_code>>12)-4].i_length;
            }
            else
            {
                i_run =     pl_DCT_tab_ac[(i_code>>12)-4].i_run;
                i_level =   pl_DCT_tab_ac[(i_code>>12)-4].i_level;
                i_length =  pl_DCT_tab_ac[(i_code>>12)-4].i_length;
             }
        }
        else if( i_code >= 1024 )
        {
            i_run =     pl_DCT_tab0[(i_code>>8)-4].i_run;
            i_length =  pl_DCT_tab0[(i_code>>8)-4].i_length;
            i_level =   pl_DCT_tab0[(i_code>>8)-4].i_level;            
        }
        else
        {
            i_run =     ppl_dct_coef[0][i_code].i_run;
            i_length =  ppl_dct_coef[0][i_code].i_length;
            i_level =   ppl_dct_coef[0][i_code].i_level;
        }

        
        RemoveBits( &p_vpar->bit_stream, i_length );

        switch( i_run )
        {
            case DCT_ESCAPE:
                i_run = GetBits( &p_vpar->bit_stream, 6 );
                i_level = GetBits( &p_vpar->bit_stream, 12 );
                i_level = (b_sign = ( i_level > 2047 )) ? 4096 - i_level
                                                        : i_level;
                break;
            case DCT_EOB:
                if( i_nc <= 1 )
                {
                    p_mb->pf_idct[i_b] = vdec_SparseIDCT;
                    p_mb->pi_sparse_pos[i_b] = i_coef;
                }
                else
                {
                    p_mb->pf_idct[i_b] = vdec_IDCT;
                }
                return;

                break;
            default:
                b_sign = GetBits( &p_vpar->bit_stream, 1 );
        }
        i_coef = i_parse;
        i_parse += i_run;
        i_nc ++;

if( i_parse >= 64 )
{
    break;
}
        
        i_pos = pi_scan[p_vpar->picture.b_alternate_scan][i_parse];
        i_level = ( i_level *
                    p_vpar->slice.i_quantizer_scale *
                    ppi_quant[i_type][i_pos] ) >> 5;
        p_mb->ppi_blocks[i_b][i_pos] = b_sign ? -i_level : i_level;
    }
fprintf( stderr, "Non intra MPEG2 end (%d)\n", i_b );
exit(0);
//p_vpar->picture.b_error = 1;
}

/*****************************************************************************
 * vpar_DecodeMPEG2Intra : decode MPEG-2 intra blocks
 *****************************************************************************/
static void vpar_DecodeMPEG2Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b )
{
    int         i_parse;
    int         i_nc;
    int         i_cc;
    int         i_coef;
    int         i_type, i_quant_type;
    int         i_code;
    int         i_length;
    int         i_pos;
    int         i_dct_dc_size;
    int         i_dct_dc_diff;
    int         i_run;
    int         i_level;
    boolean_t   b_vlc_intra;
    boolean_t   b_sign;
    int *       ppi_quant[2];
    
    /* Lookup Table for the chromatic component */
    static int pi_cc_index[12] = { 0, 0, 0, 0, 1, 2, 1, 2, 1, 2 };
    i_cc = pi_cc_index[i_b];

    /* Determine whether it is luminance or not (chrominance) */
    i_type = ( i_cc + 1 ) >> 1;
    i_quant_type = i_type | (p_vpar->sequence.i_chroma_format == CHROMA_420);

    /* Give a pointer to the quantization matrices for intra blocks */
    ppi_quant[0] = p_vpar->sequence.intra_quant.pi_matrix;
    ppi_quant[1] = p_vpar->sequence.chroma_intra_quant.pi_matrix;

#if 0    
    /* Decoding of the DC intra coefficient */
    /* The nb of bits to parse depends on i_type */
    i_code = ShowBits( &p_vpar->bit_stream, 9 + i_type );
    
    /* To reduce memory occupation, there are two lookup tables
     * See InitDCT above */
    i_code5 = i_code >> (4+i_type);
    
    /* Shall we lookup in the first or in the second table ? */
    i_select = ( i_code5 == 31 );
    /* Offset value for looking in the second table */
    i_offset = 0x1f0 + ( i_type * 0x1f0 );
    i_pos = ( i_code5 * ( ! i_select ) ) + ( ( i_code - i_offset ) * i_select );
    i_dct_dc_size = p_vpar->pppl_dct_dc_size[i_type][i_select][i_pos].i_value;
#endif
    
    if( !i_type/*i_b < 4*/ )
    {
        /* decode length */
        i_code = ShowBits(&p_vpar->bit_stream, 5);
        if (i_code<31)
        {
            i_dct_dc_size = pl_dct_dc_lum_init_table_1[i_code].i_value;
            i_length = pl_dct_dc_lum_init_table_1[i_code].i_length;
            RemoveBits( &p_vpar->bit_stream, i_length);
        }
        else
        {
            i_code = ShowBits(&p_vpar->bit_stream, 9) - 0x1f0;
            i_dct_dc_size = pl_dct_dc_lum_init_table_2[i_code].i_value;
            i_length = pl_dct_dc_lum_init_table_2[i_code].i_length;
            RemoveBits( &p_vpar->bit_stream, i_length);
        }
    }
    else
    {
        /* decode length */
        i_code = ShowBits(&p_vpar->bit_stream, 5);

        if (i_code<31)
        {
            i_dct_dc_size = pl_dct_dc_chrom_init_table_1[i_code].i_value;
            i_length = pl_dct_dc_chrom_init_table_1[i_code].i_length;
            RemoveBits(&p_vpar->bit_stream, i_length);
        }
        else
        {
            i_code = ShowBits(&p_vpar->bit_stream, 10) - 0x3e0;
            i_dct_dc_size = pl_dct_dc_chrom_init_table_2[i_code].i_value;
            i_length = pl_dct_dc_chrom_init_table_2[i_code].i_length;
            RemoveBits( &p_vpar->bit_stream, i_length);
        }
    }
    if (i_dct_dc_size==0)
        i_dct_dc_diff = 0;
    else
    {
        i_dct_dc_diff = GetBits( &p_vpar->bit_stream, i_dct_dc_size);
        if ((i_dct_dc_diff & (1<<(i_dct_dc_size-1)))==0)
            i_dct_dc_diff-= (1<<i_dct_dc_size) - 1;
    }

    /* Dump the variable length code */
    //RemoveBits( &p_vpar->bit_stream, 
    //          p_vpar->pppl_dct_dc_size[i_type][i_select][i_pos].i_length );
    
    /* Read the actual code with the good length */
    p_vpar->slice.pi_dc_dct_pred[i_cc] += i_dct_dc_diff;

    p_mb->ppi_blocks[i_b][0] = ( p_vpar->slice.pi_dc_dct_pred[i_cc] <<
                               ( 3 - p_vpar->picture.i_intra_dc_precision ) );
    i_nc = ( p_vpar->slice.pi_dc_dct_pred[i_cc] != 0 );

//fprintf( stderr, "coucou\n" );
    /* Decoding of the AC coefficients */
    
    i_coef = 0;
    b_vlc_intra = p_vpar->picture.b_intra_vlc_format;
    for( i_parse = 1; /*i_parse < 64*/; i_parse++ )
    {
        i_code = ShowBits( &p_vpar->bit_stream, 16 );
        if( i_code >= 16384 )
        {
            if( b_vlc_intra )
            {
                i_run =     pl_DCT_tab0a[(i_code>>8)-4].i_run;
                i_level =   pl_DCT_tab0a[(i_code>>8)-4].i_level;
                i_length =  pl_DCT_tab0a[(i_code>>8)-4].i_length;
//fprintf( stderr, "**********> %d, %d, %d *******\n", i_run, i_level, (i_code>>8)-4 );
            }
            else
            {
                i_run =     pl_DCT_tab_ac[(i_code>>12)-4].i_run;
                i_level =   pl_DCT_tab_ac[(i_code>>12)-4].i_level;
                i_length =  pl_DCT_tab_ac[(i_code>>12)-4].i_length;
             }
        }
        else
        {
            i_run =     ppl_dct_coef[b_vlc_intra][i_code].i_run;
            i_length =  ppl_dct_coef[b_vlc_intra][i_code].i_length;
            i_level =   ppl_dct_coef[b_vlc_intra][i_code].i_level;
        }

#if 0
        {
            int code = i_code;
            int intra_vlc_format = b_vlc_intra;
            dct_lookup_t tab;
            
    if (code>=16384 && !intra_vlc_format)
      tab = pl_DCT_tab_ac[(code>>12)-4];
    else if (code>=1024)
    {
      if (intra_vlc_format)
        tab = pl_DCT_tab0a[(code>>8)-4];
      else
        tab = pl_DCT_tab0[(code>>8)-4];
    }
    else if (code>=512)
    {
      if (intra_vlc_format)
        tab = pl_DCT_tab1a[(code>>6)-8];
      else
        tab = pl_DCT_tab1[(code>>6)-8];
    }
    else if (code>=256)
      tab = pl_DCT_tab2[(code>>4)-16];
    else if (code>=128)
      tab = pl_DCT_tab3[(code>>3)-16];
    else if (code>=64)
      tab = pl_DCT_tab4[(code>>2)-16];
    else if (code>=32)
      tab = pl_DCT_tab5[(code>>1)-16];
    else if (code>=16)
      tab = pl_DCT_tab6[code-16];
    else
    {
       fprintf( stderr, "invalid Huffman code in Decode_MPEG2_Intra_Block()\n");
    }

    if( (i_run != tab.i_run) || (i_length != tab.i_length) || (i_level != tab.i_level) )
    {
        fprintf( stderr, "ET M....... !!!\n" );
        exit(0);
    }
        }
#endif



        
        RemoveBits( &p_vpar->bit_stream, i_length );

        switch( i_run )
        {
            case DCT_ESCAPE:
                i_run = GetBits( &p_vpar->bit_stream, 6 );
                i_level = GetBits( &p_vpar->bit_stream, 12 );
                /*p_mb->ppi_blocks[i_b][i_parse] = ( b_sign = ( i_level > 2047 ) ) 
                                                          ? ( -4096 + i_level )
                                                          : i_level;*/
                i_level = (b_sign = ( i_level > 2047 )) ? 4096 - i_level
                                                        : i_level;
                break;
            case DCT_EOB:
                if( i_nc <= 0 )
                {
                    p_mb->pf_idct[i_b] = vdec_SparseIDCT;
                    p_mb->pi_sparse_pos[i_b] = i_coef;
                }
                else
                {
                    p_mb->pf_idct[i_b] = vdec_IDCT;
                }
                return;

                break;
            default:
                b_sign = GetBits( &p_vpar->bit_stream, 1 );
                //p_mb->ppi_blocks[i_b][i_parse] = b_sign ? -i_level : i_level;
        }
// fprintf( stderr, "i_code : %d (%d), run : %d, %d, %d (%4x) ", i_code ,  b_vlc_intra,
//                  i_run, i_level, i_parse, ShowBits( &p_vpar->bit_stream, 16 ) );

//fprintf( stderr, "- %4x\n",ShowBits( &p_vpar->bit_stream, 16 ) ); 
if( i_parse >= 64 )
{
    fprintf( stderr, "Beuhh dans l'intra decode (%d)\n", i_b );
    break;
}
        i_coef = i_parse;
        i_parse += i_run;
        i_nc ++;
 
        i_pos = pi_scan[p_vpar->picture.b_alternate_scan][i_parse];
        i_level = ( i_level *
                    p_vpar->slice.i_quantizer_scale *
                    ppi_quant[i_quant_type][i_pos] ) >> 4;
        p_mb->ppi_blocks[i_b][i_pos] = b_sign ? -i_level : i_level;
    }
    
fprintf( stderr, "MPEG2 end (%d)\n", i_b );
exit(0);
//p_vpar->b_error = 1;
}
