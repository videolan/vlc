/*****************************************************************************
 * esd.c : EsounD module
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: esd.c,v 1.17 2002/07/31 20:56:51 sam Exp $
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
#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>

#include <esd.h>

/*****************************************************************************
 * aout_sys_t: esd audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes some esd specific variables.
 *****************************************************************************/
struct aout_sys_t
{
    esd_format_t esd_format;
    int          i_fd;
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
    set_description( _("EsounD audio module") ); 
    set_capability( "audio output", 50 );
    set_callbacks( Open, Close );
    add_shortcut( "esound" );
vlc_module_end();

/*****************************************************************************
 * Open: open an esd socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_thread_t *p_aout = (aout_thread_t *)p_this;

    /* mpg123 does it this way */
    int i_bits = ESD_BITS16;
    int i_mode = ESD_STREAM;
    int i_func = ESD_PLAY;

    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
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
        msg_Err( p_aout, "cannot open esound socket (format 0x%08x at %ld Hz)",
                         p_aout->p_sys->esd_format, p_aout->i_rate );
        return( -1 );
    }

    p_aout->pf_setformat = SetFormat;
    p_aout->pf_getbufinfo = GetBufInfo;
    p_aout->pf_play = Play;

    return( 0 );
}

/*****************************************************************************
 * SetFormat: set the output format
 *****************************************************************************/
static int SetFormat( aout_thread_t *p_aout )
{
    int i_fd;

    i_fd = esd_open_sound(NULL);
    p_aout->i_latency = esd_get_latency(i_fd);
   
    msg_Dbg( p_aout, "aout_esd_latency: %d", p_aout->i_latency );

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
 * Close: close the Esound socket
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_thread_t *p_aout = (aout_thread_t *)p_this;

    close( p_aout->p_sys->i_fd );
}

