/*****************************************************************************
 * scope.c : Scope effect module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: scope.c,v 1.2 2002/02/27 22:57:10 sam Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include <errno.h>

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "audio_output.h"                                   /* aout_thread_t */

#define SCOPE_WIDTH 640
#define SCOPE_HEIGHT 200
#define SCOPE_ASPECT (VOUT_ASPECT_FACTOR*SCOPE_WIDTH/SCOPE_HEIGHT)

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
static void aout_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * aout_sys_t: scope audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some scope specific variables.
 *****************************************************************************/
typedef struct aout_sys_s
{
    struct aout_thread_s aout;
    struct aout_fifo_s *p_aout_fifo;

    struct vout_thread_s *p_vout;

} aout_sys_t;

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "scope effect module" )
    ADD_CAPABILITY( AOUT, 0 )
    ADD_SHORTCUT( "scope" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    aout_getfunctions( &p_module->p_functions->aout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     aout_Open        ( aout_thread_t *p_aout );
static int     aout_SetFormat   ( aout_thread_t *p_aout );
static int     aout_GetBufInfo  ( aout_thread_t *p_aout, int i_buffer_info );
static void    aout_Play        ( aout_thread_t *p_aout,
                                  byte_t *buffer, int i_size );
static void    aout_Close       ( aout_thread_t *p_aout );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void aout_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.aout.pf_open = aout_Open;
    p_function_list->functions.aout.pf_setformat = aout_SetFormat;
    p_function_list->functions.aout.pf_getbufinfo = aout_GetBufInfo;
    p_function_list->functions.aout.pf_play = aout_Play;
    p_function_list->functions.aout.pf_close = aout_Close;
}

/*****************************************************************************
 * aout_Open: open a scope effect plugin
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    p_aout->p_sys->p_vout =
        vout_CreateThread( NULL, SCOPE_WIDTH, SCOPE_HEIGHT,
                           FOURCC_I420, SCOPE_ASPECT );

    return( 0 );
}

/*****************************************************************************
 * aout_SetFormat: set the output format
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
    /* Force the output method */
    p_aout->i_format = AOUT_FMT_U16_LE;
    p_aout->i_channels = 2;

    p_aout->p_sys->aout.i_format = p_aout->i_format;
    p_aout->p_sys->aout.i_channels = p_aout->i_channels;
    p_aout->p_sys->aout.i_rate = p_aout->i_rate;

    return( 0 );
}

/*****************************************************************************
 * aout_GetBufInfo: buffer status query
 *****************************************************************************/
static int aout_GetBufInfo( aout_thread_t *p_aout, int i_buffer_limit )
{
    /* arbitrary value that should be changed */
    return( i_buffer_limit );
}

/*****************************************************************************
 * aout_Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the socket
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *p_buffer, int i_size )
{
    picture_t *p_outpic;
    int i_index;
    u8 *p_pixel;
    u16 *p_sample;

    /* This is a new frame. Get a structure from the video_output. */
    while( ( p_outpic = vout_CreatePicture( p_aout->p_sys->p_vout, 0, 0, 0 ) )
              == NULL )
    {
        if( p_aout->b_die )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    /* Blank the picture */
    for( i_index = 0 ; i_index < p_outpic->i_planes ; i_index++ )
    {
        memset( p_outpic->p[i_index].p_pixels, i_index ? 0x80 : 0x00,
                p_outpic->p[i_index].i_lines * p_outpic->p[i_index].i_pitch );
    }

    /* Left channel */
    for( i_index = 0, p_sample = (u16*)p_buffer;
         i_index < SCOPE_WIDTH && i_index < i_size / 2;
         i_index++ )
    {
        int i;
        u8 i_value = *p_sample / 256;

        for( i = 0 ; i < 8 ; i++ )
        {
            p_pixel = p_outpic->p[0].p_pixels + (p_outpic->p[0].i_pitch * i_index) / SCOPE_WIDTH + p_outpic->p[0].i_lines * (u8)(i_value+128) / 512 * p_outpic->p[0].i_pitch;
            *p_pixel = 0x9f;
            p_pixel = p_outpic->p[1].p_pixels + (p_outpic->p[1].i_pitch * i_index) / SCOPE_WIDTH + p_outpic->p[1].i_lines * (u8)(i_value+128) / 512 * p_outpic->p[1].i_pitch;
            *p_pixel = 0x00;
            p_sample += 2;
        }
    }

    /* Right channel */
    for( i_index = 0, p_sample = (u16*)p_buffer + 1;
         i_index < SCOPE_WIDTH && i_index < i_size / 2;
         i_index++ )
    {
        int i;
        u8 i_value = *p_sample / 256;

        for( i = 0 ; i < 8 ; i++ )
        {
            p_pixel = p_outpic->p[0].p_pixels + (p_outpic->p[0].i_pitch * i_index) / SCOPE_WIDTH + (p_outpic->p[0].i_lines * (u8)(i_value+128) / 512 + p_outpic->p[0].i_lines / 2) * p_outpic->p[0].i_pitch;
            *p_pixel = 0x7f;
            p_pixel = p_outpic->p[2].p_pixels + (p_outpic->p[2].i_pitch * i_index) / SCOPE_WIDTH + (p_outpic->p[2].i_lines * (u8)(i_value+128) / 512 + p_outpic->p[2].i_lines / 2) * p_outpic->p[2].i_pitch;
            *p_pixel = 0xdd;
            p_sample += 2;
        }
    }

    /* Display the picture */
    vout_DatePicture( p_aout->p_sys->p_vout, p_outpic, p_aout->date );
    vout_DisplayPicture( p_aout->p_sys->p_vout, p_outpic );
}

/*****************************************************************************
 * aout_Close: close the Esound socket
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    vout_DestroyThread( p_aout->p_sys->p_vout, NULL );
    free( p_aout->p_sys );
}

