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

/*****************************************************************************
 * filter_sys_t: marquee filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    int i_xoff, i_yoff;  /* offsets for the display string in the video window */
    char *psz_marquee;    /* marquee string */
    
    time_t last_time;
};

#define MSG_TEXT N_("Marquee text")
#define MSG_LONGTEXT N_("Marquee text to display")
#define POSX_TEXT N_("X offset, from left")
#define POSX_LONGTEXT N_("X offset, from the left screen edge" )
#define POSY_TEXT N_("Y offset, from the top")
#define POSY_LONGTEXT N_("Y offset, down from the top" )

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_capability( "sub filter", 0 );
    set_callbacks( CreateFilter, DestroyFilter );
    add_string( "marquee", "Marquee", NULL, MSG_TEXT, MSG_LONGTEXT, VLC_FALSE );
    add_integer( "marq-x", 0, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_FALSE );
    add_integer( "marq-y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_FALSE );
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
    vlc_value_t val;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    /* hook to the playlist */ 
    vlc_object_t *p_pl =
          vlc_object_find( p_this, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_pl )
    {
        return VLC_ENOOBJ;
    }


    var_Create( p_this, "marq-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_filter, "marq-x", &val );
    p_sys->i_xoff = val.i_int;
    var_Create( p_this, "marq-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_filter, "marq-y", &val );
    p_sys->i_yoff = val.i_int;

    /*  Create the playlist marquee variables, free up the data structure */   
    var_Create( p_pl, "marquee", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_pl, "marq-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_pl, "marq-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    vlc_object_release( p_pl );
 
    /* Misc init */
    p_filter->pf_sub_filter = Filter;
    p_sys->last_time = ((time_t)-1);

    return VLC_SUCCESS;
}
/*****************************************************************************
 * DestroyFilter: destroy marquee video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->psz_marquee ) free( p_sys->psz_marquee );
    free( p_sys );
 /* Delete the marquee variables from playlist */
    vlc_object_t *p_pl =
        vlc_object_find( p_this, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    var_Destroy(p_pl,"marquee");
    var_Destroy(p_pl,"marq-x");
    var_Destroy(p_pl,"marq_y");
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
    vlc_value_t val;

    if( p_sys->last_time == time( NULL ) ) return NULL;

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

    /* hook to the playlist.  Can this just be done once somewhere? */ 
    vlc_object_t *p_pl =
          vlc_object_find( p_filter, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    var_Get(p_pl, "marquee", &val);
    p_sys->psz_marquee = val.psz_string;  
    var_Get(p_pl, "marq-x", &val);
    p_sys->i_xoff = val.i_int;  
    var_Get(p_pl, "marq-y", &val);
    p_sys->i_yoff = val.i_int;  
    vlc_object_release( p_pl );   
    /* Does vlc_object_release have to be done every time? */
    
    p_spu->p_region->psz_text = strdup(p_sys->psz_marquee); 
    p_spu->i_start = date;
    p_spu->i_stop  = 0;
    p_spu->b_ephemer = VLC_TRUE;
    p_spu->b_absolute = VLC_FALSE;
    p_spu->i_x = p_sys->i_xoff;
    p_spu->i_y = p_sys->i_yoff;

    p_spu->i_flags = OSD_ALIGN_LEFT|OSD_ALIGN_TOP ;
    return p_spu;
}
