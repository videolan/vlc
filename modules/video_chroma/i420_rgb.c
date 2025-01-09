/*****************************************************************************
 * i420_rgb.c : YUV to bitmap RGB conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2004, 2008 VLC authors and VideoLAN
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damienf@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_cpu.h>
#include <vlc_chroma_probe.h>

#include "i420_rgb.h"
#include "../video_filter/filter_picture.h"
#ifdef PLUGIN_PLAIN
# include "i420_rgb_c.h"

static void SetYUV( filter_t * );
static void Set8bppPalette( filter_t *, uint8_t * );
#endif

/*****************************************************************************
 * RGB2PIXEL: assemble RGB components to a pixel value, returns a uint32_t
 *****************************************************************************/
#define RGB2PIXEL( i_r, i_g, i_b )       \
    ((((i_r) >> i_rrshift) << i_lrshift) \
   | (((i_g) >> i_rgshift) << i_lgshift) \
   | (((i_b) >> i_rbshift) << i_lbshift))

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Activate   ( filter_t * );
static void Deactivate ( filter_t * );

static void ProbeChroma(vlc_chroma_conv_vec *vec)
{
#define OUT_CHROMAS_COMMON VLC_CODEC_RGB565, VLC_CODEC_RGB555, VLC_CODEC_XRGB, \
    VLC_CODEC_RGBX, VLC_CODEC_BGRX, VLC_CODEC_XBGR

#ifndef PLUGIN_PLAIN
#define OUT_CHROMAS OUT_CHROMAS_COMMON
#else
#define OUT_CHROMAS OUT_CHROMAS_COMMON, VLC_CODEC_RGB233, VLC_CODEC_RGB332, \
    VLC_CODEC_BGR233, VLC_CODEC_BGR565, VLC_CODEC_BGR555
#endif

#if defined (PLUGIN_SSE2)
#define COST 0.75
#else
#define COST 1
#endif

    vlc_chroma_conv_add_in_outlist(vec, COST, VLC_CODEC_YV12, OUT_CHROMAS);
    vlc_chroma_conv_add_in_outlist(vec, COST, VLC_CODEC_I420, OUT_CHROMAS);
}

vlc_module_begin ()
#if defined (PLUGIN_SSE2)
    set_description( N_( "SSE2 I420,IYUV,YV12 to "
                        "RV15,RV16,RV24,RV32 conversions") )
    set_callback_video_converter( Activate, 120 )
# define vlc_CPU_capable() vlc_CPU_SSE2()
#else
    set_description( N_("I420,IYUV,YV12 to "
                       "RGB8,RV15,RV16,RV24,RV32 conversions") )
    set_callback_video_converter( Activate, 80 )
# define vlc_CPU_capable() (true)
#endif
    add_submodule()
        set_callback_chroma_conv_probe(ProbeChroma)
vlc_module_end ()

#ifndef PLUGIN_PLAIN
VIDEO_FILTER_WRAPPER_CLOSE_EXT( I420_R5G5B5, Deactivate )
VIDEO_FILTER_WRAPPER_CLOSE_EXT( I420_R5G6B5, Deactivate )
VIDEO_FILTER_WRAPPER_CLOSE_EXT( I420_A8R8G8B8, Deactivate )
VIDEO_FILTER_WRAPPER_CLOSE_EXT( I420_R8G8B8A8, Deactivate )
VIDEO_FILTER_WRAPPER_CLOSE_EXT( I420_B8G8R8A8, Deactivate )
VIDEO_FILTER_WRAPPER_CLOSE_EXT( I420_A8B8G8R8, Deactivate )
#else
VIDEO_FILTER_WRAPPER_CLOSE_EXT( I420_RGB8, Deactivate )
VIDEO_FILTER_WRAPPER_CLOSE_EXT( I420_RGB16, Deactivate )
VIDEO_FILTER_WRAPPER_CLOSE_EXT( I420_RGB32, Deactivate )
#endif

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Activate( filter_t *p_filter )
{
    if( !vlc_CPU_capable() )
        return VLC_EGENERIC;
    if( p_filter->fmt_out.video.i_width & 1
     || p_filter->fmt_out.video.i_height & 1 )
    {
        return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.orientation != p_filter->fmt_out.video.orientation )
    {
        return VLC_EGENERIC;
    }

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_CODEC_YV12:
        case VLC_CODEC_I420:
            switch( p_filter->fmt_out.video.i_chroma )
            {
#ifndef PLUGIN_PLAIN
                case VLC_CODEC_RGB565:
                    /* R5G6B5 pixel format */
                    msg_Dbg(p_filter, "RGB pixel format is R5G6B5");
                    p_filter->ops = &I420_R5G6B5_ops;
                    break;
                case VLC_CODEC_RGB555:
                    /* R5G5B5 pixel format */
                    msg_Dbg(p_filter, "RGB pixel format is R5G5B5");
                    p_filter->ops = &I420_R5G5B5_ops;
                    break;
                case VLC_CODEC_XRGB:
                    /* A8R8G8B8 pixel format */
                    msg_Dbg(p_filter, "RGB pixel format is XBGR");
                    p_filter->ops = &I420_A8R8G8B8_ops;
                    break;
                case VLC_CODEC_RGBX:
                    /* R8G8B8A8 pixel format */
                    msg_Dbg(p_filter, "RGB pixel format is RGBX");
                    p_filter->ops = &I420_R8G8B8A8_ops;
                    break;
                case VLC_CODEC_BGRX:
                    /* B8G8R8A8 pixel format */
                    msg_Dbg(p_filter, "RGB pixel format is BGRX");
                    p_filter->ops = &I420_B8G8R8A8_ops;
                    break;
                case VLC_CODEC_XBGR:
                    /* A8B8G8R8 pixel format */
                    msg_Dbg(p_filter, "RGB pixel format is XBGR");
                    p_filter->ops = &I420_A8B8G8R8_ops;
                    break;
#else
                case VLC_CODEC_RGB233:
                case VLC_CODEC_RGB332:
                case VLC_CODEC_BGR233:
                    p_filter->ops = &I420_RGB8_ops;
                    break;
                case VLC_CODEC_RGB565:
                case VLC_CODEC_BGR565:
                case VLC_CODEC_RGB555:
                case VLC_CODEC_BGR555:
                    p_filter->ops = &I420_RGB16_ops;
                    break;
                CASE_PACKED_RGBX
                    p_filter->ops = &I420_RGB32_ops;
                    break;
#endif
                default:
                    return VLC_EGENERIC;
            }
            break;

        default:
            return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
        return VLC_EGENERIC;
    p_filter->p_sys = p_sys;

    p_sys->i_buffer_size = 0;
    p_sys->p_buffer = NULL;
    switch( p_filter->fmt_out.video.i_chroma )
    {
#ifdef PLUGIN_PLAIN
        case VLC_CODEC_RGB233:
        case VLC_CODEC_RGB332:
        case VLC_CODEC_BGR233:
            p_sys->i_bytespp = 1;
            break;
#endif
        CASE_PACKED_RGB1615
            p_sys->i_bytespp = 2;
            break;
        CASE_PACKED_RGBX
        case VLC_CODEC_RGB24:
        case VLC_CODEC_BGR24:
            p_sys->i_bytespp = 4;
            break;
        default:
            free( p_sys );
            return VLC_EGENERIC;
    }

    p_sys->p_offset = malloc( p_filter->fmt_out.video.i_width
                    * ( ( p_sys->i_bytespp == 1 ) ? 2 : 1 )
                    * sizeof( int ) );
    if( p_sys->p_offset == NULL )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

#ifdef PLUGIN_PLAIN
    p_sys->p_base = malloc( p_sys->i_bytespp * RGB_TABLE_SIZE );
    if( p_sys->p_base == NULL )
    {
        free( p_sys->p_offset );
        free( p_sys );
        return -1;
    }

    SetYUV( p_filter );
#endif

    return 0;
}

/*****************************************************************************
 * Deactivate: free the chroma function
 *****************************************************************************
 * This function frees the previously allocated chroma function
 *****************************************************************************/
static void Deactivate( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

#ifdef PLUGIN_PLAIN
    free( p_sys->p_base );
#endif
    free( p_sys->p_offset );
    free( p_sys->p_buffer );
    free( p_sys );
}

#ifdef PLUGIN_PLAIN
/*****************************************************************************
 * SetYUV: compute tables and set function pointers
 *****************************************************************************/
static void SetYUV( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    unsigned i_lrshift, i_lgshift, i_lbshift;
    unsigned i_rrshift = 0;
    unsigned i_rgshift = 0;
    unsigned i_rbshift = 0;

    switch (p_filter->fmt_out.video.i_chroma)
    {
        case VLC_CODEC_XRGB:
        case VLC_CODEC_RGB24:
            i_lrshift = 16;
            i_lgshift =  8;
            i_lbshift =  0;
            break;
        case VLC_CODEC_XBGR:
        case VLC_CODEC_BGR24:
            i_lbshift = 16;
            i_lgshift =  8;
            i_lrshift =  0;
            break;
        case VLC_CODEC_RGBX:
            i_lrshift = 24;
            i_lgshift = 16;
            i_lbshift =  8;
            break;
        case VLC_CODEC_BGRX:
            i_lbshift = 24;
            i_lgshift = 16;
            i_lrshift =  8;
            break;
        case VLC_CODEC_RGB565BE:
        case VLC_CODEC_RGB565LE:
            i_lrshift = 11;
            i_lgshift = 6;
            i_lbshift = 0;
            i_rrshift = 3;
            i_rgshift = 2;
            i_rbshift = 3;
            break;
        case VLC_CODEC_BGR565BE:
        case VLC_CODEC_BGR565LE:
            i_lbshift = 11;
            i_lgshift = 6;
            i_lrshift = 0;
            i_rbshift = 3;
            i_rgshift = 2;
            i_rrshift = 3;
            break;
        case VLC_CODEC_RGB555BE:
        case VLC_CODEC_RGB555LE:
            i_lrshift = 10;
            i_lgshift = 5;
            i_lbshift = 0;
            i_rrshift = 3;
            i_rgshift = 3;
            i_rbshift = 3;
            break;
        case VLC_CODEC_BGR555BE:
        case VLC_CODEC_BGR555LE:
            i_lbshift = 10;
            i_lgshift = 5;
            i_lrshift = 0;
            i_rbshift = 3;
            i_rgshift = 3;
            i_rrshift = 3;
            break;
        case VLC_CODEC_RGB233:
            i_lrshift = 6;
            i_lgshift = 3;
            i_lbshift = 0;
            i_rrshift = 6;
            i_rgshift = 5;
            i_rbshift = 5;
            break;
        case VLC_CODEC_BGR233:
            i_lbshift = 6;
            i_lgshift = 3;
            i_lrshift = 0;
            i_rbshift = 6;
            i_rgshift = 5;
            i_rrshift = 5;
            break;
        case VLC_CODEC_RGB332:
            i_lrshift = 5;
            i_lgshift = 2;
            i_lbshift = 0;
            i_rrshift = 5;
            i_rgshift = 5;
            i_rbshift = 6;
            break;
        default:
            vlc_assert_unreachable();
    }

    /*
     * Set pointers and build YUV tables
     */

    /* Color: build red, green and blue tables */
    switch( p_filter->fmt_out.video.i_chroma )
    {
    case VLC_CODEC_RGB233:
    case VLC_CODEC_RGB332:
    case VLC_CODEC_BGR233:
        p_sys->p_rgb8 = (uint8_t *)p_sys->p_base;
        Set8bppPalette( p_filter, p_sys->p_rgb8 );
        break;

    CASE_PACKED_RGB1615
        p_sys->p_rgb16 = (uint16_t *)p_sys->p_base;
        for( unsigned i_index = 0; i_index < RED_MARGIN; i_index++ )
        {
            p_sys->p_rgb16[RED_OFFSET - RED_MARGIN + i_index] = 0;
            p_sys->p_rgb16[RED_OFFSET + 256 + i_index] =        RGB2PIXEL( 255, 0, 0 );
        }
        for( unsigned i_index = 0; i_index < GREEN_MARGIN; i_index++ )
        {
            p_sys->p_rgb16[GREEN_OFFSET - GREEN_MARGIN + i_index] = 0;
            p_sys->p_rgb16[GREEN_OFFSET + 256 + i_index] =          RGB2PIXEL( 0, 255, 0 );
        }
        for( unsigned i_index = 0; i_index < BLUE_MARGIN; i_index++ )
        {
            p_sys->p_rgb16[BLUE_OFFSET - BLUE_MARGIN + i_index] = 0;
            p_sys->p_rgb16[BLUE_OFFSET + BLUE_MARGIN + i_index] = RGB2PIXEL( 0, 0, 255 );
        }
        for( unsigned i_index = 0; i_index < 256; i_index++ )
        {
            p_sys->p_rgb16[RED_OFFSET + i_index] =   RGB2PIXEL( i_index, 0, 0 );
            p_sys->p_rgb16[GREEN_OFFSET + i_index] = RGB2PIXEL( 0, i_index, 0 );
            p_sys->p_rgb16[BLUE_OFFSET + i_index] =  RGB2PIXEL( 0, 0, i_index );
        }
        break;

    CASE_PACKED_RGBX
    case VLC_CODEC_RGB24:
    case VLC_CODEC_BGR24:
        p_sys->p_rgb32 = (uint32_t *)p_sys->p_base;
        for( unsigned i_index = 0; i_index < RED_MARGIN; i_index++ )
        {
            p_sys->p_rgb32[RED_OFFSET - RED_MARGIN + i_index] = 0;
            p_sys->p_rgb32[RED_OFFSET + 256 + i_index] =        RGB2PIXEL( 255, 0, 0 );
        }
        for( unsigned i_index = 0; i_index < GREEN_MARGIN; i_index++ )
        {
            p_sys->p_rgb32[GREEN_OFFSET - GREEN_MARGIN + i_index] = 0;
            p_sys->p_rgb32[GREEN_OFFSET + 256 + i_index] =          RGB2PIXEL( 0, 255, 0 );
        }
        for( unsigned i_index = 0; i_index < BLUE_MARGIN; i_index++ )
        {
            p_sys->p_rgb32[BLUE_OFFSET - BLUE_MARGIN + i_index] = 0;
            p_sys->p_rgb32[BLUE_OFFSET + BLUE_MARGIN + i_index] = RGB2PIXEL( 0, 0, 255 );
        }
        for( unsigned i_index = 0; i_index < 256; i_index++ )
        {
            p_sys->p_rgb32[RED_OFFSET + i_index] =   RGB2PIXEL( i_index, 0, 0 );
            p_sys->p_rgb32[GREEN_OFFSET + i_index] = RGB2PIXEL( 0, i_index, 0 );
            p_sys->p_rgb32[BLUE_OFFSET + i_index] =  RGB2PIXEL( 0, 0, i_index );
        }
        break;
    }
}

static void Set8bppPalette( filter_t *p_filter, uint8_t *p_rgb8 )
{
    #define CLIP( x ) ( ((x < 0) ? 0 : (x > 255) ? 255 : x) << 8 )
    filter_sys_t *p_sys = p_filter->p_sys;

    int y,u,v;
    int r,g,b;
    int i = 0, j = 0;
    uint16_t *p_cmap_r = p_sys->p_rgb_r;
    uint16_t *p_cmap_g = p_sys->p_rgb_g;
    uint16_t *p_cmap_b = p_sys->p_rgb_b;

    unsigned char p_lookup[PALETTE_TABLE_SIZE];

    /* This loop calculates the intersection of an YUV box and the RGB cube. */
    for ( y = 0; y <= 256; y += 16, i += 128 - 81 )
    {
        for ( u = 0; u <= 256; u += 32 )
        {
            for ( v = 0; v <= 256; v += 32 )
            {
                r = y + ( (V_RED_COEF*(v-128)) >> SHIFT );
                g = y + ( (U_GREEN_COEF*(u-128)
                         + V_GREEN_COEF*(v-128)) >> SHIFT );
                b = y + ( (U_BLUE_COEF*(u-128)) >> SHIFT );

                if( r >= 0x00 && g >= 0x00 && b >= 0x00
                        && r <= 0xff && g <= 0xff && b <= 0xff )
                {
                    /* This one should never happen unless someone
                     * fscked up my code */
                    if( j == 256 )
                    {
                        msg_Err( p_filter, "no colors left in palette" );
                        break;
                    }

                    /* Clip the colors */
                    p_cmap_r[ j ] = CLIP( r );
                    p_cmap_g[ j ] = CLIP( g );
                    p_cmap_b[ j ] = CLIP( b );

#if 0
            printf("+++Alloc RGB cmap %d (%d, %d, %d)\n", j,
               p_cmap_r[ j ] >>8, p_cmap_g[ j ] >>8,
               p_cmap_b[ j ] >>8);
#endif

                    /* Allocate color */
                    p_lookup[ i ] = 1;
                    p_rgb8[ i++ ] = j;
                    j++;
                }
                else
                {
                    p_lookup[ i ] = 0;
                    p_rgb8[ i++ ] = 0;
                }
            }
        }
    }

    /* The colors have been allocated, we can set the palette */
    /* FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
    p_filter->fmt_out.video.pf_setpalette( p_filter, p_cmap_r, p_cmap_g, p_cmap_b );*/

#if 0
    /* There will eventually be a way to know which colors
     * couldn't be allocated and try to find a replacement */
    p_vout->i_white_pixel = 0xff;
    p_vout->i_black_pixel = 0x00;
    p_vout->i_gray_pixel = 0x44;
    p_vout->i_blue_pixel = 0x3b;
#endif

    /* This loop allocates colors that got outside the RGB cube */
    for ( i = 0, y = 0; y <= 256; y += 16, i += 128 - 81 )
    {
        for ( u = 0; u <= 256; u += 32 )
        {
            for ( v = 0; v <= 256; v += 32, i++ )
            {
                int u2, v2, dist, mindist = 100000000;

                if( p_lookup[ i ] || y == 0 )
                {
                    continue;
                }

                /* Heavy. yeah. */
                for( u2 = 0; u2 <= 256; u2 += 32 )
                {
                    for( v2 = 0; v2 <= 256; v2 += 32 )
                    {
                        j = ((y>>4)<<7) + (u2>>5)*9 + (v2>>5);
                        dist = (u-u2)*(u-u2) + (v-v2)*(v-v2);

                        /* Find the nearest color */
                        if( p_lookup[ j ] && dist < mindist )
                        {
                            p_rgb8[ i ] = p_rgb8[ j ];
                            mindist = dist;
                        }

                        j -= 128;

                        /* Find the nearest color */
                        if( p_lookup[ j ] && dist + 128 < mindist )
                        {
                            p_rgb8[ i ] = p_rgb8[ j ];
                            mindist = dist + 128;
                        }
                    }
                }
            }
        }
    }
}
#endif
