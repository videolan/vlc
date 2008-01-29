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

#include <math.h>                                            /* sin(), cos() */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_vout.h>

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

static int PreciseRotateCallback( vlc_object_t *p_this, char const *psz_var,
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

    add_integer_with_range( FILTER_PREFIX "angle", 30, 0, 359, NULL,
        ANGLE_TEXT, ANGLE_LONGTEXT, VLC_FALSE );

    add_shortcut( "rotate" );
    set_callbacks( Create, Destroy );
vlc_module_end();

static const char *ppsz_filter_options[] = {
    "angle", NULL
};

/*****************************************************************************
 * filter_sys_t
 *****************************************************************************/
struct filter_sys_t
{
    int     i_angle;
    int     i_cos;
    int     i_sin;
};

static inline void cache_trigo( int i_angle, int *i_sin, int *i_cos )
{
    const double f_angle = (((double)i_angle)*M_PI)/1800.;
    *i_sin = (int)(sin( f_angle )*4096.);
    *i_cos = (int)(cos( f_angle )*4096.);
}

/*****************************************************************************
 * Create: allocates Distort video filter
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

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

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }
    p_sys = p_filter->p_sys;

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    p_sys->i_angle = var_CreateGetIntegerCommand( p_filter,
                                                  FILTER_PREFIX "angle" ) * 10;
    var_Create( p_filter, FILTER_PREFIX "deciangle",
                VLC_VAR_INTEGER|VLC_VAR_ISCOMMAND );
    var_AddCallback( p_filter, FILTER_PREFIX "angle", RotateCallback, p_sys );
    var_AddCallback( p_filter, FILTER_PREFIX "deciangle",
                     PreciseRotateCallback, p_sys );

    cache_trigo( p_sys->i_angle, &p_sys->i_sin, &p_sys->i_cos );

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Distort filter
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    free( p_filter->p_sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_plane;
    const int i_sin = p_sys->i_sin, i_cos = p_sys->i_cos;

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
        const int i_visible_lines = p_pic->p[i_plane].i_visible_lines;
        const int i_visible_pitch = p_pic->p[i_plane].i_visible_pitch;
        const int i_pitch         = p_pic->p[i_plane].i_pitch;
        const int i_hidden_pitch  = i_pitch - i_visible_pitch;

        const int i_aspect = ( i_visible_lines * p_pic->p[Y_PLANE].i_visible_pitch ) / ( p_pic->p[Y_PLANE].i_visible_lines * i_visible_pitch );
        /* = 2 for U and V planes in YUV 4:2:2, = 1 otherwise */

        const int i_line_center = i_visible_lines>>1;
        const int i_col_center  = i_visible_pitch>>1;

        const uint8_t *p_in = p_pic->p[i_plane].p_pixels;
        uint8_t *p_out = p_outpic->p[i_plane].p_pixels;
        uint8_t *p_outendline = p_out + i_visible_pitch;
        const uint8_t *p_outend = p_out + i_visible_lines * i_pitch;

        const uint8_t black_pixel = ( i_plane == Y_PLANE ) ? 0x00 : 0x80;

        const int i_line_next =  i_cos / i_aspect -i_sin*i_visible_pitch;
        const int i_col_next  = -i_sin / i_aspect -i_cos*i_visible_pitch;
        int i_line_orig0 = ( - i_cos * i_line_center / i_aspect
                             - i_sin * i_col_center + (1<<11) );
        int i_col_orig0 =    i_sin * i_line_center / i_aspect
                           - i_cos * i_col_center + (1<<11);
        for( ; p_outendline < p_outend;
             p_out += i_hidden_pitch, p_outendline += i_pitch,
             i_line_orig0 += i_line_next, i_col_orig0 += i_col_next )
        {
            for( ; p_out < p_outendline;
                 p_out++, i_line_orig0 += i_sin, i_col_orig0 += i_cos )
            {
                const int i_line_orig = (i_line_orig0>>12)*i_aspect + i_line_center;
                const int i_col_orig  = (i_col_orig0>>12)  + i_col_center;
                const uint8_t* p_orig_offset = p_in + i_line_orig * i_pitch
                                                + i_col_orig;
                const uint8_t i_line_percent = (i_line_orig0>>4) & 255;
                const uint8_t i_col_percent  = (i_col_orig0 >>4) & 255;

                if(    -1 <= i_line_orig && i_line_orig < i_visible_lines
                    && -1 <= i_col_orig  && i_col_orig  < i_visible_pitch )
                {
                #define test 1
                #undef test
                #ifdef test
                    if( ( i_col_orig > i_visible_pitch/2 ) )
                #endif
                    {
                        uint8_t i_curpix = black_pixel;
                        uint8_t i_colpix = black_pixel;
                        uint8_t i_linpix = black_pixel;
                        uint8_t i_nexpix = black_pixel;
                        if( ( 0 <= i_line_orig ) && ( 0 <= i_col_orig ) )
                            i_curpix = *p_orig_offset;
                        p_orig_offset++;

                        if(  ( i_col_orig < i_visible_pitch - 1)
                             && ( i_line_orig >= 0 ) )
                            i_colpix = *p_orig_offset;

                        p_orig_offset+=i_pitch;
                        if( ( i_line_orig < i_visible_lines - 1)
                            && ( i_col_orig  < i_visible_pitch - 1) )
                            i_nexpix = *p_orig_offset;

                        p_orig_offset--;
                        if(  ( i_line_orig < i_visible_lines - 1)
                             && ( i_col_orig >= 0 ) )
                            i_linpix = *p_orig_offset;

                        unsigned int temp = 0;
                        temp+= i_curpix *
                            (256 - i_line_percent) * ( 256 - i_col_percent );
                        temp+= i_linpix *
                            i_line_percent * (256 - i_col_percent );
                        temp+= i_nexpix *
                            ( i_col_percent) * ( i_line_percent);
                        temp+= i_colpix *
                            i_col_percent * (256 - i_line_percent );
                        *p_out = temp >> 16;
                    }
                #ifdef test
                    else if (i_col_orig == i_visible_pitch/2 )
                    {   *p_out = black_pixel;
                    }
                    else
                        *p_out = *p_orig_offset;
                #endif
                #undef test
                }
                else
                {
                    *p_out = black_pixel;
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

/*****************************************************************************
 * Angle modification callbacks.
 *****************************************************************************/
static int RotateCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;
    p_sys->i_angle = newval.i_int*10;
    cache_trigo( p_sys->i_angle, &p_sys->i_sin, &p_sys->i_cos );
    return VLC_SUCCESS;
}

static int PreciseRotateCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;
    p_sys->i_angle = newval.i_int;
    cache_trigo( p_sys->i_angle, &p_sys->i_sin, &p_sys->i_cos );
    return VLC_SUCCESS;
}
