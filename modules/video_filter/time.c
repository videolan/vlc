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
#include "filter_common.h"
#include "vlc_block.h"

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
    picture_t *p_pic;

    int i_width, i_height;
    int posx, posy;

    mtime_t i_last_date;
    filter_t *p_text;
};


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_capability( "sub filter", 0 );
    set_callbacks( CreateFilter, DestroyFilter );
    set_description( _("Logo sub filter") );
    add_shortcut( "logo" );
vlc_module_end();

/*****************************************************************************
 * CreateFilter: allocates logo video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

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
    filter_sys_t *p_sys = p_filter->p_sys;
    subpicture_t *p_spu;
    subpicture_region_t *p_region;
    video_format_t fmt;
    time_t t;
    p_spu = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_spu ) return NULL;
    
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    
    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt );
    if( !p_spu->p_region )
    {
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        return NULL;
    }
    t = time(NULL);
    p_spu->p_region->psz_text = myCtime(&t);
    p_spu->i_start = date;
    p_spu->i_stop  = date + 1000000;
    p_spu->b_ephemer = VLC_TRUE;
    p_spu->b_absolute = VLC_FALSE;
    p_spu->i_x = 10;
    p_spu->i_y = 10;
    p_spu->i_flags = 0;
    return p_spu;
}
