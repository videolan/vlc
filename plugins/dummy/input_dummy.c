/*****************************************************************************
 * input_dummy.c: dummy input plugin, to manage "vlc:***" special options
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: input_dummy.c,v 1.21 2002/07/31 20:56:51 sam Exp $
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
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/input.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux ( input_thread_t * );

/*****************************************************************************
 * access_sys_t: private input data
 *****************************************************************************/
struct demux_sys_t
{
    /* The real command */
    int i_command;

    /* Used for the pause command */
    mtime_t expiration;
};

#define COMMAND_NOP   0
#define COMMAND_QUIT  1
#define COMMAND_LOOP  2
#define COMMAND_PAUSE 3

/*****************************************************************************
 * OpenAccess: open the target, ie. do nothing
 *****************************************************************************/
int E_(OpenAccess) ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;

    p_input->stream.i_method = INPUT_METHOD_NONE;

    /* Force dummy demux plug-in */
    p_input->psz_demux = "vlc";

    return VLC_SUCCESS;
}

/*****************************************************************************
 * OpenDemux: initialize the target, ie. parse the command
 *****************************************************************************/
int E_(OpenDemux) ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    char * psz_name = p_input->psz_name;
    int i_len = strlen( psz_name );
    struct demux_sys_t * p_method;
    int   i_arg;
    
    p_input->stream.b_seekable = 0;
    p_input->pf_demux = Demux;
    p_input->pf_rewind = NULL;

    p_method = malloc( sizeof( struct demux_sys_t ) );
    if( p_method == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return -1;
    }

    p_input->p_demux_data = p_method;
    p_input->stream.p_demux_data = NULL;

    /* Check for a "vlc:nop" command */
    if( i_len == 3 && !strncasecmp( psz_name, "nop", 3 ) )
    {
        msg_Info( p_input, "command `nop'" );
        p_method->i_command = COMMAND_NOP;
        return 0;
    }

    /* Check for a "vlc:quit" command */
    if( i_len == 4 && !strncasecmp( psz_name, "quit", 4 ) )
    {
        msg_Info( p_input, "command `quit'" );
        p_method->i_command = COMMAND_QUIT;
        return 0;
    }

    /* Check for a "vlc:loop" command */
    if( i_len == 4 && !strncasecmp( psz_name, "loop", 4 ) )
    {
        msg_Info( p_input, "command `loop'" );
        p_method->i_command = COMMAND_LOOP;
        return 0;
    }

    /* Check for a "vlc:pause:***" command */
    if( i_len > 6 && !strncasecmp( psz_name, "pause:", 6 ) )
    {
        i_arg = atoi( psz_name + 6 );
        msg_Info( p_input, "command `pause %i'", i_arg );
        p_method->i_command = COMMAND_PAUSE;
        p_method->expiration = mdate() + (mtime_t)i_arg * (mtime_t)1000000;
        return 0;
    }

    msg_Err( p_input, "unknown command `%s'", psz_name );
    free( p_input->p_demux_data );
    p_input->b_error = 1;

    return -1;
}

/*****************************************************************************
 * CloseDemux: initialize the target, ie. parse the command
 *****************************************************************************/
void E_(CloseDemux) ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;

    free( p_input->p_demux_data );
}

/*****************************************************************************
 * Demux: do what the command says
 *****************************************************************************/
static int Demux( input_thread_t *p_input )
{
    struct demux_sys_t * p_method = p_input->p_demux_data;
    playlist_t *p_playlist;

    p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST, FIND_PARENT );

    if( p_playlist == NULL )
    {
        msg_Err( p_input, "we are not attached to a playlist" );
        p_input->b_error = 1;
        return 1;
    }

    switch( p_method->i_command )
    {
        case COMMAND_QUIT:
            p_input->p_vlc->b_die = 1;
            break;

        case COMMAND_LOOP:
            playlist_Goto( p_playlist, 0 );
            break;

        case COMMAND_PAUSE:
            if( mdate() < p_method->expiration )
            {
                msleep( 10000 );
            }
            else
            {
                p_input->b_eof = 1;
            }
            break;

        case COMMAND_NOP:
        default:
            p_input->b_eof = 1;
            break;
    }

    vlc_object_release( p_playlist );

    return 1;
}

