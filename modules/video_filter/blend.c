/*****************************************************************************
 * blend.c: alpha blend 2 pictures together
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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
#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include "vlc_filter.h"

/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );

static void Blend( filter_t *, picture_t *, picture_t *, picture_t *,
                   int, int );
static void BlendI420( filter_t *, picture_t *, picture_t *, picture_t *,
                       int, int );
static void BlendR16( filter_t *, picture_t *, picture_t *, picture_t *,
                      int, int );
static void BlendR24( filter_t *, picture_t *, picture_t *, picture_t *,
                      int, int );
static void BlendYUY2( filter_t *, picture_t *, picture_t *, picture_t *,
                       int, int );
static void BlendPalette( filter_t *, picture_t *, picture_t *, picture_t *,
                          int, int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Video pictures blending") );
    set_capability( "video blending", 100 );
    set_callbacks( OpenFilter, CloseFilter );
vlc_module_end();

/*****************************************************************************
 * OpenFilter: probe the filter and return score
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    /* Check if we can handle that format.
     * We could try to use a chroma filter if we can't. */
    if( ( p_filter->fmt_in.video.i_chroma != VLC_FOURCC('Y','U','V','A') &&
          p_filter->fmt_in.video.i_chroma != VLC_FOURCC('Y','U','V','P') ) ||
        ( p_filter->fmt_out.video.i_chroma != VLC_FOURCC('I','4','2','0') &&
          p_filter->fmt_out.video.i_chroma != VLC_FOURCC('Y','U','Y','2') &&
          p_filter->fmt_out.video.i_chroma != VLC_FOURCC('Y','V','1','2') &&
          p_filter->fmt_out.video.i_chroma != VLC_FOURCC('R','V','1','6') &&
          p_filter->fmt_out.video.i_chroma != VLC_FOURCC('R','V','2','4') &&
          p_filter->fmt_out.video.i_chroma != VLC_FOURCC('R','V','3','2') ) )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_filter->p_sys = p_sys =
          (filter_sys_t *)malloc(sizeof(filter_sys_t)) ) == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_EGENERIC;
    }

    /* Misc init */
    p_filter->pf_video_blend = Blend;

    msg_Dbg( p_filter, "chroma: %4.4s -> %4.4s",
             (char *)&p_filter->fmt_in.video.i_chroma,
             (char *)&p_filter->fmt_out.video.i_chroma );


    return VLC_SUCCESS;
}

/****************************************************************************
 * Blend: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static void Blend( filter_t *p_filter, picture_t *p_dst,
                   picture_t *p_dst_orig, picture_t *p_src,
                   int i_x_offset, int i_y_offset )
{
    if( p_filter->fmt_in.video.i_chroma == VLC_FOURCC('Y','U','V','A') &&
        ( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('I','4','2','0') ||
          p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','V','1','2') ) )
    {
        BlendI420( p_filter, p_dst, p_dst_orig, p_src,
                   i_x_offset, i_y_offset );
        return;
    }
    if( p_filter->fmt_in.video.i_chroma == VLC_FOURCC('Y','U','V','A') &&
        p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','U','Y','2') )
    {
        BlendYUY2( p_filter, p_dst, p_dst_orig, p_src,
                   i_x_offset, i_y_offset );
        return;
    }
    if( p_filter->fmt_in.video.i_chroma == VLC_FOURCC('Y','U','V','A') &&
        p_filter->fmt_out.video.i_chroma == VLC_FOURCC('R','V','1','6') )
    {
        BlendR16( p_filter, p_dst, p_dst_orig, p_src,
                  i_x_offset, i_y_offset );
        return;
    }
    if( p_filter->fmt_in.video.i_chroma == VLC_FOURCC('Y','U','V','A') &&
        ( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('R','V','2','4') ||
          p_filter->fmt_out.video.i_chroma == VLC_FOURCC('R','V','3','2') ) )
    {
        BlendR24( p_filter, p_dst, p_dst_orig, p_src,
                  i_x_offset, i_y_offset );
        return;
    }
    if( p_filter->fmt_in.video.i_chroma == VLC_FOURCC('Y','U','V','P') &&
        ( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('I','4','2','0') ||
          p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','V','1','2') ) )
    {
        BlendPalette( p_filter, p_dst, p_dst_orig, p_src,
                      i_x_offset, i_y_offset );
        return;
    }

    msg_Dbg( p_filter, "no matching alpha blending routine" );
}

static void BlendI420( filter_t *p_filter, picture_t *p_dst,
                       picture_t *p_dst_orig, picture_t *p_src,
                       int i_x_offset, int i_y_offset )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_src1_y, *p_src2_y, *p_dst_y;
    uint8_t *p_src1_u, *p_src2_u, *p_dst_u;
    uint8_t *p_src1_v, *p_src2_v, *p_dst_v;
    uint8_t *p_trans;
    int i_width, i_height, i_x, i_y;
    vlc_bool_t b_even_scanline = i_y_offset % 2;

    i_dst_pitch = p_dst->p[Y_PLANE].i_pitch;
    p_dst_y = p_dst->p[Y_PLANE].p_pixels + i_x_offset +
              p_filter->fmt_out.video.i_x_offset +
              p_dst->p[Y_PLANE].i_pitch *
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset );
    p_dst_u = p_dst->p[U_PLANE].p_pixels + i_x_offset/2 +
              p_filter->fmt_out.video.i_x_offset/2 +
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
              p_dst->p[U_PLANE].i_pitch;
    p_dst_v = p_dst->p[V_PLANE].p_pixels + i_x_offset/2 +
              p_filter->fmt_out.video.i_x_offset/2 +
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
              p_dst->p[V_PLANE].i_pitch;

    i_src1_pitch = p_dst_orig->p[Y_PLANE].i_pitch;
    p_src1_y = p_dst_orig->p[Y_PLANE].p_pixels + i_x_offset +
               p_filter->fmt_out.video.i_x_offset +
               p_dst_orig->p[Y_PLANE].i_pitch *
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset );
    p_src1_u = p_dst_orig->p[U_PLANE].p_pixels + i_x_offset/2 +
               p_filter->fmt_out.video.i_x_offset/2 +
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
               p_dst_orig->p[U_PLANE].i_pitch;
    p_src1_v = p_dst_orig->p[V_PLANE].p_pixels + i_x_offset/2 +
               p_filter->fmt_out.video.i_x_offset/2 +
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
               p_dst_orig->p[V_PLANE].i_pitch;

    i_src2_pitch = p_src->p[Y_PLANE].i_pitch;
    p_src2_y = p_src->p[Y_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset +
               p_src->p[Y_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;
    p_src2_u = p_src->p[U_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[U_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;
    p_src2_v = p_src->p[V_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[V_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;

    p_trans = p_src->p[A_PLANE].p_pixels +
              p_filter->fmt_in.video.i_x_offset +
              p_src->p[A_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width = __MIN( p_filter->fmt_out.video.i_visible_width - i_x_offset,
                     p_filter->fmt_in.video.i_visible_width );

    i_height = __MIN( p_filter->fmt_out.video.i_visible_height - i_y_offset,
                      p_filter->fmt_in.video.i_visible_height );

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
         p_dst_y += i_dst_pitch, p_src1_y += i_src1_pitch,
         p_src2_y += i_src2_pitch,
         p_dst_u += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src1_u += b_even_scanline ? i_src1_pitch/2 : 0,
         p_src2_u += i_src2_pitch,
         p_dst_v += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src1_v += b_even_scanline ? i_src1_pitch/2 : 0,
         p_src2_v += i_src2_pitch )
    {
        b_even_scanline = !b_even_scanline;

        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( !p_trans[i_x] )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( p_trans[i_x] == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst_y[i_x] = p_src2_y[i_x];

                if( b_even_scanline && i_x % 2 == 0 )
                {
                    p_dst_u[i_x/2] = p_src2_u[i_x];
                    p_dst_v[i_x/2] = p_src2_v[i_x];
                }
                continue;
            }

            /* Blending */
            p_dst_y[i_x] = ( (uint16_t)p_src2_y[i_x] * p_trans[i_x] +
                (uint16_t)p_src1_y[i_x] * (MAX_TRANS - p_trans[i_x]) )
                >> TRANS_BITS;

            if( b_even_scanline && i_x % 2 == 0 )
            {
                p_dst_u[i_x/2] = ( (uint16_t)p_src2_u[i_x] * p_trans[i_x] +
                (uint16_t)p_src1_u[i_x/2] * (MAX_TRANS - p_trans[i_x]) )
                >> TRANS_BITS;
                p_dst_v[i_x/2] = ( (uint16_t)p_src2_v[i_x] * p_trans[i_x] +
                (uint16_t)p_src1_v[i_x/2] * (MAX_TRANS - p_trans[i_x]) )
                >> TRANS_BITS;
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static inline void yuv_to_rgb( int *r, int *g, int *b,
                               uint8_t y1, uint8_t u1, uint8_t v1 )
{
    /* macros used for YUV pixel conversions */
#   define SCALEBITS 10
#   define ONE_HALF  (1 << (SCALEBITS - 1))
#   define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))
#   define CLAMP( x ) (((x) > 255) ? 255 : ((x) < 0) ? 0 : (x));

    int y, cb, cr, r_add, g_add, b_add;

    cb = u1 - 128;
    cr = v1 - 128;
    r_add = FIX(1.40200*255.0/224.0) * cr + ONE_HALF;
    g_add = - FIX(0.34414*255.0/224.0) * cb
            - FIX(0.71414*255.0/224.0) * cr + ONE_HALF;
    b_add = FIX(1.77200*255.0/224.0) * cb + ONE_HALF;
    y = (y1 - 16) * FIX(255.0/219.0);
    *r = CLAMP((y + r_add) >> SCALEBITS);
    *g = CLAMP((y + g_add) >> SCALEBITS);
    *b = CLAMP((y + b_add) >> SCALEBITS);
}

static void BlendR16( filter_t *p_filter, picture_t *p_dst_pic,
                      picture_t *p_dst_orig, picture_t *p_src,
                      int i_x_offset, int i_y_offset )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2_y;
    uint8_t *p_src2_u, *p_src2_v;
    uint8_t *p_trans;
    int i_width, i_height, i_x, i_y, i_pix_pitch;
    int r, g, b;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p[Y_PLANE].i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_x_offset * i_pix_pitch +
               p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
               p_dst_orig->p->i_pitch *
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src2_pitch = p_src->p[Y_PLANE].i_pitch;
    p_src2_y = p_src->p[Y_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset +
               p_src->p[Y_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;
    p_src2_u = p_src->p[U_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[U_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;
    p_src2_v = p_src->p[V_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[V_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;

    p_trans = p_src->p[A_PLANE].p_pixels +
              p_filter->fmt_in.video.i_x_offset +
              p_src->p[A_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width = __MIN( p_filter->fmt_out.video.i_visible_width - i_x_offset,
                     p_filter->fmt_in.video.i_visible_width );

    i_height = __MIN( p_filter->fmt_out.video.i_visible_height - i_y_offset,
                      p_filter->fmt_in.video.i_visible_height );

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
         p_src2_v += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( !p_trans[i_x] )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( p_trans[i_x] == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                yuv_to_rgb( &r, &g, &b,
                            p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

    ((uint16_t *)(&p_dst[i_x * i_pix_pitch]))[0] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                continue;
            }

            /* Blending */
            yuv_to_rgb( &r, &g, &b,
                        p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

    ((uint16_t *)(&p_dst[i_x * i_pix_pitch]))[0] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendR24( filter_t *p_filter, picture_t *p_dst_pic,
                      picture_t *p_dst_orig, picture_t *p_src,
                      int i_x_offset, int i_y_offset )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2_y;
    uint8_t *p_src2_u, *p_src2_v;
    uint8_t *p_trans;
    int i_width, i_height, i_x, i_y, i_pix_pitch;
    int r, g, b;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p[Y_PLANE].i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_x_offset * i_pix_pitch +
               p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
               p_dst_orig->p->i_pitch *
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src2_pitch = p_src->p[Y_PLANE].i_pitch;
    p_src2_y = p_src->p[Y_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset +
               p_src->p[Y_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;
    p_src2_u = p_src->p[U_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[U_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;
    p_src2_v = p_src->p[V_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[V_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;

    p_trans = p_src->p[A_PLANE].p_pixels +
              p_filter->fmt_in.video.i_x_offset +
              p_src->p[A_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width = __MIN( p_filter->fmt_out.video.i_visible_width - i_x_offset,
                     p_filter->fmt_in.video.i_visible_width );

    i_height = __MIN( p_filter->fmt_out.video.i_visible_height - i_y_offset,
                      p_filter->fmt_in.video.i_visible_height );

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
         p_src2_v += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( !p_trans[i_x] )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( p_trans[i_x] == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                yuv_to_rgb( &r, &g, &b,
                            p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

                p_dst[i_x * i_pix_pitch]     = r;
                p_dst[i_x * i_pix_pitch + 1] = g;
                p_dst[i_x * i_pix_pitch + 2] = b;
                continue;
            }

            /* Blending */
            yuv_to_rgb( &r, &g, &b,
                        p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

            p_dst[i_x * i_pix_pitch]     = ( r * p_trans[i_x] +
                (uint16_t)p_src1[i_x * i_pix_pitch] *
                (MAX_TRANS - p_trans[i_x]) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 1] = ( g * p_trans[i_x] +
                (uint16_t)p_src1[i_x * i_pix_pitch + 1] *
                (MAX_TRANS - p_trans[i_x]) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 2] = ( b * p_trans[i_x] +
                (uint16_t)p_src1[i_x * i_pix_pitch + 2] *
                (MAX_TRANS - p_trans[i_x]) ) >> TRANS_BITS;
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendYUY2( filter_t *p_filter, picture_t *p_dst_pic,
                       picture_t *p_dst_orig, picture_t *p_src,
                       int i_x_offset, int i_y_offset )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2_y;
    uint8_t *p_src2_u, *p_src2_v;
    uint8_t *p_trans;
    int i_width, i_height, i_x, i_y, i_pix_pitch;

    i_pix_pitch = 2;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p[Y_PLANE].i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_x_offset * i_pix_pitch +
               p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
               p_dst_orig->p->i_pitch *
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src2_pitch = p_src->p[Y_PLANE].i_pitch;
    p_src2_y = p_src->p[Y_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset +
               p_src->p[Y_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;
    p_src2_u = p_src->p[U_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[U_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;
    p_src2_v = p_src->p[V_PLANE].p_pixels +
               p_filter->fmt_in.video.i_x_offset/2 +
               p_src->p[V_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset/2;

    p_trans = p_src->p[A_PLANE].p_pixels +
              p_filter->fmt_in.video.i_x_offset +
              p_src->p[A_PLANE].i_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width = __MIN( p_filter->fmt_out.video.i_visible_width - i_x_offset,
                     p_filter->fmt_in.video.i_visible_width );

    i_height = __MIN( p_filter->fmt_out.video.i_visible_height - i_y_offset,
                      p_filter->fmt_in.video.i_visible_height );

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
         p_src2_v += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x += 2 )
        {
            if( !p_trans[i_x] )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( p_trans[i_x] == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst[i_x * 2]     = p_src2_y[i_x];
                p_dst[i_x * 2 + 1] = p_src2_u[i_x];
                p_dst[i_x * 2 + 2] = p_src2_y[i_x + 1];
                p_dst[i_x * 2 + 3] = p_src2_v[i_x + 1];
                continue;
            }

            /* Blending */
            p_dst[i_x * 2]     = ( (uint16_t)p_src2_y[i_x] * p_trans[i_x] +
                (uint16_t)p_src1[i_x * 2] *
                (MAX_TRANS - p_trans[i_x]) ) >> TRANS_BITS;
            p_dst[i_x * 2 + 1] = ( (uint16_t)p_src2_u[i_x] * p_trans[i_x] +
                (uint16_t)p_src1[i_x * 2 + 1] *
                (MAX_TRANS - p_trans[i_x]) ) >> TRANS_BITS;
            p_dst[i_x * 2 + 2] = ( (uint16_t)p_src2_y[i_x+1] * p_trans[i_x+1] +
                (uint16_t)p_src1[i_x * 2 + 2] *
                (MAX_TRANS - p_trans[i_x+1]) ) >> TRANS_BITS;
            p_dst[i_x * 2 + 3] = ( (uint16_t)p_src2_v[i_x+1] * p_trans[i_x+1] +
                (uint16_t)p_src1[i_x * 2 + 3] *
                (MAX_TRANS - p_trans[i_x+1]) ) >> TRANS_BITS;
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendPalette( filter_t *p_filter, picture_t *p_dst,
                          picture_t *p_dst_orig, picture_t *p_src,
                          int i_x_offset, int i_y_offset )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_src1_y, *p_src2, *p_dst_y;
    uint8_t *p_src1_u, *p_dst_u;
    uint8_t *p_src1_v, *p_dst_v;
    int i_width, i_height, i_x, i_y;
    vlc_bool_t b_even_scanline = i_y_offset % 2;

    i_dst_pitch = p_dst->p[Y_PLANE].i_pitch;
    p_dst_y = p_dst->p[Y_PLANE].p_pixels + i_x_offset +
              p_filter->fmt_out.video.i_x_offset +
              p_dst->p[Y_PLANE].i_pitch *
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset );
    p_dst_u = p_dst->p[U_PLANE].p_pixels + i_x_offset/2 +
              p_filter->fmt_out.video.i_x_offset/2 +
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
              p_dst->p[U_PLANE].i_pitch;
    p_dst_v = p_dst->p[V_PLANE].p_pixels + i_x_offset/2 +
              p_filter->fmt_out.video.i_x_offset/2 +
              ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
              p_dst->p[V_PLANE].i_pitch;

    i_src1_pitch = p_dst_orig->p[Y_PLANE].i_pitch;
    p_src1_y = p_dst_orig->p[Y_PLANE].p_pixels + i_x_offset +
               p_filter->fmt_out.video.i_x_offset +
               p_dst_orig->p[Y_PLANE].i_pitch *
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset );
    p_src1_u = p_dst_orig->p[U_PLANE].p_pixels + i_x_offset/2 +
               p_filter->fmt_out.video.i_x_offset/2 +
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
               p_dst_orig->p[U_PLANE].i_pitch;
    p_src1_v = p_dst_orig->p[V_PLANE].p_pixels + i_x_offset/2 +
               p_filter->fmt_out.video.i_x_offset/2 +
               ( i_y_offset + p_filter->fmt_out.video.i_y_offset ) / 2 *
               p_dst_orig->p[V_PLANE].i_pitch;

    i_src2_pitch = p_src->p->i_pitch;
    p_src2 = p_src->p->p_pixels + p_filter->fmt_in.video.i_x_offset +
             i_src2_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width = __MIN( p_filter->fmt_out.video.i_visible_width - i_x_offset,
                     p_filter->fmt_in.video.i_visible_width );

    i_height = __MIN( p_filter->fmt_out.video.i_visible_height - i_y_offset,
                      p_filter->fmt_in.video.i_visible_height );

#define MAX_TRANS 255
#define TRANS_BITS  8
#define p_trans p_src2
#define p_pal p_filter->fmt_in.video.p_palette->palette

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst_y += i_dst_pitch, p_src1_y += i_src1_pitch,
         p_src2 += i_src2_pitch,
         p_dst_u += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src1_u += b_even_scanline ? i_src1_pitch/2 : 0,
         p_dst_v += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src1_v += b_even_scanline ? i_src1_pitch/2 : 0 )
    {
        b_even_scanline = !b_even_scanline;

        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( !p_pal[p_trans[i_x]][3] )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( p_pal[p_trans[i_x]][3] == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst_y[i_x] = p_pal[p_src2[i_x]][0];

                if( b_even_scanline && i_x % 2 == 0 )
                {
                    p_dst_u[i_x/2] = p_pal[p_src2[i_x]][1];
                    p_dst_v[i_x/2] = p_pal[p_src2[i_x]][2];
                }
                continue;
            }

            /* Blending */
            p_dst_y[i_x] = ( (uint16_t)p_pal[p_src2[i_x]][0] *
                p_pal[p_trans[i_x]][3] + (uint16_t)p_src1_y[i_x] *
                (MAX_TRANS - p_pal[p_trans[i_x]][3]) ) >> TRANS_BITS;

            if( b_even_scanline && i_x % 2 == 0 )
            {
                p_dst_u[i_x/2] = ( (uint16_t)p_pal[p_src2[i_x]][1] *
                    p_pal[p_trans[i_x]][3] + (uint16_t)p_src1_u[i_x/2] *
                    (MAX_TRANS - p_pal[p_trans[i_x]][3]) ) >> TRANS_BITS;
                p_dst_v[i_x/2] = ( (uint16_t)p_pal[p_src2[i_x]][2] *
                    p_pal[p_trans[i_x]][3] + (uint16_t)p_src1_v[i_x/2] *
                    (MAX_TRANS - p_pal[p_trans[i_x]][3]) ) >> TRANS_BITS;
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS
#undef p_trans
#undef p_pal

    return;
}

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys );
}
