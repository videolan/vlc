/*****************************************************************************
 * motion.c: control VLC with laptop built-in motion sensors
 *****************************************************************************
 * Copyright (C) 2006 - 2007 the VideoLAN team
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Jérôme Decoodt <djc@videolan.org> (unimotion integration)
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

#include <math.h>
#include <unistd.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>

#ifdef __APPLE__
# include "TargetConditionals.h"
# if !TARGET_OS_IPHONE
#  define HAVE_MACOS_UNIMOTION
# endif
#endif

#ifdef HAVE_MACOS_UNIMOTION
# include "unimotion.h"
#endif

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    enum { NO_SENSOR, HDAPS_SENSOR, AMS_SENSOR, APPLESMC_SENSOR,
           UNIMOTION_SENSOR } sensor;
#ifdef HAVE_MACOS_UNIMOTION
    enum sms_hardware unimotion_hw;
#endif
    int i_calibrate;

    bool b_use_rotate;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

static void RunIntf( intf_thread_t *p_intf );
static int GetOrientation( intf_thread_t *p_intf );

#define USE_ROTATE_TEXT N_("Use the rotate video filter instead of transform")

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( N_("motion"))
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("motion control interface") )
    set_help( N_("Use HDAPS, AMS, APPLESMC or UNIMOTION motion sensors " \
                 "to rotate the video") )

    add_bool( "motion-use-rotate", false,
              USE_ROTATE_TEXT, USE_ROTATE_TEXT, false )

    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * OpenIntf: initialise interface
 *****************************************************************************/
int Open ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    FILE *f;
    int i_x = 0, i_y = 0;

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
            p_intf->p_sys->i_calibrate = fscanf( f, "(%d,%d)", &i_x, &i_y ) == 2 ? i_x: 0;
            fclose( f );
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
    else if( access( "/sys/devices/platform/applesmc.768/position", R_OK ) == 0 )
    {
        /* Apple SMC (newer macbooks) */
        /* Should be factorised with HDAPS */
        f = fopen( "/sys/devices/platform/applesmc.768/calibrate", "r" );
        if( f )
        {
            p_intf->p_sys->i_calibrate = fscanf( f, "(%d,%d)", &i_x, &i_y ) == 2 ? i_x: 0;
            fclose( f );
            p_intf->p_sys->sensor = APPLESMC_SENSOR;
        }
        else
        {
            p_intf->p_sys->sensor = NO_SENSOR;
        }
    }
#ifdef HAVE_MACOS_UNIMOTION
    else if( (p_intf->p_sys->unimotion_hw = detect_sms()) )
        p_intf->p_sys->sensor = UNIMOTION_SENSOR;
#endif
    else
    {
        /* No motion sensor support */
        p_intf->p_sys->sensor = NO_SENSOR;
    }

    p_intf->pf_run = RunIntf;

    p_intf->p_sys->b_use_rotate = var_InheritBool( p_intf, "motion-use-rotate" );

    msg_Dbg( p_intf, "Motion detection correctly loaded" );
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
#define FILTER_LENGTH 16
#define LOW_THRESHOLD 800
#define HIGH_THRESHOLD 1000
static void RunIntf( intf_thread_t *p_intf )
{
    int i_x, i_oldx = 0, i_sum = 0, i = 0;
    int p_oldx[FILTER_LENGTH];
    memset( p_oldx, 0, FILTER_LENGTH * sizeof( int ) );

    for( ;; )
    {
        const char *psz_filter, *psz_type;
        bool b_change = false;

        /* Wait a bit, get orientation, change filter if necessary */
#warning FIXME: check once (or less) per picture, not once per interval
        msleep( INTF_IDLE_SLEEP );

        int canc = vlc_savecancel();
        i_x = GetOrientation( p_intf );
        i_sum += i_x - p_oldx[i];
        p_oldx[i++] = i_x;
        if( i == FILTER_LENGTH ) i = 0;
        i_x = i_sum / FILTER_LENGTH;

        if( p_intf->p_sys->b_use_rotate )
        {
            if( i_oldx != i_x )
            {
                /* TODO: cache object pointer */
                vlc_object_t *p_obj =
                vlc_object_find_name( p_intf->p_libvlc, "rotate" );
                if( p_obj )
                {
                    var_SetInteger( p_obj, "rotate-deciangle",
                            ((3600+i_x/2)%3600) );
                    i_oldx = i_x;
                    vlc_object_release( p_obj );
                }
            }
            goto loop;
        }

        if( i_x < -HIGH_THRESHOLD && i_oldx > -LOW_THRESHOLD )
        {
            b_change = true;
            psz_filter = "transform";
            psz_type = "270";
        }
        else if( ( i_x > -LOW_THRESHOLD && i_oldx < -HIGH_THRESHOLD )
                 || ( i_x < LOW_THRESHOLD && i_oldx > HIGH_THRESHOLD ) )
        {
            b_change = true;
            psz_filter = "";
            psz_type = "";
        }
        else if( i_x > HIGH_THRESHOLD && i_oldx < LOW_THRESHOLD )
        {
            b_change = true;
            psz_filter = "transform";
            psz_type = "90";
        }

        if( b_change )
        {
#warning FIXME: refactor this plugin as a video filter!
            input_thread_t *p_input;

            p_input = playlist_CurrentInput( pl_Get( p_intf ) );
            if( p_input )
            {
                vout_thread_t *p_vout;

                p_vout = input_GetVout( p_input );
                if( p_vout )
                {
#warning FIXME: do not override the permanent configuration!
#warning FIXME: transform-type does not exist anymore
                    config_PutPsz( p_vout, "transform-type", psz_type );
                    var_SetString( p_vout, "video-filter", psz_filter );
                    vlc_object_release( p_vout );
                }
                vlc_object_release( p_input );
                i_oldx = i_x;
            }
        }
loop:
        vlc_restorecancel( canc );
    }
}
#undef FILTER_LENGTH
#undef LOW_THRESHOLD
#undef HIGH_THRESHOLD

/*****************************************************************************
 * GetOrientation: get laptop orientation, range -1800 / +1800
 *****************************************************************************/
static int GetOrientation( intf_thread_t *p_intf )
{
    FILE *f;
    int i_x = 0, i_y = 0, i_z = 0;
    int i_ret;

    switch( p_intf->p_sys->sensor )
    {
    case HDAPS_SENSOR:
        f = fopen( "/sys/devices/platform/hdaps/position", "r" );
        if( !f )
        {
            return 0;
        }

        i_ret = fscanf( f, "(%d,%d)", &i_x, &i_y );
        fclose( f );

        if( i_ret < 2 )
            return 0;
        else
            return ( i_x - p_intf->p_sys->i_calibrate ) * 10;

    case AMS_SENSOR:
        f = fopen( "/sys/devices/ams/x", "r" );
        if( !f )
        {
            return 0;
        }

        i_ret = fscanf( f, "%d", &i_x);
        fclose( f );

        if( i_ret < 1 )
            return 0;
        else
            return - i_x * 30; /* FIXME: arbitrary */

    case APPLESMC_SENSOR:
        f = fopen( "/sys/devices/platform/applesmc.768/position", "r" );
        if( !f )
        {
            return 0;
        }

        i_ret = fscanf( f, "(%d,%d,%d)", &i_x, &i_y, &i_z );
        fclose( f );

        if( i_ret < 3 )
            return 0;
        else
            return ( i_x - p_intf->p_sys->i_calibrate ) * 10;

#ifdef HAVE_MACOS_UNIMOTION
    case UNIMOTION_SENSOR:
        if( read_sms_raw( p_intf->p_sys->unimotion_hw, &i_x, &i_y, &i_z ) )
        {
            double d_norm = sqrt( i_x*i_x+i_z*i_z );
            if( d_norm < 100 )
                return 0;
            double d_x = i_x / d_norm;
            if( i_z > 0 )
                return -asin(d_x)*3600/3.141;
            else
                return 3600 + asin(d_x)*3600/3.141;
        }
        else
            return 0;
#endif
    case NO_SENSOR:
    default:
        return 0;
    }
}

