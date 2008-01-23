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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_vout.h>

#include "vlc_filter.h"

#include <math.h>                                          /* exp(), sqrt() */

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

/* Comment this to use floats instead of integers (faster for bigger sigma
 * values)
 * For sigma = 2 ints are faster
 * For sigma = 4 floats are faster
 */
#define DONT_USE_FLOATS
struct filter_sys_t
{
    double f_sigma;
    int i_dim;
#ifdef DONT_USE_FLOATS
    int *pi_distribution;
    int *pi_buffer;
    int *pi_scale;
#else
    float *pf_distribution;
    float *pf_buffer;
    float *pf_scale;
#endif
};

static void gaussianblur_InitDistribution( filter_sys_t *p_sys )
{
    double f_sigma = p_sys->f_sigma;
    int i_dim = (int)(3.*f_sigma);
#ifdef DONT_USE_FLOATS
    int *pi_distribution = (int*)malloc( (2*i_dim+1) * sizeof( int ) );
#else
    float *pf_distribution = (float*)malloc( (2*i_dim+1) * sizeof( float ) );
#endif
    int x;
    for( x = -i_dim; x <= i_dim; x++ )
    {
#ifdef DONT_USE_FLOATS
        pi_distribution[i_dim+x] =
            (int)( sqrt( exp(-(x*x)/(f_sigma*f_sigma) )
                 / (2.*M_PI*f_sigma*f_sigma) )  * (double)(1<<8) );
        printf("%d\n",pi_distribution[i_dim+x]);
#else
        pf_distribution[i_dim+x] = (float)
            sqrt( exp(-(x*x)/(f_sigma*f_sigma) ) / (2.*M_PI*f_sigma*f_sigma) );
        printf("%f\n",pf_distribution[i_dim+x]);
#endif
    }
    p_sys->i_dim = i_dim;
#ifdef DONT_USE_FLOATS
    p_sys->pi_distribution = pi_distribution;
#else
    p_sys->pf_distribution = pf_distribution;
#endif
}

static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    if(   p_filter->fmt_in.video.i_chroma != VLC_FOURCC('I','4','2','0')
       && p_filter->fmt_in.video.i_chroma != VLC_FOURCC('I','Y','U','V')
       && p_filter->fmt_in.video.i_chroma != VLC_FOURCC('J','4','2','0')
       && p_filter->fmt_in.video.i_chroma != VLC_FOURCC('Y','V','1','2')

       && p_filter->fmt_in.video.i_chroma != VLC_FOURCC('I','4','2','2')
       && p_filter->fmt_in.video.i_chroma != VLC_FOURCC('J','4','2','2')
      )
    {
        /* We only want planar YUV 4:2:0 or 4:2:2 */
        msg_Err( p_filter, "Unsupported input chroma (%4s)",
                 (char*)&(p_filter->fmt_in.video.i_chroma) );
        return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

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
#ifdef DONT_USE_FLOATS
    p_filter->p_sys->pi_buffer = NULL;
    p_filter->p_sys->pi_scale = NULL;
#else
    p_filter->p_sys->pf_buffer = NULL;
    p_filter->p_sys->pf_scale = NULL;
#endif

    return VLC_SUCCESS;
}

static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
#ifdef DONT_USE_FLOATS
    free( p_filter->p_sys->pi_distribution );
    free( p_filter->p_sys->pi_buffer );
    free( p_filter->p_sys->pi_scale );
#else
    free( p_filter->p_sys->pf_distribution );
    free( p_filter->p_sys->pf_buffer );
    free( p_filter->p_sys->pf_scale );
#endif
    free( p_filter->p_sys );
}

static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_plane;
    const int i_dim = p_sys->i_dim;
#ifdef DONT_USE_FLOATS
    int *pi_buffer;
    int *pi_scale;
    const int *pi_distribution = p_sys->pi_distribution;
#else
    float *pf_buffer;
    float *pf_scale;
    const float *pf_distribution = p_sys->pf_distribution;
#endif
    if( !p_pic ) return NULL;

    p_outpic = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }
#ifdef DONT_USE_FLOATS
    if( !p_sys->pi_buffer )
    {
        p_sys->pi_buffer = (int*)realloc( p_sys->pi_buffer,
                                          p_pic->p[Y_PLANE].i_visible_lines
                                          * p_pic->p[Y_PLANE].i_pitch
                                          * sizeof( int ) );
    }
    pi_buffer = p_sys->pi_buffer;
#else
    if( !p_sys->pf_buffer )
    {
        p_sys->pf_buffer = (float*)realloc( p_sys->pf_buffer,
                                            p_pic->p[Y_PLANE].i_visible_lines
                                            * p_pic->p[Y_PLANE].i_pitch
                                            * sizeof( float ) );
    }
    pf_buffer = p_sys->pf_buffer;
#endif
#ifdef DONT_USE_FLOATS
    if( !p_sys->pi_scale )
#else
    if( !p_sys->pf_scale )
#endif
    {
        const int i_visible_lines = p_pic->p[Y_PLANE].i_visible_lines;
        const int i_visible_pitch = p_pic->p[Y_PLANE].i_visible_pitch;
        const int i_pitch = p_pic->p[Y_PLANE].i_pitch;
        int i_col, i_line;
#ifdef DONT_USE_FLOATS
        p_sys->pi_scale = (int*)malloc( i_visible_lines * i_pitch
                                        * sizeof( int ) );
        pi_scale = p_sys->pi_scale;
#else
        p_sys->pf_scale = (float*)malloc( i_visible_lines * i_pitch
                                          * sizeof( float ) );
        pf_scale = p_sys->pf_scale;
#endif
        for( i_line = 0 ; i_line < i_visible_lines ; i_line++ )
        {
            for( i_col = 0; i_col < i_visible_pitch ; i_col++ )
            {
                int x, y;
#ifdef DONT_USE_FLOATS
                int value = 0;
#else
                double value = 0.;
#endif
                for( y = __MAX( -i_dim, -i_line );
                     y <= __MIN( i_dim, i_visible_lines - i_line - 1 );
                     y++ )
                {
                    for( x = __MAX( -i_dim, -i_col );
                         x <= __MIN( i_dim, i_visible_pitch - i_col + 1 );
                         x++ )
                    {
#ifdef DONT_USE_FLOATS
                        value += pi_distribution[y+i_dim]
                               * pi_distribution[x+i_dim];
#else
                        value += ((double)pf_distribution[y+i_dim])
                               * ((double)pf_distribution[x+i_dim]);
#endif
                    }
                }
#ifdef DONT_USE_FLOATS
                pi_scale[i_line*i_pitch+i_col] = value;
#else
                pf_scale[i_line*i_pitch+i_col] = (float)(1./value);
#endif
            }
        }
    }
#ifdef DONT_USE_FLOATS
    pi_scale = p_sys->pi_scale;
#else
    pf_scale = p_sys->pf_scale;
#endif

    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {

        uint8_t *p_in = p_pic->p[i_plane].p_pixels;
        uint8_t *p_out = p_outpic->p[i_plane].p_pixels;

        const int i_visible_lines = p_pic->p[i_plane].i_visible_lines;
        const int i_visible_pitch = p_pic->p[i_plane].i_visible_pitch;
        const int i_pitch = p_pic->p[i_plane].i_pitch;

        int i_line, i_col;
        const int x_factor = p_pic->p[Y_PLANE].i_visible_pitch/i_visible_pitch-1;
        const int y_factor = p_pic->p[Y_PLANE].i_visible_lines/i_visible_lines-1;

        for( i_line = 0 ; i_line < i_visible_lines ; i_line++ )
        {
            for( i_col = 0; i_col < i_visible_pitch ; i_col++ )
            {
#ifdef DONT_USE_FLOATS
                int value = 0;
#else
                float value = 0.;
#endif
                int x;
                const int c = i_line*i_pitch+i_col;
                for( x = __MAX( -i_dim, -i_col*(x_factor+1) );
                     x <= __MIN( i_dim, (i_visible_pitch - i_col)*(x_factor+1) + 1 );
                     x++ )
                {
#ifdef DONT_USE_FLOATS
                    value += pi_distribution[x+i_dim]
                           * p_in[c+(x>>x_factor)];
#else
                    value += pf_distribution[x+i_dim]
                           * (float)p_in[c+(x>>x_factor)];
#endif
                }
#ifdef DONT_USE_FLOATS
                pi_buffer[c] = value;
#else
                pf_buffer[c] = value;
#endif
            }
        }
        for( i_line = 0 ; i_line < i_visible_lines ; i_line++ )
        {
            for( i_col = 0; i_col < i_visible_pitch ; i_col++ )
            {
#ifdef DONT_USE_FLOATS
                int value = 0;
#else
                float value = 0.;
#endif
                int y;
                const int c = i_line*i_pitch+i_col;
                for( y = __MAX( -i_dim, (-i_line)*(y_factor+1) );
                     y <= __MIN( i_dim, (i_visible_lines - i_line)*(y_factor+1) - 1 );
                     y++ )
                {
#ifdef DONT_USE_FLOATS
                    value += pi_distribution[y+i_dim]
                           * pi_buffer[c+(y>>y_factor)*i_pitch];
#else
                    value += pf_distribution[y+i_dim]
                           * pf_buffer[c+(y>>y_factor)*i_pitch];
#endif
                }
#ifdef DONT_USE_FLOATS
                p_out[c] = (uint8_t)(value/pi_scale[(i_line<<y_factor)*(i_pitch<<x_factor)+(i_col<<x_factor)]);
#else
                p_out[c] = (uint8_t)(value*pf_scale[(i_line<<y_factor)*(i_pitch<<x_factor)+(i_col<<x_factor)]);
#endif
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
