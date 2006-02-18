/*****************************************************************************
 * rss.c : rss/atom feed display video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "vlc_filter.h"
#include "vlc_block.h"
#include "vlc_osd.h"

#include "vlc_block.h"
#include "vlc_stream.h"
#include "vlc_xml.h"
#include "charset.h"

#include "vlc_image.h"

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
    char *psz_image;
    picture_t *p_pic;

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

    text_style_t *p_style; /* font control */

    mtime_t last_date;

    char *psz_urls;
    int i_feeds;
    struct rss_feed_t *p_feeds;

    int i_ttl;
    time_t t_last_update;
    vlc_bool_t b_images;

    int i_cur_feed;
    int i_cur_item;
    int i_cur_char;
};

#define MSG_TEXT N_("RSS/Atom feed URLs")
#define MSG_LONGTEXT N_("RSS/Atom feed '|' (pipe) seperated URLs")
#define SPEED_TEXT N_("RSS/Atom feed speed")
#define SPEED_LONGTEXT N_("RSS/Atom feed speed (bigger is slower)")
#define LENGTH_TEXT N_("RSS/Atom feed max number of chars displayed")
#define LENGTH_LONGTEXT N_("RSS/Atom feed max number of chars displayed")
#define TTL_TEXT N_("Number of seconds between each forced refresh of the feeds")
#define TTL_LONGTEXT N_("Number of seconds between each forced refresh of the feeds. If 0, the feeds will never be updated.")
#define IMAGE_TEXT N_("Display feed images if available")
#define IMAGE_LONGTEXT N_("Display feed images if available")

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
    set_shortname( "RSS" );
    set_callbacks( CreateFilter, DestroyFilter );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_SUBPIC );
    add_string( "rss-urls", "rss", NULL, MSG_TEXT, MSG_LONGTEXT, VLC_FALSE );

    set_section( N_("Position"), NULL );
    add_integer( "rss-x", -1, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_TRUE );
    add_integer( "rss-y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_TRUE );
    add_integer( "rss-position", 5, NULL, POS_TEXT, POS_LONGTEXT, VLC_FALSE );
        change_integer_list( pi_pos_values, ppsz_pos_descriptions, 0 );

    set_section( N_("Font"), NULL );
    /* 5 sets the default to top [1] left [4] */
    add_integer_with_range( "rss-opacity", 255, 0, 255, NULL,
        OPACITY_TEXT, OPACITY_LONGTEXT, VLC_FALSE );
    add_integer( "rss-color", 0xFFFFFF, NULL, COLOR_TEXT, COLOR_LONGTEXT,
                  VLC_FALSE );
        change_integer_list( pi_color_values, ppsz_color_descriptions, 0 );
    add_integer( "rss-size", -1, NULL, SIZE_TEXT, SIZE_LONGTEXT, VLC_FALSE );

    set_section( N_("Misc"), NULL );
    add_integer( "rss-speed", 100000, NULL, SPEED_TEXT, SPEED_LONGTEXT,
                 VLC_FALSE );
    add_integer( "rss-length", 60, NULL, LENGTH_TEXT, LENGTH_LONGTEXT,
                 VLC_FALSE );
    add_integer( "rss-ttl", 1800, NULL, TTL_TEXT, TTL_LONGTEXT, VLC_FALSE );
    add_bool( "rss-images", 1, NULL, IMAGE_TEXT, IMAGE_LONGTEXT, VLC_FALSE );

    set_description( _("RSS and Atom feed display") );
    add_shortcut( "rss" );
    add_shortcut( "atom" );
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
    p_sys->i_ttl = __MAX( 0, var_CreateGetInteger( p_filter, "rss-ttl" ) );
    p_sys->b_images = var_CreateGetBool( p_filter, "rss-images" );
    p_sys->psz_marquee = (char *)malloc( p_sys->i_length );

    p_sys->p_style = malloc( sizeof( text_style_t ));
    memcpy( p_sys->p_style, &default_text_style, sizeof( text_style_t ));

    p_sys->i_xoff = var_CreateGetInteger( p_filter, "rss-x" );
    p_sys->i_yoff = var_CreateGetInteger( p_filter, "rss-y" );
    p_sys->i_pos = var_CreateGetInteger( p_filter, "rss-position" );
    p_sys->p_style->i_font_alpha = 255 - var_CreateGetInteger( p_filter, "rss-opacity" );
    p_sys->p_style->i_font_color = var_CreateGetInteger( p_filter, "rss-color" );
    p_sys->p_style->i_font_size = var_CreateGetInteger( p_filter, "rss-size" );

    if( p_sys->b_images == VLC_TRUE && p_sys->p_style->i_font_size == -1 )
    {
        msg_Warn( p_filter, "rrs-size wasn't specified. Feed images will thus be displayed without being resized" );
    }

    if( FetchRSS( p_filter ) )
    {
        msg_Err( p_filter, "failed while fetching RSS ... too bad" );
        vlc_mutex_unlock( &p_sys->lock );
        return VLC_EGENERIC;
    }
    p_sys->t_last_update = time( NULL );

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

    if( p_sys->p_style ) free( p_sys->p_style );
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
    var_Destroy( p_filter, "rss-ttl" );
    var_Destroy( p_filter, "rss-images" );
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
    video_format_t fmt = {0};

    subpicture_region_t *p_region;

    int i_feed, i_item;

    struct rss_feed_t *p_feed;

    vlc_mutex_lock( &p_sys->lock );

    if( p_sys->last_date
       + ( p_sys->i_cur_char == 0 && p_sys->i_cur_item == 0 ? 5 : 1 )
           /* ( ... ? 5 : 1 ) means "wait more for the 1st char" */
       * p_sys->i_speed > date )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }

    /* Do we need to update the feeds ? */
    if( p_sys->i_ttl
        && time( NULL ) > p_sys->t_last_update + (time_t)p_sys->i_ttl )
    {
        msg_Dbg( p_filter, "Forcing update of all the RSS feeds" );
        if( FetchRSS( p_filter ) )
        {
            msg_Err( p_filter, "failed while fetching RSS ... too bad" );
            vlc_mutex_unlock( &p_sys->lock );
            return NULL; /* FIXME : we most likely messed up all the data,
                          * so we might need to do something about it */
        }
        p_sys->t_last_update = time( NULL );
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

    fmt.i_chroma = VLC_FOURCC('T','E','X','T');

    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt );
    if( !p_spu->p_region )
    {
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }

    /* Generate the string that will be displayed. This string is supposed to
       be p_sys->i_length characters long. */
    i_item = p_sys->i_cur_item;
    i_feed = p_sys->i_cur_feed;
    p_feed = &p_sys->p_feeds[p_sys->i_cur_feed];

    if( p_feed->p_pic )
    {
        /* Don't display the feed's title if we have an image */
        snprintf( p_sys->psz_marquee, p_sys->i_length, "%s",
                  p_sys->p_feeds[i_feed].p_items[i_item].psz_title
                  +p_sys->i_cur_char );
    }
    else
    {
        snprintf( p_sys->psz_marquee, p_sys->i_length, "%s : %s",
                  p_sys->p_feeds[i_feed].psz_title,
                  p_sys->p_feeds[i_feed].p_items[i_item].psz_title
                  +p_sys->i_cur_char );
    }

    while( strlen( p_sys->psz_marquee ) < (unsigned int)p_sys->i_length )
    {
        i_item++;
        if( i_item == p_sys->p_feeds[i_feed].i_items ) break;
        snprintf( strchr( p_sys->psz_marquee, 0 ),
                  p_sys->i_length - strlen( p_sys->psz_marquee ),
                  " - %s",
                  p_sys->p_feeds[i_feed].p_items[i_item].psz_title );
    }

    /* Calls to snprintf might split multibyte UTF8 chars ...
     * which freetype doesn't like. */
    {
        char *a = strdup( p_sys->psz_marquee );
        char *a2 = a;
        char *b = p_sys->psz_marquee;
        EnsureUTF8( p_sys->psz_marquee );
        /* we want to use ' ' instead of '?' for erroneous chars */
        while( *b != '\0' )
        {
            if( *b != *a ) *b = ' ';
            b++;a++;
        }
        free( a2 );
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
    p_spu->p_region->p_style = p_sys->p_style;

    if( p_feed->p_pic )
    {
        /* Display the feed's image */
        picture_t *p_pic = p_feed->p_pic;
        video_format_t fmt_out = {0};

        fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
        fmt_out.i_aspect = VOUT_ASPECT_FACTOR;
        fmt_out.i_sar_num = fmt_out.i_sar_den = 1;
        fmt_out.i_width =
            fmt_out.i_visible_width = p_pic->p[Y_PLANE].i_visible_pitch;
        fmt_out.i_height =
            fmt_out.i_visible_height = p_pic->p[Y_PLANE].i_visible_lines;

        p_region = p_spu->pf_create_region( VLC_OBJECT( p_filter ), &fmt_out );
        if( !p_region )
        {
            msg_Err( p_filter, "cannot allocate SPU region" );
        }
        else
        {
            vout_CopyPicture( p_filter, &p_region->picture, p_pic );
            p_spu->p_region->p_next = p_region;
        }

        /* Offset text to display right next to the image */
        p_spu->p_region->i_x = p_pic->p[Y_PLANE].i_visible_pitch;
    }

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
 * download and resize image located at psz_url
 ***************************************************************************/
picture_t *LoadImage( filter_t *p_filter, const char *psz_url )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    video_format_t fmt_in={0}, fmt_out={0};
    picture_t *p_orig, *p_pic=NULL;
    image_handler_t *p_handler = image_HandlerCreate( p_filter );

    char *psz_local;

    psz_local = ToLocale( psz_url );
    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
    p_orig = image_ReadUrl( p_handler, psz_local, &fmt_in, &fmt_out );
    LocaleFree( psz_local );

    if( !p_orig )
    {
        msg_Warn( p_filter, "Unable to read image %s", psz_url );
    }
    else if( p_sys->p_style->i_font_size > 0 )
    {

        fmt_in.i_chroma = VLC_FOURCC('Y','U','V','A');
        fmt_in.i_height = p_orig->p[Y_PLANE].i_visible_lines;
        fmt_in.i_width = p_orig->p[Y_PLANE].i_visible_pitch;
        fmt_out.i_width = p_orig->p[Y_PLANE].i_visible_pitch
            *p_sys->p_style->i_font_size/p_orig->p[Y_PLANE].i_visible_lines;
        fmt_out.i_height = p_sys->p_style->i_font_size;

        p_pic = image_Convert( p_handler, p_orig, &fmt_in, &fmt_out );
        p_orig->pf_release( p_orig );
        if( !p_pic )
        {
            msg_Warn( p_filter, "Error while converting %s", psz_url );
        }
    }
    else
    {
        p_pic = p_orig;
    }

    image_HandlerDelete( p_handler );

    return p_pic;
}

/****************************************************************************
 * remove all ' ' '\t' '\n' '\r' characters from the begining and end of the
 * string.
 ***************************************************************************/
char *removeWhiteChars( char *psz_src )
{
    char *psz_src2 = strdup( psz_src );
    char *psz_clean = strdup( psz_src2 );
    char *psz_clean2;
    int i;
    while( ( *psz_clean == ' ' || *psz_clean == '\t'
           || *psz_clean == '\n' || *psz_clean == '\r' )
           && *psz_clean != '\0' )
    {
        psz_clean++;
    }
    i = strlen( psz_clean );
    while( --i > 0 &&
         ( psz_clean[i] == ' ' || psz_clean[i] == '\t'
        || psz_clean[i] == '\n' || psz_clean[i] == '\r' ) );
    psz_clean[i+1] = '\0';
    psz_clean2 = strdup( psz_clean );
    free( psz_src2 );
    return psz_clean2;
}

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
    vlc_bool_t b_is_item;
    vlc_bool_t b_is_image;
    int i_int;

    FreeRSS( p_filter );
    p_sys->i_feeds = 1;
    i_int = 0;
    while( p_sys->psz_urls[i_int] != 0 )
        if( p_sys->psz_urls[i_int++] == '|' )
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
        psz_buffer = strchr( psz_buffer, '|' );
        if( psz_buffer != NULL ) psz_buffer[0] = 0;

        p_feed->psz_title = NULL;
        p_feed->psz_description = NULL;
        p_feed->psz_link = NULL;
        p_feed->psz_image = NULL;
        p_feed->p_pic = NULL;
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
        b_is_item = VLC_FALSE;
        b_is_image = VLC_FALSE;

        while( xml_ReaderRead( p_xml_reader ) == 1 )
        {
            switch( xml_ReaderNodeType( p_xml_reader ) )
            {
                // Error
                case -1:
                    return 1;

                case XML_READER_STARTELEM:
                    if( psz_eltname )
                    {
                        free( psz_eltname );
                        psz_eltname = NULL;
                    }
                    psz_eltname = xml_ReaderName( p_xml_reader );
                    if( !psz_eltname )
                    {
                        return 1;
                    }
#                   define RSS_DEBUG
#                   ifdef RSS_DEBUG
                    msg_Dbg( p_filter, "element name : %s", psz_eltname );
#                   endif
                    if( !strcmp( psz_eltname, "item" )
                     || !strcmp( psz_eltname, "entry" ) )
                    {
                        b_is_item = VLC_TRUE;
                        p_feed->i_items++;
                        p_feed->p_items = (struct rss_item_t *)realloc( p_feed->p_items, p_feed->i_items * sizeof( struct rss_item_t ) );
                        p_feed->p_items[p_feed->i_items-1].psz_title = NULL;
                        p_feed->p_items[p_feed->i_items-1].psz_description
                                                                     = NULL;
                        p_feed->p_items[p_feed->i_items-1].psz_link = NULL;
                    }
                    else if( !strcmp( psz_eltname, "image" ) )
                    {
                        b_is_image = VLC_TRUE;
                    }
                    break;

                case XML_READER_ENDELEM:
                    if( psz_eltname )
                    {
                        free( psz_eltname );
                        psz_eltname = NULL;
                    }
                    psz_eltname = xml_ReaderName( p_xml_reader );
                    if( !psz_eltname )
                    {
                        return 1;
                    }
#                   ifdef RSS_DEBUG
                    msg_Dbg( p_filter, "element end : %s", psz_eltname );
#                   endif
                    if( !strcmp( psz_eltname, "item" )
                     || !strcmp( psz_eltname, "entry" ) )
                    {
                        b_is_item = VLC_FALSE;
                        i_item++;
                    }
                    else if( !strcmp( psz_eltname, "image" ) )
                    {
                        b_is_image = VLC_FALSE;
                    }
                    free( psz_eltname );
                    psz_eltname = NULL;
                    break;

                case XML_READER_TEXT:
                    if( !psz_eltname ) break;
                    psz_eltvalue = xml_ReaderValue( p_xml_reader );
                    if( !psz_eltvalue )
                    {
                        return 1;
                    }
                    else
                    {
                        char *psz_clean;
                        psz_clean = removeWhiteChars( psz_eltvalue );
                        free( psz_eltvalue ); psz_eltvalue = psz_clean;
                    }
#                   ifdef RSS_DEBUG
                    msg_Dbg( p_filter, "  text : <%s>", psz_eltvalue );
#                   endif
                    if( b_is_item == VLC_TRUE )
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
                            psz_eltvalue = NULL;
                        }
                    }
                    else if( b_is_image == VLC_TRUE )
                    {
                        if( !strcmp( psz_eltname, "url" ) )
                        {
                            p_feed->psz_image = psz_eltvalue;
                            if( p_sys->b_images == VLC_TRUE )
                                p_feed->p_pic = LoadImage( p_filter,
                                                           p_feed->psz_image );
                        }
                        else
                        {
                            free( psz_eltvalue );
                            psz_eltvalue = NULL;
                        }
                    }
                    else
                    {
                        if( !strcmp( psz_eltname, "title" ) )
                        {
                            p_feed->psz_title = psz_eltvalue;
                        }
                        else if( !strcmp( psz_eltname, "link" ) )
                        {
                            p_feed->psz_link = psz_eltvalue;
                        }
                        else if( !strcmp( psz_eltname, "description" )
                              || !strcmp( psz_eltname, "subtitle" ) )
                        {
                            p_feed->psz_description = psz_eltvalue;
                        }
                        else
                        {
                            free( psz_eltvalue );
                            psz_eltvalue = NULL;
                        }
                    }
                    break;
            }
        }

        if( p_xml_reader && p_xml ) xml_ReaderDelete( p_xml, p_xml_reader );
        if( p_stream ) stream_Delete( p_stream );
        msg_Dbg( p_filter, "Done with %s RSS feed.", psz_feed );
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
        free( p_feed->psz_image );
        if( p_feed->p_pic != NULL )
            p_feed->p_pic->pf_release( p_feed->p_pic );
    }
    free( p_sys->p_feeds );
    p_sys->i_feeds = 0;
}
