/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002, 2003 VideoLAN
 * $Id: freetype.c,v 1.2 2003/07/19 14:41:30 sigmunau Exp $
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

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <osd.h>
#include <math.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define FT_RENDER_MODE_NORMAL 0 /* Why do we have to do that ? */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static void Render    ( vout_thread_t *, picture_t *, 
		        const subpicture_t * );
static int  AddText   ( vout_thread_t *, byte_t *, text_style_t *, int, 
			int, int, mtime_t, mtime_t );
static int GetUnicodeCharFromUTF8( byte_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FONT_TEXT N_("Font")
#define FONT_LONGTEXT N_("Filename of Font")
#define FONTSIZE_TEXT N_("Font size")
#define FONTSIZE_LONGTEXT N_("The size of the fonts used by the osd module" )

vlc_module_begin();
    add_category_hint( N_("Fonts"), NULL, VLC_FALSE );
    add_file( "freetype-font", "", NULL, FONT_TEXT, FONT_LONGTEXT, VLC_FALSE );
    add_integer( "freetype-fontsize", 16, NULL, FONTSIZE_TEXT, FONTSIZE_LONGTEXT, VLC_FALSE );
    set_description( _("freetype2 font renderer") );
    set_capability( "text renderer", 100 );
    add_shortcut( "text" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/**
 Describes a string to be displayed on the video, or a linked list of
 such
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
    /** NULL-terminated list of glyphs making the string */
    FT_Glyph      *pp_glyphs;
    /** list of relative positions for the glyphs */
    FT_Vector     *p_glyph_pos;
};

/*****************************************************************************
 * vout_sys_t: osd_text local data
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the osd-text specific properties of an output thread.
 *****************************************************************************/
struct text_renderer_sys_t
{
    FT_Library     p_library;   /* handle to library     */
    FT_Face        p_face;      /* handle to face object */
    vlc_mutex_t   *lock;
    vlc_bool_t     i_use_kerning;
    uint8_t        pi_gamma[256];
};
/* more prototypes */
//static void ComputeBoundingBox( subpicture_sys_t * );
static void FreeString( subpicture_t * );

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
    double gamma_inv = 1.0f / gamma_value;
    
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
	//msg_Dbg( p_vout, "%d", p_vout->p_text_renderer_data->pi_gamma[i]);
    }

    /* Look what method was requested */
    psz_fontfile = config_GetPsz( p_vout, "freetype-font" );
    i_error = FT_Init_FreeType( &p_vout->p_text_renderer_data->p_library );
    if( i_error )
    {
        msg_Err( p_vout, "couldn't initialize freetype" );
        free( p_vout->p_text_renderer_data );
        return VLC_EGENERIC;
    }
    i_error = FT_New_Face( p_vout->p_text_renderer_data->p_library,
			   psz_fontfile, 0,
                           &p_vout->p_text_renderer_data->p_face );
    if( i_error == FT_Err_Unknown_File_Format )
    {
        msg_Err( p_vout, "file %s have unknown format", psz_fontfile );
	FT_Done_FreeType( p_vout->p_text_renderer_data->p_library );
        free( p_vout->p_text_renderer_data );
        return VLC_EGENERIC;
    }
    else if( i_error )
    {
        msg_Err( p_vout, "failed to load font file" );
	FT_Done_FreeType( p_vout->p_text_renderer_data->p_library );
        free( p_vout->p_text_renderer_data );
        return VLC_EGENERIC;
    }
    i_error = FT_Select_Charmap( p_vout->p_text_renderer_data->p_face,
				 FT_ENCODING_UNICODE );
    if ( i_error )
    {
	msg_Err( p_vout, "Font has no unicode translation table" );
	FT_Done_Face( p_vout->p_text_renderer_data->p_face );
	FT_Done_FreeType( p_vout->p_text_renderer_data->p_library );
	free( p_vout->p_text_renderer_data );
	return VLC_EGENERIC;
    }
    
    p_vout->p_text_renderer_data->i_use_kerning = FT_HAS_KERNING(p_vout->p_text_renderer_data->p_face);
    
    i_error = FT_Set_Pixel_Sizes( p_vout->p_text_renderer_data->p_face, 0, 
				  config_GetInt( p_vout, "freetype-fontsize" ) );
    if( i_error )
    {
        msg_Err( p_vout, "couldn't set font size to %d",
                 config_GetInt( p_vout, "osd-fontsize" ) );
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
    subpicture_sys_t *p_string = p_subpic->p_sys;
    int i_plane, i_error,x,y,pen_x, pen_y;
    unsigned int i;
    
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in;
        int i_pitch = p_pic->p[i_plane].i_pitch;

        p_in = p_pic->p[i_plane].p_pixels;

        if ( i_plane == 0 )
	{
	    if ( p_string->i_flags & OSD_ALIGN_BOTTOM )
	    {
		pen_y = p_pic->p[i_plane].i_lines - p_string->i_height -
		    p_string->i_y_margin;
	    }
	    else
	    {
		pen_y = p_string->i_y_margin;
	    }
	    if ( p_string->i_flags & OSD_ALIGN_RIGHT )
	    {
		pen_x = i_pitch - p_string->i_width
		    - p_string->i_x_margin;
	    }
	    else
	    {
		pen_x = p_string->i_x_margin;
	    }

	    for( i = 0; p_string->pp_glyphs[i] != NULL; i++ )
	    {
		if( p_string->pp_glyphs[i] )
		{
		    FT_Glyph p_glyph = p_string->pp_glyphs[i];
		    FT_BitmapGlyph p_image;
		    i_error = FT_Glyph_To_Bitmap( &p_glyph,
						  FT_RENDER_MODE_NORMAL,
						  &p_string->p_glyph_pos[i],
						  0 );
		    if ( i_error ) continue;
		    p_image = (FT_BitmapGlyph)p_glyph;
#define alpha p_vout->p_text_renderer_data->pi_gamma[p_image->bitmap.buffer[x+ y*p_image->bitmap.width]]
#define pixel p_in[(p_string->p_glyph_pos[i].y + pen_y + y - p_image->top)*i_pitch+x+pen_x+p_string->p_glyph_pos[i].x+p_image->left]
		    for(y = 0; y < p_image->bitmap.rows; y++ )
		    {
			for( x = 0; x < p_image->bitmap.width; x++ )
			{
			    //                                pixel = alpha;
			    //                                pixel = (pixel^alpha)^pixel;
			    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 ) +
                                ( 255 * alpha >> 8 );
			}
		    }
		    FT_Done_Glyph( p_glyph );
		}
	    }
	}
    }
}

/**
 * This function receives a string and creates a subpicture for it. It
 * also calculates the size needed for this string, and renders the
 * needed glyphs into memory. It is used as pf_add_string callback in
 * the vout method by this module
 */
static int AddText ( vout_thread_t *p_vout, byte_t *psz_string,
                     text_style_t *p_style, int i_flags, int i_hmargin, 
		     int i_vmargin, mtime_t i_start, mtime_t i_stop )
{
    subpicture_sys_t *p_string;
    int i, i_pen_y, i_pen_x, i_error, i_glyph_index, i_previous, i_char;
    subpicture_t *p_subpic;
    FT_BBox line;
    FT_BBox glyph_size;
    FT_Vector result;

    result.x = 0;
    result.y = 0;
    line.xMin = 0;
    line.xMax = 0;
    line.yMin = 0;
    line.yMax = 0;

    /* Create and initialize a subpicture */
    p_subpic = vout_CreateSubPicture( p_vout, MEMORY_SUBPICTURE );
    if ( p_subpic == NULL )
    {
	return VLC_EGENERIC;
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
    p_string = malloc( sizeof(subpicture_sys_t) );
    if ( p_string == NULL )
    {
	vout_DestroySubPicture( p_vout, p_subpic );
	return VLC_ENOMEM;
    }
    p_subpic->p_sys = p_string;
    p_string->i_flags = i_flags;
    p_string->i_x_margin = i_hmargin;
    p_string->i_y_margin = i_vmargin;

    p_string->psz_text = strdup( psz_string );
    p_string->pp_glyphs = malloc( sizeof(FT_GlyphSlot)
                                  * ( strlen( p_string->psz_text ) + 1 ) );
    if( p_string->pp_glyphs == NULL )
    {
        msg_Err( p_vout, "Out of memory" );
        return VLC_ENOMEM;
    }
    p_string->p_glyph_pos = malloc( sizeof( FT_Vector )
                                  * strlen( p_string->psz_text ) );
    if( p_string->p_glyph_pos == NULL )
    {
        msg_Err( p_vout, "Out of memory" );
        return VLC_ENOMEM;
    }

    /* Calculate relative glyph positions and a bounding box for the
     * entire string */
    i_pen_x = 0;
    i_pen_y = 0;
    i_previous = 0;
    i = 0;
    while( *psz_string )
    {
        i_char = GetUnicodeCharFromUTF8( &psz_string );
#define face p_vout->p_text_renderer_data->p_face
#define glyph face->glyph
        if ( i_char == '\n' )
        {
            i_pen_x = 0;
            result.x = __MAX( result.x, line.xMax );
            result.y += face->height >> 6;
            line.xMin = 0;
            line.xMax = 0;
            line.yMin = 0;
            line.yMax = 0;
            i_pen_y += face->height >> 6;
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
        p_string->p_glyph_pos[ i ].x = i_pen_x;
        p_string->p_glyph_pos[ i ].y = i_pen_y;
        i_error = FT_Load_Glyph( face, i_glyph_index, FT_LOAD_DEFAULT );
        if ( i_error )
        {
            msg_Err( p_vout, "FT_Load_Glyph returned %d", i_error );
            return VLC_EGENERIC;
        }
        i_error = FT_Get_Glyph( glyph, &p_string->pp_glyphs[ i ] );
        if ( i_error )
        {
            msg_Err( p_vout, "FT_Get_Glyph returned %d", i_error );
            return VLC_EGENERIC;
        }
        FT_Glyph_Get_CBox( p_string->pp_glyphs[i],
                           ft_glyph_bbox_pixels, &glyph_size );
        /* Do rest */
        line.xMax = p_string->p_glyph_pos[i].x + glyph_size.xMax - glyph_size.xMin;
        line.yMax = __MAX( line.yMax, glyph_size.yMax );
        line.yMin = __MIN( line.yMin, glyph_size.yMin );
        
        i_previous = i_glyph_index;
        i_pen_x += glyph->advance.x >> 6;
	i++;
    }
    p_string->pp_glyphs[i] = NULL;
    result.x = __MAX( result.x, line.xMax );
    result.y += line.yMax - line.yMin;
    p_string->i_height = result.y;
    p_string->i_width = result.x;
    msg_Dbg( p_vout, "string height is %d, width is %d", p_string->i_height, p_string->i_width );
    msg_Dbg( p_vout, "adding string \"%s\" at (%d,%d) start_date "I64Fd
             " end_date" I64Fd, p_string->psz_text, p_string->i_x_margin,
             p_string->i_y_margin, i_start, i_stop );
    vout_DisplaySubPicture( p_vout, p_subpic );
    return VLC_SUCCESS;
}

static void FreeString( subpicture_t *p_subpic )
{
    unsigned int i;
    subpicture_sys_t *p_string = p_subpic->p_sys;
    for ( i = 0; p_string->pp_glyphs[ i ] != NULL; i++ )
    {
	FT_Done_Glyph( p_string->pp_glyphs[ i ] );
    }
    free( p_string->psz_text );
    free( p_string->p_glyph_pos );
    free( p_string->pp_glyphs );
    free( p_string );
}

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
