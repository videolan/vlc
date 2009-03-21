/*****************************************************************************
 * input_dummy.c: dummy input plugin, to manage "vlc://" special options
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_access.h>
#include <vlc_demux.h>

#include "dummy.h"

/*****************************************************************************
 * Access functions.
 *****************************************************************************/
static ssize_t AccessRead( access_t *p_access, uint8_t *p, size_t i_size )
{
    VLC_UNUSED(p_access);
    memset( p, 0, i_size );
    return i_size;
}
static int AccessControl( access_t *p_access, int i_query, va_list args )
{
    bool        *pb_bool;
    int64_t     *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = DEFAULT_PTS_DELAY * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_GET_META:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
            return VLC_EGENERIC;

        default:
            msg_Err( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

int OpenAccess( vlc_object_t *p_this )
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
    p_access->info.b_eof = false;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = NULL;

    /* Force dummy demux plug-in */
    free( p_access->psz_demux );
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
};
enum
{
    COMMAND_NOP  = 0,
    COMMAND_QUIT = 1,
    COMMAND_PAUSE= 3,
};

static int Demux( demux_t * );
static int DemuxControl( demux_t *, int, va_list );


/*****************************************************************************
 * OpenDemux: initialize the target, ie. parse the command
 *****************************************************************************/
int OpenDemux ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    char * psz_name = p_demux->psz_path;

    int i_len = strlen( psz_name );
    demux_sys_t *p_sys;
    int   i_arg;

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = DemuxControl;
    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );

    /* Check for a "vlc://nop" command */
    if( i_len == 3 && !strncasecmp( psz_name, "nop", 3 ) )
    {
        msg_Info( p_demux, "command `nop'" );
        p_sys->i_command = COMMAND_NOP;
        return VLC_SUCCESS;
    }

    /* Check for a "vlc://quit" command */
    if( i_len == 4 && !strncasecmp( psz_name, "quit", 4 ) )
    {
        msg_Info( p_demux, "command `quit'" );
        p_sys->i_command = COMMAND_QUIT;
        return VLC_SUCCESS;
    }

    /* Check for a "vlc://pause:***" command */
    if( i_len > 6 && !strncasecmp( psz_name, "pause:", 6 ) )
    {
        i_arg = atoi( psz_name + 6 );
        msg_Info( p_demux, "command `pause %i'", i_arg );
        p_sys->i_command = COMMAND_PAUSE;
        p_sys->expiration = mdate() + (mtime_t)i_arg * (mtime_t)1000000;
        return VLC_SUCCESS;
    }
 
    msg_Err( p_demux, "unknown command `%s'", psz_name );

    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CloseDemux: initialize the target, ie. parse the command
 *****************************************************************************/
void CloseDemux ( vlc_object_t *p_this )
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
    bool b_eof = false;

    switch( p_sys->i_command )
    {
        case COMMAND_QUIT:
            b_eof = true;
            libvlc_Quit( p_demux->p_libvlc );
            break;

        case COMMAND_PAUSE:
            if( mdate() >= p_sys->expiration )
                b_eof = true;
            else
                msleep( 10000 );
            break;
 
        case COMMAND_NOP:
        default:
            b_eof = true;
            break;
    }

    return b_eof ? 0 : 1;
}

static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    return demux_vaControlHelper( p_demux->s,
                                   0, 0, 0, 1,
                                   i_query, args );
}
