/*****************************************************************************
 * audio_decoder.c: MPEG1 Layer I-II audio decoder
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

/*
 * TODO :
 *
 * - optimiser les NeedBits() et les GetBits() du code là où c'est possible ;
 *
 */

#include "int_types.h"
#include "audio_constants.h"
#include "audio_decoder.h"
#include "audio_math.h"                                    /* DCT32(), PCM() */
#include "audio_bit_stream.h"

#define NULL ((void *)0)

/*****************************************************************************
 * adec_Layer`L'_`M': decodes an mpeg 1, layer `L', mode `M', audio frame
 *****************************************************************************
 * These functions decode the audio frame which has already its header loaded
 * in the i_header member of the audio decoder thread structure and its first
 * byte of data described by the bit stream structure of the audio decoder
 * thread (there is no bit available in the bit buffer yet)
 *****************************************************************************/

/*****************************************************************************
 * adec_Layer1_Mono
 *****************************************************************************/
static __inline__ int adec_Layer1_Mono (audiodec_t * p_adec)
{
    p_adec->bit_stream.buffer = 0;
    p_adec->bit_stream.i_available = 0;
    return (0);
}

/*****************************************************************************
 * adec_Layer1_Stereo
 *****************************************************************************/
static __inline__ int adec_Layer1_Stereo (audiodec_t * p_adec)
{
    p_adec->bit_stream.buffer = 0;
    p_adec->bit_stream.i_available = 0;
    return (0);
}

/*****************************************************************************
 * adec_Layer2_Mono
 *****************************************************************************/
static __inline__ int adec_Layer2_Mono (audiodec_t * p_adec)
{
    p_adec->bit_stream.buffer = 0;
    p_adec->bit_stream.i_available = 0;
    return (0);
}

/*****************************************************************************
 * adec_Layer2_Stereo
 *****************************************************************************/
static __inline__ int adec_Layer2_Stereo (audiodec_t * p_adec, s16 * buffer)
{
    typedef struct requantization_s
    {
        u8                              i_bits_per_codeword;
        const float *                   pf_ungroup;
        float                           f_slope;
        float                           f_offset;
    } requantization_t;

    static const float                  pf_scalefactor[64] = ADEC_SCALE_FACTOR;

    static u32                          i_header;
    static int                          i_sampling_frequency, i_mode, i_bound;
    static int                          pi_allocation_0[32], pi_allocation_1[32]; /* see ISO/IEC 11172-3 2.4.1.6 */
    int                                 i_sb, i_nbal;
    float                               f_scalefactor_0, f_scalefactor_1;

    static const u8                     ppi_bitrate_per_channel_index[4][15] = ADEC_LAYER2_BITRATE_PER_CHANNEL_INDEX;
    static const u8                     ppi_sblimit[3][11] = ADEC_LAYER2_SBLIMIT;
    static const u8                     ppi_nbal[2][32] = ADEC_LAYER2_NBAL;

    static const float                  pf_ungroup3[3*3*3 * 3] = ADEC_LAYER2_UNGROUP3;
    static const float                  pf_ungroup5[5*5*5 * 3] = ADEC_LAYER2_UNGROUP5;
    static const float                  pf_ungroup9[9*9*9 * 3] = ADEC_LAYER2_UNGROUP9;

    static const requantization_t       p_requantization_cd[16] = ADEC_LAYER2_REQUANTIZATION_CD;
    static const requantization_t       p_requantization_ab1[16] = ADEC_LAYER2_REQUANTIZATION_AB1;
    static const requantization_t       p_requantization_ab2[16] = ADEC_LAYER2_REQUANTIZATION_AB2;
    static const requantization_t       p_requantization_ab3[16] = ADEC_LAYER2_REQUANTIZATION_AB3;
    static const requantization_t       p_requantization_ab4[16] = ADEC_LAYER2_REQUANTIZATION_AB4;
    static const requantization_t *     pp_requantization_ab[30] = ADEC_LAYER2_REQUANTIZATION_AB;

    static int                          i_sblimit, i_bitrate_per_channel_index;
    static int                          pi_scfsi_0[30], pi_scfsi_1[30];
    static const u8 *                   pi_nbal;
    static float                        ppf_sample_0[3][32], ppf_sample_1[3][32];
    static const requantization_t *     pp_requantization_0[30];
    static const requantization_t *     pp_requantization_1[30];
    static requantization_t             requantization;
    static const float *                pf_ungroup;

    static float                        pf_scalefactor_0_0[30], pf_scalefactor_0_1[30], pf_scalefactor_0_2[30];
    static float                        pf_scalefactor_1_0[30], pf_scalefactor_1_1[30], pf_scalefactor_1_2[30];

    int                                 i_2nbal, i_gr;
    float                               f_dummy;

    s16 *                               p_s16;

    int                                 i_need = 0, i_dump = 0;
#if 0
    static const int                    pi_framesize[512] = ADEC_FRAME_SIZE;
#endif

    /* Read the audio frame header and flush the bit buffer */
    i_header = p_adec->header;
    /* Read the sampling frequency (see ISO/IEC 11172-3 2.4.2.3) */
    i_sampling_frequency = (int)((i_header & ADEC_HEADER_SAMPLING_FREQUENCY_MASK)
        >> ADEC_HEADER_SAMPLING_FREQUENCY_SHIFT);
    /* Read the mode (see ISO/IEC 11172-3 2.4.2.3) */
    i_mode = (int)((i_header & ADEC_HEADER_MODE_MASK) >> ADEC_HEADER_MODE_SHIFT);
    /* If a CRC can be found in the frame, get rid of it */
    if ((i_header & ADEC_HEADER_PROTECTION_BIT_MASK) == 0)
    {
        NeedBits (&p_adec->bit_stream, 16);
        DumpBits (&p_adec->bit_stream, 16);
    }

    /* Find out the bitrate per channel index */
    i_bitrate_per_channel_index = (int)ppi_bitrate_per_channel_index[i_mode]
        [(i_header & ADEC_HEADER_BITRATE_INDEX_MASK) >> ADEC_HEADER_BITRATE_INDEX_SHIFT];
    /* Find out the number of subbands */
    i_sblimit = (int)ppi_sblimit[i_sampling_frequency][i_bitrate_per_channel_index];
    /* Check if the frame is valid or not */
    if (i_sblimit == 0)
    {
        return (0);                                 /* the frame is invalid */
    }
    /* Find out the number of bits allocated */
    pi_nbal = ppi_nbal[ (i_bitrate_per_channel_index <= 2) ? 0 : 1 ];

    /* Find out the `bound' subband (see ISO/IEC 11172-3 2.4.2.3) */
    if (i_mode == 1)
    {
        i_bound = (int)(((i_header & ADEC_HEADER_MODE_EXTENSION_MASK) >> (ADEC_HEADER_MODE_EXTENSION_SHIFT - 2)) + 4);
        if (i_bound > i_sblimit)
        {
            i_bound = i_sblimit;
        }
    }
    else
    {
        i_bound = i_sblimit;
    }

    /* Read the allocation information (see ISO/IEC 11172-3 2.4.1.6) */
    for (i_sb = 0; i_sb < i_bound; i_sb++)
    {
        i_2nbal = 2 * (i_nbal = (int)pi_nbal[ i_sb ]);
        NeedBits (&p_adec->bit_stream, i_2nbal);
        i_need += i_2nbal;
        pi_allocation_0[ i_sb ] = (int)(p_adec->bit_stream.buffer >> (32 - i_nbal));
        p_adec->bit_stream.buffer <<= i_nbal;
        pi_allocation_1[ i_sb ] = (int)(p_adec->bit_stream.buffer >> (32 - i_nbal));
        p_adec->bit_stream.buffer <<= i_nbal;
        p_adec->bit_stream.i_available -= i_2nbal;
        i_dump += i_2nbal;
    }
    for (; i_sb < i_sblimit; i_sb++)
    {
        i_nbal = (int)pi_nbal[ i_sb ];
        NeedBits (&p_adec->bit_stream, i_nbal);
        i_need += i_nbal;
        pi_allocation_0[ i_sb ] = (int)(p_adec->bit_stream.buffer >> (32 - i_nbal));
        DumpBits (&p_adec->bit_stream, i_nbal);
        i_dump += i_nbal;
    }

#define MACRO(p_requantization) \
    for (i_sb = 0; i_sb < i_bound; i_sb++) \
    { \
        if (pi_allocation_0[i_sb]) \
        { \
            pp_requantization_0[i_sb] = &((p_requantization)[pi_allocation_0[i_sb]]); \
            NeedBits (&p_adec->bit_stream, 2); \
            i_need += 2; \
            pi_scfsi_0[i_sb] = (int)(p_adec->bit_stream.buffer >> (32 - 2)); \
            DumpBits (&p_adec->bit_stream, 2); \
            i_dump += 2; \
        } \
        else \
        { \
            ppf_sample_0[0][i_sb] = .0; \
            ppf_sample_0[1][i_sb] = .0; \
            ppf_sample_0[2][i_sb] = .0; \
        } \
\
        if (pi_allocation_1[i_sb]) \
        { \
            pp_requantization_1[i_sb] = &((p_requantization)[pi_allocation_1[i_sb]]); \
            NeedBits (&p_adec->bit_stream, 2); \
            i_need += 2; \
            pi_scfsi_1[i_sb] = (int)(p_adec->bit_stream.buffer >> (32 - 2)); \
            DumpBits (&p_adec->bit_stream, 2); \
            i_dump += 2; \
        } \
        else \
        { \
            ppf_sample_1[0][i_sb] = .0; \
            ppf_sample_1[1][i_sb] = .0; \
            ppf_sample_1[2][i_sb] = .0; \
        } \
    } \
\
    for (; i_sb < i_sblimit; i_sb++) \
    { \
        if (pi_allocation_0[i_sb]) \
        { \
            pp_requantization_0[i_sb] = &((p_requantization)[pi_allocation_0[i_sb]]); \
            NeedBits (&p_adec->bit_stream, 4); \
            i_need += 4; \
            pi_scfsi_0[i_sb] = (int)(p_adec->bit_stream.buffer >> (32 - 2)); \
            p_adec->bit_stream.buffer <<= 2; \
            pi_scfsi_1[i_sb] = (int)(p_adec->bit_stream.buffer >> (32 - 2)); \
            p_adec->bit_stream.buffer <<= 2; \
            p_adec->bit_stream.i_available -= 4; \
            i_dump += 4; \
        } \
        else \
        { \
            ppf_sample_0[0][i_sb] = .0; \
            ppf_sample_0[1][i_sb] = .0; \
            ppf_sample_0[2][i_sb] = .0; \
            ppf_sample_1[0][i_sb] = .0; \
            ppf_sample_1[1][i_sb] = .0; \
            ppf_sample_1[2][i_sb] = .0; \
        } \
    }
/* #define MACRO */

    if (i_bitrate_per_channel_index <= 2)
    {
        MACRO (p_requantization_cd)
    }
    else
    {
        MACRO (pp_requantization_ab[i_sb])
    }

#define SWITCH(pi_scfsi,pf_scalefactor_0,pf_scalefactor_1,pf_scalefactor_2) \
    switch ((pi_scfsi)[i_sb]) \
    { \
        case 0: \
            NeedBits (&p_adec->bit_stream, (3*6)); \
            i_need += 18; \
           (pf_scalefactor_0)[i_sb] = pf_scalefactor[p_adec->bit_stream.buffer >> (32 - 6)]; \
            p_adec->bit_stream.buffer <<= 6; \
           (pf_scalefactor_1)[i_sb] = pf_scalefactor[p_adec->bit_stream.buffer >> (32 - 6)]; \
            p_adec->bit_stream.buffer <<= 6; \
           (pf_scalefactor_2)[i_sb] = pf_scalefactor[p_adec->bit_stream.buffer >> (32 - 6)]; \
            p_adec->bit_stream.buffer <<= 6; \
            p_adec->bit_stream.i_available -= (3*6); \
            i_dump += 18; \
            break; \
\
        case 1: \
            NeedBits (&p_adec->bit_stream, (2*6)); \
            i_need += 12; \
           (pf_scalefactor_0)[i_sb] = \
               (pf_scalefactor_1)[i_sb] = pf_scalefactor[p_adec->bit_stream.buffer >> (32 - 6)]; \
            p_adec->bit_stream.buffer <<= 6; \
           (pf_scalefactor_2)[i_sb] = pf_scalefactor[p_adec->bit_stream.buffer >> (32 - 6)]; \
            p_adec->bit_stream.buffer <<= 6; \
            p_adec->bit_stream.i_available -= (2*6); \
            i_dump += 12; \
            break; \
\
        case 2: \
            NeedBits (&p_adec->bit_stream, (1*6)); \
            i_need += 6; \
           (pf_scalefactor_0)[i_sb] = \
               (pf_scalefactor_1)[i_sb] = \
               (pf_scalefactor_2)[i_sb] = pf_scalefactor[p_adec->bit_stream.buffer >> (32 - 6)]; \
            DumpBits (&p_adec->bit_stream, (1*6)); \
            i_dump += 6; \
            break; \
\
        case 3: \
            NeedBits (&p_adec->bit_stream, (2*6)); \
            i_need += 12; \
           (pf_scalefactor_0)[i_sb] = pf_scalefactor[p_adec->bit_stream.buffer >> (32 - 6)]; \
            p_adec->bit_stream.buffer <<= 6; \
           (pf_scalefactor_1)[i_sb] = \
               (pf_scalefactor_2)[i_sb] = pf_scalefactor[p_adec->bit_stream.buffer >> (32 - 6)]; \
            p_adec->bit_stream.buffer <<= 6; \
            p_adec->bit_stream.i_available -= (2*6); \
            i_dump += 12; \
            break; \
    }
/* #define SWITCH */

    for (i_sb = 0; i_sb < i_bound; i_sb++)
    {
        if (pi_allocation_0[i_sb])
        {
            SWITCH (pi_scfsi_0, pf_scalefactor_0_0, pf_scalefactor_0_1, pf_scalefactor_0_2)
        }
        if (pi_allocation_1[i_sb])
        {
            SWITCH (pi_scfsi_1, pf_scalefactor_1_0, pf_scalefactor_1_1, pf_scalefactor_1_2)
        }
    }
    for (; i_sb < i_sblimit; i_sb++)
    {
        if (pi_allocation_0[i_sb])
        {
            SWITCH (pi_scfsi_0, pf_scalefactor_0_0, pf_scalefactor_0_1, pf_scalefactor_0_2)
            SWITCH (pi_scfsi_1, pf_scalefactor_1_0, pf_scalefactor_1_1, pf_scalefactor_1_2)
        }
    }
    for (; i_sb < 32; i_sb++)
    {
        ppf_sample_0[0][i_sb] = .0;
        ppf_sample_0[1][i_sb] = .0;
        ppf_sample_0[2][i_sb] = .0;
        ppf_sample_1[0][i_sb] = .0;
        ppf_sample_1[1][i_sb] = .0;
        ppf_sample_1[2][i_sb] = .0;
    }

#define GROUPTEST(pp_requantization,ppf_sample,pf_sf) \
    requantization = *((pp_requantization)[i_sb]); \
    if (requantization.pf_ungroup == NULL) \
    { \
        NeedBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
        i_need += requantization.i_bits_per_codeword; \
       (ppf_sample)[0][i_sb] = (f_scalefactor_0 = (pf_sf)[i_sb]) * (requantization.f_slope * \
           (p_adec->bit_stream.buffer >> (32 - requantization.i_bits_per_codeword)) + requantization.f_offset); \
        DumpBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
        i_dump += requantization.i_bits_per_codeword; \
\
        NeedBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
        i_need += requantization.i_bits_per_codeword; \
       (ppf_sample)[1][i_sb] = f_scalefactor_0 * (requantization.f_slope * \
           (p_adec->bit_stream.buffer >> (32 - requantization.i_bits_per_codeword)) + requantization.f_offset); \
        DumpBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
        i_dump += requantization.i_bits_per_codeword; \
\
        NeedBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
        i_need += requantization.i_bits_per_codeword; \
       (ppf_sample)[2][i_sb] = f_scalefactor_0 * (requantization.f_slope * \
           (p_adec->bit_stream.buffer >> (32 - requantization.i_bits_per_codeword)) + requantization.f_offset); \
        DumpBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
        i_dump += requantization.i_bits_per_codeword; \
    } \
    else \
    { \
        NeedBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
        i_need += requantization.i_bits_per_codeword; \
        pf_ungroup = requantization.pf_ungroup + 3 * \
           (p_adec->bit_stream.buffer >> (32 - requantization.i_bits_per_codeword)); \
        DumpBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
        i_dump += requantization.i_bits_per_codeword; \
       (ppf_sample)[0][i_sb] = (f_scalefactor_0 = (pf_sf)[i_sb]) * pf_ungroup[0]; \
       (ppf_sample)[1][i_sb] = f_scalefactor_0 * pf_ungroup[1]; \
       (ppf_sample)[2][i_sb] = f_scalefactor_0 * pf_ungroup[2]; \
    }
/* #define GROUPTEST */

#define READ_SAMPLE_L2S(pf_scalefactor_0,pf_scalefactor_1,i_grlimit) \
    for (; i_gr < (i_grlimit); i_gr++) \
    { \
        for (i_sb = 0; i_sb < i_bound; i_sb++) \
        { \
            if (pi_allocation_0[i_sb]) \
            { \
                GROUPTEST (pp_requantization_0, ppf_sample_0, (pf_scalefactor_0)) \
            } \
            if (pi_allocation_1[i_sb]) \
            { \
                GROUPTEST (pp_requantization_1, ppf_sample_1, (pf_scalefactor_1)) \
            } \
        } \
        for (; i_sb < i_sblimit; i_sb++) \
        { \
            if (pi_allocation_0[i_sb]) \
            { \
                requantization = *(pp_requantization_0[i_sb]); \
                if (requantization.pf_ungroup == NULL) \
                { \
                    NeedBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
                    i_need += requantization.i_bits_per_codeword; \
                    ppf_sample_0[0][i_sb] = (f_scalefactor_0 = (pf_scalefactor_0)[i_sb]) * \
                       (requantization.f_slope * (f_dummy = \
                       (float)(p_adec->bit_stream.buffer >> (32 - requantization.i_bits_per_codeword))) + \
                        requantization.f_offset); \
                    DumpBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
                    i_dump += requantization.i_bits_per_codeword; \
                    ppf_sample_1[0][i_sb] = (f_scalefactor_1 = (pf_scalefactor_1)[i_sb]) * \
                       (requantization.f_slope * f_dummy + requantization.f_offset); \
\
                    NeedBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
                    i_need += requantization.i_bits_per_codeword; \
                    ppf_sample_0[1][i_sb] = f_scalefactor_0 * \
                       (requantization.f_slope * (f_dummy = \
                       (float)(p_adec->bit_stream.buffer >> (32 - requantization.i_bits_per_codeword))) + \
                        requantization.f_offset); \
                    DumpBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
                    i_dump += requantization.i_bits_per_codeword; \
                    ppf_sample_1[1][i_sb] = f_scalefactor_1 * \
                       (requantization.f_slope * f_dummy + requantization.f_offset); \
\
                    NeedBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
                    i_need += requantization.i_bits_per_codeword; \
                    ppf_sample_0[2][i_sb] = f_scalefactor_0 * \
                       (requantization.f_slope * (f_dummy = \
                       (float)(p_adec->bit_stream.buffer >> (32 - requantization.i_bits_per_codeword))) + \
                        requantization.f_offset); \
                    DumpBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
                    i_dump += requantization.i_bits_per_codeword; \
                    ppf_sample_1[2][i_sb] = f_scalefactor_1 * \
                       (requantization.f_slope * f_dummy + requantization.f_offset); \
                } \
                else \
                { \
                    NeedBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
                    i_need += requantization.i_bits_per_codeword; \
                    pf_ungroup = requantization.pf_ungroup + 3 * \
                       (p_adec->bit_stream.buffer >> (32 - requantization.i_bits_per_codeword)); \
                    DumpBits (&p_adec->bit_stream, requantization.i_bits_per_codeword); \
                    i_dump += requantization.i_bits_per_codeword; \
\
                    ppf_sample_0[0][i_sb] = (f_scalefactor_0 = (pf_scalefactor_0)[i_sb]) * pf_ungroup[0]; \
                    ppf_sample_0[1][i_sb] = f_scalefactor_0 * pf_ungroup[1]; \
                    ppf_sample_0[2][i_sb] = f_scalefactor_0 * pf_ungroup[2]; \
\
                    ppf_sample_1[0][i_sb] = (f_scalefactor_1 = (pf_scalefactor_1)[i_sb]) * pf_ungroup[0]; \
                    ppf_sample_1[1][i_sb] = f_scalefactor_1 * pf_ungroup[1]; \
                    ppf_sample_1[2][i_sb] = f_scalefactor_1 * pf_ungroup[2]; \
                } \
            } \
        } \
\
        DCT32 (ppf_sample_0[0], &p_adec->bank_0); \
        PCM (&p_adec->bank_0, &p_s16, 2); \
        p_s16 -= 63; \
\
        DCT32 (ppf_sample_1[0], &p_adec->bank_1); \
        PCM (&p_adec->bank_1, &p_s16, 2); \
        p_s16 -= 1; \
\
        DCT32 (ppf_sample_0[1], &p_adec->bank_0); \
        PCM (&p_adec->bank_0, &p_s16, 2); \
        p_s16 -= 63; \
\
        DCT32 (ppf_sample_1[1], &p_adec->bank_1); \
        PCM (&p_adec->bank_1, &p_s16, 2); \
        p_s16 -= 1; \
\
        DCT32 (ppf_sample_0[2], &p_adec->bank_0); \
        PCM (&p_adec->bank_0, &p_s16, 2); \
        p_s16 -= 63; \
\
        DCT32 (ppf_sample_1[2], &p_adec->bank_1); \
        PCM (&p_adec->bank_1, &p_s16, 2); \
        p_s16 -= 1; \
    }
/* #define READ_SAMPLE_L2S */

    i_gr = 0;
    p_s16 = buffer;

    READ_SAMPLE_L2S (pf_scalefactor_0_0, pf_scalefactor_1_0, 4)
    READ_SAMPLE_L2S (pf_scalefactor_0_1, pf_scalefactor_1_1, 8)
    READ_SAMPLE_L2S (pf_scalefactor_0_2, pf_scalefactor_1_2, 12)

    p_adec->bit_stream.buffer = 0;
    p_adec->bit_stream.i_available = 0;
    return (6);
}

/**** wkn ****/

int adec_init (audiodec_t * p_adec)
{
    p_adec->bank_0.actual = p_adec->bank_0.v1;
    p_adec->bank_0.pos = 0;
    p_adec->bank_1.actual = p_adec->bank_1.v1;
    p_adec->bank_1.pos = 0;
    return 0;
}

int adec_sync_frame (audiodec_t * p_adec, adec_sync_info_t * p_sync_info)
{
    static int mpeg1_sample_rate[3] = {44100, 48000, 32000};
    static int mpeg1_layer1_bit_rate[15] = {
	0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448
    };
    static int mpeg1_layer2_bit_rate[15] = {
	0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384
    };
    static int mpeg2_layer1_bit_rate[15] = {
	0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256
    };
    static int mpeg2_layer2_bit_rate[15] = {
	0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160
    };
    u32 header;
    int index;
    int * bit_rate_table;
    int sample_rate;
    int bit_rate;
    int frame_size;

    p_adec->bit_stream.total_bytes_read = 0;
    header = GetByte (&p_adec->bit_stream) << 24;
    header |= GetByte (&p_adec->bit_stream) << 16;
    header |= GetByte (&p_adec->bit_stream) << 8;
    header |= GetByte (&p_adec->bit_stream);

    p_adec->header = header;

    /* basic header check : sync word, no emphasis */
    if ((header & 0xfff00003) != 0xfff00000)
	return 1;

    index = (header >> 10) & 3;		/* sample rate index */
    if (index > 2)
	return 1;
    sample_rate = mpeg1_sample_rate[index];

    switch ((header >> 17) & 7) {
    case 2:	/* mpeg 2, layer 2 */
	sample_rate >>= 1;		/* half sample rate for mpeg2 */
	bit_rate_table = mpeg2_layer2_bit_rate;
	break;
    case 3:	/* mpeg 2, layer 1 */
	sample_rate >>= 1;		/* half sample rate for mpeg2 */
	bit_rate_table = mpeg2_layer1_bit_rate;
	break;
    case 6:	/* mpeg1, layer 2 */
	bit_rate_table = mpeg1_layer2_bit_rate;
	break;
    case 7:	/* mpeg1, layer 1 */
	bit_rate_table = mpeg1_layer1_bit_rate;
	break;
    default:	/* invalid layer */
	return 1;
    }

    index = (header >> 12) & 15;	/* bit rate index */
    if (index > 14)
	return 1;
    bit_rate = bit_rate_table[index];

    p_sync_info->sample_rate = sample_rate;
    p_sync_info->bit_rate = bit_rate;

    if ((header & 0x60000) == 0x60000) {	/* layer 1 */
	frame_size = 48000 * bit_rate / sample_rate;
	if (header & 0x200)	/* padding */
	    frame_size += 4;
    } else {	/* layer >1 */
	frame_size = 144000 * bit_rate / sample_rate;
	if (header & 0x200)	/* padding */
	    frame_size ++;
    }

    p_sync_info->frame_size = frame_size;
    p_adec->frame_size = frame_size;

    return 0;
}

int adec_decode_frame (audiodec_t * p_adec, s16 * buffer)
{
    p_adec->bit_stream.i_available = 0;

    adec_Layer2_Stereo (p_adec, buffer);

    if (p_adec->bit_stream.total_bytes_read > p_adec->frame_size)
	return 1;

    while (p_adec->bit_stream.total_bytes_read < p_adec->frame_size)
	GetByte (&p_adec->bit_stream);

    return 0;
}
