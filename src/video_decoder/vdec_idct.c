/*****************************************************************************
 * vdec_idct.c : IDCT functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */

#include "threads.h"
#include "config.h"
#include "common.h"
#include "mtime.h"
#include "plugins.h"

#include "intf_msg.h"

#include "input.h"
#include "decoder_fifo.h"
#include "video.h"
#include "video_output.h"

#include "vdec_idct.h"
#include "video_decoder.h"
#include "vdec_motion.h"

#include "vpar_blocks.h"
#include "vpar_headers.h"
#include "vpar_synchro.h"
#include "video_parser.h"
#include "video_fifo.h"

/*
 * Local prototypes
 */

/* Our current implementation is a fast DCT, we might move to a fast DFT or
 * an MMX DCT in the future. */

/*****************************************************************************
 * vdec_InitIDCT : initialize datas for vdec_SparceIDCT
 * vdec_SparseIDCT : IDCT function for sparse matrices
 *****************************************************************************/

void vdec_InitIDCT (vdec_thread_t * p_vdec)
{
    int i;

    dctelem_t * p_pre = p_vdec->p_pre_idct;
    memset( p_pre, 0, 64*64*sizeof(dctelem_t) );

    for( i=0 ; i < 64 ; i++ )
    {
        p_pre[i*64+i] = 1 << SPARSE_SCALE_FACTOR;
        vdec_IDCT( p_vdec, &p_pre[i*64], 0) ;
    }
    return;
}

void vdec_SparseIDCT (vdec_thread_t * p_vdec, dctelem_t * p_block,
                      int i_sparse_pos)
{
    short int val;
    int * dp;
    int v;
    short int * p_dest;
    short int * p_source;
    int coeff, rr;

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
    p_source = (s16*)&p_vdec->p_pre_idct[i_sparse_pos];
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
    return;
}


/*****************************************************************************
 * vdec_IDCT : IDCT function for normal matrices
 *****************************************************************************/

#ifndef HAVE_MMX
void vdec_IDCT( vdec_thread_t * p_vdec, dctelem_t * p_block, int i_idontcare )
{
#if 0
    /* dct classique: pour tester la meilleure entre la classique et la */
    /* no classique */
    s32 tmp0, tmp1, tmp2, tmp3;
    s32 tmp10, tmp11, tmp12, tmp13;
    s32 z1, z2, z3, z4, z5;
    dctelem_t * dataptr;
    int rowctr;
    SHIFT_TEMPS

  /* Pass 1: process rows. */
  /* Note results are scaled up by sqrt(8) compared to a true IDCT; */
  /* furthermore, we scale the results by 2**PASS1_BITS. */

    dataptr = p_block;
    for (rowctr = DCTSIZE-1; rowctr >= 0; rowctr--)
    {
    /* Due to quantization, we will usually find that many of the input
     * coefficients are zero, especially the AC terms.  We can exploit this
     * by short-circuiting the IDCT calculation for any row in which all
     * the AC terms are zero.  In that case each output is equal to the
     * DC coefficient (with scale factor as needed).
     * With typical images and quantization tables, half or more of the
     * row DCT calculations can be simplified this way.
     */

        if ((dataptr[1] | dataptr[2] | dataptr[3] | dataptr[4] |
                dataptr[5] | dataptr[6] | dataptr[7]) == 0)
        {
      /* AC terms all zero */
            dctelem_t dcval = (dctelem_t) (dataptr[0] << PASS1_BITS);

            dataptr[0] = dcval;
            dataptr[1] = dcval;
            dataptr[2] = dcval;
            dataptr[3] = dcval;
            dataptr[4] = dcval;
            dataptr[5] = dcval;
            dataptr[6] = dcval;
            dataptr[7] = dcval;

            dataptr += DCTSIZE; /* advance pointer to next row */
            continue;
        }

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */

        z2 = (s32) dataptr[2];
        z3 = (s32) dataptr[6];

        z1 = MULTIPLY(z2 + z3, FIX(0.541196100));
        tmp2 = z1 + MULTIPLY(z3, - FIX(1.847759065));
        tmp3 = z1 + MULTIPLY(z2, FIX(0.765366865));

        tmp0 = ((s32) dataptr[0] + (s32) dataptr[4]) << CONST_BITS;
        tmp1 = ((s32) dataptr[0] - (s32) dataptr[4]) << CONST_BITS;

        tmp10 = tmp0 + tmp3;
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

        tmp0 = (s32) dataptr[7];
        tmp1 = (s32) dataptr[5];
        tmp2 = (s32) dataptr[3];
        tmp3 = (s32) dataptr[1];

        z1 = tmp0 + tmp3;
        z2 = tmp1 + tmp2;
        z3 = tmp0 + tmp2;
        z4 = tmp1 + tmp3;
        z5 = MULTIPLY(z3 + z4, FIX(1.175875602)); /* sqrt(2) * c3 */

        tmp0 = MULTIPLY(tmp0, FIX(0.298631336)); /* sqrt(2) * (-c1+c3+c5-c7) */
        tmp1 = MULTIPLY(tmp1, FIX(2.053119869)); /* sqrt(2) * ( c1+c3-c5+c7) */
        tmp2 = MULTIPLY(tmp2, FIX(3.072711026)); /* sqrt(2) * ( c1+c3+c5-c7) */
        tmp3 = MULTIPLY(tmp3, FIX(1.501321110)); /* sqrt(2) * ( c1+c3-c5-c7) */
        z1 = MULTIPLY(z1, - FIX(0.899976223)); /* sqrt(2) * (c7-c3) */
        z2 = MULTIPLY(z2, - FIX(2.562915447)); /* sqrt(2) * (-c1-c3) */
        z3 = MULTIPLY(z3, - FIX(1.961570560)); /* sqrt(2) * (-c3-c5) */
        z4 = MULTIPLY(z4, - FIX(0.390180644)); /* sqrt(2) * (c5-c3) */

        z3 += z5;
        z4 += z5;

        tmp0 += z1 + z3;
        tmp1 += z2 + z4;
        tmp2 += z2 + z3;
        tmp3 += z1 + z4;

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

        dataptr[0] = (dctelem_t) DESCALE(tmp10 + tmp3, CONST_BITS-PASS1_BITS);
        dataptr[7] = (dctelem_t) DESCALE(tmp10 - tmp3, CONST_BITS-PASS1_BITS);
        dataptr[1] = (dctelem_t) DESCALE(tmp11 + tmp2, CONST_BITS-PASS1_BITS);
        dataptr[6] = (dctelem_t) DESCALE(tmp11 - tmp2, CONST_BITS-PASS1_BITS);
        dataptr[2] = (dctelem_t) DESCALE(tmp12 + tmp1, CONST_BITS-PASS1_BITS);
        dataptr[5] = (dctelem_t) DESCALE(tmp12 - tmp1, CONST_BITS-PASS1_BITS);
        dataptr[3] = (dctelem_t) DESCALE(tmp13 + tmp0, CONST_BITS-PASS1_BITS);
        dataptr[4] = (dctelem_t) DESCALE(tmp13 - tmp0, CONST_BITS-PASS1_BITS);

        dataptr += DCTSIZE;             /* advance pointer to next row */
    }

  /* Pass 2: process columns. */
  /* Note that we must descale the results by a factor of 8 == 2**3, */
  /* and also undo the PASS1_BITS scaling. */

    dataptr = p_block;
    for (rowctr = DCTSIZE-1; rowctr >= 0; rowctr--)
    {
    /* Columns of zeroes can be exploited in the same way as we did with rows.
     * However, the row calculation has created many nonzero AC terms, so the
     * simplification applies less often (typically 5% to 10% of the time).
     * On machines with very fast multiplication, it's possible that the
     * test takes more time than it's worth.  In that case this section
     * may be commented out.
     */

#ifndef NO_ZERO_COLUMN_TEST /*ajoute un test mais evite des calculs */
        if ((dataptr[DCTSIZE*1] | dataptr[DCTSIZE*2] | dataptr[DCTSIZE*3] |
            dataptr[DCTSIZE*4] | dataptr[DCTSIZE*5] | dataptr[DCTSIZE*6] |
            dataptr[DCTSIZE*7]) == 0)
        {
      /* AC terms all zero */
            dctelem_t dcval = (dctelem_t) DESCALE((s32) dataptr[0], PASS1_BITS+3);

            dataptr[DCTSIZE*0] = dcval;
            dataptr[DCTSIZE*1] = dcval;
            dataptr[DCTSIZE*2] = dcval;
            dataptr[DCTSIZE*3] = dcval;
            dataptr[DCTSIZE*4] = dcval;
            dataptr[DCTSIZE*5] = dcval;
            dataptr[DCTSIZE*6] = dcval;
            dataptr[DCTSIZE*7] = dcval;

            dataptr++;          /* advance pointer to next column */
            continue;
        }
#endif

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */

        z2 = (s32) dataptr[DCTSIZE*2];
        z3 = (s32) dataptr[DCTSIZE*6];

        z1 = MULTIPLY(z2 + z3, FIX(0.541196100));
        tmp2 = z1 + MULTIPLY(z3, - FIX(1.847759065));
        tmp3 = z1 + MULTIPLY(z2, FIX(0.765366865));

        tmp0 = ((s32) dataptr[DCTSIZE*0] + (s32) dataptr[DCTSIZE*4]) << CONST_BITS;
        tmp1 = ((s32) dataptr[DCTSIZE*0] - (s32) dataptr[DCTSIZE*4]) << CONST_BITS;

        tmp10 = tmp0 + tmp3;
        tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2;
        tmp12 = tmp1 - tmp2;

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

        tmp0 = (s32) dataptr[DCTSIZE*7];
        tmp1 = (s32) dataptr[DCTSIZE*5];
        tmp2 = (s32) dataptr[DCTSIZE*3];
        tmp3 = (s32) dataptr[DCTSIZE*1];

        z1 = tmp0 + tmp3;
        z2 = tmp1 + tmp2;
        z3 = tmp0 + tmp2;
        z4 = tmp1 + tmp3;
        z5 = MULTIPLY(z3 + z4, FIX(1.175875602)); /* sqrt(2) * c3 */

        tmp0 = MULTIPLY(tmp0, FIX(0.298631336)); /* sqrt(2) * (-c1+c3+c5-c7) */
        tmp1 = MULTIPLY(tmp1, FIX(2.053119869)); /* sqrt(2) * ( c1+c3-c5+c7) */
        tmp2 = MULTIPLY(tmp2, FIX(3.072711026)); /* sqrt(2) * ( c1+c3+c5-c7) */
        tmp3 = MULTIPLY(tmp3, FIX(1.501321110)); /* sqrt(2) * ( c1+c3-c5-c7) */
        z1 = MULTIPLY(z1, - FIX(0.899976223)); /* sqrt(2) * (c7-c3) */
        z2 = MULTIPLY(z2, - FIX(2.562915447)); /* sqrt(2) * (-c1-c3) */
        z3 = MULTIPLY(z3, - FIX(1.961570560)); /* sqrt(2) * (-c3-c5) */
        z4 = MULTIPLY(z4, - FIX(0.390180644)); /* sqrt(2) * (c5-c3) */

        z3 += z5;
        z4 += z5;

        tmp0 += z1 + z3;
        tmp1 += z2 + z4;
        tmp2 += z2 + z3;
        tmp3 += z1 + z4;

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

        dataptr[DCTSIZE*0] = (dctelem_t) DESCALE(tmp10 + tmp3,
                                           CONST_BITS+PASS1_BITS+3);
        dataptr[DCTSIZE*7] = (dctelem_t) DESCALE(tmp10 - tmp3,
                                           CONST_BITS+PASS1_BITS+3);
        dataptr[DCTSIZE*1] = (dctelem_t) DESCALE(tmp11 + tmp2,
                                           CONST_BITS+PASS1_BITS+3);
        dataptr[DCTSIZE*6] = (dctelem_t) DESCALE(tmp11 - tmp2,
                                           CONST_BITS+PASS1_BITS+3);
        dataptr[DCTSIZE*2] = (dctelem_t) DESCALE(tmp12 + tmp1,
                                           CONST_BITS+PASS1_BITS+3);
        dataptr[DCTSIZE*5] = (dctelem_t) DESCALE(tmp12 - tmp1,
                                           CONST_BITS+PASS1_BITS+3);
        dataptr[DCTSIZE*3] = (dctelem_t) DESCALE(tmp13 + tmp0,
                                           CONST_BITS+PASS1_BITS+3);
        dataptr[DCTSIZE*4] = (dctelem_t) DESCALE(tmp13 - tmp0,
                                           CONST_BITS+PASS1_BITS+3);

        dataptr++;                      /* advance pointer to next column */
    }
#endif

#if 1  /*dct avec non classique*/

    s32 tmp0, tmp1, tmp2, tmp3;
    s32 tmp10, tmp11, tmp12, tmp13;
    s32 z1, z2, z3, z4, z5;
    s32 d0, d1, d2, d3, d4, d5, d6, d7;
    dctelem_t * dataptr;
    int rowctr;

    SHIFT_TEMPS

    /* Pass 1: process rows. */
    /* Note results are scaled up by sqrt(8) compared to a true IDCT; */
    /* furthermore, we scale the results by 2**PASS1_BITS. */

    dataptr = p_block;

    for (rowctr = DCTSIZE-1; rowctr >= 0; rowctr--)
    {
        /* Due to quantization, we will usually find that many of the input
         * coefficients are zero, especially the AC terms.  We can exploit this
         * by short-circuiting the IDCT calculation for any row in which all
         * the AC terms are zero.  In that case each output is equal to the
         * DC coefficient (with scale factor as needed).
         * With typical images and quantization tables, half or more of the
         * row DCT calculations can be simplified this way.
         */

        register int * idataptr = (int*)dataptr;
        d0 = dataptr[0];
        d1 = dataptr[1];
        if ( (d1 == 0) && ((idataptr[1] | idataptr[2] | idataptr[3]) == 0) )
        {
      /* AC terms all zero */
            if (d0)
            {
      /* Compute a 32 bit value to assign. */
                dctelem_t dcval = (dctelem_t) (d0 << PASS1_BITS);
                register int v = (dcval & 0xffff) | (dcval << 16);

                idataptr[0] = v;
                idataptr[1] = v;
                idataptr[2] = v;
                idataptr[3] = v;
            }

            dataptr += DCTSIZE; /* advance pointer to next row */
            continue;
        }
        d2 = dataptr[2];
        d3 = dataptr[3];
        d4 = dataptr[4];
        d5 = dataptr[5];
        d6 = dataptr[6];
        d7 = dataptr[7];

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */
        if (d6)
        {
            if (d4)
            {
                if (d2)
                {
                    if (d0)
                    {
            /* d0 != 0, d2 != 0, d4 != 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = (d0 + d4) << CONST_BITS;
                        tmp1 = (d0 - d4) << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp1 + tmp2;
                        tmp12 = tmp1 - tmp2;
                    }
                    else
                    {
                    /* d0 == 0, d2 != 0, d4 != 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = d4 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp2 - tmp0;
                        tmp12 = -(tmp0 + tmp2);
                        }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 != 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp0 = (d0 + d4) << CONST_BITS;
                        tmp1 = (d0 - d4) << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp1 + tmp2;
                        tmp12 = tmp1 - tmp2;
                        }
                    else
                    {
                    /* d0 == 0, d2 == 0, d4 != 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp0 = d4 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp2 - tmp0;
                        tmp12 = -(tmp0 + tmp2);
                        }
                }
            }
            else
            {
                if (d2)
                {
                    if (d0)
                    {
            /* d0 != 0, d2 != 0, d4 == 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = d0 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp0 + tmp2;
                        tmp12 = tmp0 - tmp2;
                    }
                    else
                    {
                    /* d0 == 0, d2 != 0, d4 == 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp10 = tmp3;
                        tmp13 = -tmp3;
                        tmp11 = tmp2;
                        tmp12 = -tmp2;
                            }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 == 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp0 = d0 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp0 + tmp2;
                        tmp12 = tmp0 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 == 0, d4 == 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp10 = tmp3;
                        tmp13 = -tmp3;
                        tmp11 = tmp2;
                        tmp12 = -tmp2;
                    }
                }
            }
        }
        else
        {
            if (d4)
            {
                if (d2)
                {
                    if (d0)
                    {
                    /* d0 != 0, d2 != 0, d4 != 0, d6 == 0 */
                        tmp2 = MULTIPLY(d2, FIX(0.541196100));
                        tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                        tmp0 = (d0 + d4) << CONST_BITS;
                        tmp1 = (d0 - d4) << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp1 + tmp2;
                        tmp12 = tmp1 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 != 0, d4 != 0, d6 == 0 */
                        tmp2 = MULTIPLY(d2, FIX(0.541196100));
                        tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                        tmp0 = d4 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp2 - tmp0;
                        tmp12 = -(tmp0 + tmp2);
                    }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 != 0, d6 == 0 */
                        tmp10 = tmp13 = (d0 + d4) << CONST_BITS;
                        tmp11 = tmp12 = (d0 - d4) << CONST_BITS;
                    }
                    else
                    {
            /* d0 == 0, d2 == 0, d4 != 0, d6 == 0 */
                        tmp10 = tmp13 = d4 << CONST_BITS;
                        tmp11 = tmp12 = -tmp10;
                    }
                }
            }
            else
            {
                if (d2)
                {
                    if (d0)
                    {
            /* d0 != 0, d2 != 0, d4 == 0, d6 == 0 */
                        tmp2 = MULTIPLY(d2, FIX(0.541196100));
                        tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                        tmp0 = d0 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp0 + tmp2;
                        tmp12 = tmp0 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 != 0, d4 == 0, d6 == 0 */
                        tmp2 = MULTIPLY(d2, FIX(0.541196100));
                        tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                        tmp10 = tmp3;
                        tmp13 = -tmp3;
                        tmp11 = tmp2;
                        tmp12 = -tmp2;
                    }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 == 0, d6 == 0 */
                        tmp10 = tmp13 = tmp11 = tmp12 = d0 << CONST_BITS;
                    }
                    else
                    {
            /* d0 == 0, d2 == 0, d4 == 0, d6 == 0 */
                        tmp10 = tmp13 = tmp11 = tmp12 = 0;
                    }
                }
            }
        }


    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

        if (d7)
            {
            if (d5)
            {
                if (d3)
                {
                    if (d1)
                    {
            /* d1 != 0, d3 != 0, d5 != 0, d7 != 0 */
                        z1 = d7 + d1;
                        z2 = d5 + d3;
                        z3 = d7 + d3;
                        z4 = d5 + d1;
                        z5 = MULTIPLY(z3 + z4, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, FIX(0.298631336));
                        tmp1 = MULTIPLY(d5, FIX(2.053119869));
                        tmp2 = MULTIPLY(d3, FIX(3.072711026));
                        tmp3 = MULTIPLY(d1, FIX(1.501321110));
                        z1 = MULTIPLY(z1, - FIX(0.899976223));
                        z2 = MULTIPLY(z2, - FIX(2.562915447));
                        z3 = MULTIPLY(z3, - FIX(1.961570560));
                        z4 = MULTIPLY(z4, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 += z1 + z3;
                        tmp1 += z2 + z4;
                        tmp2 += z2 + z3;
                        tmp3 += z1 + z4;
                    }
                    else
                    {
            /* d1 == 0, d3 != 0, d5 != 0, d7 != 0 */
                        z2 = d5 + d3;
                        z3 = d7 + d3;
                        z5 = MULTIPLY(z3 + d5, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, FIX(0.298631336));
                        tmp1 = MULTIPLY(d5, FIX(2.053119869));
                        tmp2 = MULTIPLY(d3, FIX(3.072711026));
                        z1 = MULTIPLY(d7, - FIX(0.899976223));
                        z2 = MULTIPLY(z2, - FIX(2.562915447));
                        z3 = MULTIPLY(z3, - FIX(1.961570560));
                        z4 = MULTIPLY(d5, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 += z1 + z3;
                        tmp1 += z2 + z4;
                        tmp2 += z2 + z3;
                        tmp3 = z1 + z4;
                        }
                    }
                else
                {
                    if (d1)
                    {
            /* d1 != 0, d3 == 0, d5 != 0, d7 != 0 */
                        z1 = d7 + d1;
                        z4 = d5 + d1;
                        z5 = MULTIPLY(d7 + z4, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, FIX(0.298631336));
                        tmp1 = MULTIPLY(d5, FIX(2.053119869));
                        tmp3 = MULTIPLY(d1, FIX(1.501321110));
                        z1 = MULTIPLY(z1, - FIX(0.899976223));
                        z2 = MULTIPLY(d5, - FIX(2.562915447));
                        z3 = MULTIPLY(d7, - FIX(1.961570560));
                        z4 = MULTIPLY(z4, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 += z1 + z3;
                        tmp1 += z2 + z4;
                        tmp2 = z2 + z3;
                        tmp3 += z1 + z4;
                    }
                    else
                    {
            /* d1 == 0, d3 == 0, d5 != 0, d7 != 0 */
                        z5 = MULTIPLY(d7 + d5, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, - FIX2(0.601344887));
                        tmp1 = MULTIPLY(d5, - FIX2(0.509795578));
                        z1 = MULTIPLY(d7, - FIX(0.899976223));
                        z3 = MULTIPLY(d7, - FIX(1.961570560));
                        z2 = MULTIPLY(d5, - FIX(2.562915447));
                        z4 = MULTIPLY(d5, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 += z3;
                        tmp1 += z4;
                        tmp2 = z2 + z3;
                        tmp3 = z1 + z4;
                    }
                }
            }
            else
            {
                if (d3)
                {
                    if (d1)
                    {
            /* d1 != 0, d3 != 0, d5 == 0, d7 != 0 */
                        z1 = d7 + d1;
                        z3 = d7 + d3;
                        z5 = MULTIPLY(z3 + d1, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, FIX(0.298631336));
                        tmp2 = MULTIPLY(d3, FIX(3.072711026));
                        tmp3 = MULTIPLY(d1, FIX(1.501321110));
                        z1 = MULTIPLY(z1, - FIX(0.899976223));
                        z2 = MULTIPLY(d3, - FIX(2.562915447));
                        z3 = MULTIPLY(z3, - FIX(1.961570560));
                        z4 = MULTIPLY(d1, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 += z1 + z3;
                        tmp1 = z2 + z4;
                        tmp2 += z2 + z3;
                        tmp3 += z1 + z4;
                    }
                    else
                    {
            /* d1 == 0, d3 != 0, d5 == 0, d7 != 0 */
                        z3 = d7 + d3;
                        z5 = MULTIPLY(z3, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, - FIX2(0.601344887));
                        tmp2 = MULTIPLY(d3, FIX(0.509795579));
                        z1 = MULTIPLY(d7, - FIX(0.899976223));
                        z2 = MULTIPLY(d3, - FIX(2.562915447));
                        z3 = MULTIPLY(z3, - FIX2(0.785694958));

                        tmp0 += z3;
                        tmp1 = z2 + z5;
                        tmp2 += z3;
                        tmp3 = z1 + z5;
                    }
                }
                else
                {
                    if (d1)
                    {
            /* d1 != 0, d3 == 0, d5 == 0, d7 != 0 */
                        z1 = d7 + d1;
                        z5 = MULTIPLY(z1, FIX(1.175875602));

                        tmp0 = MULTIPLY(d7, - FIX2(1.662939224));
                        tmp3 = MULTIPLY(d1, FIX2(1.111140466));
                        z1 = MULTIPLY(z1, FIX2(0.275899379));
                        z3 = MULTIPLY(d7, - FIX(1.961570560));
                        z4 = MULTIPLY(d1, - FIX(0.390180644));

                        tmp0 += z1;
                        tmp1 = z4 + z5;
                        tmp2 = z3 + z5;
                        tmp3 += z1;
                    }
                else
                    {
            /* d1 == 0, d3 == 0, d5 == 0, d7 != 0 */
                        tmp0 = MULTIPLY(d7, - FIX2(1.387039845));
                        tmp1 = MULTIPLY(d7, FIX(1.175875602));
                        tmp2 = MULTIPLY(d7, - FIX2(0.785694958));
                        tmp3 = MULTIPLY(d7, FIX2(0.275899379));
                    }
                }
            }
        }
        else
        {
            if (d5)
            {
                if (d3)
                {
                    if (d1)
                    {
            /* d1 != 0, d3 != 0, d5 != 0, d7 == 0 */
                        z2 = d5 + d3;
                        z4 = d5 + d1;
                        z5 = MULTIPLY(d3 + z4, FIX(1.175875602));

                        tmp1 = MULTIPLY(d5, FIX(2.053119869));
                        tmp2 = MULTIPLY(d3, FIX(3.072711026));
                        tmp3 = MULTIPLY(d1, FIX(1.501321110));
                        z1 = MULTIPLY(d1, - FIX(0.899976223));
                        z2 = MULTIPLY(z2, - FIX(2.562915447));
                        z3 = MULTIPLY(d3, - FIX(1.961570560));
                        z4 = MULTIPLY(z4, - FIX(0.390180644));

                        z3 += z5;
                        z4 += z5;

                        tmp0 = z1 + z3;
                        tmp1 += z2 + z4;
                        tmp2 += z2 + z3;
                        tmp3 += z1 + z4;
                    }
                    else
                    {
            /* d1 == 0, d3 != 0, d5 != 0, d7 == 0 */
                        z2 = d5 + d3;
                        z5 = MULTIPLY(z2, FIX(1.175875602));

                        tmp1 = MULTIPLY(d5, FIX2(1.662939225));
                        tmp2 = MULTIPLY(d3, FIX2(1.111140466));
                        z2 = MULTIPLY(z2, - FIX2(1.387039845));
                        z3 = MULTIPLY(d3, - FIX(1.961570560));
                        z4 = MULTIPLY(d5, - FIX(0.390180644));

                        tmp0 = z3 + z5;
                        tmp1 += z2;
                        tmp2 += z2;
                        tmp3 = z4 + z5;
                    }
                }
                else
                {
                    if (d1)
                    {
            /* d1 != 0, d3 == 0, d5 != 0, d7 == 0 */
                        z4 = d5 + d1;
                        z5 = MULTIPLY(z4, FIX(1.175875602));

                        tmp1 = MULTIPLY(d5, - FIX2(0.509795578));
                        tmp3 = MULTIPLY(d1, FIX2(0.601344887));
                        z1 = MULTIPLY(d1, - FIX(0.899976223));
                        z2 = MULTIPLY(d5, - FIX(2.562915447));
                        z4 = MULTIPLY(z4, FIX2(0.785694958));

                        tmp0 = z1 + z5;
                        tmp1 += z4;
                        tmp2 = z2 + z5;
                        tmp3 += z4;
                    }
                    else
                    {
            /* d1 == 0, d3 == 0, d5 != 0, d7 == 0 */
                        tmp0 = MULTIPLY(d5, FIX(1.175875602));
                        tmp1 = MULTIPLY(d5, FIX2(0.275899380));
                        tmp2 = MULTIPLY(d5, - FIX2(1.387039845));
                        tmp3 = MULTIPLY(d5, FIX2(0.785694958));
                    }
                }
            }
            else
            {
                if (d3)
                {
                    if (d1)
                    {
            /* d1 != 0, d3 != 0, d5 == 0, d7 == 0 */
                        z5 = d3 + d1;

                        tmp2 = MULTIPLY(d3, - FIX(1.451774981));
                        tmp3 = MULTIPLY(d1, (FIX(0.211164243) - 1));
                        z1 = MULTIPLY(d1, FIX(1.061594337));
                        z2 = MULTIPLY(d3, - FIX(2.172734803));
                        z4 = MULTIPLY(z5, FIX(0.785694958));
                        z5 = MULTIPLY(z5, FIX(1.175875602));

                        tmp0 = z1 - z4;
                        tmp1 = z2 + z4;
                        tmp2 += z5;
                        tmp3 += z5;
                    }
                    else
                    {
            /* d1 == 0, d3 != 0, d5 == 0, d7 == 0 */
                        tmp0 = MULTIPLY(d3, - FIX2(0.785694958));
                        tmp1 = MULTIPLY(d3, - FIX2(1.387039845));
                        tmp2 = MULTIPLY(d3, - FIX2(0.275899379));
                        tmp3 = MULTIPLY(d3, FIX(1.175875602));
                    }
                }
                else
                {
                    if (d1)
                    {
            /* d1 != 0, d3 == 0, d5 == 0, d7 == 0 */
                        tmp0 = MULTIPLY(d1, FIX2(0.275899379));
                        tmp1 = MULTIPLY(d1, FIX2(0.785694958));
                        tmp2 = MULTIPLY(d1, FIX(1.175875602));
                        tmp3 = MULTIPLY(d1, FIX2(1.387039845));
                    }
                    else
                    {
            /* d1 == 0, d3 == 0, d5 == 0, d7 == 0 */
                        tmp0 = tmp1 = tmp2 = tmp3 = 0;
                    }
                }
            }
        }

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

        dataptr[0] = (dctelem_t) DESCALE(tmp10 + tmp3, CONST_BITS-PASS1_BITS);
        dataptr[7] = (dctelem_t) DESCALE(tmp10 - tmp3, CONST_BITS-PASS1_BITS);
        dataptr[1] = (dctelem_t) DESCALE(tmp11 + tmp2, CONST_BITS-PASS1_BITS);
        dataptr[6] = (dctelem_t) DESCALE(tmp11 - tmp2, CONST_BITS-PASS1_BITS);
        dataptr[2] = (dctelem_t) DESCALE(tmp12 + tmp1, CONST_BITS-PASS1_BITS);
        dataptr[5] = (dctelem_t) DESCALE(tmp12 - tmp1, CONST_BITS-PASS1_BITS);
        dataptr[3] = (dctelem_t) DESCALE(tmp13 + tmp0, CONST_BITS-PASS1_BITS);
        dataptr[4] = (dctelem_t) DESCALE(tmp13 - tmp0, CONST_BITS-PASS1_BITS);

        dataptr += DCTSIZE;              /* advance pointer to next row */
    }

  /* Pass 2: process columns. */
  /* Note that we must descale the results by a factor of 8 == 2**3, */
  /* and also undo the PASS1_BITS scaling. */

    dataptr = p_block;
    for (rowctr = DCTSIZE-1; rowctr >= 0; rowctr--)
    {
    /* Columns of zeroes can be exploited in the same way as we did with rows.
     * However, the row calculation has created many nonzero AC terms, so the
     * simplification applies less often (typically 5% to 10% of the time).
     * On machines with very fast multiplication, it's possible that the
     * test takes more time than it's worth.  In that case this section
     * may be commented out.
     */

        d0 = dataptr[DCTSIZE*0];
        d1 = dataptr[DCTSIZE*1];
        d2 = dataptr[DCTSIZE*2];
        d3 = dataptr[DCTSIZE*3];
        d4 = dataptr[DCTSIZE*4];
        d5 = dataptr[DCTSIZE*5];
        d6 = dataptr[DCTSIZE*6];
        d7 = dataptr[DCTSIZE*7];

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */
        if (d6)
        {
            if (d4)
            {
                if (d2)
                {
                    if (d0)
                    {
            /* d0 != 0, d2 != 0, d4 != 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = (d0 + d4) << CONST_BITS;
                        tmp1 = (d0 - d4) << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp1 + tmp2;
                        tmp12 = tmp1 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 != 0, d4 != 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = d4 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp2 - tmp0;
                        tmp12 = -(tmp0 + tmp2);
                    }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 != 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp0 = (d0 + d4) << CONST_BITS;
                        tmp1 = (d0 - d4) << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp1 + tmp2;
                        tmp12 = tmp1 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 == 0, d4 != 0, d6 != 0 */
                        tmp2 = MULTIPLY(d6, -FIX2(1.306562965));
                        tmp3 = MULTIPLY(d6, FIX(0.541196100));

                        tmp0 = d4 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp2 - tmp0;
                        tmp12 = -(tmp0 + tmp2);
                    }
                }
            }
            else
            {
                if (d2)
                {
                    if (d0)
                    {
            /* d0 != 0, d2 != 0, d4 == 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp0 = d0 << CONST_BITS;

                        tmp10 = tmp0 + tmp3;
                        tmp13 = tmp0 - tmp3;
                        tmp11 = tmp0 + tmp2;
                        tmp12 = tmp0 - tmp2;
                    }
                    else
                    {
            /* d0 == 0, d2 != 0, d4 == 0, d6 != 0 */
                        z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
                        tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
                        tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

                        tmp10 = tmp3;
                        tmp13 = -tmp3;
                        tmp11 = tmp2;
                        tmp12 = -tmp2;
                    }
                }
                else
                {
                    if (d0)
                    {
            /* d0 != 0, d2 == 0, d4 == 0, d6 != 0 */
                    tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                    tmp3 = MULTIPLY(d6, FIX(0.541196100));

                    tmp0 = d0 << CONST_BITS;

                    tmp10 = tmp0 + tmp3;
                    tmp13 = tmp0 - tmp3;
                    tmp11 = tmp0 + tmp2;
                    tmp12 = tmp0 - tmp2;
                }
                else
                {
            /* d0 == 0, d2 == 0, d4 == 0, d6 != 0 */
                    tmp2 = MULTIPLY(d6, - FIX2(1.306562965));
                    tmp3 = MULTIPLY(d6, FIX(0.541196100));
                    tmp10 = tmp3;
                    tmp13 = -tmp3;
                    tmp11 = tmp2;
                    tmp12 = -tmp2;
                }
            }
        }
    }
    else
    {
        if (d4)
        {
            if (d2)
            {
                if (d0)
                {
            /* d0 != 0, d2 != 0, d4 != 0, d6 == 0 */
                    tmp2 = MULTIPLY(d2, FIX(0.541196100));
                    tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                    tmp0 = (d0 + d4) << CONST_BITS;
                    tmp1 = (d0 - d4) << CONST_BITS;

                    tmp10 = tmp0 + tmp3;
                    tmp13 = tmp0 - tmp3;
                    tmp11 = tmp1 + tmp2;
                    tmp12 = tmp1 - tmp2;
                }
                else
                {
            /* d0 == 0, d2 != 0, d4 != 0, d6 == 0 */
                    tmp2 = MULTIPLY(d2, FIX(0.541196100));
                    tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                    tmp0 = d4 << CONST_BITS;

                    tmp10 = tmp0 + tmp3;
                    tmp13 = tmp0 - tmp3;
                    tmp11 = tmp2 - tmp0;
                    tmp12 = -(tmp0 + tmp2);
                }
            }
            else
            {
                if (d0)
                {
            /* d0 != 0, d2 == 0, d4 != 0, d6 == 0 */
                    tmp10 = tmp13 = (d0 + d4) << CONST_BITS;
                    tmp11 = tmp12 = (d0 - d4) << CONST_BITS;
                }
                else
                {
            /* d0 == 0, d2 == 0, d4 != 0, d6 == 0 */
                    tmp10 = tmp13 = d4 << CONST_BITS;
                    tmp11 = tmp12 = -tmp10;
                }
            }
        }
        else
        {
        if (d2)
        {
            if (d0)
            {
            /* d0 != 0, d2 != 0, d4 == 0, d6 == 0 */
                    tmp2 = MULTIPLY(d2, FIX(0.541196100));
                    tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                    tmp0 = d0 << CONST_BITS;

                    tmp10 = tmp0 + tmp3;
                    tmp13 = tmp0 - tmp3;
                    tmp11 = tmp0 + tmp2;
                    tmp12 = tmp0 - tmp2;
            }
            else
            {
            /* d0 == 0, d2 != 0, d4 == 0, d6 == 0 */
                    tmp2 = MULTIPLY(d2, FIX(0.541196100));
                    tmp3 = MULTIPLY(d2, (FIX(1.306562965) + .5));

                    tmp10 = tmp3;
                    tmp13 = -tmp3;
                    tmp11 = tmp2;
                    tmp12 = -tmp2;
            }
        }
        else
        {
            if (d0)
                {
            /* d0 != 0, d2 == 0, d4 == 0, d6 == 0 */
                    tmp10 = tmp13 = tmp11 = tmp12 = d0 << CONST_BITS;
                }
                else
                {
            /* d0 == 0, d2 == 0, d4 == 0, d6 == 0 */
                    tmp10 = tmp13 = tmp11 = tmp12 = 0;
                }
            }
        }
    }

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */
    if (d7)
    {
        if (d5)
        {
            if (d3)
            {
                if (d1)
                {
            /* d1 != 0, d3 != 0, d5 != 0, d7 != 0 */
                    z1 = d7 + d1;
                    z2 = d5 + d3;
                    z3 = d7 + d3;
                    z4 = d5 + d1;
                    z5 = MULTIPLY(z3 + z4, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, FIX(0.298631336));
                    tmp1 = MULTIPLY(d5, FIX(2.053119869));
                    tmp2 = MULTIPLY(d3, FIX(3.072711026));
                    tmp3 = MULTIPLY(d1, FIX(1.501321110));
                    z1 = MULTIPLY(z1, - FIX(0.899976223));
                    z2 = MULTIPLY(z2, - FIX(2.562915447));
                    z3 = MULTIPLY(z3, - FIX(1.961570560));
                    z4 = MULTIPLY(z4, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 += z1 + z3;
                    tmp1 += z2 + z4;
                    tmp2 += z2 + z3;
                    tmp3 += z1 + z4;
                }
                else
                {
            /* d1 == 0, d3 != 0, d5 != 0, d7 != 0 */
                    z2 = d5 + d3;
                    z3 = d7 + d3;
                    z5 = MULTIPLY(z3 + d5, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, FIX(0.298631336));
                    tmp1 = MULTIPLY(d5, FIX(2.053119869));
                    tmp2 = MULTIPLY(d3, FIX(3.072711026));
                    z1 = MULTIPLY(d7, - FIX(0.899976223));
                    z2 = MULTIPLY(z2, - FIX(2.562915447));
                    z3 = MULTIPLY(z3, - FIX(1.961570560));
                    z4 = MULTIPLY(d5, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 += z1 + z3;
                    tmp1 += z2 + z4;
                    tmp2 += z2 + z3;
                    tmp3 = z1 + z4;
                }
            }
            else
            {
                if (d1)
                {
            /* d1 != 0, d3 == 0, d5 != 0, d7 != 0 */
                    z1 = d7 + d1;
                    z4 = d5 + d1;
                    z5 = MULTIPLY(d7 + z4, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, FIX(0.298631336));
                    tmp1 = MULTIPLY(d5, FIX(2.053119869));
                    tmp3 = MULTIPLY(d1, FIX(1.501321110));
                    z1 = MULTIPLY(z1, - FIX(0.899976223));
                    z2 = MULTIPLY(d5, - FIX(2.562915447));
                    z3 = MULTIPLY(d7, - FIX(1.961570560));
                    z4 = MULTIPLY(z4, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 += z1 + z3;
                    tmp1 += z2 + z4;
                    tmp2 = z2 + z3;
                    tmp3 += z1 + z4;
                }
                else
                {
            /* d1 == 0, d3 == 0, d5 != 0, d7 != 0 */
                    z5 = MULTIPLY(d5 + d7, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, - FIX2(0.601344887));
                    tmp1 = MULTIPLY(d5, - FIX2(0.509795578));
                    z1 = MULTIPLY(d7, - FIX(0.899976223));
                    z3 = MULTIPLY(d7, - FIX(1.961570560));
                    z2 = MULTIPLY(d5, - FIX(2.562915447));
                    z4 = MULTIPLY(d5, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 += z3;
                    tmp1 += z4;
                    tmp2 = z2 + z3;
                    tmp3 = z1 + z4;
                }
            }
        }
        else
        {
            if (d3)
            {
                if (d1)
                {
            /* d1 != 0, d3 != 0, d5 == 0, d7 != 0 */
                    z1 = d7 + d1;
                    z3 = d7 + d3;
                    z5 = MULTIPLY(z3 + d1, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, FIX(0.298631336));
                    tmp2 = MULTIPLY(d3, FIX(3.072711026));
                    tmp3 = MULTIPLY(d1, FIX(1.501321110));
                    z1 = MULTIPLY(z1, - FIX(0.899976223));
                    z2 = MULTIPLY(d3, - FIX(2.562915447));
                    z3 = MULTIPLY(z3, - FIX(1.961570560));
                    z4 = MULTIPLY(d1, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 += z1 + z3;
                    tmp1 = z2 + z4;
                    tmp2 += z2 + z3;
                    tmp3 += z1 + z4;
                }
                else
                {
            /* d1 == 0, d3 != 0, d5 == 0, d7 != 0 */
                    z3 = d7 + d3;
                    z5 = MULTIPLY(z3, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, - FIX2(0.601344887));
                    z1 = MULTIPLY(d7, - FIX(0.899976223));
                    tmp2 = MULTIPLY(d3, FIX(0.509795579));
                    z2 = MULTIPLY(d3, - FIX(2.562915447));
                    z3 = MULTIPLY(z3, - FIX2(0.785694958));

                    tmp0 += z3;
                    tmp1 = z2 + z5;
                    tmp2 += z3;
                    tmp3 = z1 + z5;
                }
            }
            else
            {
                if (d1)
                {
            /* d1 != 0, d3 == 0, d5 == 0, d7 != 0 */
                    z1 = d7 + d1;
                    z5 = MULTIPLY(z1, FIX(1.175875602));

                    tmp0 = MULTIPLY(d7, - FIX2(1.662939224));
                    tmp3 = MULTIPLY(d1, FIX2(1.111140466));
                    z1 = MULTIPLY(z1, FIX2(0.275899379));
                    z3 = MULTIPLY(d7, - FIX(1.961570560));
                    z4 = MULTIPLY(d1, - FIX(0.390180644));

                    tmp0 += z1;
                    tmp1 = z4 + z5;
                    tmp2 = z3 + z5;
                    tmp3 += z1;
                }
                else
                {
            /* d1 == 0, d3 == 0, d5 == 0, d7 != 0 */
                    tmp0 = MULTIPLY(d7, - FIX2(1.387039845));
                    tmp1 = MULTIPLY(d7, FIX(1.175875602));
                    tmp2 = MULTIPLY(d7, - FIX2(0.785694958));
                    tmp3 = MULTIPLY(d7, FIX2(0.275899379));
                }
            }
        }
    }
    else
    {
        if (d5)
        {
            if (d3)
            {
                if (d1)
                {
            /* d1 != 0, d3 != 0, d5 != 0, d7 == 0 */
                    z2 = d5 + d3;
                    z4 = d5 + d1;
                    z5 = MULTIPLY(d3 + z4, FIX(1.175875602));

                    tmp1 = MULTIPLY(d5, FIX(2.053119869));
                    tmp2 = MULTIPLY(d3, FIX(3.072711026));
                    tmp3 = MULTIPLY(d1, FIX(1.501321110));
                    z1 = MULTIPLY(d1, - FIX(0.899976223));
                    z2 = MULTIPLY(z2, - FIX(2.562915447));
                    z3 = MULTIPLY(d3, - FIX(1.961570560));
                    z4 = MULTIPLY(z4, - FIX(0.390180644));

                    z3 += z5;
                    z4 += z5;

                    tmp0 = z1 + z3;
                    tmp1 += z2 + z4;
                    tmp2 += z2 + z3;
                    tmp3 += z1 + z4;
                }
                else
                {
            /* d1 == 0, d3 != 0, d5 != 0, d7 == 0 */
                    z2 = d5 + d3;
                    z5 = MULTIPLY(z2, FIX(1.175875602));

                    tmp1 = MULTIPLY(d5, FIX2(1.662939225));
                    tmp2 = MULTIPLY(d3, FIX2(1.111140466));
                    z2 = MULTIPLY(z2, - FIX2(1.387039845));
                    z3 = MULTIPLY(d3, - FIX(1.961570560));
                    z4 = MULTIPLY(d5, - FIX(0.390180644));

                    tmp0 = z3 + z5;
                    tmp1 += z2;
                    tmp2 += z2;
                    tmp3 = z4 + z5;
                }
            }
            else
            {
                if (d1)
                {
            /* d1 != 0, d3 == 0, d5 != 0, d7 == 0 */
                    z4 = d5 + d1;
                    z5 = MULTIPLY(z4, FIX(1.175875602));

                    tmp1 = MULTIPLY(d5, - FIX2(0.509795578));
                    tmp3 = MULTIPLY(d1, FIX2(0.601344887));
                    z1 = MULTIPLY(d1, - FIX(0.899976223));
                    z2 = MULTIPLY(d5, - FIX(2.562915447));
                    z4 = MULTIPLY(z4, FIX2(0.785694958));

                    tmp0 = z1 + z5;
                    tmp1 += z4;
                    tmp2 = z2 + z5;
                    tmp3 += z4;
                }
                else
                {
            /* d1 == 0, d3 == 0, d5 != 0, d7 == 0 */
                    tmp0 = MULTIPLY(d5, FIX(1.175875602));
                    tmp1 = MULTIPLY(d5, FIX2(0.275899380));
                    tmp2 = MULTIPLY(d5, - FIX2(1.387039845));
                    tmp3 = MULTIPLY(d5, FIX2(0.785694958));
                }
            }
        }
        else
        {
            if (d3)
            {
                if (d1)
                {
            /* d1 != 0, d3 != 0, d5 == 0, d7 == 0 */
                    z5 = d3 + d1;

                    tmp2 = MULTIPLY(d3, - FIX(1.451774981));
                    tmp3 = MULTIPLY(d1, (FIX(0.211164243) - 1));
                    z1 = MULTIPLY(d1, FIX(1.061594337));
                    z2 = MULTIPLY(d3, - FIX(2.172734803));
                    z4 = MULTIPLY(z5, FIX(0.785694958));
                    z5 = MULTIPLY(z5, FIX(1.175875602));

                    tmp0 = z1 - z4;
                    tmp1 = z2 + z4;
                    tmp2 += z5;
                    tmp3 += z5;
                }
                else
                {
            /* d1 == 0, d3 != 0, d5 == 0, d7 == 0 */
                    tmp0 = MULTIPLY(d3, - FIX2(0.785694958));
                    tmp1 = MULTIPLY(d3, - FIX2(1.387039845));
                    tmp2 = MULTIPLY(d3, - FIX2(0.275899379));
                    tmp3 = MULTIPLY(d3, FIX(1.175875602));
                }
            }
            else
            {
                if (d1)
                {
            /* d1 != 0, d3 == 0, d5 == 0, d7 == 0 */
                    tmp0 = MULTIPLY(d1, FIX2(0.275899379));
                    tmp1 = MULTIPLY(d1, FIX2(0.785694958));
                    tmp2 = MULTIPLY(d1, FIX(1.175875602));
                    tmp3 = MULTIPLY(d1, FIX2(1.387039845));
                }
                else
                {
            /* d1 == 0, d3 == 0, d5 == 0, d7 == 0 */
                    tmp0 = tmp1 = tmp2 = tmp3 = 0;
                }
            }
        }
    }

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

    dataptr[DCTSIZE*0] = (dctelem_t) DESCALE(tmp10 + tmp3,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*7] = (dctelem_t) DESCALE(tmp10 - tmp3,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*1] = (dctelem_t) DESCALE(tmp11 + tmp2,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*6] = (dctelem_t) DESCALE(tmp11 - tmp2,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*2] = (dctelem_t) DESCALE(tmp12 + tmp1,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*5] = (dctelem_t) DESCALE(tmp12 - tmp1,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*3] = (dctelem_t) DESCALE(tmp13 + tmp0,
                       CONST_BITS+PASS1_BITS+3);
    dataptr[DCTSIZE*4] = (dctelem_t) DESCALE(tmp13 - tmp0,
                       CONST_BITS+PASS1_BITS+3);

    dataptr++;             /* advance pointer to next column */
    }
#endif
}
#endif
