/*****************************************************************************
 * transrate.c
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * Copyright (C) 2003 Freebox S.A.
 * Copyright (C) 2003 Antoine Missout
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 * $Id: transrate.c,v 1.4 2003/11/22 17:03:57 fenrir Exp $
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

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );

static int  transrate_video_process( sout_stream_t *, sout_stream_id_t *, sout_buffer_t *, sout_buffer_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Transrate stream") );
    set_capability( "sout stream", 50 );
    add_shortcut( "transrate" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_stream_sys_t
{
    sout_stream_t   *p_out;

    int             i_vbitrate;

    mtime_t         i_first_frame;
    mtime_t         i_dts, i_pts;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    char *val;

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    p_sys->p_out = sout_stream_new( p_stream->p_sout, p_stream->psz_next );

    p_sys->i_vbitrate   = 0;

    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "vb" ) ) )
    {
        p_sys->i_vbitrate = atoi( val );
        if( p_sys->i_vbitrate < 16000 )
        {
            p_sys->i_vbitrate *= 1000;
        }
    }
    else
    {
        p_sys->i_vbitrate = 3000000;
    }
    p_sys->i_first_frame = 0;

    msg_Dbg( p_stream, "codec video %dkb/s",
             p_sys->i_vbitrate / 1024 );

    if( !p_sys->p_out )
    {
        msg_Err( p_stream, "cannot create chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    p_stream->p_sout->i_padding += 200;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    sout_stream_delete( p_sys->p_out );
    free( p_sys );
}



/*****************************************************************************
 * sout_stream_id_t:
 *****************************************************************************/

typedef struct
{
    uint8_t run;
    short level;
} RunLevel;

typedef struct
{
    uint8_t *p_c;
    uint8_t *p_r;
    uint8_t *p_w;
    uint8_t *p_ow;
    uint8_t *p_rw;

    int i_bit_in;
    int i_bit_out;
    uint32_t i_bit_in_cache;
    uint32_t i_bit_out_cache;

    uint32_t i_byte_in;
    uint32_t i_byte_out;
} bs_transrate_t;

typedef struct
{
    bs_transrate_t bs;

    /* MPEG2 state */

    // seq header
    unsigned int horizontal_size_value;
    unsigned int vertical_size_value;

    // pic header
    unsigned int picture_coding_type;

    // pic code ext
    unsigned int f_code[2][2];
    /* unsigned int intra_dc_precision; */
    unsigned int picture_structure;
    unsigned int frame_pred_frame_dct;
    unsigned int concealment_motion_vectors;
    unsigned int q_scale_type;
    unsigned int intra_vlc_format;
    /* unsigned int alternate_scan; */

    // slice or mb
    // quantizer_scale_code
    unsigned int quantizer_scale;
    unsigned int new_quantizer_scale;
    unsigned int last_coded_scale;
    int   h_offset, v_offset;

    // mb
    double quant_corr, fact_x;
} transrate_t;


struct sout_stream_id_t
{
    void            *id;
    vlc_bool_t      b_transrate;

    sout_buffer_t   *p_current_buffer;
    sout_buffer_t   *p_next_gop;
    mtime_t         i_next_gop_duration;
    size_t          i_next_gop_size;

    transrate_t     tr;
};


static sout_stream_id_t * Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;
    sout_stream_id_t    *id;

    id = malloc( sizeof( sout_stream_id_t ) );
    id->id = NULL;

    if( p_fmt->i_cat == VIDEO_ES
            && p_fmt->i_codec == VLC_FOURCC('m', 'p', 'g', 'v') )
    {
        msg_Dbg( p_stream,
                 "creating video transrating for fcc=`%4.4s'",
                 (char*)&p_fmt->i_codec );

        id->p_current_buffer = NULL;
        id->p_next_gop = NULL;
        id->i_next_gop_duration = 0;
        id->i_next_gop_size = 0;
        memset( &id->tr, 0, sizeof( transrate_t ) );
        id->tr.bs.i_byte_in = id->tr.bs.i_byte_out = 0;
        id->tr.quant_corr = 0.0;
        id->tr.fact_x = 1.0;

        /* open output stream */
        id->id = p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
        id->b_transrate = VLC_TRUE;
    }
    else
    {
        msg_Dbg( p_stream, "not transrating a stream (fcc=`%4.4s')", (char*)&p_fmt->i_codec );
        id->id = p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
        id->b_transrate = VLC_FALSE;

        if( id->id == NULL )
        {
            free( id );
            return NULL;
        }
    }

    return id;
}

static int     Del      ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    if( id->id )
    {
        p_sys->p_out->pf_del( p_sys->p_out, id->id );
    }
    free( id );

    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 sout_buffer_t *p_buffer )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    if( id->b_transrate )
    {
        sout_buffer_t *p_buffer_out;
        transrate_video_process( p_stream, id, p_buffer, &p_buffer_out );

        if( p_buffer_out )
        {
            return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer_out );
        }
        return VLC_SUCCESS;
    }
    else if( id->id != NULL )
    {
        return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer );
    }
    else
    {
        sout_BufferDelete( p_stream->p_sout, p_buffer );
        return VLC_EGENERIC;
    }
}

/****************************************************************************
 * transrater, code from M2VRequantizer http://www.metakine.com/
 ****************************************************************************/

// toggles:
// #define NDEBUG // turns off asserts
/* This is awful magic --Meuuh */
//#define REACT_DELAY (1024.0*128.0)
#define REACT_DELAY (1024.0*4.0)

// notes:
//
// - intra block:
//      - the quantiser is increment by one step
//
// - non intra block:
//      - in P_FRAME we keep the original quantiser but drop the last coefficient
//        if there is more than one
//      - in B_FRAME we multiply the quantiser by a factor
//
// - I_FRAME is recoded when we're 5.0 * REACT_DELAY late
// - P_FRAME is recoded when we're 2.5 * REACT_DELAY late
// - B_FRAME are always recoded

// if we're getting *very* late (60 * REACT_DELAY)
//
// - intra blocks quantiser is incremented two step
// - drop a few coefficients but always keep the first one

// useful constants
enum
{
    I_TYPE = 1,
    P_TYPE = 2,
    B_TYPE = 3
};


// gcc
#ifdef HAVE_BUILTIN_EXPECT
#define likely(x) __builtin_expect ((x) != 0, 1)
#define unlikely(x) __builtin_expect ((x) != 0, 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#define BITS_IN_BUF (8)

#define LOG(msg) fprintf (stderr, msg)
#define LOGF(format, args...) fprintf (stderr, format, args)

static inline void bs_write( bs_transrate_t *s, unsigned int val, int n)
{
    assert(n < 32);
    assert(!(val & (0xffffffffU << n)));

    while (unlikely(n >= s->i_bit_out))
    {
        s->p_w[0] = (s->i_bit_out_cache << s->i_bit_out ) | (val >> (n - s->i_bit_out));
        s->p_w++;
        n -= s->i_bit_out;
        s->i_bit_out_cache = 0;
        val &= ~(0xffffffffU << n);
        s->i_bit_out = BITS_IN_BUF;
    }

    if (likely(n))
    {
        s->i_bit_out_cache = (s->i_bit_out_cache << n) | val;
        s->i_bit_out -= n;
    }

    assert(s->i_bit_out > 0);
    assert(s->i_bit_out <= BITS_IN_BUF);
}

static inline void bs_refill( bs_transrate_t *s )
{
    assert((s->p_r - s->p_c) >= 1);
    s->i_bit_in_cache |= s->p_c[0] << (24 - s->i_bit_in);
    s->i_bit_in += 8;
    s->p_c++;
}

static inline void bs_flush( bs_transrate_t *s, unsigned int n )
{
    assert(s->i_bit_in >= n);

    s->i_bit_in_cache <<= n;
    s->i_bit_in -= n;

    assert( (!n) || ((n>0) && !(s->i_bit_in_cache & 0x1)) );

    while (unlikely(s->i_bit_in < 24)) bs_refill( s );
}

static inline unsigned int bs_read( bs_transrate_t *s, unsigned int n)
{
    unsigned int Val = ((unsigned int)s->i_bit_in_cache) >> (32 - n);
    bs_flush( s, n );
    return Val;
}

static inline unsigned int bs_copy( bs_transrate_t *s, unsigned int n)
{
    unsigned int Val = bs_read( s, n);
    bs_write(s, Val, n);
    return Val;
}

static inline void bs_flush_read( bs_transrate_t *s )
{
    int i = s->i_bit_in & 0x7;
    if( i )
    {
        assert(((unsigned int)bs->i_bit_in_cache) >> (32 - i) == 0);
        s->i_bit_in_cache <<= i;
        s->i_bit_in -= i;
    }
    s->p_c += -1 * (s->i_bit_in >> 3);
    s->i_bit_in = 0;
}
static inline void bs_flush_write( bs_transrate_t *s )
{
    if( s->i_bit_out != 8 ) bs_write(s, 0, s->i_bit_out);
}

/////---- begin ext mpeg code

const uint8_t non_linear_mquant_table[32] =
{
    0, 1, 2, 3, 4, 5, 6, 7,
    8,10,12,14,16,18,20,22,
    24,28,32,36,40,44,48,52,
    56,64,72,80,88,96,104,112
};
const uint8_t map_non_linear_mquant[113] =
{
    0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,
    16,17,17,17,18,18,18,18,19,19,19,19,20,20,20,20,21,21,21,21,22,22,
    22,22,23,23,23,23,24,24,24,24,24,24,24,25,25,25,25,25,25,25,26,26,
    26,26,26,26,26,26,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,29,
    29,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,31,31,31,31,31
};

static int scale_quant( unsigned int q_scale_type, double quant )
{
    int iquant;
    if (q_scale_type)
    {
        iquant = (int) floor(quant+0.5);

        /* clip mquant to legal (linear) range */
        if (iquant<1) iquant = 1;
        if (iquant>112) iquant = 112;

        iquant = non_linear_mquant_table[map_non_linear_mquant[iquant]];
    }
    else
    {
        /* clip mquant to legal (linear) range */
        iquant = (int)floor(quant+0.5);
        if (iquant<2) iquant = 2;
        if (iquant>62) iquant = 62;
        iquant = (iquant/2)*2; // Must be *even*
    }
    return iquant;
}

static int increment_quant( transrate_t *tr, int quant )
{
    if( tr->q_scale_type)
    {
        assert(quant >= 1 && quant <= 112);
        quant = map_non_linear_mquant[quant] + 1;
        if( tr->quant_corr < -60.0f) quant++;
        if( quant > 31) quant = 31;
        quant = non_linear_mquant_table[quant];
    }
    else
    {
        assert(!(quant & 1));
        quant += 2;
        if( tr->quant_corr < -60.0f) quant += 2;
        if (quant > 62) quant = 62;
    }
    return quant;
}

static inline int intmax( register int x, register int y )
{
    return x < y ? y : x;
}
static inline int intmin( register int x, register int y )
{
    return x < y ? x : y;
}

static int getNewQuant( transrate_t *tr, int curQuant)
{
    bs_transrate_t *bs = &tr->bs;

    double calc_quant, quant_to_use;
    int mquant = 0;

    tr->quant_corr = (((bs->i_byte_in - (bs->p_r - 4 - bs->p_c)) / tr->fact_x) - (bs->i_byte_out + (bs->p_w - bs->p_ow))) / REACT_DELAY;
    calc_quant = curQuant * tr->fact_x;
    quant_to_use = calc_quant - tr->quant_corr;

    switch (tr->picture_coding_type )
    {
        case I_TYPE:
        case P_TYPE:
            mquant = increment_quant( tr, curQuant);
            break;

        case B_TYPE:
            mquant = intmax(scale_quant( tr->q_scale_type, quant_to_use), increment_quant( tr, curQuant));
            break;

        default:
            assert(0);
            break;
    }

    /*
        LOGF("type: %s orig_quant: %3i calc_quant: %7.1f quant_corr: %7.1f using_quant: %3i\n",
        (picture_coding_type == I_TYPE ? "I_TYPE" : (picture_coding_type == P_TYPE ? "P_TYPE" : "B_TYPE")),
        (int)curQuant, (float)calc_quant, (float)quant_corr, (int)mquant);
    */

    assert(mquant >= curQuant);

    return mquant;
}

static inline int isNotEmpty(RunLevel *blk)
{
    return (blk->level);
}

#include "putvlc.h"

static void putAC( bs_transrate_t *bs, int run, int signed_level, int vlcformat)
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
    if (run==0 && (val==1 || val==-1)) bs_write( bs, 2|(val<0),2);
    else putAC( bs, run,val,0);
}

static void putnonintrablk( bs_transrate_t *bs, RunLevel *blk)
{
    assert(blk->level);

    putACfirst( bs, blk->run, blk->level);
    blk++;

    while(blk->level)
    {
        putAC( bs, blk->run, blk->level, 0);
        blk++;
    }

    bs_write( bs, 2,2);
}

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

    switch( tr->picture_coding_type)
    {
        case I_TYPE:

            tab = MB_I + UBITS (bs->i_bit_in_cache, 1);
            bs_flush( bs, tab->len );
            macroblock_modes = tab->modes;

            if ((! ( tr->frame_pred_frame_dct)) && ( tr->picture_structure == FRAME_PICTURE))
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

        if (f_code) delta += UBITS (bs->i_bit_in_cache, f_code);
        bs_copy( bs, f_code);

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

static inline int get_luma_dc_dct_diff( bs_transrate_t *bs )
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
            bs_copy( bs, tab->len);
            //dc_diff = UBITS (bs->i_bit_in_cache, size) - UBITS (SBITS (~bs->i_bit_in_cache, 1), size);
            dc_diff = UBITS (bs->i_bit_in_cache, size);
            if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
            bs_copy( bs, size);
            return dc_diff;
        }
        else
        {
            bs_copy( bs, 3);
            return 0;
        }
    }
    else
    {
        tab = DC_long + (UBITS (bs->i_bit_in_cache, 9) - 0x1e0);
        size = tab->size;
        bs_copy( bs, tab->len);
        //dc_diff = UBITS (bs->i_bit_in_cache, size) - UBITS (SBITS (~bs->i_bit_in_cache, 1), size);
        dc_diff = UBITS (bs->i_bit_in_cache, size);
        if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
        bs_copy( bs, size);
        return dc_diff;
    }
}

static inline int get_chroma_dc_dct_diff( bs_transrate_t *bs )
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
            bs_copy( bs, tab->len);
            //dc_diff = UBITS (bs->i_bit_in_cache, size) - UBITS (SBITS (~bs->i_bit_in_cache, 1), size);
            dc_diff = UBITS (bs->i_bit_in_cache, size);
            if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
            bs_copy( bs, size);
            return dc_diff;
        } else
        {
            bs_copy( bs, 2);
            return 0;
        }
    }
    else
    {
        tab = DC_long + (UBITS (bs->i_bit_in_cache, 10) - 0x3e0);
        size = tab->size;
        bs_copy( bs, tab->len + 1);
        //dc_diff = UBITS (bs->i_bit_in_cache, size) - UBITS (SBITS (~bs->i_bit_in_cache, 1), size);
        dc_diff = UBITS (bs->i_bit_in_cache, size);
        if (!(dc_diff >> (size - 1))) dc_diff = (dc_diff + 1) - (1 << size);
        bs_copy( bs, size);
        return dc_diff;
    }
}

static void get_intra_block_B14( bs_transrate_t *bs, const int i_qscale, const int i_qscale_new )
{
    int tst;
    int i, li;
    int val;
    const DCTtab * tab;

    /* Basic sanity check --Meuuh */
    if( i_qscale == 0 )
    {
        return;
    }

    tst = i_qscale_new/i_qscale + ((i_qscale_new%i_qscale) ? 1 : 0);

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
            if (val >= tst)
            {
                val = (val ^ SBITS (bs->i_bit_in_cache, 1)) - SBITS (bs->i_bit_in_cache, 1);
                putAC( bs, i - li - 1, (val * i_qscale) / i_qscale_new, 0);
                li = i;
            }

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
            if (abs(val) >= tst)
            {
                putAC( bs, i - li - 1, (val * i_qscale) / i_qscale_new, 0);
                li = i;
            }

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
            bs_flush( bs, 16);
            i += tab->run;
            if (i < 64) goto normal_code;
        }
        break;  /* illegal, check needed to avoid buffer overflow */
    }

    bs_copy( bs, 2);    /* end of block code */
}

static void get_intra_block_B15( bs_transrate_t *bs,  const int i_qscale, int const i_qscale_new )
{
    int tst;
    int i, li;
    int val;
    const DCTtab * tab;

    /* Basic sanity check --Meuuh */
    if( i_qscale == 0 )
    {
        return;
    }

    tst = i_qscale_new/i_qscale + ((i_qscale_new%i_qscale) ? 1 : 0);

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
                if (val >= tst)
                {
                    val = (val ^ SBITS (bs->i_bit_in_cache, 1)) - SBITS (bs->i_bit_in_cache, 1);
                    putAC( bs, i - li - 1, (val * i_qscale) / i_qscale_new, 1);
                    li = i;
                }

                bs_flush( bs, 1 );
                continue;
            }
            else
            {
                i += (UBITS (bs->i_bit_in_cache, 12) & 0x3F) - 64;

                if (i >= 64) break; /* illegal, check against buffer overflow */

                bs_flush( bs, 12 );
                val = SBITS (bs->i_bit_in_cache, 12);
                if (abs(val) >= tst)
                {
                    putAC( bs, i - li - 1, (val * i_qscale) / i_qscale_new, 1);
                    li = i;
                }

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
        break;  /* illegal, check needed to avoid buffer overflow */
    }

    bs_copy( bs, 4);    /* end of block code */
}


static int get_non_intra_block_drop( transrate_t *tr, RunLevel *blk)
{
    bs_transrate_t *bs = &tr->bs;

    int i, li;
    int val;
    const DCTtab * tab;
    RunLevel *sblk = blk + 1;

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
            if (i >= 64) break; /* end of block */

    normal_code:

            bs_flush( bs, tab->len );
            val = tab->level;
            val = (val ^ SBITS (bs->i_bit_in_cache, 1)) - SBITS (bs->i_bit_in_cache, 1); /* if (bitstream_get (1)) val = -val; */

            blk->level = val;
            blk->run = i - li - 1;
            li = i;
            blk++;

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
        break;  /* illegal, check needed to avoid buffer overflow */
    }
    bs_flush( bs, 2 ); /* dump end of block code */

    // remove last coeff
    if (blk != sblk)
    {
        blk--;
        // remove more coeffs if very late
        if ((tr->quant_corr < -60.0f) && (blk != sblk))
        {
            blk--;
            if ((tr->quant_corr < -80.0f) && (blk != sblk))
            {
                blk--;
                if ((tr->quant_corr < -100.0f) && (blk != sblk))
                {
                    blk--;
                    if ((tr->quant_corr < -120.0f) && (blk != sblk))
                        blk--;
                }
            }
        }
    }

    blk->level = 0;

    return i;
}

static int get_non_intra_block_rq( bs_transrate_t *bs, RunLevel *blk,  const int i_qscale, const int i_qscale_new )
{
    int tst;
    int i, li;
    int val;
    const DCTtab * tab;

    /* Basic sanity check --Meuuh */
    if( i_qscale == 0 )
    {
        return 0;
    }

    tst = i_qscale_new/i_qscale + ((i_qscale_new%i_qscale) ? 1 : 0);

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
            if (val >= tst)
            {
                val = (val ^ SBITS (bs->i_bit_in_cache, 1)) - SBITS (bs->i_bit_in_cache, 1);
                blk->level = (val * i_qscale) / i_qscale_new;
                blk->run = i - li - 1;
                li = i;
                blk++;
            }

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
            if (abs(val) >= tst)
            {
                blk->level = (val * i_qscale) / i_qscale_new;
                blk->run = i - li - 1;
                li = i;
                blk++;
            }

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
        break;  /* illegal, check needed to avoid buffer overflow */
    }
    bs_flush( bs, 2 );    /* dump end of block code */

    blk->level = 0;

    return i;
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

    switch ( tr->picture_coding_type)
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

static inline void put_quantiser( transrate_t *tr )
{
    bs_transrate_t *bs = &tr->bs;

    bs_write( bs, tr->q_scale_type ? map_non_linear_mquant[tr->new_quantizer_scale] : tr->new_quantizer_scale >> 1, 5);
    tr->last_coded_scale = tr->new_quantizer_scale;
}

static int slice_init( transrate_t *tr,  int code)
{
    bs_transrate_t *bs = &tr->bs;
    int offset;
    const MBAtab * mba;

    tr->v_offset = (code - 1) * 16;

    tr->quantizer_scale = get_quantizer_scale( tr );
    if ( tr->picture_coding_type == P_TYPE)
    {
        tr->new_quantizer_scale = tr->quantizer_scale;
    }
    else
    {
        tr->new_quantizer_scale = getNewQuant(tr, tr->quantizer_scale);
    }
    put_quantiser( tr );

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
            bs_copy( bs, 11);
        }
        else
        {
            return -1;
        }
    }

    bs_copy( bs, mba->len + 1);
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
    return 0;
}

static void mpeg2_slice( transrate_t *tr, const int code )
{
    bs_transrate_t *bs = &tr->bs;

    if( slice_init( tr, code ) )
    {
        return;
    }

    for( ;; )
    {
        int macroblock_modes;
        int mba_inc;
        const MBAtab * mba;

        macroblock_modes = get_macroblock_modes( tr );
        if (macroblock_modes & MACROBLOCK_QUANT) tr->quantizer_scale = get_quantizer_scale( tr );

        //LOGF("blk %i : ", h_offset >> 4);

        if (macroblock_modes & MACROBLOCK_INTRA)
        {
            //LOG("intra "); if (macroblock_modes & MACROBLOCK_QUANT) LOGF("got new quant: %i ", quantizer_scale);

            tr->new_quantizer_scale = increment_quant( tr, tr->quantizer_scale);
            if (tr->last_coded_scale == tr->new_quantizer_scale) macroblock_modes &= 0xFFFFFFEF; // remove MACROBLOCK_QUANT
            else macroblock_modes |= MACROBLOCK_QUANT; //add MACROBLOCK_QUANT
            putmbdata( tr, macroblock_modes);
            if (macroblock_modes & MACROBLOCK_QUANT) put_quantiser( tr );

            //if (macroblock_modes & MACROBLOCK_QUANT) LOGF("put new quant: %i ", new_quantizer_scale);

            if (tr->concealment_motion_vectors)
            {
                if (tr->picture_structure != FRAME_PICTURE)
                {
                    bs_copy( bs, 1); /* remove field_select */
                }
                /* like motion_frame, but parsing without actual motion compensation */
                get_motion_delta( bs, tr->f_code[0][0]);
                get_motion_delta( bs, tr->f_code[0][1]);

                bs_copy( bs, 1); /* remove marker_bit */
            }

            if( tr->intra_vlc_format )
            {
                /* Luma */
                get_luma_dc_dct_diff( bs );     get_intra_block_B15( bs, tr->quantizer_scale, tr->new_quantizer_scale );
                get_luma_dc_dct_diff( bs );     get_intra_block_B15( bs, tr->quantizer_scale, tr->new_quantizer_scale );
                get_luma_dc_dct_diff( bs );     get_intra_block_B15( bs, tr->quantizer_scale, tr->new_quantizer_scale );
                get_luma_dc_dct_diff( bs );     get_intra_block_B15( bs, tr->quantizer_scale, tr->new_quantizer_scale );
                /* Chroma */
                get_chroma_dc_dct_diff( bs );   get_intra_block_B15( bs, tr->quantizer_scale, tr->new_quantizer_scale );
                get_chroma_dc_dct_diff( bs );   get_intra_block_B15( bs, tr->quantizer_scale, tr->new_quantizer_scale );
            }
            else
            {
                /* Luma */
                get_luma_dc_dct_diff( bs );     get_intra_block_B14( bs, tr->quantizer_scale, tr->new_quantizer_scale );
                get_luma_dc_dct_diff( bs );     get_intra_block_B14( bs, tr->quantizer_scale, tr->new_quantizer_scale );
                get_luma_dc_dct_diff( bs );     get_intra_block_B14( bs, tr->quantizer_scale, tr->new_quantizer_scale );
                get_luma_dc_dct_diff( bs );     get_intra_block_B14( bs, tr->quantizer_scale, tr->new_quantizer_scale );
                /* Chroma */
                get_chroma_dc_dct_diff( bs );   get_intra_block_B14( bs, tr->quantizer_scale, tr->new_quantizer_scale );
                get_chroma_dc_dct_diff( bs );   get_intra_block_B14( bs, tr->quantizer_scale, tr->new_quantizer_scale );
            }
        }
        else
        {
            RunLevel block[6][65]; // terminated by level = 0, so we need 64+1
            int new_coded_block_pattern = 0;

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

            if ( tr->picture_coding_type == P_TYPE) tr->new_quantizer_scale = tr->quantizer_scale;
            else tr->new_quantizer_scale = getNewQuant( tr, tr->quantizer_scale);

            //LOG("non intra "); if (macroblock_modes & MACROBLOCK_QUANT) LOGF("got new quant: %i ", quantizer_scale);

            if (macroblock_modes & MACROBLOCK_PATTERN)
            {
                const int cbp = get_coded_block_pattern( bs );

                if( tr->picture_coding_type == P_TYPE )
                {
                    if( cbp&0x20 ) get_non_intra_block_drop( tr, block[0] );
                    if( cbp&0x10 ) get_non_intra_block_drop( tr, block[1] );
                    if( cbp&0x08 ) get_non_intra_block_drop( tr, block[2] );
                    if( cbp&0x04 ) get_non_intra_block_drop( tr, block[3] );
                    if( cbp&0x02 ) get_non_intra_block_drop( tr, block[4] );
                    if( cbp&0x01 ) get_non_intra_block_drop( tr, block[5] );

                    new_coded_block_pattern = cbp;
                }
                else
                {
                    if( cbp&0x20 )
                    {
                        get_non_intra_block_rq( bs, block[0], tr->quantizer_scale, tr->new_quantizer_scale );
                        if( isNotEmpty( block[0] ) ) new_coded_block_pattern |= 0x20;
                    }
                    if( cbp&0x10 )
                    {
                        get_non_intra_block_rq( bs, block[1], tr->quantizer_scale, tr->new_quantizer_scale );
                        if( isNotEmpty( block[1] ) ) new_coded_block_pattern |= 0x10;
                    }
                    if( cbp&0x08 )
                    {
                        get_non_intra_block_rq( bs, block[2], tr->quantizer_scale, tr->new_quantizer_scale );
                        if( isNotEmpty( block[2] ) ) new_coded_block_pattern |= 0x08;
                    }
                    if( cbp&0x04 )
                    {
                        get_non_intra_block_rq( bs, block[3], tr->quantizer_scale, tr->new_quantizer_scale );
                        if( isNotEmpty( block[3] ) ) new_coded_block_pattern |= 0x04;
                    }
                    if( cbp&0x02 )
                    {
                        get_non_intra_block_rq( bs, block[4], tr->quantizer_scale, tr->new_quantizer_scale );
                        if( isNotEmpty( block[4] ) ) new_coded_block_pattern |= 0x02;
                    }
                    if( cbp&0x01 )
                    {
                        get_non_intra_block_rq( bs, block[5], tr->quantizer_scale, tr->new_quantizer_scale );
                        if( isNotEmpty( block[5] ) ) new_coded_block_pattern |= 0x01;
                    }
                    if( !new_coded_block_pattern) macroblock_modes &= 0xFFFFFFED; // remove MACROBLOCK_PATTERN and MACROBLOCK_QUANT flag
                }
            }

            if (tr->last_coded_scale == tr->new_quantizer_scale) macroblock_modes &= 0xFFFFFFEF; // remove MACROBLOCK_QUANT
            else if (macroblock_modes & MACROBLOCK_PATTERN) macroblock_modes |= MACROBLOCK_QUANT; //add MACROBLOCK_QUANT
            assert( (macroblock_modes & MACROBLOCK_PATTERN) || !(macroblock_modes & MACROBLOCK_QUANT) );

            putmbdata( tr, macroblock_modes);
            if( macroblock_modes & MACROBLOCK_QUANT )
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
                bs_write( bs, cbptable[new_coded_block_pattern].code,cbptable[new_coded_block_pattern].len);

                if (new_coded_block_pattern & 0x20) putnonintrablk( bs, block[0]);
                if (new_coded_block_pattern & 0x10) putnonintrablk( bs, block[1]);
                if (new_coded_block_pattern & 0x08) putnonintrablk( bs, block[2]);
                if (new_coded_block_pattern & 0x04) putnonintrablk( bs, block[3]);
                if (new_coded_block_pattern & 0x02) putnonintrablk( bs, block[4]);
                if (new_coded_block_pattern & 0x01) putnonintrablk( bs, block[5]);
            }
        }

        //LOGF("\n\to: %i c: %i n: %i\n", quantizer_scale, last_coded_scale, new_quantizer_scale);

        NEXT_MACROBLOCK;

        mba_inc = 0;
        for( ;; )
        {
            if (bs->i_bit_in_cache >= 0x10000000)
            {
                mba = MBA_5 + (UBITS (bs->i_bit_in_cache, 5) - 2);
                break;
            }
            else if (bs->i_bit_in_cache >= 0x03000000)
            {
                mba = MBA_11 + (UBITS (bs->i_bit_in_cache, 11) - 24);
                break;
            }
            else if( UBITS (bs->i_bit_in_cache, 11 ) == 8 )
            {
                /* macroblock_escape */
                mba_inc += 33;
                bs_copy( bs, 11);
            }
            else
            {
                /* EOS or error */
                return;
            }
        }
        bs_copy( bs, mba->len);
        mba_inc += mba->mba;

        while( mba_inc-- )
        {
            NEXT_MACROBLOCK;
        }
    }
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
        tr->horizontal_size_value = (bs->p_c[0] << 4) | (bs->p_c[1] >> 4);
        tr->vertical_size_value = ((bs->p_c[1] & 0xF) << 8) | bs->p_c[2];
        if(!tr->horizontal_size_value || !tr->vertical_size_value )
        {
            return -1;
        }

        memcpy(bs->p_w, bs->p_c, 8 );
        bs->p_c += 8;
        bs->p_w += 8;
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
            /* tr->alternate_scan = (bs->p_c[3] >> 2) & 0x1; */


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

        if( ( tr->picture_coding_type == B_TYPE && tr->quant_corr <  2.5f ) || // don't recompress if we're in advance!
            ( tr->picture_coding_type == P_TYPE && tr->quant_corr < -2.5f ) ||
            ( tr->picture_coding_type == I_TYPE && tr->quant_corr < -5.0f ) )
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

                // in this case, we'll just use the original slice !
                memcpy(outTemp, inTemp, bs->p_c - inTemp);
                bs->p_w = outTemp + (bs->p_c - inTemp);

                // adjust bs->i_byte_out
                bs->i_byte_out -= (bs->p_w - outTemp) - (bs->p_c - inTemp);
            }
        }
    }
    return 0;
}

static void process_frame( sout_stream_t *p_stream,
                           sout_stream_id_t *id, sout_buffer_t *in, sout_buffer_t **out )
{
    transrate_t *tr = &id->tr;
    bs_transrate_t *bs = &tr->bs;

    sout_buffer_t       *p_out;

    /* The output buffer can't be bigger than the input buffer. */
    p_out = sout_BufferNew( p_stream->p_sout, in->i_size );

    p_out->i_length = in->i_length;
    p_out->i_dts    = in->i_dts;
    p_out->i_pts    = in->i_pts;

    sout_BufferChain( out, p_out );

    bs->p_rw = bs->p_ow = bs->p_w = p_out->p_buffer;
    bs->p_c = bs->p_r = in->p_buffer;
    bs->p_r += in->i_size + 4;
    bs->p_rw += in->i_size;
    *(in->p_buffer + in->i_size) = 0;
    *(in->p_buffer + in->i_size + 1) = 0;
    *(in->p_buffer + in->i_size + 2) = 1;
    *(in->p_buffer + in->i_size + 3) = 0;
    bs->i_byte_in += in->i_size;

    for ( ; ; )
    {
        uint8_t *p_end = &in->p_buffer[in->i_size];

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

            if( bs->p_c >= p_end)
            {
                break;
            }
        }

        if( bs->p_c >= p_end )
        {
            break;
        }

        /* Copy the start code */
        memcpy(bs->p_w, bs->p_c, 3 );
        bs->p_c += 3;
        bs->p_w += 3;

        if (do_next_start_code( tr ) )
        {
            /* Error */
            break;
        }

        tr->quant_corr = (((bs->i_byte_in - (bs->p_r - 4 - bs->p_c)) / tr->fact_x) - (bs->i_byte_out + (bs->p_w - bs->p_ow))) / REACT_DELAY;
    }

    bs->i_byte_out += bs->p_w - bs->p_ow;
    p_out->i_size = bs->p_w - bs->p_ow;
}

static int transrate_video_process( sout_stream_t *p_stream,
               sout_stream_id_t *id, sout_buffer_t *in, sout_buffer_t **out )
{
    transrate_t    *tr = &id->tr;
    bs_transrate_t *bs = &tr->bs;
    vlc_bool_t     b_gop = VLC_FALSE;

    *out = NULL;

    if( GetDWBE( in->p_buffer ) != 0x100 )
    {
        uint8_t *p = in->p_buffer;
        uint8_t *p_end = &in->p_buffer[in->i_buffer];
        uint32_t code = GetDWBE( p );

        /* We may have a GOP */
        while( p < p_end - 4 )
        {
            if( code == 0x1b8 )
            {
                b_gop = VLC_TRUE;
                break;
            }
            else if( code == 0x100 )
            {
                break;
            }
            code = ( code << 8 )|p[4];
            p++;
        }
    }


    if( b_gop )
    {
        while ( id->p_current_buffer != NULL )
        {
            sout_buffer_t * p_next = id->p_current_buffer->p_next;
            if ( tr->fact_x == 1.0 )
            {
                bs->i_byte_out += id->p_current_buffer->i_size;
                id->p_current_buffer->p_next = NULL;
                sout_BufferChain( out, id->p_current_buffer );
            }
            else
            {
                process_frame( p_stream, id, id->p_current_buffer, out );
                sout_BufferDelete(p_stream->p_sout, id->p_current_buffer);
            }
            id->p_current_buffer = p_next;
        }

        if ( id->i_next_gop_duration )
        {
            mtime_t i_bitrate = (mtime_t)id->i_next_gop_size * 8000
                                    / (id->i_next_gop_duration / 1000);
            static mtime_t i_old_bitrate = 0;
            static mtime_t i_old_duration = 0;
            if (i_old_bitrate)
            {
                msg_Dbg(p_stream, "bitrate = %lld -> %lld", i_old_bitrate,
                        (mtime_t)bs->i_byte_out * 8000 / (i_old_duration / 1000));
            }
            i_old_bitrate = i_bitrate;
            i_old_duration = id->i_next_gop_duration;
            if ( i_bitrate > p_stream->p_sys->i_vbitrate )
            {
                tr->fact_x = (double)i_bitrate / p_stream->p_sys->i_vbitrate;
            }
            else
            {
                tr->fact_x = 1.0;
            }
            msg_Dbg(p_stream, "new fact_x = %f", tr->fact_x);

            id->p_current_buffer = id->p_next_gop;
            id->p_next_gop = NULL;
            id->i_next_gop_duration = 0;
            id->i_next_gop_size = 0;
            bs->i_byte_in = 0;
            bs->i_byte_out  = 0;
        }
    }

    /* Store the buffer for the next GOP. */
    sout_BufferChain( &id->p_next_gop, in );
    id->i_next_gop_duration += in->i_length;
    id->i_next_gop_size += in->i_size;

    if ( id->p_current_buffer != NULL )
    {
        sout_buffer_t * p_next = id->p_current_buffer->p_next;
        if ( tr->fact_x == 1.0 )
        {
            bs->i_byte_out += id->p_current_buffer->i_size;
            id->p_current_buffer->p_next = NULL;
            sout_BufferChain( out, id->p_current_buffer );
        }
        else
        {
            process_frame( p_stream, id, id->p_current_buffer, out );
            sout_BufferDelete(p_stream->p_sout, id->p_current_buffer);
        }
        id->p_current_buffer = p_next;
    }

    return VLC_SUCCESS;
}

