/*****************************************************************************
 * clone.c : Clone video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: clone.c,v 1.9 2002/07/31 20:56:51 sam Exp $
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

static void RemoveAllVout  ( vout_thread_t *p_vout );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define COUNT_TEXT N_("Number of clones")
#define COUNT_LONGTEXT N_("Select the number of video windows in which to "\
    "clone the video")

vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_integer( "clone-count", 2, NULL, COUNT_TEXT, COUNT_LONGTEXT );
    set_description( _("image clone video module") );
    set_capability( "video filter", 0 );
    add_shortcut( "clone" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: Clone video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Clone specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    int    i_clones;
    vout_thread_t **pp_vout;
};

/*****************************************************************************
 * Create: allocates Clone video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
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

    /* Look what method was requested */
    p_vout->p_sys->i_clones = config_GetInt( p_vout, "clone-count" );

    p_vout->p_sys->i_clones = __MAX( 1, __MIN( 99, p_vout->p_sys->i_clones ) );

    msg_Dbg( p_vout, "spawning %i clone(s)", p_vout->p_sys->i_clones );

    p_vout->p_sys->pp_vout = malloc( p_vout->p_sys->i_clones *
                                     sizeof(vout_thread_t *) );
    if( p_vout->p_sys->pp_vout == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * Init: initialize Clone video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int   i_index, i_vout;
    picture_t *p_pic;
    
    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video outputs" );

    for( i_vout = 0; i_vout < p_vout->p_sys->i_clones; i_vout++ )
    {
        p_vout->p_sys->pp_vout[ i_vout ] =
                vout_CreateThread( p_vout,
                            p_vout->render.i_width, p_vout->render.i_height,
                            p_vout->render.i_chroma, p_vout->render.i_aspect );
        if( p_vout->p_sys->pp_vout[ i_vout ] == NULL )
        {
            msg_Err( p_vout, "failed to clone %i vout threads",
                             p_vout->p_sys->i_clones );
            p_vout->p_sys->i_clones = i_vout;
            RemoveAllVout( p_vout );
            return 0;
        }
    }

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    return( 0 );
}

/*****************************************************************************
 * End: terminate Clone video thread output method
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
 * Destroy: destroy Clone video thread output method
 *****************************************************************************
 * Terminate an output method created by CloneCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    RemoveAllVout( p_vout );

    free( p_vout->p_sys->pp_vout );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Clone image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    int i_vout, i_plane;

    for( i_vout = 0; i_vout < p_vout->p_sys->i_clones; i_vout++ )
    {
        while( ( p_outpic =
            vout_CreatePicture( p_vout->p_sys->pp_vout[ i_vout ], 0, 0, 0 )
               ) == NULL )
        {
            if( p_vout->b_die || p_vout->b_error )
            {
                vout_DestroyPicture(
                    p_vout->p_sys->pp_vout[ i_vout ], p_outpic );
                return;
            }

            msleep( VOUT_OUTMEM_SLEEP );
        }

        vout_DatePicture( p_vout->p_sys->pp_vout[ i_vout ],
                          p_outpic, p_pic->date );
        vout_LinkPicture( p_vout->p_sys->pp_vout[ i_vout ], p_outpic );

        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        {
            u8 *p_in, *p_in_end, *p_out;
            int i_in_pitch = p_pic->p[i_plane].i_pitch;
            const int i_out_pitch = p_outpic->p[i_plane].i_pitch;

            p_in = p_pic->p[i_plane].p_pixels;

            p_in_end = p_in + p_outpic->p[i_plane].i_lines
                               * p_pic->p[i_plane].i_pitch;

            p_out = p_outpic->p[i_plane].p_pixels;

            while( p_in < p_in_end )
            {
                p_vout->p_vlc->pf_memcpy( p_out, p_in, i_out_pitch );
                p_in += i_in_pitch;
                p_out += i_out_pitch;
            }
        }

        vout_UnlinkPicture( p_vout->p_sys->pp_vout[ i_vout ], p_outpic );
        vout_DisplayPicture( p_vout->p_sys->pp_vout[ i_vout ], p_outpic );
    }
}

/*****************************************************************************
 * RemoveAllVout: destroy all the child video output threads
 *****************************************************************************/
static void RemoveAllVout( vout_thread_t *p_vout )
{
    while( p_vout->p_sys->i_clones )
    {
         --p_vout->p_sys->i_clones;
         vout_DestroyThread( p_vout->p_sys->pp_vout[p_vout->p_sys->i_clones] );
    }
}

