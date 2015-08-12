/*****************************************************************************
 * vlc_text_style.h: text_style_t definition and helpers.
 *****************************************************************************
 * Copyright (C) 1999-2010 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman _AT_ videolan _DOT_ org>
 *          basOS G <noxelia 4t gmail , com>
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

#ifndef VLC_TEXT_STYLE_H
#define VLC_TEXT_STYLE_H 1

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Text style
 *
 * A text style is used to specify the formatting of text.
 * A font renderer can use the supplied information to render the
 * text specified.
 */
typedef struct
{
    /* Family font names */
    char *     psz_fontname;      /**< The name of the font */
    char *     psz_monofontname;  /**< The name of the mono font */

    uint16_t   i_features;        /**< Feature flags (means non default) */
    uint16_t   i_style_flags;     /**< Formatting style flags */

    /* Font style */
    float      f_font_relsize;    /**< The font size in % */
    int        i_font_size;       /**< The font size in pixels */
    int        i_font_color;      /**< The color of the text 0xRRGGBB
                                       (native endianness) */
    uint8_t    i_font_alpha;      /**< The transparency of the text.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
    int        i_spacing;         /**< The spaceing between glyphs in pixels */

    /* Outline */
    int        i_outline_color;   /**< The color of the outline 0xRRGGBB */
    uint8_t    i_outline_alpha;   /**< The transparency of the outline.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
    int        i_outline_width;   /**< The width of the outline in pixels */

    /* Shadow */
    int        i_shadow_color;    /**< The color of the shadow 0xRRGGBB */
    uint8_t    i_shadow_alpha;    /**< The transparency of the shadow.
                                        0x00 is fully opaque,
                                        0xFF fully transparent */
    int        i_shadow_width;    /**< The width of the shadow in pixels */

    /* Background (and karaoke) */
    int        i_background_color;/**< The color of the background 0xRRGGBB */
    uint8_t    i_background_alpha;/**< The transparency of the background.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
    int        i_karaoke_background_color;/**< Background color for karaoke 0xRRGGBB */
    uint8_t    i_karaoke_background_alpha;/**< The transparency of the karaoke bg.
                                       0x00 is fully opaque,
                                       0xFF fully transparent */
} text_style_t;

/* Features flags for \ref i_features */
#define STYLE_NO_DEFAULTS               0x0
#define STYLE_FULLY_SET                 0xFFFF
#define STYLE_HAS_FONT_COLOR            (1 << 0)
#define STYLE_HAS_FONT_ALPHA            (1 << 1)
#define STYLE_HAS_FLAGS                 (1 << 2)
#define STYLE_HAS_OUTLINE_COLOR         (1 << 3)
#define STYLE_HAS_OUTLINE_ALPHA         (1 << 4)
#define STYLE_HAS_SHADOW_COLOR          (1 << 5)
#define STYLE_HAS_SHADOW_ALPHA          (1 << 6)
#define STYLE_HAS_BACKGROUND_COLOR      (1 << 7)
#define STYLE_HAS_BACKGROUND_ALPHA      (1 << 8)
#define STYLE_HAS_K_BACKGROUND_COLOR    (1 << 9)
#define STYLE_HAS_K_BACKGROUND_ALPHA    (1 << 10)

/* Style flags for \ref text_style_t */
#define STYLE_BOLD              (1 << 0)
#define STYLE_ITALIC            (1 << 1)
#define STYLE_OUTLINE           (1 << 2)
#define STYLE_SHADOW            (1 << 3)
#define STYLE_BACKGROUND        (1 << 4)
#define STYLE_UNDERLINE         (1 << 5)
#define STYLE_STRIKEOUT         (1 << 6)
#define STYLE_HALFWIDTH         (1 << 7)
#define STYLE_MONOSPACED        (1 << 8)

#define STYLE_DEFAULT_FONT_SIZE 20
#define STYLE_DEFAULT_REL_FONT_SIZE 5.0


typedef struct text_segment_t text_segment_t;
/**
 * Text segment for subtitles
 *
 * This structure is used to store a formatted text, with mixed styles
 * Every segment is comprised of one text and a unique style
 *
 * On style change, a new segment is created with the next part of text
 * and the new style, and chained to the list
 *
 * Create with text_segment_New and clean the chain with
 * text_segment_ChainDelete
 */
struct text_segment_t {
    char *psz_text;                   /**< text string of the segment */
    text_style_t *style;              /**< style applied to this segment */
    text_segment_t *p_next;           /**< next segment */
};

/**
 * Create a default text style
 */
VLC_API text_style_t * text_style_New( void );

/**
 * Create a text style
 *
 * Set feature flags as argument if you want to set style defaults
 */
VLC_API text_style_t * text_style_Create( int );

/**
 * Copy a text style into another
 */
VLC_API text_style_t * text_style_Copy( text_style_t *, const text_style_t * );

/**
 * Duplicate a text style
 */
VLC_API text_style_t * text_style_Duplicate( const text_style_t * );

/**
 * Merge two styles using non default values
 *
 * Set b_override to true if you also want to overwrite non-defaults
 */
VLC_API void text_style_Merge( text_style_t *, const text_style_t *, bool b_override );

/**
 * Delete a text style created by text_style_New or text_style_Duplicate
 */
VLC_API void text_style_Delete( text_style_t * );

/**
 * This function will create a new text segment.
 *
 * You should use text_segment_ChainDelete to destroy it, to clean all
 * the linked segments, or text_segment_Delete to free a specic one
 *
 * This duplicates the string passed as argument
 */
VLC_API text_segment_t *text_segment_New( const char * );

/**
 * This function will create a new text segment and duplicates the style passed as argument
 *
 * You should use text_segment_ChainDelete to destroy it, to clean all
 * the linked segments, or text_segment_Delete to free a specic one
 *
 * This doesn't initialize the text.
 */
VLC_API text_segment_t *text_segment_NewInheritStyle( const text_style_t* p_style );

/**
 * Delete a text segment and its content.
 *
 * This assumes the segment is not part of a chain
 */
VLC_API void text_segment_Delete( text_segment_t * );

/**
 * This function will destroy a list of text segments allocated
 * by text_segment_New.
 *
 * You may pass it NULL.
 */
VLC_API void text_segment_ChainDelete( text_segment_t * );

/**
 * This function will copy a text_segment and its chain into a new one
 *
 * You may give it NULL, but it will return NULL.
 */
VLC_API text_segment_t * text_segment_Copy( text_segment_t * );

#ifdef __cplusplus
}
#endif

#endif /* VLC_TEXT_STYLE_H */

