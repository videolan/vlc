/*****************************************************************************
 * svg.c : Put SVG on the video
 *****************************************************************************
 * Copyright (C) 2002, 2003 VideoLAN
 * $Id: svg.c,v 1.2 2003/07/23 17:26:56 oaubert Exp $
 *
 * Authors: Olivier Aubert <oaubert@lisi.univ-lyon1.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option ) any later version.
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
#include <stdlib.h>                                      /* malloc( ), free( ) */
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include <librsvg-2/librsvg/rsvg.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );
static void Render    ( vout_thread_t *, picture_t *,
                        const subpicture_t * );
static subpicture_t *AddText ( vout_thread_t *p_vout, int i_channel,
                               char *psz_string, text_style_t *p_style, int i_flags,
                               int i_hmargin, int i_vmargin, mtime_t i_start,
                               mtime_t i_stop );
static byte_t *svg_GetTemplate ();
static void svg_SizeCallback  (int *width, int *height, gpointer data );
static void svg_RenderPicture (vout_thread_t *p_vout,
                               subpicture_sys_t *p_string );
static void FreeString( subpicture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define TEMPLATE_TEXT N_( "SVG template file" )
#define TEMPLATE_LONGTEXT N_( "Location of a file holding a SVG template for automatic string conversion" )

vlc_module_begin();
 set_capability( "text renderer", 100 );
 add_shortcut( "svg" );
 add_string( "svg-template-file", "", NULL, TEMPLATE_TEXT, TEMPLATE_LONGTEXT, VLC_TRUE );
 set_callbacks( Create, Destroy );
vlc_module_end();

/**
   Describes a SVG string to be displayed on the video
*/
struct subpicture_sys_t
{
    int            i_width;
    int            i_height;
    int            i_chroma;
    /** The SVG source associated with this subpicture */
    byte_t        *psz_text;
    /* The rendered SVG, as a GdkPixbuf */
    GdkPixbuf      *p_rendition;
};

/*****************************************************************************
 * vout_sys_t: svg local data
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the svg specific properties of an output thread.
 *****************************************************************************/
struct text_renderer_sys_t
{
    /* The SVG template used to convert strings */
    byte_t        *psz_template;
    vlc_mutex_t   *lock;
};

/*****************************************************************************
 * Create: allocates svg video thread output method
 *****************************************************************************
 * This function allocates and initializes a  vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = ( vout_thread_t * )p_this;

    /* Allocate structure */
    p_vout->p_text_renderer_data = malloc( sizeof( text_renderer_sys_t ) );
    if( p_vout->p_text_renderer_data == NULL )
    {
        msg_Err( p_vout, "Out of memory" );
        return VLC_ENOMEM;
    }
    p_vout->pf_add_string = AddText;

    /* Initialize psz_template */
    p_vout->p_text_renderer_data->psz_template = svg_GetTemplate( p_this );
    if( !p_vout->p_text_renderer_data->psz_template )
    {
        msg_Err( p_vout, "Out of memory" );
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

static byte_t *svg_GetTemplate( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = ( vout_thread_t * )p_this;
    char *psz_filename;
    char *psz_template;
    FILE *file;

    psz_filename = config_GetPsz( p_vout, "svg-template-file" );
    if( !psz_filename || psz_filename[0] == 0 )
    {
        /* No filename. Use a default value. */
        psz_template = NULL;
    }
    else
    {
        /* Read the template */
        file = fopen( psz_filename, "rt" );
        if( !file )
        {
            msg_Warn( p_this, "SVG template file %s does not exist.", psz_filename );
            psz_template = NULL;
        }
        else
        {
            struct stat s;
            int i_ret;

            i_ret = lstat( psz_filename, &s );
            if( i_ret )
            {
                /* Problem accessing file information. Should not
                   happen as we could open it. */
                psz_template = NULL;
            }
            else
            {
                fprintf( stderr, "Reading %ld bytes from %s\n", (long)s.st_size, psz_filename );

                psz_template = malloc( s.st_size + 42 );
                if( !psz_template )
                {
                    msg_Err( p_vout, "Out of memory" );
                    return NULL;
                }
                fread( psz_template, s.st_size, 1, file );
                fclose( file );
            }
        }
    }
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
    vout_thread_t *p_vout = ( vout_thread_t * )p_this;
    free( p_vout->p_text_renderer_data->psz_template );
    free( p_vout->p_text_renderer_data );
}

/*****************************************************************************
 * Render: render SVG in picture
 *****************************************************************************
 * This function merges the previously rendered SVG subpicture into a picture
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic,
                    const subpicture_t *p_subpic )
{
    subpicture_sys_t *p_string = p_subpic->p_sys;
    guchar *pixels_in = NULL;
    guchar *pixels_out = NULL;
    int rowstride_in, rowstride_out;
    int channels_in, channels_out;
    int x, y;
    int i_width, i_height;
    int alpha;

    if( p_string->p_rendition == NULL ) {
        /* Something changed ( presumably the dimensions ). Get the new
           dimensions and update the pixbuf */
        p_string->i_width = p_vout->output.i_width;
        p_string->i_height = p_vout->output.i_height;
        svg_RenderPicture( p_vout, p_string );
    }

    /* This rendering code is in no way optimized. If someone has some
       time to lose to make it work faster, please do.
    */

    /* FIXME: The alpha value is not taken into account. */

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

    pixels_in = gdk_pixbuf_get_pixels( p_string->p_rendition );
    pixels_out = p_pic->p->p_pixels;

    rowstride_in = gdk_pixbuf_get_rowstride( p_string->p_rendition );
    rowstride_out = p_pic->p->i_pitch;

    channels_in = gdk_pixbuf_get_n_channels( p_string->p_rendition );
    channels_out = p_pic->p->i_pixel_pitch;

    alpha = gdk_pixbuf_get_has_alpha( p_string->p_rendition );

#define INDEX_IN( x, y ) ( y * rowstride_in + x * channels_in )
#define INDEX_OUT( x, y ) ( y * rowstride_out + x * channels_out )
#define UV_INDEX_OUT( x, y ) ( y * p_pic->p[U_PLANE].i_pitch / 2 + x * p_pic->p[U_PLANE].i_pixel_pitch / 2 )

    i_width = gdk_pixbuf_get_width( p_string->p_rendition );
    i_height = gdk_pixbuf_get_height( p_string->p_rendition );

    switch( p_vout->output.i_chroma )
    {
        /* I420 target, no scaling */
    case VLC_FOURCC( 'I','4','2','0' ):
    case VLC_FOURCC( 'I','Y','U','V' ):
    case VLC_FOURCC( 'Y','V','1','2' ):
        for( y = 0; y < i_height; y++ )
        {
            for( x = 0; x < i_width; x++ )
            {
                guchar *p_in;
                int i_out;
                int i_uv_out;

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
                if( (alpha && ALPHA( p_in ) > 10 ) || ( ! alpha )) {
                    i_out = INDEX_OUT( x, y );

                    p_pic->p[Y_PLANE].p_pixels[i_out] = .299 * R( p_in ) + .587 * G( p_in ) + .114 * B( p_in );

                    if( ( x % 2 == 0 ) && ( y % 2 == 0 ) ) {
                        i_uv_out = UV_INDEX_OUT( x, y );

                        p_pic->p[U_PLANE].p_pixels[i_uv_out] = -.1687 * R( p_in ) - .3313 * G( p_in ) + .5 * B( p_in ) + 128;
                        p_pic->p[V_PLANE].p_pixels[i_uv_out] = .5 * R( p_in ) - .4187 * G( p_in ) - .0813 * B( p_in ) + 128;
                    }
                }
            }
        }
        break;

        /* RV32 target, scaling */
    case VLC_FOURCC( 'R','V','2','4' ):
    case VLC_FOURCC( 'R','V','3','2' ):
        for( y = 0; y < i_height; y++ )
        {
            for( x = 0; x < i_width; x++ )
            {
                guchar *p_in;
                guchar *p_out;

                p_in = &pixels_in[INDEX_IN( x, y )];
                p_out = &pixels_out[INDEX_OUT( x, y )];

                *p_out = *p_in;
                *( p_out+1 ) = *( p_in+1 );
                *( p_out+2 ) = *( p_in+2 );
            }
        }
        break;

    default:
        msg_Err( p_vout, "unknown chroma, can't render SVG" );
        break;
    }
}

static void svg_SizeCallback( int *width, int *height, gpointer data )
{
    subpicture_sys_t *p_string = data;

    *width = p_string->i_width;
    *height = p_string->i_height;
    return;
}

static void svg_RenderPicture( vout_thread_t *p_vout,
                               subpicture_sys_t *p_string )
{
    /* Render the SVG string p_string->psz_text into a new picture_t
       p_string->p_rendition with dimensions ( ->i_width, ->i_height ) */
    RsvgHandle *p_handle;
    GError *error;

    p_handle = rsvg_handle_new();

    rsvg_handle_set_size_callback( p_handle, svg_SizeCallback, p_string, NULL );

    rsvg_handle_write( p_handle,
                       p_string->psz_text, strlen( p_string->psz_text ) + 1,
                       &error );
    rsvg_handle_close( p_handle, &error );

    p_string->p_rendition = rsvg_handle_get_pixbuf( p_handle );
    rsvg_handle_free( p_handle );
}


/**
 * This function receives a SVG string and creates a subpicture for it.
 * It is used as pf_add_string callback in the vout method by this module.
 */
static subpicture_t *AddText ( vout_thread_t *p_vout, int i_channel,
                               char *psz_string, text_style_t *p_style, int i_flags,
                               int i_hmargin, int i_vmargin, mtime_t i_start,
                               mtime_t i_stop )
{
    subpicture_sys_t *p_string;
    subpicture_t *p_subpic;

    msg_Dbg( p_vout, "adding string \"%s\" start_date "I64Fd
             " end_date" I64Fd, psz_string, i_start, i_stop );

    /* Create and initialize a subpicture */
    p_subpic = vout_CreateSubPicture( p_vout, i_channel, GRAPH_CONTENT,
                                      MEMORY_SUBPICTURE );
    if( p_subpic == NULL )
    {
        return NULL;
    }

    p_subpic->pf_render = Render;
    p_subpic->pf_destroy = FreeString;
    p_subpic->i_start = i_start;
    p_subpic->i_stop = i_stop;
    if( i_stop == 0 )
    {
        p_subpic->b_ephemer = VLC_TRUE;
    }
    else
    {
        p_subpic->b_ephemer = VLC_FALSE;
    }

    /* Create and initialize private data for the subpicture */
    p_string = malloc( sizeof( subpicture_sys_t ) );
    if( p_string == NULL )
    {
        vout_DestroySubPicture( p_vout, p_subpic );
        return NULL;
    }
    p_subpic->p_sys = p_string;

    /* Check if the data is SVG or pure text. In the latter case,
       convert the text to SVG. FIXME: find a better test */
    if( strstr( psz_string, "<svg" ))
    {
        /* Data is SVG: duplicate */
        p_string->psz_text = strdup( psz_string );
    }
    else
    {
        /* Data is text. Convert to SVG */
        int length;
        byte_t* psz_template = p_vout->p_text_renderer_data->psz_template;
        length = strlen( psz_string ) + strlen( psz_template ) + 42;
        p_string->psz_text = malloc( length + 1 );
        if( p_string->psz_text == NULL )
        {
            return NULL;
        }
        snprintf( p_string->psz_text, length, psz_template, psz_string );
    }

    p_string->i_width = p_vout->output.i_width;
    p_string->i_height = p_vout->output.i_height;
    p_string->i_chroma = p_vout->output.i_chroma;

    /* Render the SVG.
       The input data is stored in the p_string structure,
       and the function updates the p_rendition attribute. */
    svg_RenderPicture( p_vout, p_string );

    vout_DisplaySubPicture( p_vout, p_subpic );
    return p_subpic;
}

static void FreeString( subpicture_t *p_subpic )
{
    subpicture_sys_t *p_string = p_subpic->p_sys;

    free( p_string->psz_text );
    free( p_string->p_rendition );
    free( p_string );
}
