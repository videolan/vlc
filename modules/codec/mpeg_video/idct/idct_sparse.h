/*****************************************************************************
 * idct_sparse.h : Sparse IDCT functions (must be include at the end)
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: idct_sparse.h,v 1.1 2002/08/04 17:23:42 sam Exp $
 *
 * Author: Gaël Hendryckx <jimmy@via.ecp.fr>
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
 * InitIDCT : initialize data for SparseIDCT
 *****************************************************************************/
static void InitIDCT ( void ** pp_idct_data )
{
    int i;
    dctelem_t * p_pre;

    *pp_idct_data = malloc( sizeof(dctelem_t) * 64 * 64 );
    p_pre = (dctelem_t *) *pp_idct_data;
    memset( p_pre, 0, 64 * 64 * sizeof(dctelem_t) );

    for( i = 0 ; i < 64 ; i++ )
    {
        p_pre[i*64+i] = 1 << SPARSE_SCALE_FACTOR;
        IDCT( &p_pre[i*64] ) ;
    }

    InitBlock();

    RestoreCPUState();
}

/*****************************************************************************
 * SparseIDCT : IDCT function for sparse matrices
 *****************************************************************************/
static inline void SparseIDCT( dctelem_t * p_block, void * p_idct_data,
                               int i_sparse_pos )
{
    short int val;
    int * dp;
    int v;
    short int * p_dest;
    short int * p_source;
    int coeff, rr;
    dctelem_t * p_pre = (dctelem_t *) p_idct_data;

    /* If DC Coefficient. */
    if ( i_sparse_pos == 0 )
    {
        dp=(int *)p_block;
        val=RIGHT_SHIFT((*p_block + 4), 3);
        /* Compute int to assign.  This speeds things up a bit */
        v = ((val & 0xffff) | (val << 16));
        dp[0] = v;     dp[1] = v;     dp[2] = v;     dp[3] = v;
        dp[4] = v;     dp[5] = v;     dp[6] = v;     dp[7] = v;
        dp[8] = v;     dp[9] = v;     dp[10] = v;    dp[11] = v;
        dp[12] = v;    dp[13] = v;    dp[14] = v;    dp[15] = v;
        dp[16] = v;    dp[17] = v;    dp[18] = v;    dp[19] = v;
        dp[20] = v;    dp[21] = v;    dp[22] = v;    dp[23] = v;
        dp[24] = v;    dp[25] = v;    dp[26] = v;    dp[27] = v;
        dp[28] = v;    dp[29] = v;    dp[30] = v;    dp[31] = v;
        return;
    }
    /* Some other coefficient. */
    p_dest = (s16*)p_block;
    p_source = (s16*)&p_pre[i_sparse_pos * 64];
    coeff = (int)p_dest[i_sparse_pos];
    for( rr=0 ; rr < 4 ; rr++ )
    {
        p_dest[0] = (p_source[0] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[1] = (p_source[1] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[2] = (p_source[2] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[3] = (p_source[3] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[4] = (p_source[4] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[5] = (p_source[5] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[6] = (p_source[6] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[7] = (p_source[7] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[8] = (p_source[8] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[9] = (p_source[9] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[10] = (p_source[10] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[11] = (p_source[11] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[12] = (p_source[12] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[13] = (p_source[13] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[14] = (p_source[14] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest[15] = (p_source[15] * coeff) >> SPARSE_SCALE_FACTOR;
        p_dest += 16;
        p_source += 16;
    }
}

/*****************************************************************************
 * Final declarations
 *****************************************************************************/
static void SparseIDCTCopy( dctelem_t * p_block, yuv_data_t * p_dest,
                            int i_stride, void * p_idct_data, int i_sparse_pos )
{
    SparseIDCT( p_block, p_idct_data, i_sparse_pos );
    CopyBlock( p_block, p_dest, i_stride );
}

static void SparseIDCTAdd( dctelem_t * p_block, yuv_data_t * p_dest,
                           int i_stride, void * p_idct_data, int i_sparse_pos )
{
    SparseIDCT( p_block, p_idct_data, i_sparse_pos );
    AddBlock( p_block, p_dest, i_stride );
}
