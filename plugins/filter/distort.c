/*****************************************************************************
 * distort.c : Misc video effects plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: distort.c,v 1.17 2002/07/20 18:01:42 sam Exp $
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
#include <errno.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <math.h>                                            /* sin(), cos() */

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "filter_common.h"

#define DISTORT_MODE_WAVE    1
#define DISTORT_MODE_RIPPLE  2

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
#define MODE_TEXT N_("Distort mode")
#define MODE_LONGTEXT N_("one of \"wave\" and \"ripple\"")

static char *mode_list[] = { "wave", "ripple", NULL };

MODULE_CONFIG_START
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
ADD_STRING_FROM_LIST( "distort-mode", "wave", mode_list, NULL, MODE_TEXT, MODE_LONGTEXT )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("miscellaneous video effects module") )
    /* Capability score set to 0 because we don't want to be spawned
     * as a video output unless explicitly requested to */
    ADD_CAPABILITY( VOUT_FILTER, 0 )
    ADD_SHORTCUT( "distort" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

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
 * Local prototypes
 *****************************************************************************/
static int  vout_Create    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static int  vout_Manage    ( vout_thread_t * );
static void vout_Render    ( vout_thread_t *, picture_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );

static void DistortWave    ( vout_thread_t *, picture_t *, picture_t * );
static void DistortRipple  ( vout_thread_t *, picture_t *, picture_t * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_render     = vout_Render;
    p_function_list->functions.vout.pf_display    = vout_Display;
}

/*****************************************************************************
 * vout_Create: allocates Distort video thread output method
 *****************************************************************************
 * This function allocates and initializes a Distort vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    char *psz_method, *psz_method_tmp;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }
    p_vout->p_sys->i_mode = 0;
    /* Look what method was requested from command line*/
    if( !(psz_method = psz_method_tmp = config_GetPsz( p_vout, "filter" )) )
    {
        msg_Err( p_vout, "configuration variable %s empty", "filter" );
        return( 1 );
    }
    while( *psz_method && *psz_method != ':' )
    {
        psz_method++;
    }

    if( !strcmp( psz_method, ":wave" ) )
    {
        p_vout->p_sys->i_mode = DISTORT_MODE_WAVE;
    }
    else if( !strcmp( psz_method, ":ripple" ) )
    {
        p_vout->p_sys->i_mode = DISTORT_MODE_RIPPLE;
    }
    free( psz_method_tmp );
    if( !p_vout->p_sys->i_mode )
    {
        /* No method given in commandline. Look what method was
         requested in configuration system */
        if( !(psz_method = psz_method_tmp
              = config_GetPsz( p_vout, "distort_mode" )) )
        {
            msg_Err( p_vout, "configuration variable %s empty, using 'wave'",
                             "distort_mode" );
            p_vout->p_sys->i_mode = DISTORT_MODE_WAVE;
        }
        else {
        
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
    }
    free( psz_method_tmp );
    
    return( 0 );
}
    
/*****************************************************************************
 * vout_Init: initialize Distort video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video output" );

    p_vout->p_sys->p_vout =
        vout_CreateThread( p_vout,
                           p_vout->render.i_width, p_vout->render.i_height,
                           p_vout->render.i_chroma, p_vout->render.i_aspect );

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "cannot open vout, aborting" );

        return( 0 );
    }
 
    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    p_vout->p_sys->f_angle = 0.0;
    p_vout->p_sys->last_date = 0;

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate Distort video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
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
 * vout_Destroy: destroy Distort video thread output method
 *****************************************************************************
 * Terminate an output method created by DistortCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    vout_DestroyThread( p_vout->p_sys->p_vout );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle Distort events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    return( 0 );
}

/*****************************************************************************
 * vout_Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Distort image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
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
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Invert image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
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
        u8 black_pixel;
        u8 *p_in, *p_out;

        p_in = p_inpic->p[i_index].p_pixels;
        p_out = p_outpic->p[i_index].p_pixels;

        i_num_lines = p_inpic->p[i_index].i_lines;

        black_pixel = ( i_index == Y_PLANE ) ? 0x00 : 0x80;

        /* Ok, we do 3 times the sin() calculation for each line. So what ? */
        for( i_line = 0 ; i_line < i_num_lines ; i_line++ )
        {
            /* Calculate today's offset, don't go above 1/20th of the screen */
            i_offset = (double)(p_inpic->p[i_index].i_pitch)
                         * sin( f_angle + 10.0 * (double)i_line
                                               / (double)i_num_lines )
                         / 20.0;

            if( i_offset )
            {
                if( i_offset < 0 )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in - i_offset,
                                 p_inpic->p[i_index].i_pitch + i_offset );
                    p_in += p_inpic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch;
                    memset( p_out + i_offset, black_pixel, -i_offset );
                }
                else
                {
                    p_vout->p_vlc->pf_memcpy( p_out + i_offset, p_in,
                                 p_inpic->p[i_index].i_pitch - i_offset );
                    memset( p_out, black_pixel, i_offset );
                    p_in += p_inpic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch;
                }
            }
            else
            {
                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_inpic->p[i_index].i_pitch );
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
        u8 black_pixel;
        u8 *p_in, *p_out;

        black_pixel = ( i_index == Y_PLANE ) ? 0x00 : 0x80;

        i_num_lines = p_inpic->p[i_index].i_lines;

        i_first_line = i_num_lines * 4 / 5;

        p_in = p_inpic->p[i_index].p_pixels;
        p_out = p_outpic->p[i_index].p_pixels;

        p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                  i_first_line * p_inpic->p[i_index].i_pitch );

        p_in += i_first_line * p_inpic->p[i_index].i_pitch;
        p_out += i_first_line * p_outpic->p[i_index].i_pitch;

        /* Ok, we do 3 times the sin() calculation for each line. So what ? */
        for( i_line = i_first_line ; i_line < i_num_lines ; i_line++ )
        {
            /* Calculate today's offset, don't go above 1/20th of the screen */
            i_offset = (double)(p_inpic->p[i_index].i_pitch)
                         * sin( f_angle + 2.0 * (double)i_line
                                              / (double)( 1 + i_line
                                                            - i_first_line) )
                         * (double)(i_line - i_first_line)
                         / (double)i_num_lines
                         / 8.0;

            if( i_offset )
            {
                if( i_offset < 0 )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in - i_offset,
                                 p_inpic->p[i_index].i_pitch + i_offset );
                    p_in -= p_inpic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch;
                    memset( p_out + i_offset, black_pixel, -i_offset );
                }
                else
                {
                    p_vout->p_vlc->pf_memcpy( p_out + i_offset, p_in,
                                 p_inpic->p[i_index].i_pitch - i_offset );
                    memset( p_out, black_pixel, i_offset );
                    p_in -= p_inpic->p[i_index].i_pitch;
                    p_out += p_outpic->p[i_index].i_pitch;
                }
            }
            else
            {
                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_inpic->p[i_index].i_pitch );
                p_in -= p_inpic->p[i_index].i_pitch;
                p_out += p_outpic->p[i_index].i_pitch;
            }

        }
    }
}
