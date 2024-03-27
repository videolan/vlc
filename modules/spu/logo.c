/*****************************************************************************
 * logo.c : logo video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2006 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Simon Latapie <garf@videolan.org>
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
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include "common.h"

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_mouse.h>
#include <vlc_subpicture.h>
#include <vlc_url.h>
#include <vlc_image.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FILE_TEXT N_("Logo filenames")
#define FILE_LONGTEXT N_("Full path of the image files to use. Format is " \
"<image>[,<delay in ms>[,<alpha>]][;<image>[,<delay>[,<alpha>]]][;...]. " \
"If you only have one file, simply enter its filename.")
#define REPEAT_TEXT N_("Animation loops")
#define REPEAT_LONGTEXT N_("Number of loops for the logo animation. " \
        "-1 = continuous, 0 = disabled")
#define DELAY_TEXT N_("Display time in ms")
#define DELAY_LONGTEXT N_("Individual image display time of 0 - 60000 ms.")

#undef POSX_LONGTEXT
#undef POSY_LONGTEXT
#define POSX_LONGTEXT N_("X offset, from top-left, or from relative position. " \
                         "You can move the logo by left-clicking it." )
#define POSY_LONGTEXT N_("Y offset, from top-left, or from relative position. " \
                         "You can move the logo by left-clicking it." )

#define LOGO_HELP N_("Use a local picture as logo on the video")

#define CFG_PREFIX "logo-"

static int  OpenSub  ( filter_t * );
static int  OpenVideo( filter_t * );
static void Close    ( filter_t * );

vlc_module_begin ()
    set_subcategory( SUBCAT_VIDEO_SUBPIC )
    set_help(LOGO_HELP)
    set_callback_sub_source( OpenSub, 0 )
    set_description( N_("Logo sub source") )
    set_shortname( N_("Logo overlay") )
    add_shortcut( "logo" )

    add_loadfile(CFG_PREFIX "file", NULL, FILE_TEXT, FILE_LONGTEXT)
    add_integer( CFG_PREFIX "x", -1, POSX_TEXT, POSX_LONGTEXT )
    add_integer( CFG_PREFIX "y", -1, POSY_TEXT, POSY_LONGTEXT )
    /* default to 1000 ms per image, continuously cycle through them */
    add_integer( CFG_PREFIX "delay", 1000, DELAY_TEXT, DELAY_LONGTEXT )
    add_integer( CFG_PREFIX "repeat", -1, REPEAT_TEXT, REPEAT_LONGTEXT )
    add_integer_with_range( CFG_PREFIX "opacity", 255, 0, 255,
        OPACITY_TEXT, OPACITY_LONGTEXT )
    add_integer( CFG_PREFIX "position", -1, POS_TEXT, POS_LONGTEXT )
        change_integer_list( pi_pos_values, ppsz_pos_descriptions )

    /* video output filter submodule */
    add_submodule ()
    set_callback_video_filter( OpenVideo )
    set_description( N_("Logo video filter") )
    add_shortcut( "logo" )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/**
 * Structure to hold the set of individual logo image names, times,
 * transparencies
 */
typedef struct
{
    int i_delay;       /* -1 means use default delay */
    int i_alpha;       /* -1 means use default alpha */
    picture_t *p_pic;

} logo_t;

/**
 * Logo list structure.
 */
typedef struct
{
    logo_t *p_logo;         /* the parsing's result */
    unsigned int i_count;   /* the number of logo images to be displayed */

    int i_repeat;         /* how often to repeat the images, image time in ms */
    vlc_tick_t i_next_pic;     /* when to bring up a new logo image */

    unsigned int i_counter; /* index into the list of logo images */

    int i_delay;            /* default delay (0 - 60000 ms) */
    int i_alpha;            /* default alpha */

} logo_list_t;

/**
 * Private logo data holder
 */
typedef struct
{
    vlc_blender_t *p_blend;

    vlc_mutex_t lock;

    logo_list_t list;

    int i_pos;
    int i_pos_x;
    int i_pos_y;

    /* On the fly control variable */
    bool b_spu_update;

    /* */
    bool b_mouse_grab;
} filter_sys_t;

static const char *const ppsz_filter_options[] = {
    "file", "x", "y", "delay", "repeat", "opacity", "position", NULL
};

static const char *const ppsz_filter_callbacks[] = {
    "logo-file",
    "logo-x",
    "logo-y",
    "logo-position",
    "logo-opacity",
    "logo-repeat",
    NULL
};

static int OpenCommon( filter_t *, bool b_sub );

static subpicture_t *FilterSub( filter_t *, vlc_tick_t );
static picture_t    *FilterVideo( filter_t *, picture_t * );

static int Mouse( filter_t *, vlc_mouse_t *, const vlc_mouse_t * );

static int LogoCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );

static void LogoListLoad( vlc_object_t *, logo_list_t *, const char * );
static void LogoListUnload( logo_list_t * );
static logo_t *LogoListNext( logo_list_t *p_list, vlc_tick_t i_date );
static logo_t *LogoListCurrent( logo_list_t *p_list );

/**
 * Open the sub source
 */
static int OpenSub( filter_t *p_filter )
{
    return OpenCommon( p_filter, true );
}

/**
 * Open the video filter
 */
static int OpenVideo( filter_t *p_filter )
{
    return OpenCommon( p_filter, false );
}

static const struct vlc_filter_operations filter_sub_ops = {
    .source_sub = FilterSub, .close = Close,
};

static const struct vlc_filter_operations filter_video_ops = {
    .filter_video = FilterVideo,
    .video_mouse = Mouse,
    .close = Close,
};

/**
 * Common open function
 */
static int OpenCommon( filter_t *p_filter, bool b_sub )
{
    filter_sys_t *p_sys;
    char *psz_filename;

    /* */
    if( !b_sub && !es_format_IsSimilar( &p_filter->fmt_in, &p_filter->fmt_out ) )
    {
        msg_Err( p_filter, "Input and output format does not match" );
        return VLC_EGENERIC;
    }

    /* */
    p_filter->p_sys = p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* */
    p_sys->p_blend = NULL;
    if( !b_sub )
    {

        p_sys->p_blend = filter_NewBlend( VLC_OBJECT(p_filter),
                                          &p_filter->fmt_in.video );
        if( !p_sys->p_blend )
        {
            free( p_sys );
            return VLC_EGENERIC;
        }
    }

    /* */
    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    /* */
    logo_list_t *p_list = &p_sys->list;

    psz_filename = var_CreateGetStringCommand( p_filter, "logo-file" );
    if( !psz_filename )
    {
        if( p_sys->p_blend )
            filter_DeleteBlend( p_sys->p_blend );
        free( p_sys );
        return VLC_ENOMEM;
    }
    if( *psz_filename == '\0' )
        msg_Warn( p_filter, "no logo file specified" );

    p_list->i_alpha = var_CreateGetIntegerCommand( p_filter, "logo-opacity");
    p_list->i_alpha = VLC_CLIP( p_list->i_alpha, 0, 255 );
    p_list->i_delay = var_CreateGetIntegerCommand( p_filter, "logo-delay" );
    p_list->i_repeat = var_CreateGetIntegerCommand( p_filter, "logo-repeat" );

    p_sys->i_pos = var_CreateGetIntegerCommand( p_filter, "logo-position" );
    p_sys->i_pos_x = var_CreateGetIntegerCommand( p_filter, "logo-x" );
    p_sys->i_pos_y = var_CreateGetIntegerCommand( p_filter, "logo-y" );

    /* Ignore alignment if a position is given for video filter */
    if( !b_sub && p_sys->i_pos_x >= 0 && p_sys->i_pos_y >= 0 )
        p_sys->i_pos = -1;

    vlc_mutex_init( &p_sys->lock );
    LogoListLoad( VLC_OBJECT(p_filter), p_list, psz_filename );
    p_sys->b_spu_update = true;
    p_sys->b_mouse_grab = false;

    for( int i = 0; ppsz_filter_callbacks[i]; i++ )
        var_AddCallback( p_filter, ppsz_filter_callbacks[i],
                         LogoCallback, p_sys );

    /* Misc init */
    if( b_sub )
        p_filter->ops = &filter_sub_ops;
    else
        p_filter->ops = &filter_video_ops;

    free( psz_filename );
    return VLC_SUCCESS;
}

/**
 * Common close function
 */
static void Close( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    for( int i = 0; ppsz_filter_callbacks[i]; i++ )
        var_DelCallback( p_filter, ppsz_filter_callbacks[i],
                         LogoCallback, p_sys );

    if( p_sys->p_blend )
        filter_DeleteBlend( p_sys->p_blend );

    LogoListUnload( &p_sys->list );
    free( p_sys );
}

/**
 * Sub source
 */
static subpicture_t *FilterSub( filter_t *p_filter, vlc_tick_t date )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    logo_list_t *p_list = &p_sys->list;

    subpicture_t *p_spu;
    subpicture_region_t *p_region;
    picture_t *p_pic;
    logo_t *p_logo;

    vlc_mutex_lock( &p_sys->lock );
    /* Basic test:  b_spu_update occurs on a dynamic change,
                    & i_next_pic is the general timer, when to
                    look at updating the logo image */

    if( ( !p_sys->b_spu_update && p_list->i_next_pic > date ) ||
        !p_list->i_repeat )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }

    /* adjust index to the next logo */
    p_logo = LogoListNext( p_list, date );
    p_sys->b_spu_update = false;

    p_pic = p_logo->p_pic;

    /* Allocate the subpicture internal data. */
    p_spu = filter_NewSubpicture( p_filter );
    if( !p_spu )
        goto exit;

    p_spu->i_start = date;
    p_spu->i_stop = VLC_TICK_INVALID;
    p_spu->b_ephemer = true;

    /* Send an empty subpicture to clear the display when needed */
    if( p_list->i_repeat != -1 && p_list->i_counter == 0 )
    {
        p_list->i_repeat--;
        if( p_list->i_repeat < 0 )
            goto exit;
    }
    if( !p_pic || !p_logo->i_alpha ||
        ( p_logo->i_alpha == -1 && !p_list->i_alpha ) )
        goto exit;

    /* Create new SPU region */
    p_region = subpicture_region_ForPicture( NULL, p_pic );
    if( !p_region )
    {
        msg_Err( p_filter, "cannot allocate SPU region" );
        subpicture_Delete( p_spu );
        p_spu = NULL;
        goto exit;
    }

    /*  where to locate the logo: */
    if( p_sys->i_pos < 0 )
    {   /*  set to an absolute xy */
        p_region->i_align = SUBPICTURE_ALIGN_LEFT | SUBPICTURE_ALIGN_TOP;
        p_region->b_absolute = true;
    }
    else
    {   /* set to one of the 9 relative locations */
        p_region->i_align = p_sys->i_pos;
        p_region->b_absolute = false;
    }

    p_region->i_x = p_sys->i_pos_x > 0 ? p_sys->i_pos_x : 0;
    p_region->i_y = p_sys->i_pos_y > 0 ? p_sys->i_pos_y : 0;

    vlc_spu_regions_push( &p_spu->regions, p_region );

    p_spu->i_alpha = ( p_logo->i_alpha != -1 ?
                       p_logo->i_alpha : p_list->i_alpha );

exit:
    vlc_mutex_unlock( &p_sys->lock );

    return p_spu;
}

/**
 * Video filter
 */
static picture_t *FilterVideo( filter_t *p_filter, picture_t *p_src )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    logo_list_t *p_list = &p_sys->list;

    picture_t *p_dst = filter_NewPicture( p_filter );
    if( !p_dst )
        goto exit;

    picture_Copy( p_dst, p_src );

    /* */
    vlc_mutex_lock( &p_sys->lock );

    logo_t *p_logo;
    if( p_list->i_next_pic < p_src->date )
        p_logo = LogoListNext( p_list, p_src->date );
    else
        p_logo = LogoListCurrent( p_list );

    /* */
    const picture_t *p_pic = p_logo->p_pic;
    if( p_pic )
    {
        const video_format_t *p_fmt = &p_pic->format;
        const int i_dst_w = p_filter->fmt_out.video.i_visible_width;
        const int i_dst_h = p_filter->fmt_out.video.i_visible_height;

        if( p_sys->i_pos >= 0 )
        {
            if( p_sys->i_pos & SUBPICTURE_ALIGN_TOP )
                p_sys->i_pos_y = 0;
            else if( p_sys->i_pos & SUBPICTURE_ALIGN_BOTTOM )
                p_sys->i_pos_y = i_dst_h - p_fmt->i_visible_height;
            else
                p_sys->i_pos_y = ( i_dst_h - p_fmt->i_visible_height ) / 2;

            if( p_sys->i_pos & SUBPICTURE_ALIGN_LEFT )
                p_sys->i_pos_x = 0;
            else if( p_sys->i_pos & SUBPICTURE_ALIGN_RIGHT )
                p_sys->i_pos_x = i_dst_w - p_fmt->i_visible_width;
            else
                p_sys->i_pos_x = ( i_dst_w - p_fmt->i_visible_width ) / 2;
        }

        if( p_sys->i_pos_x < 0 || p_sys->i_pos_y < 0 )
        {
            msg_Warn( p_filter,
                "logo(%ix%i) doesn't fit into video(%ix%i)",
                p_fmt->i_visible_width, p_fmt->i_visible_height,
                i_dst_w,i_dst_h );
            p_sys->i_pos_x = (p_sys->i_pos_x > 0) ? p_sys->i_pos_x : 0;
            p_sys->i_pos_y = (p_sys->i_pos_y > 0) ? p_sys->i_pos_y : 0;
        }

        /* */
        const int i_alpha = p_logo->i_alpha != -1 ? p_logo->i_alpha : p_list->i_alpha;
        if( filter_ConfigureBlend( p_sys->p_blend, i_dst_w, i_dst_h, p_fmt ) ||
            filter_Blend( p_sys->p_blend, p_dst, p_sys->i_pos_x, p_sys->i_pos_y,
                          p_pic, i_alpha ) )
        {
            msg_Err( p_filter, "failed to blend a picture" );
        }
    }
    vlc_mutex_unlock( &p_sys->lock );

exit:
    picture_Release( p_src );
    return p_dst;
}

static int Mouse( filter_t *p_filter, vlc_mouse_t *p_new,
                  const vlc_mouse_t *p_old )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    logo_t *p_logo = LogoListCurrent( &p_sys->list );
    const picture_t *p_pic = p_logo->p_pic;

    if( p_pic )
    {
        const video_format_t *p_fmt = &p_pic->format;
        const int i_logo_w = p_fmt->i_visible_width;
        const int i_logo_h = p_fmt->i_visible_height;

        /* Check if we are over the logo */
        const bool b_over = p_new->i_x >= p_sys->i_pos_x &&
                            p_new->i_x <  p_sys->i_pos_x + i_logo_w &&
                            p_new->i_y >= p_sys->i_pos_y &&
                            p_new->i_y <  p_sys->i_pos_y + i_logo_h;

        if( b_over && vlc_mouse_HasPressed( p_old, p_new, MOUSE_BUTTON_LEFT ) )
            p_sys->b_mouse_grab = true;
        else if( vlc_mouse_HasReleased( p_old, p_new, MOUSE_BUTTON_LEFT ) )
            p_sys->b_mouse_grab = false;

        if( p_sys->b_mouse_grab )
        {
            int i_dx, i_dy;
            vlc_mouse_GetMotion( &i_dx, &i_dy, p_old, p_new );
            p_sys->i_pos_x = VLC_CLIP( p_sys->i_pos_x + i_dx, 0,
                                    (int)p_filter->fmt_in.video.i_width  - i_logo_w );
            p_sys->i_pos_y = VLC_CLIP( p_sys->i_pos_y + i_dy, 0,
                                    (int)p_filter->fmt_in.video.i_height - i_logo_h );
        }

        if( p_sys->b_mouse_grab || b_over )
        {
            vlc_mutex_unlock( &p_sys->lock );
            return VLC_EGENERIC;
        }
    }
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Callback to update params on the fly
 *****************************************************************************/
static int LogoCallback( vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(oldval);
    filter_sys_t *p_sys = p_data;
    logo_list_t *p_list = &p_sys->list;

    vlc_mutex_lock( &p_sys->lock );
    if( !strcmp( psz_var, "logo-file" ) )
    {
        LogoListUnload( p_list );
        LogoListLoad( p_this, p_list, newval.psz_string );
    }
    else if ( !strcmp( psz_var, "logo-x" ) )
    {
        p_sys->i_pos_x = newval.i_int;
    }
    else if ( !strcmp( psz_var, "logo-y" ) )
    {
        p_sys->i_pos_y = newval.i_int;
    }
    else if ( !strcmp( psz_var, "logo-position" ) )
    {
        p_sys->i_pos = newval.i_int;
    }
    else if ( !strcmp( psz_var, "logo-opacity" ) )
    {
        p_list->i_alpha = VLC_CLIP( newval.i_int, 0, 255 );
    }
    else if ( !strcmp( psz_var, "logo-repeat" ) )
    {
        p_list->i_repeat = newval.i_int;
    }
    p_sys->b_spu_update = true;
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

/**
 * It loads the logo image into memory.
 */
static picture_t *LoadPicture( vlc_object_t *p_this, const char *psz_filename )
{
    if( !psz_filename )
        return NULL;

    image_handler_t *p_image = image_HandlerCreate( p_this );
    if( !p_image )
        return NULL;

    video_format_t fmt_out;
    video_format_Init( &fmt_out, VLC_CODEC_YUVA );

    char *psz_url = vlc_path2uri( psz_filename, NULL );
    picture_t *p_pic = image_ReadUrl( p_image, psz_url, &fmt_out );
    free( psz_url );
    image_HandlerDelete( p_image );
    video_format_Clean( &fmt_out );

    return p_pic;
}

/**
 * It loads the logo images into memory.
 *
 * Read the logo-file input switch, obtaining a list of images and
 * associated durations and transparencies. Store the image(s), and
 * times. An image without a stated time or opacity will use the
 * logo-delay and logo-opacity values.
 */
static void LogoListLoad( vlc_object_t *p_this, logo_list_t *p_logo_list,
                          const char *psz_filename )
{
    char *psz_list; /* the list: <logo>[,[<delay>[,[<alpha>]]]][;...] */
    char *psz_original;
    logo_t *p_logo;         /* the parsing's result */

    p_logo_list->i_counter = 0;
    p_logo_list->i_next_pic = 0;

    psz_original = psz_list = strdup( psz_filename );

    if( !psz_list )
        return;

    /* Count the number logos == number of ';' + 1 */
    p_logo_list->i_count = 1;
    for( unsigned i = 0; i < strlen( psz_list ); i++ )
    {
        if( psz_list[i] == ';' )
            p_logo_list->i_count++;
    }

    p_logo_list->p_logo =
    p_logo              = calloc( p_logo_list->i_count, sizeof(*p_logo) );

    if( !p_logo )
    {
        free( psz_list );
        return;
    }

    /* Fill the data */
    for( unsigned i = 0; i < p_logo_list->i_count; i++ )
    {
        char *p_c  = strchr( psz_list, ';' );
        char *p_c2 = strchr( psz_list, ',' );

        p_logo[i].i_alpha = -1; /* use default settings */
        p_logo[i].i_delay = -1; /* use default settings */

        if( p_c2 && ( p_c2 < p_c || !p_c ) )
        {
            /* <logo>,<delay>[,<alpha>] type */
            if( p_c2[1] != ',' && p_c2[1] != ';' && p_c2[1] != '\0' )
                p_logo[i].i_delay = atoi( p_c2+1 );
            *p_c2 = '\0';
            if( ( p_c2 = strchr( p_c2+1, ',' ) )
                && ( p_c2 < p_c || !p_c ) && p_c2[1] != ';' && p_c2[1] != '\0' )
                p_logo[i].i_alpha = atoi( p_c2 + 1 );
        }
        else
        {
            /* <logo> type */
            if( p_c )
                *p_c = '\0';
        }

        msg_Dbg( p_this, "logo file name %s, delay %d, alpha %d",
                 psz_list, p_logo[i].i_delay, p_logo[i].i_alpha );
        p_logo[i].p_pic = LoadPicture( p_this, psz_list );
        if( !p_logo[i].p_pic )
        {
            msg_Warn( p_this, "error while loading logo %s, will be skipped",
                      psz_list );
        }

        if( p_c )
            psz_list = &p_c[1];
    }

    /* initialize so that on the first update it will wrap back to 0 */
    p_logo_list->i_counter = p_logo_list->i_count - 1;

    free( psz_original );
}

/**
 * Unload a list of logo and release associated resources.
 */
static void LogoListUnload( logo_list_t *p_list )
{
    for( unsigned i = 0; i < p_list->i_count; i++ )
    {
        logo_t *p_logo = &p_list->p_logo[i];

        if( p_logo->p_pic )
            picture_Release( p_logo->p_pic );
    }
    free( p_list->p_logo );
}

/**
 * Go to the next logo and return its pointer.
 */
static logo_t *LogoListNext( logo_list_t *p_list, vlc_tick_t i_date )
{
    p_list->i_counter = ( p_list->i_counter + 1 ) % p_list->i_count;

    logo_t *p_logo = LogoListCurrent( p_list );

    p_list->i_next_pic = i_date + VLC_TICK_FROM_MS( p_logo->i_delay != -1 ?
                          p_logo->i_delay : p_list->i_delay );
    return p_logo;
}
/**
 * Return the current logo pointer
 */
static logo_t *LogoListCurrent( logo_list_t *p_list )
{
    return &p_list->p_logo[p_list->i_counter];
}
