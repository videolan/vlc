/*****************************************************************************
 * adjust.c : Contrast/Hue/Saturation/Brightness video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: adjust.c,v 1.4 2002/12/12 10:56:24 garf Exp $
 *
 * Authors: Simon Latapie <garf@via.ecp.fr>, Samuel Hocevar <sam@zoy.org>
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


#define eight_times( x )    x x x x x x x x

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define CONT_TEXT N_("Contrast")
#define CONT_LONGTEXT N_("Default to 1")
#define HUE_TEXT N_("Hue")
#define HUE_LONGTEXT N_("Between 0 and 360. Default to 0")
#define SAT_TEXT N_("Saturation")
#define SAT_LONGTEXT N_("Default to 1")
#define LUM_TEXT N_("Brightness")
#define LUM_LONGTEXT N_("Default to 1")


vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_float( "Contrast", 1.0, NULL, CONT_TEXT, CONT_LONGTEXT );
    add_float( "Brightness", 1.0, NULL, LUM_TEXT, LUM_LONGTEXT );
    add_integer( "Hue", 0, NULL, HUE_TEXT, HUE_LONGTEXT );
    add_float( "Saturation", 1.0, NULL, SAT_TEXT, SAT_LONGTEXT );
    set_description( _("Contrast/Hue/Saturation/Brightness filter") );
    set_capability( "video filter", 0 );
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

inline static int32_t maxmin( int32_t a )
{
    if ( a > 255 )
        return 255;
    else if ( a < 0 )
        return 0;
    else
        return a;
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
        return( 1 );
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;

    return( 0 );
}

/*****************************************************************************
 * Init: initialize adjust video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
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

    p_vout->p_sys->p_vout = vout_Create( p_vout,
                     p_vout->render.i_width, p_vout->render.i_height,
                     p_vout->render.i_chroma, p_vout->render.i_aspect );

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "can't open vout, aborting" );

        return( 0 );
    }
 
    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    return( 0 );
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

    vout_Destroy( p_vout->p_sys->p_vout );

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
    picture_t *p_outpic;
    int i_index;
    s32 cont;
    s32 lum;

    /* Contrast is a fast but cludged function, so I put this gap to be
cleaner :) */    
    s32 dec;

    double hue;
    int i_sat;
    int i_Sin;
    int i_Cos;
    int p_lum_func[256];
    int i;
    
    /* This is a new frame. Get a structure from the video_output. */

    cont = (s32) ( config_GetFloat( p_vout, "Contrast" ) * 255 );
    lum = (s32) ( ( config_GetFloat( p_vout, "Brightness" ) * 255 ) - 255 );
    hue =  config_GetInt( p_vout, "Hue" ) * 3.14159 / 180 ; /* convert in radian */
    i_sat = (int) (  config_GetFloat( p_vout, "Saturation" ) * 256 );
    
    dec = 128 - ( cont / 2 );
    i_Sin = (int) ( sin(hue) * 256);
    i_Cos = (int) ( cos(hue) * 256);

    
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

    for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
    {

     if ( i_index==0 )
    {

        u8 *p_in, *p_in_end, *p_out;

        p_in = p_pic->p[i_index].p_pixels;
        p_in_end = p_in + p_pic->p[i_index].i_lines
                                * p_pic->p[i_index].i_pitch -8;
        
        p_out = p_outpic->p[i_index].p_pixels;

        for( i = 0 ; i < 256 ; i++ )
        {
            p_lum_func[ i ] = maxmin( (( i * cont ) >> 8 ) + lum + dec );
        }

        
        for( ; p_in < p_in_end ; )
        {
            /* Do 8 pixels at a time */

            eight_times( *p_out = p_lum_func[ *p_in ]; p_out++; p_in++; )

        }

        p_in_end += 8;

        for( ; p_in < p_in_end ; )
        {
            /* Do 1 pixel at a time */
                *p_out = p_lum_func[ *p_in ]; p_out++; p_in++;
        }
    }
    else
    {    
     if ( i_index==1 )
    {

        u8 *p_in_u, *p_in_v, *p_in_end, *p_out_u, *p_out_v, i_u, i_v;
        s32 cospsin, cosmsin;

        p_in_u = p_pic->p[i_index].p_pixels;
        p_in_v = p_pic->p[i_index + 1].p_pixels;
        p_in_end = p_in_u + p_pic->p[i_index].i_lines
                                * p_pic->p[i_index].i_pitch -8;
        
        p_out_u = p_outpic->p[i_index].p_pixels;
        p_out_v = p_outpic->p[i_index + 1].p_pixels;

        cospsin = 32768 * ( cos(hue) + sin(hue) );
        cosmsin = 32768 * ( cos(hue) - sin(hue) );

        if ( i_sat > 256 )
        {
            for( ; p_in_u < p_in_end ; )
            {
                /* Do 8 pixels at a time */

       eight_times( i_u = *p_in_u ;
                    i_v = *p_in_v ;
                    *p_out_u = maxmin( (( ((i_u * i_Cos + i_v * i_Sin - cospsin) >> 8)  * i_sat) >> 8) + 128);
                    *p_out_v = maxmin( (( ((i_v * i_Cos - i_u * i_Sin - cosmsin) >> 8)  * i_sat) >> 8) + 128);
                    p_out_u++; p_in_u++; p_out_v++; p_in_v++; )

            }

            p_in_end += 8;

            for( ; p_in_u < p_in_end ; )
            {
                /* Do 1 pixel at a time */
                    i_u = *p_in_u ;
                    i_v = *p_in_v ;
                    *p_out_u = maxmin( (( ((i_u * i_Cos + i_v * i_Sin - cospsin) >> 8)  * i_sat) >> 8) + 128);
                    *p_out_v = maxmin( (( ((i_v * i_Cos - i_u * i_Sin - cosmsin) >> 8)  * i_sat) >> 8) + 128);
                    p_out_u++; p_in_u++; p_out_v++; p_in_v++;


            }
        }
        else
        {
            for( ; p_in_u < p_in_end ; )
            {
                /* Do 8 pixels at a time */

       eight_times( i_u = *p_in_u ;
                    i_v = *p_in_v ;
                    *p_out_u = (( ((i_u * i_Cos + i_v * i_Sin - cospsin) >> 8)  * i_sat) >> 8) + 128;
                    *p_out_v = (( ((i_v * i_Cos - i_u * i_Sin - cosmsin) >> 8)  * i_sat) >> 8) + 128;
                    p_out_u++; p_in_u++; p_out_v++; p_in_v++; )

            }

            p_in_end += 8;

            for( ; p_in_u < p_in_end ; )
            {
                /* Do 1 pixel at a time */
                    i_u = *p_in_u ;
                    i_v = *p_in_v ;
                    *p_out_u = (( ((i_u * i_Cos + i_v * i_Sin - cospsin) >> 8)  * i_sat) >> 8) + 128;
                    *p_out_v = (( ((i_v * i_Cos - i_u * i_Sin - cosmsin) >> 8)  * i_sat) >> 8) + 128;
                    p_out_u++; p_in_u++; p_out_v++; p_in_v++;

            }
        }
    }

    }
    
    
    }

    vout_UnlinkPicture( p_vout->p_sys->p_vout, p_outpic );

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}
