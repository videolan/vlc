/*****************************************************************************
 * postprocessing_mmxext.c: Post Processing plugin MMXEXT
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: postprocessing_mmxext.c,v 1.5 2002/12/18 14:17:10 sam Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <vlc/vlc.h> /* only use uint8_t, uint32_t .... */

#include "postprocessing.h"
#include "postprocessing_common.h"

/*****************************************************************************
 *
 * Internals functions common to pp_Deblock_V and pp_Deblock_H
 *
 *****************************************************************************/

/*****************************************************************************
 * MMX stuff
 *****************************************************************************/


/* XXX PP_THR1 need to be defined as ULL */

/* Use same things as in idct but how it work ? */
#define UNUSED_LONGLONG( foo ) \
    static const unsigned long long foo __asm__ (#foo)  __attribute__((unused))

/* to calculate isDC_mode for mmx */
UNUSED_LONGLONG( mmx_thr1 ) = ( PP_THR1 << 56 )|
                              ( PP_THR1 << 48 )|
                              ( PP_THR1 << 40 )|
                              ( PP_THR1 << 32 )|
                              ( PP_THR1 << 24 )|
                              ( PP_THR1 << 16 )|
                              ( PP_THR1 <<  8 )|
                              ( PP_THR1 );

UNUSED_LONGLONG( mmx_127_thr1 ) = ( ( 127ULL - PP_THR1 ) << 56 )|
                                  ( ( 127ULL - PP_THR1 ) << 48 )|
                                  ( ( 127ULL - PP_THR1 ) << 40 )|
                                  ( ( 127ULL - PP_THR1 ) << 32 )|
                                  ( ( 127ULL - PP_THR1 ) << 24 )|
                                  ( ( 127ULL - PP_THR1 ) << 16 )|
                                  ( ( 127ULL - PP_THR1 ) <<  8 )|
                                  ( ( 127ULL - PP_THR1 ) );

UNUSED_LONGLONG( mmx_127_2xthr1_1 ) = ( ( 127ULL - PP_2xTHR1 -1) << 56 )|
                                    ( ( 127ULL - PP_2xTHR1 -1 ) << 48 )|
                                    ( ( 127ULL - PP_2xTHR1 -1 ) << 40 )|
                                    ( ( 127ULL - PP_2xTHR1 -1 ) << 32 )|
                                    ( ( 127ULL - PP_2xTHR1 -1 ) << 24 )|
                                    ( ( 127ULL - PP_2xTHR1 -1 ) << 16 )|
                                    ( ( 127ULL - PP_2xTHR1 -1 ) <<  8 )|
                                    ( ( 127ULL - PP_2xTHR1 -1 ) );

UNUSED_LONGLONG( mmx_m2_5_m5_2 ) = 0xfffe0005fffb0002ULL;


/* find min bytes from r ans set it in r, t is destroyed */
#define MMXEXT_GET_PMIN( r, t ) \
   "movq      " #r ",     " #t "                                \n" \
   "psrlq       $8,       " #t "                                \n" \
   "pminub    " #t ",     " #r "                                \n" \
   "pshufw $0xf5, " #r ", " #t " #instead of shift with tmp reg \n" \
   "pminub    " #t ",     " #r "                                \n" \
   "pshufw $0xfe, " #r ", " #t "                                \n" \
   "pminub    " #t ",     " #r "                                \n"

 /* find mzx bytes from r ans set it in r, t is destroyed */
#define MMXEXT_GET_PMAX( r, t ) \
   "movq      " #r ",     " #t "                                \n" \
   "psrlq       $8,       " #t "                                \n" \
   "pmaxub    " #t ",     " #r "                                \n" \
   "pshufw $0xf5, " #r ", " #t "                                \n" \
   "pmaxub    " #t ",     " #r "                                \n" \
   "pshufw $0xfe, " #r ", " #t "                                \n" \
   "pmaxub    " #t ",     " #r "                                \n"



#define MMXEXT_GET_LMINMAX( s, m, M, t ) \
    "movq   " #s ",        " #t "   \n" \
    "pminub " #t ",        " #m "   \n" \
    "pmaxub " #t ",        " #M "   \n"

/* Some tips for MMX

    * |a-b| :
        d1 = a - b with unsigned saturate
        d2 = b - a  with ...
        |a-b| = d1 | d2


*/

/****************************************************************************
 * pp_deblock_isDC_mode : Check if we will use DC mode or Default mode
 ****************************************************************************
 * Use constant PP_THR1 and PP_THR2 ( PP_2xTHR1 )
 *
 * Called for for each pixel on a boundary block when doing deblocking
 *  so need to be fast ...
 *
 ****************************************************************************/
static inline int pp_deblock_isDC_mode( uint8_t *p_v )
{
    unsigned int i_eq_cnt;


    /* algo :
       x = v[i] - v[i+1] without signed saturation
        ( XXX see if there is'nt problem, but can't be with signed
        sat because pixel will be saturate :(
       so x within [-128, 127] and we have to test if it fit in [-M, M]
       we add 127-M with wrap around -> good value fit in [ 127-2*M, 127]
       and if x >= 127 - 2 * M ie x > 127 -2*M - 1 value is good
    */
#if 0
    __asm__ __volatile__ (
   "                                #* Do (v0-v1) to (v7-v8)            \n"
   "movq      (%1),         %%mm1   #  load v0->v7                      \n"
   "movq      1(%1),        %%mm2   #  load v1->v8                      \n"
   "psubb    %%mm2,         %%mm1   #  v[i]-v[i+1]                      \n"
   "paddb     mmx_127_thr1, %%mm1   #  + 127-THR1 with wrap             \n"
   "pcmpgtb   mmx_127_2xthr1_1, %%mm1 #  >  127 -2*thr1 - 1             \n"
   "pxor      %%mm0,        %%mm0   # mm0 = 0                           \n"
   "psadbw    %%mm1,        %%mm0                                       \n"
   "movd      %%mm0,        %0      #                                   \n"
   "negl      %0                                                        \n"
   "andl      $255,         %0"

       : "=r"(i_eq_cnt) : "r" (p_v) );
#endif
     __asm__ __volatile__ (
   "                                #* Do (v0-v1) to (v7-v8)            \n"
   "movq      (%1),         %%mm1   #  load v0->v7                      \n"
   "pxor      %%mm0,        %%mm0   # mm0 = 0                           \n"
   "movq      1(%1),        %%mm2   #  load v1->v8                      \n"
   "psubb    %%mm2,         %%mm1   #  v[i]-v[i+1]                      \n"
   "paddb     mmx_127_thr1, %%mm1   #  + 127-THR1 with wrap             \n"
   "pcmpgtb   mmx_127_2xthr1_1, %%mm1 #  >  127 -2*thr1 - 1             \n"
   "psadbw    %%mm1,        %%mm0                                       \n"
   "movd      %%mm0,        %0      #                                   \n"
   "negl      %0"

       : "=r"(i_eq_cnt) : "r" (p_v) );

    /* last test, hey, 9 don't fit in MMX */
    if((  ( p_v[8] - p_v[9] + PP_THR1 )&0xffff )<= PP_2xTHR1 )
    {
         i_eq_cnt++;
    }

#if 0
    /* algo :  if ( | v[i] -v[i+1] | <= PP_THR1 ) { i_eq_cnt++; } */
    i_eq_cnt = 0;

    for( i =0; i < 9; i++ )
    {
        if((  ( p_v[i] - p_v[i+1] + PP_THR1 )&0xffff )<= PP_2xTHR1 )
        {
            i_eq_cnt++;
        }
    }
#endif

    return( (i_eq_cnt >= PP_THR2 ) ? 1 : 0 );
}

static inline int pp_deblock_isMinMaxOk( uint8_t *p_v, int i_QP )
{
    int i_range;

    __asm__ __volatile__ (
   "movq        1(%1),      %%mm0   # 8 bytes                   \n"
   "movq        %%mm0,      %%mm1                               \n"
    MMXEXT_GET_PMIN( %%mm0, %%mm7 )
    MMXEXT_GET_PMAX( %%mm1, %%mm7 )
   "psubd       %%mm0,      %%mm1   # max - min                 \n"
   "movd        %%mm1,      %0                                  \n"
   "andl        $255,       %0" : "=r"(i_range) : "r"(p_v) );

#if 0
    int i_max, i_min;
    int i;

    i_min = i_max = p_v[1];
    for( i = 2; i < 9; i++ )
    {
        if( i_max < p_v[i] ) i_max = p_v[i];
        if( i_min > p_v[i] ) i_min = p_v[i];
    }
    i_range = i_max - i_min;
#endif

    return( i_range< 2*i_QP ? 1 : 0 );
}


static inline void pp_deblock_DefaultMode( uint8_t i_v[10], int i_stride,
                                      int i_QP )
{
    int d, i_delta;
    int a3x0, a3x0_, a3x1, a3x2;
    int b_neg;

    /* d = CLIP( 5(a3x0' - a3x0)//8, 0, (v4-v5)/2 ).d( abs(a3x0) < QP ) */

    /* First calculate a3x0 */
    __asm__ __volatile__ (
   "pxor    %%mm7,  %%mm7           # mm7 = 0          \n"
   "movq    mmx_m2_5_m5_2, %%mm6    # mm6 =(2,-5,5,-2) \n"
   "movd    3(%1),  %%mm0           \n"
   "punpcklbw %%mm7,%%mm0           \n"
   "pmaddwd %%mm6,  %%mm0           \n"
   "pshufw  $0xfe,  %%mm0, %%mm1    \n"
   "paddd   %%mm1,  %%mm0           \n"
   "movd    %%mm0,  %0" : "=r"(a3x0) :"r"(i_v) );
#if 0
    a3x0 = 2 * ( i_v[3] - i_v[6] ) + 5 *( i_v[5] - i_v[4] );
#endif

    if( a3x0 < 0 )
    {
        b_neg = 1;
        a3x0  = -a3x0;
    }
    else
    {
        b_neg = 0;
    }
    /* XXX Now a3x0 is abs( a3x0 ) */
    if( ( a3x0 < 8 * i_QP )&&( a3x0 != 0 ) ) /* |a3x0| < 8*i_QP */
    {
        /* calculate a3x1 et a3x2 */
        __asm__ __volatile__ (
       "                                # mm7 = 0                   \n"
       "                                # mm6 = ( 2, -5, 5, -2 )    \n"
       "movd    1(%2),  %%mm0           \n"
       "movd    5(%2),  %%mm2           \n"
       "punpcklbw %%mm7,%%mm0           \n"
       "punpcklbw %%mm7,%%mm2           \n"
       "pmaddwd %%mm6,  %%mm0           \n"
       "pmaddwd %%mm6,  %%mm2           \n"
       "pshufw  $0xfe,  %%mm0, %%mm1    \n"
       "paddd   %%mm1,  %%mm0           # mm0 = a3x1    \n"
       "movd    %%mm0,  %0              \n"
       "pshufw  $0xfe,  %%mm2, %%mm1    \n"
       "paddd   %%mm1,  %%mm2           # mm2 = a3x2    \n"
       "movd    %%mm2,  %1              \n"
        : "=r"(a3x1), "=r"(a3x2) : "r"(i_v) );
#if 0
        a3x1 = 2 * ( i_v[1] - i_v[4] ) + 5 * ( i_v[3] - i_v[2] );
        a3x2 = 2 * ( i_v[5] - i_v[8] ) + 5 * ( i_v[7] - i_v[6] );
#endif

        if( a3x1 < 0) a3x1 = -a3x1; /* abs( a3x1 ) */
        if( a3x2 < 0) a3x2 = -a3x2; /* abs( a3x2 ) */

        a3x0_ = PP_MIN3( a3x0, a3x1, a3x2 );

        d = 5 *( a3x0 - a3x0_ ) / 8; /* always > 0 */

        i_delta = ( i_v[4] - i_v[5] ) / 2;
        /* clip into [0, i_delta] or [i_delta, 0] */
        if( i_delta < 0 )
        {
            if( !b_neg ) /* since true d has sgn(d) = - sgn( a3x0 ) */
            {
                d = -d;
                if( d < i_delta ) d = i_delta;
                i_v[4] -= d;
                i_v[5] += d;
            }
        }
        else
        {
            if( b_neg )
            {
                if( d > i_delta ) d = i_delta;
                i_v[4] -= d;
                i_v[5] += d;
            }
        }
    }
}



static inline void pp_deblock_DCMode( uint8_t *p_v, /*  = int i_v[10] */
                                 int i_QP )
{
    int i_p0, i_p9;
    i_p0 = PP_ABS( p_v[1] - p_v[0] ) < i_QP ? p_v[0] : p_v[1];
    i_p9 = PP_ABS( p_v[8] - p_v[9] ) < i_QP ? p_v[9] : p_v[8];

    /* mm0 = 8 pix unmodified
     -We will process first 4 pixel
       mm0 = 8 pix unmodified
       mm1 = for the first part of the 4 first pix
             (v1) -> (p0) -> ... ( word )
             (v2)    (v1)
             (v3)    (v2)
             (v4)    (v3)

           = for the commoin part between first and last pix
             (v2) -> (v3) -> ... ( word )
             (v3)    (v4)
             (v4)    (v5)
             (v5)    (v6)

           = for the last part of the 4 last pix
             (v5) -> (v6) -> ... ( word )
             (v6)    (v7)
             (v7)    (v8)
             (v8)    (p9)

       mm2 = acu for first new pix
       mm3 = acu for last pix
       mm4 = unused
       mm5 = p0
       mm6 = p9 << 48
       mm7 = 0 */
    __asm__ __volatile__ (
   "pxor        %%mm7,      %%mm7   \n"
   "movq        1(%0),      %%mm0   # get 8 pix             \n"
   "                                # unpack into mm1       \n"
   "movq        %%mm0,      %%mm1   \n"
   "punpcklbw   %%mm7,      %%mm1   \n"
   "                                # get p_0 and i_p9      \n"
   "movd        %1,         %%mm5   \n"
   "movd        %2,         %%mm6   \n"
   "psllq       $48,        %%mm6   \n"
   "                                \n"
   "movq        %%mm1,      %%mm3   # p_v[5-8] = v[1-4] !!  \n"
   "movq        %%mm1,      %%mm2   \n"
   "psllw       $2,         %%mm2   # p_v[1-4] = 4*v[1-4]   \n"
   "                                \n"
   "psllq       $16,        %%mm1   \n"
   "por         %%mm5,      %%mm1   # mm1 =( p0, v1, v2 ,v3)\n"
   "                                \n"
   "paddw       %%mm1,      %%mm2   \n"
   "paddw       %%mm1,      %%mm2   \n"
   "                                \n"
   "pshufw      $0x90,%%mm1,%%mm1   # mm1 =( p0, p0, v1, v2)\n"
   "paddw       %%mm1,      %%mm2   \n"
   "paddw       %%mm1,      %%mm2   \n"
   "                                \n"
   "pshufw      $0x90,%%mm1,%%mm1   # mm1 =( p0, p0, p0, v2)\n"
   "paddw       %%mm1,      %%mm2   \n"
   "                                \n"
   "pshufw      $0x90,%%mm1,%%mm1   # mm1 =( p0, p0, p0, p0)\n"
   "paddw       %%mm1,      %%mm2   \n"
   "                                # Now last part a little borring\n"
   "                                # last part for mm2, beginig for mm3\n"
   "movq        %%mm0,      %%mm1   \n"
   "psrlq       $8,         %%mm1   \n"
   "punpcklbw   %%mm7,      %%mm1   # mm1 =( v2, v3, v4, v5 )\n"
   "paddw       %%mm1,      %%mm2   \n"
   "paddw       %%mm1,      %%mm2   \n"
   "paddw       %%mm1,      %%mm3   \n"

   "                                \n"
   "movq        %%mm0,      %%mm1   \n"
   "psrlq       $16,        %%mm1   \n"
   "punpcklbw   %%mm7,      %%mm1   # mm1 =( v3, v4, v5, v6 )\n"
   "psllw       $1,         %%mm1   \n"
   "paddw       %%mm1,      %%mm2   \n"
   "paddw       %%mm1,      %%mm3   \n"
   "                                \n"
   "movq        %%mm0,      %%mm1   \n"
   "psrlq       $24,        %%mm1   \n"
   "punpcklbw   %%mm7,      %%mm1   # mm1 =( v4, v5, v6, v7)    \n"
   "paddw       %%mm1,      %%mm2   \n"
   "paddw       %%mm1,      %%mm3   \n"
   "paddw       %%mm1,      %%mm3   \n"
   "                                \n"
   "movq        %%mm0,      %%mm1   \n"
   "psrlq       $32,        %%mm1   \n"
   "punpcklbw   %%mm7,      %%mm1   # mm1 =( v5, v6, v7, v8)    \n"
   "paddw       %%mm1,      %%mm2   \n"
   "psllw       $2,         %%mm1   \n"
   "paddw       %%mm1,      %%mm3   \n"
   "                                # Now last part for last 4 pix \n"
   "                                # \n"
   "movq        %%mm0,      %%mm1   \n"
   "punpckhbw   %%mm7,      %%mm1   # mm1 = ( v5, v6, v7, v8)      \n"
   "                                \n"
   "psrlq       $16,        %%mm1   \n"
   "por         %%mm6,      %%mm1   # mm1 =( v6, v7, v8, p9 )\n"
   "                                \n"
   "paddw       %%mm1,      %%mm3   \n"
   "paddw       %%mm1,      %%mm3   \n"
   "                                \n"
   "pshufw      $0xf9,%%mm1,%%mm1   # mm1 =( v7, v8, p9, p9)\n"
   "paddw       %%mm1,      %%mm3   \n"
   "paddw       %%mm1,      %%mm3   \n"
   "                                \n"
   "pshufw      $0xf9,%%mm1,%%mm1   # mm1 =( v8, p9, p9, p9)\n"
   "paddw       %%mm1,      %%mm3   \n"
   "                                \n"
   "pshufw      $0xf9,%%mm1,%%mm1   # mm1 =( p9, p9, p9, p9)\n"
   "paddw       %%mm1,      %%mm3   \n"

   "psrlw       $4,         %%mm2   \n"
   "psrlw       $4,         %%mm3   \n"
   "packuswb    %%mm3,      %%mm2   \n"
   "movq        %%mm2,      1(%0)   \n"

    : : "r"(p_v), "r"(i_p0), "r"(i_p9) : "memory" );

#if 0
    for( i = 1; i < 9; i++ )
    {
        v[i] = p_v[i]; /* save 8 pix that will be modified */
    }

    p_v[1] = ( 6 * i_p0                        + 4 * v[1]
                + 2 *( v[2] + v[3]) + v[4] + v[5]) >> 4;

    p_v[2] = ( 4 * i_p0    + 2 * v[1]          + 4 * v[2]
                + 2 *( v[3] + v[4]) + v[5] + v[6]) >> 4;

    p_v[3] = ( 2 * i_p0    + 2 * (v[1] + v[2]) + 4 * v[3]
                + 2 *( v[4] + v[5]) + v[6] + v[7]) >> 4;

    p_v[4] = ( i_p0 + v[1] + 2 * (v[2] + v[3]) + 4 * v[4]
                + 2 *( v[5] + v[6]) + v[7] + v[8]) >> 4;

    p_v[5] = ( v[1] + v[2] + 2 * (v[3] + v[4]) + 4 * v[5]
                + 2 *( v[6] + v[7]) + v[8] + i_p9) >> 4;

    p_v[6] = ( v[2] + v[3] + 2 * (v[4] + v[5]) + 4 * v[6]
            + 2 *( v[7] + v[8]) + 2 * i_p9) >> 4;

    p_v[7] = ( v[3] + v[4] + 2 * (v[5] + v[6]) + 4 * v[7]
                + 2 * v[8] + 4 * i_p9) >> 4;

    p_v[8] = ( v[4] + v[5] + 2 * (v[6] + v[7]) + 4 * v[8]
                                    + 6 * i_p9) >> 4;
#endif

}



/*****************************************************************************/
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*    ---------- filter Vertical lines so follow horizontal edges --------   */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*****************************************************************************/

void E_( pp_deblock_V )( uint8_t *p_plane,
                         int i_width, int i_height, int i_stride,
                         QT_STORE_T *p_QP_store, int i_QP_stride,
                         int b_chroma )
{
    int x, y, i;
    uint8_t *p_v;
    int i_QP_scale; /* use to do ( ? >> i_QP_scale ) */
    int i_QP;

    uint8_t i_v[10];

    i_QP_scale = b_chroma ? 5 : 4 ;

    for( y = 8; y < i_height - 4; y += 8 )
    {
        p_v = p_plane + ( y - 5 )* i_stride;
        for( x = 0; x < i_width; x++ )
        {
            /* First get  10 vert pix to use them without i_stride */
            for( i = 0; i < 10; i++ )
            {
                i_v[i] = p_v[i*i_stride + x];
            }

            i_QP = p_QP_store[(y>>i_QP_scale)*i_QP_stride+
                                (x>>i_QP_scale)];
            /* XXX QP is for v5 */
            if( pp_deblock_isDC_mode( i_v ) )
            {
                if( pp_deblock_isMinMaxOk( i_v, i_QP ) )
                {
                    pp_deblock_DCMode( i_v, i_QP );
                }
            }
            else
            {
                pp_deblock_DefaultMode( i_v, i_stride, i_QP );

            }
            /* Copy back, XXX only 1-8 were modified */
            for( i = 1; i < 9; i++ )
            {
                p_v[i*i_stride + x] = i_v[i];
            }

        }
    }

    return;
}
/*****************************************************************************/
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*     --------- filter Horizontal lines so follow vertical edges --------   */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*****************************************************************************/

void E_( pp_deblock_H )( uint8_t *p_plane,
                         int i_width, int i_height, int i_stride,
                         QT_STORE_T *p_QP_store, int i_QP_stride,
                            int b_chroma )
{
    int x, y;
    uint8_t *p_v;
    int i_QP_scale;
    int i_QP;

    i_QP_scale = b_chroma ? 5 : 4 ;

    for( y = 0; y < i_height; y++ )
    {
        p_v = p_plane + y * i_stride - 5;
        for( x = 8; x < i_width - 4; x += 8 )
        {
            /* p_v point 5 pix before a block boundary */
            /* XXX QP is for v5 */
            i_QP = p_QP_store[(y>>i_QP_scale)*i_QP_stride+
                                 (x>>i_QP_scale)];
            if( pp_deblock_isDC_mode( p_v + x ) )
            {
                if( pp_deblock_isMinMaxOk( p_v+ x, i_QP ) )
                {
                    pp_deblock_DCMode( p_v+x, i_QP );
                }
            }
            else
            {
                pp_deblock_DefaultMode( p_v+x, i_stride, i_QP );
            }
        }
    }

    return;
}


/*****************************************************************************
 *
 * Internals functions common to pp_Dering_Y pp_Dering_C
 *
 *****************************************************************************/

static inline void pp_dering_MinMax( uint8_t *p_block, int i_stride,
                                     int *pi_min, int *pi_max )
{

    /* First we will extract min/max for each pix on vertical line
        and next extract global min/max */
    __asm__ __volatile__(

    "leal   (%2,%3),        %%eax       \n"
    "movq   (%2),           %%mm0 #load line \n"
    "movq   %%mm0,          %%mm1       \n"

    MMXEXT_GET_LMINMAX( (%%eax),        %%mm0, %%mm1, %%mm7 )
    MMXEXT_GET_LMINMAX( (%%eax, %3),    %%mm0, %%mm1, %%mm7 )
    MMXEXT_GET_LMINMAX( (%%eax, %3,2), %%mm0, %%mm1, %%mm7 )
    MMXEXT_GET_LMINMAX( (%2, %3, 4),   %%mm0, %%mm1, %%mm7 )
    "leal   (%%eax,%3,4),  %%eax       \n"

    MMXEXT_GET_LMINMAX( (%%eax),        %%mm0, %%mm1, %%mm7 )
    MMXEXT_GET_LMINMAX( (%%eax, %3),    %%mm0, %%mm1, %%mm7 )
    MMXEXT_GET_LMINMAX( (%%eax, %3,2), %%mm0, %%mm1, %%mm7 )
    MMXEXT_GET_PMIN( %%mm0, %%mm7 )
    MMXEXT_GET_PMAX( %%mm1, %%mm7 )
    "movd   %%mm0,  %%eax               \n"
    "andl   $255,   %%eax               \n"
    "movl   %%eax,  (%0)                \n"
    "movd   %%mm1,  %%eax               \n"
    "andl   $255,   %%eax               \n"
    "movl   %%eax,  (%1)                \n"

     : : "r"(pi_min), "r"(pi_max), "r"(p_block), "r"(i_stride) : "%eax", "memory" );
#if 0

    i_min = 255; i_max = 0;

    for( y = 0; y < 8; y++ )
    {
        for( x = 0; x < 8; x++ )
        {
            if( i_min > p_block[x] ) i_min = p_block[x];
            if( i_max < p_block[x] ) i_max = p_block[x];
        }
        p_block += i_stride;
    }

    *pi_min = i_min;
    *pi_max = i_max;
#endif
}


static inline void pp_dering_BinIndex( uint8_t  *p_block, int i_stride,
                                       int i_thr, uint32_t *p_bin )
{
    int y;
    uint32_t i_bin;

    /* first create mm7 with all bytes set to thr and mm6 = 0 */
    __asm__ __volatile__(
   "movl        %0,     %%eax   \n"
   "movb        %%al,   %%ah    \n"
   "movd        %%eax,  %%mm7   \n"
   "pshufw      $0x00,  %%mm7,  %%mm7   \n"
   "pxor        %%mm6,  %%mm6   \n"
    : : "r"(i_thr) : "%eax" );

    for( y = 0; y < 10; y++ )
    {
        __asm__ __volatile__(
       "movq        (%1),       %%mm0   \n"
       "psubusb     %%mm7,      %%mm0   \n" /* sat makes that x <= thr --> 0 */
       "pcmpeqb     %%mm6,      %%mm0   \n" /* p_block <= i_thr ? -1 : 0 */
       "pmovmskb    %%mm0,      %0      \n" /* i_bin msb of each bytes */
         : "=r"(i_bin) :"r"(p_block) );
        /* Now last 2 tests */
        if( p_block[8] <= i_thr ) i_bin |= 1 << 8;
        if( p_block[9] <= i_thr ) i_bin |= 1 << 9;

        i_bin |= (~i_bin) << 16;  /* for detect three 1 or three 0*/
        *p_bin = ( i_bin >> 1 )&&( i_bin )&&( i_bin << 1 );

        p_block += i_stride;
        p_bin++;
    }

#if 0
    int x, y;
    for( y = 0; y < 10; y++ )
    {
        i_bin = 0;
        for( x = 0; x < 10; x++ )
        {
            if( p_block[x] > i_thr )
            {
                i_bin |= 1 << x;
            }
        }
        i_bin |= (~i_bin) << 16;  /* for detect also three 0 */
        *p_bin = i_bin&( i_bin >> 1 )&( i_bin << 1 );
        *p_bin = i_bin;
        p_block += i_stride;
        p_bin++;
    }
#endif

}

static inline void pp_dering_Filter( uint8_t  *p_block, int i_stride,
                                     uint32_t *p_bin,
                                     int i_QP )
{
    int x, y;
    uint32_t i_bin;
    uint8_t i_flt[8][8];
    int i_f;
    uint8_t *p_sav;
    int i_QP_2;

    p_sav = p_block;
    i_QP_2 = i_QP >> 1;

    for( y = 0; y < 8; y++ )
    {
        i_bin = p_bin[y] & p_bin[y+1] & p_bin[y+2]; /* To be optimised */
        i_bin |= i_bin >> 16; /* detect 0 or 1 */

        for( x = 0; x < 8; x++ )
        {
            if( i_bin&0x02 ) /* 0x02 since 10 index but want 1-9 */
            {
                /* apply dering */
                /* 1 2 1
                   2 4 2   + (8)
                   1 2 1 */
                i_f =   p_block[x - i_stride - 1] +
                      ( p_block[x - i_stride    ] << 1)+
                        p_block[x - i_stride + 1] +

                      ( p_block[x - 1] << 1 )+
                      ( p_block[x    ] << 2 )+
                      ( p_block[x + 1] << 1 )+

                        p_block[x + i_stride - 1] +
                      ( p_block[x + i_stride    ] << 1 ) +
                        p_block[x + i_stride + 1];

                i_flt[y][x] = ( 8 + i_f ) >> 4;
            }
            else
            {
                i_flt[y][x] = p_block[x];
            }

            i_bin >>= 1;

        }
        p_block += i_stride;
    }

    /* Create mm7 with all bytes  set to QP/2 */
    __asm__ __volatile__(
   "movl        %0,     %%eax   \n"
   "shrl        $1,     %%eax   \n" /* i_QP/2 */
   "movb        %%al,   %%ah    \n"
   "movd        %%eax,  %%mm7   \n"
   "pshufw      $0x00,  %%mm7,  %%mm7   \n"
    : : "r"(i_QP) : "%eax" );

    for( y = 0; y < 8; y++ )
    {
        /* clamp those values and copy them */
        __asm__ __volatile__(
       "movq    (%0),   %%mm0   \n" /* mm0 = i_ftl[y][0] ... i_ftl[y][7] */
       "movq    (%1),   %%mm1   \n" /* mm1 = p_sav[0] ... p_sav[7] */
       "movq    %%mm1,  %%mm2   \n"
       "psubusb %%mm7,  %%mm1   \n" /* mm1 = psav - i_QP/2 ( >= 0 ) */
       "paddusb %%mm7,  %%mm2   \n" /* mm2 = psav + i_QP/2 ( <= 255 ) */
       "pmaxub  %%mm1,  %%mm0   \n" /* psav - i_QP/2 <= mm0 */
       "pminub  %%mm2,  %%mm0   \n" /*                  mm0 <= psav + i_QP/2 */
       "movq    %%mm0,  (%1)    \n"
        : :"r"(i_flt[y]), "r"(p_sav) : "memory" );

        p_sav+= i_stride;
    }
}


/*****************************************************************************/
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*     ----------------- Dering filter on Y and C blocks -----------------   */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*****************************************************************************/

void E_( pp_dering_Y )( uint8_t *p_plane,
                        int i_width, int i_height, int i_stride,
                        QT_STORE_T *p_QP_store, int i_QP_stride )
{
    int x, y, k;
    int i_max[4], i_min[4], i_range[4];
    int i_thr[4];
    int i_max_range, i_kmax;
    uint32_t i_bin[4][10];
    uint8_t  *p_block[4];
    QT_STORE_T *p_QP;

    /* We process 4 blocks/loop*/
    for( y = 8; y < i_height-8; y += 16 )
    {
        /* +---+
           |0|1|
           +-+-+   :))
           |2|3|
           +-+-+ */

        p_block[0] = p_plane + y * i_stride + 8;
        p_block[1] = p_block[0] + 8;
        p_block[2] = p_block[0] + ( i_stride << 3 );
        p_block[3] = p_block[2] + 8;

        for( x = 8; x < i_width-8; x += 16 )
        {
            /* 1: Calculate threshold */
            /* Calculate max/min for each block */
            pp_dering_MinMax( p_block[0], i_stride, &i_min[0], &i_max[0] );
            pp_dering_MinMax( p_block[1], i_stride, &i_min[1], &i_max[1] );
            pp_dering_MinMax( p_block[2], i_stride, &i_min[2], &i_max[2] );
            pp_dering_MinMax( p_block[3], i_stride, &i_min[3], &i_max[3] );
            /* Calculate range, max_range and thr */
            i_max_range = 0; i_kmax = 0;
            for( k = 0; k < 4; k++ )
            {
                i_range[k] = i_max[k] - i_min[k];
                i_thr[k] = ( i_max[k] + i_min[k] + 1 )/2;
                if( i_max_range < i_max[k])
                {
                    i_max_range = i_max[k];
                    i_kmax = k;
                }
            }
            /* Now rearrange thr */
            if(  i_max_range > 64 )
            {
                for( k = 1; k < 5; k++ )
                {
                    if( i_range[k] < 16 )
                    {
                        i_thr[k] = 0;
                    }
                    else
                    if( i_range[k] < 32 )
                    {
                        i_thr[k] = i_thr[i_kmax];
                    }
                }
            }
            else
            {
                for( k = 1; k < 5; k++ )
                {
                    if( i_range[k] < 16 )
                    {
                        i_thr[k] = 0;
                    }
                }
            }
            /* 2: Index acquisition 10x10 ! so " -i_stride - 1"*/
            pp_dering_BinIndex( p_block[0] - i_stride - 1, i_stride,
                                i_thr[0], i_bin[0] );
            pp_dering_BinIndex( p_block[1] - i_stride - 1, i_stride,
                                i_thr[1], i_bin[1] );
            pp_dering_BinIndex( p_block[2] - i_stride - 1, i_stride,
                                i_thr[2], i_bin[2] );
            pp_dering_BinIndex( p_block[3] - i_stride - 1, i_stride,
                                i_thr[3], i_bin[3] );


            /* 3: adaptive smoothing */
            /* since we begin at (8,8) QP can be different for each block */
            p_QP = &( p_QP_store[( y >> 4) * i_QP_stride + (x >> 4)] );

            pp_dering_Filter( p_block[0], i_stride,
                              i_bin[0], p_QP[0] );

            pp_dering_Filter( p_block[1], i_stride,
                              i_bin[1], p_QP[1] );

            pp_dering_Filter( p_block[2], i_stride,
                              i_bin[2], p_QP[i_QP_stride] );

            pp_dering_Filter( p_block[3], i_stride,
                              i_bin[3], p_QP[i_QP_stride+1] );

            p_block[0] += 8;
            p_block[1] += 8;
            p_block[2] += 8;
            p_block[3] += 8;
        }
    }
}

void E_( pp_dering_C )( uint8_t *p_plane,
                        int i_width, int i_height, int i_stride,
                        QT_STORE_T *p_QP_store, int i_QP_stride )
{
    int x, y;
    int i_max, i_min;
    int i_thr;
    uint32_t i_bin[10];

    uint8_t *p_block;

    for( y = 8; y < i_height-8; y += 8 )
    {

        p_block = p_plane + y * i_stride + 8;
        for( x = 8; x < i_width-8; x += 8 )
        {

            /* 1: Calculate threshold */
            /* Calculate max/min for each block */
            pp_dering_MinMax( p_block, i_stride,
                              &i_min, &i_max );
            /* Calculate thr*/
            i_thr = ( i_max + i_min + 1 )/2;

            /* 2: Index acquisition 10x10 */
            /* point on 10x10 in wich we have our 8x8 block */
            pp_dering_BinIndex( p_block - i_stride -1, i_stride,
                                i_thr,
                                i_bin );

            /* 3: adaptive smoothing */
            pp_dering_Filter( p_block, i_stride,
                              i_bin,
                              p_QP_store[(y>>5)*i_QP_stride+ (x>>5)]);
            p_block += 8;
        }
    }
}
