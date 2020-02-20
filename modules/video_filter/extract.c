/*****************************************************************************
 * extract.c : Extract RGB components
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
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

#include <math.h>

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
static int ExtractCallback( vlc_object_t *, char const *,
                            vlc_value_t, vlc_value_t, void * );

static void make_projection_matrix( filter_t *, int color, int *matrix );
static void get_custom_from_yuv( picture_t *, picture_t *, int const, int const, int const, int const * );
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
    set_capability( "video filter", 0 )
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
typedef struct
{
    vlc_mutex_t lock;
    int *projection_matrix;
    uint32_t i_color;
} filter_sys_t;

/*****************************************************************************
 * Create
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_I420_10L:
        case VLC_CODEC_I420_10B:
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
    filter_sys_t *p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_filter->p_sys = p_sys;

    p_sys->projection_matrix = malloc( 9 * sizeof( int ) );
    if( !p_sys->projection_matrix )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    p_sys->i_color = var_CreateGetIntegerCommand( p_filter,
                                               FILTER_PREFIX "component" );
    /* Matrix won't be used for RED, GREEN or BLUE in planar formats */
    make_projection_matrix( p_filter, p_sys->i_color,
                            p_sys->projection_matrix );
    vlc_mutex_init( &p_sys->lock );
    var_AddCallback( p_filter, FILTER_PREFIX "component",
                     ExtractCallback, p_sys );

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
    case VLC_CODEC_I420_10L:
    case VLC_CODEC_I420_10B:
    case VLC_CODEC_J420:
    case VLC_CODEC_YV12:
    case VLC_CODEC_I422:
    case VLC_CODEC_J422:
        get_custom_from_yuv( p_pic, p_outpic, Y_PLANE, U_PLANE, V_PLANE, p_sys->projection_matrix );
        break;

        CASE_PACKED_YUV_422
            get_custom_from_packedyuv422( p_pic, p_outpic, p_sys->projection_matrix );
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

#define U8 128
#define V8 128

#define U10 512
#define V10 512

static void mmult( double *res, double *a, double *b )
{
    for( int i = 0; i < 3; i++ )
    {
        for( int j = 0; j < 3; j++ )
        {
            res[ i*3 + j ] = 0.;
            for( int k = 0; k < 3; k++ )
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
    msg_Dbg( p_filter, "red: %f", red );
    msg_Dbg( p_filter, "green: %f", green );
    msg_Dbg( p_filter, "blue: %f", blue );
    mmult( result1, rgb_matrix, right_matrix );
    mmult( result, left_matrix, result1 );
    for( int i = 0; i < 9; i++ )
    {
        matrix[i] = (int)result[i];
    }
    msg_Dbg( p_filter, "Projection matrix:" );
    msg_Dbg( p_filter, "%6d %6d %6d", matrix[0], matrix[1], matrix[2] );
    msg_Dbg( p_filter, "%6d %6d %6d", matrix[3], matrix[4], matrix[5] );
    msg_Dbg( p_filter, "%6d %6d %6d", matrix[6], matrix[7], matrix[8] );
}

#define IS_YUV_10BITS(fmt) (fmt == VLC_CODEC_I420_10L || fmt == VLC_CODEC_I420_10B)

#define GET_CUSTOM_PIX() \
    do \
    { \
        val = (*y_in[0] * m[3] + (**u_in - u) * m[4] + (**v_in - v) * m[5]) / 65536 + u; \
        *(*u_out)++ = VLC_CLIP( val, 0, maxval ); \
        val = (*y_in[0] * m[6] + (**u_in - u) * m[7] + (**v_in - v) * m[8]) / 65536 + v; \
        *(*v_out)++ = VLC_CLIP( val, 0, maxval ); \
        val = (*y_in[0]++ * m[0] + (**u_in - u) * m[1] + (**v_in - v) * m[2]) / 65536; \
        *y_out[0]++ = VLC_CLIP( val, 0, maxval ); \
        val = (*y_in[0]++ * m[0] + (**u_in - u) * m[1] + (**v_in - v) * m[2]) / 65536; \
        *y_out[0]++ = VLC_CLIP( val, 0, maxval ); \
        val = (*y_in[1]++ * m[0] + (**u_in - u) * m[1] + (**v_in - v) * m[2]) / 65536; \
        *y_out[1]++ = VLC_CLIP( val, 0, maxval ); \
        val = (*y_in[1]++ * m[0] + (*(*u_in)++ - u) * m[1] + (*(*v_in)++ - v) * m[2]) / 65536; \
        *y_out[1]++ = VLC_CLIP( val, 0, maxval ); \
    } while (0);

static inline void
get_custom_pix_8b( uint8_t *y_in[2], uint8_t *y_out[2],
                   uint8_t **u_in, uint8_t **u_out,
                   uint8_t **v_in, uint8_t **v_out,
                   uint16_t const u, uint16_t const v,
                   int const *m, int maxval )
{
    uint8_t val;
    GET_CUSTOM_PIX();
}

static inline void
get_custom_pix_10b( uint16_t *y_in[2], uint16_t *y_out[2],
                    uint16_t **u_in, uint16_t **u_out,
                    uint16_t **v_in, uint16_t **v_out,
                    uint16_t const u, uint16_t const v,
                    int const *m, int maxval )
{
    uint16_t val;
    GET_CUSTOM_PIX();
}

static void
get_custom_from_yuv( picture_t *p_inpic, picture_t *p_outpic,
                     int const yp, int const up, int const vp, int const *m )
{
    int const   i_in_pitch  = p_inpic->p[yp].i_pitch;
    int const   i_out_pitch = p_outpic->p[yp].i_pitch;
    int const   i_visible_pitch = p_inpic->p[yp].i_visible_pitch;
    int const   i_visible_lines = p_inpic->p[yp].i_visible_lines;
    int const   i_uv_visible_pitch = p_inpic->p[up].i_visible_pitch;
    uint8_t     *y_in[2] = { p_inpic->p[yp].p_pixels };
    uint8_t     *u_in = p_inpic->p[up].p_pixels;
    uint8_t     *v_in = p_inpic->p[vp].p_pixels;
    uint8_t     *y_out[2] = { p_outpic->p[yp].p_pixels };
    uint8_t     *u_out = p_outpic->p[up].p_pixels;
    uint8_t     *v_out = p_outpic->p[vp].p_pixels;
    uint8_t *const y_end = y_in[0] + i_visible_lines * i_in_pitch;

    while (y_in[0] < y_end)
    {
        y_in[1] = y_in[0] + i_in_pitch;
        y_out[1] = y_out[0] + i_out_pitch;
        for (uint8_t *const y_row_end = y_in[0] + i_visible_pitch; y_in[0] < y_row_end; )
        {
            !IS_YUV_10BITS(p_inpic->format.i_chroma)
                ? get_custom_pix_8b(y_in, y_out, &u_in, &u_out, &v_in, &v_out, U8,
                                 V8, m, 255)
                : get_custom_pix_10b((uint16_t **)y_in, (uint16_t **)y_out,
                                  (uint16_t **)&u_in, (uint16_t **)&u_out,
                                  (uint16_t **)&v_in, (uint16_t **)&v_out, U10,
                                  V10, m, 1023);
        }
        y_in[0] += 2 * i_in_pitch - i_visible_pitch;
        y_out[0] += 2 * i_out_pitch - i_visible_pitch;
        u_in += p_inpic->p[up].i_pitch  - i_uv_visible_pitch;
        u_out += p_outpic->p[up].i_pitch - i_uv_visible_pitch;
        v_in += p_inpic->p[vp].i_pitch  - i_uv_visible_pitch;
        v_out += p_outpic->p[vp].i_pitch - i_uv_visible_pitch;
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
            *uout = vlc_uint8( (*yin * m[3] + (*uin-U8) * m[4] + (*vin-V8) * m[5]) / 65536 + U8 );
            uout += 4;
            *vout = vlc_uint8( (*yin * m[6] + (*uin-U8) * m[7] + (*vin-V8) * m[8]) / 65536 + V8 );
            vout += 4;
            *yout = vlc_uint8( (*yin * m[0] + (*uin-U8) * m[1] + (*vin-V8) * m[2]) / 65536 );
            yin  += 2;
            yout += 2;
            *yout = vlc_uint8( (*yin * m[0] + (*uin-U8) * m[1] + (*vin-V8) * m[2]) / 65536 );
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
