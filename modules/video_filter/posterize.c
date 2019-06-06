/*****************************************************************************
 * posterize.c : Posterize video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2010 VLC authors and VideoLAN
 *
 * Authors: Branko Kokanovic <branko.kokanovic@gmail.com>
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

#include <assert.h>
#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create      ( vlc_object_t * );
static void Destroy     ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static void PlanarYUVPosterize( picture_t *, picture_t *, int);
static void PackedYUVPosterize( picture_t *, picture_t *, int);
static void RVPosterize( picture_t *, picture_t *, bool, int );
static void YuvPosterization( uint8_t *, uint8_t *, uint8_t *, uint8_t *,
                    uint8_t, uint8_t, uint8_t, uint8_t, int );

static const char *const ppsz_filter_options[] = {
    "level", NULL
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define POSTERIZE_LEVEL_TEXT N_("Posterize level")
#define POSTERIZE_LEVEL_LONGTEXT N_("Posterize level "\
                    "(number of colors is cube of this value)" )

#define CFG_PREFIX "posterize-"

vlc_module_begin ()
    set_description( N_("Posterize video filter") )
    set_shortname( N_("Posterize" ) )
    set_help( N_("Posterize video by lowering the number of colors") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter", 0 )
    add_integer_with_range( CFG_PREFIX "level", 6, 2, 256,
                           POSTERIZE_LEVEL_TEXT, POSTERIZE_LEVEL_LONGTEXT,
                           false )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * callback prototypes
 *****************************************************************************/
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * filter_sys_t: adjust filter method descriptor
 *****************************************************************************/
typedef struct
{
    atomic_int i_level;
} filter_sys_t;

/*****************************************************************************
 * Create: allocates Posterize video thread output method
 *****************************************************************************
 * This function allocates and initializes a Posterize vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        CASE_PLANAR_YUV_SQUARE
            break;
        CASE_PACKED_YUV_422
            break;
        case VLC_CODEC_RGB24:
            break;
        case VLC_CODEC_RGB32:
            break;
        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    atomic_init( &p_sys->i_level,
                 var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "level" ) );

    var_AddCallback( p_filter, CFG_PREFIX "level", FilterCallback, p_sys );

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Posterize video thread output method
 *****************************************************************************
 * Terminate an output method created by PosterizeCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, CFG_PREFIX "level", FilterCallback, p_sys );
    free( p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Posterize image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;

    if( !p_pic ) return NULL;

    filter_sys_t *p_sys = p_filter->p_sys;
    int level = atomic_load( &p_sys->i_level );

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        picture_Release( p_pic );
        return NULL;
    }

    switch( p_pic->format.i_chroma )
    {
        case VLC_CODEC_RGB24:
            RVPosterize( p_pic, p_outpic, false, level );
            break;
        case VLC_CODEC_RGB32:
            RVPosterize( p_pic, p_outpic, true, level );
            break;
        CASE_PLANAR_YUV_SQUARE
            PlanarYUVPosterize( p_pic, p_outpic, level );
            break;
        CASE_PACKED_YUV_422
            PackedYUVPosterize( p_pic, p_outpic, level );
            break;
        default:
            vlc_assert_unreachable();
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

/*****************************************************************************
 * For a given level, calculates new posterized value for pixel whose value x
 * is in range of 0-255
 *****************************************************************************/
#define POSTERIZE_PIXEL(x, level) \
       (((( x * level ) >> 8 ) * 255 ) / ( level - 1 ))

/*****************************************************************************
 * PlanarYUVPosterize: Posterize one frame of the planar YUV video
 *****************************************************************************
 * This function posterizes one frame of the video by iterating through video
 * lines. In every pass, start of Y, U and V planes is calculated and for
 * every pixel we calculate new values of YUV values.
 *****************************************************************************/
static void PlanarYUVPosterize( picture_t *p_pic, picture_t *p_outpic,
                               int i_level )
{
    uint8_t *p_in_y, *p_in_u, *p_in_v, *p_in_end_y, *p_line_end_y, *p_out_y,
            *p_out_u, *p_out_v;
    int i_current_line = 0;

    p_in_y = p_pic->p[Y_PLANE].p_pixels;
    p_in_end_y = p_in_y + p_pic->p[Y_PLANE].i_visible_lines
        * p_pic->p[Y_PLANE].i_pitch;
    p_out_y = p_outpic->p[Y_PLANE].p_pixels;

    /* iterate for every visible line in the frame */
    while( p_in_y < p_in_end_y )
    {
        p_line_end_y = p_in_y + p_pic->p[Y_PLANE].i_visible_pitch;
        /* calculate start of U plane line */
        p_in_u = p_pic->p[U_PLANE].p_pixels
            + p_pic->p[U_PLANE].i_pitch * ( i_current_line / 2 );
        p_out_u = p_outpic->p[U_PLANE].p_pixels
            + p_outpic->p[U_PLANE].i_pitch * ( i_current_line / 2 );
        /* calculate start of V plane line */
        p_in_v = p_pic->p[V_PLANE].p_pixels
            + p_pic->p[V_PLANE].i_pitch * ( i_current_line / 2 );
        p_out_v = p_outpic->p[V_PLANE].p_pixels
            + p_outpic->p[V_PLANE].i_pitch * ( i_current_line / 2 );
        /* iterate for every two pixels in line */
        while( p_in_y < p_line_end_y )
        {
            uint8_t y1, y2, u, v;
            uint8_t posterized_y1, posterized_y2, posterized_u, posterized_v;
            /* retrieve original YUV values */
            y1 = *p_in_y++;
            y2 = *p_in_y++;
            u = *p_in_u++;
            v = *p_in_v++;
            /* do posterization */
            YuvPosterization( &posterized_y1, &posterized_y2, &posterized_u,
                             &posterized_v, y1, y2, u, v, i_level );
            /* put posterized valued */
            *p_out_y++ = posterized_y1;
            *p_out_y++ = posterized_y2;
            *p_out_u++ = posterized_u;
            *p_out_v++ = posterized_v;
        }
        p_in_y += p_pic->p[Y_PLANE].i_pitch
            - p_pic->p[Y_PLANE].i_visible_pitch;
        p_out_y += p_outpic->p[Y_PLANE].i_pitch
            - p_outpic->p[Y_PLANE].i_visible_pitch;
        i_current_line++;
    }
}

/*****************************************************************************
 * PackedYUVPosterize: Posterize one frame of the packed YUV video
 *****************************************************************************
 * This function posterizes one frame of the video by iterating through video
 * lines. In every pass, we calculate new values for pixels (UYVY, VYUY, YUYV
 * and YVYU formats are supported)
 *****************************************************************************/
static void PackedYUVPosterize( picture_t *p_pic, picture_t *p_outpic, int i_level )
{
    uint8_t *p_in, *p_in_end, *p_line_end, *p_out;
    uint8_t y1, y2, u, v;

    p_in = p_pic->p[0].p_pixels;
    p_in_end = p_in + p_pic->p[0].i_visible_lines
        * p_pic->p[0].i_pitch;
    p_out = p_outpic->p[0].p_pixels;

    while( p_in < p_in_end )
    {
        p_line_end = p_in + p_pic->p[0].i_visible_pitch;
        while( p_in < p_line_end )
        {
            uint8_t posterized_y1, posterized_y2, posterized_u, posterized_v;
            /* extract proper pixel values */
            switch( p_pic->format.i_chroma )
            {
                case VLC_CODEC_UYVY:
                    u = *p_in++;
                    y1 = *p_in++;
                    v = *p_in++;
                    y2 = *p_in++;
                    break;
                case VLC_CODEC_VYUY:
                    v = *p_in++;
                    y1 = *p_in++;
                    u = *p_in++;
                    y2 = *p_in++;
                    break;
                case VLC_CODEC_YUYV:
                    y1 = *p_in++;
                    u = *p_in++;
                    y2 = *p_in++;
                    v = *p_in++;
                    break;
                case VLC_CODEC_YVYU:
                    y1 = *p_in++;
                    v = *p_in++;
                    y2 = *p_in++;
                    u = *p_in++;
                    break;
                default:
                    vlc_assert_unreachable();
            }
            /* do posterization */
            YuvPosterization( &posterized_y1, &posterized_y2, &posterized_u,
                             &posterized_v, y1, y2, u, v, i_level );
            /* put posterized values in proper place */
            switch( p_pic->format.i_chroma )
            {
                case VLC_CODEC_UYVY:
                    *p_out++ = posterized_u;
                    *p_out++ = posterized_y1;
                    *p_out++ = posterized_v;
                    *p_out++ = posterized_y2;
                    break;
                case VLC_CODEC_VYUY:
                    *p_out++ = posterized_v;
                    *p_out++ = posterized_y1;
                    *p_out++ = posterized_u;
                    *p_out++ = posterized_y2;
                    break;
                case VLC_CODEC_YUYV:
                    *p_out++ = posterized_y1;
                    *p_out++ = posterized_u;
                    *p_out++ = posterized_y2;
                    *p_out++ = posterized_v;
                    break;
                case VLC_CODEC_YVYU:
                    *p_out++ = posterized_y1;
                    *p_out++ = posterized_v;
                    *p_out++ = posterized_y2;
                    *p_out++ = posterized_u;
                    break;
                default:
                    vlc_assert_unreachable();
            }
        }
        p_in += p_pic->p[0].i_pitch - p_pic->p[0].i_visible_pitch;
        p_out += p_outpic->p[0].i_pitch
            - p_outpic->p[0].i_visible_pitch;
    }
}

/*****************************************************************************
 * RVPosterize: Posterize one frame of the RV24/RV32 video
 *****************************************************************************
 * This function posterizes one frame of the video by iterating through video
 * lines and calculating new values for every byte in chunks of 3 (RV24) or
 * 4 (RV32) bytes
 *****************************************************************************/
static void RVPosterize( picture_t *p_pic, picture_t *p_outpic,
                                 bool rv32, int level )
{
    uint8_t *p_in, *p_in_end, *p_line_end, *p_out, pixel;

    p_in = p_pic->p[0].p_pixels;
    p_in_end = p_in + p_pic->p[0].i_visible_lines
        * p_pic->p[0].i_pitch;
    p_out = p_outpic->p[0].p_pixels;

    while( p_in < p_in_end )
    {
        p_line_end = p_in + p_pic->p[0].i_visible_pitch;
        while( p_in < p_line_end )
        {
            pixel = *p_in++;
            *p_out++ = POSTERIZE_PIXEL( pixel, level );
            pixel = *p_in++;
            *p_out++ = POSTERIZE_PIXEL( pixel, level );
            pixel = *p_in++;
            *p_out++ = POSTERIZE_PIXEL( pixel, level );
            /* for rv32 we take 4 chunks at the time */
            if ( rv32 )
            {
                pixel = *p_in++;
                *p_out++ = POSTERIZE_PIXEL( pixel, level );
            }
        }
        p_in += p_pic->p[0].i_pitch - p_pic->p[0].i_visible_pitch;
        p_out += p_outpic->p[0].i_pitch
            - p_outpic->p[0].i_visible_pitch;
    }
}

/*****************************************************************************
 * YuvPosterization: Lowers the color depth of YUV color space
 *****************************************************************************
 * This function lowers the color level of YUV color space to specified level
 * by converting YUV color values to theirs RGB equivalents, calculates new
 * values and then converts RGB values to YUV values again.
 *****************************************************************************/
static void YuvPosterization( uint8_t* posterized_y1, uint8_t* posterized_y2,
                             uint8_t* posterized_u, uint8_t* posterized_v,
                             uint8_t y1, uint8_t y2, uint8_t u, uint8_t v,
                             int i_level ) {
    int r1, g1, b1; /* for y1 new value */
    int r2, b2, g2; /* for y2 new value */
    int r3, g3, b3; /* for new values of u and v */
    /* fist convert YUV -> RGB */
    yuv_to_rgb( &r1, &g1, &b1, y1, u, v );
    yuv_to_rgb( &r2, &g2, &b2, y1, u, v );
    yuv_to_rgb( &r3, &g3, &b3, ( y1 + y2 ) / 2, u, v );
    /* round RGB values to specified posterize level */
    r1 = POSTERIZE_PIXEL( r1, i_level );
    g1 = POSTERIZE_PIXEL( g1, i_level );
    b1 = POSTERIZE_PIXEL( b1, i_level );
    r2 = POSTERIZE_PIXEL( r2, i_level );
    g2 = POSTERIZE_PIXEL( g2, i_level );
    b2 = POSTERIZE_PIXEL( b2, i_level );
    r3 = POSTERIZE_PIXEL( r3, i_level );
    g3 = POSTERIZE_PIXEL( g3, i_level );
    b3 = POSTERIZE_PIXEL( b3, i_level );
    /* convert from calculated RGB -> YUV */
    *posterized_y1 = ( ( 66 * r1 + 129 * g1 +  25 * b1 + 128 ) >> 8 ) +  16;
    *posterized_y2 = ( ( 66 * r2 + 129 * g2 +  25 * b2 + 128 ) >> 8 ) +  16;
    *posterized_u = ( ( -38 * r3 -  74 * g3 + 112 * b3 + 128 ) >> 8 ) + 128;
    *posterized_v = ( ( 112 * r3 -  94 * g3 -  18 * b3 + 128 ) >> 8 ) + 128;
}

static int FilterCallback ( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)p_this; (void)oldval;
    filter_sys_t *p_sys = p_data;

    if( !strcmp( psz_var, CFG_PREFIX "level" ) )
        atomic_store( &p_sys->i_level, newval.i_int );

    return VLC_SUCCESS;
}
