/*****************************************************************************
 * osdmenu.c: osd filter module
 *****************************************************************************
 * Copyright (C) 2004-2007 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implid warranty of
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
#include <vlc_filter.h>

#include <vlc_osd.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* FIXME: Future extension make the definition file in XML format. */
#define OSD_FILE_TEXT N_("Configuration file")
#define OSD_FILE_LONGTEXT N_( \
    "Configuration file for the OSD Menu." )
#define OSD_PATH_TEXT N_("Path to OSD menu images")
#define OSD_PATH_LONGTEXT N_( \
    "Path to the OSD menu images. This will override the path defined in the " \
    "OSD configuration file." )

#define POSX_TEXT N_("X coordinate")
#define POSX_LONGTEXT N_("You can move the OSD menu by left-clicking on it." )

#define POSY_TEXT N_("Y coordinate")
#define POSY_LONGTEXT N_("You can move the OSD menu by left-clicking on it." )

#define POS_TEXT N_("Menu position")
#define POS_LONGTEXT N_( \
  "You can enforce the OSD menu position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values, eg. 6 = top-right).")

#define TIMEOUT_TEXT N_("Menu timeout")
#define TIMEOUT_LONGTEXT N_( \
    "OSD menu pictures get a default timeout of 15 seconds added to their " \
    "remaining time. This will ensure that they are at least the specified " \
    "time visible.")

#define OSD_UPDATE_TEXT N_("Menu update interval" )
#define OSD_UPDATE_LONGTEXT N_( \
    "The default is to update the OSD menu picture every 200 ms. Shorten the" \
    " update time for environments that experience transmissions errors. " \
    "Be careful with this option as encoding OSD menu pictures is very " \
    "computing intensive. The range is 0 - 1000 ms." )

#define OSD_ALPHA_TEXT N_("Alpha transparency value (default 255)")
#define OSD_ALPHA_LONGTEXT N_( \
    "The transparency of the OSD menu can be changed by giving a value " \
    "between 0 and 255. A lower value specifies more transparency a higher " \
    "means less transparency. The default is being not transparent " \
    "(value 255) the minimum is fully transparent (value 0)." )

static const int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_pos_descriptions[] =
{ N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

/* subsource functions */
static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );
static subpicture_t *Filter( filter_t *, mtime_t );

static int OSDMenuUpdateEvent( vlc_object_t *, char const *,
                    vlc_value_t, vlc_value_t, void * );
static int OSDMenuVisibleEvent( vlc_object_t *, char const *,
                    vlc_value_t, vlc_value_t, void * );
static int OSDMenuCallback( vlc_object_t *, char const *,
                            vlc_value_t, vlc_value_t, void * );

static int MouseEvent( filter_t *,
                       const vlc_mouse_t *,
                       const vlc_mouse_t *,
                       const video_format_t * );

#define OSD_CFG "osdmenu-"

#if defined( WIN32 )
#define OSD_DEFAULT_CFG "osdmenu/default.cfg"
#else
#define OSD_DEFAULT_CFG PKGDATADIR"/osdmenu/default.cfg"
#endif

#define OSD_UPDATE_MIN     0
#define OSD_UPDATE_DEFAULT 300
#define OSD_UPDATE_MAX     1000

vlc_module_begin ()
    set_capability( "sub source", 100 )
    set_description( N_("On Screen Display menu") )
    set_shortname( N_("OSD menu") )
    add_shortcut( "osdmenu" )

    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SUBPIC )

    set_callbacks( CreateFilter, DestroyFilter )

    add_integer( OSD_CFG "x", -1, POSX_TEXT, POSX_LONGTEXT, false )
    add_integer( OSD_CFG "y", -1, POSY_TEXT, POSY_LONGTEXT, false )
    add_integer( OSD_CFG "position", 8, POS_TEXT, POS_LONGTEXT,
                 false )
        change_integer_list( pi_pos_values, ppsz_pos_descriptions )
    add_loadfile( OSD_CFG "file", OSD_DEFAULT_CFG, OSD_FILE_TEXT,
        OSD_FILE_LONGTEXT, false )
    add_directory( OSD_CFG "file-path", NULL, OSD_PATH_TEXT,
        OSD_PATH_LONGTEXT, false )
    add_integer( OSD_CFG "timeout", 15, TIMEOUT_TEXT,
        TIMEOUT_LONGTEXT, false )
    add_integer_with_range( OSD_CFG "update", OSD_UPDATE_DEFAULT,
        OSD_UPDATE_MIN, OSD_UPDATE_MAX, OSD_UPDATE_TEXT,
        OSD_UPDATE_LONGTEXT, true )
    add_integer_with_range( OSD_CFG "alpha", 255, 0, 255,
        OSD_ALPHA_TEXT, OSD_ALPHA_LONGTEXT, true )

vlc_module_end ()

/*****************************************************************************
 * Sub source code
 *****************************************************************************/

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct filter_sys_t
{
    int          i_position;    /* relative positioning of SPU images */
    int          i_x;           /* absolute positioning of SPU images */
    int          i_y;           /* absolute positioning of SPU images */
    mtime_t      i_last_date;   /* last mdate SPU object has been sent to SPU subsytem */
    mtime_t      i_timeout;     /* duration SPU object is valid on the video output in seconds */

    bool   b_absolute;    /* do we use absolute positioning or relative? */
    bool   b_update;      /* Update OSD Menu by sending SPU objects */
    bool   b_visible;     /* OSD Menu is visible */
    mtime_t      i_update;      /* Update the OSD menu every n ms */
    mtime_t      i_end_date;    /* End data of display OSD menu */
    int          i_alpha;       /* alpha transparency value */

    char        *psz_file;      /* OSD Menu configuration file */
    char        *psz_path;      /* Path to OSD Menu pictures */
    osd_menu_t  *p_menu;        /* pointer to OSD Menu object */

    /* menu interaction */
    bool  b_clicked;
    uint32_t    i_mouse_x;
    uint32_t    i_mouse_y;
};

/*****************************************************************************
 * CreateFilter: Create the filter and open the definition file
 *****************************************************************************/
static int CreateFilter ( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = NULL;

    p_filter->p_sys = p_sys = (filter_sys_t *) malloc( sizeof(filter_sys_t) );
    if( !p_filter->p_sys )
        return VLC_ENOMEM;
    memset( p_sys, 0, sizeof(filter_sys_t) );

    /* Populating struct */
    p_sys->psz_path = var_CreateGetString( p_this, OSD_CFG "file-path" );
    p_sys->psz_file = var_CreateGetString( p_this, OSD_CFG "file" );
    if( (p_sys->psz_file == NULL) ||
        (*p_sys->psz_file == '\0') )
    {
        msg_Err( p_filter, "unable to get filename" );
        goto error;
    }

    p_sys->i_x = var_CreateGetIntegerCommand( p_this, OSD_CFG "x" );
    p_sys->i_y = var_CreateGetIntegerCommand( p_this, OSD_CFG "y" );
    p_sys->i_position = var_CreateGetIntegerCommand( p_this, OSD_CFG "position" );
    p_sys->i_alpha = var_CreateGetIntegerCommand( p_this, OSD_CFG "alpha" );

    /* in micro seconds - divide by 2 to match user expectations */
    p_sys->i_timeout = var_CreateGetIntegerCommand( p_this, OSD_CFG "timeout" );
    p_sys->i_timeout = (mtime_t)(p_sys->i_timeout * 1000000) >> 2;
    p_sys->i_update  = var_CreateGetIntegerCommand( p_this, OSD_CFG "update" );
    p_sys->i_update = (mtime_t)(p_sys->i_update * 1000); /* in micro seconds */

    var_AddCallback( p_filter, OSD_CFG "position", OSDMenuCallback, p_sys );
    var_AddCallback( p_filter, OSD_CFG "timeout", OSDMenuCallback, p_sys );
    var_AddCallback( p_filter, OSD_CFG "update", OSDMenuCallback, p_sys );
    var_AddCallback( p_filter, OSD_CFG "alpha", OSDMenuCallback, p_sys );

    /* Load the osd menu subsystem */
    p_sys->p_menu = osd_MenuCreate( p_this, p_sys->psz_file );
    if( p_sys->p_menu == NULL )
        goto error;

    /* FIXME: this plugin is not at all thread-safe w.r.t. callbacks */
    p_sys->p_menu->i_position = p_sys->i_position;

    /* Check if menu position was overridden */
    p_sys->b_absolute = true;
    if( (p_sys->i_x < 0) || (p_sys->i_y < 0) )
    {
        p_sys->b_absolute = false;
        p_sys->p_menu->i_x = 0;
        p_sys->p_menu->i_y = 0;
    }
    else
    {
        p_sys->p_menu->i_x = p_sys->i_x;
        p_sys->p_menu->i_y = p_sys->i_y;
    }

    /* Set up p_filter */
    p_sys->i_last_date = mdate();

    /* Keep track of OSD Events */
    p_sys->b_update  = false;
    p_sys->b_visible = false;
    p_sys->b_clicked = false;

    /* Listen to osd menu core updates/visible settings. */
    var_AddCallback( p_sys->p_menu, "osd-menu-update",
                     OSDMenuUpdateEvent, p_filter );
    var_AddCallback( p_sys->p_menu, "osd-menu-visible",
                     OSDMenuVisibleEvent, p_filter );

    /* Attach subpicture source callback */
    p_filter->pf_sub_source = Filter;
    p_filter->pf_sub_mouse  = MouseEvent;

    es_format_Init( &p_filter->fmt_out, SPU_ES, VLC_CODEC_SPU );
    p_filter->fmt_out.i_priority = 0;

    return VLC_SUCCESS;

error:
    msg_Err( p_filter, "osdmenu filter discarded" );

    free( p_sys->psz_path );
    free( p_sys->psz_file );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * DestroyFilter: Make a clean exit of this plugin
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, OSD_CFG "position", OSDMenuCallback, p_sys );
    var_DelCallback( p_filter, OSD_CFG "timeout", OSDMenuCallback, p_sys );
    var_DelCallback( p_filter, OSD_CFG "update", OSDMenuCallback, p_sys );
    var_DelCallback( p_filter, OSD_CFG "alpha", OSDMenuCallback, p_sys );

    var_DelCallback( p_sys->p_menu, "osd-menu-update",
                     OSDMenuUpdateEvent, p_filter );
    var_DelCallback( p_sys->p_menu, "osd-menu-visible",
                     OSDMenuVisibleEvent, p_filter );

    var_Destroy( p_this, OSD_CFG "file-path" );
    var_Destroy( p_this, OSD_CFG "file" );
    var_Destroy( p_this, OSD_CFG "x" );
    var_Destroy( p_this, OSD_CFG "y" );
    var_Destroy( p_this, OSD_CFG "position" );
    var_Destroy( p_this, OSD_CFG "timeout" );
    var_Destroy( p_this, OSD_CFG "update" );
    var_Destroy( p_this, OSD_CFG "alpha" );

    osd_MenuDelete( p_filter, p_sys->p_menu );
    free( p_sys->psz_path );
    free( p_sys->psz_file );
    free( p_sys );
}

/*****************************************************************************
 * OSDMenuEvent: callback for OSD Menu events
 *****************************************************************************/
static int OSDMenuVisibleEvent( vlc_object_t *p_this, char const *psz_var,
                    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval);
    VLC_UNUSED(newval);
    filter_t *p_filter = (filter_t *) p_data;

    p_filter->p_sys->b_visible = true;
    p_filter->p_sys->b_update = true;
    return VLC_SUCCESS;
}

static int OSDMenuUpdateEvent( vlc_object_t *p_this, char const *psz_var,
                    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval);
    VLC_UNUSED(newval);
    filter_t *p_filter = (filter_t *) p_data;
    filter_sys_t *p_sys = p_filter->p_sys;

    p_sys->b_update = p_sys->b_visible ? true : false;
    p_sys->i_end_date = (mtime_t) 0;
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
    fmt.i_chroma = VLC_CODEC_TEXT;
    fmt.i_sar_num = fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_visible_width = i_width;
    fmt.i_height = fmt.i_visible_height = i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_region = subpicture_region_New( &fmt );
    if( !p_region )
    {
        msg_Err( p_filter, "cannot allocate another SPU region" );
        return NULL;
    }
    p_region->psz_text = strdup( psz_text );
    p_region->i_x = 0;
    p_region->i_y = 40;
#if 0
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
    subpicture_region_t *p_region = NULL;
    video_format_t       fmt;
    video_palette_t      palette;

    if( !p_spu ) return NULL;

    /* Create new SPU region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = (p_pic == NULL) ? VLC_CODEC_YUVP : VLC_CODEC_YUVA;
    fmt.i_sar_num = fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_visible_width = i_width;
    fmt.i_height = fmt.i_visible_height = i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    if( fmt.i_chroma == VLC_CODEC_YUVP )
    {
        fmt.p_palette = &palette;
        fmt.p_palette->i_entries = 0;
        fmt.i_visible_width = 0;
        fmt.i_visible_height = 0;
    }

    p_region = subpicture_region_New( &fmt );
    if( !p_region )
    {
        msg_Err( p_filter, "cannot allocate SPU region" );
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        return NULL;
    }
    /* FIXME the copy is probably not needed anymore */
    if( p_pic )
        picture_Copy( p_region->p_picture, p_pic );

    p_region->i_x = 0;
    p_region->i_y = 0;
    p_region->i_align = p_filter->p_sys->i_position;
    p_region->i_alpha = p_filter->p_sys->i_alpha;
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
    subpicture_t *p_spu = NULL;
    subpicture_region_t *p_region = NULL;
    int i_x, i_y;

    if( !p_sys->b_update || (p_sys->i_update <= 0) )
            return NULL;

    /* Am I too early?
    */
    if( ( ( p_sys->i_last_date + p_sys->i_update ) > i_date ) &&
        ( p_sys->i_end_date > 0 ) )
        return NULL; /* we are too early, so wait */

    /* Allocate the subpicture internal data. */
    p_spu = filter_NewSubpicture( p_filter );
    if( !p_spu )
        return NULL;

    p_spu->b_ephemer = true;
    p_spu->b_fade = true;
    if( p_filter->p_sys->p_menu->i_style == OSD_MENU_STYLE_CONCAT )
        p_spu->b_absolute = true;
    else
        p_spu->b_absolute = p_sys->b_absolute;

    /* Determine the duration of the subpicture */
    if( p_sys->i_end_date > 0 )
    {
        /* Display the subpicture again. */
        p_spu->i_stop = p_sys->i_end_date - i_date;
        if( ( i_date + p_sys->i_update ) >= p_sys->i_end_date )
            p_sys->b_update = false;
    }
    else
    {
        /* There is a new OSD picture to display */
        p_spu->i_stop = i_date + p_sys->i_timeout;
        p_sys->i_end_date = p_spu->i_stop;
    }

    p_spu->i_start = p_sys->i_last_date = i_date;

    /* Send an empty subpicture to clear the display
     * when OSD menu should be hidden and menu picture is not allocated.
     */
    if( !p_filter->p_sys->p_menu->p_state->p_pic ||
        !p_filter->p_sys->b_visible )
    {
        p_spu->i_alpha = 0xFF; /* Picture is completely non transparent. */
        return p_spu;
    }

    if( p_sys->b_clicked )
    {
        p_sys->b_clicked = false;
        osd_MenuActivate( p_filter );
    }
    /* Create new spu regions
    */
    p_region = create_picture_region( p_filter, p_spu,
        p_filter->p_sys->p_menu->p_state->i_width,
        p_filter->p_sys->p_menu->p_state->i_height,
        p_filter->p_sys->p_menu->p_state->p_pic );

    if( !p_region )
    {
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        return NULL;
    }

    p_spu->i_alpha = p_filter->p_sys->i_alpha;

    /* proper positioning of OSD menu image */
    if( p_filter->p_sys->p_menu->i_style == OSD_MENU_STYLE_CONCAT )
    {
        i_x = p_filter->p_sys->p_menu->p_button->i_x;
        i_y = p_filter->p_sys->p_menu->p_button->i_y;
    }
    else
    {
        i_x = p_filter->p_sys->p_menu->p_state->i_x;
        i_y = p_filter->p_sys->p_menu->p_state->i_y;
    }
    p_region->i_x = i_x;
    p_region->i_y = i_y;

    if( p_filter->p_sys->p_menu->i_style == OSD_MENU_STYLE_CONCAT )
    {
        subpicture_region_t *p_region_list = NULL;
        subpicture_region_t *p_region_tail = NULL;
        osd_menu_t *p_osd = p_filter->p_sys->p_menu;
        osd_button_t *p_button = p_osd->p_button;

        /* Construct the entire OSD from individual images */
        while( p_button != NULL )
        {
            osd_button_t *p_tmp = NULL;
            subpicture_region_t *p_new = NULL;

            p_new = create_picture_region( p_filter, p_spu,
                    p_button->p_current_state->p_pic->p[Y_PLANE].i_visible_pitch,
                    p_button->p_current_state->p_pic->p[Y_PLANE].i_visible_lines,
                    p_button->p_current_state->p_pic );
            if( !p_new )
            {
                /* Cleanup when bailing out */
                subpicture_region_ChainDelete( p_region_list );
                subpicture_region_Delete( p_region );

                p_filter->pf_sub_buffer_del( p_filter, p_spu );
                return NULL;
            }

            if( !p_region_list )
            {
                p_region_list = p_new;
                p_region_tail = p_new;
            }
            else
            {
                p_new->i_x = i_x+p_region_tail->fmt.i_visible_width;
                p_new->i_y = i_y+p_button->i_y;
                p_region_tail->p_next = p_new;
                p_region_tail = p_new;
            }
            p_tmp = p_button->p_next;
            p_button = p_tmp;
        };
        p_region->p_next = p_region_list;
    }
#if 0
    p_region->p_next = create_text_region( p_filter, p_spu,
        p_filter->p_sys->p_menu->p_state->i_width, p_filter->p_sys->p_menu->p_state->i_height,
        p_filter->p_sys->p_menu->p_state->p_visible->psz_action );
#endif
    p_spu->p_region = p_region;
    return p_spu;
}

static int OSDMenuCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *) p_data;

    if( !p_sys )
        return VLC_SUCCESS;

    if( !strcmp( psz_var, OSD_CFG"position") )
    {
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
        unsigned int i;
        for( i=0; i < ARRAY_SIZE(pi_pos_values); i++ )
        {
            if( newval.i_int == pi_pos_values[i] )
            {
                p_sys->i_position = newval.i_int % 11;
                break;
            }
        }
#undef ARRAY_SIZE
    }
    else if( !strcmp( psz_var, OSD_CFG"x") ||
             !strcmp( psz_var, OSD_CFG"y"))
    {
        p_sys->b_absolute = true;
        if( (p_sys->i_x < 0) || (p_sys->i_y < 0) )
        {
            p_sys->b_absolute = false;
            p_sys->p_menu->i_x = 0;
            p_sys->p_menu->i_y = 0;
        }
        else if( (p_sys->i_x >= 0) || (p_sys->i_y >= 0) )
        {
            p_sys->p_menu->i_x = p_sys->i_x;
            p_sys->p_menu->i_y = p_sys->i_y;
        }
    }
    else if( !strcmp( psz_var, OSD_CFG"update") )
        p_sys->i_update =  newval.i_int * INT64_C(1000);
    else if( !strcmp( psz_var, OSD_CFG"timeout") )
        p_sys->i_update = newval.i_int % 1000;
    else if( !strcmp( psz_var, OSD_CFG"alpha") )
        p_sys->i_alpha = newval.i_int % 256;

    p_sys->b_update = p_sys->b_visible ? true : false;
    return VLC_SUCCESS;
}

static int MouseEvent( filter_t *p_filter,
                       const vlc_mouse_t *p_old,
                       const vlc_mouse_t *p_new,
                       const video_format_t *p_fmt )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( !vlc_mouse_HasPressed( p_old, p_new, MOUSE_BUTTON_LEFT ) )
        return VLC_SUCCESS;

    osd_button_t *p_button = osd_ButtonFind( VLC_OBJECT(p_filter),
                                             p_new->i_x,
                                             p_new->i_y,
                                             p_fmt->i_width,
                                             p_fmt->i_height,
                                             1000, 1000 );
    if( !p_button )
        return VLC_SUCCESS;

    osd_ButtonSelect( VLC_OBJECT(p_filter), p_button );
    p_sys->b_update = p_sys->b_visible ? true : false;
    p_sys->b_clicked = true;
    msg_Dbg( p_filter, "mouse clicked %s (%d,%d)", p_button->psz_name, p_new->i_x, p_new->i_y );
    return VLC_SUCCESS;
}
