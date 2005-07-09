/*****************************************************************************
 * vout_dummy.c: Dummy video output display method for testing purposes
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
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
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/vout.h>

#define DUMMY_WIDTH 16
#define DUMMY_HEIGHT 16
#define DUMMY_MAX_DIRECTBUFFERS 10

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Init       ( vout_thread_t * );
static void End        ( vout_thread_t * );
static int  Manage     ( vout_thread_t * );
static void Render     ( vout_thread_t *, picture_t * );
static void Display    ( vout_thread_t *, picture_t * );
static void SetPalette ( vout_thread_t *, uint16_t *, uint16_t *, uint16_t * );

/*****************************************************************************
 * OpenVideo: activates dummy video thread output method
 *****************************************************************************
 * This function initializes a dummy vout method.
 *****************************************************************************/
int E_(OpenVideo) ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = Render;
    p_vout->pf_display = Display;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize dummy video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index, i_chroma;
    char *psz_chroma;
    picture_t *p_pic;
    vlc_bool_t b_chroma = 0;

    psz_chroma = config_GetPsz( p_vout, "dummy-chroma" );
    if( psz_chroma )
    {
        if( strlen( psz_chroma ) >= 4 )
        {
            i_chroma = VLC_FOURCC( psz_chroma[0], psz_chroma[1],
                                   psz_chroma[2], psz_chroma[3] );
            b_chroma = 1;
        }

        free( psz_chroma );
    }

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    if( b_chroma )
    {
        msg_Dbg( p_vout, "forcing chroma 0x%.8x (%4.4s)",
                         i_chroma, (char*)&i_chroma );
        p_vout->output.i_chroma = i_chroma;
        if ( i_chroma == VLC_FOURCC( 'R', 'G', 'B', '2' ) )
        {
            p_vout->output.pf_setpalette = SetPalette;
        }
        p_vout->output.i_width  = p_vout->render.i_width;
        p_vout->output.i_height = p_vout->render.i_height;
        p_vout->output.i_aspect = p_vout->render.i_aspect;
    }
    else
    {
        /* Use same chroma as input */
        p_vout->output.i_chroma = p_vout->render.i_chroma;
        p_vout->output.i_rmask  = p_vout->render.i_rmask;
        p_vout->output.i_gmask  = p_vout->render.i_gmask;
        p_vout->output.i_bmask  = p_vout->render.i_bmask;
        p_vout->output.i_width  = p_vout->render.i_width;
        p_vout->output.i_height = p_vout->render.i_height;
        p_vout->output.i_aspect = p_vout->render.i_aspect;
    }

    /* Try to initialize DUMMY_MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < DUMMY_MAX_DIRECTBUFFERS )
    {
        p_pic = NULL;

        /* Find an empty picture slot */
        for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
        {
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
            {
                p_pic = p_vout->p_picture + i_index;
                break;
            }
        }

        /* Allocate the picture */
        if( p_pic == NULL )
        {
            break;
        }

        vout_AllocatePicture( VLC_OBJECT(p_vout), p_pic, p_vout->output.i_chroma,
                              p_vout->output.i_width, p_vout->output.i_height,
                              p_vout->output.i_aspect );

        if( p_pic->i_planes == 0 )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return( 0 );
}

/*****************************************************************************
 * End: terminate dummy video thread output method
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
 * Manage: handle dummy events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    return( 0 );
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* No need to do anything, the fake direct buffers stay as they are */
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* No need to do anything, the fake direct buffers stay as they are */
}

/*****************************************************************************
 * SetPalette: set the palette for the picture
 *****************************************************************************/
static void SetPalette ( vout_thread_t *p_vout,
                         uint16_t *red, uint16_t *green, uint16_t *blue )
{
    /* No need to do anything, the fake direct buffers stay as they are */
}
