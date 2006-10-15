/*****************************************************************************
 * rotate.c : video rotation filter
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <vlc/decoder.h>

#include "vlc_filter.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

static int RotateCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data );

#define ANGLE_TEXT N_("Angle in degrees")
#define ANGLE_LONGTEXT N_("Angle in degrees (0 to 359)")

#define FILTER_PREFIX "rotate-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Rotate video filter") );
    set_shortname( _( "Rotate" ));
    set_capability( "video filter2", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    add_integer_with_range( FILTER_PREFIX "angle", 0, 0, 359, NULL,
        ANGLE_TEXT, ANGLE_LONGTEXT, VLC_FALSE );

    add_shortcut( "rotate" );
    set_callbacks( Create, Destroy );
vlc_module_end();

static const char *ppsz_filter_options[] = {
    "angle", NULL
};

/*****************************************************************************
 * vout_sys_t: Distort video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Distort specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    int     i_angle;
    int     i_cos;
    int     i_sin;

    mtime_t last_date;
};

/*****************************************************************************
 * Create: allocates Distort video thread output method
 *****************************************************************************
 * This function allocates and initializes a Distort vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    var_Create( p_filter, FILTER_PREFIX "angle",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    p_filter->pf_video_filter = Filter;

    p_filter->p_sys->i_angle = var_GetInteger( p_filter,
                                               FILTER_PREFIX "angle" );
    p_filter->p_sys->last_date = 0;

    var_Create( p_filter->p_libvlc, "rotate_angle", VLC_VAR_INTEGER );
    var_AddCallback( p_filter->p_libvlc,
                     "rotate_angle", RotateCallback, p_filter->p_sys );
    var_SetInteger( p_filter->p_libvlc, "rotate_angle",
                    p_filter->p_sys->i_angle );

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

    var_DelCallback( p_filter->p_libvlc,
                     "rotate_angle", RotateCallback, p_filter->p_sys );
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
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_index;
    int i_sin = p_sys->i_sin, i_cos = p_sys->i_cos;
    mtime_t new_date = mdate();

    if( !p_pic ) return NULL;

    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }

    p_sys->last_date = new_date;

    for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
    {
        int i_line, i_num_lines, i_line_center, i_col, i_num_cols, i_col_center;
        uint8_t black_pixel;
        uint8_t *p_in, *p_out;

        p_in = p_pic->p[i_index].p_pixels;
        p_out = p_outpic->p[i_index].p_pixels;

        i_num_lines = p_pic->p[i_index].i_visible_lines;
        i_num_cols = p_pic->p[i_index].i_visible_pitch;
        i_line_center = i_num_lines/2;
        i_col_center = i_num_cols/2;

        black_pixel = ( i_index == Y_PLANE ) ? 0x00 : 0x80;

        for( i_line = 0 ; i_line < i_num_lines ; i_line++ )
        {
            for( i_col = 0; i_col < i_num_cols ; i_col++ )
            {
                int i_line_orig, i_col_orig;
                i_line_orig = ( ( i_cos * (i_line-i_line_center)
                                + i_sin * (i_col-i_col_center)
                                + 128 )>>8 )
                              + i_line_center;
                i_col_orig = ( (-i_sin * (i_line-i_line_center)
                               + i_cos * (i_col-i_col_center)
                               + 128 )>>8 )
                             + i_col_center;

                if(    0 <= i_line_orig && i_line_orig < i_num_lines
                    && 0 <= i_col_orig && i_col_orig < i_num_cols )
                {

                    p_out[i_line*i_num_cols+i_col] =
                        p_in[i_line_orig*i_num_cols+i_col_orig];
                }
                else
                {
                    p_out[i_line*i_num_cols+i_col] = black_pixel;
                }
            }
        }
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

static int RotateCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_data;

    if( !strcmp( psz_var, "rotate_angle" ) )
    {
        double f_angle;

        p_sys->i_angle = newval.i_int;

        f_angle = (((double)p_sys->i_angle)*M_PI)/180.;
        p_sys->i_sin = (int)(sin( f_angle )*256.);
        p_sys->i_cos = (int)(cos( f_angle )*256.);
    }
    return VLC_SUCCESS;
}
