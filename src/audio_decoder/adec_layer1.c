/*****************************************************************************
 * adec_layer1.c: MPEG Layer I audio decoder
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

#include "int_types.h"

#include "adec_generic.h"
#include "adec_math.h"                                     /* DCT32(), PCM() */
#include "adec_bit_stream.h"

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

static u8 adec_layer1_allocation_table[15] =
{
    0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

static int adec_bound_table[4] = { 4, 8, 12, 16 };

int adec_layer1_mono( audiodec_t * p_adec, s16 * buffer )
{
    u8 allocation[32];
    float slope[32];
    float offset[32];
    float sample[32];

    int sb;
    int s;

    /* parse allocation */

    for (sb = 0; sb < 32; sb += 2)
    {
        u8 tmp;
        tmp = GetByte ( &p_adec->bit_stream );

        if ( (tmp >> 4) > 14 )
        {
            return 1;
        }

        allocation[sb] = adec_layer1_allocation_table [tmp >> 4];

        if ((tmp & 15) > 14)
        {
            return 1;
        }

        allocation[sb+1] = adec_layer1_allocation_table [tmp & 15];
    }

    /* parse scalefactors */

    for ( sb = 0; sb < 32; sb++ )
    {
        if ( allocation[sb] )
        {
            int index;
            float scalefactor;

            NeedBits ( &p_adec->bit_stream, 6 );
            index = p_adec->bit_stream.buffer >> (32 - 6);
            DumpBits ( &p_adec->bit_stream, 6 );

            scalefactor = adec_scalefactor_table[index];

            slope[sb] = adec_slope_table[allocation[sb]-2] * scalefactor;
            offset[sb] = adec_offset_table[allocation[sb]-2] * scalefactor;
        }
    }

    /* parse samples */

    for (s = 0; s < 12; s++)
    {
        s16 * XXX_buf;

        for (sb = 0; sb < 32; sb++)
        {
            if (!allocation[sb])
            {
                sample[sb] = 0;
            }
            else
            {
                int code;

                NeedBits (&p_adec->bit_stream, allocation[sb]);
                code = p_adec->bit_stream.buffer >> (32 - allocation[sb]);
                DumpBits (&p_adec->bit_stream, allocation[sb]);

                sample[sb] = slope[sb] * code + offset[sb];
            }
        }

        DCT32 (sample, &p_adec->bank_0);
        XXX_buf = buffer;
        PCM (&p_adec->bank_0, &XXX_buf, 1);
        buffer += 32;
    }

    return 0;
}

int adec_layer1_stereo (audiodec_t * p_adec, s16 * buffer)
{
    u8 allocation_0[32], allocation_1[32];
    float slope_0[32], slope_1[32];
    float offset_0[32], offset_1[32];
    float sample_0[32], sample_1[32];

    int bound;
    int sb;
    int s;

    /* calculate bound */

    bound = 32;
    if ( (p_adec->header & 0xc0) == 0x40)
    {   /* intensity stereo */
        int index;
        index = (p_adec->header >> 4) & 3;
        bound = adec_bound_table[index];
    }

    /* parse allocation */

    for (sb = 0; sb < bound; sb++)
    {
        u8 tmp;
        tmp = GetByte (&p_adec->bit_stream);
        if ((tmp >> 4) > 14)
        {
            return 1;
        }
        allocation_0[sb] = adec_layer1_allocation_table [tmp >> 4];
        if ((tmp & 15) > 14)
        {
            return 1;
        }
        allocation_1[sb] = adec_layer1_allocation_table [tmp & 15];
    }

    for (; sb < 32; sb += 2)
    {
        u8 tmp;
        tmp = GetByte (&p_adec->bit_stream);
        if ((tmp >> 4) > 14)
        {
            return 1;
        }
        allocation_0[sb] = allocation_1[sb] = adec_layer1_allocation_table [tmp >> 4];
        if ((tmp & 15) > 14)
        {
            return 1;
        }
        allocation_0[sb+1] = allocation_1[sb+1] = adec_layer1_allocation_table [tmp & 15];
    }

    /* parse scalefactors */

    for ( sb = 0; sb < 32; sb++ )
    {
        if ( allocation_0[sb] )
        {
            int index;
            float scalefactor;

            NeedBits (&p_adec->bit_stream, 6);
            index = p_adec->bit_stream.buffer >> (32 - 6);
            DumpBits (&p_adec->bit_stream, 6);

            scalefactor = adec_scalefactor_table[index];

            slope_0[sb] = adec_slope_table[allocation_0[sb]-2] * scalefactor;
            offset_0[sb] = adec_offset_table[allocation_0[sb]-2] * scalefactor;
        }

        if (allocation_1[sb])
        {
            int index;
            float scalefactor;

            NeedBits (&p_adec->bit_stream, 6);
            index = p_adec->bit_stream.buffer >> (32 - 6);
            DumpBits (&p_adec->bit_stream, 6);

            scalefactor = adec_scalefactor_table[index];

            slope_1[sb] = adec_slope_table[allocation_1[sb]-2] * scalefactor;
            offset_1[sb] = adec_offset_table[allocation_1[sb]-2] * scalefactor;
        }
    }

    /* parse samples */

    for (s = 0; s < 12; s++)
    {
        s16 * XXX_buf;

        for (sb = 0; sb < bound; sb++)
        {
            if (!allocation_0[sb])
            {
                sample_0[sb] = 0;
            }
            else
            {
                int code;

                NeedBits (&p_adec->bit_stream, allocation_0[sb]);
                code = p_adec->bit_stream.buffer >> (32 - allocation_0[sb]);
                DumpBits (&p_adec->bit_stream, allocation_0[sb]);

                sample_0[sb] = slope_0[sb] * code + offset_0[sb];
            }

            if ( !allocation_1[sb] )
            {
                sample_1[sb] = 0;
            }
            else
            {
                int code;

                NeedBits (&p_adec->bit_stream, allocation_1[sb]);
                code = p_adec->bit_stream.buffer >> (32 - allocation_1[sb]);
                DumpBits (&p_adec->bit_stream, allocation_1[sb]);

                sample_1[sb] = slope_1[sb] * code + offset_1[sb];
            }
        }

        for (; sb < 32; sb++)
        {
            if (!allocation_0[sb])
            {
                sample_0[sb] = 0;
                sample_1[sb] = 0;
            }
            else
            {
                int code;

                NeedBits (&p_adec->bit_stream, allocation_0[sb]);
                code = p_adec->bit_stream.buffer >> (32 - allocation_0[sb]);
                DumpBits (&p_adec->bit_stream, allocation_0[sb]);

                sample_0[sb] = slope_0[sb] * code + offset_0[sb];
                sample_1[sb] = slope_1[sb] * code + offset_1[sb];
            }
        }

        DCT32 (sample_0, &p_adec->bank_0);
        XXX_buf = buffer;
        PCM (&p_adec->bank_0, &XXX_buf, 2);
        DCT32 (sample_1, &p_adec->bank_1);
        XXX_buf = buffer+1;
        PCM (&p_adec->bank_1, &XXX_buf, 2);
        buffer += 64;
    }

    return 0;
}

