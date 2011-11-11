/*****************************************************************************
 * fbosd.c : framebuffer osd plugin for vlc
 *****************************************************************************
 * Copyright (C) 2007-2008, the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Paul Saman
 * Copied from modules/video_output/fb.c by Samuel Hocevar <sam@zoy.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>
#include <vlc_modules.h>

#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <fcntl.h>                                                 /* open() */
#include <unistd.h>                                               /* close() */

#include <sys/ioctl.h>
#include <sys/mman.h>                                              /* mmap() */

#include <linux/fb.h>

#include <vlc_image.h>
#include <vlc_interface.h>
#include <vlc_input.h>
#include <vlc_vout.h>
#include <vlc_filter.h>
#include <vlc_osd.h>
#include <vlc_strings.h>

#undef FBOSD_BLENDING
#undef FBOSD_DEBUG

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );
static void Run       ( intf_thread_t * );

static int  Init      ( intf_thread_t * );
static void End       ( intf_thread_t * );

static int  OpenDisplay    ( intf_thread_t * );
static void CloseDisplay   ( intf_thread_t * );

/* Load modules needed for rendering and blending */
#if defined(FBOSD_BLENDING)
static int  OpenBlending     ( intf_thread_t * );
static void CloseBlending    ( intf_thread_t * );
#endif
static int  OpenTextRenderer ( intf_thread_t * );
static void CloseTextRenderer( intf_thread_t * );

/* Manipulate the overlay buffer */
static int  OverlayCallback( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );

static picture_t *AllocatePicture( video_format_t * );
static void DeAllocatePicture( picture_t *, video_format_t * );
static void SetOverlayTransparency( intf_thread_t *,
                                    bool );
static picture_t *LoadImage( intf_thread_t *, video_format_t *,
                             char * );

#if defined(FBOSD_BLENDING)
static int BlendPicture( intf_thread_t *, video_format_t *,
                         video_format_t *, picture_t *, picture_t * );
#else
static picture_t *ConvertImage( intf_thread_t *, picture_t *,
                                video_format_t *, video_format_t * );
#endif
static int RenderPicture( intf_thread_t *, int, int,
                          picture_t *, picture_t * );
static picture_t *RenderText( intf_thread_t *, const char *,
                              text_style_t *, video_format_t * );

#define DEVICE_TEXT N_("Framebuffer device")
#define DEVICE_LONGTEXT N_( \
    "Framebuffer device to use for rendering (usually /dev/fb0).")

#define ASPECT_RATIO_TEXT N_("Video aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_( \
    "Aspect ratio of the video image (4:3, 16:9). Default is square pixels." )

#define FBOSD_IMAGE_TEXT N_("Image file")
#define FBOSD_IMAGE_LONGTEXT N_( \
    "Filename of image file to use on the overlay framebuffer." )

#define ALPHA_TEXT N_("Transparency of the image")
#define ALPHA_LONGTEXT N_( "Transparency value of the new image " \
    "used in blending. By default it set to fully opaque (255). " \
    "(from 0 for full transparency to 255 for full opacity)" )

#define FBOSD_TEXT N_("Text")
#define FBOSD_LONGTEXT N_( "Text to display on the overlay framebuffer." )

#define POSX_TEXT N_("X coordinate")
#define POSX_LONGTEXT N_("X coordinate of the rendered image")

#define POSY_TEXT N_("Y coordinate")
#define POSY_LONGTEXT N_("Y coordinate of the rendered image")

#define POS_TEXT N_("Position")
#define POS_LONGTEXT N_( \
  "You can enforce the picture position on the overlay " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values, e.g. 6=top-right).")

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

#define CLEAR_TEXT N_( "Clear overlay framebuffer" )
#define CLEAR_LONGTEXT N_( "The displayed overlay images is cleared by " \
    "making the overlay completely transparent. All previously rendered " \
    "images and text will be cleared from the cache." )

#define RENDER_TEXT N_( "Render text or image" )
#define RENDER_LONGTEXT N_( "Render the image or text in current overlay " \
    "buffer." )

#define DISPLAY_TEXT N_( "Display on overlay framebuffer" )
#define DISPLAY_LONGTEXT N_( "All rendered images and text will be " \
    "displayed on the overlay framebuffer." )

static const int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_pos_descriptions[] =
{ N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

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

vlc_module_begin ()
    set_shortname( "fbosd" )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )

    add_loadfile( "fbosd-dev", "/dev/fb0", DEVICE_TEXT, DEVICE_LONGTEXT,
                  false )
    add_string( "fbosd-aspect-ratio", "", ASPECT_RATIO_TEXT,
                ASPECT_RATIO_LONGTEXT, true )

    add_string( "fbosd-image", NULL, FBOSD_IMAGE_TEXT,
                FBOSD_IMAGE_LONGTEXT, true )
    add_string( "fbosd-text", NULL, FBOSD_TEXT,
                FBOSD_LONGTEXT, true )

    add_integer_with_range( "fbosd-alpha", 255, 0, 255, ALPHA_TEXT,
                            ALPHA_LONGTEXT, true )

    set_section( N_("Position"), NULL )
    add_integer( "fbosd-x", 0, POSX_TEXT,
                 POSX_LONGTEXT, false )
    add_integer( "fbosd-y", 0, POSY_TEXT,
                 POSY_LONGTEXT, false )
    add_integer( "fbosd-position", 8, POS_TEXT, POS_LONGTEXT, true )
        change_integer_list( pi_pos_values, ppsz_pos_descriptions );

    set_section( N_("Font"), NULL )
    add_integer_with_range( "fbosd-font-opacity", 255, 0, 255,
        OPACITY_TEXT, OPACITY_LONGTEXT, false )
    add_integer( "fbosd-font-color", 0x00FFFFFF, COLOR_TEXT, COLOR_LONGTEXT,
                 false )
        change_integer_list( pi_color_values, ppsz_color_descriptions );
    add_integer( "fbosd-font-size", -1, SIZE_TEXT, SIZE_LONGTEXT,
                 false )

    set_section( N_("Commands"), NULL )
    add_bool( "fbosd-clear", false, CLEAR_TEXT, CLEAR_LONGTEXT, true )
    add_bool( "fbosd-render", false, RENDER_TEXT, RENDER_LONGTEXT, true )
    add_bool( "fbosd-display", false, DISPLAY_TEXT, DISPLAY_LONGTEXT, true )

    set_description( N_("GNU/Linux osd/overlay framebuffer interface") )
    set_capability( "interface", 10 )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * fbosd_render_t: render descriptor
 *****************************************************************************/
struct fbosd_render_t
{
#define FBOSD_RENDER_IMAGE 0
#define FBOSD_RENDER_TEXT  1
    int             i_type;

#define FBOSD_STATE_FREE     0
#define FBOSD_STATE_RESERVED 1
#define FBOSD_STATE_RENDER   2
    int             i_state;

    /* Font style */
    text_style_t*   p_text_style;                            /* font control */
    char            *psz_string;

    /* Position */
    bool            b_absolute;
    int             i_x;
    int             i_y;
    int             i_pos;
    int             i_alpha;                      /* transparency for images */
};
#define FBOSD_RENDER_MAX 10

/*****************************************************************************
 * intf_sys_t: interface framebuffer method descriptor
 *****************************************************************************/
struct intf_sys_t
{
    /* Framebuffer information */
    int                         i_fd;                       /* device handle */
    struct fb_var_screeninfo    var_info;        /* current mode information */
    bool                  b_pan;     /* does device supports panning ? */
    struct fb_cmap              fb_cmap;                /* original colormap */
    uint16_t                    *p_palette;              /* original palette */

    /* Overlay framebuffer format */
    video_format_t  fmt_out;
    picture_t       *p_overlay;
    size_t          i_page_size;                                /* page size */
    int             i_width;
    int             i_height;
    int             i_aspect;
    int             i_bytes_per_pixel;

    /* Image and Picture rendering */
    image_handler_t *p_image;
#if defined(FBOSD_BLENDING)
    filter_t *p_blend;                              /* alpha blending module */
#endif
    filter_t *p_text;                                /* text renderer module */

    /* Render */
    struct fbosd_render_t render[FBOSD_RENDER_MAX];

    /* Font style */
    text_style_t    *p_style;                                /* font control */

    /* Position */
    bool      b_absolute;
    int       i_x;
    int       i_y;
    int       i_pos;

    int       i_alpha;                      /* transparency for images */

    /* commands control */
    bool      b_need_update;    /* update display with \overlay buffer */
    bool      b_clear;      /* clear overlay buffer make it tranparent */
    bool      b_render;   /* render an image or text in overlay buffer */
};

/*****************************************************************************
 * Create: allocates FB interface thread output method
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t    *p_sys;
    char          *psz_aspect;
    char          *psz_tmp;
    int i;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = p_sys = calloc( 1, sizeof( intf_sys_t ) );
    if( !p_intf->p_sys )
        return VLC_ENOMEM;

    p_sys->p_style = text_style_New();
    if( !p_sys->p_style )
    {
        free( p_intf->p_sys );
        return VLC_ENOMEM;
    }

    p_intf->pf_run = Run;

    p_sys->p_image = image_HandlerCreate( p_this );
    if( !p_sys->p_image )
    {
        text_style_Delete( p_sys->p_style );
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_sys->i_alpha = var_CreateGetIntegerCommand( p_intf, "fbosd-alpha" );
    var_AddCallback( p_intf, "fbosd-alpha", OverlayCallback, NULL );

    /* Use PAL by default */
    p_sys->i_width  = p_sys->fmt_out.i_width  = 704;
    p_sys->i_height = p_sys->fmt_out.i_height = 576;

    p_sys->i_aspect = -1;
    psz_aspect =
            var_CreateGetNonEmptyString( p_intf, "fbosd-aspect-ratio" );
    if( psz_aspect )
    {
        char *psz_parser = strchr( psz_aspect, ':' );

        if( psz_parser )
        {
            *psz_parser++ = '\0';
            p_sys->i_aspect = ( atoi( psz_aspect )
                              * VOUT_ASPECT_FACTOR ) / atoi( psz_parser );
            p_sys->fmt_out.i_sar_num = p_sys->i_aspect    * p_sys->i_height;
            p_sys->fmt_out.i_sar_den = VOUT_ASPECT_FACTOR * p_sys->i_width;
        }
        msg_Dbg( p_intf, "using aspect ratio %d:%d",
                  atoi( psz_aspect ), atoi( psz_parser ) );

        free( psz_aspect );
    }

    psz_tmp = var_CreateGetNonEmptyStringCommand( p_intf, "fbosd-image" );
    var_AddCallback( p_intf, "fbosd-image", OverlayCallback, NULL );
    if( psz_tmp && *psz_tmp )
    {
        p_sys->render[0].i_type = FBOSD_RENDER_IMAGE;
        p_sys->render[0].i_state = FBOSD_STATE_RENDER;
        p_sys->render[0].psz_string = strdup( psz_tmp );
    }
    free( psz_tmp );

    psz_tmp = var_CreateGetNonEmptyStringCommand( p_intf, "fbosd-text" );
    var_AddCallback( p_intf, "fbosd-text", OverlayCallback, NULL );
    if( psz_tmp && *psz_tmp )
    {
        p_sys->render[1].i_type = FBOSD_RENDER_TEXT;
        p_sys->render[1].i_state = FBOSD_STATE_RENDER;
        p_sys->render[1].psz_string = strdup( psz_tmp );
    }
    free( psz_tmp );

    p_sys->i_pos = var_CreateGetIntegerCommand( p_intf, "fbosd-position" );
    p_sys->i_x = var_CreateGetIntegerCommand( p_intf, "fbosd-x" );
    p_sys->i_y = var_CreateGetIntegerCommand( p_intf, "fbosd-y" );

    var_AddCallback( p_intf, "fbosd-position", OverlayCallback, NULL );
    var_AddCallback( p_intf, "fbosd-x", OverlayCallback, NULL );
    var_AddCallback( p_intf, "fbosd-y", OverlayCallback, NULL );

    p_sys->p_style->i_font_size =
            var_CreateGetIntegerCommand( p_intf, "fbosd-font-size" );
    p_sys->p_style->i_font_color =
            var_CreateGetIntegerCommand( p_intf, "fbosd-font-color" );
    p_sys->p_style->i_font_alpha = 255 -
            var_CreateGetIntegerCommand( p_intf, "fbosd-font-opacity" );

    var_AddCallback( p_intf, "fbosd-font-color", OverlayCallback, NULL );
    var_AddCallback( p_intf, "fbosd-font-size", OverlayCallback, NULL );
    var_AddCallback( p_intf, "fbosd-font-opacity", OverlayCallback, NULL );

    for( i = 0; i < FBOSD_RENDER_MAX; i++ )
        p_sys->render[i].p_text_style = text_style_New();

    p_sys->b_clear = var_CreateGetBoolCommand( p_intf, "fbosd-clear" );
    p_sys->b_render = var_CreateGetBoolCommand( p_intf, "fbosd-render" );
    p_sys->b_need_update = var_CreateGetBoolCommand( p_intf, "fbosd-display" );

    var_AddCallback( p_intf, "fbosd-clear", OverlayCallback, NULL );
    var_AddCallback( p_intf, "fbosd-render", OverlayCallback, NULL );
    var_AddCallback( p_intf, "fbosd-display", OverlayCallback, NULL );

    /* Check if picture position was overridden */
    p_sys->b_absolute = true;
    if( ( p_sys->i_x >= 0 ) && ( p_sys->i_y >= 0 ) )
    {
        p_sys->b_absolute = false;
        p_sys->i_y = (p_sys->i_y < p_sys->i_height) ?
                        p_sys->i_y : p_sys->i_height;
        p_sys->i_x = (p_sys->i_x < p_sys->i_width) ?
                        p_sys->i_x : p_sys->i_width;
    }

    p_sys->render[0].i_x = p_sys->render[1].i_x = p_sys->i_x;
    p_sys->render[0].i_y = p_sys->render[1].i_y = p_sys->i_y;
    p_sys->render[0].i_pos = p_sys->render[1].i_pos = p_sys->i_pos;
    p_sys->render[0].i_alpha = p_sys->render[1].i_alpha = p_sys->i_alpha;

    /* Initialize framebuffer */
    if( OpenDisplay( p_intf ) )
    {
        Destroy( VLC_OBJECT(p_intf) );
        return VLC_EGENERIC;
    }

    Init( p_intf );

#if defined(FBOSD_BLENDING)
    /* Load the blending module */
    if( OpenBlending( p_intf ) )
    {
        msg_Err( p_intf, "Unable to load image blending module" );
        Destroy( VLC_OBJECT(p_intf) );
        return VLC_EGENERIC;
    }
#endif

    /* Load text renderer module */
    if( OpenTextRenderer( p_intf ) )
    {
        msg_Err( p_intf, "Unable to load text rendering module" );
        Destroy( VLC_OBJECT(p_intf) );
        return VLC_EGENERIC;
    }

    p_sys->b_render = true;
    p_sys->b_need_update = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy FB interface thread output method
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;
    int i;


    p_sys->b_need_update = false;
    p_sys->b_render = false;
    p_sys->b_clear = false;

    var_DelCallback( p_intf, "fbosd-alpha", OverlayCallback, NULL );
    var_Destroy( p_intf, "fbosd-alpha" );

    var_DelCallback( p_intf, "fbosd-x", OverlayCallback, NULL );
    var_DelCallback( p_intf, "fbosd-y", OverlayCallback, NULL );
    var_DelCallback( p_intf, "fbosd-position", OverlayCallback, NULL );
    var_DelCallback( p_intf, "fbosd-image", OverlayCallback, NULL );
    var_DelCallback( p_intf, "fbosd-text", OverlayCallback, NULL );
    var_DelCallback( p_intf, "fbosd-font-size", OverlayCallback, NULL );
    var_DelCallback( p_intf, "fbosd-font-color", OverlayCallback, NULL );
    var_DelCallback( p_intf, "fbosd-font-opacity", OverlayCallback, NULL );
    var_DelCallback( p_intf, "fbosd-clear", OverlayCallback, NULL );
    var_DelCallback( p_intf, "fbosd-render", OverlayCallback, NULL );
    var_DelCallback( p_intf, "fbosd-display", OverlayCallback, NULL );

    var_Destroy( p_intf, "fbosd-x" );
    var_Destroy( p_intf, "fbosd-y" );
    var_Destroy( p_intf, "fbosd-position" );
    var_Destroy( p_intf, "fbosd-image" );
    var_Destroy( p_intf, "fbosd-text" );
    var_Destroy( p_intf, "fbosd-font-size" );
    var_Destroy( p_intf, "fbosd-font-color" );
    var_Destroy( p_intf, "fbosd-font-opacity" );
    var_Destroy( p_intf, "fbosd-clear" );
    var_Destroy( p_intf, "fbosd-render" );
    var_Destroy( p_intf, "fbosd-display" );

    var_Destroy( p_intf, "fbosd-aspect-ratio" );

    CloseDisplay( p_intf );

    for( i = 0; i < FBOSD_RENDER_MAX; i++ )
    {
        free( p_sys->render[i].psz_string );
        p_sys->render[i].i_state = FBOSD_STATE_FREE;
        text_style_Delete( p_sys->render[i].p_text_style );
    }

#if defined(FBOSD_BLENDING)
    if( p_sys->p_blend ) CloseBlending( p_intf );
#endif
    if( p_sys->p_text )  CloseTextRenderer( p_intf );

    if( p_sys->p_image )
        image_HandlerDelete( p_sys->p_image );
    if( p_sys->p_overlay )
        picture_Release( p_sys->p_overlay );

    text_style_Delete( p_sys->p_style );
    free( p_sys );
}

#if defined(FBOSD_BLENDING)
static int OpenBlending( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->p_blend ) return VLC_EGENERIC;

    p_intf->p_sys->p_blend =
            vlc_object_create( p_intf, sizeof(filter_t) );
    p_intf->p_sys->p_blend->fmt_out.video.i_x_offset =
        p_intf->p_sys->p_blend->fmt_out.video.i_y_offset = 0;
    p_intf->p_sys->p_blend->fmt_out.video.i_sar_num =
            p_intf->p_sys->fmt_out.i_sar_num;
    p_intf->p_sys->p_blend->fmt_out.video.i_sar_den =
            p_intf->p_sys->fmt_out.i_sar_den;
    p_intf->p_sys->p_blend->fmt_out.video.i_chroma =
            p_intf->p_sys->fmt_out.i_chroma;
    if( var_InheritBool( p_intf, "freetype-yuvp" ) )
        p_intf->p_sys->p_blend->fmt_in.video.i_chroma =
                VLC_CODEC_YUVP;
    else
        p_intf->p_sys->p_blend->fmt_in.video.i_chroma =
                VLC_CODEC_YUVA;

    p_intf->p_sys->p_blend->p_module =
        module_need( p_intf->p_sys->p_blend, "video blending", NULL, false );

    if( !p_intf->p_sys->p_blend->p_module )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static void CloseBlending( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->p_blend )
    {
        if( p_intf->p_sys->p_blend->p_module )
            module_unneed( p_intf->p_sys->p_blend,
                           p_intf->p_sys->p_blend->p_module );

        vlc_object_release( p_intf->p_sys->p_blend );
    }
}
#endif

static int OpenTextRenderer( intf_thread_t *p_intf )
{
    char *psz_modulename = NULL;

    if( p_intf->p_sys->p_text ) return VLC_EGENERIC;

    p_intf->p_sys->p_text =
            vlc_object_create( p_intf, sizeof(filter_t) );

    p_intf->p_sys->p_text->fmt_out.video.i_width =
        p_intf->p_sys->p_text->fmt_out.video.i_visible_width =
        p_intf->p_sys->i_width;
    p_intf->p_sys->p_text->fmt_out.video.i_height =
        p_intf->p_sys->p_text->fmt_out.video.i_visible_height =
        p_intf->p_sys->i_height;

    psz_modulename = var_CreateGetString( p_intf, "text-renderer" );
    if( psz_modulename && *psz_modulename )
    {
        p_intf->p_sys->p_text->p_module =
            module_need( p_intf->p_sys->p_text, "text renderer",
                            psz_modulename, true );
    }
    if( !p_intf->p_sys->p_text->p_module )
    {
        p_intf->p_sys->p_text->p_module =
            module_need( p_intf->p_sys->p_text, "text renderer", NULL, false );
    }
    free( psz_modulename );

    if( !p_intf->p_sys->p_text->p_module )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static void CloseTextRenderer( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->p_text )
    {
        if( p_intf->p_sys->p_text->p_module )
            module_unneed( p_intf->p_sys->p_text,
                           p_intf->p_sys->p_text->p_module );

        vlc_object_release( p_intf->p_sys->p_text );
    }
}

/*****************************************************************************
 * AllocatePicture:
 * allocate a picture buffer for use with the overlay fb.
 *****************************************************************************/
static picture_t *AllocatePicture( video_format_t *p_fmt )
{
    picture_t *p_picture = picture_NewFromFormat( p_fmt );
    if( !p_picture )
        return NULL;

    if( !p_fmt->p_palette &&
        ( p_fmt->i_chroma == VLC_CODEC_YUVP ) )
    {
        p_fmt->p_palette = malloc( sizeof(video_palette_t) );
        if( !p_fmt->p_palette )
        {
            picture_Release( p_picture );
            return NULL;
        }
    }
    else
    {
        p_fmt->p_palette = NULL;
    }

    return p_picture;
}

/*****************************************************************************
 * DeAllocatePicture:
 * Deallocate a picture buffer and free all associated memory.
 *****************************************************************************/
static void DeAllocatePicture( picture_t *p_pic, video_format_t *p_fmt )
{
    if( p_fmt )
    {
        free( p_fmt->p_palette );
        p_fmt->p_palette = NULL;
    }

    if( p_pic )
        picture_Release( p_pic );
}

/*****************************************************************************
 * SetOverlayTransparency: Set the transparency for this overlay fb,
 * - true is make transparent
 * - false is make non tranparent
 *****************************************************************************/
static void SetOverlayTransparency( intf_thread_t *p_intf,
                                    bool b_transparent )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    size_t i_size = p_sys->fmt_out.i_width * p_sys->fmt_out.i_height
                        * p_sys->i_bytes_per_pixel;
    size_t i_page_size = (p_sys->i_page_size > i_size) ?
                            i_size : p_sys->i_page_size;

    if( p_sys->p_overlay )
    {
        msg_Dbg( p_intf, "Make overlay %s",
                 b_transparent ? "transparent" : "opaque" );
        if( b_transparent )
            memset( p_sys->p_overlay->p[0].p_pixels, 0xFF, i_page_size );
        else
            memset( p_sys->p_overlay->p[0].p_pixels, 0x00, i_page_size );
    }
}

#if defined(FBOSD_BLENDING)
/*****************************************************************************
 * BlendPicture: Blend two pictures together..
 *****************************************************************************/
static int BlendPicture( intf_thread_t *p_intf, video_format_t *p_fmt_src,
                         video_format_t *p_fmt_dst, picture_t *p_pic_src,
                         picture_t *p_pic_dst )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    if( p_sys->p_blend && p_sys->p_blend->p_module )
    {
        int i_x_offset = p_sys->i_x;
        int i_y_offset = p_sys->i_y;

        memcpy( &p_sys->p_blend->fmt_in.video, p_fmt_src, sizeof( video_format_t ) );

        /* Update the output picture size */
        p_sys->p_blend->fmt_out.video.i_width =
            p_sys->p_blend->fmt_out.video.i_visible_width =
                p_fmt_dst->i_width;
        p_sys->p_blend->fmt_out.video.i_height =
            p_sys->p_blend->fmt_out.video.i_visible_height =
                p_fmt_dst->i_height;

        i_x_offset = __MAX( i_x_offset, 0 );
        i_y_offset = __MAX( i_y_offset, 0 );

        p_sys->p_blend->pf_video_blend( p_sys->p_blend, p_pic_dst,
            p_pic_src, p_pic_dst, i_x_offset, i_y_offset,
            p_sys->i_alpha );

        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static int InvertAlpha( intf_thread_t *p_intf, picture_t **p_pic, video_format_t fmt )
{
    uint8_t *p_begin = NULL, *p_end = NULL;
    uint8_t i_skip = 0;

    if( *p_pic && ((*p_pic)->i_planes != 1) )
    {
        msg_Err( p_intf,
                 "cannot invert alpha channel too many planes %d (only 1 supported)",
                 (*p_pic)->i_planes );
        return VLC_EGENERIC;
    }

    switch( fmt.i_chroma )
    {
        case VLC_CODEC_RGB24:
            p_begin = (uint8_t *)(*p_pic)->p[Y_PLANE].p_pixels;
            p_end   = (uint8_t *)(*p_pic)->p[Y_PLANE].p_pixels +
                      ( fmt.i_height * (*p_pic)->p[Y_PLANE].i_pitch );
            i_skip = 3;
            break;
        case VLC_CODEC_RGB32:
            p_begin = (uint8_t *)(*p_pic)->p[Y_PLANE].p_pixels;
            p_end   = (uint8_t *)(*p_pic)->p[Y_PLANE].p_pixels +
                      ( fmt.i_height * (*p_pic)->p[Y_PLANE].i_pitch );
            i_skip = 4;
            break;
        default:
            msg_Err( p_intf, "cannot invert alpha channel chroma not supported %4.4s",
                    (char *)&fmt.i_chroma );
            return VLC_EGENERIC;
    }

    for( ; p_begin < p_end; p_begin += i_skip )
    {
        uint8_t i_opacity = 0;

        if( *p_begin != 0xFF )
            i_opacity = 255 - *p_begin;
        *p_begin = i_opacity;
    }
    /* end of kludge */
    return VLC_SUCCESS;
}
#endif

/*****************************************************************************
 * RenderPicture: Render the picture into the p_dest buffer.
 * We don't take transparent pixels into account, so we don't have to blend
 * the two images together.
 *****************************************************************************/
static int RenderPicture( intf_thread_t *p_intf, int i_x_offset, int i_y_offset,
                          picture_t *p_src, picture_t *p_dest )
{
    int i;
    VLC_UNUSED( p_intf );

    if( !p_dest && !p_src ) return VLC_EGENERIC;

    for( i = 0; i < p_src->i_planes ; i++ )
    {
        if( p_src->p[i].i_pitch == p_dest->p[i].i_pitch )
        {
            /* There are margins, but with the same width : perfect ! */
            vlc_memcpy( p_dest->p[i].p_pixels, p_src->p[i].p_pixels,
                        p_src->p[i].i_pitch * p_src->p[i].i_visible_lines );
        }
        else
        {
            /* We need to proceed line by line */
            uint8_t *p_in  = p_src->p[i].p_pixels;
            uint8_t *p_out = p_dest->p[i].p_pixels;

            int i_x = i_x_offset * p_src->p[i].i_pixel_pitch;
            int i_x_clip, i_y_clip;

            /* Check boundaries, clip the image if necessary */
            i_x_clip = ( i_x + p_src->p[i].i_visible_pitch ) - p_dest->p[i].i_visible_pitch;
            i_x_clip = ( i_x_clip > 0 ) ? i_x_clip : 0;

            i_y_clip = ( i_y_offset + p_src->p[i].i_visible_lines ) - p_dest->p[i].i_visible_lines;
            i_y_clip = ( i_y_clip > 0 ) ? i_y_clip : 0;
#if defined(FBOSD_DEBUG)
            msg_Dbg( p_intf, "i_pitch (%d,%d), (%d,%d)/(%d,%d)",
                     p_dest->p[i].i_visible_pitch, p_src->p[i].i_visible_pitch,
                     i_x_offset, i_y_offset, i_x, i_x_clip );
#endif
            if( ( i_y_offset <= p_dest->p[i].i_visible_lines ) &&
                ( i_x <= p_dest->p[i].i_visible_pitch ) )
            {
                int i_line;

                p_out += ( i_y_offset * p_dest->p[i].i_pitch );
                for( i_line = 0; i_line < ( p_src->p[i].i_visible_lines - i_y_clip ); i_line++ )
                {
                    vlc_memcpy( p_out + i_x, p_in,
                                p_src->p[i].i_visible_pitch - i_x_clip );
                    p_in += p_src->p[i].i_pitch;
                    p_out += p_dest->p[i].i_pitch;
                }
            }
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RenderText - Render text to the desired picture format
 *****************************************************************************/
static picture_t *RenderText( intf_thread_t *p_intf, const char *psz_string,
                              text_style_t *p_style, video_format_t *p_fmt )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    subpicture_region_t *p_region;
    picture_t *p_dest = NULL;

    if( !psz_string ) return p_dest;

    if( p_sys->p_text && p_sys->p_text->p_module )
    {
        video_format_t fmt;

        memset( &fmt, 0, sizeof(fmt) );
        fmt.i_chroma = VLC_CODEC_TEXT;
        fmt.i_width  = fmt.i_visible_width = 0;
        fmt.i_height = fmt.i_visible_height = 0;
        fmt.i_x_offset = 0;
        fmt.i_y_offset = 0;

        p_region = subpicture_region_New( &fmt );
        if( !p_region )
            return p_dest;

        p_region->psz_text = strdup( psz_string );
        if( !p_region->psz_text )
        {
            subpicture_region_Delete( p_region );
            return NULL;
        }
        p_region->p_style = text_style_Duplicate( p_style );
        p_region->i_align = SUBPICTURE_ALIGN_LEFT | SUBPICTURE_ALIGN_TOP;

        if( p_sys->p_text->pf_render_text )
        {
            video_format_t fmt_out;

            memset( &fmt_out, 0, sizeof(video_format_t) );

            p_sys->p_text->pf_render_text( p_sys->p_text,
                                           p_region, p_region, NULL );

#if defined(FBOSD_BLENDING)
            fmt_out = p_region->fmt;
            fmt_out.i_bits_per_pixel = 32;
            vlc_memcpy( p_fmt, &fmt_out, sizeof(video_format_t) );

            /* FIXME not needed to copy the picture anymore no ? */
            p_dest = AllocatePicture( VLC_OBJECT(p_intf), &fmt_out );
            if( !p_dest )
            {
                subpicture_region_Delete( p_region );
                return NULL;
            }
            picture_Copy( p_dest, p_region->p_picture );
#else
            fmt_out.i_chroma = p_fmt->i_chroma;
            p_dest = ConvertImage( p_intf, p_region->p_picture,
                                   &p_region->fmt, &fmt_out );
#endif
            subpicture_region_Delete( p_region );
            return p_dest;
        }
        subpicture_region_Delete( p_region );
    }
    return p_dest;
}

/*****************************************************************************
 * LoadImage: Load an image from file into a picture buffer.
 *****************************************************************************/
static picture_t *LoadImage( intf_thread_t *p_intf, video_format_t *p_fmt,
                             char *psz_file )
{
    picture_t  *p_pic = NULL;

    if( psz_file && p_intf->p_sys->p_image )
    {
        video_format_t fmt_in, fmt_out;

        memset( &fmt_in, 0, sizeof(fmt_in) );
        memset( &fmt_out, 0, sizeof(fmt_out) );

        fmt_out.i_chroma = p_fmt->i_chroma;
        p_pic = image_ReadUrl( p_intf->p_sys->p_image, psz_file,
                               &fmt_in, &fmt_out );

        msg_Dbg( p_intf, "image size %dx%d chroma %4.4s",
                 fmt_out.i_width, fmt_out.i_height,
                 (char *)&p_fmt->i_chroma );
    }
    return p_pic;
}

#if ! defined(FBOSD_BLENDING)
/*****************************************************************************
 * Convertmage: Convert image to another fourcc
 *****************************************************************************/
static picture_t *ConvertImage( intf_thread_t *p_intf, picture_t *p_pic,
                         video_format_t *p_fmt_in, video_format_t *p_fmt_out )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    picture_t  *p_old = NULL;

    if( p_sys->p_image )
    {
        p_old = image_Convert( p_sys->p_image, p_pic, p_fmt_in, p_fmt_out );

        msg_Dbg( p_intf, "converted image size %dx%d chroma %4.4s",
                 p_fmt_out->i_width, p_fmt_out->i_height,
                 (char *)&p_fmt_out->i_chroma );
    }
    return p_old;
}
#endif

/*****************************************************************************
 * Init: initialize framebuffer video thread output method
 *****************************************************************************/
static int Init( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    /* Initialize the output structure: RGB with square pixels, whatever
     * the input format is, since it's the only format we know */
    switch( p_sys->var_info.bits_per_pixel )
    {
    case 8: /* FIXME: set the palette */
        p_sys->fmt_out.i_chroma = VLC_CODEC_RGB8; break;
    case 15:
        p_sys->fmt_out.i_chroma = VLC_CODEC_RGB15; break;
    case 16:
        p_sys->fmt_out.i_chroma = VLC_CODEC_RGB16; break;
    case 24:
        p_sys->fmt_out.i_chroma = VLC_CODEC_RGB24; break;
    case 32:
        p_sys->fmt_out.i_chroma = VLC_CODEC_RGB32; break;
    default:
        msg_Err( p_intf, "unknown screen depth %i",
                 p_sys->var_info.bits_per_pixel );
        return VLC_EGENERIC;
    }

    p_sys->fmt_out.i_bits_per_pixel = p_sys->var_info.bits_per_pixel;
    p_sys->fmt_out.i_width  = p_sys->i_width;
    p_sys->fmt_out.i_height = p_sys->i_height;

    /* Assume we have square pixels */
    if( p_sys->i_aspect < 0 )
    {
        p_sys->fmt_out.i_sar_num = 1;
        p_sys->fmt_out.i_sar_den = 1;
    }
    else
    {
        p_sys->fmt_out.i_sar_num = p_sys->i_aspect    * p_sys->i_height;
        p_sys->fmt_out.i_sar_den = VOUT_ASPECT_FACTOR * p_sys->i_width;
    }

    /* Allocate overlay buffer */
    p_sys->p_overlay = AllocatePicture( &p_sys->fmt_out );
    if( !p_sys->p_overlay ) return VLC_EGENERIC;

    SetOverlayTransparency( p_intf, true );

    /* We know the chroma, allocate a buffer which will be used
     * to write to the overlay framebuffer */
    p_sys->p_overlay->p->i_pixel_pitch = p_sys->i_bytes_per_pixel;
    p_sys->p_overlay->p->i_lines = p_sys->var_info.yres;
    p_sys->p_overlay->p->i_visible_lines = p_sys->var_info.yres;

    if( p_sys->var_info.xres_virtual )
    {
        p_sys->p_overlay->p->i_pitch = p_sys->var_info.xres_virtual
                             * p_sys->i_bytes_per_pixel;
    }
    else
    {
        p_sys->p_overlay->p->i_pitch = p_sys->var_info.xres
                             * p_sys->i_bytes_per_pixel;
    }

    p_sys->p_overlay->p->i_visible_pitch = p_sys->var_info.xres
                                 * p_sys->i_bytes_per_pixel;

    p_sys->p_overlay->i_planes = 1;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate framebuffer interface
 *****************************************************************************/
static void End( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    /* CleanUp */
    SetOverlayTransparency( p_intf, false );
    if( p_sys->p_overlay )
    {
        int ret;
        ret = write( p_sys->i_fd, p_sys->p_overlay->p[0].p_pixels,
                     p_sys->i_page_size );
        if( ret < 0 )
            msg_Err( p_intf, "unable to clear overlay" );
    }

    DeAllocatePicture( p_intf->p_sys->p_overlay,
                       &p_intf->p_sys->fmt_out );
    p_intf->p_sys->p_overlay = NULL;
}

/*****************************************************************************
 * OpenDisplay: initialize framebuffer
 *****************************************************************************/
static int OpenDisplay( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    char *psz_device;                             /* framebuffer device path */
    struct fb_fix_screeninfo    fix_info;     /* framebuffer fix information */

    /* Open framebuffer device */
    if( !(psz_device = var_InheritString( p_intf, "fbosd-dev" )) )
    {
        msg_Err( p_intf, "don't know which fb osd/overlay device to open" );
        return VLC_EGENERIC;
    }

    p_sys->i_fd = vlc_open( psz_device, O_RDWR );
    if( p_sys->i_fd == -1 )
    {
        msg_Err( p_intf, "cannot open %s (%m)", psz_device );
        free( psz_device );
        return VLC_EGENERIC;
    }
    free( psz_device );

    /* Get framebuffer device information */
    if( ioctl( p_sys->i_fd, FBIOGET_VSCREENINFO, &p_sys->var_info ) )
    {
        msg_Err( p_intf, "cannot get fb info (%m)" );
        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    /* Get some info on the framebuffer itself */
    if( ioctl( p_sys->i_fd, FBIOGET_FSCREENINFO, &fix_info ) == 0 )
    {
        p_sys->i_width = p_sys->fmt_out.i_width = p_sys->var_info.xres;
        p_sys->i_height = p_sys->fmt_out.i_height = p_sys->var_info.yres;
    }

    /* FIXME: if the image is full-size, it gets cropped on the left
     * because of the xres / xres_virtual slight difference */
    msg_Dbg( p_intf, "%ix%i (virtual %ix%i)",
             p_sys->var_info.xres, p_sys->var_info.yres,
             p_sys->var_info.xres_virtual,
             p_sys->var_info.yres_virtual );

    p_sys->fmt_out.i_width = p_sys->i_width;
    p_sys->fmt_out.i_height = p_sys->i_height;

    p_sys->p_palette = NULL;
    p_sys->b_pan = ( fix_info.ypanstep || fix_info.ywrapstep );

    switch( p_sys->var_info.bits_per_pixel )
    {
    case 8:
        p_sys->p_palette = malloc( 8 * 256 * sizeof( uint16_t ) );
        if( !p_sys->p_palette )
        {
            close( p_sys->i_fd );
            return VLC_ENOMEM;
        }
        p_sys->fb_cmap.start = 0;
        p_sys->fb_cmap.len = 256;
        p_sys->fb_cmap.red = p_sys->p_palette;
        p_sys->fb_cmap.green  = p_sys->p_palette + 256 * sizeof( uint16_t );
        p_sys->fb_cmap.blue   = p_sys->p_palette + 2 * 256 * sizeof( uint16_t );
        p_sys->fb_cmap.transp = p_sys->p_palette + 3 * 256 * sizeof( uint16_t );

        /* Save the colormap */
        ioctl( p_sys->i_fd, FBIOGETCMAP, &p_sys->fb_cmap );

        p_sys->i_bytes_per_pixel = 1;
        break;

    case 15:
    case 16:
        p_sys->i_bytes_per_pixel = 2;
        break;

    case 24:
        p_sys->i_bytes_per_pixel = 3;
        break;

    case 32:
        p_sys->i_bytes_per_pixel = 4;
        break;

    default:
        msg_Err( p_intf, "screen depth %d is not supported",
                         p_sys->var_info.bits_per_pixel );

        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    p_sys->i_page_size = p_sys->i_width * p_sys->i_height
                         * p_sys->i_bytes_per_pixel;

    msg_Dbg( p_intf, "framebuffer type=%d, visual=%d, ypanstep=%d, "
             "ywrap=%d, accel=%d", fix_info.type, fix_info.visual,
             fix_info.ypanstep, fix_info.ywrapstep, fix_info.accel );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDisplay: terminate FB interface thread
 *****************************************************************************/
static void CloseDisplay( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    /* Restore palette */
    if( p_sys->var_info.bits_per_pixel == 8 )
    {
        ioctl( p_sys->i_fd, FBIOPUTCMAP, &p_sys->fb_cmap );
        free( p_sys->p_palette );
        p_sys->p_palette = NULL;
    }

    /* Close fb */
    close( p_sys->i_fd );
}

static void Render( intf_thread_t *p_intf, struct fbosd_render_t *render )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    if( render->i_state != FBOSD_STATE_RENDER ) return;
    if( !render->psz_string ) return;

    if( render->i_type == FBOSD_RENDER_IMAGE )
    {
        picture_t *p_pic;
        p_pic = LoadImage( p_intf, &p_sys->fmt_out, render->psz_string );
        if( p_pic )
        {
            RenderPicture( p_intf, render->i_x, render->i_y,
                           p_pic, p_sys->p_overlay );
            picture_Release( p_pic );
        }
    }
    else if( render->i_type == FBOSD_RENDER_TEXT )
    {
        picture_t *p_text;
#if defined(FBOSD_BLENDING)
        video_format_t fmt_in;
        memset( &fmt_in, 0, sizeof(video_format_t) );
        p_text = RenderText( p_intf, render->psz_string, render->p_text_style,
                             &fmt_in );
        if( p_text )
        {
            BlendPicture( p_intf, &fmt_in, &p_sys->fmt_out,
                          p_text, p_sys->p_overlay );
            msg_Dbg( p_intf, "releasing picture" );
            DeAllocatePicture( p_text, &fmt_in );
        }
#else
        p_text = RenderText( p_intf, render->psz_string, render->p_text_style,
                             &p_sys->fmt_out );
        if( p_text )
        {
            RenderPicture( p_intf, render->i_x, render->i_y,
                           p_text, p_sys->p_overlay );
            picture_Release( p_text );
        }
#endif
    }
}

static void RenderClear( intf_thread_t *p_intf, struct fbosd_render_t *render )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    text_style_Delete( render->p_text_style );
    render->p_text_style = text_style_New();
    free( render->psz_string );
    render->psz_string = NULL;

    render->i_x = p_sys->i_x;
    render->i_y = p_sys->i_y;
    render->i_pos = p_sys->i_pos;
    render->i_alpha = p_sys->i_alpha;
    render->b_absolute = p_sys->b_absolute;
    render->i_state = FBOSD_STATE_FREE;
}

static bool isRendererReady( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    int i;

    /* Check if there are more items to render */
    for( i = 0; i < FBOSD_RENDER_MAX; i++ )
    {
        if( p_sys->render[i].i_state == FBOSD_STATE_RESERVED )
            return false;
    }
    return true;
}

/*****************************************************************************
 * Run: thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    int canc = vlc_savecancel();

    while( vlc_object_alive( p_intf ) )
    {
        int i;

        /* Is there somthing to render? */
        for( i = 0; i < FBOSD_RENDER_MAX; i++ )
        {
            if( p_sys->render[i].i_state == FBOSD_STATE_RENDER )
            {
                Render( p_intf, &p_sys->render[i] );
                RenderClear( p_intf, &p_sys->render[i] );
            }
        }

        if( p_sys->b_clear )
        {
            SetOverlayTransparency( p_intf, true );

            var_SetString( p_intf, "fbosd-image", "" );
            var_SetString( p_intf, "fbosd-text", "" );

            p_sys->b_clear = false;
            p_sys->b_need_update = true;
        }

        if( p_sys->b_need_update && p_sys->p_overlay &&
            isRendererReady( p_intf ) )
        {
            int ret;
#if defined(FBOSD_BLENDING)
            /* Reverse alpha channel to work around FPGA bug */
            InvertAlpha( p_intf, &p_sys->p_overlay, p_sys->fmt_out );
#endif
            ret = write( p_sys->i_fd, p_sys->p_overlay->p[0].p_pixels,
                         p_sys->i_page_size );
            if( ret < 0 )
                msg_Err( p_intf, "unable to write to overlay" );
            lseek( p_sys->i_fd, 0, SEEK_SET );

            /* clear the picture */
            memset( p_sys->p_overlay->p[0].p_pixels, 0xFF, p_sys->i_page_size );
            p_sys->b_need_update = false;
        }

        msleep( INTF_IDLE_SLEEP );
    }

    End( p_intf );
    vlc_restorecancel( canc );
}

static int OverlayCallback( vlc_object_t *p_this, char const *psz_cmd,
                 vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *) p_this;
    intf_sys_t *p_sys = p_intf->p_sys;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( !strncmp( psz_cmd, "fbosd-display", 13 ) )
    {
        p_sys->b_need_update = true;
    }
    else if( !strncmp( psz_cmd, "fbosd-clear", 11 ) )
    {
        int i;
        /* Clear the entire render list */
        for( i = 0; i < FBOSD_RENDER_MAX; i++ )
        {
            RenderClear( p_intf, &p_sys->render[i] );
        }
        p_sys->b_clear = true;
    }
    else if( !strncmp( psz_cmd, "fbosd-render", 12 ) )
    {
        int i;
        /* Are we already busy with on slot ? */
        for( i = 0; i < FBOSD_RENDER_MAX; i++ )
        {
            if( p_sys->render[i].i_state == FBOSD_STATE_RESERVED )
            {
                p_sys->render[i].i_state = FBOSD_STATE_RENDER;
                break;
            }
        }
    }
    else
    {
        int i;
        /* Are we already busy with on slot ? */
        for( i = 0; i < FBOSD_RENDER_MAX; i++ )
        {
            if( p_sys->render[i].i_state == FBOSD_STATE_RESERVED )
                break;
        }
        /* No, then find first FREE slot */
        if( i == FBOSD_RENDER_MAX )
        {
            for( i = 0; i < FBOSD_RENDER_MAX; i++ )
            {
                if( p_sys->render[i].i_state == FBOSD_STATE_FREE )
                    break;
            }
            if( i == FBOSD_RENDER_MAX )
            {
                msg_Warn( p_this, "render space depleated" );
                return VLC_SUCCESS;
            }
        }
        /* Found a free slot */
        p_sys->render[i].i_state = FBOSD_STATE_RESERVED;
        if( !strncmp( psz_cmd, "fbosd-image", 11 ) )
        {
            free( p_sys->render[i].psz_string );
            p_sys->render[i].psz_string = strdup( newval.psz_string );
            p_sys->render[i].i_type = FBOSD_RENDER_IMAGE;
        }
        else if( !strncmp( psz_cmd, "fbosd-text", 10 ) )
        {
            free( p_sys->render[i].psz_string );
            p_sys->render[i].psz_string = strdup( newval.psz_string );
            p_sys->render[i].i_type = FBOSD_RENDER_TEXT;
        }
        else if( !strncmp( psz_cmd, "fbosd-x", 7 ) )
        {
            p_sys->render[i].b_absolute = false;
            p_sys->render[i].i_x = (newval.i_int < p_sys->i_width) ?
                                    newval.i_int : p_sys->i_width;
        }
        else if( !strncmp( psz_cmd, "fbosd-y", 7 ) )
        {
            p_sys->render[i].b_absolute = false;
            p_sys->render[i].i_y = (newval.i_int < p_sys->i_height) ?
                                    newval.i_int : p_sys->i_height;
        }
        else if( !strncmp( psz_cmd, "fbosd-position", 14 ) )
        {
            p_sys->render[i].b_absolute = true;
            p_sys->render[i].i_pos = newval.i_int;
        }
        else if( !strncmp( psz_cmd, "fbosd-font-size", 15 ) )
        {
            p_sys->render[i].p_text_style->i_font_size = newval.i_int;
        }
        else if( !strncmp( psz_cmd, "fbosd-font-color", 16 ) )
        {
            p_sys->render[i].p_text_style->i_font_color = newval.i_int;
        }
        else if( !strncmp( psz_cmd, "fbosd-font-opacity", 18 ) )
        {
            p_sys->render[i].p_text_style->i_font_alpha = 255 - newval.i_int;
        }
        else if( !strncmp( psz_cmd, "fbosd-alpha", 11 ) )
        {
            p_sys->render[i].i_alpha = newval.i_int;
        }
    }
    return VLC_SUCCESS;
}
