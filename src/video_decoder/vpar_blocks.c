/*****************************************************************************
 * vpar_blocks.c : blocks parsing
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vpar_blocks.c,v 1.7 2001/08/23 10:08:26 massiot Exp $
 *
 * Authors: Michel Lespinasse <walken@zoy.org>
 *          Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include "vdec_ext-plugins.h"
#include "vpar_pool.h"
#include "video_parser.h"

#include "vpar_blocks.h"

/*
 * Welcome to vpar_blocks.c ! Here's where the heavy processor-critical parsing
 * task is done. This file is divided in several parts :
 *  - Decoding of coded blocks
 *  - Decoding of motion vectors
 *  - Decoding of the other macroblock structures
 *  - Picture data parsing management (slices and error handling)
 * It's a pretty long file. Good luck and have a nice day.
 */


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

/*****************************************************************************
 * GetLumaDCDiff : Get the luminance DC coefficient difference
 *****************************************************************************/
static __inline__ int GetLumaDCDiff( vpar_thread_t * p_vpar )
{
    lookup_t *  p_tab;
    int         i_size, i_dc_diff, i_code;

    if( (i_code = ShowBits( &p_vpar->bit_stream, 5 )) < 0x1F )
    {
        p_tab = DC_lum_5 + i_code;
        i_size = p_tab->i_value;
        if( i_size )
        {
            RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
            i_dc_diff = GetBits( &p_vpar->bit_stream, i_size );
            if ((i_dc_diff & (1 << (i_size - 1))) == 0)
            {
                i_dc_diff -= (1 << i_size) - 1;
            }
            return( i_dc_diff );
        }
        else
        {
            RemoveBits( &p_vpar->bit_stream, 3 );
            return 0;
        }
    }
    else
    {
        p_tab = DC_long - 0x1e0 + ShowBits( &p_vpar->bit_stream, 9 );
        RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
        i_size = p_tab->i_value;
        i_dc_diff = GetBits( &p_vpar->bit_stream, i_size );
        if ((i_dc_diff & (1 << (i_size - 1))) == 0)
        {
            i_dc_diff -= (1 << i_size) - 1;
        }
        return( i_dc_diff );
    }
}

/*****************************************************************************
 * GetChromaDCDiff : Get the chrominance DC coefficient difference
 *****************************************************************************/
static __inline__ int GetChromaDCDiff( vpar_thread_t * p_vpar )
{
    lookup_t *  p_tab;
    int         i_size, i_dc_diff, i_code;

    if( (i_code = ShowBits( &p_vpar->bit_stream, 5 )) < 0x1F )
    {
        p_tab = DC_chrom_5 + i_code;
        i_size = p_tab->i_value;
        if( i_size )
        {
            RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
            i_dc_diff = GetBits( &p_vpar->bit_stream, i_size );
            if ((i_dc_diff & (1 << (i_size - 1))) == 0)
            {
                i_dc_diff -= (1 << i_size) - 1;
            }
            return( i_dc_diff );
        }
        else
        {
            RemoveBits( &p_vpar->bit_stream, 2 );
            return 0;
        }
    }
    else
    {
        p_tab = DC_long - 0x3e0 + ShowBits( &p_vpar->bit_stream, 10 );
        RemoveBits( &p_vpar->bit_stream, p_tab->i_length + 1 );
        i_size = p_tab->i_value;
        i_dc_diff = GetBits( &p_vpar->bit_stream, i_size );
        if ((i_dc_diff & (1 << (i_size - 1))) == 0)
        {
            i_dc_diff -= (1 << i_size) - 1;
        }
        return( i_dc_diff );
    }
}


#define SATURATE(val)                                                       \
    if ((u32)(val + 2048) > 4095)                                           \
    {                                                                       \
       val = (val > 0) ? 2047 : -2048;                                      \
    }

/*****************************************************************************
 * MPEG2IntraB14 : Decode an intra block according to ISO/IEC 13818-2 table B14
 *****************************************************************************/
static void MPEG2IntraB14( vpar_thread_t * p_vpar, idct_inner_t * p_idct )
{
    int         i_coeff, i_mismatch, i_code, i_pos, i_value, i_nc;
    s32         i_sign;
    dct_lookup_t * p_tab;

    int         i_q_scale = p_vpar->mb.i_quantizer_scale;
    u8 *        pi_quant = p_vpar->sequence.intra_quant.pi_matrix;
    dctelem_t * p_dest = p_idct->pi_block;
    u8 *        p_scan = p_vpar->picture.pi_scan;

    i_coeff = 0;
    i_mismatch = ~p_dest[0];
    i_nc = (p_dest[0] != 0);

    for( ; ; )
    {
        if( (i_code = ShowBits( &p_vpar->bit_stream, 5 )) >= 0x5 )
        {
            p_tab = DCT_B14AC_5 - 5 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff >= 64 )
            {
                /* End of block */
                break;
            }

store_coeff:
            i_nc++;
            i_pos = p_scan[ i_coeff ];
            RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
            i_value = (p_tab->i_level * i_q_scale * pi_quant[i_pos])
                            >> 4;

            i_sign = GetSignedBits( &p_vpar->bit_stream, 1 );
            /* if (i_sign) i_value = -i_value; */
            i_value = (i_value ^ i_sign) - i_sign;

            SATURATE( i_value );
            p_dest[i_pos] = i_value;
            i_mismatch ^= i_value;

            continue;
        }
        else if( (i_code = ShowBits( &p_vpar->bit_stream, 8 )) >= 0x4 )
        {
            p_tab = DCT_B14_8 - 4 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                /* Normal coefficient */
                goto store_coeff;
            }

            /* Escape code */
            i_coeff += (GetBits( &p_vpar->bit_stream, 12 ) & 0x3F) - 64;
            if( i_coeff >= 64 )
            {
                /* Illegal, but needed to avoid overflow */
                intf_WarnMsg( 2, "Intra-B14 coeff is out of bound" );
                p_vpar->picture.b_error = 1;
                break;
            }

            i_nc++;
            i_pos = p_scan[i_coeff];
            i_value = (GetSignedBits( &p_vpar->bit_stream, 12 )
                        * i_q_scale * pi_quant[i_pos]) / 16;

            SATURATE( i_value );
            p_dest[i_pos] = i_value;
            i_mismatch ^= i_value;
            continue;
        }
        else if( (i_code = ShowBits( &p_vpar->bit_stream, 16)) >= 0x0200 )
        {
            p_tab = DCT_B14_10 - 8 + (i_code >> 6);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else if( i_code >= 0x0080 )
        {
            p_tab = DCT_13 - 16 + (i_code >> 3);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else if( i_code >= 0x0020 )
        {
            p_tab = DCT_15 - 16 + (i_code >> 1);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else
        {
            p_tab = DCT_16 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }

        intf_WarnMsg( 2, "Intra-B14 coeff is out of bound" );
        p_vpar->picture.b_error = 1;
        break;
    }

    p_dest[63] ^= i_mismatch & 1;
    RemoveBits( &p_vpar->bit_stream, 2 ); /* End of Block */

    if( i_nc <= 1 )
    {
        if( p_dest[63] )
        {
            if( i_nc == 0 )
            {
                p_idct->pf_idct = p_vpar->pf_sparse_idct;
                p_idct->i_sparse_pos = 63;
            }
            else
            {
                p_idct->pf_idct = p_vpar->pf_idct;
            }
        }
        else 
        {
            p_idct->pf_idct = p_vpar->pf_sparse_idct;
            p_idct->i_sparse_pos = i_coeff - p_tab->i_run;
        }
    }
    else
    {
        p_idct->pf_idct = p_vpar->pf_idct;
    }
}

/*****************************************************************************
 * MPEG2IntraB15 : Decode an intra block according to ISO/IEC 13818-2 table B15
 *****************************************************************************/
static void MPEG2IntraB15( vpar_thread_t * p_vpar, idct_inner_t * p_idct )
{
    int         i_coeff, i_mismatch, i_code, i_pos, i_value, i_nc;
    s32         i_sign;
    dct_lookup_t * p_tab;

    int         i_q_scale = p_vpar->mb.i_quantizer_scale;
    u8 *        pi_quant = p_vpar->sequence.intra_quant.pi_matrix;
    dctelem_t * p_dest = p_idct->pi_block;
    u8 *        p_scan = p_vpar->picture.pi_scan;

    i_coeff = 0;
    i_mismatch = ~p_dest[0];
    i_nc = (p_dest[0] != 0);

    for( ; ; )
    {
        if( (i_code = ShowBits( &p_vpar->bit_stream, 8 )) >= 0x4 )
        {
            p_tab = DCT_B15_8 - 4 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {

store_coeff:
                i_nc++;
                i_pos = p_scan[ i_coeff ];
                RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
                i_value = (p_tab->i_level * i_q_scale * pi_quant[i_pos])
                                >> 4;

                i_sign = GetSignedBits( &p_vpar->bit_stream, 1 );
                /* if (i_sign) i_value = -i_value; */
                i_value = (i_value ^ i_sign) - i_sign;

                SATURATE( i_value );
                p_dest[i_pos] = i_value;
                i_mismatch ^= i_value;

                continue;
            }
            else
            {
                if( i_coeff >= 128 )
                {
                    /* End of block */
                    break;
                }

                /* Escape code */
                i_coeff += (GetBits( &p_vpar->bit_stream, 12 ) & 0x3F) - 64;
                if( i_coeff >= 64 )
                {
                    /* Illegal, but needed to avoid overflow */
                    intf_WarnMsg( 2, "Intra-B15 coeff is out of bound" );
                    p_vpar->picture.b_error = 1;
                    break;
                }

                i_nc++;
                i_pos = p_scan[i_coeff];
                i_value = (GetSignedBits( &p_vpar->bit_stream, 12 )
                            * i_q_scale * pi_quant[i_pos]) / 16;

                SATURATE( i_value );
                p_dest[i_pos] = i_value;
                i_mismatch ^= i_value;
                continue;
            }
        }
        else if( (i_code = ShowBits( &p_vpar->bit_stream, 16)) >= 0x0200 )
        {
            p_tab = DCT_B15_10 - 8 + (i_code >> 6);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else if( i_code >= 0x0080 )
        {
            p_tab = DCT_13 - 16 + (i_code >> 3);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else if( i_code >= 0x0020 )
        {
            p_tab = DCT_15 - 16 + (i_code >> 1);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else
        {
            p_tab = DCT_16 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }

        intf_WarnMsg( 2, "Intra-B15 coeff is out of bound" );
        p_vpar->picture.b_error = 1;
        break;
    }

    p_dest[63] ^= i_mismatch & 1;
    RemoveBits( &p_vpar->bit_stream, 4 ); /* End of Block */

    if( i_nc <= 1 )
    {
        if( p_dest[63] )
        {
            if( i_nc == 0 )
            {
                p_idct->pf_idct = p_vpar->pf_sparse_idct;
                p_idct->i_sparse_pos = 63;
            }
            else
            {
                p_idct->pf_idct = p_vpar->pf_idct;
            }
        }
        else 
        {
            p_idct->pf_idct = p_vpar->pf_sparse_idct;
            p_idct->i_sparse_pos = i_coeff - p_tab->i_run;
        }
    }
    else
    {
        p_idct->pf_idct = p_vpar->pf_idct;
    }
}

/*****************************************************************************
 * MPEG2NonIntra : Decode a non-intra MPEG-2 block
 *****************************************************************************/
static void MPEG2NonIntra( vpar_thread_t * p_vpar, idct_inner_t * p_idct )
{
    int         i_coeff, i_mismatch, i_code, i_pos, i_value, i_nc;
    s32         i_sign;
    dct_lookup_t * p_tab;

    int         i_q_scale = p_vpar->mb.i_quantizer_scale;
    u8 *        pi_quant = p_vpar->sequence.nonintra_quant.pi_matrix;
    dctelem_t * p_dest = p_idct->pi_block;
    u8 *        p_scan = p_vpar->picture.pi_scan;
static int meuh = 0;
meuh++;
    if( meuh == 3745 )
        i_coeff = 0;

    i_coeff = -1;
    i_mismatch = 1;
    i_nc = 0;

    if( (i_code = ShowBits( &p_vpar->bit_stream, 5 )) >= 0x5 )
    {
        p_tab = DCT_B14DC_5 - 5 + i_code;
        goto coeff_1;
    }
    else
    {
        goto coeff_2;
    }

    for( ; ; )
    {
        if( (i_code = ShowBits( &p_vpar->bit_stream, 5 )) >= 0x5 )
        {
            p_tab = DCT_B14AC_5 - 5 + i_code;
coeff_1:
            i_coeff += p_tab->i_run;
            if( i_coeff >= 64 )
            {
                /* End of block */
                break;
            }

store_coeff:
            i_nc++;
            i_pos = p_scan[ i_coeff ];
            RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
            i_value = ((2 * p_tab->i_level + 1) * i_q_scale * pi_quant[i_pos])
                            >> 5;

            i_sign = GetSignedBits( &p_vpar->bit_stream, 1 );
            /* if (i_sign) i_value = -i_value; */
            i_value = (i_value ^ i_sign) - i_sign;

            SATURATE( i_value );
            p_dest[i_pos] = i_value;
            i_mismatch ^= i_value;

            continue;
        }
coeff_2:
        if( (i_code = ShowBits( &p_vpar->bit_stream, 8 )) >= 0x4 )
        {
            p_tab = DCT_B14_8 - 4 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                /* Normal coefficient */
                goto store_coeff;
            }

            /* Escape code */
            i_coeff += (GetBits( &p_vpar->bit_stream, 12 ) & 0x3F) - 64;
            if( i_coeff >= 64 )
            {
                /* Illegal, but needed to avoid overflow */
                intf_WarnMsg( 2, "MPEG2NonIntra coeff is out of bound" );
                p_vpar->picture.b_error = 1;
                break;
            }

            i_nc++;
            i_pos = p_scan[i_coeff];
            i_value = 2 * (ShowSignedBits( &p_vpar->bit_stream, 1 )
                            + GetSignedBits( &p_vpar->bit_stream, 12 )) + 1;

            i_value = (i_value * i_q_scale * pi_quant[i_pos]) / 32;

            SATURATE( i_value );
            p_dest[i_pos] = i_value;
            i_mismatch ^= i_value;
            continue;
        }
        else if( (i_code = ShowBits( &p_vpar->bit_stream, 16)) >= 0x0200 )
        {
            p_tab = DCT_B14_10 - 8 + (i_code >> 6);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else if( i_code >= 0x0080 )
        {
            p_tab = DCT_13 - 16 + (i_code >> 3);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else if( i_code >= 0x0020 )
        {
            p_tab = DCT_15 - 16 + (i_code >> 1);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else
        {
            p_tab = DCT_16 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }

        intf_WarnMsg( 2, "MPEG2NonIntra coeff is out of bound" );
        p_vpar->picture.b_error = 1;
        break;
    }

    p_dest[63] ^= i_mismatch & 1;
    RemoveBits( &p_vpar->bit_stream, 2 ); /* End of Block */

    if( i_nc <= 1 )
    {
        if( p_dest[63] )
        {
            if( i_nc == 0 )
            {
                p_idct->pf_idct = p_vpar->pf_sparse_idct;
                p_idct->i_sparse_pos = 63;
            }
            else
            {
                p_idct->pf_idct = p_vpar->pf_idct;
            }
        }
        else 
        {
            p_idct->pf_idct = p_vpar->pf_sparse_idct;
            if( i_nc == 0 )
            {
                p_idct->i_sparse_pos = 0;
            }
            else
            {
                p_idct->i_sparse_pos = i_coeff - p_tab->i_run;
            }
        }
    }
    else
    {
        p_idct->pf_idct = p_vpar->pf_idct;
    }
}

/*****************************************************************************
 * MPEG1Intra : Decode an MPEG-1 intra block
 *****************************************************************************/
static void MPEG1Intra( vpar_thread_t * p_vpar, idct_inner_t * p_idct )
{
    int         i_coeff, i_code, i_pos, i_value, i_nc;
    s32         i_sign;
    dct_lookup_t * p_tab;

    int         i_q_scale = p_vpar->mb.i_quantizer_scale;
    u8 *        pi_quant = p_vpar->sequence.intra_quant.pi_matrix;
    dctelem_t * p_dest = p_idct->pi_block;
    u8 *        p_scan = p_vpar->picture.pi_scan;

    i_coeff = 0;
    i_nc = (p_dest[0] != 0);

    for( ; ; )
    {
        if( (i_code = ShowBits( &p_vpar->bit_stream, 5 )) >= 0x5 )
        {
            p_tab = DCT_B14AC_5 - 5 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff >= 64 )
            {
                /* End of block */
                break;
            }

store_coeff:
            i_nc++;
            i_pos = p_scan[ i_coeff ];
            RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
            i_value = (p_tab->i_level * i_q_scale * pi_quant[i_pos])
                            >> 4;

            /* Oddification */
            i_value = (i_value - 1) | 1;

            i_sign = GetSignedBits( &p_vpar->bit_stream, 1 );
            /* if (i_sign) i_value = -i_value; */
            i_value = (i_value ^ i_sign) - i_sign;

            SATURATE( i_value );
            p_dest[i_pos] = i_value;

            continue;
        }
        else if( (i_code = ShowBits( &p_vpar->bit_stream, 8 )) >= 0x4 )
        {
            p_tab = DCT_B14_8 - 4 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                /* Normal coefficient */
                goto store_coeff;
            }

            /* Escape code */
            i_coeff += (GetBits( &p_vpar->bit_stream, 12 ) & 0x3F) - 64;
            if( i_coeff >= 64 )
            {
                /* Illegal, but needed to avoid overflow */
                intf_WarnMsg( 2, "MPEG1Intra coeff is out of bound" );
                p_vpar->picture.b_error = 1;
                break;
            }

            i_nc++;
            i_pos = p_scan[i_coeff];
            
            i_value = ShowSignedBits( &p_vpar->bit_stream, 8 );
            if( !(i_value & 0x7F) )
            {
                RemoveBits( &p_vpar->bit_stream, 8 );
                i_value = ShowBits( &p_vpar->bit_stream, 8 ) + 2 * i_value;
            }

            i_value = (i_value * i_q_scale * pi_quant[i_pos]) / 16;

            /* Oddification */
            i_value = (i_value + ~ShowSignedBits( &p_vpar->bit_stream, 1 )) | 1;

            SATURATE( i_value );
            p_dest[i_pos] = i_value;
            RemoveBits( &p_vpar->bit_stream, 8 );
            continue;
        }
        else if( (i_code = ShowBits( &p_vpar->bit_stream, 16)) >= 0x0200 )
        {
            p_tab = DCT_B14_10 - 8 + (i_code >> 6);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else if( i_code >= 0x0080 )
        {
            p_tab = DCT_13 - 16 + (i_code >> 3);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else if( i_code >= 0x0020 )
        {
            p_tab = DCT_15 - 16 + (i_code >> 1);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else
        {
            p_tab = DCT_16 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }

        intf_WarnMsg( 2, "MPEG1Intra coeff is out of bound" );
        p_vpar->picture.b_error = 1;
        break;
    }

    RemoveBits( &p_vpar->bit_stream, 2 ); /* End of Block */

    if( i_nc <= 1 )
    {
        p_idct->pf_idct = p_vpar->pf_sparse_idct;
        p_idct->i_sparse_pos = i_coeff - p_tab->i_run;
    }
    else
    {
        p_idct->pf_idct = p_vpar->pf_idct;
    }
}

/*****************************************************************************
 * MPEG1NonIntra : Decode a non-intra MPEG-1 block
 *****************************************************************************/
static void MPEG1NonIntra( vpar_thread_t * p_vpar, idct_inner_t * p_idct )
{
    int         i_coeff, i_code, i_pos, i_value, i_nc;
    s32         i_sign;
    dct_lookup_t * p_tab;

    int         i_q_scale = p_vpar->mb.i_quantizer_scale;
    u8 *        pi_quant = p_vpar->sequence.nonintra_quant.pi_matrix;
    dctelem_t * p_dest = p_idct->pi_block;
    u8 *        p_scan = p_vpar->picture.pi_scan;

    i_coeff = -1;
    i_nc = 0;

    if( (i_code = ShowBits( &p_vpar->bit_stream, 5 )) >= 0x5 )
    {
        p_tab = DCT_B14DC_5 - 5 + i_code;
        goto coeff_1;
    }
    else
    {
        goto coeff_2;
    }

    for( ; ; )
    {
        if( (i_code = ShowBits( &p_vpar->bit_stream, 5 )) >= 0x5 )
        {
            p_tab = DCT_B14AC_5 - 5 + i_code;
coeff_1:
            i_coeff += p_tab->i_run;
            if( i_coeff >= 64 )
            {
                /* End of block */
                break;
            }

store_coeff:
            i_nc++;
            i_pos = p_scan[ i_coeff ];
            RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
            i_value = ((2 * p_tab->i_level + 1) * i_q_scale * pi_quant[i_pos])
                            >> 5;

            /* Oddification */
            i_value = (i_value - 1) | 1;

            i_sign = GetSignedBits( &p_vpar->bit_stream, 1 );
            /* if (i_sign) i_value = -i_value; */
            i_value = (i_value ^ i_sign) - i_sign;

            SATURATE( i_value );
            p_dest[i_pos] = i_value;

            continue;
        }
coeff_2:
        if( (i_code = ShowBits( &p_vpar->bit_stream, 8 )) >= 0x4 )
        {
            p_tab = DCT_B14_8 - 4 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                /* Normal coefficient */
                goto store_coeff;
            }

            /* Escape code */
            i_coeff += (GetBits( &p_vpar->bit_stream, 12 ) & 0x3F) - 64;
            if( i_coeff >= 64 )
            {
                /* Illegal, but needed to avoid overflow */
                intf_WarnMsg( 2, "MPEG1NonIntra coeff is out of bound" );
                p_vpar->picture.b_error = 1;
                break;
            }

            i_nc++;
            i_pos = p_scan[i_coeff];
            i_value = ShowSignedBits( &p_vpar->bit_stream, 8 );
            if( !(i_value & 0x7F) )
            {
                RemoveBits( &p_vpar->bit_stream, 8 );
                i_value = ShowBits( &p_vpar->bit_stream, 8 ) + 2 * i_value;
            }
            i_sign = ShowSignedBits( &p_vpar->bit_stream, 1 );
            i_value = 2 * (i_sign + i_value) + 1;
            i_value = (i_value * i_q_scale * pi_quant[i_pos]) / 32;

            /* Oddification */
            i_value = (i_value + ~i_sign) | 1;

            SATURATE( i_value );
            p_dest[i_pos] = i_value;
            RemoveBits( &p_vpar->bit_stream, 8 );
            continue;
        }
        else if( (i_code = ShowBits( &p_vpar->bit_stream, 16)) >= 0x0200 )
        {
            p_tab = DCT_B14_10 - 8 + (i_code >> 6);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else if( i_code >= 0x0080 )
        {
            p_tab = DCT_13 - 16 + (i_code >> 3);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else if( i_code >= 0x0020 )
        {
            p_tab = DCT_15 - 16 + (i_code >> 1);
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }
        else
        {
            p_tab = DCT_16 + i_code;
            i_coeff += p_tab->i_run;
            if( i_coeff < 64 )
            {
                goto store_coeff;
            }
        }

        intf_WarnMsg( 2, "MPEG1NonIntra coeff is out of bound" );
        p_vpar->picture.b_error = 1;
        break;
    }

    RemoveBits( &p_vpar->bit_stream, 2 ); /* End of Block */

    if( i_nc <= 1 )
    {
        p_idct->pf_idct = p_vpar->pf_sparse_idct;
        if( i_nc == 0 )
        {
            p_idct->i_sparse_pos = 0;
        }
        else
        {
            p_idct->i_sparse_pos = i_coeff - p_tab->i_run;
        }
    }
    else
    {
        p_idct->pf_idct = p_vpar->pf_idct;
    }
}

#undef SATURATE

/*****************************************************************************
 * *MB : decode all blocks of the macroblock
 *****************************************************************************/
#define DECODE_LUMABLOCK( i_b, p_dest, PF_MBFUNC )                          \
    p_idct = &p_mb->p_idcts[i_b];                                           \
    memset( p_idct->pi_block, 0, 64*sizeof(dctelem_t) );                    \
    p_idct->p_dct_data = p_dest;                                            \
    p_vpar->mb.pi_dc_dct_pred[0] += GetLumaDCDiff( p_vpar );                \
    p_idct->pi_block[0] = p_vpar->mb.pi_dc_dct_pred[0]                      \
                         << (3 - p_vpar->picture.i_intra_dc_precision );    \
    PF_MBFUNC( p_vpar, p_idct );

#define DECLARE_INTRAMB( PSZ_NAME, PF_MBFUNC )                              \
static __inline__ void PSZ_NAME( vpar_thread_t * p_vpar,                    \
                                 macroblock_t * p_mb )                      \
{                                                                           \
    int             i_dct_offset;                                           \
    yuv_data_t *    p_lum_dest;                                             \
    idct_inner_t *  p_idct;                                                 \
                                                                            \
    p_lum_dest = p_mb->pp_dest[0] + p_vpar->mb.i_offset;                    \
                                                                            \
    if( p_mb->i_mb_modes & DCT_TYPE_INTERLACED )                            \
    {                                                                       \
        i_dct_offset = p_vpar->picture.i_field_width;                       \
        p_mb->i_lum_dct_stride = p_vpar->picture.i_field_width * 2;         \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        i_dct_offset = p_vpar->picture.i_field_width * 8;                   \
        p_mb->i_lum_dct_stride = p_vpar->picture.i_field_width;             \
    }                                                                       \
    p_mb->i_chrom_dct_stride = p_vpar->picture.i_field_width >> 1;          \
                                                                            \
    DECODE_LUMABLOCK( 0, p_lum_dest, PF_MBFUNC );                           \
    DECODE_LUMABLOCK( 1, p_lum_dest + 8, PF_MBFUNC );                       \
    DECODE_LUMABLOCK( 2, p_lum_dest + i_dct_offset, PF_MBFUNC );            \
    DECODE_LUMABLOCK( 3, p_lum_dest + i_dct_offset + 8, PF_MBFUNC );        \
                                                                            \
    p_idct = &p_mb->p_idcts[4];                                             \
    memset( p_idct->pi_block, 0, 64*sizeof(dctelem_t) );                    \
    p_idct->p_dct_data = p_mb->pp_dest[1] + (p_vpar->mb.i_offset >> 1);     \
    p_vpar->mb.pi_dc_dct_pred[1] += GetChromaDCDiff( p_vpar );              \
    p_idct->pi_block[0] = p_vpar->mb.pi_dc_dct_pred[1]                      \
                         << (3 - p_vpar->picture.i_intra_dc_precision );    \
    PF_MBFUNC( p_vpar, p_idct );                                            \
                                                                            \
    p_idct = &p_mb->p_idcts[5];                                             \
    memset( p_idct->pi_block, 0, 64*sizeof(dctelem_t) );                    \
    p_idct->p_dct_data = p_mb->pp_dest[2] + (p_vpar->mb.i_offset >> 1);     \
    p_vpar->mb.pi_dc_dct_pred[2] += GetChromaDCDiff( p_vpar );              \
    p_idct->pi_block[0] = p_vpar->mb.pi_dc_dct_pred[2]                      \
                         << (3 - p_vpar->picture.i_intra_dc_precision );    \
    PF_MBFUNC( p_vpar, p_idct );                                            \
}

DECLARE_INTRAMB( MPEG1IntraMB, MPEG1Intra );
DECLARE_INTRAMB( MPEG2IntraB14MB, MPEG2IntraB14 );
DECLARE_INTRAMB( MPEG2IntraB15MB, MPEG2IntraB15 );

#undef DECLARE_INTRAMB
#undef DECODE_LUMABLOCK

#define DECODE_BLOCK( i_b, p_dest, PF_MBFUNC )                              \
    if( p_mb->i_coded_block_pattern & (1 << (5 - i_b)) )                    \
    {                                                                       \
        p_idct = &p_mb->p_idcts[i_b];                                       \
        memset( p_idct->pi_block, 0, 64*sizeof(dctelem_t) );                \
        p_idct->p_dct_data = p_dest;                                        \
        PF_MBFUNC( p_vpar, p_idct );                                        \
    }

#define DECLARE_NONINTRAMB( PSZ_NAME, PF_MBFUNC )                           \
static __inline__ void PSZ_NAME( vpar_thread_t * p_vpar,                    \
                                 macroblock_t * p_mb )                      \
{                                                                           \
    int             i_dct_offset;                                           \
    yuv_data_t *    p_lum_dest;                                             \
    idct_inner_t *  p_idct;                                                 \
                                                                            \
    p_lum_dest = p_mb->pp_dest[0] + p_vpar->mb.i_offset;                    \
                                                                            \
    if( p_mb->i_mb_modes & DCT_TYPE_INTERLACED )                            \
    {                                                                       \
        i_dct_offset = p_vpar->picture.i_field_width;                       \
        p_mb->i_lum_dct_stride = p_vpar->picture.i_field_width * 2;         \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        i_dct_offset = p_vpar->picture.i_field_width * 8;                   \
        p_mb->i_lum_dct_stride = p_vpar->picture.i_field_width;             \
    }                                                                       \
    p_mb->i_chrom_dct_stride = p_vpar->picture.i_field_width >> 1;          \
                                                                            \
    DECODE_BLOCK( 0, p_lum_dest, PF_MBFUNC );                               \
    DECODE_BLOCK( 1, p_lum_dest + 8, PF_MBFUNC );                           \
    DECODE_BLOCK( 2, p_lum_dest + i_dct_offset, PF_MBFUNC );                \
    DECODE_BLOCK( 3, p_lum_dest + i_dct_offset + 8, PF_MBFUNC );            \
    DECODE_BLOCK( 4, p_mb->pp_dest[1] + (p_vpar->mb.i_offset >> 1),         \
                  PF_MBFUNC );                                              \
    DECODE_BLOCK( 5, p_mb->pp_dest[2] + (p_vpar->mb.i_offset >> 1),         \
                  PF_MBFUNC );                                              \
}

DECLARE_NONINTRAMB( MPEG1NonIntraMB, MPEG1NonIntra );
DECLARE_NONINTRAMB( MPEG2NonIntraMB, MPEG2NonIntra );

#undef DECLARE_NONINTRAMB
#undef DECODE_BLOCK


/*
 * Motion vectors
 */

/****************************************************************************
 * MotionDelta : Parse the next motion delta
 ****************************************************************************/
static __inline__ int MotionDelta( vpar_thread_t * p_vpar, int i_f_code )
{
    int         i_delta, i_sign, i_code;
    lookup_t *  p_tab;

    if( ShowBits( &p_vpar->bit_stream, 1 ) )
    {
        RemoveBits( &p_vpar->bit_stream, 1 );
        return 0;
    }

    if( (i_code = ShowBits( &p_vpar->bit_stream, 6 )) >= 0x3 )
    {
        p_tab = MV_4 + (i_code >> 2);
        i_delta = (p_tab->i_value << i_f_code) + 1;
        RemoveBits( &p_vpar->bit_stream, p_tab->i_length );

        i_sign = GetSignedBits( &p_vpar->bit_stream, 1 );
        if( i_f_code )
        {
            i_delta += GetBits( &p_vpar->bit_stream, i_f_code );
        }

        return (i_delta ^ i_sign) - i_sign;

    }
    else
    {
        p_tab = MV_10 + ShowBits( &p_vpar->bit_stream, 10 );
        i_delta = (p_tab->i_value << i_f_code) + 1;
        RemoveBits( &p_vpar->bit_stream, p_tab->i_length );

        i_sign = GetSignedBits( &p_vpar->bit_stream, 1 );
        if( i_f_code )
        {
            i_delta += GetBits( &p_vpar->bit_stream, i_f_code );
        }

        return (i_delta ^ i_sign) - i_sign;
    }
}

/****************************************************************************
 * BoundMotionVector : Bound a motion_vector :-)
 ****************************************************************************/
static __inline__ int BoundMotionVector( int i_vector, int i_f_code )
{
    int i_limit;

    i_limit = 16 << i_f_code;

    if( i_vector >= i_limit )
    {
        return i_vector - 2 * i_limit;
    }
    else if( i_vector < -i_limit)
    {
        return i_vector + 2 * i_limit;
    }
    else
    {
        return i_vector;
    }
}

/****************************************************************************
 * GetDMV : Decode a differential motion vector (Dual Prime Arithmetic)
 ****************************************************************************/
static __inline__ int GetDMV( vpar_thread_t * p_vpar )
{
    dmv_lookup_t * p_tab;

    p_tab = DMV_2 + ShowBits( &p_vpar->bit_stream, 2 );
    RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
    return( p_tab->i_value );
}

/****************************************************************************
 * Motion* : Parse motion vectors
 ****************************************************************************/
#define MOTION_BLOCK( b_aver, i_x, i_y, i_dest,                             \
                      pp_src, i_src, i_str, i_hei,                          \
                      b_s_half )                                            \
    do {                                                                    \
        motion_inner_t * p_m_inner = p_mb->p_motions + p_mb->i_nb_motions;  \
        p_m_inner->b_average = b_aver;                                      \
        p_m_inner->i_x_pred = i_x;                                          \
        p_m_inner->i_y_pred = i_y;                                          \
        p_m_inner->pp_source[0] = pp_src[0];                                \
        p_m_inner->pp_source[1] = pp_src[1];                                \
        p_m_inner->pp_source[2] = pp_src[2];                                \
        p_m_inner->i_dest_offset = i_dest;                                  \
        p_m_inner->i_src_offset = i_src;                                    \
        p_m_inner->i_stride = i_str;                                        \
        p_m_inner->i_height = i_hei;                                        \
        p_m_inner->b_second_half = b_s_half;                                \
        p_mb->i_nb_motions++;                                               \
    } while( 0 );

/* MPEG-1 predictions. */

static void MotionMPEG1( vpar_thread_t * p_vpar,
                                    macroblock_t * p_mb,
                                    motion_t * p_motion,
                                    boolean_t b_average )
{
    int i_motion_x, i_motion_y;
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;
    
    i_motion_x = p_motion->ppi_pmv[0][0]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_motion_x = BoundMotionVector( i_motion_x, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[0][0] = i_motion_x;

    i_motion_y = p_motion->ppi_pmv[0][1]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_motion_y = BoundMotionVector( i_motion_y, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[0][1] = i_motion_y;

    if( p_motion->pi_f_code[1] )
    {
        i_motion_x <<= 1;
        i_motion_y <<= 1;
    }

    MOTION_BLOCK( b_average, i_motion_x, i_motion_y, i_offset,
                  p_motion->pppi_ref[0], i_offset, i_width, 16, 0 );
}

static void MotionMPEG1Reuse( vpar_thread_t * p_vpar,
                                         macroblock_t * p_mb,
                                         motion_t * p_motion,
                                         boolean_t b_average )
{
    int i_motion_x, i_motion_y;
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;

    i_motion_x = p_motion->ppi_pmv[0][0];
    i_motion_y = p_motion->ppi_pmv[0][1];

    if( p_motion->pi_f_code[1] )
    {
        i_motion_x <<= 1;
        i_motion_y <<= 1;
    }

     MOTION_BLOCK( b_average, p_motion->ppi_pmv[0][0], p_motion->ppi_pmv[0][1],
                  i_offset, p_motion->pppi_ref[0], i_offset, i_width, 16, 0 );
}

/* MPEG-2 frame predictions. */

static void MotionFrameFrame( vpar_thread_t * p_vpar,
                                         macroblock_t * p_mb,
                                         motion_t * p_motion,
                                         boolean_t b_average )
{
    int i_motion_x, i_motion_y;
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;

    i_motion_x = p_motion->ppi_pmv[0][0]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_motion_x = BoundMotionVector( i_motion_x, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[1][0] = p_motion->ppi_pmv[0][0] = i_motion_x;

    i_motion_y = p_motion->ppi_pmv[0][1]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[1] );
    i_motion_y = BoundMotionVector( i_motion_y, p_motion->pi_f_code[1] );
    p_motion->ppi_pmv[1][1] = p_motion->ppi_pmv[0][1] = i_motion_y;

    MOTION_BLOCK( b_average, i_motion_x, i_motion_y, i_offset,
                  p_motion->pppi_ref[0], i_offset, i_width, 16, 0 );
}

static void MotionFrameField( vpar_thread_t * p_vpar,
                                         macroblock_t * p_mb,
                                         motion_t * p_motion,
                                         boolean_t b_average )
{
    int i_motion_x, i_motion_y, i_field_select;
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;

    i_field_select = GetSignedBits( &p_vpar->bit_stream, 1 );

    i_motion_x = p_motion->ppi_pmv[0][0]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_motion_x = BoundMotionVector( i_motion_x, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[0][0] = i_motion_x;

    i_motion_y = (p_motion->ppi_pmv[0][1] >> 1)
                        + MotionDelta( p_vpar, p_motion->pi_f_code[1] );
    /* According to ISO/IEC 13818-2 section 7.6.3.2, the vertical motion
     * vector is restricted to a range that implies it doesn't need to
     * be bound. */
    /* i_motion_y = BoundMotionVector( i_motion_y, p_motion->pi_f_code[1] ); */
    p_motion->ppi_pmv[0][1] = i_motion_y << 1;

    MOTION_BLOCK( b_average, i_motion_x, i_motion_y, i_offset,
                  p_motion->pppi_ref[0], i_offset + (i_field_select & i_width),
                  i_width * 2, 8, 0 );

    i_field_select = GetSignedBits( &p_vpar->bit_stream, 1 );

    i_motion_x = p_motion->ppi_pmv[1][0]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_motion_x = BoundMotionVector( i_motion_x, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[1][0] = i_motion_x;

    i_motion_y = (p_motion->ppi_pmv[1][1] >> 1)
                        + MotionDelta( p_vpar, p_motion->pi_f_code[1] );
    /* i_motion_y = BoundMotionVector( i_motion_y, p_motion->pi_f_code[1] ); */
    p_motion->ppi_pmv[1][1] = i_motion_y << 1;

    MOTION_BLOCK( b_average, i_motion_x, i_motion_y, i_offset + i_width,
                  p_motion->pppi_ref[0], i_offset + (i_field_select & i_width),
                  i_width * 2, 8, 0 );
}

static void MotionFrameDMV( vpar_thread_t * p_vpar,
                                       macroblock_t * p_mb,
                                       motion_t * p_motion,
                                       boolean_t b_average )
{
    int i_motion_x, i_motion_y;
    int i_dmv_x, i_dmv_y;
    int i_tmp_x, i_tmp_y;

    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;
    int m;

    i_motion_x = p_motion->ppi_pmv[0][0]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_motion_x = BoundMotionVector( i_motion_x, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[1][0] = p_motion->ppi_pmv[0][0] = i_motion_x;

    i_dmv_x = GetDMV( p_vpar );

    i_motion_y = (p_motion->ppi_pmv[0][1] >> 1)
                        + MotionDelta( p_vpar, p_motion->pi_f_code[1] );
    /* i_motion_y = BoundMotionVector( i_motion_y, p_motion->pi_f_code[1] ); */
    p_motion->ppi_pmv[1][1] = p_motion->ppi_pmv[0][1] = i_motion_y << 1;

    i_dmv_y = GetDMV( p_vpar );

    /* First field. */
    MOTION_BLOCK( 0, i_motion_x, i_motion_y, i_offset,
                  p_motion->pppi_ref[0], i_offset, i_width * 2, 8, 0 );
    m = p_vpar->picture.b_top_field_first ? 1 : 3;
    i_tmp_x = ((i_motion_x * m + (i_motion_x > 0)) >> 1) + i_dmv_x;
    i_tmp_y = ((i_motion_y * m + (i_motion_y > 0)) >> 1) + i_dmv_y - 1;
    MOTION_BLOCK( 1, i_tmp_x, i_tmp_y, i_offset, p_motion->pppi_ref[0],
                  i_offset + i_width, i_width * 2, 8, 0 );

    /* Second field. */
    MOTION_BLOCK( 0, i_motion_x, i_motion_y, i_offset + i_width,
                  p_motion->pppi_ref[0], i_offset + i_width, i_width * 2, 8, 0 );
    m = p_vpar->picture.b_top_field_first ? 3 : 1;
    i_tmp_x = ((i_motion_x * m + (i_motion_x > 0)) >> 1) + i_dmv_x;
    i_tmp_y = ((i_motion_y * m + (i_motion_y > 0)) >> 1) + i_dmv_y + 1;
    MOTION_BLOCK( 1, i_tmp_x, i_tmp_y, i_offset + i_width,
                  p_motion->pppi_ref[0], i_offset, i_width * 2, 8, 0 );
}

static void MotionFrameZero( vpar_thread_t * p_vpar,
                                        macroblock_t * p_mb,
                                        motion_t * p_motion,
                                        boolean_t b_average )
{
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;

    MOTION_BLOCK( b_average, 0, 0, i_offset, p_motion->pppi_ref[0],
                  i_offset, i_width, 16, 0 );
}

static void MotionFrameReuse( vpar_thread_t * p_vpar,
                                         macroblock_t * p_mb,
                                         motion_t * p_motion,
                                         boolean_t b_average )
{
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;

    MOTION_BLOCK( b_average, p_motion->ppi_pmv[0][0], p_motion->ppi_pmv[0][1],
                  i_offset, p_motion->pppi_ref[0], i_offset, i_width, 16, 0 );
}

/* MPEG-2 field predictions. */

static void MotionFieldField( vpar_thread_t * p_vpar,
                                         macroblock_t * p_mb,
                                         motion_t * p_motion,
                                         boolean_t b_average )
{
    int i_motion_x, i_motion_y;
    boolean_t b_field_select;
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;

    b_field_select = GetBits( &p_vpar->bit_stream, 1 );

    i_motion_x = p_motion->ppi_pmv[0][0]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_motion_x = BoundMotionVector( i_motion_x, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[1][0] = p_motion->ppi_pmv[0][0] = i_motion_x;

    i_motion_y = p_motion->ppi_pmv[0][1]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[1] );
    i_motion_y = BoundMotionVector( i_motion_y, p_motion->pi_f_code[1] );
    p_motion->ppi_pmv[1][1] = p_motion->ppi_pmv[0][1] = i_motion_y;

    MOTION_BLOCK( b_average, i_motion_x, i_motion_y, i_offset,
                  p_motion->pppi_ref[b_field_select], i_offset, i_width, 16, 0 );
}

static void MotionField16x8( vpar_thread_t * p_vpar,
                                        macroblock_t * p_mb,
                                        motion_t * p_motion,
                                        boolean_t b_average )
{
    int i_motion_x, i_motion_y;
    boolean_t b_field_select;
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;

    /* First half. */
    b_field_select = GetBits( &p_vpar->bit_stream, 1 );

    i_motion_x = p_motion->ppi_pmv[0][0]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_motion_x = BoundMotionVector( i_motion_x, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[0][0] = i_motion_x;

    i_motion_y = p_motion->ppi_pmv[0][1]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[1] );
    i_motion_y = BoundMotionVector( i_motion_y, p_motion->pi_f_code[1] );
    p_motion->ppi_pmv[0][1] = i_motion_y;

    MOTION_BLOCK( b_average, i_motion_x, i_motion_y, i_offset,
                  p_motion->pppi_ref[b_field_select], i_offset, i_width, 8, 0 );

    /* Second half. */
    b_field_select = GetBits( &p_vpar->bit_stream, 1 );

    i_motion_x = p_motion->ppi_pmv[1][0]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_motion_x = BoundMotionVector( i_motion_x, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[1][0] = i_motion_x;

    i_motion_y = p_motion->ppi_pmv[1][1]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[1] );
    i_motion_y = BoundMotionVector( i_motion_y, p_motion->pi_f_code[1] );
    p_motion->ppi_pmv[1][1] = i_motion_y;

    MOTION_BLOCK( b_average, i_motion_x, i_motion_y, i_offset,
                  p_motion->pppi_ref[b_field_select], i_offset, i_width, 8, 1 );
}

static void MotionFieldDMV( vpar_thread_t * p_vpar,
                                       macroblock_t * p_mb,
                                       motion_t * p_motion,
                                       boolean_t b_average )
{
    int i_motion_x, i_motion_y;
    int i_dmv_x, i_dmv_y;
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;
    boolean_t b_current_field = p_vpar->picture.b_current_field;

    i_motion_x = p_motion->ppi_pmv[0][0]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_motion_x = BoundMotionVector( i_motion_x, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[1][0] = p_motion->ppi_pmv[0][0] = i_motion_x;

    i_dmv_x = GetDMV( p_vpar );

    i_motion_y = p_motion->ppi_pmv[0][1]
                        + MotionDelta( p_vpar, p_motion->pi_f_code[1] );
    i_motion_y = BoundMotionVector( i_motion_y, p_motion->pi_f_code[1] );
    p_motion->ppi_pmv[1][1] = p_motion->ppi_pmv[0][1] = i_motion_y;

    i_dmv_y = GetDMV( p_vpar );

    MOTION_BLOCK( 0, i_motion_x, i_motion_y, i_offset,
                  p_motion->pppi_ref[b_current_field],
                  i_offset, i_width, 16, 0 );

    i_motion_x = ((i_motion_x + (i_motion_x > 0)) >> 1) + i_dmv_x;
    i_motion_y = ((i_motion_y + (i_motion_y > 0)) >> 1) + i_dmv_y
                    + 2 * b_current_field - 1;
    MOTION_BLOCK( 1, i_motion_x, i_motion_y, i_offset,
                  p_motion->pppi_ref[!b_current_field],
                  i_offset, i_width, 16, 0 );
}

static void MotionFieldZero( vpar_thread_t * p_vpar,
                                        macroblock_t * p_mb,
                                        motion_t * p_motion,
                                        boolean_t b_average )
{
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;
    boolean_t b_current_field = p_vpar->picture.b_current_field;

    MOTION_BLOCK( b_average, 0, 0, i_offset, p_motion->pppi_ref[b_current_field],
                  i_offset, i_width, 16, 0 );
}

static void MotionFieldReuse( vpar_thread_t * p_vpar,
                                         macroblock_t * p_mb,
                                         motion_t * p_motion,
                                         boolean_t b_average )
{
    int i_offset = p_vpar->mb.i_offset;
    int i_width = p_vpar->picture.i_field_width;
    boolean_t b_current_field = p_vpar->picture.b_current_field;

    MOTION_BLOCK( b_average, p_motion->ppi_pmv[0][0], p_motion->ppi_pmv[0][1],
                  i_offset, p_motion->pppi_ref[b_current_field],
                  i_offset, i_width, 16, 0 );
}

/* MPEG-2 concealment motion vectors. */

static void MotionFrameConceal( vpar_thread_t * p_vpar,
                                           macroblock_t * p_mv,
                                           motion_t * p_motion )
{
    int i_tmp;

    i_tmp = p_motion->ppi_pmv[0][0]
                + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_tmp = BoundMotionVector( i_tmp, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[1][0] = p_motion->ppi_pmv[0][0] = i_tmp;

    i_tmp = p_motion->ppi_pmv[0][1]
                + MotionDelta( p_vpar, p_motion->pi_f_code[1] );
    i_tmp = BoundMotionVector( i_tmp, p_motion->pi_f_code[1] );
    p_motion->ppi_pmv[1][1] = p_motion->ppi_pmv[0][1] = i_tmp;

    /* Marker bit. */
    RemoveBits( &p_vpar->bit_stream, 1 );
}

static void MotionFieldConceal( vpar_thread_t * p_vpar,
                                           macroblock_t * p_mv,
                                           motion_t * p_motion )
{
    int i_tmp;

    /* field_select */
    RemoveBits( &p_vpar->bit_stream, 1 );

    i_tmp = p_motion->ppi_pmv[0][0]
                + MotionDelta( p_vpar, p_motion->pi_f_code[0] );
    i_tmp = BoundMotionVector( i_tmp, p_motion->pi_f_code[0] );
    p_motion->ppi_pmv[1][0] = p_motion->ppi_pmv[0][0] = i_tmp;

    i_tmp = p_motion->ppi_pmv[0][1]
                + MotionDelta( p_vpar, p_motion->pi_f_code[1] );
    i_tmp = BoundMotionVector( i_tmp, p_motion->pi_f_code[1] );
    p_motion->ppi_pmv[1][1] = p_motion->ppi_pmv[0][1] = i_tmp;

    /* Marker bit. */
    RemoveBits( &p_vpar->bit_stream, 1 );
}


/*
 * Macroblock information structures
 */

/*****************************************************************************
 * MacroblockAddressIncrement : Get the macroblock_address_increment field
 *****************************************************************************/
static __inline__ int MacroblockAddressIncrement( vpar_thread_t * p_vpar )
{
    lookup_t *  p_tab;
    int         i_code;
    int         i_mba = 0;

    for( ; ; )
    {
        if( (i_code = ShowBits( &p_vpar->bit_stream, 5 ) ) >= 0x2 )
        {
            p_tab = MBA_5 - 2 + i_code;
            RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
            return( i_mba + p_tab->i_value );
        }
        else if( (i_code = ShowBits( &p_vpar->bit_stream, 11 )) >= 0x18 )
        {
            p_tab = MBA_11 - 24 + i_code;
            RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
            return( i_mba + p_tab->i_value );
        }
        else switch( i_code )
        {
        case 8:
            /* Macroblock escape */
            i_mba += 33;
            /* continue... */
        case 15:
            /* Macroblock stuffing (MPEG-1 ONLY) */
            RemoveBits( &p_vpar->bit_stream, 11 );
            break;

        default:
            /* End of slice, or error */
            return 0;
        }
    }
}

/*****************************************************************************
 * CodedPattern : coded_block_pattern
 *****************************************************************************/
static __inline__ int CodedPattern( vpar_thread_t * p_vpar )
{
    lookup_t *  p_tab;
    int         i_code;

    if( (i_code = ShowBits( &p_vpar->bit_stream, 7 )) >= 0x10 ) /* ? */
    {
        p_tab = CBP_7 - 16 + i_code;
        RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
        return( p_tab->i_value );
    }
    else
    {
        p_tab = CBP_9 + ShowBits( &p_vpar->bit_stream, 9 );
        RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
        return( p_tab->i_value );
    }
}

/*****************************************************************************
 * MacroblockModes : Get the macroblock_modes structure
 *****************************************************************************/
static __inline__ int MacroblockModes( vpar_thread_t * p_vpar,
                                       macroblock_t * p_mb,
                                       int i_coding_type,
                                       int i_structure )
{
    int         i_mb_modes;
    lookup_t *  p_tab;

    switch( i_coding_type )
    {
    case I_CODING_TYPE:
        p_tab = MB_I + ShowBits( &p_vpar->bit_stream, 1 );
        RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
        i_mb_modes = p_tab->i_value;

        if( (i_structure == FRAME_STRUCTURE) &&
            (!p_vpar->picture.b_frame_pred_frame_dct) )
        {
            i_mb_modes |= GetBits( &p_vpar->bit_stream, 1 )
                                * DCT_TYPE_INTERLACED;
        }
        return( i_mb_modes );

    case P_CODING_TYPE:
        p_tab = MB_P + ShowBits( &p_vpar->bit_stream, 5 );
        RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
        i_mb_modes = p_tab->i_value;

        if( i_structure != FRAME_STRUCTURE )
        {
            if( i_mb_modes & MB_MOTION_FORWARD )
            {
                i_mb_modes |= GetBits( &p_vpar->bit_stream, 2 )
                                    * MOTION_TYPE_BASE;
            }
            return( i_mb_modes );
        }
        else if( p_vpar->picture.b_frame_pred_frame_dct )
        {
            if( i_mb_modes & MB_MOTION_FORWARD )
            {
                i_mb_modes |= MC_FRAME;
            }
            return( i_mb_modes );
        }
        else
        {
            if( i_mb_modes & MB_MOTION_FORWARD )
            {
                i_mb_modes |= GetBits( &p_vpar->bit_stream, 2 )
                                    * MOTION_TYPE_BASE;
            }
            if( i_mb_modes & (MB_INTRA | MB_PATTERN) )
            {
                i_mb_modes |= GetBits( &p_vpar->bit_stream, 1 )
                                    * DCT_TYPE_INTERLACED;
            }
            return( i_mb_modes );
        }

    case B_CODING_TYPE:
        p_tab = MB_B + ShowBits( &p_vpar->bit_stream, 6 );
        RemoveBits( &p_vpar->bit_stream, p_tab->i_length );
        i_mb_modes = p_tab->i_value;

        if( i_structure != FRAME_STRUCTURE )
        {
            if( !( i_mb_modes & MB_INTRA ) )
            {
                i_mb_modes |= GetBits( &p_vpar->bit_stream, 2 )
                                    * MOTION_TYPE_BASE;
            }
            return( i_mb_modes );
        }
        else if( p_vpar->picture.b_frame_pred_frame_dct )
        {
            i_mb_modes |= MC_FRAME;
            return( i_mb_modes );
        }
        else
        {
            if( i_mb_modes & MB_INTRA )
            {
                goto mb_intra;
            }
            i_mb_modes |= GetBits( &p_vpar->bit_stream, 2 )
                                * MOTION_TYPE_BASE;
            if( i_mb_modes & (MB_INTRA | MB_PATTERN) )
            {
mb_intra:
                i_mb_modes |= GetBits( &p_vpar->bit_stream, 1 )
                                    * DCT_TYPE_INTERLACED;
            }
            return( i_mb_modes );
        }

    case D_CODING_TYPE:
        RemoveBits( &p_vpar->bit_stream, 1 );
        return( MB_INTRA );

    default:
        return( 0 );
    }
}


/*
 * Picture data parsing management
 */

/*****************************************************************************
 * ParseSlice : Parse the next slice structure
 *****************************************************************************/
#define MOTION( pf_routine, i_direction )                                   \
    if( (i_direction) & MB_MOTION_FORWARD )                                 \
    {                                                                       \
        pf_routine( p_vpar, p_mb, &p_vpar->mb.f_motion, 0 );                \
        if( (i_coding_type == B_CODING_TYPE)                                \
                && ((i_direction) & MB_MOTION_BACKWARD) )                   \
        {                                                                   \
            pf_routine( p_vpar, p_mb, &p_vpar->mb.b_motion, 1 );            \
        }                                                                   \
    }                                                                       \
    else if( (i_coding_type == B_CODING_TYPE)                               \
                 && ((i_direction) & MB_MOTION_BACKWARD) )                  \
    {                                                                       \
        pf_routine( p_vpar, p_mb, &p_vpar->mb.b_motion, 0 );                \
    }

#define CHECK_BOUNDARIES                                                    \
    i_offset = p_vpar->mb.i_offset;                                         \
    if( i_offset == i_width )                                               \
    {                                                                       \
        if( i_coding_type != I_CODING_TYPE ||                               \
            p_vpar->picture.b_concealment_mv )                              \
        {                                                                   \
            p_f_motion->pppi_ref[0][0] += 16 * i_offset;                    \
            p_f_motion->pppi_ref[0][1] += 4 * i_offset;                     \
            p_f_motion->pppi_ref[0][2] += 4 * i_offset;                     \
        }                                                                   \
        if( i_coding_type == B_CODING_TYPE )                                \
        {                                                                   \
            p_b_motion->pppi_ref[0][0] += 16 * i_offset;                    \
            p_b_motion->pppi_ref[0][1] += 4 * i_offset;                     \
            p_b_motion->pppi_ref[0][2] += 4 * i_offset;                     \
        }                                                                   \
        p_dest[0] += 16 * i_offset;                                         \
        p_dest[1] += 4 * i_offset;                                          \
        p_dest[2] += 4 * i_offset;                                          \
        i_offset = 0;                                                       \
    }                                                                       \
    p_vpar->mb.i_offset = i_offset;

#define PARSEERROR                                                          \
    if( p_vpar->picture.b_error )                                           \
    {                                                                       \
        /* Go to the next slice. */                                         \
        p_vpar->pool.pf_free_mb( &p_vpar->pool, p_mb );                     \
        return;                                                             \
    }

static __inline__ void ParseSlice( vpar_thread_t * p_vpar,
                                   u32 i_vert_code, boolean_t b_mpeg2,
                                   int i_coding_type, int i_structure )
{
    int             i_offset, i_width;
    picture_t *     pp_forward_ref[2];
    yuv_data_t *    p_dest[3];

    motion_t *      p_f_motion = &p_vpar->mb.f_motion;
    motion_t *      p_b_motion = &p_vpar->mb.b_motion;

    /* Parse header. */
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

    /* Calculate the position of the macroblock. */
    i_width = p_vpar->sequence.i_width;
    i_offset = (i_vert_code - 1) * i_width * 4;

    /* Initialize motion context. */
    pp_forward_ref[0] = p_vpar->sequence.p_forward;

    if( i_structure != FRAME_STRUCTURE )
    {
        i_offset <<= 1;
        pp_forward_ref[1] = p_vpar->sequence.p_forward;

        if( i_coding_type != B_CODING_TYPE && p_vpar->picture.b_second_field )
        {
            pp_forward_ref[!p_vpar->picture.b_current_field] =
                p_vpar->picture.p_picture;
        }
        if( i_coding_type != I_CODING_TYPE || p_vpar->picture.b_concealment_mv )
        {
            p_f_motion->pppi_ref[1][0] =
                    pp_forward_ref[1]->p_y + i_offset * 4 + i_width;
            p_f_motion->pppi_ref[1][1] =
                    pp_forward_ref[1]->p_u + i_offset + (i_width >> 1);
            p_f_motion->pppi_ref[1][2] =
                    pp_forward_ref[1]->p_v + i_offset + (i_width >> 1);
        }
        if( i_coding_type == B_CODING_TYPE )
        {
            p_b_motion->pppi_ref[1][0] =
                p_vpar->sequence.p_backward->p_y + i_offset * 4 + i_width;
            p_b_motion->pppi_ref[1][1] =
                p_vpar->sequence.p_backward->p_u + i_offset + (i_width >> 1);
            p_b_motion->pppi_ref[1][2] =
                p_vpar->sequence.p_backward->p_v + i_offset + (i_width >> 1);
        }
    }

    if( i_coding_type != I_CODING_TYPE || p_vpar->picture.b_concealment_mv )
    {
        p_f_motion->pppi_ref[0][0] = pp_forward_ref[0]->p_y + i_offset * 4;
        p_f_motion->pppi_ref[0][1] = pp_forward_ref[0]->p_u + i_offset;
        p_f_motion->pppi_ref[0][2] = pp_forward_ref[0]->p_v + i_offset;
        p_f_motion->pi_f_code[0] = p_vpar->picture.ppi_f_code[0][0];
        p_f_motion->pi_f_code[1] = p_vpar->picture.ppi_f_code[0][1];
        p_f_motion->ppi_pmv[0][0] = p_f_motion->ppi_pmv[0][1] = 0;
        p_f_motion->ppi_pmv[1][0] = p_f_motion->ppi_pmv[1][1] = 0;
    }

    if( i_coding_type == B_CODING_TYPE )
    {
        p_b_motion->pppi_ref[0][0] = p_vpar->sequence.p_backward->p_y
                                        + i_offset * 4;
        p_b_motion->pppi_ref[0][1] = p_vpar->sequence.p_backward->p_u
                                        + i_offset;
        p_b_motion->pppi_ref[0][2] = p_vpar->sequence.p_backward->p_v
                                        + i_offset;
        p_b_motion->pi_f_code[0] = p_vpar->picture.ppi_f_code[1][0];
        p_b_motion->pi_f_code[1] = p_vpar->picture.ppi_f_code[1][1];
        p_b_motion->ppi_pmv[0][0] = p_b_motion->ppi_pmv[0][1] = 0;
        p_b_motion->ppi_pmv[1][0] = p_b_motion->ppi_pmv[1][1] = 0;
    }

    /* Initialize destination pointers. */
    p_dest[0] = p_vpar->picture.p_picture->p_y + i_offset * 4;
    p_dest[1] = p_vpar->picture.p_picture->p_u + i_offset;
    p_dest[2] = p_vpar->picture.p_picture->p_v + i_offset;

    if( i_structure == BOTTOM_FIELD )
    {
        p_dest[0] += i_width;
        p_dest[1] += i_width >> 1;
        p_dest[2] += i_width >> 1;
    }
    i_width = p_vpar->picture.i_field_width;

    /* Reset intra DC coefficients predictors (ISO/IEC 13818-2 7.2.1). */
    p_vpar->mb.pi_dc_dct_pred[0] = p_vpar->mb.pi_dc_dct_pred[1]
        = p_vpar->mb.pi_dc_dct_pred[2]
        = 1 << (7 + p_vpar->picture.i_intra_dc_precision);

    p_vpar->mb.i_offset = MacroblockAddressIncrement( p_vpar ) << 4;

    for( ; ; )
    {
        /* Decode macroblocks. */
        macroblock_t *  p_mb;
        int             i_mb_modes;

        /* Get a macroblock structure. */
        p_mb = p_vpar->pool.pf_new_mb( &p_vpar->pool );
        p_mb->i_nb_motions = 0;
        p_mb->pp_dest[0] = p_dest[0]; 
        p_mb->pp_dest[1] = p_dest[1]; 
        p_mb->pp_dest[2] = p_dest[2]; 

        /* Parse off macroblock_modes structure. */
        p_mb->i_mb_modes = i_mb_modes =
                MacroblockModes( p_vpar, p_mb, i_coding_type, i_structure );

        if( i_mb_modes & MB_QUANT )
        {
            LoadQuantizerScale( p_vpar );
        }

        if( i_mb_modes & MB_INTRA )
        {
            if( p_vpar->picture.b_concealment_mv )
            {
                if( i_structure == FRAME_STRUCTURE )
                {
                    MotionFrameConceal( p_vpar, p_mb, p_f_motion );
                }
                else
                {
                    MotionFieldConceal( p_vpar, p_mb, p_f_motion );
                }
            }
            else
            {
                /* Reset motion vectors predictors. */
                p_f_motion->ppi_pmv[0][0] = p_f_motion->ppi_pmv[0][1] = 0;
                p_f_motion->ppi_pmv[1][0] = p_f_motion->ppi_pmv[1][1] = 0;
                p_b_motion->ppi_pmv[0][0] = p_b_motion->ppi_pmv[0][1] = 0;
                p_b_motion->ppi_pmv[1][0] = p_b_motion->ppi_pmv[1][1] = 0;
            }

            /* Decode blocks */
            p_mb->i_coded_block_pattern = (1 << 6) - 1;
            if( b_mpeg2 )
            {
                if( p_vpar->picture.b_intra_vlc_format )
                {
                    MPEG2IntraB15MB( p_vpar, p_mb );
                }
                else
                {
                    MPEG2IntraB14MB( p_vpar, p_mb );
                }
            }
            else
            {
                MPEG1IntraMB( p_vpar, p_mb );
            }

            if( i_coding_type == D_CODING_TYPE )
            {
                RemoveBits( &p_vpar->bit_stream, 1 );
            }
        }
        else
        {
            /* Non-intra block */
            if( !b_mpeg2 )
            {
                if( (i_mb_modes & MOTION_TYPE_MASK) == MC_FRAME )
                {
                    MOTION( MotionMPEG1, i_mb_modes );
                }
                else
                {
                    /* Non-intra MB without forward mv in a P picture. */
                    p_f_motion->ppi_pmv[0][0] = p_f_motion->ppi_pmv[0][1] = 0;
                    p_f_motion->ppi_pmv[1][0] = p_f_motion->ppi_pmv[1][1] = 0;
                    MOTION( MotionFrameZero, MB_MOTION_FORWARD );
                }
            }
            else if( i_structure == FRAME_STRUCTURE )
            {
                switch( i_mb_modes & MOTION_TYPE_MASK )
                {
                case MC_FRAME:
                    MOTION( MotionFrameFrame, i_mb_modes );
                    break;

                case MC_FIELD:
                    MOTION( MotionFrameField, i_mb_modes );
                    break;

                case MC_DMV:
                    MOTION( MotionFrameDMV, MB_MOTION_FORWARD );
                    break;

                case 0:
                    /* Non-intra MB without forward mv in a P picture. */
                    p_f_motion->ppi_pmv[0][0] = p_f_motion->ppi_pmv[0][1] = 0;
                    p_f_motion->ppi_pmv[1][0] = p_f_motion->ppi_pmv[1][1] = 0;
                    MOTION( MotionFrameZero, MB_MOTION_FORWARD );
                }
            }
            else
            {
                /* Field structure. */
                switch( i_mb_modes & MOTION_TYPE_MASK )
                {
                case MC_FIELD:
                    MOTION( MotionFieldField, i_mb_modes );
                    break;

                case MC_16X8:
                    MOTION( MotionField16x8, i_mb_modes );
                    break;

                case MC_DMV:
                    MOTION( MotionFieldDMV, i_mb_modes );
                    break;

                case 0:
                    /* Non-intra MB without forward mv in a P picture. */
                    p_f_motion->ppi_pmv[0][0] = p_f_motion->ppi_pmv[0][1] = 0;
                    p_f_motion->ppi_pmv[1][0] = p_f_motion->ppi_pmv[1][1] = 0;
                    MOTION( MotionFieldZero, MB_MOTION_FORWARD );

                }
            }

            /* ISO/IEC 13818-2 6.3.17.4 : Coded Block Pattern */
            if( i_mb_modes & MB_PATTERN )
            {
                p_mb->i_coded_block_pattern = CodedPattern( p_vpar );
                if( b_mpeg2 )
                {
                    MPEG2NonIntraMB( p_vpar, p_mb );
                }
                else
                {
                    MPEG1NonIntraMB( p_vpar, p_mb );
                }
            }
            else
            {
                p_mb->i_coded_block_pattern = 0;
            }

            /* Reset intra DC coefficients predictors. */
            p_vpar->mb.pi_dc_dct_pred[0] = p_vpar->mb.pi_dc_dct_pred[1]
                = p_vpar->mb.pi_dc_dct_pred[2]
                = 1 << (7 + p_vpar->picture.i_intra_dc_precision);
        }

        /* End of macroblock. */
        PARSEERROR;
        p_vpar->pool.pf_decode_mb( &p_vpar->pool, p_mb );

        /* Prepare context for the next macroblock. */
        p_vpar->mb.i_offset += 16;
        CHECK_BOUNDARIES;

        if( ShowBits( &p_vpar->bit_stream, 1 ) )
        {
            /* Macroblock Address Increment == 1 */
            RemoveBits( &p_vpar->bit_stream, 1 );
        }
        else
        {
            /* Check for skipped macroblock(s). */
            int i_mba_inc;

            i_mba_inc = MacroblockAddressIncrement( p_vpar );
            if( !i_mba_inc )
            {
                /* End of slice. */
                break;
            }

            /* Reset intra DC predictors. */
            p_vpar->mb.pi_dc_dct_pred[0] = p_vpar->mb.pi_dc_dct_pred[1]
                = p_vpar->mb.pi_dc_dct_pred[2]
                = 1 << (7 + p_vpar->picture.i_intra_dc_precision);

            if( i_coding_type == P_CODING_TYPE )
            {
                p_f_motion->ppi_pmv[0][0] = p_f_motion->ppi_pmv[0][1] = 0;
                p_f_motion->ppi_pmv[1][0] = p_f_motion->ppi_pmv[1][1] = 0;

                do {
                    p_mb = p_vpar->pool.pf_new_mb( &p_vpar->pool );
                    p_mb->i_mb_modes = 0;
                    p_mb->i_nb_motions = 0;
                    p_mb->i_coded_block_pattern = 0;
                    p_mb->pp_dest[0] = p_dest[0]; 
                    p_mb->pp_dest[1] = p_dest[1]; 
                    p_mb->pp_dest[2] = p_dest[2]; 

                    if( i_structure == FRAME_STRUCTURE )
                    {
                        MOTION( MotionFrameZero, MB_MOTION_FORWARD );
                    }
                    else
                    {
                        MOTION( MotionFieldZero, MB_MOTION_FORWARD );
                    }

                    p_vpar->pool.pf_decode_mb( &p_vpar->pool, p_mb );
                    p_vpar->mb.i_offset += 16;
                    CHECK_BOUNDARIES;
                } while( --i_mba_inc );
            }
            else
            {
                do {
                    p_mb = p_vpar->pool.pf_new_mb( &p_vpar->pool );
                    p_mb->i_mb_modes = 0;
                    p_mb->i_nb_motions = 0;
                    p_mb->i_coded_block_pattern = 0;
                    p_mb->pp_dest[0] = p_dest[0]; 
                    p_mb->pp_dest[1] = p_dest[1]; 
                    p_mb->pp_dest[2] = p_dest[2]; 

                    if( !b_mpeg2 )
                    {
                        MOTION( MotionMPEG1Reuse, i_mb_modes );
                    }
                    else if( i_structure == FRAME_STRUCTURE )
                    {
                        MOTION( MotionFrameReuse, i_mb_modes );
                    }
                    else
                    {
                        MOTION( MotionFieldReuse, i_mb_modes );
                    }

                    p_vpar->pool.pf_decode_mb( &p_vpar->pool, p_mb );
                    p_vpar->mb.i_offset += 16;
                    CHECK_BOUNDARIES;
                } while( --i_mba_inc );
            }
        }
    }

    NextStartCode( &p_vpar->bit_stream );
}

/*****************************************************************************
 * PictureData : Parse off all macroblocks (ISO/IEC 13818-2 6.2.3.7)
 *****************************************************************************/
static __inline__ void vpar_PictureData( vpar_thread_t * p_vpar,
                                         boolean_t b_mpeg2,
                                         int i_coding_type, int i_structure )
{
    u32         i_dummy;

    NextStartCode( &p_vpar->bit_stream );
    while( !p_vpar->picture.b_error && !p_vpar->p_fifo->b_die )
    {
        if( ((i_dummy = ShowBits( &p_vpar->bit_stream, 32 ))
                 < SLICE_START_CODE_MIN) ||
            (i_dummy > SLICE_START_CODE_MAX) )
        {
            break;
        }
        RemoveBits32( &p_vpar->bit_stream );

        /* Decode slice data. */
        ParseSlice( p_vpar, i_dummy & 255, b_mpeg2, i_coding_type,
                    i_structure );
    }
}

#define DECLARE_PICD( FUNCNAME, B_MPEG2, I_CODING_TYPE, I_STRUCTURE )       \
void FUNCNAME( vpar_thread_t * p_vpar )                                     \
{                                                                           \
    vpar_PictureData( p_vpar, B_MPEG2, I_CODING_TYPE, I_STRUCTURE );        \
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

#undef DECLARE_PICD

