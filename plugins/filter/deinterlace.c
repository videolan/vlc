/*****************************************************************************
 * deinterlace.c : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: deinterlace.c,v 1.17 2002/06/11 09:44:21 gbazin Exp $
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

#define DEINTERLACE_DISCARD 1
#define DEINTERLACE_MEAN    2
#define DEINTERLACE_BLEND   3
#define DEINTERLACE_BOB     4
#define DEINTERLACE_LINEAR  5

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list );

static void RenderBob    ( vout_thread_t *, picture_t *, picture_t *, int );
static void RenderMean   ( vout_thread_t *, picture_t *, picture_t * );
static void RenderBlend  ( vout_thread_t *, picture_t *, picture_t * );
static void RenderLinear ( vout_thread_t *, picture_t *, picture_t *, int );

static void Merge        ( void *, const void *, const void *, size_t );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
#define MODE_TEXT N_("Deinterlace mode")
#define MODE_LONGTEXT N_("one of \"discard\", \"blend\", \"mean\", \"bob\" or \"linear\"")

static char *mode_list[] = { "discard", "blend", "mean", "bob", "linear", NULL };

MODULE_CONFIG_START
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
ADD_STRING_FROM_LIST ( "deinterlace-mode", "discard", mode_list, NULL, \
    MODE_TEXT, MODE_LONGTEXT )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("deinterlacing module") )
    /* Capability score set to 0 because we don't want to be spawned
     * as a video output unless explicitly requested to */
    ADD_CAPABILITY( VOUT_FILTER, 0 )
    ADD_SHORTCUT( "deinterlace" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * vout_sys_t: Deinterlace video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Deinterlace specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_s
{
    int        i_mode;        /* Deinterlace mode */
    vlc_bool_t b_double_rate; /* Shall we double the framerate? */

    mtime_t    last_date;
    mtime_t    next_date;

    vout_thread_t *p_vout;
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
 * vout_Create: allocates Deinterlace video thread output method
 *****************************************************************************
 * This function allocates and initializes a Deinterlace vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    char *psz_method;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return 1;
    }

    p_vout->p_sys->i_mode = DEINTERLACE_DISCARD;
    p_vout->p_sys->b_double_rate = 0;
    p_vout->p_sys->last_date = 0;

    /* Look what method was requested */
    psz_method = config_GetPsz( p_vout, "deinterlace-mode" );

    if( psz_method == NULL )
    {
        msg_Err( p_vout, "configuration variable %s empty",
                         "deinterlace-mode" );
        msg_Err( p_vout, "no deinterlace mode provided, using \"discard\"" );
    }
    else
    {
        if( !strcmp( psz_method, "discard" ) )
        {
            p_vout->p_sys->i_mode = DEINTERLACE_DISCARD;
        }
        else if( !strcmp( psz_method, "mean" ) )
        {
            p_vout->p_sys->i_mode = DEINTERLACE_MEAN;
        }
        else if( !strcmp( psz_method, "blend" )
                  || !strcmp( psz_method, "average" )
                  || !strcmp( psz_method, "combine-fields" ) )
        {
            p_vout->p_sys->i_mode = DEINTERLACE_BLEND;
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

        free( psz_method );
    }

    return 0;
}

/*****************************************************************************
 * vout_Init: initialize Deinterlace video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;
    
    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure, full of directbuffers since we want
     * the decoder to output directly to our structures. */
    switch( p_vout->render.i_chroma )
    {
        case FOURCC_I420:
        case FOURCC_IYUV:
        case FOURCC_YV12:
        case FOURCC_I422:
            p_vout->output.i_chroma = p_vout->render.i_chroma;
            p_vout->output.i_width  = p_vout->render.i_width;
            p_vout->output.i_height = p_vout->render.i_height;
            p_vout->output.i_aspect = p_vout->render.i_aspect;
            break;

        default:
            return 0; /* unknown chroma */
            break;
    }

    /* Try to open the real video output, with half the height our images */
    msg_Dbg( p_vout, "spawning the real video output" );

    switch( p_vout->render.i_chroma )
    {
    case FOURCC_I420:
    case FOURCC_IYUV:
    case FOURCC_YV12:
        switch( p_vout->p_sys->i_mode )
        {
        case DEINTERLACE_BOB:
        case DEINTERLACE_MEAN:
        case DEINTERLACE_DISCARD:
            p_vout->p_sys->p_vout =
                vout_CreateThread( p_vout,
                       p_vout->output.i_width, p_vout->output.i_height / 2,
                       p_vout->output.i_chroma, p_vout->output.i_aspect );
            break;

        case DEINTERLACE_BLEND:
        case DEINTERLACE_LINEAR:
            p_vout->p_sys->p_vout =
                vout_CreateThread( p_vout,
                       p_vout->output.i_width, p_vout->output.i_height,
                       p_vout->output.i_chroma, p_vout->output.i_aspect );
            break;
        }
        break;

    case FOURCC_I422:
        p_vout->p_sys->p_vout =
            vout_CreateThread( p_vout,
                       p_vout->output.i_width, p_vout->output.i_height,
                       FOURCC_I420, p_vout->output.i_aspect );
        break;

    default:
        break;
    }

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "cannot open vout, aborting" );

        return 0;
    }
 
    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    return 0;
}

/*****************************************************************************
 * vout_End: terminate Deinterlace video thread output method
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
 * vout_Destroy: destroy Deinterlace video thread output method
 *****************************************************************************
 * Terminate an output method created by DeinterlaceCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    vout_DestroyThread( p_vout->p_sys->p_vout );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle Deinterlace events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    return 0;
}

/*****************************************************************************
 * vout_Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Deinterlace image,
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static void vout_Render ( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *pp_outpic[2];

    /* Get a new picture */
    while( ( pp_outpic[0] = vout_CreatePicture( p_vout->p_sys->p_vout,
                                             0, 0, 0 ) )
              == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
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
            RenderBob( p_vout, pp_outpic[0], p_pic, 0 );
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
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function does nothing, since all the rendering was already done.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

/*****************************************************************************
 * RenderBob: renders a bob picture
 *****************************************************************************/
static void RenderBob( vout_thread_t *p_vout,
                       picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        u8 *p_in, *p_out_end, *p_out;
        int i_increment;

        p_in = p_pic->p[i_plane].p_pixels
                   + i_field * p_pic->p[i_plane].i_pitch;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_lines;

        switch( p_vout->render.i_chroma )
        {
        case FOURCC_I420:
        case FOURCC_IYUV:
        case FOURCC_YV12:

            for( ; p_out < p_out_end ; )
            {
                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_pic->p[i_plane].i_pitch );

                p_out += p_pic->p[i_plane].i_pitch;
                p_in += 2 * p_pic->p[i_plane].i_pitch;
            }
            break;

        case FOURCC_I422:

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
 * RenderLinear: displays previously rendered output
 *****************************************************************************/
static void RenderLinear( vout_thread_t *p_vout,
                          picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        u8 *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels
                   + i_field * p_pic->p[i_plane].i_pitch;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_lines;

        if( i_field == 0 )
        {
            p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                      p_pic->p[i_plane].i_pitch );
            p_in += 2 * p_pic->p[i_plane].i_pitch;
            p_out += p_pic->p[i_plane].i_pitch;
        }

        p_out_end -= p_outpic->p[i_plane].i_pitch;

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

#if 0
        if( i_field == 0 )
        {
            p_in -= 2 * p_pic->p[i_plane].i_pitch;
            p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                      p_pic->p[i_plane].i_pitch );
        }
#endif
    }
}

static void RenderMean( vout_thread_t *p_vout,
                        picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        u8 *p_in, *p_out_end, *p_out;

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
        u8 *p_in, *p_out_end, *p_out;

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

static void Merge( void *p_dest, const void *p_s1,
                   const void *p_s2, size_t i_bytes )
{
    u8* p_end = (u8*)p_dest + i_bytes - 8;

    while( (u8*)p_dest < p_end )
    {
        *(u8*)p_dest++ = ( (u16)(*(u8*)p_s1++) + (u16)(*(u8*)p_s2++) ) >> 1;
        *(u8*)p_dest++ = ( (u16)(*(u8*)p_s1++) + (u16)(*(u8*)p_s2++) ) >> 1;
        *(u8*)p_dest++ = ( (u16)(*(u8*)p_s1++) + (u16)(*(u8*)p_s2++) ) >> 1;
        *(u8*)p_dest++ = ( (u16)(*(u8*)p_s1++) + (u16)(*(u8*)p_s2++) ) >> 1;
        *(u8*)p_dest++ = ( (u16)(*(u8*)p_s1++) + (u16)(*(u8*)p_s2++) ) >> 1;
        *(u8*)p_dest++ = ( (u16)(*(u8*)p_s1++) + (u16)(*(u8*)p_s2++) ) >> 1;
        *(u8*)p_dest++ = ( (u16)(*(u8*)p_s1++) + (u16)(*(u8*)p_s2++) ) >> 1;
        *(u8*)p_dest++ = ( (u16)(*(u8*)p_s1++) + (u16)(*(u8*)p_s2++) ) >> 1;
    }

    p_end += 8;

    while( (u8*)p_dest < p_end )
    {
        *(u8*)p_dest++ = ( (u16)(*(u8*)p_s1++) + (u16)(*(u8*)p_s2++) ) >> 1;
    }
}
