/*****************************************************************************
 * motion_blur.c : motion blur filter for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "filter_common.h"

/*****************************************************************************
 * Local protypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static void RenderBlur    ( vout_thread_t *, picture_t *, picture_t *, picture_t * );
static void CopyPicture ( vout_thread_t*, picture_t *, picture_t * );

static int  SendEvents( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MODE_TEXT N_("Blur factor (1-127)")
#define MODE_LONGTEXT N_("The degree of blurring from 1 to 127.")

vlc_module_begin();
    set_shortname( _("Motion blur") );
    set_description( _("Motion blur filter") );
    set_capability( "video filter", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    add_integer_with_range( "blur-factor", 80, 1, 127, NULL,
        MODE_TEXT, MODE_LONGTEXT, VLC_FALSE );
    
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: Deinterlace video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Deinterlace specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    int        i_factor;        /* Deinterlace mode */
    vlc_bool_t b_double_rate; /* Shall we double the framerate? */

    mtime_t    last_date;
    mtime_t    next_date;

    vout_thread_t *p_vout;
    picture_t *p_lastpic;
};

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * Create: allocates Deinterlace video thread output method
 *****************************************************************************
 * This function allocates and initializes a Deinterlace vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

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

    p_vout->p_sys->i_factor = config_GetInt( p_vout, "blur-factor" );
    p_vout->p_sys->b_double_rate = 0;
    p_vout->p_sys->last_date = 0;
    p_vout->p_sys->p_lastpic = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize Deinterlace video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;
    video_format_t fmt = {0};

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure, full of directbuffers since we want
     * the decoder to output directly to our structures. */
    switch( p_vout->render.i_chroma )
    {
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('I','4','2','2'):
            p_vout->output.i_chroma = p_vout->render.i_chroma;
            p_vout->output.i_width  = p_vout->render.i_width;
            p_vout->output.i_height = p_vout->render.i_height;
            p_vout->output.i_aspect = p_vout->render.i_aspect;
            break;

        default:
            return VLC_EGENERIC; /* unknown chroma */
            break;
    }

    msg_Dbg( p_vout, "spawning the real video output" );

    fmt.i_width = fmt.i_visible_width = p_vout->output.i_width;
    fmt.i_height = fmt.i_visible_height = p_vout->output.i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_chroma = p_vout->output.i_chroma;
    fmt.i_aspect = p_vout->output.i_aspect;
    fmt.i_sar_num = p_vout->output.i_aspect * fmt.i_height / fmt.i_width;
    fmt.i_sar_den = VOUT_ASPECT_FACTOR;

    switch( p_vout->render.i_chroma )
    {
    case VLC_FOURCC('I','4','2','0'):
    case VLC_FOURCC('I','Y','U','V'):
    case VLC_FOURCC('Y','V','1','2'):
        p_vout->p_sys->p_vout = vout_Create( p_vout, &fmt );
        break;
    default:
        break;
    }

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "cannot open vout, aborting" );

        return VLC_EGENERIC;
    }

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    ADD_PARENT_CALLBACKS( SendEventsToChild );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Deinterlace video thread output method
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
 * Destroy: destroy Deinterlace video thread output method
 *****************************************************************************
 * Terminate an output method created by DeinterlaceCreateOutputMethod
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
 * This function send the currently rendered image to Deinterlace image,
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static void Render ( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t * p_outpic;
    while( ( p_outpic = vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
           == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }
    vout_DatePicture( p_vout, p_outpic, p_pic->date );

    if ( p_vout->p_sys->p_lastpic == NULL )
    {
        /* Get a new picture */
        while( ( p_vout->p_sys->p_lastpic =
                 vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
               == NULL )
        {
            if( p_vout->b_die || p_vout->b_error )
            {
                return;
            }
            msleep( VOUT_OUTMEM_SLEEP );
        }
        CopyPicture( p_vout, p_vout->p_sys->p_lastpic, p_pic );
        CopyPicture( p_vout, p_outpic, p_pic );
        vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
        return;
    }

    /* Get a new picture */
    RenderBlur( p_vout, p_vout->p_sys->p_lastpic, p_pic, p_outpic );
    vout_DestroyPicture( p_vout, p_vout->p_sys->p_lastpic );


    /* Get a new picture */
    while( ( p_vout->p_sys->p_lastpic =
             vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
           == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }
    CopyPicture( p_vout, p_vout->p_sys->p_lastpic, p_outpic );
    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}

/* FIXME: this is a verbatim copy from src/video_output/vout_pictures.c */
/* XXX: the order is fucked up!! */
static void CopyPicture( vout_thread_t * p_vout,
                         picture_t *p_dest, picture_t *p_src )
{
    int i;

    for( i = 0; i < p_src->i_planes ; i++ )
    {
        if( p_src->p[i].i_pitch == p_dest->p[i].i_pitch )
        {
            /* There are margins, but with the same width : perfect ! */
            p_vout->p_vlc->pf_memcpy(
                         p_dest->p[i].p_pixels, p_src->p[i].p_pixels,
                         p_src->p[i].i_pitch * p_src->p[i].i_visible_lines );
        }
        else
        {
            /* We need to proceed line by line */
            uint8_t *p_in = p_src->p[i].p_pixels;
            uint8_t *p_out = p_dest->p[i].p_pixels;
            int i_line;

            for( i_line = p_src->p[i].i_visible_lines; i_line--; )
            {
                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_src->p[i].i_visible_pitch );
                p_in += p_src->p[i].i_pitch;
                p_out += p_dest->p[i].i_pitch;
            }
        }
    }
}

/*****************************************************************************
 * RenderBlur: renders a blurred picture
 *****************************************************************************/
static void RenderBlur( vout_thread_t *p_vout, picture_t *p_oldpic,
                        picture_t *p_newpic, picture_t *p_outpic )
{
    int i_plane;
    int i_oldfactor = p_vout->p_sys->i_factor;
    int i_newfactor = 128 - p_vout->p_sys->i_factor;
    for( i_plane = 0; i_plane < p_outpic->i_planes; i_plane++ )
    {
        uint8_t *p_old, *p_new, *p_out, *p_out_end, *p_out_line_end;
        p_out = p_outpic->p[i_plane].p_pixels;
        p_new = p_newpic->p[i_plane].p_pixels;
        p_old = p_oldpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch *
                             p_outpic->p[i_plane].i_visible_lines;
        while ( p_out < p_out_end )
        {
            p_out_line_end = p_out + p_outpic->p[i_plane].i_visible_pitch;

            while ( p_out < p_out_line_end )
            {
                *p_out++ = (((*p_old++) * i_oldfactor) +
                            ((*p_new++) * i_newfactor)) >> 7;

//                *p_out++ = (*p_old++ >> 1) + (*p_new++ >> 1);
            }

            p_old += p_oldpic->p[i_plane].i_pitch
                      - p_oldpic->p[i_plane].i_visible_pitch;
            p_new += p_newpic->p[i_plane].i_pitch
                      - p_newpic->p[i_plane].i_visible_pitch;
            p_out += p_outpic->p[i_plane].i_pitch
                      - p_outpic->p[i_plane].i_visible_pitch;
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
