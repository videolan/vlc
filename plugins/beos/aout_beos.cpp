/*****************************************************************************
 * aout_beos.cpp: BeOS audio output
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: aout_beos.cpp,v 1.22 2002/02/24 20:51:09 gbazin Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
#include <stdio.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <kernel/OS.h>
#include <View.h>
#include <Application.h>
#include <Message.h>
#include <Locker.h>
#include <media/MediaDefs.h>
#include <game/PushGameSound.h>
#include <malloc.h>
#include <string.h>

extern "C"
{
#include <videolan/vlc.h>

#include "audio_output.h"
}

/*****************************************************************************
 * aout_sys_t: BeOS audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some BeOS specific variables.
 *****************************************************************************/
typedef struct aout_sys_s
{
    BPushGameSound * p_sound;
    gs_audio_format * p_format;
    void * p_buffer;
    long i_buffer_size;
    long i_buffer_pos;

} aout_sys_t;

extern "C"
{

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     aout_Open        ( aout_thread_t *p_aout );
static int     aout_SetFormat   ( aout_thread_t *p_aout );
static long    aout_GetBufInfo  ( aout_thread_t *p_aout, long l_buffer_info );
static void    aout_Play        ( aout_thread_t *p_aout,
                                  byte_t *buffer, int i_size );
static void    aout_Close       ( aout_thread_t *p_aout );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( aout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.aout.pf_open = aout_Open;
    p_function_list->functions.aout.pf_setformat = aout_SetFormat;
    p_function_list->functions.aout.pf_getbufinfo = aout_GetBufInfo;
    p_function_list->functions.aout.pf_play = aout_Play;
    p_function_list->functions.aout.pf_close = aout_Close;
}

/*****************************************************************************
 * aout_Open: opens a BPushGameSound
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    /* Allocate structure */
    p_aout->p_sys = (aout_sys_t*) malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Allocate gs_audio_format */
    p_aout->p_sys->p_format = (gs_audio_format *) malloc( sizeof( gs_audio_format ) );
    if( p_aout->p_sys->p_format == NULL )
    {
        free( p_aout->p_sys );
        intf_ErrMsg("error: cannot allocate memory for gs_audio_format" );
        return( 1 );
    }

    /* Initialize some variables */
    p_aout->p_sys->p_format->frame_rate = 44100.0;
    p_aout->p_sys->p_format->channel_count = p_aout->i_channels;
    p_aout->p_sys->p_format->format = gs_audio_format::B_GS_S16;
    p_aout->p_sys->p_format->byte_order = B_MEDIA_LITTLE_ENDIAN;
    p_aout->p_sys->p_format->buffer_size = 4*8192;
    p_aout->p_sys->i_buffer_pos = 0;

    /* Allocate BPushGameSound */
    p_aout->p_sys->p_sound = new BPushGameSound( 8192,
                                                 p_aout->p_sys->p_format,
                                                 2, NULL );
    if( p_aout->p_sys->p_sound == NULL )
    {
        free( p_aout->p_sys->p_format );
        free( p_aout->p_sys );
        intf_ErrMsg("error: cannot allocate memory for BPushGameSound" );
        return( 1 );
    }

    if( p_aout->p_sys->p_sound->InitCheck() != B_OK )
    {
        free( p_aout->p_sys->p_format );
        free( p_aout->p_sys );
        intf_ErrMsg("error: cannot allocate memory for BPushGameSound" );
        return( 1 );
    }

    p_aout->p_sys->p_sound->StartPlaying( );

    p_aout->p_sys->p_sound->LockForCyclic( &p_aout->p_sys->p_buffer,
                            (size_t *)&p_aout->p_sys->i_buffer_size );

    return( 0 );
}

/*****************************************************************************
 * aout_SetFormat: sets the dsp output format
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
    return( 0 );
}

/*****************************************************************************
 * aout_GetBufInfo: buffer status query
 *****************************************************************************/
static long aout_GetBufInfo( aout_thread_t *p_aout, long l_buffer_limit )
{
    /* Each value is 4 bytes long (stereo signed 16 bits) */
    long i_hard_pos = 4 * p_aout->p_sys->p_sound->CurrentPosition();

    i_hard_pos = p_aout->p_sys->i_buffer_pos - i_hard_pos;
    if( i_hard_pos < 0 )
    {
         i_hard_pos += p_aout->p_sys->i_buffer_size;
    }

    return( i_hard_pos );
}

/*****************************************************************************
 * aout_Play: plays a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the dsp
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    long i_newbuf_pos;

    if( (i_newbuf_pos = p_aout->p_sys->i_buffer_pos + i_size)
              > p_aout->p_sys->i_buffer_size )
    {
        memcpy( (void *)((int)p_aout->p_sys->p_buffer
                        + p_aout->p_sys->i_buffer_pos),
                buffer,
                p_aout->p_sys->i_buffer_size - p_aout->p_sys->i_buffer_pos );

        memcpy( (void *)((int)p_aout->p_sys->p_buffer),
                buffer + p_aout->p_sys->i_buffer_size - p_aout->p_sys->i_buffer_pos,
                i_size - ( p_aout->p_sys->i_buffer_size
                             - p_aout->p_sys->i_buffer_pos ) );
        
        p_aout->p_sys->i_buffer_pos = i_newbuf_pos - p_aout->p_sys->i_buffer_size;

    }
    else
    {
        memcpy( (void *)((int)p_aout->p_sys->p_buffer + p_aout->p_sys->i_buffer_pos),
                buffer, i_size );

        p_aout->p_sys->i_buffer_pos = i_newbuf_pos;
    }
}

/*****************************************************************************
 * aout_Close: closes the dsp audio device
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    p_aout->p_sys->p_sound->UnlockCyclic();
    p_aout->p_sys->p_sound->StopPlaying( );
    delete p_aout->p_sys->p_sound;
    free( p_aout->p_sys->p_format );
    free( p_aout->p_sys );
}

} /* extern "C" */

