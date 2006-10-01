/*****************************************************************************
 * marq.c : marquee display video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Mark Moriarty
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Antoine Cellerier <dionoea . videolan \ org>
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

#include <time.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "vlc_filter.h"
#include "vlc_block.h"
#include "vlc_osd.h"
#include "vlc_playlist.h"
#include "vlc_meta.h"
#include "vlc_input.h"
#include <vlc/aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );
static subpicture_t *Filter( filter_t *, mtime_t );


static int MarqueeCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data );
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
 * filter_sys_t: marquee filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    int i_xoff, i_yoff;  /* offsets for the display string in the video window */
    int i_pos; /* permit relative positioning (top, bottom, left, right, center) */
    int i_timeout;

    char *psz_marquee;    /* marquee string */

    text_style_t *p_style; /* font control */

    time_t last_time;

    vlc_bool_t b_need_update;
};

#define MSG_TEXT N_("Text")
#define MSG_LONGTEXT N_( \
    "Marquee text to display. " \
    "(Available format strings: " \
    "Time related: %Y = year, %m = month, %d = day, %H = hour, " \
    "%M = minute, %S = second, ... " \
    "Meta data related: $a = artist, $b = album, $c = copyright, " \
    "$d = description, $e = encoded by, $g = genre, " \
    "$l = language, $n = track num, $p = now playing, " \
    "$r = rating, $s = subtitles language, $t = title, "\
    "$u = url, $A = date, " \
    "$B = audio bitrate (in kb/s), $C = chapter," \
    "$D = duration, $F = full name with path, $I = title, "\
    "$L = time left, " \
    "$N = name, $O = audio language, $P = position (in %), $R = rate, " \
    "$S = audio sample rate (in kHz), " \
    "$T = time, $U = publisher, $V = volume, $_ = new line) ")
#define POSX_TEXT N_("X offset")
#define POSX_LONGTEXT N_("X offset, from the left screen edge." )
#define POSY_TEXT N_("Y offset")
#define POSY_LONGTEXT N_("Y offset, down from the top." )
#define TIMEOUT_TEXT N_("Timeout")
#define TIMEOUT_LONGTEXT N_("Number of milliseconds the marquee must remain " \
                            "displayed. Default value is " \
                            "0 (remains forever).")
#define OPACITY_TEXT N_("Opacity")
#define OPACITY_LONGTEXT N_("Opacity (inverse of transparency) of " \
    "overlayed text. 0 = transparent, 255 = totally opaque. " )
#define SIZE_TEXT N_("Font size, pixels")
#define SIZE_LONGTEXT N_("Font size, in pixels. Default is -1 (use default " \
    "font size)." )

#define COLOR_TEXT N_("Color")
#define COLOR_LONGTEXT N_("Color of the text that will be rendered on "\
    "the video. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white" )

#define POS_TEXT N_("Marquee position")
#define POS_LONGTEXT N_( \
  "You can enforce the marquee position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values, eg 6 = top-right).")

static int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static char *ppsz_pos_descriptions[] =
     { N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
     N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_capability( "sub filter", 0 );
    set_shortname( N_("Marquee" ));
    set_callbacks( CreateFilter, DestroyFilter );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_SUBPIC );
    add_string( "marq-marquee", "VLC", NULL, MSG_TEXT, MSG_LONGTEXT,
                VLC_FALSE );

    set_section( N_("Position"), NULL );
    add_integer( "marq-x", -1, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_TRUE );
    add_integer( "marq-y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_TRUE );
    add_integer( "marq-position", 5, NULL, POS_TEXT, POS_LONGTEXT, VLC_FALSE );

    set_section( N_("Font"), NULL );
    /* 5 sets the default to top [1] left [4] */
    change_integer_list( pi_pos_values, ppsz_pos_descriptions, 0 );
    add_integer_with_range( "marq-opacity", 255, 0, 255, NULL,
        OPACITY_TEXT, OPACITY_LONGTEXT, VLC_FALSE );
    add_integer( "marq-color", 0xFFFFFF, NULL, COLOR_TEXT, COLOR_LONGTEXT,
                  VLC_FALSE );
        change_integer_list( pi_color_values, ppsz_color_descriptions, 0 );
    add_integer( "marq-size", -1, NULL, SIZE_TEXT, SIZE_LONGTEXT, VLC_FALSE );

    set_section( N_("Misc"), NULL );
    add_integer( "marq-timeout", 0, NULL, TIMEOUT_TEXT, TIMEOUT_LONGTEXT,
                 VLC_FALSE );

    set_description( _("Marquee display") );
    add_shortcut( "marq" );
    add_shortcut( "time" );
vlc_module_end();

/*****************************************************************************
 * CreateFilter: allocates marquee video filter
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

    p_sys->p_style = malloc( sizeof( text_style_t ) );
    memcpy( p_sys->p_style, &default_text_style, sizeof( text_style_t ) );

#define CREATE_VAR( stor, type, var ) \
    p_sys->stor = var_CreateGet##type( p_filter->p_libvlc, var ); \
    var_AddCallback( p_filter->p_libvlc, var, MarqueeCallback, p_sys );

    CREATE_VAR( i_xoff, Integer, "marq-x" );
    CREATE_VAR( i_yoff, Integer, "marq-y" );
    CREATE_VAR( i_timeout,Integer, "marq-timeout" );
    CREATE_VAR( i_pos, Integer, "marq-position" );
    CREATE_VAR( psz_marquee, String, "marq-marquee" );
    CREATE_VAR( p_style->i_font_alpha, Integer, "marq-opacity" );
    CREATE_VAR( p_style->i_font_color, Integer, "marq-color" );
    CREATE_VAR( p_style->i_font_size, Integer, "marq-size" );

    p_sys->p_style->i_font_alpha = 255 - p_sys->p_style->i_font_alpha ;

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

    if( p_sys->p_style ) free( p_sys->p_style );
    if( p_sys->psz_marquee ) free( p_sys->psz_marquee );
    free( p_sys );

    /* Delete the marquee variables */
#define DEL_VAR(var) \
    var_DelCallback( p_filter->p_libvlc, var, MarqueeCallback, p_sys ); \
    var_Destroy( p_filter->p_libvlc, var );
    DEL_VAR( "marq-x" );
    DEL_VAR( "marq-y" );
    DEL_VAR( "marq-marquee" );
    DEL_VAR( "marq-timeout" );
    DEL_VAR( "marq-position" );
    DEL_VAR( "marq-color" );
    DEL_VAR( "marq-opacity" );
    DEL_VAR( "marq-size" );
}
/****************************************************************************
 * String formating functions
 ****************************************************************************/

static char *FormatTime(char *tformat )
{
    char buffer[255];
    time_t curtime;
#if defined(HAVE_LOCALTIME_R)
    struct tm loctime;
#else
    struct tm *loctime;
#endif

    /* Get the current time.  */
    curtime = time( NULL );

    /* Convert it to local time representation.  */
#if defined(HAVE_LOCALTIME_R)
    localtime_r( &curtime, &loctime );
    strftime( buffer, 255, tformat, &loctime );
#else
    loctime = localtime( &curtime );
    strftime( buffer, 255, tformat, loctime );
#endif
    return strdup( buffer );
}

#define INSERT_STRING( check, string )                              \
                    if( check && string )                           \
                    {                                               \
                        int len = strlen( string );                 \
                        dst = realloc( dst,                         \
                                       i_size = i_size + len + 1 ); \
                        strncpy( d, string, len+1 );                \
                        d += len;                                   \
                    }                                               \
                    else                                            \
                    {                                               \
                        *d = '-';                                   \
                        d++;                                        \
                    }
char *FormatMeta( vlc_object_t *p_object, char *string )
{
    char *s = string;
    char *dst = malloc( 1000 );
    char *d = dst;
    int b_is_format = 0;
    char buf[10];
    int i_size = strlen( string );

    playlist_t *p_playlist = pl_Yield( p_object );
    input_thread_t *p_input = p_playlist->p_input;
    input_item_t *p_item = NULL;
    pl_Release( p_object );
    if( p_input )
    {
        vlc_object_yield( p_input );
        p_item = p_input->input.p_item;
        if( p_item )
            vlc_mutex_lock( &p_item->lock );
    }

    sprintf( dst, string );

    while( *s )
    {
        if( b_is_format )
        {
            switch( *s )
            {
                case 'a':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_artist );
                    break;
                case 'b':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_album );
                    break;
                case 'c':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_copyright );
                    break;
                case 'd':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_description );
                    break;
                case 'e':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_encodedby );
                    break;
                case 'g':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_genre );
                    break;
                case 'l':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_language );
                    break;
                case 'n':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_tracknum );
                    break;
                case 'p':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_nowplaying );
                    break;
                case 'r':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_rating );
                    break;
                case 's':
                {
                    char *lang;
                    if( p_input )
                    {
                        lang = var_GetString( p_input, "sub-language" );
                    }
                    else
                    {
                        lang = strdup( "-" );
                    }
                    INSERT_STRING( 1, lang );
                    free( lang );
                    break;
                }
                case 't':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_title );
                    break;
                case 'u':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_url );
                    break;
                case 'A':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_date );
                    break;
                case 'B':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%d",
                                  var_GetInteger( p_input, "bit-rate" )/1000 );
                    }
                    else
                    {
                        sprintf( buf, "-" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'C':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%d",
                                  var_GetInteger( p_input, "chapter" ) );
                    }
                    else
                    {
                        sprintf( buf, "-" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'D':
                    if( p_item )
                    {
                        sprintf( buf, "%02d:%02d:%02d",
                                 (int)(p_item->i_duration/(3600000000)),
                                 (int)((p_item->i_duration/(60000000))%60),
                                 (int)((p_item->i_duration/1000000)%60) );
                    }
                    else
                    {
                        sprintf( buf, "--:--:--" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'F':
                    INSERT_STRING( p_item, p_item->psz_uri );
                    break;
                case 'I':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%d",
                                  var_GetInteger( p_input, "title" ) );
                    }
                    else
                    {
                        sprintf( buf, "-" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'L':
                    if( p_item && p_input )
                    {
                        sprintf( buf, "%02d:%02d:%02d",
                     (int)((p_item->i_duration-p_input->i_time)/(3600000000)),
                     (int)(((p_item->i_duration-p_input->i_time)/(60000000))%60),
                     (int)(((p_item->i_duration-p_input->i_time)/1000000)%60) );
                    }
                    else
                    {
                        sprintf( buf, "--:--:--" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'N':
                    INSERT_STRING( p_item, p_item->psz_name );
                    break;
                case 'O':
                {
                    char *lang;
                    if( p_input )
                    {
                        lang = var_GetString( p_input, "audio-language" );
                    }
                    else
                    {
                        lang = strdup( "-" );
                    }
                    INSERT_STRING( 1, lang );
                    free( lang );
                    break;
                }
                case 'P':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%2.1lf",
                                  var_GetFloat( p_input, "position" ) * 100. );
                    }
                    else
                    {
                        sprintf( buf, "--.-%%" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'R':
                    if( p_input )
                    {
                        int r = var_GetInteger( p_input, "rate" );
                        snprintf( buf, 10, "%d.%d", r/1000, r%1000 );
                    }
                    else
                    {
                        sprintf( buf, "-" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'S':
                    if( p_input )
                    {
                        int r = var_GetInteger( p_input, "sample-rate" );
                        snprintf( buf, 10, "%d.%d", r/1000, (r/100)%10 );
                    }
                    else
                    {
                        sprintf( buf, "-" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'T':
                    if( p_input )
                    {
                        sprintf( buf, "%02d:%02d:%02d",
                                 (int)(p_input->i_time/(3600000000)),
                                 (int)((p_input->i_time/(60000000))%60),
                                 (int)((p_input->i_time/1000000)%60) );
                    }
                    else
                    {
                        sprintf( buf, "--:--:--" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'U':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_publisher );
                    break;
                case 'V':
                {
                    audio_volume_t volume;
                    aout_VolumeGet( p_object, &volume );
                    snprintf( buf, 10, "%d", volume );
                    INSERT_STRING( 1, buf );
                    break;
                }
                case '_':
                    *d = '\n';
                    d++;
                    break;

                default:
                    *d = *s;
                    d++;
                    break;
            }
            b_is_format = 0;
        }
        else if( *s == '$' )
        {
            b_is_format = 1;
        }
        else
        {
            *d = *s;
            d++;
        }
        s++;
    }
    *d = '\0';

    if( p_input )
    {
        vlc_object_release( p_input );
        if( p_item )
            vlc_mutex_unlock( &p_item->lock );
    }

    return dst;
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
    char *buf;

    if( p_sys->last_time == time( NULL ) )
    {
        return NULL;
    }

/*    if( p_sys->b_need_update == VLC_FALSE )
    {
        return NULL;
    }*/

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

    buf = FormatTime( p_sys->psz_marquee );
    p_spu->p_region->psz_text = FormatMeta( VLC_OBJECT( p_filter ), buf );
    free( buf );
    p_spu->i_start = date;
    p_spu->i_stop  = p_sys->i_timeout == 0 ? 0 : date + p_sys->i_timeout * 1000;
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
    p_spu->p_region->p_style = p_sys->p_style;

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
    else if ( !strncmp( psz_var, "marq-color", 8 ) )  /* "marq-col" */
    {
        p_sys->p_style->i_font_color = newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-opacity", 8 ) ) /* "marq-opa" */
    {
        p_sys->p_style->i_font_alpha = 255 - newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-size", 6 ) )
    {
        p_sys->p_style->i_font_size = newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-timeout", 12 ) )
    {
        p_sys->i_timeout = newval.i_int;
    }
    else if ( !strncmp( psz_var, "marq-position", 8 ) )
    /* willing to accept a match against marq-pos */
    {
        p_sys->i_pos = newval.i_int;
        p_sys->i_xoff = -1;       /* force to relative positioning */
    }
    p_sys->b_need_update = VLC_TRUE;
    return VLC_SUCCESS;
}
