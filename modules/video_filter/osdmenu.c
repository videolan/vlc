/*****************************************************************************
 * osdmenu.c: osd filter module
 *****************************************************************************
 * Copyright (C) 2004-2005 M2X
 * $Id: osdmenu.c 11131 2005-05-23 11:04:07Z hartman $
 *
 * Authors: Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implid warranty of
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
#include <stdlib.h>
#include <string.h>
#include <vlc/vlc.h>
#include <vlc/input.h>

#include <vlc_filter.h>
#include <vlc_video.h>

#include <vlc_osd.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* FIXME: Future extension make the definition file in XML format. */
#define OSD_FILE_TEXT N_("OSD menu configuration file")
#define OSD_FILE_LONGTEXT N_( \
    "An OSD menu configuration file that menu actions with button images" )

#define OSD_PATH_TEXT N_("Path to OSD menu images")
#define OSD_PATH_LONGTEXT N_( \
    "Specify another path to the OSD menu images. This will override the path as defined in the " \
    "OSD configuration file." )

#define POSX_TEXT N_("X coordinate of the OSD menu")
#define POSX_LONGTEXT N_("You can move the OSD menu by left-clicking on it." )

#define POSY_TEXT N_("Y coordinate of the OSD menu")
#define POSY_LONGTEXT N_("You can move the OSD menu by left-clicking on it." )

#define POS_TEXT N_("OSD menu position")
#define POS_LONGTEXT N_( \
  "You can enforce the OSD menu position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values).")

#define TIMEOUT_TEXT N_("Timeout of OSD menu")
#define TIMEOUT_LONGTEXT N_( \
    "OSD menu pictures get a default timeout of 15 seconds added to their " \
    "remaining time. This will ensure that they are at least the specified " \
    "time visible.")

#define OSD_UPDATE_TEXT N_("Update speed of OSD menu")
#define OSD_UPDATE_LONGTEXT N_( \
    "Update the OSD menu picture every 200 ms (default). Shorten the update time for " \
    "environments that experience transmissions errors. Be careful with this option " \
    "because encoding OSD menu pictures is very computing intensive. The range is 0 - 1000 ms." )

static int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static char *ppsz_pos_descriptions[] =
{ N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

/* subfilter functions */
static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );
static subpicture_t *Filter( filter_t *, mtime_t );
static int OSDMenuUpdateEvent( vlc_object_t *, char const *,
                    vlc_value_t, vlc_value_t, void * );                    
static int OSDMenuVisibleEvent( vlc_object_t *, char const *,
                    vlc_value_t, vlc_value_t, void * );

#define OSD_CFG "osdmenu-"

#if defined( WIN32 ) || defined( UNDER_CE )
#define OSD_DEFAULT_CFG "osdmenu/default.cfg"
#else
#define OSD_DEFAULT_CFG "share/osdmenu/default.cfg"
#endif

#define OSD_UPDATE_MIN     0
#define OSD_UPDATE_DEFAULT 0
#define OSD_UPDATE_MAX     1000

vlc_module_begin();
    add_integer( OSD_CFG "x", -1, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_FALSE );
    add_integer( OSD_CFG "y", -1, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_FALSE );
    add_integer( OSD_CFG "position", 8, NULL, POS_TEXT, POS_LONGTEXT, VLC_FALSE );
        change_integer_list( pi_pos_values, ppsz_pos_descriptions, 0 );
    add_string( OSD_CFG "file", OSD_DEFAULT_CFG, NULL, OSD_FILE_TEXT,
        OSD_FILE_LONGTEXT, VLC_FALSE );
    add_string( OSD_CFG "file-path", NULL, NULL, OSD_PATH_TEXT,
        OSD_PATH_LONGTEXT, VLC_FALSE );
    add_integer( OSD_CFG "timeout", 15, NULL, TIMEOUT_TEXT,
        TIMEOUT_LONGTEXT, VLC_FALSE );
    add_integer_with_range( OSD_CFG "update", OSD_UPDATE_DEFAULT,
        OSD_UPDATE_MIN, OSD_UPDATE_MAX, NULL, OSD_UPDATE_TEXT,
        OSD_UPDATE_LONGTEXT, VLC_TRUE );

    set_capability( "sub filter", 100 );
    set_description( N_("On Screen Display menu subfilter") );
    set_shortname( N_("OSD menu") );
    add_shortcut( "osdmenu" );
/*
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_SUBPIC );
*/
    set_callbacks( CreateFilter, DestroyFilter );
vlc_module_end();

/*****************************************************************************
 * Sub filter code
 *****************************************************************************/

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct filter_sys_t
{
    vlc_mutex_t  lock;

    int          position;      /* relative positioning of SPU images */
    mtime_t      i_last_date;   /* last mdate SPU object has been sent to SPU subsytem */
    mtime_t      i_timeout;     /* duration SPU object is valid on the video output in seconds */

    vlc_bool_t   b_absolute;    /* do we use absolute positioning or relative? */
    vlc_bool_t   b_update;      /* Update OSD Menu by sending SPU objects */
    vlc_bool_t   b_visible;     /* OSD Menu is visible */
    mtime_t      i_update;      /* Update the OSD menu every n ms */
    mtime_t      i_end_date;    /* End data of display OSD menu */

    char        *psz_file;      /* OSD Menu configuration file */
    osd_menu_t  *p_menu;        /* pointer to OSD Menu object */
};

/*****************************************************************************
 * CreateFilter: Create the filter and open the definition file
 *****************************************************************************/
static int CreateFilter ( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    vlc_value_t val;
    int i_posx, i_posy;

    p_filter->p_sys = (filter_sys_t *) malloc( sizeof( filter_sys_t ) );
    if( !p_filter->p_sys )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    /* Populating struct */
    p_filter->p_sys->p_menu = NULL;
    p_filter->p_sys->psz_file = NULL;

    vlc_mutex_init( p_filter, &p_filter->p_sys->lock );

    p_filter->p_sys->psz_file = config_GetPsz( p_filter, OSD_CFG "file" );
    if( p_filter->p_sys->psz_file == NULL || *p_filter->p_sys->psz_file == '\0' ) 
    {
        msg_Err( p_filter, "unable to get filename" );
        goto error;
    }

    var_Create( p_this, OSD_CFG "position", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, OSD_CFG "position", &val );
    p_filter->p_sys->position = val.i_int;
    var_Create( p_this, OSD_CFG "x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, OSD_CFG "x", &val );
    i_posx = val.i_int;
    var_Create( p_this, OSD_CFG "y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, OSD_CFG "y", &val );
    i_posy = val.i_int;

    /* in micro seconds - divide by 2 to match user expectations */
    var_Create( p_this, OSD_CFG "timeout", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, OSD_CFG "timeout", &val );
    p_filter->p_sys->i_timeout = (mtime_t)(val.i_int * 1000000) >> 2; 
    var_Create( p_this, OSD_CFG "update", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, OSD_CFG "update", &val );
    p_filter->p_sys->i_update = (mtime_t)(val.i_int * 1000); /* in micro seconds */

    /* Load the osd menu subsystem */
    p_filter->p_sys->p_menu = osd_MenuCreate( p_this, p_filter->p_sys->psz_file );
    if( p_filter->p_sys->p_menu == NULL )
        goto error;

    /* Check if menu position was overridden */
    p_filter->p_sys->b_absolute = VLC_TRUE;
    if( i_posx < 0 || i_posy < 0)
    {
        p_filter->p_sys->b_absolute = VLC_FALSE;
        p_filter->p_sys->p_menu->i_x = 0;
        p_filter->p_sys->p_menu->i_y = 0;
    }
    else if( i_posx >= 0 || i_posy >= 0 )
    {
        p_filter->p_sys->p_menu->i_x = i_posx;
        p_filter->p_sys->p_menu->i_y = i_posy;
    }
    else if( p_filter->p_sys->p_menu->i_x < 0 || p_filter->p_sys->p_menu->i_y < 0 )
    {
        p_filter->p_sys->b_absolute = VLC_FALSE;
        p_filter->p_sys->p_menu->i_x = 0;
        p_filter->p_sys->p_menu->i_y = 0;
    }

    /* Set up p_filter */
    p_filter->p_sys->i_last_date = mdate();

    /* Keep track of OSD Events */
    p_filter->p_sys->b_update  = VLC_FALSE;
    p_filter->p_sys->b_visible = VLC_FALSE;

    var_AddCallback( p_filter->p_sys->p_menu, "osd-menu-update", OSDMenuUpdateEvent, p_filter );        
    var_AddCallback( p_filter->p_sys->p_menu, "osd-menu-visible", OSDMenuVisibleEvent, p_filter );        

    /* Attach subpicture filter callback */
    p_filter->pf_sub_filter = Filter;

    es_format_Init( &p_filter->fmt_out, SPU_ES, VLC_FOURCC( 's','p','u',' ' ) );
    p_filter->fmt_out.i_priority = 0;

    msg_Dbg( p_filter, "successfully loaded osdmenu filter" );    
    return VLC_SUCCESS;

error:
    msg_Err( p_filter, "osdmenu filter discarded" );
    vlc_mutex_destroy( &p_filter->p_sys->lock );
    if( p_filter->p_sys->p_menu )
    {
        osd_MenuDelete( p_this, p_filter->p_sys->p_menu );
        p_filter->p_sys->p_menu = NULL;
    }
    if( p_filter->p_sys->psz_file ) free( p_filter->p_sys->psz_file );
    if( p_filter->p_sys ) free( p_filter->p_sys );
    return VLC_EGENERIC;    
}

/*****************************************************************************
 * DestroyFilter: Make a clean exit of this plugin
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_Destroy( p_this, OSD_CFG "file" );
    var_Destroy( p_this, OSD_CFG "x" );
    var_Destroy( p_this, OSD_CFG "y" );
    var_Destroy( p_this, OSD_CFG "position" );
    var_Destroy( p_this, OSD_CFG "timeout" );
    var_Destroy( p_this, OSD_CFG "update" );

    var_DelCallback( p_sys->p_menu, "osd-menu-update", OSDMenuUpdateEvent, p_filter );
    var_DelCallback( p_sys->p_menu, "osd-menu-visible", OSDMenuVisibleEvent, p_filter );

    osd_MenuDelete( p_filter, p_sys->p_menu );

    vlc_mutex_destroy( &p_filter->p_sys->lock );
    if( p_sys->psz_file) free( p_sys->psz_file );
    if( p_sys ) free( p_sys );

    msg_Dbg( p_filter, "osdmenu filter destroyed" );
}

/*****************************************************************************
 * OSDMenuEvent: callback for OSD Menu events
 *****************************************************************************/
static int OSDMenuVisibleEvent( vlc_object_t *p_this, char const *psz_var,
                    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    filter_t *p_filter = (filter_t *) p_data;

    p_filter->p_sys->b_visible = VLC_TRUE;
    return VLC_SUCCESS;
}

static int OSDMenuUpdateEvent( vlc_object_t *p_this, char const *psz_var,
                    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    filter_t *p_filter = (filter_t *) p_data;

    p_filter->p_sys->b_update = VLC_TRUE;
    p_filter->p_sys->i_end_date = (mtime_t) 0;
    return VLC_SUCCESS;
}

#if 0
/*****************************************************************************
 * create_text_region : compose a text region SPU
 *****************************************************************************/
static subpicture_region_t *create_text_region( filter_t *p_filter, subpicture_t *p_spu, 
    int i_width, int i_height, const char *psz_text )
{
    subpicture_region_t *p_region;
    video_format_t       fmt;

    /* Create new SPU region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC( 'T','E','X','T' );
    fmt.i_aspect = VOUT_ASPECT_FACTOR;
    fmt.i_sar_num = fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_visible_width = i_width;
    fmt.i_height = fmt.i_visible_height = i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt );
    if( !p_region )
    {
        msg_Err( p_filter, "cannot allocate another SPU region" );
        return NULL;
    }
    p_region->psz_text = strdup( psz_text );
    p_region->i_x = 0; 
    p_region->i_y = 40;
#if 1
    msg_Dbg( p_filter, "SPU text region position (%d,%d) (%d,%d) [%s]", 
        p_region->i_x, p_region->i_y, 
        p_region->fmt.i_width, p_region->fmt.i_height, p_region->psz_text );
#endif
    return p_region;
}
#endif

/*****************************************************************************
 * create_picture_region : compose a picture region SPU
 *****************************************************************************/
static subpicture_region_t *create_picture_region( filter_t *p_filter, subpicture_t *p_spu,
    int i_width, int i_height, picture_t *p_pic )
{
    subpicture_region_t *p_region;
    video_format_t       fmt;

    if( !p_spu ) return NULL;

    /* Create new SPU region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = (p_pic == NULL) ? VLC_FOURCC('Y','U','V','P') : VLC_FOURCC('Y','U','V','A');
    fmt.i_aspect = VOUT_ASPECT_FACTOR;
    fmt.i_sar_num = fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_visible_width = i_width;
    fmt.i_height = fmt.i_visible_height = i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt );
    if( !p_region )
    {
        msg_Err( p_filter, "cannot allocate SPU region" );
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        return NULL;
    }
    if( !p_pic && ( fmt.i_chroma == VLC_FOURCC('Y','U','V','P') ) )
    {
        p_region->fmt.p_palette->i_entries = 0;
        p_region->fmt.i_width = p_region->fmt.i_visible_width = 0;
        p_region->fmt.i_height = p_region->fmt.i_visible_height = 0;
    }
    if( p_pic != NULL )
        vout_CopyPicture( p_filter, &p_region->picture, p_pic );

    p_region->i_x = 0;
    p_region->i_y = 0;
#if 0
    msg_Dbg( p_filter, "SPU picture region position (%d,%d) (%d,%d) [%p]",
        p_region->i_x, p_region->i_y,
        p_region->fmt.i_width, p_region->fmt.i_height, p_pic );
#endif
    return p_region;
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************
 * This function outputs subpictures at regular time intervals.
 ****************************************************************************/
static subpicture_t *Filter( filter_t *p_filter, mtime_t i_date )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    subpicture_t *p_spu;
    subpicture_region_t *p_region;

    if( !p_sys->b_update )
            return NULL;
            
    /* Am I too early? */
    if( ( ( p_sys->i_last_date + p_sys->i_update ) > i_date ) &&
        ( p_sys->i_end_date > 0 ) )
        return NULL; /* we are too early, so wait */
    
    /* Allocate the subpicture internal data. */
    p_spu = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_spu ) return NULL;
    p_spu->b_ephemer = VLC_TRUE;
    p_spu->b_fade = VLC_TRUE;    
    p_spu->b_absolute = p_sys->b_absolute;
    p_spu->i_flags = p_sys->position;

    /* Determine the duration of the subpicture */
    if( p_sys->i_end_date > 0 )
    {
        /* Display the subpicture again. */
        p_spu->i_stop = p_sys->i_end_date - i_date;
        if( ( i_date + p_sys->i_update ) >= p_sys->i_end_date )
            p_sys->b_update = VLC_FALSE;
    }
    else
    {
        /* There is a new OSD picture to display */
        p_spu->i_stop = i_date + p_sys->i_timeout;
        p_sys->i_end_date = p_spu->i_stop;
    }
    
    p_sys->i_last_date = i_date;
    p_spu->i_start = p_sys->i_last_date = i_date;

    /* Send an empty subpicture to clear the display
     * when OSD menu should be hidden and menu picture is not allocated.
     */
    if( !p_filter->p_sys->p_menu->p_state->p_pic ||
        ( p_filter->p_sys->b_visible == VLC_FALSE ) )
    {
        /* Create new spu regions and allocate an empty picture in it. */
        p_region = create_picture_region( p_filter, p_spu,
            p_filter->p_sys->p_menu->p_state->i_width,
            p_filter->p_sys->p_menu->p_state->i_height,
            NULL );

        /* proper positioning of OSD menu image */
        p_spu->i_x = p_filter->p_sys->p_menu->p_state->i_x;
        p_spu->i_y = p_filter->p_sys->p_menu->p_state->i_y;
        p_spu->p_region = p_region;
        p_spu->i_alpha = 0xFF; /* Picture is completely transparent. */
        return p_spu;
    }

    /* Create new spu regions */
    p_region = create_picture_region( p_filter, p_spu,
        p_filter->p_sys->p_menu->p_state->i_width,
        p_filter->p_sys->p_menu->p_state->i_height,
        p_filter->p_sys->p_menu->p_state->p_pic );
#if 0
    p_region->p_next = create_text_region( p_filter, p_spu,
        p_filter->p_sys->p_menu->p_state->i_width, p_filter->p_sys->p_menu->p_state->i_height,
        p_filter->p_sys->p_menu->p_state->p_visible->psz_action );
#endif

    /* proper positioning of OSD menu image */
    p_spu->i_x = p_filter->p_sys->p_menu->p_state->i_x;
    p_spu->i_y = p_filter->p_sys->p_menu->p_state->i_y;
    p_spu->p_region = p_region;
    return p_spu;
}
