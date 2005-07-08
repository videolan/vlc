/*****************************************************************************
 * frame.c: MPEG2 video transrating module
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN (Centrale Réseaux) and its contributors
 * Copyright (C) 2003 Antoine Missout <antoine.missout@metakine.com>
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Antoine Missout <antoine.missout@metakine.com>
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

// useful constants
enum
{
    I_TYPE = 1,
    P_TYPE = 2,
    B_TYPE = 3
};


/////---- begin ext mpeg code

#include "putvlc.h"

#include "getvlc.h"

static const int non_linear_quantizer_scale [] =
{
     0,  1,  2,  3,  4,  5,   6,   7,
     8, 10, 12, 14, 16, 18,  20,  22,
    24, 28, 32, 36, 40, 44,  48,  52,
    56, 64, 72, 80, 88, 96, 104, 112
};

static inline int get_macroblock_modes( transrate_t *tr )
{
    bs_transrate_t *bs = &tr->bs;

    int macroblock_modes;
    const MBtab * tab;

    switch( tr->picture_coding_type )
    {
        case I_TYPE:

            tab = MB_I + UBITS (bs->i_bit_in_cache, 1);
            bs_flush( bs, tab->len );
            macroblock_modes = tab->modes;

            if ((!(tr->frame_pred_frame_dct)) && (tr->picture_structure == FRAME_PICTURE))
            {
                macroblock_modes |= UBITS (bs->i_bit_in_cache, 1) * DCT_TYPE_INTERLACED;
                bs_flush( bs, 1 );
            }

            return macroblock_modes;

        case P_TYPE:

            tab = MB_P + UBITS (bs->i_bit_in_cache, 5);
            bs_flush( bs, tab->len );
            macroblock_modes = tab->modes;

            if (tr->picture_structure != FRAME_PICTURE)
            {
                if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
                {
                    macroblock_modes |= UBITS (bs->i_bit_in_cache, 2) * MOTION_TYPE_BASE;
                    bs_flush( bs, 2 );
                }
                return macroblock_modes;
            }
            else if (tr->frame_pred_frame_dct)
            {
                if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
                    macroblock_modes |= MC_FRAME;
                return macroblock_modes;
            }
            else
            {
                if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
                {
                    macroblock_modes |= UBITS (bs->i_bit_in_cache, 2) * MOTION_TYPE_BASE;
                    bs_flush( bs, 2 );
                }
                if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
                {
                    macroblock_modes |= UBITS (bs->i_bit_in_cache, 1) * DCT_TYPE_INTERLACED;
                    bs_flush( bs, 1 );
                }
                return macroblock_modes;
            }

        case B_TYPE:

            tab = MB_B + UBITS (bs->i_bit_in_cache, 6);
            bs_flush( bs, tab->len );
            macroblock_modes = tab->modes;

            if( tr->picture_structure != FRAME_PICTURE)
            {
                if (! (macroblock_modes & MACROBLOCK_INTRA))
                {
                    macroblock_modes |= UBITS (bs->i_bit_in_cache, 2) * MOTION_TYPE_BASE;
                    bs_flush( bs, 2 );
                }
                return macroblock_modes;
            }
            else if (tr->frame_pred_frame_dct)
            {
                /* if (! (macroblock_modes & MACROBLOCK_INTRA)) */
                macroblock_modes |= MC_FRAME;
                return macroblock_modes;
            }
            else
            {
                if (macroblock_modes & MACROBLOCK_INTRA) goto intra;
                macroblock_modes |= UBITS (bs->i_bit_in_cache, 2) * MOTION_TYPE_BASE;
                bs_flush( bs, 2 );
                if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
                {
                    intra:
                    macroblock_modes |= UBITS (bs->i_bit_in_cache, 1) * DCT_TYPE_INTERLACED;
                    bs_flush( bs, 1 );
                }
                return macroblock_modes;
            }

        default:
            return 0;
    }

}

static inline int get_quantizer_scale( transrate_t *tr )
{
    bs_transrate_t *bs = &tr->bs;

    int quantizer_scale_code;

    quantizer_scale_code = UBITS (bs->i_bit_in_cache, 5);
    bs_flush( bs, 5 );

    if( tr->q_scale_type )
        return non_linear_quantizer_scale[quantizer_scale_code];
    else
        return quantizer_scale_code << 1;
}

static inline int get_motion_delta( bs_transrate_t *bs, const int f_code )
{
    int delta;
    int sign;
    const MVtab * tab;

    if (bs->i_bit_in_cache & 0x80000000)
    {
        bs_copy( bs, 1 );
        return 0;
    }
    else if (bs->i_bit_in_cache >= 0x0c000000)
    {

        tab = MV_4 + UBITS (bs->i_bit_in_cache, 4);
        delta = (tab->delta << f_code) + 1;
        bs_copy( bs, tab->len);

        sign = SBITS (bs->i_bit_in_cache, 1);
        bs_copy( bs, 1 );

        if (f_code)
        {
            delta += UBITS (bs->i_bit_in_cache, f_code);
            bs_copy( bs, f_code);
        }

        return (delta ^ sign) - sign;
    }
    else
    {

        tab = MV_10 + UBITS (bs->i_bit_in_cache, 10);
        delta = (tab->delta << f_code) + 1;
        bs_copy( bs, tab->len);

        sign = SBITS (bs->i_bit_in_cache, 1);
        bs_copy( bs, 1);

        if (f_code)
        {
            delta += UBITS (bs->i_bit_in_cache, f_code);
            bs_copy( bs, f_code);
        }

        return (delta ^ sign) - sign;
    }
}


static inline int get_dmv( bs_transrate_t *bs )
{
    const DMVtab * tab;

    tab = DMV_2 + UBITS (bs->i_bit_in_cache, 2);
    bs_copy( bs, tab->len);
    return tab->dmv;
}

static inline int get_coded_block_pattern( bs_transrate_t *bs )
{
    const CBPtab * tab;

    if (bs->i_bit_in_cache >= 0x20000000)
    {
        tab = CBP_7 + (UBITS (bs->i_bit_in_cache, 7) - 16);
        bs_flush( bs, tab->len );
        return tab->cbp;
    }
    else
    {
        tab = CBP_9 + UBITS (bs->i_bit_in_cache, 9);
        bs_flush( bs, tab->len );
        return tab->cbp;
    }
}

static inline int get_luma_dc_dct_diff( bs_transrate_t *bs, uint32_t *bits, uint8_t *len )
{
    const DCtab * tab;
    int size;
    int dc_diff;

    if (bs->i_bit_in_cache < 0xf8000000)
    {
        tab = DC_lum_5 + UBITS (bs->i_bit_in_cache, 5);
        size = tab->size;
        if (size)
        {
            *bits = bs_read( bs, tab->len );
            *len = tab->len;
            //dc_diff = UBITS (bs->i_bit_in_cache, size) - UBITS (SBITS (~bs->i_bit_in_cache, 1), size);
            dc_diff = UBITS (bs->i_bit_in_cache, size);
            if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
            *bits <<= size;
            *bits |= bs_read( bs, size );
            *len += size;
            return dc_diff;
        }
        else
        {
            *bits = bs_read( bs, 3 );
            *len = 3;
            return 0;
        }
    }
    else
    {
        tab = DC_long + (UBITS (bs->i_bit_in_cache, 9) - 0x1e0);
        size = tab->size;
        *bits = bs_read( bs, tab->len );
        *len = tab->len;
        //dc_diff = UBITS (bs->i_bit_in_cache, size) - UBITS (SBITS (~bs->i_bit_in_cache, 1), size);
        dc_diff = UBITS (bs->i_bit_in_cache, size);
        if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
        *bits <<= size;
        *bits |= bs_read( bs, size );
        *len += size;
        return dc_diff;
    }
}

static inline int get_chroma_dc_dct_diff( bs_transrate_t *bs, uint32_t *bits, uint8_t *len )
{
    const DCtab * tab;
    int size;
    int dc_diff;

    if (bs->i_bit_in_cache < 0xf8000000)
    {
        tab = DC_chrom_5 + UBITS (bs->i_bit_in_cache, 5);
        size = tab->size;
        if (size)
        {
            *bits = bs_read( bs, tab->len );
            *len = tab->len;
            //dc_diff = UBITS (bs->i_bit_in_cache, size) - UBITS (SBITS (~bs->i_bit_in_cache, 1), size);
            dc_diff = UBITS (bs->i_bit_in_cache, size);
            if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
            *bits <<= size;
            *bits |= bs_read( bs, size );
            *len += size;
            return dc_diff;
        }
        else
        {
            *bits = bs_read( bs, 2 );
            *len = 2;
            return 0;
        }
    }
    else
    {
        tab = DC_long + (UBITS (bs->i_bit_in_cache, 10) - 0x3e0);
        size = tab->size;
        *bits = bs_read( bs, tab->len + 1 );
        *len = tab->len + 1;
        //dc_diff = UBITS (bs->i_bit_in_cache, size) - UBITS (SBITS (~bs->i_bit_in_cache, 1), size);
        dc_diff = UBITS (bs->i_bit_in_cache, size);
        if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
        *bits <<= size;
        *bits |= bs_read( bs, size );
        *len += size;
        return dc_diff;
    }
}

static void motion_fr_frame( bs_transrate_t *bs, unsigned int f_code[2] )
{
    get_motion_delta( bs, f_code[0] );
    get_motion_delta( bs, f_code[1] );
}

static void motion_fr_field( bs_transrate_t *bs, unsigned int f_code[2] )
{
    bs_copy( bs, 1);

    get_motion_delta( bs, f_code[0]);
    get_motion_delta( bs, f_code[1]);

    bs_copy( bs, 1);

    get_motion_delta( bs, f_code[0]);
    get_motion_delta( bs, f_code[1]);
}

static void motion_fr_dmv( bs_transrate_t *bs, unsigned int f_code[2] )
{
    get_motion_delta( bs, f_code[0]);
    get_dmv( bs );

    get_motion_delta( bs, f_code[1]);
    get_dmv( bs );
}

static void motion_fi_field( bs_transrate_t *bs, unsigned int f_code[2] )
{
    bs_copy( bs, 1);

    get_motion_delta( bs, f_code[0]);
    get_motion_delta( bs, f_code[1]);
}

static void motion_fi_16x8( bs_transrate_t *bs, unsigned int f_code[2] )
{
    bs_copy( bs, 1);

    get_motion_delta( bs, f_code[0]);
    get_motion_delta( bs, f_code[1]);

    bs_copy( bs, 1);

    get_motion_delta( bs, f_code[0]);
    get_motion_delta( bs, f_code[1]);
}

static void motion_fi_dmv( bs_transrate_t *bs, unsigned int f_code[2] )
{
    get_motion_delta( bs, f_code[0]);
    get_dmv( bs );

    get_motion_delta( bs, f_code[1]);
    get_dmv( bs );
}


#define MOTION_CALL(routine,direction)                      \
do {                                                        \
    if ((direction) & MACROBLOCK_MOTION_FORWARD)            \
        routine( bs, tr->f_code[0]);                        \
    if ((direction) & MACROBLOCK_MOTION_BACKWARD)           \
        routine( bs, tr->f_code[1]);                        \
} while (0)

#define NEXT_MACROBLOCK                                         \
do {                                                            \
    tr->h_offset += 16;                                         \
    if( tr->h_offset == tr->horizontal_size_value)              \
    {                                                           \
        tr->v_offset += 16;                                         \
        if (tr->v_offset > (tr->vertical_size_value - 16)) return;      \
        tr->h_offset = 0;                                       \
    }                                                           \
} while (0)

static void putmbdata( transrate_t *tr, int macroblock_modes )
{
    bs_transrate_t *bs = &tr->bs;

    bs_write( bs,
              mbtypetab[tr->picture_coding_type-1][macroblock_modes&0x1F].code,
              mbtypetab[tr->picture_coding_type-1][macroblock_modes&0x1F].len);

    switch ( tr->picture_coding_type )
    {
        case I_TYPE:
            if ((! (tr->frame_pred_frame_dct)) && (tr->picture_structure == FRAME_PICTURE))
                bs_write( bs, macroblock_modes & DCT_TYPE_INTERLACED ? 1 : 0, 1);
            break;

        case P_TYPE:
            if (tr->picture_structure != FRAME_PICTURE)
            {
                if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
                    bs_write( bs, (macroblock_modes & MOTION_TYPE_MASK) / MOTION_TYPE_BASE, 2);
                break;
            }
            else if (tr->frame_pred_frame_dct) break;
            else
            {
                if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
                    bs_write( bs, (macroblock_modes & MOTION_TYPE_MASK) / MOTION_TYPE_BASE, 2);
                if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
                    bs_write( bs, macroblock_modes & DCT_TYPE_INTERLACED ? 1 : 0, 1);
                break;
            }

        case B_TYPE:
            if (tr->picture_structure != FRAME_PICTURE)
            {
                if (! (macroblock_modes & MACROBLOCK_INTRA))
                    bs_write( bs, (macroblock_modes & MOTION_TYPE_MASK) / MOTION_TYPE_BASE, 2);
                break;
            }
            else if (tr->frame_pred_frame_dct) break;
            else
            {
                if (macroblock_modes & MACROBLOCK_INTRA) goto intra;
                bs_write( bs, (macroblock_modes & MOTION_TYPE_MASK) / MOTION_TYPE_BASE, 2);
                if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
                {
                    intra:
                    bs_write( bs, macroblock_modes & DCT_TYPE_INTERLACED ? 1 : 0, 1);
                }
                break;
            }
    }
}

static const uint8_t map_non_linear_mquant[113] =
{
    0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,
    16,17,17,17,18,18,18,18,19,19,19,19,20,20,20,20,21,21,21,21,22,22,
    22,22,23,23,23,23,24,24,24,24,24,24,24,25,25,25,25,25,25,25,26,26,
    26,26,26,26,26,26,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,29,
    29,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,31,31,31,31,31
};
static inline void put_quantiser( transrate_t *tr )
{
    bs_transrate_t *bs = &tr->bs;

    bs_write( bs, tr->q_scale_type ? map_non_linear_mquant[tr->new_quantizer_scale] : tr->new_quantizer_scale >> 1, 5 );
    tr->last_coded_scale = tr->new_quantizer_scale;
}

/* generate variable length code for macroblock_address_increment (6.3.16) */
static inline void putaddrinc( transrate_t *tr, int addrinc )
{
    bs_transrate_t *bs = &tr->bs;

    while ( addrinc >= 33 )
    {
        bs_write( bs, 0x08, 11 ); /* macroblock_escape */
        addrinc -= 33;
    }

    bs_write( bs, addrinctab[addrinc].code, addrinctab[addrinc].len );
}

static int slice_init( transrate_t *tr,  int code )
{
    bs_transrate_t *bs = &tr->bs;
    int offset;
    const MBAtab * mba;

    tr->v_offset = (code - 1) * 16;

    tr->quantizer_scale = get_quantizer_scale( tr );
    if ( tr->new_quantizer_scale < tr->quantizer_scale )
        tr->new_quantizer_scale = scale_quant( tr, tr->qrate );

    /*LOGF("************************\nstart of slice %i in %s picture. ori quant: %i new quant: %i\n", code,
        (picture_coding_type == I_TYPE ? "I_TYPE" : (picture_coding_type == P_TYPE ? "P_TYPE" : "B_TYPE")),
        quantizer_scale, new_quantizer_scale);*/

    /* ignore intra_slice and all the extra data */
    while (bs->i_bit_in_cache & 0x80000000)
    {
        bs_flush( bs, 9 );
    }

    /* decode initial macroblock address increment */
    offset = 0;
    for( ;; )
    {
        if (bs->i_bit_in_cache >= 0x08000000)
        {
            mba = MBA_5 + (UBITS (bs->i_bit_in_cache, 6) - 2);
            break;
        }
        else if (bs->i_bit_in_cache >= 0x01800000)
        {
            mba = MBA_11 + (UBITS (bs->i_bit_in_cache, 12) - 24);
            break;
        }
        else if( UBITS (bs->i_bit_in_cache, 12 ) == 8 )
        {
            /* macroblock_escape */
            offset += 33;
            bs_flush(bs, 11);
        }
        else
        {
            return -1;
        }
    }

    bs_flush(bs, mba->len + 1);
    tr->h_offset = (offset + mba->mba) << 4;

    while( tr->h_offset - (int)tr->horizontal_size_value >= 0)
    {
        tr->h_offset -= tr->horizontal_size_value;
        tr->v_offset += 16;
    }

    if( tr->v_offset > tr->vertical_size_value - 16 )
    {
        return -1;
    }
    return (offset + mba->mba);
}

static void mpeg2_slice( transrate_t *tr, const int code )
{
    bs_transrate_t *bs = &tr->bs;
    int mba_inc;
    int first_in_slice = 1;

    if( (mba_inc = slice_init( tr, code )) < 0 )
    {
        return;
    }

    for( ;; )
    {
        const MBAtab * mba;
        int macroblock_modes;
        int mba_local;
        int i;

        while (unlikely(bs->i_bit_in < 24)) bs_refill( bs );

        macroblock_modes = get_macroblock_modes( tr );
        if (macroblock_modes & MACROBLOCK_QUANT)
            tr->quantizer_scale = get_quantizer_scale( tr );
        if (tr->new_quantizer_scale < tr->quantizer_scale)
            tr->new_quantizer_scale = scale_quant( tr, tr->qrate );

        //LOGF("blk %i : ", h_offset >> 4);

        if (macroblock_modes & MACROBLOCK_INTRA)
        {
            RunLevel block[6][65]; // terminated by level = 0, so we need 64+1
            RunLevel new_block[6][65]; // terminated by level = 0, so we need 64+1
            uint32_t dc[6];
            uint8_t  dc_len[6];

            // begin saving data
            int batb;
            uint8_t   p_n_ow[32], *p_n_w,
                    *p_o_ow = bs->p_ow, *p_o_w = bs->p_w;
            uint32_t  i_n_bit_out, i_n_bit_out_cache,
                    i_o_bit_out  = bs->i_bit_out, i_o_bit_out_cache = bs->i_bit_out_cache;

            bs->i_bit_out_cache = 0; bs->i_bit_out = BITS_IN_BUF;
            bs->p_ow = bs->p_w = p_n_ow;

            //LOG("intra "); if (macroblock_modes & MACROBLOCK_QUANT) LOGF("got new quant: %i ", quantizer_scale);

            if (tr->concealment_motion_vectors)
            {
                if (tr->picture_structure != FRAME_PICTURE)
                {
                    bs_copy(bs, 1); /* remove field_select */
                }
                /* like motion_frame, but parsing without actual motion compensation */
                get_motion_delta(bs, tr->f_code[0][0]);
                get_motion_delta(bs, tr->f_code[0][1]);

                bs_copy(bs, 1); /* remove marker_bit */
            }

            assert(bs->p_w - bs->p_ow < 32);

            p_n_w = bs->p_w;
            i_n_bit_out = bs->i_bit_out;
            i_n_bit_out_cache = bs->i_bit_out_cache;
            assert(bs->p_ow == p_n_ow);

            bs->i_bit_out = i_o_bit_out ;
            bs->i_bit_out_cache = i_o_bit_out_cache;
            bs->p_ow = p_o_ow;
            bs->p_w = p_o_w;
            // end saving data

            if( tr->intra_vlc_format )
            {
                /* Luma */
                for ( i = 0; i < 4; i++ )
                {
                    get_luma_dc_dct_diff( bs, dc + i, dc_len + i );
                    get_intra_block_B15( tr, block[i] );
                    if (tr->b_error) return;
                }
                /* Chroma */
                for ( ; i < 6; i++ )
                {
                    get_chroma_dc_dct_diff( bs, dc + i, dc_len + i );
                    get_intra_block_B15( tr, block[i] );
                    if (tr->b_error) return;
                }
            }
            else
            {
                /* Luma */
                for ( i = 0; i < 4; i++ )
                {
                    get_luma_dc_dct_diff( bs, dc + i, dc_len + i );
                    get_intra_block_B14( tr, block[i] );
                    if (tr->b_error) return;
                }
                /* Chroma */
                for ( ; i < 6; i++ )
                {
                    get_chroma_dc_dct_diff( bs, dc + i, dc_len + i );
                    get_intra_block_B14( tr, block[i] );
                    if (tr->b_error) return;
                }
            }

            transrate_mb( tr, block, new_block, 0x3f, 1 );

            if (tr->last_coded_scale == tr->new_quantizer_scale)
                macroblock_modes &= ~MACROBLOCK_QUANT;

            if ( first_in_slice )
            {
                put_quantiser( tr );
                bs_write( bs, 0, 1 );
                macroblock_modes &= ~MACROBLOCK_QUANT;
            }
            putaddrinc( tr, mba_inc );
            mba_inc = 0;
            putmbdata( tr, macroblock_modes );
            if( macroblock_modes & MACROBLOCK_QUANT )
            {
                put_quantiser( tr );
            }

            // put saved motion data...
            for (batb = 0; batb < (p_n_w - p_n_ow); batb++)
            {
                bs_write( bs, p_n_ow[batb], 8 );
            }
            bs_write( bs, i_n_bit_out_cache, BITS_IN_BUF - i_n_bit_out );
            // end saved motion data...

            for ( i = 0; i < 6; i++ )
            {
                bs_write( bs, *(dc + i), *(dc_len + i) );
                putintrablk( bs, new_block[i], tr->intra_vlc_format );
            }
    
        }
        else
        {
            RunLevel block[6][65]; // terminated by level = 0, so we need 64+1
            RunLevel new_block[6][65]; // terminated by level = 0, so we need 64+1
            int new_coded_block_pattern = 0;
            int cbp = 0;

            // begin saving data
            int batb;
            uint8_t   p_n_ow[32], *p_n_w,
                    *p_o_ow = bs->p_ow, *p_o_w = bs->p_w;
            uint32_t  i_n_bit_out, i_n_bit_out_cache,
                    i_o_bit_out  = bs->i_bit_out, i_o_bit_out_cache = bs->i_bit_out_cache;

            bs->i_bit_out_cache = 0; bs->i_bit_out = BITS_IN_BUF;
            bs->p_ow = bs->p_w = p_n_ow;

            if (tr->picture_structure == FRAME_PICTURE)
                switch (macroblock_modes & MOTION_TYPE_MASK)
                {
                    case MC_FRAME: MOTION_CALL (motion_fr_frame, macroblock_modes); break;
                    case MC_FIELD: MOTION_CALL (motion_fr_field, macroblock_modes); break;
                    case MC_DMV: MOTION_CALL (motion_fr_dmv, MACROBLOCK_MOTION_FORWARD); break;
                }
            else
                switch (macroblock_modes & MOTION_TYPE_MASK)
                {
                    case MC_FIELD: MOTION_CALL (motion_fi_field, macroblock_modes); break;
                    case MC_16X8: MOTION_CALL (motion_fi_16x8, macroblock_modes); break;
                    case MC_DMV: MOTION_CALL (motion_fi_dmv, MACROBLOCK_MOTION_FORWARD); break;
                }

            //LOG("non intra "); if (macroblock_modes & MACROBLOCK_QUANT) LOGF("got new quant: %i ", quantizer_scale);

            if (macroblock_modes & MACROBLOCK_PATTERN)
            {
                int last_in_slice;

                cbp = get_coded_block_pattern( bs );

                for ( i = 0; i < 6; i++ )
                {
                    if ( cbp & (1 << (5 - i)) )
                    {
                        get_non_intra_block( tr, block[i] );
                        if (tr->b_error) return;
                    }
                }
                last_in_slice = !UBITS( bs->i_bit_in_cache, 11 );

                new_coded_block_pattern = transrate_mb( tr, block, new_block,
                                                        cbp, 0 );

                if ( !new_coded_block_pattern &&
                        !(macroblock_modes
                            & (MACROBLOCK_MOTION_FORWARD
                                | MACROBLOCK_MOTION_BACKWARD))
                        && (first_in_slice || last_in_slice) )
                {
                    /* First mb in slice, just code a 0-mv mb.
                     * This is wrong for last in slice, but it only shows
                     * a few artefacts. */
                    macroblock_modes |= MACROBLOCK_MOTION_FORWARD;
                    if (tr->picture_structure == FRAME_PICTURE)
                    {
                        macroblock_modes |= MC_FRAME;
                        bs_write( bs, 0x3, 2 ); /* motion vectors */
                    }
                    else
                    {
                        macroblock_modes |= MC_FIELD;
                        bs_write( bs,
                             (tr->picture_structure == BOTTOM_FIELD ? 1 : 0),
                             1); /* motion field select */
                        bs_write( bs, 0x3, 2 ); /* motion vectors */
                    }
                }

                if ( !new_coded_block_pattern )
                {
                    macroblock_modes &= ~MACROBLOCK_PATTERN;
                    macroblock_modes &= ~MACROBLOCK_QUANT;
                }
                else
                {
                    if ( tr->last_coded_scale == tr->new_quantizer_scale )
                    {
                        macroblock_modes &= ~MACROBLOCK_QUANT;
                    }
                    else
                    {
                        macroblock_modes |= MACROBLOCK_QUANT;
                    }
                }
            }

            assert(bs->p_w - bs->p_ow < 32);

            p_n_w = bs->p_w;
            i_n_bit_out = bs->i_bit_out;
            i_n_bit_out_cache = bs->i_bit_out_cache;
            assert(bs->p_ow == p_n_ow);

            bs->i_bit_out = i_o_bit_out ;
            bs->i_bit_out_cache = i_o_bit_out_cache;
            bs->p_ow = p_o_ow;
            bs->p_w = p_o_w;
            // end saving data

            if ( macroblock_modes &
                    (MACROBLOCK_MOTION_FORWARD | MACROBLOCK_MOTION_BACKWARD
                      | MACROBLOCK_PATTERN) )
            {
                if ( first_in_slice )
                {
                    put_quantiser( tr );
                    bs_write( bs, 0, 1 );
                    macroblock_modes &= ~MACROBLOCK_QUANT;
                }
                putaddrinc( tr, mba_inc );
                mba_inc = 0;
                putmbdata( tr, macroblock_modes );
                if ( macroblock_modes & MACROBLOCK_QUANT )
                {
                    put_quantiser( tr );
                }

                // put saved motion data...
                for (batb = 0; batb < (p_n_w - p_n_ow); batb++)
                {
                    bs_write( bs, p_n_ow[batb], 8 );
                }
                bs_write( bs, i_n_bit_out_cache, BITS_IN_BUF - i_n_bit_out);
                // end saved motion data...

                if (macroblock_modes & MACROBLOCK_PATTERN)
                {
                    /* Write CBP */
                    bs_write( bs, cbptable[new_coded_block_pattern].code,
                              cbptable[new_coded_block_pattern].len );

                    for ( i = 0; i < 6; i++ )
                    {
                        if ( new_coded_block_pattern & (1 << (5 - i)) )
                        {
                            putnonintrablk( bs, new_block[i] );
                        }
                    }
                }
            }
            else
            {
                /* skipped macroblock */
                mba_inc++;
            }
        }

        if (bs->p_c > bs->p_r || bs->p_w > bs->p_rw)
        {
            tr->b_error = 1;
            return;
        }
        //LOGF("\n\to: %i c: %i n: %i\n", quantizer_scale, last_coded_scale, new_quantizer_scale);

        NEXT_MACROBLOCK;

        first_in_slice = 0;
        mba_local = 0;
        for ( ; ; )
        {
            if ( bs->i_bit_in_cache >= 0x10000000 )
            {
                mba = MBA_5 + (UBITS (bs->i_bit_in_cache, 5) - 2);
                break;
            }
            else if ( bs->i_bit_in_cache >= 0x03000000 )
            {
                mba = MBA_11 + (UBITS (bs->i_bit_in_cache, 11) - 24);
                break;
            }
            else if ( UBITS( bs->i_bit_in_cache, 11 ) == 8 )
            {
                /* macroblock_escape */
                mba_inc += 33;
                mba_local += 33;
                bs_flush(bs, 11);
            }
            else
            {
                /* EOS or error */
                return;
            }
        }
        bs_flush(bs, mba->len);
        mba_inc += mba->mba;
        mba_local += mba->mba;

        while( mba_local-- )
        {
            NEXT_MACROBLOCK;
        }
    }
}

static const uint8_t mpeg2_scan_norm[64] ATTR_ALIGN(16) = {
    /* Zig-Zag scan pattern */
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

static const uint8_t mpeg2_scan_alt[64] ATTR_ALIGN(16) = {
    /* Alternate scan pattern */
     0, 8,  16, 24,  1,  9,  2, 10, 17, 25, 32, 40, 48, 56, 57, 49,
    41, 33, 26, 18,  3, 11,  4, 12, 19, 27, 34, 42, 50, 58, 35, 43,
    51, 59, 20, 28,  5, 13,  6, 14, 21, 29, 36, 44, 52, 60, 37, 45,
    53, 61, 22, 30,  7, 15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63
};

static const int16_t default_intra_matrix[64] = {
        8, 16, 19, 22, 26, 27, 29, 34,
        16, 16, 22, 24, 27, 29, 34, 37,
        19, 22, 26, 27, 29, 34, 34, 38,
        22, 22, 26, 27, 29, 34, 37, 40,
        22, 26, 27, 29, 32, 35, 40, 48,
        26, 27, 29, 32, 35, 40, 48, 58,
        26, 27, 29, 34, 38, 46, 56, 69,
        27, 29, 35, 38, 46, 56, 69, 83
};

static int mpeg2_header_sequence( transrate_t * tr )
{
    bs_transrate_t *bs = &tr->bs;
    int has_intra = 0, has_non_intra = 0;
    int i;

    i = (bs->p_c[0] << 16) | (bs->p_c[1] << 8) | bs->p_c[2];
    tr->horizontal_size_value = i >> 12;
    tr->vertical_size_value = i & 0xfff;
    tr->horizontal_size_value = (tr->horizontal_size_value + 15) & ~15;
    tr->vertical_size_value = (tr->vertical_size_value + 15) & ~15;
    if ( !tr->horizontal_size_value || !tr->vertical_size_value )
    {
        return -1;
    }

    if ( tr->mpeg4_matrix )
    {
        if (bs->p_c[7] & 2)
        {
            has_intra = 1;
            for (i = 0; i < 64; i++)
                tr->intra_quantizer_matrix[mpeg2_scan_norm[i]] =
                (bs->p_c[i+7] << 7) | (bs->p_c[i+8] >> 1);
        }
        else
        {
            for (i = 0; i < 64; i++)
                tr->intra_quantizer_matrix[mpeg2_scan_norm[i]] =
                default_intra_matrix[i];
        }

        if (bs->p_c[7+64] & 1)
        {
            has_non_intra = 1;
            for (i = 0; i < 64; i++)
                tr->non_intra_quantizer_matrix[mpeg2_scan_norm[i]] =
                bs->p_c[i+8+64];
        }
        else
        {
            for (i = 0; i < 64; i++)
                tr->non_intra_quantizer_matrix[i] = 16;
        }
    }

    /* Write quantization matrices */
    memcpy( bs->p_w, bs->p_c, 8 );
    bs->p_c += 8;

    if ( tr->mpeg4_matrix )
    {
        memset( &bs->p_w[8], 0, 128 );
        bs->p_w[7] |= 2;
        bs->p_w[7] &= ~1;
        for (i = 0; i < 64; i++)
        {
            bs->p_w[i+7] |= mpeg4_default_intra_matrix[mpeg2_scan_norm[i]] >> 7;
            bs->p_w[i+8] |= mpeg4_default_intra_matrix[mpeg2_scan_norm[i]] << 1;
        }

        bs->p_w[7+64] |= 1;
        for (i = 0; i < 64; i++)
        {
            bs->p_w[i+8+64] |= mpeg4_default_intra_matrix[mpeg2_scan_norm[i]];
        }
        bs->p_w += 8 + 128;
        bs->p_c += (has_intra + has_non_intra) * 64;
    }
    else
    {
        bs->p_w += 8;
    }

    tr->scan = mpeg2_scan_norm;

    return 0;
}

/////---- end ext mpeg code

static int do_next_start_code( transrate_t *tr )
{
    bs_transrate_t *bs = &tr->bs;
    uint8_t ID;

    // get start code
    ID = bs->p_c[0];

    /* Copy one byte */
    *bs->p_w++ = *bs->p_c++;

    if (ID == 0x00) // pic header
    {
        tr->picture_coding_type = (bs->p_c[1] >> 3) & 0x7;
        bs->p_c[1] |= 0x7; bs->p_c[2] = 0xFF; bs->p_c[3] |= 0xF8; // vbv_delay is now 0xFFFF

        memcpy(bs->p_w, bs->p_c, 4);
        bs->p_c += 4;
        bs->p_w += 4;
    }
    else if (ID == 0xB3) // seq header
    {
        mpeg2_header_sequence(tr);
    }
    else if (ID == 0xB5) // extension
    {
        if ((bs->p_c[0] >> 4) == 0x8) // pic coding ext
        {
            tr->f_code[0][0] = (bs->p_c[0] & 0xF) - 1;
            tr->f_code[0][1] = (bs->p_c[1] >> 4) - 1;
            tr->f_code[1][0] = (bs->p_c[1] & 0xF) - 1;
            tr->f_code[1][1] = (bs->p_c[2] >> 4) - 1;

            /* tr->intra_dc_precision = (bs->p_c[2] >> 2) & 0x3; */
            tr->picture_structure = bs->p_c[2] & 0x3;
            tr->frame_pred_frame_dct = (bs->p_c[3] >> 6) & 0x1;
            tr->concealment_motion_vectors = (bs->p_c[3] >> 5) & 0x1;
            tr->q_scale_type = (bs->p_c[3] >> 4) & 0x1;
            tr->intra_vlc_format = (bs->p_c[3] >> 3) & 0x1;
            if ( (bs->p_c[3] >> 2) & 0x1 )
                tr->scan = mpeg2_scan_alt;

            memcpy(bs->p_w, bs->p_c, 5);
            bs->p_c += 5;
            bs->p_w += 5;
        }
        else
        {
            *bs->p_w++ = *bs->p_c++;
        }
    }
    else if (ID == 0xB8) // gop header
    {
        memcpy(bs->p_w, bs->p_c, 4);
        bs->p_c += 4;
        bs->p_w += 4;
    }
    else if ((ID >= 0x01) && (ID <= 0xAF)) // slice
    {
        uint8_t *outTemp = bs->p_w, *inTemp = bs->p_c;

        if( tr->qrate != 1.0 )
        {
            if( !tr->horizontal_size_value || !tr->vertical_size_value )
            {
                return -1;
            }

            // init bit buffer
            bs->i_bit_in_cache = 0; bs->i_bit_in = 0;
            bs->i_bit_out_cache = 0; bs->i_bit_out = BITS_IN_BUF;

            // get 32 bits
            bs_refill( bs );
            bs_refill( bs );
            bs_refill( bs );
            bs_refill( bs );

            // begin bit level recoding
            mpeg2_slice(tr, ID);
            if (tr->b_error) return -1;

            bs_flush_read( bs );
            bs_flush_write( bs );
            // end bit level recoding

            /* Basic sanity checks --Meuuh */
            if (bs->p_c > bs->p_r || bs->p_w > bs->p_rw)
            {
                return -1;
            }

            /*LOGF("type: %s code: %02i in : %6i out : %6i diff : %6i fact: %2.2f\n",
            (picture_coding_type == I_TYPE ? "I_TYPE" : (picture_coding_type == P_TYPE ? "P_TYPE" : "B_TYPE")),
            ID,  bs->p_c - inTemp, bs->p_w - outTemp, (bs->p_w - outTemp) - (bs->p_c - inTemp), (float)(bs->p_c - inTemp) / (float)(bs->p_w - outTemp));*/

            if (bs->p_w - outTemp > bs->p_c - inTemp) // yes that might happen, rarely
            {

                /*LOGF("*** slice bigger than before !! (type: %s code: %i in : %i out : %i diff : %i)\n",
                (picture_coding_type == I_TYPE ? "I_TYPE" : (picture_coding_type == P_TYPE ? "P_TYPE" : "B_TYPE")),
                ID, bs->p_c - inTemp, bs->p_w - outTemp, (bs->p_w - outTemp) - (bs->p_c - inTemp));*/

                if ( !tr->mpeg4_matrix )
                {
                    // in this case, we'll just use the original slice !
                    memcpy(outTemp, inTemp, bs->p_c - inTemp);
                    bs->p_w = outTemp + (bs->p_c - inTemp);

                    // adjust bs->i_byte_out
                    bs->i_byte_out -= (bs->p_w - outTemp) - (bs->p_c - inTemp);
                }
                else
                {
                    fprintf(stderr, "bad choice for mpeg4-matrix...\n");
                }
            }
        }
    }
    return 0;
}

int process_frame( sout_stream_t *p_stream, sout_stream_id_t *id,
                   block_t *in, block_t **out, int i_handicap )
{
    transrate_t *tr = &id->tr;
    bs_transrate_t *bs = &tr->bs;

    block_t       *p_out;

    double        f_drift, f_fact;
    int           i_drift;

    p_out = block_New( p_stream, in->i_buffer * 3 );

    p_out->i_length = in->i_length;
    p_out->i_dts    = in->i_dts;
    p_out->i_pts    = in->i_pts;
    p_out->i_flags  = in->i_flags;

    bs->p_rw = bs->p_ow = bs->p_w = p_out->p_buffer;
    bs->p_c = bs->p_r = in->p_buffer;
    bs->p_r += in->i_buffer + 4;
    bs->p_rw += in->i_buffer * 2;
    *(in->p_buffer + in->i_buffer) = 0;
    *(in->p_buffer + in->i_buffer + 1) = 0;
    *(in->p_buffer + in->i_buffer + 2) = 1;
    *(in->p_buffer + in->i_buffer + 3) = 0;

    /* Calculate how late we are */
    bs->i_byte_in = in->i_buffer;
    bs->i_byte_out  = 0;

    i_drift = tr->i_current_output + tr->i_remaining_input
                - tr->i_wanted_output;
    f_drift = (double)i_drift / tr->i_wanted_output;
    f_fact = (double)(tr->i_wanted_output - tr->i_current_output)
                    / tr->i_remaining_input;

    if ( in->i_flags & BLOCK_FLAG_TYPE_I )
    {
        /* This is the last picture of the GOP ; only transrate if we're
         * very late. */
        if ( 0 && f_drift > 0.085 )
        {
            tr->i_minimum_error = (f_drift - 0.085) * 50.0 * 50.0;
            tr->i_admissible_error = (f_drift - 0.085) * 50.0 * 75.0;
            tr->qrate = 1.0 + (f_drift - 0.085) * 50.0;
            msg_Warn( p_stream, "transrating I %d/%d",
                      tr->i_minimum_error, tr->i_admissible_error );
        }
        else
        {
            tr->i_minimum_error = 0;
            tr->i_admissible_error = 0;
            tr->qrate = 1.0;
        }
    }
    else if ( in->i_flags & BLOCK_FLAG_TYPE_P )
    {
        if ( f_fact < 0.8 )
        {
            tr->i_minimum_error = (0.8 - f_fact) * 3000.0 + i_handicap;
            tr->i_admissible_error = (0.8 - f_fact) * 3500.0 + i_handicap;
            tr->qrate = 1.0 + (0.8 - f_fact) * 70.0;
        }
        else
        {
            tr->i_minimum_error = 0;
            tr->i_admissible_error = 0;
            tr->qrate = 1.0;
        }
    }
    else
    {
        if ( f_fact < 1.2 )
        {
            tr->i_minimum_error = (1.2 - f_fact) * 1750.0 + i_handicap;
            tr->i_admissible_error = (1.2 - f_fact) * 2250.0 + i_handicap;
            tr->qrate = 1.0 + (1.2 - f_fact) * 45.0;
        }
        else
        {
            tr->i_minimum_error = 0;
            tr->i_admissible_error = 0;
            tr->qrate = 1.0;
        }
    }

    tr->new_quantizer_scale = 0;
    tr->b_error = 0;

    for ( ; ; )
    {
        uint8_t *p_end = &in->p_buffer[in->i_buffer];

        /* Search next start code */
        for( ;; )
        {
            if( bs->p_c < p_end - 3 && bs->p_c[0] == 0 && bs->p_c[1] == 0 && bs->p_c[2] == 1 )
            {
                /* Next start code */
                break;
            }
            else if( bs->p_c < p_end - 6 &&
                     bs->p_c[0] == 0 && bs->p_c[1] == 0 && bs->p_c[2] == 0 &&
                     bs->p_c[3] == 0 && bs->p_c[4] == 0 && bs->p_c[5] == 0 )
            {
                /* remove stuffing (looking for 6 0x00 bytes) */
                bs->p_c++;
            }
            else
            {
                /* Copy */
                *bs->p_w++ = *bs->p_c++;
            }

            if( bs->p_c >= p_end )
            {
                break;
            }
        }

        if( bs->p_c >= p_end )
        {
            break;
        }

        /* Copy the start code */
        memcpy( bs->p_w, bs->p_c, 3 );
        bs->p_c += 3;
        bs->p_w += 3;

        if ( do_next_start_code( tr ) )
        {
            /* Error */
            msg_Err( p_stream, "error in do_next_start_code()" );
            block_Release( p_out );
            tr->i_remaining_input -= in->i_buffer;
            tr->i_current_output += in->i_buffer;
            return -1;
        }
    }

    bs->i_byte_out += bs->p_w - bs->p_ow;
    p_out->i_buffer = bs->p_w - bs->p_ow;

#if 0
    if ( in->i_flags & BLOCK_FLAG_TYPE_P && f_fact < 0.8 )
    {
        double f_ratio = (in->i_buffer - p_out->i_buffer) / in->i_buffer;
        if ( f_ratio < (0.8 - f_fact) * 0.1 && i_handicap < 200 )
        {
            block_Release( p_out );
            return process_frame( p_stream, id, in, out, i_handicap + 50 );
        }
    }

    if ( in->i_flags & BLOCK_FLAG_TYPE_B && f_fact < 1.1 )
    {
        double f_ratio = (double)(in->i_buffer - p_out->i_buffer)
                            / in->i_buffer;
        if ( f_ratio < (1.1 - f_fact) * 0.1 && i_handicap < 400 )
        {
#ifdef DEBUG_TRANSRATER
            msg_Dbg( p_stream, "%d: %d -> %d big (f: %f d: %f)",
                     tr->picture_coding_type, in->i_buffer, p_out->i_buffer,
                     f_fact, f_drift);
#endif
            block_Release( p_out );
            return process_frame( p_stream, id, in, out, i_handicap + 100 );
        }
    }
#endif

#if 0
    {
        int toto;
        for ( toto = 0; toto < p_out->i_buffer; toto++ )
            if (in->p_buffer[toto] != p_out->p_buffer[toto])
                msg_Dbg(p_stream, "toto %d %x %x", toto, in->p_buffer[toto], p_out->p_buffer[toto]);
    }
#endif

    block_ChainAppend( out, p_out );
    tr->i_remaining_input -= in->i_buffer;
    tr->i_current_output += p_out->i_buffer;

#ifdef DEBUG_TRANSRATER
    msg_Dbg( p_stream, "%d: %d -> %d (%d/%d)",
             tr->picture_coding_type, in->i_buffer, p_out->i_buffer,
             tr->i_minimum_error, tr->i_admissible_error );
#endif

    return 0;
}


