/*****************************************************************************
 * fake.c : Fake video input for VLC
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Author: Christophe Massiot <massiot@via.ecp.fr>
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
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_image.h>

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for fake streams. This " \
    "value should be set in milliseconds." )
#define FPS_TEXT N_("Framerate")
#define FPS_LONGTEXT N_( \
    "Number of frames per second (eg. 24, 25, 29.97, 30).")
#define ID_TEXT N_("ID")
#define ID_LONGTEXT N_( \
    "Set the ID of the fake elementary stream for use in " \
    "#duplicate{} constructs (default 0).")
#define DURATION_TEXT N_("Duration in ms")
#define DURATION_LONGTEXT N_( \
    "Duration of the fake streaming before faking an " \
    "end-of-file (default is -1 meaning that the stream is unlimited when " \
    "fake is forced, or lasts for 10 seconds otherwise. 0, means that the " \
    "stream is unlimited).")

vlc_module_begin ()
    set_shortname( N_("Fake") )
    set_description( N_("Fake input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_integer( "fake-caching", DEFAULT_PTS_DELAY / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, true )
    add_float( "fake-fps", 25.0, NULL, FPS_TEXT, FPS_LONGTEXT, true )
    add_integer( "fake-id", 0, NULL, ID_TEXT, ID_LONGTEXT, true )
    add_integer( "fake-duration", -1, NULL, DURATION_TEXT, DURATION_LONGTEXT,
                 true )

    add_shortcut( "fake" )
    set_capability( "access_demux", 10 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

struct demux_sys_t
{
    float f_fps;
    mtime_t i_last_pts, i_duration, i_first_pts, i_end_pts, i_pause_pts;

    es_out_id_t  *p_es_video;
};

/*****************************************************************************
 * Open: opens fake device
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    es_format_t fmt;

    if( *p_demux->psz_access != '\0' )
    {
        /* if an access is provided, then it has to be "fake" */
        if( strcmp( p_demux->psz_access, "fake" ) )
            return VLC_EGENERIC;

        msg_Dbg( p_demux, "fake:// access_demux detected" );
    }
    else
    {
       /**
        * access is not provided,
        * then let's see if path could be an image
        **/

        if( !p_demux->psz_path || !*p_demux->psz_path )
            return VLC_EGENERIC;

        vlc_fourcc_t i_codec = image_Ext2Fourcc( p_demux->psz_path );
        if( !i_codec )
            return VLC_EGENERIC;
        msg_Dbg( p_demux, "still image detected with codec format %4.4s",
                 (const char*)&i_codec );
    }

    if( p_demux->psz_path && *p_demux->psz_path )
    {
        /* set up fake-file on the fly */
        var_Create( p_demux->p_parent, "fake-file", VLC_VAR_STRING );
        var_SetString( p_demux->p_parent, "fake-file", p_demux->psz_path );
    }

    /* Set up p_demux */
    DEMUX_INIT_COMMON(); p_sys = p_demux->p_sys;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    p_sys->i_duration =
        (mtime_t)var_CreateGetInteger( p_demux, "fake-duration" ) * 1000;
    if( p_sys->i_duration < 0 )
    {
        if( !strcmp( p_demux->psz_access, "fake" ) )
            p_sys->i_duration = 0;
        else
            p_sys->i_duration = 10000*1000;
    }
    p_sys->f_fps = var_CreateGetFloat( p_demux, "fake-fps" );

    /* Declare the elementary stream */
    es_format_Init( &fmt, VIDEO_ES, VLC_FOURCC('f','a','k','e') );
    fmt.i_id = var_CreateGetInteger( p_demux, "fake-id" );
    p_sys->p_es_video = es_out_Add( p_demux->out, &fmt );

    /* Update default_pts to a suitable value for access */
    var_Create( p_demux, "fake-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close device, free resources
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool *pb, b;
    int64_t    *pi64, i64;
    double     *pf, f;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            pb = (bool *)va_arg( args, bool * );
            *pb = true;
            return VLC_SUCCESS;

        case DEMUX_SET_PAUSE_STATE:
            b = (bool)va_arg( args, int );
            if ( b )
            {
                p_sys->i_pause_pts = mdate();
            }
            else if ( p_sys->i_pause_pts )
            {
                mtime_t i_pause_duration = mdate() - p_sys->i_pause_pts;
                p_sys->i_first_pts += i_pause_duration;
                p_sys->i_last_pts += i_pause_duration;
                if ( p_sys->i_duration )
                    p_sys->i_end_pts += i_pause_duration;
                p_sys->i_pause_pts = 0;
            }
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t *)va_arg( args, int64_t * );
            *pi64 = (int64_t)var_GetInteger( p_demux, "fake-caching" ) * 1000;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            if( p_sys->i_duration <= 0 )
                return VLC_EGENERIC;
            pf = (double*)va_arg( args, double* );
            *pf = (double)( p_sys->i_last_pts - p_sys->i_first_pts )
                            / (double)(p_sys->i_duration);
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            if( p_sys->i_duration <= 0 )
                return VLC_EGENERIC;
            f = (double)va_arg( args, double );
            i64 = f * (double)p_sys->i_duration;
            p_sys->i_first_pts = p_sys->i_last_pts - i64;
            p_sys->i_end_pts = p_sys->i_first_pts + p_sys->i_duration;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t *)va_arg( args, int64_t * );
            *pi64 = p_sys->i_last_pts - p_sys->i_first_pts;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            if( p_sys->i_duration <= 0 )
                return VLC_EGENERIC;
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_duration;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            p_sys->i_first_pts = p_sys->i_last_pts - i64;
            p_sys->i_end_pts = p_sys->i_first_pts + p_sys->i_duration;
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if ( !p_sys->i_last_pts )
    {
        p_sys->i_last_pts = p_sys->i_first_pts = mdate();
        if ( p_sys->i_duration )
            p_sys->i_end_pts = p_sys->i_first_pts + p_sys->i_duration;
    }
    else
    {
        p_sys->i_last_pts += (mtime_t)(1000000.0 / p_sys->f_fps);
        if ( p_sys->i_duration && p_sys->i_last_pts > p_sys->i_end_pts )
            return 0;
        mwait( p_sys->i_last_pts );
    }

    block_t *p_block = block_New( p_demux, 1 );
    p_block->i_flags |= BLOCK_FLAG_TYPE_I;
    p_block->i_dts = p_block->i_pts = p_sys->i_last_pts;

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->p_es_video, p_block );

    return 1;
}
