/*****************************************************************************
 * ac3_bit_allocate.c: ac3 allocation tables
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: ac3_bit_allocate.c,v 1.1 2001/11/13 12:09:17 henri Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <string.h>                                              /* memcpy() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "ac3_imdct.h"
#include "ac3_downmix.h"
#include "ac3_decoder.h"

#include "ac3_internal.h"                                 /* DELTA_BIT_REUSE */


static void ba_compute_psd (bit_allocate_t * p_bit, s16 start, s16 end, s16 exps[]);

static void ba_compute_excitation (bit_allocate_t * p_bit, s16 start, s16 end, s16 fgain,
                                   s16 fastleak, s16 slowleak, s16 is_lfe);
static void ba_compute_mask (bit_allocate_t * p_bit, s16 start, s16 end, u16 fscod,
                             u16 deltbae, u16 deltnseg, u16 deltoffst[],
                             u16 deltba[], u16 deltlen[]);
static void ba_compute_bap (bit_allocate_t * p_bit, s16 start, s16 end,
                            s16 snroffset, s16 bap[]);

/* Misc LUTs for bit allocation process */

static const s16 slowdec[]  = { 0x0f,  0x11,  0x13,  0x15  };
static const s16 fastdec[]  = { 0x3f,  0x53,  0x67,  0x7b  };
static const s16 slowgain[] = { 0x540, 0x4d8, 0x478, 0x410 };
static const s16 dbpbtab[]  = { 0x000, 0x700, 0x900, 0xb00 };

static const u16 floortab[] = { 0x2f0, 0x2b0, 0x270, 0x230, 0x1f0, 0x170, 0x0f0, 0xf800 };
static const s16 fastgain[] = { 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380, 0x400  };

static const s16 bndtab[] = {  0,  1,  2,   3,   4,   5,   6,   7,   8,   9,
                        10, 11, 12,  13,  14,  15,  16,  17,  18,  19,
                        20, 21, 22,  23,  24,  25,  26,  27,  28,  31,
                        34, 37, 40,  43,  46,  49,  55,  61,  67,  73,
                        79, 85, 97, 109, 121, 133, 157, 181, 205, 229 };

static const s16 bndsz[]  = { 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
                        1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
                        1,  1,  1,  1,  1,  1,  1,  1,  3,  3,
                        3,  3,  3,  3,  3,  6,  6,  6,  6,  6,
                        6, 12, 12, 12, 12, 24, 24, 24, 24, 24 };

static const s16 masktab[] = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
                     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 28, 28, 29,
                     29, 29, 30, 30, 30, 31, 31, 31, 32, 32, 32, 33, 33, 33, 34, 34,
                     34, 35, 35, 35, 35, 35, 35, 36, 36, 36, 36, 36, 36, 37, 37, 37,
                     37, 37, 37, 38, 38, 38, 38, 38, 38, 39, 39, 39, 39, 39, 39, 40,
                     40, 40, 40, 40, 40, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
                     41, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 43, 43, 43,
                     43, 43, 43, 43, 43, 43, 43, 43, 43, 44, 44, 44, 44, 44, 44, 44,
                     44, 44, 44, 44, 44, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
                     45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 46, 46, 46,
                     46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
                     46, 46, 46, 46, 46, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
                     47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 48, 48, 48,
                     48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
                     48, 48, 48, 48, 48, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
                     49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49,  0,  0,  0 };


static const s16 latab[] = { 0x0040, 0x003f, 0x003e, 0x003d, 0x003c, 0x003b, 0x003a, 0x0039,
                    0x0038, 0x0037, 0x0036, 0x0035, 0x0034, 0x0034, 0x0033, 0x0032,
                    0x0031, 0x0030, 0x002f, 0x002f, 0x002e, 0x002d, 0x002c, 0x002c,
                    0x002b, 0x002a, 0x0029, 0x0029, 0x0028, 0x0027, 0x0026, 0x0026,
                    0x0025, 0x0024, 0x0024, 0x0023, 0x0023, 0x0022, 0x0021, 0x0021,
                    0x0020, 0x0020, 0x001f, 0x001e, 0x001e, 0x001d, 0x001d, 0x001c,
                    0x001c, 0x001b, 0x001b, 0x001a, 0x001a, 0x0019, 0x0019, 0x0018,
                    0x0018, 0x0017, 0x0017, 0x0016, 0x0016, 0x0015, 0x0015, 0x0015,
                    0x0014, 0x0014, 0x0013, 0x0013, 0x0013, 0x0012, 0x0012, 0x0012,
                    0x0011, 0x0011, 0x0011, 0x0010, 0x0010, 0x0010, 0x000f, 0x000f,
                    0x000f, 0x000e, 0x000e, 0x000e, 0x000d, 0x000d, 0x000d, 0x000d,
                    0x000c, 0x000c, 0x000c, 0x000c, 0x000b, 0x000b, 0x000b, 0x000b,
                    0x000a, 0x000a, 0x000a, 0x000a, 0x000a, 0x0009, 0x0009, 0x0009,
                    0x0009, 0x0009, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
                    0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0006, 0x0006,
                    0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0005, 0x0005,
                    0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0004, 0x0004,
                    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
                    0x0004, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
                    0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0002,
                    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
                    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
                    0x0002, 0x0002, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
                    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
                    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
                    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
                    0x0001, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
                    0x0000, 0x0000, 0x0000, 0x0000};

static const s16 hth[][50] = {{ 0x04d0, 0x04d0, 0x0440, 0x0400, 0x03e0, 0x03c0, 0x03b0, 0x03b0,
                      0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x0390, 0x0390, 0x0390,
                      0x0380, 0x0380, 0x0370, 0x0370, 0x0360, 0x0360, 0x0350, 0x0350,
                      0x0340, 0x0340, 0x0330, 0x0320, 0x0310, 0x0300, 0x02f0, 0x02f0,
                      0x02f0, 0x02f0, 0x0300, 0x0310, 0x0340, 0x0390, 0x03e0, 0x0420,
                      0x0460, 0x0490, 0x04a0, 0x0460, 0x0440, 0x0440, 0x0520, 0x0800,
                      0x0840, 0x0840 },

                    { 0x04f0, 0x04f0, 0x0460, 0x0410, 0x03e0, 0x03d0, 0x03c0, 0x03b0,
                      0x03b0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x0390, 0x0390,
                      0x0390, 0x0380, 0x0380, 0x0380, 0x0370, 0x0370, 0x0360, 0x0360,
                      0x0350, 0x0350, 0x0340, 0x0340, 0x0320, 0x0310, 0x0300, 0x02f0,
                      0x02f0, 0x02f0, 0x02f0, 0x0300, 0x0320, 0x0350, 0x0390, 0x03e0,
                      0x0420, 0x0450, 0x04a0, 0x0490, 0x0460, 0x0440, 0x0480, 0x0630,
                      0x0840, 0x0840 },

                    { 0x0580, 0x0580, 0x04b0, 0x0450, 0x0420, 0x03f0, 0x03e0, 0x03d0,
                      0x03c0, 0x03b0, 0x03b0, 0x03b0, 0x03a0, 0x03a0, 0x03a0, 0x03a0,
                      0x03a0, 0x03a0, 0x03a0, 0x03a0, 0x0390, 0x0390, 0x0390, 0x0390,
                      0x0380, 0x0380, 0x0380, 0x0370, 0x0360, 0x0350, 0x0340, 0x0330,
                      0x0320, 0x0310, 0x0300, 0x02f0, 0x02f0, 0x02f0, 0x0300, 0x0310,
                      0x0330, 0x0350, 0x03c0, 0x0410, 0x0470, 0x04a0, 0x0460, 0x0440,
                      0x0450, 0x04e0 }};


static const s16 baptab[] = { 0,  1,  1,  1,  1,  1,  2,  2,  3,  3,  3,  4,  4,  5,  5,  6,
                     6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9, 10,
                     10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14,
                     14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15 };

static __inline__ u16 max_value (s16 a, s16 b)
{
    return (a > b ? a : b);
}

static __inline__ u16 min_value (s16 a, s16 b)
{
    return (a < b ? a : b);
}

static __inline__ s16 logadd (s16 a, s16 b)
{
    s16 c;

    if ((c = a - b) >= 0) {
        return (a + latab[min_value(((c) >> 1), 255)]);
    } else {
        return (b + latab[min_value(((-c) >> 1), 255)]);
    }
}

static __inline__ s16 calc_lowcomp (s16 a, s16 b0, s16 b1, s16 bin)
{
    if (bin < 7) {
        if ((b0 + 256) == b1)
            a = 384;
        else if (b0 > b1)
            a = max_value(0, a - 64);
    } else if (bin < 20) {
        if ((b0 + 256) == b1)
            a = 320;
        else if (b0 > b1)
            a = max_value(0, a - 64) ;
    } else
        a = max_value(0, a - 128);

    return a;
}

void bit_allocate (ac3dec_t * p_ac3dec)
{
    u16 i;
    s16 fgain;
    s16 snroffset;
    s16 start;
    s16 end;
    s16 fastleak;
    s16 slowleak;

    /* Only perform bit_allocation if the exponents have changed or we
     * have new sideband information */
    if (p_ac3dec->audblk.chexpstr[0]  == 0 && p_ac3dec->audblk.chexpstr[1] == 0 &&
        p_ac3dec->audblk.chexpstr[2]  == 0 && p_ac3dec->audblk.chexpstr[3] == 0 &&
        p_ac3dec->audblk.chexpstr[4]  == 0 && p_ac3dec->audblk.cplexpstr   == 0 &&
        p_ac3dec->audblk.lfeexpstr    == 0 && p_ac3dec->audblk.baie        == 0 &&
        p_ac3dec->audblk.snroffste    == 0 && p_ac3dec->audblk.deltbaie    == 0)
        return;

    /* Do some setup before we do the bit alloc */
    p_ac3dec->bit_allocate.sdecay = slowdec[p_ac3dec->audblk.sdcycod];
    p_ac3dec->bit_allocate.fdecay = fastdec[p_ac3dec->audblk.fdcycod];
    p_ac3dec->bit_allocate.sgain = slowgain[p_ac3dec->audblk.sgaincod];
    p_ac3dec->bit_allocate.dbknee = dbpbtab[p_ac3dec->audblk.dbpbcod];
    p_ac3dec->bit_allocate.floor = floortab[p_ac3dec->audblk.floorcod];

    /* if all the SNR offset constants are zero then the whole block is zero */
    if (!p_ac3dec->audblk.csnroffst    && !p_ac3dec->audblk.fsnroffst[0] &&
        !p_ac3dec->audblk.fsnroffst[1] && !p_ac3dec->audblk.fsnroffst[2] &&
        !p_ac3dec->audblk.fsnroffst[3] && !p_ac3dec->audblk.fsnroffst[4] &&
        !p_ac3dec->audblk.cplfsnroffst && !p_ac3dec->audblk.lfefsnroffst) {
        memset(p_ac3dec->audblk.fbw_bap,0,sizeof(u16) * 256 * 5);
        memset(p_ac3dec->audblk.cpl_bap,0,sizeof(u16) * 256);
        memset(p_ac3dec->audblk.lfe_bap,0,sizeof(u16) * 7);
        return;
    }

    for (i = 0; i < p_ac3dec->bsi.nfchans; i++) {
        start = 0;
        end = p_ac3dec->audblk.endmant[i] ;
        fgain = fastgain[p_ac3dec->audblk.fgaincod[i]];
        snroffset = (((p_ac3dec->audblk.csnroffst - 15) << 4) + p_ac3dec->audblk.fsnroffst[i]) << 2;
        fastleak = 0;
        slowleak = 0;

        ba_compute_psd (&p_ac3dec->bit_allocate, start, end, p_ac3dec->audblk.fbw_exp[i]);

        ba_compute_excitation (&p_ac3dec->bit_allocate, start, end , fgain, fastleak, slowleak, 0);

        ba_compute_mask (&p_ac3dec->bit_allocate, start, end, p_ac3dec->syncinfo.fscod,
                         p_ac3dec->audblk.deltbae[i],
                         p_ac3dec->audblk.deltnseg[i],
                         p_ac3dec->audblk.deltoffst[i],
                         p_ac3dec->audblk.deltba[i],
                         p_ac3dec->audblk.deltlen[i]);

        ba_compute_bap (&p_ac3dec->bit_allocate, start, end, snroffset, p_ac3dec->audblk.fbw_bap[i]);
    }

    if (p_ac3dec->audblk.cplinu) {
        start = p_ac3dec->audblk.cplstrtmant;
        end = p_ac3dec->audblk.cplendmant;
        fgain = fastgain[p_ac3dec->audblk.cplfgaincod];
        snroffset = (((p_ac3dec->audblk.csnroffst - 15) << 4) + p_ac3dec->audblk.cplfsnroffst) << 2 ;
        fastleak = (p_ac3dec->audblk.cplfleak << 8) + 768;
        slowleak = (p_ac3dec->audblk.cplsleak << 8) + 768;

        ba_compute_psd (&p_ac3dec->bit_allocate, start, end, p_ac3dec->audblk.cpl_exp);

        ba_compute_excitation (&p_ac3dec->bit_allocate, start, end , fgain, fastleak, slowleak, 0);

        ba_compute_mask (&p_ac3dec->bit_allocate, start, end, p_ac3dec->syncinfo.fscod,
                         p_ac3dec->audblk.cpldeltbae,
                         p_ac3dec->audblk.cpldeltnseg,
                         p_ac3dec->audblk.cpldeltoffst,
                         p_ac3dec->audblk.cpldeltba,
                         p_ac3dec->audblk.cpldeltlen);

        ba_compute_bap (&p_ac3dec->bit_allocate, start, end, snroffset, p_ac3dec->audblk.cpl_bap);
    }

    if (p_ac3dec->bsi.lfeon) {
        start = 0;
        end = 7;
        fgain = fastgain[p_ac3dec->audblk.lfefgaincod];
        snroffset = (((p_ac3dec->audblk.csnroffst - 15) << 4) + p_ac3dec->audblk.lfefsnroffst) << 2 ;
        fastleak = 0;
        slowleak = 0;

        ba_compute_psd (&p_ac3dec->bit_allocate, start, end, p_ac3dec->audblk.lfe_exp);

        ba_compute_excitation (&p_ac3dec->bit_allocate, start, end , fgain, fastleak, slowleak, 1);

        ba_compute_mask (&p_ac3dec->bit_allocate, start, end, p_ac3dec->syncinfo.fscod, 2, 0, 0, 0, 0);

        ba_compute_bap (&p_ac3dec->bit_allocate, start, end, snroffset, p_ac3dec->audblk.lfe_bap);
    }
}


static void ba_compute_psd (bit_allocate_t * p_bit, s16 start, s16 end, s16 exps[])
{
    int bin,j,k;
    s16 lastbin = 0;

    /* Map the exponents into dBs */
    for (bin=start; bin<end; bin++) {
        p_bit->psd[bin] = (3072 - (exps[bin] << 7));
    }

    /* Integrate the psd function over each bit allocation band */
    j = start;
    k = masktab[start];

    do {
        lastbin = min_value(bndtab[k] + bndsz[k], end);
        p_bit->bndpsd[k] = p_bit->psd[j];
        j++;

        for (; j < lastbin; j++) {
            p_bit->bndpsd[k] = logadd(p_bit->bndpsd[k],p_bit->psd[j]);
        }

        k++;
    } while (end > lastbin);
}

static void ba_compute_excitation (bit_allocate_t * p_bit, s16 start, s16 end, 
                        s16 fgain, s16 fastleak, s16 slowleak, s16 is_lfe)
{
    int bin;
    s16 bndstrt;
    s16 bndend;
    s16 lowcomp = 0;
    s16 begin = 0;

    /* Compute excitation function */
    bndstrt = masktab[start];
    bndend = masktab[end - 1] + 1;

    if (bndstrt == 0) { /* For fbw and lfe channels */
        lowcomp = calc_lowcomp(lowcomp, p_bit->bndpsd[0], p_bit->bndpsd[1], 0);
        p_bit->excite[0] = p_bit->bndpsd[0] - fgain - lowcomp;
        lowcomp = calc_lowcomp(lowcomp, p_bit->bndpsd[1], p_bit->bndpsd[2], 1);
        p_bit->excite[1] = p_bit->bndpsd[1] - fgain - lowcomp;
        begin = 7 ;

        /* Note: Do not call calc_lowcomp() for the last band of the lfe channel, (bin = 6) */
        for (bin = 2; bin < 7; bin++) {
            if (!(is_lfe && (bin == 6)))
                lowcomp = calc_lowcomp (lowcomp, p_bit->bndpsd[bin], p_bit->bndpsd[bin+1], bin);
            fastleak = p_bit->bndpsd[bin] - fgain;
            slowleak = p_bit->bndpsd[bin] - p_bit->sgain;
            p_bit->excite[bin] = fastleak - lowcomp;

            if (!(is_lfe && (bin == 6))) {
                if (p_bit->bndpsd[bin] <= 
                        p_bit->bndpsd[bin+1])
                {
                    begin = bin + 1 ;
                    break;
                }
            }
        }

        for (bin = begin; bin < min_value(bndend, 22); bin++) {
            if (!(is_lfe && (bin == 6)))
                lowcomp = calc_lowcomp (lowcomp, p_bit->bndpsd[bin],
                            p_bit->bndpsd[bin+1], bin);
            fastleak -= p_bit->fdecay ;
            fastleak = max_value(fastleak, p_bit->bndpsd[bin] - fgain);
            slowleak -= p_bit->sdecay ;
            slowleak = max_value(slowleak, p_bit->bndpsd[bin] - p_bit->sgain);
            p_bit->excite[bin] = max_value(fastleak - lowcomp, slowleak);
        }
        begin = 22;
    } else { /* For coupling channel */
        begin = bndstrt;
    }

    for (bin = begin; bin < bndend; bin++) {
        fastleak -= p_bit->fdecay;
        fastleak = max_value(fastleak, p_bit->bndpsd[bin] - fgain);
        slowleak -= p_bit->sdecay;
        slowleak = max_value(slowleak, p_bit->bndpsd[bin] - p_bit->sgain);
        p_bit->excite[bin] = max_value(fastleak, slowleak) ;
    }
}

static void ba_compute_mask (bit_allocate_t * p_bit, s16 start, s16 end, u16 fscod,
                             u16 deltbae, u16 deltnseg, u16 deltoffst[],
                             u16 deltba[], u16 deltlen[])
{
    int bin,k;
    s16 bndstrt;
    s16 bndend;
    s16 delta;

    bndstrt = masktab[start];
    bndend = masktab[end - 1] + 1;

    /* Compute the masking curve */
    for (bin = bndstrt; bin < bndend; bin++) {
        if (p_bit->bndpsd[bin] < p_bit->dbknee) {
            p_bit->excite[bin] += ((p_bit->dbknee - p_bit->bndpsd[bin]) >> 2);
        }
        p_bit->mask[bin] = max_value(p_bit->excite[bin], hth[fscod][bin]);
    }

    /* Perform delta bit modulation if necessary */
    if ((deltbae == DELTA_BIT_REUSE) || (deltbae == DELTA_BIT_NEW)) {
        s16 band = 0;
        s16 seg = 0;

        for (seg = 0; seg < deltnseg+1; seg++) {
            band += deltoffst[seg];
            if (deltba[seg] >= 4) {
                delta = (deltba[seg] - 3) << 7;
            } else {
                delta = (deltba[seg] - 4) << 7;
            }
            for (k = 0; k < deltlen[seg]; k++) {
                p_bit->mask[band] += delta;
                band++;
            }
        }
    }
}

static void ba_compute_bap (bit_allocate_t * p_bit, s16 start, s16 end, s16 snroffset,
                            s16 bap[])
{
    int i,j,k;
    s16 lastbin = 0;
    s16 address = 0;

    /* Compute the bit allocation pointer for each bin */
    i = start;
    j = masktab[start];

    do {
        lastbin = min_value(bndtab[j] + bndsz[j], end);
        p_bit->mask[j] -= snroffset;
        p_bit->mask[j] -= p_bit->floor;

        if (p_bit->mask[j] < 0)
            p_bit->mask[j] = 0;

        p_bit->mask[j] &= 0x1fe0;
        p_bit->mask[j] += p_bit->floor;
        for (k = i; k < lastbin; k++) {
            address = (p_bit->psd[i] - p_bit->mask[j]) >> 5;
            address = min_value(63, max_value(0, address));
            bap[i] = baptab[address];
            i++;
        }
        j++;
    } while (end > lastbin);
}
