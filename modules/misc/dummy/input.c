/*****************************************************************************
 * input_dummy.c: dummy input plugin, to manage "vlc:***" special options
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id$
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
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/input.h>

/*****************************************************************************
 * Access functions.
 *****************************************************************************/
static int AccessRead( access_t *p_access, uint8_t *p, int i_size )
{
    memset( p, 0, i_size );
    return i_size;
}
static int AccessControl( access_t *p_access, int i_query, va_list args )
{
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = DEFAULT_PTS_DELAY * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
            return VLC_EGENERIC;

        default:
            msg_Err( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

int E_(OpenAccess)( vlc_object_t *p_this )
{
    access_t *p_access = (access_t*)p_this;

    /* Init p_access */
    p_access->pf_read = AccessRead;
    p_access->pf_block = NULL;
    p_access->pf_seek = NULL;
    p_access->pf_control = AccessControl;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = NULL;

    /* Force dummy demux plug-in */
    p_access->psz_demux = strdup( "vlc" );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Demux
 *****************************************************************************/
struct demux_sys_t
{
    /* The real command */
    int i_command;

    /* Used for the pause command */
    mtime_t expiration;
    
    /* The command to run */
    char* psz_command;
};
enum
{
    COMMAND_NOP  = 0,
    COMMAND_QUIT = 1,
    COMMAND_LOOP = 2,
    COMMAND_PAUSE= 3,
    COMMAND_RUN  = 4,
};

static int Demux( demux_t * );
static int DemuxControl( demux_t *, int, va_list );


/*****************************************************************************
 * OpenDemux: initialize the target, ie. parse the command
 *****************************************************************************/
int E_(OpenDemux) ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    char * psz_name = p_demux->psz_path;

    int i_len = strlen( psz_name );
    demux_sys_t *p_sys;
    int   i_arg;

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = DemuxControl;
    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );

    /* Check for a "vlc:nop" command */
    if( i_len == 3 && !strncasecmp( psz_name, "nop", 3 ) )
    {
        msg_Info( p_demux, "command `nop'" );
        p_sys->i_command = COMMAND_NOP;
        return VLC_SUCCESS;
    }

    /* Check for a "vlc:quit" command */
    if( i_len == 4 && !strncasecmp( psz_name, "quit", 4 ) )
    {
        msg_Info( p_demux, "command `quit'" );
        p_sys->i_command = COMMAND_QUIT;
        return VLC_SUCCESS;
    }

    /* Check for a "vlc:loop" command */
    if( i_len == 4 && !strncasecmp( psz_name, "loop", 4 ) )
    {
        msg_Info( p_demux, "command `loop'" );
        p_sys->i_command = COMMAND_LOOP;
        return VLC_SUCCESS;
    }

    /* Check for a "vlc:pause:***" command */
    if( i_len > 6 && !strncasecmp( psz_name, "pause:", 6 ) )
    {
        i_arg = atoi( psz_name + 6 );
        msg_Info( p_demux, "command `pause %i'", i_arg );
        p_sys->i_command = COMMAND_PAUSE;
        p_sys->expiration = mdate() + (mtime_t)i_arg * (mtime_t)1000000;
        return VLC_SUCCESS;
    }
    
    /* Check for a "vlc:run:***" command */
    if ( i_len > 4 && !strncasecmp( psz_name, "run:", 4 ) )
    {
       p_sys->psz_command = malloc( i_len - 4 );
       strcpy( p_sys->psz_command, psz_name + 4 );
       msg_Info( p_demux, "command `run program %s'", p_sys->psz_command );
       p_sys->i_command = COMMAND_RUN;
       return VLC_SUCCESS;
    } 

    msg_Err( p_demux, "unknown command `%s'", psz_name );

    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CloseDemux: initialize the target, ie. parse the command
 *****************************************************************************/
void E_(CloseDemux) ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;

    free( p_demux->p_sys );
}

/*****************************************************************************
 * Demux: do what the command says
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    playlist_t *p_playlist;
    vlc_bool_t b_eof = VLC_FALSE;

    p_playlist = vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST, FIND_PARENT );

    if( p_playlist == NULL )
    {
        msg_Err( p_demux, "we are not attached to a playlist" );
        return -1;
    }

    switch( p_sys->i_command )
    {
        case COMMAND_QUIT:
            b_eof = p_demux->p_vlc->b_die = VLC_TRUE;
            break;

        case COMMAND_LOOP:
            playlist_Goto( p_playlist, 0 );
            break;

        case COMMAND_PAUSE:
            if( mdate() >= p_sys->expiration )
                b_eof = VLC_TRUE;
            else
                msleep( 10000 );
            break;
        
        case COMMAND_RUN:
            var_SetString( p_playlist, "run-program-command", p_sys->psz_command );
            free( p_sys->psz_command );
            b_eof = VLC_TRUE;
            break;

        case COMMAND_NOP:
        default:
            b_eof = VLC_TRUE;
            break;       
    }

    vlc_object_release( p_playlist );
    return b_eof ? 0 : 1;
}

static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    return demux2_vaControlHelper( p_demux->s,
                                   0, 0, 0, 1,
                                   i_query, args );
}
