/*****************************************************************************
 * transrate.h: MPEG2 video transrating module
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

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
    uint8_t intra_quantizer_matrix [64];
    uint8_t non_intra_quantizer_matrix [64];
    int mpeg4_matrix;

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
    const uint8_t * scan;

    // slice or mb
    // quantizer_scale_code
    unsigned int quantizer_scale;
    unsigned int new_quantizer_scale;
    unsigned int last_coded_scale;
    int   h_offset, v_offset;
    bool b_error;

    // mb
    double qrate;
    int i_admissible_error, i_minimum_error;

    /* input buffers */
    ssize_t i_total_input, i_remaining_input;
    /* output buffers */
    ssize_t i_current_output, i_wanted_output;
} transrate_t;


struct sout_stream_id_t
{
    void            *id;
    bool      b_transrate;

    block_t         *p_current_buffer;
    block_t           *p_next_gop;
    mtime_t         i_next_gop_duration;
    size_t          i_next_gop_size;

    transrate_t     tr;
};


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

static inline void bs_write( bs_transrate_t *s, unsigned int val, int n )
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

static inline unsigned int bs_read( bs_transrate_t *s, unsigned int n )
{
    unsigned int Val = ((unsigned int)s->i_bit_in_cache) >> (32 - n);
    bs_flush( s, n );
    return Val;
}

static inline unsigned int bs_copy( bs_transrate_t *s, unsigned int n )
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
        assert(((unsigned int)s->i_bit_in_cache) >> (32 - i) == 0);
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

int scale_quant( transrate_t *tr, double qrate );
int transrate_mb( transrate_t *tr, RunLevel blk[6][65], RunLevel new_blk[6][65], int i_cbp, int intra );
void get_intra_block_B14( transrate_t *tr, RunLevel *blk );
void get_intra_block_B15( transrate_t *tr, RunLevel *blk );
int get_non_intra_block( transrate_t *tr, RunLevel *blk );
void putnonintrablk( bs_transrate_t *bs, RunLevel *blk);
void putintrablk( bs_transrate_t *bs, RunLevel *blk, int vlcformat);

int process_frame( sout_stream_t *p_stream, sout_stream_id_t *id,
                   block_t *in, block_t **out, int i_handicap );
