/*****************************************************************************
 * vpar_blocks.h : video parser blocks management
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vpar_blocks.h,v 1.4 2001/07/17 09:48:08 massiot Exp $
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
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "threads.h"
 *  "input.h"
 *  "video.h"
 *  "video_output.h"
 *  "decoder_fifo.h"
 *  "video_fifo.h"
 *****************************************************************************/

/*****************************************************************************
 * macroblock_parsing_t : macroblock context & predictors
 *****************************************************************************/
typedef struct
{
    unsigned char       i_quantizer_scale;        /* scale of the quantization
                                                   * matrices                */
    int                 pi_dc_dct_pred[3];          /* ISO/IEC 13818-2 7.2.1 */
    int                 pppi_pmv[2][2][2];  /* Motion vect predictors, 7.6.3 */
    int                 i_motion_dir;/* Used for the next skipped macroblock */

    /* Context used to optimize block parsing */
    int                 i_motion_type, i_mv_count, i_mv_format;
    boolean_t           b_dmv, b_dct_type;

    /* Coordinates of the upper-left pixel of the macroblock, in lum and
     * chroma */
    int                 i_l_x, i_l_y, i_c_x, i_c_y;
} macroblock_parsing_t;

/*****************************************************************************
 * lookup_t : entry type for lookup tables                                   *
 *****************************************************************************/
typedef struct lookup_s
{
    int    i_value;
    int    i_length;
} lookup_t;

/*****************************************************************************
 * ac_lookup_t : special entry type for lookup tables about ac coefficients
 *****************************************************************************/
typedef struct dct_lookup_s
{
    char   i_run;
    char   i_level;
    char   i_length;
} dct_lookup_t;

/*****************************************************************************
 * Standard codes
 *****************************************************************************/

/* Macroblock Address Increment types */
#define MB_ADDRINC_ESCAPE               8
#define MB_ADDRINC_STUFFING             15

/* Error constant for lookup tables */
#define MB_ERROR                        (-1)

/* Scan */
#define SCAN_ZIGZAG                     0
#define SCAN_ALT                        1

/* Constant for block decoding */
#define DCT_EOB                         64
#define DCT_ESCAPE                      65

/*****************************************************************************
 * Constants
 *****************************************************************************/
extern u8       pi_default_intra_quant[64];
extern u8       pi_default_nonintra_quant[64];
extern u8       pi_scan[2][64];

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vpar_InitCrop( struct vpar_thread_s* p_vpar );
void vpar_InitMbAddrInc( struct vpar_thread_s * p_vpar );
void vpar_InitPMBType( struct vpar_thread_s * p_vpar );
void vpar_InitBMBType( struct vpar_thread_s * p_vpar );
void vpar_InitCodedPattern( struct vpar_thread_s * p_vpar );
void vpar_InitDCTTables( struct vpar_thread_s * p_vpar );
void vpar_InitScanTable( struct vpar_thread_s * p_vpar );

typedef void (*f_picture_data_t)( struct vpar_thread_s * p_vpar,
                                  int i_mb_base );
#define PROTO_PICD( FUNCNAME )                                              \
void FUNCNAME( struct vpar_thread_s * p_vpar, int i_mb_base );

PROTO_PICD( vpar_PictureDataGENERIC )
#if (VPAR_OPTIM_LEVEL > 0)
PROTO_PICD( vpar_PictureData1I )
PROTO_PICD( vpar_PictureData1P )
PROTO_PICD( vpar_PictureData1B )
PROTO_PICD( vpar_PictureData1D )
PROTO_PICD( vpar_PictureData2IF )
PROTO_PICD( vpar_PictureData2PF )
PROTO_PICD( vpar_PictureData2BF )
#endif
#if (VPAR_OPTIM_LEVEL > 1)
PROTO_PICD( vpar_PictureData2IT )
PROTO_PICD( vpar_PictureData2PT )
PROTO_PICD( vpar_PictureData2BT )
PROTO_PICD( vpar_PictureData2IB )
PROTO_PICD( vpar_PictureData2PB )
PROTO_PICD( vpar_PictureData2BB )
#endif

