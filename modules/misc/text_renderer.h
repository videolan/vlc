/*****************************************************************************
 * text_renderer.h: common text renderer code
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Bernie Purcell <bitmap@videolan.org>
 *          Laurent Aimar < fenrir AT videolan DOT org >
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

typedef struct font_stack_t font_stack_t;
struct font_stack_t
{
    char          *psz_name;
    int            i_size;
    uint32_t       i_color;            /* ARGB */
    uint32_t       i_karaoke_bg_color; /* ARGB */

    font_stack_t  *p_next;
};

static void SetupLine( filter_t *p_filter, const char *psz_text_in,
                       UCHAR **psz_text_out, uint32_t *pi_runs,
                       uint32_t **ppi_run_lengths, TR_FONT_STYLE_PTR **ppp_styles,
                       TR_FONT_STYLE_PTR p_style );

static TR_FONT_STYLE_PTR GetStyleFromFontStack( filter_sys_t *p_sys,
                                          font_stack_t **p_fonts, bool b_bold, bool b_italic,
                                          bool b_uline );

static int PushFont( font_stack_t **p_font, const char *psz_name, int i_size,
                     uint32_t i_color, uint32_t i_karaoke_bg_color )
{
    font_stack_t *p_new;

    if( !p_font )
        return VLC_EGENERIC;

    p_new = malloc( sizeof( font_stack_t ) );
    if( ! p_new )
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

static int PopFont( font_stack_t **p_font )
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

static int PeekFont( font_stack_t **p_font, char **psz_name, int *i_size,
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

static int HandleFontAttributes( xml_reader_t *p_xml_reader,
                                  font_stack_t **p_fonts, int i_scale )
{
    int        rv;
    char      *psz_fontname = NULL;
    uint32_t   i_font_color = 0xffffff;
    int        i_font_alpha = 0;
    uint32_t   i_karaoke_bg_color = 0x00ffffff;
    int        i_font_size  = 24;

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
        i_font_size = i_font_size * 1000 / i_scale;
    }
    i_font_alpha = (i_font_color >> 24) & 0xff;
    i_font_color &= 0x00ffffff;

    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
    {
        char *psz_name = xml_ReaderName( p_xml_reader );
        char *psz_value = xml_ReaderValue( p_xml_reader );

        if( psz_name && psz_value )
        {
            if( !strcasecmp( "face", psz_name ) )
            {
                free( psz_fontname );
                psz_fontname = strdup( psz_value );
            }
            else if( !strcasecmp( "size", psz_name ) )
            {
                if( ( *psz_value == '+' ) || ( *psz_value == '-' ) )
                {
                    int i_value = atoi( psz_value );

                    if( ( i_value >= -5 ) && ( i_value <= 5 ) )
                        i_font_size += ( i_value * i_font_size ) / 10;
                    else if( i_value < -5 )
                        i_font_size = - i_value;
                    else if( i_value > 5 )
                        i_font_size = i_value;
                }
                else
                    i_font_size = atoi( psz_value );
            }
            else if( !strcasecmp( "color", psz_name ) )
            {
                if( psz_value[0] == '#' )
                {
                    i_font_color = strtol( psz_value + 1, NULL, 16 );
                    i_font_color &= 0x00ffffff;
                }
                else
                {
                    for( int i = 0; p_html_colors[i].psz_name != NULL; i++ )
                    {
                        if( !strncasecmp( psz_value, p_html_colors[i].psz_name, strlen(p_html_colors[i].psz_name) ) )
                        {
                            i_font_color = p_html_colors[i].i_value;
                            break;
                        }
                    }
                }
            }
            else if( !strcasecmp( "alpha", psz_name ) &&
                     ( psz_value[0] == '#' ) )
            {
                i_font_alpha = strtol( psz_value + 1, NULL, 16 );
                i_font_alpha &= 0xff;
            }
        }
        free( psz_name );
        free( psz_value );
    }
    rv = PushFont( p_fonts,
                   psz_fontname,
                   i_font_size * i_scale / 1000,
                   (i_font_color & 0xffffff) | ((i_font_alpha & 0xff) << 24),
                   i_karaoke_bg_color );

    free( psz_fontname );

    return rv;
}

static void SetKaraokeLen( uint32_t i_runs, uint32_t *pi_run_lengths,
                           uint32_t i_k_runs, uint32_t *pi_k_run_lengths )
{
    /* Karaoke tags _PRECEDE_ the text they specify a duration
     * for, therefore we are working out the length for the
     * previous tag, and first time through we have nothing
     */
    if( pi_k_run_lengths )
    {
        int i_chars = 0;
        uint32_t i;

        /* Work out how many characters are presently in the string
         */
        for( i = 0; i < i_runs; i++ )
            i_chars += pi_run_lengths[ i ];

        /* Subtract away those we've already allocated to other
         * karaoke tags
         */
        for( i = 0; i < i_k_runs; i++ )
            i_chars -= pi_k_run_lengths[ i ];

        pi_k_run_lengths[ i_k_runs - 1 ] = i_chars;
    }
}

static void SetupKaraoke( xml_reader_t *p_xml_reader, uint32_t *pi_k_runs,
                          uint32_t **ppi_k_run_lengths,
                          uint32_t **ppi_k_durations )
{
    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
    {
        char *psz_name = xml_ReaderName( p_xml_reader );
        char *psz_value = xml_ReaderValue( p_xml_reader );

        if( psz_name && psz_value &&
            !strcasecmp( "t", psz_name ) )
        {
            if( ppi_k_durations && ppi_k_run_lengths )
            {
                (*pi_k_runs)++;

                if( *ppi_k_durations )
                {
                    *ppi_k_durations = (uint32_t *)
                        realloc( *ppi_k_durations,
                                 *pi_k_runs * sizeof( uint32_t ) );
                }
                else if( *pi_k_runs == 1 )
                {
                    *ppi_k_durations = (uint32_t *)
                        malloc( *pi_k_runs * sizeof( uint32_t ) );
                }

                if( *ppi_k_run_lengths )
                {
                    *ppi_k_run_lengths = (uint32_t *)
                        realloc( *ppi_k_run_lengths,
                                 *pi_k_runs * sizeof( uint32_t ) );
                }
                else if( *pi_k_runs == 1 )
                {
                    *ppi_k_run_lengths = (uint32_t *)
                        malloc( *pi_k_runs * sizeof( uint32_t ) );
                }
                if( *ppi_k_durations )
                    (*ppi_k_durations)[ *pi_k_runs - 1 ] = atoi( psz_value );

                if( *ppi_k_run_lengths )
                    (*ppi_k_run_lengths)[ *pi_k_runs - 1 ] = 0;
            }
        }
        free( psz_name );
        free( psz_value );
    }
}

/* Turn any multiple-whitespaces into single spaces */
static void HandleWhiteSpace( char *psz_node )
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

/* */
static int ProcessNodes( filter_t *p_filter,
                         xml_reader_t *p_xml_reader,
                         text_style_t *p_font_style,
                         UCHAR *psz_text,
                         int *pi_len,

                         uint32_t *pi_runs,
                         uint32_t **ppi_run_lengths,
                         TR_FONT_STYLE_PTR **ppp_styles,

                         bool b_karaoke,
                         uint32_t *pi_k_runs,
                         uint32_t **ppi_k_run_lengths,
                         uint32_t **ppi_k_durations )
{
    int           rv             = VLC_SUCCESS;
    filter_sys_t *p_sys          = p_filter->p_sys;
    UCHAR        *psz_text_orig  = psz_text;
    font_stack_t *p_fonts        = NULL;
    vlc_value_t   val;
    int           i_scale        = 1000;

    char *psz_node  = NULL;

    bool b_italic = false;
    bool b_bold   = false;
    bool b_uline  = false;

    if( VLC_SUCCESS == var_Get( p_filter, "scale", &val ))
        i_scale = val.i_int;

    if( p_font_style )
    {
        rv = PushFont( &p_fonts,
               p_font_style->psz_fontname,
               p_font_style->i_font_size * i_scale / 1000,
               (p_font_style->i_font_color & 0xffffff) |
                   ((p_font_style->i_font_alpha & 0xff) << 24),
               (p_font_style->i_karaoke_background_color & 0xffffff) |
                   ((p_font_style->i_karaoke_background_alpha & 0xff) << 24));

        if( p_font_style->i_style_flags & STYLE_BOLD )
            b_bold = true;
        if( p_font_style->i_style_flags & STYLE_ITALIC )
            b_italic = true;
        if( p_font_style->i_style_flags & STYLE_UNDERLINE )
            b_uline = true;
    }
    else
    {
        rv = PushFont( &p_fonts,
                       TR_DEFAULT_FONT,
                       p_sys->i_font_size,
                       (p_sys->i_font_color & 0xffffff) |
                          (((255-p_sys->i_font_opacity) & 0xff) << 24),
                       0x00ffffff );
    }
    if( rv != VLC_SUCCESS )
        return rv;

    while ( ( xml_ReaderRead( p_xml_reader ) == 1 ) )
    {
        switch ( xml_ReaderNodeType( p_xml_reader ) )
        {
            case XML_READER_NONE:
                break;
            case XML_READER_ENDELEM:
                psz_node = xml_ReaderName( p_xml_reader );
                if( psz_node )
                {
                    if( !strcasecmp( "font", psz_node ) )
                        PopFont( &p_fonts );
                    else if( !strcasecmp( "b", psz_node ) )
                        b_bold   = false;
                    else if( !strcasecmp( "i", psz_node ) )
                        b_italic = false;
                    else if( !strcasecmp( "u", psz_node ) )
                        b_uline  = false;

                    free( psz_node );
                }
                break;
            case XML_READER_STARTELEM:
                psz_node = xml_ReaderName( p_xml_reader );
                if( psz_node )
                {
                    if( !strcasecmp( "font", psz_node ) )
                        rv = HandleFontAttributes( p_xml_reader, &p_fonts, i_scale );
                    else if( !strcasecmp( "b", psz_node ) )
                        b_bold = true;
                    else if( !strcasecmp( "i", psz_node ) )
                        b_italic = true;
                    else if( !strcasecmp( "u", psz_node ) )
                        b_uline = true;
                    else if( !strcasecmp( "br", psz_node ) )
                    {
                        SetupLine( p_filter, "\n", &psz_text,
                                   pi_runs, ppi_run_lengths, ppp_styles,
                                   GetStyleFromFontStack( p_sys,
                                                          &p_fonts,
                                                          b_bold,
                                                          b_italic,
                                                          b_uline ) );
                    }
                    else if( !strcasecmp( "k", psz_node ) )
                    {
                        /* Only valid in karaoke */
                        if( b_karaoke )
                        {
                            if( *pi_k_runs > 0 )
                            {
                                SetKaraokeLen( *pi_runs, *ppi_run_lengths,
                                               *pi_k_runs, *ppi_k_run_lengths );
                            }
                            SetupKaraoke( p_xml_reader, pi_k_runs,
                                          ppi_k_run_lengths, ppi_k_durations );
                        }
                    }

                    free( psz_node );
                }
                break;
            case XML_READER_TEXT:
                psz_node = xml_ReaderValue( p_xml_reader );
                if( psz_node )
                {
                    /* */
                    HandleWhiteSpace( psz_node );
                    resolve_xml_special_chars( psz_node );

                    SetupLine( p_filter, psz_node, &psz_text,
                               pi_runs, ppi_run_lengths, ppp_styles,
                               GetStyleFromFontStack( p_sys,
                                                      &p_fonts,
                                                      b_bold,
                                                      b_italic,
                                                      b_uline ) );
                    free( psz_node );
                }
                break;
        }
        if( rv != VLC_SUCCESS )
        {
            psz_text = psz_text_orig;
            break;
        }
    }
    if( b_karaoke )
    {
        SetKaraokeLen( *pi_runs, *ppi_run_lengths,
                       *pi_k_runs, *ppi_k_run_lengths );
    }

    *pi_len = psz_text - psz_text_orig;

    while( VLC_SUCCESS == PopFont( &p_fonts ) );

    return rv;
}
