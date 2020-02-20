/*****************************************************************************
 * rss.c : rss/atom feed display video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2006 VLC authors and VideoLAN
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
 *          Rémi Duraffort <ivoire -at- videolan -dot- org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

#include <time.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_subpicture.h>
#include <vlc_block.h>
#include <vlc_stream.h>
#include <vlc_xml.h>
#include <vlc_charset.h>
#include <vlc_image.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );
static subpicture_t *Filter( filter_t *, vlc_tick_t );

static struct rss_feed_t *FetchRSS( filter_t * );
static void FreeRSS( struct rss_feed_t *, int );
static int ParseUrls( filter_t *, char * );

static void Fetch( void * );

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

typedef struct
{
    vlc_mutex_t lock;
    vlc_timer_t timer;  /* Timer to refresh the rss feeds */
    bool b_fetched;

    int i_xoff, i_yoff;  /* offsets for the display string in the video window */
    int i_pos; /* permit relative positioning (top, bottom, left, right, center) */
    vlc_tick_t i_speed;
    int i_length;

    char *psz_marquee;    /* marquee string */

    text_style_t *p_style; /* font control */

    vlc_tick_t last_date;

    int i_feeds;
    rss_feed_t *p_feeds;

    bool b_images;
    int i_title;

    int i_cur_feed;
    int i_cur_item;
    int i_cur_char;
} filter_sys_t;

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
#define SIZE_LONGTEXT N_("Font size, in pixels. Default is 0 (use default " \
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

#define RSS_HELP N_("Display a RSS or ATOM Feed on your video")

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
    set_capability( "sub source", 1 )
    set_shortname( N_("RSS / Atom") )
    set_help(RSS_HELP)
    set_callbacks( CreateFilter, DestroyFilter )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SUBPIC )
    add_string( CFG_PREFIX "urls", NULL, MSG_TEXT, MSG_LONGTEXT, false )

    set_section( N_("Position"), NULL )
    add_integer( CFG_PREFIX "x", 0, POSX_TEXT, POSX_LONGTEXT, true )
    add_integer( CFG_PREFIX "y", 0, POSY_TEXT, POSY_LONGTEXT, true )
    add_integer( CFG_PREFIX "position", -1, POS_TEXT, POS_LONGTEXT, false )
        change_integer_list( pi_pos_values, ppsz_pos_descriptions )

    set_section( N_("Font"), NULL )
    /* 5 sets the default to top [1] left [4] */
    add_integer_with_range( CFG_PREFIX "opacity", 255, 0, 255,
        OPACITY_TEXT, OPACITY_LONGTEXT, false )
    add_rgb(CFG_PREFIX "color", 0xFFFFFF, COLOR_TEXT, COLOR_LONGTEXT)
        change_integer_list( pi_color_values, ppsz_color_descriptions )
    add_integer( CFG_PREFIX "size", 0, SIZE_TEXT, SIZE_LONGTEXT, false )
        change_integer_range( 0, 4096)

    set_section( N_("Misc"), NULL )
    add_integer( CFG_PREFIX "speed", 100000, SPEED_TEXT, SPEED_LONGTEXT,
                 false )
    add_integer( CFG_PREFIX "length", 60, LENGTH_TEXT, LENGTH_LONGTEXT,
                 false )
    add_integer( CFG_PREFIX "ttl", 1800, TTL_TEXT, TTL_LONGTEXT, false )
    add_bool( CFG_PREFIX "images", true, IMAGE_TEXT, IMAGE_LONGTEXT, false )
    add_integer( CFG_PREFIX "title", default_title, TITLE_TEXT, TITLE_LONGTEXT,
                 false )
        change_integer_list( pi_title_modes, ppsz_title_modes )

    set_description( N_("RSS and Atom feed display") )
    add_shortcut( "rss", "atom" )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "urls", "x", "y", "position", "opacity", "color", "size", "speed", "length",
    "ttl", "images", "title", NULL
};

/*****************************************************************************
 * CreateFilter: allocates RSS video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    char *psz_urls;
    int i_ttl;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    /* Get the urls to parse: must be non empty */
    psz_urls = var_CreateGetNonEmptyString( p_filter, CFG_PREFIX "urls" );
    if( !psz_urls )
    {
        msg_Err( p_filter, "The list of urls must not be empty" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Fill the p_sys structure with the configuration */
    p_sys->i_title = var_CreateGetInteger( p_filter, CFG_PREFIX "title" );
    p_sys->i_cur_feed = 0;
    p_sys->i_cur_item = p_sys->i_title == scroll_title ? -1 : 0;
    p_sys->i_cur_char = 0;
    p_sys->i_feeds = 0;
    p_sys->p_feeds = NULL;
    p_sys->i_speed = VLC_TICK_FROM_US( var_CreateGetInteger( p_filter, CFG_PREFIX "speed" ) );
    p_sys->i_length = var_CreateGetInteger( p_filter, CFG_PREFIX "length" );
    p_sys->b_images = var_CreateGetBool( p_filter, CFG_PREFIX "images" );

    i_ttl = __MAX( 0, var_CreateGetInteger( p_filter, CFG_PREFIX "ttl" ) );

    p_sys->psz_marquee = malloc( p_sys->i_length + 1 );
    if( p_sys->psz_marquee == NULL )
    {
        free( psz_urls );
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_sys->psz_marquee[p_sys->i_length] = '\0';

    p_sys->p_style = text_style_Create( STYLE_NO_DEFAULTS );
    if( p_sys->p_style == NULL )
        goto error;

    p_sys->i_xoff = var_CreateGetInteger( p_filter, CFG_PREFIX "x" );
    p_sys->i_yoff = var_CreateGetInteger( p_filter, CFG_PREFIX "y" );
    p_sys->i_pos = var_CreateGetInteger( p_filter, CFG_PREFIX "position" );
    p_sys->p_style->i_font_alpha = var_CreateGetInteger( p_filter, CFG_PREFIX "opacity" );
    p_sys->p_style->i_font_color = var_CreateGetInteger( p_filter, CFG_PREFIX "color" );
    p_sys->p_style->i_features |= STYLE_HAS_FONT_ALPHA | STYLE_HAS_FONT_COLOR;
    p_sys->p_style->i_font_size = var_CreateGetInteger( p_filter, CFG_PREFIX "size" );

    if( p_sys->b_images && p_sys->p_style->i_font_size == -1 )
    {
        msg_Warn( p_filter, "rss-size wasn't specified. Feed images will thus "
                            "be displayed without being resized" );
    }

    /* Parse the urls */
    if( ParseUrls( p_filter, psz_urls ) )
        goto error;

    /* Misc init */
    vlc_mutex_init( &p_sys->lock );
    p_filter->pf_sub_source = Filter;
    p_sys->last_date = (vlc_tick_t)0;
    p_sys->b_fetched = false;

    /* Create and arm the timer */
    if( vlc_timer_create( &p_sys->timer, Fetch, p_filter ) )
        goto error;
    vlc_timer_schedule_asap( p_sys->timer, vlc_tick_from_sec(i_ttl) );

    free( psz_urls );
    return VLC_SUCCESS;

error:
    if( p_sys->p_style )
        text_style_Delete( p_sys->p_style );
    free( p_sys->psz_marquee );
    free( psz_urls );
    free( p_sys );
    return VLC_ENOMEM;
}
/*****************************************************************************
 * DestroyFilter: destroy RSS video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_timer_destroy( p_sys->timer );

    text_style_Delete( p_sys->p_style );
    free( p_sys->psz_marquee );
    FreeRSS( p_sys->p_feeds, p_sys->i_feeds );
    free( p_sys );
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************
 * This function outputs subpictures at regular time intervals.
 ****************************************************************************/
static subpicture_t *Filter( filter_t *p_filter, vlc_tick_t date )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    subpicture_t *p_spu;
    video_format_t fmt;
    subpicture_region_t *p_region;

    int i_feed, i_item;
    rss_feed_t *p_feed;

    vlc_mutex_lock( &p_sys->lock );

    /* Check if the feeds have been fetched and that we have some feeds */
    /* TODO: check that we have items for each feeds */
    if( !p_sys->b_fetched && p_sys->i_feeds > 0 )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }

    if( p_sys->last_date
       + ( p_sys->i_cur_char == 0 &&
           p_sys->i_cur_item == ( p_sys->i_title == scroll_title ? -1 : 0 ) ? 5 : 1 )
           /* ( ... ? 5 : 1 ) means "wait 5 times more for the 1st char" */
       * p_sys->i_speed > date )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }

    p_sys->last_date = date;
    p_sys->i_cur_char++;

    if( p_sys->i_cur_item == -1 ?
            p_sys->p_feeds[p_sys->i_cur_feed].psz_title[p_sys->i_cur_char] == 0 :
            p_sys->p_feeds[p_sys->i_cur_feed].p_items[p_sys->i_cur_item].psz_title[p_sys->i_cur_char] == 0 )
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

    video_format_Init( &fmt, VLC_CODEC_TEXT );

    p_spu->p_region = subpicture_region_New( &fmt );
    if( !p_spu->p_region )
    {
        subpicture_Delete( p_spu );
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

    p_spu->p_region->p_text = text_segment_New(p_sys->psz_marquee);
    if( p_sys->p_style->i_font_size > 0 )
        p_spu->p_region->fmt.i_visible_height = p_sys->p_style->i_font_size;
    p_spu->i_start = date;
    p_spu->i_stop  = 0;
    p_spu->b_ephemer = true;

    /*  where to locate the string: */
    if( p_sys->i_pos < 0 )
    {   /*  set to an absolute xy */
        p_spu->p_region->i_align = SUBPICTURE_ALIGN_LEFT | SUBPICTURE_ALIGN_TOP;
        p_spu->b_absolute = true;
    }
    else
    {   /* set to one of the 9 relative locations */
        p_spu->p_region->i_align = p_sys->i_pos;
        p_spu->b_absolute = false;
    }
    p_spu->p_region->i_x = p_sys->i_xoff;
    p_spu->p_region->i_y = p_sys->i_yoff;

    p_spu->p_region->p_text->style = text_style_Duplicate( p_sys->p_style );

    if( p_feed->p_pic )
    {
        /* Display the feed's image */
        picture_t *p_pic = p_feed->p_pic;
        video_format_t fmt_out;

        video_format_Init( &fmt_out, VLC_CODEC_YUVA );

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
            p_region->i_x = p_spu->p_region->i_x;
            p_region->i_y = p_spu->p_region->i_y;
            /* FIXME the copy is probably not needed anymore */
            picture_Copy( p_region->p_picture, p_pic );
            p_spu->p_region->p_next = p_region;

            /* Offset text to display right next to the image */
            p_spu->p_region->i_x += fmt_out.i_visible_width;
        }
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
    video_format_t fmt_out;
    picture_t *p_orig;
    picture_t *p_pic = NULL;
    image_handler_t *p_handler = image_HandlerCreate( p_filter );

    video_format_Init( &fmt_out, VLC_CODEC_YUVA );

    p_orig = image_ReadUrl( p_handler, psz_url, &fmt_out );

    if( !p_orig )
    {
        msg_Warn( p_filter, "Unable to read image %s", psz_url );
    }
    else if( p_sys->p_style->i_font_size > 0 )
    {
        video_format_t fmt_in;
        video_format_Copy( &fmt_in, &fmt_out );

        fmt_in.i_height = p_orig->p[Y_PLANE].i_visible_lines;
        fmt_in.i_width = p_orig->p[Y_PLANE].i_visible_pitch;
        fmt_out.i_width = p_orig->p[Y_PLANE].i_visible_pitch
            *p_sys->p_style->i_font_size/p_orig->p[Y_PLANE].i_visible_lines;
        fmt_out.i_height = p_sys->p_style->i_font_size;

        p_pic = image_Convert( p_handler, p_orig, &fmt_in, &fmt_out );
        picture_Release( p_orig );
        video_format_Clean( &fmt_in );
        if( !p_pic )
        {
            msg_Warn( p_filter, "Error while converting %s", psz_url );
        }
    }
    else
    {
        p_pic = p_orig;
    }

    video_format_Clean( &fmt_out );
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
 * Parse url list, psz_urls must be non empty (TODO: check it !)
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
    p_sys->p_feeds = vlc_alloc( p_sys->i_feeds, sizeof( rss_feed_t ) );
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
 * Parse the rss feed
 ***************************************************************************/
static bool ParseFeed( filter_t *p_filter, xml_reader_t *p_xml_reader,
                      rss_feed_t *p_feed )
{
    VLC_UNUSED(p_filter);
    const char *node;
    char *psz_eltname = NULL;

    bool b_is_item = false;
    bool b_is_image = false;

    int i_item = 0;
    int type;

    while( (type = xml_ReaderNextNode( p_xml_reader, &node )) > 0 )
    {
        switch( type )
        {
        case XML_READER_STARTELEM:
#ifdef RSS_DEBUG
            msg_Dbg( p_filter, "element <%s>", node );
#endif
            free(psz_eltname);
            psz_eltname = strdup( node );
            if( unlikely(!psz_eltname) )
                goto end;

            /* rss or atom */
            if( !strcmp( node, "item" ) || !strcmp( node, "entry" ) )
            {
                b_is_item = true;
                p_feed->i_items++;
                p_feed->p_items = xrealloc( p_feed->p_items,
                                     p_feed->i_items * sizeof( rss_item_t ) );
                p_feed->p_items[p_feed->i_items-1].psz_title = NULL;
                p_feed->p_items[p_feed->i_items-1].psz_description = NULL;
                p_feed->p_items[p_feed->i_items-1].psz_link = NULL;
            }
            /* rss */
            else if( !strcmp( node, "image" ) )
            {
                b_is_image = true;
            }
            /* atom */
            else if( !strcmp( node, "link" ) )
            {
                const char *name, *value;
                char *psz_href = NULL;
                char *psz_rel = NULL;

                while( (name = xml_ReaderNextAttr( p_xml_reader, &value )) != NULL )
                {
                    if( !strcmp( name, "rel" ) )
                    {
                        free( psz_rel );
                        psz_rel = strdup( value );
                    }
                    else if( !strcmp( name, "href" ) )
                    {
                        free( psz_href );
                        psz_href = strdup( value );
                    }
                }

                /* "rel" and "href" must be defined */
                if( psz_rel && psz_href )
                {
                    if( !strcmp( psz_rel, "alternate" ) && !b_is_item &&
                        !b_is_image && !p_feed->psz_link )
                    {
                        p_feed->psz_link = psz_href;
                    }
                    /* this isn't in the rfc but i found some ... */
                    else if( ( !strcmp( psz_rel, "logo" ) ||
                               !strcmp( psz_rel, "icon" ) )
                             && !b_is_item && !b_is_image
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
            FREENULL( psz_eltname );
#ifdef RSS_DEBUG
            msg_Dbg( p_filter, "element end </%s>", node );
#endif
            /* rss or atom */
            if( !strcmp( node, "item" ) || !strcmp( node, "entry" ) )
            {
                b_is_item = false;
                i_item++;
            }
            /* rss */
            else if( !strcmp( node, "image" ) )
            {
                b_is_image = false;
            }
            break;

        case XML_READER_TEXT:
        {
            if( !psz_eltname )
                break;

            char *psz_eltvalue = removeWhiteChars( node );

#ifdef RSS_DEBUG
            msg_Dbg( p_filter, "  text : \"%s\"", psz_eltvalue );
#endif
            /* Is it an item ? */
            if( b_is_item )
            {
                rss_item_t *p_item = p_feed->p_items+i_item;
                /* rss/atom */
                if( !strcmp( psz_eltname, "title" ) && !p_item->psz_title )
                {
                    p_item->psz_title = psz_eltvalue;
                }
                else if( !strcmp( psz_eltname, "link" ) /* rss */
                         && !p_item->psz_link )
                {
                    p_item->psz_link = psz_eltvalue;
                }
                /* rss/atom */
                else if( ( !strcmp( psz_eltname, "description" ) ||
                           !strcmp( psz_eltname, "summary" ) )
                          && !p_item->psz_description )
                {
                    p_item->psz_description = psz_eltvalue;
                }
                else
                {
                    free( psz_eltvalue );
                }
            }
            /* Is it an image ? */
            else if( b_is_image )
            {
                if( !strcmp( psz_eltname, "url" ) && !p_feed->psz_image )
                    p_feed->psz_image = psz_eltvalue;
                else
                    free( psz_eltvalue );
            }
            else
            {
                /* rss/atom */
                if( !strcmp( psz_eltname, "title" ) && !p_feed->psz_title )
                {
                    p_feed->psz_title = psz_eltvalue;
                }
                /* rss */
                else if( !strcmp( psz_eltname, "link" ) && !p_feed->psz_link )
                {
                    p_feed->psz_link = psz_eltvalue;
                }
                /* rss ad atom */
                else if( ( !strcmp( psz_eltname, "description" ) ||
                           !strcmp( psz_eltname, "subtitle" ) )
                         && !p_feed->psz_description )
                {
                    p_feed->psz_description = psz_eltvalue;
                }
                /* rss */
                else if( ( !strcmp( psz_eltname, "logo" ) ||
                           !strcmp( psz_eltname, "icon" ) )
                         && !p_feed->psz_image )
                {
                    p_feed->psz_image = psz_eltvalue;
                }
                else
                {
                    free( psz_eltvalue );
                }
            }
            break;
        }
        }
    }

    free( psz_eltname );
    return true;

end:
    return false;
}


/****************************************************************************
 * FetchRSS (or Atom) feeds
 ***************************************************************************/
static rss_feed_t* FetchRSS( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    stream_t *p_stream;
    xml_reader_t *p_xml_reader;
    int i_feed;

    /* These data are not modified after the creation of the module so we don't
       need to hold the lock */
    int i_feeds = p_sys->i_feeds;
    bool b_images = p_sys->b_images;

    /* Allocate a new structure */
    rss_feed_t *p_feeds = vlc_alloc( i_feeds, sizeof( rss_feed_t ) );
    if( !p_feeds )
        return NULL;

    /* Fetch all feeds and parse them */
    for( i_feed = 0; i_feed < i_feeds; i_feed++ )
    {
        rss_feed_t *p_feed = p_feeds + i_feed;
        rss_feed_t *p_old_feed = p_sys->p_feeds + i_feed;

        /* Initialize the structure */
        p_feed->psz_title = NULL;
        p_feed->psz_description = NULL;
        p_feed->psz_link = NULL;
        p_feed->psz_image = NULL;
        p_feed->p_pic = NULL;
        p_feed->i_items = 0;
        p_feed->p_items = NULL;

        p_feed->psz_url = strdup( p_old_feed->psz_url );

        /* Fetch the feed */
        msg_Dbg( p_filter, "opening %s RSS/Atom feed ...", p_feed->psz_url );

        p_stream = vlc_stream_NewURL( p_filter, p_feed->psz_url );
        if( !p_stream )
        {
            msg_Err( p_filter, "Failed to open %s for reading", p_feed->psz_url );
            p_xml_reader = NULL;
            goto error;
        }

        p_xml_reader = xml_ReaderCreate( p_filter, p_stream );
        if( !p_xml_reader )
        {
            msg_Err( p_filter, "Failed to open %s for parsing", p_feed->psz_url );
            goto error;
        }

        /* Parse the feed */
        if( !ParseFeed( p_filter, p_xml_reader, p_feed ) )
            goto error;

        /* If we have a image: load it if requiere */
        if( b_images && p_feed->psz_image && !p_feed->p_pic )
        {
            p_feed->p_pic = LoadImage( p_filter, p_feed->psz_image );
        }

        msg_Dbg( p_filter, "done with %s RSS/Atom feed", p_feed->psz_url );
        xml_ReaderDelete( p_xml_reader );
        vlc_stream_Delete( p_stream );
    }

    return p_feeds;

error:
    FreeRSS( p_feeds, i_feed + 1 );
    if( p_xml_reader )
        xml_ReaderDelete( p_xml_reader );
    if( p_stream )
        vlc_stream_Delete( p_stream );

    return NULL;
}

/****************************************************************************
 * FreeRSS
 ***************************************************************************/
static void FreeRSS( rss_feed_t *p_feeds, int i_feeds )
{
    for( int i_feed = 0; i_feed < i_feeds; i_feed++ )
    {
        rss_feed_t *p_feed = p_feeds+i_feed;
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
    free( p_feeds );
}

static void Fetch( void *p_data )
{
    filter_t *p_filter = p_data;
    filter_sys_t *p_sys = p_filter->p_sys;

    msg_Dbg( p_filter, "Updating the rss feeds" );
    rss_feed_t *p_feeds = FetchRSS( p_filter );
    if( !p_feeds )
    {
        msg_Err( p_filter, "Unable to fetch the feeds" );
        return;
    }

    rss_feed_t *p_old_feeds = p_sys->p_feeds;

    vlc_mutex_lock( &p_sys->lock );
    /* Update the feeds */
    p_sys->p_feeds = p_feeds;
    p_sys->b_fetched = true;
    /* Set all current info to the original values */
    p_sys->i_cur_feed = 0;
    p_sys->i_cur_item = p_sys->i_title == scroll_title ? -1 : 0;
    p_sys->i_cur_char = 0;
    vlc_mutex_unlock( &p_sys->lock );

    if( p_old_feeds )
        FreeRSS( p_old_feeds, p_sys->i_feeds );
}
