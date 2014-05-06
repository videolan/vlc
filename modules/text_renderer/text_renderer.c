/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Bernie Purcell <bitmap@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Felix Paul Kühne <fkuehne@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <vlc_common.h>
#include <vlc_variables.h>
#include <vlc_filter.h>                                      /* filter_sys_t */
#include <vlc_xml.h>                                         /* xml_reader */
#include <vlc_charset.h>                                     /* ToCharset */
#include <vlc_strings.h>                         /* resolve_xml_special_chars */

#include "text_renderer.h"

text_style_t *CreateStyle( char *psz_fontname, int i_font_size,
                                  uint32_t i_font_color, uint32_t i_karaoke_bg_color,
                                  int i_style_flags )
{
    text_style_t *p_style = text_style_New();
    if( !p_style )
        return NULL;

    p_style->psz_fontname = psz_fontname ? strdup( psz_fontname ) : NULL;
    p_style->i_font_size  = i_font_size;
    p_style->i_font_color = (i_font_color & 0x00ffffff) >>  0;
    p_style->i_font_alpha = (i_font_color & 0xff000000) >> 24;
    p_style->i_karaoke_background_color = (i_karaoke_bg_color & 0x00ffffff) >>  0;
    p_style->i_karaoke_background_alpha = (i_karaoke_bg_color & 0xff000000) >> 24;
    p_style->i_style_flags |= i_style_flags;
    return p_style;
}

int PushFont( font_stack_t **p_font, const char *psz_name, int i_size,
                     uint32_t i_color, uint32_t i_karaoke_bg_color )
{
    if( !p_font )
        return VLC_EGENERIC;

    font_stack_t *p_new = malloc( sizeof(*p_new) );
    if( !p_new )
        return VLC_ENOMEM;

    p_new->p_next = NULL;

    if( psz_name )
        p_new->psz_name = strdup( psz_name );
    else
        p_new->psz_name = NULL;

    p_new->i_size              = i_size;
    p_new->i_color             = i_color;
    p_new->i_karaoke_bg_color  = i_karaoke_bg_color;

    if( !*p_font )
    {
        *p_font = p_new;
    }
    else
    {
        font_stack_t *p_last;

        for( p_last = *p_font;
             p_last->p_next;
             p_last = p_last->p_next )
        ;

        p_last->p_next = p_new;
    }
    return VLC_SUCCESS;
}

int PopFont( font_stack_t **p_font )
{
    font_stack_t *p_last, *p_next_to_last;

    if( !p_font || !*p_font )
        return VLC_EGENERIC;

    p_next_to_last = NULL;
    for( p_last = *p_font;
         p_last->p_next;
         p_last = p_last->p_next )
    {
        p_next_to_last = p_last;
    }

    if( p_next_to_last )
        p_next_to_last->p_next = NULL;
    else
        *p_font = NULL;

    free( p_last->psz_name );
    free( p_last );

    return VLC_SUCCESS;
}

int PeekFont( font_stack_t **p_font, char **psz_name, int *i_size,
                     uint32_t *i_color, uint32_t *i_karaoke_bg_color )
{
    font_stack_t *p_last;

    if( !p_font || !*p_font )
        return VLC_EGENERIC;

    for( p_last=*p_font;
         p_last->p_next;
         p_last=p_last->p_next )
    ;

    *psz_name            = p_last->psz_name;
    *i_size              = p_last->i_size;
    *i_color             = p_last->i_color;
    *i_karaoke_bg_color  = p_last->i_karaoke_bg_color;

    return VLC_SUCCESS;
}

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

int HandleFontAttributes( xml_reader_t *p_xml_reader,
                                 font_stack_t **p_fonts )
{
    int        rv;
    char      *psz_fontname = NULL;
    uint32_t   i_font_color = 0xffffff;
    int        i_font_alpha = 255;
    uint32_t   i_karaoke_bg_color = 0x00ffffff;
    int        i_font_size  = STYLE_DEFAULT_FONT_SIZE;

    /* Default all attributes to the top font in the stack -- in case not
     * all attributes are specified in the sub-font
     */
    if( VLC_SUCCESS == PeekFont( p_fonts,
                                 &psz_fontname,
                                 &i_font_size,
                                 &i_font_color,
                                 &i_karaoke_bg_color ))
    {
        psz_fontname = strdup( psz_fontname );
    }
    i_font_alpha = (i_font_color >> 24) & 0xff;
    i_font_color &= 0x00ffffff;

    const char *name, *value;
    while( (name = xml_ReaderNextAttr( p_xml_reader, &value )) != NULL )
    {
        if( !strcasecmp( "face", name ) )
        {
            free( psz_fontname );
            psz_fontname = strdup( value );
        }
        else if( !strcasecmp( "size", name ) )
        {
            if( ( *value == '+' ) || ( *value == '-' ) )
            {
                int i_value = atoi( value );

                if( ( i_value >= -5 ) && ( i_value <= 5 ) )
                    i_font_size += ( i_value * i_font_size ) / 10;
                else if( i_value < -5 )
                    i_font_size = - i_value;
                else if( i_value > 5 )
                    i_font_size = i_value;
            }
            else
                i_font_size = atoi( value );
        }
        else if( !strcasecmp( "color", name ) )
        {
            if( value[0] == '#' )
            {
                i_font_color = strtol( value + 1, NULL, 16 );
                i_font_color &= 0x00ffffff;
            }
            else
            {
                char *end;
                uint32_t i_value = strtol( value, &end, 16 );
                if( *end == '\0' || *end == ' ' )
                    i_font_color = i_value & 0x00ffffff;

                for( int i = 0; p_html_colors[i].psz_name != NULL; i++ )
                {
                    if( !strncasecmp( value, p_html_colors[i].psz_name, strlen(p_html_colors[i].psz_name) ) )
                    {
                        i_font_color = p_html_colors[i].i_value;
                        break;
                    }
                }
            }
        }
        else if( !strcasecmp( "alpha", name ) && ( value[0] == '#' ) )
        {
            i_font_alpha = strtol( value + 1, NULL, 16 );
            i_font_alpha &= 0xff;
        }
    }
    rv = PushFont( p_fonts,
                   psz_fontname,
                   i_font_size,
                   (i_font_color & 0xffffff) | ((uint32_t)(i_font_alpha & 0xff) << 24),
                   i_karaoke_bg_color );

    free( psz_fontname );

    return rv;
}

int HandleTT(font_stack_t **p_fonts, const char *p_fontfamily )
{
    char      *psz_unused_fontname = NULL;
    uint32_t   i_font_color = 0xffffff;
    uint32_t   i_karaoke_bg_color = 0x00ffffff;
    int        i_font_size  = STYLE_DEFAULT_FONT_SIZE;

    /* Default all attributes to the top font in the stack -- in case not
     * all attributes are specified in the sub-font
     */
    PeekFont( p_fonts,
             &psz_unused_fontname,
             &i_font_size,
             &i_font_color,
             &i_karaoke_bg_color );

    /* Keep all the parent's font attributes, but change to a monospace font */
    return PushFont( p_fonts,
                   p_fontfamily,
                   i_font_size,
                   i_font_color,
                   i_karaoke_bg_color );
}

/* Turn any multiple-whitespaces into single spaces */
void HandleWhiteSpace( char *psz_node )
{
    char *s = strpbrk( psz_node, "\t\r\n " );
    while( s )
    {
        int i_whitespace = strspn( s, "\t\r\n " );

        if( i_whitespace > 1 )
            memmove( &s[1],
                     &s[i_whitespace],
                     strlen( s ) - i_whitespace + 1 );
        *s++ = ' ';

        s = strpbrk( s, "\t\r\n " );
    }
}

unsigned SetupText( filter_t *p_filter,
                           uni_char_t *psz_text_out,
                           text_style_t **pp_styles,
                           uint32_t *pi_k_dates,

                           const char *psz_text_in,
                           text_style_t *p_style,
                           uint32_t i_k_date )
{
    size_t i_string_length;

    size_t i_string_bytes;
    uni_char_t *psz_tmp = ToCharset( FREETYPE_TO_UCS, psz_text_in, &i_string_bytes );
    if( psz_tmp )
    {
        memcpy( psz_text_out, psz_tmp, i_string_bytes );
        i_string_length = i_string_bytes / sizeof( *psz_tmp );
        free( psz_tmp );
    }
    else
    {
        msg_Warn( p_filter, "failed to convert string to unicode (%s)",
                  vlc_strerror_c(errno) );
        i_string_length = 0;
    }

    if( i_string_length > 0 )
    {
        for( unsigned i = 0; i < i_string_length; i++ )
            pp_styles[i] = p_style;
    }
    else
    {
        text_style_Delete( p_style );
    }
    if( i_string_length > 0 && pi_k_dates )
    {
        for( unsigned i = 0; i < i_string_length; i++ )
            pi_k_dates[i] = i_k_date;
    }
    return i_string_length;
}

bool FaceStyleEquals( const text_style_t *p_style1,
                             const text_style_t *p_style2 )
{
    if( !p_style1 || !p_style2 )
        return false;
    if( p_style1 == p_style2 )
        return true;

    const int i_style_mask = STYLE_BOLD | STYLE_ITALIC;
    return (p_style1->i_style_flags & i_style_mask) == (p_style2->i_style_flags & i_style_mask) &&
           !strcmp( p_style1->psz_fontname, p_style2->psz_fontname );
}

text_style_t *GetStyleFromFontStack( filter_t *p_filter,
                                     font_stack_t **p_fonts,
                                     text_style_t *style,
                                     int i_style_flags )
{
    char       *psz_fontname = NULL;
    uint32_t    i_font_color = var_InheritInteger( p_filter, "freetype-color" );
    i_font_color = VLC_CLIP( i_font_color, 0, 0xFFFFFF );
    i_font_color = i_font_color & 0x00ffffff;

    int         i_font_size  = style->i_font_size;
    uint32_t    i_karaoke_bg_color = i_font_color;

    if( PeekFont( p_fonts, &psz_fontname, &i_font_size,
                  &i_font_color, &i_karaoke_bg_color ) )
        return NULL;

    return CreateStyle( psz_fontname, i_font_size, i_font_color,
                        i_karaoke_bg_color,
                        i_style_flags );
}

int ProcessNodes( filter_t *p_filter,
                         uni_char_t *psz_text,
                         text_style_t **pp_styles,
                         uint32_t *pi_k_dates,
                         int *pi_len,
                         xml_reader_t *p_xml_reader,
                         text_style_t *p_font_style,
                         text_style_t *p_default_style )
{
    int           rv      = VLC_SUCCESS;
    int i_text_length     = 0;
    font_stack_t *p_fonts = NULL;
    uint32_t i_k_date     = 0;

    int i_style_flags = 0;

    if( p_font_style )
    {
        /* If the font is not specified in the style, assume the system font */
        if(!p_font_style->psz_fontname)
             p_font_style->psz_fontname = strdup(p_default_style->psz_fontname);

        rv = PushFont( &p_fonts,
               p_font_style->psz_fontname,
               p_font_style->i_font_size > 0 ? p_font_style->i_font_size
                                             : p_default_style->i_font_size,
               (p_font_style->i_font_color & 0xffffff) |
                   ((p_font_style->i_font_alpha & 0xff) << 24),
               (p_font_style->i_karaoke_background_color & 0xffffff) |
                   ((p_font_style->i_karaoke_background_alpha & 0xff) << 24));

        i_style_flags = p_font_style->i_style_flags & (STYLE_BOLD |
                                                       STYLE_ITALIC |
                                                       STYLE_UNDERLINE |
                                                       STYLE_STRIKEOUT);
    }
    else
    {
        uint32_t i_font_size = p_default_style->i_font_size;
        uint32_t i_font_color = var_InheritInteger( p_filter, "freetype-color" );
        i_font_color = VLC_CLIP( i_font_color, 0, 0xFFFFFF );
        int i_font_alpha = p_default_style->i_font_alpha;
        rv = PushFont( &p_fonts,
                       p_default_style->psz_fontname,
                       i_font_size,
                       (i_font_color & 0xffffff) |
                       ((uint32_t)(i_font_alpha & 0xff) << 24),
                       0x00ffffff );
    }
    if( p_default_style->i_style_flags & STYLE_BOLD )
        i_style_flags |= STYLE_BOLD;

    if( rv != VLC_SUCCESS )
        return rv;

    const char *node;
    int type;

    while ( (type = xml_ReaderNextNode( p_xml_reader, &node )) > 0 )
    {
        switch ( type )
        {
            case XML_READER_ENDELEM:
                if( !strcasecmp( "font", node ) )
                    PopFont( &p_fonts );
                else if( !strcasecmp( "tt", node ) )
                    PopFont( &p_fonts );
                else if( !strcasecmp( "b", node ) )
                    i_style_flags &= ~STYLE_BOLD;
               else if( !strcasecmp( "i", node ) )
                    i_style_flags &= ~STYLE_ITALIC;
                else if( !strcasecmp( "u", node ) )
                    i_style_flags &= ~STYLE_UNDERLINE;
                else if( !strcasecmp( "s", node ) )
                    i_style_flags &= ~STYLE_STRIKEOUT;
                break;

            case XML_READER_STARTELEM:
                if( !strcasecmp( "font", node ) )
                    HandleFontAttributes( p_xml_reader, &p_fonts );
                else if( !strcasecmp( "tt", node ) )
                    HandleTT( &p_fonts, p_default_style->psz_monofontname );
                else if( !strcasecmp( "b", node ) )
                    i_style_flags |= STYLE_BOLD;
                else if( !strcasecmp( "i", node ) )
                    i_style_flags |= STYLE_ITALIC;
                else if( !strcasecmp( "u", node ) )
                    i_style_flags |= STYLE_UNDERLINE;
                else if( !strcasecmp( "s", node ) )
                    i_style_flags |= STYLE_STRIKEOUT;
                else if( !strcasecmp( "br", node ) )
                {
                    i_text_length += SetupText( p_filter,
                                                &psz_text[i_text_length],
                                                &pp_styles[i_text_length],
                                                pi_k_dates ? &pi_k_dates[i_text_length] : NULL,
                                                "\n",
                                                GetStyleFromFontStack( p_filter,
                                                                       &p_fonts,
                                                                       p_default_style,
                                                                       i_style_flags ),
                                                i_k_date );
                }
                else if( !strcasecmp( "k", node ) )
                {
                    /* Karaoke tags */
                    const char *name, *value;
                    while( (name = xml_ReaderNextAttr( p_xml_reader, &value )) != NULL )
                    {
                        if( !strcasecmp( "t", name ) && value )
                            i_k_date += atoi( value );
                    }
                }
                break;

            case XML_READER_TEXT:
            {
                char *psz_node = strdup( node );
                if( unlikely(!psz_node) )
                    break;

                HandleWhiteSpace( psz_node );
                resolve_xml_special_chars( psz_node );

                i_text_length += SetupText( p_filter,
                                            &psz_text[i_text_length],
                                            &pp_styles[i_text_length],
                                            pi_k_dates ? &pi_k_dates[i_text_length] : NULL,
                                            psz_node,
                                            GetStyleFromFontStack( p_filter,
                                                                   &p_fonts,
                                                                   p_default_style,
                                                                   i_style_flags ),
                                            i_k_date );
                free( psz_node );
                break;
            }
        }
    }

    *pi_len = i_text_length;

    while( VLC_SUCCESS == PopFont( &p_fonts ) );

    return VLC_SUCCESS;
}


