/*****************************************************************************
 * extract.c : Extract RGB components
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea .t videolan d@t org>
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
#include "filter_picture.h"

#include "math.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create      ( vlc_object_t * );
static void Destroy     ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static int ExtractCallback( vlc_object_t *, char const *,
                            vlc_value_t, vlc_value_t, void * );

static void get_red_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_green_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_blue_from_yuv420( picture_t *, picture_t *, int, int, int );
static void get_red_from_yuv422( picture_t *, picture_t *, int, int, int );
static void get_green_from_yuv422( picture_t *, picture_t *, int, int, int );
static void get_blue_from_yuv422( picture_t *, picture_t *, int, int, int );
static void make_projection_matrix( filter_t *, int color, int *matrix );
static void get_custom_from_yuv420( picture_t *, picture_t *, int, int, int, int * );
static void get_custom_from_yuv422( picture_t *, picture_t *, int, int, int, int * );
static void get_custom_from_packedyuv422( picture_t *, picture_t *, int * );


#define COMPONENT_TEXT N_("RGB component to extract")
#define COMPONENT_LONGTEXT N_("RGB component to extract. 0 for Red, 1 for Green and 2 for Blue.")
#define FILTER_PREFIX "extract-"

static const int pi_component_values[] = { 0xFF0000, 0x00FF00, 0x0000FF };
static const char *const ppsz_component_descriptions[] = {
    "Red", "Green", "Blue" };

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Extract RGB component video filter") )
    set_shortname( N_("Extract" ))
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )
    add_shortcut( "extract" )

    add_integer_with_range( FILTER_PREFIX "component", 0xFF0000, 1, 0xFFFFFF,
                            COMPONENT_TEXT, COMPONENT_LONGTEXT, false )
        change_integer_list( pi_component_values, ppsz_component_descriptions )

    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "component", NULL
};

enum { RED=0xFF0000, GREEN=0x00FF00, BLUE=0x0000FF };
struct filter_sys_t
{
    vlc_mutex_t lock;
    int *projection_matrix;
    uint32_t i_color;
};

/*****************************************************************************
 * Create
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
        case VLC_CODEC_YV12:

        case VLC_CODEC_I422:
        case VLC_CODEC_J422:

        CASE_PACKED_YUV_422
            break;

        default:
            /* We only want planar YUV 4:2:0 or 4:2:2 */
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;
    p_filter->p_sys->projection_matrix = malloc( 9 * sizeof( int ) );
    if( !p_filter->p_sys->projection_matrix )
    {
        free( p_filter->p_sys );
        return VLC_ENOMEM;
    }

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    p_filter->p_sys->i_color = var_CreateGetIntegerCommand( p_filter,
                                               FILTER_PREFIX "component" );
    /* Matrix won't be used for RED, GREEN or BLUE in planar formats */
    make_projection_matrix( p_filter, p_filter->p_sys->i_color,
                            p_filter->p_sys->projection_matrix );
    vlc_mutex_init( &p_filter->p_sys->lock );
    var_AddCallback( p_filter, FILTER_PREFIX "component",
                     ExtractCallback, p_filter->p_sys );

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, FILTER_PREFIX "component", ExtractCallback,
                     p_sys );
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys->projection_matrix );
    free( p_sys );
}

/*****************************************************************************
 * Render
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    vlc_mutex_lock( &p_sys->lock );
    switch( p_pic->format.i_chroma )
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
        case VLC_CODEC_YV12:
            switch( p_sys->i_color )
            {
                case RED:
                    get_red_from_yuv420( p_pic, p_outpic,
                                         Y_PLANE, U_PLANE, V_PLANE );
                    break;
                case GREEN:
                    get_green_from_yuv420( p_pic, p_outpic,
                                           Y_PLANE, U_PLANE, V_PLANE );
                    break;
                case BLUE:
                    get_blue_from_yuv420( p_pic, p_outpic,
                                          Y_PLANE, U_PLANE, V_PLANE );
                    break;
                default:
                    get_custom_from_yuv420( p_pic, p_outpic,
                                            Y_PLANE, U_PLANE, V_PLANE,
                                            p_sys->projection_matrix);
                    break;
            }
            break;

        case VLC_CODEC_I422:
        case VLC_CODEC_J422:
            switch( p_filter->p_sys->i_color )
            {
                case RED:
                    get_red_from_yuv422( p_pic, p_outpic,
                                         Y_PLANE, U_PLANE, V_PLANE );
                    break;
                case GREEN:
                    get_green_from_yuv422( p_pic, p_outpic,
                                           Y_PLANE, U_PLANE, V_PLANE );
                    break;
                case BLUE:
                    get_blue_from_yuv422( p_pic, p_outpic,
                                          Y_PLANE, U_PLANE, V_PLANE );
                    break;
                default:
                    get_custom_from_yuv422( p_pic, p_outpic,
                                            Y_PLANE, U_PLANE, V_PLANE,
                                            p_sys->projection_matrix);
                    break;
            }
            break;

        CASE_PACKED_YUV_422
            get_custom_from_packedyuv422( p_pic, p_outpic,
                                          p_sys->projection_matrix );
            break;

        default:
            vlc_mutex_unlock( &p_sys->lock );
            msg_Warn( p_filter, "Unsupported input chroma (%4.4s)",
                      (char*)&(p_pic->format.i_chroma) );
            picture_Release( p_pic );
            return NULL;
    }
    vlc_mutex_unlock( &p_sys->lock );

    return CopyInfoAndRelease( p_outpic, p_pic );
}

#define U 128
#define V 128

static void mmult( double *res, double *a, double *b )
{
    int i, j, k;
    for( i = 0; i < 3; i++ )
    {
        for( j = 0; j < 3; j++ )
        {
            res[ i*3 + j ] = 0.;
            for( k = 0; k < 3; k++ )
            {
                res[ i*3 + j ] += a[ i*3 + k ] * b[ k*3 + j ];
            }
        }
    }
}
static void make_projection_matrix( filter_t *p_filter, int color, int *matrix )
{
    double left_matrix[9] =
        {  76.24500,  149.68500,  29.07000,
          -43.02765,  -84.47235, 127.50000,
          127.50000, -106.76534, -20.73466 };
    double right_matrix[9] =
        { 257.00392,   0.00000,  360.31950,
          257.00392, -88.44438, -183.53583,
          257.00392, 455.41095,    0.00000 };
    double red = ((double)(( 0xFF0000 & color )>>16))/255.;
    double green = ((double)(( 0x00FF00 & color )>>8))/255.;
    double blue = ((double)( 0x0000FF & color ))/255.;
    double norm = sqrt( red*red + green*green + blue*blue );
    if( norm > 0 )
    {
        red /= norm;
        green /= norm;
        blue /= norm;
    }
    /* XXX: We might still need to norm the rgb_matrix */
    double rgb_matrix[9] =
        { red*red,    red*green,   red*blue,
          red*green,  green*green, green*blue,
          red*blue,   green*blue,  blue*blue };
    double result1[9];
    double result[9];
    int i;
    msg_Dbg( p_filter, "red: %f", red );
    msg_Dbg( p_filter, "green: %f", green );
    msg_Dbg( p_filter, "blue: %f", blue );
    mmult( result1, rgb_matrix, right_matrix );
    mmult( result, left_matrix, result1 );
    for( i = 0; i < 9; i++ )
    {
        matrix[i] = (int)result[i];
    }
    msg_Dbg( p_filter, "Projection matrix:" );
    msg_Dbg( p_filter, "%6d %6d %6d", matrix[0], matrix[1], matrix[2] );
    msg_Dbg( p_filter, "%6d %6d %6d", matrix[3], matrix[4], matrix[5] );
    msg_Dbg( p_filter, "%6d %6d %6d", matrix[6], matrix[7], matrix[8] );
}

static void get_custom_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                    int yp, int up, int vp, int *m )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
            *uout++ = vlc_uint8( (*y1in * m[3] + (*uin-U) * m[4] + (*vin-V) * m[5])
                      / 65536 + U );
            *vout++ = vlc_uint8( (*y1in * m[6] + (*uin-U) * m[7] + (*vin-V) * m[8])
                      / 65536 + V );
            *y1out++ = vlc_uint8( (*y1in++ * m[0] + (*uin-U) * m[1] + (*vin-V) * m[2])
                       / 65536 );
            *y1out++ = vlc_uint8( (*y1in++ * m[0] + (*uin-U) * m[1] + (*vin-V) * m[2])
                       / 65536 );
            *y2out++ = vlc_uint8( (*y2in++ * m[0] + (*uin-U) * m[1] + (*vin-V) * m[2])
                       / 65536 );
            *y2out++ = vlc_uint8( (*y2in++ * m[0] + (*uin++ - U) * m[1] + (*vin++ -V) * m[2])
                       / 65536 );
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch  - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}
static void get_custom_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                    int yp, int up, int vp, int *m )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
            *uout++ = vlc_uint8( (*y1in * m[3] + (*uin-U) * m[4] + (*vin-V) * m[5])
                      / 65536 + U );
            *vout++ = vlc_uint8( (*y1in * m[6] + (*uin-U) * m[7] + (*vin-V) * m[8])
                      / 65536 + V );
            *y1out++ = vlc_uint8( (*y1in++ * m[0] + (*uin-U) * m[1] + (*vin-V) * m[2])
                       / 65536 );
            *y1out++ = vlc_uint8( (*y1in++ * m[0] + (*uin++ -U) * m[1] + (*vin++ -V) * m[2])
                       / 65536 );
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch  - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_custom_from_packedyuv422( picture_t *p_inpic,
                                          picture_t *p_outpic,
                                          int *m )
{
    int i_y_offset, i_u_offset, i_v_offset;
    if( GetPackedYuvOffsets( p_inpic->format.i_chroma, &i_y_offset,
                         &i_u_offset, &i_v_offset ) != VLC_SUCCESS )
        return;

    uint8_t *yin = p_inpic->p->p_pixels + i_y_offset;
    uint8_t *uin = p_inpic->p->p_pixels + i_u_offset;
    uint8_t *vin = p_inpic->p->p_pixels + i_v_offset;

    uint8_t *yout = p_outpic->p->p_pixels + i_y_offset;
    uint8_t *uout = p_outpic->p->p_pixels + i_u_offset;
    uint8_t *vout = p_outpic->p->p_pixels + i_v_offset;

    const int i_in_pitch  = p_inpic->p->i_pitch;
    const int i_out_pitch = p_outpic->p->i_pitch;
    const int i_visible_pitch = p_inpic->p->i_visible_pitch;
    const int i_visible_lines = p_inpic->p->i_visible_lines;

    const uint8_t *yend = yin + i_visible_lines * i_in_pitch;
    while( yin < yend )
    {
        const uint8_t *ylend = yin + i_visible_pitch;
        while( yin < ylend )
        {
            *uout = vlc_uint8( (*yin * m[3] + (*uin-U) * m[4] + (*vin-V) * m[5])
                      / 65536 + U );
            uout += 4;
            *vout = vlc_uint8( (*yin * m[6] + (*uin-U) * m[7] + (*vin-V) * m[8])
                     / 65536 + V );
            vout += 4;
            *yout = vlc_uint8( (*yin * m[0] + (*uin-U) * m[1] + (*vin-V) * m[2])
                       / 65536 );
            yin  += 2;
            yout += 2;
            *yout = vlc_uint8( (*yin * m[0] + (*uin-U) * m[1] + (*vin-V) * m[2])
                       / 65536 );
            yin  += 2;
            yout += 2;
            uin  += 4;
            vin  += 4;
        }
        yin  += i_in_pitch  - i_visible_pitch;
        yout += i_out_pitch - i_visible_pitch;
        uin  += i_in_pitch  - i_visible_pitch;
        uout += i_out_pitch - i_visible_pitch;
        vin  += i_in_pitch  - i_visible_pitch;
        vout += i_out_pitch - i_visible_pitch;
    }
}

static void get_red_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
/*
19595   0   27473
-11058  0   -15504
32768   0   45941
*/
            *uout++ = vlc_uint8( (*y1in * -11058 + (*vin - V) * -15504)
                      / 65536 + U );
            *vout++ = vlc_uint8( (*y1in * 32768 + (*vin - V) * 45941)
                      / 65536 + V );
            *y1out++ = vlc_uint8( (*y1in++ * 19595 + (*vin - V) * 27473)
                       / 65536 );
            *y1out++ = vlc_uint8( (*y1in++ * 19595 + (*vin - V) * 27473)
                       / 65536 );
            *y2out++ = vlc_uint8( (*y2in++ * 19594 + (*vin - V) * 27473)
                       / 65536 );
            *y2out++ = vlc_uint8( (*y2in++ * 19594 + (*vin++ - V) * 27473)
                       / 65536 );
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_green_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;

    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
/*
38470   -13239  -27473
-21710  7471    15504
-27439  9443    19595
*/
            *uout++ = vlc_uint8( (*y1in * -21710 + (*uin-U) * 7471 + (*vin-V) * 15504)
                      / 65536 + U );
            *vout++ = vlc_uint8( (*y1in * -27439 + (*uin-U) * 9443 + (*vin-V) * 19595)
                      / 65536 + V );
            *y1out++ = vlc_uint8( (*y1in++ * 38470 + (*uin-U) * -13239 + (*vin-V) * -27473)
                       / 65536 );
            *y1out++ = vlc_uint8( (*y1in++ * 38470 + (*uin-U) * -13239 + (*vin-V) * -27473)
                       / 65536 );
            *y2out++ = vlc_uint8( (*y2in++ * 38470 + (*uin-U) * -13239 + (*vin-V) * -27473)
                       / 65536 );
            *y2out++ = vlc_uint8( (*y2in++ * 38470 + (*uin++ - U) * -13239 + (*vin++ -V) * -27473)
                       / 65536 );
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch  - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_blue_from_yuv420( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *y2in;
    uint8_t *uin  = p_inpic->p[up].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *y2out;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        y2in  = y1in + i_in_pitch;
        y2out = y1out + i_out_pitch;
        while( y1in < y1end )
        {
/*
7471    13239   0
32768   58065   0
-5329   -9443   0
*/
            *uout++ = vlc_uint8( (*y1in* 32768 + (*uin - U) * 58065 )
                      / 65536 + U );
            *vout++ = vlc_uint8( (*y1in * -5329 + (*uin - U) * -9443 )
                      / 65536 + V );
            *y1out++ = vlc_uint8( (*y1in++ * 7471 + (*uin - U) * 13239 )
                       / 65536 );
            *y1out++ = vlc_uint8( (*y1in++ * 7471 + (*uin - U) * 13239 )
                       / 65536 );
            *y2out++ = vlc_uint8( (*y2in++ * 7471 + (*uin - U) * 13239 )
                       / 65536 );
            *y2out++ = vlc_uint8( (*y2in++ * 7471 + (*uin++ - U) * 13239 )
                       / 65536 );
        }
        y1in  += 2*i_in_pitch  - i_visible_pitch;
        y1out += 2*i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch  - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vout  += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
    }
}

static void get_red_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_inpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
/*
19595   0   27473
-11058  0   -15504
32768   0   45941
*/
            *uout++ = vlc_uint8( (*y1in * -11058 + (*vin - V) * -15504)
                      / 65536 + U );
            *vout++ = vlc_uint8( (*y1in * 32768 + (*vin - V) * 45941)
                      / 65536 + V );
            *y1out++ = vlc_uint8( (*y1in++ * 19595 + (*vin - V) * 27473)
                       / 65536 );
            *y1out++ = vlc_uint8( (*y1in++ * 19595 + (*vin++ - V) * 27473)
                       / 65536 );
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_green_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                   int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;
    uint8_t *vin  = p_inpic->p[vp].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
/*
38470   -13239  -27473
-21710  7471    15504
-27439  9443    19595
*/
            *uout++ = vlc_uint8( (*y1in * -21710 + (*uin-U) * 7471 + (*vin-V) * 15504)
                      / 65536 + U );
            *vout++ = vlc_uint8( (*y1in * -27439 + (*uin-U) * 9443 + (*vin-V) * 19595)
                      / 65536 + V );
            *y1out++ = vlc_uint8( (*y1in++ * 38470 + (*uin-U) * -13239 + (*vin-V) * -27473)
                       / 65536 );
            *y1out++ = vlc_uint8( (*y1in++ * 38470 + (*uin++-U) * -13239 + (*vin++-V) * -27473)
                       / 65536 );
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch  - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vin   += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static void get_blue_from_yuv422( picture_t *p_inpic, picture_t *p_outpic,
                                 int yp, int up, int vp )
{
    uint8_t *y1in = p_inpic->p[yp].p_pixels;
    uint8_t *uin  = p_inpic->p[up].p_pixels;

    uint8_t *y1out = p_outpic->p[yp].p_pixels;
    uint8_t *uout  = p_outpic->p[up].p_pixels;
    uint8_t *vout  = p_outpic->p[vp].p_pixels;

    const int i_in_pitch  = p_inpic->p[yp].i_pitch;
    const int i_out_pitch = p_outpic->p[yp].i_pitch;

    const int i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    const int i_visible_lines = p_inpic->p[yp].i_visible_lines;
    const int i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;

    const uint8_t *yend = y1in + i_visible_lines * i_in_pitch;
    while( y1in < yend )
    {
        const uint8_t *y1end = y1in + i_visible_pitch;
        while( y1in < y1end )
        {
/*
7471    13239   0
32768   58065   0
-5329   -9443   0
*/
            *uout++ = vlc_uint8( (*y1in* 32768 + (*uin - U) * 58065 )
                      / 65536 + U );
            *vout++ = vlc_uint8( (*y1in * -5329 + (*uin - U) * -9443 )
                      / 65536 + V );
            *y1out++ = vlc_uint8( (*y1in++ * 7471 + (*uin - U) * 13239 )
                       / 65536 );
            *y1out++ = vlc_uint8( (*y1in++ * 7471 + (*uin++ - U) * 13239 )
                       / 65536 );
        }
        y1in  += i_in_pitch  - i_visible_pitch;
        y1out += i_out_pitch - i_visible_pitch;
        uin   += p_inpic->p[up].i_pitch - i_uv_visible_pitch;
        uout  += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        vout  += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
    }
}

static int ExtractCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;

    vlc_mutex_lock( &p_sys->lock );
    if( !strcmp( psz_var, FILTER_PREFIX "component" ) )
    {
        p_sys->i_color = newval.i_int;
        /* Matrix won't be used for RED, GREEN or BLUE in planar formats */
        make_projection_matrix( (filter_t *)p_this, p_sys->i_color,
                                p_sys->projection_matrix );
    }
    else
    {
        msg_Warn( p_this, "Unknown callback command." );
    }
    vlc_mutex_unlock( &p_sys->lock );
    return VLC_SUCCESS;
}
