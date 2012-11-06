/*****************************************************************************
 * motiondetect.c : Second version of a motion detection plugin.
 *****************************************************************************
 * Copyright (C) 2000-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <vlc_sout.h>

#include <vlc_filter.h>
#include "filter_picture.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

#define FILTER_PREFIX "motiondetect-"

vlc_module_begin ()
    set_description( N_("Motion detect video filter") )
    set_shortname( N_( "Motion Detect" ))
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )

    add_shortcut( "motion" )
    set_callbacks( Create, Destroy )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_t *Filter( filter_t *, picture_t * );
static void GaussianConvolution( uint32_t *, uint32_t *, int, int, int );
static int FindShapes( uint32_t *, uint32_t *, int, int, int,
                       int *, int *, int *, int *, int *);
static void Draw( filter_t *p_filter, uint8_t *p_pix, int i_pix_pitch, int i_pix_size );
#define NUM_COLORS (5000)

struct filter_sys_t
{
    bool is_yuv_planar;
    bool b_old;
    picture_t *p_old;
    uint32_t *p_buf;
    uint32_t *p_buf2;

    /* */
    int i_colors;
    int colors[NUM_COLORS];
    int color_x_min[NUM_COLORS];
    int color_x_max[NUM_COLORS];
    int color_y_min[NUM_COLORS];
    int color_y_max[NUM_COLORS];
};

/*****************************************************************************
 * Create
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    const video_format_t *p_fmt = &p_filter->fmt_in.video;
    filter_sys_t *p_sys;
    bool is_yuv_planar;

    switch( p_fmt->i_chroma )
    {
        CASE_PLANAR_YUV
            is_yuv_planar = true;
            break;

        CASE_PACKED_YUV_422
            is_yuv_planar = false;
            break;

        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                     (char*)&(p_fmt->i_chroma) );
            return VLC_EGENERIC;
    }
    p_filter->pf_video_filter = Filter;

    /* Allocate structure */
    p_filter->p_sys = p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->is_yuv_planar = is_yuv_planar;
    p_sys->b_old = false;
    p_sys->p_old = picture_NewFromFormat( p_fmt );
    p_sys->p_buf  = calloc( p_fmt->i_width * p_fmt->i_height, sizeof(*p_sys->p_buf) );
    p_sys->p_buf2 = calloc( p_fmt->i_width * p_fmt->i_height, sizeof(*p_sys->p_buf) );

    if( !p_sys->p_old || !p_sys->p_buf || !p_sys->p_buf2 )
    {
        free( p_sys->p_buf2 );
        free( p_sys->p_buf );
        if( p_sys->p_old )
            picture_Release( p_sys->p_old );
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys->p_buf2 );
    free( p_sys->p_buf );
    picture_Release( p_sys->p_old );
    free( p_sys );
}


/*****************************************************************************
 * Filter YUV Planar/Packed
 *****************************************************************************/
static void PreparePlanar( filter_t *p_filter, picture_t *p_inpic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    const video_format_t *p_fmt = &p_filter->fmt_in.video;

    uint8_t *p_oldpix   = p_sys->p_old->p[Y_PLANE].p_pixels;
    const int i_old_pitch = p_sys->p_old->p[Y_PLANE].i_pitch;

    const uint8_t *p_inpix = p_inpic->p[Y_PLANE].p_pixels;
    const int i_src_pitch = p_inpic->p[Y_PLANE].i_pitch;

    /**
     * Substract Y planes
     */
    for( unsigned y = 0; y < p_fmt->i_height; y++ )
    {
        for( unsigned x = 0; x < p_fmt->i_width; x++ )
            p_sys->p_buf2[y*p_fmt->i_width+x] = abs( p_inpix[y*i_src_pitch+x] - p_oldpix[y*i_old_pitch+x] );
    }

    int i_chroma_dx;
    int i_chroma_dy;
    switch( p_inpic->format.i_chroma )
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
        case VLC_CODEC_YV12:
            i_chroma_dx = 2;
            i_chroma_dy = 2;
            break;

        case VLC_CODEC_I422:
        case VLC_CODEC_J422:
            i_chroma_dx = 2;
            i_chroma_dy = 1;
            break;

        default:
            msg_Warn( p_filter, "Not taking chroma into account" );
            return;
    }

    const uint8_t *p_inpix_u = p_inpic->p[U_PLANE].p_pixels;
    const uint8_t *p_inpix_v = p_inpic->p[V_PLANE].p_pixels;
    const int i_src_pitch_u = p_inpic->p[U_PLANE].i_pitch;
    const int i_src_pitch_v = p_inpic->p[V_PLANE].i_pitch;

    const uint8_t *p_oldpix_u = p_sys->p_old->p[U_PLANE].p_pixels;
    const uint8_t *p_oldpix_v = p_sys->p_old->p[V_PLANE].p_pixels;
    const int i_old_pitch_u = p_sys->p_old->p[U_PLANE].i_pitch;
    const int i_old_pitch_v = p_sys->p_old->p[V_PLANE].i_pitch;

    for( unsigned y = 0; y < p_fmt->i_height/i_chroma_dy; y++ )
    {
        for( unsigned x = 0; x < p_fmt->i_width/i_chroma_dx; x ++ )
        {
            const int d = abs( p_inpix_u[y*i_src_pitch_u+x] - p_oldpix_u[y*i_old_pitch_u+x] ) +
                          abs( p_inpix_v[y*i_src_pitch_v+x] - p_oldpix_v[y*i_old_pitch_v+x] );
            int i, j;

            for( j = 0; j < i_chroma_dy; j++ )
            {
                for( i = 0; i < i_chroma_dx; i++ )
                    p_sys->p_buf2[i_chroma_dy*p_fmt->i_width*j + i_chroma_dx*i] = d;
            }
        }
    }
}

static int PreparePacked( filter_t *p_filter, picture_t *p_inpic, int *pi_pix_offset )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    const video_format_t *p_fmt = &p_filter->fmt_in.video;

    int i_y_offset, i_u_offset, i_v_offset;
    if( GetPackedYuvOffsets( p_fmt->i_chroma,
                             &i_y_offset, &i_u_offset, &i_v_offset ) )
    {
        msg_Warn( p_filter, "Unsupported input chroma (%4.4s)",
                  (char*)&p_fmt->i_chroma );
        return VLC_EGENERIC;
    }
    *pi_pix_offset = i_y_offset;

    /* Substract all planes at once */
    uint8_t *p_oldpix   = p_sys->p_old->p[Y_PLANE].p_pixels;
    const int i_old_pitch = p_sys->p_old->p[Y_PLANE].i_pitch;

    const uint8_t *p_inpix = p_inpic->p[Y_PLANE].p_pixels;
    const int i_src_pitch = p_inpic->p[Y_PLANE].i_pitch;

    for( unsigned y = 0; y < p_fmt->i_height; y++ )
    {
        for( unsigned x = 0; x < p_fmt->i_width; x+=2 )
        {
            int d;
            d = abs( p_inpix[y*i_src_pitch+2*x+i_u_offset] - p_oldpix[y*i_old_pitch+2*x+i_u_offset] ) +
                abs( p_inpix[y*i_src_pitch+2*x+i_v_offset] - p_oldpix[y*i_old_pitch+2*x+i_v_offset] );

            for( int i = 0; i < 2; i++ )
                p_sys->p_buf2[y*p_fmt->i_width+x+i] =
                    abs( p_inpix[y*i_src_pitch+2*(x+i)+i_y_offset] - p_oldpix[y*i_old_pitch+2*(x+i)+i_y_offset] ) + d;
        }
    }
    return VLC_SUCCESS;
}

static picture_t *Filter( filter_t *p_filter, picture_t *p_inpic )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_inpic )
        return NULL;

    picture_t *p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_inpic );
        return NULL;
    }
    picture_Copy( p_outpic, p_inpic );

    if( !p_sys->b_old )
    {
        picture_Copy( p_sys->p_old, p_inpic );
        p_sys->b_old = true;
        goto exit;
    }

    int i_pix_offset;
    int i_pix_size;
    if( p_sys->is_yuv_planar )
    {
        PreparePlanar( p_filter, p_inpic );
        i_pix_offset = 0;
        i_pix_size = 1;
    }
    else
    {
        if( PreparePacked( p_filter, p_inpic, &i_pix_offset ) )
            goto exit;
        i_pix_size = 2;
    }

    /**
     * Get the areas where movement was detected
     */
    const video_format_t *p_fmt = &p_filter->fmt_in.video;
    p_sys->i_colors = FindShapes( p_sys->p_buf2, p_sys->p_buf, p_fmt->i_width, p_fmt->i_width, p_fmt->i_height,
                                  p_sys->colors, p_sys->color_x_min, p_sys->color_x_max, p_sys->color_y_min, p_sys->color_y_max );

    /**
     * Count final number of shapes
     * Draw rectangles (there can be more than 1 moving shape in 1 rectangle)
     */
    Draw( p_filter, &p_outpic->p[Y_PLANE].p_pixels[i_pix_offset], p_outpic->p[Y_PLANE].i_pitch, i_pix_size );

    /**
     * We're done. Lets keep a copy of the picture
     * TODO we may just picture_Release with a latency of 1 if the filters/vout
     * handle it correctly */
    picture_Copy( p_sys->p_old, p_inpic );

exit:
    picture_Release( p_inpic );
    return p_outpic;
}


/*****************************************************************************
 * Gaussian Convolution
 *****************************************************************************
 *    Gaussian convolution ( sigma == 1.4 )
 *
 *    |  2  4  5  4  2  |   |  2  4  4  4  2 |
 *    |  4  9 12  9  4  |   |  4  8 12  8  4 |
 *    |  5 12 15 12  5  | ~ |  4 12 16 12  4 |
 *    |  4  9 12  9  4  |   |  4  8 12  8  4 |
 *    |  2  4  5  4  2  |   |  2  4  4  4  2 |
 *****************************************************************************/
static void GaussianConvolution( uint32_t *p_inpix, uint32_t *p_smooth,
                                 int i_src_pitch, int i_num_lines,
                                 int i_src_visible )
{
    int x,y;

    /* A bit overkill but ... simpler */
    memset( p_smooth, 0, sizeof(*p_smooth) * i_src_pitch * i_num_lines );

    for( y = 2; y < i_num_lines - 2; y++ )
    {
        for( x = 2; x < i_src_visible - 2; x++ )
        {
            p_smooth[y*i_src_visible+x] = (uint32_t)(
              /* 2 rows up */
                ( p_inpix[(y-2)*i_src_pitch+x-2] )
              + ((p_inpix[(y-2)*i_src_pitch+x-1]
              +   p_inpix[(y-2)*i_src_pitch+x]
              +   p_inpix[(y-2)*i_src_pitch+x+1])<<1 )
              + ( p_inpix[(y-2)*i_src_pitch+x+2] )
              /* 1 row up */
              + ((p_inpix[(y-1)*i_src_pitch+x-2]
              + ( p_inpix[(y-1)*i_src_pitch+x-1]<<1 )
              + ( p_inpix[(y-1)*i_src_pitch+x]*3 )
              + ( p_inpix[(y-1)*i_src_pitch+x+1]<<1 )
              +   p_inpix[(y-1)*i_src_pitch+x+2]
              /* */
              +   p_inpix[y*i_src_pitch+x-2]
              + ( p_inpix[y*i_src_pitch+x-1]*3 )
              + ( p_inpix[y*i_src_pitch+x]<<2 )
              + ( p_inpix[y*i_src_pitch+x+1]*3 )
              +   p_inpix[y*i_src_pitch+x+2]
              /* 1 row down */
              +   p_inpix[(y+1)*i_src_pitch+x-2]
              + ( p_inpix[(y+1)*i_src_pitch+x-1]<<1 )
              + ( p_inpix[(y+1)*i_src_pitch+x]*3 )
              + ( p_inpix[(y+1)*i_src_pitch+x+1]<<1 )
              +   p_inpix[(y+1)*i_src_pitch+x+2] )<<1 )
              /* 2 rows down */
              + ( p_inpix[(y+2)*i_src_pitch+x-2] )
              + ((p_inpix[(y+2)*i_src_pitch+x-1]
              +   p_inpix[(y+2)*i_src_pitch+x]
              +   p_inpix[(y+2)*i_src_pitch+x+1])<<1 )
              + ( p_inpix[(y+2)*i_src_pitch+x+2] )
              ) >> 6 /* 115 */;
        }
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static int FindShapes( uint32_t *p_diff, uint32_t *p_smooth,
                       int i_pitch, int i_visible, int i_lines,
                       int *colors,
                       int *color_x_min, int *color_x_max,
                       int *color_y_min, int *color_y_max )
{
    int last = 1;
    int i, j;

    /**
     * Apply some smoothing to remove noise
     */
    GaussianConvolution( p_diff, p_smooth, i_pitch, i_lines, i_visible );

    /**
     * Label the shapes and build the labels dependencies list
     */
    for( j = 0; j < i_pitch; j++ )
    {
        p_smooth[j] = 0;
        p_smooth[(i_lines-1)*i_pitch+j] = 0;
    }
    for( i = 1; i < i_lines-1; i++ )
    {
        p_smooth[i*i_pitch] = 0;
        for( j = 1; j < i_pitch-1; j++ )
        {
            if( p_smooth[i*i_pitch+j] > 15 )
            {
                if( p_smooth[(i-1)*i_pitch+j-1] )
                {
                    p_smooth[i*i_pitch+j] = p_smooth[(i-1)*i_pitch+j-1];
                }
                else if( p_smooth[(i-1)*i_pitch+j] )
                    p_smooth[i*i_pitch+j] = p_smooth[(i-1)*i_pitch+j];
                else if( p_smooth[i*i_pitch+j-1] )
                    p_smooth[i*i_pitch+j] = p_smooth[i*i_pitch+j-1];
                else
                {
                    if( last < NUM_COLORS )
                    {
                        p_smooth[i*i_pitch+j] = last;
                        colors[last] = last;
                        last++;
                    }
                }
                #define CHECK( A ) \
                if( p_smooth[A] && p_smooth[A] != p_smooth[i*i_pitch+j] ) \
                { \
                    if( p_smooth[A] < p_smooth[i*i_pitch+j] ) \
                        colors[p_smooth[i*i_pitch+j]] = p_smooth[A]; \
                    else \
                        colors[p_smooth[A]] = p_smooth[i*i_pitch+j]; \
                }
                CHECK( i*i_pitch+j-1 );
                CHECK( (i-1)*i_pitch+j-1 );
                CHECK( (i-1)*i_pitch+j );
                CHECK( (i-1)*i_pitch+j+1 );
                #undef CHECK
            }
            else
            {
                p_smooth[i*i_pitch+j] = 0;
            }
        }
        p_smooth[i*i_pitch+j] = 0;
    }

    /**
     * Initialise empty rectangle list
     */
    for( i = 1; i < last; i++ )
    {
        color_x_min[i] = -1;
        color_x_max[i] = -1;
        color_y_min[i] = -1;
        color_y_max[i] = -1;
    }

    /**
     * Compute rectangle coordinates
     */
    for( i = 0; i < i_pitch * i_lines; i++ )
    {
        if( p_smooth[i] )
        {
            while( colors[p_smooth[i]] != (int)p_smooth[i] )
                p_smooth[i] = colors[p_smooth[i]];
            if( color_x_min[p_smooth[i]] == -1 )
            {
                color_x_min[p_smooth[i]] =
                color_x_max[p_smooth[i]] = i % i_pitch;
                color_y_min[p_smooth[i]] =
                color_y_max[p_smooth[i]] = i / i_pitch;
            }
            else
            {
                int x = i % i_pitch, y = i / i_pitch;
                if( x < color_x_min[p_smooth[i]] )
                    color_x_min[p_smooth[i]] = x;
                if( x > color_x_max[p_smooth[i]] )
                    color_x_max[p_smooth[i]] = x;
                if( y < color_y_min[p_smooth[i]] )
                    color_y_min[p_smooth[i]] = y;
                if( y > color_y_max[p_smooth[i]] )
                    color_y_max[p_smooth[i]] = y;
            }
        }
    }

    /**
     * Merge overlaping rectangles
     */
    for( i = 1; i < last; i++ )
    {
        if( colors[i] != i ) continue;
        if( color_x_min[i] == -1 ) continue;
        for( j = i+1; j < last; j++ )
        {
            if( colors[j] != j ) continue;
            if( color_x_min[j] == -1 ) continue;
            if( __MAX( color_x_min[i], color_x_min[j] ) < __MIN( color_x_max[i], color_x_max[j] ) &&
                __MAX( color_y_min[i], color_y_min[j] ) < __MIN( color_y_max[i], color_y_max[j] ) )
            {
                color_x_min[i] = __MIN( color_x_min[i], color_x_min[j] );
                color_x_max[i] = __MAX( color_x_max[i], color_x_max[j] );
                color_y_min[i] = __MIN( color_y_min[i], color_y_min[j] );
                color_y_max[i] = __MAX( color_y_max[i], color_y_max[j] );
                color_x_min[j] = -1;
                j = 0;
            }
        }
    }

    return last;
}

static void Draw( filter_t *p_filter, uint8_t *p_pix, int i_pix_pitch, int i_pix_size )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    int i, j;

    for( i = 1, j = 0; i < p_sys->i_colors; i++ )
    {
        int x, y;

        if( p_sys->colors[i] != i )
            continue;

        const int color_x_min = p_sys->color_x_min[i];
        const int color_x_max = p_sys->color_x_max[i];
        const int color_y_min = p_sys->color_y_min[i];
        const int color_y_max = p_sys->color_y_max[i];

        if( color_x_min == -1 )
            continue;
        if( ( color_y_max - color_y_min ) * ( color_x_max - color_x_min ) < 16 )
            continue;

        j++;

        y = color_y_min;
        for( x = color_x_min; x <= color_x_max; x++ )
            p_pix[y*i_pix_pitch+x*i_pix_size] = 0xff;

        y = color_y_max;
        for( x = color_x_min; x <= color_x_max; x++ )
            p_pix[y*i_pix_pitch+x*i_pix_size] = 0xff;

        x = color_x_min;
        for( y = color_y_min; y <= color_y_max; y++ )
            p_pix[y*i_pix_pitch+x*i_pix_size] = 0xff;

        x = color_x_max;
        for( y = color_y_min; y <= color_y_max; y++ )
            p_pix[y*i_pix_pitch+x*i_pix_size] = 0xff;
    }
    msg_Dbg( p_filter, "Counted %d moving shapes.", j );
}
