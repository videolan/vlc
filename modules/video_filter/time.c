/*****************************************************************************
 * time.c : time display video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
 * $Id: logo.c 8721 2004-09-17 10:21:00Z gbazin $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
 * filter_sys_t: time filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    int i_xoff, i_yoff;  /* offsets for the display string in the video window */
    char *psz_head;  /* text to put before the day/time */
};

#define MSG_TEXT N_("Timestamp Prefix")
#define MSG_LONGTEXT N_("Text (name) to display before the date and time")
#define POSX_TEXT N_("X coordinate of the timestamp")
#define POSX_LONGTEXT N_("Positive offset, from the left" )
#define POSY_TEXT N_("Y coordinate of the timestamp")
#define POSY_LONGTEXT N_("Positive offset, down from the top" )
#define TRANS_TEXT N_("Transparency of the timestamp")

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_capability( "sub filter", 0 );
    set_callbacks( CreateFilter, DestroyFilter );
    add_string( "text-message", NULL, NULL, MSG_TEXT, MSG_LONGTEXT, VLC_FALSE );
    add_integer( "timestamp-x", 0, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_FALSE );
    add_integer( "timestamp-y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_FALSE );
    set_description( _("Time display sub filter") );
    add_shortcut( "time" );
vlc_module_end();

/*****************************************************************************
 * CreateFilter: allocates logo video filter
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

    var_Create( p_this, "timestamp-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_filter, "timestamp-x", &val );
    p_sys->i_xoff = val.i_int;
    var_Create( p_this, "timestamp-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_filter, "timestamp-y", &val );
    p_sys->i_yoff = val.i_int;
    p_sys->psz_head = strdup(var_CreateGetString( p_this, "text-message" ));

    
    /* Misc init */
    p_filter->pf_sub_filter = Filter;

    return VLC_SUCCESS;
}
/*****************************************************************************
 * DestroyFilter: destroy logo video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys );
}

char *myCtime( time_t *t )
{
#ifdef HAVE_CTIME_R
    char tmp[27];
    ctime_r( t, tmp );
    return strdup( tmp );
#else
    return strdup(ctime(t));
#endif
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************
 * This function outputs subpictures at regular time intervals.
 ****************************************************************************/
static subpicture_t *Filter( filter_t *p_filter, mtime_t date )
{
    subpicture_t *p_spu;
    video_format_t fmt;
    time_t t;
    char *psz_time, *psz_string;
    filter_sys_t *p_sys;

    p_sys = p_filter->p_sys;
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
    t = time(NULL);
    
 
    psz_time = myCtime( &t );
    asprintf( &psz_string, "%s%s", p_sys->psz_head, psz_time );
    
    free( psz_time );
    p_spu->p_region->psz_text = psz_string;
    p_spu->i_start = date;
    p_spu->i_stop  = date + 1000000;
    p_spu->b_ephemer = VLC_TRUE;
    p_spu->b_absolute = VLC_FALSE;
    p_spu->i_x = p_sys->i_xoff;
    p_spu->i_y = p_sys->i_yoff;
           
    p_spu->i_flags = OSD_ALIGN_LEFT|OSD_ALIGN_TOP ;
    return p_spu;
}
