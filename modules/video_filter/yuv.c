/*****************************************************************************
 * yuv.c : YUV plans modifier video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: yuv.c,v 1.1 2002/10/26 01:08:13 garf Exp $
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

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "filter_common.h"

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

#define Y_TEXT N_("Y plan modifier")
#define Y_LONGTEXT N_("between 0 and 255, default to 255")
#define Y_INV_TEXT N_("invert Y plan")
#define Y_INV_LONGTEXT N_("same function as invert filter")

#define U_TEXT N_("U plan modifier")
#define U_LONGTEXT N_("between 0 and 255, default to 255")
#define U_INV_TEXT N_("invert U plan")
#define U_INV_LONGTEXT N_("same function as invert filter")

#define V_TEXT N_("V plan modifier")
#define V_LONGTEXT N_("between 0 and 255, default to 255")
#define V_INV_TEXT N_("invert V plan")
#define V_INV_LONGTEXT N_("same function as invert filter")

vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_integer( "Y plan", 255, NULL, Y_TEXT, Y_LONGTEXT );
    add_bool( "invert Y", 0, NULL, Y_INV_TEXT, Y_INV_LONGTEXT );
    add_integer( "U plan", 255, NULL, U_TEXT, U_LONGTEXT );
    add_bool( "invert U", 0, NULL, U_INV_TEXT, U_INV_LONGTEXT );
    add_integer( "V plan", 255, NULL, V_TEXT, V_LONGTEXT );
    add_bool( "invert V", 0, NULL, V_INV_TEXT, V_INV_LONGTEXT );
    set_description( _("yuv plan modifier filter") );
    set_capability( "video filter", 0 );
    add_shortcut( "yuv" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: YUV video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the YUV specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
        int    i_coeff[3];
        vlc_bool_t  b_invert[3];
        vout_thread_t *p_vout;
};

/*****************************************************************************
 * Create: allocates YUV video thread output method
 *****************************************************************************
 * This function allocates and initializes a YUV vout method.
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
 * Init: initialize YUV video thread output method
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

    p_vout->p_sys->p_vout =
        vout_CreateThread( p_vout,
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
 * End: terminate YUV video thread output method
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
 * Destroy: destroy YUV video thread output method
 *****************************************************************************
 * Terminate an output method created by YUVCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{   
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    vout_DestroyThread( p_vout->p_sys->p_vout );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to YUV modified image, 
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic;
    int i_index;

    /* This is a new frame. Get a structure from the video_output. */

    p_vout->p_sys->i_coeff[0] = config_GetInt( p_vout, "Y plan" ) ;
    p_vout->p_sys->i_coeff[1] = config_GetInt( p_vout, "U plan" ) ;
    p_vout->p_sys->i_coeff[2] = config_GetInt( p_vout, "V plan" ) ;

    p_vout->p_sys->b_invert[0] = config_GetInt( p_vout, "invert Y" ) ;
    p_vout->p_sys->b_invert[1] = config_GetInt( p_vout, "invert U" ) ;
    p_vout->p_sys->b_invert[2] = config_GetInt( p_vout, "invert V" ) ;
    
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
        u8 *p_in, *p_in_end, *p_out;
        s32 i_coeff;
        vlc_bool_t b_inv;

        i_coeff =  p_vout->p_sys->i_coeff[i_index];
        b_inv =  p_vout->p_sys->b_invert[i_index];
        p_in = p_pic->p[i_index].p_pixels;
        p_in_end = p_in + p_pic->p[i_index].i_lines
                                * p_pic->p[i_index].i_pitch -8;
        
        p_out = p_outpic->p[i_index].p_pixels;

        for( ; p_in < p_in_end ; )
        {
            /* Do 8 pixels at a time */
                *p_out = ( *p_in * i_coeff * (1 - 2 * b_inv)) >> 8; p_out++; p_in++;
                *p_out = ( *p_in * i_coeff * (1 - 2 * b_inv)) >> 8; p_out++; p_in++;
                *p_out = ( *p_in * i_coeff * (1 - 2 * b_inv)) >> 8; p_out++; p_in++;
                *p_out = ( *p_in * i_coeff * (1 - 2 * b_inv)) >> 8; p_out++; p_in++;
                *p_out = ( *p_in * i_coeff * (1 - 2 * b_inv)) >> 8; p_out++; p_in++;
                *p_out = ( *p_in * i_coeff * (1 - 2 * b_inv)) >> 8; p_out++; p_in++;
                *p_out = ( *p_in * i_coeff * (1 - 2 * b_inv)) >> 8; p_out++; p_in++;
                *p_out = ( *p_in * i_coeff * (1 - 2 * b_inv)) >> 8; p_out++; p_in++;
        }

        p_in_end += 8;

        for( ; p_in < p_in_end ; )
        {
            /* Do 1 pixel at a time */
                *p_out = ( *p_in * i_coeff * (1 - 2 * b_inv)) >> 8; p_out++; p_in++;
        }
    }

    vout_UnlinkPicture( p_vout->p_sys->p_vout, p_outpic );

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}

