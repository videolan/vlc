/*****************************************************************************
 * v4l.c : Video4Linux input module for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: v4l.c,v 1.1 2002/08/08 00:35:10 sam Exp $
 *
 * Author: Samuel Hocevar <sam@zoy.org>
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
#include <vlc/vlc.h>
#include <vlc/input.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  V4lOpen  ( vlc_object_t * );
static void V4lClose ( vlc_object_t * ); 
static int  V4lRead  ( input_thread_t *, byte_t *, size_t );

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Video4Linux input module") );
    set_capability( "access", 80 );
    set_callbacks( V4lOpen, V4lClose );
vlc_module_end();

static int i_fd;

/*****************************************************************************
 * V4lOpen: open device
 *****************************************************************************/
static int V4lOpen( vlc_object_t *p_this )
{   
    input_thread_t * p_input = (input_thread_t *)p_this;

    p_input->pf_read = V4lRead;
    p_input->pf_seek = NULL;
    p_input->pf_set_area = NULL;
    p_input->pf_set_program = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_FILE;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    i_fd = open( "/dev/v4l/video0", O_RDWR );

    return 0;
}

/*****************************************************************************
 * V4lClose: close device
 *****************************************************************************/
static void V4lClose( vlc_object_t *p_this )
{
    input_thread_t *   p_input = (input_thread_t *)p_this;
    //thread_data_t *p_data = (thread_data_t *)p_input->p_access_data;

    //close( p_data->i_handle );
    close( i_fd );
    //free( p_data );
}

/*****************************************************************************
 * V4lRead: reads from the device into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
#define WIDTH 640
#define HEIGHT 480

static int V4lRead( input_thread_t * p_input, byte_t * p_buffer,
                    size_t i_len )
{   
    struct video_capability vid_caps;
    struct video_mbuf vid_buf;
    struct video_mmap vid_mmap;

    char *map = NULL;

    //thread_data_t *     p_data;
    int i_read = 0;

    if( ioctl( i_fd, VIDIOCGCAP, &vid_caps ) == -1 )
    {
        printf("ioctl (VIDIOCGCAP) failed\n");
        return 0;
    }

    if( ioctl( i_fd, VIDIOCGMBUF, &vid_buf ) == -1 )
    {
        // to do a normal read()
        map = malloc (WIDTH * HEIGHT * 3);
        len = read (fd_webcam, map, WIDTH * HEIGHT * 3);
        if (len <=  0)
        {
            free (map);
            return (NULL);
        }
        *size = 0;
        return (map);
    }

    //p_data = (thread_data_t *)p_input->p_access_data;
    return i_read;
}

