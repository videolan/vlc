/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002, 2003 VideoLAN
 * $Id: freetype.c,v 1.38 2003/12/08 17:48:13 yoann Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
#include <osd.h>
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

#if defined(HAVE_ICONV)
#include <iconv.h>
#endif
#if defined(HAVE_FRIBIDI)
#include <fribidi/fribidi.h>
#endif

typedef struct line_desc_t line_desc_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static void Render    ( vout_thread_t *, picture_t *,
                        const subpicture_t * );
static void RenderI420( vout_thread_t *, picture_t *,
                        const subpicture_t * );
static void RenderYUY2( vout_thread_t *, picture_t *,
                        const subpicture_t * );
static void RenderRV32( vout_thread_t *, picture_t *,
                        const subpicture_t * );
static subpicture_t *AddText ( vout_thread_t *, char *, text_style_t *, int,
                               int, int, mtime_t, mtime_t );

static void FreeString( subpicture_t * );

#if !defined(HAVE_ICONV)
static int  GetUnicodeCharFromUTF8( byte_t ** );
#endif

static line_desc_t *NewLine( byte_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FONT_TEXT N_("Font")
#define FONT_LONGTEXT N_("Filename of Font")
#define FONTSIZE_TEXT N_("Font size in pixels")
#define FONTSIZE_LONGTEXT N_("The size of the fonts used by the osd module. If \
 set to something different than 0 this option will override the relative font size " )
#define FONTSIZER_TEXT N_("Font size")
#define FONTSIZER_LONGTEXT N_("The size of the fonts used by the osd module" )

static int   pi_sizes[] = { 20, 18, 16, 12, 6 };
static char *ppsz_sizes_text[] = { N_("Smaller"), N_("Small"), N_("Normal"),
                                   N_("Large"), N_("Larger") };

vlc_module_begin();
    add_category_hint( N_("Fonts"), NULL, VLC_FALSE );
    add_file( "freetype-font", DEFAULT_FONT, NULL, FONT_TEXT, FONT_LONGTEXT, VLC_FALSE );
    add_integer( "freetype-fontsize", 0, NULL, FONTSIZE_TEXT, FONTSIZE_LONGTEXT, VLC_TRUE );
    add_integer( "freetype-rel-fontsize", 16, NULL, FONTSIZER_TEXT, FONTSIZER_LONGTEXT,
                 VLC_FALSE );
        change_integer_list( pi_sizes, ppsz_sizes_text, 0 );
    set_description( _("freetype2 font renderer") );
    set_capability( "text renderer", 100 );
    add_shortcut( "text" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/**
 * Private data in a subpicture. Describes a string.
 */
struct subpicture_sys_t
{
    int            i_x_margin;
    int            i_y_margin;
    int            i_width;
    int            i_height;
    int            i_flags;
    /** The string associated with this subpicture */
    byte_t        *psz_text;
    line_desc_t   *p_lines;
};

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

/*****************************************************************************
 * text_renderer_sys_t: freetype local data
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the freetype specific properties of an output thread.
 *****************************************************************************/
struct text_renderer_sys_t
{
    FT_Library     p_library;   /* handle to library     */
    FT_Face        p_face;      /* handle to face object */
    vlc_mutex_t   *lock;
    vlc_bool_t     i_use_kerning;
    uint8_t        pi_gamma[256];
};

/*****************************************************************************
 * Create: allocates osd-text video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
 *****************************************************************************/
#define gamma_value 2.0
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *psz_fontfile;
    int i, i_error;
    int i_fontsize = 0;
    double gamma_inv = 1.0f / gamma_value;
    vlc_value_t val;

    /* Allocate structure */
    p_vout->p_text_renderer_data = malloc( sizeof( text_renderer_sys_t ) );
    if( p_vout->p_text_renderer_data == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    for (i = 0; i < 256; i++) {
        p_vout->p_text_renderer_data->pi_gamma[i] =
            (uint8_t)( pow( (double)i / 255.0f, gamma_inv) * 255.0f );
    }

    var_Create( p_vout, "freetype-font", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "freetype-fontsize",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "freetype-rel-fontsize",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* Look what method was requested */
    var_Get( p_vout, "freetype-font", &val );
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
        msg_Err( p_vout, "user didn't specify a font" );
        free( p_vout->p_text_renderer_data );
        return VLC_EGENERIC;
#endif
    }

    i_error = FT_Init_FreeType( &p_vout->p_text_renderer_data->p_library );
    if( i_error )
    {
        msg_Err( p_vout, "couldn't initialize freetype" );
        free( p_vout->p_text_renderer_data );
        return VLC_EGENERIC;
    }

    i_error = FT_New_Face( p_vout->p_text_renderer_data->p_library,
                           psz_fontfile ? psz_fontfile : "", 0,
                           &p_vout->p_text_renderer_data->p_face );
    if( i_error == FT_Err_Unknown_File_Format )
    {
        msg_Err( p_vout, "file %s have unknown format", psz_fontfile );
        FT_Done_FreeType( p_vout->p_text_renderer_data->p_library );
        free( p_vout->p_text_renderer_data );
        if( psz_fontfile ) free( psz_fontfile );
        return VLC_EGENERIC;
    }
    else if( i_error )
    {
        msg_Err( p_vout, "failed to load font file %s", psz_fontfile );
        FT_Done_FreeType( p_vout->p_text_renderer_data->p_library );
        free( p_vout->p_text_renderer_data );
        if( psz_fontfile ) free( psz_fontfile );
        return VLC_EGENERIC;
    }
    if( psz_fontfile ) free( psz_fontfile );

    i_error = FT_Select_Charmap( p_vout->p_text_renderer_data->p_face,
                                 ft_encoding_unicode );
    if( i_error )
    {
        msg_Err( p_vout, "Font has no unicode translation table" );
        FT_Done_Face( p_vout->p_text_renderer_data->p_face );
        FT_Done_FreeType( p_vout->p_text_renderer_data->p_library );
        free( p_vout->p_text_renderer_data );
        return VLC_EGENERIC;
    }

    p_vout->p_text_renderer_data->i_use_kerning =
        FT_HAS_KERNING(p_vout->p_text_renderer_data->p_face);

    var_Get( p_vout, "freetype-fontsize", &val );

    if( val.i_int )
    {
        i_fontsize = val.i_int;
    }
    else
    {
        var_Get( p_vout, "freetype-rel-fontsize", &val );
        i_fontsize = (int) p_vout->render.i_height / val.i_int;
    }
    msg_Dbg( p_vout, "Using fontsize: %i", i_fontsize);

    i_error = FT_Set_Pixel_Sizes( p_vout->p_text_renderer_data->p_face, 0,
                                  i_fontsize );
    if( i_error )
    {
        msg_Err( p_vout, "couldn't set font size to %d", i_fontsize );
        free( p_vout->p_text_renderer_data );
        return VLC_EGENERIC;
    }
    p_vout->pf_add_string = AddText;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Clone video thread output method
 *****************************************************************************
 * Clean up all data and library connections
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    FT_Done_Face( p_vout->p_text_renderer_data->p_face );
    FT_Done_FreeType( p_vout->p_text_renderer_data->p_library );
    free( p_vout->p_text_renderer_data );
}

/*****************************************************************************
 * Render: place string in picture
 *****************************************************************************
 * This function merges the previously rendered freetype glyphs into a picture
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic,
                    const subpicture_t *p_subpic )
{
    switch( p_vout->output.i_chroma )
    {
        /* I420 target, no scaling */
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('Y','V','1','2'):
            RenderI420( p_vout, p_pic, p_subpic );
            break;
#if 0
        /* RV16 target, scaling */
        case VLC_FOURCC('R','V','1','6'):
            RenderRV16( p_vout, p_pic, p_subpic );
            break;
#endif
        /* RV32 target, scaling */
        case VLC_FOURCC('R','V','2','4'):
        case VLC_FOURCC('R','V','3','2'):
            RenderRV32( p_vout, p_pic, p_subpic );
            break;
        /* NVidia or BeOS overlay, no scaling */
        case VLC_FOURCC('Y','U','Y','2'):
            RenderYUY2( p_vout, p_pic, p_subpic );
            break;

        default:
            msg_Err( p_vout, "unknown chroma, can't render SPU" );
            break;
    }
}

/**
 * Draw a string on a i420 (or similar) picture
 */
static void RenderI420( vout_thread_t *p_vout, picture_t *p_pic,
                    const subpicture_t *p_subpic )
{
    subpicture_sys_t *p_string = p_subpic->p_sys;
    int i_plane, x, y, pen_x, pen_y;
    unsigned int i;
    line_desc_t *p_line;

    for( p_line = p_subpic->p_sys->p_lines; p_line != NULL; p_line = p_line->p_next )
    {
        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        {
            uint8_t *p_in;
            int i_pic_pitch = p_pic->p[ i_plane ].i_pitch;
            int i_pic_width = p_pic->p[ i_plane ].i_visible_pitch;

            p_in = p_pic->p[ i_plane ].p_pixels;

            if ( i_plane == 0 )
            {
                if ( p_string->i_flags & OSD_ALIGN_BOTTOM )
                {
                    pen_y = p_pic->p[ i_plane ].i_lines - p_string->i_height -
                        p_string->i_y_margin;
                }
                else
                {
                    pen_y = p_string->i_y_margin;
                }
                pen_y += p_vout->p_text_renderer_data->p_face->size->metrics.ascender >> 6;
                if ( p_string->i_flags & OSD_ALIGN_RIGHT )
                {
                    pen_x = i_pic_width - p_line->i_width
                        - p_string->i_x_margin;
                }
                else if ( p_string->i_flags & OSD_ALIGN_LEFT )
                {
                    pen_x = p_string->i_x_margin;
                }
                else
                {
                    pen_x = i_pic_width / 2 - p_line->i_width / 2
                        + p_string->i_x_margin;
                }

                for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
                {
                    FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];
#define alpha p_vout->p_text_renderer_data->pi_gamma[ p_glyph->bitmap.buffer[ x + y * p_glyph->bitmap.width ] ]
#define pixel p_in[ ( p_line->p_glyph_pos[ i ].y + pen_y + y - p_glyph->top ) * i_pic_pitch + x + pen_x + p_line->p_glyph_pos[ i ].x + p_glyph->left ]
                    for(y = 0; y < p_glyph->bitmap.rows; y++ )
                    {
                        for( x = 0; x < p_glyph->bitmap.width; x++ )
                        {
                            pen_y--;
                            pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                            pen_y++; pen_x--;
                            pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                            pen_x += 2;
                            pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                            pen_y++; pen_x--;
                            pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                            pen_y--;
                        }
                    }
                    for(y = 0; y < p_glyph->bitmap.rows; y++ )
                    {
                        for( x = 0; x < p_glyph->bitmap.width; x++ )
                        {
                            pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 ) +
                                ( 255 * alpha >> 8 );
                        }
                    }
#undef alpha
#undef pixel
                }
            }
            else
            {
                if ( p_string->i_flags & OSD_ALIGN_BOTTOM )
                {
                    pen_y = p_pic->p[i_plane].i_lines - ( p_string->i_height>>1) -
                        (p_string->i_y_margin>>1);
                }
                else
                {
                    pen_y = p_string->i_y_margin >> 1;
                }
                pen_y += p_vout->p_text_renderer_data->p_face->size->metrics.ascender >> 7;
                if ( p_string->i_flags & OSD_ALIGN_RIGHT )
                {
                    pen_x = i_pic_width - ( p_line->i_width >> 1 )
                        - ( p_string->i_x_margin >> 1 );
                }
                else if ( p_string->i_flags & OSD_ALIGN_LEFT )
                {
                    pen_x = p_string->i_x_margin >> 1;
                }
                else
                {
                    pen_x = i_pic_width / 2 - p_line->i_width / 4
                        + p_string->i_x_margin / 2;
                }

                for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
                {
                    FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];
#define alpha p_vout->p_text_renderer_data->pi_gamma[ p_glyph->bitmap.buffer[ ( x + y * p_glyph->bitmap.width ) ] ]
#define pixel p_in[ ( ( p_line->p_glyph_pos[ i ].y >> 1 ) + pen_y + ( y >> 1 ) -  ( p_glyph->top >> 1 ) ) * i_pic_pitch + ( x >> 1 ) + pen_x + ( p_line->p_glyph_pos[ i ].x >> 1 ) + ( p_glyph->left >>1) ]
                    for( y = 0; y < p_glyph->bitmap.rows; y+=2 )
                    {
                        for( x = 0; x < p_glyph->bitmap.width; x+=2 )
                        {
                            pixel = ( ( pixel * ( 0xFF - alpha ) ) >> 8 ) +
                                ( 0x80 * alpha >> 8 );
#undef alpha
#undef pixel
                        }
                    }
                }
            }
        }
    }
}

/**
 * Draw a string on a YUY2 picture
 */
static void RenderYUY2( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_subpic )
{
    subpicture_sys_t *p_string = p_subpic->p_sys;
    int x, y, pen_x, pen_y;
    unsigned int i;
    line_desc_t *p_line;

    for( p_line = p_subpic->p_sys->p_lines; p_line != NULL;
         p_line = p_line->p_next )
    {
        uint8_t *p_in;
        int i_pic_pitch = p_pic->p[0].i_pitch;
        int i_pic_width = p_pic->p[0].i_visible_pitch;

        p_in = p_pic->p[0].p_pixels;

        if ( p_string->i_flags & OSD_ALIGN_BOTTOM )
        {
            pen_y = p_pic->p[0].i_lines - p_string->i_height -
                p_string->i_y_margin;
        }
        else
        {
            pen_y = p_string->i_y_margin;
        }
        pen_y += p_vout->p_text_renderer_data->p_face->size->metrics.ascender >> 6;
        if ( p_string->i_flags & OSD_ALIGN_RIGHT )
        {
            pen_x = i_pic_width - p_line->i_width
                - p_string->i_x_margin;
        }
        else if ( p_string->i_flags & OSD_ALIGN_LEFT )
        {
            pen_x = p_string->i_x_margin;
        }
        else
        {
            pen_x = i_pic_width / 2 /2 - p_line->i_width / 2 + p_string->i_x_margin;
        }

        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];
#define alpha p_vout->p_text_renderer_data->pi_gamma[ p_glyph->bitmap.buffer[ x + y * p_glyph->bitmap.width ] ]
#define pixel p_in[ ( p_line->p_glyph_pos[ i ].y + pen_y + y - p_glyph->top ) * i_pic_pitch + 2 * ( x + pen_x + p_line->p_glyph_pos[ i ].x + p_glyph->left ) ]
            for(y = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++ )
                {
                    pen_y--;
                    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                    pen_y++; pen_x--;
                    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                    pen_x += 2;
                    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                    pen_y++; pen_x--;
                    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                    pen_y--;
                }
            }
            for(y = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++ )
                {
                    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 ) +
                        ( 255 * alpha >> 8 );
                }
            }
#undef alpha
#undef pixel
        }
    }
}

/**
 * Draw a string on a RV32 picture
 */
static void RenderRV32( vout_thread_t *p_vout, picture_t *p_pic,
                    const subpicture_t *p_subpic )
{
    subpicture_sys_t *p_string = p_subpic->p_sys;
    int i_plane, x, y, pen_x, pen_y;
    unsigned int i;
    line_desc_t *p_line;

    i_plane = 0;

    for( p_line = p_subpic->p_sys->p_lines; p_line != NULL; p_line = p_line->p_next )
    {
        uint8_t *p_in;
        int i_pic_pitch = p_pic->p[ i_plane ].i_pitch;
        int i_pic_width = p_pic->p[ i_plane ].i_visible_pitch;

        p_in = p_pic->p[ i_plane ].p_pixels;

        if ( p_string->i_flags & OSD_ALIGN_BOTTOM )
        {
            pen_y = p_pic->p[ i_plane ].i_lines - p_string->i_height -
                p_string->i_y_margin;
        }
        else
        {
            pen_y = p_string->i_y_margin;
        }
        pen_y += p_vout->p_text_renderer_data->p_face->size->metrics.ascender >> 6;
        if ( p_string->i_flags & OSD_ALIGN_RIGHT )
        {
            pen_x = i_pic_width - p_line->i_width
                - p_string->i_x_margin;
        }
        else if ( p_string->i_flags & OSD_ALIGN_LEFT )
        {
            pen_x = p_string->i_x_margin;
        }
        else
        {
            pen_x = i_pic_width / 2 / 4 - p_line->i_width / 2
                + p_string->i_x_margin;
        }

        for( i = 0; p_line->pp_glyphs[i] != NULL; i++ )
        {
            FT_BitmapGlyph p_glyph = p_line->pp_glyphs[ i ];
#define alpha p_vout->p_text_renderer_data->pi_gamma[ p_glyph->bitmap.buffer[ x + y * p_glyph->bitmap.width ] ]
#define pixel( c ) p_in[ ( p_line->p_glyph_pos[ i ].y + pen_y + y - p_glyph->top ) * i_pic_pitch + ( x + pen_x + p_line->p_glyph_pos[ i ].x + p_glyph->left ) * 4 + c ]
            for(y = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++ )
                {
                    pen_y--;
                    pixel( 0 ) = ( ( pixel( 0 ) * ( 255 - alpha ) ) >> 8 );
                    pixel( 1 ) = ( ( pixel( 1 ) * ( 255 - alpha ) ) >> 8 );
                    pixel( 2 ) = ( ( pixel( 2 ) * ( 255 - alpha ) ) >> 8 );
                    pen_y++; pen_x--;
                    pixel( 0 ) = ( ( pixel( 0 ) * ( 255 - alpha ) ) >> 8 );
                    pixel( 1 ) = ( ( pixel( 1 ) * ( 255 - alpha ) ) >> 8 );
                    pixel( 2 ) = ( ( pixel( 2 ) * ( 255 - alpha ) ) >> 8 );
                    pen_x += 2;
                    pixel( 0 ) = ( ( pixel( 0 ) * ( 255 - alpha ) ) >> 8 );
                    pixel( 1 ) = ( ( pixel( 1 ) * ( 255 - alpha ) ) >> 8 );
                    pixel( 2 ) = ( ( pixel( 2 ) * ( 255 - alpha ) ) >> 8 );
                    pen_y++; pen_x--;
                    pixel( 0 ) = ( ( pixel( 0 ) * ( 255 - alpha ) ) >> 8 );
                    pixel( 1 ) = ( ( pixel( 1 ) * ( 255 - alpha ) ) >> 8 );
                    pixel( 2 ) = ( ( pixel( 2 ) * ( 255 - alpha ) ) >> 8 );
                    pen_y--;
                }
            }
            for(y = 0; y < p_glyph->bitmap.rows; y++ )
            {
                for( x = 0; x < p_glyph->bitmap.width; x++ )
                {
                    pixel( 0 ) = ( ( pixel( 0 ) * ( 255 - alpha ) ) >> 8 ) +
                        ( 255 * alpha >> 8 );
                    pixel( 1 ) = ( ( pixel( 1 ) * ( 255 - alpha ) ) >> 8 ) +
                        ( 255 * alpha >> 8 );
                    pixel( 2 ) = ( ( pixel( 2 ) * ( 255 - alpha ) ) >> 8 ) +
                        ( 255 * alpha >> 8 );
                }
            }
#undef alpha
#undef pixel
        }
    }
}

/**
 * This function receives a string and creates a subpicture for it. It
 * also calculates the size needed for this string, and renders the
 * needed glyphs into memory. It is used as pf_add_string callback in
 * the vout method by this module
 */
static subpicture_t *AddText ( vout_thread_t *p_vout, char *psz_string,
                     text_style_t *p_style, int i_flags, int i_hmargin,
                     int i_vmargin, mtime_t i_start, mtime_t i_stop )
{
    subpicture_sys_t *p_string;
    int i, i_pen_y, i_pen_x, i_error, i_glyph_index, i_previous;
    subpicture_t *p_subpic;
    line_desc_t  *p_line,  *p_next;
    uint32_t *p_unicode_string, i_char;
    int i_string_length;
    iconv_t iconv_handle;

    FT_BBox line;
    FT_BBox glyph_size;
    FT_Vector result;
    FT_Glyph tmp_glyph;

    /* Sanity check */
    if ( !psz_string || !*psz_string )
    {
        return NULL;
    }

    result.x = 0;
    result.y = 0;
    line.xMin = 0;
    line.xMax = 0;
    line.yMin = 0;
    line.yMax = 0;

    p_line = 0;
    p_string = 0;
    p_subpic = 0;

    /* Create and initialize a subpicture */
    p_subpic = vout_CreateSubPicture( p_vout, MEMORY_SUBPICTURE );
    if ( p_subpic == NULL )
    {
        return NULL;
    }
    p_subpic->p_sys = 0;
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
    p_string = malloc( sizeof(subpicture_sys_t) );
    if ( p_string == NULL )
    {
        msg_Err( p_vout, "Out of memory" );
        goto error;
    }
    p_subpic->p_sys = p_string;
    p_string->i_flags = i_flags;
    p_string->i_x_margin = i_hmargin;
    p_string->i_y_margin = i_vmargin;
    p_string->p_lines = 0;
    p_string->psz_text = strdup( psz_string );

#if defined(HAVE_ICONV)
    p_unicode_string = malloc( ( strlen(psz_string) + 1 ) * sizeof(uint32_t) );
    if( p_unicode_string == NULL )
    {
        msg_Err( p_vout, "Out of memory" );
        goto error;
    }
#if defined(WORDS_BIGENDIAN)
    iconv_handle = iconv_open( "UCS-4BE", "UTF-8" );
#else
    iconv_handle = iconv_open( "UCS-4LE", "UTF-8" );
#endif
    if( iconv_handle == (iconv_t)-1 )
    {
        msg_Warn( p_vout, "Unable to do convertion" );
        goto error;
    }

    {
        char *p_in_buffer, *p_out_buffer;
        size_t i_in_bytes, i_out_bytes, i_out_bytes_left, i_ret;
        i_in_bytes = strlen( psz_string );
        i_out_bytes = i_in_bytes * sizeof( uint32_t );
        i_out_bytes_left = i_out_bytes;
        p_in_buffer = psz_string;
        p_out_buffer = (char *)p_unicode_string;
        i_ret = iconv( iconv_handle, &p_in_buffer, &i_in_bytes, &p_out_buffer, &i_out_bytes_left );
        if( i_in_bytes )
        {
            msg_Warn( p_vout, "Failed to convert string to unicode (%s), bytes left %d", strerror(errno), i_in_bytes );
            goto error;
        }
        *(uint32_t*)p_out_buffer = 0;
        i_string_length = ( i_out_bytes - i_out_bytes_left ) / sizeof(uint32_t);
    }

#if defined(HAVE_FRIBIDI)
    {
        uint32_t *p_fribidi_string;
        FriBidiCharType base_dir = FRIBIDI_TYPE_ON;
        p_fribidi_string = malloc( ( i_string_length + 1 ) * sizeof(uint32_t) );
        fribidi_log2vis( (FriBidiChar*)p_unicode_string, i_string_length,
                         &base_dir, (FriBidiChar*)p_fribidi_string, NULL, NULL,
                         NULL );
        free( p_unicode_string );
        p_unicode_string = p_fribidi_string;
        p_fribidi_string[ i_string_length ] = 0;
    }
#endif
#endif

    /* Calculate relative glyph positions and a bounding box for the
     * entire string */
    p_line = NewLine( psz_string );
    if( p_line == NULL )
    {
        msg_Err( p_vout, "Out of memory" );
        goto error;
    }
    p_string->p_lines = p_line;
    i_pen_x = 0;
    i_pen_y = 0;
    i_previous = 0;
    i = 0;

#define face p_vout->p_text_renderer_data->p_face
#define glyph face->glyph

    while( *p_unicode_string )
    {
        i_char = *p_unicode_string++;
        if ( i_char == '\r' ) /* ignore CR chars wherever they may be */
        {
            continue;
        }

        if ( i_char == '\n' )
        {
            p_next = NewLine( psz_string );
            if( p_next == NULL )
            {
                msg_Err( p_vout, "Out of memory" );
                goto error;
            }
            p_line->p_next = p_next;
            p_line->i_width = line.xMax;
            p_line->i_height = face->size->metrics.height >> 6;
            p_line->pp_glyphs[ i ] = NULL;
            p_line = p_next;
            result.x = __MAX( result.x, line.xMax );
            result.y += face->size->metrics.height >> 6;
            i_pen_x = 0;
            line.xMin = 0;
            line.xMax = 0;
            line.yMin = 0;
            line.yMax = 0;
            i_pen_y += face->size->metrics.height >> 6;
            msg_Dbg( p_vout, "Creating new line, i is %d", i );
            i = 0;
            continue;
        }

        i_glyph_index = FT_Get_Char_Index( face, i_char );
        if ( p_vout->p_text_renderer_data->i_use_kerning && i_glyph_index
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
        if ( i_error )
        {
            msg_Err( p_vout, "FT_Load_Glyph returned %d", i_error );
            goto error;
        }
        i_error = FT_Get_Glyph( glyph, &tmp_glyph );
        if ( i_error )
        {
            msg_Err( p_vout, "FT_Get_Glyph returned %d", i_error );
            goto error;
        }
        FT_Glyph_Get_CBox( tmp_glyph, ft_glyph_bbox_pixels, &glyph_size );
        i_error = FT_Glyph_To_Bitmap( &tmp_glyph, ft_render_mode_normal,
                                      NULL, 1 );
        if ( i_error ) continue;
        p_line->pp_glyphs[ i ] = (FT_BitmapGlyph)tmp_glyph;

        /* Do rest */
        line.xMax = p_line->p_glyph_pos[i].x + glyph_size.xMax - glyph_size.xMin;
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
    vout_DisplaySubPicture( p_vout, p_subpic );
    return p_subpic;

#undef face
#undef glyph

 error:
    FreeString( p_subpic );
    vout_DestroySubPicture( p_vout, p_subpic );
    return NULL;
}

static void FreeString( subpicture_t *p_subpic )
{
    unsigned int i;
    subpicture_sys_t *p_string = p_subpic->p_sys;
    line_desc_t *p_line, *p_next;

    if( p_subpic->p_sys == NULL ) return;

    for( p_line = p_string->p_lines; p_line != NULL; p_line = p_next )
    {
        p_next = p_line->p_next;
        for( i = 0; p_line->pp_glyphs[ i ] != NULL; i++ )
        {
            FT_Done_Glyph( (FT_Glyph)p_line->pp_glyphs[ i ] );
        }
        free( p_line->pp_glyphs );
        free( p_line->p_glyph_pos );
        free( p_line );
    }

    free( p_string->psz_text );
    free( p_string );
}

#if !defined( HAVE_ICONV )
/* convert one or more utf8 bytes into a unicode character */
static int GetUnicodeCharFromUTF8( byte_t **ppsz_utf8_string )
{
    int i_remaining_bytes, i_char = 0;
    if( ( **ppsz_utf8_string & 0xFC ) == 0xFC )
    {
        i_char = **ppsz_utf8_string & 1;
        i_remaining_bytes = 5;
    }
    else if( ( **ppsz_utf8_string & 0xF8 ) == 0xF8 )
    {
        i_char = **ppsz_utf8_string & 3;
        i_remaining_bytes = 4;
    }
    else if( ( **ppsz_utf8_string & 0xF0 ) == 0xF0 )
    {
        i_char = **ppsz_utf8_string & 7;
        i_remaining_bytes = 3;
    }
    else if( ( **ppsz_utf8_string & 0xE0 ) == 0xE0 )
    {
        i_char = **ppsz_utf8_string & 15;
        i_remaining_bytes = 2;
    }
    else if( ( **ppsz_utf8_string & 0xC0 ) == 0xC0 )
    {
        i_char = **ppsz_utf8_string & 31;
        i_remaining_bytes = 1;
    }
    else
    {
        i_char = **ppsz_utf8_string;
        i_remaining_bytes = 0;
    }
    while( i_remaining_bytes )
    {
        (*ppsz_utf8_string)++;
        i_remaining_bytes--;
        i_char = ( i_char << 6 ) + ( **ppsz_utf8_string & 0x3F );
    }
    (*ppsz_utf8_string)++;
    return i_char;
}
#endif

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
