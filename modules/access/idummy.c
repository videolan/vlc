/*****************************************************************************
 * idummy.c: dummy input plugin, to manage "vlc://" special options
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_demux.h>
#include <vlc_charset.h>

static int OpenDemux( vlc_object_t * );
static void CloseDemux( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Dummy") )
    set_description( N_("Dummy input") )
    set_capability( "access_demux", 0 )
    set_callbacks( OpenDemux, CloseDemux )
    add_shortcut( "dummy", "vlc" )
vlc_module_end ()

static int DemuxControl( demux_t *, int, va_list );

static int DemuxNoOp( demux_t *demux )
{
    (void) demux;
    return 0;
}

static int DemuxHold( demux_t *demux )
{
    (void) demux;
    msleep( 10000 ); /* FIXME!!! */
    return 1;
}

struct demux_sys_t
{
    mtime_t end;
    mtime_t length;
};

static int DemuxPause( demux_t *demux )
{
    demux_sys_t *p_sys = demux->p_sys;
    mtime_t now = mdate();

    if( now >= p_sys->end )
        return 0;

    msleep( 10000 ); /* FIXME!!! */
    return 1;
}

static int ControlPause( demux_t *demux, int query, va_list args )
{
    demux_sys_t *p_sys = demux->p_sys;

    switch( query )
    {
        case DEMUX_GET_POSITION:
        {
            double *ppos = va_arg( args, double * );
            double pos;
            mtime_t now = mdate();

            pos = 1. + ((double)(now - p_sys->end) / (double)p_sys->length);
            *ppos = (pos <= 1.) ? pos : 1.;
            break;
        }

        case DEMUX_SET_POSITION:
        {
            double pos = va_arg( args, double );
            mtime_t now = mdate();

            p_sys->end = now + (p_sys->length * (1. - pos));
            break;
        }

        case DEMUX_GET_LENGTH:
        {
            mtime_t *plen = va_arg( args, mtime_t * );
            *plen = p_sys->length;
            break;
        }

        case DEMUX_GET_TIME:
        {
            mtime_t *ppos = va_arg( args, mtime_t * );
            *ppos = mdate() + p_sys->length - p_sys->end;
            break;
        }

        case DEMUX_SET_TIME:
        {
            mtime_t pos = va_arg( args, mtime_t );
            p_sys->end = mdate() + p_sys->length - pos;
            break;
        }

        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = true;
            break;

        default:
            return DemuxControl( demux, query, args );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * OpenDemux: initialize the target, ie. parse the command
 *****************************************************************************/
static int OpenDemux( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    char * psz_name = p_demux->psz_location;

    p_demux->p_sys = NULL;

    /* Check for a "vlc://nop" command */
    if( !strcasecmp( psz_name, "nop" ) )
    {
nop:
        msg_Info( p_demux, "command `nop'" );
        p_demux->pf_demux = DemuxNoOp;
        p_demux->pf_control = DemuxControl;
        return VLC_SUCCESS;
    }

    /* Check for a "vlc://quit" command */
    if( !strcasecmp( psz_name, "quit" ) )
    {
        msg_Info( p_demux, "command `quit'" );
        p_demux->pf_demux = DemuxNoOp;
        p_demux->pf_control = DemuxControl;
        libvlc_Quit( p_demux->p_libvlc );
        return VLC_SUCCESS;
    }

    if( !strcasecmp( psz_name, "pause" ) )
    {
        msg_Info( p_demux, "command `pause'" );

        p_demux->pf_demux = DemuxHold;
        p_demux->pf_control = DemuxControl;
        return VLC_SUCCESS;
    }

    /* Check for a "vlc://pause:***" command */
    if( !strncasecmp( psz_name, "pause:", 6 ) )
    {
        double f = us_atof( psz_name + 6 );
        mtime_t length = f * CLOCK_FREQ;

        msg_Info( p_demux, "command `pause %f'", f );
        if( length == 0 )
            goto nop; /* avoid division by zero */

        demux_sys_t *p_sys = malloc( sizeof( *p_sys ) );
        if( p_sys == NULL )
            return VLC_ENOMEM;

        p_sys->end = mdate() + length;
        p_sys->length = length;

        p_demux->p_sys = p_sys;
        p_demux->pf_demux = DemuxPause;
        p_demux->pf_control = ControlPause;
        return VLC_SUCCESS;
    }
 
    msg_Err( p_demux, "unknown command `%s'", psz_name );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CloseDemux: initialize the target, ie. parse the command
 *****************************************************************************/
static void CloseDemux( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;

    free( p_demux->p_sys );
}

static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    (void)p_demux; (void)i_query; (void)args;
    switch( i_query )
    {
    case DEMUX_GET_PTS_DELAY:
    {
        int64_t *pi_pts_delay = va_arg( args, int64_t * );
        *pi_pts_delay = DEFAULT_PTS_DELAY;
        return VLC_SUCCESS;
    }
    default:
        return VLC_EGENERIC;
    }
}
