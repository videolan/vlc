/*****************************************************************************
 * gaussianblur.c : gaussian blur video filter
 *****************************************************************************
 * Copyright (C) 2000-2007 the VideoLAN team
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

#include <vlc/vlc.h>
#include <vlc_vout.h>

#include "vlc_filter.h"

#include <math.h>                                                  /* exp() */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

#define SIGMA_TEXT N_("Gaussian's std deviation")
#define SIGMA_LONGTEXT N_( \
    "Gaussian's standard deviation. The bluring will take " \
    "into account pixels up to 3*sigma away in any direction.")

#define FILTER_PREFIX "gaussianblur-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Gaussian blur video filter") );
    set_shortname( _( "Gaussian Blur" ));
    set_capability( "video filter2", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    add_float( FILTER_PREFIX "sigma", 2., NULL, SIGMA_TEXT, SIGMA_LONGTEXT,
               VLC_FALSE );

    set_callbacks( Create, Destroy );
vlc_module_end();

static const char *ppsz_filter_options[] = {
    "sigma", NULL
};

struct filter_sys_t
{
    double f_sigma;
    int *pi_distribution;
    int i_dim;
};

static void gaussianblur_InitDistribution( filter_sys_t *p_sys )
{
    double f_sigma = p_sys->f_sigma;
    int i_dim = (int)(3.*f_sigma);
    int *pi_distribution = (int*)malloc( (2*i_dim+1) * (2*i_dim+1) * sizeof( int ) );
    int x, y;
    for( x = -i_dim; x <= i_dim; x++ )
        for( y = -i_dim; y <= i_dim; y++ )
            pi_distribution[(i_dim+y)*(2*i_dim+1)+(i_dim+x)] =
                (int)( exp(-(x*x+y*y)/(2.*f_sigma*f_sigma))
                       / (2.*M_PI*f_sigma*f_sigma) * (double)(1<<16) );
    p_sys->i_dim = i_dim;
    p_sys->pi_distribution = pi_distribution;
}

static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    p_filter->pf_video_filter = Filter;

    p_filter->p_sys->f_sigma =
        var_CreateGetFloat( p_filter, FILTER_PREFIX "sigma" );
    if( p_filter->p_sys->f_sigma <= 0. )
    {
        msg_Err( p_filter, "sigma must be positive" );
        return VLC_EGENERIC;
    }
    gaussianblur_InitDistribution( p_filter->p_sys );
    msg_Dbg( p_filter, "gaussian distribution is %d pixels wide",
             p_filter->p_sys->i_dim*2+1 );

    return VLC_SUCCESS;
}

static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    free( p_filter->p_sys->pi_distribution );
    free( p_filter->p_sys );
}

static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_plane;

    if( !p_pic ) return NULL;

    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }

    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {

        uint8_t *p_in = p_pic->p[i_plane].p_pixels;
        uint8_t *p_out = p_outpic->p[i_plane].p_pixels;

        const int i_visible_lines = p_pic->p[i_plane].i_visible_lines;
        const int i_visible_pitch = p_pic->p[i_plane].i_visible_pitch;
        const int i_pitch = p_pic->p[i_plane].i_pitch;

        const int i_dim = p_sys->i_dim;
        const int i_dim2 = 2*i_dim+1;
        const int *pi_distribution = p_sys->pi_distribution;

        int i_line, i_col;
        const int factor = i_plane ? 1 : 0;

        for( i_line = 0 ; i_line < i_visible_lines ; i_line++ )
        {
            uint8_t *p_o = &p_out[i_line*i_pitch];
            for( i_col = 0; i_col < i_visible_pitch ; i_col++ )
            {
                int value = 0;
                int scale = 0;
                int x, y;
                for( y = __MAX( -i_dim, -i_line );
                     y <= __MIN( i_dim, i_visible_lines - i_line - 1 );
                     y++ )
                {
                    const int *pi_d = &pi_distribution[(y+i_dim)*i_dim2+i_dim];
                    const uint8_t *p_i = &p_in[(i_line+(y>>factor))*i_pitch+(i_col)];
                    for( x = __MAX( -i_dim, -i_col );
                         x <= __MIN( i_dim, i_visible_pitch - i_col + 1 );
                         x++ )
                    {
                         const int weight = pi_d[x];
                         value += weight * p_i[x>>factor];
                         scale += weight;
                    }
                }
                p_o[i_col] = value / scale;
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
