/*****************************************************************************
 * helpers.c : Generic helper functions for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 *
 * Author: Juha Jeronen <juha.jeronen@jyu.fi>
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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#ifdef CAN_COMPILE_MMXEXT
#   include "mmx.h"
#   include <stdalign.h>
#endif

#include <stdint.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_cpu.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

#include "deinterlace.h" /* definition of p_sys, needed for Merge() */
#include "common.h"      /* FFMIN3 et al. */
#include "merge.h"

#include "helpers.h"

/*****************************************************************************
 * Internal functions
 *****************************************************************************/

/**
 * This internal function converts a normal (full frame) plane_t into a
 * field plane_t.
 *
 * Field plane_t's can be used e.g. for a weaving copy operation from two
 * source frames into one destination frame.
 *
 * The pixels themselves will not be touched; only the metadata is generated.
 * The same pixel data is shared by both the original plane_t and the field
 * plane_t. Note, however, that the bottom field's data starts from the
 * second line, so for the bottom field, the actual pixel pointer value
 * does not exactly match the original plane pixel pointer value. (It points
 * one line further down.)
 *
 * The caller must allocate p_dst (creating a local variable is fine).
 *
 * @param p_dst Field plane_t is written here. Must be non-NULL.
 * @param p_src Original full-frame plane_t. Must be non-NULL.
 * @param i_field Extract which field? 0 = top field, 1 = bottom field.
 * @see plane_CopyPixels()
 * @see ComposeFrame()
 * @see RenderPhosphor()
 */
static void FieldFromPlane( plane_t *p_dst, const plane_t *p_src, int i_field )
{
    assert( p_dst != NULL );
    assert( p_src != NULL );
    assert( i_field == 0  ||  i_field == 1 );

    /* Start with a copy of the metadata, and then update it to refer
       to one field only.

       We utilize the fact that plane_CopyPixels() differentiates between
       visible_pitch and pitch.

       The other field will be defined as the "margin" by doubling the pitch.
       The visible pitch will be left as in the original.
    */
    (*p_dst) = (*p_src);
    p_dst->i_lines /= 2;
    p_dst->i_visible_lines /= 2;
    p_dst->i_pitch *= 2;
    /* For the bottom field, skip the first line in the pixel data. */
    if( i_field == 1 )
        p_dst->p_pixels += p_src->i_pitch;
}

#define T 10
/**
 * Internal helper function for EstimateNumBlocksWithMotion():
 * estimates whether there is motion in the given 8x8 block on one plane
 * between two images. The block as a whole and its fields are evaluated
 * separately, and use different motion thresholds.
 *
 * This is a low-level function only used by EstimateNumBlocksWithMotion().
 * There is no need to call this function manually.
 *
 * For interpretation of pi_top and pi_bot, it is assumed that the block
 * starts on an even-numbered line (belonging to the top field).
 *
 * The b_mmx parameter avoids the need to call vlc_CPU() separately
 * for each block.
 *
 * @param[in] p_pix_p Base pointer to the block in previous picture
 * @param[in] p_pix_c Base pointer to the same block in current picture
 * @param i_pitch_prev i_pitch of previous picture
 * @param i_pitch_curr i_pitch of current picture
 * @param[out] pi_top 1 if top field of the block had motion, 0 if no
 * @param[out] pi_bot 1 if bottom field of the block had motion, 0 if no
 * @return 1 if the block had motion, 0 if no
 * @see EstimateNumBlocksWithMotion()
 */
static int TestForMotionInBlock( uint8_t *p_pix_p, uint8_t *p_pix_c,
                                 int i_pitch_prev, int i_pitch_curr,
                                 int* pi_top, int* pi_bot )
{
/* Pixel luma/chroma difference threshold to detect motion. */

    int32_t i_motion = 0;
    int32_t i_top_motion = 0;
    int32_t i_bot_motion = 0;

    for( int y = 0; y < 8; ++y )
    {
        uint8_t *pc = p_pix_c;
        uint8_t *pp = p_pix_p;
        int score = 0;
        for( int x = 0; x < 8; ++x )
        {
            int_fast16_t C = abs((*pc) - (*pp));
            if( C > T )
                ++score;

            ++pc;
            ++pp;
        }

        i_motion += score;
        if( y % 2 == 0 )
            i_top_motion += score;
        else
            i_bot_motion += score;

        p_pix_c += i_pitch_curr;
        p_pix_p += i_pitch_prev;
    }

    /* Field motion thresholds.

       Empirical value - works better in practice than the "4" that
       would be consistent with the full-block threshold.

       Especially the opening scene of The Third ep. 1 (just after the OP)
       works better with this. It also fixes some talking scenes in
       Stellvia ep. 1, where the cadence would otherwise catch on incorrectly,
       leading to more interlacing artifacts than by just using the emergency
       mode frame composer.
    */
    (*pi_top) = ( i_top_motion >= 8 );
    (*pi_bot) = ( i_bot_motion >= 8 );

    /* Full-block threshold = (8*8)/8: motion is detected if 1/8 of the block
       changes "enough". */
    return (i_motion >= 8);
}

#ifdef CAN_COMPILE_MMXEXT
VLC_MMX
static int TestForMotionInBlockMMX( uint8_t *p_pix_p, uint8_t *p_pix_c,
                                    int i_pitch_prev, int i_pitch_curr,
                                    int* pi_top, int* pi_bot )
{
    int32_t i_motion = 0;
    int32_t i_top_motion = 0;
    int32_t i_bot_motion = 0;

    static alignas (8) const mmx_t bT   = { .ub = { T, T, T, T, T, T, T, T } };
    pxor_r2r( mm6, mm6 ); /* zero, used in psadbw */
    movq_m2r( bT,  mm5 );

    pxor_r2r( mm3, mm3 ); /* score (top field) */
    pxor_r2r( mm4, mm4 ); /* score (bottom field) */
    for( int y = 0; y < 8; y+=2 )
    {
        /* top field */
        movq_m2r( *((uint64_t*)p_pix_c), mm0 );
        movq_m2r( *((uint64_t*)p_pix_p), mm1 );
        movq_r2r( mm0, mm2 );
        psubusb_r2r( mm1, mm2 );
        psubusb_r2r( mm0, mm1 );

        pcmpgtb_r2r( mm5, mm2 );
        pcmpgtb_r2r( mm5, mm1 );
        psadbw_r2r(  mm6, mm2 );
        psadbw_r2r(  mm6, mm1 );

        paddd_r2r( mm2, mm1 );
        paddd_r2r( mm1, mm3 ); /* add to top field score */

        p_pix_c += i_pitch_curr;
        p_pix_p += i_pitch_prev;

        /* bottom field - handling identical to top field, except... */
        movq_m2r( *((uint64_t*)p_pix_c), mm0 );
        movq_m2r( *((uint64_t*)p_pix_p), mm1 );
        movq_r2r( mm0, mm2 );
        psubusb_r2r( mm1, mm2 );
        psubusb_r2r( mm0, mm1 );

        pcmpgtb_r2r( mm5, mm2 );
        pcmpgtb_r2r( mm5, mm1 );
        psadbw_r2r(  mm6, mm2 );
        psadbw_r2r(  mm6, mm1 );

        paddd_r2r( mm2, mm1 );
        paddd_r2r( mm1, mm4 ); /* ...here we add to bottom field score */

        p_pix_c += i_pitch_curr;
        p_pix_p += i_pitch_prev;
    }
    movq_r2r(  mm3, mm7 ); /* score (total) */
    paddd_r2r( mm4, mm7 );
    movd_r2m( mm3, i_top_motion );
    movd_r2m( mm4, i_bot_motion );
    movd_r2m( mm7, i_motion );

    /* The loop counts actual score * 255. */
    i_top_motion /= 255;
    i_bot_motion /= 255;
    i_motion     /= 255;

    emms();

    (*pi_top) = ( i_top_motion >= 8 );
    (*pi_bot) = ( i_bot_motion >= 8 );
    return (i_motion >= 8);
}
#endif
#undef T

/*****************************************************************************
 * Public functions
 *****************************************************************************/

/* See header for function doc. */
void ComposeFrame( filter_t *p_filter,
                   picture_t *p_outpic,
                   picture_t *p_inpic_top, picture_t *p_inpic_bottom,
                   compose_chroma_t i_output_chroma, bool swapped_uv_conversion )
{
    assert( p_outpic != NULL );
    assert( p_inpic_top != NULL );
    assert( p_inpic_bottom != NULL );

    /* Valid 4:2:0 chroma handling modes. */
    assert( i_output_chroma == CC_ALTLINE       ||
            i_output_chroma == CC_UPCONVERT     ||
            i_output_chroma == CC_SOURCE_TOP    ||
            i_output_chroma == CC_SOURCE_BOTTOM ||
            i_output_chroma == CC_MERGE );

    filter_sys_t *p_sys = p_filter->p_sys;

    const bool b_upconvert_chroma = i_output_chroma == CC_UPCONVERT;

    for( int i_plane = 0 ; i_plane < p_inpic_top->i_planes ; i_plane++ )
    {
        bool b_is_chroma_plane = ( i_plane == U_PLANE || i_plane == V_PLANE );

        int i_out_plane;
        if( b_is_chroma_plane  &&  b_upconvert_chroma  && swapped_uv_conversion )
        {
            if( i_plane == U_PLANE )
                i_out_plane = V_PLANE;
            else /* V_PLANE */
                i_out_plane = U_PLANE;
        }
        else
        {
            i_out_plane = i_plane;
        }

        /* Copy luma or chroma, alternating between input fields. */
        if( !b_is_chroma_plane  ||  i_output_chroma == CC_ALTLINE )
        {
            /* Do an alternating line copy. This is always done for luma,
               and for 4:2:2 chroma. It can be requested for 4:2:0 chroma
               using CC_ALTLINE (see function doc).

               Note that when we get here, the number of lines matches
               in input and output.
            */
            plane_t dst_top;
            plane_t dst_bottom;
            plane_t src_top;
            plane_t src_bottom;
            FieldFromPlane( &dst_top,    &p_outpic->p[i_out_plane],   0 );
            FieldFromPlane( &dst_bottom, &p_outpic->p[i_out_plane],   1 );
            FieldFromPlane( &src_top,    &p_inpic_top->p[i_plane],    0 );
            FieldFromPlane( &src_bottom, &p_inpic_bottom->p[i_plane], 1 );

            /* Copy each field from the corresponding source. */
            plane_CopyPixels( &dst_top,    &src_top    );
            plane_CopyPixels( &dst_bottom, &src_bottom );
        }
        else /* Input 4:2:0, on a chroma plane, and not in altline mode. */
        {
            if( i_output_chroma == CC_UPCONVERT )
            {
                /* Upconverting copy - use all data from both input fields.

                   This produces an output picture with independent chroma
                   for each field. It can be used for general input when
                   the two input frames are different.

                   The output is 4:2:2, but the input is 4:2:0. Thus the output
                   has twice the lines of the input, and each full chroma plane
                   in the input corresponds to a field chroma plane in the
                   output.
                */
                plane_t dst_top;
                plane_t dst_bottom;
                FieldFromPlane( &dst_top,    &p_outpic->p[i_out_plane], 0 );
                FieldFromPlane( &dst_bottom, &p_outpic->p[i_out_plane], 1 );

                /* Copy each field from the corresponding source. */
                plane_CopyPixels( &dst_top,    &p_inpic_top->p[i_plane]    );
                plane_CopyPixels( &dst_bottom, &p_inpic_bottom->p[i_plane] );
            }
            else if( i_output_chroma == CC_SOURCE_TOP )
            {
                /* Copy chroma of input top field. Ignore chroma of input
                   bottom field. Input and output are both 4:2:0, so we just
                   copy the whole plane. */
                plane_CopyPixels( &p_outpic->p[i_out_plane],
                                  &p_inpic_top->p[i_plane] );
            }
            else if( i_output_chroma == CC_SOURCE_BOTTOM )
            {
                /* Copy chroma of input bottom field. Ignore chroma of input
                   top field. Input and output are both 4:2:0, so we just
                   copy the whole plane. */
                plane_CopyPixels( &p_outpic->p[i_out_plane],
                                  &p_inpic_bottom->p[i_plane] );
            }
            else /* i_output_chroma == CC_MERGE */
            {
                /* Average the chroma of the input fields.
                   Input and output are both 4:2:0. */
                uint8_t *p_in_top, *p_in_bottom, *p_out_end, *p_out;
                p_in_top    = p_inpic_top->p[i_plane].p_pixels;
                p_in_bottom = p_inpic_bottom->p[i_plane].p_pixels;
                p_out = p_outpic->p[i_out_plane].p_pixels;
                p_out_end = p_out + p_outpic->p[i_out_plane].i_pitch
                                  * p_outpic->p[i_out_plane].i_visible_lines;

                int w = FFMIN3( p_inpic_top->p[i_plane].i_visible_pitch,
                                p_inpic_bottom->p[i_plane].i_visible_pitch,
                                p_outpic->p[i_plane].i_visible_pitch );

                for( ; p_out < p_out_end ; )
                {
                    Merge( p_out, p_in_top, p_in_bottom, w );
                    p_out       += p_outpic->p[i_out_plane].i_pitch;
                    p_in_top    += p_inpic_top->p[i_plane].i_pitch;
                    p_in_bottom += p_inpic_bottom->p[i_plane].i_pitch;
                }
                EndMerge();
            }
        }
    }
}

/* See header for function doc. */
int EstimateNumBlocksWithMotion( const picture_t* p_prev,
                                 const picture_t* p_curr,
                                 int *pi_top, int *pi_bot)
{
    assert( p_prev != NULL );
    assert( p_curr != NULL );

    int i_score_top = 0;
    int i_score_bot = 0;

    if( p_prev->i_planes != p_curr->i_planes )
        return -1;

    int (*motion_in_block)(uint8_t *, uint8_t *, int , int, int *, int *) =
        TestForMotionInBlock;
    /* We must tell our inline helper whether to use MMX acceleration. */
#ifdef CAN_COMPILE_MMXEXT
    if (vlc_CPU_MMXEXT())
        motion_in_block = TestForMotionInBlockMMX;
#endif

    int i_score = 0;
    for( int i_plane = 0 ; i_plane < p_prev->i_planes ; i_plane++ )
    {
        /* Sanity check */
        if( p_prev->p[i_plane].i_visible_lines !=
            p_curr->p[i_plane].i_visible_lines )
            return -1;

        const int i_pitch_prev = p_prev->p[i_plane].i_pitch;
        const int i_pitch_curr = p_curr->p[i_plane].i_pitch;

        /* Last pixels and lines (which do not make whole blocks) are ignored.
           Shouldn't really matter for our purposes. */
        const int i_mby = p_prev->p[i_plane].i_visible_lines / 8;
        const int w = FFMIN( p_prev->p[i_plane].i_visible_pitch,
                             p_curr->p[i_plane].i_visible_pitch );
        const int i_mbx = w / 8;

        for( int by = 0; by < i_mby; ++by )
        {
            uint8_t *p_pix_p = &p_prev->p[i_plane].p_pixels[i_pitch_prev*8*by];
            uint8_t *p_pix_c = &p_curr->p[i_plane].p_pixels[i_pitch_curr*8*by];

            for( int bx = 0; bx < i_mbx; ++bx )
            {
                int i_top_temp, i_bot_temp;
                i_score += motion_in_block( p_pix_p, p_pix_c,
                                            i_pitch_prev, i_pitch_curr,
                                            &i_top_temp, &i_bot_temp );
                i_score_top += i_top_temp;
                i_score_bot += i_bot_temp;

                p_pix_p += 8;
                p_pix_c += 8;
            }
        }
    }

    if( pi_top )
        (*pi_top) = i_score_top;
    if( pi_bot )
        (*pi_bot) = i_score_bot;

    return i_score;
}

/* Threshold (value from Transcode 1.1.5) */
#define T 100

#ifdef CAN_COMPILE_MMXEXT
VLC_MMX
static int CalculateInterlaceScoreMMX( const picture_t* p_pic_top,
                                       const picture_t* p_pic_bot )
{
    assert( p_pic_top->i_planes == p_pic_bot->i_planes );

    /* Amount of bits must be known for MMX, thus int32_t.
       Doesn't hurt the C implementation. */
    int32_t i_score_mmx = 0; /* this must be divided by 255 when finished  */
    int32_t i_score_c   = 0; /* this counts as-is (used for non-MMX parts) */

    pxor_r2r( mm7, mm7 ); /* we will keep score in mm7 */

    for( int i_plane = 0 ; i_plane < p_pic_top->i_planes ; ++i_plane )
    {
        /* Sanity check */
        if( p_pic_top->p[i_plane].i_visible_lines !=
            p_pic_bot->p[i_plane].i_visible_lines )
            return -1;

        const int i_lasty = p_pic_top->p[i_plane].i_visible_lines-1;
        const int w = FFMIN( p_pic_top->p[i_plane].i_visible_pitch,
                             p_pic_bot->p[i_plane].i_visible_pitch );
        const int wm8 = w % 8;   /* remainder */
        const int w8  = w - wm8; /* part of width that is divisible by 8 */

        /* Current line / neighbouring lines picture pointers */
        const picture_t *cur = p_pic_bot;
        const picture_t *ngh = p_pic_top;
        int wc = cur->p[i_plane].i_pitch;
        int wn = ngh->p[i_plane].i_pitch;

        /* Transcode 1.1.5 only checks every other line. Checking every line
           works better for anime, which may contain horizontal,
           one pixel thick cartoon outlines.
        */
        for( int y = 1; y < i_lasty; ++y )
        {
            uint8_t *p_c = &cur->p[i_plane].p_pixels[y*wc];     /* this line */
            uint8_t *p_p = &ngh->p[i_plane].p_pixels[(y-1)*wn]; /* prev line */
            uint8_t *p_n = &ngh->p[i_plane].p_pixels[(y+1)*wn]; /* next line */

            int x = 0;

            /* Easy-to-read C version further below.

               Assumptions: 0 < T < 127
                            # of pixels < (2^32)/255
               Note: calculates score * 255
            */
            static alignas (8) const mmx_t b0 = {
                .uq = 0x0000000000000000ULL };
            static alignas (8) const mmx_t b128 = {
                .uq = 0x8080808080808080ULL };
            static alignas (8) const mmx_t bT = {
                .ub = { T, T, T, T, T, T, T, T } };

            for( ; x < w8; x += 8 )
            {
                movq_m2r( *((int64_t*)p_c), mm0 );
                movq_m2r( *((int64_t*)p_p), mm1 );
                movq_m2r( *((int64_t*)p_n), mm2 );

                psubb_m2r( b128, mm0 );
                psubb_m2r( b128, mm1 );
                psubb_m2r( b128, mm2 );

                psubsb_r2r( mm0, mm1 );
                psubsb_r2r( mm0, mm2 );

                pxor_r2r( mm3, mm3 );
                pxor_r2r( mm4, mm4 );
                pxor_r2r( mm5, mm5 );
                pxor_r2r( mm6, mm6 );

                punpcklbw_r2r( mm1, mm3 );
                punpcklbw_r2r( mm2, mm4 );
                punpckhbw_r2r( mm1, mm5 );
                punpckhbw_r2r( mm2, mm6 );

                pmulhw_r2r( mm3, mm4 );
                pmulhw_r2r( mm5, mm6 );

                packsswb_r2r(mm4, mm6);
                pcmpgtb_m2r( bT, mm6 );
                psadbw_m2r( b0, mm6 );
                paddd_r2r( mm6, mm7 );

                p_c += 8;
                p_p += 8;
                p_n += 8;
            }

            for( ; x < w; ++x )
            {
                /* Worst case: need 17 bits for "comb". */
                int_fast32_t C = *p_c;
                int_fast32_t P = *p_p;
                int_fast32_t N = *p_n;

                /* Comments in Transcode's filter_ivtc.c attribute this
                   combing metric to Gunnar Thalin.

                    The idea is that if the picture is interlaced, both
                    expressions will have the same sign, and this comes
                    up positive. The value T = 100 has been chosen such
                    that a pixel difference of 10 (on average) will
                    trigger the detector.
                */
                int_fast32_t comb = (P - C) * (N - C);
                if( comb > T )
                    ++i_score_c;

                ++p_c;
                ++p_p;
                ++p_n;
            }

            /* Now the other field - swap current and neighbour pictures */
            const picture_t *tmp = cur;
            cur = ngh;
            ngh = tmp;
            int tmp_pitch = wc;
            wc = wn;
            wn = tmp_pitch;
        }
    }

    movd_r2m( mm7, i_score_mmx );
    emms();

    return i_score_mmx/255 + i_score_c;
}
#endif

/* See header for function doc. */
int CalculateInterlaceScore( const picture_t* p_pic_top,
                             const picture_t* p_pic_bot )
{
    /*
        We use the comb metric from the IVTC filter of Transcode 1.1.5.
        This was found to work better for the particular purpose of IVTC
        than RenderX()'s comb metric.

        Note that we *must not* subsample at all in order to catch interlacing
        in telecined frames with localized motion (e.g. anime with characters
        talking, where only mouths move and everything else stays still.)
    */

    assert( p_pic_top != NULL );
    assert( p_pic_bot != NULL );

    if( p_pic_top->i_planes != p_pic_bot->i_planes )
        return -1;

#ifdef CAN_COMPILE_MMXEXT
    if (vlc_CPU_MMXEXT())
        return CalculateInterlaceScoreMMX( p_pic_top, p_pic_bot );
#endif

    int32_t i_score = 0;

    for( int i_plane = 0 ; i_plane < p_pic_top->i_planes ; ++i_plane )
    {
        /* Sanity check */
        if( p_pic_top->p[i_plane].i_visible_lines !=
            p_pic_bot->p[i_plane].i_visible_lines )
            return -1;

        const int i_lasty = p_pic_top->p[i_plane].i_visible_lines-1;
        const int w = FFMIN( p_pic_top->p[i_plane].i_visible_pitch,
                             p_pic_bot->p[i_plane].i_visible_pitch );

        /* Current line / neighbouring lines picture pointers */
        const picture_t *cur = p_pic_bot;
        const picture_t *ngh = p_pic_top;
        int wc = cur->p[i_plane].i_pitch;
        int wn = ngh->p[i_plane].i_pitch;

        /* Transcode 1.1.5 only checks every other line. Checking every line
           works better for anime, which may contain horizontal,
           one pixel thick cartoon outlines.
        */
        for( int y = 1; y < i_lasty; ++y )
        {
            uint8_t *p_c = &cur->p[i_plane].p_pixels[y*wc];     /* this line */
            uint8_t *p_p = &ngh->p[i_plane].p_pixels[(y-1)*wn]; /* prev line */
            uint8_t *p_n = &ngh->p[i_plane].p_pixels[(y+1)*wn]; /* next line */

            for( int x = 0; x < w; ++x )
            {
                /* Worst case: need 17 bits for "comb". */
                int_fast32_t C = *p_c;
                int_fast32_t P = *p_p;
                int_fast32_t N = *p_n;

                /* Comments in Transcode's filter_ivtc.c attribute this
                   combing metric to Gunnar Thalin.

                    The idea is that if the picture is interlaced, both
                    expressions will have the same sign, and this comes
                    up positive. The value T = 100 has been chosen such
                    that a pixel difference of 10 (on average) will
                    trigger the detector.
                */
                int_fast32_t comb = (P - C) * (N - C);
                if( comb > T )
                    ++i_score;

                ++p_c;
                ++p_p;
                ++p_n;
            }

            /* Now the other field - swap current and neighbour pictures */
            const picture_t *tmp = cur;
            cur = ngh;
            ngh = tmp;
            int tmp_pitch = wc;
            wc = wn;
            wn = tmp_pitch;
        }
    }

    return i_score;
}
#undef T
