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
typedef void (*f_decode_block_t)( vpar_thread_t *, macroblock_t *, int );
static void vpar_DecodeMPEG1Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG1Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG2Non( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );
static void vpar_DecodeMPEG2Intra( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_b );

/*
 * Initialisation tables
 */
lookup_t pl_coded_pattern_init_table[512] = 
    { {MB_ERROR, 0}, {MB_ERROR, 0}, {39, 9}, {27, 9}, {59, 9}, {55, 9}, {47, 9}, {31, 9},
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

/* Fonction for filling up the lookup table for mb_addr_inc */
void __inline__ FillMbAddrIncTable( vpar_thread_t * p_vpar,
                                    int i_start, int i_end, int i_step, 
                                    int * pi_value, int i_length )
{
    int i_dummy, i_dummy2;
    for( i_dummy = i_start ; i_dummy < i_end ; i_dummy += i_step )
        for( i_dummy2 = 0 ; i_dummy2 < i_step ; i_dummy2 ++ )
        {
            p_vpar->pl_mb_addr_inc[i_dummy + i_dummy2].i_value = * pi_value;
            p_vpar->pl_mb_addr_inc[i_dummy + i_dummy2].i_length = i_length;
        }
    (*pi_value)--;
}
    
/* Fonction that initialize the table using the last one */
void InitMbAddrInc( vpar_thread_t * p_vpar )
{
    int i_dummy;
    int *  pi_value;
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
    pi_value = (int *) malloc( sizeof( int ) );
    * pi_value = 33;
    FillMbAddrIncTable( p_vpar, 1024, 2048, 1024, pi_value, 1 );
    FillMbAddrIncTable( p_vpar, 512, 1024, 256, pi_value, 3 );
    FillMbAddrIncTable( p_vpar, 256, 512, 128, pi_value, 4 );
    FillMbAddrIncTable( p_vpar, 128, 256, 64, pi_value, 5 );
    FillMbAddrIncTable( p_vpar, 96, 128, 16, pi_value, 7 );
    FillMbAddrIncTable( p_vpar, 48, 96, 8, pi_value, 8 );
    FillMbAddrIncTable( p_vpar, 36, 48, 2, pi_value, 10 );
    FillMbAddrIncTable( p_vpar, 24, 36, 1, pi_value, 11 );
}
/*****************************************************************************
 * InitDCT : Initialize tables giving the length of the dct coefficient
 *           from the vlc code
 *****************************************************************************/

void InitDCTTables( vpar_thread_t * p_vpar )
{
#if 0
    /* Tables are cut in two parts to reduce memory occupation */
    
    /* Table B-12, dct_dc_size_luminance, codes 00xxx ... 11110 */
    p_vpar->pppl_dct_dc_size[0][0] =
    { {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
      {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
      {0, 3}, {0, 3}, {0, 3}, {0, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3},
      {4, 3}, {4, 3}, {4, 3}, {4, 3}, {5, 4}, {5, 4}, {6, 5}, {MB_ERROR, 0}
    };

    /* Table B-12, dct_dc_size_luminance, codes 111110xxx ... 111111111 */
    p_vpar->pppl_dct_dc_lum_size[1][0] =
    { {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6},
      {8, 7}, {8, 7}, {8, 7}, {8, 7}, {9, 8}, {9, 8}, {10,9}, {11,9}
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0},
      {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}, {MB_ERROR, 0}
    };

    /* Table B-13, dct_dc_size_chrominance, codes 00xxx ... 11110 */
    p_vpar->pppl_dct_dc_chrom_size[0][1] =
    { {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
      {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
      {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
      {3, 3}, {3, 3}, {3, 3}, {3, 3}, {4, 4}, {4, 4}, {5, 5}, {MB_ERROR, 0}
    };

    /* Table B-13, dct_dc_size_chrominance, codes 111110xxxx ... 1111111111 */
    p_vpar->pppl_dct_dc_size[1][1] =
    { {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
      {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
      {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7},
      {8, 8}, {8, 8}, {8, 8}, {8, 8}, {9, 9}, {9, 9}, {10,10}, {11,10}
    };
#endif
}

/*****************************************************************************
 * Init*MBType : Initialize lookup table for the Macroblock type
 * ***************************************************************************/

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
        p_vpar->pl_mb_type[i_mb_type][i_dummy].i_value = i_value;
        p_vpar->pl_mb_type[i_mb_type][i_dummy].i_length = i_length;
    }
}

/* Fonction that fills the table for P MB_Type */
void InitPMBType( vpar_thread_t * p_vpar )
{
    FillMBType( p_vpar, 0, 32, 64, MB_MOTION_FORWARD|MB_PATTERN, 1 );
    FillMBType( p_vpar, 0, 16, 32, MB_PATTERN, 2 );
    FillMBType( p_vpar, 0, 8, 16, MB_MOTION_FORWARD, 3 );
    FillMBType( p_vpar, 0, 6, 8, MB_INTRA, 5 );
    FillMBType( p_vpar, 0, 4, 6, MB_QUANT|MB_MOTION_FORWARD|MB_PATTERN, 5 );
    FillMBType( p_vpar, 0, 2, 4, MB_QUANT|MB_PATTERN, 5 );
    p_vpar->pl_mb_type[0][1].i_value = MB_QUANT|MB_INTRA;
    p_vpar->pl_mb_type[0][1].i_length = 6;
    p_vpar->pl_mb_type[0][0].i_value = MB_ERROR;
    p_vpar->pl_mb_type[0][0].i_length = 0;
}

/* Fonction that fills the table for B MB_Type */
void InitBMBType( vpar_thread_t * p_vpar )
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
    p_vpar->pl_mb_type[1][3].i_value = MB_QUANT|MB_MOTION_FORWARD|MB_PATTERN;
    p_vpar->pl_mb_type[1][3].i_length = 6;
    p_vpar->pl_mb_type[1][2].i_value = MB_QUANT|MB_MOTION_BACKWARD|MB_PATTERN;
    p_vpar->pl_mb_type[1][2].i_length = 6;
    p_vpar->pl_mb_type[1][1].i_value = MB_QUANT|MB_INTRA;
    p_vpar->pl_mb_type[1][1].i_length = 6;
    p_vpar->pl_mb_type[1][0].i_value =MB_ERROR;
    p_vpar->pl_mb_type[1][0].i_length = 0;
}

/*****************************************************************************
 * InitCodedPattern : Initialize the lookup table for decoding 
 *                    coded block pattern
 *****************************************************************************/
void InitCodedPattern( vpar_thread_t * p_vpar )
{
    memcpy( p_vpar->pl_coded_pattern, pl_coded_pattern_init_table ,
            sizeof(pl_coded_pattern_init_table) );
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

        /* Set the field we use for motion compensation */
        p_mb->ppi_field_select[0][0] = p_mb->ppi_field_select[0][1]
                                     = ( p_vpar->picture.i_current_structure == BOTTOM_FIELD );
        
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
        (*p_vpar->sequence.pf_decode_mv)( p_vpar, p_mb, 0 );
    }

    if( p_vpar->mb.i_mb_type & MB_MOTION_BACKWARD )
    {
        (*p_vpar->sequence.pf_decode_mv)( p_vpar, p_mb, 1 );
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
    p_data1 = (elem_t*) p_mb->p_picture->p_y;
             // + p_mb->i_l_x + p_mb->i_l_y*(p_vpar->sequence.i_width);

    for( i_b = 0 ; i_b < 4 ; i_b++, i_mask >>= 1 )
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
    p_data1 = (elem_t*) p_mb->p_picture->p_u
              + (p_mb->i_c_x >> pi_chroma_hor[p_vpar->sequence.i_chroma_format])
              + (p_mb->i_c_y >> pi_chroma_ver[p_vpar->sequence.i_chroma_format])
                * (p_vpar->sequence.i_chroma_width);
    p_data2 = (elem_t*) p_mb->p_picture->p_v
              + (p_mb->i_c_x >> pi_chroma_hor[p_vpar->sequence.i_chroma_format])
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
}

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
    p_mb->i_l_x = p_vpar->mb.i_l_x;
    p_mb->i_l_y = p_vpar->mb.i_l_y;
    p_mb->i_c_x = p_vpar->mb.i_c_x;
    p_mb->i_c_y = p_vpar->mb.i_c_y;
    p_mb->i_chroma_nb_blocks = p_vpar->sequence.i_chroma_nb_blocks;
    p_mb->pf_chroma_motion = pf_chroma_motion[p_vpar->sequence.i_chroma_format];

    p_mb->p_forward = p_vpar->sequence.p_forward;
    p_mb->p_backward = p_vpar->sequence.p_backward;
    
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
    p_vpar->mb.i_addr_inc += p_vpar->pl_mb_addr_inc[i_index].i_value;
    /* Dump the good number of bits */
    DumpBits( &p_vpar->bit_stream, p_vpar->pl_mb_addr_inc[i_index].i_length );
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
    p_vpar->mb.i_mb_type = (p_vpar->picture.pf_macroblock_type)( p_vpar );

    /* SCALABILITY : warning, we don't know if spatial_temporal_weight_code
     * has to be dropped, take care if you use scalable streams. */
    /* DumpBits( &p_vpar->bit_stream, 2 ); */
    
    if( (p_vpar->picture.i_coding_type == P_CODING_TYPE) &&
        !(p_vpar->mb.i_mb_type & (MB_MOTION_FORWARD|MB_INTRA)) )
    {
        /* Special No-MC macroblock in P pictures (7.6.3.5). */
        memset( p_vpar->slice.pppi_pmv, 0, 8*sizeof(int) );
        memset( p_mb->pppi_motion_vectors, 0, 8*sizeof(int) );
        
        p_vpar->mb.i_motion_type = MOTION_FRAME;
        p_mb->ppi_field_select[0][0] = ( p_vpar->picture.i_current_structure == BOTTOM_FIELD );
    }
    else if( !(p_vpar->mb.i_mb_type & (MB_MOTION_FORWARD | MB_MOTION_BACKWARD))
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

     if( p_vpar->mb.i_mb_type & MB_INTRA )
    {
        /* For the intra macroblocks, we use an empty motion
         * compensation function */
        p_mb->pf_motion = vdec_DummyRecon;
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
        if( p_vpar->mb.b_dct_type = GetBits( &p_vpar->bit_stream, 1 ) )
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
                                          {MB_INTRA, 2} };
    /* Dump the good number of bits */
    DumpBits( &p_vpar->bit_stream, pl_mb_Itype[i_type].i_length );
    return pl_mb_Itype[i_type].i_value;
}

/*****************************************************************************
 * vpar_PMBType : macroblock_type in P pictures
 *****************************************************************************/
int vpar_PMBType( vpar_thread_t * p_vpar )
{
    /* Testing on 6 bits */
    int                i_type = ShowBits( &p_vpar->bit_stream, 6 );
    /* Dump the good number of bits */
    DumpBits( &p_vpar->bit_stream, p_vpar->pl_mb_type[0][i_type].i_length );
    /* return the value from the lookup table for P type */
    return p_vpar->pl_mb_type[0][i_type].i_value;
}

/*****************************************************************************
 * vpar_BMBType : macroblock_type in B pictures
 *****************************************************************************/
int vpar_BMBType( vpar_thread_t * p_vpar )
{
     /* Testing on 6 bits */
    int                i_type = ShowBits( &p_vpar->bit_stream, 6 );
    /* Dump the good number of bits */
    DumpBits( &p_vpar->bit_stream, p_vpar->pl_mb_type[1][i_type].i_length );
    /* return the value from the lookup table for B type */
    return p_vpar->pl_mb_type[1][i_type].i_value;
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
    int      i_vlc = ShowBits( &p_vpar->bit_stream, 9 );
    DumpBits( &p_vpar->bit_stream, p_vpar->pl_coded_pattern[i_vlc].i_length );
    return p_vpar->pl_coded_pattern[i_vlc].i_value;
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
    /* Lookup Table for the chromatic component */
    static int pi_cc_index[12] = { 0, 0, 0, 0, 1, 2, 1, 2, 1, 2 };
    int        i_cc = pi_cc_index[i_b];
    /* Determine whether it is luminance or not (chrominance) */
    int        i_type = ( i_cc + 1 ) / 2;

    /* Decoding of the DC intra coefficient */
    /* The nb of bits to parse depends on i_type */
    int        i_code = ShowBits( &p_vpar->bit_stream, 9 + i_type );
    /* To reduce memory occupation, there are two lookup tables
     * See InitDCT above */
    int        i_code5 = i_code >> 4;
    /* Shall we lookup in the first or in the second table ? */
    int        i_select = ( i_code5 - 1 ) / 31;
    /* Offset value for looking in the second table */
    int        i_offset = 0x1f0 + ( i_type * 0x1f0 );
    int        i_pos = i_code5 * ( ! i_select ) +
                       ( i_code - i_offset ) * i_select;
    int        i_dct_dc_size;
    int        i_dct_dc_diff;
    i_dct_dc_size = p_vpar->pppl_dct_dc_size[i_select][i_type][i_pos].i_value;
    /* Dump the variable length code */
    DumpBits( &p_vpar->bit_stream, 
              p_vpar->pppl_dct_dc_size[i_select][i_type][i_pos].i_length );
    i_dct_dc_diff = GetBits( &p_vpar->bit_stream, i_dct_dc_size );
    p_vpar->slice.pi_dc_dct_pred[i_cc] += i_dct_dc_diff;
    
    /* Decoding of the AC coefficients */
    //int        i_dummy = 1;
    
}
