/*****************************************************************************
 * text_style.c
 *****************************************************************************
 * Copyright (C) 1999-2010 VLC authors and VideoLAN
 *
 * Author: basOS G <noxelia 4t gmail , com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_text_style.h>

#include <ctype.h>

static const struct {
    const char *psz_name;
    uint32_t   i_value;
} p_html_colors[] = {
    /* Official html colors */
    { "Aqua",    0x00FFFF },
    { "Black",   0x000000 },
    { "Blue",    0x0000FF },
    { "Fuchsia", 0xFF00FF },
    { "Gray",    0x808080 },
    { "Green",   0x008000 },
    { "Lime",    0x00FF00 },
    { "Maroon",  0x800000 },
    { "Navy",    0x000080 },
    { "Olive",   0x808000 },
    { "Purple",  0x800080 },
    { "Red",     0xFF0000 },
    { "Silver",  0xC0C0C0 },
    { "Teal",    0x008080 },
    { "White",   0xFFFFFF },
    { "Yellow",  0xFFFF00 },

    /* Common ones */
    { "AliceBlue", 0xF0F8FF },
    { "AntiqueWhite", 0xFAEBD7 },
    { "Aqua", 0x00FFFF },
    { "Aquamarine", 0x7FFFD4 },
    { "Azure", 0xF0FFFF },
    { "Beige", 0xF5F5DC },
    { "Bisque", 0xFFE4C4 },
    { "Black", 0x000000 },
    { "BlanchedAlmond", 0xFFEBCD },
    { "Blue", 0x0000FF },
    { "BlueViolet", 0x8A2BE2 },
    { "Brown", 0xA52A2A },
    { "BurlyWood", 0xDEB887 },
    { "CadetBlue", 0x5F9EA0 },
    { "Chartreuse", 0x7FFF00 },
    { "Chocolate", 0xD2691E },
    { "Coral", 0xFF7F50 },
    { "CornflowerBlue", 0x6495ED },
    { "Cornsilk", 0xFFF8DC },
    { "Crimson", 0xDC143C },
    { "Cyan", 0x00FFFF },
    { "DarkBlue", 0x00008B },
    { "DarkCyan", 0x008B8B },
    { "DarkGoldenRod", 0xB8860B },
    { "DarkGray", 0xA9A9A9 },
    { "DarkGrey", 0xA9A9A9 },
    { "DarkGreen", 0x006400 },
    { "DarkKhaki", 0xBDB76B },
    { "DarkMagenta", 0x8B008B },
    { "DarkOliveGreen", 0x556B2F },
    { "Darkorange", 0xFF8C00 },
    { "DarkOrchid", 0x9932CC },
    { "DarkRed", 0x8B0000 },
    { "DarkSalmon", 0xE9967A },
    { "DarkSeaGreen", 0x8FBC8F },
    { "DarkSlateBlue", 0x483D8B },
    { "DarkSlateGray", 0x2F4F4F },
    { "DarkSlateGrey", 0x2F4F4F },
    { "DarkTurquoise", 0x00CED1 },
    { "DarkViolet", 0x9400D3 },
    { "DeepPink", 0xFF1493 },
    { "DeepSkyBlue", 0x00BFFF },
    { "DimGray", 0x696969 },
    { "DimGrey", 0x696969 },
    { "DodgerBlue", 0x1E90FF },
    { "FireBrick", 0xB22222 },
    { "FloralWhite", 0xFFFAF0 },
    { "ForestGreen", 0x228B22 },
    { "Fuchsia", 0xFF00FF },
    { "Gainsboro", 0xDCDCDC },
    { "GhostWhite", 0xF8F8FF },
    { "Gold", 0xFFD700 },
    { "GoldenRod", 0xDAA520 },
    { "Gray", 0x808080 },
    { "Grey", 0x808080 },
    { "Green", 0x008000 },
    { "GreenYellow", 0xADFF2F },
    { "HoneyDew", 0xF0FFF0 },
    { "HotPink", 0xFF69B4 },
    { "IndianRed", 0xCD5C5C },
    { "Indigo", 0x4B0082 },
    { "Ivory", 0xFFFFF0 },
    { "Khaki", 0xF0E68C },
    { "Lavender", 0xE6E6FA },
    { "LavenderBlush", 0xFFF0F5 },
    { "LawnGreen", 0x7CFC00 },
    { "LemonChiffon", 0xFFFACD },
    { "LightBlue", 0xADD8E6 },
    { "LightCoral", 0xF08080 },
    { "LightCyan", 0xE0FFFF },
    { "LightGoldenRodYellow", 0xFAFAD2 },
    { "LightGray", 0xD3D3D3 },
    { "LightGrey", 0xD3D3D3 },
    { "LightGreen", 0x90EE90 },
    { "LightPink", 0xFFB6C1 },
    { "LightSalmon", 0xFFA07A },
    { "LightSeaGreen", 0x20B2AA },
    { "LightSkyBlue", 0x87CEFA },
    { "LightSlateGray", 0x778899 },
    { "LightSlateGrey", 0x778899 },
    { "LightSteelBlue", 0xB0C4DE },
    { "LightYellow", 0xFFFFE0 },
    { "Lime", 0x00FF00 },
    { "LimeGreen", 0x32CD32 },
    { "Linen", 0xFAF0E6 },
    { "Magenta", 0xFF00FF },
    { "Maroon", 0x800000 },
    { "MediumAquaMarine", 0x66CDAA },
    { "MediumBlue", 0x0000CD },
    { "MediumOrchid", 0xBA55D3 },
    { "MediumPurple", 0x9370D8 },
    { "MediumSeaGreen", 0x3CB371 },
    { "MediumSlateBlue", 0x7B68EE },
    { "MediumSpringGreen", 0x00FA9A },
    { "MediumTurquoise", 0x48D1CC },
    { "MediumVioletRed", 0xC71585 },
    { "MidnightBlue", 0x191970 },
    { "MintCream", 0xF5FFFA },
    { "MistyRose", 0xFFE4E1 },
    { "Moccasin", 0xFFE4B5 },
    { "NavajoWhite", 0xFFDEAD },
    { "Navy", 0x000080 },
    { "OldLace", 0xFDF5E6 },
    { "Olive", 0x808000 },
    { "OliveDrab", 0x6B8E23 },
    { "Orange", 0xFFA500 },
    { "OrangeRed", 0xFF4500 },
    { "Orchid", 0xDA70D6 },
    { "PaleGoldenRod", 0xEEE8AA },
    { "PaleGreen", 0x98FB98 },
    { "PaleTurquoise", 0xAFEEEE },
    { "PaleVioletRed", 0xD87093 },
    { "PapayaWhip", 0xFFEFD5 },
    { "PeachPuff", 0xFFDAB9 },
    { "Peru", 0xCD853F },
    { "Pink", 0xFFC0CB },
    { "Plum", 0xDDA0DD },
    { "PowderBlue", 0xB0E0E6 },
    { "Purple", 0x800080 },
    { "RebeccaPurple", 0x663399 },
    { "Red", 0xFF0000 },
    { "RosyBrown", 0xBC8F8F },
    { "RoyalBlue", 0x4169E1 },
    { "SaddleBrown", 0x8B4513 },
    { "Salmon", 0xFA8072 },
    { "SandyBrown", 0xF4A460 },
    { "SeaGreen", 0x2E8B57 },
    { "SeaShell", 0xFFF5EE },
    { "Sienna", 0xA0522D },
    { "Silver", 0xC0C0C0 },
    { "SkyBlue", 0x87CEEB },
    { "SlateBlue", 0x6A5ACD },
    { "SlateGray", 0x708090 },
    { "SlateGrey", 0x708090 },
    { "Snow", 0xFFFAFA },
    { "SpringGreen", 0x00FF7F },
    { "SteelBlue", 0x4682B4 },
    { "Tan", 0xD2B48C },
    { "Teal", 0x008080 },
    { "Thistle", 0xD8BFD8 },
    { "Tomato", 0xFF6347 },
    { "Turquoise", 0x40E0D0 },
    { "Violet", 0xEE82EE },
    { "Wheat", 0xF5DEB3 },
    { "White", 0xFFFFFF },
    { "WhiteSmoke", 0xF5F5F5 },
    { "Yellow", 0xFFFF00 },
    { "YellowGreen", 0x9ACD32 },

    { NULL, 0 }
};

/* */
text_style_t *text_style_New( void )
{
    return text_style_Create( STYLE_FULLY_SET );
}

text_style_t *text_style_Create( int i_defaults )
{
    text_style_t *p_style = calloc( 1, sizeof(*p_style) );
    if( !p_style )
        return NULL;

    if( i_defaults == STYLE_NO_DEFAULTS )
        return p_style;

    /* initialize to default text style (FIXME: by flag) */
    p_style->psz_fontname = NULL;
    p_style->psz_monofontname = NULL;
    p_style->i_features = STYLE_FULLY_SET;
    p_style->i_style_flags = STYLE_OUTLINE;
    p_style->f_font_relsize = STYLE_DEFAULT_REL_FONT_SIZE;
    p_style->i_font_size = STYLE_DEFAULT_FONT_SIZE;
    p_style->i_font_color = 0xffffff;
    p_style->i_font_alpha = STYLE_ALPHA_OPAQUE;
    p_style->i_outline_color = 0x000000;
    p_style->i_outline_alpha = STYLE_ALPHA_OPAQUE;
    p_style->i_shadow_color = 0x808080;
    p_style->i_shadow_alpha = STYLE_ALPHA_OPAQUE;
    p_style->i_background_color = 0x000000;
    p_style->i_background_alpha = STYLE_ALPHA_OPAQUE;
    p_style->i_outline_width = 1;
    p_style->i_shadow_width = 0;
    p_style->i_spacing = -1;
    p_style->e_wrapinfo = STYLE_WRAP_DEFAULT;

    return p_style;
}

text_style_t *text_style_Copy( text_style_t *p_dst, const text_style_t *p_src )
{
    if( !p_src )
        return p_dst;

    /* */
    *p_dst = *p_src;

    if( p_src->psz_fontname )
        p_dst->psz_fontname = strdup( p_src->psz_fontname );

    if( p_src->psz_monofontname )
        p_dst->psz_monofontname = strdup( p_src->psz_monofontname );

    return p_dst;
}

#define MERGE(var, fflag) \
    if( (p_src->i_features & fflag) && (b_override || !(p_dst->i_features & fflag)) )\
        p_dst->var = p_src->var

#define MERGE_SIZE(var) \
    if( p_src->var > 0 && (b_override || p_dst->var <= 0) )\
        p_dst->var = p_src->var

void text_style_Merge( text_style_t *p_dst, const text_style_t *p_src, bool b_override )
{
    if( p_src->psz_fontname && (!p_dst->psz_fontname || b_override) )
    {
        free( p_dst->psz_fontname );
        p_dst->psz_fontname = strdup( p_src->psz_fontname );
    }

    if( p_src->psz_monofontname && (!p_dst->psz_monofontname || b_override) )
    {
        free( p_dst->psz_monofontname );
        p_dst->psz_monofontname = strdup( p_src->psz_monofontname );
    }

    if( p_src->i_features != STYLE_NO_DEFAULTS )
    {
        MERGE(i_font_color,         STYLE_HAS_FONT_COLOR);
        MERGE(i_font_alpha,         STYLE_HAS_FONT_ALPHA);
        MERGE(i_outline_color,      STYLE_HAS_OUTLINE_COLOR);
        MERGE(i_outline_alpha,      STYLE_HAS_OUTLINE_ALPHA);
        MERGE(i_shadow_color,       STYLE_HAS_SHADOW_COLOR);
        MERGE(i_shadow_alpha,       STYLE_HAS_SHADOW_ALPHA);
        MERGE(i_background_color,   STYLE_HAS_BACKGROUND_COLOR);
        MERGE(i_background_alpha,   STYLE_HAS_BACKGROUND_ALPHA);
        MERGE(e_wrapinfo,            STYLE_HAS_WRAP_INFO);
        p_dst->i_features |= p_src->i_features;
        p_dst->i_style_flags |= p_src->i_style_flags;
    }

    MERGE_SIZE(f_font_relsize);
    MERGE_SIZE(i_font_size);
    MERGE_SIZE(i_outline_width);
    MERGE_SIZE(i_shadow_width);
    MERGE_SIZE(i_spacing);
}

#undef MERGE
#undef MERGE_SIZE

text_style_t *text_style_Duplicate( const text_style_t *p_src )
{
    if( !p_src )
        return NULL;

    text_style_t *p_dst = calloc( 1, sizeof(*p_dst) );
    if( p_dst )
        text_style_Copy( p_dst, p_src );
    return p_dst;
}

void text_style_Delete( text_style_t *p_style )
{
    if( p_style )
        free( p_style->psz_fontname );
    if( p_style )
        free( p_style->psz_monofontname );
    free( p_style );
}

void text_segment_ruby_ChainDelete( text_segment_ruby_t *p_ruby )
{
    while( p_ruby )
    {
        text_segment_ruby_t *p_next = p_ruby->p_next;
        free( p_ruby->psz_base );
        free( p_ruby->psz_rt );
        free( p_ruby );
        p_ruby = p_next;
    }
}

text_segment_ruby_t *text_segment_ruby_New( const char *psz_base,
                                            const char *psz_rt )
{
    text_segment_ruby_t *p_rb = malloc(sizeof(*p_rb));
    if( p_rb )
    {
        p_rb->p_next = NULL;
        p_rb->psz_base = strdup( psz_base );
        p_rb->psz_rt = strdup( psz_rt );
        if( !p_rb->psz_base || !p_rb->psz_rt )
        {
            text_segment_ruby_ChainDelete( p_rb );
            return NULL;
        }
    }
    return p_rb;
}

static text_segment_ruby_t *text_segment_ruby_Duplicate( const text_segment_ruby_t *p_src )
{
    text_segment_ruby_t *p_dup = NULL;
    text_segment_ruby_t **pp_append = &p_dup;
    for ( ; p_src ; p_src = p_src->p_next )
    {
        *pp_append = text_segment_ruby_New( p_src->psz_base, p_src->psz_rt );
        if( *pp_append )
            pp_append = &((*pp_append)->p_next);
    }
    return p_dup;
}

text_segment_t *text_segment_New( const char *psz_text )
{
    text_segment_t* segment = calloc( 1, sizeof(*segment) );
    if( !segment )
        return NULL;

    if ( psz_text )
        segment->psz_text = strdup( psz_text );

    return segment;
}

text_segment_t *text_segment_NewInheritStyle( const text_style_t* p_style )
{
    if ( !p_style )
        return NULL; //FIXME: Allow this, even if it is an alias to text_segment_New( NULL ) ?
    text_segment_t* p_segment = text_segment_New( NULL );
    if ( unlikely( !p_segment ) )
        return NULL;
    p_segment->style = text_style_Duplicate( p_style );
    if ( unlikely( !p_segment->style ) )
    {
        text_segment_Delete( p_segment );
        return NULL;
    }
    return p_segment;
}

text_segment_t *text_segment_FromRuby( text_segment_ruby_t *p_ruby )
{
    text_segment_t *p_segment = text_segment_New( NULL );
    if( p_segment )
    {
        p_segment->p_ruby = p_ruby;
        size_t i_base = 1;
        for( text_segment_ruby_t *p = p_ruby; p; p = p->p_next )
            i_base += strlen( p->psz_base );
        p_segment->psz_text = malloc( i_base );
        /* Fallback for those not understanding p_ruby */
        if( p_segment->psz_text )
        {
            *p_segment->psz_text = 0;
            for( text_segment_ruby_t *p = p_ruby; p; p = p->p_next )
                strcat( p_segment->psz_text, p->psz_base );
        }
    }
    return p_segment;
}

void text_segment_Delete( text_segment_t* segment )
{
    if ( segment != NULL )
    {
        free( segment->psz_text );
        text_style_Delete( segment->style );
        text_segment_ruby_ChainDelete( segment->p_ruby );
        free( segment );
    }
}

void text_segment_ChainDelete( text_segment_t *segment )
{
    while( segment != NULL )
    {
        text_segment_t *p_next = segment->p_next;

        text_segment_Delete( segment );

        segment = p_next;
    }
}

text_segment_t *text_segment_Copy( text_segment_t *p_src )
{
    text_segment_t *p_dst = NULL, *p_dst0 = NULL;

    while( p_src ) {
        text_segment_t *p_new = text_segment_New( p_src->psz_text );

        if( unlikely( !p_new ) )
            break;

        p_new->style = text_style_Duplicate( p_src->style );
        p_new->p_ruby = text_segment_ruby_Duplicate( p_src->p_ruby );

        if( p_dst == NULL )
        {
            p_dst = p_dst0 = p_new;
        }
        else
        {
            p_dst->p_next = p_new;
            p_dst = p_dst->p_next;
        }

        p_src = p_src->p_next;
    }

    return p_dst0;
}

unsigned int vlc_html_color( const char *psz_value, bool* ok )
{
    unsigned int color = 0;
    char* psz_end;
    bool b_ret = false;

    const char *psz_hex = (*psz_value == '#') ? psz_value + 1 : psz_value;

    if( psz_hex != psz_value ||
        (*psz_hex >= '0' && *psz_hex <= '9') ||
        (*psz_hex >= 'A' && *psz_hex <= 'F') )
    {
        uint32_t i_value = (uint32_t)strtoul( psz_hex, &psz_end, 16 );
        if( *psz_end == 0 || isspace( *psz_end ) )
        {
            switch( psz_end - psz_hex )
            {
                case 8:
                    color = (i_value << 24) | (i_value >> 8);
                    b_ret = true;
                    break;
                case 6:
                    color = i_value | 0xFF000000;
                    b_ret = true;
                    break;
                default:
                    break;
            }
        }
    }

    if( !b_ret && psz_hex == psz_value &&
        !strncmp( "rgb", psz_value, 3 ) )
    {
        unsigned r,g,b,a = 0xFF;
        if( psz_value[3] == 'a' )
            b_ret = (sscanf( psz_value, "rgba(%3u,%3u,%3u,%3u)", &r, &g, &b, &a ) == 4);
        else
            b_ret = (sscanf( psz_value, "rgb(%3u,%3u,%3u)", &r, &g, &b ) == 3);
        color = (a << 24) | (r << 16) | (g << 8) | b;
    }

    if( !b_ret && psz_hex == psz_value )
    {
        for( int i = 0; p_html_colors[i].psz_name != NULL; i++ )
        {
            if( !strcasecmp( psz_value, p_html_colors[i].psz_name ) )
            {
                // Assume opaque color since the table doesn't specify an alpha
                color = p_html_colors[i].i_value | 0xFF000000;
                b_ret = true;
                break;
            }
        }
    }

    if ( ok != NULL )
        *ok = b_ret;

    return color;
}
