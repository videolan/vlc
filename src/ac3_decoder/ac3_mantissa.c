/*****************************************************************************
 * ac3_mantissa.c: ac3 mantissa computation
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include "defs.h"

#include <stdio.h>                                           /* "intf_msg.h" */

#include "common.h"

#include "ac3_decoder.h"
#include "ac3_internal.h"
#include "ac3_bit_stream.h"

#include "intf_msg.h"

#define Q0 ((-2 << 15) / 3.0)
#define Q1 (0)
#define Q2 ((2 << 15) / 3.0)
static float q_1_0[ 32 ] = { Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0,
			     Q1, Q1, Q1, Q1, Q1, Q1, Q1, Q1, Q1,
			     Q2, Q2, Q2, Q2, Q2, Q2, Q2, Q2, Q2,
			     0, 0, 0, 0, 0 };
static float q_1_1[ 32 ] = { Q0, Q0, Q0, Q1, Q1, Q1, Q2, Q2, Q2,
			     Q0, Q0, Q0, Q1, Q1, Q1, Q2, Q2, Q2,
			     Q0, Q0, Q0, Q1, Q1, Q1, Q2, Q2, Q2,
			     0, 0, 0, 0, 0 };
static float q_1_2[ 32 ] = { Q0, Q1, Q2, Q0, Q1, Q2, Q0, Q1, Q2,
			     Q0, Q1, Q2, Q0, Q1, Q2, Q0, Q1, Q2,
			     Q0, Q1, Q2, Q0, Q1, Q2, Q0, Q1, Q2,
			     0, 0, 0, 0, 0 };
#undef Q0
#undef Q1
#undef Q2

#define Q0 ((-4 << 15) / 5.0)
#define Q1 ((-2 << 15) / 5.0)
#define Q2 (0)
#define Q3 ((2 << 15) / 5.0)
#define Q4 ((4 << 15) / 5.0)
static float q_2_0[ 128 ] =
  { Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,
    Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,
    Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,
    Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,
    Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,
     0, 0, 0 };
static float q_2_1[ 128 ] =
  { Q0,Q0,Q0,Q0,Q0,Q1,Q1,Q1,Q1,Q1,Q2,Q2,Q2,Q2,Q2,Q3,Q3,Q3,Q3,Q3,Q4,Q4,Q4,Q4,Q4,
    Q0,Q0,Q0,Q0,Q0,Q1,Q1,Q1,Q1,Q1,Q2,Q2,Q2,Q2,Q2,Q3,Q3,Q3,Q3,Q3,Q4,Q4,Q4,Q4,Q4,
    Q0,Q0,Q0,Q0,Q0,Q1,Q1,Q1,Q1,Q1,Q2,Q2,Q2,Q2,Q2,Q3,Q3,Q3,Q3,Q3,Q4,Q4,Q4,Q4,Q4,
    Q0,Q0,Q0,Q0,Q0,Q1,Q1,Q1,Q1,Q1,Q2,Q2,Q2,Q2,Q2,Q3,Q3,Q3,Q3,Q3,Q4,Q4,Q4,Q4,Q4,
    Q0,Q0,Q0,Q0,Q0,Q1,Q1,Q1,Q1,Q1,Q2,Q2,Q2,Q2,Q2,Q3,Q3,Q3,Q3,Q3,Q4,Q4,Q4,Q4,Q4,
     0, 0, 0 };
static float q_2_2[ 128 ] =
  { Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,
    Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,
    Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,
    Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,
    Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,
     0, 0, 0 };
#undef Q0
#undef Q1
#undef Q2
#undef Q3
#undef Q4

#define Q0 ((-10 << 15) / 11.0)
#define Q1 ((-8 << 15) / 11.0)
#define Q2 ((-6 << 15) / 11.0)
#define Q3 ((-4 << 15) / 11.0)
#define Q4 ((-2 << 15) / 11.0)
#define Q5 (0)
#define Q6 ((2 << 15) / 11.0)
#define Q7 ((4 << 15) / 11.0)
#define Q8 ((6 << 15) / 11.0)
#define Q9 ((8 << 15) / 11.0)
#define QA ((10 << 15) / 11.0)
static float q_4_0[ 128 ] = { Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0,
			      Q1, Q1, Q1, Q1, Q1, Q1, Q1, Q1, Q1, Q1, Q1,
			      Q2, Q2, Q2, Q2, Q2, Q2, Q2, Q2, Q2, Q2, Q2,
			      Q3, Q3, Q3, Q3, Q3, Q3, Q3, Q3, Q3, Q3, Q3,
			      Q4, Q4, Q4, Q4, Q4, Q4, Q4, Q4, Q4, Q4, Q4,
			      Q5, Q5, Q5, Q5, Q5, Q5, Q5, Q5, Q5, Q5, Q5,
			      Q6, Q6, Q6, Q6, Q6, Q6, Q6, Q6, Q6, Q6, Q6,
			      Q7, Q7, Q7, Q7, Q7, Q7, Q7, Q7, Q7, Q7, Q7,
			      Q8, Q8, Q8, Q8, Q8, Q8, Q8, Q8, Q8, Q8, Q8,
			      Q9, Q9, Q9, Q9, Q9, Q9, Q9, Q9, Q9, Q9, Q9,
			      QA, QA, QA, QA, QA, QA, QA, QA, QA, QA, QA,
			       0,  0,  0,  0,  0,  0,  0};
static float q_4_1[ 128 ] = { Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			      Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			      Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			      Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			      Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			      Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			      Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			      Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			      Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			      Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			      Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
			       0,  0,  0,  0,  0,  0,  0};
#undef Q0
#undef Q1
#undef Q2
#undef Q3
#undef Q4
#undef Q5
#undef Q6
#undef Q7
#undef Q8
#undef Q9
#undef QA

/* Lookup tables of 0.16 two's complement quantization values */

static float q_3[8] = { (-6 << 15)/7.0, (-4 << 15)/7.0, (-2 << 15)/7.0,
                          0           , (2 << 15)/7.0, (4 << 15)/7.0,
		        (6 << 15)/7.0, 0 };

static float q_5[16] = { (-14 << 15)/15.0, (-12 << 15)/15.0, (-10 << 15)/15.0,
                         (-8 << 15)/15.0, (-6 << 15)/15.0, (-4 << 15)/15.0,
                         (-2 << 15)/15.0,    0            ,  (2 << 15)/15.0,
                          (4 << 15)/15.0,  (6 << 15)/15.0,  (8 << 15)/15.0,
                         (10 << 15)/15.0, (12 << 15)/15.0, (14 << 15)/15.0,
			 0 };

/* These store the persistent state of the packed mantissas */
static float q_1[2];
static float q_2[2];
static float q_4[1];
static s32 q_1_pointer;
static s32 q_2_pointer;
static s32 q_4_pointer;

/* Conversion from bap to number of bits in the mantissas
 * zeros account for cases 0,1,2,4 which are special cased */
static u16 qnttztab[16] = { 0, 0, 0, 3, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16};

static float exp_lut[ 25 ] =
{
    6.10351562500000000000000000e-05,
    3.05175781250000000000000000e-05,
    1.52587890625000000000000000e-05,
    7.62939453125000000000000000e-06,
    3.81469726562500000000000000e-06,
    1.90734863281250000000000000e-06,
    9.53674316406250000000000000e-07,
    4.76837158203125000000000000e-07,
    2.38418579101562500000000000e-07,
    1.19209289550781250000000000e-07,
    5.96046447753906250000000000e-08,
    2.98023223876953125000000000e-08,
    1.49011611938476562500000000e-08,
    7.45058059692382812500000000e-09,
    3.72529029846191406250000000e-09,
    1.86264514923095703125000000e-09,
    9.31322574615478515625000000e-10,
    4.65661287307739257812500000e-10,
    2.32830643653869628906250000e-10,
    1.16415321826934814453125000e-10,
    5.82076609134674072265625000e-11,
    2.91038304567337036132812500e-11,
    1.45519152283668518066406250e-11,
    7.27595761418342590332031250e-12,
    3.63797880709171295166015625e-12,
};

/* Fetch an unpacked, left justified, and properly biased/dithered mantissa value */
static __inline__ float float_get (ac3dec_t * p_ac3dec, u16 bap, u16 exp)
{
    u32 group_code;

    /* If the bap is 0-5 then we have special cases to take care of */
    switch (bap) {
    case 0:
	return (0);	/* FIXME dither */

    case 1:
	if (q_1_pointer >= 0) {
	    return (q_1[q_1_pointer--] * exp_lut[exp]);
	}

	NeedBits (&(p_ac3dec->bit_stream), 5);
	group_code = p_ac3dec->bit_stream.buffer >> (32 - 5);
	DumpBits (&(p_ac3dec->bit_stream), 5);
	if (group_code >= 27) {
	    intf_ErrMsg ( "ac3dec error: invalid mantissa\n" );
	}

	q_1[ 1 ] = q_1_1[ group_code ];
	q_1[ 0 ] = q_1_2[ group_code ];

	q_1_pointer = 1;

	return (q_1_0[group_code] * exp_lut[exp]);

    case 2:
	if (q_2_pointer >= 0) {
	    return (q_2[q_2_pointer--] * exp_lut[exp]);
	}
	NeedBits (&(p_ac3dec->bit_stream), 7);
	group_code = p_ac3dec->bit_stream.buffer >> (32 - 7);
	DumpBits (&(p_ac3dec->bit_stream), 7);

	if (group_code >= 125) {
	    intf_ErrMsg ( "ac3dec error: invalid mantissa\n" );
	}

	q_2[ 1 ] = q_2_1[ group_code ];
	q_2[ 0 ] = q_2_2[ group_code ];

	q_2_pointer = 1;

	return (q_2_0[ group_code ] * exp_lut[exp]);

    case 3:
	NeedBits (&(p_ac3dec->bit_stream), 3);
	group_code = p_ac3dec->bit_stream.buffer >> (32 - 3);
	DumpBits (&(p_ac3dec->bit_stream), 3);

	if (group_code >= 7) {
	    intf_ErrMsg ( "ac3dec error: invalid mantissa\n" );
	}

	return (q_3[group_code] * exp_lut[exp]);

    case 4:
	if (q_4_pointer >= 0) {
	    return (q_4[q_4_pointer--] * exp_lut[exp]);
	}
	NeedBits (&(p_ac3dec->bit_stream), 7);
	group_code = p_ac3dec->bit_stream.buffer >> (32 - 7);
	DumpBits (&(p_ac3dec->bit_stream), 7);

	if (group_code >= 121) {
	    intf_ErrMsg ( "ac3dec error: invalid mantissa\n" );
	}

	q_4[ 0 ] = q_4_1[ group_code ];

	q_4_pointer = 0;

	return (q_4_0[ group_code ] * exp_lut[exp]);

    case 5:
	NeedBits (&(p_ac3dec->bit_stream), 4);
	group_code = p_ac3dec->bit_stream.buffer >> (32 - 4);
	DumpBits (&(p_ac3dec->bit_stream), 4);

	if (group_code >= 15) {
	    intf_ErrMsg ( "ac3dec error: invalid mantissa\n" );
	}

	return (q_5[group_code] * exp_lut[exp]);

    default:
	NeedBits (&(p_ac3dec->bit_stream), qnttztab[bap]);
	group_code = (((s32)(p_ac3dec->bit_stream.buffer)) >> (32 - qnttztab[bap])) << (16 - qnttztab[bap]);
	DumpBits (&(p_ac3dec->bit_stream), qnttztab[bap]);

	return (((s32)group_code) * exp_lut[exp]);
    }
}

static __inline__ void uncouple_channel (ac3dec_t * p_ac3dec, u32 ch)
{
    u32 bnd = 0;
    u32 i,j;
    float cpl_coord = 0;
    u32 cpl_exp_tmp;
    u32 cpl_mant_tmp;

    for (i = p_ac3dec->audblk.cplstrtmant; i < p_ac3dec->audblk.cplendmant;) {
        if (!p_ac3dec->audblk.cplbndstrc[bnd]) {
            cpl_exp_tmp = p_ac3dec->audblk.cplcoexp[ch][bnd] + 3 * p_ac3dec->audblk.mstrcplco[ch];
            if (p_ac3dec->audblk.cplcoexp[ch][bnd] == 15)
                cpl_mant_tmp = (p_ac3dec->audblk.cplcomant[ch][bnd]) << 12;
            else
                cpl_mant_tmp = ((0x10) | p_ac3dec->audblk.cplcomant[ch][bnd]) << 11;

            cpl_coord = ((s16)cpl_mant_tmp) * exp_lut[cpl_exp_tmp];
        }
        bnd++;

        for (j=0;j < 12; j++) {
            p_ac3dec->coeffs.fbw[ch][i]  = cpl_coord * p_ac3dec->audblk.cplfbw[i];
            i++;
        }
    }
}

void mantissa_unpack (ac3dec_t * p_ac3dec)
{
    int i, j;

    q_1_pointer = -1;
    q_2_pointer = -1;
    q_4_pointer = -1;

    if (p_ac3dec->audblk.cplinu) {
        /* 1 */
        for (i = 0; !p_ac3dec->audblk.chincpl[i]; i++) {
            for (j = 0; j < p_ac3dec->audblk.endmant[i]; j++) {
                p_ac3dec->coeffs.fbw[i][j] = float_get (p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.fbw_exp[i][j]);
            }
        }

        /* 2 */
        for (j = 0; j < p_ac3dec->audblk.endmant[i]; j++) {
            p_ac3dec->coeffs.fbw[i][j] = float_get (p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.fbw_exp[i][j]);
        }
        for (j = p_ac3dec->audblk.cplstrtmant; j < p_ac3dec->audblk.cplendmant; j++) {
            p_ac3dec->audblk.cplfbw[j] = float_get (p_ac3dec, p_ac3dec->audblk.cpl_bap[j], p_ac3dec->audblk.cpl_exp[j]);
        }
        uncouple_channel (p_ac3dec, i);

        /* 3 */
        for (i++; i < p_ac3dec->bsi.nfchans; i++) {
            for (j = 0; j < p_ac3dec->audblk.endmant[i]; j++) {
                p_ac3dec->coeffs.fbw[i][j] = float_get (p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.fbw_exp[i][j]);
            }
            if (p_ac3dec->audblk.chincpl[i]) {
                uncouple_channel (p_ac3dec, i);
            }
        }
    } else {
        for (i = 0; i < p_ac3dec->bsi.nfchans; i++) {
            for (j = 0; j < p_ac3dec->audblk.endmant[i]; j++) {
                p_ac3dec->coeffs.fbw[i][j] = float_get (p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j], p_ac3dec->audblk.fbw_exp[i][j]);
            }
        }
    }

    if (p_ac3dec->bsi.lfeon) {
        /* There are always 7 mantissas for lfe, no dither for lfe */
        for (j = 0; j < 7; j++) {
            p_ac3dec->coeffs.lfe[j] = float_get (p_ac3dec, p_ac3dec->audblk.lfe_bap[j], p_ac3dec->audblk.lfe_exp[j]);
        }
    }
}
