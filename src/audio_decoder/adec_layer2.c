/*****************************************************************************
 * adec_layer2.c: MPEG Layer II audio decoder
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

/*
 * TODO :
 *
 * - optimiser les NeedBits() et les GetBits() du code là où c'est possible ;
 *
 */

#include "defs.h"

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "stream_control.h"
#include "input_ext-dec.h"

#include "adec_generic.h"
#include "audio_decoder.h"
#include "adec_math.h"                                     /* DCT32(), PCM() */

#define NULL ((void *)0)

/**** wkn ****/

static float adec_scalefactor_table[64] =
{   /* 2 ^ (1 - i/3) */
    2.0000000000000000, 1.5874010519681994, 1.2599210498948732,
    1.0000000000000000, 0.7937005259840998, 0.6299605249474366,
    0.5000000000000000, 0.3968502629920499, 0.3149802624737183,
    0.2500000000000000, 0.1984251314960249, 0.1574901312368591,
    0.1250000000000000, 0.0992125657480125, 0.0787450656184296,
    0.0625000000000000, 0.0496062828740062, 0.0393725328092148,
    0.0312500000000000, 0.0248031414370031, 0.0196862664046074,
    0.0156250000000000, 0.0124015707185016, 0.0098431332023037,
    0.0078125000000000, 0.0062007853592508, 0.0049215666011518,
    0.0039062500000000, 0.0031003926796254, 0.0024607833005759,
    0.0019531250000000, 0.0015501963398127, 0.0012303916502880,
    0.0009765625000000, 0.0007750981699063, 0.0006151958251440,
    0.0004882812500000, 0.0003875490849532, 0.0003075979125720,
    0.0002441406250000, 0.0001937745424766, 0.0001537989562860,
    0.0001220703125000, 0.0000968872712383, 0.0000768994781430,
    0.0000610351562500, 0.0000484436356191, 0.0000384497390715,
    0.0000305175781250, 0.0000242218178096, 0.0000192248695357,
    0.0000152587890625, 0.0000121109089048, 0.0000096124347679,
    0.0000076293945312, 0.0000060554544524, 0.0000048062173839,
    0.0000038146972656, 0.0000030277272262, 0.0000024031086920,
    0.0000019073486328, 0.0000015138636131, 0.0000012015543460,
    0.0000009536743164 /* last element is not in the standard... invalid ??? */
};

static float adec_slope_table[15] =
{
    0.6666666666666666, 0.2857142857142857, 0.1333333333333333,
    0.0645161290322581, 0.0317460317460317, 0.0157480314960630,
    0.0078431372549020, 0.0039138943248532, 0.0019550342130987,
    0.0009770395701026, 0.0004884004884005, 0.0002441704309608,
    0.0001220777635354, 0.0000610370189520, 0.0000305180437934
};

static float adec_offset_table[15] =
{
    -0.6666666666666666, -0.8571428571428571, -0.9333333333333333,
    -0.9677419354838710, -0.9841269841269841, -0.9921259842519685,
    -0.9960784313725490, -0.9980430528375733, -0.9990224828934506,
    -0.9995114802149487, -0.9997557997557998, -0.9998779147845196,
    -0.9999389611182323, -0.9999694814905240, -0.9999847409781033
};

static int adec_bound_table[4] = { 4, 8, 12, 16 };

typedef struct
{
    s8 nbal[32];
    u8 * alloc[32];
} alloc_table_t;

#define L3 -1
#define L5 -2
#define L9 -3

static void adec_layer2_get_table( u32 header, u8 freq_table[15],
                                   alloc_table_t ** alloc, int * sblimit )
{
    static s8 table_ab0[16] =
    {
        0, L3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
    };
    static s8 table_ab3[16] =
    {
        0, L3, L5, 3, L9, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16
    };
    static s8 table_ab11[8] =
    {
        0, L3, L5, 3, L9, 4, 5, 16
    };
    static s8 table_ab23[8] =
    {
        0, L3, L5, 16
    };
    static alloc_table_t mpeg1_ab =
    {
        {4,4,4,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,0,0},
        {table_ab0,  table_ab0,  table_ab0,  table_ab3,
         table_ab3,  table_ab3,  table_ab3,  table_ab3,
         table_ab3,  table_ab3,  table_ab3,  table_ab11,
         table_ab11, table_ab11, table_ab11, table_ab11,
         table_ab11, table_ab11, table_ab11, table_ab11,
         table_ab11, table_ab11, table_ab11, table_ab23,
         table_ab23, table_ab23, table_ab23, table_ab23,
         table_ab23, table_ab23, NULL, NULL}
    };

    static s8 table_cd[16] =
    {
        0, L3, L5, L9, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };
    static alloc_table_t mpeg1_cd =
    {
        {4,4,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {table_cd, table_cd, table_cd, table_cd,
         table_cd, table_cd, table_cd, table_cd,
         table_cd, table_cd, table_cd, table_cd,
         NULL, NULL, NULL, NULL,
         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
    };

    static s8 table_0[16] =
    {
        0, L3, L5, 3, L9, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
    };
    static s8 table_4[8] =
    {
        0, L3, L5, L9, 4, 5, 6, 7
    };
    static alloc_table_t mpeg2 =
    {
        {4,4,4,4,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0},
        {table_0, table_0, table_0, table_0,
         table_4, table_4, table_4, table_4,
         table_4, table_4, table_4, table_4,
         table_4, table_4, table_4, table_4,
         table_4, table_4, table_4, table_4,
         table_4, table_4, table_4, table_4,
         table_4, table_4, table_4, table_4,
         table_4, table_4, NULL, NULL}
    };

    static alloc_table_t * alloc_table [4] =
    {
        &mpeg2, &mpeg1_cd, &mpeg1_ab, &mpeg1_ab
    };
    static int sblimit_table[12] =
    {
        30, 8, 27, 30, 30, 8, 27, 27, 30, 12, 27, 30
    };

    int index;

    if (!(header & 0x80000))
    {
        index = 0; /* mpeg2 */
    }
    else
    {
        index = (header >> 12) & 15; /* mpeg1, bitrate */
        index = freq_table [index];
    }

    *alloc = alloc_table[index];
    index |= (header >> 8) & 12;
    *sblimit = sblimit_table[index];
}

int adec_layer2_mono( adec_thread_t * p_adec, s16 * buffer )
{
    static u8 freq_table[15] = {2, 1, 1, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2};
    static float L3_table[3] = {-2/3.0, 0, 2/3.0};
    static float L5_table[5] = {-4/5.0, -2/5.0, 0, 2/5.0, 4/5.0};
    static float L9_table[9] = {-8/9.0, -6/9.0, -4/9.0, -2/9.0, 0,
                                2/9.0, 4/9.0, 6/9.0, 8/9.0};

    s8 allocation[32];
    u8 scfsi[32];
    float slope[3][32];
    float offset[3][32];
    float sample[3][32];
    alloc_table_t * alloc_table;

    int sblimit;
    int sb;
    int gr0, gr1;
    int s;
    int i_read_bits = 0;

    /* get the right allocation table */
    adec_layer2_get_table (p_adec->header, freq_table, &alloc_table, &sblimit);

    /* parse allocation */
    //sblimit=27;

    for (sb = 0; sb < sblimit; sb++)
    {
        int index;

        index = GetBits( &p_adec->bit_stream, alloc_table->nbal[sb] );
        i_read_bits += alloc_table->nbal[sb];

        allocation[sb] = alloc_table->alloc[sb][index];
    }

    /* parse scfsi */

    for (sb = 0; sb < sblimit; sb++)
    {
        if (allocation[sb])
        {
            scfsi[sb] = GetBits (&p_adec->bit_stream, 2);
            i_read_bits += 2;
        }
    }

    /* parse scalefactors */

    for (sb = 0; sb < sblimit; sb++)
    {
        if (allocation[sb])
        {
            int index_0, index_1, index_2;

            switch (scfsi[sb])
            {
            case 0:
                index_0 = GetBits(&p_adec->bit_stream,6);
                index_1 = GetBits(&p_adec->bit_stream,6);
                index_2 = GetBits(&p_adec->bit_stream,6);
                i_read_bits += 18;

                if (allocation[sb] < 0)
                {
                    slope[0][sb] = adec_scalefactor_table[index_0];
                    slope[1][sb] = adec_scalefactor_table[index_1];
                    slope[2][sb] = adec_scalefactor_table[index_2];
                }
                else
                {
                    float r_scalefactor;
                    float r_slope, r_offset;

                    r_slope = adec_slope_table[allocation[sb]-2];
                    r_offset = adec_offset_table[allocation[sb]-2];

                    r_scalefactor = adec_scalefactor_table[index_0];
                    slope[0][sb] = r_slope * r_scalefactor;
                    offset[0][sb] = r_offset * r_scalefactor;

                    r_scalefactor = adec_scalefactor_table[index_1];
                    slope[1][sb] = r_slope * r_scalefactor;
                    offset[1][sb] = r_offset * r_scalefactor;

                    r_scalefactor = adec_scalefactor_table[index_2];
                    slope[2][sb] = r_slope * r_scalefactor;
                    offset[2][sb] = r_offset * r_scalefactor;
                }
                break;

            case 1:
                index_0 = GetBits(&p_adec->bit_stream,6);
                index_1 = GetBits(&p_adec->bit_stream,6);
                i_read_bits += 12;

                if (allocation[sb] < 0)
                {
                    slope[0][sb] = slope[1][sb] =
                        adec_scalefactor_table[index_0];
                    slope[2][sb] = adec_scalefactor_table[index_1];
                }
                else
                {
                    float r_scalefactor;
                    float r_slope, r_offset;

                    r_slope = adec_slope_table[allocation[sb]-2];
                    r_offset = adec_offset_table[allocation[sb]-2];

                    r_scalefactor = adec_scalefactor_table[index_0];
                    slope[0][sb] = slope[1][sb] = r_slope * r_scalefactor;
                    offset[0][sb] = offset[1][sb] =
                        r_offset * r_scalefactor;

                    r_scalefactor = adec_scalefactor_table[index_1];
                    slope[2][sb] = r_slope * r_scalefactor;
                    offset[2][sb] = r_offset * r_scalefactor;
                }
                break;

            case 2:
                index_0 = GetBits( &p_adec->bit_stream, 6 );
                i_read_bits += 6;

                if (allocation[sb] < 0)
                {
                    slope[0][sb] = slope[1][sb] = slope[2][sb] =
                        adec_scalefactor_table[index_0];
                }
                else
                {
                    float r_scalefactor;
                    float r_slope, r_offset;

                    r_slope = adec_slope_table[allocation[sb]-2];
                    r_offset = adec_offset_table[allocation[sb]-2];

                    r_scalefactor = adec_scalefactor_table[index_0];
                    slope[0][sb] = slope[1][sb] = slope[2][sb] =
                        r_slope * r_scalefactor;
                    offset[0][sb] = offset[1][sb] = offset[2][sb] =
                        r_offset * r_scalefactor;
                }
                break;

            case 3:
                index_0 = GetBits(&p_adec->bit_stream,6);
                index_1 = GetBits(&p_adec->bit_stream,6);
                i_read_bits += 12;

                if (allocation[sb] < 0)
                {
                    slope[0][sb] = adec_scalefactor_table[index_0];
                    slope[1][sb] = slope[2][sb] =
                        adec_scalefactor_table[index_1];
                }
                else
                {
                    float r_scalefactor;
                    float r_slope, r_offset;

                    r_slope = adec_slope_table[allocation[sb]-2];
                    r_offset = adec_offset_table[allocation[sb]-2];

                    r_scalefactor = adec_scalefactor_table[index_0];
                    slope[0][sb] = r_slope * r_scalefactor;
                    offset[0][sb] = r_offset * r_scalefactor;

                    r_scalefactor = adec_scalefactor_table[index_1];
                    slope[1][sb] = slope[2][sb] = r_slope * r_scalefactor;
                    offset[1][sb] = offset[2][sb] =
                        r_offset * r_scalefactor;
                }
                break;
            }
        }
    }

    /* parse samples */

    for (gr0 = 0; gr0 < 3; gr0++)
    {
        for (gr1 = 0; gr1 < 4; gr1++)
        {
            s16 * XXX_buf;

            for (sb = 0; sb < sblimit; sb++)
            {
                int code;

                switch (allocation[sb])
                {
                    case 0:
                        sample[0][sb] = sample[1][sb] = sample[2][sb] = 0;
                        break;

                    case L3:
                        code = GetBits( &p_adec->bit_stream, 5 );
                        i_read_bits += 5;

                        sample[0][sb] = slope[gr0][sb] * L3_table[code % 3];
                        code /= 3;
                        sample[1][sb] = slope[gr0][sb] * L3_table[code % 3];
                        code /= 3;
                        sample[2][sb] = slope[gr0][sb] * L3_table[code];
                    break;

                    case L5:
                        code = GetBits( &p_adec->bit_stream, 7 );
                        i_read_bits += 7;

                        sample[0][sb] = slope[gr0][sb] * L5_table[code % 5];
                        code /= 5;
                        sample[1][sb] = slope[gr0][sb] * L5_table[code % 5];
                        code /= 5;
                        sample[2][sb] = slope[gr0][sb] * L5_table[code];
                    break;

                    case L9:
                        code = GetBits( &p_adec->bit_stream, 10 );
                        i_read_bits += 10;

                        sample[0][sb] = slope[gr0][sb] * L9_table[code % 9];
                        code /= 9;
                        sample[1][sb] = slope[gr0][sb] * L9_table[code % 9];
                        code /= 9;
                        sample[2][sb] = slope[gr0][sb] * L9_table[code];
                    break;

                    default:
                        for (s = 0; s < 3; s++)
                        {
                            code = GetBits( &p_adec->bit_stream,
                                            allocation[sb] );
                            i_read_bits += allocation[sb];

                            sample[s][sb] =
                                slope[gr0][sb] * code + offset[gr0][sb];
                        }
                }
            }

            for (; sb < 32; sb++)
            {
                sample[0][sb] = sample[1][sb] = sample[2][sb] = 0;
            }

            for (s = 0; s < 3; s++)
            {
                DCT32 (sample[s], &p_adec->bank_0);
                XXX_buf = buffer;
                PCM (&p_adec->bank_0, &XXX_buf, 2);

                /* FIXME: one shouldn't have to do it twice ! */
                DCT32 (sample[s], &p_adec->bank_1);
                XXX_buf = buffer+1;
                PCM (&p_adec->bank_1, &XXX_buf, 2);

                buffer += 64;
            }
        }
    }

    p_adec->i_read_bits += i_read_bits;

    return 0;
}

int adec_layer2_stereo( adec_thread_t * p_adec, s16 * buffer )
{
    static u8 freq_table[15] = {3, 0, 0, 0, 1, 0, 1, 2, 2, 2, 3, 3, 3, 3, 3};
    static float L3_table[3] = {-2/3.0, 0, 2/3.0};
    static float L5_table[5] = {-4/5.0, -2/5.0, 0, 2/5.0, 4/5.0};
    static float L9_table[9] = {-8/9.0, -6/9.0, -4/9.0, -2/9.0, 0,
                                2/9.0, 4/9.0, 6/9.0, 8/9.0};

    s8 allocation_0[32], allocation_1[32];
    u8 scfsi_0[32], scfsi_1[32];
    float slope_0[3][32], slope_1[3][32];
    float offset_0[3][32], offset_1[3][32];
    float sample_0[3][32], sample_1[3][32];
    alloc_table_t * alloc_table;

    int sblimit;
    int bound;
    int sb;
    int gr0, gr1;
    int s;
    int i_read_bits = 0;

    /* get the right allocation table */
    adec_layer2_get_table (p_adec->header, freq_table, &alloc_table, &sblimit);

    /* calculate bound */
    bound = sblimit;
    if ((p_adec->header & 0xc0) == 0x40) { /* intensity stereo */
        int index;
        index = (p_adec->header >> 4) & 3;
        if (adec_bound_table[index] < sblimit)
        {
            bound = adec_bound_table[index];
        }
    }

    /* parse allocation */

    for (sb = 0; sb < bound; sb++)
    {
        int index;

        index = GetBits( &p_adec->bit_stream, alloc_table->nbal[sb] );
        allocation_0[sb] = alloc_table->alloc[sb][index];

        index = GetBits( &p_adec->bit_stream, alloc_table->nbal[sb] );
        allocation_1[sb] = alloc_table->alloc[sb][index];

        i_read_bits += alloc_table->nbal[sb] * 2;
    }

    for (; sb < sblimit; sb++)
    {
        int index;

        index = GetBits( &p_adec->bit_stream, alloc_table->nbal[sb] );
        allocation_0[sb] = allocation_1[sb] = alloc_table->alloc[sb][index];
        i_read_bits += alloc_table->nbal[sb];
    }

    /* parse scfsi */

    for (sb = 0; sb < sblimit; sb++)
    {
        if (allocation_0[sb])
        {
            scfsi_0[sb] = GetBits (&p_adec->bit_stream, 2);
            i_read_bits += 2;
        }

        if (allocation_1[sb])
        {
            scfsi_1[sb] = GetBits (&p_adec->bit_stream, 2);
            i_read_bits += 2;
        }
    }

    /* parse scalefactors */

    for (sb = 0; sb < sblimit; sb++)
    {
        if (allocation_0[sb])
        {
            int index_0, index_1, index_2;

            switch (scfsi_0[sb])
            {
            case 0:
                index_0 = GetBits(&p_adec->bit_stream,6);
                index_1 = GetBits(&p_adec->bit_stream,6);
                index_2 = GetBits(&p_adec->bit_stream,6);
                i_read_bits += 18;

                if (allocation_0[sb] < 0)
                {
                    slope_0[0][sb] = adec_scalefactor_table[index_0];
                    slope_0[1][sb] = adec_scalefactor_table[index_1];
                    slope_0[2][sb] = adec_scalefactor_table[index_2];
                }
                else
                {
                    float scalefactor;
                    float slope, offset;

                    slope = adec_slope_table[allocation_0[sb]-2];
                    offset = adec_offset_table[allocation_0[sb]-2];

                    scalefactor = adec_scalefactor_table[index_0];
                    slope_0[0][sb] = slope * scalefactor;
                    offset_0[0][sb] = offset * scalefactor;

                    scalefactor = adec_scalefactor_table[index_1];
                    slope_0[1][sb] = slope * scalefactor;
                    offset_0[1][sb] = offset * scalefactor;

                    scalefactor = adec_scalefactor_table[index_2];
                    slope_0[2][sb] = slope * scalefactor;
                    offset_0[2][sb] = offset * scalefactor;
                }
                break;

            case 1:
                index_0 = GetBits(&p_adec->bit_stream,6);
                index_1 = GetBits(&p_adec->bit_stream,6);
                i_read_bits += 12;

                if (allocation_0[sb] < 0)
                {
                    slope_0[0][sb] = slope_0[1][sb] =
                        adec_scalefactor_table[index_0];
                    slope_0[2][sb] = adec_scalefactor_table[index_1];
                }
                else
                {
                    float scalefactor;
                    float slope, offset;

                    slope = adec_slope_table[allocation_0[sb]-2];
                    offset = adec_offset_table[allocation_0[sb]-2];

                    scalefactor = adec_scalefactor_table[index_0];
                    slope_0[0][sb] = slope_0[1][sb] = slope * scalefactor;
                    offset_0[0][sb] = offset_0[1][sb] =
                            offset * scalefactor;

                    scalefactor = adec_scalefactor_table[index_1];
                    slope_0[2][sb] = slope * scalefactor;
                    offset_0[2][sb] = offset * scalefactor;
                }
                break;

            case 2:
                index_0 = GetBits( &p_adec->bit_stream, 6 );
                i_read_bits += 6;

                if (allocation_0[sb] < 0)
                {
                    slope_0[0][sb] = slope_0[1][sb] = slope_0[2][sb] =
                        adec_scalefactor_table[index_0];
                }
                else
                {
                    float scalefactor;
                    float slope, offset;

                    slope = adec_slope_table[allocation_0[sb]-2];
                    offset = adec_offset_table[allocation_0[sb]-2];

                    scalefactor = adec_scalefactor_table[index_0];
                    slope_0[0][sb] = slope_0[1][sb] = slope_0[2][sb] =
                        slope * scalefactor;
                    offset_0[0][sb] = offset_0[1][sb] = offset_0[2][sb] =
                        offset * scalefactor;
                }
                break;

            case 3:
                index_0 = GetBits(&p_adec->bit_stream,6);
                index_1 = GetBits(&p_adec->bit_stream,6);
                i_read_bits += 12;

                if (allocation_0[sb] < 0)
                {
                    slope_0[0][sb] = adec_scalefactor_table[index_0];
                    slope_0[1][sb] = slope_0[2][sb] =
                        adec_scalefactor_table[index_1];
                }
                else
                {
                    float scalefactor;
                    float slope, offset;

                    slope = adec_slope_table[allocation_0[sb]-2];
                    offset = adec_offset_table[allocation_0[sb]-2];

                    scalefactor = adec_scalefactor_table[index_0];
                    slope_0[0][sb] = slope * scalefactor;
                    offset_0[0][sb] = offset * scalefactor;

                    scalefactor = adec_scalefactor_table[index_1];
                    slope_0[1][sb] = slope_0[2][sb] = slope * scalefactor;
                    offset_0[1][sb] = offset_0[2][sb] =
                        offset * scalefactor;
                }
                break;
            }
        }

        if (allocation_1[sb])
        {
            int index_0, index_1, index_2;

            switch (scfsi_1[sb])
            {
            case 0:
                index_0 = GetBits(&p_adec->bit_stream,6);
                index_1 = GetBits(&p_adec->bit_stream,6);
                index_2 = GetBits(&p_adec->bit_stream,6);
                i_read_bits += 18;

                if (allocation_1[sb] < 0)
                {
                    slope_1[0][sb] = adec_scalefactor_table[index_0];
                    slope_1[1][sb] = adec_scalefactor_table[index_1];
                    slope_1[2][sb] = adec_scalefactor_table[index_2];
                }
                else
                {
                    float scalefactor;
                    float slope, offset;

                    slope = adec_slope_table[allocation_1[sb]-2];
                    offset = adec_offset_table[allocation_1[sb]-2];

                    scalefactor = adec_scalefactor_table[index_0];
                    slope_1[0][sb] = slope * scalefactor;
                    offset_1[0][sb] = offset * scalefactor;

                    scalefactor = adec_scalefactor_table[index_1];
                    slope_1[1][sb] = slope * scalefactor;
                    offset_1[1][sb] = offset * scalefactor;

                    scalefactor = adec_scalefactor_table[index_2];
                    slope_1[2][sb] = slope * scalefactor;
                    offset_1[2][sb] = offset * scalefactor;
                }
                break;

            case 1:
                index_0 = GetBits(&p_adec->bit_stream,6);
                index_1 = GetBits(&p_adec->bit_stream,6);
                i_read_bits += 12;

                if (allocation_1[sb] < 0)
                {
                    slope_1[0][sb] = slope_1[1][sb] =
                        adec_scalefactor_table[index_0];
                    slope_1[2][sb] = adec_scalefactor_table[index_1];
                }
                else
                {
                    float scalefactor;
                    float slope, offset;

                    slope = adec_slope_table[allocation_1[sb]-2];
                    offset = adec_offset_table[allocation_1[sb]-2];

                    scalefactor = adec_scalefactor_table[index_0];
                    slope_1[0][sb] = slope_1[1][sb] = slope * scalefactor;
                    offset_1[0][sb] = offset_1[1][sb] =
                        offset * scalefactor;

                    scalefactor = adec_scalefactor_table[index_1];
                    slope_1[2][sb] = slope * scalefactor;
                    offset_1[2][sb] = offset * scalefactor;
                }
                break;

            case 2:
                index_0 = GetBits( &p_adec->bit_stream, 6 );
                i_read_bits += 6;

                if (allocation_1[sb] < 0)
                {
                    slope_1[0][sb] = slope_1[1][sb] = slope_1[2][sb] =
                        adec_scalefactor_table[index_0];
                }
                else
                {
                    float scalefactor;
                    float slope, offset;

                    slope = adec_slope_table[allocation_1[sb]-2];
                    offset = adec_offset_table[allocation_1[sb]-2];

                    scalefactor = adec_scalefactor_table[index_0];
                    slope_1[0][sb] = slope_1[1][sb] = slope_1[2][sb] =
                        slope * scalefactor;
                    offset_1[0][sb] = offset_1[1][sb] = offset_1[2][sb] =
                        offset * scalefactor;
                }
                break;

            case 3:
                index_0 = GetBits(&p_adec->bit_stream,6);
                index_1 = GetBits(&p_adec->bit_stream,6);
                i_read_bits += 12;

                if (allocation_1[sb] < 0)
                {
                    slope_1[0][sb] = adec_scalefactor_table[index_0];
                    slope_1[1][sb] = slope_1[2][sb] =
                        adec_scalefactor_table[index_1];
                }
                else
                {
                    float scalefactor;
                    float slope, offset;

                    slope = adec_slope_table[allocation_1[sb]-2];
                    offset = adec_offset_table[allocation_1[sb]-2];

                    scalefactor = adec_scalefactor_table[index_0];
                    slope_1[0][sb] = slope * scalefactor;
                    offset_1[0][sb] = offset * scalefactor;

                    scalefactor = adec_scalefactor_table[index_1];
                    slope_1[1][sb] = slope_1[2][sb] = slope * scalefactor;
                    offset_1[1][sb] = offset_1[2][sb] =
                        offset * scalefactor;
                }
                break;
            }
        }
    }

    /* parse samples */

    for (gr0 = 0; gr0 < 3; gr0++)
    {
        for (gr1 = 0; gr1 < 4; gr1++)
        {
            s16 * XXX_buf;

            for (sb = 0; sb < bound; sb++)
            {
                int code;

                switch (allocation_0[sb])
                {
                    case 0:
                        sample_0[0][sb] = sample_0[1][sb] = sample_0[2][sb] = 0;
                        break;

                    case L3:
                        code = GetBits( &p_adec->bit_stream, 5 );
                        i_read_bits += 5;

                        sample_0[0][sb] = slope_0[gr0][sb] * L3_table[code % 3];
                        code /= 3;
                        sample_0[1][sb] = slope_0[gr0][sb] * L3_table[code % 3];
                        code /= 3;
                        sample_0[2][sb] = slope_0[gr0][sb] * L3_table[code];
                    break;

                    case L5:
                        code = GetBits( &p_adec->bit_stream, 7 );
                        i_read_bits += 7;

                        sample_0[0][sb] = slope_0[gr0][sb] * L5_table[code % 5];
                        code /= 5;
                        sample_0[1][sb] = slope_0[gr0][sb] * L5_table[code % 5];
                        code /= 5;
                        sample_0[2][sb] = slope_0[gr0][sb] * L5_table[code];
                    break;

                    case L9:
                        code = GetBits( &p_adec->bit_stream, 10 );
                        i_read_bits += 10;

                        sample_0[0][sb] = slope_0[gr0][sb] * L9_table[code % 9];
                        code /= 9;
                        sample_0[1][sb] = slope_0[gr0][sb] * L9_table[code % 9];
                        code /= 9;
                        sample_0[2][sb] = slope_0[gr0][sb] * L9_table[code];
                    break;

                    default:
                        for (s = 0; s < 3; s++)
                        {
                            code = GetBits( &p_adec->bit_stream,
                                            allocation_0[sb] );
                            i_read_bits += allocation_0[sb];

                            sample_0[s][sb] =
                                slope_0[gr0][sb] * code + offset_0[gr0][sb];
                        }
                }

                switch (allocation_1[sb])
                {
                    case 0:
                        sample_1[0][sb] = sample_1[1][sb] = sample_1[2][sb] = 0;
                    break;

                    case L3:
                        code = GetBits( &p_adec->bit_stream, 5 );
                        i_read_bits += 5;

                        sample_1[0][sb] = slope_1[gr0][sb] * L3_table[code % 3];
                        code /= 3;
                        sample_1[1][sb] = slope_1[gr0][sb] * L3_table[code % 3];
                        code /= 3;
                        sample_1[2][sb] = slope_1[gr0][sb] * L3_table[code];
                    break;

                    case L5:
                        code = GetBits( &p_adec->bit_stream, 7 );
                        i_read_bits += 7;

                        sample_1[0][sb] = slope_1[gr0][sb] * L5_table[code % 5];
                        code /= 5;
                        sample_1[1][sb] = slope_1[gr0][sb] * L5_table[code % 5];
                        code /= 5;
                        sample_1[2][sb] = slope_1[gr0][sb] * L5_table[code];
                    break;

                    case L9:
                        code = GetBits( &p_adec->bit_stream, 10 );
                        i_read_bits += 10;

                        sample_1[0][sb] = slope_1[gr0][sb] * L9_table[code % 9];
                        code /= 9;
                        sample_1[1][sb] = slope_1[gr0][sb] * L9_table[code % 9];
                        code /= 9;
                        sample_1[2][sb] = slope_1[gr0][sb] * L9_table[code];
                    break;

                    default:
                        for (s = 0; s < 3; s++)
                        {
                            code = GetBits( &p_adec->bit_stream,
                                            allocation_1[sb] );
                            i_read_bits += allocation_1[sb];

                            sample_1[s][sb] =
                                slope_1[gr0][sb] * code + offset_1[gr0][sb];
                        }
                }
            }

            for (; sb < sblimit; sb++)
            {
                int code;

                switch (allocation_0[sb])
                {
                    case 0:
                        sample_0[0][sb] = sample_0[1][sb] = sample_0[2][sb] = 0;
                        sample_1[0][sb] = sample_1[1][sb] = sample_1[2][sb] = 0;
                    break;

                    case L3:
                        code = GetBits( &p_adec->bit_stream, 5 );
                        i_read_bits += 5;

                        sample_0[0][sb] = slope_0[gr0][sb] * L3_table[code % 3];
                        sample_1[0][sb] = slope_1[gr0][sb] * L3_table[code % 3];
                        code /= 3;
                        sample_0[1][sb] = slope_0[gr0][sb] * L3_table[code % 3];
                        sample_1[1][sb] = slope_1[gr0][sb] * L3_table[code % 3];
                        code /= 3;
                        sample_0[2][sb] = slope_0[gr0][sb] * L3_table[code];
                        sample_1[2][sb] = slope_1[gr0][sb] * L3_table[code];
                    break;

                    case L5:
                        code = GetBits( &p_adec->bit_stream, 7 );
                        i_read_bits += 7;

                        sample_0[0][sb] = slope_0[gr0][sb] * L5_table[code % 5];
                        sample_1[0][sb] = slope_1[gr0][sb] * L5_table[code % 5];
                        code /= 5;
                        sample_0[1][sb] = slope_0[gr0][sb] * L5_table[code % 5];
                        sample_1[1][sb] = slope_1[gr0][sb] * L5_table[code % 5];
                        code /= 5;
                        sample_0[2][sb] = slope_0[gr0][sb] * L5_table[code];
                        sample_1[2][sb] = slope_1[gr0][sb] * L5_table[code];
                    break;

                    case L9:
                        code = GetBits( &p_adec->bit_stream, 10 );
                        i_read_bits += 10;

                        sample_0[0][sb] = slope_0[gr0][sb] * L9_table[code % 9];
                        sample_1[0][sb] = slope_1[gr0][sb] * L9_table[code % 9];
                        code /= 9;
                        sample_0[1][sb] = slope_0[gr0][sb] * L9_table[code % 9];
                        sample_1[1][sb] = slope_1[gr0][sb] * L9_table[code % 9];
                        code /= 9;
                        sample_0[2][sb] = slope_0[gr0][sb] * L9_table[code];
                        sample_1[2][sb] = slope_1[gr0][sb] * L9_table[code];
                    break;

                    default:
                        for (s = 0; s < 3; s++)
                        {
                            code = GetBits( &p_adec->bit_stream,
                                            allocation_0[sb] );
                            i_read_bits += allocation_0[sb];

                            sample_0[s][sb] =
                                slope_0[gr0][sb] * code + offset_0[gr0][sb];
                            sample_1[s][sb] =
                                slope_1[gr0][sb] * code + offset_1[gr0][sb];
                        }
                }
            }

            for (; sb < 32; sb++)
            {
                sample_0[0][sb] = sample_0[1][sb] = sample_0[2][sb] = 0;
                sample_1[0][sb] = sample_1[1][sb] = sample_1[2][sb] = 0;
            }

            for (s = 0; s < 3; s++)
            {
                DCT32 (sample_0[s], &p_adec->bank_0);
                XXX_buf = buffer;
                PCM (&p_adec->bank_0, &XXX_buf, 2);

                DCT32 (sample_1[s], &p_adec->bank_1);
                XXX_buf = buffer+1;
                PCM (&p_adec->bank_1, &XXX_buf, 2);

                buffer += 64;
            }
        }
    }

    p_adec->i_read_bits += i_read_bits;

    return 0;
}

