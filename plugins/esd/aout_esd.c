/*****************************************************************************
 * aout_esd.c : Esound functions library
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: aout_esd.c,v 1.21 2002/02/24 22:06:50 sam Exp $
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

/* TODO:
 *
 * - use the libesd function to get latency when it's not buggy anymore
 *
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <esd.h>

#include <videolan/vlc.h>

#include "audio_output.h"                                   /* aout_thread_t */

/*****************************************************************************
 * aout_sys_t: esd audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some esd specific variables.
 *****************************************************************************/
typedef struct aout_sys_s
{
    esd_format_t esd_format;
    int          i_fd;

} aout_sys_t;

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
void _M( aout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.aout.pf_open = aout_Open;
    p_function_list->functions.aout.pf_setformat = aout_SetFormat;
    p_function_list->functions.aout.pf_getbufinfo = aout_GetBufInfo;
    p_function_list->functions.aout.pf_play = aout_Play;
    p_function_list->functions.aout.pf_close = aout_Close;
}

/*****************************************************************************
 * aout_Open: open an esd socket
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    /* mpg123 does it this way */
    int i_bits = ESD_BITS16;
    int i_mode = ESD_STREAM;
    int i_func = ESD_PLAY;

    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Initialize some variables */
    p_aout->i_rate = esd_audio_rate; /* We use actual esd rate value, not
                                      * initial value */

    i_bits = ESD_BITS16;
    i_mode = ESD_STREAM;
    i_func = ESD_PLAY;
    p_aout->p_sys->esd_format = (i_bits | i_mode | i_func) & (~ESD_MASK_CHAN);

    if( p_aout->i_channels == 1 )
    {
        p_aout->p_sys->esd_format |= ESD_MONO;
    }
    else
    {
        p_aout->p_sys->esd_format |= ESD_STEREO;
    }

    /* open a socket for playing a stream
     * and try to open /dev/dsp if there's no EsounD */
    if ( (p_aout->p_sys->i_fd
            = esd_play_stream_fallback(p_aout->p_sys->esd_format,
                p_aout->i_rate, NULL, "vlc")) < 0 )
    {
        intf_ErrMsg( "aout error: can't open esound socket"
                     " (format 0x%08x at %ld Hz)",
                     p_aout->p_sys->esd_format, p_aout->i_rate );
        return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * aout_SetFormat: set the output format
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
    int i_fd;

    i_fd = esd_open_sound(NULL);
    p_aout->i_latency = esd_get_latency(i_fd);
   
    intf_WarnMsg(2, "aout_esd_latency: %d",p_aout->i_latency);

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
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    int i_amount;
    
    if (p_aout->p_sys->esd_format & ESD_STEREO)
    {
        if (p_aout->p_sys->esd_format & ESD_BITS16)
        {
            i_amount = (44100 * (ESD_BUF_SIZE + 64)) / p_aout->i_rate;
        }
        else
        {
            i_amount = (44100 * (ESD_BUF_SIZE + 128)) / p_aout->i_rate;
        }
    }
    else
    {
        if (p_aout->p_sys->esd_format & ESD_BITS16)
        {
            i_amount = (2 * 44100 * (ESD_BUF_SIZE + 128)) / p_aout->i_rate;
        }
        else
        {
            i_amount = (2 * 44100 * (ESD_BUF_SIZE + 256)) / p_aout->i_rate;
        }
    }

    write( p_aout->p_sys->i_fd, buffer, i_size );
}

/*****************************************************************************
 * aout_Close: close the Esound socket
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    close( p_aout->p_sys->i_fd );
}

