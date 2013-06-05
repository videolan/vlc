/*****************************************************************************
 * svg.c : Put SVG on the video
 *****************************************************************************
 * Copyright (C) 2002, 2003 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Olivier Aubert <oaubert@lisi.univ-lyon1.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option ) any later version.
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>
#include <vlc_filter.h>

#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#elif defined( _WIN32 )
#   include <io.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>                                  /* g_object_unref( ) */
#include <librsvg/rsvg.h>

typedef struct svg_rendition_t svg_rendition_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );
static int  RenderText( filter_t *p_filter, subpicture_region_t *p_region_out,
                        subpicture_region_t *p_region_in,
                        const vlc_fourcc_t * );
static char *svg_GetTemplate( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define TEMPLATE_TEXT N_( "SVG template file" )
#define TEMPLATE_LONGTEXT N_( "Location of a file holding a SVG template "\
        "for automatic string conversion" )

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_category( SUBCAT_INPUT_SCODEC )
    set_capability( "text renderer", 99 )
    add_shortcut( "svg" )
    add_string( "svg-template-file", "", TEMPLATE_TEXT, TEMPLATE_LONGTEXT, true )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/**
   Describes a SVG string to be displayed on the video
*/
struct svg_rendition_t
{
    int            i_width;
    int            i_height;
    int            i_chroma;
    /** The SVG source associated with this subpicture */
    char           *psz_text;
    /* The rendered SVG, as a GdkPixbuf */
    GdkPixbuf      *p_rendition;
};

static int Render( filter_t *, subpicture_region_t *, svg_rendition_t *, int, int);
static char *svg_GetTemplate ();
static void svg_set_size( filter_t *p_filter, int width, int height );
static void svg_SizeCallback  ( int *width, int *height, gpointer data );
static void svg_RenderPicture ( filter_t *p_filter,
                                svg_rendition_t *p_svg );
static void FreeString( svg_rendition_t * );

/*****************************************************************************
 * filter_sys_t: svg local data
 *****************************************************************************
 * This structure is part of the filter thread descriptor.
 * It describes the svg specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    /* The SVG template used to convert strings */
    char          *psz_template;
    /* Default size for rendering. Initialized to the output size. */
    int            i_width;
    int            i_height;
};

/*****************************************************************************
 * Create: allocates svg video thread output method
 *****************************************************************************
 * This function allocates and initializes a  vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = ( filter_t * )p_this;
    filter_sys_t *p_sys;

    /* Allocate structure */
    p_sys = malloc( sizeof( filter_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* Initialize psz_template */
    p_sys->psz_template = svg_GetTemplate( p_this );
    if( !p_sys->psz_template )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_sys->i_width = p_filter->fmt_out.video.i_width;
    p_sys->i_height = p_filter->fmt_out.video.i_height;

    p_filter->pf_render_text = RenderText;
    p_filter->pf_render_html = NULL;
    p_filter->p_sys = p_sys;

    /* MUST call this before any RSVG funcs */
    rsvg_init( );

    return VLC_SUCCESS;
}

static char *svg_GetTemplate( vlc_object_t *p_this )
{
    filter_t *p_filter = ( filter_t * )p_this;
    char *psz_filename;
    char *psz_template;
    FILE *file;

    psz_filename = var_InheritString( p_filter, "svg-template-file" );
    if( !psz_filename || (psz_filename[0] == 0) )
    {
        /* No filename. Use a default value. */
        psz_template = NULL;
    }
    else
    {
        /* Read the template */
        file = vlc_fopen( psz_filename, "rt" );
        if( !file )
        {
            msg_Warn( p_this, "SVG template file %s does not exist.",
                                         psz_filename );
            psz_template = NULL;
        }
        else
        {
            struct stat s;

            if( fstat( fileno( file ), &s ) )
            {
                /* Problem accessing file information. Should not
                   happen as we could open it. */
                psz_template = NULL;
            }
            else
            if( ((signed)s.st_size) < 0 )
            {
                msg_Err( p_this, "SVG template too big" );
                psz_template = NULL;
            }
            else
            {
                msg_Dbg( p_this, "reading %ld bytes from template %s",
                         (unsigned long)s.st_size, psz_filename );

                psz_template = calloc( 1, s.st_size + 42 );
                if( !psz_template )
                {
                    fclose( file );
                    free( psz_filename );
                    return NULL;
                }
                if(! fread( psz_template, s.st_size, 1, file ) )
                {
                    msg_Dbg( p_this, "No data read from template." );
                }
            }
            fclose( file );
        }
    }
    free( psz_filename );
    if( !psz_template )
    {
        /* Either there was no file, or there was an error.
           Use the default value */
        psz_template = strdup( "<?xml version='1.0' encoding='UTF-8' standalone='no'?> \
<svg version='1' preserveAspectRatio='xMinYMin meet' viewBox='0 0 800 600'> \
  <text x='10' y='560' fill='white' font-size='32'  \
        font-family='sans-serif'>%s</text></svg>" );
    }

    return psz_template;
}

/*****************************************************************************
 * Destroy: destroy Clone video thread output method
 *****************************************************************************
 * Clean up all data and library connections
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = ( filter_t * )p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys->psz_template );
    free( p_sys );
    rsvg_term( );
}

/*****************************************************************************
 * Render: render SVG in picture
 *****************************************************************************/
static int Render( filter_t *p_filter, subpicture_region_t *p_region,
                   svg_rendition_t *p_svg, int i_width, int i_height )
{
    video_format_t fmt;
    uint8_t *p_y, *p_u, *p_v, *p_a;
    int x, y, i_pitch, i_u_pitch;
    guchar *pixels_in = NULL;
    int rowstride_in;
    int channels_in;
    int alpha;
    picture_t *p_pic;

    if ( p_filter->p_sys->i_width != i_width ||
         p_filter->p_sys->i_height != i_height )
    {
        svg_set_size( p_filter, i_width, i_height );
        p_svg->p_rendition = NULL;
    }

    if( p_svg->p_rendition == NULL ) {
        svg_RenderPicture( p_filter, p_svg );
        if( ! p_svg->p_rendition )
        {
            msg_Err( p_filter, "Cannot render SVG" );
            return VLC_EGENERIC;
        }
    }
    i_width = gdk_pixbuf_get_width( p_svg->p_rendition );
    i_height = gdk_pixbuf_get_height( p_svg->p_rendition );

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof( video_format_t ) );
    fmt.i_chroma = VLC_CODEC_YUVA;
    fmt.i_width = fmt.i_visible_width = i_width;
    fmt.i_height = fmt.i_visible_height = i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    p_region->p_picture = picture_NewFromFormat( &fmt );
    if( !p_region->p_picture )
        return VLC_EGENERIC;
    p_region->fmt = fmt;

    p_region->i_x = p_region->i_y = 0;
    p_y = p_region->p_picture->Y_PIXELS;
    p_u = p_region->p_picture->U_PIXELS;
    p_v = p_region->p_picture->V_PIXELS;
    p_a = p_region->p_picture->A_PIXELS;

    i_pitch = p_region->p_picture->Y_PITCH;
    i_u_pitch = p_region->p_picture->U_PITCH;

    /* Initialize the region pixels (only the alpha will be changed later) */
    memset( p_y, 0x00, i_pitch * p_region->fmt.i_height );
    memset( p_u, 0x80, i_u_pitch * p_region->fmt.i_height );
    memset( p_v, 0x80, i_u_pitch * p_region->fmt.i_height );

    p_pic = p_region->p_picture;

    /* Copy the data */

    /* This rendering code is in no way optimized. If someone has some time to
       make it work faster or better, please do.
    */

    /*
      p_pixbuf->get_rowstride() is the number of bytes in a line.
      p_pixbuf->get_height() is the number of lines.

      The number of bytes of p_pixbuf->p_pixels is get_rowstride * get_height

      if( has_alpha() ) {
      alpha = pixels [ n_channels * ( y*rowstride + x ) + 3 ];
      }
      red   = pixels [ n_channels * ( y*rowstride ) + x ) ];
      green = pixels [ n_channels * ( y*rowstride ) + x ) + 1 ];
      blue  = pixels [ n_channels * ( y*rowstride ) + x ) + 2 ];
    */

    pixels_in = gdk_pixbuf_get_pixels( p_svg->p_rendition );
    rowstride_in = gdk_pixbuf_get_rowstride( p_svg->p_rendition );
    channels_in = gdk_pixbuf_get_n_channels( p_svg->p_rendition );
    alpha = gdk_pixbuf_get_has_alpha( p_svg->p_rendition );

    /*
      This crashes the plugin (if !alpha). As there is always an alpha value,
      it does not matter for the moment :

    if( !alpha )
      memset( p_a, 0xFF, i_pitch * p_region->fmt.i_height );
    */

#define INDEX_IN( x, y ) ( y * rowstride_in + x * channels_in )
#define INDEX_OUT( x, y ) ( y * i_pitch + x * p_pic->p[Y_PLANE].i_pixel_pitch )

    for( y = 0; y < i_height; y++ )
    {
        for( x = 0; x < i_width; x++ )
        {
            guchar *p_in;
            int i_out;

            p_in = &pixels_in[INDEX_IN( x, y )];

#define R( pixel ) *pixel
#define G( pixel ) *( pixel+1 )
#define B( pixel ) *( pixel+2 )
#define ALPHA( pixel ) *( pixel+3 )

            /* From http://www.geocrawler.com/archives/3/8263/2001/6/0/6020594/ :
               Y = 0.29900 * R + 0.58700 * G + 0.11400 * B
               U = -0.1687 * r  - 0.3313 * g + 0.5 * b + 128
               V = 0.5   * r - 0.4187 * g - 0.0813 * b + 128
            */
            if ( alpha ) {
                i_out = INDEX_OUT( x, y );

                p_pic->Y_PIXELS[i_out] = .299 * R( p_in ) + .587 * G( p_in ) + .114 * B( p_in );

                p_pic->U_PIXELS[i_out] = -.1687 * R( p_in ) - .3313 * G( p_in ) + .5 * B( p_in ) + 128;
                p_pic->V_PIXELS[i_out] = .5 * R( p_in ) - .4187 * G( p_in ) - .0813 * B( p_in ) + 128;

                p_pic->A_PIXELS[i_out] = ALPHA( p_in );
            }
        }
    }

    return VLC_SUCCESS;
}

static void svg_set_size( filter_t *p_filter, int width, int height )
{
  p_filter->p_sys->i_width = width;
  p_filter->p_sys->i_height = height;
}

static void svg_SizeCallback( int *width, int *height, gpointer data )
{
    filter_t *p_filter = data;

    *width = p_filter->p_sys->i_width;
    *height = p_filter->p_sys->i_height;
    return;
}

static void svg_RenderPicture( filter_t *p_filter,
                               svg_rendition_t *p_svg )
{
    /* Render the SVG string p_string->psz_text into a new picture_t
       p_string->p_rendition with dimensions ( ->i_width, ->i_height ) */
    RsvgHandle *p_handle;
    GError *error = NULL;

    p_svg->p_rendition = NULL;

    p_handle = rsvg_handle_new();

    if( !p_handle )
    {
        msg_Err( p_filter, "Error creating SVG reader" );
        return;
    }

    rsvg_handle_set_size_callback( p_handle, svg_SizeCallback, p_filter, NULL );

    if( ! rsvg_handle_write( p_handle,
                 ( guchar* )p_svg->psz_text, strlen( p_svg->psz_text ),
                 &error ) )
    {
        msg_Err( p_filter, "error while rendering SVG: %s", error->message );
        g_object_unref( G_OBJECT( p_handle ) );
        return;
    }

    if( ! rsvg_handle_close( p_handle, &error ) )
    {
        msg_Err( p_filter, "error while rendering SVG (close): %s", error->message );
        g_object_unref( G_OBJECT( p_handle ) );
        return;
    }

    p_svg->p_rendition = rsvg_handle_get_pixbuf( p_handle );

    g_object_unref( G_OBJECT( p_handle ) );
}


static int RenderText( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in,
                       const vlc_fourcc_t *p_chroma_list )
{
    VLC_UNUSED(p_chroma_list);

    filter_sys_t *p_sys = p_filter->p_sys;
    svg_rendition_t *p_svg = NULL;
    char *psz_string;

    /* Sanity check */
    if( !p_region_in || !p_region_out ) return VLC_EGENERIC;
    psz_string = p_region_in->psz_text;
    if( !psz_string || !*psz_string ) return VLC_EGENERIC;

    p_svg = malloc( sizeof( svg_rendition_t ) );
    if( !p_svg )
        return VLC_ENOMEM;

    p_region_out->i_x = p_region_in->i_x;
    p_region_out->i_y = p_region_in->i_y;

    /* Check if the data is SVG or pure text. In the latter case,
       convert the text to SVG. FIXME: find a better test */
    if( strstr( psz_string, "<svg" ))
    {
        /* Data is SVG: duplicate */
        p_svg->psz_text = strdup( psz_string );
        if( !p_svg->psz_text )
        {
            free( p_svg );
            return VLC_ENOMEM;
        }
    }
    else
    {
        /* Data is text. Convert to SVG */
        /* FIXME: handle p_style attributes */
        int length;
        char* psz_template = p_sys->psz_template;
        length = strlen( psz_string ) + strlen( psz_template ) + 42;
        p_svg->psz_text = calloc( 1, length + 1 );
        if( !p_svg->psz_text )
        {
            free( p_svg );
            return VLC_ENOMEM;
        }
        snprintf( p_svg->psz_text, length, psz_template, psz_string );
    }
    p_svg->i_width = p_sys->i_width;
    p_svg->i_height = p_sys->i_height;
    p_svg->i_chroma = VLC_CODEC_YUVA;

    /* Render the SVG.
       The input data is stored in the p_string structure,
       and the function updates the p_rendition attribute. */
    svg_RenderPicture( p_filter, p_svg );

    Render( p_filter, p_region_out, p_svg, p_svg->i_width, p_svg->i_height );
    FreeString( p_svg );

    return VLC_SUCCESS;
}

static void FreeString( svg_rendition_t *p_svg )
{
    free( p_svg->psz_text );
    /* p_svg->p_rendition is a GdkPixbuf, and its allocation is
       managed through ref. counting */
    if( p_svg->p_rendition )
        g_object_unref( p_svg->p_rendition );
    free( p_svg );
}
