/*****************************************************************************
 * deinterlace.c : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: deinterlace.c,v 1.1 2001/12/30 07:09:55 sam Exp $
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

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "filter_common.h"

#define DEINTERLACE_MODE_BOB     1
#define DEINTERLACE_MODE_BLEND   2

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list );

static void *memblend( void *, const void *, const void *, size_t );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "deinterlacing module" )
    /* Capability score set to 0 because we don't want to be spawned
     * as a video output unless explicitly requested to */
    ADD_CAPABILITY( VOUT, 0 )
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
typedef struct vout_sys_s
{
    int i_mode;
    struct vout_thread_s *p_vout;

} vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s *, struct picture_s * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = NULL;
}

/*****************************************************************************
 * intf_Probe: return a score
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    return( 0 );
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
        intf_ErrMsg("error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Look what method was requested */
    psz_method = main_GetPszVariable( VOUT_FILTER_VAR, "" );

    while( *psz_method && *psz_method != ':' )
    {
        psz_method++;
    }

    if( !strcmp( psz_method, ":bob" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_MODE_BOB;
    }
    else if( !strcmp( psz_method, ":blend" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_MODE_BLEND;
    }
    else
    {
        intf_ErrMsg( "filter error: no valid deinterlace mode provided, "
                     "using deinterlace:bob" );
        p_vout->p_sys->i_mode = DEINTERLACE_MODE_BOB;
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize Deinterlace video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    char *psz_filter;
    picture_t *p_pic;
    
    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure, full of directbuffers since we want
     * the decoder to output directly to our structures. */
    switch( p_vout->render.i_chroma )
    {
        case YUV_420_PICTURE:
        case YUV_422_PICTURE:
            p_vout->output.i_chroma = p_vout->render.i_chroma;
            p_vout->output.i_width  = p_vout->render.i_width;
            p_vout->output.i_height = p_vout->render.i_height;
            p_vout->output.i_aspect = p_vout->render.i_aspect;
            break;

        default:
            return( 0 ); /* unknown chroma */
            break;
    }

    /* Try to open the real video output, with half the height our images */
    psz_filter = main_GetPszVariable( VOUT_FILTER_VAR, "" );
    main_PutPszVariable( VOUT_FILTER_VAR, "" );

    intf_WarnMsg( 1, "filter: spawning the real video output" );

    switch( p_vout->render.i_chroma )
    {
    case YUV_420_PICTURE:
        switch( p_vout->p_sys->i_mode )
        {
        case DEINTERLACE_MODE_BOB:
            p_vout->p_sys->p_vout =
                vout_CreateThread( NULL,
                       p_vout->output.i_width, p_vout->output.i_height / 2,
                       p_vout->output.i_chroma, p_vout->output.i_aspect );
            break;

        case DEINTERLACE_MODE_BLEND:
            p_vout->p_sys->p_vout =
                vout_CreateThread( NULL,
                       p_vout->output.i_width, p_vout->output.i_height,
                       p_vout->output.i_chroma, p_vout->output.i_aspect );
            break;
        }
        break;

    case YUV_422_PICTURE:
        p_vout->p_sys->p_vout =
            vout_CreateThread( NULL,
                       p_vout->output.i_width, p_vout->output.i_height,
                       YUV_420_PICTURE, p_vout->output.i_aspect );
        break;

    default:
        break;
    }

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        intf_ErrMsg( "filter error: can't open vout, aborting" );

        return( 0 );
    }
 
    main_PutPszVariable( VOUT_FILTER_VAR, psz_filter );

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    return( 0 );
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
        free( PP_OUTPUTPICTURE[ i_index ]->planes[ 0 ].p_data );
    }
}

/*****************************************************************************
 * vout_Destroy: destroy Deinterlace video thread output method
 *****************************************************************************
 * Terminate an output method created by DeinterlaceCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    vout_DestroyThread( p_vout->p_sys->p_vout, NULL );

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
    return( 0 );
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Deinterlace image,
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic;
    int i_index, i_field;

    for( i_field = 0 ; i_field < 2 ; i_field++ )
    {
        /* Get a structure from the video_output. */
        while( ( p_outpic = vout_CreatePicture( p_vout->p_sys->p_vout,
                                                0, 0, 0 ) )
                  == NULL )
        {
            if( p_vout->b_die || p_vout->b_error )
            {
                return;
            }
            msleep( VOUT_OUTMEM_SLEEP );
        }   

        /* XXX: completely arbitrary values ! */
        vout_DatePicture( p_vout->p_sys->p_vout, p_outpic,
                          mdate() + (mtime_t)(50000 + i_field * 20000) );

        /* Copy image and skip lines */
        for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
        {
            pixel_data_t *p_in, *p_out_end, *p_out;
            int i_increment;

            p_in = p_pic->planes[ i_index ].p_data
                       + i_field * p_pic->planes[ i_index ].i_line_bytes;

            p_out = p_outpic->planes[ i_index ].p_data;
            p_out_end = p_out + p_outpic->planes[ i_index ].i_bytes;

            switch( p_vout->render.i_chroma )
            {
            case YUV_420_PICTURE:

                switch( p_vout->p_sys->i_mode )
                {
                case DEINTERLACE_MODE_BOB:
                    for( ; p_out < p_out_end ; )
                    {
                        FAST_MEMCPY( p_out, p_in,
                                     p_pic->planes[ i_index ].i_line_bytes );

                        p_out += p_pic->planes[ i_index ].i_line_bytes;
                        p_in += 2 * p_pic->planes[ i_index ].i_line_bytes;
                    }
                    break;

                case DEINTERLACE_MODE_BLEND:
                    if( i_index != Y_PLANE )
                    {
                        for( ; p_out < p_out_end ; )
                        {
                            FAST_MEMCPY( p_out, p_in,
                                     p_pic->planes[ i_index ].i_line_bytes );

                            p_out += p_pic->planes[ i_index ].i_line_bytes;

                            FAST_MEMCPY( p_out, p_in,
                                     p_pic->planes[ i_index ].i_line_bytes );

                            p_out += p_pic->planes[ i_index ].i_line_bytes;
                            p_in += 2 * p_pic->planes[ i_index ].i_line_bytes;
                        }
                        break;
                    }

                    if( i_field == 0 )
                    {
                        FAST_MEMCPY( p_out, p_in,
                                     p_pic->planes[ i_index ].i_line_bytes );
                        p_in += 2 * p_pic->planes[ i_index ].i_line_bytes;
                        p_out += p_pic->planes[ i_index ].i_line_bytes;
                    }

                    for( ; p_out < p_out_end ; )
                    {
                        FAST_MEMCPY( p_out, p_in,
                                     p_pic->planes[ i_index ].i_line_bytes );

                        p_out += p_pic->planes[ i_index ].i_line_bytes;

                        memblend( p_out, p_in, p_in + 2 * p_pic->planes[ i_index ].i_line_bytes, p_pic->planes[ i_index ].i_line_bytes );

                        p_in += 2 * p_pic->planes[ i_index ].i_line_bytes;
                        p_out += p_pic->planes[ i_index ].i_line_bytes;
                    }
                    break;
                }
                break;

            case YUV_422_PICTURE:

                i_increment = 2 * p_pic->planes[ i_index ].i_line_bytes;

                if( i_index == Y_PLANE )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        FAST_MEMCPY( p_out, p_in,
                                 p_pic->planes[ i_index ].i_line_bytes );
                        p_out += p_pic->planes[ i_index ].i_line_bytes;
                        FAST_MEMCPY( p_out, p_in,
                                 p_pic->planes[ i_index ].i_line_bytes );
                        p_out += p_pic->planes[ i_index ].i_line_bytes;
                        p_in += i_increment;
                    }
                }
                else
                {
                    for( ; p_out < p_out_end ; )
                    {
                        FAST_MEMCPY( p_out, p_in,
                                 p_pic->planes[ i_index ].i_line_bytes );
                        p_out += p_pic->planes[ i_index ].i_line_bytes;
                        p_in += i_increment;
                    }
                }
                break;

            default:
                break;
            }
        }

        vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
    }
}

static void *memblend( void *p_dest, const void *p_s1,
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

    return p_dest;
}

