/*****************************************************************************
 * ac3_mantissa.c: ac3 mantissa computation
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: ac3_mantissa.c,v 1.26 2001/05/06 04:32:02 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Aaron Holtzman <aholtzma@engr.uvic.ca>
 *          Renaud Dartus <reno@videolan.org>
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

#include <string.h>                                              /* memcpy() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "audio_output.h"

#include "ac3_decoder.h"
#include "ac3_decoder_thread.h"

#include "ac3_internal.h"

#include "intf_msg.h"

#define Q0 ((-2 << 15) / 3.0)
#define Q1 (0)
#define Q2 ((2 << 15) / 3.0)
static const float q_1_0[ 32 ] =
{
    Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0,
    Q1, Q1, Q1, Q1, Q1, Q1, Q1, Q1, Q1,
    Q2, Q2, Q2, Q2, Q2, Q2, Q2, Q2, Q2,
    0, 0, 0, 0, 0
};
static const float q_1_1[ 32 ] =
{
    Q0, Q0, Q0, Q1, Q1, Q1, Q2, Q2, Q2,
    Q0, Q0, Q0, Q1, Q1, Q1, Q2, Q2, Q2,
    Q0, Q0, Q0, Q1, Q1, Q1, Q2, Q2, Q2,
    0, 0, 0, 0, 0
};
static const float q_1_2[ 32 ] =
{
    Q0, Q1, Q2, Q0, Q1, Q2, Q0, Q1, Q2,
    Q0, Q1, Q2, Q0, Q1, Q2, Q0, Q1, Q2,
    Q0, Q1, Q2, Q0, Q1, Q2, Q0, Q1, Q2,
    0, 0, 0, 0, 0
};
#undef Q0
#undef Q1
#undef Q2

#define Q0 ((-4 << 15) / 5.0)
#define Q1 ((-2 << 15) / 5.0)
#define Q2 (0)
#define Q3 ((2 << 15) / 5.0)
#define Q4 ((4 << 15) / 5.0)
static const float q_2_0[ 128 ] =
{
    Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,Q0,
    Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,Q1,
    Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,Q2,
    Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,Q3,
    Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,Q4,
    0, 0, 0
};
static const float q_2_1[ 128 ] =
{
    Q0,Q0,Q0,Q0,Q0,Q1,Q1,Q1,Q1,Q1,Q2,Q2,Q2,Q2,Q2,Q3,Q3,Q3,Q3,Q3,Q4,Q4,Q4,Q4,Q4,
    Q0,Q0,Q0,Q0,Q0,Q1,Q1,Q1,Q1,Q1,Q2,Q2,Q2,Q2,Q2,Q3,Q3,Q3,Q3,Q3,Q4,Q4,Q4,Q4,Q4,
    Q0,Q0,Q0,Q0,Q0,Q1,Q1,Q1,Q1,Q1,Q2,Q2,Q2,Q2,Q2,Q3,Q3,Q3,Q3,Q3,Q4,Q4,Q4,Q4,Q4,
    Q0,Q0,Q0,Q0,Q0,Q1,Q1,Q1,Q1,Q1,Q2,Q2,Q2,Q2,Q2,Q3,Q3,Q3,Q3,Q3,Q4,Q4,Q4,Q4,Q4,
    Q0,Q0,Q0,Q0,Q0,Q1,Q1,Q1,Q1,Q1,Q2,Q2,Q2,Q2,Q2,Q3,Q3,Q3,Q3,Q3,Q4,Q4,Q4,Q4,Q4,
    0, 0, 0
};
static const float q_2_2[ 128 ] =
{
    Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,
    Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,
    Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,
    Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,
    Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,Q0,Q1,Q2,Q3,Q4,
    0, 0, 0
};
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
static const float q_4_0[ 128 ] =
{
    Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0, Q0,
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
    0,  0,  0,  0,  0,  0,  0
};
static const float q_4_1[ 128 ] =
{
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
    Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, QA,
    0,  0,  0,  0,  0,  0,  0
};
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

static const float q_3[8] =
{
    (-6 << 15)/7.0, (-4 << 15)/7.0, (-2 << 15)/7.0,
    0           , (2 << 15)/7.0, (4 << 15)/7.0,
    (6 << 15)/7.0, 0
};

static const float q_5[16] =
{
    (-14 << 15)/15.0, (-12 << 15)/15.0, (-10 << 15)/15.0,
    (-8 << 15)/15.0,  (-6 << 15)/15.0,  (-4 << 15)/15.0,
    (-2 << 15)/15.0,  0            ,    (2 << 15)/15.0,
    (4 << 15)/15.0,   (6 << 15)/15.0,   (8 << 15)/15.0,
    (10 << 15)/15.0,  (12 << 15)/15.0,  (14 << 15)/15.0,
    0
};

/* Conversion from bap to number of bits in the mantissas
 * zeros account for cases 0,1,2,4 which are special cased */
static const u16 qnttztab[16] =
{
    0, 0, 0, 3, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16
};

static const float scale_factor[25] =
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

static const u16 dither_lut[256] =
{
 0x0000, 0xa011, 0xe033, 0x4022, 0x6077, 0xc066, 0x8044, 0x2055,
 0xc0ee, 0x60ff, 0x20dd, 0x80cc, 0xa099, 0x0088, 0x40aa, 0xe0bb,
 0x21cd, 0x81dc, 0xc1fe, 0x61ef, 0x41ba, 0xe1ab, 0xa189, 0x0198,
 0xe123, 0x4132, 0x0110, 0xa101, 0x8154, 0x2145, 0x6167, 0xc176,
 0x439a, 0xe38b, 0xa3a9, 0x03b8, 0x23ed, 0x83fc, 0xc3de, 0x63cf,
 0x8374, 0x2365, 0x6347, 0xc356, 0xe303, 0x4312, 0x0330, 0xa321,
 0x6257, 0xc246, 0x8264, 0x2275, 0x0220, 0xa231, 0xe213, 0x4202,
 0xa2b9, 0x02a8, 0x428a, 0xe29b, 0xc2ce, 0x62df, 0x22fd, 0x82ec,
 0x8734, 0x2725, 0x6707, 0xc716, 0xe743, 0x4752, 0x0770, 0xa761,
 0x47da, 0xe7cb, 0xa7e9, 0x07f8, 0x27ad, 0x87bc, 0xc79e, 0x678f,
 0xa6f9, 0x06e8, 0x46ca, 0xe6db, 0xc68e, 0x669f, 0x26bd, 0x86ac,
 0x6617, 0xc606, 0x8624, 0x2635, 0x0660, 0xa671, 0xe653, 0x4642,
 0xc4ae, 0x64bf, 0x249d, 0x848c, 0xa4d9, 0x04c8, 0x44ea, 0xe4fb,
 0x0440, 0xa451, 0xe473, 0x4462, 0x6437, 0xc426, 0x8404, 0x2415,
 0xe563, 0x4572, 0x0550, 0xa541, 0x8514, 0x2505, 0x6527, 0xc536,
 0x258d, 0x859c, 0xc5be, 0x65af, 0x45fa, 0xe5eb, 0xa5c9, 0x05d8,
 0xae79, 0x0e68, 0x4e4a, 0xee5b, 0xce0e, 0x6e1f, 0x2e3d, 0x8e2c,
 0x6e97, 0xce86, 0x8ea4, 0x2eb5, 0x0ee0, 0xaef1, 0xeed3, 0x4ec2,
 0x8fb4, 0x2fa5, 0x6f87, 0xcf96, 0xefc3, 0x4fd2, 0x0ff0, 0xafe1,
 0x4f5a, 0xef4b, 0xaf69, 0x0f78, 0x2f2d, 0x8f3c, 0xcf1e, 0x6f0f,
 0xede3, 0x4df2, 0x0dd0, 0xadc1, 0x8d94, 0x2d85, 0x6da7, 0xcdb6,
 0x2d0d, 0x8d1c, 0xcd3e, 0x6d2f, 0x4d7a, 0xed6b, 0xad49, 0x0d58,
 0xcc2e, 0x6c3f, 0x2c1d, 0x8c0c, 0xac59, 0x0c48, 0x4c6a, 0xec7b,
 0x0cc0, 0xacd1, 0xecf3, 0x4ce2, 0x6cb7, 0xcca6, 0x8c84, 0x2c95,
 0x294d, 0x895c, 0xc97e, 0x696f, 0x493a, 0xe92b, 0xa909, 0x0918,
 0xe9a3, 0x49b2, 0x0990, 0xa981, 0x89d4, 0x29c5, 0x69e7, 0xc9f6,
 0x0880, 0xa891, 0xe8b3, 0x48a2, 0x68f7, 0xc8e6, 0x88c4, 0x28d5,
 0xc86e, 0x687f, 0x285d, 0x884c, 0xa819, 0x0808, 0x482a, 0xe83b,
 0x6ad7, 0xcac6, 0x8ae4, 0x2af5, 0x0aa0, 0xaab1, 0xea93, 0x4a82,
 0xaa39, 0x0a28, 0x4a0a, 0xea1b, 0xca4e, 0x6a5f, 0x2a7d, 0x8a6c,
 0x4b1a, 0xeb0b, 0xab29, 0x0b38, 0x2b6d, 0x8b7c, 0xcb5e, 0x6b4f,
 0x8bf4, 0x2be5, 0x6bc7, 0xcbd6, 0xeb83, 0x4b92, 0x0bb0, 0xaba1
};

static __inline__ u16 dither_gen (mantissa_t * p_mantissa)
{
        s16 state;

        state = dither_lut[p_mantissa->lfsr_state >> 8] ^ 
                    (p_mantissa->lfsr_state << 8);
        p_mantissa->lfsr_state = (u16) state;
        return ( (state * (s32) (0.707106 * 256.0)) >> 8 );
}


/* Fetch an unpacked, left justified, and properly biased/dithered mantissa value */
static __inline__ float coeff_get_float (ac3dec_t * p_ac3dec, u16 bap, u16 dithflag,
                                   u16 exp)
{
    u16 group_code = 0;

    /* If the bap is 0-5 then we have special cases to take care of */
    switch (bap)
    {
        case 0:
            if (dithflag)
            {
                return ( dither_gen(&p_ac3dec->mantissa) * scale_factor[exp] );
            }    
            return (0);

        case 1:
            if (p_ac3dec->mantissa.q_1_pointer >= 0)
            {
                return (p_ac3dec->mantissa.q_1[p_ac3dec->mantissa.q_1_pointer--] *
                        scale_factor[exp]);
            }

            p_ac3dec->total_bits_read += 5;
            if ((group_code = GetBits (&p_ac3dec->bit_stream,5)) > 26)
            {
                intf_WarnMsg ( 1, "ac3dec error: invalid mantissa (1)" );
                return 0;
            }
    
            p_ac3dec->mantissa.q_1[ 1 ] = q_1_1[ group_code ];
            p_ac3dec->mantissa.q_1[ 0 ] = q_1_2[ group_code ];
    
            p_ac3dec->mantissa.q_1_pointer = 1;
    
            return (q_1_0[group_code] * scale_factor[exp]);
    
        case 2:
            if (p_ac3dec->mantissa.q_2_pointer >= 0)
            {
                return (p_ac3dec->mantissa.q_2[p_ac3dec->mantissa.q_2_pointer--] *
                        scale_factor[exp]);
            }
            
            p_ac3dec->total_bits_read += 7;
            if ((group_code = GetBits (&p_ac3dec->bit_stream,7)) > 124)
            {
                intf_WarnMsg ( 1, "ac3dec error: invalid mantissa (2)" );
                return 0;
            }

            p_ac3dec->mantissa.q_2[ 1 ] = q_2_1[ group_code ];
            p_ac3dec->mantissa.q_2[ 0 ] = q_2_2[ group_code ];

            p_ac3dec->mantissa.q_2_pointer = 1;

            return (q_2_0[group_code] * scale_factor[exp]);

        case 3:
            p_ac3dec->total_bits_read += 3;
            if ((group_code = GetBits (&p_ac3dec->bit_stream,3)) > 6)
            {
                intf_WarnMsg ( 1, "ac3dec error: invalid mantissa (3)" );
                return 0;
            }

            return (q_3[group_code] * scale_factor[exp]);

        case 4:
            if (p_ac3dec->mantissa.q_4_pointer >= 0)
            {
                return (p_ac3dec->mantissa.q_4[p_ac3dec->mantissa.q_4_pointer--] *
                        scale_factor[exp]);
            }

            p_ac3dec->total_bits_read += 7;
            if ((group_code = GetBits (&p_ac3dec->bit_stream,7)) > 120)
            {
                intf_WarnMsg ( 1, "ac3dec error: invalid mantissa (4)" );
                return 0;
            }

            p_ac3dec->mantissa.q_4[ 0 ] = q_4_1[group_code];

            p_ac3dec->mantissa.q_4_pointer = 0;

            return (q_4_0[group_code] * scale_factor[exp]);

        case 5:
            p_ac3dec->total_bits_read += 4;
            if ((group_code = GetBits (&p_ac3dec->bit_stream,4)) > 14)
            {
                intf_WarnMsg ( 1, "ac3dec error: invalid mantissa (5)" );
                return 0;
            }

            return (q_5[group_code] * scale_factor[exp]);

        default:
            group_code = GetBits (&p_ac3dec->bit_stream,qnttztab[bap]);
            group_code <<= 16 - qnttztab[bap];
            p_ac3dec->total_bits_read += qnttztab[bap];

            return ((s16)(group_code) * scale_factor[exp]);
    }
}

/* Uncouple the coupling channel into a fbw channel */
static __inline__ void uncouple_channel (ac3dec_t * p_ac3dec, u32 ch)
{
    u32 bnd = 0;
    u32 sub_bnd = 0;
    u32 i,j;
    float cpl_coord = 1.0;
    u32 cpl_exp_tmp;
    u32 cpl_mant_tmp;

    for (i = p_ac3dec->audblk.cplstrtmant; i < p_ac3dec->audblk.cplendmant;)
    {
        if (!p_ac3dec->audblk.cplbndstrc[sub_bnd++])
        {
            cpl_exp_tmp = p_ac3dec->audblk.cplcoexp[ch][bnd] +
                3 * p_ac3dec->audblk.mstrcplco[ch];
            if (p_ac3dec->audblk.cplcoexp[ch][bnd] == 15)
            {
                cpl_mant_tmp = (p_ac3dec->audblk.cplcomant[ch][bnd]) << 11;
            }
            else
            {
                cpl_mant_tmp = ((0x10) | p_ac3dec->audblk.cplcomant[ch][bnd]) << 10;
            }
            cpl_coord = (cpl_mant_tmp) * scale_factor[cpl_exp_tmp] * 8.0f;

            /* Invert the phase for the right channel if necessary */
            if (p_ac3dec->bsi.acmod == 0x02 && p_ac3dec->audblk.phsflginu &&
                    ch == 1 && p_ac3dec->audblk.phsflg[bnd])
            {
                cpl_coord *= -1;
            }
            bnd++;
        }

        for (j=0;j < 12; j++)
        {
            /* Get new dither values for each channel if necessary,
             * so the channels are uncorrelated */
            if (p_ac3dec->audblk.dithflag[ch] && !p_ac3dec->audblk.cpl_bap[i])
            {
                p_ac3dec->samples[ch][i] = cpl_coord * dither_gen(&p_ac3dec->mantissa) *
                    scale_factor[p_ac3dec->audblk.cpl_exp[i]];
            } else {
                p_ac3dec->samples[ch][i]  = cpl_coord * p_ac3dec->audblk.cpl_flt[i];
            }
            i++;
        }
    }
}

void mantissa_unpack (ac3dec_t * p_ac3dec)
{
    int i, j;
    u32 done_cpl = 0;

    p_ac3dec->mantissa.q_1_pointer = -1;
    p_ac3dec->mantissa.q_2_pointer = -1;
    p_ac3dec->mantissa.q_4_pointer = -1;

    for (i=0; i< p_ac3dec->bsi.nfchans; i++) {
        for (j=0; j < p_ac3dec->audblk.endmant[i]; j++)
            p_ac3dec->samples[i][j] = coeff_get_float(p_ac3dec, p_ac3dec->audblk.fbw_bap[i][j],
                    p_ac3dec->audblk.dithflag[i], p_ac3dec->audblk.fbw_exp[i][j]);

        if (p_ac3dec->audblk.cplinu && p_ac3dec->audblk.chincpl[i] && !(done_cpl)) {
        /* ncplmant is equal to 12 * ncplsubnd
         * Don't dither coupling channel until channel
         * separation so that interchannel noise is uncorrelated 
         */
            for (j=p_ac3dec->audblk.cplstrtmant; j < p_ac3dec->audblk.cplendmant; j++)
                p_ac3dec->audblk.cpl_flt[j] = coeff_get_float(p_ac3dec, p_ac3dec->audblk.cpl_bap[j],
                        0, p_ac3dec->audblk.cpl_exp[j]);
            done_cpl = 1;
        }
    }
    
    /* uncouple the channel if necessary */
    if (p_ac3dec->audblk.cplinu) {
        for (i=0; i< p_ac3dec->bsi.nfchans; i++) {
            if (p_ac3dec->audblk.chincpl[i])
                uncouple_channel(p_ac3dec, i);
        }
    }

    if (p_ac3dec->bsi.lfeon) {
        /* There are always 7 mantissas for lfe, no dither for lfe */
        for (j=0; j < 7 ; j++)
            p_ac3dec->samples[5][j] = coeff_get_float(p_ac3dec, p_ac3dec->audblk.lfe_bap[j],
                    0, p_ac3dec->audblk.lfe_exp[j]);
    }
}

