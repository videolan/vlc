/*****************************************************************************
 * yuvp.c: YUVP to YUVA/RGBA chroma converter
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * $Id$
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
    set_capability( "video filter2", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static picture_t *Filter( filter_t *, picture_t * );
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
          p_filter->fmt_out.video.i_chroma != VLC_CODEC_RGBA ) ||
        p_filter->fmt_in.video.i_width  != p_filter->fmt_out.video.i_width ||
        p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height )
    {
        return VLC_EGENERIC;
    }

    p_filter->pf_video_filter = Filter;

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
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_out;

    if( !p_pic )
        return NULL;

    const video_palette_t *p_yuvp = p_filter->fmt_in.video.p_palette;

    assert( p_yuvp != NULL );
    assert( p_filter->fmt_in.video.i_chroma == VLC_CODEC_YUVP );
    assert( p_filter->fmt_in.video.i_width == p_filter->fmt_out.video.i_width );
    assert( p_filter->fmt_in.video.i_height == p_filter->fmt_out.video.i_height );

    /* Request output picture */
    p_out = filter_NewPicture( p_filter );
    if( !p_out )
    {
        picture_Release( p_pic );
        return NULL;
    }

    if( p_filter->fmt_out.video.i_chroma == VLC_CODEC_YUVA )
    {
        for( unsigned int y = 0; y < p_filter->fmt_in.video.i_height; y++ )
        {
            const uint8_t *p_line = &p_pic->p->p_pixels[y*p_pic->p->i_pitch];
            uint8_t *p_y = &p_out->Y_PIXELS[y*p_out->Y_PITCH];
            uint8_t *p_u = &p_out->U_PIXELS[y*p_out->U_PITCH];
            uint8_t *p_v = &p_out->V_PIXELS[y*p_out->V_PITCH];
            uint8_t *p_a = &p_out->A_PIXELS[y*p_out->A_PITCH];

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
        assert( p_filter->fmt_out.video.i_chroma == VLC_CODEC_RGBA );

        /* Create a RGBA palette */
        video_palette_t rgbp;
        rgbp.i_entries = p_yuvp->i_entries;
        for( int i = 0; i < p_yuvp->i_entries; i++ )
        {
            Yuv2Rgb( &rgbp.palette[i][0], &rgbp.palette[i][1], &rgbp.palette[i][2],
                     p_yuvp->palette[i][0], p_yuvp->palette[i][1], p_yuvp->palette[i][2] );
            rgbp.palette[i][3] = p_yuvp->palette[i][3];
        }

        /* */
        for( unsigned int y = 0; y < p_filter->fmt_in.video.i_height; y++ )
        {
            const uint8_t *p_line = &p_pic->p->p_pixels[y*p_pic->p->i_pitch];
            uint8_t *p_rgba = &p_out->p->p_pixels[y*p_out->p->i_pitch];

            for( unsigned int x = 0; x < p_filter->fmt_in.video.i_width; x++ )
            {
                const int v = p_line[x];

                if( v >= rgbp.i_entries )  /* maybe assert ? */
                    continue;

                p_rgba[4*x+0] = rgbp.palette[v][0];
                p_rgba[4*x+1] = rgbp.palette[v][1];
                p_rgba[4*x+2] = rgbp.palette[v][2];
                p_rgba[4*x+3] = rgbp.palette[v][3];
            }
        }
    }

    picture_CopyProperties( p_out, p_pic );
    picture_Release( p_pic );
    return p_out;
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
