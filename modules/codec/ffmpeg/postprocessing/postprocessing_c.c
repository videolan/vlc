/*****************************************************************************
 * postprocessing_c.c: Post Processing plugin in C
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: postprocessing_c.c,v 1.1 2002/08/04 22:13:06 fenrir Exp $
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

#include <vlc/vlc.h> /* only use u8, u32 .... */

#include "postprocessing.h"
#include "postprocessing_common.h"

/*****************************************************************************
 *
 * Internals functions common to pp_deblock_V and pp_deblock_H
 *
 *****************************************************************************/

/****************************************************************************
 * pp_deblock_isDC_mode : Check if we will use DC mode or Default mode
 ****************************************************************************
 * Use constant PP_THR1 and PP_THR2 ( PP_2xTHR1 )
 *
 * Called for for each pixel on a boundary block when doing deblocking
 *  so need to be fast ...
 *
 ****************************************************************************/
static inline int pp_deblock_isDC_mode( u8 *p_v )
{
    int i_eq_cnt;

    /* algo :  if ( | v[i] -v[i+1] | <= PP_THR1 ) { i_eq_cnt++; } */
    i_eq_cnt = 0;
    if((  ( p_v[0] - p_v[1] + PP_THR1 )&0xffff )<= PP_2xTHR1 ) i_eq_cnt++;
    if((  ( p_v[1] - p_v[2] + PP_THR1 )&0xffff )<= PP_2xTHR1 ) i_eq_cnt++;
    if((  ( p_v[2] - p_v[3] + PP_THR1 )&0xffff )<= PP_2xTHR1 ) i_eq_cnt++;
    if((  ( p_v[3] - p_v[4] + PP_THR1 )&0xffff )<= PP_2xTHR1 ) i_eq_cnt++;
    if((  ( p_v[4] - p_v[5] + PP_THR1 )&0xffff )<= PP_2xTHR1 ) i_eq_cnt++;
    if((  ( p_v[5] - p_v[6] + PP_THR1 )&0xffff )<= PP_2xTHR1 ) i_eq_cnt++;
    if((  ( p_v[6] - p_v[7] + PP_THR1 )&0xffff )<= PP_2xTHR1 ) i_eq_cnt++;
    if((  ( p_v[7] - p_v[8] + PP_THR1 )&0xffff )<= PP_2xTHR1 ) i_eq_cnt++;
    if((  ( p_v[8] - p_v[9] + PP_THR1 )&0xffff )<= PP_2xTHR1 ) i_eq_cnt++;

#if 0
    int i;
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

static inline int pp_deblock_isMinMaxOk( u8 *p_v, int i_QP )
{
    int i_max, i_min;

    i_min = i_max = p_v[1];  
    if( i_max < p_v[1] ) i_max = p_v[1];
    if( i_min > p_v[1] ) i_min = p_v[1];
    if( i_max < p_v[2] ) i_max = p_v[2];
    if( i_min > p_v[2] ) i_min = p_v[2];
    if( i_max < p_v[3] ) i_max = p_v[3];
    if( i_min > p_v[3] ) i_min = p_v[3];
    if( i_max < p_v[4] ) i_max = p_v[4];
    if( i_min > p_v[4] ) i_min = p_v[4];
    if( i_max < p_v[5] ) i_max = p_v[5];
    if( i_min > p_v[5] ) i_min = p_v[5];
    if( i_max < p_v[6] ) i_max = p_v[6];
    if( i_min > p_v[6] ) i_min = p_v[6];
    if( i_max < p_v[7] ) i_max = p_v[7];
    if( i_min > p_v[7] ) i_min = p_v[7];
    if( i_max < p_v[8] ) i_max = p_v[8];
    if( i_min > p_v[8] ) i_min = p_v[8];

#if 0
    int i;
    int i_range;
    for( i = 2; i < 9; i++ )
    {
        if( i_max < p_v[i] ) i_max = p_v[i];
        if( i_min > p_v[i] ) i_min = p_v[i];
    }
    i_range = i_max - i_min;
#endif
    return( i_max - i_min < 2*i_QP ? 1 : 0 );
}


static inline void pp_deblock_DefaultMode( u8 i_v[10], int i_stride,
                                      int i_QP )
{
    int d, i_delta;
    int a3x0, a3x0_, a3x1, a3x2;
    int b_neg;

    /* d = CLIP( 5(a3x0' - a3x0)//8, 0, (v4-v5)/2 ).d( abs(a3x0) < QP ) */

    /* First calculate a3x0 */
    a3x0 = 2 * ( i_v[3] - i_v[6] ) + 5 *( i_v[5] - i_v[4] );

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
        a3x1 = 2 * ( i_v[1] - i_v[4] ) + 5 * ( i_v[3] - i_v[2] );
        a3x2 = 2 * ( i_v[5] - i_v[8] ) + 5 * ( i_v[7] - i_v[6] );

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



static inline void pp_deblock_DCMode( u8 *p_v, /*  = int i_v[10] */
                                 int i_QP )
{
    int v[10];

    int i;
    
    int i_p0, i_p9;
    i_p0 = PP_ABS( p_v[1] - p_v[0] ) < i_QP ? p_v[0] : p_v[1];
    i_p9 = PP_ABS( p_v[8] - p_v[9] ) < i_QP ? p_v[9] : p_v[8];

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

}



/*****************************************************************************/
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*    ---------- filter Vertical lines so follow horizontal edges --------   */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/*****************************************************************************/

void E_( pp_deblock_V )( u8 *p_plane, 
                         int i_width, int i_height, int i_stride,
                         QT_STORE_T *p_QP_store, int i_QP_stride,
                         int b_chroma )
{
    int x, y, i;
    u8 *p_v;
    int i_QP_scale; /* use to do ( ? >> i_QP_scale ) */
    int i_QP;
    
    u8 i_v[10];
    
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

void E_( pp_deblock_H )( u8 *p_plane, 
                         int i_width, int i_height, int i_stride,
                         QT_STORE_T *p_QP_store, int i_QP_stride,
                         int b_chroma )
{
    int x, y;
    u8 *p_v;
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

static inline void pp_dering_MinMax( u8 *p_block, int i_stride,
                                     int *pi_min, int *pi_max )
{
    int y;
    int i_min, i_max;

    i_min = 255; i_max = 0;
    
    for( y = 0; y < 8; y++ )
    {
        if( i_min > p_block[0] ) i_min = p_block[0];
        if( i_max < p_block[0] ) i_max = p_block[0];
        if( i_min > p_block[1] ) i_min = p_block[1];
        if( i_max < p_block[1] ) i_max = p_block[1];
        if( i_min > p_block[2] ) i_min = p_block[2];
        if( i_max < p_block[2] ) i_max = p_block[2];
        if( i_min > p_block[3] ) i_min = p_block[3];
        if( i_max < p_block[3] ) i_max = p_block[3];
        if( i_min > p_block[4] ) i_min = p_block[4];
        if( i_max < p_block[4] ) i_max = p_block[4];
        if( i_min > p_block[5] ) i_min = p_block[5];
        if( i_max < p_block[5] ) i_max = p_block[5];
        if( i_min > p_block[6] ) i_min = p_block[6];
        if( i_max < p_block[6] ) i_max = p_block[6];
        if( i_min > p_block[7] ) i_min = p_block[7];
        if( i_max < p_block[7] ) i_max = p_block[7];
#if 0
        int x;
        for( x = 0; x < 8; x++ )
        {
            if( i_min > p_block[x] ) i_min = p_block[x];
            if( i_max < p_block[x] ) i_max = p_block[x];
        }
#endif
        p_block += i_stride;
    }
            
    *pi_min = i_min;
    *pi_max = i_max;
}


static inline void pp_dering_BinIndex( u8  *p_block, int i_stride, int i_thr,
                                       u32 *p_bin )
{
    int x, y;
    u32 i_bin;

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

        p_block += i_stride;
        p_bin++;
    }
}

static inline void pp_dering_Filter( u8  *p_block, int i_stride,
                                     u32 *p_bin,
                                     int i_QP )
{
    int x, y;
    u32 i_bin;
    int i_flt[8][8];
    int i_f;
    u8 *p_sav;
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

                i_f = ( 8 + i_f ) >> 4;

                /* Clamp this value */

                if( i_f - p_block[x] > ( i_QP_2 ) )
                {
                    i_flt[y][x] = p_block[x] + i_QP_2;
                }
                else
                if( i_f - p_block[x] < -i_QP_2 )
                {
                    i_flt[y][x] = p_block[x] - i_QP_2;
                }
                else
                {
                    i_flt[y][x] = i_f ; 
                }
            }
            else
            {
                i_flt[y][x] = p_block[x];
            }
            i_bin >>= 1;
 
        }
        p_block += i_stride;
    }
    for( y = 0; y < 8; y++ )
    {
        for( x = 0; x < 8; x++ )
        {
            p_sav[x] = i_flt[y][x];
        }
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

void E_( pp_dering_Y )( u8 *p_plane, 
                        int i_width, int i_height, int i_stride,
                        QT_STORE_T *p_QP_store, int i_QP_stride )
{
    int x, y, k;
    int i_max[4], i_min[4], i_range[4];
    int i_thr[4];
    int i_max_range, i_kmax;
    u32 i_bin[4][10];
    u8  *p_block[4];
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
            for( k = 0; k <= 4; k++ )
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

void E_( pp_dering_C )( u8 *p_plane, 
                        int i_width, int i_height, int i_stride,
                        QT_STORE_T *p_QP_store, int i_QP_stride )
{
    int x, y;
    int i_max, i_min;
    int i_thr;
    u32 i_bin[10];
   
    u8 *p_block;
    

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
