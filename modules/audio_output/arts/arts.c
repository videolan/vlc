/*****************************************************************************
 * arts.c : aRts module
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Emmanuel Blindauer <manu@agat.net>
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
#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>

#include <artsc.h>

/*****************************************************************************
 * aout_sys_t: arts audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some arts specific variables.
 *****************************************************************************/
struct aout_sys_t
{
    arts_stream_t stream;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

static int  SetFormat    ( aout_thread_t * );
static int  GetBufInfo   ( aout_thread_t *, int );
static void Play         ( aout_thread_t *, byte_t *, int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
   set_description( _("aRts audio module") );
   set_capability( "audio output", 50 );
   set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize arts connection to server
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_thread_t *p_aout = (aout_thread_t *)p_this;
    int i_err = 0;

    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return( 1 );
    }

    i_err = arts_init();
    
    if (i_err < 0)
    {
        msg_Err( p_aout, "arts_init failed (%s)", arts_error_text(i_err) );
        free( p_aout->p_sys );
        return(-1);
    }

    p_aout->pf_setformat = SetFormat;
    p_aout->pf_getbufinfo = GetBufInfo;
    p_aout->pf_play = Play;

    p_aout->p_sys->stream =
        arts_play_stream( p_aout->i_rate, 16, p_aout->i_channels, "vlc" );

    return( 0 );
}

/*****************************************************************************
 * SetFormat: set the output format
 *****************************************************************************/
static int SetFormat( aout_thread_t *p_aout )
{
   /*Not ready*/ 
/*    p_aout->i_latency = esd_get_latency(i_fd);*/
    p_aout->i_latency = 0;
   
    //msg_Dbg( p_aout, "aout_arts_latency: %d", p_aout->i_latency );

    return( 0 );
}

/*****************************************************************************
 * GetBufInfo: buffer status query
 *****************************************************************************/
static int GetBufInfo( aout_thread_t *p_aout, int i_buffer_limit )
{
    /* arbitrary value that should be changed */
    return( i_buffer_limit );
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the socket
 *****************************************************************************/
static void Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    int i_err = arts_write( p_aout->p_sys->stream, buffer, i_size );

    if( i_err < 0 )
    {
        msg_Err( p_aout, "arts_write failed (%s)", arts_error_text(i_err) );
    }
}

/*****************************************************************************
 * Close: close the Esound socket
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_thread_t *p_aout = (aout_thread_t *)p_this;

    arts_close_stream( p_aout->p_sys->stream );
    free( p_aout->p_sys );
}

