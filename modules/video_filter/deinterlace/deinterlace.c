/*****************************************************************************
 * deinterlace.c : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 VideoLAN
 * $Id: deinterlace.c,v 1.11 2003/03/30 18:14:38 gbazin Exp $
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

#include "../filter_common.h"

#define DEINTERLACE_DISCARD 1
#define DEINTERLACE_MEAN    2
#define DEINTERLACE_BLEND   3
#define DEINTERLACE_BOB     4
#define DEINTERLACE_LINEAR  5

/*****************************************************************************
 * Local protypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static void RenderDiscard( vout_thread_t *, picture_t *, picture_t *, int );
static void RenderBob    ( vout_thread_t *, picture_t *, picture_t *, int );
static void RenderMean   ( vout_thread_t *, picture_t *, picture_t * );
static void RenderBlend  ( vout_thread_t *, picture_t *, picture_t * );
static void RenderLinear ( vout_thread_t *, picture_t *, picture_t *, int );

static void Merge        ( void *, const void *, const void *, size_t );

static int  SendEvents   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

static void SetFilterMethod( vout_thread_t *p_vout, char *psz_method );
static vout_thread_t *SpawnRealVout( vout_thread_t *p_vout );

/*****************************************************************************
 * Callback prototypes
 *****************************************************************************/
static int FilterCallback ( vlc_object_t *, char const *,
                            vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MODE_TEXT N_("deinterlace mode")
#define MODE_LONGTEXT N_("you can choose the default deinterlace mode")

static char *mode_list[] = { "discard", "blend", "mean", "bob", "linear", NULL };

vlc_module_begin();
    add_category_hint( N_("Deinterlace"), NULL, VLC_FALSE );
    add_string_from_list( "deinterlace-mode", "discard", mode_list, NULL,
                          MODE_TEXT, MODE_LONGTEXT, VLC_FALSE );
    set_description( _("video deinterlacing filter") );
    set_capability( "video filter", 0 );
    add_shortcut( "deinterlace" );
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
    int        i_mode;        /* Deinterlace mode */
    vlc_bool_t b_double_rate; /* Shall we double the framerate? */

    mtime_t    last_date;
    mtime_t    next_date;

    vout_thread_t *p_vout;

    vlc_mutex_t filter_lock;
};

/*****************************************************************************
 * Create: allocates Deinterlace video thread output method
 *****************************************************************************
 * This function allocates and initializes a Deinterlace vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vlc_value_t val;

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

    p_vout->p_sys->i_mode = DEINTERLACE_DISCARD;
    p_vout->p_sys->b_double_rate = 0;
    p_vout->p_sys->last_date = 0;
    vlc_mutex_init( p_vout, &p_vout->p_sys->filter_lock );

    /* Look what method was requested */
    val.psz_string = config_GetPsz( p_vout, "deinterlace-mode" );

    var_Create( p_vout, "deinterlace-mode", VLC_VAR_STRING );

    if( val.psz_string == NULL )
    {
        msg_Err( p_vout, "configuration variable %s empty",
                         "deinterlace-mode" );
        msg_Err( p_vout, "no deinterlace mode provided, using \"discard\"" );

        val.psz_string = strdup( "discard" );
    }

    var_Set( p_vout, "deinterlace-mode", val );

    SetFilterMethod( p_vout, val.psz_string );

    free( val.psz_string );

    var_AddCallback( p_vout, "deinterlace-mode", FilterCallback, NULL );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SetFilterMethod: setup the deinterlace method to use.
 *****************************************************************************/
static void SetFilterMethod( vout_thread_t *p_vout, char *psz_method )
{
    if( !strcmp( psz_method, "discard" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_DISCARD;
        p_vout->p_sys->b_double_rate = 0;
    }
    else if( !strcmp( psz_method, "mean" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_MEAN;
        p_vout->p_sys->b_double_rate = 0;
    }
    else if( !strcmp( psz_method, "blend" )
             || !strcmp( psz_method, "average" )
             || !strcmp( psz_method, "combine-fields" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_BLEND;
        p_vout->p_sys->b_double_rate = 0;
    }
    else if( !strcmp( psz_method, "bob" )
             || !strcmp( psz_method, "progressive-scan" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_BOB;
        p_vout->p_sys->b_double_rate = 1;
    }
    else if( !strcmp( psz_method, "linear" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_LINEAR;
        p_vout->p_sys->b_double_rate = 1;
    }
    else
    {
        msg_Err( p_vout, "no valid deinterlace mode provided, "
                 "using \"discard\"" );
    }

    msg_Dbg( p_vout, "using %s deinterlace method", psz_method );
}

/*****************************************************************************
 * Init: initialize Deinterlace video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

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

    /* Try to open the real video output */
    p_vout->p_sys->p_vout = SpawnRealVout( p_vout );

    if( p_vout->p_sys->p_vout == NULL )
    {
        /* Everything failed */
        msg_Err( p_vout, "cannot open vout, aborting" );

        return VLC_EGENERIC;
    }

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SpawnRealVout: spawn the real video output.
 *****************************************************************************/
static vout_thread_t *SpawnRealVout( vout_thread_t *p_vout )
{
    vout_thread_t *p_real_vout = NULL;

    msg_Dbg( p_vout, "spawning the real video output" );

    switch( p_vout->render.i_chroma )
    {
    case VLC_FOURCC('I','4','2','0'):
    case VLC_FOURCC('I','Y','U','V'):
    case VLC_FOURCC('Y','V','1','2'):
        switch( p_vout->p_sys->i_mode )
        {
        case DEINTERLACE_MEAN:
        case DEINTERLACE_DISCARD:
            p_real_vout =
                vout_Create( p_vout,
                       p_vout->output.i_width, p_vout->output.i_height / 2,
                       p_vout->output.i_chroma, p_vout->output.i_aspect );
            break;

        case DEINTERLACE_BOB:
        case DEINTERLACE_BLEND:
        case DEINTERLACE_LINEAR:
            p_real_vout =
                vout_Create( p_vout,
                       p_vout->output.i_width, p_vout->output.i_height,
                       p_vout->output.i_chroma, p_vout->output.i_aspect );
            break;
        }
        break;

    case VLC_FOURCC('I','4','2','2'):
        p_real_vout =
            vout_Create( p_vout,
                       p_vout->output.i_width, p_vout->output.i_height,
                       VLC_FOURCC('I','4','2','0'), p_vout->output.i_aspect );
        break;

    default:
        break;
    }

    return p_real_vout;
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

    DEL_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    vlc_object_detach( p_vout->p_sys->p_vout );
    vout_Destroy( p_vout->p_sys->p_vout );

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
    picture_t *pp_outpic[2];

    vlc_mutex_lock( &p_vout->p_sys->filter_lock );

    /* Get a new picture */
    while( ( pp_outpic[0] = vout_CreatePicture( p_vout->p_sys->p_vout,
                                             0, 0, 0 ) )
              == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
     }

    vout_DatePicture( p_vout->p_sys->p_vout, pp_outpic[0], p_pic->date );

    /* If we are using double rate, get an additional new picture */
    if( p_vout->p_sys->b_double_rate )
    {
        while( ( pp_outpic[1] = vout_CreatePicture( p_vout->p_sys->p_vout,
                                                 0, 0, 0 ) )
                  == NULL )
        {
            if( p_vout->b_die || p_vout->b_error )
            {
                vout_DestroyPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
                vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
                return;
            }
	    msleep( VOUT_OUTMEM_SLEEP );
        }

        /* 20ms is a bit arbitrary, but it's only for the first image we get */
        if( !p_vout->p_sys->last_date )
        {
            vout_DatePicture( p_vout->p_sys->p_vout, pp_outpic[1],
                              p_pic->date + 20000 );
        }
        else
        {
            vout_DatePicture( p_vout->p_sys->p_vout, pp_outpic[1],
                      (3 * p_pic->date - p_vout->p_sys->last_date) / 2 );
        }
        p_vout->p_sys->last_date = p_pic->date;
    }

    switch( p_vout->p_sys->i_mode )
    {
        case DEINTERLACE_DISCARD:
            RenderDiscard( p_vout, pp_outpic[0], p_pic, 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            break;

        case DEINTERLACE_BOB:
            RenderBob( p_vout, pp_outpic[0], p_pic, 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            RenderBob( p_vout, pp_outpic[1], p_pic, 1 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[1] );
            break;

        case DEINTERLACE_LINEAR:
            RenderLinear( p_vout, pp_outpic[0], p_pic, 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            RenderLinear( p_vout, pp_outpic[1], p_pic, 1 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[1] );
            break;

        case DEINTERLACE_MEAN:
            RenderMean( p_vout, pp_outpic[0], p_pic );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            break;

        case DEINTERLACE_BLEND:
            RenderBlend( p_vout, pp_outpic[0], p_pic );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            break;
    }

    vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
}

/*****************************************************************************
 * RenderDiscard: only keep TOP or BOTTOM field, discard the other.
 *****************************************************************************/
static void RenderDiscard( vout_thread_t *p_vout,
                           picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;
        int i_increment;

        p_in = p_pic->p[i_plane].p_pixels
                   + i_field * p_pic->p[i_plane].i_pitch;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_lines;

        switch( p_vout->render.i_chroma )
        {
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('Y','V','1','2'):

            for( ; p_out < p_out_end ; )
            {
                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_pic->p[i_plane].i_pitch );

                p_out += p_pic->p[i_plane].i_pitch;
                p_in += 2 * p_pic->p[i_plane].i_pitch;
            }
            break;

        case VLC_FOURCC('I','4','2','2'):

            i_increment = 2 * p_pic->p[i_plane].i_pitch;

            if( i_plane == Y_PLANE )
            {
                for( ; p_out < p_out_end ; )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );
                    p_out += p_pic->p[i_plane].i_pitch;
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );
                    p_out += p_pic->p[i_plane].i_pitch;
                    p_in += i_increment;
                }
            }
            else
            {
                for( ; p_out < p_out_end ; )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );
                    p_out += p_pic->p[i_plane].i_pitch;
                    p_in += i_increment;
                }
            }
            break;

        default:
            break;
        }
    }
}

/*****************************************************************************
 * RenderBob: renders a BOB picture - simple copy
 *****************************************************************************/
static void RenderBob( vout_thread_t *p_vout,
                       picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;
        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_lines;

        /* For BOTTOM field we need to add the first line */
        if( i_field == 1 )
        {
            p_vout->p_vlc->pf_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_pic->p[i_plane].i_pitch;
        }

        p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

        for( ; p_out < p_out_end ; )
        {
            p_vout->p_vlc->pf_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

            p_out += p_pic->p[i_plane].i_pitch;

            p_vout->p_vlc->pf_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

            p_in += 2 * p_pic->p[i_plane].i_pitch;
            p_out += p_pic->p[i_plane].i_pitch;
        }

        p_vout->p_vlc->pf_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

        /* For TOP field we need to add the last line */
        if( i_field == 0 )
        {
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_pic->p[i_plane].i_pitch;
            p_vout->p_vlc->pf_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
        }
    }
}

/*****************************************************************************
 * RenderLinear: BOB with linear interpolation
 *****************************************************************************/
static void RenderLinear( vout_thread_t *p_vout,
                          picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;
        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_lines;

        /* For BOTTOM field we need to add the first line */
        if( i_field == 1 )
        {
            p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                      p_pic->p[i_plane].i_pitch );
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_pic->p[i_plane].i_pitch;
        }

        p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

        for( ; p_out < p_out_end ; )
        {
            p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                      p_pic->p[i_plane].i_pitch );

            p_out += p_pic->p[i_plane].i_pitch;

            Merge( p_out, p_in, p_in + 2 * p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_in += 2 * p_pic->p[i_plane].i_pitch;
            p_out += p_pic->p[i_plane].i_pitch;
        }

        p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                  p_pic->p[i_plane].i_pitch );

        /* For TOP field we need to add the last line */
        if( i_field == 0 )
        {
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_pic->p[i_plane].i_pitch;
            p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                      p_pic->p[i_plane].i_pitch );
        }
    }
}

static void RenderMean( vout_thread_t *p_vout,
                        picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_lines;

        /* All lines: mean value */
        for( ; p_out < p_out_end ; )
        {
            Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_out += p_pic->p[i_plane].i_pitch;
            p_in += 2 * p_pic->p[i_plane].i_pitch;
        }
    }
}

static void RenderBlend( vout_thread_t *p_vout,
                         picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_lines;

        /* First line: simple copy */
        p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                  p_pic->p[i_plane].i_pitch );
        p_out += p_pic->p[i_plane].i_pitch;

        /* Remaining lines: mean value */
        for( ; p_out < p_out_end ; )
        {
            Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_out += p_pic->p[i_plane].i_pitch;
            p_in += p_pic->p[i_plane].i_pitch;
        }
    }
}

static void Merge( void *_p_dest, const void *_p_s1,
                   const void *_p_s2, size_t i_bytes )
{
    uint8_t* p_dest = (uint8_t*)_p_dest;
    const uint8_t *p_s1 = (const uint8_t *)_p_s1;
    const uint8_t *p_s2 = (const uint8_t *)_p_s2;
    uint8_t* p_end = p_dest + i_bytes - 8;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }

    p_end += 8;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *_p_vout )
{
    vout_thread_t *p_vout = (vout_thread_t *)_p_vout;
    vlc_value_t sentval = newval;

    if( !strcmp( psz_var, "mouse-y" ) )
    {
        switch( p_vout->p_sys->i_mode )
        {
            case DEINTERLACE_MEAN:
            case DEINTERLACE_DISCARD:
                sentval.i_int *= 2;
                break;
        }
    }

    var_Set( p_vout, psz_var, sentval );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * FilterCallback: called when changing the deinterlace method on the fly.
 *****************************************************************************/
static int FilterCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    int i_old_mode = p_vout->p_sys->i_mode;

    vlc_mutex_lock( &p_vout->p_sys->filter_lock );

    SetFilterMethod( p_vout, newval.psz_string );

    switch( p_vout->render.i_chroma )
    {
    case VLC_FOURCC('I','4','2','2'):
        vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
        return VLC_SUCCESS;
        break;

    case VLC_FOURCC('I','4','2','0'):
    case VLC_FOURCC('I','Y','U','V'):
    case VLC_FOURCC('Y','V','1','2'):
        switch( p_vout->p_sys->i_mode )
        {
        case DEINTERLACE_MEAN:
        case DEINTERLACE_DISCARD:
            if( ( i_old_mode == DEINTERLACE_MEAN )
                || ( i_old_mode == DEINTERLACE_DISCARD ) )
            {
                vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
                return VLC_SUCCESS;
            }
            break;

        case DEINTERLACE_BOB:
        case DEINTERLACE_BLEND:
        case DEINTERLACE_LINEAR:
            if( ( i_old_mode == DEINTERLACE_BOB )
                || ( i_old_mode == DEINTERLACE_BLEND )
                || ( i_old_mode == DEINTERLACE_LINEAR ) )
            {
                vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
                return VLC_SUCCESS;
            }
            break;
        }
        break;

    default:
        break;
    }

    /* We need to kill the old vout */

    DEL_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    vlc_object_detach( p_vout->p_sys->p_vout );
    vout_Destroy( p_vout->p_sys->p_vout );

    /* Try to open a new video output */
    p_vout->p_sys->p_vout = SpawnRealVout( p_vout );

    if( p_vout->p_sys->p_vout == NULL )
    {
        /* Everything failed */
        msg_Err( p_vout, "cannot open vout, aborting" );

        vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
        return VLC_EGENERIC;
    }

    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
    return VLC_SUCCESS;
}
