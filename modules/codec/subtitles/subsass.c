/*****************************************************************************
 * subsass.c : ASS/SSA subtitles decoder
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "subsdec.h"


void ParseSSAString( decoder_t *p_dec,
                     char *psz_subtitle,
                     subpicture_t *p_spu_in )
{
    /* We expect MKV formatted SSA:
     * ReadOrder, Layer, Style, CharacterName, MarginL, MarginR,
     * MarginV, Effect, Text */
    decoder_sys_t   *p_sys = p_dec->p_sys;
    subpicture_t    *p_spu = p_spu_in;
    ssa_style_t     *p_style = NULL;
    char            *psz_new_subtitle = NULL;
    char            *psz_buffer_sub = NULL;
    char            *psz_style = NULL;
    char            *psz_style_start = NULL;
    char            *psz_style_end = NULL;
    int             i_text = 0, i_comma = 0, i_strlen = 0, i;
    int             i_margin_l = 0, i_margin_r = 0, i_margin_v = 0;

    psz_buffer_sub = psz_subtitle;

    p_spu->p_region->psz_html = NULL;

    i_comma = 0;
    while( i_comma < 8 && *psz_buffer_sub != '\0' )
    {
        if( *psz_buffer_sub == ',' )
        {
            i_comma++;
            if( i_comma == 2 )
                psz_style_start = &psz_buffer_sub[1];
            else if( i_comma == 3 )
                psz_style_end = &psz_buffer_sub[0];
            else if( i_comma == 4 )
                i_margin_l = (int)strtol( &psz_buffer_sub[1], NULL, 10 );
            else if( i_comma == 5 )
                i_margin_r = (int)strtol( &psz_buffer_sub[1], NULL, 10 );
            else if( i_comma == 6 )
                i_margin_v = (int)strtol( &psz_buffer_sub[1], NULL, 10 );
        }
        psz_buffer_sub++;
    }

    if( *psz_buffer_sub == '\0' && i_comma == 8 )
    {
        msg_Dbg( p_dec, "couldn't find all fields in this SSA line" );
        return;
    }

    psz_new_subtitle = malloc( strlen( psz_buffer_sub ) + 1);
    i_text = 0;
    while( psz_buffer_sub[0] != '\0' )
    {
        if( psz_buffer_sub[0] == '\\' && psz_buffer_sub[1] == 'n' )
        {
            psz_new_subtitle[i_text] = ' ';
            i_text++;
            psz_buffer_sub += 2;
        }
        else if( psz_buffer_sub[0] == '\\' && psz_buffer_sub[1] == 'N' )
        {
            psz_new_subtitle[i_text] = '\n';
            i_text++;
            psz_buffer_sub += 2;
        }
        else if( psz_buffer_sub[0] == '{' )
        {
            /* { } Control code in SSA. Cannot be escaped */
            while( psz_buffer_sub[0] != '\0' &&
                   psz_buffer_sub[0] != '}' )
            {
                psz_buffer_sub++;
            }
            if( psz_buffer_sub[0] == '}' ) psz_buffer_sub++;
        }
        else
        {
            psz_new_subtitle[i_text] = psz_buffer_sub[0];
            i_text++;
            psz_buffer_sub++;
        }
    }
    psz_new_subtitle[i_text] = '\0';

    i_strlen = __MAX( psz_style_end - psz_style_start, 0);
    psz_style = strndup( psz_style_start, i_strlen );

    for( i = 0; i < p_sys->i_ssa_styles; i++ )
    {
        if( !strcmp( p_sys->pp_ssa_styles[i]->psz_stylename, psz_style ) )
            p_style = p_sys->pp_ssa_styles[i];
    }
    free( psz_style );

    p_spu->p_region->psz_text = psz_new_subtitle;
    if( p_style == NULL )
    {
        p_spu->p_region->i_align = SUBPICTURE_ALIGN_BOTTOM | p_sys->i_align;
        p_spu->p_region->i_x = p_sys->i_align ? 20 : 0;
        p_spu->p_region->i_y = 10;
    }
    else
    {
        msg_Dbg( p_dec, "style is: %s", p_style->psz_stylename);
        p_spu->p_region->p_style = &p_style->font_style;
        p_spu->p_region->i_align = p_style->i_align;
        if( p_style->i_align & SUBPICTURE_ALIGN_LEFT )
        {
            p_spu->p_region->i_x = (i_margin_l) ? i_margin_l : p_style->i_margin_h;
        }
        else if( p_style->i_align & SUBPICTURE_ALIGN_RIGHT )
        {
            p_spu->p_region->i_x = (i_margin_r) ? i_margin_r : p_style->i_margin_h;
        }
        p_spu->p_region->i_y = (i_margin_v) ? i_margin_v : p_style->i_margin_v;
    }
}

/*****************************************************************************
 * ParseColor: SSA stores color in BBGGRR, in ASS it uses AABBGGRR
 * The string value in the string can be a pure integer, or hexadecimal &HBBGGRR
 *****************************************************************************/
static void ParseColor( char *psz_color, int *pi_color, int *pi_alpha )
{
    int i_color = 0;
    if( !strncasecmp( psz_color, "&H", 2 ) )
    {
        /* textual HEX representation */
        i_color = (int) strtol( psz_color+2, NULL, 16 );
    }
    else i_color = (int) strtol( psz_color, NULL, 0 );

    *pi_color = 0;
    *pi_color |= ( ( i_color & 0x000000FF ) << 16 ); /* Red */
    *pi_color |= ( ( i_color & 0x0000FF00 ) );       /* Green */
    *pi_color |= ( ( i_color & 0x00FF0000 ) >> 16 ); /* Blue */

    if( pi_alpha != NULL )
        *pi_alpha = ( i_color & 0xFF000000 ) >> 24;
}

/*****************************************************************************
 * ParseSSAHeader: Retrieve global formatting information etc
 *****************************************************************************/
void ParseSSAHeader( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    char *psz_parser = NULL;
    char *psz_header = malloc( p_dec->fmt_in.i_extra+1 );
    int i_section_type = 1;

    memcpy( psz_header, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra );
    psz_header[ p_dec->fmt_in.i_extra] = '\0';

    /* Handle [Script Info] section */
    psz_parser = strcasestr( psz_header, "[Script Info]" );
    if( psz_parser == NULL ) goto eof;

    psz_parser = GotoNextLine( psz_parser );

    while( psz_parser[0] != '\0' )
    {
        int temp;
        char buffer_text[MAX_LINE + 1];

        if( psz_parser[0] == '!' || psz_parser[0] == ';' ) /* comment */;
        else if( sscanf( psz_parser, "PlayResX: %d", &temp ) == 1 )
            p_sys->i_original_width = ( temp > 0 ) ? temp : -1;
        else if( sscanf( psz_parser, "PlayResY: %d", &temp ) == 1 )
            p_sys->i_original_height = ( temp > 0 ) ? temp : -1;
        else if( sscanf( psz_parser, "Script Type: %8192s", buffer_text ) == 1 )
        {
            if( !strcasecmp( buffer_text, "V4.00+" ) ) p_sys->b_ass = true;
        }
        else if( !strncasecmp( psz_parser, "[V4 Styles]", 11 ) )
            i_section_type = 1;
        else if( !strncasecmp( psz_parser, "[V4+ Styles]", 12) )
        {
            i_section_type = 2;
            p_sys->b_ass = true;
        }
        else if( !strncasecmp( psz_parser, "[Events]", 8 ) )
            i_section_type = 4;
        else if( !strncasecmp( psz_parser, "Style:", 6 ) )
        {
            int i_font_size, i_bold, i_italic, i_border, i_outline, i_shadow,
                i_underline, i_strikeout, i_scale_x, i_scale_y, i_spacing,
                i_align, i_margin_l, i_margin_r, i_margin_v;

            char psz_temp_stylename[MAX_LINE+1];
            char psz_temp_fontname[MAX_LINE+1];
            char psz_temp_color1[MAX_LINE+1];
            char psz_temp_color2[MAX_LINE+1];
            char psz_temp_color3[MAX_LINE+1];
            char psz_temp_color4[MAX_LINE+1];

            if( i_section_type == 1 ) /* V4 */
            {
                if( sscanf( psz_parser, "Style: %8192[^,],%8192[^,],%d,%8192[^,],%8192[^,],%8192[^,],%8192[^,],%d,%d,%d,%d,%d,%d,%d,%d,%d%*[^\r\n]",
                    psz_temp_stylename, psz_temp_fontname, &i_font_size,
                    psz_temp_color1, psz_temp_color2, psz_temp_color3,
                    psz_temp_color4, &i_bold, &i_italic,
                    &i_border, &i_outline, &i_shadow, &i_align, &i_margin_l,
                    &i_margin_r, &i_margin_v ) == 16 )
                {
                    ssa_style_t *p_style = malloc( sizeof(ssa_style_t) );

                    p_style->psz_stylename = strdup( psz_temp_stylename );
                    p_style->font_style.psz_fontname = strdup( psz_temp_fontname );
                    p_style->font_style.i_font_size = i_font_size;

                    ParseColor( psz_temp_color1, &p_style->font_style.i_font_color, NULL );
                    ParseColor( psz_temp_color4, &p_style->font_style.i_shadow_color, NULL );
                    p_style->font_style.i_outline_color = p_style->font_style.i_shadow_color;
                    p_style->font_style.i_font_alpha = p_style->font_style.i_outline_alpha
                                                     = p_style->font_style.i_shadow_alpha = 0x00;
                    p_style->font_style.i_style_flags = 0;
                    if( i_bold ) p_style->font_style.i_style_flags |= STYLE_BOLD;
                    if( i_italic ) p_style->font_style.i_style_flags |= STYLE_ITALIC;

                    if( i_border == 1 )
                        p_style->font_style.i_style_flags |= (STYLE_ITALIC | STYLE_OUTLINE);
                    else if( i_border == 3 )
                    {
                        p_style->font_style.i_style_flags |= STYLE_BACKGROUND;
                        p_style->font_style.i_background_color = p_style->font_style.i_shadow_color;
                        p_style->font_style.i_background_alpha = p_style->font_style.i_shadow_alpha;
                    }
                    p_style->font_style.i_shadow_width = i_shadow;
                    p_style->font_style.i_outline_width = i_outline;

                    p_style->i_align = 0;
                    if( i_align == 1 || i_align == 5 || i_align == 9 )
                        p_style->i_align |= SUBPICTURE_ALIGN_LEFT;
                    if( i_align == 3 || i_align == 7 || i_align == 11 )
                        p_style->i_align |= SUBPICTURE_ALIGN_RIGHT;
                    if( i_align < 4 )
                        p_style->i_align |= SUBPICTURE_ALIGN_BOTTOM;
                    else if( i_align < 8 )
                        p_style->i_align |= SUBPICTURE_ALIGN_TOP;

                    p_style->i_margin_h = ( p_style->i_align & SUBPICTURE_ALIGN_RIGHT ) ?
                                                        i_margin_r : i_margin_l;
                    p_style->i_margin_v = i_margin_v;
                    p_style->i_margin_percent_h = 0;
                    p_style->i_margin_percent_v = 0;

                    p_style->font_style.i_karaoke_background_color = 0xffffff;
                    p_style->font_style.i_karaoke_background_alpha = 0xff;

                    TAB_APPEND( p_sys->i_ssa_styles, p_sys->pp_ssa_styles, p_style );
                }
                else msg_Warn( p_dec, "SSA v4 styleline parsing failed" );
            }
            else if( i_section_type == 2 ) /* V4+ */
            {
                /* Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour,
                   Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline,
                   Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
                */
                if( sscanf( psz_parser, "Style: %8192[^,],%8192[^,],%d,%8192[^,],%8192[^,],%8192[^,],%8192[^,],%d,%d,%d,%d,%d,%d,%d,%*f,%d,%d,%d,%d,%d,%d,%d%*[^\r\n]",
                    psz_temp_stylename, psz_temp_fontname, &i_font_size,
                    psz_temp_color1, psz_temp_color2, psz_temp_color3, psz_temp_color4, &i_bold, &i_italic,
                    &i_underline, &i_strikeout, &i_scale_x, &i_scale_y, &i_spacing, &i_border, &i_outline,
                    &i_shadow, &i_align, &i_margin_l, &i_margin_r, &i_margin_v ) == 21 )
                {
                    ssa_style_t *p_style = malloc( sizeof(ssa_style_t) );

                    p_style->psz_stylename = strdup( psz_temp_stylename );
                    p_style->font_style.psz_fontname = strdup( psz_temp_fontname );
                    p_style->font_style.i_font_size = i_font_size;
                    ParseColor( psz_temp_color1, &p_style->font_style.i_font_color,
                                &p_style->font_style.i_font_alpha );
                    ParseColor( psz_temp_color3, &p_style->font_style.i_outline_color,
                                &p_style->font_style.i_outline_alpha );
                    ParseColor( psz_temp_color4, &p_style->font_style.i_shadow_color,
                                &p_style->font_style.i_shadow_alpha );

                    p_style->font_style.i_style_flags = 0;
                    if( i_bold ) p_style->font_style.i_style_flags |= STYLE_BOLD;
                    if( i_italic ) p_style->font_style.i_style_flags |= STYLE_ITALIC;
                    if( i_underline ) p_style->font_style.i_style_flags |= STYLE_UNDERLINE;
                    if( i_strikeout ) p_style->font_style.i_style_flags |= STYLE_STRIKEOUT;
                    if( i_border == 1 ) p_style->font_style.i_style_flags |= (STYLE_ITALIC | STYLE_OUTLINE);
                    else if( i_border == 3 )
                    {
                        p_style->font_style.i_style_flags |= STYLE_BACKGROUND;
                        p_style->font_style.i_background_color = p_style->font_style.i_shadow_color;
                        p_style->font_style.i_background_alpha = p_style->font_style.i_shadow_alpha;
                    }
                    p_style->font_style.i_shadow_width  = ( i_border == 1 ) ? i_shadow : 0;
                    p_style->font_style.i_outline_width = ( i_border == 1 ) ? i_outline : 0;
                    p_style->font_style.i_spacing = i_spacing;
                    //p_style->font_style.f_angle = f_angle;

                    p_style->i_align = 0;
                    if( i_align == 0x1 || i_align == 0x4 || i_align == 0x7 )
                        p_style->i_align |= SUBPICTURE_ALIGN_LEFT;
                    if( i_align == 0x3 || i_align == 0x6 || i_align == 0x9 )
                        p_style->i_align |= SUBPICTURE_ALIGN_RIGHT;
                    if( i_align == 0x7 || i_align == 0x8 || i_align == 0x9 )
                        p_style->i_align |= SUBPICTURE_ALIGN_TOP;
                    if( i_align == 0x1 || i_align == 0x2 || i_align == 0x3 )
                        p_style->i_align |= SUBPICTURE_ALIGN_BOTTOM;
                    p_style->i_margin_h = ( p_style->i_align & SUBPICTURE_ALIGN_RIGHT ) ?
                                            i_margin_r : i_margin_l;
                    p_style->i_margin_v = i_margin_v;
                    p_style->i_margin_percent_h = 0;
                    p_style->i_margin_percent_v = 0;

                    p_style->font_style.i_karaoke_background_color = 0xffffff;
                    p_style->font_style.i_karaoke_background_alpha = 0xff;

                    /*TODO: Ignored: angle i_scale_x|y (fontscaling), i_encoding */
                    TAB_APPEND( p_sys->i_ssa_styles, p_sys->pp_ssa_styles, p_style );
                }
                else msg_Dbg( p_dec, "SSA V4+ styleline parsing failed" );
            }
        }
        psz_parser = GotoNextLine( psz_parser );
    }

eof:
    free( psz_header );
    return;
}


