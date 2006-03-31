/*****************************************************************************
 * screen.c: Screen capture module.
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "screen.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for screen capture. "\
    "This value should be set in milliseconds." )
#define FPS_TEXT N_("Frame rate")
#define FPS_LONGTEXT N_( \
    "Desired frame rate for the capture." )

#ifdef WIN32
#define FRAGS_TEXT N_("Capture fragment size")
#define FRAGS_LONGTEXT N_( \
    "Optimize the capture by fragmenting the screen in chunks " \
    "of predefined height (16 might be a good value, and 0 means disabled)." )
#endif

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#ifdef WIN32
#   define SCREEN_FPS 1
#else
#   define SCREEN_FPS 5
#endif

vlc_module_begin();
    set_description( _("Screen Input") );
    set_shortname( N_("Screen" ));
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_integer( "screen-caching", DEFAULT_PTS_DELAY / 1000, NULL,
        CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_float( "screen-fps", SCREEN_FPS, 0, FPS_TEXT, FPS_LONGTEXT, VLC_TRUE );

#ifdef WIN32
    add_integer( "screen-fragment-size", 0, NULL, FRAGS_TEXT,
        FRAGS_LONGTEXT, VLC_TRUE );
#endif

    set_capability( "access_demux", 0 );
    add_shortcut( "screen" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control( demux_t *, int, va_list );
static int Demux  ( demux_t * );

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    vlc_value_t val;

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    /* Update default_pts to a suitable value for screen access */
    var_Create( p_demux, "screen-caching", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    var_Create( p_demux, "screen-fps", VLC_VAR_FLOAT|VLC_VAR_DOINHERIT );
    var_Get( p_demux, "screen-fps", &val );
    p_sys->f_fps = val.f_float;
    p_sys->i_incr = 1000000 / val.f_float;
    p_sys->i_next_date = 0;

    if( screen_InitCapture( p_demux ) != VLC_SUCCESS )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "screen width: %i, height: %i, depth: %i",
             p_sys->fmt.video.i_width, p_sys->fmt.video.i_height,
             p_sys->fmt.video.i_bits_per_pixel );

    p_sys->es = es_out_Add( p_demux->out, &p_sys->fmt );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    screen_CloseCapture( p_demux );
    free( p_sys );
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block;

    if( !p_sys->i_next_date ) p_sys->i_next_date = mdate();

    /* Frame skipping if necessary */
    while( mdate() >= p_sys->i_next_date + p_sys->i_incr )
        p_sys->i_next_date += p_sys->i_incr;

    mwait( p_sys->i_next_date );
    p_block = screen_Capture( p_demux );
    if( !p_block )
    {
        p_sys->i_next_date += p_sys->i_incr;
        return 1;
    }

    p_block->i_dts = p_block->i_pts = p_sys->i_next_date;

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->es, p_block );

    p_sys->i_next_date += p_sys->i_incr;

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    vlc_bool_t *pb;
    int64_t *pi64;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
            /* TODO */
            pb = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
            *pb = VLC_FALSE;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = (int64_t)var_GetInteger( p_demux, "screen-caching" ) *1000;
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }
}
