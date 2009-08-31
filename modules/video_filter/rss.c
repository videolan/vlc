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
 * Atom : http://www.ietf.org/rfc/rfc4287.txt
 * RSS : http://www.rssboard.org/rss-specification
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
#include <vlc_block.h>
#include <vlc_osd.h>

#include <vlc_block.h>
#include <vlc_stream.h>
#include <vlc_xml.h>
#include <vlc_charset.h>

#include <vlc_image.h>

#include <time.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );
static subpicture_t *Filter( filter_t *, mtime_t );

static int FetchRSS( filter_t * );
static void FreeRSS( filter_t * );
static int ParseUrls( filter_t *, char * );

static const int pi_color_values[] = {
               0xf0000000, 0x00000000, 0x00808080, 0x00C0C0C0,
               0x00FFFFFF, 0x00800000, 0x00FF0000, 0x00FF00FF, 0x00FFFF00,
               0x00808000, 0x00008000, 0x00008080, 0x0000FF00, 0x00800080,
               0x00000080, 0x000000FF, 0x0000FFFF};
static const char *const ppsz_color_descriptions[] = {
               N_("Default"), N_("Black"),
               N_("Gray"), N_("Silver"), N_("White"), N_("Maroon"), N_("Red"),
               N_("Fuchsia"), N_("Yellow"), N_("Olive"), N_("Green"),
               N_("Teal"), N_("Lime"), N_("Purple"), N_("Navy"), N_("Blue"),
               N_("Aqua") };

/*****************************************************************************
 * filter_sys_t: rss filter descriptor
 *****************************************************************************/

typedef struct rss_item_t
{
    char *psz_title;
    char *psz_description;
    char *psz_link;
} rss_item_t;

typedef struct rss_feed_t
{
    char *psz_url;
    char *psz_title;
    char *psz_description;
    char *psz_link;
    char *psz_image;
    picture_t *p_pic;

    int i_items;
    rss_item_t *p_items;
} rss_feed_t;

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

    int i_feeds;
    rss_feed_t *p_feeds;

    int i_ttl;
    time_t t_last_update;
    bool b_images;
    int i_title;

    int i_cur_feed;
    int i_cur_item;
    int i_cur_char;
};

#define MSG_TEXT N_("Feed URLs")
#define MSG_LONGTEXT N_("RSS/Atom feed '|' (pipe) separated URLs.")
#define SPEED_TEXT N_("Speed of feeds")
#define SPEED_LONGTEXT N_("Speed of the RSS/Atom feeds in microseconds (bigger is slower).")
#define LENGTH_TEXT N_("Max length")
#define LENGTH_LONGTEXT N_("Maximum number of characters displayed on the " \
                "screen." )
#define TTL_TEXT N_("Refresh time")
#define TTL_LONGTEXT N_("Number of seconds between each forced refresh " \
        "of the feeds. 0 means that the feeds are never updated." )
#define IMAGE_TEXT N_("Feed images")
#define IMAGE_LONGTEXT N_("Display feed images if available.")

#define POSX_TEXT N_("X offset")
#define POSX_LONGTEXT N_("X offset, from the left screen edge." )
#define POSY_TEXT N_("Y offset")
#define POSY_LONGTEXT N_("Y offset, down from the top." )
#define OPACITY_TEXT N_("Opacity")
#define OPACITY_LONGTEXT N_("Opacity (inverse of transparency) of " \
    "overlay text. 0 = transparent, 255 = totally opaque." )

#define SIZE_TEXT N_("Font size, pixels")
#define SIZE_LONGTEXT N_("Font size, in pixels. Default is -1 (use default " \
    "font size)." )

#define COLOR_TEXT N_("Color")
#define COLOR_LONGTEXT N_("Color of the text that will be rendered on "\
    "the video. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white" )

#define POS_TEXT N_("Text position")
#define POS_LONGTEXT N_( \
  "You can enforce the text position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom; you can " \
  "also use combinations of these values, eg 6 = top-right).")

#define TITLE_TEXT N_("Title display mode")
#define TITLE_LONGTEXT N_("Title display mode. Default is 0 (hidden) if the feed has an image and feed images are enabled, 1 otherwise.")

static const int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_pos_descriptions[] =
     { N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
     N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

enum title_modes {
    default_title=-1,
    hide_title,
    prepend_title,
    scroll_title };

static const int pi_title_modes[] = { default_title, hide_title, prepend_title, scroll_title };
static const char *const ppsz_title_modes[] =
    { N_("Default"), N_("Don't show"), N_("Always visible"), N_("Scroll with feed") };

#define CFG_PREFIX "rss-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_capability( "sub filter", 1 )
    set_shortname( "RSS / Atom" )
    set_callbacks( CreateFilter, DestroyFilter )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SUBPIC )
    add_string( CFG_PREFIX "urls", "rss", NULL, MSG_TEXT, MSG_LONGTEXT, false )

    set_section( N_("Position"), NULL )
    add_integer( CFG_PREFIX "x", 0, NULL, POSX_TEXT, POSX_LONGTEXT, true )
    add_integer( CFG_PREFIX "y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, true )
    add_integer( CFG_PREFIX "position", -1, NULL, POS_TEXT, POS_LONGTEXT, false )
        change_integer_list( pi_pos_values, ppsz_pos_descriptions, NULL )

    set_section( N_("Font"), NULL )
    /* 5 sets the default to top [1] left [4] */
    add_integer_with_range( CFG_PREFIX "opacity", 255, 0, 255, NULL,
        OPACITY_TEXT, OPACITY_LONGTEXT, false )
    add_integer( CFG_PREFIX "color", 0xFFFFFF, NULL, COLOR_TEXT, COLOR_LONGTEXT,
                  false )
        change_integer_list( pi_color_values, ppsz_color_descriptions, NULL )
    add_integer( CFG_PREFIX "size", -1, NULL, SIZE_TEXT, SIZE_LONGTEXT, false )

    set_section( N_("Misc"), NULL )
    add_integer( CFG_PREFIX "speed", 100000, NULL, SPEED_TEXT, SPEED_LONGTEXT,
                 false )
    add_integer( CFG_PREFIX "length", 60, NULL, LENGTH_TEXT, LENGTH_LONGTEXT,
                 false )
    add_integer( CFG_PREFIX "ttl", 1800, NULL, TTL_TEXT, TTL_LONGTEXT, false )
    add_bool( CFG_PREFIX "images", 1, NULL, IMAGE_TEXT, IMAGE_LONGTEXT, false )
    add_integer( CFG_PREFIX "title", default_title, NULL, TITLE_TEXT, TITLE_LONGTEXT, false )
        change_integer_list( pi_title_modes, ppsz_title_modes, NULL )

    set_description( N_("RSS and Atom feed display") )
    add_shortcut( "rss" )
    add_shortcut( "atom" )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "urls", "x", "y", "position", "color", "size", "speed", "length",
    "ttl", "images", "title", NULL
};

/*****************************************************************************
 * CreateFilter: allocates RSS video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    int i_feed;
    int i_ret = VLC_ENOMEM;
    char *psz_urls;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    psz_urls = var_CreateGetString( p_filter, CFG_PREFIX "urls" );
    p_sys->i_title = var_CreateGetInteger( p_filter, CFG_PREFIX "title" );
    p_sys->i_cur_feed = 0;
    p_sys->i_cur_item = p_sys->i_title == scroll_title ? -1 : 0;
    p_sys->i_cur_char = 0;
    p_sys->i_feeds = 0;
    p_sys->p_feeds = NULL;
    p_sys->i_speed = var_CreateGetInteger( p_filter, CFG_PREFIX "speed" );
    p_sys->i_length = var_CreateGetInteger( p_filter, CFG_PREFIX "length" );
    p_sys->i_ttl = __MAX( 0, var_CreateGetInteger( p_filter, CFG_PREFIX "ttl" ) );
    p_sys->b_images = var_CreateGetBool( p_filter, CFG_PREFIX "images" );

    p_sys->psz_marquee = malloc( p_sys->i_length + 1 );
    if( p_sys->psz_marquee == NULL )
        goto error;
    p_sys->psz_marquee[p_sys->i_length] = '\0';

    p_sys->p_style = text_style_New();
    if( p_sys->p_style == NULL )
        goto error;

    p_sys->i_xoff = var_CreateGetInteger( p_filter, CFG_PREFIX "x" );
    p_sys->i_yoff = var_CreateGetInteger( p_filter, CFG_PREFIX "y" );
    p_sys->i_pos = var_CreateGetInteger( p_filter, CFG_PREFIX "position" );
    p_sys->p_style->i_font_alpha = 255 - var_CreateGetInteger( p_filter, CFG_PREFIX "opacity" );
    p_sys->p_style->i_font_color = var_CreateGetInteger( p_filter, CFG_PREFIX "color" );
    p_sys->p_style->i_font_size = var_CreateGetInteger( p_filter, CFG_PREFIX "size" );

    if( p_sys->b_images == true && p_sys->p_style->i_font_size == -1 )
    {
        msg_Warn( p_filter, "rss-size wasn't specified. Feed images will thus be displayed without being resized" );
    }

    /* Parse the urls */
    ParseUrls( p_filter, psz_urls );
    free( psz_urls );

    if( FetchRSS( p_filter ) )
    {
        msg_Err( p_filter, "failed while fetching RSS ... too bad" );
        text_style_Delete( p_sys->p_style );
        i_ret = VLC_EGENERIC;
        goto error;
    }
    p_sys->t_last_update = time( NULL );

    if( p_sys->i_feeds == 0 )
    {
        text_style_Delete( p_sys->p_style );
        i_ret = VLC_EGENERIC;
        goto error;
    }
    for( i_feed=0; i_feed < p_sys->i_feeds; i_feed ++ )
    {
        if( p_sys->p_feeds[i_feed].i_items == 0 )
        {
            DestroyFilter( p_this );
            return VLC_EGENERIC;
        }
    }

    /* Misc init */
    vlc_mutex_init( &p_sys->lock );
    p_filter->pf_sub_filter = Filter;
    p_sys->last_date = (mtime_t)0;

    return VLC_SUCCESS;

error:
    free( p_sys->psz_marquee );
    free( p_sys );
    return i_ret;
}
/*****************************************************************************
 * DestroyFilter: destroy RSS video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    text_style_Delete( p_sys->p_style );
    free( p_sys->psz_marquee );
    FreeRSS( p_filter );
    free( p_sys );
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
    subpicture_region_t *p_region;

    int i_feed, i_item;
    rss_feed_t *p_feed;

    memset( &fmt, 0, sizeof(video_format_t) );

    vlc_mutex_lock( &p_sys->lock );

    if( p_sys->last_date
       + ( p_sys->i_cur_char == 0 && p_sys->i_cur_item == ( p_sys->i_title == scroll_title ? -1 : 0 ) ? 5 : 1 )
           /* ( ... ? 5 : 1 ) means "wait 5 times more for the 1st char" */
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
            msg_Err( p_filter, "Failed while fetching RSS ... too bad" );
            vlc_mutex_unlock( &p_sys->lock );
            return NULL; /* FIXME : we most likely messed up all the data,
                          * so we might need to do something about it */
        }
        p_sys->t_last_update = time( NULL );
    }

    p_sys->last_date = date;
    p_sys->i_cur_char++;
    if( p_sys->i_cur_item == -1 ? p_sys->p_feeds[p_sys->i_cur_feed].psz_title[p_sys->i_cur_char] == 0 : p_sys->p_feeds[p_sys->i_cur_feed].p_items[p_sys->i_cur_item].psz_title[p_sys->i_cur_char] == 0 )
    {
        p_sys->i_cur_char = 0;
        p_sys->i_cur_item++;
        if( p_sys->i_cur_item >= p_sys->p_feeds[p_sys->i_cur_feed].i_items )
        {
            if( p_sys->i_title == scroll_title )
                p_sys->i_cur_item = -1;
            else
                p_sys->i_cur_item = 0;
            p_sys->i_cur_feed = (p_sys->i_cur_feed + 1)%p_sys->i_feeds;
        }
    }

    p_spu = filter_NewSubpicture( p_filter );
    if( !p_spu )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }

    fmt.i_chroma = VLC_CODEC_TEXT;

    p_spu->p_region = subpicture_region_New( &fmt );
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
    p_feed = &p_sys->p_feeds[i_feed];

    if( ( p_feed->p_pic && p_sys->i_title == default_title )
        || p_sys->i_title == hide_title )
    {
        /* Don't display the feed's title if we have an image */
        snprintf( p_sys->psz_marquee, p_sys->i_length, "%s",
                  p_sys->p_feeds[i_feed].p_items[i_item].psz_title
                  +p_sys->i_cur_char );
    }
    else if( ( !p_feed->p_pic && p_sys->i_title == default_title )
             || p_sys->i_title == prepend_title )
    {
        snprintf( p_sys->psz_marquee, p_sys->i_length, "%s : %s",
                  p_sys->p_feeds[i_feed].psz_title,
                  p_sys->p_feeds[i_feed].p_items[i_item].psz_title
                  +p_sys->i_cur_char );
    }
    else /* scrolling title */
    {
        if( i_item == -1 )
            snprintf( p_sys->psz_marquee, p_sys->i_length, "%s : %s",
                      p_sys->p_feeds[i_feed].psz_title + p_sys->i_cur_char,
                      p_sys->p_feeds[i_feed].p_items[i_item+1].psz_title );
        else
            snprintf( p_sys->psz_marquee, p_sys->i_length, "%s",
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
    if( p_sys->p_style->i_font_size > 0 )
        p_spu->p_region->fmt.i_visible_height = p_sys->p_style->i_font_size;
    p_spu->i_start = date;
    p_spu->i_stop  = 0;
    p_spu->b_ephemer = true;

    /*  where to locate the string: */
    if( p_sys->i_pos < 0 )
    {   /*  set to an absolute xy */
        p_spu->p_region->i_align = OSD_ALIGN_LEFT | OSD_ALIGN_TOP;
        p_spu->b_absolute = true;
    }
    else
    {   /* set to one of the 9 relative locations */
        p_spu->p_region->i_align = p_sys->i_pos;
        p_spu->b_absolute = false;
    }

    p_spu->p_region->p_style = text_style_Duplicate( p_sys->p_style );

    if( p_feed->p_pic )
    {
        /* Display the feed's image */
        picture_t *p_pic = p_feed->p_pic;
        video_format_t fmt_out;

        memset( &fmt_out, 0, sizeof(video_format_t) );

        fmt_out.i_chroma = VLC_CODEC_YUVA;
        fmt_out.i_aspect = VOUT_ASPECT_FACTOR;
        fmt_out.i_sar_num = fmt_out.i_sar_den = 1;
        fmt_out.i_width =
            fmt_out.i_visible_width = p_pic->p[Y_PLANE].i_visible_pitch;
        fmt_out.i_height =
            fmt_out.i_visible_height = p_pic->p[Y_PLANE].i_visible_lines;

        p_region = subpicture_region_New( &fmt_out );
        if( !p_region )
        {
            msg_Err( p_filter, "cannot allocate SPU region" );
        }
        else
        {
            p_region->i_x = p_sys->i_xoff;
            p_region->i_y = p_sys->i_yoff;
            /* FIXME the copy is probably not needed anymore */
            picture_Copy( p_region->p_picture, p_pic );
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

#undef LoadImage /* do not conflict with Win32 API */

/****************************************************************************
 * download and resize image located at psz_url
 ***************************************************************************/
static picture_t *LoadImage( filter_t *p_filter, const char *psz_url )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    video_format_t fmt_in;
    video_format_t fmt_out;
    picture_t *p_orig;
    picture_t *p_pic = NULL;
    image_handler_t *p_handler = image_HandlerCreate( p_filter );

    memset( &fmt_in, 0, sizeof(video_format_t) );
    memset( &fmt_out, 0, sizeof(video_format_t) );

    fmt_out.i_chroma = VLC_CODEC_YUVA;
    p_orig = image_ReadUrl( p_handler, psz_url, &fmt_in, &fmt_out );

    if( !p_orig )
    {
        msg_Warn( p_filter, "Unable to read image %s", psz_url );
    }
    else if( p_sys->p_style->i_font_size > 0 )
    {

        fmt_in.i_chroma = VLC_CODEC_YUVA;
        fmt_in.i_height = p_orig->p[Y_PLANE].i_visible_lines;
        fmt_in.i_width = p_orig->p[Y_PLANE].i_visible_pitch;
        fmt_out.i_width = p_orig->p[Y_PLANE].i_visible_pitch
            *p_sys->p_style->i_font_size/p_orig->p[Y_PLANE].i_visible_lines;
        fmt_out.i_height = p_sys->p_style->i_font_size;

        p_pic = image_Convert( p_handler, p_orig, &fmt_in, &fmt_out );
        picture_Release( p_orig );
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
static char *removeWhiteChars( const char *psz_src )
{
    char *psz_src2,*psz_clean, *psz_clean2;
    psz_src2 = psz_clean = strdup( psz_src );
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
 * Parse url list, psz_urls must be non empty
 ***************************************************************************/
static int ParseUrls( filter_t *p_filter, char *psz_urls )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    char *psz_urls2 = psz_urls;

    p_sys->i_feeds = 1;

    /* Count the number of feeds */
    while( *psz_urls )
    {
        if( *psz_urls == '|' )
            p_sys->i_feeds++;
        psz_urls++;
    }

    /* Allocate the structure */
    p_sys->p_feeds = malloc( p_sys->i_feeds * sizeof( rss_feed_t ) );
    if( !p_sys->p_feeds )
        return VLC_ENOMEM;

    /* Loop on all urls and fill in the struct */
    psz_urls = psz_urls2;
    for( int i = 0; i < p_sys->i_feeds; i++ )
    {
        rss_feed_t* p_feed = p_sys->p_feeds + i;
        char *psz_end;

        if( i < p_sys->i_feeds - 1 )
        {
            psz_end = strchr( psz_urls, '|' );
            *psz_end = '\0';
        }
        else
            psz_end = psz_urls;

        p_feed->i_items = 0;
        p_feed->p_items = NULL;
        p_feed->psz_title = NULL;
        p_feed->psz_link = NULL;
        p_feed->psz_description = NULL;
        p_feed->psz_image = NULL;
        p_feed->p_pic = NULL;
        p_feed->psz_url = strdup( psz_urls );

        psz_urls = psz_end + 1;
    }

    return VLC_SUCCESS;
}


/****************************************************************************
 * FetchRSS (or Atom) feeds
 ***************************************************************************/
static int FetchRSS( filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    stream_t *p_stream;
    xml_t *p_xml;
    xml_reader_t *p_xml_reader;
    int i_ret = 1;

    char *psz_eltname = NULL;
    char *psz_eltvalue = NULL;

    int i_feed;
    int i_item;
    bool b_is_item;
    bool b_is_image;

    p_xml = xml_Create( p_filter );
    if( !p_xml )
    {
        msg_Err( p_filter, "Failed to open XML parser" );
        return 1;
    }

    for( i_feed = 0; i_feed < p_sys->i_feeds; i_feed++ )
    {
        rss_feed_t *p_feed = p_sys->p_feeds+i_feed;

        FREENULL( p_feed->psz_title );
        FREENULL( p_feed->psz_description );
        FREENULL( p_feed->psz_link );
        FREENULL( p_feed->psz_image );
        if( p_feed->p_pic )
        {
            picture_Release( p_feed->p_pic );
            p_feed->p_pic = NULL;
        }
        for( int i = 0; i < p_feed->i_items; i++ )
        {
            rss_item_t *p_item = p_feed->p_items + i;
            free( p_item->psz_title );
            free( p_item->psz_link );
            free( p_item->psz_description );
        }
        p_feed->i_items = 0;
        FREENULL( p_feed->p_items );

        msg_Dbg( p_filter, "opening %s RSS/Atom feed ...", p_feed->psz_url );

        p_stream = stream_UrlNew( p_filter, p_feed->psz_url );
        if( !p_stream )
        {
            msg_Err( p_filter, "Failed to open %s for reading", p_feed->psz_url );
            p_xml_reader = NULL;
            goto error;
        }

        p_xml_reader = xml_ReaderCreate( p_xml, p_stream );
        if( !p_xml_reader )
        {
            msg_Err( p_filter, "Failed to open %s for parsing", p_feed->psz_url );
            goto error;
        }

        i_item = 0;
        b_is_item = false;
        b_is_image = false;

        while( xml_ReaderRead( p_xml_reader ) == 1 )
        {
            switch( xml_ReaderNodeType( p_xml_reader ) )
            {
                // Error
                case -1:
                    goto error;

                case XML_READER_STARTELEM:
                    free( psz_eltname );
                    psz_eltname = xml_ReaderName( p_xml_reader );
                    if( !psz_eltname )
                        goto error;

#                   ifdef RSS_DEBUG
                    msg_Dbg( p_filter, "element name: %s", psz_eltname );
#                   endif
                    if( !strcmp( psz_eltname, "item" ) /* rss */
                     || !strcmp( psz_eltname, "entry" ) ) /* atom */
                    {
                        b_is_item = true;
                        p_feed->i_items++;
                        p_feed->p_items = realloc( p_feed->p_items, p_feed->i_items * sizeof( rss_item_t ) );
                        p_feed->p_items[p_feed->i_items-1].psz_title = NULL;
                        p_feed->p_items[p_feed->i_items-1].psz_description
                                                                     = NULL;
                        p_feed->p_items[p_feed->i_items-1].psz_link = NULL;
                    }
                    else if( !strcmp( psz_eltname, "image" ) ) /* rss */
                    {
                        b_is_image = true;
                    }
                    else if( !strcmp( psz_eltname, "link" ) ) /* atom */
                    {
                        char *psz_href = NULL;
                        char *psz_rel = NULL;
                        while( xml_ReaderNextAttr( p_xml_reader )
                               == VLC_SUCCESS )
                        {
                            char *psz_name = xml_ReaderName( p_xml_reader );
                            char *psz_value = xml_ReaderValue( p_xml_reader );
                            if( !strcmp( psz_name, "rel" ) )
                            {
                                if( psz_rel )
                                {
                                    msg_Dbg( p_filter, "\"rel\" attribute of link atom duplicated (last value: %s)", psz_value );
                                    free( psz_rel );
                                }
                                psz_rel = psz_value;
                            }
                            else if( !strcmp( psz_name, "href" ) )
                            {
                                if( psz_href )
                                {
                                    msg_Dbg( p_filter, "\"href\" attribute of link atom duplicated (last value: %s)", psz_href );
                                    free( psz_href );
                                }
                                psz_href = psz_value;
                            }
                            else
                            {
                                free( psz_value );
                            }
                            free( psz_name );
                        }
                        if( psz_rel && psz_href )
                        {
                            if( !strcmp( psz_rel, "alternate" )
                                && b_is_item == false
                                && b_is_image == false
                                && !p_feed->psz_link )
                            {
                                p_feed->psz_link = psz_href;
                            }
                            /* this isn't in the rfc but i found some ... */
                            else if( ( !strcmp( psz_rel, "logo" )
                                    || !strcmp( psz_rel, "icon" ) )
                                    && b_is_item == false
                                    && b_is_image == false
                                    && !p_feed->psz_image )
                            {
                                p_feed->psz_image = psz_href;
                            }
                            else
                            {
                                free( psz_href );
                            }
                        }
                        else
                        {
                            free( psz_href );
                        }
                        free( psz_rel );
                    }
                    break;

                case XML_READER_ENDELEM:
                    free( psz_eltname );
                    psz_eltname = xml_ReaderName( p_xml_reader );
                    if( !psz_eltname )
                        goto error;

#                   ifdef RSS_DEBUG
                    msg_Dbg( p_filter, "element end : %s", psz_eltname );
#                   endif
                    if( !strcmp( psz_eltname, "item" ) /* rss */
                     || !strcmp( psz_eltname, "entry" ) ) /* atom */
                    {
                        b_is_item = false;
                        i_item++;
                    }
                    else if( !strcmp( psz_eltname, "image" ) ) /* rss */
                    {
                        b_is_image = false;
                    }
                    FREENULL( psz_eltname );
                    break;

                case XML_READER_TEXT:
                    if( !psz_eltname ) break;
                    psz_eltvalue = xml_ReaderValue( p_xml_reader );
                    if( !psz_eltvalue )
                    {
                        goto error;
                    }
                    else
                    {
                        char *psz_clean = removeWhiteChars( psz_eltvalue );
                        free( psz_eltvalue );
                        psz_eltvalue = psz_clean;
                    }
#                   ifdef RSS_DEBUG
                    msg_Dbg( p_filter, "  text : <%s>", psz_eltvalue );
#                   endif
                    if( b_is_item == true )
                    {
                        rss_item_t *p_item = p_feed->p_items+i_item;
                        if( !strcmp( psz_eltname, "title" ) /* rss/atom */
                            && !p_item->psz_title )
                        {
                            p_item->psz_title = psz_eltvalue;
                        }
                        else if( !strcmp( psz_eltname, "link" ) /* rss */
                                 && !p_item->psz_link )
                        {
                            p_item->psz_link = psz_eltvalue;
                        }
                        else if((!strcmp( psz_eltname, "description" ) /* rss */
                              || !strcmp( psz_eltname, "summary" ) ) /* atom */
                              && !p_item->psz_description )
                        {
                            p_item->psz_description = psz_eltvalue;
                        }
                        else
                        {
                            FREENULL( psz_eltvalue );
                        }
                    }
                    else if( b_is_image == true )
                    {
                        if( !strcmp( psz_eltname, "url" ) /* rss */
                            && !p_feed->psz_image )
                        {
                            p_feed->psz_image = psz_eltvalue;
                        }
                        else
                        {
                            FREENULL( psz_eltvalue );
                        }
                    }
                    else
                    {
                        if( !strcmp( psz_eltname, "title" ) /* rss/atom */
                            && !p_feed->psz_title )
                        {
                            p_feed->psz_title = psz_eltvalue;
                        }
                        else if( !strcmp( psz_eltname, "link" ) /* rss */
                                 && !p_feed->psz_link )
                        {
                            p_feed->psz_link = psz_eltvalue;
                        }
                        else if((!strcmp( psz_eltname, "description" ) /* rss */
                              || !strcmp( psz_eltname, "subtitle" ) ) /* atom */
                              && !p_feed->psz_description )
                        {
                            p_feed->psz_description = psz_eltvalue;
                        }
                        else if( ( !strcmp( psz_eltname, "logo" ) /* atom */
                              || !strcmp( psz_eltname, "icon" ) ) /* atom */
                              && !p_feed->psz_image )
                        {
                            p_feed->psz_image = psz_eltvalue;
                        }
                        else
                        {
                            FREENULL( psz_eltvalue );
                        }
                    }
                    break;
            }
        }

        if( p_sys->b_images == true
            && p_feed->psz_image && !p_feed->p_pic )
        {
            p_feed->p_pic = LoadImage( p_filter, p_feed->psz_image );
        }

        xml_ReaderDelete( p_xml, p_xml_reader );
        stream_Delete( p_stream );
        msg_Dbg( p_filter, "done with %s RSS/Atom feed", p_feed->psz_url );
    }

    free( psz_eltname );
    free( psz_eltvalue );
    xml_Delete( p_xml );
    return 0;

error:
    free( psz_eltname );
    free( psz_eltvalue );

    if( p_xml_reader )
        xml_ReaderDelete( p_xml, p_xml_reader );
    if( p_stream )
        stream_Delete( p_stream );
    if( p_xml )
        xml_Delete( p_xml );

    return i_ret;
}

/****************************************************************************
 * FreeRSS
 ***************************************************************************/
static void FreeRSS( filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    for( int i_feed = 0; i_feed < p_sys->i_feeds; i_feed++ )
    {
        rss_feed_t *p_feed = p_sys->p_feeds+i_feed;
        for( int i_item = 0; i_item < p_feed->i_items; i_item++ )
        {
            rss_item_t *p_item = p_feed->p_items+i_item;
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
            picture_Release( p_feed->p_pic );
        free( p_feed->psz_url );
    }
    free( p_sys->p_feeds );
    p_sys->i_feeds = 0;
}
