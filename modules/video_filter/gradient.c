/*****************************************************************************
 * gradient.c : Gradient and edge detection video effects plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <math.h>                                            /* sin(), cos() */

#include <vlc/vlc.h>
#include <vlc/sout.h>
#include <vlc/decoder.h>

#include "vlc_filter.h"

enum { GRADIENT, EDGE, HOUGH };

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

static void FilterGradient( filter_t *, picture_t *, picture_t * );
static void FilterEdge    ( filter_t *, picture_t *, picture_t * );
static void FilterHough   ( filter_t *, picture_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MODE_TEXT N_("Distort mode")
#define MODE_LONGTEXT N_("Distort mode, one of \"gradient\", \"edge\" and \"hough\".")

#define GRADIENT_TEXT N_("Gradient image type")
#define GRADIENT_LONGTEXT N_("Gradient image type (0 or 1). 0 will " \
        "turn the image to white while 1 will keep colors." )

#define CARTOON_TEXT N_("Apply cartoon effect")
#define CARTOON_LONGTEXT N_("Apply cartoon effect. It is only used by " \
    "\"gradient\" and \"edge\".")

static char *mode_list[] = { "gradient", "edge", "hough" };
static char *mode_list_text[] = { N_("Gradient"), N_("Edge"), N_("Hough") };

#define FILTER_PREFIX "gradient-"

vlc_module_begin();
    set_description( _("Gradient video filter") );
    set_shortname( N_( "Gradient" ));
    set_capability( "video filter2", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER2 );

    add_string( FILTER_PREFIX "mode", "gradient", NULL,
                MODE_TEXT, MODE_LONGTEXT, VLC_FALSE );
        change_string_list( mode_list, mode_list_text, 0 );

    add_integer_with_range( FILTER_PREFIX "type", 0, 0, 1, NULL,
                GRADIENT_TEXT, GRADIENT_LONGTEXT, VLC_FALSE );
    add_bool( FILTER_PREFIX "cartoon", 1, NULL,
                CARTOON_TEXT, CARTOON_LONGTEXT, VLC_FALSE );

    add_shortcut( "gradient" );
    set_callbacks( Create, Destroy );
vlc_module_end();

static const char *ppsz_filter_options[] = {
    "mode", "type", "cartoon", NULL
};

/*****************************************************************************
 * vout_sys_t: Distort video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Distort specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    int i_mode;

    /* For the gradient mode */
    int i_gradient_type;
    vlc_bool_t b_cartoon;

    /* For hough mode */
    int *p_pre_hough;
};

/*****************************************************************************
 * Create: allocates Distort video thread output method
 *****************************************************************************
 * This function allocates and initializes a Distort vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    char *psz_method;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    p_filter->pf_video_filter = Filter;

    p_filter->p_sys->p_pre_hough = NULL;

    sout_CfgParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                   p_filter->p_cfg );

    var_Create( p_filter, FILTER_PREFIX "mode",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_filter, FILTER_PREFIX "type",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_filter, FILTER_PREFIX "cartoon",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    if( !(psz_method = var_GetString( p_filter, FILTER_PREFIX "mode" )) )
    {
        msg_Err( p_filter, "configuration variable "
                 FILTER_PREFIX "mode empty" );
        p_filter->p_sys->i_mode = GRADIENT;
    }
    else
    {
        if( !strcmp( psz_method, "gradient" ) )
        {
            p_filter->p_sys->i_mode = GRADIENT;
        }
        else if( !strcmp( psz_method, "edge" ) )
        {
            p_filter->p_sys->i_mode = EDGE;
        }
        else if( !strcmp( psz_method, "hough" ) )
        {
            p_filter->p_sys->i_mode = HOUGH;
        }
        else
        {
            msg_Err( p_filter, "no valid gradient mode provided" );
            p_filter->p_sys->i_mode = GRADIENT;
        }
    }
    free( psz_method );

    p_filter->p_sys->i_gradient_type =
        var_GetInteger( p_filter, FILTER_PREFIX "type" );
    p_filter->p_sys->b_cartoon =
        var_GetInteger( p_filter, FILTER_PREFIX "cartoon" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Distort video thread output method
 *****************************************************************************
 * Terminate an output method created by DistortCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    if(p_filter->p_sys->p_pre_hough)
        free(p_filter->p_sys->p_pre_hough);

    free( p_filter->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Distort image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;

    if( !p_pic ) return NULL;

    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }

    switch( p_filter->p_sys->i_mode )
    {
        case EDGE:
            FilterEdge( p_filter, p_pic, p_outpic );
            break;

        case GRADIENT:
            FilterGradient( p_filter, p_pic, p_outpic );
            break;

        case HOUGH:
            FilterHough( p_filter, p_pic, p_outpic );
            break;

        default:
            break;
    }

    p_outpic->date = p_pic->date;
    p_outpic->b_force = p_pic->b_force;
    p_outpic->i_nb_fields = p_pic->i_nb_fields;
    p_outpic->b_progressive = p_pic->b_progressive;
    p_outpic->b_top_field_first = p_pic->b_top_field_first;

    if( p_pic->pf_release )
        p_pic->pf_release( p_pic );

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
static void GaussianConvolution( picture_t *p_inpic, uint32_t *p_smooth )
{
    uint8_t *p_inpix = p_inpic->p[Y_PLANE].p_pixels;
    int i_src_pitch = p_inpic->p[Y_PLANE].i_pitch;
    int i_src_visible = p_inpic->p[Y_PLANE].i_visible_pitch;
    int i_num_lines = p_inpic->p[Y_PLANE].i_visible_lines;

    int x,y;
    for( y = 2; y < i_num_lines - 2; y++ )
    {
        for( x = 2; x < i_src_visible - 2; x++ )
        {
            p_smooth[y*i_src_visible+x] = (uint32_t)(
              /* 2 rows up */
                ( p_inpix[(y-2)*i_src_pitch+x-2]<<1 )
              + ( p_inpix[(y-2)*i_src_pitch+x-1]<<2 )
              + ( p_inpix[(y-2)*i_src_pitch+x]<<2 )
              + ( p_inpix[(y-2)*i_src_pitch+x+1]<<2 )
              + ( p_inpix[(y-2)*i_src_pitch+x+2]<<1 )
              /* 1 row up */
              + ( p_inpix[(y-1)*i_src_pitch+x-2]<<2 )
              + ( p_inpix[(y-1)*i_src_pitch+x-1]<<3 )
              + ( p_inpix[(y-1)*i_src_pitch+x]*12 )
              + ( p_inpix[(y-1)*i_src_pitch+x+1]<<3 )
              + ( p_inpix[(y-1)*i_src_pitch+x+2]<<2 )
              /* */
              + ( p_inpix[y*i_src_pitch+x-2]<<2 )
              + ( p_inpix[y*i_src_pitch+x-1]*12 )
              + ( p_inpix[y*i_src_pitch+x]<<4 )
              + ( p_inpix[y*i_src_pitch+x+1]*12 )
              + ( p_inpix[y*i_src_pitch+x+2]<<2 )
              /* 1 row down */
              + ( p_inpix[(y+1)*i_src_pitch+x-2]<<2 )
              + ( p_inpix[(y+1)*i_src_pitch+x-1]<<3 )
              + ( p_inpix[(y+1)*i_src_pitch+x]*12 )
              + ( p_inpix[(y+1)*i_src_pitch+x+1]<<3 )
              + ( p_inpix[(y+1)*i_src_pitch+x+2]<<2 )
              /* 2 rows down */
              + ( p_inpix[(y+2)*i_src_pitch+x-2]<<1 )
              + ( p_inpix[(y+2)*i_src_pitch+x-1]<<2 )
              + ( p_inpix[(y+2)*i_src_pitch+x]<<2 )
              + ( p_inpix[(y+2)*i_src_pitch+x+1]<<2 )
              + ( p_inpix[(y+2)*i_src_pitch+x+2]<<1 )
              ) >> 7 /* 115 */;
        }
    }
}

/*****************************************************************************
 * FilterGradient: Sobel
 *****************************************************************************/
static void FilterGradient( filter_t *p_filter, picture_t *p_inpic,
                                                picture_t *p_outpic )
{
    int x, y;
    int i_src_pitch = p_inpic->p[Y_PLANE].i_pitch;
    int i_src_visible = p_inpic->p[Y_PLANE].i_visible_pitch;
    int i_dst_pitch = p_outpic->p[Y_PLANE].i_pitch;
    int i_num_lines = p_inpic->p[Y_PLANE].i_visible_lines;

    uint8_t *p_inpix = p_inpic->p[Y_PLANE].p_pixels;
    uint8_t *p_outpix = p_outpic->p[Y_PLANE].p_pixels;

    uint32_t *p_smooth = (uint32_t *)malloc( i_num_lines * i_src_visible * sizeof(uint32_t));

    if( !p_smooth ) return;

    if( p_filter->p_sys->b_cartoon )
    {
        p_filter->p_vlc->pf_memcpy( p_outpic->p[U_PLANE].p_pixels,
            p_inpic->p[U_PLANE].p_pixels,
            p_outpic->p[U_PLANE].i_lines * p_outpic->p[U_PLANE].i_pitch );
        p_filter->p_vlc->pf_memcpy( p_outpic->p[V_PLANE].p_pixels,
            p_inpic->p[V_PLANE].p_pixels,
            p_outpic->p[V_PLANE].i_lines * p_outpic->p[V_PLANE].i_pitch );
    }
    else
    {
        p_filter->p_vlc->pf_memset( p_outpic->p[U_PLANE].p_pixels, 0x80,
            p_outpic->p[U_PLANE].i_lines * p_outpic->p[U_PLANE].i_pitch );
        p_filter->p_vlc->pf_memset( p_outpic->p[V_PLANE].p_pixels, 0x80,
            p_outpic->p[V_PLANE].i_lines * p_outpic->p[V_PLANE].i_pitch );
    }

    GaussianConvolution( p_inpic, p_smooth );

    /* Sobel gradient

     | -1 0 1 |     |  1  2  1 |
     | -2 0 2 | and |  0  0  0 |
     | -1 0 1 |     | -1 -2 -1 | */

    for( y = 1; y < i_num_lines - 1; y++ )
    {
        for( x = 1; x < i_src_visible - 1; x++ )
        {
            uint32_t a =
            (
              abs(
                ( ( p_smooth[(y-1)*i_src_visible+x]
                    - p_smooth[(y+1)*i_src_visible+x] ) <<1 )
               + ( p_smooth[(y-1)*i_src_visible+x-1]
                   - p_smooth[(y+1)*i_src_visible+x-1] )
               + ( p_smooth[(y-1)*i_src_visible+x+1]
                   - p_smooth[(y+1)*i_src_visible+x+1] )
              )
            +
              abs(
                ( ( p_smooth[y*i_src_visible+x-1]
                    - p_smooth[y*i_src_visible+x+1] ) <<1 )
               + ( p_smooth[(y-1)*i_src_visible+x-1]
                   - p_smooth[(y-1)*i_src_visible+x+1] )
               + ( p_smooth[(y+1)*i_src_visible+x-1]
                   - p_smooth[(y+1)*i_src_visible+x+1] )
              )
            );
            if( p_filter->p_sys->i_gradient_type )
            {
                if( p_filter->p_sys->b_cartoon )
                {
                    if( a > 60 )
                    {
                        p_outpix[y*i_dst_pitch+x] = 0x00;
                    }
                    else
                    {
                        if( p_smooth[y*i_src_visible+x] > 0xa0 )
                            p_outpix[y*i_dst_pitch+x] =
                                0xff - ((0xff - p_inpix[y*i_src_pitch+x] )>>2);
                        else if( p_smooth[y*i_src_visible+x] > 0x70 )
                            p_outpix[y*i_dst_pitch+x] =
                                0xa0 - ((0xa0 - p_inpix[y*i_src_pitch+x] )>>2);
                        else if( p_smooth[y*i_src_visible+x] > 0x28 )
                            p_outpix[y*i_dst_pitch+x] =
                                0x70 - ((0x70 - p_inpix[y*i_src_pitch+x] )>>2);
                        else
                            p_outpix[y*i_dst_pitch+x] =
                                0x28 - ((0x28 - p_inpix[y*i_src_pitch+x] )>>2);
                    }
                }
                else
                {
                    if( a>>8 )
                        p_outpix[y*i_dst_pitch+x] = 255;
                    else
                        p_outpix[y*i_dst_pitch+x] = (uint8_t)a;
                }
            }
            else
            {
                if( a>>8 )
                    p_outpix[y*i_dst_pitch+x] = 0;
                else
                    p_outpix[y*i_dst_pitch+x] = (uint8_t)(255 - a);
            }
        }
    }

    if( p_smooth ) free( p_smooth );
}

/*****************************************************************************
 * FilterEdge: Canny edge detection algorithm
 *****************************************************************************
 * http://fourier.eng.hmc.edu/e161/lectures/canny/node1.html
 * (well ... my implementation isn't really the canny algorithm ... but some
 * ideas are the same)
 *****************************************************************************/
/* angle : | */
#define THETA_Y 0
/* angle : - */
#define THETA_X 1
/* angle : / */
#define THETA_P 2
/* angle : \ */
#define THETA_M 3
static void FilterEdge( filter_t *p_filter, picture_t *p_inpic,
                                            picture_t *p_outpic )
{
    int x, y;

    int i_src_pitch = p_inpic->p[Y_PLANE].i_pitch;
    int i_src_visible = p_inpic->p[Y_PLANE].i_visible_pitch;
    int i_dst_pitch = p_outpic->p[Y_PLANE].i_pitch;
    int i_num_lines = p_inpic->p[Y_PLANE].i_visible_lines;

    uint8_t *p_inpix = p_inpic->p[Y_PLANE].p_pixels;
    uint8_t *p_outpix = p_outpic->p[Y_PLANE].p_pixels;

    uint32_t *p_smooth = malloc( i_num_lines*i_src_visible * sizeof(uint32_t) );
    uint32_t *p_grad = malloc( i_num_lines*i_src_visible *sizeof(uint32_t) );
    uint8_t *p_theta = malloc( i_num_lines*i_src_visible *sizeof(uint8_t) );

    if( !p_smooth || !p_grad || !p_theta ) return;

    if( p_filter->p_sys->b_cartoon )
    {
        p_filter->p_vlc->pf_memcpy( p_outpic->p[U_PLANE].p_pixels,
            p_inpic->p[U_PLANE].p_pixels,
            p_outpic->p[U_PLANE].i_lines * p_outpic->p[U_PLANE].i_pitch );
        p_filter->p_vlc->pf_memcpy( p_outpic->p[V_PLANE].p_pixels,
            p_inpic->p[V_PLANE].p_pixels,
            p_outpic->p[V_PLANE].i_lines * p_outpic->p[V_PLANE].i_pitch );
    }
    else
    {
        p_filter->p_vlc->pf_memset( p_outpic->p[Y_PLANE].p_pixels, 0xff,
              p_outpic->p[Y_PLANE].i_lines * p_outpic->p[Y_PLANE].i_pitch );
        p_filter->p_vlc->pf_memset( p_outpic->p[U_PLANE].p_pixels, 0x80,
            p_outpic->p[U_PLANE].i_lines * p_outpic->p[U_PLANE].i_pitch );
        memset( p_outpic->p[V_PLANE].p_pixels, 0x80,
            p_outpic->p[V_PLANE].i_lines * p_outpic->p[V_PLANE].i_pitch );
    }

    GaussianConvolution( p_inpic, p_smooth );

    /* Sobel gradient

     | -1 0 1 |     |  1  2  1 |
     | -2 0 2 | and |  0  0  0 |
     | -1 0 1 |     | -1 -2 -1 | */

    for( y = 1; y < i_num_lines - 1; y++ )
    {
        for( x = 1; x < i_src_visible - 1; x++ )
        {

            int gradx =
                ( ( p_smooth[(y-1)*i_src_visible+x]
                    - p_smooth[(y+1)*i_src_visible+x] ) <<1 )
               + ( p_smooth[(y-1)*i_src_visible+x-1]
                   - p_smooth[(y+1)*i_src_visible+x-1] )
               + ( p_smooth[(y-1)*i_src_visible+x+1]
                   - p_smooth[(y+1)*i_src_visible+x+1] );
            int grady =
                ( ( p_smooth[y*i_src_visible+x-1]
                    - p_smooth[y*i_src_visible+x+1] ) <<1 )
               + ( p_smooth[(y-1)*i_src_visible+x-1]
                   - p_smooth[(y-1)*i_src_visible+x+1] )
               + ( p_smooth[(y+1)*i_src_visible+x-1]
                   - p_smooth[(y+1)*i_src_visible+x+1] );

            p_grad[y*i_src_visible+x] = (uint32_t)(abs( gradx ) + abs( grady ));

            /* tan( 22.5 ) = 0,414213562 .. * 128 = 53
             * tan( 26,565051177 ) = 0.5
             * tan( 45 + 22.5 ) = 2,414213562 .. * 128 = 309
             * tan( 63,434948823 ) 2 */
            if( (grady<<1) > gradx )
                p_theta[y*i_src_visible+x] = THETA_P;
            else if( (grady<<1) < -gradx )
                p_theta[y*i_src_visible+x] = THETA_M;
            else if( !gradx || abs(grady) > abs(gradx)<<1 )
                p_theta[y*i_src_visible+x] = THETA_Y;
            else
                p_theta[y*i_src_visible+x] = THETA_X;
        }
    }

    /* edge computing */
    for( y = 1; y < i_num_lines - 1; y++ )
    {
        for( x = 1; x < i_src_visible - 1; x++ )
        {
            if( p_grad[y*i_src_visible+x] > 40 )
            {
                switch( p_theta[y*i_src_visible+x] )
                {
                    case THETA_Y:
                        if(    p_grad[y*i_src_visible+x] > p_grad[(y-1)*i_src_visible+x]
                            && p_grad[y*i_src_visible+x] > p_grad[(y+1)*i_src_visible+x] )
                        {
                            p_outpix[y*i_dst_pitch+x] = 0;
                        } else goto colorize;
                        break;
                    case THETA_P:
                        if(    p_grad[y*i_src_visible+x] > p_grad[(y-1)*i_src_visible+x-1]
                            && p_grad[y*i_src_visible+x] > p_grad[(y+1)*i_src_visible+x+1] )
                        {
                            p_outpix[y*i_dst_pitch+x] = 0;
                        } else goto colorize;
                        break;
                    case THETA_M:
                        if(    p_grad[y*i_src_visible+x] > p_grad[(y-1)*i_src_visible+x+1]
                            && p_grad[y*i_src_visible+x] > p_grad[(y+1)*i_src_visible+x-1] )
                        {
                            p_outpix[y*i_dst_pitch+x] = 0;
                        } else goto colorize;
                        break;
                    case THETA_X:
                        if(    p_grad[y*i_src_visible+x] > p_grad[y*i_src_visible+x-1]
                            && p_grad[y*i_src_visible+x] > p_grad[y*i_src_visible+x+1] )
                        {
                            p_outpix[y*i_dst_pitch+x] = 0;
                        } else goto colorize;
                        break;
                }
            }
            else
            {
                colorize:
                if( p_filter->p_sys->b_cartoon )
                {
                    if( p_smooth[y*i_src_visible+x] > 0xa0 )
                        p_outpix[y*i_dst_pitch+x] = (uint8_t)
                            0xff - ((0xff - p_inpix[y*i_src_pitch+x] )>>2);
                    else if( p_smooth[y*i_src_visible+x] > 0x70 )
                        p_outpix[y*i_dst_pitch+x] =(uint8_t)
                            0xa0 - ((0xa0 - p_inpix[y*i_src_pitch+x] )>>2);
                    else if( p_smooth[y*i_src_visible+x] > 0x28 )
                        p_outpix[y*i_dst_pitch+x] =(uint8_t)
                            0x70 - ((0x70 - p_inpix[y*i_src_pitch+x] )>>2);
                    else
                        p_outpix[y*i_dst_pitch+x] =(uint8_t)
                            0x28 - ((0x28 - p_inpix[y*i_src_pitch+x] )>>2);
                }
            }
        }
    }
    if( p_smooth ) free( p_smooth );
    if( p_grad ) free( p_grad );
    if( p_theta) free( p_theta );
}

/*****************************************************************************
 * FilterHough
 *****************************************************************************/
#define p_pre_hough p_filter->p_sys->p_pre_hough
static void FilterHough( filter_t *p_filter, picture_t *p_inpic,
                                             picture_t *p_outpic )
{
    int x, y, i;
    int i_src_visible = p_inpic->p[Y_PLANE].i_visible_pitch;
    int i_dst_pitch = p_outpic->p[Y_PLANE].i_pitch;
    int i_num_lines = p_inpic->p[Y_PLANE].i_visible_lines;

    uint8_t *p_outpix = p_outpic->p[Y_PLANE].p_pixels;

    int i_diag = sqrt( i_num_lines * i_num_lines +
                        i_src_visible * i_src_visible);
    int i_max, i_phi_max, i_rho, i_rho_max;
    int i_nb_steps = 90;
    double d_step = M_PI / i_nb_steps;
    double d_sin;
    double d_cos;
    uint32_t *p_smooth;
    int *p_hough = malloc( i_diag * i_nb_steps * sizeof(int) );
    if( ! p_hough ) return;
    p_smooth = (uint32_t *)malloc( i_num_lines*i_src_visible*sizeof(uint32_t));
    if( !p_smooth ) return;

    if( ! p_pre_hough )
    {
        msg_Dbg(p_filter, "Starting precalculation");
        p_pre_hough = malloc( i_num_lines*i_src_visible*i_nb_steps*sizeof(int));
        if( ! p_pre_hough ) return;
        for( i = 0 ; i < i_nb_steps ; i++)
        {
            d_sin = sin(d_step * i);
            d_cos = cos(d_step * i);
            for( y = 0 ; y < i_num_lines ; y++ )
                for( x = 0 ; x < i_src_visible ; x++ )
                {
                    p_pre_hough[(i*i_num_lines+y)*i_src_visible + x] =
                        ceil(x*d_sin + y*d_cos);
                }
        }
        msg_Dbg(p_filter, "Precalculation done");
    }

    memset( p_hough, 0, i_diag * i_nb_steps * sizeof(int) );

    p_filter->p_vlc->pf_memcpy(
        p_outpic->p[Y_PLANE].p_pixels, p_inpic->p[Y_PLANE].p_pixels,
        p_outpic->p[Y_PLANE].i_lines * p_outpic->p[Y_PLANE].i_pitch );
    p_filter->p_vlc->pf_memcpy(
        p_outpic->p[U_PLANE].p_pixels, p_inpic->p[U_PLANE].p_pixels,
        p_outpic->p[U_PLANE].i_lines * p_outpic->p[U_PLANE].i_pitch );
    p_filter->p_vlc->pf_memcpy(
        p_outpic->p[V_PLANE].p_pixels, p_inpic->p[V_PLANE].p_pixels,
        p_outpic->p[V_PLANE].i_lines * p_outpic->p[V_PLANE].i_pitch );

    GaussianConvolution( p_inpic, p_smooth );

    /* Sobel gradient

     | -1 0 1 |     |  1  2  1 |
     | -2 0 2 | and |  0  0  0 |
     | -1 0 1 |     | -1 -2 -1 | */

    i_max = 0;
    i_rho_max = 0;
    i_phi_max = 0;
    for( y = 4; y < i_num_lines - 4; y++ )
    {
        for( x = 4; x < i_src_visible - 4; x++ )
        {
            uint32_t a =
            (
              abs(
                ( ( p_smooth[(y-1)*i_src_visible+x]
                    - p_smooth[(y+1)*i_src_visible+x] ) <<1 )
               + ( p_smooth[(y-1)*i_src_visible+x-1]
                   - p_smooth[(y+1)*i_src_visible+x-1] )
               + ( p_smooth[(y-1)*i_src_visible+x+1]
                   - p_smooth[(y+1)*i_src_visible+x+1] )
              )
            +
              abs(
                ( ( p_smooth[y*i_src_visible+x-1]
                    - p_smooth[y*i_src_visible+x+1] ) <<1 )
               + ( p_smooth[(y-1)*i_src_visible+x-1]
                   - p_smooth[(y-1)*i_src_visible+x+1] )
               + ( p_smooth[(y+1)*i_src_visible+x-1]
                   - p_smooth[(y+1)*i_src_visible+x+1] )
              )
            );
            if( a>>8 )
            {
                for( i = 0 ; i < i_nb_steps ; i ++ )
                {
                    i_rho = p_pre_hough[(i*i_num_lines+y)*i_src_visible + x];
                    if( p_hough[i_rho + i_diag/2 + i * i_diag]++ > i_max )
                    {
                        i_max = p_hough[i_rho + i_diag/2 + i * i_diag];
                        i_rho_max = i_rho;
                        i_phi_max = i;
                    }
                }
            }
        }
    }

    d_sin = sin(i_phi_max*d_step);
    d_cos = cos(i_phi_max*d_step);
    if( d_cos != 0 )
    {
        for( x = 0 ; x < i_src_visible ; x++ )
        {
            y = (i_rho_max - x * d_sin) / d_cos;
            if( y >= 0 && y < i_num_lines )
                p_outpix[y*i_dst_pitch+x] = 255;
        }
    }

    if( p_hough ) free( p_hough );
    if( p_smooth ) free( p_smooth );
}
#undef p_pre_hough
