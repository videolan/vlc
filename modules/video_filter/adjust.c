/*****************************************************************************
 * adjust.c : Contrast/Hue/Saturation/Brightness video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Simon Latapie <garf@via.ecp.fr>
 *          Antoine Cellerier <dionoea -at- videolan d0t org>
 *          Martin Briza <gamajun@seznam.cz> (SSE)
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
#include "filter_picture.h"

#include "adjust_sat_hue.h"

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

#define eight_times( x )    x x x x x x x x

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *FilterPlanar( filter_t *, picture_t * );
static picture_t *FilterPacked( filter_t *, picture_t * );
static int AdjustCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define THRES_TEXT N_("Brightness threshold")
#define THRES_LONGTEXT N_("When this mode is enabled, pixels will be " \
        "shown as black or white. The threshold value will be the brightness " \
        "defined below." )
#define CONT_TEXT N_("Image contrast (0-2)")
#define CONT_LONGTEXT N_("Set the image contrast, between 0 and 2. Defaults to 1.")
#define HUE_TEXT N_("Image hue (0-360)")
#define HUE_LONGTEXT N_("Set the image hue, between 0 and 360. Defaults to 0.")
#define SAT_TEXT N_("Image saturation (0-3)")
#define SAT_LONGTEXT N_("Set the image saturation, between 0 and 3. Defaults to 1.")
#define LUM_TEXT N_("Image brightness (0-2)")
#define LUM_LONGTEXT N_("Set the image brightness, between 0 and 2. Defaults to 1.")
#define GAMMA_TEXT N_("Image gamma (0-10)")
#define GAMMA_LONGTEXT N_("Set the image gamma, between 0.01 and 10. Defaults to 1.")

vlc_module_begin ()
    set_description( N_("Image properties filter") )
    set_shortname( N_("Image adjust" ))
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )

    add_float_with_range( "contrast", 1.0, 0.0, 2.0,
                          CONT_TEXT, CONT_LONGTEXT, false )
        change_safe()
    add_float_with_range( "brightness", 1.0, 0.0, 2.0,
                           LUM_TEXT, LUM_LONGTEXT, false )
        change_safe()
    add_integer_with_range( "hue", 0, 0, 360,
                            HUE_TEXT, HUE_LONGTEXT, false )
        change_safe()
    add_float_with_range( "saturation", 1.0, 0.0, 3.0,
                          SAT_TEXT, SAT_LONGTEXT, false )
        change_safe()
    add_float_with_range( "gamma", 1.0, 0.01, 10.0,
                          GAMMA_TEXT, GAMMA_LONGTEXT, false )
        change_safe()
    add_bool( "brightness-threshold", false,
              THRES_TEXT, THRES_LONGTEXT, false )
        change_safe()

    add_shortcut( "adjust" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "contrast", "brightness", "hue", "saturation", "gamma",
    "brightness-threshold", NULL
};

/*****************************************************************************
 * filter_sys_t: adjust filter method descriptor
 *****************************************************************************/
struct filter_sys_t
{
    vlc_mutex_t lock;
    double     f_contrast;
    double     f_brightness;
    int        i_hue;
    double     f_saturation;
    double     f_gamma;
    bool       b_brightness_threshold;
    int        (* pf_process_sat_hue)( picture_t *, picture_t *, int, int, int,
                                       int, int );
    int        (* pf_process_sat_hue_clip)( picture_t *, picture_t *, int, int,
                                            int, int, int );
};

/*****************************************************************************
 * Create: allocates adjust video filter
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;
    p_sys = p_filter->p_sys;

    /* needed to get options passed in transcode using the
     * adjust{name=value} syntax */
    config_ChainParse( p_filter, "", ppsz_filter_options,
                   p_filter->p_cfg );

    p_sys->f_contrast = var_CreateGetFloatCommand( p_filter, "contrast" );
    p_sys->f_brightness = var_CreateGetFloatCommand( p_filter, "brightness" );
    p_sys->i_hue = var_CreateGetIntegerCommand( p_filter, "hue" );
    p_sys->f_saturation = var_CreateGetFloatCommand( p_filter, "saturation" );
    p_sys->f_gamma = var_CreateGetFloatCommand( p_filter, "gamma" );
    p_sys->b_brightness_threshold =
        var_CreateGetBoolCommand( p_filter, "brightness-threshold" );

    /* Choose Planar/Packed function and pointer to a Hue/Saturation processing
     * function*/
    switch( p_filter->fmt_in.video.i_chroma )
    {
        CASE_PLANAR_YUV
            /* Planar YUV */
            p_filter->pf_video_filter = FilterPlanar;
            p_sys->pf_process_sat_hue_clip = planar_sat_hue_clip_C;
            p_sys->pf_process_sat_hue = planar_sat_hue_C;
            break;

        CASE_PACKED_YUV_422
            /* Packed YUV 4:2:2 */
            p_filter->pf_video_filter = FilterPacked;
            p_sys->pf_process_sat_hue_clip = packed_sat_hue_clip_C;
            p_sys->pf_process_sat_hue = packed_sat_hue_C;
            break;

        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    vlc_mutex_init( &p_sys->lock );
    var_AddCallback( p_filter, "contrast",   AdjustCallback, p_sys );
    var_AddCallback( p_filter, "brightness", AdjustCallback, p_sys );
    var_AddCallback( p_filter, "hue",        AdjustCallback, p_sys );
    var_AddCallback( p_filter, "saturation", AdjustCallback, p_sys );
    var_AddCallback( p_filter, "gamma",      AdjustCallback, p_sys );
    var_AddCallback( p_filter, "brightness-threshold",
                                             AdjustCallback, p_sys );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy adjust video filter
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, "contrast",   AdjustCallback, p_sys );
    var_DelCallback( p_filter, "brightness", AdjustCallback, p_sys );
    var_DelCallback( p_filter, "hue",        AdjustCallback, p_sys );
    var_DelCallback( p_filter, "saturation", AdjustCallback, p_sys );
    var_DelCallback( p_filter, "gamma",      AdjustCallback, p_sys );
    var_DelCallback( p_filter, "brightness-threshold",
                                             AdjustCallback, p_sys );

    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
}

/*****************************************************************************
 * Run the filter on a Planar YUV picture
 *****************************************************************************/
static picture_t *FilterPlanar( filter_t *p_filter, picture_t *p_pic )
{
    int pi_luma[256];
    int pi_gamma[256];

    picture_t *p_outpic;
    uint8_t *p_in, *p_in_end, *p_line_end;
    uint8_t *p_out;

    bool b_thres;
    double  f_hue;
    double  f_gamma;
    int32_t i_cont, i_lum;
    int i_sat, i_sin, i_cos, i_x, i_y;
    int i;

    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    /* Get variables */
    vlc_mutex_lock( &p_sys->lock );
    i_cont = (int)( p_sys->f_contrast * 255 );
    i_lum = (int)( (p_sys->f_brightness - 1.0)*255 );
    f_hue = (float)( p_sys->i_hue * M_PI / 180 );
    i_sat = (int)( p_sys->f_saturation * 256 );
    f_gamma = 1.0 / p_sys->f_gamma;
    b_thres = p_sys->b_brightness_threshold;
    vlc_mutex_unlock( &p_sys->lock );

    /*
     * Threshold mode drops out everything about luma, contrast and gamma.
     */
    if( !b_thres )
    {

        /* Contrast is a fast but kludged function, so I put this gap to be
         * cleaner :) */
        i_lum += 128 - i_cont / 2;

        /* Fill the gamma lookup table */
        for( i = 0 ; i < 256 ; i++ )
        {
            pi_gamma[ i ] = clip_uint8_vlc( pow(i / 255.0, f_gamma) * 255.0);
        }

        /* Fill the luma lookup table */
        for( i = 0 ; i < 256 ; i++ )
        {
            pi_luma[ i ] = pi_gamma[clip_uint8_vlc( i_lum + i_cont * i / 256)];
        }
    }
    else
    {
        /*
         * We get luma as threshold value: the higher it is, the darker is
         * the image. Should I reverse this?
         */
        for( i = 0 ; i < 256 ; i++ )
        {
            pi_luma[ i ] = (i < i_lum) ? 0 : 255;
        }

        /*
         * Desaturates image to avoid that strange yellow halo...
         */
        i_sat = 0;
    }

    /*
     * Do the Y plane
     */

    p_in = p_pic->p[Y_PLANE].p_pixels;
    p_in_end = p_in + p_pic->p[Y_PLANE].i_visible_lines
                      * p_pic->p[Y_PLANE].i_pitch - 8;

    p_out = p_outpic->p[Y_PLANE].p_pixels;

    for( ; p_in < p_in_end ; )
    {
        p_line_end = p_in + p_pic->p[Y_PLANE].i_visible_pitch - 8;

        for( ; p_in < p_line_end ; )
        {
            /* Do 8 pixels at a time */
            *p_out++ = pi_luma[ *p_in++ ]; *p_out++ = pi_luma[ *p_in++ ];
            *p_out++ = pi_luma[ *p_in++ ]; *p_out++ = pi_luma[ *p_in++ ];
            *p_out++ = pi_luma[ *p_in++ ]; *p_out++ = pi_luma[ *p_in++ ];
            *p_out++ = pi_luma[ *p_in++ ]; *p_out++ = pi_luma[ *p_in++ ];
        }

        p_line_end += 8;

        for( ; p_in < p_line_end ; )
        {
            *p_out++ = pi_luma[ *p_in++ ];
        }

        p_in += p_pic->p[Y_PLANE].i_pitch
              - p_pic->p[Y_PLANE].i_visible_pitch;
        p_out += p_outpic->p[Y_PLANE].i_pitch
               - p_outpic->p[Y_PLANE].i_visible_pitch;
    }

    /*
     * Do the U and V planes
     */

    i_sin = sin(f_hue) * 256;
    i_cos = cos(f_hue) * 256;

    i_x = ( cos(f_hue) + sin(f_hue) ) * 32768;
    i_y = ( cos(f_hue) - sin(f_hue) ) * 32768;

    if ( i_sat > 256 )
    {
        /* Currently no errors are implemented in the function, if any are added
         * check them here */
        p_sys->pf_process_sat_hue_clip( p_pic, p_outpic, i_sin, i_cos, i_sat,
                                        i_x, i_y );
    }
    else
    {
        /* Currently no errors are implemented in the function, if any are added
         * check them here */
        p_sys->pf_process_sat_hue( p_pic, p_outpic, i_sin, i_cos, i_sat,
                                        i_x, i_y );
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

/*****************************************************************************
 * Run the filter on a Packed YUV picture
 *****************************************************************************/
static picture_t *FilterPacked( filter_t *p_filter, picture_t *p_pic )
{
    int pi_luma[256];
    int pi_gamma[256];

    picture_t *p_outpic;
    uint8_t *p_in, *p_in_end, *p_line_end;
    uint8_t *p_out;
    int i_y_offset, i_u_offset, i_v_offset;

    int i_pitch, i_visible_pitch;

    bool b_thres;
    double  f_hue;
    double  f_gamma;
    int32_t i_cont, i_lum;
    int i_sat, i_sin, i_cos, i_x, i_y;
    int i;

    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_pic ) return NULL;

    i_pitch = p_pic->p->i_pitch;
    i_visible_pitch = p_pic->p->i_visible_pitch;

    if( GetPackedYuvOffsets( p_pic->format.i_chroma, &i_y_offset,
                             &i_u_offset, &i_v_offset ) != VLC_SUCCESS )
    {
        msg_Warn( p_filter, "Unsupported input chroma (%4.4s)",
                  (char*)&(p_pic->format.i_chroma) );

        picture_Release( p_pic );
        return NULL;
    }

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );

        picture_Release( p_pic );
        return NULL;
    }

    /* Get variables */
    vlc_mutex_lock( &p_sys->lock );
    i_cont = (int)( p_sys->f_contrast * 255 );
    i_lum = (int)( (p_sys->f_brightness - 1.0)*255 );
    f_hue = (float)( p_sys->i_hue * M_PI / 180 );
    i_sat = (int)( p_sys->f_saturation * 256 );
    f_gamma = 1.0 / p_sys->f_gamma;
    b_thres = p_sys->b_brightness_threshold;
    vlc_mutex_unlock( &p_sys->lock );

    /*
     * Threshold mode drops out everything about luma, contrast and gamma.
     */
    if( !b_thres )
    {

        /* Contrast is a fast but kludged function, so I put this gap to be
         * cleaner :) */
        i_lum += 128 - i_cont / 2;

        /* Fill the gamma lookup table */
        for( i = 0 ; i < 256 ; i++ )
        {
          pi_gamma[ i ] = clip_uint8_vlc( pow(i / 255.0, f_gamma) * 255.0);
        }

        /* Fill the luma lookup table */
        for( i = 0 ; i < 256 ; i++ )
        {
            pi_luma[ i ] = pi_gamma[clip_uint8_vlc( i_lum + i_cont * i / 256)];
        }
    }
    else
    {
        /*
         * We get luma as threshold value: the higher it is, the darker is
         * the image. Should I reverse this?
         */
        for( i = 0 ; i < 256 ; i++ )
        {
            pi_luma[ i ] = (i < i_lum) ? 0 : 255;
        }

        /*
         * Desaturates image to avoid that strange yellow halo...
         */
        i_sat = 0;
    }

    /*
     * Do the Y plane
     */

    p_in = p_pic->p->p_pixels + i_y_offset;
    p_in_end = p_in + p_pic->p->i_visible_lines * p_pic->p->i_pitch - 8 * 4;

    p_out = p_outpic->p->p_pixels + i_y_offset;

    for( ; p_in < p_in_end ; )
    {
        p_line_end = p_in + i_visible_pitch - 8 * 4;

        for( ; p_in < p_line_end ; )
        {
            /* Do 8 pixels at a time */
            *p_out = pi_luma[ *p_in ]; p_in += 2; p_out += 2;
            *p_out = pi_luma[ *p_in ]; p_in += 2; p_out += 2;
            *p_out = pi_luma[ *p_in ]; p_in += 2; p_out += 2;
            *p_out = pi_luma[ *p_in ]; p_in += 2; p_out += 2;
            *p_out = pi_luma[ *p_in ]; p_in += 2; p_out += 2;
            *p_out = pi_luma[ *p_in ]; p_in += 2; p_out += 2;
            *p_out = pi_luma[ *p_in ]; p_in += 2; p_out += 2;
            *p_out = pi_luma[ *p_in ]; p_in += 2; p_out += 2;
        }

        p_line_end += 8 * 4;

        for( ; p_in < p_line_end ; )
        {
            *p_out = pi_luma[ *p_in ]; p_in += 2; p_out += 2;
        }

        p_in += i_pitch - p_pic->p->i_visible_pitch;
        p_out += i_pitch - p_outpic->p->i_visible_pitch;
    }

    /*
     * Do the U and V planes
     */

    i_sin = sin(f_hue) * 256;
    i_cos = cos(f_hue) * 256;

    i_x = ( cos(f_hue) + sin(f_hue) ) * 32768;
    i_y = ( cos(f_hue) - sin(f_hue) ) * 32768;

    if ( i_sat > 256 )
    {
        if ( p_sys->pf_process_sat_hue_clip( p_pic, p_outpic, i_sin, i_cos, i_sat,
                                             i_x, i_y ) != VLC_SUCCESS )
        {
            /* Currently only one error can happen in the function, but if there
             * will be more of them, this message must go away */
            msg_Warn( p_filter, "Unsupported input chroma (%4.4s)",
                      (char*)&(p_pic->format.i_chroma) );
            picture_Release( p_pic );
            return NULL;
        }
    }
    else
    {
        if ( p_sys->pf_process_sat_hue( p_pic, p_outpic, i_sin, i_cos, i_sat,
                                        i_x, i_y ) != VLC_SUCCESS )
        {
            /* Currently only one error can happen in the function, but if there
             * will be more of them, this message must go away */
            msg_Warn( p_filter, "Unsupported input chroma (%4.4s)",
                      (char*)&(p_pic->format.i_chroma) );
            picture_Release( p_pic );
            return NULL;
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

static int AdjustCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;

    vlc_mutex_lock( &p_sys->lock );
    if( !strcmp( psz_var, "contrast" ) )
        p_sys->f_contrast = newval.f_float;
    else if( !strcmp( psz_var, "brightness" ) )
        p_sys->f_brightness = newval.f_float;
    else if( !strcmp( psz_var, "hue" ) )
        p_sys->i_hue = newval.i_int;
    else if( !strcmp( psz_var, "saturation" ) )
        p_sys->f_saturation = newval.f_float;
    else if( !strcmp( psz_var, "gamma" ) )
        p_sys->f_gamma = newval.f_float;
    else if( !strcmp( psz_var, "brightness-threshold" ) )
        p_sys->b_brightness_threshold = newval.b_bool;
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}
