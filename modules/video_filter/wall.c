/*****************************************************************************
 * wall.c : Wall video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: wall.c,v 1.2 2002/08/26 09:12:46 sam Exp $
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
#define COLS_TEXT N_("number of columns")
#define COLS_LONGTEXT N_("Select the number of horizontal videowindows in " \
    "which to split the video")

#define ROWS_TEXT N_("number of rows")
#define ROWS_LONGTEXT N_("Select the number of vertical videowindows in " \
    "which to split the video")

#define ACTIVE_TEXT N_("active windows")
#define ACTIVE_LONGTEXT N_("comma separated list of active windows, " \
    "defaults to all")

vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_integer( "wall-cols", 3, NULL, COLS_TEXT, COLS_LONGTEXT );
    add_integer( "wall-rows", 3, NULL, ROWS_TEXT, ROWS_LONGTEXT );
    add_string( "wall-active", NULL, NULL, ACTIVE_TEXT, ACTIVE_LONGTEXT );
    set_description( _("image wall video module") );
    set_capability( "video filter", 0 );
    add_shortcut( "wall" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: Wall video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Wall specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    int    i_col;
    int    i_row;
    int    i_vout;
    struct vout_list_t
    {
        vlc_bool_t b_active;
        int i_width;
        int i_height;
        vout_thread_t *p_vout;
    } *pp_vout;
};

/*****************************************************************************
 * Create: allocates Wall video thread output method
 *****************************************************************************
 * This function allocates and initializes a Wall vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *psz_method, *psz_tmp, *psz_method_tmp;
    int i_vout;

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
    p_vout->p_sys->i_col = config_GetInt( p_vout, "wall-cols" );
    p_vout->p_sys->i_row = config_GetInt( p_vout, "wall-rows" );

    p_vout->p_sys->i_col = __MAX( 1, __MIN( 15, p_vout->p_sys->i_col ) );
    p_vout->p_sys->i_row = __MAX( 1, __MIN( 15, p_vout->p_sys->i_row ) );

    msg_Dbg( p_vout, "opening a %i x %i wall",
             p_vout->p_sys->i_col, p_vout->p_sys->i_row );

    p_vout->p_sys->pp_vout = malloc( p_vout->p_sys->i_row *
                                     p_vout->p_sys->i_col *
                                     sizeof(struct vout_list_t) );
    if( p_vout->p_sys->pp_vout == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        free( p_vout->p_sys );
        return( 1 );
    }

    psz_method_tmp = psz_method = config_GetPsz( p_vout, "wall-active" );

    /* If no trailing vout are specified, take them all */
    if( psz_method == NULL )
    {
        for( i_vout = p_vout->p_sys->i_row * p_vout->p_sys->i_col;
             i_vout--; )
        {
            p_vout->p_sys->pp_vout[i_vout].b_active = 1;
        }
    }
    /* If trailing vout are specified, activate only the requested ones */
    else
    {
        for( i_vout = p_vout->p_sys->i_row * p_vout->p_sys->i_col;
             i_vout--; )
        {
            p_vout->p_sys->pp_vout[i_vout].b_active = 0;
        }

        while( *psz_method )
        {
            psz_tmp = psz_method;
            while( *psz_tmp && *psz_tmp != ',' )
            {
                psz_tmp++;
            }

            if( *psz_tmp )
            {
                *psz_tmp = '\0';
                i_vout = atoi( psz_method );
                psz_method = psz_tmp + 1;
            }
            else
            {
                i_vout = atoi( psz_method );
                psz_method = psz_tmp;
            }

            if( i_vout >= 0 &&
                i_vout < p_vout->p_sys->i_row * p_vout->p_sys->i_col )
            {
                p_vout->p_sys->pp_vout[i_vout].b_active = 1;
            }
        }
    }

    free( psz_method_tmp );

    return( 0 );
}

/*****************************************************************************
 * Init: initialize Wall video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index, i_row, i_col, i_width, i_height;
    picture_t *p_pic;
    
    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video outputs" );

    p_vout->p_sys->i_vout = 0;

    /* FIXME: use bresenham instead of those ugly divisions */
    for( i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++ )
        {
            if( i_col + 1 < p_vout->p_sys->i_col )
            {
                i_width = ( p_vout->render.i_width
                             / p_vout->p_sys->i_col ) & ~0x1;
            }
            else
            {
                i_width = p_vout->render.i_width
                           - ( ( p_vout->render.i_width
                                  / p_vout->p_sys->i_col ) & ~0x1 ) * i_col;
            }

            if( i_row + 1 < p_vout->p_sys->i_row )
            {
                i_height = ( p_vout->render.i_height
                              / p_vout->p_sys->i_row ) & ~0x3;
            }
            else
            {
                i_height = p_vout->render.i_height
                            - ( ( p_vout->render.i_height
                                   / p_vout->p_sys->i_row ) & ~0x3 ) * i_row;
            }

            p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].i_width = i_width;
            p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].i_height = i_height;

            if( !p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].b_active )
            {
                p_vout->p_sys->i_vout++;
                continue;
            }

            p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].p_vout =
                vout_CreateThread( p_vout, i_width, i_height,
                                   p_vout->render.i_chroma,
                                   p_vout->render.i_aspect
                                    * p_vout->render.i_height / i_height
                                    * i_width / p_vout->render.i_width );
            if( p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].p_vout == NULL )
            {
                msg_Err( p_vout, "failed to get %ix%i vout threads",
                                 p_vout->p_sys->i_col, p_vout->p_sys->i_row );
                RemoveAllVout( p_vout );
                return 0;
            }

            p_vout->p_sys->i_vout++;
        }
    }

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    return( 0 );
}

/*****************************************************************************
 * End: terminate Wall video thread output method
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
 * Destroy: destroy Wall video thread output method
 *****************************************************************************
 * Terminate an output method created by WallCreateOutputMethod
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
 * This function send the currently rendered image to Wall image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    int i_col, i_row, i_vout, i_plane;
    int pi_left_skip[VOUT_MAX_PLANES], pi_top_skip[VOUT_MAX_PLANES];

    i_vout = 0;

    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        pi_top_skip[i_plane] = 0;
    }

    for( i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        {
            pi_left_skip[i_plane] = 0;
        }

        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++ )
        {
            if( !p_vout->p_sys->pp_vout[ i_vout ].b_active )
            {
                for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
                {
                    pi_left_skip[i_plane] +=
                        p_vout->p_sys->pp_vout[ i_vout ].i_width
                         * p_pic->p[i_plane].i_pitch / p_vout->output.i_width;
                }
                i_vout++;
                continue;
            }

            while( ( p_outpic =
                vout_CreatePicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                    0, 0, 0 )
                   ) == NULL )
            {
                if( p_vout->b_die || p_vout->b_error )
                {
                    vout_DestroyPicture(
                        p_vout->p_sys->pp_vout[ i_vout ].p_vout, p_outpic );
                    return;
                }

                msleep( VOUT_OUTMEM_SLEEP );
            }

            vout_DatePicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                              p_outpic, p_pic->date );
            vout_LinkPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                              p_outpic );

            for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
            {
                u8 *p_in, *p_in_end, *p_out;
                int i_in_pitch = p_pic->p[i_plane].i_pitch;
                int i_out_pitch = p_outpic->p[i_plane].i_pitch;

                p_in = p_pic->p[i_plane].p_pixels
                        + pi_top_skip[i_plane] + pi_left_skip[i_plane];

                p_in_end = p_in + p_outpic->p[i_plane].i_lines
                                   * p_pic->p[i_plane].i_pitch;

                p_out = p_outpic->p[i_plane].p_pixels;

                while( p_in < p_in_end )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in, i_out_pitch );
                    p_in += i_in_pitch;
                    p_out += i_out_pitch;
                }

                pi_left_skip[i_plane] += i_out_pitch;
            }

            vout_UnlinkPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                p_outpic );
            vout_DisplayPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                 p_outpic );

            i_vout++;
        }

        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        {
            pi_top_skip[i_plane] += p_vout->p_sys->pp_vout[ i_vout ].i_height
                                     * p_pic->p[i_plane].i_lines
                                     / p_vout->output.i_height
                                     * p_pic->p[i_plane].i_pitch;
        }
    }
}

/*****************************************************************************
 * RemoveAllVout: destroy all the child video output threads
 *****************************************************************************/
static void RemoveAllVout( vout_thread_t *p_vout )
{
    while( p_vout->p_sys->i_vout )
    {
         --p_vout->p_sys->i_vout;
         if( p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].b_active )
         {
             vout_DestroyThread(
               p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].p_vout );
         }
    }
}
