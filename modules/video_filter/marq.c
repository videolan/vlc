/*****************************************************************************
 * marq.c : marquee display video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
 * $Id: time.c 8751 2004-09-20 21:51:41Z gbazin $
 *
 * Authors: Mark Moriarty
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "vlc_filter.h"
#include "vlc_block.h"
#include "osd.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );
static subpicture_t *Filter( filter_t *, mtime_t );


static int MarqueeCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data );

/*****************************************************************************
 * filter_sys_t: marquee filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    int i_xoff, i_yoff;  /* offsets for the display string in the video window */
    int i_timeout;

    char *psz_marquee;    /* marquee string */

    time_t last_time;

    vlc_bool_t b_need_update;
};

#define MSG_TEXT N_("Marquee text")
#define MSG_LONGTEXT N_("Marquee text to display")
#define POSX_TEXT N_("X offset, from left")
#define POSX_LONGTEXT N_("X offset, from the left screen edge" )
#define POSY_TEXT N_("Y offset, from the top")
#define POSY_LONGTEXT N_("Y offset, down from the top" )
#define TIMEOUT_TEXT N_("Marquee timeout")
#define TIMEOUT_LONGTEXT N_("Defines the time the marquee must remain " \
                            "displayed, in milliseconds. Default value is " \
                            "0 (remain forever).")

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_capability( "sub filter", 0 );
    set_callbacks( CreateFilter, DestroyFilter );
    add_string( "marq-marquee", "Marquee", NULL, MSG_TEXT, MSG_LONGTEXT, VLC_FALSE );
    add_integer( "marq-x", 0, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_FALSE );
    add_integer( "marq-y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_FALSE );
    add_integer( "marq-timeout", 0, NULL, TIMEOUT_TEXT, TIMEOUT_LONGTEXT,
                 VLC_FALSE );
    set_description( _("Marquee display sub filter") );
    add_shortcut( "marq" );
vlc_module_end();

/*****************************************************************************
 * CreateFilter: allocates marquee video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    vlc_object_t *p_pl;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    /* hook to the playlist */
    p_pl = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_pl )
    {
        return VLC_ENOOBJ;
    }
/* p_access->p_libvlc p_demux->p_libvlc */

    p_sys->i_xoff = var_CreateGetInteger( p_pl , "marq-x" );
    p_sys->i_yoff = var_CreateGetInteger( p_pl , "marq-y" );
    p_sys->i_timeout = var_CreateGetInteger( p_pl , "marq-timeout" );
    p_sys->psz_marquee =  var_CreateGetString( p_pl, "marq-marquee" );

    var_AddCallback( p_pl, "marq-x", MarqueeCallback, p_sys );
    var_AddCallback( p_pl, "marq-y", MarqueeCallback, p_sys );
    var_AddCallback( p_pl, "marq-marquee", MarqueeCallback, p_sys );
    var_AddCallback( p_pl, "marq-timeout", MarqueeCallback, p_sys );

    vlc_object_release( p_pl );

    /* Misc init */
    p_filter->pf_sub_filter = Filter;
    p_sys->last_time = ((time_t)-1);
    p_sys->b_need_update = VLC_TRUE;

    return VLC_SUCCESS;
}
/*****************************************************************************
 * DestroyFilter: destroy marquee video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_object_t *p_pl;

    if( p_sys->psz_marquee ) free( p_sys->psz_marquee );
    free( p_sys );

    /* Delete the marquee variables from playlist */
    p_pl = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_pl )
    {
        return;
    }
    var_Destroy( p_pl , "marq-marquee" );
    var_Destroy( p_pl , "marq-x" );
    var_Destroy( p_pl , "marq-y" );
    var_Destroy( p_pl , "marq-timeout" );
    vlc_object_release( p_pl );
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************
 * This function outputs subpictures at regular time intervals.
 ****************************************************************************/
static subpicture_t *Filter( filter_t *p_filter, mtime_t date )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    subpicture_t *p_spu;
    video_format_t fmt;
    time_t t;

    if( p_sys->last_time == time( NULL ) )
    {
        return NULL;
    }

    if( p_sys->b_need_update == VLC_FALSE )
    {
        return NULL;
    }

    p_spu = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_spu ) return NULL;

    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = 0;
    fmt.i_y_offset = 0;

    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt );
    if( !p_spu->p_region )
    {
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        return NULL;
    }

    t = p_sys->last_time = time( NULL );

    p_spu->p_region->psz_text = strdup(p_sys->psz_marquee);
    p_spu->i_start = date;
    p_spu->i_stop  = p_sys->i_timeout == 0 ? 0 : date + p_sys->i_timeout * 1000;
    p_spu->b_ephemer = VLC_TRUE;
    p_spu->b_absolute = VLC_FALSE;
    p_spu->i_x = p_sys->i_xoff;
    p_spu->i_y = p_sys->i_yoff;

    p_spu->i_flags = OSD_ALIGN_LEFT|OSD_ALIGN_TOP ;

    p_sys->b_need_update = VLC_FALSE;
    return p_spu;
}

/**********************************************************************
 * Callback to update params on the fly
 **********************************************************************/
static int MarqueeCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    filter_sys_t *p_sys = (filter_sys_t *) p_data;

    if( !strncmp( psz_var, "marq-marquee", 7 ) )
    {
        if( p_sys->psz_marquee ) free( p_sys->psz_marquee );
        p_sys->psz_marquee = strdup( newval.psz_string );
    }
    else if ( !strncmp( psz_var, "marq-x", 6 ) )
    {
        p_sys->i_xoff = newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-y", 6 ) )
    {
        p_sys->i_yoff = newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-timeout", 12 ) )
    {
        p_sys->i_timeout = newval.i_int;
    }
    p_sys->b_need_update = VLC_TRUE;
    return VLC_SUCCESS;
}
