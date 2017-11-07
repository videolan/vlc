/*****************************************************************************
 * css_style.c : CSS styles conversions
 *****************************************************************************
 * Copyright (C) 2017 VideoLabs, VLC authors and VideoLAN
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

#include "css_parser.h"
#include "css_style.h"

static void Color( vlc_css_term_t term,
                   int *color, uint8_t *alpha,
                   uint16_t *feat, int cflag, int aflag )
{
    if( term.type == TYPE_FUNCTION )
    {
        if( term.function ) /* func( expr ) */
        {
            if( ( !strcmp( term.psz, "rgb" ) && term.function->i_count == 3 ) ||
                ( !strcmp( term.psz, "rgba" ) && term.function->i_count == 4 ) )
            {
                *color = (((int)term.function->seq[0].term.val) << 16) |
                         (((int)term.function->seq[1].term.val) << 8) |
                          ((int)term.function->seq[2].term.val);
                *feat |= cflag;
                if( term.psz[3] != 0 ) /* rgba */
                {
                    *alpha = term.function->seq[3].term.val * STYLE_ALPHA_OPAQUE;
                    *feat |= aflag;
                }
            }
        }
    }
    else if( term.type == TYPE_STRING ||
             term.type == TYPE_HEXCOLOR ||
             term.type == TYPE_IDENTIFIER )
    {
        bool b_valid = false;
        unsigned i_color = vlc_html_color( term.psz, &b_valid );
        if( b_valid )
        {
            *alpha = (i_color & 0xFF000000) >> 24;
            *color = i_color & 0x00FFFFFF;
            *feat |= cflag|aflag;
        }
    }
}

static void OutlineWidth( vlc_css_term_t term, text_style_t *p_style )
{
    if( term.type >= TYPE_PIXELS )
    {
        p_style->i_outline_width = term.val;
        p_style->i_style_flags |= STYLE_OUTLINE;
        p_style->i_features |= STYLE_HAS_FLAGS;
    }
}

static void OutlineColor( vlc_css_term_t term, text_style_t *p_style )
{
    Color( term, &p_style->i_outline_color, &p_style->i_outline_alpha,
           &p_style->i_features, STYLE_HAS_OUTLINE_COLOR, STYLE_HAS_OUTLINE_ALPHA );
}

static void ShadowDrop( vlc_css_term_t term, text_style_t *p_style )
{
    if( term.type >= TYPE_PIXELS )
    {
        p_style->i_shadow_width = term.val;
        p_style->i_style_flags |= STYLE_SHADOW;
        p_style->i_features |= STYLE_HAS_FLAGS;
    }
}

static void ShadowColor( vlc_css_term_t term, text_style_t *p_style )
{
    Color( term, &p_style->i_shadow_color, &p_style->i_shadow_alpha,
           &p_style->i_features, STYLE_HAS_SHADOW_COLOR, STYLE_HAS_SHADOW_ALPHA );
}

void webvtt_FillStyleFromCssDeclaration( const vlc_css_declaration_t *p_decl, text_style_t *p_style )
{
    if( !p_decl->psz_property || !p_style )
        return;

    /* Only support simple expressions for now */
    if( p_decl->expr->i_count < 1 )
        return;

    vlc_css_term_t term0 = p_decl->expr->seq[0].term;

    if( !strcasecmp( p_decl->psz_property, "color" ) )
    {
        Color( term0, &p_style->i_font_color, &p_style->i_font_alpha,
               &p_style->i_features, STYLE_HAS_FONT_COLOR, STYLE_HAS_FONT_ALPHA );
    }
    else if( !strcasecmp( p_decl->psz_property, "text-decoration" ) )
    {
        if( term0.type == TYPE_STRING )
        {
            if( !strcasecmp( term0.psz, "none" ) )
            {
                p_style->i_style_flags &= ~(STYLE_STRIKEOUT|STYLE_UNDERLINE);
                p_style->i_features |= STYLE_HAS_FLAGS;
            }
            else if( !strcasecmp( term0.psz, "line-through" ) )
            {
                p_style->i_style_flags |= STYLE_STRIKEOUT;
                p_style->i_features |= STYLE_HAS_FLAGS;
            }
            else if( !strcasecmp( term0.psz, "underline" ) )
            {
                p_style->i_style_flags |= STYLE_UNDERLINE;
                p_style->i_features |= STYLE_HAS_FLAGS;
            }
        }
    }
    else if( !strcasecmp( p_decl->psz_property, "text-shadow" ) )
    {
        ShadowDrop( term0, p_style );
        if( p_decl->expr->i_count == 3 )
            ShadowColor( p_decl->expr->seq[2].term, p_style );
    }
    else if( !strcasecmp( p_decl->psz_property, "background-color" ) )
    {
        Color( term0, &p_style->i_background_color, &p_style->i_background_alpha,
               &p_style->i_features, STYLE_HAS_BACKGROUND_COLOR, STYLE_HAS_BACKGROUND_ALPHA );
        p_style->i_style_flags |= STYLE_BACKGROUND;
        p_style->i_features |= STYLE_HAS_FLAGS;
    }
    else if( !strcasecmp( p_decl->psz_property, "outline-color" ) )
    {
        OutlineColor( term0, p_style );
    }
    else if( !strcasecmp( p_decl->psz_property, "outline-width" ) )
    {
        OutlineWidth( term0, p_style );
    }
    else if( !strcasecmp( p_decl->psz_property, "outline" ) )
    {
        OutlineWidth( term0, p_style );
        if( p_decl->expr->i_count == 3 )
            OutlineColor( p_decl->expr->seq[2].term, p_style );
    }
    else if( !strcasecmp( p_decl->psz_property, "font-family" ) )
    {
        if( term0.type >= TYPE_STRING )
        {
            char *psz_font = NULL;
            const char *psz = strchr( term0.psz, ',' );
            if( psz )
                psz_font = strndup( term0.psz, psz - term0.psz + 1 );
            else
                psz_font = strdup( term0.psz );
            free( p_style->psz_fontname );
            p_style->psz_fontname = vlc_css_unquoted( psz_font );
            free( psz_font );
        }
    }
    else if( !strcasecmp( p_decl->psz_property, "font-style" ) )
    {
        if( term0.type >= TYPE_STRING )
        {
            if( !strcasecmp(term0.psz, "normal") )
            {
                p_style->i_style_flags &= ~STYLE_ITALIC;
                p_style->i_features |= STYLE_HAS_FLAGS;
            }
            else if( !strcasecmp(term0.psz, "italic") )
            {
                p_style->i_style_flags |= STYLE_ITALIC;
                p_style->i_features |= STYLE_HAS_FLAGS;
            }
        }
    }
    else if( !strcasecmp( p_decl->psz_property, "font-weight" ) )
    {
        if( term0.type >= TYPE_STRING )
        {
            if( !strcasecmp(term0.psz, "normal") )
            {
                p_style->i_style_flags &= ~STYLE_BOLD;
                p_style->i_features |= STYLE_HAS_FLAGS;
            }
            if( !strcasecmp(term0.psz, "bold") )
            {
                p_style->i_style_flags |= STYLE_BOLD;
                p_style->i_features |= STYLE_HAS_FLAGS;
            }
        }
        else if( term0.type == TYPE_NONE )
        {
            if( term0.val >= 700.0 )
                p_style->i_style_flags |= STYLE_BOLD;
            else
                p_style->i_style_flags &= ~STYLE_BOLD;
            p_style->i_features |= STYLE_HAS_FLAGS;
        }
    }
    else if( !strcasecmp( p_decl->psz_property, "font-size" ) )
    {
        if( term0.type == TYPE_PIXELS )
            p_style->i_font_size = term0.val;
        else if( term0.type == TYPE_EMS )
            p_style->f_font_relsize = term0.val * 5.33 / 1.06;
        else if( term0.type == TYPE_PERCENT )
            p_style->f_font_relsize = term0.val * 5.33 / 100;
    }
    else if( !strcasecmp( p_decl->psz_property, "font" ) )
    {
        /* what to do ? */
    }
    else if( !strcasecmp( p_decl->psz_property, "white-space" ) )
    {
        if( term0.type >= TYPE_STRING )
        {
            if( !strcasecmp(term0.psz, "normal" ) )
                p_style->e_wrapinfo = STYLE_WRAP_DEFAULT;
            if( !strcasecmp(term0.psz, "nowrap" ) )
                p_style->e_wrapinfo = STYLE_WRAP_NONE;
        }
    }
}
