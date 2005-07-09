/*****************************************************************************
 * adjust.c : Contrast/Hue/Saturation/Brightness video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Simon Latapie <garf@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <math.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "filter_common.h"

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

#define eight_times( x )    x x x x x x x x

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static int  SendEvents( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define CONT_TEXT N_("Image contrast (0-2)")
#define CONT_LONGTEXT N_("Set the image contrast, between 0 and 2. Defaults to 1")
#define HUE_TEXT N_("Image hue (0-360)")
#define HUE_LONGTEXT N_("Set the image hue, between 0 and 360. Defaults to 0")
#define SAT_TEXT N_("Image saturation (0-3)")
#define SAT_LONGTEXT N_("Set the image saturation, between 0 and 3. Defaults to 1")
#define LUM_TEXT N_("Image brightness (0-2)")
#define LUM_LONGTEXT N_("Set the image brightness, between 0 and 2. Defaults to 1")
#define GAMMA_TEXT N_("Image gamma (0-10)")
#define GAMMA_LONGTEXT N_("Set the image gamma, between 0.01 and 10. Defaults to 1")


vlc_module_begin();
    set_description( _("Image properties filter") );
    set_shortname( N_("Image adjust" ));
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );
    set_capability( "video filter", 0 );

    add_float_with_range( "contrast", 1.0, 0.0, 2.0, NULL, CONT_TEXT, CONT_LONGTEXT, VLC_FALSE );
    add_float_with_range( "brightness", 1.0, 0.0, 2.0, NULL, LUM_TEXT, LUM_LONGTEXT, VLC_FALSE );
    add_integer_with_range( "hue", 0, 0, 360, NULL, HUE_TEXT, HUE_LONGTEXT, VLC_FALSE );
    add_float_with_range( "saturation", 1.0, 0.0, 3.0, NULL, SAT_TEXT, SAT_LONGTEXT, VLC_FALSE );
    add_float_with_range( "gamma", 1.0, 0.01, 10.0, NULL, GAMMA_TEXT, GAMMA_LONGTEXT, VLC_FALSE );

    add_shortcut( "adjust" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: adjust video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the adjust specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    vout_thread_t *p_vout;
};

inline static int32_t clip( int32_t a )
{
    return (a > 255) ? 255 : (a < 0) ? 0 : a;
}

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * Create: allocates adjust video thread output method
 *****************************************************************************
 * This function allocates and initializes a adjust vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;
    
    var_Create( p_vout, "contrast", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "brightness", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "hue", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "saturation", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "gamma", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize adjust video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;
    video_format_t fmt = {0};

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    fmt.i_width = fmt.i_visible_width = p_vout->render.i_width;
    fmt.i_height = fmt.i_visible_height = p_vout->render.i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_chroma = p_vout->render.i_chroma;
    fmt.i_aspect = p_vout->render.i_aspect;
    fmt.i_sar_num = p_vout->render.i_aspect * fmt.i_height / fmt.i_width;
    fmt.i_sar_den = VOUT_ASPECT_FACTOR;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video output" );

    p_vout->p_sys->p_vout = vout_Create( p_vout, &fmt );

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "can't open vout, aborting" );

        return VLC_EGENERIC;
    }

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    ADD_PARENT_CALLBACKS( SendEventsToChild );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate adjust video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }
}

/*****************************************************************************
 * Destroy: destroy adjust video thread output method
 *****************************************************************************
 * Terminate an output method created by adjustCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    if( p_vout->p_sys->p_vout )
    {
        DEL_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );
        vlc_object_detach( p_vout->p_sys->p_vout );
        vout_Destroy( p_vout->p_sys->p_vout );
    }

    DEL_PARENT_CALLBACKS( SendEventsToChild );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to adjust modified image,
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    int pi_luma[256];
    int pi_gamma[256];

    picture_t *p_outpic;
    uint8_t *p_in, *p_in_v, *p_in_end, *p_line_end;
    uint8_t *p_out, *p_out_v;

    double  f_hue;
    double  f_gamma;
    int32_t i_cont, i_lum;
    int i_sat, i_sin, i_cos, i_x, i_y;
    int i;
    vlc_value_t val;

    /* This is a new frame. Get a structure from the video_output. */
    while( ( p_outpic = vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
              == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    vout_DatePicture( p_vout->p_sys->p_vout, p_outpic, p_pic->date );
    vout_LinkPicture( p_vout->p_sys->p_vout, p_outpic );

    /* Getvariables */
    var_Get( p_vout, "contrast", &val );
    i_cont = (int) ( val.f_float * 255 );
    var_Get( p_vout, "brightness", &val );
    i_lum = (int) (( val.f_float - 1.0 ) * 255 );
    var_Get( p_vout, "hue", &val );
    f_hue = (float) ( val.i_int * M_PI / 180 );
    var_Get( p_vout, "saturation", &val );
    i_sat = (int) (val.f_float * 256 );
    var_Get( p_vout, "gamma", &val );
    f_gamma = 1.0 / val.f_float;

    /* Contrast is a fast but kludged function, so I put this gap to be
     * cleaner :) */
    i_lum += 128 - i_cont / 2;

    /* Fill the gamma lookup table */
    for( i = 0 ; i < 256 ; i++ )
    {
      pi_gamma[ i ] = clip( pow(i / 255.0, f_gamma) * 255.0);
    }

    /* Fill the luma lookup table */
    for( i = 0 ; i < 256 ; i++ )
    {
        pi_luma[ i ] = pi_gamma[clip( i_lum + i_cont * i / 256)];
    }

    /*
     * Do the Y plane
     */

    p_in = p_pic->p[0].p_pixels;
    p_in_end = p_in + p_pic->p[0].i_visible_lines * p_pic->p[0].i_pitch - 8;

    p_out = p_outpic->p[0].p_pixels;

    for( ; p_in < p_in_end ; )
    {
        p_line_end = p_in + p_pic->p[0].i_visible_pitch - 8;

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

        p_in += p_pic->p[0].i_pitch - p_pic->p[0].i_visible_pitch;
        p_out += p_outpic->p[0].i_pitch - p_outpic->p[0].i_visible_pitch;
    }

    /*
     * Do the U and V planes
     */

    p_in = p_pic->p[1].p_pixels;
    p_in_v = p_pic->p[2].p_pixels;
    p_in_end = p_in + p_pic->p[1].i_visible_lines * p_pic->p[1].i_pitch - 8;

    p_out = p_outpic->p[1].p_pixels;
    p_out_v = p_outpic->p[2].p_pixels;

    i_sin = sin(f_hue) * 256;
    i_cos = cos(f_hue) * 256;

    i_x = ( cos(f_hue) + sin(f_hue) ) * 32768;
    i_y = ( cos(f_hue) - sin(f_hue) ) * 32768;

    if ( i_sat > 256 )
    {
#define WRITE_UV_CLIP() \
    i_u = *p_in++ ; i_v = *p_in_v++ ; \
    *p_out++ = clip( (( ((i_u * i_cos + i_v * i_sin - i_x) >> 8) \
                           * i_sat) >> 8) + 128); \
    *p_out_v++ = clip( (( ((i_v * i_cos - i_u * i_sin - i_y) >> 8) \
                           * i_sat) >> 8) + 128)

        uint8_t i_u, i_v;

        for( ; p_in < p_in_end ; )
        {
            p_line_end = p_in + p_pic->p[1].i_visible_pitch - 8;

            for( ; p_in < p_line_end ; )
            {
                /* Do 8 pixels at a time */
                WRITE_UV_CLIP(); WRITE_UV_CLIP();
                WRITE_UV_CLIP(); WRITE_UV_CLIP();
                WRITE_UV_CLIP(); WRITE_UV_CLIP();
                WRITE_UV_CLIP(); WRITE_UV_CLIP();
            }

            p_line_end += 8;

            for( ; p_in < p_line_end ; )
            {
                WRITE_UV_CLIP();
            }

            p_in += p_pic->p[1].i_pitch - p_pic->p[1].i_visible_pitch;
            p_in_v += p_pic->p[2].i_pitch - p_pic->p[2].i_visible_pitch;
            p_out += p_outpic->p[1].i_pitch - p_outpic->p[1].i_visible_pitch;
            p_out_v += p_outpic->p[2].i_pitch - p_outpic->p[2].i_visible_pitch;
        }
    }
    else
    {
#define WRITE_UV() \
    i_u = *p_in++ ; i_v = *p_in_v++ ; \
    *p_out++ = (( ((i_u * i_cos + i_v * i_sin - i_x) >> 8) \
                       * i_sat) >> 8) + 128; \
    *p_out_v++ = (( ((i_v * i_cos - i_u * i_sin - i_y) >> 8) \
                       * i_sat) >> 8) + 128

        uint8_t i_u, i_v;

        for( ; p_in < p_in_end ; )
        {
            p_line_end = p_in + p_pic->p[1].i_visible_pitch - 8;

            for( ; p_in < p_line_end ; )
            {
                /* Do 8 pixels at a time */
                WRITE_UV(); WRITE_UV(); WRITE_UV(); WRITE_UV();
                WRITE_UV(); WRITE_UV(); WRITE_UV(); WRITE_UV();
            }

            p_line_end += 8;

            for( ; p_in < p_line_end ; )
            {
                WRITE_UV();
            }

            p_in += p_pic->p[1].i_pitch - p_pic->p[1].i_visible_pitch;
            p_in_v += p_pic->p[2].i_pitch - p_pic->p[2].i_visible_pitch;
            p_out += p_outpic->p[1].i_pitch - p_outpic->p[1].i_visible_pitch;
            p_out_v += p_outpic->p[2].i_pitch - p_outpic->p[2].i_visible_pitch;
        }
    }

    vout_UnlinkPicture( p_vout->p_sys->p_vout, p_outpic );

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    var_Set( (vlc_object_t *)p_data, psz_var, newval );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendEventsToChild: forward events to the child/children vout
 *****************************************************************************/
static int SendEventsToChild( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    var_Set( p_vout->p_sys->p_vout, psz_var, newval );
    return VLC_SUCCESS;
}
