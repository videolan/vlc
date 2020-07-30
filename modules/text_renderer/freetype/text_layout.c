/*****************************************************************************
 * text_layout.c : Text shaping and layout
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 * Authors: Salah-Eddin Shaban <salshaaban@gmail.com>
 *          Laurent Aimar <fenrir@videolan.org>
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Naohiro Koriyama <nkoriyama@gmail.com>
 *          David Fuhrmann <dfuhrmann@videolan.org>
 *          Erwan Tulou <erwan10@videolan.org>
 *          Devin Heitmueller <dheitmueller@kernellabs.com>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/** \ingroup freetype
 * @{
 * \file
 * Text shaping and layout
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_text_style.h>

/* Freetype */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H
#include FT_SYNTHESIS_H

/* RTL */
#if defined(HAVE_FRIBIDI)
# define FRIBIDI_NO_DEPRECATED 1
# include <fribidi.h>
#endif

/* Complex Scripts */
#if defined(HAVE_HARFBUZZ)
# include <hb.h>
# include <hb-ft.h>
#endif

#include "freetype.h"
#include "text_layout.h"
#include "platform_fonts.h"

#include <stdlib.h>

/* Win32 */
#ifdef _WIN32
# undef HAVE_FONTCONFIG
# define HAVE_FONT_FALLBACK
#endif

/* FontConfig */
#ifdef HAVE_FONTCONFIG
# define HAVE_FONT_FALLBACK
#endif

/* Android */
#ifdef __ANDROID__
# define HAVE_FONT_FALLBACK
#endif

/* Darwin */
#ifdef __APPLE__
# define HAVE_FONT_FALLBACK
#endif

#ifndef HAVE_FONT_FALLBACK
# warning YOU ARE MISSING FONTS FALLBACK. TEXT WILL BE INCORRECT
#endif

/**
 * Within a paragraph, run_desc_t represents a run of characters
 * having the same font face, size, and style, Unicode script
 * and text direction
 */
typedef struct run_desc_t
{
    int                         i_start_offset;
    int                         i_end_offset;
    vlc_face_id_t              *p_faceid;
    const text_style_t         *p_style;

#ifdef HAVE_HARFBUZZ
    hb_script_t                 script;
    hb_direction_t              direction;
    hb_buffer_t                *p_buffer;
#endif

} run_desc_t;

/**
 * Glyph bitmaps. Advance and offset are 26.6 values
 */
typedef struct glyph_bitmaps_t
{
    vlc_ftcache_glyph_t cglyph;
    vlc_ftcache_custom_glyph_t coutline;
    FT_Glyph p_shadow;
    FT_BBox  glyph_bbox;
    FT_BBox  outline_bbox;
    FT_BBox  shadow_bbox;
    int      i_x_offset;
    int      i_y_offset;
    int      i_x_advance;
    int      i_y_advance;
} glyph_bitmaps_t;

typedef struct paragraph_t
{
    uni_char_t          *p_code_points;    /**< Unicode code points */
    int                 *pi_glyph_indices; /**< Glyph index values within the run's font face */
    text_style_t       **pp_styles;
    ruby_block_t       **pp_ruby;
    vlc_face_id_t      **pp_facesidx;      /**< Used to determine run boundaries when performing font fallback */
    int                 *pi_run_ids;       /**< The run to which each glyph belongs */
    glyph_bitmaps_t     *p_glyph_bitmaps;
    int                  i_size;
    run_desc_t          *p_runs;
    int                  i_runs_count;
    int                  i_runs_size;

#ifdef HAVE_HARFBUZZ
    hb_script_t         *p_scripts;
#endif

#ifdef HAVE_FRIBIDI
    FriBidiCharType     *p_types;
#if FRIBIDI_MAJOR_VERSION >= 1
    FriBidiBracketType  *p_btypes;
#endif
    FriBidiLevel        *p_levels;
    FriBidiStrIndex     *pi_reordered_indices;
    FriBidiParType       paragraph_type;
#endif

} paragraph_t;

static void FreeLine( line_desc_t *p_line )
{
    for( int i = 0; i < p_line->i_character_count; i++ )
    {
        line_character_t *ch = &p_line->p_character[i];
        FT_Done_Glyph( (FT_Glyph)ch->p_glyph );
        if( ch->p_outline )
            FT_Done_Glyph( (FT_Glyph)ch->p_outline );
        if( ch->p_shadow && ch->p_shadow != ch->p_glyph )
            FT_Done_Glyph( (FT_Glyph)ch->p_shadow );
    }

//    if( p_line->p_ruby )
//        FreeLine( p_line->p_ruby );

    free( p_line->p_character );
    free( p_line );
}

void FreeLines( line_desc_t *p_lines )
{
    for( line_desc_t *p_line = p_lines; p_line != NULL; )
    {
        line_desc_t *p_next = p_line->p_next;
        FreeLine( p_line );
        p_line = p_next;
    }
}

line_desc_t *NewLine( int i_count )
{
    line_desc_t *p_line = malloc( sizeof(*p_line) );

    if( !p_line )
        return NULL;

    p_line->p_next = NULL;
    p_line->i_width = 0;
    p_line->i_height = 0;
    p_line->origin.x = 0;
    p_line->origin.y = 0;
    p_line->i_character_count = 0;
    p_line->i_first_visible_char_index = -1;
    p_line->i_last_visible_char_index = -2;

    BBoxInit( &p_line->bbox );

    p_line->p_character = calloc( i_count, sizeof(*p_line->p_character) );
    if( !p_line->p_character )
    {
        free( p_line );
        return NULL;
    }
    return p_line;
}

static void ShiftChar( line_character_t *c, int x, int y )
{
    c->bbox.yMin += y;
    c->bbox.yMax += y;
    c->bbox.xMin += x;
    c->bbox.xMax += x;
}

static void ShiftLine( line_desc_t *p_line, int x, int y )
{
    for( int i=0; i<p_line->i_character_count; i++ )
        ShiftChar( &p_line->p_character[i], x, y );
    p_line->origin.y += y;
    p_line->origin.x += x;
    p_line->bbox.yMin += y;
    p_line->bbox.yMax += y;
    p_line->bbox.xMin += x;
    p_line->bbox.xMax += x;
}

static void MoveLineTo( line_desc_t *p_line, int x, int y )
{
    ShiftLine( p_line, x - p_line->bbox.xMin,
                       y - p_line->bbox.yMax );
}

static void IndentCharsFrom( line_desc_t *p_line, int i_start, int i_count, int w, int h )
{
    for( int i=0; i<i_count; i++ )
    {
        line_character_t *p_ch = &p_line->p_character[i_start + i];
        ShiftChar( p_ch, w, h );
        BBoxEnlarge( &p_line->bbox, &p_ch->bbox );
    }
}

static int RubyBaseAdvance( const line_desc_t *p_line, int i_start, int *pi_count )
{
    int i_total = 0;
    *pi_count = 0;
    for( int i = i_start; i < p_line->i_character_count; i++ )
    {
        if( p_line->p_character[i].p_ruby != p_line->p_character[i_start].p_ruby )
            break;
        (*pi_count)++;
        i_total += (p_line->p_character[i].bbox.xMax - p_line->p_character[i].bbox.xMin);
    }
    return i_total;
}

static void FixGlyph( FT_Glyph glyph, FT_BBox *p_bbox,
                      FT_Pos i_x_advance, FT_Pos i_y_advance,
                      const FT_Vector *p_pen )
{
    FT_BitmapGlyph glyph_bmp = (FT_BitmapGlyph)glyph;
    if( p_bbox->xMin >= p_bbox->xMax )
    {
        p_bbox->xMin = FT_CEIL(p_pen->x);
        p_bbox->xMax = FT_CEIL(p_pen->x + i_x_advance);
        glyph_bmp->left = p_bbox->xMin;
    }
    if( p_bbox->yMin >= p_bbox->yMax )
    {
        p_bbox->yMax = FT_CEIL(p_pen->y);
        p_bbox->yMin = FT_CEIL(p_pen->y + i_y_advance);
        glyph_bmp->top  = p_bbox->yMax;
    }
}

static void FreeParagraph( paragraph_t *p_paragraph )
{
    free( p_paragraph->p_runs );
    free( p_paragraph->pi_glyph_indices );
    free( p_paragraph->p_glyph_bitmaps );
    free( p_paragraph->pi_run_ids );
    free( p_paragraph->pp_facesidx );
    free( p_paragraph->pp_ruby );
    free( p_paragraph->pp_styles );
    free( p_paragraph->p_code_points );

#ifdef HAVE_HARFBUZZ
    free( p_paragraph->p_scripts );
#endif

#ifdef HAVE_FRIBIDI
    free( p_paragraph->pi_reordered_indices );
    free( p_paragraph->p_types );
#if FRIBIDI_MAJOR_VERSION >= 1
    free( p_paragraph->p_btypes );
#endif
    free( p_paragraph->p_levels );
#endif

    free( p_paragraph );
}

static paragraph_t *NewParagraph( filter_t *p_filter,
                                  int i_size,
                                  const uni_char_t *p_code_points,
                                  text_style_t **pp_styles,
                                  ruby_block_t **pp_ruby,
                                  int i_runs_size )
{
    paragraph_t *p_paragraph = calloc( 1, sizeof( paragraph_t ) );
    if( !p_paragraph )
        return 0;

    p_paragraph->i_size = i_size;
    p_paragraph->p_code_points =
            vlc_alloc( i_size, sizeof( *p_paragraph->p_code_points ) );
    p_paragraph->pi_glyph_indices =
            vlc_alloc( i_size, sizeof( *p_paragraph->pi_glyph_indices ) );
    p_paragraph->pp_styles =
            vlc_alloc( i_size, sizeof( *p_paragraph->pp_styles ) );
    p_paragraph->pp_facesidx =
            calloc( i_size, sizeof( *p_paragraph->pp_facesidx ) );
    p_paragraph->pi_run_ids =
            calloc( i_size, sizeof( *p_paragraph->pi_run_ids ) );
    p_paragraph->p_glyph_bitmaps =
            calloc( i_size, sizeof( *p_paragraph->p_glyph_bitmaps ) );
    if( pp_ruby )
        p_paragraph->pp_ruby = calloc( i_size, sizeof( *p_paragraph->pp_ruby ) );

    p_paragraph->p_runs = calloc( i_runs_size, sizeof( run_desc_t ) );
    p_paragraph->i_runs_size = i_runs_size;
    p_paragraph->i_runs_count = 0;

    if( !p_paragraph->p_code_points || !p_paragraph->pi_glyph_indices
     || !p_paragraph->pp_styles || !p_paragraph->pp_facesidx
     || !p_paragraph->pi_run_ids|| !p_paragraph->p_glyph_bitmaps
     || !p_paragraph->p_runs )
        goto error;

    if( p_code_points )
        memcpy( p_paragraph->p_code_points, p_code_points,
                i_size * sizeof( *p_code_points ) );
    if( pp_styles )
        memcpy( p_paragraph->pp_styles, pp_styles,
                i_size * sizeof( *pp_styles ) );
    if( p_paragraph->pp_ruby )
        memcpy( p_paragraph->pp_ruby, pp_ruby, i_size * sizeof( *pp_ruby ) );

#ifdef HAVE_HARFBUZZ
    p_paragraph->p_scripts = vlc_alloc( i_size, sizeof( *p_paragraph->p_scripts ) );
    if( !p_paragraph->p_scripts )
        goto error;
#endif

#ifdef HAVE_FRIBIDI
    p_paragraph->p_levels = vlc_alloc( i_size, sizeof( *p_paragraph->p_levels ) );
    p_paragraph->p_types = vlc_alloc( i_size, sizeof( *p_paragraph->p_types ) );
#if FRIBIDI_MAJOR_VERSION >= 1
    p_paragraph->p_btypes = vlc_alloc( i_size, sizeof( *p_paragraph->p_btypes ) );
#endif
    p_paragraph->pi_reordered_indices =
            vlc_alloc( i_size, sizeof( *p_paragraph->pi_reordered_indices ) );

    if( !p_paragraph->p_levels || !p_paragraph->p_types
     || !p_paragraph->pi_reordered_indices )
        goto error;

    for( int i=0; i<i_size; i++ )
        p_paragraph->pi_reordered_indices[i] = i;

    int i_direction = var_InheritInteger( p_filter, "freetype-text-direction" );
    if( i_direction == 0 )
        p_paragraph->paragraph_type = FRIBIDI_PAR_LTR;
    else if( i_direction == 1 )
        p_paragraph->paragraph_type = FRIBIDI_PAR_RTL;
    else
        p_paragraph->paragraph_type = FRIBIDI_PAR_ON;
#endif

    return p_paragraph;

error:
    FreeParagraph( p_paragraph );
    return 0;
}

#ifdef HAVE_FRIBIDI
static int AnalyzeParagraph( paragraph_t *p_paragraph )
{
    int i_max;
    fribidi_get_bidi_types(  p_paragraph->p_code_points,
                             p_paragraph->i_size,
                             p_paragraph->p_types );
#if FRIBIDI_MAJOR_VERSION >= 1
    fribidi_get_bracket_types( p_paragraph->p_code_points,
                               p_paragraph->i_size,
                               p_paragraph->p_types,
                               p_paragraph->p_btypes );
    i_max = fribidi_get_par_embedding_levels_ex(
                                      p_paragraph->p_types,
                                      p_paragraph->p_btypes,
                                      p_paragraph->i_size,
                                      &p_paragraph->paragraph_type,
                                      p_paragraph->p_levels );
#else
    i_max = fribidi_get_par_embedding_levels(
                                      p_paragraph->p_types,
                                      p_paragraph->i_size,
                                      &p_paragraph->paragraph_type,
                                      p_paragraph->p_levels );
#endif
    if( i_max == 0 )
        return VLC_EGENERIC;

#ifdef HAVE_HARFBUZZ
    hb_unicode_funcs_t *p_funcs =
        hb_unicode_funcs_create( hb_unicode_funcs_get_default() );
    for( int i = 0; i < p_paragraph->i_size; ++i )
        p_paragraph->p_scripts[ i ] =
            hb_unicode_script( p_funcs, p_paragraph->p_code_points[ i ] );
    hb_unicode_funcs_destroy( p_funcs );

    hb_script_t i_last_script;
    int i_last_script_index = -1;
    int i_last_set_index = -1;

    /*
     * For shaping to work, characters that are assigned HB_SCRIPT_COMMON or
     * HB_SCRIPT_INHERITED should be resolved to the last encountered valid
     * script value, if any, and to the first one following them otherwise
     */
    for( int i = 0; i < p_paragraph->i_size; ++i )
    {
        if( p_paragraph->p_scripts[ i ] == HB_SCRIPT_COMMON
            || p_paragraph->p_scripts[ i ] == HB_SCRIPT_INHERITED)
        {
            if( i_last_script_index != -1)
            {
                p_paragraph->p_scripts[ i ] = i_last_script;
                i_last_set_index = i;
            }
        }
        else
        {
            for( int j = i_last_set_index + 1; j < i; ++j )
                p_paragraph->p_scripts[ j ] = p_paragraph->p_scripts[ i ];

            i_last_script = p_paragraph->p_scripts[ i ];
            i_last_script_index = i;
            i_last_set_index = i;
        }
    }
#endif //HAVE_HARFBUZZ

    return VLC_SUCCESS;
}
#endif //HAVE_FRIBIDI

static int AddRun( filter_t *p_filter,
                   paragraph_t *p_paragraph,
                   int i_start_offset,
                   int i_end_offset,
                   vlc_face_id_t *p_faceid,
                   const text_style_t *p_style )
{
    if( i_start_offset >= i_end_offset
     || i_start_offset < 0 || i_start_offset >= p_paragraph->i_size
     || i_end_offset <= 0  || i_end_offset > p_paragraph->i_size )
    {
        msg_Err( p_filter,
                 "AddRun() invalid parameters. Paragraph size: %d, "
                 "Start offset: %d, End offset: %d",
                 p_paragraph->i_size, i_start_offset, i_end_offset );
        return VLC_EGENERIC;
    }

    if( p_paragraph->i_runs_count == p_paragraph->i_runs_size )
    {
        run_desc_t *p_new_runs =
            realloc( p_paragraph->p_runs,
                     p_paragraph->i_runs_size * 2 * sizeof( *p_new_runs ) );
        if( !p_new_runs )
            return VLC_ENOMEM;

        memset( p_new_runs + p_paragraph->i_runs_size , 0,
                p_paragraph->i_runs_size * sizeof( *p_new_runs ) );

        p_paragraph->p_runs = p_new_runs;
        p_paragraph->i_runs_size *= 2;
    }

    int i_run_id = p_paragraph->i_runs_count;
    run_desc_t *p_run = p_paragraph->p_runs + p_paragraph->i_runs_count++;
    p_run->i_start_offset = i_start_offset;
    p_run->i_end_offset = i_end_offset;
    p_run->p_faceid = p_faceid;

    if( p_style )
        p_run->p_style = p_style;
    else
        p_run->p_style = p_paragraph->pp_styles[ i_start_offset ];

#ifdef HAVE_HARFBUZZ
    p_run->script = p_paragraph->p_scripts[ i_start_offset ];
    p_run->direction = p_paragraph->p_levels[ i_start_offset ] & 1 ?
            HB_DIRECTION_RTL : HB_DIRECTION_LTR;
#endif

    for( int i = i_start_offset; i < i_end_offset; ++i )
        p_paragraph->pi_run_ids[ i ] = i_run_id;

    return VLC_SUCCESS;
}

/**
 * Add a run with font fallback, possibly breaking the run further
 * into runs of glyphs that end up having the same font face.
 */
static int AddRunWithFallback( filter_t *p_filter, paragraph_t *p_paragraph,
                               int i_start_offset, int i_end_offset )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( i_start_offset >= i_end_offset
     || i_start_offset < 0 || i_start_offset >= p_paragraph->i_size
     || i_end_offset <= 0  || i_end_offset > p_paragraph->i_size )
    {
        msg_Err( p_filter,
                 "AddRunWithFallback() invalid parameters. Paragraph size: %d, "
                 "Start offset: %d, End offset: %d",
                 p_paragraph->i_size, i_start_offset, i_end_offset );
        return VLC_EGENERIC;
    }

    const text_style_t *p_style = p_paragraph->pp_styles[ i_start_offset ];

    /* Maximum number of faces to try for each run */
    #define MAX_FACES 5
    vlc_face_id_t *pp_facesidx[ MAX_FACES ] = {0};
    vlc_face_id_t *p_faceidx = NULL;

    for( int i = i_start_offset; i < i_end_offset; ++i )
    {
        int i_index = 0;
        int i_glyph_index = 0;

#ifdef HAVE_FRIBIDI
        /*
         * For white space, punctuation and neutral characters, try to use
         * the font of the previous character, if any. See #20466.
         */
        if( p_faceidx &&
            ( p_paragraph->p_types[ i ] == FRIBIDI_TYPE_WS
           || p_paragraph->p_types[ i ] == FRIBIDI_TYPE_CS
           || p_paragraph->p_types[ i ] == FRIBIDI_TYPE_ON ) )
        {
            i_glyph_index = vlc_ftcache_LookupCMapIndex( p_sys->ftcache, p_faceidx,
                                                         p_paragraph->p_code_points[ i ] );
            if( i_glyph_index )
            {
                p_paragraph->pp_facesidx[ i ] = p_faceidx;
                continue;
            }
        }
#endif

        do {
            p_faceidx = pp_facesidx[ i_index ];
            if( !p_faceidx )
                p_faceidx = pp_facesidx[ i_index ] =
                     SelectAndLoadFace( p_filter, p_style, p_paragraph->p_code_points[ i ] );
            if( !p_faceidx )
                continue;
            i_glyph_index = vlc_ftcache_LookupCMapIndex( p_sys->ftcache, p_faceidx,
                                             p_paragraph->p_code_points[ i ] );
            if( i_glyph_index )
                p_paragraph->pp_facesidx[ i ] = p_faceidx;

        } while( i_glyph_index == 0 && ++i_index < MAX_FACES );
    }

    int i_run_start = i_start_offset;
    for( int i = i_start_offset; i <= i_end_offset; ++i )
    {
        if( i == i_end_offset
         || p_paragraph->pp_facesidx[ i_run_start ] != p_paragraph->pp_facesidx[ i ] )
        {
            if( p_paragraph->pp_facesidx[ i_run_start ] &&
                AddRun( p_filter, p_paragraph, i_run_start, i,
                        p_paragraph->pp_facesidx[ i_run_start ], NULL ) )
                return VLC_EGENERIC;

            i_run_start = i;
        }
    }

    return VLC_SUCCESS;
}

static bool FaceStyleEquals( filter_t *p_filter, const text_style_t *p_style1,
                             const text_style_t *p_style2 )
{
    if( !p_style1 || !p_style2 )
        return false;
    if( p_style1 == p_style2 )
        return true;

    const int i_style_mask = STYLE_BOLD | STYLE_ITALIC | STYLE_HALFWIDTH | STYLE_DOUBLEWIDTH;

    const char *psz_fontname1 = p_style1->i_style_flags & STYLE_MONOSPACED
                              ? p_style1->psz_monofontname : p_style1->psz_fontname;

    const char *psz_fontname2 = p_style2->i_style_flags & STYLE_MONOSPACED
                              ? p_style2->psz_monofontname : p_style2->psz_fontname;

    const int i_size1 = ConvertToLiveSize( p_filter, p_style1 );
    const int i_size2 = ConvertToLiveSize( p_filter, p_style2 );

    return (p_style1->i_style_flags & i_style_mask) == (p_style2->i_style_flags & i_style_mask)
         && i_size1 == i_size2
         && !strcasecmp( psz_fontname1, psz_fontname2 );
}

/**
 * Segment a paragraph into runs
 */
static int ItemizeParagraph( filter_t *p_filter, paragraph_t *p_paragraph )
{
    if( p_paragraph->i_size <= 0 )
    {
        msg_Err( p_filter,
                 "ItemizeParagraph() invalid parameters. Paragraph size: %d",
                 p_paragraph->i_size );
        return VLC_EGENERIC;
    }

    int i_last_run_start = 0;
    const text_style_t *p_last_style = p_paragraph->pp_styles[ 0 ];

#ifdef HAVE_HARFBUZZ
    hb_script_t last_script = p_paragraph->p_scripts[ 0 ];
    FriBidiLevel last_level = p_paragraph->p_levels[ 0 ];
#endif

    for( int i = 0; i <= p_paragraph->i_size; ++i )
    {
        if( i == p_paragraph->i_size
#ifdef HAVE_HARFBUZZ
            || last_script != p_paragraph->p_scripts[ i ]
            || last_level != p_paragraph->p_levels[ i ]
#endif
            || !FaceStyleEquals( p_filter, p_last_style, p_paragraph->pp_styles[ i ] ) )
        {
            int i_ret = AddRunWithFallback( p_filter, p_paragraph, i_last_run_start, i );
            if( i_ret )
                return i_ret;

            if( i < p_paragraph->i_size )
            {
                i_last_run_start = i;
                p_last_style = p_paragraph->pp_styles[ i ];
#ifdef HAVE_HARFBUZZ
                last_script = p_paragraph->p_scripts[ i ];
                last_level = p_paragraph->p_levels[ i ];
#endif
            }
        }
    }
    return VLC_SUCCESS;
}

#ifdef HAVE_HARFBUZZ
/**
 * Shape an itemized paragraph using HarfBuzz.
 * This is where the glyphs of complex scripts get their positions
 * (offsets and advance values) and final forms.
 * Glyph substitutions of base glyphs and diacritics may take place,
 * so the paragraph size may change.
 */
static int ShapeParagraphHarfBuzz( filter_t *p_filter,
                                   paragraph_t **p_old_paragraph )
{
    paragraph_t *p_paragraph = *p_old_paragraph;
    paragraph_t *p_new_paragraph = 0;
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_total_glyphs = 0;
    int i_ret = VLC_EGENERIC;

    if( p_paragraph->i_size <= 0 || p_paragraph->i_runs_count <= 0 )
    {
        msg_Err( p_filter, "ShapeParagraphHarfBuzz() invalid parameters. "
                 "Paragraph size: %d. Runs count %d",
                 p_paragraph->i_size, p_paragraph->i_runs_count );
        return VLC_EGENERIC;
    }

    for( int i = 0; i < p_paragraph->i_runs_count; ++i )
    {
        run_desc_t *p_run = p_paragraph->p_runs + i;

        /*
         * With HarfBuzz and no font fallback, this is where font faces
         * are loaded. In the other two paths (shaping with FriBidi or no
         * shaping at all), faces are loaded in LoadGlyphs().
         *
         * If we have font fallback, font faces in all paths will be
         * loaded in AddRunWithFallback(), except for runs of codepoints
         * for which no font could be found.
         */
        if( !p_run->p_faceid )
        {
            p_run->p_faceid = SelectAndLoadFace( p_filter, p_run->p_style,
                                                 p_paragraph->p_code_points[p_run->i_start_offset] );
            if( !p_run->p_faceid )
            {
                p_run->p_faceid = p_sys->p_faceid;
                p_run->p_style = p_sys->p_default_style;
            }
        }

        vlc_face_id_t *p_faceid = p_run->p_faceid;
        const text_style_t *p_style = p_run->p_style;

        if(!p_faceid)
            goto error;

        vlc_ftcache_metrics_t metrics;
        metrics.height_px = ConvertToLiveSize( p_filter, p_style );
        metrics.width_px = GetFontWidthForStyle( p_style, metrics.height_px );

        FT_Face p_face = vlc_ftcache_LoadFaceByID( p_sys->ftcache, p_faceid, &metrics );
        if(!p_face)
            goto error;

        hb_font_t *p_hb_font = hb_ft_font_create( p_face, 0 );
        if( !p_hb_font )
        {
            msg_Err( p_filter,
                     "ShapeParagraphHarfBuzz(): hb_ft_font_create() error" );
            goto error;
        }

        p_run->p_buffer = hb_buffer_create();
        if( !p_run->p_buffer )
        {
            msg_Err( p_filter,
                     "ShapeParagraphHarfBuzz(): hb_buffer_create() error" );
            hb_font_destroy( p_hb_font );
            goto error;
        }

        hb_buffer_set_direction( p_run->p_buffer, p_run->direction );
        hb_buffer_set_script( p_run->p_buffer, p_run->script );
#ifdef __OS2__
        hb_buffer_add_utf16( p_run->p_buffer,
                             p_paragraph->p_code_points + p_run->i_start_offset,
                             p_run->i_end_offset - p_run->i_start_offset, 0,
                             p_run->i_end_offset - p_run->i_start_offset );
#else
        hb_buffer_add_utf32( p_run->p_buffer,
                             p_paragraph->p_code_points + p_run->i_start_offset,
                             p_run->i_end_offset - p_run->i_start_offset, 0,
                             p_run->i_end_offset - p_run->i_start_offset );
#endif
        hb_shape( p_hb_font, p_run->p_buffer, 0, 0 );

        hb_font_destroy( p_hb_font );
        p_hb_font = 0;

        const unsigned length = hb_buffer_get_length( p_run->p_buffer );
        if( length == 0 )
        {
            msg_Err( p_filter,
                     "ShapeParagraphHarfBuzz() invalid glyph count in shaped run" );
            goto error;
        }

        i_total_glyphs += length;
    }

    p_new_paragraph = NewParagraph( p_filter, i_total_glyphs,
                                    NULL, NULL, NULL,
                                    p_paragraph->i_runs_size );
    if( !p_new_paragraph )
    {
        i_ret = VLC_ENOMEM;
        goto error;
    }
    if( p_paragraph->pp_ruby )
    {
        p_new_paragraph->pp_ruby = calloc(p_new_paragraph->i_size,
                                          sizeof(ruby_block_t *));
    }

    p_new_paragraph->paragraph_type = p_paragraph->paragraph_type;

    int i_index = 0;
    for( int i = 0; i < p_paragraph->i_runs_count; ++i )
    {
        run_desc_t *p_run = p_paragraph->p_runs + i;
        unsigned int i_glyph_count;
        const hb_glyph_info_t *p_infos =
                hb_buffer_get_glyph_infos( p_run->p_buffer, &i_glyph_count );;
        const hb_glyph_position_t *p_positions =
                hb_buffer_get_glyph_positions( p_run->p_buffer, &i_glyph_count );
        for( unsigned int j = 0; j < i_glyph_count; ++j )
        {
            /*
             * HarfBuzz reverses the order of glyphs in RTL runs. We reverse
             * it again here to keep the glyphs in their logical order.
             * For line breaking of paragraphs to work correctly, visual
             * reordering should be done after line breaking has taken
             * place.
             */
            int i_run_index = p_run->direction == HB_DIRECTION_LTR ?
                    j : i_glyph_count - 1 - j;
            int i_source_index =
                    p_infos[ i_run_index ].cluster + p_run->i_start_offset;

            p_new_paragraph->p_code_points[ i_index ] = 0;
            p_new_paragraph->pi_glyph_indices[ i_index ] =
                p_infos[ i_run_index ].codepoint;
            p_new_paragraph->p_scripts[ i_index ] =
                p_paragraph->p_scripts[ i_source_index ];
            p_new_paragraph->p_types[ i_index ] =
                p_paragraph->p_types[ i_source_index ];
            p_new_paragraph->p_levels[ i_index ] =
                p_paragraph->p_levels[ i_source_index ];
            p_new_paragraph->pp_styles[ i_index ] =
                p_paragraph->pp_styles[ i_source_index ];
            if( p_new_paragraph->pp_ruby )
                p_new_paragraph->pp_ruby[ i_index ] =
                    p_paragraph->pp_ruby[ i_source_index ];
            p_new_paragraph->p_glyph_bitmaps[ i_index ].i_x_offset =
                p_positions[ i_run_index ].x_offset;
            p_new_paragraph->p_glyph_bitmaps[ i_index ].i_y_offset =
                p_positions[ i_run_index ].y_offset;
            p_new_paragraph->p_glyph_bitmaps[ i_index ].i_x_advance =
                p_positions[ i_run_index ].x_advance;
            p_new_paragraph->p_glyph_bitmaps[ i_index ].i_y_advance =
                p_positions[ i_run_index ].y_advance;

            ++i_index;
        }
        if( AddRun( p_filter, p_new_paragraph, i_index - i_glyph_count,
                    i_index, p_run->p_faceid, p_run->p_style ) )
            goto error;
    }

    for( int i = 0; i < p_paragraph->i_runs_count; ++i )
    {
        hb_buffer_destroy( p_paragraph->p_runs[ i ].p_buffer );
    }
    FreeParagraph( *p_old_paragraph );
    *p_old_paragraph = p_new_paragraph;

    return VLC_SUCCESS;

error:
    for( int i = 0; i < p_paragraph->i_runs_count; ++i )
    {
        if( p_paragraph->p_runs[ i ].p_buffer )
            hb_buffer_destroy( p_paragraph->p_runs[ i ].p_buffer );
    }

    if( p_new_paragraph )
        FreeParagraph( p_new_paragraph );

    return i_ret;
}
#endif

#ifdef HAVE_FRIBIDI
#ifndef HAVE_HARFBUZZ
/**
 * Shape a paragraph with FriBidi.
 * Shaping with FriBidi is currently limited to mirroring and simple
 * Arabic shaping.
 */
static int ShapeParagraphFriBidi( filter_t *p_filter, paragraph_t *p_paragraph )
{

    if( p_paragraph->i_size <= 0 )
    {
        msg_Err( p_filter,
                "ShapeParagraphFriBidi() invalid parameters. Paragraph size: %d",
                 p_paragraph->i_size );
        return VLC_EGENERIC;
    }

    FriBidiJoiningType *p_joining_types =
            vlc_alloc( p_paragraph->i_size, sizeof( *p_joining_types ) );
    if( !p_joining_types )
        return VLC_ENOMEM;

    fribidi_get_joining_types( p_paragraph->p_code_points,
                               p_paragraph->i_size, p_joining_types );
    fribidi_join_arabic( p_paragraph->p_types, p_paragraph->i_size,
                         p_paragraph->p_levels, p_joining_types );
    fribidi_shape( FRIBIDI_FLAGS_DEFAULT | FRIBIDI_FLAGS_ARABIC,
                   p_paragraph->p_levels,
                   p_paragraph->i_size,
                   p_joining_types,
                   p_paragraph->p_code_points );

    free( p_joining_types );

    return VLC_SUCCESS;
}

/**
 * Zero-width invisible characters include Unicode control characters and
 * zero-width spaces among other things. If not removed they can show up in the
 * text as squares or other glyphs depending on the font. Zero-width spaces are
 * inserted when shaping with FriBidi, when it performs glyph substitution for
 * ligatures.
 */
static int RemoveZeroWidthCharacters( filter_t *p_filter, paragraph_t *p_paragraph )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    for( int i = 0; i < p_paragraph->i_size; ++i )
    {
        uni_char_t ch = p_paragraph->p_code_points[ i ];
        if( ch == 0xfeff
         || ch == 0x061c
         || ( ch >= 0x202a && ch <= 0x202e )
         || ( ch >= 0x2060 && ch <= 0x2069 )
         || ( ch >= 0x200b && ch <= 0x200f ) )
        {
            glyph_bitmaps_t *p_bitmaps = p_paragraph->p_glyph_bitmaps + i;
            vlc_ftcache_Custom_Glyph_Release( &p_bitmaps->coutline );
            vlc_ftcache_Glyph_Release( p_sys->ftcache, &p_bitmaps->cglyph );
            p_bitmaps->p_shadow = 0;
            p_bitmaps->i_x_advance = 0;
            p_bitmaps->i_y_advance = 0;
        }
    }

    return VLC_SUCCESS;
}

/**
 * Set advance values of non-spacing marks to zero. Diacritics are
 * not positioned correctly but the text is more readable.
 * For full shaping HarfBuzz is required.
 */
static int ZeroNsmAdvance( paragraph_t *p_paragraph )
{
    for( int i = 0; i < p_paragraph->i_size; ++i )
        if( p_paragraph->p_types[ i ] == FRIBIDI_TYPE_NSM )
        {
            p_paragraph->p_glyph_bitmaps[ i ].i_x_advance = 0;
            p_paragraph->p_glyph_bitmaps[ i ].i_y_advance = 0;
        }
    return VLC_SUCCESS;
}
#endif
#endif

static void ReleaseGlyphBitMaps(filter_t *p_filter, glyph_bitmaps_t *p_bitmaps)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    if( p_bitmaps->p_shadow &&
        p_bitmaps->p_shadow != p_bitmaps->cglyph.p_glyph &&
        p_bitmaps->p_shadow != p_bitmaps->coutline.p_glyph )
        FT_Done_Glyph( p_bitmaps->p_shadow );
    vlc_ftcache_Custom_Glyph_Release( &p_bitmaps->coutline );
    vlc_ftcache_Glyph_Release( p_sys->ftcache, &p_bitmaps->cglyph );
}

static int CreateOutlinedGlyph( FT_Glyph src, FT_Glyph *dest, void *priv )
{
    filter_t *p_filter = priv;
    filter_sys_t *p_sys = p_filter->p_sys;
    if(FT_Glyph_StrokeBorder( &src, p_sys->p_stroker, 0, 0 ))
        return -1;
    *dest = src;
    return 0;
}

/**
 * Load the glyphs of a paragraph. When shaping with HarfBuzz the glyph indices
 * have already been determined at this point, as well as the advance values.
 */
static int LoadGlyphs( filter_t *p_filter, paragraph_t *p_paragraph,
                       bool b_use_glyph_indices, bool b_overwrite_advance,
                       unsigned *pi_max_advance_x )
{
    if( p_paragraph->i_size <= 0 || p_paragraph->i_runs_count <= 0 )
    {
        msg_Err( p_filter, "LoadGlyphs() invalid parameters. "
                 "Paragraph size: %d. Runs count %d", p_paragraph->i_size,
                 p_paragraph->i_runs_count );
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = p_filter->p_sys;
    *pi_max_advance_x = 0;

    for( int i = 0; i < p_paragraph->i_runs_count; ++i )
    {
        run_desc_t *p_run = p_paragraph->p_runs + i;
        const text_style_t *p_style = p_run->p_style;
        vlc_ftcache_metrics_t metrics;

        if( p_run->p_faceid )
        {
            metrics.height_px = ConvertToLiveSize( p_filter, p_style );
            metrics.width_px = GetFontWidthForStyle( p_style, metrics.height_px );
            if(! vlc_ftcache_LoadFaceByID( p_sys->ftcache, p_run->p_faceid, &metrics ) )
                p_run->p_faceid = NULL;
        }

        if( !p_run->p_faceid ) /* Fallback on default font and style */
        {
            metrics.height_px = ConvertToLiveSize( p_filter, p_sys->p_default_style );
            metrics.width_px = GetFontWidthForStyle( p_sys->p_default_style, metrics.height_px );
            if( vlc_ftcache_LoadFaceByID( p_sys->ftcache, p_sys->p_faceid, &metrics ) )
            {
                p_run->p_faceid = p_sys->p_faceid;
                p_run->p_style = p_style = p_sys->p_default_style;
            }
            else continue; /* can't do much from now */
        }

        int i_stroker_radius = 0;
        if( p_sys->p_stroker && (p_style->i_style_flags & STYLE_OUTLINE) )
        {
            double f_outline_thickness = p_sys->i_outline_thickness / 100.0;
            f_outline_thickness = VLC_CLIP( f_outline_thickness, 0.0, 0.5 );
            i_stroker_radius = ( metrics.height_px << 6 ) * f_outline_thickness;
            FT_Stroker_Set( p_sys->p_stroker,
                            i_stroker_radius,
                            FT_STROKER_LINECAP_ROUND,
                            FT_STROKER_LINEJOIN_ROUND, 0 );
        }

        for( int j = p_run->i_start_offset; j < p_run->i_end_offset; ++j )
        {
            int i_glyph_index;
            if( b_use_glyph_indices )
                i_glyph_index = p_paragraph->pi_glyph_indices[ j ];
            else
            {
                i_glyph_index = vlc_ftcache_LookupCMapIndex( p_sys->ftcache, p_run->p_faceid,
                                                 p_paragraph->p_code_points[ j ] );
            }

            glyph_bitmaps_t *p_bitmaps = p_paragraph->p_glyph_bitmaps + j;

#define SKIP_GLYPH( p_bitmaps ) \
    { \
        vlc_ftcache_Glyph_Init( &p_bitmaps->cglyph );\
        vlc_ftcache_Custom_Glyph_Init( &p_bitmaps->coutline ); \
        p_bitmaps->p_shadow = 0; \
        p_bitmaps->i_x_advance = 0; \
        p_bitmaps->i_y_advance = 0; \
        continue; \
    }

            if( !i_glyph_index )
            {
                uni_char_t codepoint = p_paragraph->p_code_points[ j ];
                /*
                 * If the font has no support for special space characters, use regular
                 * space glyphs instead of the .notdef glyph.
                 */
                if( codepoint == 0x0009 || codepoint == 0x00A0
                 || codepoint == 0x1680 || codepoint == 0x3000
                 || codepoint == 0x202F || codepoint == 0x205F
                 || ( codepoint >= 0x2000 && codepoint <= 0x200A )
#ifdef HAVE_FRIBIDI
                 || p_paragraph->p_types[ j ] == FRIBIDI_TYPE_WS
                 || p_paragraph->p_types[ j ] == FRIBIDI_TYPE_CS
                 || p_paragraph->p_types[ j ] == FRIBIDI_TYPE_SS
#endif
                 )
                {
                    i_glyph_index = 3;
                }
                /* Skip carriage returns */
                else if( codepoint == 0x0D
#ifdef HAVE_FRIBIDI
                 || p_paragraph->p_types[ j ] == FRIBIDI_TYPE_BS
#endif
                 )
                    SKIP_GLYPH( p_bitmaps )
            }


            FT_Long style_flags;
            if( vlc_ftcache_GetGlyphForCurrentFace( p_sys->ftcache,
                                                    i_glyph_index,
                                                    &p_bitmaps->cglyph,
                                                    &style_flags ) )
                SKIP_GLYPH( p_bitmaps );

#undef SKIP_GLYPH

            const bool b_embolden = ( p_style->i_style_flags & STYLE_BOLD ) &&
                                   !( style_flags & FT_STYLE_FLAG_BOLD );
            const bool b_oblique = ( p_style->i_style_flags & STYLE_ITALIC ) &&
                                   !( style_flags & FT_STYLE_FLAG_ITALIC );
            /* Apply missing style by modifying the outline */
            if( (b_embolden || b_oblique) &&
                p_bitmaps->cglyph.p_glyph->format == FT_GLYPH_FORMAT_OUTLINE )
            {
                FT_Glyph transformed;
                if( !FT_Glyph_Copy( p_bitmaps->cglyph.p_glyph, &transformed ) )
                {
                    /* using a copy from now */
                    if( b_oblique )
                    {
                        FT_Matrix matrix = { .xx = 0x10000L, .xy = 0.12 * 0x10000L,
                                             .yy = 0x10000L, .yx = 0 };
                        FT_Glyph_Transform( transformed, &matrix, 0 );
                    }
                    if( b_embolden )
                        FT_Outline_Embolden( &((FT_OutlineGlyph)transformed)->outline, 1<<6 );
                    vlc_ftcache_Glyph_Release( p_sys->ftcache, &p_bitmaps->cglyph );
                    p_bitmaps->cglyph.p_glyph = transformed;
                }
            }

            /* !warn: style STYLE_OUTLINE != glyph FORMAT_OUTLINE */
            if( p_sys->p_stroker && (p_style->i_style_flags & STYLE_OUTLINE) )
            {
                p_bitmaps->coutline.p_glyph =
                    vlc_ftcache_GetOutlinedGlyph( p_sys->ftcache, p_run->p_faceid, i_glyph_index,
                                                  &metrics, style_flags, i_stroker_radius,
                                                  p_bitmaps->cglyph.p_glyph,
                                                  CreateOutlinedGlyph, p_filter,
                                                  &p_bitmaps->coutline.ref );
            }

            if( p_style->i_shadow_alpha != STYLE_ALPHA_TRANSPARENT )
                p_bitmaps->p_shadow = p_bitmaps->coutline.p_glyph ?
                                      p_bitmaps->coutline.p_glyph : p_bitmaps->cglyph.p_glyph;

            if( b_overwrite_advance )
            {
                p_bitmaps->i_x_advance = p_bitmaps->cglyph.p_glyph->advance.x >> 10;
                p_bitmaps->i_y_advance = p_bitmaps->cglyph.p_glyph->advance.y >> 10;
            }

            unsigned i_x_advance = FT_FLOOR( abs( p_bitmaps->i_x_advance ) );
            if( i_x_advance > *pi_max_advance_x )
                *pi_max_advance_x = i_x_advance;
        }
    }
    return VLC_SUCCESS;
}

static int LayoutLine( filter_t *p_filter,
                       paragraph_t *p_paragraph,
                       int i_first_char, int i_last_char,
                       bool b_grid,
                       line_desc_t **pp_line )
{
    if( p_paragraph->i_size <= 0 || p_paragraph->i_runs_count <= 0
     || i_first_char < 0 || i_last_char < 0
     || i_first_char > i_last_char
     || i_last_char >= p_paragraph->i_size )
    {
        msg_Err( p_filter,
                 "LayoutLine() invalid parameters. "
                 "Paragraph size: %d. Runs count: %d. "
                 "Start char: %d. End char: %d",
                 p_paragraph->i_size, p_paragraph->i_runs_count,
                 i_first_char, i_last_char );
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = p_filter->p_sys;
    int i_last_run = -1;
    run_desc_t *p_run = 0;
    const text_style_t *p_style = 0;
    FT_Face p_face = 0;
    FT_Vector pen = { .x = 0, .y = 0 };

    vlc_ftcache_metrics_t metrics = { 0 };
    int i_font_max_advance_y = 0;
    int i_ul_offset = 0;
    int i_ul_thickness = 0;

#ifdef HAVE_FRIBIDI
    bool b_reordered = ( 0 !=
        fribidi_reorder_line( 0, &p_paragraph->p_types[i_first_char],
                          1 + i_last_char - i_first_char,
                          0, p_paragraph->paragraph_type,
                          &p_paragraph->p_levels[i_first_char],
                          0, &p_paragraph->pi_reordered_indices[i_first_char] ) );
#endif

    line_desc_t *p_line = NewLine( 1 + i_last_char - i_first_char );
    if( !p_line )
        return VLC_ENOMEM;

    for( int i = i_first_char; i <= i_last_char; ++i )
    {
        int i_paragraph_index;
#ifdef HAVE_FRIBIDI
        if( b_reordered )
            i_paragraph_index = p_paragraph->pi_reordered_indices[ i ];
        else
#endif
        i_paragraph_index = i;

        line_character_t *p_ch = &p_line->p_character[p_line->i_character_count];
        p_ch->p_style = p_paragraph->pp_styles[ i_paragraph_index ];

        if( p_paragraph->pp_ruby )
            p_ch->p_ruby = p_paragraph->pp_ruby[ i ];

        glyph_bitmaps_t *p_bitmaps =
                p_paragraph->p_glyph_bitmaps + i_paragraph_index;

        if( !p_bitmaps->cglyph.p_glyph )
        {
            BBoxInit( &p_ch->bbox );
            continue;
        }

        if( i_last_run != p_paragraph->pi_run_ids[ i_paragraph_index ] )
        {
            i_last_run = p_paragraph->pi_run_ids[ i_paragraph_index ];
            p_run = p_paragraph->p_runs + i_last_run;
            p_style = p_run->p_style;

            metrics.height_px = ConvertToLiveSize( p_filter, p_style );
            metrics.width_px = GetFontWidthForStyle( p_style, metrics.height_px );

            p_face = vlc_ftcache_LoadFaceByID( p_sys->ftcache, p_run->p_faceid, &metrics );
        }

        FT_Vector pen_new = {
            .x = pen.x + p_paragraph->p_glyph_bitmaps[ i_paragraph_index ].i_x_offset,
            .y = pen.y + p_paragraph->p_glyph_bitmaps[ i_paragraph_index ].i_y_offset
        };
        FT_Vector pen_shadow = {
            .x = pen_new.x + p_sys->f_shadow_vector_x * ( metrics.width_px << 6 ),
            .y = pen_new.y + p_sys->f_shadow_vector_y * ( metrics.height_px << 6 )
        };

        /* Shadow being a reference to main glyph, it must be processed first */
        if( p_bitmaps->p_shadow &&
            FT_Glyph_To_Bitmap( &p_bitmaps->p_shadow, FT_RENDER_MODE_NORMAL,
                                &pen_shadow, 0 ) )
        {
            p_bitmaps->p_shadow = 0;
        }

        /* Ensure we don't release reference */
        FT_Glyph bitmapglyph = p_bitmaps->cglyph.p_glyph;
        if( FT_Glyph_To_Bitmap( &bitmapglyph,
                                FT_RENDER_MODE_NORMAL,
                                &pen_new, 0 ) )
        {
            ReleaseGlyphBitMaps( p_filter, p_bitmaps );
            continue;
        }

        /* release the source glyph or reference */
        vlc_ftcache_Glyph_Release( p_sys->ftcache, &p_bitmaps->cglyph );
        p_bitmaps->cglyph.p_glyph = bitmapglyph;

        if( p_bitmaps->coutline.p_glyph )
        {
            bitmapglyph = p_bitmaps->coutline.p_glyph;
            if( FT_Glyph_To_Bitmap( &bitmapglyph,
                                    FT_RENDER_MODE_NORMAL,
                                    &pen_new, 0 ) )
                bitmapglyph = NULL;
            vlc_ftcache_Custom_Glyph_Release( &p_bitmaps->coutline );
            p_bitmaps->coutline.p_glyph = bitmapglyph;
        }

        FT_Glyph_Get_CBox( p_bitmaps->cglyph.p_glyph, FT_GLYPH_BBOX_PIXELS,
                           &p_bitmaps->glyph_bbox );
        FixGlyph( p_bitmaps->cglyph.p_glyph, &p_bitmaps->glyph_bbox,
                  p_bitmaps->i_x_advance, p_bitmaps->i_y_advance,
                  &pen_new );
        if( p_bitmaps->coutline.p_glyph )
        {
            FT_Glyph_Get_CBox( p_bitmaps->coutline.p_glyph, FT_GLYPH_BBOX_PIXELS,
                               &p_bitmaps->outline_bbox );
            FixGlyph( p_bitmaps->coutline.p_glyph, &p_bitmaps->outline_bbox,
                      p_bitmaps->i_x_advance, p_bitmaps->i_y_advance,
                      &pen_new );
        }
        if( p_bitmaps->p_shadow )
        {
            FT_Glyph_Get_CBox( p_bitmaps->p_shadow, FT_GLYPH_BBOX_PIXELS,
                               &p_bitmaps->shadow_bbox );
            FixGlyph( p_bitmaps->p_shadow, &p_bitmaps->shadow_bbox,
                      p_bitmaps->i_x_advance, p_bitmaps->i_y_advance,
                      &pen_shadow );
        }

        int i_line_offset    = 0;
        int i_line_thickness = 0;

        if( p_ch->p_style->i_style_flags & (STYLE_UNDERLINE | STYLE_STRIKEOUT) )
        {
            i_line_offset =
                labs( FT_FLOOR( FT_MulFix( p_face->underline_position,
                                           p_face->size->metrics.y_scale ) ) );

            i_line_thickness =
                labs( FT_CEIL( FT_MulFix( p_face->underline_thickness,
                                          p_face->size->metrics.y_scale ) ) );

            if( p_ch->p_style->i_style_flags & STYLE_STRIKEOUT )
            {
                /* Move the baseline to make it strikethrough instead of
                 * underline. That means that strikethrough takes precedence
                 */
                i_line_offset -=
                    labs( FT_FLOOR( FT_MulFix( p_face->descender * 2,
                                               p_face->size->metrics.y_scale ) ) );
                p_bitmaps->glyph_bbox.yMax =
                    __MAX( p_bitmaps->glyph_bbox.yMax,
                           - i_line_offset );
                p_bitmaps->glyph_bbox.yMin =
                    __MIN( p_bitmaps->glyph_bbox.yMin,
                           i_line_offset - i_line_thickness );
            }
            else if( i_line_thickness > 0 )
            {
                p_bitmaps->glyph_bbox.yMin =
                    __MIN( p_bitmaps->glyph_bbox.yMin,
                           - i_line_offset - i_line_thickness );

                /* The real underline thickness and position are
                 * updated once the whole line has been parsed */
                i_ul_offset = __MAX( i_ul_offset, i_line_offset );
                i_ul_thickness = __MAX( i_ul_thickness, i_line_thickness );
                i_line_thickness = -1;
            }
        }

        p_ch->p_glyph = ( FT_BitmapGlyph ) p_bitmaps->cglyph.p_glyph;
        p_ch->p_outline = ( FT_BitmapGlyph ) p_bitmaps->coutline.p_glyph;
        p_ch->p_shadow = ( FT_BitmapGlyph ) p_bitmaps->p_shadow;

        p_ch->i_line_thickness = i_line_thickness;
        p_ch->i_line_offset = i_line_offset;

        /* Compute bounding box for all glyphs */
        p_ch->bbox = p_bitmaps->glyph_bbox;
        if( p_bitmaps->coutline.p_glyph )
            BBoxEnlarge( &p_ch->bbox, &p_bitmaps->outline_bbox );
        if( p_bitmaps->p_shadow )
            BBoxEnlarge( &p_ch->bbox, &p_bitmaps->shadow_bbox );

        BBoxEnlarge( &p_line->bbox, &p_ch->bbox );

        pen.x += p_bitmaps->i_x_advance;
        pen.y += p_bitmaps->i_y_advance;

        /* Get max advance for grid mode */
        if( b_grid && i_font_max_advance_y == 0 && p_face )
        {
            i_font_max_advance_y = labs( FT_FLOOR( FT_MulFix( p_face->max_advance_height,
                                      p_face->size->metrics.y_scale ) ) );
        }

        /* Keep track of blank/spaces in front/end of line */
        if( p_ch->p_glyph->bitmap.rows )
        {
            if( p_line->i_first_visible_char_index < 0 )
                p_line->i_first_visible_char_index = p_line->i_character_count;
            p_line->i_last_visible_char_index = p_line->i_character_count;
        }

        p_line->i_character_count++;
    }

    /* Second pass for ruby layout */
    if( p_paragraph->pp_ruby )
    {
        const int i_ruby_baseline = p_line->bbox.yMax;
        const ruby_block_t *p_prevruby = NULL;
        for( int i = 0; i < p_line->i_character_count; ++i )
        {
            line_character_t *p_ch = &p_line->p_character[i];
            if( p_ch->p_ruby == p_prevruby || !p_ch->p_glyph )
                continue;
            p_prevruby = p_ch->p_ruby;
            if( !p_ch->p_ruby )
                continue;
            line_desc_t *p_rubyline = p_ch->p_ruby->p_laid;
            if( !p_rubyline )
                continue;

            int i_rubyadvance = (p_rubyline->bbox.xMax - p_rubyline->bbox.xMin);
            int i_rubyheight = (p_rubyline->bbox.yMax - p_rubyline->bbox.yMin);
            MoveLineTo( p_rubyline, p_ch->bbox.xMin, i_ruby_baseline + i_rubyheight );
            BBoxEnlarge( &p_line->bbox, &p_rubyline->bbox );

            int i_count;
            int i_baseadvance = RubyBaseAdvance( p_line, i, &i_count );
            if( i_baseadvance < i_rubyadvance )
            {
                IndentCharsFrom( p_line, i, i_count, (i_rubyadvance - i_baseadvance) / 2, 0 );
                IndentCharsFrom( p_line, i + i_count, p_line->i_character_count - (i + i_count),
                                 (i_rubyadvance - i_baseadvance + 1), 0 );
            }
            else if( i_baseadvance > i_rubyadvance + 1 )
            {
                ShiftLine( p_rubyline, (i_baseadvance - i_rubyadvance) / 2, 0 );
                BBoxEnlarge( &p_line->bbox, &p_rubyline->bbox ); /* shouldn't be needed */
            }
        }
    }

    p_line->i_width = __MAX( 0, p_line->bbox.xMax - p_line->bbox.xMin );

    if( b_grid )
        p_line->i_height = i_font_max_advance_y;
    else
        p_line->i_height = __MAX( 0, p_line->bbox.yMax - p_line->bbox.yMin );

    if( i_ul_thickness > 0 )
    {
        for( int i = 0; i < p_line->i_character_count; i++ )
        {
            line_character_t *ch = &p_line->p_character[i];
            if( ch->i_line_thickness < 0 )
            {
                ch->i_line_offset    = i_ul_offset;
                ch->i_line_thickness = i_ul_thickness;
            }
        }
    }

    *pp_line = p_line;
    return VLC_SUCCESS;
}

static inline bool IsWhitespaceAt( paragraph_t *p_paragraph, size_t i )
{
    return ( p_paragraph->p_code_points[ i ] == ' '
#ifdef HAVE_FRIBIDI
            || p_paragraph->p_types[ i ] == FRIBIDI_TYPE_WS
#endif
    );
}

static int LayoutParagraph( filter_t *p_filter, paragraph_t *p_paragraph,
                            unsigned i_max_width, unsigned i_max_advance_x,
                            bool b_grid, bool b_balance,
                            line_desc_t **pp_lines )
{
    if( p_paragraph->i_size <= 0 || p_paragraph->i_runs_count <= 0 )
    {
        msg_Err( p_filter, "LayoutParagraph() invalid parameters. "
                 "Paragraph size: %d. Runs count %d",
                 p_paragraph->i_size, p_paragraph->i_runs_count );
        return VLC_EGENERIC;
    }

    /*
     * Check max line width to allow for outline and shadow glyphs,
     * and any extra width caused by visual reordering
     */
    if( i_max_width <= i_max_advance_x )
    {
        msg_Err( p_filter, "LayoutParagraph(): Invalid max width" );
        return VLC_EGENERIC;
    }

    i_max_width <<= 6;
    i_max_advance_x <<= 6;

    int i_line_start = 0;
    FT_Pos i_width = 0;
    FT_Pos i_preferred_width = i_max_width;
    FT_Pos i_total_width = 0;
    FT_Pos i_last_space_width = 0;
    int i_last_space = -1;
    line_desc_t *p_first_line = NULL;
    line_desc_t **pp_line = &p_first_line;

    for( int i = 0; i < p_paragraph->i_size; ++i )
    {
        if( !IsWhitespaceAt( p_paragraph, i ) || i != i_last_space + 1 )
            i_total_width += p_paragraph->p_glyph_bitmaps[ i ].i_x_advance;
        else
            i_last_space = i;
    }
    i_last_space = -1;

    if( i_total_width == 0 )
    {
        for( int i=0; i < p_paragraph->i_size; ++i )
            ReleaseGlyphBitMaps( p_filter, &p_paragraph->p_glyph_bitmaps[ i ] );
        return VLC_SUCCESS;
    }

    if( b_balance )
    {
        int i_line_count = i_total_width / (i_max_width - i_max_advance_x) + 1;
        i_preferred_width = i_total_width / i_line_count;
    }

    for( int i = 0; i <= p_paragraph->i_size; ++i )
    {
        if( i == p_paragraph->i_size )
        {
            if( i_line_start < i )
                if( LayoutLine( p_filter, p_paragraph,
                                i_line_start, i - 1, b_grid, pp_line ) )
                    goto error;

            break;
        }

        if( p_paragraph->pp_ruby &&
            p_paragraph->pp_ruby[i] &&
            p_paragraph->pp_ruby[i]->p_laid )
        {
            /* Just forward as non breakable */
            const ruby_block_t *p_rubyseq = p_paragraph->pp_ruby[i];
            int i_advance = 0;
            int i_advanceruby = p_rubyseq->p_laid->i_width;
            while( i + 1 < p_paragraph->i_size &&
                   p_rubyseq == p_paragraph->pp_ruby[i + 1] )
                i_advance += p_paragraph->p_glyph_bitmaps[ i++ ].i_x_advance;
            /* Just forward as non breakable */
            i_width += (i_advance < i_advanceruby) ? i_advanceruby : i_advance;
            continue;
        }

        if( IsWhitespaceAt( p_paragraph, i ) )
        {
            if( i_line_start == i )
            {
                /*
                 * Free orphaned white space glyphs not belonging to any lines.
                 * At this point p_shadow points to either p_glyph or p_outline,
                 * so we should not free it explicitly.
                 */
                ReleaseGlyphBitMaps( p_filter, &p_paragraph->p_glyph_bitmaps[ i ] );
                i_line_start = i + 1;
                continue;
            }

            if( i_last_space == i - 1 )
            {
                i_last_space = i;
                continue;
            }

            i_last_space = i;
            i_last_space_width = i_width;
        }

        const run_desc_t *p_run = &p_paragraph->p_runs[p_paragraph->pi_run_ids[i]];
        const int i_advance_x = p_paragraph->p_glyph_bitmaps[ i ].i_x_advance;

        if( ( i_last_space_width + i_advance_x > i_preferred_width &&
              p_run->p_style->e_wrapinfo == STYLE_WRAP_DEFAULT )
            || i_width + i_advance_x > i_max_width )
        {
            if( i_line_start == i )
            {
                /* If wrapping, algorithm would not end shifting lines down.
                 *  Not wrapping, that can't be rendered anymore. */
                msg_Dbg( p_filter, "LayoutParagraph(): First glyph width in line exceeds maximum, skipping" );
                for( ; i < p_paragraph->i_size; ++i )
                    ReleaseGlyphBitMaps( p_filter, &p_paragraph->p_glyph_bitmaps[ i ] );
                return VLC_SUCCESS;
            }

            int i_newline_start;
            if( i_last_space > i_line_start && p_run->p_style->e_wrapinfo == STYLE_WRAP_DEFAULT )
                i_newline_start = i_last_space; /* we break line on last space */
            else
                i_newline_start = i; /* we break line on last char */

            if( LayoutLine( p_filter, p_paragraph, i_line_start,
                            i_newline_start - 1, b_grid, pp_line ) )
                goto error;

            /* Handle early end of renderable content;
               We're over size and we can't break space */
            if( p_run->p_style->e_wrapinfo == STYLE_WRAP_NONE )
            {
                for( ; i < p_paragraph->i_size; ++i )
                    ReleaseGlyphBitMaps( p_filter, &p_paragraph->p_glyph_bitmaps[ i ] );
                break;
            }

            pp_line = &( *pp_line )->p_next;

            /* If we created a line up to previous space, we only keep the difference for
               our current width since that split */
            if( i_newline_start == i_last_space )
            {
                i_width = i_width - i_last_space_width;
                if( i_newline_start + 1 < p_paragraph->i_size )
                {
                    i_line_start = i_newline_start + 1;
                    ReleaseGlyphBitMaps( p_filter, &p_paragraph->p_glyph_bitmaps[ i_newline_start ] );
                }
                else
                    i_line_start = i_newline_start; // == i
            }
            else
            {
                i_width = 0;
                i_line_start = i_newline_start;
            }
            i_last_space_width = 0;
        }
        i_width += i_advance_x;
    }

    *pp_lines = p_first_line;
    return VLC_SUCCESS;

error:
    for( int i = i_line_start; i < p_paragraph->i_size; ++i )
        ReleaseGlyphBitMaps( p_filter, &p_paragraph->p_glyph_bitmaps[ i ] );
    if( p_first_line )
        FreeLines( p_first_line );
    return VLC_EGENERIC;
}

static paragraph_t * BuildParagraph( filter_t *p_filter,
                                     int i_size,
                                     const uni_char_t *p_uchars,
                                     text_style_t **pp_styles,
                                     ruby_block_t **pp_ruby,
                                     int i_runs_size,
                                     unsigned *pi_max_advance_x )
{
    paragraph_t *p_paragraph = NewParagraph( p_filter, i_size,
                                p_uchars,
                                pp_styles,
                                pp_ruby,
                                i_runs_size );
    if( !p_paragraph )
        return NULL;

#ifdef HAVE_FRIBIDI
    if( AnalyzeParagraph( p_paragraph ) )
        goto error;
#endif

    if( ItemizeParagraph( p_filter, p_paragraph ) )
        goto error;

#if defined HAVE_HARFBUZZ
    if( ShapeParagraphHarfBuzz( p_filter, &p_paragraph ) )
        goto error;

    if( LoadGlyphs( p_filter, p_paragraph, true, false, pi_max_advance_x ) )
        goto error;

#elif defined HAVE_FRIBIDI
    if( ShapeParagraphFriBidi( p_filter, p_paragraph ) )
        goto error;
    if( LoadGlyphs( p_filter, p_paragraph, false, true, pi_max_advance_x ) )
        goto error;
    if( RemoveZeroWidthCharacters( p_filter, p_paragraph ) )
        goto error;
    if( ZeroNsmAdvance( p_paragraph ) )
        goto error;
#else
    if( LoadGlyphs( p_filter, p_paragraph, false, true, pi_max_advance_x ) )
        goto error;
#endif

    return p_paragraph;

error:
    if( p_paragraph )
        FreeParagraph( p_paragraph );

    return NULL;
}

static int LayoutRubyText( filter_t *p_filter,
                           const uni_char_t *p_uchars,
                           int i_uchars,
                           text_style_t *p_style,
                           line_desc_t **pp_line )
{
    unsigned int i_max_advance_x;

    text_style_t **pp_styles = malloc(sizeof(*pp_styles) * i_uchars);
    for(int i=0;i<i_uchars;i++)
        pp_styles[i] = p_style;

    paragraph_t *p_paragraph = BuildParagraph( p_filter, i_uchars,
                                               p_uchars, pp_styles,
                                               NULL, 1,
                                               &i_max_advance_x );
    if( !p_paragraph )
    {
        free( pp_styles );
        return VLC_EGENERIC;
    }

    if( LayoutLine( p_filter, p_paragraph,
                    0, p_paragraph->i_size - 1,
                    false, pp_line ) )
    {
        for( int i = 0; i < p_paragraph->i_size; ++i )
            ReleaseGlyphBitMaps( p_filter, &p_paragraph->p_glyph_bitmaps[ i ] );
        free( pp_styles );
        FreeParagraph( p_paragraph );
        return VLC_EGENERIC;
    }

    FreeParagraph( p_paragraph );
    free( pp_styles );

    return VLC_SUCCESS;
}

int LayoutTextBlock( filter_t *p_filter,
                     const layout_text_block_t *p_textblock,
                     line_desc_t **pp_lines, FT_BBox *p_bbox,
                     int *pi_max_face_height )
{
    line_desc_t *p_first_line = 0;
    line_desc_t **pp_line = &p_first_line;
    size_t i_paragraph_start = 0;
    unsigned i_total_height = 0;
    unsigned i_max_advance_x = 0;
    int i_max_face_height = 0;

    /* Prepare ruby content */
    if( p_textblock->pp_ruby )
    {
        ruby_block_t *p_prev = NULL;
        for( size_t i=0; i<p_textblock->i_count; i++ )
        {
            if( p_textblock->pp_ruby[i] == p_prev )
                continue;
            p_prev = p_textblock->pp_ruby[i];
            if( p_prev )
                LayoutRubyText( p_filter, p_prev->p_uchars, p_prev->i_count,
                                p_prev->p_style, &p_prev->p_laid );
        }
    }
    /* !Prepare ruby content */

    for( size_t i = 0; i <= p_textblock->i_count; ++i )
    {
        if( i == p_textblock->i_count || p_textblock->p_uchars[ i ] == '\n' )
        {
            if( i_paragraph_start == i )
            {
                i_paragraph_start = i + 1;
                continue;
            }

            paragraph_t *p_paragraph =
                    BuildParagraph( p_filter,
                                    i - i_paragraph_start,
                                    &p_textblock->p_uchars[i_paragraph_start],
                                    &p_textblock->pp_styles[i_paragraph_start],
                                    p_textblock->pp_ruby ?
                                    &p_textblock->pp_ruby[i_paragraph_start] : NULL,
                                    20, &i_max_advance_x );
            if( !p_paragraph )
            {
                if( p_first_line ) FreeLines( p_first_line );
                return VLC_ENOMEM;
            }

            if( LayoutParagraph( p_filter, p_paragraph,
                                 p_textblock->i_max_width,
                                 i_max_advance_x,
                                 p_textblock->b_grid, p_textblock->b_balanced,
                                 pp_line ) )
            {
                FreeParagraph( p_paragraph );
                if( p_first_line ) FreeLines( p_first_line );
                return VLC_EGENERIC;
            }

            FreeParagraph( p_paragraph );

            for( ; *pp_line; pp_line = &(*pp_line)->p_next )
            {
                /* only cut at max i_max_height + 1 line due to
                 * approximate font sizing vs region size */
                if( p_textblock->i_max_height > 0 && i_total_height > p_textblock->i_max_height )
                {
                    i_total_height = p_textblock->i_max_height + 1;
                    line_desc_t *p_todelete = *pp_line;
                    while( p_todelete ) /* Drop extra lines */
                    {
                        line_desc_t *p_next = p_todelete->p_next;
                        FreeLine( p_todelete );
                        p_todelete = p_next;
                    }
                    *pp_line = NULL;
                    i = p_textblock->i_count + 1; /* force no more paragraphs */
                    break; /* ! no p_next ! */
                }
                else if( (*pp_line)->i_height > i_max_face_height )
                {
                    i_max_face_height = (*pp_line)->i_height;
                }
                i_total_height += (*pp_line)->i_height;
            }
            i_paragraph_start = i + 1;
        }
    }

    int i_base_line = 0;
    FT_BBox bbox;
    BBoxInit( &bbox );

    for( line_desc_t *p_line = p_first_line; p_line; p_line = p_line->p_next )
    {
        p_line->origin.y = i_base_line;
        p_line->bbox.yMin -= i_base_line;
        p_line->bbox.yMax -= i_base_line;
        BBoxEnlarge( &bbox, &p_line->bbox );

        i_base_line += i_max_face_height;
    }

    *pi_max_face_height = i_max_face_height;
    *pp_lines = p_first_line;
    *p_bbox = bbox;
    return VLC_SUCCESS;
}
