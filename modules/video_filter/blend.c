/*****************************************************************************
 * blend.c: alpha blend 2 pictures together
 *****************************************************************************
 * Copyright (C) 2003-2008 the VideoLAN team
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include "vlc_filter.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Video pictures blending") )
    set_capability( "video blending", 100 )
    set_callbacks( OpenFilter, CloseFilter )
vlc_module_end ()


/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    int i_dummy;
};

#define FCC_YUVA VLC_FOURCC('Y','U','V','A')
#define FCC_YUVP VLC_FOURCC('Y','U','V','P')
#define FCC_RGBA VLC_FOURCC('R','G','B','A')

#define FCC_I420 VLC_FOURCC('I','4','2','0')
#define FCC_YV12 VLC_FOURCC('Y','V','1','2')
#define FCC_YUY2 VLC_FOURCC('Y','U','Y','2')
#define FCC_UYVY VLC_FOURCC('U','Y','V','Y')
#define FCC_YVYU VLC_FOURCC('Y','V','Y','U')
#define FCC_RV15 VLC_FOURCC('R','V','1','5')
#define FCC_RV16 VLC_FOURCC('R','V','1','6')
#define FCC_RV24 VLC_FOURCC('R','V','2','4')
#define FCC_RV32 VLC_FOURCC('R','V','3','2')

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static void Blend( filter_t *, picture_t *, picture_t *,
                   int, int, int );

/* YUVA */
static void BlendYUVAI420( filter_t *, picture_t *, picture_t *,
                           int, int, int, int, int );
static void BlendYUVARV16( filter_t *, picture_t *, picture_t *,
                           int, int, int, int, int );
static void BlendYUVARV24( filter_t *, picture_t *, picture_t *,
                           int, int, int, int, int );
static void BlendYUVAYUVPacked( filter_t *, picture_t *, picture_t *,
                                int, int, int, int, int );

/* I420, YV12 */
static void BlendI420I420( filter_t *, picture_t *, picture_t *,
                           int, int, int, int, int );
static void BlendI420I420_no_alpha(
                           filter_t *, picture_t *, picture_t *,
                           int, int, int, int );
static void BlendI420R16( filter_t *, picture_t *, picture_t *,
                           int, int, int, int, int );
static void BlendI420R24( filter_t *, picture_t *, picture_t *,
                          int, int, int, int, int );
static void BlendI420YUVPacked( filter_t *, picture_t *,
                                picture_t *, int, int, int, int, int );

/* YUVP */
static void BlendPalI420( filter_t *, picture_t *, picture_t *,
                          int, int, int, int, int );
static void BlendPalYUVPacked( filter_t *, picture_t *, picture_t *,
                               int, int, int, int, int );
static void BlendPalRV( filter_t *, picture_t *, picture_t *,
                        int, int, int, int, int );

/* RGBA */
static void BlendRGBAI420( filter_t *, picture_t *, picture_t *,
                           int, int, int, int, int );
static void BlendRGBAYUVPacked( filter_t *, picture_t *,
                                picture_t *, int, int, int, int, int );
static void BlendRGBAR16( filter_t *, picture_t *, picture_t *,
                          int, int, int, int, int );
static void BlendRGBAR24( filter_t *, picture_t *, picture_t *,
                          int, int, int, int, int );

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
    if( ( in_chroma  != FCC_YUVA && in_chroma  != FCC_I420 &&
          in_chroma  != FCC_YV12 && in_chroma  != FCC_YUVP &&
          in_chroma  != FCC_RGBA ) ||
        ( out_chroma != FCC_I420 && out_chroma != FCC_YUY2 &&
          out_chroma != FCC_YV12 && out_chroma != FCC_UYVY &&
          out_chroma != FCC_YVYU && out_chroma != FCC_RV15 &&
          out_chroma != FCC_YVYU && out_chroma != FCC_RV16 &&
          out_chroma != FCC_RV24 && out_chroma != FCC_RV32 ) )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    p_filter->p_sys = p_sys = malloc(sizeof(filter_sys_t));
    if( !p_sys )
        return VLC_ENOMEM;

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
typedef void (*BlendFunction)( filter_t *,
                       picture_t *, picture_t *,
                       int , int , int , int , int );

#define FCC_PLANAR_420 { FCC_I420, FCC_YV12, 0 }
#define FCC_PACKED_422 { FCC_YUY2, FCC_UYVY, FCC_YVYU, 0 }
#define FCC_RGB_16 { FCC_RV15, FCC_RV16, 0 }
#define FCC_RGB_24 { FCC_RV24, FCC_RV32, 0 }

#define BLEND_CFG( fccSrc, fctPlanar, fctPacked, fctRgb16, fctRgb24  ) \
    { .src = fccSrc, .p_dst = FCC_PLANAR_420, .pf_blend = fctPlanar }, \
    { .src = fccSrc, .p_dst = FCC_PACKED_422, .pf_blend = fctPacked }, \
    { .src = fccSrc, .p_dst = FCC_RGB_16,     .pf_blend = fctRgb16  }, \
    { .src = fccSrc, .p_dst = FCC_RGB_24,     .pf_blend = fctRgb24  }

static const struct
{
    vlc_fourcc_t src;
    vlc_fourcc_t p_dst[16];
    BlendFunction pf_blend;
} p_blend_cfg[] = {

    BLEND_CFG( FCC_YUVA, BlendYUVAI420, BlendYUVAYUVPacked, BlendYUVARV16, BlendYUVARV24 ),

    BLEND_CFG( FCC_YUVP, BlendPalI420, BlendPalYUVPacked, BlendPalRV, BlendPalRV ),

    BLEND_CFG( FCC_RGBA, BlendRGBAI420, BlendRGBAYUVPacked, BlendRGBAR16, BlendRGBAR24 ),

    BLEND_CFG( FCC_I420, BlendI420I420, BlendI420YUVPacked, BlendI420R16, BlendI420R24 ),

    BLEND_CFG( FCC_YV12, BlendI420I420, BlendI420YUVPacked, BlendI420R16, BlendI420R24 ),

    { 0, {0,}, NULL }
};

static void Blend( filter_t *p_filter,
                   picture_t *p_dst, picture_t *p_src,
                   int i_x_offset, int i_y_offset, int i_alpha )
{
    int i_width, i_height;

    if( i_alpha == 0 )
        return;

    i_width = __MIN((int)p_filter->fmt_out.video.i_visible_width - i_x_offset,
                    (int)p_filter->fmt_in.video.i_visible_width);

    i_height = __MIN((int)p_filter->fmt_out.video.i_visible_height -i_y_offset,
                     (int)p_filter->fmt_in.video.i_visible_height);

    if( i_width <= 0 || i_height <= 0 )
        return;

    video_format_FixRgb( &p_filter->fmt_out.video );
    video_format_FixRgb( &p_filter->fmt_in.video );

#if 0
    msg_Dbg( p_filter, "chroma: %4.4s -> %4.4s\n",
             (char *)&p_filter->fmt_in.video.i_chroma,
             (char *)&p_filter->fmt_out.video.i_chroma );
#endif

    for( int i = 0; p_blend_cfg[i].src != 0; i++ )
    {
        if( p_blend_cfg[i].src != p_filter->fmt_in.video.i_chroma )
            continue;
        for( int j = 0; p_blend_cfg[i].p_dst[j] != 0; j++ )
        {
            if( p_blend_cfg[i].p_dst[j] != p_filter->fmt_out.video.i_chroma )
                continue;

            p_blend_cfg[i].pf_blend( p_filter, p_dst, p_src,
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
static inline uint8_t vlc_uint8( int v )
{
    if( v > 255 )
        return 255;
    else if( v < 0 )
        return 0;
    return v;
}

#define MAX_TRANS 255
#define TRANS_BITS  8

static inline int vlc_blend( int v1, int v2, int a )
{
    /* TODO bench if the tests really increase speed */
    if( a == 0 )
        return v2;
    else if( a == MAX_TRANS )
        return v1;
    return ( v1 * a + v2 * (MAX_TRANS - a ) ) >> TRANS_BITS;
}

static inline int vlc_alpha( int t, int a )
{
    if( a == 255 )
        return t;
    return (t * a) / 255;
}

static inline void yuv_to_rgb( int *r, int *g, int *b,
                               uint8_t y1, uint8_t u1, uint8_t v1 )
{
    /* macros used for YUV pixel conversions */
#   define SCALEBITS 10
#   define ONE_HALF  (1 << (SCALEBITS - 1))
#   define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))

    int y, cb, cr, r_add, g_add, b_add;

    cb = u1 - 128;
    cr = v1 - 128;
    r_add = FIX(1.40200*255.0/224.0) * cr + ONE_HALF;
    g_add = - FIX(0.34414*255.0/224.0) * cb
            - FIX(0.71414*255.0/224.0) * cr + ONE_HALF;
    b_add = FIX(1.77200*255.0/224.0) * cb + ONE_HALF;
    y = (y1 - 16) * FIX(255.0/219.0);
    *r = vlc_uint8( (y + r_add) >> SCALEBITS );
    *g = vlc_uint8( (y + g_add) >> SCALEBITS );
    *b = vlc_uint8( (y + b_add) >> SCALEBITS );
#undef FIX
#undef ONE_HALF
#undef SCALEBITS
}

static inline void rgb_to_yuv( uint8_t *y, uint8_t *u, uint8_t *v,
                               int r, int g, int b )
{
    *y = ( ( (  66 * r + 129 * g +  25 * b + 128 ) >> 8 ) + 16 );
    *u =   ( ( -38 * r -  74 * g + 112 * b + 128 ) >> 8 ) + 128 ;
    *v =   ( ( 112 * r -  94 * g -  18 * b + 128 ) >> 8 ) + 128 ;
}

static uint8_t *vlc_plane_start( int *pi_pitch,
                                 picture_t *p_picture,
                                 int i_plane,
                                 int i_x_offset, int i_y_offset,
                                 const video_format_t *p_fmt,
                                 int r )
{
    const int i_pitch = p_picture->p[i_plane].i_pitch;
    uint8_t *p_pixels = p_picture->p[i_plane].p_pixels;

    const int i_dx = ( i_x_offset + p_fmt->i_x_offset ) / r;
    const int i_dy = ( i_y_offset + p_fmt->i_y_offset ) / r;

    if( pi_pitch )
        *pi_pitch = i_pitch;
    return &p_pixels[ i_dy * i_pitch + i_dx ];
}

static void vlc_yuv_packed_index( int *pi_y, int *pi_u, int *pi_v, vlc_fourcc_t i_chroma )
{
    static const struct {
        vlc_fourcc_t chroma;
        int y, u ,v;
    } p_index[] = {
        { FCC_YUY2, 0, 1, 3 },
        { FCC_UYVY, 1, 0, 2 },
        { FCC_YVYU, 0, 3, 1 },
        { 0, 0, 0, 0 }
    };
    int i;

    for( i = 0; p_index[i].chroma != 0; i++ )
    {
        if( p_index[i].chroma == i_chroma )
            break;
    }
    *pi_y = p_index[i].y;
    *pi_u = p_index[i].u;
    *pi_v = p_index[i].v;
}

static void vlc_blend_packed( uint8_t *p_dst,
                              int i_offset0, int i_offset1, int i_offset2,
                              int c0, int c1, int c2, int i_alpha,
                              bool b_do12 )
{
    p_dst[i_offset0] = vlc_blend( c0, p_dst[i_offset0], i_alpha );
    if( b_do12 )
    {
        p_dst[i_offset1] = vlc_blend( c1, p_dst[i_offset1], i_alpha );
        p_dst[i_offset2] = vlc_blend( c2, p_dst[i_offset2], i_alpha );
    }
}

static void vlc_blend_rgb16( uint16_t *p_dst,
                             int R, int G, int B, int i_alpha,
                             const video_format_t *p_fmt )
{
    const int i_pix = *p_dst;
    const int r = ( i_pix & p_fmt->i_rmask ) >> p_fmt->i_lrshift;
    const int g = ( i_pix & p_fmt->i_gmask ) >> p_fmt->i_lgshift;
    const int b = ( i_pix & p_fmt->i_bmask ) >> p_fmt->i_lbshift;

    *p_dst = ( vlc_blend( R >> p_fmt->i_rrshift, r, i_alpha ) << p_fmt->i_lrshift ) |
             ( vlc_blend( G >> p_fmt->i_rgshift, g, i_alpha ) << p_fmt->i_lgshift ) |
             ( vlc_blend( B >> p_fmt->i_rbshift, b, i_alpha ) << p_fmt->i_lbshift );
}

static void vlc_rgb_index( int *pi_rindex, int *pi_gindex, int *pi_bindex,
                           const video_format_t *p_fmt )
{
    if( p_fmt->i_chroma != FCC_RV24 && p_fmt->i_chroma != FCC_RV32 )
        return;

    /* XXX it will works only if mask are 8 bits aligned */
#ifdef WORDS_BIGENDIAN
    const int i_mask_bits = p_fmt->i_chroma == FCC_RV24 ? 24 : 32;
    *pi_rindex = ( i_mask_bits - p_fmt->i_lrshift ) / 8;
    *pi_gindex = ( i_mask_bits - p_fmt->i_lgshift ) / 8;
    *pi_bindex = ( i_mask_bits - p_fmt->i_lbshift ) / 8;
#else
    *pi_rindex = p_fmt->i_lrshift / 8;
    *pi_gindex = p_fmt->i_lgshift / 8;
    *pi_bindex = p_fmt->i_lbshift / 8;
#endif
}

/***********************************************************************
 * YUVA
 ***********************************************************************/
static void BlendYUVAI420( filter_t *p_filter,
                           picture_t *p_dst, picture_t *p_src,
                           int i_x_offset, int i_y_offset,
                           int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_src_y, *p_dst_y;
    uint8_t *p_src_u, *p_dst_u;
    uint8_t *p_src_v, *p_dst_v;
    uint8_t *p_trans;
    int i_x, i_y, i_trans = 0;
    bool b_even_scanline = i_y_offset % 2;

    p_dst_y = vlc_plane_start( &i_dst_pitch, p_dst, Y_PLANE,
                               i_x_offset, i_y_offset, &p_filter->fmt_out.video, 1 );
    p_dst_u = vlc_plane_start( NULL, p_dst, U_PLANE,
                               i_x_offset, i_y_offset, &p_filter->fmt_out.video, 2 );
    p_dst_v = vlc_plane_start( NULL, p_dst, V_PLANE,
                               i_x_offset, i_y_offset, &p_filter->fmt_out.video, 2 );

    p_src_y = vlc_plane_start( &i_src_pitch, p_src, Y_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );
    p_src_u = vlc_plane_start( NULL, p_src, U_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_src_v = vlc_plane_start( NULL, p_src, V_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_trans = vlc_plane_start( NULL, p_src, A_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src_pitch,
         p_dst_y += i_dst_pitch, p_src_y += i_src_pitch,
         p_dst_u += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src_u += i_src_pitch,
         p_dst_v += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src_v += i_src_pitch )
    {
        b_even_scanline = !b_even_scanline;

        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( p_trans )
                i_trans = vlc_alpha( p_trans[i_x], i_alpha );

            if( !i_trans )
                continue;

            /* Blending */
            p_dst_y[i_x] = vlc_blend( p_src_y[i_x], p_dst_y[i_x], i_trans );
            if( b_even_scanline && i_x % 2 == 0 )
            {
                p_dst_u[i_x/2] = vlc_blend( p_src_u[i_x], p_dst_u[i_x/2], i_trans );
                p_dst_v[i_x/2] = vlc_blend( p_src_v[i_x], p_dst_v[i_x/2], i_trans );
            }
        }
    }
}

static void BlendYUVARV16( filter_t *p_filter,
                           picture_t *p_dst_pic, picture_t *p_src,
                           int i_x_offset, int i_y_offset,
                           int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src_y;
    uint8_t *p_src_u, *p_src_v;
    uint8_t *p_trans;
    int i_x, i_y, i_pix_pitch, i_trans = 0;
    int r, g, b;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    p_src_y = vlc_plane_start( &i_src_pitch, p_src, Y_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );
    p_src_u = vlc_plane_start( NULL, p_src, U_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_src_v = vlc_plane_start( NULL, p_src, V_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_trans = vlc_plane_start( NULL, p_src, A_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src_pitch,
         p_dst += i_dst_pitch,
         p_src_y += i_src_pitch, p_src_u += i_src_pitch,
         p_src_v += i_src_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( p_trans )
                i_trans = vlc_alpha( p_trans[i_x], i_alpha );
            if( !i_trans )
                continue;

            /* Blending */
            yuv_to_rgb( &r, &g, &b,
                        p_src_y[i_x], p_src_u[i_x], p_src_v[i_x] );

            vlc_blend_rgb16( (uint16_t*)&p_dst[i_x * i_pix_pitch],
                             r, g, b, i_trans, &p_filter->fmt_out.video );
        }
    }
}

static void BlendYUVARV24( filter_t *p_filter,
                           picture_t *p_dst_pic, picture_t *p_src,
                           int i_x_offset, int i_y_offset,
                           int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src_y;
    uint8_t *p_src_u, *p_src_v;
    uint8_t *p_trans;
    int i_x, i_y, i_pix_pitch, i_trans = 0;
    int r, g, b;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    p_src_y = vlc_plane_start( &i_src_pitch, p_src, Y_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );
    p_src_u = vlc_plane_start( NULL, p_src, U_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_src_v = vlc_plane_start( NULL, p_src, V_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_trans = vlc_plane_start( NULL, p_src, A_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );

    if( (i_pix_pitch == 4)
     && (((((intptr_t)p_dst)|i_dst_pitch) /* FIXME? */
          & 3) == 0) )
    {
        /*
        ** if picture pixels are 32 bits long and lines addresses are 32 bit
        ** aligned, optimize rendering
        */
        uint32_t *p32_dst = (uint32_t *)p_dst;
        uint32_t i32_dst_pitch = (uint32_t)(i_dst_pitch>>2);

        int i_rshift, i_gshift, i_bshift;
        uint32_t i_rmask, i_gmask, i_bmask;

        i_rmask = p_filter->fmt_out.video.i_rmask;
        i_gmask = p_filter->fmt_out.video.i_gmask;
        i_bmask = p_filter->fmt_out.video.i_bmask;
        i_rshift = p_filter->fmt_out.video.i_lrshift;
        i_gshift = p_filter->fmt_out.video.i_lgshift;
        i_bshift = p_filter->fmt_out.video.i_lbshift;

        /* Draw until we reach the bottom of the subtitle */
        for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src_pitch,
             p32_dst += i32_dst_pitch,
             p_src_y += i_src_pitch, p_src_u += i_src_pitch,
             p_src_v += i_src_pitch )
        {
            /* Draw until we reach the end of the line */
            for( i_x = 0; i_x < i_width; i_x++ )
            {
                if( p_trans )
                    i_trans = vlc_alpha( p_trans[i_x], i_alpha );
                if( !i_trans )
                    continue;

                if( i_trans == MAX_TRANS )
                {
                    /* Completely opaque. Completely overwrite underlying pixel */
                    yuv_to_rgb( &r, &g, &b,
                                p_src_y[i_x], p_src_u[i_x], p_src_v[i_x] );

                    p32_dst[i_x] = (r<<i_rshift) |
                                   (g<<i_gshift) |
                                   (b<<i_bshift);
                }
                else
                {
                    /* Blending */
                    uint32_t i_pix_dst = p32_dst[i_x];
                    yuv_to_rgb( &r, &g, &b,
                                p_src_y[i_x], p_src_u[i_x], p_src_v[i_x] );

                    p32_dst[i_x] = ( vlc_blend( r, (i_pix_dst & i_rmask)>>i_rshift, i_trans ) << i_rshift ) |
                                   ( vlc_blend( g, (i_pix_dst & i_gmask)>>i_gshift, i_trans ) << i_gshift ) |
                                   ( vlc_blend( b, (i_pix_dst & i_bmask)>>i_bshift, i_trans ) << i_bshift );
                }
            }
        }
    }
    else
    {
        int i_rindex, i_gindex, i_bindex;
        uint32_t i_rmask, i_gmask, i_bmask;

        i_rmask = p_filter->fmt_out.video.i_rmask;
        i_gmask = p_filter->fmt_out.video.i_gmask;
        i_bmask = p_filter->fmt_out.video.i_bmask;

        vlc_rgb_index( &i_rindex, &i_gindex, &i_bindex, &p_filter->fmt_out.video );

        /* Draw until we reach the bottom of the subtitle */
        for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src_pitch,
             p_dst += i_dst_pitch,
             p_src_y += i_src_pitch, p_src_u += i_src_pitch,
             p_src_v += i_src_pitch )
        {
            /* Draw until we reach the end of the line */
            for( i_x = 0; i_x < i_width; i_x++ )
            {
                if( p_trans )
                    i_trans = vlc_alpha( p_trans[i_x], i_alpha );
                if( !i_trans )
                    continue;

                /* Blending */
                yuv_to_rgb( &r, &g, &b,
                            p_src_y[i_x], p_src_u[i_x], p_src_v[i_x] );

                vlc_blend_packed( &p_dst[ i_x * i_pix_pitch],
                                  i_rindex, i_gindex, i_bindex,
                                  r, g, b, i_alpha, true );
            }
        }
    }
}

static void BlendYUVAYUVPacked( filter_t *p_filter,
                                picture_t *p_dst_pic, picture_t *p_src,
                                int i_x_offset, int i_y_offset,
                                int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src_y;
    uint8_t *p_src_u, *p_src_v;
    uint8_t *p_trans;
    int i_x, i_y, i_pix_pitch, i_trans = 0;
    bool b_even = !((i_x_offset + p_filter->fmt_out.video.i_x_offset)%2);
    int i_l_offset, i_u_offset, i_v_offset;

    vlc_yuv_packed_index( &i_l_offset, &i_u_offset, &i_v_offset,
                          p_filter->fmt_out.video.i_chroma );

    i_pix_pitch = 2;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    p_src_y = vlc_plane_start( &i_src_pitch, p_src, Y_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );
    p_src_u = vlc_plane_start( NULL, p_src, U_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_src_v = vlc_plane_start( NULL, p_src, V_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_trans = vlc_plane_start( NULL, p_src, A_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );

    i_width &= ~1; /* Needs to be a multiple of 2 */

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++, p_trans += i_src_pitch,
         p_dst += i_dst_pitch,
         p_src_y += i_src_pitch, p_src_u += i_src_pitch,
         p_src_v += i_src_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++, b_even = !b_even )
        {
            i_trans = vlc_alpha( p_trans[i_x], i_alpha );
            if( !i_trans )
                continue;

            /* Blending */
            if( b_even )
            {
                int i_u;
                int i_v;
                /* FIXME what's with 0xaa ? */
                if( p_trans[i_x+1] > 0xaa )
                {
                    i_u = (p_src_u[i_x]+p_src_u[i_x+1])>>1;
                    i_v = (p_src_v[i_x]+p_src_v[i_x+1])>>1;
                }
                else
                {
                    i_u = p_src_u[i_x];
                    i_v = p_src_v[i_x];
                }

                vlc_blend_packed( &p_dst[i_x * 2],
                                  i_l_offset, i_u_offset, i_v_offset,
                                  p_src_y[i_x], i_u, i_v, i_trans, true );
            }
            else
            {
                p_dst[i_x * 2 + i_l_offset] = vlc_blend( p_src_y[i_x], p_dst[i_x * 2 + i_l_offset], i_trans );
            }
        }
    }
}
/***********************************************************************
 * I420, YV12
 ***********************************************************************/
static void BlendI420I420( filter_t *p_filter,
                           picture_t *p_dst, picture_t *p_src,
                           int i_x_offset, int i_y_offset,
                           int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_src_y, *p_dst_y;
    uint8_t *p_src_u, *p_dst_u;
    uint8_t *p_src_v, *p_dst_v;
    int i_x, i_y;
    bool b_even_scanline = i_y_offset % 2;

    if( i_alpha == 0xff )
    {
        BlendI420I420_no_alpha( p_filter, p_dst, p_src,
                                i_x_offset, i_y_offset, i_width, i_height );
        return;
    }


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

    p_src_y = vlc_plane_start( &i_src_pitch, p_src, Y_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );
    p_src_u = vlc_plane_start( NULL, p_src, U_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_src_v = vlc_plane_start( NULL, p_src, V_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    i_width &= ~1;

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst_y += i_dst_pitch,
         p_src_y += i_src_pitch )
    {
        if( b_even_scanline )
        {
            p_dst_u  += i_dst_pitch/2;
            p_dst_v  += i_dst_pitch/2;
        }
        b_even_scanline = !b_even_scanline;

        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( !i_alpha )
                continue;

            /* Blending */
            p_dst_y[i_x] = vlc_blend( p_src_y[i_x], p_dst_y[i_x], i_alpha );
            if( b_even_scanline && i_x % 2 == 0 )
            {
                p_dst_u[i_x/2] = vlc_blend( p_src_u[i_x/2], p_dst_u[i_x/2], i_alpha );
                p_dst_v[i_x/2] = vlc_blend( p_src_v[i_x/2], p_dst_v[i_x/2], i_alpha );
            }
        }
        if( i_y%2 == 1 )
        {
            p_src_u += i_src_pitch/2;
            p_src_v += i_src_pitch/2;
        }
    }
}
static void BlendI420I420_no_alpha( filter_t *p_filter,
                                    picture_t *p_dst, picture_t *p_src,
                                    int i_x_offset, int i_y_offset,
                                    int i_width, int i_height )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_src_y, *p_dst_y;
    uint8_t *p_src_u, *p_dst_u;
    uint8_t *p_src_v, *p_dst_v;
    int i_y;
    bool b_even_scanline = i_y_offset % 2;

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

    p_src_y = vlc_plane_start( &i_src_pitch, p_src, Y_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );
    p_src_u = vlc_plane_start( NULL, p_src, U_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_src_v = vlc_plane_start( NULL, p_src, V_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );

    i_width &= ~1;

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height;
            i_y++, p_dst_y += i_dst_pitch, p_src_y += i_src_pitch )
    {
        /* Completely opaque. Completely overwrite underlying pixel */
        vlc_memcpy( p_dst_y, p_src_y, i_width );
        if( b_even_scanline )
        {
            p_dst_u  += i_dst_pitch/2;
            p_dst_v  += i_dst_pitch/2;
        }
        else
        {
            vlc_memcpy( p_dst_u, p_src_u, i_width/2 );
            vlc_memcpy( p_dst_v, p_src_v, i_width/2 );
        }
        b_even_scanline = !b_even_scanline;
        if( i_y%2 == 1 )
        {
            p_src_u += i_src_pitch/2;
            p_src_v += i_src_pitch/2;
        }
    }
}

static void BlendI420R16( filter_t *p_filter,
                          picture_t *p_dst_pic, picture_t *p_src,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src_y;
    uint8_t *p_src_u, *p_src_v;
    int i_x, i_y, i_pix_pitch;
    int r, g, b;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    p_src_y = vlc_plane_start( &i_src_pitch, p_src, Y_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );
    p_src_u = vlc_plane_start( NULL, p_src, U_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_src_v = vlc_plane_start( NULL, p_src, V_PLANE,
                                0, 0, &p_filter->fmt_in.video, 2 );

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch,
         p_src_y += i_src_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            /* Blending */
            yuv_to_rgb( &r, &g, &b,
                        p_src_y[i_x], p_src_u[i_x/2], p_src_v[i_x/2] );

            vlc_blend_rgb16( (uint16_t*)&p_dst[i_x * i_pix_pitch],
                             r, g, b, i_alpha, &p_filter->fmt_out.video );
        }
        if( i_y%2 == 1 )
        {
            p_src_u += i_src_pitch/2;
            p_src_v += i_src_pitch/2;
        }
    }
}

static void BlendI420R24( filter_t *p_filter,
                          picture_t *p_dst_pic, picture_t *p_src,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src_y;
    uint8_t *p_src_u, *p_src_v;
    int i_x, i_y, i_pix_pitch;
    int i_rindex, i_gindex, i_bindex;
    int r, g, b;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    p_src_y = vlc_plane_start( &i_src_pitch, p_src, Y_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );
    p_src_u = vlc_plane_start( NULL, p_src, U_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_src_v = vlc_plane_start( NULL, p_src, V_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );

    vlc_rgb_index( &i_rindex, &i_gindex, &i_bindex, &p_filter->fmt_out.video );

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch,
         p_src_y += i_src_pitch, p_src_u += i_src_pitch,
         p_src_v += i_src_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            if( !i_alpha )
                continue;

            /* Blending */
            yuv_to_rgb( &r, &g, &b,
                        p_src_y[i_x], p_src_u[i_x/2], p_src_v[i_x/2] );

            vlc_blend_packed( &p_dst[i_x * i_pix_pitch],
                              i_rindex, i_gindex, i_bindex, r, g, b, i_alpha, true );
        }
        if( i_y%2 == 1 )
        {
            p_src_u += i_src_pitch/2;
            p_src_v += i_src_pitch/2;
        }
    }
}

static void BlendI420YUVPacked( filter_t *p_filter,
                                picture_t *p_dst_pic, picture_t *p_src,
                                int i_x_offset, int i_y_offset,
                                int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src_y;
    uint8_t *p_src_u, *p_src_v;
    int i_x, i_y, i_pix_pitch;
    bool b_even = !((i_x_offset + p_filter->fmt_out.video.i_x_offset)%2);
    int i_l_offset, i_u_offset, i_v_offset;

    vlc_yuv_packed_index( &i_l_offset, &i_u_offset, &i_v_offset,
                          p_filter->fmt_out.video.i_chroma );

    i_pix_pitch = 2;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    p_src_y = vlc_plane_start( &i_src_pitch, p_src, Y_PLANE,
                               0, 0, &p_filter->fmt_in.video, 1 );
    p_src_u = vlc_plane_start( NULL, p_src, U_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );
    p_src_v = vlc_plane_start( NULL, p_src, V_PLANE,
                               0, 0, &p_filter->fmt_in.video, 2 );

    i_width &= ~1; /* Needs to be a multiple of 2 */

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch,
         p_src_y += i_src_pitch, p_src_u += i_src_pitch,
         p_src_v += i_src_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++, b_even = !b_even )
        {
            if( !i_alpha )
                continue;

            /* Blending */
            vlc_blend_packed( &p_dst[i_x * 2],
                              i_l_offset, i_u_offset, i_v_offset,
                              p_src_y[i_x], p_src_u[i_x/2], p_src_v[i_x/2], i_alpha, b_even );
        }
        if( i_y%2 == 1 )
        {
            p_src_u += i_src_pitch/2;
            p_src_v += i_src_pitch/2;
        }
    }
}

/***********************************************************************
 * YUVP
 ***********************************************************************/
static void BlendPalI420( filter_t *p_filter,
                          picture_t *p_dst, picture_t *p_src_pic,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_src, *p_dst_y;
    uint8_t *p_dst_u;
    uint8_t *p_dst_v;
    int i_x, i_y, i_trans;
    bool b_even_scanline = i_y_offset % 2;

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

    i_src_pitch = p_src_pic->p->i_pitch;
    p_src = p_src_pic->p->p_pixels + p_filter->fmt_in.video.i_x_offset +
            i_src_pitch * p_filter->fmt_in.video.i_y_offset;

#define p_pal p_filter->fmt_in.video.p_palette->palette

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst_y += i_dst_pitch,
         p_src += i_src_pitch,
         p_dst_u += b_even_scanline ? i_dst_pitch/2 : 0,
         p_dst_v += b_even_scanline ? i_dst_pitch/2 : 0 )
    {
        const uint8_t *p_trans = p_src;
        b_even_scanline = !b_even_scanline;

        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            i_trans = vlc_alpha( p_pal[p_trans[i_x]][3], i_alpha );
            if( !i_trans )
                continue;

            /* Blending */
            p_dst_y[i_x] = vlc_blend( p_pal[p_src[i_x]][0], p_dst_y[i_x], i_trans );
            if( b_even_scanline && ((i_x % 2) == 0) )
            {
                p_dst_u[i_x/2] = vlc_blend( p_pal[p_src[i_x]][1], p_dst_u[i_x/2], i_trans );
                p_dst_v[i_x/2] = vlc_blend( p_pal[p_src[i_x]][2], p_dst_v[i_x/2], i_trans );
            }
        }
    }
#undef p_pal
}

static void BlendPalYUVPacked( filter_t *p_filter,
                               picture_t *p_dst_pic, picture_t *p_src_pic,
                               int i_x_offset, int i_y_offset,
                               int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_src, *p_dst;
    int i_x, i_y, i_pix_pitch, i_trans;
    bool b_even = !((i_x_offset + p_filter->fmt_out.video.i_x_offset)%2);
    int i_l_offset, i_u_offset, i_v_offset;

    vlc_yuv_packed_index( &i_l_offset, &i_u_offset, &i_v_offset,
                          p_filter->fmt_out.video.i_chroma );

    i_pix_pitch = 2;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_pix_pitch * (i_x_offset +
            p_filter->fmt_out.video.i_x_offset) + p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src_pitch = p_src_pic->p->i_pitch;
    p_src = p_src_pic->p->p_pixels + p_filter->fmt_in.video.i_x_offset +
            i_src_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width &= ~1; /* Needs to be a multiple of 2 */

#define p_pal p_filter->fmt_in.video.p_palette->palette

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src += i_src_pitch )
    {
        const uint8_t *p_trans = p_src;
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++, b_even = !b_even )
        {
            i_trans = vlc_alpha( p_pal[p_trans[i_x]][3], i_alpha );
            if( !i_trans )
                continue;

            /* Blending */
            if( b_even )
            {
                uint16_t i_u;
                uint16_t i_v;
                if( p_trans[i_x+1] > 0xaa )
                {
                    i_u = (p_pal[p_src[i_x]][1] + p_pal[p_src[i_x+1]][1]) >> 1;
                    i_v = (p_pal[p_src[i_x]][2] + p_pal[p_src[i_x+1]][2]) >> 1;
                }
                else
                {
                    i_u = p_pal[p_src[i_x]][1];
                    i_v = p_pal[p_src[i_x]][2];
                }

                vlc_blend_packed( &p_dst[i_x * 2],
                                  i_l_offset, i_u_offset, i_v_offset,
                                  p_pal[p_src[i_x]][0], i_u, i_v, i_trans, true );
            }
            else
            {
                p_dst[i_x * 2 + i_l_offset] = vlc_blend( p_pal[p_src[i_x]][0], p_dst[i_x * 2 + i_l_offset], i_trans );
            }
        }
    }
#undef p_pal
}

static void BlendPalRV( filter_t *p_filter,
                        picture_t *p_dst_pic, picture_t *p_src_pic,
                        int i_x_offset, int i_y_offset,
                        int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_src, *p_dst;
    int i_x, i_y, i_pix_pitch, i_trans;
    video_palette_t rgbpalette;
    int i_rindex, i_gindex, i_bindex;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_pix_pitch * (i_x_offset +
            p_filter->fmt_out.video.i_x_offset) + p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src_pitch = p_src_pic->p->i_pitch;
    p_src = p_src_pic->p->p_pixels + p_filter->fmt_in.video.i_x_offset +
            i_src_pitch * p_filter->fmt_in.video.i_y_offset;

#define p_pal p_filter->fmt_in.video.p_palette->palette
#define rgbpal rgbpalette.palette

    /* Convert palette first */
    for( i_y = 0; i_y < p_filter->fmt_in.video.p_palette->i_entries && i_y < 256; i_y++ )
    {
        int r, g, b;

        yuv_to_rgb( &r, &g, &b, p_pal[i_y][0], p_pal[i_y][1], p_pal[i_y][2] );
        rgbpal[i_y][0] = r;
        rgbpal[i_y][1] = g;
        rgbpal[i_y][2] = b;
    }

    /* */
    vlc_rgb_index( &i_rindex, &i_gindex, &i_bindex, &p_filter->fmt_out.video );

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src += i_src_pitch )
    {
        const uint8_t *p_trans = p_src;
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            i_trans = vlc_alpha( p_pal[p_trans[i_x]][3], i_alpha );
            if( !i_trans )
                continue;

            /* Blending */
            if( p_filter->fmt_out.video.i_chroma == FCC_RV15 || p_filter->fmt_out.video.i_chroma == FCC_RV16 )
                vlc_blend_rgb16( (uint16_t*)&p_dst[i_x * i_pix_pitch],
                                  rgbpal[p_src[i_x]][0], rgbpal[p_src[i_x]][1], rgbpal[p_src[i_x]][2],
                                  i_trans,
                                  &p_filter->fmt_out.video );
            else
                vlc_blend_packed( &p_dst[i_x * i_pix_pitch],
                                  i_rindex, i_gindex, i_bindex,
                                  rgbpal[p_src[i_x]][0], rgbpal[p_src[i_x]][1], rgbpal[p_src[i_x]][2],
                                  i_trans, true );
        }
    }

#undef p_pal
#undef rgbpal
}

/***********************************************************************
 * RGBA
 ***********************************************************************/
static void BlendRGBAI420( filter_t *p_filter,
                           picture_t *p_dst, picture_t *p_src_pic,
                           int i_x_offset, int i_y_offset,
                           int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch, i_src_pix_pitch;
    uint8_t *p_dst_y;
    uint8_t *p_dst_u;
    uint8_t *p_dst_v;
    uint8_t *p_src;
    int i_x, i_y, i_trans;
    uint8_t y, u, v;

    bool b_even_scanline = i_y_offset % 2;

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

    i_src_pix_pitch = p_src_pic->p->i_pixel_pitch;
    i_src_pitch = p_src_pic->p->i_pitch;
    p_src = p_src_pic->p->p_pixels +
            p_filter->fmt_in.video.i_x_offset * i_src_pix_pitch +
            p_src_pic->p->i_pitch * p_filter->fmt_in.video.i_y_offset;


    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst_y += i_dst_pitch,
         p_dst_u += b_even_scanline ? i_dst_pitch/2 : 0,
         p_dst_v += b_even_scanline ? i_dst_pitch/2 : 0,
         p_src += i_src_pitch )
    {
        b_even_scanline = !b_even_scanline;

        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            const int R = p_src[i_x * i_src_pix_pitch + 0];
            const int G = p_src[i_x * i_src_pix_pitch + 1];
            const int B = p_src[i_x * i_src_pix_pitch + 2];

            i_trans = vlc_alpha( p_src[i_x * i_src_pix_pitch + 3], i_alpha );
            if( !i_trans )
                continue;

            /* Blending */
            rgb_to_yuv( &y, &u, &v, R, G, B );

            p_dst_y[i_x] = vlc_blend( y, p_dst_y[i_x], i_trans );
            if( b_even_scanline && i_x % 2 == 0 )
            {
                p_dst_u[i_x/2] = vlc_blend( u, p_dst_u[i_x/2], i_trans );
                p_dst_v[i_x/2] = vlc_blend( v, p_dst_v[i_x/2], i_trans );
            }
        }
    }
}

static void BlendRGBAR24( filter_t *p_filter,
                          picture_t *p_dst_pic, picture_t *p_src_pic,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src;
    int i_x, i_y, i_pix_pitch, i_trans, i_src_pix_pitch;
    int i_rindex, i_gindex, i_bindex;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src_pix_pitch = p_src_pic->p->i_pixel_pitch;
    i_src_pitch = p_src_pic->p->i_pitch;
    p_src = p_src_pic->p->p_pixels +
            p_filter->fmt_in.video.i_x_offset * i_src_pix_pitch +
            p_src_pic->p->i_pitch * p_filter->fmt_in.video.i_y_offset;

    vlc_rgb_index( &i_rindex, &i_gindex, &i_bindex, &p_filter->fmt_out.video );

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src += i_src_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            const int R = p_src[i_x * i_src_pix_pitch + 0];
            const int G = p_src[i_x * i_src_pix_pitch + 1];
            const int B = p_src[i_x * i_src_pix_pitch + 2];

            i_trans = vlc_alpha( p_src[i_x * i_src_pix_pitch + 3], i_alpha );
            if( !i_trans )
                continue;

            /* Blending */
            vlc_blend_packed( &p_dst[i_x * i_pix_pitch],
                              i_rindex, i_gindex, i_bindex,
                              R, G, B, i_trans, true );
        }
    }
}

static void BlendRGBAR16( filter_t *p_filter,
                          picture_t *p_dst_pic, picture_t *p_src_pic,
                          int i_x_offset, int i_y_offset,
                          int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch;
    uint8_t *p_dst, *p_src;
    int i_x, i_y, i_pix_pitch, i_trans, i_src_pix_pitch;

    i_pix_pitch = p_dst_pic->p->i_pixel_pitch;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src_pix_pitch = p_src_pic->p->i_pixel_pitch;
    i_src_pitch = p_src_pic->p->i_pitch;
    p_src = p_src_pic->p->p_pixels +
            p_filter->fmt_in.video.i_x_offset * i_src_pix_pitch +
            p_src_pic->p->i_pitch * p_filter->fmt_in.video.i_y_offset;

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch, p_src += i_src_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++ )
        {
            const int R = p_src[i_x * i_src_pix_pitch + 0];
            const int G = p_src[i_x * i_src_pix_pitch + 1];
            const int B = p_src[i_x * i_src_pix_pitch + 2];

            i_trans = vlc_alpha( p_src[i_x * i_src_pix_pitch + 3], i_alpha );
            if( !i_trans )
                continue;

            /* Blending */
            vlc_blend_rgb16( (uint16_t*)&p_dst[i_x * i_pix_pitch],
                             R, G, B, i_trans, &p_filter->fmt_out.video );
        }
    }
}

static void BlendRGBAYUVPacked( filter_t *p_filter,
                                picture_t *p_dst_pic, picture_t *p_src_pic,
                                int i_x_offset, int i_y_offset,
                                int i_width, int i_height, int i_alpha )
{
    int i_src_pitch, i_dst_pitch, i_src_pix_pitch;
    uint8_t *p_dst, *p_src;
    int i_x, i_y, i_pix_pitch, i_trans;
    bool b_even = !((i_x_offset + p_filter->fmt_out.video.i_x_offset)%2);
    int i_l_offset, i_u_offset, i_v_offset;
    uint8_t y, u, v;

    vlc_yuv_packed_index( &i_l_offset, &i_u_offset, &i_v_offset,
                          p_filter->fmt_out.video.i_chroma );

    i_pix_pitch = 2;
    i_dst_pitch = p_dst_pic->p->i_pitch;
    p_dst = p_dst_pic->p->p_pixels + i_x_offset * i_pix_pitch +
            p_filter->fmt_out.video.i_x_offset * i_pix_pitch +
            p_dst_pic->p->i_pitch *
            ( i_y_offset + p_filter->fmt_out.video.i_y_offset );

    i_src_pix_pitch = p_src_pic->p->i_pixel_pitch;
    i_src_pitch = p_src_pic->p->i_pitch;
    p_src = p_src_pic->p->p_pixels +
            p_filter->fmt_in.video.i_x_offset * i_src_pitch +
            p_src_pic->p->i_pitch * p_filter->fmt_in.video.i_y_offset;

    i_width &= ~1; /* Needs to be a multiple of 2 */

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < i_height; i_y++,
         p_dst += i_dst_pitch,
         p_src += i_src_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0; i_x < i_width; i_x++, b_even = !b_even )
        {
            const int R = p_src[i_x * i_src_pix_pitch + 0];
            const int G = p_src[i_x * i_src_pix_pitch + 1];
            const int B = p_src[i_x * i_src_pix_pitch + 2];

            i_trans = vlc_alpha( p_src[i_x * i_src_pix_pitch + 3], i_alpha );
            if( !i_trans )
                continue;

            /* Blending */
            rgb_to_yuv( &y, &u, &v, R, G, B );

            vlc_blend_packed( &p_dst[i_x * 2],
                              i_l_offset, i_u_offset, i_v_offset,
                              y, u, v, i_trans, b_even );
        }
    }
}
