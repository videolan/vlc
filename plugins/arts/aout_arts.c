/*****************************************************************************
 * aout_arts.c : aRts functions library
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Blindauer Emmanuel <manu@agat.net>
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
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <artsc.h>

#include <videolan/vlc.h>

#include "audio_output.h"                                   /* aout_thread_t */

/*****************************************************************************
 * aout_sys_t: arts audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some arts specific variables.
 *****************************************************************************/
typedef struct aout_sys_s
{
    arts_stream_t stream;

} aout_sys_t;

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
 * aout_Open: initialize arts connection to server
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    int i_err = 0;

    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    i_err = arts_init();
    
    if (i_err < 0)
    {
        intf_ErrMsg( "aout error: arts_init (%s)", arts_error_text(i_err) );
        free( p_aout->p_sys );
        return(-1);
    }

    p_aout->p_sys->stream =
        arts_play_stream( p_aout->l_rate, 16, p_aout->i_channels, "vlc" );

    return( 0 );
}

/*****************************************************************************
 * aout_SetFormat: set the output format
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
   /*Not ready*/ 
/*    p_aout->i_latency = esd_get_latency(i_fd);*/
    p_aout->i_latency = 0;
   
    //intf_WarnMsg( 5, "aout_arts_latency: %d",p_aout->i_latency);

    return( 0 );
}

/*****************************************************************************
 * aout_GetBufInfo: buffer status query
 *****************************************************************************/
static long aout_GetBufInfo( aout_thread_t *p_aout, long l_buffer_limit )
{
    /* arbitrary value that should be changed */
    return( l_buffer_limit );
}

/*****************************************************************************
 * aout_Play: play a sound samples buffer
 *****************************************************************************
 * This function writes a buffer of i_length bytes in the socket
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{

    int i_err = arts_write( p_aout->p_sys->stream, buffer, i_size );

    if( i_err < 0 )
    {
        intf_ErrMsg( "aout error: arts_write (%s)", arts_error_text(i_err) );
    }

}

/*****************************************************************************
 * aout_Close: close the Esound socket
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    arts_close_stream( p_aout->p_sys->stream );
    free( p_aout->p_sys );
}

