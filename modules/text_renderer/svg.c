/*****************************************************************************
 * svg.c : Put SVG on the video
 *****************************************************************************
 * Copyright (C) 2002, 2003 VLC authors and VideoLAN
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
#include <vlc_subpicture.h>
#include <vlc_strings.h>

#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>                                  /* g_object_unref( ) */
#include <librsvg/rsvg.h>
#include <cairo/cairo.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );
static int  RenderText( filter_t *p_filter, subpicture_region_t *p_region_out,
                        subpicture_region_t *p_region_in,
                        const vlc_fourcc_t * );

typedef struct
{
    char *psz_file_template;
    const char *psz_token;
} filter_sys_t;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define SVG_TEMPLATE_BODY_TOKEN   "<!--$SVGBODY$-->"
#define SVG_TEMPLATE_BODY_TOKEN_L 16

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

static void svg_RescaletoFit  ( filter_t *, int *width, int *height, float * );
static picture_t * svg_RenderPicture ( filter_t *p_filter, const char * );

static void svg_LoadTemplate( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    char *psz_template = NULL;
    char *psz_filename = var_InheritString( p_filter, "svg-template-file" );
    if( psz_filename && psz_filename[0] )
    {
        /* Read the template */
        FILE *file = vlc_fopen( psz_filename, "rt" );
        if( !file )
        {
            msg_Warn( p_filter, "SVG template file %s does not exist.",
                                         psz_filename );
        }
        else
        {
            struct stat s;
            if( fstat( fileno( file ), &s ) || ((signed)s.st_size) < 0 )
            {
                msg_Err( p_filter, "SVG template invalid" );
            }
            else
            {
                msg_Dbg( p_filter, "reading %ld bytes from template %s",
                         (unsigned long)s.st_size, psz_filename );

                psz_template = malloc( s.st_size + 1 );
                if( psz_template )
                {
                    psz_template[ s.st_size ] = 0;
                    ssize_t i_read = fread( psz_template, s.st_size, 1, file );
                    if( i_read != 1 )
                    {
                        free( psz_template );
                        psz_template = NULL;
                    }
                }
            }
            fclose( file );
        }
    }
    free( psz_filename );

    if( psz_template )
    {
        p_sys->psz_token = strstr( psz_template, SVG_TEMPLATE_BODY_TOKEN );
        if( !p_sys->psz_token )
        {
            msg_Err( p_filter, "'%s' not found in SVG template", SVG_TEMPLATE_BODY_TOKEN );
            free( psz_template );
        }
        else *((char*)p_sys->psz_token) = 0;
    }

    p_sys->psz_file_template = psz_template;
}

static char *svg_GetDocument( filter_t *p_filter, int i_width, int i_height, const char *psz_body )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    char *psz_result;
    VLC_UNUSED(i_width);VLC_UNUSED(i_height);

    if( p_sys->psz_file_template )
    {
        if( asprintf( &psz_result, "%s%s%s",
                      p_sys->psz_file_template,
                      psz_body,
                      &p_sys->psz_token[SVG_TEMPLATE_BODY_TOKEN_L] ) < 0 )
            psz_result = NULL;
    }
    else
    {
        /* Either there was no file, or there was an error.
           Use the default value */
        const char *psz_temp = "<?xml version='1.0' encoding='UTF-8' standalone='no'?>"
                    "<svg preserveAspectRatio='xMinYMin meet'>" // viewBox='0 0 %d %d'>"
                    "<rect fill='none' width='100%%' height='100%%'></rect>"
                    "<text fill='white' font-family='sans-serif' font-size='32px'>%s</text>"
                    "</svg>";
        if( asprintf( &psz_result, psz_temp, /*i_width, i_height,*/ psz_body ) < 0 )
            psz_result = NULL;
    }

    return psz_result;
}

/*****************************************************************************
 * Create: allocates svg video thread output method
 *****************************************************************************
 * This function allocates and initializes a  vout method.
 *****************************************************************************/

static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = ( filter_t * )p_this;

    filter_sys_t *p_sys = calloc( 1, sizeof(*p_sys) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_filter->p_sys = p_sys;

    p_filter->pf_render = RenderText;
    svg_LoadTemplate( p_filter );

#if (GLIB_MAJOR_VERSION < 2 || GLIB_MINOR_VERSION < 36)
    g_type_init( );
#endif

    return VLC_SUCCESS;
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
#if (GLIB_MAJOR_VERSION < 2 || GLIB_MINOR_VERSION < 36)
    rsvg_term();
#endif
    free( p_sys->psz_file_template );
    free( p_sys );
}

static void svg_RescaletoFit( filter_t *p_filter, int *width, int *height, float *scale )
{
    *scale = 1.0;

    if( *width > 0 && *height > 0 )
    {
        if( (unsigned)*width > p_filter->fmt_out.video.i_visible_width )
            *scale = (1.0 * p_filter->fmt_out.video.i_visible_width / *width);

        if( (unsigned)*height > p_filter->fmt_out.video.i_visible_height )
        {
            float y_scale = (1.0 * p_filter->fmt_out.video.i_visible_height / *height);
            if( y_scale < *scale )
                *scale = y_scale;
        }

        *width *= *scale;
        *height *= *scale;
    }
}

static picture_t * svg_RenderPicture( filter_t *p_filter,
                                      const char *psz_svgdata )
{
    RsvgHandle *p_handle;
    GError *error = NULL;

    p_handle = rsvg_handle_new_from_data( (const guint8 *)psz_svgdata,
                                          strlen( psz_svgdata ), &error );
    if( !p_handle )
    {
        msg_Err( p_filter, "error while rendering SVG: %s", error->message );
        return NULL;
    }

    RsvgDimensionData dim;
    rsvg_handle_get_dimensions( p_handle, &dim );
    float scale;
    svg_RescaletoFit( p_filter, &dim.width, &dim.height, &scale );

    /* Create a new subpicture region */
    video_format_t fmt;
    video_format_Init( &fmt, VLC_CODEC_BGRA ); /* CAIRO_FORMAT_ARGB32 == VLC_CODEC_BGRA, go figure */
    fmt.i_bits_per_pixel = 32;
    fmt.i_chroma = VLC_CODEC_BGRA;
    fmt.i_width = fmt.i_visible_width = dim.width;
    fmt.i_height = fmt.i_visible_height = dim.height;

    picture_t *p_picture = picture_NewFromFormat( &fmt );
    if( !p_picture )
    {
        video_format_Clean( &fmt );
        g_object_unref( G_OBJECT( p_handle ) );
        return NULL;
    }
    memset( p_picture->p[0].p_pixels, 0x00, p_picture->p[0].i_pitch * p_picture->p[0].i_lines );

    cairo_surface_t* surface = cairo_image_surface_create_for_data( p_picture->p->p_pixels,
                                                                    CAIRO_FORMAT_ARGB32,
                                                                    fmt.i_width, fmt.i_height,
                                                                    p_picture->p[0].i_pitch );
    if( !surface )
    {
        g_object_unref( G_OBJECT( p_handle ) );
        picture_Release( p_picture );
        return NULL;
    }

    cairo_t *cr = cairo_create( surface );
    if( !cr )
    {
        msg_Err( p_filter, "error while creating cairo surface" );
        cairo_surface_destroy( surface );
        g_object_unref( G_OBJECT( p_handle ) );
        picture_Release( p_picture );
        return NULL;
    }

    if( ! rsvg_handle_render_cairo( p_handle, cr ) )
    {
        msg_Err( p_filter, "error while rendering SVG" );
        cairo_destroy( cr );
        cairo_surface_destroy( surface );
        g_object_unref( G_OBJECT( p_handle ) );
        picture_Release( p_picture );
        return NULL;
    }

    cairo_destroy( cr );
    cairo_surface_destroy( surface );
    g_object_unref( G_OBJECT( p_handle ) );

    return p_picture;
}

static char * SegmentsToSVG( text_segment_t *p_segment, int i_height, int *pi_total_size )
{
    char *psz_result = NULL;

    i_height = 6 * i_height / 100;
    *pi_total_size = 0;

    for( ; p_segment; p_segment = p_segment->p_next )
    {
        char *psz_prev = psz_result;
        char *psz_encoded = vlc_xml_encode( p_segment->psz_text );
        if( asprintf( &psz_result, "%s<tspan x='0' dy='%upx'>%s</tspan>\n",
                                   (psz_prev) ? psz_prev : "",
                                    i_height,
                                    psz_encoded ) < 0 )
            psz_result = NULL;
        free( psz_prev );
        free( psz_encoded );

        *pi_total_size += i_height;
    }

    return psz_result;
}

static int RenderText( filter_t *p_filter, subpicture_region_t *p_region_out,
                       subpicture_region_t *p_region_in,
                       const vlc_fourcc_t *p_chroma_list )
{
    /* Sanity check */
    if( !p_region_in || !p_region_out || !p_region_in->p_text )
        return VLC_EGENERIC;

    for( size_t i=0; p_chroma_list[i]; i++ )
    {
        if( p_chroma_list[i] == VLC_CODEC_BGRA )
            break;
        if( p_chroma_list[i] == 0 )
            return VLC_EGENERIC;
    }

    p_region_out->i_x = p_region_in->i_x;
    p_region_out->i_y = p_region_in->i_y;

    unsigned i_width = p_filter->fmt_out.video.i_visible_width;
    if( (unsigned) p_region_out->i_x <= i_width )
        i_width -= p_region_out->i_x;

    unsigned i_height = p_filter->fmt_out.video.i_visible_height;
    if( (unsigned) p_region_out->i_y <= i_height )
        i_height -= p_region_out->i_y;

    if( i_height == 0 || i_width == 0 )
        return VLC_EGENERIC;

    char *psz_svg;
    /* Check if the data is SVG or pure text. In the latter case,
       convert the text to SVG. FIXME: find a better test */
    if( p_region_in->p_text && strstr( p_region_in->p_text->psz_text, "<svg" ) )
    {
        psz_svg = strdup( p_region_in->p_text->psz_text );
    }
    else
    {
        /* Data is text. Convert to SVG */
        int i_total;
        psz_svg = SegmentsToSVG( p_region_in->p_text, i_height, &i_total );
        if( psz_svg )
        {
            char *psz_doc = svg_GetDocument( p_filter, i_width, i_total, psz_svg );
            free( psz_svg );
            psz_svg = psz_doc;
        }
    }

    if( !psz_svg )
        return VLC_EGENERIC;

    picture_t *p_picture = svg_RenderPicture( p_filter, psz_svg );

    free( psz_svg );

    if (p_picture)
    {
        p_region_out->p_picture = p_picture;
        video_format_Clean( &p_region_out->fmt );
        video_format_Copy( &p_region_out->fmt, &p_picture->format );
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}
