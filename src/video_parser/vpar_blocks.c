/*****************************************************************************
 * vpar_blocks.c : blocks parsing
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vpar_blocks.c,v 1.75 2001/02/11 01:15:12 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Marc Dressler <polux@via.ecp.fr>
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
#include "defs.h"

#include <string.h>                                                /* memset */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "video.h"
#include "video_output.h"

#include "video_decoder.h"
#include "vdec_motion.h"
#include "../video_decoder/vdec_idct.h"

#include "vpar_blocks.h"
#include "../video_decoder/vpar_headers.h"
#include "../video_decoder/vpar_synchro.h"
#include "../video_decoder/video_parser.h"
#include "../video_decoder/video_fifo.h"

/*
 * Welcome to vpar_blocks.c ! Here's where the heavy processor-critical parsing
 * task is done. This file is divided in several parts :
 *  - Initialization of the lookup tables
 *  - Decoding of coded blocks
 *  - Decoding of motion vectors
 *  - Decoding of the other macroblock structures
 *  - Picture data parsing management (slices and error handling)
 * It's a pretty long file. Good luck and have a nice day.
 */


/*
 * Initialization tables
 */

    /* Table for coded_block_pattern resolution */
static lookup_t     pl_coded_pattern[512] =
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

    /* Table B-12, dct_dc_size_luminance, codes 00XXX ... 11110 */

static lookup_t     pl_dct_dc_lum_init_table_1[32] =
    { {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
      {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
      {0, 3}, {0, 3}, {0, 3}, {0, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3},
      {4, 3}, {4, 3}, {4, 3}, {4, 3}, {5, 4}, {5, 4}, {6, 5}, {MB_ERROR, 0}
    };

static lookup_t     ppl_dct_dc_init_table_1[2][32] =
{    { {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
      {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
      {0, 3}, {0, 3}, {0, 3}, {0, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3},
      {4, 3}, {4, 3}, {4, 3}, {4, 3}, {5, 4}, {5, 4}, {6, 5}, {MB_ERROR, 0}},
{ {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
      {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
      {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
      {3, 3}, {3, 3}, {3, 3}, {3, 3}, {4, 4}, {4, 4}, {5, 5}, {MB_ERROR, 0}
    }
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

static lookup_t     ppl_dct_dc_init_table_2[2][32] =
{    { {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6},
      {8, 7}, {8, 7}, {8, 7}, {8, 7}, {9, 8}, {9, 8}, {10,9}, {11,9},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}},
    { {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
      {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
      {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7},
      {8, 8}, {8, 8}, {8, 8}, {8, 8}, {9, 9}, {9, 9}, {10,10}, {11,10}
    }
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

    /* Pointers on tables of dct coefficients */
static dct_lookup_t * ppl_dct_tab1[2] = { pl_DCT_tab_ac, pl_DCT_tab0a };

static dct_lookup_t * ppl_dct_tab2[2] = { pl_DCT_tab_ac, pl_DCT_tab_dc };


/*
 * Initialization of lookup tables
 */

/*****************************************************************************
 * vpar_InitMbAddrInc : Initialize the lookup table for mb_addr_inc
 *****************************************************************************/

/* Function for filling up the lookup table for mb_addr_inc */
static void FillMbAddrIncTable( vpar_thread_t * p_vpar,
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
 * vpar_Init*MBType : Initialize lookup table for the Macroblock type
 *****************************************************************************/

/* Fonction for filling up the tables */
static void FillMBType( vpar_thread_t * p_vpar,
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
 * vpar_InitDCTTables : Initialize tables giving the length of the dct
 *                      coefficient from the vlc code
 *****************************************************************************/

/* First fonction for filling the table */
static void FillDCTTable( dct_lookup_t * p_tab_dest, dct_lookup_t * p_tab_src,
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

    /* XXX?? MB_ERROR is replaced by 0 because if we use -1 we
     * can block in DecodeMPEG2Intra and others */
    memset( p_vpar->ppl_dct_coef[0], 0, 16 );
    memset( p_vpar->ppl_dct_coef[1], 0, 16 );

    /* For table B14 & B15, we have a pointer to tables */
    /* We fill the table thanks to the fonction defined above */
    FillDCTTable( p_vpar->ppl_dct_coef[0], pl_DCT_tab0, 256, 60,  4 );
    FillDCTTable( p_vpar->ppl_dct_coef[0], pl_DCT_tab1,  64,  8,  8 );
    FillDCTTable( p_vpar->ppl_dct_coef[0], pl_DCT_tab2,  16, 16, 16 );
    FillDCTTable( p_vpar->ppl_dct_coef[0], pl_DCT_tab3,   8, 16, 16 );
    FillDCTTable( p_vpar->ppl_dct_coef[0], pl_DCT_tab4,   4, 16, 16 );
    FillDCTTable( p_vpar->ppl_dct_coef[0], pl_DCT_tab5,   2, 16, 16 );
    FillDCTTable( p_vpar->ppl_dct_coef[0], pl_DCT_tab6,   1, 16, 16 );

    FillDCTTable( p_vpar->ppl_dct_coef[1], pl_DCT_tab0a, 256, 60, 4 );
    FillDCTTable( p_vpar->ppl_dct_coef[1], pl_DCT_tab1a,  64,  8,  8 );
    FillDCTTable( p_vpar->ppl_dct_coef[1], pl_DCT_tab2,   16, 16, 16 );
    FillDCTTable( p_vpar->ppl_dct_coef[1], pl_DCT_tab3,    8, 16, 16 );
    FillDCTTable( p_vpar->ppl_dct_coef[1], pl_DCT_tab4,    4, 16, 16 );
    FillDCTTable( p_vpar->ppl_dct_coef[1], pl_DCT_tab5,    2, 16, 16 );
    FillDCTTable( p_vpar->ppl_dct_coef[1], pl_DCT_tab6,    1, 16, 16 );
}

/*****************************************************************************
 * vpar_InitScanTable : Initialize scan table
 *****************************************************************************/
void vpar_InitScanTable( vpar_thread_t * p_vpar )
{
    int     i;

    memcpy( p_vpar->ppi_scan, pi_scan, sizeof(pi_scan) );
    p_vpar->pf_norm_scan( p_vpar->ppi_scan );

    /* If scan table has changed, we must change the quantization matrices. */
    for( i = 0; i < 64; i++ )
    {
        p_vpar->pi_default_intra_quant[ p_vpar->ppi_scan[0][i] ] =
            pi_default_intra_quant[ pi_scan[0][i] ];
        p_vpar->pi_default_nonintra_quant[ p_vpar->ppi_scan[0][i] ] =
            pi_default_nonintra_quant[ pi_scan[0][i] ];
    }
}


/*
 * Block parsing
 */

#define SATURATE(val)                                                       \
    if( val > 2047 )                                                        \
        val = 2047;                                                         \
    else if( val < -2048 )                                                  \
        val = -2048;

/*****************************************************************************
 * DecodeMPEG1NonIntra : decode MPEG-1 non-intra blocks
 *****************************************************************************/
static __inline__ void DecodeMPEG1NonIntra( vpar_thread_t * p_vpar,
                                            macroblock_t * p_mb, int i_b,
                                            boolean_t b_chroma, int i_cc )
{
    int             i_parse;
    int             i_nc;
    int             i_coef;
    int             i_code;
    int             i_length;
    int             i_pos;
    int             i_run;
    int             i_level;
    boolean_t       b_sign;
    dct_lookup_t *  p_tab;

    /* There should be no D picture in non-intra blocks */
    if( p_vpar->picture.i_coding_type == D_CODING_TYPE )
        intf_ErrMsg("vpar error : D-picture in non intra block");
    
    /* Decoding of the AC coefficients */

    i_nc = 0;
    i_coef = 0;
    b_sign = 0;

    for( i_parse = 0; !p_vpar->p_fifo->b_die; i_parse++ )
    {
        i_code = ShowBits( &p_vpar->bit_stream, 16 );
        if( i_code >= 16384 )
        {
            p_tab = &ppl_dct_tab2[(i_parse == 0)][(i_code>>12)-4];
        }
        else
        {
            p_tab = &p_vpar->ppl_dct_coef[0][i_code];
        }
        i_run =     p_tab->i_run;
        i_level =   p_tab->i_level;
        i_length =  p_tab->i_length;

        RemoveBits( &p_vpar->bit_stream, i_length );

        switch( i_run )
        {
            case DCT_ESCAPE:
                i_run = GetBits( &p_vpar->bit_stream, 6 );
                i_level = GetBits( &p_vpar->bit_stream, 8 );
                if (i_level == 0)
                    i_level = GetBits( &p_vpar->bit_stream, 8 );
                else if (i_level == 128)
                    i_level = GetBits( &p_vpar->bit_stream, 8 ) - 256;
                else if (i_level > 128)
                    i_level -= 256;

                if( (b_sign = i_level < 0) )
                    i_level = -i_level;
                
                break;
            case DCT_EOB:
                if( i_nc <= 1 )
                {
                    p_mb->pf_idct[i_b] = p_vpar->pf_sparse_idct;
                    p_mb->pi_sparse_pos[i_b] = i_coef;
                }
                else
                {
                    p_mb->pf_idct[i_b] = p_vpar->pf_idct;
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

        i_pos = p_vpar->ppi_scan[p_vpar->picture.b_alternate_scan][i_parse];
        i_level = ( ((i_level << 1) + 1) * p_vpar->mb.i_quantizer_scale
                    * p_vpar->sequence.nonintra_quant.pi_matrix[i_pos] ) >> 4;

        /* Mismatch control */
        /* Equivalent to : if( (val & 1) == 0 ) val = val - 1; */
        i_level = (i_level - 1) | 1;

        i_level = b_sign ? -i_level : i_level;
        
        SATURATE( i_level );
        
        p_mb->ppi_blocks[i_b][i_pos] = i_level;
    }

    intf_ErrMsg("vpar error: DCT coeff (non-intra) is out of bounds");
    p_vpar->picture.b_error = 1;
}

/*****************************************************************************
 * DecodeMPEG1Intra : decode MPEG-1 intra blocks
 *****************************************************************************/
static __inline__ void DecodeMPEG1Intra( vpar_thread_t * p_vpar,
                                         macroblock_t * p_mb, int i_b,
                                         boolean_t b_chroma, int i_cc )
{
    int             i_parse;
    int             i_nc;
    int             i_coef;
    int             i_code;
    int             i_length;
    int             i_pos;
    int             i_dct_dc_size;
    int             i_dct_dc_diff;
    int             i_run;
    int             i_level;
    boolean_t       b_sign;
    dct_lookup_t *  p_tab;
    lookup_t *      p_lookup;
    u8 *            pi_quant = p_vpar->sequence.intra_quant.pi_matrix;

    /* decode length */
    i_code = ShowBits(&p_vpar->bit_stream, 5);

    if (i_code<31)
    {
        p_lookup = &ppl_dct_dc_init_table_1[b_chroma][i_code];
    }
    else
    {
        i_code = ShowBits(&p_vpar->bit_stream, (9+b_chroma)) - (0x1f0 * (b_chroma + 1));
        p_lookup = &ppl_dct_dc_init_table_2[b_chroma][i_code];
    }
    i_dct_dc_size = p_lookup->i_value;
    i_length = p_lookup->i_length;
    RemoveBits( &p_vpar->bit_stream, i_length );

    if (i_dct_dc_size == 0)
        i_dct_dc_diff = 0;
    else
    {
        i_dct_dc_diff = GetBits( &p_vpar->bit_stream, i_dct_dc_size);
        if ((i_dct_dc_diff & (1<<(i_dct_dc_size-1))) == 0)
            i_dct_dc_diff -= (1<<i_dct_dc_size) - 1;
    }

    /* Read the actual code with the good length */
    p_vpar->mb.pi_dc_dct_pred[i_cc] += i_dct_dc_diff;

    p_mb->ppi_blocks[i_b][0] = p_vpar->mb.pi_dc_dct_pred[i_cc] << 3;

    i_nc = ( p_vpar->mb.pi_dc_dct_pred[i_cc] != 0 );

    if( p_vpar->picture.i_coding_type == D_CODING_TYPE )
    {
        /* Remove end_of_macroblock (always 1, prevents startcode emulation)
         * ISO/IEC 11172-2 section 2.4.2.7 and 2.4.3.6 */
        RemoveBits( &p_vpar->bit_stream, 1 );
        /* D pictures do not have AC coefficients */
        return;
    }

    
    /* Decoding of the AC coefficients */
    i_coef = 0;
    b_sign = 0;

    for( i_parse = 1; !p_vpar->p_fifo->b_die; i_parse++ )
    {
        i_code = ShowBits( &p_vpar->bit_stream, 16 );
        /* We use 2 main tables for the coefficients */
        if( i_code >= 16384 )
        {
            p_tab = &ppl_dct_tab1[0][(i_code>>12)-4];
        }
        else
        {
            p_tab = &p_vpar->ppl_dct_coef[0][i_code];
        }
        i_run =     p_tab->i_run;
        i_length =  p_tab->i_length;
        i_level =   p_tab->i_level;

        RemoveBits( &p_vpar->bit_stream, i_length );

        switch( i_run )
        {
            case DCT_ESCAPE:
                i_run = GetBits( &p_vpar->bit_stream, 6 );
                i_level = GetBits( &p_vpar->bit_stream, 8 );
                if (i_level == 0)
                    i_level = GetBits( &p_vpar->bit_stream, 8 );
                else if (i_level == 128)
                    i_level = GetBits( &p_vpar->bit_stream, 8 ) - 256;
                else if (i_level > 128)
                    i_level -= 256;
                if( (b_sign = i_level < 0) )
                    i_level = -i_level;
                break;
            case DCT_EOB:
                if( i_nc <= 1 )
                {
                    p_mb->pf_idct[i_b] = p_vpar->pf_sparse_idct;
                    p_mb->pi_sparse_pos[i_b] = i_coef;
                }
                else
                {
                    p_mb->pf_idct[i_b] = p_vpar->pf_idct;
                }
                return;

                break;
            default:
                b_sign = GetBits( &p_vpar->bit_stream, 1 );
        }
        
        /* Prepare the next block */
        i_coef = i_parse;
        i_parse += i_run;
        i_nc ++;

        if( i_parse >= 64 )
        {
            /* We have an error in the stream */
            break;
        }

        /* Determine the position of the block in the frame */
        i_pos = p_vpar->ppi_scan[p_vpar->picture.b_alternate_scan][i_parse];
        i_level = ( i_level *
                    p_vpar->mb.i_quantizer_scale * pi_quant[i_pos] ) >> 3;

        /* Mismatch control */
        /* Equivalent to : if( (val & 1) == 0 ) val = val - 1; */
        i_level = (i_level - 1) | 1;

        i_level = b_sign ? -i_level : i_level;
        
        SATURATE( i_level );
        
        p_mb->ppi_blocks[i_b][i_pos] = i_level;
    }

    intf_ErrMsg("vpar error: DCT coeff (intra) is out of bounds");
    p_vpar->picture.b_error = 1;
}

/*****************************************************************************
 * DecodeMPEG2NonIntra : decode MPEG-2 non-intra blocks
 *****************************************************************************/
static __inline__ void DecodeMPEG2NonIntra( vpar_thread_t * p_vpar,
                                            macroblock_t * p_mb, int i_b,
                                            boolean_t b_chroma, int i_cc )
{
    int             i_parse;
    int             i_nc;
    int             i_coef;
    int             i_code;
    int             i_length;
    int             i_pos;
    int             i_run;
    int             i_level;
    int             i_mismatch = 1;
    boolean_t       b_sign;
    u8 *            pi_quant;
    dct_lookup_t *  p_tab;
    /* FIXME : if you want to enable other types of chroma, replace this
     * by p_vpar->sequence.i_chroma_format. */
    int             i_chroma_format = CHROMA_420;

    /* Give a pointer to the quantization matrices for intra blocks */
    if( (i_chroma_format == CHROMA_420) || (!b_chroma) )
    {
        pi_quant = p_vpar->sequence.nonintra_quant.pi_matrix;
    }
    else
    {
        pi_quant = p_vpar->sequence.chroma_nonintra_quant.pi_matrix;
    }

    /* Decoding of the AC coefficients */

    i_nc = 0;
    i_coef = 0;
    for( i_parse = 0; !p_vpar->p_fifo->b_die; i_parse++ )
    {
        i_code = ShowBits( &p_vpar->bit_stream, 16 );
        if( i_code >= 16384 )
        {
            p_tab = &ppl_dct_tab2[(i_parse == 0)][(i_code>>12)-4];
        }
        else
        {
            p_tab = &p_vpar->ppl_dct_coef[0][i_code];
        }
        i_run =     p_tab->i_run;
        i_level =   p_tab->i_level;
        i_length =  p_tab->i_length;

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
                    p_mb->pf_idct[i_b] = p_vpar->pf_sparse_idct;
                    p_mb->pi_sparse_pos[i_b] = i_coef;
                }
                else
                {
                    p_mb->pf_idct[i_b] = p_vpar->pf_idct;
                }
                
                /* Mismatch control */
                p_mb->ppi_blocks[i_b][63] ^= i_mismatch & 1;
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

        i_pos = p_vpar->ppi_scan[p_vpar->picture.b_alternate_scan][i_parse];
        i_level = ( ((i_level << 1) + 1) * p_vpar->mb.i_quantizer_scale
                    * pi_quant[i_pos] ) >> 5;

        /* Could be replaced by (i_level ^ sign) - sign */
        i_level = b_sign ? -i_level : i_level;
        
        SATURATE( i_level );
        
        p_mb->ppi_blocks[i_b][i_pos] = i_level;
        
        /* Mismatch control */
        i_mismatch ^= i_level;
    }

    intf_ErrMsg("vpar error: DCT coeff (non-intra) is out of bounds");
    p_vpar->picture.b_error = 1;
}

/*****************************************************************************
 * DecodeMPEG2Intra : decode MPEG-2 intra blocks
 *****************************************************************************/
static __inline__ void DecodeMPEG2Intra( vpar_thread_t * p_vpar,
                                         macroblock_t * p_mb, int i_b,
                                         boolean_t b_chroma, int i_cc )
{
    int             i_parse;
    int             i_nc;
    int             i_coef;
    int             i_code;
    int             i_length;
    int             i_pos;
    int             i_dct_dc_size;
    int             i_dct_dc_diff;
    int             i_run;
    int             i_level;
    int             i_mismatch = 1;
    boolean_t       b_vlc_intra;
    boolean_t       b_sign;
    u8 *            pi_quant;
    dct_lookup_t *  p_tab;
    lookup_t *      p_lookup;
    /* FIXME : if you want to enable other types of chroma, replace this
     * by p_vpar->sequence.i_chroma_format. */
    int             i_chroma_format = CHROMA_420;

    /* Give a pointer to the quantization matrices for intra blocks */
    if( (i_chroma_format == CHROMA_420) || (!b_chroma) )
    {
        pi_quant = p_vpar->sequence.intra_quant.pi_matrix;
    }
    else
    {
        pi_quant = p_vpar->sequence.chroma_intra_quant.pi_matrix;
    }

    /* decode length */
    i_code = ShowBits( &p_vpar->bit_stream, 5 );

    if (i_code < 31)
    {
        p_lookup = &ppl_dct_dc_init_table_1[b_chroma][i_code];
    }
    else
    {
        i_code = ShowBits(&p_vpar->bit_stream, (9+b_chroma)) - (0x1f0 * (b_chroma + 1));
        p_lookup = &ppl_dct_dc_init_table_2[b_chroma][i_code];
    }
    i_dct_dc_size = p_lookup->i_value;
    i_length = p_lookup->i_length;
    RemoveBits( &p_vpar->bit_stream, i_length );

    if (i_dct_dc_size == 0)
        i_dct_dc_diff = 0;
    else
    {
        i_dct_dc_diff = GetBits( &p_vpar->bit_stream, i_dct_dc_size);
        if ((i_dct_dc_diff & (1<<(i_dct_dc_size-1))) == 0)
            i_dct_dc_diff -= (1<<i_dct_dc_size) - 1;
    }

    /* Read the actual code with the good length */
    p_vpar->mb.pi_dc_dct_pred[i_cc] += i_dct_dc_diff;

    p_mb->ppi_blocks[i_b][0] = ( p_vpar->mb.pi_dc_dct_pred[i_cc] <<
                               ( 3 - p_vpar->picture.i_intra_dc_precision ) );

    i_nc = ( p_vpar->mb.pi_dc_dct_pred[i_cc] != 0 );

    /* Decoding of the AC coefficients */

    i_coef = 0;
    b_vlc_intra = p_vpar->picture.b_intra_vlc_format;
    for( i_parse = 1; !p_vpar->p_fifo->b_die; i_parse++ )
    {
        i_code = ShowBits( &p_vpar->bit_stream, 16 );
        /* We use 2 main tables for the coefficients */
        if( i_code >= 16384 )
        {
            p_tab = &ppl_dct_tab1[b_vlc_intra][(i_code>>(12-(4*b_vlc_intra)))-4];
        }
        else
        {
            p_tab = &p_vpar->ppl_dct_coef[b_vlc_intra][i_code];
        }
        i_run =     p_tab->i_run;
        i_level =   p_tab->i_level;
        i_length =  p_tab->i_length;

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
                    p_mb->pf_idct[i_b] = p_vpar->pf_sparse_idct;
                    p_mb->pi_sparse_pos[i_b] = i_coef;
                }
                else
                {
                    p_mb->pf_idct[i_b] = p_vpar->pf_idct;
                }
                
                /* Mismatch control */
                p_mb->ppi_blocks[i_b][63] ^= i_mismatch & 1;
                return;

                break;
            default:
                b_sign = GetBits( &p_vpar->bit_stream, 1 );
        }
        
        /* Prepare the next block */
        i_coef = i_parse;
        i_parse += i_run;
        i_nc ++;

        if( i_parse >= 64 )
        {
            /* We have an error in the stream */
            break;
        }

        /* Determine the position of the block in the frame */
        i_pos = p_vpar->ppi_scan[p_vpar->picture.b_alternate_scan][i_parse];
        i_level = ( i_level *
                    p_vpar->mb.i_quantizer_scale *
                    pi_quant[i_pos] ) >> 4;

        i_level = b_sign ? -i_level : i_level;
        
        SATURATE( i_level );
        
        p_mb->ppi_blocks[i_b][i_pos] = i_level;
        
        /* Mismatch control */
        i_mismatch ^= i_level;
    }

    intf_ErrMsg("vpar error: DCT coeff (intra) is out of bounds");
    p_vpar->picture.b_error = 1;
}

/*****************************************************************************
 * Decode*Blocks : decode blocks
 *****************************************************************************/
static int      pi_x[12] = {0,8,0,8,0,0,0,0,8,8,8,8};
static int      pi_y[2][12] = { {0,0,8,8,0,0,8,8,0,0,8,8},
                                {0,0,1,1,0,0,1,1,0,0,1,1} };
#define PARSEERROR                                                      \
if( p_vpar->picture.b_error )                                           \
{                                                                       \
    return;                                                             \
}

#define DECODEBLOCKS( MBFUNC, BLOCKFUNC )                               \
static void MBFUNC( vpar_thread_t * p_vpar, macroblock_t * p_mb )       \
{                                                                       \
    /* FIXME : if you want to enable other types of chroma, replace this\
     * by p_vpar->sequence.i_chroma_format. */                          \
    int         i_chroma_format = CHROMA_420;                           \
    int         i_mask, i_b;                                            \
    yuv_data_t *    p_data1;                                            \
    yuv_data_t *    p_data2;                                            \
                                                                        \
    i_mask = 1 << (3 + (1 << i_chroma_format));                         \
                                                                        \
    /* luminance */                                                     \
    p_data1 = p_mb->p_picture->p_y                                      \
        + p_mb->i_l_x + p_vpar->mb.i_l_y * (p_vpar->sequence.i_width);  \
                                                                        \
    for( i_b = 0 ; i_b < 4 ; i_b++, i_mask >>= 1 )                      \
    {                                                                   \
        if( p_mb->i_coded_block_pattern & i_mask )                      \
        {                                                               \
            memset( p_mb->ppi_blocks[i_b], 0, 64*sizeof(dctelem_t) );   \
            BLOCKFUNC( p_vpar, p_mb, i_b, 0, 0 );                       \
                                                                        \
            /* Calculate block coordinates. */                          \
            p_mb->p_data[i_b] = p_data1                                 \
                                + pi_y[p_vpar->mb.b_dct_type][i_b]      \
                                * p_vpar->picture.i_l_stride            \
                                + pi_x[i_b];                            \
                                                                        \
            PARSEERROR                                                  \
        }                                                               \
    }                                                                   \
                                                                        \
    /* chrominance */                                                   \
    p_data1 = p_mb->p_picture->p_u                                      \
              + p_mb->i_c_x                                             \
              + p_vpar->mb.i_c_y                                        \
                * (p_vpar->sequence.i_chroma_width);                    \
    p_data2 = p_mb->p_picture->p_v                                      \
               + p_mb->i_c_x                                            \
               + p_vpar->mb.i_c_y                                       \
                * (p_vpar->sequence.i_chroma_width);                    \
                                                                        \
    for( i_b = 4; i_b < 4 + (1 << i_chroma_format);                     \
         i_b++, i_mask >>= 1 )                                          \
    {                                                                   \
        yuv_data_t *    pp_data[2] = {p_data1, p_data2};                \
                                                                        \
        if( p_mb->i_coded_block_pattern & i_mask )                      \
        {                                                               \
            memset( p_mb->ppi_blocks[i_b], 0, 64*sizeof(dctelem_t) );   \
            BLOCKFUNC( p_vpar, p_mb, i_b, 1, 1 + ((i_b - 4) & 1) );     \
                                                                        \
            /* Calculate block coordinates. */                          \
            p_mb->p_data[i_b] = pp_data[i_b & 1]                        \
                                 + pi_y[p_vpar->mb.b_dct_type][i_b]     \
                                   * p_vpar->picture.i_c_stride         \
                                 + pi_x[i_b];                           \
                                                                        \
            PARSEERROR                                                  \
        }                                                               \
    }                                                                   \
}

DECODEBLOCKS( DecodeMPEG1NonIntraMB, DecodeMPEG1NonIntra );
DECODEBLOCKS( DecodeMPEG1IntraMB, DecodeMPEG1Intra );
DECODEBLOCKS( DecodeMPEG2NonIntraMB, DecodeMPEG2NonIntra );
DECODEBLOCKS( DecodeMPEG2IntraMB, DecodeMPEG2Intra );

#undef PARSEERROR


/*
 * Motion vectors
 */

/****************************************************************************
 * MotionCode : Parse the next motion code
 ****************************************************************************/
static __inline__ int MotionCode( vpar_thread_t * p_vpar )
{
    int i_code;
    static lookup_t pl_mv_tab0[8] =
        { {-1,0}, {3,3}, {2,2}, {2,2}, {1,1}, {1,1}, {1,1}, {1,1} };
    /* Table B-10, motion_code, codes 0000011 ... 000011x */
    static lookup_t pl_mv_tab1[8] =
        { {-1,0}, {-1,0}, {-1,0}, {7,6}, {6,6}, {5,6}, {4,5}, {4,5} };
    /* Table B-10, motion_code, codes 0000001100 ... 000001011x */
    static lookup_t pl_mv_tab2[12] = {
        {16,9}, {15,9}, {14,9}, {13,9},
        {12,9}, {11,9}, {10,8}, {10,8},
        {9,8},  {9,8},  {8,8},  {8,8} };

    if( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        return 0;
    }
    if( (i_code = ShowBits( &p_vpar->bit_stream, 9) ) >= 64 )
    {
        i_code >>= 6;
        RemoveBits( &p_vpar->bit_stream, pl_mv_tab0[i_code].i_length );
        return( GetBits( &p_vpar->bit_stream, 1 ) ?
            -pl_mv_tab0[i_code].i_value : pl_mv_tab0[i_code].i_value );
    }

    if( i_code >= 24 )
    {
        i_code >>= 3;
        RemoveBits( &p_vpar->bit_stream, pl_mv_tab1[i_code].i_length );
        return( GetBits( &p_vpar->bit_stream, 1 ) ?
            -pl_mv_tab1[i_code].i_value : pl_mv_tab1[i_code].i_value );
    }

    if( (i_code -= 12) < 0 )
    {
        p_vpar->picture.b_error = 1;
        intf_ErrMsg( "vpar error: Invalid motion_vector code" );
        return 0;
    }

    RemoveBits( &p_vpar->bit_stream, pl_mv_tab2[i_code].i_length );
    return( GetBits( &p_vpar->bit_stream, 1 ) ?
        -pl_mv_tab2[i_code].i_value : pl_mv_tab2[i_code].i_value );
}

/****************************************************************************
 * DecodeMotionVector : Decode a motion_vector
 ****************************************************************************/
static __inline__ void DecodeMotionVector( int * pi_prediction, int i_r_size,
        int i_motion_code, int i_motion_residual, int i_full_pel )
{
    int i_limit, i_vector;

    /* ISO/IEC 13818-1 section 7.6.3.1 */
    i_limit = 16 << i_r_size;
    i_vector = *pi_prediction >> i_full_pel;

    if( i_motion_code > 0 )
    {
        i_vector += ((i_motion_code-1) << i_r_size) + i_motion_residual + 1;
        if( i_vector >= i_limit )
            i_vector -= i_limit + i_limit;
    }
    else if( i_motion_code < 0 )
    {
        i_vector -= ((-i_motion_code-1) << i_r_size) + i_motion_residual + 1;
        if( i_vector < -i_limit )
            i_vector += i_limit + i_limit;
    }
    *pi_prediction = i_vector << i_full_pel;
}

/****************************************************************************
 * MotionVector : Parse the next motion_vector field
 ****************************************************************************/
static __inline__ void MotionVector( vpar_thread_t * p_vpar,
                                     macroblock_t * p_mb, int i_r,
                                     int i_s, int i_full_pel, int i_structure,
                                     int i_h_r_size, int i_v_r_size )
{
    int i_motion_code, i_motion_residual;
    int pi_dm_vector[2];

    i_motion_code = MotionCode( p_vpar );
    i_motion_residual = (i_h_r_size != 0 && i_motion_code != 0) ?
                        GetBits( &p_vpar->bit_stream, i_h_r_size) : 0;
    DecodeMotionVector( &p_vpar->mb.pppi_pmv[i_r][i_s][0], i_h_r_size,
                        i_motion_code, i_motion_residual, i_full_pel );
    p_mb->pppi_motion_vectors[i_r][i_s][0] = p_vpar->mb.pppi_pmv[i_r][i_s][0];

    if( p_vpar->mb.b_dmv )
    {
        if( GetBits(&p_vpar->bit_stream, 1) )
        {
            pi_dm_vector[0] = GetBits( &p_vpar->bit_stream, 1 ) ? -1 : 1;
        }
        else
        {
            pi_dm_vector[0] = 0;
        }
    }

    i_motion_code = MotionCode( p_vpar );
    i_motion_residual = (i_v_r_size != 0 && i_motion_code != 0) ?
                        GetBits( &p_vpar->bit_stream, i_v_r_size) : 0;


    if( (p_vpar->mb.i_mv_format == MOTION_FIELD)
        && (i_structure == FRAME_STRUCTURE) )
    {
         p_vpar->mb.pppi_pmv[i_r][i_s][1] >>= 1;
    }

    DecodeMotionVector( &p_vpar->mb.pppi_pmv[i_r][i_s][1], i_v_r_size,
                        i_motion_code, i_motion_residual, i_full_pel );

    if( (p_vpar->mb.i_mv_format == MOTION_FIELD)
        && (i_structure == FRAME_STRUCTURE) )
         p_vpar->mb.pppi_pmv[i_r][i_s][1] <<= 1;

    p_mb->pppi_motion_vectors[i_r][i_s][1] = p_vpar->mb.pppi_pmv[i_r][i_s][1];

    if( p_vpar->mb.b_dmv )
    {
        if( GetBits(&p_vpar->bit_stream, 1) )
        {
            pi_dm_vector[1] = GetBits( &p_vpar->bit_stream, 1 ) ? -1 : 1;
        }
        else
        {
            pi_dm_vector[1] = 0;
        }

        /* Dual Prime Arithmetic (ISO/IEC 13818-2 section 7.6.3.6). */

#define i_mv_x  p_mb->pppi_motion_vectors[0][0][0]
        if( i_structure == FRAME_STRUCTURE )
        {
#define i_mv_y  (p_mb->pppi_motion_vectors[0][0][1] << 1)
            if( p_vpar->picture.b_top_field_first )
            {
                /* vector for prediction of top field from bottom field */
                p_mb->ppi_dmv[0][0] = ((i_mv_x + (i_mv_x > 0)) >> 1) + pi_dm_vector[0];
                p_mb->ppi_dmv[0][1] = ((i_mv_y + (i_mv_y > 0)) >> 1) + pi_dm_vector[1] - 1;

                /* vector for prediction of bottom field from top field */
                p_mb->ppi_dmv[1][0] = ((3*i_mv_x + (i_mv_x > 0)) >> 1) + pi_dm_vector[0];
                p_mb->ppi_dmv[1][1] = ((3*i_mv_y + (i_mv_y > 0)) >> 1) + pi_dm_vector[1] + 1;
            }
            else
            {
                /* vector for prediction of top field from bottom field */
                p_mb->ppi_dmv[0][0] = ((3*i_mv_x + (i_mv_x > 0)) >> 1) + pi_dm_vector[0];
                p_mb->ppi_dmv[0][1] = ((3*i_mv_y + (i_mv_y > 0)) >> 1) + pi_dm_vector[1] - 1;

                /* vector for prediction of bottom field from top field */
                p_mb->ppi_dmv[1][0] = ((i_mv_x + (i_mv_x > 0)) >> 1) + pi_dm_vector[0];
                p_mb->ppi_dmv[1][1] = ((i_mv_y + (i_mv_y > 0)) >> 1) + pi_dm_vector[1] + 1;
            }
#undef i_mv_y
        }
        else
        {
#define i_mv_y  p_mb->pppi_motion_vectors[0][0][1]
            /* vector for prediction from field of opposite 'parity' */
            p_mb->ppi_dmv[0][0] = ((i_mv_x + (i_mv_x > 0)) >> 1) + pi_dm_vector[0];
            p_mb->ppi_dmv[0][1] = ((i_mv_y + (i_mv_y > 0)) >> 1) + pi_dm_vector[1];

            /* correct for vertical field shift */
            if( i_structure == TOP_FIELD )
                p_mb->ppi_dmv[0][1]--;
            else
                p_mb->ppi_dmv[0][1]++;
#undef i_mv_y
        }
#undef i_mv_x
    }
}

/*****************************************************************************
 * DecodeMVMPEG1 : Parse the next MPEG-1 motion vectors
 *****************************************************************************/
static void DecodeMVMPEG1( vpar_thread_t * p_vpar,
                           macroblock_t * p_mb, int i_s, int i_structure )
{
    int i_r_size = i_s ? p_vpar->picture.i_backward_f_code - 1 :
                         p_vpar->picture.i_forward_f_code - 1;
    MotionVector( p_vpar, p_mb, 0, i_s,
                  p_vpar->picture.pb_full_pel_vector[i_s], FRAME_STRUCTURE,
                  i_r_size, i_r_size );
}

/*****************************************************************************
 * DecodeMVMPEG2 : Parse the next MPEG-2 motion_vectors field
 *****************************************************************************/
static void DecodeMVMPEG2( vpar_thread_t * p_vpar,
                           macroblock_t * p_mb, int i_s, int i_structure )
{
    if( p_vpar->mb.i_mv_count == 1 )
    {
        if( p_vpar->mb.i_mv_format == MOTION_FIELD && !p_vpar->mb.b_dmv )
        {
            p_mb->ppi_field_select[0][i_s] = p_mb->ppi_field_select[1][i_s]
                                            = GetBits( &p_vpar->bit_stream, 1 );
        }
        MotionVector( p_vpar, p_mb, 0, i_s, 0, i_structure,
                      p_vpar->picture.ppi_f_code[i_s][0] - 1,
                      p_vpar->picture.ppi_f_code[i_s][1] - 1 );
        p_vpar->mb.pppi_pmv[1][i_s][0] = p_vpar->mb.pppi_pmv[0][i_s][0];
        p_vpar->mb.pppi_pmv[1][i_s][1] = p_vpar->mb.pppi_pmv[0][i_s][1];
        p_mb->pppi_motion_vectors[1][i_s][0] = p_vpar->mb.pppi_pmv[0][i_s][0];
        p_mb->pppi_motion_vectors[1][i_s][1] = p_vpar->mb.pppi_pmv[0][i_s][1];
    }
    else
    {
        p_mb->ppi_field_select[0][i_s] = GetBits( &p_vpar->bit_stream, 1 );
        MotionVector( p_vpar, p_mb, 0, i_s, 0, i_structure,
                      p_vpar->picture.ppi_f_code[i_s][0] - 1,
                      p_vpar->picture.ppi_f_code[i_s][1] - 1 );
        p_mb->ppi_field_select[1][i_s] = GetBits( &p_vpar->bit_stream, 1 );
        MotionVector( p_vpar, p_mb, 1, i_s, 0, i_structure,
                      p_vpar->picture.ppi_f_code[i_s][0] - 1,
                      p_vpar->picture.ppi_f_code[i_s][1] - 1 );
    }
}


/*
 * Macroblock information structures
 */

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
 * IMBType : macroblock_type in I pictures
 *****************************************************************************/
static __inline__ int IMBType( vpar_thread_t * p_vpar )
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
 * PMBType : macroblock_type in P pictures
 *****************************************************************************/
static __inline__ int PMBType( vpar_thread_t * p_vpar )
{
    /* Testing on 6 bits */
    int                i_type = ShowBits( &p_vpar->bit_stream, 6 );

    /* Dump the good number of bits */
    RemoveBits( &p_vpar->bit_stream, p_vpar->ppl_mb_type[0][i_type].i_length );
    /* return the value from the lookup table for P type */
    return p_vpar->ppl_mb_type[0][i_type].i_value;
}

/*****************************************************************************
 * BMBType : macroblock_type in B pictures
 *****************************************************************************/
static __inline__ int BMBType( vpar_thread_t * p_vpar )
{
     /* Testing on 6 bits */
    int                i_type = ShowBits( &p_vpar->bit_stream, 6 );

    /* Dump the good number of bits */
    RemoveBits( &p_vpar->bit_stream, p_vpar->ppl_mb_type[1][i_type].i_length );

    /* return the value from the lookup table for B type */
    return p_vpar->ppl_mb_type[1][i_type].i_value;
}

/*****************************************************************************
 * DMBType : macroblock_type in D pictures
 *****************************************************************************/
static __inline__ int DMBType( vpar_thread_t * p_vpar )
{
    return GetBits( &p_vpar->bit_stream, 1 );
}

/*****************************************************************************
 * CodedPattern420 : coded_block_pattern with 4:2:0 chroma
 *****************************************************************************/
static __inline__ int CodedPattern420( vpar_thread_t * p_vpar )
{
    /* Take the max 9 bits length vlc code for testing */
    int      i_vlc = ShowBits( &p_vpar->bit_stream, 9 );

    /* Trash the good number of bits read in the lookup table */
    RemoveBits( &p_vpar->bit_stream, pl_coded_pattern[i_vlc].i_length );

    /* return the value from the vlc table */
    return pl_coded_pattern[i_vlc].i_value;
}

/*****************************************************************************
 * CodedPattern422 : coded_block_pattern with 4:2:2 chroma
 *****************************************************************************/
static __inline__ int CodedPattern422( vpar_thread_t * p_vpar )
{
    int      i_vlc = ShowBits( &p_vpar->bit_stream, 9 );

    RemoveBits( &p_vpar->bit_stream, pl_coded_pattern[i_vlc].i_length );

    /* Supplementary 2 bits long code for 4:2:2 format */
    return pl_coded_pattern[i_vlc].i_value |
           (GetBits( &p_vpar->bit_stream, 2 ) << 6);
}

/*****************************************************************************
 * CodedPattern444 : coded_block_pattern with 4:4:4 chroma
 *****************************************************************************/
static __inline__ int CodedPattern444( vpar_thread_t * p_vpar )
{
    int      i_vlc = ShowBits( &p_vpar->bit_stream, 9 );

    RemoveBits( &p_vpar->bit_stream, pl_coded_pattern[i_vlc].i_length );

    return pl_coded_pattern[i_vlc].i_value |
           (GetBits( &p_vpar->bit_stream, 6 ) << 6);
}

/*****************************************************************************
 * InitMacroblock : Initialize macroblock values
 *****************************************************************************/
static __inline__ void InitMacroblock( vpar_thread_t * p_vpar,
                                       macroblock_t * p_mb, int i_coding_type,
                                       int i_structure )
{
    int     i_chroma_format = CHROMA_420; /* FIXME : MP@ML */

    p_mb->i_chroma_nb_blocks = 1 << i_chroma_format;
    p_mb->p_picture = p_vpar->picture.p_picture;

    if( i_coding_type == B_CODING_TYPE )
        p_mb->p_backward = p_vpar->sequence.p_backward;
    else
        p_mb->p_backward = NULL;
    if( (i_coding_type == P_CODING_TYPE) || (i_coding_type == B_CODING_TYPE) )
        p_mb->p_forward = p_vpar->sequence.p_forward;
    else
        p_mb->p_forward = NULL;

    p_mb->i_l_x = p_vpar->mb.i_l_x;
    p_mb->i_c_x = p_vpar->mb.i_c_x;
    p_mb->i_motion_l_y = p_vpar->mb.i_l_y;
    p_mb->i_motion_c_y = p_vpar->mb.i_c_y;
    if( (p_mb->b_motion_field = (i_structure == BOTTOM_FIELD)) )
    {
        p_mb->i_motion_l_y--;
        p_mb->i_motion_c_y--;
    }
    p_mb->i_addb_l_stride = (p_mb->i_l_stride = p_vpar->picture.i_l_stride) - 8;
    p_mb->i_addb_c_stride = (p_mb->i_c_stride = p_vpar->picture.i_c_stride) - 8;
    p_mb->b_P_second = ( (i_structure != p_vpar->picture.i_current_structure)
                           && i_coding_type == P_CODING_TYPE );
}

/*****************************************************************************
 * UpdateContext : Update the p_vpar contextual values
 *****************************************************************************/
static __inline__ void UpdateContext( vpar_thread_t * p_vpar, int i_structure )
{
    /* Update macroblock real position. */
    p_vpar->mb.i_l_x += 16;
    p_vpar->mb.i_l_y += (p_vpar->mb.i_l_x / p_vpar->sequence.i_width)
                        * (2 - (i_structure == FRAME_STRUCTURE)) * 16;
    p_vpar->mb.i_l_x %= p_vpar->sequence.i_width;

    p_vpar->mb.i_c_x += p_vpar->sequence.i_chroma_mb_width;
    p_vpar->mb.i_c_y += (p_vpar->mb.i_c_x / p_vpar->sequence.i_chroma_width)
                        * (2 - (i_structure == FRAME_STRUCTURE))
                        * p_vpar->sequence.i_chroma_mb_height;
    p_vpar->mb.i_c_x %= p_vpar->sequence.i_chroma_width;
}

/*****************************************************************************
 * SkippedMacroblock : Generate a skipped macroblock with NULL motion vector
 *****************************************************************************/
static __inline__ void SkippedMacroblock( vpar_thread_t * p_vpar, int i_mb,
                                          int i_mb_base, int i_coding_type,
                                          int i_structure  )
{
    macroblock_t *  p_mb;
    int             i_chroma_format = CHROMA_420; /* FIXME : MP@ML */

    if( i_coding_type == I_CODING_TYPE )
    {
        intf_ErrMsg("vpar error: skipped macroblock in I-picture");
        p_vpar->picture.b_error = 1;
        return;
    }

    if( (p_mb = vpar_NewMacroblock( &p_vpar->vfifo )) == NULL )
    {
        /* b_die == 1 */
        return;
    }
#ifdef VDEC_SMP
    p_vpar->picture.pp_mb[i_mb_base + i_mb] = p_mb;
#endif

    InitMacroblock( p_vpar, p_mb, i_coding_type, i_structure );

    /* Motion type is picture structure. */
    p_mb->pf_motion = p_vpar->ppf_motion_skipped[i_chroma_format]
                                                [i_structure];
    p_mb->i_coded_block_pattern = 0;

    /* Motion direction and motion vectors depend on the coding type. */
    if( i_coding_type == B_CODING_TYPE )
    {
        p_mb->i_mb_type = p_vpar->mb.i_motion_dir;
        memcpy( p_mb->pppi_motion_vectors, p_vpar->mb.pppi_pmv,
                sizeof( p_vpar->mb.pppi_pmv ) );
    }
    else
    {
        p_mb->i_mb_type = MB_MOTION_FORWARD;
        memset( p_mb->pppi_motion_vectors, 0, 8*sizeof(int) );
    }

    /* Set the field we use for motion compensation */
    p_mb->ppi_field_select[0][0] = p_mb->ppi_field_select[0][1]
                                 = ( i_structure == BOTTOM_FIELD );

    UpdateContext( p_vpar, i_structure );

#ifndef VDEC_SMP
    /* Decode the macroblock NOW ! */
    vpar_DecodeMacroblock ( &p_vpar->vfifo, p_mb );
#endif
}

/*****************************************************************************
 * MacroblockModes : Get the macroblock_modes structure
 *****************************************************************************/
static __inline__ void MacroblockModes( vpar_thread_t * p_vpar,
                                        macroblock_t * p_mb,
                                        int i_coding_type,
                                        int i_structure )
{
    static int          ppi_mv_count[2][4] = { {0, 1, 2, 1}, {0, 2, 1, 1} };
    static int          ppi_mv_format[2][4] = { {0, 1, 1, 1}, {0, 1, 2, 1} };

    /* Get macroblock_type. */
    switch( i_coding_type )
    {
    case P_CODING_TYPE:
        p_mb->i_mb_type = PMBType( p_vpar );
        break;
    case B_CODING_TYPE:
        p_mb->i_mb_type = BMBType( p_vpar );
        break;
    case I_CODING_TYPE:
        p_mb->i_mb_type = IMBType( p_vpar );
        break;
    case D_CODING_TYPE:
        p_mb->i_mb_type = DMBType( p_vpar );
    }

    if( i_coding_type == B_CODING_TYPE )
    {
        /* We need to remember the motion direction of the last macroblock
         * before a skipped macroblock (ISO/IEC 13818-2 7.6.6) */
        p_vpar->mb.i_motion_dir = p_mb->i_mb_type
                              & (MB_MOTION_FORWARD | MB_MOTION_BACKWARD);
    }

    /* SCALABILITY : warning, we don't know if spatial_temporal_weight_code
     * has to be dropped, take care if you use scalable streams. */
    /* RemoveBits( &p_vpar->bit_stream, 2 ); */

    if( (i_coding_type == P_CODING_TYPE || i_coding_type == B_CODING_TYPE)
        && (p_mb->i_mb_type & (MB_MOTION_FORWARD | MB_MOTION_BACKWARD)) )
    {
        if( !(i_structure == FRAME_STRUCTURE
               && p_vpar->picture.b_frame_pred_frame_dct) )
        {
            p_vpar->mb.i_motion_type = GetBits( &p_vpar->bit_stream, 2 );
        }
        else
        {
            p_vpar->mb.i_motion_type = MOTION_FRAME;
        }

        /* XXX?? */
        p_vpar->mb.i_mv_count = ppi_mv_count[i_structure == FRAME_STRUCTURE]
                                            [p_vpar->mb.i_motion_type];
        p_vpar->mb.i_mv_format = ppi_mv_format[i_structure == FRAME_STRUCTURE]
                                              [p_vpar->mb.i_motion_type];
        p_vpar->mb.b_dmv = p_vpar->mb.i_motion_type == MOTION_DMV;
    }

    p_vpar->mb.b_dct_type = 0;
    if( (i_structure == FRAME_STRUCTURE) &&
        (!p_vpar->picture.b_frame_pred_frame_dct) &&
        (p_mb->i_mb_type & (MB_PATTERN|MB_INTRA)) )
    {
        if( (p_vpar->mb.b_dct_type = GetBits( &p_vpar->bit_stream, 1 )) )
        {
            /* The DCT is coded on fields. Jump one line between each
             * sample. */
            p_mb->i_addb_l_stride <<= 1;
            p_mb->i_addb_l_stride += 8;
#if 0
            /* FIXME: MP@ML */
            /* With CHROMA_420, the DCT is necessarily frame-coded. */
            if( i_chroma_format != CHROMA_420 )
            {
                p_mb->i_addb_c_stride <<= 1;
                p_mb->i_addb_c_stride += 8;
            }
#endif
        }
    }
}

/*****************************************************************************
 * ParseMacroblock : Parse the next macroblock
 *****************************************************************************/
#define PARSEERROR                                                      \
if( p_vpar->picture.b_error )                                           \
{                                                                       \
    /* Mark this block as skipped (better than green blocks), and       \
     * go to the next slice. */                                         \
    (*pi_mb_address)--;                                                 \
    vpar_DestroyMacroblock( &p_vpar->vfifo, p_mb );                     \
    return;                                                             \
}

static __inline__ void ParseMacroblock(
                           vpar_thread_t * p_vpar,
                           int * pi_mb_address,     /* previous address to be
                                                     * used for mb_addr_incr */
                           int i_mb_previous,          /* actual previous mb */
                           int i_mb_base,     /* non-zero if field structure */
                           /* The following parameters are explicit in
                            * optimized routines : */
                           boolean_t b_mpeg2,             /* you know what ? */
                           int i_coding_type,                /* I, P, B or D */
                           int i_structure )   /* T(OP), B(OTTOM) or F(RAME) */
{
    int             i_mb;
    int             i_inc;
    int             i_chroma_format = CHROMA_420; /* FIXME : MP@ML */
    macroblock_t *  p_mb;

    i_inc = MacroblockAddressIncrement( p_vpar );
    *pi_mb_address += i_inc;

    if( i_inc < 0 )
    {
        intf_ErrMsg( "vpar error: bad address increment (%d)", i_inc );
        p_vpar->picture.b_error = 1;
        return;
    }

    if( *pi_mb_address - i_mb_previous - 1 )
    {
        /* Skipped macroblock (ISO/IEC 13818-2 7.6.6). */

        /* Reset DC predictors (7.2.1). */
        p_vpar->mb.pi_dc_dct_pred[0] = p_vpar->mb.pi_dc_dct_pred[1]
            = p_vpar->mb.pi_dc_dct_pred[2]
            = 1 << (7 + p_vpar->picture.i_intra_dc_precision);

        if( i_coding_type == P_CODING_TYPE )
        {
            /* Reset motion vector predictors (ISO/IEC 13818-2 7.6.3.4). */
            memset( p_vpar->mb.pppi_pmv, 0, 8*sizeof(int) );
        }

        for( i_mb = i_mb_previous + 1; i_mb < *pi_mb_address; i_mb++ )
        {
            SkippedMacroblock( p_vpar, i_mb, i_mb_base, i_coding_type,
                               i_structure );
        }
    }

    /* Get a macroblock structure. */
    if( (p_mb = vpar_NewMacroblock( &p_vpar->vfifo )) == NULL )
    {
        /* b_die == 1 */
        return;
    }
#ifdef VDEC_SMP
    p_vpar->picture.pp_mb[i_mb_base + *pi_mb_address] = p_mb;
#endif

    InitMacroblock( p_vpar, p_mb, i_coding_type, i_structure );

    /* Parse off macroblock_modes structure. */
    MacroblockModes( p_vpar, p_mb, i_coding_type, i_structure );

    if( p_mb->i_mb_type & MB_QUANT )
    {
        LoadQuantizerScale( p_vpar );
    }

    if( (i_coding_type == P_CODING_TYPE || i_coding_type == B_CODING_TYPE)
         && (p_mb->i_mb_type & MB_MOTION_FORWARD) )
    {
        if( b_mpeg2 )
            DecodeMVMPEG2( p_vpar, p_mb, 0, i_structure );
        else
            DecodeMVMPEG1( p_vpar, p_mb, 0, i_structure );
        PARSEERROR
    }

    if( (i_coding_type == B_CODING_TYPE)
         && (p_mb->i_mb_type & MB_MOTION_BACKWARD) )
    {
        if( b_mpeg2 )
            DecodeMVMPEG2( p_vpar, p_mb, 1, i_structure );
        else
            DecodeMVMPEG1( p_vpar, p_mb, 1, i_structure );
        PARSEERROR
    }

    if( i_coding_type == P_CODING_TYPE
         && !(p_mb->i_mb_type & (MB_MOTION_FORWARD|MB_INTRA)) )
    {
        /* Special No-MC macroblock in P pictures (7.6.3.5). */
        p_mb->i_mb_type |= MB_MOTION_FORWARD;
        memset( p_vpar->mb.pppi_pmv, 0, 8*sizeof(int) );
        memset( p_mb->pppi_motion_vectors, 0, 8*sizeof(int) );
        p_vpar->mb.i_motion_type = 1 + (i_structure == FRAME_STRUCTURE);
        p_mb->ppi_field_select[0][0] = (i_structure == BOTTOM_FIELD);
    }

    if( (i_coding_type != I_CODING_TYPE) && !(p_mb->i_mb_type & MB_INTRA) )
    {
        /* Reset DC predictors (7.2.1). */
        p_vpar->mb.pi_dc_dct_pred[0] = p_vpar->mb.pi_dc_dct_pred[1]
            = p_vpar->mb.pi_dc_dct_pred[2]
            = 1 << (7 + p_vpar->picture.i_intra_dc_precision);

        /* Motion function pointer. */
        p_mb->pf_motion = p_vpar->pppf_motion[i_chroma_format]
                                             [i_structure == FRAME_STRUCTURE]
                                             [p_vpar->mb.i_motion_type];

        if( p_mb->i_mb_type & MB_PATTERN )
        {
            switch( i_chroma_format )
            {
            case CHROMA_420:
                p_mb->i_coded_block_pattern = CodedPattern420( p_vpar );
                break;
            case CHROMA_422:
                p_mb->i_coded_block_pattern = CodedPattern422( p_vpar );
                break;
            case CHROMA_444:
                p_mb->i_coded_block_pattern = CodedPattern444( p_vpar );
            }
        }
        else
        {
            p_mb->i_coded_block_pattern = 0;
        }

        /*
         * Effectively decode blocks.
         */
        if( b_mpeg2 )
            DecodeMPEG2NonIntraMB( p_vpar, p_mb );
        else
            DecodeMPEG1NonIntraMB( p_vpar, p_mb );
        PARSEERROR
    }
    else
    {
        if( !p_vpar->picture.b_concealment_mv )
        {
            /* Reset MV predictors. */
            memset( p_vpar->mb.pppi_pmv, 0, 8*sizeof(int) );
        }
        else
        {
            if( b_mpeg2 )
                DecodeMVMPEG2( p_vpar, p_mb, 0, i_structure );
            else
                DecodeMVMPEG1( p_vpar, p_mb, 0, i_structure );
            RemoveBits( &p_vpar->bit_stream, 1 );
        }

        if( p_mb->i_mb_type & MB_PATTERN )
        {
            switch( i_chroma_format )
            {
            case CHROMA_420:
                p_mb->i_coded_block_pattern = CodedPattern420( p_vpar );
                break;
            case CHROMA_422:
                p_mb->i_coded_block_pattern = CodedPattern422( p_vpar );
                break;
            case CHROMA_444:
                p_mb->i_coded_block_pattern = CodedPattern444( p_vpar );
            }
        }
        else
        {
            p_mb->i_coded_block_pattern =
                                (1 << (4 + (1 << i_chroma_format))) - 1;
        }

        /*
         * Effectively decode blocks.
         */
        if( b_mpeg2 )
            DecodeMPEG2IntraMB( p_vpar, p_mb );
        else
            DecodeMPEG1IntraMB( p_vpar, p_mb );
        PARSEERROR
    }

    if( !p_vpar->picture.b_error )
    {
        UpdateContext( p_vpar, i_structure );
#ifndef VDEC_SMP
        /* Decode the macroblock NOW ! */
        vpar_DecodeMacroblock ( &p_vpar->vfifo, p_mb );
#endif
    }
    else
    {
        /* Mark this block as skipped (better than green blocks), and go
         * to the next slice. */
        (*pi_mb_address)--;
        vpar_DestroyMacroblock( &p_vpar->vfifo, p_mb );
    }
}

#undef PARSEERROR


/*
 * Picture data parsing management
 */

/*****************************************************************************
 * SliceHeader : Parse the next slice structure
 *****************************************************************************/
static __inline__ void SliceHeader( vpar_thread_t * p_vpar,
                                    int * pi_mb_address, int i_mb_base,
                                    u32 i_vert_code, boolean_t b_mpeg2,
                                    int i_coding_type, int i_structure )
{
    int         i_mb_address_save = *pi_mb_address;

    p_vpar->picture.b_error = 0;

#if 0
    /* FIXME : MP@ML doesn't support this. */
    if( b_high )
    {
        /* Picture with more than 2800 lines. */
        i_vert_code += GetBits( &p_vpar->bit_stream, 3 ) << 7;
    }
    if( b_dp_scalable )
    {
        /* DATA_PARTITIONING scalability. */
        RemoveBits( &p_vpar->bit_stream, 7 ); /* priority_breakpoint */
    }
#endif

    LoadQuantizerScale( p_vpar );

    if( GetBits( &p_vpar->bit_stream, 1 ) )
    {
        /* intra_slice, slice_id */
        RemoveBits( &p_vpar->bit_stream, 8 );
        /* extra_information_slice */
        while( GetBits( &p_vpar->bit_stream, 1 ) )
        {
            RemoveBits( &p_vpar->bit_stream, 8 );
        }
    }
    *pi_mb_address = (i_vert_code - 1) * p_vpar->sequence.i_mb_width;

    if( *pi_mb_address < i_mb_address_save )
    {
        intf_ErrMsg( "vpar error: slices do not follow, maybe a PES has been trashed" );
        p_vpar->picture.b_error = 1;
        return;
    }

    /* Reset DC coefficients predictors (ISO/IEC 13818-2 7.2.1). */
    p_vpar->mb.pi_dc_dct_pred[0] = p_vpar->mb.pi_dc_dct_pred[1]
        = p_vpar->mb.pi_dc_dct_pred[2]
        = 1 << (7 + p_vpar->picture.i_intra_dc_precision);

    /* Reset motion vector predictors (ISO/IEC 13818-2 7.6.3.4). */
    memset( p_vpar->mb.pppi_pmv, 0, 8*sizeof(int) );

    do
    {
        ParseMacroblock( p_vpar, pi_mb_address, i_mb_address_save,
                         i_mb_base, b_mpeg2, i_coding_type, i_structure );

        i_mb_address_save = *pi_mb_address;
        if( p_vpar->picture.b_error )
        {
            return;
        }
    }
    while( ShowBits( &p_vpar->bit_stream, 23 ) && !p_vpar->p_fifo->b_die );
    NextStartCode( &p_vpar->bit_stream );
}

/*****************************************************************************
 * PictureData : Parse off all macroblocks (ISO/IEC 13818-2 6.2.3.7)
 *****************************************************************************/
static __inline__ void vpar_PictureData( vpar_thread_t * p_vpar,
                                         int i_mb_base, boolean_t b_mpeg2,
                                         int i_coding_type, int i_structure )
{
    int         i_mb_address = 0;
    u32         i_dummy;

    NextStartCode( &p_vpar->bit_stream );
    while( ((i_coding_type != I_CODING_TYPE && i_coding_type != D_CODING_TYPE)
             || !p_vpar->picture.b_error)
           && i_mb_address < (p_vpar->sequence.i_mb_size
                                >> (i_structure != FRAME_STRUCTURE))
           && !p_vpar->p_fifo->b_die )
    {
        if( ((i_dummy = ShowBits( &p_vpar->bit_stream, 32 ))
                 < SLICE_START_CODE_MIN) ||
            (i_dummy > SLICE_START_CODE_MAX) )
        {
            intf_ErrMsg("vpar error: premature end of picture");
            p_vpar->picture.b_error = 1;
            break;
        }
        RemoveBits32( &p_vpar->bit_stream );

        /* Decode slice data. */
        SliceHeader( p_vpar, &i_mb_address, i_mb_base, i_dummy & 255,
                     b_mpeg2, i_coding_type, i_structure );
    }

    /* Try to recover from error. If we missed less than half the
     * number of macroblocks of the picture, mark the missed ones
     * as skipped. */
    if( (i_coding_type == P_CODING_TYPE || i_coding_type == B_CODING_TYPE)
        && p_vpar->picture.b_error &&
        ( (i_mb_address-i_mb_base) > (p_vpar->sequence.i_mb_size >> 1)
           || (i_structure != FRAME_STRUCTURE
               && (i_mb_address-i_mb_base) >
                         (p_vpar->sequence.i_mb_size >> 2) ) ) )
    {
        int         i_mb;

        p_vpar->picture.b_error = 0;
        for( i_mb = i_mb_address + 1;
             i_mb < (p_vpar->sequence.i_mb_size
                     << (i_structure != FRAME_STRUCTURE));
             i_mb++ )
        {
            SkippedMacroblock( p_vpar, i_mb, i_mb_base,
                               i_coding_type,
                               i_structure );
        }
    }
}

#define DECLARE_PICD( FUNCNAME, B_MPEG2, I_CODING_TYPE, I_STRUCTURE )       \
void FUNCNAME( vpar_thread_t * p_vpar, int i_mb_base )                      \
{                                                                           \
    vpar_PictureData( p_vpar, i_mb_base, B_MPEG2, I_CODING_TYPE,            \
                      I_STRUCTURE );                                        \
}

DECLARE_PICD( vpar_PictureDataGENERIC, p_vpar->sequence.b_mpeg2,
              p_vpar->picture.i_coding_type, p_vpar->picture.i_structure );
#if (VPAR_OPTIM_LEVEL > 0)
DECLARE_PICD( vpar_PictureData1I, 0, I_CODING_TYPE, FRAME_STRUCTURE );
DECLARE_PICD( vpar_PictureData1P, 0, P_CODING_TYPE, FRAME_STRUCTURE );
DECLARE_PICD( vpar_PictureData1B, 0, B_CODING_TYPE, FRAME_STRUCTURE );
DECLARE_PICD( vpar_PictureData1D, 0, D_CODING_TYPE, FRAME_STRUCTURE );
DECLARE_PICD( vpar_PictureData2IF, 1, I_CODING_TYPE, FRAME_STRUCTURE );
DECLARE_PICD( vpar_PictureData2PF, 1, P_CODING_TYPE, FRAME_STRUCTURE );
DECLARE_PICD( vpar_PictureData2BF, 1, B_CODING_TYPE, FRAME_STRUCTURE );
#endif
#if (VPAR_OPTIM_LEVEL > 1)
DECLARE_PICD( vpar_PictureData2IT, 1, I_CODING_TYPE, TOP_FIELD );
DECLARE_PICD( vpar_PictureData2PT, 1, P_CODING_TYPE, TOP_FIELD );
DECLARE_PICD( vpar_PictureData2BT, 1, B_CODING_TYPE, TOP_FIELD );
DECLARE_PICD( vpar_PictureData2IB, 1, I_CODING_TYPE, BOTTOM_FIELD );
DECLARE_PICD( vpar_PictureData2PB, 1, P_CODING_TYPE, BOTTOM_FIELD );
DECLARE_PICD( vpar_PictureData2BB, 1, B_CODING_TYPE, BOTTOM_FIELD );
#endif
