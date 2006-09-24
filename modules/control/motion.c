/*****************************************************************************
 * motion.c: control VLC with laptop built-in motion sensors
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    enum { NO_SENSOR, HDAPS_SENSOR, AMS_SENSOR } sensor;

    int i_last_x, i_calibrate;
    int i_threshold;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

static void RunIntf( intf_thread_t *p_intf );
static int GetOrientation( intf_thread_t *p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_shortname( _("motion"));
    set_category( CAT_INTERFACE );
    set_description( _("motion control interface") );

    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * OpenIntf: initialise interface
 *****************************************************************************/
int Open ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    FILE *f;
    int i_x, i_y;

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return VLC_ENOMEM;
    }

    if( access( "/sys/devices/platform/hdaps/position", R_OK ) == 0 )
    {
        /* IBM HDAPS support */
        f = fopen( "/sys/devices/platform/hdaps/calibrate", "r" );
        if( f )
        {
            i_x = i_y = 0;
            fscanf( f, "(%d,%d)", &i_x, &i_y );
            fclose( f );
            p_intf->p_sys->i_calibrate = i_x;
            p_intf->p_sys->sensor = HDAPS_SENSOR;
        }
        else
        {
            p_intf->p_sys->sensor = NO_SENSOR;
        }
    }
    else if( access( "/sys/devices/ams/x", R_OK ) == 0 )
    {
        /* Apple Motion Sensor support */
        p_intf->p_sys->sensor = AMS_SENSOR;
    }
    else
    {
        /* No motion sensor support */
        p_intf->p_sys->sensor = NO_SENSOR;
    }

    p_intf->pf_run = RunIntf;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    free( p_intf->p_sys );
}

/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
static void RunIntf( intf_thread_t *p_intf )
{
    int i_x, i_oldx = 0;

    while( !intf_ShouldDie( p_intf ) )
    {
#define LOW_THRESHOLD 80
#define HIGH_THRESHOLD 100
        vout_thread_t *p_vout;
        char *psz_filter, *psz_type;
        vlc_bool_t b_change = VLC_FALSE;

        /* Wait a bit, get orientation, change filter if necessary */
        msleep( INTF_IDLE_SLEEP );

        i_x = GetOrientation( p_intf );

        if( i_x < -HIGH_THRESHOLD && i_oldx > -LOW_THRESHOLD )
        {
            b_change = VLC_TRUE;
            psz_filter = "transform";
            psz_type = "270";
        }
        else if( ( i_x > -LOW_THRESHOLD && i_oldx < -HIGH_THRESHOLD )
                 || ( i_x < LOW_THRESHOLD && i_oldx > HIGH_THRESHOLD ) )
        {
            b_change = VLC_TRUE;
            psz_filter = "";
            psz_type = "";
        }
        else if( i_x > HIGH_THRESHOLD && i_oldx < LOW_THRESHOLD )
        {
            b_change = VLC_TRUE;
            psz_filter = "transform";
            psz_type = "90";
        }

        if( !b_change )
        {
            continue;
        }

        p_vout = (vout_thread_t *)
            vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
        if( !p_vout )
        {
            continue;
        }

        config_PutPsz( p_vout, "transform-type", psz_type );
        var_SetString( p_vout, "vout-filter", psz_filter );
        vlc_object_release( p_vout );

        i_oldx = i_x;
    }
}

/*****************************************************************************
 * GetOrientation: get laptop orientation, range -180 / +180
 *****************************************************************************/
static int GetOrientation( intf_thread_t *p_intf )
{
    FILE *f;
    int i_x, i_y;

    switch( p_intf->p_sys->sensor )
    {
    case HDAPS_SENSOR:
        f = fopen( "/sys/devices/platform/hdaps/position", "r" );
        if( !f )
        {
            return 0;
        }

        i_x = i_y = 0;
        fscanf( f, "(%d,%d)", &i_x, &i_y );
        fclose( f );

        return i_x - p_intf->p_sys->i_calibrate;

    case AMS_SENSOR:
        f = fopen( "/sys/devices/ams/x", "r" );
        if( !f )
        {
            return 0;
        }

        fscanf( f, "%d", &i_x);
        fclose( f );

        return - i_x * 3; /* FIXME: arbitrary */

    default:
        return 0;
    }
}

