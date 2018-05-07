/*****************************************************************************
 * yuvp.c: YUVP to YUVA/RGBA chroma converter
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar < fenrir @ videolan.org >
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
#include <assert.h>

/* TODO:
 *  Add anti-aliasing support (specially for DVD where only 4 colors are used)
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("YUVP converter") )
    set_capability( "video converter", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static picture_t *Convert_Filter( filter_t *, picture_t * );
static void Convert( filter_t *, picture_t *, picture_t * );
static void Yuv2Rgb( uint8_t *r, uint8_t *g, uint8_t *b, int y1, int u1, int v1 );

/*****************************************************************************
 * Open: probe the filter and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;

    /* It only supports YUVP to YUVA/RGBA without scaling
     * (if scaling is required another filter can do it) */
    if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_YUVP ||
        ( p_filter->fmt_out.video.i_chroma != VLC_CODEC_YUVA &&
          p_filter->fmt_out.video.i_chroma != VLC_CODEC_RGBA &&
          p_filter->fmt_out.video.i_chroma != VLC_CODEC_ARGB &&
          p_filter->fmt_out.video.i_chroma != VLC_CODEC_BGRA ) ||
        p_filter->fmt_in.video.i_width  != p_filter->fmt_out.video.i_width ||
        p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height ||
        p_filter->fmt_in.video.orientation != p_filter->fmt_out.video.orientation )
    {
        return VLC_EGENERIC;
    }

    p_filter->pf_video_filter = Convert_Filter;

    msg_Dbg( p_filter, "YUVP to %4.4s converter",
             (const char*)&p_filter->fmt_out.video.i_chroma );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: clean up the filter
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    VLC_UNUSED(p_this );
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************/
VIDEO_FILTER_WRAPPER( Convert )

static void Convert( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    const video_palette_t *p_yuvp = p_filter->fmt_in.video.p_palette;

    assert( p_yuvp != NULL );
    assert( p_filter->fmt_in.video.i_chroma == VLC_CODEC_YUVP );
    assert( p_filter->fmt_in.video.i_width == p_filter->fmt_out.video.i_width );
    assert( p_filter->fmt_in.video.i_height == p_filter->fmt_out.video.i_height );

    if( p_filter->fmt_out.video.i_chroma == VLC_CODEC_YUVA )
    {
        for( unsigned int y = 0; y < p_filter->fmt_in.video.i_height; y++ )
        {
            const uint8_t *p_line = &p_source->p->p_pixels[y*p_source->p->i_pitch];
            uint8_t *p_y = &p_dest->Y_PIXELS[y*p_dest->Y_PITCH];
            uint8_t *p_u = &p_dest->U_PIXELS[y*p_dest->U_PITCH];
            uint8_t *p_v = &p_dest->V_PIXELS[y*p_dest->V_PITCH];
            uint8_t *p_a = &p_dest->A_PIXELS[y*p_dest->A_PITCH];

            for( unsigned int x = 0; x < p_filter->fmt_in.video.i_width; x++ )
            {
                const int v = p_line[x];

                if( v > p_yuvp->i_entries )  /* maybe assert ? */
                    continue;

                p_y[x] = p_yuvp->palette[v][0];
                p_u[x] = p_yuvp->palette[v][1];
                p_v[x] = p_yuvp->palette[v][2];
                p_a[x] = p_yuvp->palette[v][3];
            }
        }
    }
    else
    {
        video_palette_t rgbp;
        int r, g, b, a;

        switch( p_filter->fmt_out.video.i_chroma )
        {
            case VLC_CODEC_ARGB: r = 1, g = 2, b = 3, a = 0; break;
            case VLC_CODEC_RGBA: r = 0, g = 1, b = 2, a = 3; break;
            case VLC_CODEC_BGRA: r = 2, g = 1, b = 0, a = 3; break;
            default:
                vlc_assert_unreachable();
        }
        /* Create a RGBA palette */
        rgbp.i_entries = p_yuvp->i_entries;
        for( int i = 0; i < p_yuvp->i_entries; i++ )
        {
            if( p_yuvp->palette[i][3] == 0 )
            {
                memset( rgbp.palette[i], 0, sizeof( rgbp.palette[i] ) );
                continue;
            }
            Yuv2Rgb( &rgbp.palette[i][r], &rgbp.palette[i][g], &rgbp.palette[i][b],
                     p_yuvp->palette[i][0], p_yuvp->palette[i][1], p_yuvp->palette[i][2] );
            rgbp.palette[i][a] = p_yuvp->palette[i][3];
        }

        for( unsigned int y = 0; y < p_filter->fmt_in.video.i_height; y++ )
        {
            const uint8_t *p_line = &p_source->p->p_pixels[y*p_source->p->i_pitch];
            uint8_t *p_pixels = &p_dest->p->p_pixels[y*p_dest->p->i_pitch];

            for( unsigned int x = 0; x < p_filter->fmt_in.video.i_width; x++ )
            {
                const int v = p_line[x];

                if( v >= rgbp.i_entries )  /* maybe assert ? */
                    continue;

                p_pixels[4*x+0] = rgbp.palette[v][0];
                p_pixels[4*x+1] = rgbp.palette[v][1];
                p_pixels[4*x+2] = rgbp.palette[v][2];
                p_pixels[4*x+3] = rgbp.palette[v][3];
            }
        }

    }
}

/* FIXME copied from blend.c */
static inline uint8_t vlc_uint8( int v )
{
    if( v > 255 )
        return 255;
    else if( v < 0 )
        return 0;
    return v;
}
static void Yuv2Rgb( uint8_t *r, uint8_t *g, uint8_t *b, int y1, int u1, int v1 )
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
}
