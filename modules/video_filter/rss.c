/*****************************************************************************
 * rss.c : rss feed display video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
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

#include "vlc_block.h"
#include "vlc_stream.h"
#include "vlc_xml.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );
static subpicture_t *Filter( filter_t *, mtime_t );

static int FetchRSS( filter_t * );
static void FreeRSS( filter_t * );

static int pi_color_values[] = { 0xf0000000, 0x00000000, 0x00808080, 0x00C0C0C0,
               0x00FFFFFF, 0x00800000, 0x00FF0000, 0x00FF00FF, 0x00FFFF00,
               0x00808000, 0x00008000, 0x00008080, 0x0000FF00, 0x00800080,
               0x00000080, 0x000000FF, 0x0000FFFF};
static char *ppsz_color_descriptions[] = { N_("Default"), N_("Black"),
               N_("Gray"), N_("Silver"), N_("White"), N_("Maroon"), N_("Red"),
               N_("Fuchsia"), N_("Yellow"), N_("Olive"), N_("Green"),
               N_("Teal"), N_("Lime"), N_("Purple"), N_("Navy"), N_("Blue"),
               N_("Aqua") };

/*****************************************************************************
 * filter_sys_t: rss filter descriptor
 *****************************************************************************/

struct rss_item_t
{
    char *psz_title;
    char *psz_description;
    char *psz_link;
};

struct rss_feed_t
{
    char *psz_title;
    char *psz_description;
    char *psz_link;

    int i_items;
    struct rss_item_t *p_items;
};

struct filter_sys_t
{
    vlc_mutex_t lock;
    vlc_mutex_t *p_lock;

    int i_xoff, i_yoff;  /* offsets for the display string in the video window */
    int i_pos; /* permit relative positioning (top, bottom, left, right, center) */
    int i_speed;
    int i_length;

    char *psz_marquee;    /* marquee string */

    int  i_font_color, i_font_opacity, i_font_size; /* font control */

    mtime_t last_date;

    //vlc_bool_t b_need_update;

    char *psz_urls;
    int i_feeds;
    struct rss_feed_t *p_feeds;

    int i_cur_feed;
    int i_cur_item;
    int i_cur_char;
};

#define MSG_TEXT N_("RSS feed URLs")
#define MSG_LONGTEXT N_("RSS feed comma seperated URLs")
#define SPEED_TEXT N_("RSS feed speed")
#define SPEED_LONGTEXT N_("RSS feed speed (bigger is slower)")
#define LENGTH_TEXT N_("RSS feed max number of chars displayed")
#define LENGTH_LONGTEXT N_("RSS feed max number of chars displayed")

#define POSX_TEXT N_("X offset, from left")
#define POSX_LONGTEXT N_("X offset, from the left screen edge" )
#define POSY_TEXT N_("Y offset, from the top")
#define POSY_LONGTEXT N_("Y offset, down from the top" )
#define OPACITY_TEXT N_("Opacity")
#define OPACITY_LONGTEXT N_("The opacity (inverse of transparency) of " \
    "overlay text. 0 = transparent, 255 = totally opaque. " )
#define SIZE_TEXT N_("Font size, pixels")
#define SIZE_LONGTEXT N_("Specify the font size, in pixels, " \
    "with -1 = use freetype-fontsize" )

#define COLOR_TEXT N_("Text Default Color")
#define COLOR_LONGTEXT N_("The color of overlay text. 1 byte for each color, hexadecimal. " \
    "#000000 = all colors off, " \
    "0xFF0000 = just Red, 0xFFFFFF = all color on [White]" )

#define POS_TEXT N_("Marquee position")
#define POS_LONGTEXT N_( \
  "You can enforce the marquee position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values by adding them).")

static int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static char *ppsz_pos_descriptions[] =
     { N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
     N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_capability( "sub filter", 0 );
    set_shortname( N_("RSS" ));
    set_callbacks( CreateFilter, DestroyFilter );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_SUBPIC );
    add_string( "rss-urls", "rss", NULL, MSG_TEXT, MSG_LONGTEXT, VLC_FALSE );

    set_section( N_("Position"), NULL );
    add_integer( "rss-x", -1, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_FALSE );
    add_integer( "rss-y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_FALSE );
    add_integer( "rss-position", 5, NULL, POS_TEXT, POS_LONGTEXT, VLC_TRUE );

    set_section( N_("Font"), NULL );
    /* 5 sets the default to top [1] left [4] */
    change_integer_list( pi_pos_values, ppsz_pos_descriptions, 0 );
    add_integer_with_range( "rss-opacity", 255, 0, 255, NULL,
        OPACITY_TEXT, OPACITY_LONGTEXT, VLC_FALSE );
    add_integer( "rss-color", 0xFFFFFF, NULL, COLOR_TEXT, COLOR_LONGTEXT, VLC_TRUE );
        change_integer_list( pi_color_values, ppsz_color_descriptions, 0 );
    add_integer( "rss-size", -1, NULL, SIZE_TEXT, SIZE_LONGTEXT, VLC_FALSE );

    set_section( N_("Misc"), NULL );
    add_integer( "rss-speed", 100000, NULL, SPEED_TEXT, SPEED_LONGTEXT,
                 VLC_FALSE );
    add_integer( "rss-length", 60, NULL, LENGTH_TEXT, LENGTH_LONGTEXT,
                 VLC_FALSE );

    set_description( _("RSS feed display sub filter") );
    add_shortcut( "rss" );
vlc_module_end();

/*****************************************************************************
 * CreateFilter: allocates RSS video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    int i_feed;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    vlc_mutex_init( p_filter, &p_sys->lock );
    vlc_mutex_lock( &p_sys->lock );

    p_sys->psz_urls = var_CreateGetString( p_filter, "rss-urls" );
    p_sys->i_cur_feed = 0;
    p_sys->i_cur_item = 0;
    p_sys->i_cur_char = 0;
    p_sys->i_feeds = 0;
    p_sys->p_feeds = NULL;
    p_sys->i_speed = var_CreateGetInteger( p_filter, "rss-speed" );
    p_sys->i_length = var_CreateGetInteger( p_filter, "rss-length" );
    p_sys->psz_marquee = (char *)malloc( p_sys->i_length );

    p_sys->i_xoff = var_CreateGetInteger( p_filter, "rss-x" );
    p_sys->i_yoff = var_CreateGetInteger( p_filter, "rss-y" );
    p_sys->i_pos = var_CreateGetInteger( p_filter, "rss-position" );
    var_Create( p_filter, "rss-opacity", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    p_sys->i_font_opacity = var_CreateGetInteger( p_filter, "rss-opacity" );
    p_sys->i_font_color = var_CreateGetInteger( p_filter, "rss-color" );
    p_sys->i_font_size = var_CreateGetInteger( p_filter, "rss-size" );

    if( FetchRSS( p_filter ) )
    {
        msg_Err( p_filter, "failed while fetching RSS ... too bad" );
        vlc_mutex_unlock( &p_sys->lock );
        return VLC_EGENERIC;
    }

    if( p_sys->i_feeds == 0 )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return VLC_EGENERIC;
    }
    for( i_feed=0; i_feed < p_sys->i_feeds; i_feed ++ )
        if( p_sys->p_feeds[i_feed].i_items == 0 )
        {
            vlc_mutex_unlock( &p_sys->lock );
            return VLC_EGENERIC;
        }

    /* Misc init */
    p_filter->pf_sub_filter = Filter;
    p_sys->last_date = (mtime_t)0;

    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}
/*****************************************************************************
 * DestroyFilter: destroy RSS video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_mutex_lock( &p_sys->lock );

    if( p_sys->psz_marquee ) free( p_sys->psz_marquee );
    free( p_sys->psz_urls );
    FreeRSS( p_filter );
    vlc_mutex_unlock( &p_sys->lock );
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );

    /* Delete the RSS variables */
    var_Destroy( p_filter, "rss-urls" );
    var_Destroy( p_filter, "rss-speed" );
    var_Destroy( p_filter, "rss-length" );
    var_Destroy( p_filter, "rss-x" );
    var_Destroy( p_filter, "rss-y" );
    var_Destroy( p_filter, "rss-position" );
    var_Destroy( p_filter, "rss-color");
    var_Destroy( p_filter, "rss-opacity");
    var_Destroy( p_filter, "rss-size");
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

    int i_feed, i_item;

    vlc_mutex_lock( &p_sys->lock );

    /* wait more for the 1st char */
    if( p_sys->last_date + ( p_sys->i_cur_char == 0 && p_sys->i_cur_item == 0 ? 5 : 1 ) * p_sys->i_speed > date )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }

    p_sys->last_date = date;
    p_sys->i_cur_char++;
    if( p_sys->p_feeds[p_sys->i_cur_feed].p_items[p_sys->i_cur_item].psz_title[p_sys->i_cur_char] == 0 )
    {
        p_sys->i_cur_char = 0;
        p_sys->i_cur_item++;
        if( p_sys->i_cur_item >= p_sys->p_feeds[p_sys->i_cur_feed].i_items )
        {
            p_sys->i_cur_item = 0;
            p_sys->i_cur_feed = (p_sys->i_cur_feed + 1)%p_sys->i_feeds;
        }
    }

    p_spu = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_spu )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }

    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = 0;
    fmt.i_height = 0;
    fmt.i_x_offset = 0;
    fmt.i_y_offset = 0;
    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt );
    if( !p_spu->p_region )
    {
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }

    i_item = p_sys->i_cur_item;
    i_feed = p_sys->i_cur_feed;
    snprintf( p_sys->psz_marquee, p_sys->i_length, "%s : %s", p_sys->p_feeds[i_feed].psz_title, p_sys->p_feeds[i_feed].p_items[i_item].psz_title+p_sys->i_cur_char );
    while( strlen( p_sys->psz_marquee ) < (unsigned int)p_sys->i_length )
    {
        i_item++;
        if( i_item == p_sys->p_feeds[i_feed].i_items ) break;
        snprintf( strchr( p_sys->psz_marquee, 0 ), p_sys->i_length - strlen( p_sys->psz_marquee ), " - %s", p_sys->p_feeds[i_feed].p_items[i_item].psz_title );
    }

    p_spu->p_region->psz_text = strdup(p_sys->psz_marquee);
    p_spu->i_start = date;
    p_spu->i_stop  = 0;
    p_spu->b_ephemer = VLC_TRUE;

    /*  where to locate the string: */
    if( p_sys->i_xoff < 0 || p_sys->i_yoff < 0 )
    {   /* set to one of the 9 relative locations */
        p_spu->i_flags = p_sys->i_pos;
        p_spu->i_x = 0;
        p_spu->i_y = 0;
        p_spu->b_absolute = VLC_FALSE;
    }
    else
    {   /*  set to an absolute xy, referenced to upper left corner */
        p_spu->i_flags = OSD_ALIGN_LEFT | OSD_ALIGN_TOP;
        p_spu->i_x = p_sys->i_xoff;
        p_spu->i_y = p_sys->i_yoff;
        p_spu->b_absolute = VLC_TRUE;
    }
    p_spu->i_height = 1;
    p_spu->p_region->i_text_color = p_sys->i_font_color;
    p_spu->p_region->i_text_alpha = 255 - p_sys->i_font_opacity;
    p_spu->p_region->i_text_size = p_sys->i_font_size;

    vlc_mutex_unlock( &p_sys->lock );
    return p_spu;
}

/****************************************************************************
 * RSS related functions
 ****************************************************************************
 * You should always lock the p_filter mutex before using any of these
 * functions
 ***************************************************************************/

/****************************************************************************
 * FetchRSS
 ***************************************************************************/
static int FetchRSS( filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    stream_t *p_stream = NULL;
    xml_t *p_xml = NULL;
    xml_reader_t *p_xml_reader = NULL;

    char *psz_eltname = NULL;
    char *psz_eltvalue = NULL;
    char *psz_feed = NULL;
    char *psz_buffer = NULL;
    char *psz_buffer_2 = NULL;

    int i_feed;
    int i_item;
    int i_is_item;
    int i_int;

    FreeRSS( p_filter );
    p_sys->i_feeds = 1;
    i_int = 0;
    while( p_sys->psz_urls[i_int] != 0 )
        if( p_sys->psz_urls[i_int++] == ',' )
            p_sys->i_feeds++;
    p_sys->p_feeds = (struct rss_feed_t *)malloc( p_sys->i_feeds
                                * sizeof( struct rss_feed_t ) );

    p_xml = xml_Create( p_filter );
    if( !p_xml )
    {
        msg_Err( p_filter, "Failed to open XML parser" );
        return 1;
    }

    psz_buffer = strdup( p_sys->psz_urls );
    psz_buffer_2 = psz_buffer; /* keep track so we can free it */
    for( i_feed = 0; i_feed < p_sys->i_feeds; i_feed++ )
    {
        struct rss_feed_t *p_feed = p_sys->p_feeds+i_feed;

        if( psz_buffer == NULL ) break;
        if( psz_buffer[0] == 0 ) psz_buffer++;
        psz_feed = psz_buffer;
        psz_buffer = strchr( psz_buffer, ',' );
        if( psz_buffer != NULL ) psz_buffer[0] = 0;

        p_feed->psz_title = NULL;
        p_feed->psz_description = NULL;
        p_feed->psz_link = NULL;
        p_feed->i_items = 0;
        p_feed->p_items = NULL;

        msg_Dbg( p_filter, "Opening %s RSS feed ...", psz_feed );

        p_stream = stream_UrlNew( p_filter, psz_feed );
        if( !p_stream )
        {
            msg_Err( p_filter, "Failed to open %s for reading", psz_feed );
            return 1;
        }

        p_xml_reader = xml_ReaderCreate( p_xml, p_stream );
        if( !p_xml_reader )
        {
            msg_Err( p_filter, "Failed to open %s for parsing", psz_feed );
            return 1;
        }

        i_item = 0;
        i_is_item = VLC_FALSE;

        while( xml_ReaderRead( p_xml_reader ) == 1 )
        {
            switch( xml_ReaderNodeType( p_xml_reader ) )
            {
                // Error
                case -1:
                    return 1;

                case XML_READER_STARTELEM:
                    if( psz_eltname ) free( psz_eltname );
                    psz_eltname = xml_ReaderName( p_xml_reader );
                    if( !psz_eltname )
                    {
                        return 1;
                    }
                    msg_Dbg( p_filter, "element name : %s", psz_eltname );
                    if( !strcmp( psz_eltname, "item" ) )
                    {
                        i_is_item = VLC_TRUE;
                        p_feed->i_items++;
                        p_feed->p_items = (struct rss_item_t *)realloc( p_feed->p_items, p_feed->i_items * sizeof( struct rss_item_t ) );
                    }
                    break;

                case XML_READER_ENDELEM:
                    if( psz_eltname ) free( psz_eltname );
                    psz_eltname = xml_ReaderName( p_xml_reader );
                    if( !psz_eltname )
                    {
                        return 1;
                    }
                    msg_Dbg( p_filter, "element end : %s", psz_eltname );
                    if( !strcmp( psz_eltname, "item" ) )
                    {
                        i_is_item = VLC_FALSE;
                        i_item++;
                    }
                    free( psz_eltname ); psz_eltname = NULL;
                    break;

                case XML_READER_TEXT:
                    psz_eltvalue = xml_ReaderValue( p_xml_reader );
                    if( !psz_eltvalue )
                    {
                        return 1;
                    }
                    msg_Dbg( p_filter, "  text : %s", psz_eltvalue );
                    if( i_is_item == VLC_FALSE )
                    {
                        if( !strcmp( psz_eltname, "title" ) )
                        {
                            p_feed->psz_title = psz_eltvalue;
                        }
                        else if( !strcmp( psz_eltname, "link" ) )
                        {
                            p_feed->psz_link = psz_eltvalue;
                        }
                        else if( !strcmp( psz_eltname, "description" ) )
                        {
                            p_feed->psz_description = psz_eltvalue;
                        }
                        else
                        {
                            free( psz_eltvalue );
                        }
                    }
                    else
                    {
                        struct rss_item_t *p_item;
                        p_item = p_feed->p_items+i_item;
                        if( !strcmp( psz_eltname, "title" ) )
                        {
                            p_item->psz_title = psz_eltvalue;
                        }
                        else if( !strcmp( psz_eltname, "link" ) )
                        {
                            p_item->psz_link = psz_eltvalue;
                        }
                        else if( !strcmp( psz_eltname, "description" ) )
                        {
                            p_item->psz_description = psz_eltvalue;
                        }
                        else
                        {
                            free( psz_eltvalue );
                        }
                    }
                    break;
            }
        }

        if( p_xml_reader && p_xml ) xml_ReaderDelete( p_xml, p_xml_reader );
        if( p_stream ) stream_Delete( p_stream );
    }
    free( psz_buffer_2 );
    if( p_xml ) xml_Delete( p_xml );

    return 0;
}

/****************************************************************************
 * FreeRSS
 ***************************************************************************/
static void FreeRSS( filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    struct rss_item_t *p_item;
    struct rss_feed_t *p_feed;

    int i_feed;
    int i_item;

    for( i_feed = 0; i_feed < p_sys->i_feeds; i_feed++ )
    {
        p_feed = p_sys->p_feeds+i_feed;
        for( i_item = 0; i_item < p_feed->i_items; i_item++ )
        {
            p_item = p_feed->p_items+i_item;
            free( p_item->psz_title );
            free( p_item->psz_link );
            free( p_item->psz_description );
        }
        free( p_feed->p_items );
        free( p_feed->psz_title);
        free( p_feed->psz_link );
        free( p_feed->psz_description );
    }
    free( p_sys->p_feeds );
    p_sys->i_feeds = 0;
}
