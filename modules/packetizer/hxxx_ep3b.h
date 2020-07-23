/*****************************************************************************
 * hxxx_ep3b.h
 *****************************************************************************
 * Copyright (C) 2014-2015 VLC authors and VideoLAN
 *                    2018 VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include <vlc_bits.h>

static inline uint8_t *hxxx_ep3b_to_rbsp( uint8_t *p, uint8_t *end, unsigned *pi_prev, size_t i_count )
{
    for( size_t i=0; i<i_count; i++ )
    {
        if( ++p >= end )
            return p;

        *pi_prev = (*pi_prev << 1) | (!*p);

        if( *p == 0x03 &&
           ( p + 1 ) != end ) /* Never escape sequence if no next byte */
        {
            if( (*pi_prev & 0x06) == 0x06 )
            {
                ++p;
                *pi_prev = !*p;
            }
        }
    }
    return p;
}

#if 0
/* Discards emulation prevention three bytes */
static inline uint8_t * hxxx_ep3b_to_rbsp(const uint8_t *p_src, size_t i_src, size_t *pi_ret)
{
    uint8_t *p_dst;
    if(!p_src || !(p_dst = malloc(i_src)))
        return NULL;

    size_t j = 0;
    for (size_t i = 0; i < i_src; i++) {
        if (i < i_src - 3 &&
            p_src[i] == 0 && p_src[i+1] == 0 && p_src[i+2] == 3) {
            p_dst[j++] = 0;
            p_dst[j++] = 0;
            i += 2;
            continue;
        }
        p_dst[j++] = p_src[i];
    }
    *pi_ret = j;
    return p_dst;
}
#endif

/* vlc_bits's bs_t forward callback for stripping emulation prevention three bytes */
struct hxxx_bsfw_ep3b_ctx_s
{
    unsigned i_prev;
    size_t i_bytepos;
};

static void hxxx_bsfw_ep3b_ctx_init( struct hxxx_bsfw_ep3b_ctx_s *ctx )
{
    ctx->i_prev = 0;
    ctx->i_bytepos = 0;
}

static size_t hxxx_bsfw_byte_forward_ep3b( bs_t *s, size_t i_count )
{
    struct hxxx_bsfw_ep3b_ctx_s *ctx = (struct hxxx_bsfw_ep3b_ctx_s *) s->p_priv;
    if( s->p == NULL )
    {
        s->p = s->p_start;
        ctx->i_bytepos = 1;
        return 1;
    }

    if( s->p >= s->p_end )
        return 0;

    s->p = hxxx_ep3b_to_rbsp( s->p, s->p_end, &ctx->i_prev, i_count );
    ctx->i_bytepos += i_count;
    return i_count;
}

static size_t hxxx_bsfw_byte_pos_ep3b( const bs_t *s )
{
    struct hxxx_bsfw_ep3b_ctx_s *ctx = (struct hxxx_bsfw_ep3b_ctx_s *) s->p_priv;
    return ctx->i_bytepos;
}

static const bs_byte_callbacks_t hxxx_bsfw_ep3b_callbacks =
{
    hxxx_bsfw_byte_forward_ep3b,
    hxxx_bsfw_byte_pos_ep3b,
};
