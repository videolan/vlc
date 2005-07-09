/*****************************************************************************
 * block.c: MPEG2 video transrating module
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * Copyright (C) 2003 Antoine Missout
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Antoine Missout
 *          Michel Lespinasse <walken@zoy.org>
 *          Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
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
#include <stdio.h>
#include <stdlib.h>
#define NDEBUG 1
#include <assert.h>
#include <math.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>
#include <vlc/input.h>

#include "transrate.h"

/****************************************************************************
 * transrater, code from M2VRequantizer http://www.metakine.com/
 ****************************************************************************/

/////---- begin ext mpeg code

#include "getvlc.h"
#include "putvlc.h"

static inline int saturate( int i_value )
{
    if ( i_value > 2047 )
        return 2047;
    if ( i_value < -2048 )
        return -2048;
    return i_value;
}

static int64_t get_score( const RunLevel *blk, RunLevel *new_blk, int i_qscale, int i_qscale_new )
{
    int64_t score = 0;
    int i1 = -1, i2 = -1;

    while ( new_blk->level )
    {
        int new_level = new_blk->level;
        int level = blk->level;
	if ( i1 > 64 || i2 > 64 || !blk->run || !new_blk->run ) return score;
        if ( i1 + blk->run == i2 + new_blk->run )
        {
            int64_t tmp = saturate(level * i_qscale) 
                            - saturate(new_level * i_qscale_new);
            i1 += blk->run;
            i2 += new_blk->run;
            score += tmp * tmp;
            blk++;
            new_blk++;
        }
        else
        {
            int64_t tmp = saturate(level * i_qscale);
            i1 += blk->run;
            score += tmp * tmp;
            blk++;
        }
    }

    while ( blk->level )
    {
        int level = blk->level;
        int64_t tmp = saturate(level * i_qscale);
        i1 += blk->run;
        score += tmp * tmp;
        blk++;
    }

    return score;
}

static void change_qscale( const RunLevel *blk, RunLevel *new_blk, int i_qscale, int i_qscale_new, int intra )
{
    int i = 0, li = 0;
    int rounding;
    if ( intra )
        rounding = i_qscale_new / 3;
    else
        rounding = i_qscale_new / 6;

    while ( blk->level )
    {
        int level = blk->level > 0 ? blk->level : -blk->level;
        int new_level = saturate(level * i_qscale) / i_qscale_new;
        i += blk->run;

        if ( new_level )
        {
            new_blk->run = i - li;
            new_blk->level = blk->level > 0 ? new_level : -new_level;
            new_blk++;
            li = i;
        }
        blk++;
    }
    new_blk->level = 0;
}

static const uint8_t non_linear_mquant_table[32] =
{
    0, 1, 2, 3, 4, 5, 6, 7,
    8,10,12,14,16,18,20,22,
    24,28,32,36,40,44,48,52,
    56,64,72,80,88,96,104,112
};
static const uint8_t map_non_linear_mquant[113] =
{
    0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,
    16,17,17,17,18,18,18,18,19,19,19,19,20,20,20,20,21,21,21,21,22,22,
    22,22,23,23,23,23,24,24,24,24,24,24,24,25,25,25,25,25,25,25,26,26,
    26,26,26,26,26,26,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,29,
    29,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,31,31,31,31,31
};

int scale_quant( transrate_t *tr, double qrate )
{
    int i_quant = (int)floor( tr->quantizer_scale * qrate + 0.5 );

    if ( tr->q_scale_type )
    {
        if ( i_quant < 1 )
            i_quant = 1;
        if ( i_quant > 112 )
            i_quant = 112;
        i_quant = non_linear_mquant_table[map_non_linear_mquant[i_quant]];
    }
    else
    {
        if ( i_quant < 2 )
            i_quant = 2;
        if ( i_quant > 62 )
            i_quant = 62;
        i_quant = (i_quant / 2) * 2; // Must be *even*
    }

    return i_quant;
}

int increment_quant( transrate_t *tr, int i_quant )
{
    if ( tr->q_scale_type )
    {
        assert( i_quant >= 1 && i_quant <= 112 );
        i_quant = map_non_linear_mquant[i_quant] + 1;
        if ( i_quant > 31 )
            i_quant = 31;
        i_quant = non_linear_mquant_table[i_quant];
    }
    else
    {
        assert(!(i_quant & 1));
        i_quant += 2;
        if ( i_quant > 62 )
            i_quant = 62;
    }
    return i_quant;
}


static int decrement_quant( transrate_t *tr, int i_quant )
{
    if ( tr->q_scale_type )
    {
        assert( i_quant >= 1 && i_quant <= 112 );
        i_quant = map_non_linear_mquant[i_quant] - 1;
        if ( i_quant < 1 )
            i_quant = 1;
        i_quant = non_linear_mquant_table[i_quant];
    }
    else
    {
        assert(!(i_quant & 1));
        i_quant -= 2;
        if ( i_quant < 2 )
            i_quant = 2;
    }
    return i_quant;
}

static void quantize_block( transrate_t *tr, RunLevel *new_blk, int intra )
{
    RunLevel old_blk[65];
    RunLevel *blk = old_blk;
    const uint8_t *old_matrix, *new_matrix;
    int i = 0, li = 0;

    memcpy( blk, new_blk, 65 * sizeof(RunLevel) );
    if ( intra )
    {
        old_matrix = tr->intra_quantizer_matrix;
        new_matrix = mpeg4_default_intra_matrix;
    }
    else
    {
        old_matrix = tr->non_intra_quantizer_matrix;
        new_matrix = mpeg4_default_non_intra_matrix;
    }

    while ( blk->level )
    {
        int level = blk->level > 0 ? blk->level : -blk->level;
        int new_level = (level * old_matrix[i] + new_matrix[i]/2)
                            / new_matrix[i];
        i += blk->run;

        if (new_level)
        {
            new_blk->run = i - li;
            new_blk->level = blk->level > 0 ? new_level : -new_level;
            new_blk++;
            li = i;
        }
        blk++;
    }
    new_blk->level = 0;
}

int transrate_mb( transrate_t *tr, RunLevel blk[6][65], RunLevel new_blk[6][65],
                  int i_cbp, int intra )
{
    int i_qscale = tr->quantizer_scale;
    int i_guessed_qscale = tr->new_quantizer_scale;
    int64_t i_last_error = 0;
    int i_last_qscale;
    int i_last_qscale_same_error = 0;
    int i_direction = 0;
    int i_new_cbp;
    int i_nb_blocks = 0;
    int i_nb_coeffs = 0;
    int i;

    for ( i = 0; i < 6; i++ )
    {
        if ( i_cbp & (1 << (5 - i)) )
        {
            RunLevel *cur_blk = blk[i];
            i_nb_blocks++;
            while ( cur_blk->level )
            {
                cur_blk++;
                i_nb_coeffs++;
            }
        }
    }

    /* See if we can change quantizer scale */
    for ( ; ; )
    {
        int64_t i_error = 0;
        i_new_cbp = 0;

        for ( i = 0; i < 6; i++ )
        {
            if ( i_cbp & (1 << (5 - i)) )
            {
                int64_t i_block_error;
                change_qscale( blk[i], new_blk[i], i_qscale, i_guessed_qscale,
                               intra );
                i_block_error = get_score( blk[i], new_blk[i],
                                           i_qscale, i_guessed_qscale );
                if ( i > 3 ) i_block_error *= 4;
                if ( i_block_error > i_error )
                    i_error = i_block_error;
                if ( new_blk[i]->level )
                    i_new_cbp |= (1 << (5 - i));
            }
        }

        if ( i_error >= (int64_t)tr->i_minimum_error
                && i_error <= (int64_t)tr->i_admissible_error )
        {
            break;
        }
        if ( i_nb_coeffs <= 15 && i_error <= (int64_t)tr->i_admissible_error )
        {
            /* There is no interest in changing the qscale (takes up 5 bits
             * we won't regain) */
            break;
        }

        if ( !i_direction )
        {
            if ( i_error > (int64_t)tr->i_admissible_error )
            {
                i_direction = -1;
                i_last_qscale = i_guessed_qscale;
                i_guessed_qscale = decrement_quant( tr, i_guessed_qscale );
            }
            else
            {
                i_direction = +1;
                i_last_qscale = i_guessed_qscale;
                i_guessed_qscale = increment_quant( tr, i_guessed_qscale );
                i_last_error = i_error;
                i_last_qscale_same_error = i_last_qscale;
            }
            if ( i_guessed_qscale == i_last_qscale )
                break;
        }
        else if ( i_direction < 0 )
        {
            if ( i_error > (int64_t)tr->i_admissible_error )
            {
                i_last_qscale = i_guessed_qscale;
                i_guessed_qscale = decrement_quant( tr, i_guessed_qscale );
                if ( i_guessed_qscale == i_last_qscale )
                    break;
            }
            else
            {
                break;
            }
        }
        else
        {
            if ( i_error < (int64_t)tr->i_minimum_error )
            {
                i_last_qscale = i_guessed_qscale;
                i_guessed_qscale = increment_quant( tr, i_guessed_qscale );
                if ( i_error > i_last_error )
                {
                    i_last_error = i_error;
                    i_last_qscale_same_error = i_last_qscale;
                }
                if ( i_guessed_qscale == i_last_qscale )
                {
                    if ( i_last_error == i_error )
                    {
                        i_guessed_qscale = i_last_qscale_same_error;
                        if ( i_guessed_qscale == i_qscale )
                        {
                            memcpy( new_blk, blk, sizeof(RunLevel)*65*6 );
                            i_new_cbp = i_cbp;
                        }
                        else
                        {
                            i_new_cbp = 0;
                            for ( i = 0; i < 6; i++ )
                            {
                                if ( i_cbp & (1 << (5 - i)) )
                                {
                                    change_qscale( blk[i], new_blk[i],
                                                   i_qscale, i_guessed_qscale,
                                                   intra );
                                    if ( new_blk[i]->level )
                                        i_new_cbp |= (1 << (5 - i));
                                }
                            }
                        }
                    }
                    break;
                }
            }
            else
            {
                if ( i_error > (int64_t)tr->i_admissible_error
                        || i_last_error == i_error )
                {
                    i_guessed_qscale = i_last_qscale_same_error;
                    if ( i_guessed_qscale == i_qscale )
                    {
                        memcpy( new_blk, blk, sizeof(RunLevel)*65*6 );
                        i_new_cbp = i_cbp;
                    }
                    else
                    {
                        i_new_cbp = 0;
                        for ( i = 0; i < 6; i++ )
                        {
                            if ( i_cbp & (1 << (5 - i)) )
                            {
                                change_qscale( blk[i], new_blk[i],
                                               i_qscale, i_guessed_qscale,
                                               intra );
                                if ( new_blk[i]->level )
                                    i_new_cbp |= (1 << (5 - i));
                            }
                        }
                    }
                }
                break;
            }
        }
    }

    tr->new_quantizer_scale = i_guessed_qscale;

#if 0
    /* Now see if we can drop coeffs */
    for ( i = 0; i < 6; i++ )
    {
        if ( i_new_cbp & (1 << (5 - i)) )
        {
            for ( ; ; )
            {
                RunLevel *last_blk = new_blk[i];
                uint8_t old_level;

                while ( last_blk[1].level )
                    last_blk++;
                if ( last_blk == new_blk[i] )
                    break;
                old_level = last_blk->level;
                last_blk->level = 0;
                i_error = get_score( blk[i], new_blk[i],
                                     i_qscale, i_guessed_qscale );
                if ( i_error > tr->i_admissible_error )
                {
                    last_blk->level = old_level;
                    break;
                }
            }
        }
    }
#endif

    return i_new_cbp;
}

void get_intra_block_B14( transrate_t *tr, RunLevel *blk )
{
    bs_transrate_t *bs = &tr->bs;
    int i, li;
    int val;
    const DCTtab * tab;

    li = i = 0;

    for( ;; )
    {
        if (bs->i_bit_in_cache >= 0x28000000)
        {
            tab = DCT_B14AC_5 + (UBITS (bs->i_bit_in_cache, 5) - 5);

            i += tab->run;
            if (i >= 64) break; /* end of block */

    normal_code:
            bs_flush( bs, tab->len );
            val = tab->level;
            val = (val ^ SBITS (bs->i_bit_in_cache, 1)) - SBITS (bs->i_bit_in_cache, 1);
            blk->level = val;
            blk->run = i - li - 1;
            li = i;
            blk++;

            bs_flush( bs, 1 );
            continue;
        }
        else if (bs->i_bit_in_cache >= 0x04000000)
        {
            tab = DCT_B14_8 + (UBITS (bs->i_bit_in_cache, 8) - 4);

            i += tab->run;
            if (i < 64) goto normal_code;

            /* escape code */
            i += (UBITS (bs->i_bit_in_cache, 12) & 0x3F) - 64;
            if (i >= 64) break; /* illegal, check needed to avoid buffer overflow */

            bs_flush( bs, 12 );
            val = SBITS (bs->i_bit_in_cache, 12);
            blk->level = val;
            blk->run = i - li - 1;
            li = i;
            blk++;

            bs_flush( bs, 12 );

            continue;
        }
        else if (bs->i_bit_in_cache >= 0x02000000)
        {
            tab = DCT_B14_10 + (UBITS (bs->i_bit_in_cache, 10) - 8);
            i += tab->run;
            if (i < 64 ) goto normal_code;
        }
        else if (bs->i_bit_in_cache >= 0x00800000)
        {
            tab = DCT_13 + (UBITS (bs->i_bit_in_cache, 13) - 16);
            i += tab->run;
            if (i < 64 ) goto normal_code;
        }
        else if (bs->i_bit_in_cache >= 0x00200000)
        {
            tab = DCT_15 + (UBITS (bs->i_bit_in_cache, 15) - 16);
            i += tab->run;
            if (i < 64 ) goto normal_code;
        }
        else
        {
            tab = DCT_16 + UBITS (bs->i_bit_in_cache, 16);
            bs_flush( bs, 16 );
            i += tab->run;
            if (i < 64 ) goto normal_code;
        }
        fprintf(stderr, "Err in B14\n");
	tr->b_error = 1;
        break;  /* illegal, check needed to avoid buffer overflow */
    }
    bs_flush( bs, 2 );    /* dump end of block code */
    blk->level = 0;

    if ( tr->mpeg4_matrix )
        quantize_block( tr, blk, 1 );
}

void get_intra_block_B15( transrate_t *tr, RunLevel *blk )
{
    bs_transrate_t *bs = &tr->bs;
    int i, li;
    int val;
    const DCTtab * tab;

    li = i = 0;

    for( ;; )
    {
        if (bs->i_bit_in_cache >= 0x04000000)
        {
            tab = DCT_B15_8 + (UBITS (bs->i_bit_in_cache, 8) - 4);

            i += tab->run;
            if (i < 64)
            {
    normal_code:
                bs_flush( bs, tab->len );

                val = tab->level;
                val = (val ^ SBITS (bs->i_bit_in_cache, 1)) - SBITS (bs->i_bit_in_cache, 1);
                blk->level = val;
                blk->run = i - li - 1;
                li = i;
                blk++;

                bs_flush( bs, 1 );
                continue;
            }
            else
            {
                i += (UBITS (bs->i_bit_in_cache, 12) & 0x3F) - 64;

                if (i >= 64) break; /* illegal, check against buffer overflow */

                bs_flush( bs, 12 );
                val = SBITS (bs->i_bit_in_cache, 12);
                blk->level = val;
                blk->run = i - li - 1;
                li = i;
                blk++;

                bs_flush( bs, 12 );
                continue;
            }
        }
        else if (bs->i_bit_in_cache >= 0x02000000)
        {
            tab = DCT_B15_10 + (UBITS (bs->i_bit_in_cache, 10) - 8);
            i += tab->run;
            if (i < 64) goto normal_code;
        }
        else if (bs->i_bit_in_cache >= 0x00800000)
        {
            tab = DCT_13 + (UBITS (bs->i_bit_in_cache, 13) - 16);
            i += tab->run;
            if (i < 64) goto normal_code;
        }
        else if (bs->i_bit_in_cache >= 0x00200000)
        {
            tab = DCT_15 + (UBITS (bs->i_bit_in_cache, 15) - 16);
            i += tab->run;
            if (i < 64) goto normal_code;
        }
        else
        {
            tab = DCT_16 + UBITS (bs->i_bit_in_cache, 16);
            bs_flush( bs, 16 );
            i += tab->run;
            if (i < 64) goto normal_code;
        }
        fprintf(stderr, "Err in B15\n");
	tr->b_error = 1;
        break;  /* illegal, check needed to avoid buffer overflow */
    }
    bs_flush( bs, 4 );    /* dump end of block code */
    blk->level = 0;

    if ( tr->mpeg4_matrix )
        quantize_block( tr, blk, 1 );
}


int get_non_intra_block( transrate_t *tr, RunLevel *blk )
{
    bs_transrate_t *bs = &tr->bs;
    int i, li;
    int val;
    const DCTtab * tab;

    li = i = -1;

    if (bs->i_bit_in_cache >= 0x28000000)
    {
        tab = DCT_B14DC_5 + (UBITS (bs->i_bit_in_cache, 5) - 5);
        goto entry_1;
    }
    else goto entry_2;

    for( ;; )
    {
        if (bs->i_bit_in_cache >= 0x28000000)
        {
            tab = DCT_B14AC_5 + (UBITS (bs->i_bit_in_cache, 5) - 5);

    entry_1:
            i += tab->run;
            if (i >= 64)
            break;  /* end of block */

    normal_code:

            bs_flush( bs, tab->len );
            val = tab->level;
            val = (val ^ SBITS (bs->i_bit_in_cache, 1)) - SBITS (bs->i_bit_in_cache, 1);
            blk->level = val;
            blk->run = i - li - 1;
            li = i;
            blk++;

            //if ( ((val) && (tab->level < tst)) || ((!val) && (tab->level >= tst)) )
            //  LOGF("level: %i val: %i tst : %i q: %i nq : %i\n", tab->level, val, tst, q, nq);

            bs_flush( bs, 1 );
            continue;
        }

    entry_2:
        if (bs->i_bit_in_cache >= 0x04000000)
        {
            tab = DCT_B14_8 + (UBITS (bs->i_bit_in_cache, 8) - 4);

            i += tab->run;
            if (i < 64) goto normal_code;

            /* escape code */

            i += (UBITS (bs->i_bit_in_cache, 12) & 0x3F) - 64;

            if (i >= 64) break; /* illegal, check needed to avoid buffer overflow */

            bs_flush( bs, 12 );
            val = SBITS (bs->i_bit_in_cache, 12);
            blk->level = val;
            blk->run = i - li - 1;
            li = i;
            blk++;

            bs_flush( bs, 12 );
            continue;
        }
        else if (bs->i_bit_in_cache >= 0x02000000)
        {
            tab = DCT_B14_10 + (UBITS (bs->i_bit_in_cache, 10) - 8);
            i += tab->run;
            if (i < 64) goto normal_code;
        }
        else if (bs->i_bit_in_cache >= 0x00800000)
        {
            tab = DCT_13 + (UBITS (bs->i_bit_in_cache, 13) - 16);
            i += tab->run;
            if (i < 64) goto normal_code;
        }
        else if (bs->i_bit_in_cache >= 0x00200000)
        {
            tab = DCT_15 + (UBITS (bs->i_bit_in_cache, 15) - 16);
            i += tab->run;
            if (i < 64) goto normal_code;
        }
        else
        {
            tab = DCT_16 + UBITS (bs->i_bit_in_cache, 16);
            bs_flush( bs, 16 );

            i += tab->run;
            if (i < 64) goto normal_code;
        }
        fprintf(stderr, "Err in non-intra\n");
	tr->b_error = 1;
        break;  /* illegal, check needed to avoid buffer overflow */
    }
    bs_flush( bs, 2 );    /* dump end of block code */
    blk->level = 0;

    if ( tr->mpeg4_matrix )
        quantize_block( tr, blk, 0 );

    return i;
}

static inline void putAC( bs_transrate_t *bs, int run, int signed_level, int vlcformat)
{
    int level, len;
    const VLCtable *ptab = NULL;

    level = (signed_level<0) ? -signed_level : signed_level; /* abs(signed_level) */

    assert(!(run<0 || run>63 || level==0 || level>2047));

    len = 0;

    if (run<2 && level<41)
    {
        if (vlcformat)  ptab = &dct_code_tab1a[run][level-1];
        else ptab = &dct_code_tab1[run][level-1];
        len = ptab->len;
    }
    else if (run<32 && level<6)
    {
        if (vlcformat) ptab = &dct_code_tab2a[run-2][level-1];
        else ptab = &dct_code_tab2[run-2][level-1];
        len = ptab->len;
    }

    if (len) /* a VLC code exists */
    {
        bs_write( bs, ptab->code, len);
        bs_write( bs, signed_level<0, 1); /* sign */
    }
    else
    {
        bs_write( bs, 1l, 6); /* Escape */
        bs_write( bs, run, 6); /* 6 bit code for run */
        bs_write( bs, ((unsigned int)signed_level) & 0xFFF, 12);
    }
}


static inline void putACfirst( bs_transrate_t *bs, int run, int val)
{
    if (run==0 && (val==1 || val==-1)) bs_write( bs, 2|(val<0), 2 );
    else putAC( bs, run, val, 0);
}

void putnonintrablk( bs_transrate_t *bs, RunLevel *blk)
{
    assert(blk->level);

    putACfirst( bs, blk->run, blk->level );
    blk++;

    while (blk->level)
    {
        putAC( bs, blk->run, blk->level, 0 );
        blk++;
    }

    bs_write( bs, 2, 2 );
}

void putintrablk( bs_transrate_t *bs, RunLevel *blk, int vlcformat)
{
    while (blk->level)
    {
        putAC( bs, blk->run, blk->level, vlcformat );
        blk++;
    }

    if (vlcformat)
        bs_write( bs, 6, 4 );
    else
        bs_write( bs, 2, 2 );
}

