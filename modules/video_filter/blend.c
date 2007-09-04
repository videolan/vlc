/*****************************************************************************
 * blend.c: alpha blend 2 pictures together
 *****************************************************************************
 * Copyright (C) 2003-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Antoine Cellerier <dionoea @t videolan dot org>
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
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc_vout.h>
#include "vlc_filter.h"

/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    int i_dummy;
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );

static void Blend( filter_t *, picture_t *, picture_t *, picture_t *,
                   int, int, int );

/* TODO i_alpha support for BlendR16 */
/* YUVA */
static void BlendI420( filter_t *, picture_t *, picture_t *, picture_t *,
                       int, int, int, int, int );
static void BlendR16( filter_t *, picture_t *, picture_t *, picture_t *,
                      int, int, int, int, int );
static void BlendR24( filter_t *, picture_t *, picture_t *, picture_t *,
                      int, int, int, int, int );
static void BlendYUVPacked( filter_t *, picture_t *, picture_t *, picture_t *,
                            int, int, int, int, int );

/* I420, YV12 */
static void BlendI420I420( filter_t *, picture_t *, picture_t *, picture_t *,
                           int, int, int, int, int );
static void BlendI420I420_no_alpha(
                           filter_t *, picture_t *, picture_t *, picture_t *,
                           int, int, int, int );
static void BlendI420R16( filter_t *, picture_t *, picture_t *, picture_t *,
                           int, int, int, int, int );
static void BlendI420R24( filter_t *, picture_t *, picture_t *, picture_t *,
                          int, int, int, int, int );
static void BlendI420YUVPacked( filter_t *, picture_t *, picture_t *,
                                picture_t *, int, int, int, int, int );

/* YUVP */
static void BlendPalI420( filter_t *, picture_t *, picture_t *, picture_t *,
                          int, int, int, int, int );
static void BlendPalYUVPacked( filter_t *, picture_t *, picture_t *, picture_t *,
                               int, int, int, int, int );
static void BlendPalRV( filter_t *, picture_t *, picture_t *, picture_t *,
                        int, int, int, int, int );

/* RGBA */
static void BlendRGBAI420( filter_t *, picture_t *, picture_t *, picture_t *,
                           int, int, int, int, int );
static void BlendRGBAYUVPacked( filter_t *, picture_t *, picture_t *,
                                picture_t *, int, int, int, int, int );
static void BlendRGBAR16( filter_t *, picture_t *, picture_t *, picture_t *,
                          int, int, int, int, int );
static void BlendRGBAR24( filter_t *, picture_t *, picture_t *, picture_t *,
                          int, int, int, int, int );

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
    int in_chroma = p_filter->fmt_in.video.i_chroma;
    int out_chroma = p_filter->fmt_out.video.i_chroma;
    if( ( in_chroma  != VLC_FOURCC('Y','U','V','A') &&
          in_chroma  != VLC_FOURCC('I','4','2','0') &&
          in_chroma  != VLC_FOURCC('Y','V','1','2') &&
          in_chroma  != VLC_FOURCC('Y','U','V','P') &&
          in_chroma  != VLC_FOURCC('R','G','B','A') ) ||
        ( out_chroma != VLC_FOURCC('I','4','2','0') &&
          out_chroma != VLC_FOURCC('Y','U','Y','2') &&
          out_chroma != VLC_FOURCC('Y','V','1','2') &&
          out_chroma != VLC_FOURCC('U','Y','V','Y') &&
          out_chroma != VLC_FOURCC('Y','V','Y','U') &&
          out_chroma != VLC_FOURCC('R','V','1','6') &&
          out_chroma != VLC_FOURCC('R','V','2','4') &&
          out_chroma != VLC_FOURCC('R','V','3','2') ) )
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

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys );
}

/****************************************************************************
 * Blend: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static void Blend( filter_t *p_filter, picture_t *p_dst,
                   picture_t *p_dst_orig, picture_t *p_src,
                   int i_x_offset, int i_y_offset, int i_alpha )
{
    int i_width, i_height;

    if( i_alpha == 0 ) return;

    i_width = __MIN((int)p_filter->fmt_out.video.i_visible_width - i_x_offset,
                    (int)p_filter->fmt_in.video.i_visible_width);

    i_height = __MIN((int)p_filter->fmt_out.video.i_visible_height -i_y_offset,
                     (int)p_filter->fmt_in.video.i_visible_height);

    if( i_width <= 0 || i_height <= 0 ) return;

#if 0
    printf( "chroma: %4.4s -> %4.4s\n",
             (char *)&p_filter->fmt_in.video.i_chroma,
             (char *)&p_filter->fmt_out.video.i_chroma );
#endif

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_FOURCC('Y','U','V','A'):
            switch( p_filter->fmt_out.video.i_chroma )
            {
                case VLC_FOURCC('I','4','2','0'):
                case VLC_FOURCC('Y','V','1','2'):
                    BlendI420( p_filter, p_dst, p_dst_orig, p_src,
                               i_x_offset, i_y_offset,
                               i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('Y','U','Y','2'):
                case VLC_FOURCC('U','Y','V','Y'):
                case VLC_FOURCC('Y','V','Y','U'):
                    BlendYUVPacked( p_filter, p_dst, p_dst_orig, p_src,
                                    i_x_offset, i_y_offset,
                                    i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('R','V','1','6'):
                    BlendR16( p_filter, p_dst, p_dst_orig, p_src,
                              i_x_offset, i_y_offset,
                              i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('R','V','2','4'):
                case VLC_FOURCC('R','V','3','2'):
                    BlendR24( p_filter, p_dst, p_dst_orig, p_src,
                              i_x_offset, i_y_offset,
                              i_width, i_height, i_alpha );
                    return;
            }
        case VLC_FOURCC('Y','U','V','P'):
            switch( p_filter->fmt_out.video.i_chroma )
            {
                case VLC_FOURCC('I','4','2','0'):
                case VLC_FOURCC('Y','V','1','2'):
                    BlendPalI420( p_filter, p_dst, p_dst_orig, p_src,
                                  i_x_offset, i_y_offset,
                                  i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('Y','U','Y','2'):
                case VLC_FOURCC('U','Y','V','Y'):
                case VLC_FOURCC('Y','V','Y','U'):
                    BlendPalYUVPacked( p_filter, p_dst, p_dst_orig, p_src,
                                       i_x_offset, i_y_offset,
                                       i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('R','V','1','6'):
                case VLC_FOURCC('R','V','2','4'):
                case VLC_FOURCC('R','V','3','2'):
                    BlendPalRV( p_filter, p_dst, p_dst_orig, p_src,
                                i_x_offset, i_y_offset,
                                i_width, i_height, i_alpha );
                    return;
            }
        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('I','4','2','0'):
            switch( p_filter->fmt_out.video.i_chroma )
            {
                case VLC_FOURCC('I','4','2','0'):
                case VLC_FOURCC('Y','V','1','2'):
                    if( i_alpha == 0xff )
                        BlendI420I420_no_alpha(
                                   p_filter, p_dst, p_dst_orig, p_src,
                                   i_x_offset, i_y_offset,
                                   i_width, i_height );
                    else
                        BlendI420I420( p_filter, p_dst, p_dst_orig, p_src,
                                       i_x_offset, i_y_offset,
                                       i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('Y','U','Y','2'):
                case VLC_FOURCC('U','Y','V','Y'):
                case VLC_FOURCC('Y','V','Y','U'):
                    BlendI420YUVPacked( p_filter, p_dst, p_dst_orig, p_src,
                                        i_x_offset, i_y_offset,
                                        i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('R','V','1','6'):
                    BlendI420R16( p_filter, p_dst, p_dst_orig, p_src,
                                  i_x_offset, i_y_offset,
                                  i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('R','V','2','4'):
                case VLC_FOURCC('R','V','3','2'):
                    BlendI420R24( p_filter, p_dst, p_dst_orig, p_src,
                                  i_x_offset, i_y_offset,
                                  i_width, i_height, i_alpha );
                    return;
            }
        case VLC_FOURCC('R','G','B','A'):
            switch( p_filter->fmt_out.video.i_chroma )
            {
                case VLC_FOURCC('I','4','2','0'):
                case VLC_FOURCC('Y','V','1','2'):
                    BlendRGBAI420( p_filter, p_dst, p_dst_orig, p_src,
                                   i_x_offset, i_y_offset,
                                   i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('Y','U','Y','2'):
                case VLC_FOURCC('U','Y','V','Y'):
                case VLC_FOURCC('Y','V','Y','U'):
                    BlendRGBAYUVPacked( p_filter, p_dst, p_dst_orig, p_src,
                                        i_x_offset, i_y_offset,
                                        i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('R','V','2','4'):
                case VLC_FOURCC('R','V','3','2'):
                    BlendRGBAR24( p_filter, p_dst, p_dst_orig, p_src,
                                  i_x_offset, i_y_offset,
                                  i_width, i_height, i_alpha );
                    return;
                case VLC_FOURCC('R','V','1','6'):
                    BlendRGBAR16( p_filter, p_dst, p_dst_orig, p_src,
                                  i_x_offset, i_y_offset,
                                  i_width, i_height, i_alpha );
                    return;
            }
    }

    msg_Dbg( p_filter, "no matching alpha blending routine "
             "(chroma: %4.4s -> %4.4s)",
             (char *)&p_filter->fmt_in.video.i_chroma,
             (char *)&p_filter->fmt_out.video.i_chroma );
}

/***********************************************************************
 * Utils
 ***********************************************************************/
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

static inline void rgb_to_yuv( uint8_t *y, uint8_t *u, uint8_t *v,
                               int r, int g, int b )
{
    *y = ( ( (  66 * r + 129 * g +  25 * b + 128 ) >> 8 ) + 16 );
    *u =   ( ( -38 * r -  74 * g + 112 * b + 128 ) >> 8 ) + 128 ;
    *v =   ( ( 112 * r -  94 * g -  18 * b + 128 ) >> 8 ) + 128 ;
}

/***********************************************************************
 * YUVA
 ***********************************************************************/
static void BlendI420( filter_t *p_filter, picture_t *p_dst,
                       picture_t *p_dst_orig, picture_t *p_src,
                       int i_x_offset, int i_y_offset,
                       int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_src1_y, *p_src2_y, *p_dst_y;
    uint8_t *p_src1_u, *p_src2_u, *p_dst_u;
    uint8_t *p_src1_v, *p_src2_v, *p_dst_v;
    uint8_t *p_trans;
    int i_x, i_y, i_trans = 0;
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
            if( p_trans )
                i_trans = ( p_trans[i_x] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS )
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
            p_dst_y[i_x] = ( (uint16_t)p_src2_y[i_x] * i_trans +
                (uint16_t)p_src1_y[i_x] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;

            if( b_even_scanline && i_x % 2 == 0 )
            {
                p_dst_u[i_x/2] = ( (uint16_t)p_src2_u[i_x] * i_trans +
                (uint16_t)p_src1_u[i_x/2] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;
                p_dst_v[i_x/2] = ( (uint16_t)p_src2_v[i_x] * i_trans +
                (uint16_t)p_src1_v[i_x/2] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendR16( filter_t *p_filter, picture_t *p_dst_pic,
                      picture_t *p_dst_orig, picture_t *p_src,
                      int i_x_offset, int i_y_offset,
                      int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2_y;
    uint8_t *p_src2_u, *p_src2_v;
    uint8_t *p_trans;
    int i_x, i_y, i_pix_pitch, i_trans = 0;
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
            if( p_trans )
                i_trans = ( p_trans[i_x] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                yuv_to_rgb( &r, &g, &b,
                            p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

    ((uint16_t *)(&p_dst[i_x * i_pix_pitch]))[0] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                continue;
            }

            /* Blending */
            /* FIXME: do the blending */
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
                      int i_x_offset, int i_y_offset,
                      int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2_y;
    uint8_t *p_src2_u, *p_src2_v;
    uint8_t *p_trans;
    int i_x, i_y, i_pix_pitch, i_trans = 0;
    int r, g, b;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p->i_pitch;
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

#define MAX_TRANS 255
#define TRANS_BITS  8

    if( (i_pix_pitch == 4)
     && (((((intptr_t)p_dst)|((intptr_t)p_src1)|i_dst_pitch|i_src1_pitch)
          & 3) == 0) )
    {
        /*
        ** if picture pixels are 32 bits long and lines addresses are 32 bit
        ** aligned, optimize rendering
        */
        uint32_t *p32_dst = (uint32_t *)p_dst;
        uint32_t i32_dst_pitch = (uint32_t)(i_dst_pitch>>2);
        uint32_t *p32_src1 = (uint32_t *)p_src1;
        uint32_t i32_src1_pitch = (uint32_t)(i_src1_pitch>>2);

        int i_rshift, i_gshift, i_bshift;
        uint32_t i_rmask, i_gmask, i_bmask;

        if( p_dst_pic->p_heap )
        {
            i_rmask = p_dst_pic->p_heap->i_rmask;
            i_gmask = p_dst_pic->p_heap->i_gmask;
            i_bmask = p_dst_pic->p_heap->i_bmask;
            i_rshift = p_dst_pic->p_heap->i_lrshift;
            i_gshift = p_dst_pic->p_heap->i_lgshift;
            i_bshift = p_dst_pic->p_heap->i_lbshift;
        }
        else
        {
            i_rmask = p_dst_pic->format.i_rmask;
            i_gmask = p_dst_pic->format.i_gmask;
            i_bmask = p_dst_pic->format.i_bmask;

            if( (i_rmask == 0x00FF0000)
             && (i_gmask == 0x0000FF00)
             && (i_bmask == 0x000000FF) )
            {
                /* X8R8G8B8 pixel layout */
                i_rshift = 16;
                i_bshift = 8;
                i_gshift = 0;
            }
            else if( (i_rmask == 0xFF000000)
                  && (i_gmask == 0x00FF0000)
                  && (i_bmask == 0x0000FF00) )
            {
                /* R8G8B8X8 pixel layout */
                i_rshift = 24;
                i_bshift = 16;
                i_gshift = 8;
            }
            else
            {
                goto slower;
            }
        }
        /* Draw until we reach the bottom of the subtitle */
        for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
             p32_dst += i32_dst_pitch, p32_src1 += i32_src1_pitch,
             p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
             p_src2_v += i_src2_pitch )
        {
            /* Draw until we reach the end of the line */
            for( i_x = 0; i_x < i_width; i_x++ )
            {
                if( p_trans )
                    i_trans = ( p_trans[i_x] * i_alpha ) / 255;
                if( !i_trans )
                {
                    /* Completely transparent. Don't change pixel */
                    continue;
                }
                else if( i_trans == MAX_TRANS )
                {
                    /* Completely opaque. Completely overwrite underlying pixel */
                    yuv_to_rgb( &r, &g, &b,
                                p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

                    p32_dst[i_x] = (r<<i_rshift)
                                 + (g<<i_gshift)
                                 + (b<<i_bshift);
                }
                else
                {
                    /* Blending */
                    uint32_t i_pix_src1 = p32_src1[i_x];
                    yuv_to_rgb( &r, &g, &b,
                                p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

                    p32_dst[i_x] = ( ( r * i_trans +
                                     (uint16_t)((i_pix_src1 & i_rmask)>>i_rshift) *
                                     (MAX_TRANS - i_trans) ) >> TRANS_BITS) << i_rshift
                                 | ( ( g * i_trans +
                                     (uint16_t)((i_pix_src1 & i_gmask)>>i_gshift) *
                                     (MAX_TRANS - i_trans) ) >> TRANS_BITS) << i_gshift
                                 | ( ( b * i_trans +
                                     (uint16_t)((i_pix_src1 & i_bmask)>>i_bshift) *
                                     (MAX_TRANS - i_trans) ) >> TRANS_BITS) << i_bshift;
                }
            }
        }
    }
    else
    {
        int i_rindex, i_bindex, i_gindex;
        uint32_t i_rmask, i_gmask, i_bmask;

        slower:

        i_rmask = p_dst_pic->format.i_rmask;
        i_gmask = p_dst_pic->format.i_gmask;
        i_bmask = p_dst_pic->format.i_bmask;

        /*
        ** quick and dirty way to get byte index from mask
        ** will only work correctly if mask are 8 bit aligned
        ** and are 8 bit long
        */
#ifdef WORDS_BIGENDIAN
        i_rindex = ((i_rmask>>16) & 1)
                 | ((i_rmask>>8) & 2)
                 | ((i_rmask) & 3);
        i_gindex = ((i_gmask>>16) & 1)
                 | ((i_gmask>>8) & 2)
                 | ((i_gmask) & 3);
        i_bindex = ((i_bmask>>16) & 1)
                 | ((i_bmask>>8) & 2)
                 | ((i_bmask) & 3);
#else
        i_rindex = ((i_rmask>>24) & 3)
                 | ((i_rmask>>16) & 2)
                 | ((i_rmask>>8) & 1);
        i_gindex = ((i_gmask>>24) & 3)
                 | ((i_gmask>>16) & 2)
                 | ((i_gmask>>8) & 1);
        i_bindex = ((i_bmask>>24) & 3)
                 | ((i_bmask>>16) & 2)
                 | ((i_bmask>>8) & 1);
#endif

        /* Draw until we reach the bottom of the subtitle */
        for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
             p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
             p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
             p_src2_v += i_src2_pitch )
        {
            /* Draw until we reach the end of the line */
            for( i_x = 0; i_x < i_width; i_x++ )
            {
                if( p_trans )
                    i_trans = ( p_trans[i_x] * i_alpha ) / 255;
                if( !i_trans )
                {
                    /* Completely transparent. Don't change pixel */
                    continue;
                }
                else 
                {
                    int i_pos = i_x * i_pix_pitch;
                    if( i_trans == MAX_TRANS )
                    {

                        /* Completely opaque. Completely overwrite underlying pixel */
                        yuv_to_rgb( &r, &g, &b,
                                    p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

                        p_dst[i_pos + i_rindex ] = r;
                        p_dst[i_pos + i_gindex ] = g;
                        p_dst[i_pos + i_bindex ] = b;
                    }
                    else
                    {
                        int i_rpos = i_pos + i_rindex;
                        int i_gpos = i_pos + i_gindex;
                        int i_bpos = i_pos + i_bindex;

                        /* Blending */
                        yuv_to_rgb( &r, &g, &b,
                                    p_src2_y[i_x], p_src2_u[i_x], p_src2_v[i_x] );

                        p_dst[i_rpos] = ( r * i_trans +
                                        (uint16_t)p_src1[i_rpos] *
                                        (MAX_TRANS - i_trans) ) >> TRANS_BITS;
                        p_dst[i_gpos] = ( r * i_trans +
                                        (uint16_t)p_src1[i_gpos] *
                                        (MAX_TRANS - i_trans) ) >> TRANS_BITS;
                        p_dst[i_bpos] = ( r * i_trans +
                                        (uint16_t)p_src1[i_gpos] *
                                        (MAX_TRANS - i_trans) ) >> TRANS_BITS;
                    }
                }
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendYUVPacked( filter_t *p_filter, picture_t *p_dst_pic,
                            picture_t *p_dst_orig, picture_t *p_src,
                            int i_x_offset, int i_y_offset,
                            int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2_y;
    uint8_t *p_src2_u, *p_src2_v;
    uint8_t *p_trans;
    int i_x, i_y, i_pix_pitch, i_trans = 0;
    vlc_bool_t b_even = !((i_x_offset + p_filter->fmt_out.video.i_x_offset)%2);
    int i_l_offset = 0, i_u_offset = 0, i_v_offset = 0;

    if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','U','Y','2') )
    {
        i_l_offset = 0;
        i_u_offset = 1;
        i_v_offset = 3;
    }
    else if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('U','Y','V','Y') )
    {
        i_l_offset = 1;
        i_u_offset = 0;
        i_v_offset = 2;
    }
    else if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','V','Y','U') )
    {
        i_l_offset = 0;
        i_u_offset = 3;
        i_v_offset = 1;
    }

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

    i_width = (i_width >> 1) << 1; /* Needs to be a multiple of 2 */

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
         p_src2_v += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++, b_even = !b_even )
        {
            i_trans = ( p_trans[i_x] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst[i_x * 2 + i_l_offset]     = p_src2_y[i_x];

                if( b_even )
                {
                    if( p_trans[i_x+1] > 0xaa )
                    {
                        p_dst[i_x * 2 + i_u_offset] = (p_src2_u[i_x]+p_src2_u[i_x+1])>>1;
                        p_dst[i_x * 2 + i_v_offset] = (p_src2_v[i_x]+p_src2_v[i_x+1])>>1;
                    }
                    else
                    {
                        p_dst[i_x * 2 + i_u_offset] = p_src2_u[i_x];
                        p_dst[i_x * 2 + i_v_offset] = p_src2_v[i_x];
                    }
                }
            }
            else
            {
                /* Blending */
                p_dst[i_x * 2 + i_l_offset]     = ( (uint16_t)p_src2_y[i_x] * i_trans +
                    (uint16_t)p_src1[i_x * 2 + i_l_offset] * (MAX_TRANS - i_trans) )
                    >> TRANS_BITS;

                if( b_even )
                {
                    uint16_t i_u = 0;
                    uint16_t i_v = 0;
                    if( p_trans[i_x+1] > 0xaa )
                    {
                        i_u = (p_src2_u[i_x]+p_src2_u[i_x+1])>>1;
                        i_v = (p_src2_v[i_x]+p_src2_v[i_x+1])>>1;
                    }
                    else
                    {
                        i_u = p_src2_u[i_x];
                        i_v = p_src2_v[i_x];
                    }
                    p_dst[i_x * 2 + i_u_offset] = ( (uint16_t)i_u * i_trans +
                        (uint16_t)p_src1[i_x * 2 + i_u_offset] * (MAX_TRANS - i_trans) )
                        >> TRANS_BITS;
                    p_dst[i_x * 2 + i_v_offset] = ( (uint16_t)i_v * i_trans +
                        (uint16_t)p_src1[i_x * 2 + i_v_offset] * (MAX_TRANS - i_trans) )
                        >> TRANS_BITS;
                }
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}
/***********************************************************************
 * I420, YV12
 ***********************************************************************/
static void BlendI420I420( filter_t *p_filter, picture_t *p_dst,
                           picture_t *p_dst_orig, picture_t *p_src,
                           int i_x_offset, int i_y_offset,
                           int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_src1_y, *p_src2_y, *p_dst_y;
    uint8_t *p_src1_u, *p_src2_u, *p_dst_u;
    uint8_t *p_src1_v, *p_src2_v, *p_dst_v;
    int i_x, i_y;
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

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst_y += i_dst_pitch, p_src1_y += i_src1_pitch,
         p_src2_y += i_src2_pitch )
    {
        if( b_even_scanline )
        {
            p_dst_u  += i_dst_pitch/2;
            p_dst_v  += i_dst_pitch/2;
            p_src1_u += i_src1_pitch/2;
            p_src1_v += i_src1_pitch/2;
        }
        b_even_scanline = !b_even_scanline;

        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( i_alpha == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst_y[i_x] = p_src2_y[i_x];

                if( b_even_scanline && i_x % 2 == 0 )
                {
                    p_dst_u[i_x/2] = p_src2_u[i_x/2];
                    p_dst_v[i_x/2] = p_src2_v[i_x/2];
                }
                continue;
            }

            /* Blending */
            p_dst_y[i_x] = ( (uint16_t)p_src2_y[i_x] * i_alpha +
                (uint16_t)p_src1_y[i_x] * (MAX_TRANS - i_alpha) )
                >> TRANS_BITS;

            if( b_even_scanline && i_x % 2 == 0 )
            {
                p_dst_u[i_x/2] = ( (uint16_t)p_src2_u[i_x/2] * i_alpha +
                (uint16_t)p_src1_u[i_x/2] * (MAX_TRANS - i_alpha) )
                >> TRANS_BITS;
                p_dst_v[i_x/2] = ( (uint16_t)p_src2_v[i_x/2] * i_alpha +
                (uint16_t)p_src1_v[i_x/2] * (MAX_TRANS - i_alpha) )
                >> TRANS_BITS;
            }
        }
        if( i_y%2 == 1 )
        {
            p_src2_u += i_src2_pitch/2;
            p_src2_v += i_src2_pitch/2;
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}
static void BlendI420I420_no_alpha( filter_t *p_filter, picture_t *p_dst,
                                    picture_t *p_dst_orig, picture_t *p_src,
                                    int i_x_offset, int i_y_offset,
                                    int i_width, int i_height )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_src1_y, *p_src2_y, *p_dst_y;
    uint8_t *p_src1_u, *p_src2_u, *p_dst_u;
    uint8_t *p_src1_v, *p_src2_v, *p_dst_v;
    int i_y;
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

    i_width &= ~1;

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst_y += i_dst_pitch, p_src1_y += i_src1_pitch,
         p_src2_y += i_src2_pitch )
    {
        /* Completely opaque. Completely overwrite underlying pixel */
        p_filter->p_libvlc->pf_memcpy( p_dst_y, p_src2_y, i_width );
        if( b_even_scanline )
        {
            p_dst_u  += i_dst_pitch/2;
            p_dst_v  += i_dst_pitch/2;
            p_src1_u += i_src1_pitch/2;
            p_src1_v += i_src1_pitch/2;
        }
        else
        {
            p_filter->p_libvlc->pf_memcpy( p_dst_u, p_src2_u, i_width/2 );
            p_filter->p_libvlc->pf_memcpy( p_dst_v, p_src2_v, i_width/2 );
        }
        b_even_scanline = !b_even_scanline;
        if( i_y%2 == 1 )
        {
            p_src2_u += i_src2_pitch/2;
            p_src2_v += i_src2_pitch/2;
        }
    }

    return;
}

static void BlendI420R16( filter_t *p_filter, picture_t *p_dst_pic,
                          picture_t *p_dst_orig, picture_t *p_src,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2_y;
    uint8_t *p_src2_u, *p_src2_v;
    int i_x, i_y, i_pix_pitch;
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

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2_y += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( i_alpha == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                yuv_to_rgb( &r, &g, &b,
                            p_src2_y[i_x], p_src2_u[i_x/2], p_src2_v[i_x/2] );

    ((uint16_t *)(&p_dst[i_x * i_pix_pitch]))[0] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                continue;
            }

            /* Blending */
            /* FIXME: do the blending */
            yuv_to_rgb( &r, &g, &b,
                        p_src2_y[i_x], p_src2_u[i_x/2], p_src2_v[i_x/2] );

    ((uint16_t *)(&p_dst[i_x * i_pix_pitch]))[0] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
        if( i_y%2 == 1 )
        {
            p_src2_u += i_src2_pitch/2;
            p_src2_v += i_src2_pitch/2;
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendI420R24( filter_t *p_filter, picture_t *p_dst_pic,
                          picture_t *p_dst_orig, picture_t *p_src,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2_y;
    uint8_t *p_src2_u, *p_src2_v;
    int i_x, i_y, i_pix_pitch;
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

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
         p_src2_v += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( i_alpha == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                yuv_to_rgb( &r, &g, &b,
                            p_src2_y[i_x], p_src2_u[i_x/2], p_src2_v[i_x/2] );

                p_dst[i_x * i_pix_pitch]     = r;
                p_dst[i_x * i_pix_pitch + 1] = g;
                p_dst[i_x * i_pix_pitch + 2] = b;
                continue;
            }

            /* Blending */
            yuv_to_rgb( &r, &g, &b,
                        p_src2_y[i_x], p_src2_u[i_x/2], p_src2_v[i_x/2] );

            p_dst[i_x * i_pix_pitch]     = ( r * i_alpha +
                (uint16_t)p_src1[i_x * i_pix_pitch] *
                (MAX_TRANS - i_alpha) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 1] = ( g * i_alpha +
                (uint16_t)p_src1[i_x * i_pix_pitch + 1] *
                (MAX_TRANS - i_alpha) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 2] = ( b * i_alpha +
                (uint16_t)p_src1[i_x * i_pix_pitch + 2] *
                (MAX_TRANS - i_alpha) ) >> TRANS_BITS;
        }
        if( i_y%2 == 1 )
        {
            p_src2_u += i_src2_pitch/2;
            p_src2_v += i_src2_pitch/2;
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendI420YUVPacked( filter_t *p_filter, picture_t *p_dst_pic,
                                picture_t *p_dst_orig, picture_t *p_src,
                                int i_x_offset, int i_y_offset,
                                int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2_y;
    uint8_t *p_src2_u, *p_src2_v;
    int i_x, i_y, i_pix_pitch;
    vlc_bool_t b_even = !((i_x_offset + p_filter->fmt_out.video.i_x_offset)%2);
    int i_l_offset = 0, i_u_offset = 0, i_v_offset = 0;

    if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','U','Y','2') )
    {
        i_l_offset = 0;
        i_u_offset = 1;
        i_v_offset = 3;
    }
    else if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('U','Y','V','Y') )
    {
        i_l_offset = 1;
        i_u_offset = 0;
        i_v_offset = 2;
    }
    else if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','V','Y','U') )
    {
        i_l_offset = 0;
        i_u_offset = 3;
        i_v_offset = 1;
    }

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

    i_width &= ~1; /* Needs to be a multiple of 2 */

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2_y += i_src2_pitch, p_src2_u += i_src2_pitch,
         p_src2_v += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++, b_even = !b_even )
        {
            if( i_alpha == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst[i_x * 2 + i_l_offset]     = p_src2_y[i_x];

                if( b_even )
                {
                    p_dst[i_x * 2 + i_u_offset] = p_src2_u[i_x/2];
                    p_dst[i_x * 2 + i_v_offset] = p_src2_v[i_x/2];
                }
            }
            else
            {
                /* Blending */
                p_dst[i_x * 2 + i_l_offset]     = ( (uint16_t)p_src2_y[i_x] * i_alpha +
                    (uint16_t)p_src1[i_x * 2 + i_l_offset] * (MAX_TRANS - i_alpha) )
                    >> TRANS_BITS;

                if( b_even )
                {
                    uint16_t i_u = p_src2_u[i_x/2];
                    uint16_t i_v = p_src2_v[i_x/2];
                    p_dst[i_x * 2 + i_u_offset] = ( (uint16_t)i_u * i_alpha +
                        (uint16_t)p_src1[i_x * 2 + i_u_offset] * (MAX_TRANS - i_alpha) )
                        >> TRANS_BITS;
                    p_dst[i_x * 2 + i_v_offset] = ( (uint16_t)i_v * i_alpha +
                        (uint16_t)p_src1[i_x * 2 + i_v_offset] * (MAX_TRANS - i_alpha) )
                        >> TRANS_BITS;
                }
            }
        }
        if( i_y%2 == 1 )
        {
            p_src2_u += i_src2_pitch/2;
            p_src2_v += i_src2_pitch/2;
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

/***********************************************************************
 * YUVP
 ***********************************************************************/
static void BlendPalI420( filter_t *p_filter, picture_t *p_dst,
                          picture_t *p_dst_orig, picture_t *p_src,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_src1_y, *p_src2, *p_dst_y;
    uint8_t *p_src1_u, *p_dst_u;
    uint8_t *p_src1_v, *p_dst_v;
    int i_x, i_y, i_trans;
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
            i_trans = ( p_pal[p_trans[i_x]][3] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst_y[i_x] = p_pal[p_src2[i_x]][0];

                if( b_even_scanline && ((i_x % 2) == 0) )
                {
                    p_dst_u[i_x/2] = p_pal[p_src2[i_x]][1];
                    p_dst_v[i_x/2] = p_pal[p_src2[i_x]][2];
                }
                continue;
            }

            /* Blending */
            p_dst_y[i_x] = ( (uint16_t)p_pal[p_src2[i_x]][0] * i_trans +
                (uint16_t)p_src1_y[i_x] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;

            if( b_even_scanline && ((i_x % 2) == 0) )
            {
                p_dst_u[i_x/2] = ( (uint16_t)p_pal[p_src2[i_x]][1] * i_trans +
                    (uint16_t)p_src1_u[i_x/2] * (MAX_TRANS - i_trans) )
                    >> TRANS_BITS;
                p_dst_v[i_x/2] = ( (uint16_t)p_pal[p_src2[i_x]][2] * i_trans +
                    (uint16_t)p_src1_v[i_x/2] * (MAX_TRANS - i_trans) )
                    >> TRANS_BITS;
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS
#undef p_trans
#undef p_pal

    return;
}

static void BlendPalYUVPacked( filter_t *p_filter, picture_t *p_dst_pic,
                               picture_t *p_dst_orig, picture_t *p_src,
                               int i_x_offset, int i_y_offset,
                               int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_src1, *p_src2, *p_dst;
    int i_x, i_y, i_pix_pitch, i_trans;
    vlc_bool_t b_even = !((i_x_offset + p_filter->fmt_out.video.i_x_offset)%2);
    int i_l_offset = 0, i_u_offset = 0, i_v_offset = 0;

    if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','U','Y','2') )
    {
        i_l_offset = 0;
        i_u_offset = 1;
        i_v_offset = 3;
    }
    else if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('U','Y','V','Y') )
    {
        i_l_offset = 1;
        i_u_offset = 0;
        i_v_offset = 2;
    }
    else if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','V','Y','U') )
    {
        i_l_offset = 0;
        i_u_offset = 3;
        i_v_offset = 1;
    }

    i_pix_pitch = 2;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_pix_pitch * (i_x_offset +
            p_filter->fmt_out.video.i_x_offset) + p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p->i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_pix_pitch * (i_x_offset +
             p_filter->fmt_out.video.i_x_offset) + p_dst_orig->p->i_pitch *
             ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src2_pitch = p_src->p->i_pitch;
    p_src2 = p_src->p->p_pixels + p_filter->fmt_in.video.i_x_offset +
             i_src2_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width = (i_width >> 1) << 1; /* Needs to be a multiple of 2 */

#define MAX_TRANS 255
#define TRANS_BITS  8
#define p_trans p_src2
#define p_pal p_filter->fmt_in.video.p_palette->palette

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch, p_src2 += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++, b_even = !b_even )
        {
            i_trans = ( p_pal[p_trans[i_x]][3] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst[i_x * 2 + i_l_offset]     = p_pal[p_src2[i_x]][0];

                if( b_even )
                {
                    if( p_trans[i_x+1] > 0xaa )
                    {
                        p_dst[i_x * 2 + i_u_offset] = (p_pal[p_src2[i_x]][1] + p_pal[p_src2[i_x+1]][1]) >> 1;
                        p_dst[i_x * 2 + i_v_offset] = (p_pal[p_src2[i_x]][2] + p_pal[p_src2[i_x+1]][2]) >> 1;
                    }
                    else
                    {
                        p_dst[i_x * 2 + i_u_offset] = p_pal[p_src2[i_x]][1];
                        p_dst[i_x * 2 + i_v_offset] = p_pal[p_src2[i_x]][2];
                    }
                }
            }
            else
            {
                /* Blending */
                p_dst[i_x * 2 + i_l_offset]     = ( (uint16_t)p_pal[p_src2[i_x]][0] *
                    i_trans + (uint16_t)p_src1[i_x * 2 + i_l_offset] *
                    (MAX_TRANS - i_trans) ) >> TRANS_BITS;

                if( b_even )
                {
                    uint16_t i_u = 0;
                    uint16_t i_v = 0;
                    if( p_trans[i_x+1] > 0xaa )
                    {
                        i_u = (p_pal[p_src2[i_x]][1] + p_pal[p_src2[i_x+1]][1]) >> 1;
                        i_v = (p_pal[p_src2[i_x]][2] + p_pal[p_src2[i_x+1]][2]) >> 1;
                    }
                    else
                    {
                        i_u = p_pal[p_src2[i_x]][1];
                        i_v = p_pal[p_src2[i_x]][2];
                    }

                    p_dst[i_x * 2 + i_u_offset] = ( (uint16_t)i_u *
                        i_trans + (uint16_t)p_src1[i_x * 2 + i_u_offset] *
                        (MAX_TRANS - i_trans) ) >> TRANS_BITS;
                    p_dst[i_x * 2 + i_v_offset] = ( (uint16_t)i_v *
                        i_trans + (uint16_t)p_src1[i_x * 2 + i_v_offset] *
                        (MAX_TRANS - i_trans) ) >> TRANS_BITS;
                }
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS
#undef p_trans
#undef p_pal

    return;
}

static void BlendPalRV( filter_t *p_filter, picture_t *p_dst_pic,
                        picture_t *p_dst_orig, picture_t *p_src,
                        int i_x_offset, int i_y_offset,
                        int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_src1, *p_src2, *p_dst;
    int i_x, i_y, i_pix_pitch, i_trans;
    int r, g, b;
    video_palette_t rgbpalette;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_pix_pitch * (i_x_offset +
            p_filter->fmt_out.video.i_x_offset) + p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p->i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_pix_pitch * (i_x_offset +
             p_filter->fmt_out.video.i_x_offset) + p_dst_orig->p->i_pitch *
             ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src2_pitch = p_src->p->i_pitch;
    p_src2 = p_src->p->p_pixels + p_filter->fmt_in.video.i_x_offset +
             i_src2_pitch * p_filter->fmt_in.video.i_y_offset;

#define MAX_TRANS 255
#define TRANS_BITS  8
#define p_trans p_src2
#define p_pal p_filter->fmt_in.video.p_palette->palette
#define rgbpal rgbpalette.palette

    /* Convert palette first */
    for( i_y = 0; i_y < p_filter->fmt_in.video.p_palette->i_entries &&
         i_y < 256; i_y++ )
    {
        yuv_to_rgb( &r, &g, &b, p_pal[i_y][0], p_pal[i_y][1], p_pal[i_y][2] );

        if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('R','V','1','6') )
        {
            *(uint16_t *)rgbpal[i_y] =
                ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
        else
        {
            rgbpal[i_y][0] = r; rgbpal[i_y][1] = g; rgbpal[i_y][2] = b;
        }
    }

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch, p_src2 += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            i_trans = ( p_pal[p_trans[i_x]][3] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS ||
                     p_filter->fmt_out.video.i_chroma ==
                     VLC_FOURCC('R','V','1','6') )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst[i_x * i_pix_pitch]     = rgbpal[p_src2[i_x]][0];
                p_dst[i_x * i_pix_pitch + 1] = rgbpal[p_src2[i_x]][1];
                if( p_filter->fmt_out.video.i_chroma !=
                    VLC_FOURCC('R','V','1','6') )
                p_dst[i_x * i_pix_pitch + 2] = rgbpal[p_src2[i_x]][2];
                continue;
            }

            /* Blending */
            p_dst[i_x * i_pix_pitch]     = ( (uint16_t)rgbpal[p_src2[i_x]][0] *
                i_trans + (uint16_t)p_src1[i_x * i_pix_pitch] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 1] = ( (uint16_t)rgbpal[p_src2[i_x]][1] *
                i_trans + (uint16_t)p_src1[i_x * i_pix_pitch + 1] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 2] = ( (uint16_t)rgbpal[p_src2[i_x]][2] *
                i_trans + (uint16_t)p_src1[i_x * i_pix_pitch + 2] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS
#undef p_trans
#undef p_pal
#undef rgbpal

    return;
}

/***********************************************************************
 * RGBA
 ***********************************************************************/
static void BlendRGBAI420( filter_t *p_filter, picture_t *p_dst,
                           picture_t *p_dst_orig, picture_t *p_src,
                           int i_x_offset, int i_y_offset,
                           int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch, i_src_pix_pitch;
    uint8_t *p_src1_y, *p_dst_y;
    uint8_t *p_src1_u, *p_dst_u;
    uint8_t *p_src1_v, *p_dst_v;
    uint8_t *p_src2;
    int i_x, i_y, i_trans;
    uint8_t y, u, v;

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

    i_src_pix_pitch = p_src->p->i_pixel_pitch;
    i_src2_pitch = p_src->p->i_pitch;
    p_src2 = p_src->p->p_pixels +
             p_filter->fmt_in.video.i_x_offset * i_src2_pitch +
             p_src->p->i_pitch * p_filter->fmt_in.video.i_y_offset;


#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst_y += i_dst_pitch, p_src1_y += i_src1_pitch,
         p_dst_u += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src1_u += b_even_scanline ? i_src1_pitch/2 : 0,
         p_dst_v += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src1_v += b_even_scanline ? i_src1_pitch/2 : 0,
         p_src2 += i_src2_pitch )
    {
        b_even_scanline = !b_even_scanline;

        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
#define     R         ( p_src2[i_x * i_src_pix_pitch + 0] )
#define     G         ( p_src2[i_x * i_src_pix_pitch + 1] )
#define     B         ( p_src2[i_x * i_src_pix_pitch + 2] )
            i_trans = ( p_src2[i_x * i_src_pix_pitch + 3] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                rgb_to_yuv( &y, &u, &v, R, G, B );
                p_dst_y[i_x] = y;

                if( b_even_scanline && i_x % 2 == 0 )
                {
                    p_dst_u[i_x/2] = u;
                    p_dst_v[i_x/2] = v;
                }
                continue;
            }

            /* Blending */
            rgb_to_yuv( &y, &u, &v, R, G, B );
            p_dst_y[i_x] = ( (uint16_t)y * i_trans +
                (uint16_t)p_src1_y[i_x] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;

            if( b_even_scanline && i_x % 2 == 0 )
            {
                p_dst_u[i_x/2] = ( (uint16_t)u * i_trans +
                (uint16_t)p_src1_u[i_x/2] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;
                p_dst_v[i_x/2] = ( (uint16_t)v * i_trans +
                (uint16_t)p_src1_v[i_x/2] * (MAX_TRANS - i_trans) )
                >> TRANS_BITS;
            }
#undef      R
#undef      G
#undef      B
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendRGBAR24( filter_t *p_filter, picture_t *p_dst_pic,
                          picture_t *p_dst_orig, picture_t *p_src,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2;
    int i_x, i_y, i_pix_pitch, i_trans, i_src_pix_pitch;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p->i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_x_offset * i_pix_pitch +
             p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
             p_dst_orig->p->i_pitch *
             ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src_pix_pitch = p_src->p->i_pixel_pitch;
    i_src2_pitch = p_src->p->i_pitch;
    p_src2 = p_src->p->p_pixels +
             p_filter->fmt_in.video.i_x_offset * i_pix_pitch +
             p_src->p->i_pitch * p_filter->fmt_in.video.i_y_offset;

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch, p_src2 += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
#define     R         ( p_src2[i_x * i_src_pix_pitch + 0] )
#define     G         ( p_src2[i_x * i_src_pix_pitch + 1] )
#define     B         ( p_src2[i_x * i_src_pix_pitch + 2] )
            i_trans = ( p_src2[i_x * i_src_pix_pitch + 3] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                p_dst[i_x * i_pix_pitch + 0] = R;
                p_dst[i_x * i_pix_pitch + 1] = G;
                p_dst[i_x * i_pix_pitch + 2] = B;
                continue;
            }

            /* Blending */
            p_dst[i_x * i_pix_pitch + 0] = ( R * i_trans +
                (uint16_t)p_src1[i_x * i_pix_pitch + 0] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 1] = ( G * i_trans +
                (uint16_t)p_src1[i_x * i_pix_pitch + 1] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
            p_dst[i_x * i_pix_pitch + 2] = ( B * i_trans +
                (uint16_t)p_src1[i_x * i_pix_pitch + 2] *
                (MAX_TRANS - i_trans) ) >> TRANS_BITS;
#undef      R
#undef      G
#undef      B
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendRGBAR16( filter_t *p_filter, picture_t *p_dst_pic,
                          picture_t *p_dst_orig, picture_t *p_src,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src1, *p_src2;
    int i_x, i_y, i_pix_pitch, i_trans, i_src_pix_pitch;
    uint16_t i_pix;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src1_pitch = p_dst_orig->p->i_pitch;
    p_src1 = p_dst_orig->p->p_pixels + i_x_offset * i_pix_pitch +
             p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
             p_dst_orig->p->i_pitch *
             ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src_pix_pitch = p_src->p->i_pixel_pitch;
    i_src2_pitch = p_src->p->i_pitch;
    p_src2 = p_src->p->p_pixels +
             p_filter->fmt_in.video.i_x_offset * i_pix_pitch +
             p_src->p->i_pitch * p_filter->fmt_in.video.i_y_offset;

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch, p_src2 += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
#define     R         ( p_src2[i_x * i_src_pix_pitch + 0] )
#define     G         ( p_src2[i_x * i_src_pix_pitch + 1] )
#define     B         ( p_src2[i_x * i_src_pix_pitch + 2] )
            i_trans = ( p_src2[i_x * i_src_pix_pitch + 3] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
                continue;
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                *((uint16_t *)(&p_dst[i_x * i_pix_pitch])) = ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);
                continue;
            }

            /* Blending */
            i_pix = *((uint16_t *)(&p_dst[i_x * i_pix_pitch]));
            *((uint16_t *)(&p_dst[i_x * i_pix_pitch])) =
                ( ( ( (R >> 3)*i_trans
                    + (i_pix >> 11) * (MAX_TRANS - i_trans) )
                    >> TRANS_BITS ) << 11 )
              | ( ( ( (G >> 2)*i_trans
                    + ((i_pix & 0x07e0)>> 5) * (MAX_TRANS - i_trans) )
                    >> TRANS_BITS ) << 5  )
              | ( ( ( (B >> 3)*i_trans
                    + (i_pix & 0x001f) * (MAX_TRANS - i_trans) )
                    >> TRANS_BITS ) );
#undef      R
#undef      G
#undef      B
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}

static void BlendRGBAYUVPacked( filter_t *p_filter, picture_t *p_dst_pic,
                                picture_t *p_dst_orig, picture_t *p_src,
                                int i_x_offset, int i_y_offset,
                                int i_width, int i_height, int i_alpha )
{
    int i_src1_pitch, i_src2_pitch, i_dst_pitch, i_src_pix_pitch;
    uint8_t *p_dst, *p_src1, *p_src2;
    uint8_t *p_trans;
    int i_x, i_y, i_pix_pitch, i_trans;
    vlc_bool_t b_even = !((i_x_offset + p_filter->fmt_out.video.i_x_offset)%2);
    int i_l_offset = 0, i_u_offset = 0, i_v_offset = 0;
    uint8_t y, u, v;

    if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','U','Y','2') )
    {
        i_l_offset = 0;
        i_u_offset = 1;
        i_v_offset = 3;
    }
    else if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('U','Y','V','Y') )
    {
        i_l_offset = 1;
        i_u_offset = 0;
        i_v_offset = 2;
    }
    else if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','V','Y','U') )
    {
        i_l_offset = 0;
        i_u_offset = 3;
        i_v_offset = 1;
    }

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

    i_src_pix_pitch = p_src->p->i_pixel_pitch;
    i_src2_pitch = p_src->p->i_pitch;
    p_src2 = p_src->p->p_pixels +
             p_filter->fmt_in.video.i_x_offset * i_src2_pitch +
             p_src->p->i_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width = (i_width >> 1) << 1; /* Needs to be a multiple of 2 */

#define MAX_TRANS 255
#define TRANS_BITS  8

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src2_pitch,
         p_dst += i_dst_pitch, p_src1 += i_src1_pitch,
         p_src2 += i_src2_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++, b_even = !b_even )
        {
#define     R         ( p_src2[i_x * i_src_pix_pitch + 0] )
#define     G         ( p_src2[i_x * i_src_pix_pitch + 1] )
#define     B         ( p_src2[i_x * i_src_pix_pitch + 2] )
            i_trans = ( p_src2[i_x * i_src_pix_pitch + 3] * i_alpha ) / 255;
            if( !i_trans )
            {
                /* Completely transparent. Don't change pixel */
            }
            else if( i_trans == MAX_TRANS )
            {
                /* Completely opaque. Completely overwrite underlying pixel */
                rgb_to_yuv( &y, &u, &v, R, G, B );
                p_dst[i_x * 2 + i_l_offset] = y;

                if( b_even )
                {
                    p_dst[i_x * 2 + i_u_offset] = u;
                    p_dst[i_x * 2 + i_v_offset] = v;
                }
            }
            else
            {
                /* Blending */
                rgb_to_yuv( &y, &u, &v, R, G, B );
                p_dst[i_x * 2 + i_l_offset]     = ( (uint16_t)y * i_trans +
                    (uint16_t)p_src1[i_x * 2 + i_l_offset] * (MAX_TRANS - i_trans) )
                    >> TRANS_BITS;

                if( b_even )
                {
                    p_dst[i_x * 2 + i_u_offset] = ( (uint16_t)u * i_trans +
                        (uint16_t)p_src1[i_x * 2 + i_u_offset] * (MAX_TRANS - i_trans) )
                        >> TRANS_BITS;
                    p_dst[i_x * 2 + i_v_offset] = ( (uint16_t)v * i_trans +
                        (uint16_t)p_src1[i_x * 2 + i_v_offset] * (MAX_TRANS - i_trans) )
                        >> TRANS_BITS;
                }
            }
        }
    }

#undef MAX_TRANS
#undef TRANS_BITS

    return;
}
