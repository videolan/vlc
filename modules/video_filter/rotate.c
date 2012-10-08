/*****************************************************************************
 * rotate.c : video rotation filter
 *****************************************************************************
 * Copyright (C) 2000-2008 the VideoLAN team
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

#include <math.h>                                            /* sin(), cos() */

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_filter.h>
#include "filter_picture.h"
#include "../control/motionlib.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static picture_t *FilterPacked( filter_t *, picture_t * );

static int RotateCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data );

static int PreciseRotateCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data );

#define ANGLE_TEXT N_("Angle in degrees")
#define ANGLE_LONGTEXT N_("Angle in degrees (0 to 359)")
#define MOTION_TEXT N_("Use motion sensors")
#define MOTION_LONGTEXT N_("Use HDAPS, AMS, APPLESMC or UNIMOTION motion " \
    "sensors to rotate the video")

#define FILTER_PREFIX "rotate-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Rotate video filter") )
    set_shortname( N_( "Rotate" ))
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_integer_with_range( FILTER_PREFIX "angle", 30, 0, 359,
        ANGLE_TEXT, ANGLE_LONGTEXT, false )
    add_bool( FILTER_PREFIX "use-motion", false, MOTION_TEXT,
              MOTION_LONGTEXT, false )

    add_shortcut( "rotate" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "angle", "use-motion", NULL
};

/*****************************************************************************
 * filter_sys_t
 *****************************************************************************/
struct filter_sys_t
{
    vlc_spinlock_t lock;
    int            i_cos;
    int            i_sin;
    int            i_angle;
    motion_sensors_t *p_motion;
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

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

    switch( p_filter->fmt_in.video.i_chroma )
    {
        CASE_PLANAR_YUV
            p_filter->pf_video_filter = Filter;
            break;

        CASE_PACKED_YUV_422
            p_filter->pf_video_filter = FilterPacked;
            break;

        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;
    p_sys = p_filter->p_sys;

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    p_sys->p_motion = NULL;
    if( var_InheritBool( p_filter, FILTER_PREFIX "use-motion" ) )
    {
        p_sys->p_motion = motion_create( VLC_OBJECT( p_filter ) );
        if( p_sys->p_motion == NULL )
        {
            free( p_filter->p_sys );
            return VLC_EGENERIC;
        }
    }
    else
    {
        int i_angle = var_CreateGetIntegerCommand( p_filter,
                                                   FILTER_PREFIX "angle" ) * 10;
        cache_trigo( i_angle, &p_sys->i_sin, &p_sys->i_cos );
        var_Create( p_filter, FILTER_PREFIX "deciangle",
                    VLC_VAR_INTEGER|VLC_VAR_ISCOMMAND );
        vlc_spin_init( &p_sys->lock );
        var_AddCallback( p_filter, FILTER_PREFIX "angle",
                         RotateCallback, p_sys );
        var_AddCallback( p_filter, FILTER_PREFIX "deciangle",
                         PreciseRotateCallback, p_sys );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Distort filter
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->p_motion != NULL )
        motion_destroy( p_sys->p_motion );
    else
    {
        var_DelCallback( p_filter, FILTER_PREFIX "angle",
                         RotateCallback, p_sys );
        var_DelCallback( p_filter, FILTER_PREFIX "deciangle",
                         PreciseRotateCallback, p_sys );
        vlc_spin_destroy( &p_sys->lock );
    }
    free( p_sys );
}

/*****************************************************************************
 *
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

    if( p_sys->p_motion != NULL )
    {
        int i_angle = motion_get_angle( p_sys->p_motion );
        if( p_sys->i_angle != i_angle )
        {
            p_sys->i_angle = i_angle;
            i_angle = ((3600+i_angle/2)%3600);
            cache_trigo( i_angle, &p_sys->i_sin, &p_sys->i_cos );
        }
    }
    else
        vlc_spin_lock( &p_sys->lock );

    const int i_sin = p_sys->i_sin;
    const int i_cos = p_sys->i_cos;

    if( p_sys->p_motion == NULL )
        vlc_spin_unlock( &p_sys->lock );

    for( int i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        plane_t *p_srcp = &p_pic->p[i_plane];
        plane_t *p_dstp = &p_outpic->p[i_plane];

        const int i_visible_lines = p_srcp->i_visible_lines;
        const int i_visible_pitch = p_srcp->i_visible_pitch;

        const int i_aspect = __MAX( 1, ( i_visible_lines * p_pic->p[Y_PLANE].i_visible_pitch ) / ( p_pic->p[Y_PLANE].i_visible_lines * i_visible_pitch ));
        /* = 2 for U and V planes in YUV 4:2:2, = 1 otherwise */

        const int i_line_center = i_visible_lines>>1;
        const int i_col_center  = i_visible_pitch>>1;

        const uint8_t black_pixel = ( i_plane == Y_PLANE ) ? 0x00 : 0x80;

        const int i_line_next =  i_cos / i_aspect -i_sin*i_visible_pitch;
        const int i_col_next  = -i_sin / i_aspect -i_cos*i_visible_pitch;
        int i_line_orig0 = ( - i_cos * i_line_center / i_aspect
                             - i_sin * i_col_center + (1<<11) );
        int i_col_orig0 =    i_sin * i_line_center / i_aspect
                           - i_cos * i_col_center + (1<<11);
        for( int y = 0; y < i_visible_lines; y++)
        {
            uint8_t *p_out = &p_dstp->p_pixels[y * p_dstp->i_pitch];

            for( int x = 0; x < i_visible_pitch; x++, p_out++ )
            {
                const int i_line_orig = (i_line_orig0>>12)*i_aspect + i_line_center;
                const int i_col_orig  = (i_col_orig0>>12)  + i_col_center;
                const uint8_t *p_orig_offset = &p_srcp->p_pixels[i_line_orig * p_srcp->i_pitch + i_col_orig];
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

                        p_orig_offset += p_srcp->i_pitch;
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

                i_line_orig0 += i_sin;
                i_col_orig0 += i_cos;
            }

            i_line_orig0 += i_line_next;
            i_col_orig0 += i_col_next;
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

/*****************************************************************************
 *
 *****************************************************************************/
static picture_t *FilterPacked( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_pic ) return NULL;

    int i_u_offset, i_v_offset, i_y_offset;

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
        picture_Release( p_pic );
        return NULL;
    }

    const int i_visible_pitch = p_pic->p->i_visible_pitch>>1; /* In fact it's i_visible_pixels */
    const int i_visible_lines = p_pic->p->i_visible_lines;

    const uint8_t *p_in   = p_pic->p->p_pixels+i_y_offset;
    const uint8_t *p_in_u = p_pic->p->p_pixels+i_u_offset;
    const uint8_t *p_in_v = p_pic->p->p_pixels+i_v_offset;
    const int i_in_pitch  = p_pic->p->i_pitch;

    uint8_t *p_out   = p_outpic->p->p_pixels+i_y_offset;
    uint8_t *p_out_u = p_outpic->p->p_pixels+i_u_offset;
    uint8_t *p_out_v = p_outpic->p->p_pixels+i_v_offset;
    const int i_out_pitch = p_outpic->p->i_pitch;

    const int i_line_center = i_visible_lines>>1;
    const int i_col_center  = i_visible_pitch>>1;

    if( p_sys->p_motion != NULL )
    {
        int i_angle = motion_get_angle( p_sys->p_motion );
        if( p_sys->i_angle != i_angle )
        {
            p_sys->i_angle = i_angle;
            i_angle = ((3600+i_angle/2)%3600);
            cache_trigo( i_angle, &p_sys->i_sin, &p_sys->i_cos );
        }
    }
    else
        vlc_spin_lock( &p_sys->lock );

    const int i_sin = p_sys->i_sin;
    const int i_cos = p_sys->i_cos;

    if( p_sys->p_motion == NULL )
        vlc_spin_unlock( &p_sys->lock );

    int i_col, i_line;
    for( i_line = 0; i_line < i_visible_lines; i_line++ )
    {
        for( i_col = 0; i_col < i_visible_pitch; i_col++ )
        {
            int i_line_orig;
            int i_col_orig;
            /* Handle "1st Y", U and V */
            i_line_orig = i_line_center +
                ( ( i_sin * ( i_col - i_col_center )
                  + i_cos * ( i_line - i_line_center ) )>>12 );
            i_col_orig = i_col_center +
                ( ( i_cos * ( i_col - i_col_center )
                  - i_sin * ( i_line - i_line_center ) )>>12 );
            if( 0 <= i_col_orig && i_col_orig < i_visible_pitch
             && 0 <= i_line_orig && i_line_orig < i_visible_lines )
            {
                p_out[i_line*i_out_pitch+2*i_col] = p_in[i_line_orig*i_in_pitch+2*i_col_orig];
                i_col_orig /= 2;
                p_out_u[i_line*i_out_pitch+2*i_col] = p_in_u[i_line_orig*i_in_pitch+4*i_col_orig];
                p_out_v[i_line*i_out_pitch+2*i_col] = p_in_v[i_line_orig*i_in_pitch+4*i_col_orig];
            }
            else
            {
                p_out[i_line*i_out_pitch+2*i_col] = 0x00;
                p_out_u[i_line*i_out_pitch+2*i_col] = 0x80;
                p_out_v[i_line*i_out_pitch+2*i_col] = 0x80;
            }

            /* Handle "2nd Y" */
            i_col++;
            if( i_col >= i_visible_pitch )
                break;

            i_line_orig = i_line_center +
                ( ( i_sin * ( i_col - i_col_center )
                  + i_cos * ( i_line - i_line_center ) )>>12 );
            i_col_orig = i_col_center +
                ( ( i_cos * ( i_col - i_col_center )
                  - i_sin * ( i_line - i_line_center ) )>>12 );
            if( 0 <= i_col_orig && i_col_orig < i_visible_pitch
             && 0 <= i_line_orig && i_line_orig < i_visible_lines )
            {
                p_out[i_line*i_out_pitch+2*i_col] = p_in[i_line_orig*i_in_pitch+2*i_col_orig];
            }
            else
            {
                p_out[i_line*i_out_pitch+2*i_col] = 0x00;
            }
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

/*****************************************************************************
 * Angle modification callbacks.
 *****************************************************************************/
static int RotateCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    oldval.i_int *= 10;
    newval.i_int *= 10;
    return PreciseRotateCallback( p_this, psz_var, oldval, newval, p_data );
}

static int PreciseRotateCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;
    int i_sin, i_cos;

    cache_trigo( newval.i_int, &i_sin, &i_cos );
    vlc_spin_lock( &p_sys->lock );
    p_sys->i_sin = i_sin;
    p_sys->i_cos = i_cos;
    vlc_spin_unlock( &p_sys->lock );
    return VLC_SUCCESS;
}
