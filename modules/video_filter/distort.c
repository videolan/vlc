/*****************************************************************************
 * distort.c : Misc video effects plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <math.h>                                            /* sin(), cos() */

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "filter_common.h"

#define DISTORT_MODE_WAVE    1
#define DISTORT_MODE_RIPPLE  2

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static void DistortWave    ( vout_thread_t *, picture_t *, picture_t * );
static void DistortRipple  ( vout_thread_t *, picture_t *, picture_t * );

static int  SendEvents   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MODE_TEXT N_("Distort mode")
#define MODE_LONGTEXT N_("Distort mode, one of \"wave\" and \"ripple\"")

static char *mode_list[] = { "wave", "ripple" };
static char *mode_list_text[] = { N_("Wave"), N_("Ripple") };

vlc_module_begin();
    set_description( _("Distort video filter") );
    set_shortname( N_( "Distortion" ));
    set_capability( "video filter", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    add_string( "distort-mode", "wave", NULL, MODE_TEXT, MODE_LONGTEXT,
                VLC_FALSE );
        change_string_list( mode_list, mode_list_text, 0 );

    add_shortcut( "distort" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: Distort video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Distort specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    int i_mode;
    vout_thread_t *p_vout;

    /* For the wave mode */
    double  f_angle;
    mtime_t last_date;
};

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * Create: allocates Distort video thread output method
 *****************************************************************************
 * This function allocates and initializes a Distort vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *psz_method, *psz_method_tmp;

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

    p_vout->p_sys->i_mode = 0;

    if( !(psz_method = psz_method_tmp
          = config_GetPsz( p_vout, "distort-mode" )) )
    {
        msg_Err( p_vout, "configuration variable %s empty, using 'wave'",
                         "distort-mode" );
        p_vout->p_sys->i_mode = DISTORT_MODE_WAVE;
    }
    else
    {

        if( !strcmp( psz_method, "wave" ) )
        {
            p_vout->p_sys->i_mode = DISTORT_MODE_WAVE;
        }
        else if( !strcmp( psz_method, "ripple" ) )
        {
            p_vout->p_sys->i_mode = DISTORT_MODE_RIPPLE;
        }
        else
        {
            msg_Err( p_vout, "no valid distort mode provided, "
                             "using wave" );
            p_vout->p_sys->i_mode = DISTORT_MODE_WAVE;
        }
    }
    free( psz_method_tmp );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize Distort video thread output method
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
        msg_Err( p_vout, "cannot open vout, aborting" );
        return VLC_EGENERIC;
    }

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    ADD_PARENT_CALLBACKS( SendEventsToChild );

    p_vout->p_sys->f_angle = 0.0;
    p_vout->p_sys->last_date = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Distort video thread output method
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
 * Destroy: destroy Distort video thread output method
 *****************************************************************************
 * Terminate an output method created by DistortCreateOutputMethod
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
 * This function send the currently rendered image to Distort image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic;

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

    switch( p_vout->p_sys->i_mode )
    {
        case DISTORT_MODE_WAVE:
            DistortWave( p_vout, p_pic, p_outpic );
            break;

        case DISTORT_MODE_RIPPLE:
            DistortRipple( p_vout, p_pic, p_outpic );
            break;

        default:
            break;
    }

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}

/*****************************************************************************
 * DistortWave: draw a wave effect on the picture
 *****************************************************************************/
static void DistortWave( vout_thread_t *p_vout, picture_t *p_inpic,
                                                picture_t *p_outpic )
{
    int i_index;
    double f_angle;
    mtime_t new_date = mdate();

    p_vout->p_sys->f_angle += (new_date - p_vout->p_sys->last_date) / 200000.0;
    p_vout->p_sys->last_date = new_date;
    f_angle = p_vout->p_sys->f_angle;

    for( i_index = 0 ; i_index < p_inpic->i_planes ; i_index++ )
    {
        int i_line, i_num_lines, i_offset;
        uint8_t black_pixel;
        uint8_t *p_in, *p_out;

        p_in = p_inpic->p[i_index].p_pixels;
        p_out = p_outpic->p[i_index].p_pixels;

        i_num_lines = p_inpic->p[i_index].i_visible_lines;

        black_pixel = ( i_index == Y_PLANE ) ? 0x00 : 0x80;

        /* Ok, we do 3 times the sin() calculation for each line. So what ? */
        for( i_line = 0 ; i_line < i_num_lines ; i_line++ )
        {
            /* Calculate today's offset, don't go above 1/20th of the screen */
            i_offset = (int)( (double)(p_inpic->p[i_index].i_visible_pitch)
                         * sin( f_angle + 10.0 * (double)i_line
                                               / (double)i_num_lines )
                         / 20.0 );

            if( i_offset )
            {
                if( i_offset < 0 )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in - i_offset,
                             p_inpic->p[i_index].i_visible_pitch + i_offset );
                    p_in += p_inpic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch;
                    memset( p_out + i_offset, black_pixel, -i_offset );
                }
                else
                {
                    p_vout->p_vlc->pf_memcpy( p_out + i_offset, p_in,
                             p_inpic->p[i_index].i_visible_pitch - i_offset );
                    memset( p_out, black_pixel, i_offset );
                    p_in += p_inpic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch;
                }
            }
            else
            {
                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_inpic->p[i_index].i_visible_pitch );
                p_in += p_inpic->p[i_index].i_pitch;
                p_out += p_outpic->p[i_index].i_pitch;
            }

        }
    }
}

/*****************************************************************************
 * DistortRipple: draw a ripple effect at the bottom of the picture
 *****************************************************************************/
static void DistortRipple( vout_thread_t *p_vout, picture_t *p_inpic,
                                                  picture_t *p_outpic )
{
    int i_index;
    double f_angle;
    mtime_t new_date = mdate();

    p_vout->p_sys->f_angle -= (p_vout->p_sys->last_date - new_date) / 100000.0;
    p_vout->p_sys->last_date = new_date;
    f_angle = p_vout->p_sys->f_angle;

    for( i_index = 0 ; i_index < p_inpic->i_planes ; i_index++ )
    {
        int i_line, i_first_line, i_num_lines, i_offset;
        uint8_t black_pixel;
        uint8_t *p_in, *p_out;

        black_pixel = ( i_index == Y_PLANE ) ? 0x00 : 0x80;

        i_num_lines = p_inpic->p[i_index].i_visible_lines;

        i_first_line = i_num_lines * 4 / 5;

        p_in = p_inpic->p[i_index].p_pixels;
        p_out = p_outpic->p[i_index].p_pixels;

        for( i_line = 0 ; i_line < i_first_line ; i_line++ )
        {
            p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                      p_inpic->p[i_index].i_visible_pitch );
            p_in += p_inpic->p[i_index].i_pitch;
            p_out += p_outpic->p[i_index].i_pitch;
        }

        /* Ok, we do 3 times the sin() calculation for each line. So what ? */
        for( i_line = i_first_line ; i_line < i_num_lines ; i_line++ )
        {
            /* Calculate today's offset, don't go above 1/20th of the screen */
            i_offset = (int)( (double)(p_inpic->p[i_index].i_pitch)
                         * sin( f_angle + 2.0 * (double)i_line
                                              / (double)( 1 + i_line
                                                            - i_first_line) )
                         * (double)(i_line - i_first_line)
                         / (double)i_num_lines
                         / 8.0 );

            if( i_offset )
            {
                if( i_offset < 0 )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in - i_offset,
                             p_inpic->p[i_index].i_visible_pitch + i_offset );
                    p_in -= p_inpic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch;
                    memset( p_out + i_offset, black_pixel, -i_offset );
                }
                else
                {
                    p_vout->p_vlc->pf_memcpy( p_out + i_offset, p_in,
                             p_inpic->p[i_index].i_visible_pitch - i_offset );
                    memset( p_out, black_pixel, i_offset );
                    p_in -= p_inpic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch;
                }
            }
            else
            {
                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_inpic->p[i_index].i_visible_pitch );
                p_in -= p_inpic->p[i_index].i_pitch;
                p_out += p_outpic->p[i_index].i_pitch;
            }

        }
    }
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
