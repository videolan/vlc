/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2005 VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
 *          Gildas Bazin <gbazin@videolan.org>
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

#ifdef HAVE_LINUX_LIMITS_H
#   include <linux/limits.h>
#endif

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include "osd.h"
#include "vlc_block.h"
#include "vlc_filter.h"

#include <math.h>

#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#ifdef SYS_DARWIN
#define DEFAULT_FONT "/System/Library/Fonts/LucidaGrande.dfont"
#elif defined( SYS_BEOS )
#define DEFAULT_FONT "/boot/beos/etc/fonts/ttfonts/Swiss721.ttf"
#elif defined( WIN32 )
#define DEFAULT_FONT "" /* Default font found at run-time */
#else
#define DEFAULT_FONT "/usr/share/fonts/truetype/freefont/FreeSerifBold.ttf"
#endif

#if defined(HAVE_FRIBIDI)
#include <fribidi/fribidi.h>
#endif

typedef struct line_desc_t line_desc_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create ( vlc_object_t * );
static void Destroy( vlc_object_t * );

/* The RenderText call maps to pf_render_string, defined in vlc_filter.h */
static subpicture_region_t *RenderText( filter_t *, subpicture_t *, subpicture_region_t* );
static line_desc_t *NewLine( byte_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FONT_TEXT N_("Font")
#define FONT_LONGTEXT N_("Font filename")
#define FONTSIZE_TEXT N_("Font size in pixels")
#define FONTSIZE_LONGTEXT N_("The size of the fonts used by the osd module. " \
    "If set to something different than 0 this option will override the " \
    "relative font size " )
#define OPACITY_TEXT N_("Opacity, 0..255")
#define OPACITY_LONGTEXT N_("The opacity (inverse of transparency) of overlay text. " \
    "1 = transparent, 255 = totally opaque. " )
#define COLOR_TEXT N_("Text Default Color")
#define COLOR_LONGTEXT N_("The color of overlay text. 1 byte for each color, hexadecimal." \
    "#000000 = all colors off, 0xFF0000 = just Red, 0xFFFFFF = all color on [White]" )
#define FONTSIZER_TEXT N_("Font size")
#define FONTSIZER_LONGTEXT N_("The size of the fonts used by the osd module" )

static int   pi_sizes[] = { 20, 18, 16, 12, 6 };
static char *ppsz_sizes_text[] = { N_("Smaller"), N_("Small"), N_("Normal"),
                                   N_("Large"), N_("Larger") };
static int pi_color_values[] = {
  0x00000000, 0x00808080, 0x00C0C0C0, 0x00FFFFFF, 0x00800000,
  0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00808000, 0x00008000, 0x00008080, 
  0x0000FF00, 0x00800080, 0x00000080, 0x000000FF, 0x0000FFFF }; 

static char *ppsz_color_descriptions[] = {
  N_("Black"), N_("Gray"), N_("Silver"), N_("White"), N_("Maroon"),
  N_("Red"), N_("Fuchsia"), N_("Yellow"), N_("Olive"), N_("Green"), N_("Teal"),
  N_("Lime"), N_("Purple"), N_("Navy"), N_("Blue"), N_("Aqua") };

vlc_module_begin();
    set_shortname( _("Freetype"));
    set_description( _("Freetype2 font renderer") );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_TEXT );

    add_file( "freetype-font", DEFAULT_FONT, NULL, FONT_TEXT, FONT_LONGTEXT,
              VLC_FALSE );

    add_integer( "freetype-fontsize", 0, NULL, FONTSIZE_TEXT,
                 FONTSIZE_LONGTEXT, VLC_TRUE );

    /* opacity valid on 0..255, with default 255 = fully opaque */
    add_integer_with_range( "freetype-opacity", 255, 0, 255, NULL,
        OPACITY_TEXT, OPACITY_LONGTEXT, VLC_FALSE );

    /* hook to the color values list, with default 0x00ffffff = white */
    add_integer( "freetype-color", 0x00FFFFFF, NULL, COLOR_TEXT,
                 COLOR_LONGTEXT, VLC_TRUE );
        change_integer_list( pi_color_values, ppsz_color_descriptions, 0 );

    add_integer( "freetype-rel-fontsize", 16, NULL, FONTSIZER_TEXT,
                 FONTSIZER_LONGTEXT, VLC_FALSE );
        change_integer_list( pi_sizes, ppsz_sizes_text, 0 );

    set_capability( "text renderer", 100 );
    add_shortcut( "text" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/**
 * Private data in a subpicture. Describes a string.
 */
typedef struct subpicture_data_t
{
    int            i_width;
    int            i_height;
    /** The string associated with this subpicture */
    byte_t        *psz_text;
    line_desc_t   *p_lines;

} subpicture_data_t;

struct line_desc_t
{
    /** NULL-terminated list of glyphs making the string */
    FT_BitmapGlyph *pp_glyphs;
    /** list of relative positions for the glyphs */
    FT_Vector      *p_glyph_pos;
    int             i_height;
    int             i_width;
    line_desc_t    *p_next;
};

static subpicture_region_t *Render( filter_t *, subpicture_t *, subpicture_data_t *, uint8_t, int, int, int );
static void FreeString( subpicture_data_t * );
static void FreeLine( line_desc_t * );

/*****************************************************************************
 * filter_sys_t: freetype local data
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the freetype specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    FT_Library     p_library;   /* handle to library     */
    FT_Face        p_face;      /* handle to face object */
    vlc_bool_t     i_use_kerning;
    uint8_t        i_font_opacity; /* freetype-opacity */
    int            i_font_color;  /* freetype-color */
    int            i_red, i_blue, i_green; /* function vars to render */
    int            i_font_size;
    uint8_t        i_opacity; /*  function var to render */
    uint8_t        pi_gamma[256];
};

/*****************************************************************************
 * Create: allocates osd-text video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    char *psz_fontfile = NULL;
    int i, i_error;
    vlc_value_t val;

    /* Allocate structure */
    p_sys = malloc( sizeof( filter_sys_t ) );
    if( !p_sys )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }
    p_sys->p_face = 0;
    p_sys->p_library = 0;
    p_sys->i_font_size = 0;
    
    for( i = 0; i < 256; i++ )
    {
        double f_i_norm, f_pow, f_res;
        f_i_norm = (double)i/ 255.0;
        f_pow = pow( f_i_norm, 0.5f );
        f_res = f_pow * 255.0;
        p_sys->pi_gamma[i] = (uint8_t)f_res;
    }

    var_Create( p_filter, "freetype-font",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_filter, "freetype-fontsize",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_filter, "freetype-rel-fontsize",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_filter, "freetype-opacity",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_filter, "freetype-opacity", &val );
    p_sys->i_font_opacity = __MAX( __MIN( val.i_int, 255 ), 0 );
    var_Create( p_filter, "freetype-color",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_filter, "freetype-color", &val );
    if( ( val.i_int > -1 ) && ( val.i_int < 0x01000000 ) )  /* valid range */
    {
            p_sys->i_font_color = val.i_int;
    }
    else msg_Warn(p_filter, "Invalid freetype color specified, using white [0xFFFFFF]");        

    /* Look what method was requested */
    var_Get( p_filter, "freetype-font", &val );
    psz_fontfile = val.psz_string;
    if( !psz_fontfile || !*psz_fontfile )
    {
        if( psz_fontfile ) free( psz_fontfile );
        psz_fontfile = (char *)malloc( PATH_MAX + 1 );
#ifdef WIN32
        GetWindowsDirectory( psz_fontfile, PATH_MAX + 1 );
        strcat( psz_fontfile, "\\fonts\\arial.ttf" );
#elif SYS_DARWIN
        strcpy( psz_fontfile, DEFAULT_FONT );
#else
        msg_Err( p_filter, "user didn't specify a font" );
        goto error;
#endif
    }

    i_error = FT_Init_FreeType( &p_sys->p_library );
    if( i_error )
    {
        msg_Err( p_filter, "couldn't initialize freetype" );
        goto error;
    }

    i_error = FT_New_Face( p_sys->p_library, psz_fontfile ? psz_fontfile : "",
                           0, &p_sys->p_face );
    if( i_error == FT_Err_Unknown_File_Format )
    {
        msg_Err( p_filter, "file %s have unknown format", psz_fontfile );
        goto error;
    }
    else if( i_error )
    {
        msg_Err( p_filter, "failed to load font file %s", psz_fontfile );
        goto error;
    }

    i_error = FT_Select_Charmap( p_sys->p_face, ft_encoding_unicode );
    if( i_error )
    {
        msg_Err( p_filter, "Font has no unicode translation table" );
        goto error;
    }

    p_sys->i_use_kerning = FT_HAS_KERNING( p_sys->p_face );

    
    var_Get( p_filter, "freetype-fontsize", &val );
    if( val.i_int )
    {
        p_sys->i_font_size = val.i_int;
    }
    else
    {
        var_Get( p_filter, "freetype-rel-fontsize", &val );
        p_sys->i_font_size = (int)p_filter->fmt_out.video.i_height / val.i_int;
    }
    if( p_sys->i_font_size <= 0 )
    {
        msg_Warn( p_filter, "Invalid fontsize, using 12" );
        p_sys->i_font_size = 12;
    }
    msg_Dbg( p_filter, "Using fontsize: %i", p_sys->i_font_size);

    i_error = FT_Set_Pixel_Sizes( p_sys->p_face, 0, p_sys->i_font_size );
    if( i_error )
    {
        msg_Err( p_filter, "couldn't set font size to %d", p_sys->i_font_size );
        goto error;
    }

    if( psz_fontfile ) free( psz_fontfile );
    p_filter->pf_render_string = RenderText;
    p_filter->p_sys = p_sys;
    return VLC_SUCCESS;

 error:
    if( p_sys->p_face ) FT_Done_Face( p_sys->p_face );
    if( p_sys->p_library ) FT_Done_FreeType( p_sys->p_library );
    if( psz_fontfile ) free( psz_fontfile );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Destroy: destroy Clone video thread output method
 *****************************************************************************
 * Clean up all data and library connections
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    FT_Done_Face( p_sys->p_face );
    FT_Done_FreeType( p_sys->p_library );
    free( p_sys );
}

/*****************************************************************************
 * Render: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
static subpicture_region_t *Render( filter_t *p_filter, subpicture_t *p_spu,
                                    subpicture_data_t *p_string,
                                    uint8_t opacity, 
                                    int red, int green, int blue)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    line_desc_t *p_line;
    uint8_t *p_y, *p_u, *p_v, *p_a;
    video_format_t fmt;
    int i, x, y, i_pitch;
    uint8_t i_y, i_u, i_v;  /* YUV values, derived from incoming RGB */
    subpicture_region_t *p_region;
  
    /* calculate text color components: */
    i_y = (uint8_t) ( (  66 * red + 129 * green +  25 * blue + 128) >> 8) +  16;
    i_u = (uint8_t) ( ( -38 * red -  74 * green + 112 * blue + 128) >> 8) + 128;
    i_v = (uint8_t) ( ( 112 * red -  94 * green -  18 * blue + 128) >> 8) + 128;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('Y','U','V','A');
    fmt.i_aspect = VOUT_ASPECT_FACTOR;
    fmt.i_width = fmt.i_visible_width = p_string->i_width + 2;
    fmt.i_height = fmt.i_visible_height = p_string->i_height + 2;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt );
    if( !p_region )
    {
        msg_Err( p_filter, "cannot allocate SPU region" );
        return NULL;
    }

    p_region->i_x = p_region->i_y = 0;
    p_y = p_region->picture.Y_PIXELS;
    p_u = p_region->picture.U_PIXELS;
    p_v = p_region->picture.V_PIXELS;
    p_a = p_region->picture.A_PIXELS;
    i_pitch = p_region->picture.Y_PITCH;

    /* Initialize the region pixels */
    memset( p_y, 0x00, i_pitch * p_region->fmt.i_height );
    memset( p_u, 0x80, i_pitch * p_region->fmt.i_height );
    memset( p_v, 0x80, i_pitch * p_region->fmt.i_height );
    memset( p_a, 0x00, i_pitch * p_region->fmt.i_height );

#define pi_gamma p_sys->pi_gamma

    for( p_line = p_string->p_lines; p_line != NULL; p_line = p_line->p_next )
    {
        int i_glyph_tmax = 0;
        int i_bitmap_offset, i_offset;
        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];
            i_glyph_tmax = __MAX( i_glyph_tmax, p_glyph->top );
        }

        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];

            i_offset = ( p_line->p_glyph_pos[ i ].y +
                i_glyph_tmax - p_glyph->top + 1 ) *
                i_pitch + p_line->p_glyph_pos[ i ].x + p_glyph->left + 1;

            for( y = 0, i_bitmap_offset = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++, i_bitmap_offset++ )
                {
                    if( !pi_gamma[p_glyph->bitmap.buffer[i_bitmap_offset]] )
                        continue;

                    i_offset -= i_pitch;
                    p_a[i_offset + x] = ((uint16_t)p_a[i_offset + x] +
                      pi_gamma[p_glyph->bitmap.buffer[i_bitmap_offset]])*opacity/512;
                    i_offset += i_pitch; x--;
                    p_a[i_offset + x] = ((uint16_t)p_a[i_offset + x] +
                      pi_gamma[p_glyph->bitmap.buffer[i_bitmap_offset]])*opacity/512;
                    x += 2;
                    p_a[i_offset + x] = ((uint16_t)p_a[i_offset + x] +
                      pi_gamma[p_glyph->bitmap.buffer[i_bitmap_offset]])*opacity/512;
                    i_offset += i_pitch; x--;
                    p_a[i_offset + x] = ((uint16_t)p_a[i_offset + x] +
                      pi_gamma[p_glyph->bitmap.buffer[i_bitmap_offset]])*opacity/512;
                    i_offset -= i_pitch;
                }
                i_offset += i_pitch;
            }

            i_offset = ( p_line->p_glyph_pos[ i ].y +
                i_glyph_tmax - p_glyph->top + 1 ) *
                i_pitch + p_line->p_glyph_pos[ i ].x + p_glyph->left + 1;

            for( y = 0, i_bitmap_offset = 0; y < p_glyph->bitmap.rows; y++ )
            {
               for( x = 0; x < p_glyph->bitmap.width; x++, i_bitmap_offset++ )
               {
                   p_y[i_offset + x] = i_y*opacity*pi_gamma[p_glyph->bitmap.buffer[i_bitmap_offset]]/(256*256);
                   p_u[i_offset + x] = i_u; 
                   p_v[i_offset + x] = i_v;
               }
               i_offset += i_pitch;
            }

#undef pi_gamma
        }
    }
    return p_region;
}

/**
 * This function receives a string and creates a subpicture for it. It
 * also calculates the size needed for this string, and renders the
 * needed glyphs into memory. It is used as pf_add_string callback in
 * the vout method by this module
 */
static subpicture_region_t *RenderText( filter_t *p_filter,
                                        subpicture_t *p_subpic,
                                        subpicture_region_t *p_region )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    subpicture_data_t *p_string = 0;
    line_desc_t  *p_line = 0, *p_next = 0, *p_prev = 0;
    int i, i_pen_y, i_pen_x, i_error, i_glyph_index, i_previous;
    uint32_t *psz_unicode, *psz_unicode_orig = 0, i_char, *psz_line_start;
    int i_string_length;
    char *psz_string;
    vlc_iconv_t iconv_handle = (vlc_iconv_t)(-1);
    int i_font_color, i_font_opacity, i_font_size;
    subpicture_region_t *p_res;

    FT_BBox line;
    FT_BBox glyph_size;
    FT_Vector result;
    FT_Glyph tmp_glyph;

    /* Sanity check */
    if( !p_region ) return NULL;
    psz_string = p_region->psz_text;
    if( !psz_string || !*psz_string ) goto error;
    i_font_color   = p_region->i_font_color;
    i_font_opacity = p_region->i_font_opacity;
    i_font_size    = p_region->i_font_size;

    result.x = 0;
    result.y = 0;
    line.xMin = 0;
    line.xMax = 0;
    line.yMin = 0;
    line.yMax = 0;

    /* Create and initialize private data for the subpicture */
    p_string = malloc( sizeof(subpicture_data_t) );
    if( !p_string )
    {
        msg_Err( p_filter, "out of memory" );
        goto error;
    }
    p_string->p_lines = 0;
    p_string->psz_text = strdup( psz_string );

    psz_unicode = psz_unicode_orig =
        malloc( ( strlen(psz_string) + 1 ) * sizeof(uint32_t) );
    if( psz_unicode == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        goto error;
    }
#if defined(WORDS_BIGENDIAN)
    iconv_handle = vlc_iconv_open( "UCS-4BE", "UTF-8" );
#else
    iconv_handle = vlc_iconv_open( "UCS-4LE", "UTF-8" );
#endif
    if( iconv_handle == (vlc_iconv_t)-1 )
    {
        msg_Warn( p_filter, "unable to do conversion" );
        goto error;
    }

    /* Set up the glyphs for the desired font size.  By definition,
       p_sys->i_font_size is a valid value, else the initial Create would
       have failed. Using -1 as a flag to use the freetype-fontsize */
    if ( i_font_size < 0 )  
    {
            FT_Set_Pixel_Sizes( p_sys->p_face, 0, p_sys->i_font_size );
    }
    else          
    {
        i_error = FT_Set_Pixel_Sizes( p_sys->p_face, 0, i_font_size );
        if( i_error )
        {
            msg_Warn( p_filter, "Invalid font size to RenderText, using %d", 
                      p_sys->i_font_size );
            FT_Set_Pixel_Sizes( p_sys->p_face, 0, p_sys->i_font_size );
        }
    }

    {
        char *p_in_buffer, *p_out_buffer;
        size_t i_in_bytes, i_out_bytes, i_out_bytes_left, i_ret;
        i_in_bytes = strlen( psz_string );
        i_out_bytes = i_in_bytes * sizeof( uint32_t );
        i_out_bytes_left = i_out_bytes;
        p_in_buffer = psz_string;
        p_out_buffer = (char *)psz_unicode;
        i_ret = vlc_iconv( iconv_handle, &p_in_buffer, &i_in_bytes,
                           &p_out_buffer, &i_out_bytes_left );

        vlc_iconv_close( iconv_handle );

        if( i_in_bytes )
        {
            msg_Warn( p_filter, "failed to convert string to unicode (%s), "
                      "bytes left %d", strerror(errno), i_in_bytes );
            goto error;
        }
        *(uint32_t*)p_out_buffer = 0;
        i_string_length = (i_out_bytes - i_out_bytes_left) / sizeof(uint32_t);
    }

#if defined(HAVE_FRIBIDI)
    {
        uint32_t *p_fribidi_string;
        FriBidiCharType base_dir = FRIBIDI_TYPE_ON;
        p_fribidi_string = malloc( (i_string_length + 1) * sizeof(uint32_t) );
        fribidi_log2vis( (FriBidiChar*)psz_unicode, i_string_length,
                         &base_dir, (FriBidiChar*)p_fribidi_string, 0, 0, 0 );
        free( psz_unicode_orig );
        psz_unicode = psz_unicode_orig = p_fribidi_string;
        p_fribidi_string[ i_string_length ] = 0;
    }
#endif

    /* Calculate relative glyph positions and a bounding box for the
     * entire string */
    p_line = NewLine( psz_string );
    if( p_line == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        goto error;
    }
    p_string->p_lines = p_line;
    i_pen_x = 0;
    i_pen_y = 0;
    i_previous = 0;
    i = 0;
    psz_line_start = psz_unicode;

#define face p_sys->p_face
#define glyph face->glyph

    while( *psz_unicode )
    {
        i_char = *psz_unicode++;
        if( i_char == '\r' ) /* ignore CR chars wherever they may be */
        {
            continue;
        }

        if( i_char == '\n' )
        {
            psz_line_start = psz_unicode;
            p_next = NewLine( psz_string );
            if( p_next == NULL )
            {
                msg_Err( p_filter, "out of memory" );
                goto error;
            }
            p_line->p_next = p_next;
            p_line->i_width = line.xMax;
            p_line->i_height = face->size->metrics.height >> 6;
            p_line->pp_glyphs[ i ] = NULL;
            p_prev = p_line;
            p_line = p_next;
            result.x = __MAX( result.x, line.xMax );
            result.y += face->size->metrics.height >> 6;
            i_pen_x = 0;
            i_previous = 0;
            line.xMin = 0;
            line.xMax = 0;
            line.yMin = 0;
            line.yMax = 0;
            i_pen_y += face->size->metrics.height >> 6;
#if 0
            msg_Dbg( p_filter, "Creating new line, i is %d", i );
#endif
            i = 0;
            continue;
        }

        i_glyph_index = FT_Get_Char_Index( face, i_char );
        if( p_sys->i_use_kerning && i_glyph_index
            && i_previous )
        {
            FT_Vector delta;
            FT_Get_Kerning( face, i_previous, i_glyph_index,
                            ft_kerning_default, &delta );
            i_pen_x += delta.x >> 6;

        }
        p_line->p_glyph_pos[ i ].x = i_pen_x;
        p_line->p_glyph_pos[ i ].y = i_pen_y;
        i_error = FT_Load_Glyph( face, i_glyph_index, FT_LOAD_DEFAULT );
        if( i_error )
        {
            msg_Err( p_filter, "FT_Load_Glyph returned %d", i_error );
            goto error;
        }
        i_error = FT_Get_Glyph( glyph, &tmp_glyph );
        if( i_error )
        {
            msg_Err( p_filter, "FT_Get_Glyph returned %d", i_error );
            goto error;
        }
        FT_Glyph_Get_CBox( tmp_glyph, ft_glyph_bbox_pixels, &glyph_size );
        i_error = FT_Glyph_To_Bitmap( &tmp_glyph, ft_render_mode_normal,
                                      NULL, 1 );
        if( i_error ) continue;
        p_line->pp_glyphs[ i ] = (FT_BitmapGlyph)tmp_glyph;

        /* Do rest */
        line.xMax = p_line->p_glyph_pos[i].x + glyph_size.xMax - glyph_size.xMin + ((FT_BitmapGlyph)tmp_glyph)->left;
        if( line.xMax > p_filter->fmt_out.video.i_visible_width - 20 )
        {
            p_line->pp_glyphs[ i ] = NULL;
            FreeLine( p_line );
            p_line = NewLine( psz_string );
            if( p_prev )
            {
                p_prev->p_next = p_line;
            }
            else
            {
                p_string->p_lines = p_line;
            }
            while( psz_unicode > psz_line_start && *psz_unicode != ' ' )
            {
                psz_unicode--;
            }
            if( psz_unicode == psz_line_start )
            {
                msg_Warn( p_filter, "unbreakable string" );
                goto error;
            }
            else
            {

                *psz_unicode = '\n';
            }
            psz_unicode = psz_line_start;
            i_pen_x = 0;
            i_previous = 0;
            line.xMin = 0;
            line.xMax = 0;
            line.yMin = 0;
            line.yMax = 0;
            i = 0;
            continue;
        }
        line.yMax = __MAX( line.yMax, glyph_size.yMax );
        line.yMin = __MIN( line.yMin, glyph_size.yMin );

        i_previous = i_glyph_index;
        i_pen_x += glyph->advance.x >> 6;
        i++;
    }

    p_line->i_width = line.xMax;
    p_line->i_height = face->size->metrics.height >> 6;
    p_line->pp_glyphs[ i ] = NULL;
    result.x = __MAX( result.x, line.xMax );
    result.y += line.yMax - line.yMin;
    p_string->i_height = result.y;
    p_string->i_width = result.x;

#undef face
#undef glyph
/* check to see whether to use the default color/opacity, or another one: */
    if( i_font_color < 0 ) 
    {
        p_sys->i_blue  =   p_sys->i_font_color & 0x000000FF;
        p_sys->i_green = ( p_sys->i_font_color & 0x0000FF00 ) >>  8;
        p_sys->i_red   = ( p_sys->i_font_color & 0x00FF0000 ) >> 16;
    }
    else
    {
        p_sys->i_blue  =   i_font_color & 0x000000FF;
        p_sys->i_green = ( i_font_color & 0x0000FF00 ) >>  8;
        p_sys->i_red   = ( i_font_color & 0x00FF0000 ) >> 16;
    }
    if( i_font_opacity < 0 )
    {
        p_sys->i_opacity = p_sys->i_font_opacity;
    }
    else
    {
        p_sys->i_opacity = (uint8_t)( i_font_opacity & 0x000000FF );
    }

    p_res = Render( p_filter, p_subpic, p_string, p_sys->i_opacity, 
                    p_sys->i_red, p_sys->i_green, p_sys->i_blue );
    FreeString( p_string );
    if( psz_unicode_orig ) free( psz_unicode_orig );
    return p_res;

 error:
    FreeString( p_string );
    if( p_subpic ) p_filter->pf_sub_buffer_del( p_filter, p_subpic );
    if( psz_unicode_orig ) free( psz_unicode_orig );
    return NULL;
}

static void FreeLine( line_desc_t *p_line )
{
    unsigned int i;
    for( i = 0; p_line->pp_glyphs[ i ] != NULL; i++ )
    {
        FT_Done_Glyph( (FT_Glyph)p_line->pp_glyphs[ i ] );
    }
    free( p_line->pp_glyphs );
    free( p_line->p_glyph_pos );
    free( p_line );
}

static void FreeString( subpicture_data_t *p_string )
{
    line_desc_t *p_line, *p_next;

    if( !p_string ) return;

    for( p_line = p_string->p_lines; p_line != NULL; p_line = p_next )
    {
        p_next = p_line->p_next;
        FreeLine( p_line );
    }

    free( p_string->psz_text );
    free( p_string );
}

static line_desc_t *NewLine( byte_t *psz_string )
{
    int i_count;
    line_desc_t *p_line = malloc( sizeof(line_desc_t) );
    if( !p_line )
    {
        return NULL;
    }
    p_line->i_height = 0;
    p_line->i_width = 0;
    p_line->p_next = NULL;

    /* We don't use CountUtf8Characters() here because we are not acutally
     * sure the string is utf8. Better be safe than sorry. */
    i_count = strlen( psz_string );

    p_line->pp_glyphs = malloc( sizeof(FT_BitmapGlyph)
                                * ( i_count + 1 ) );
    if( p_line->pp_glyphs == NULL )
    {
        free( p_line );
        return NULL;
    }
    p_line->pp_glyphs[0] = NULL;

    p_line->p_glyph_pos = malloc( sizeof( FT_Vector )
                                  * i_count + 1 );
    if( p_line->p_glyph_pos == NULL )
    {
        free( p_line->pp_glyphs );
        free( p_line );
        return NULL;
    }

    return p_line;
}
