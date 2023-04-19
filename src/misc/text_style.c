/*****************************************************************************
 * text_style.c
 *****************************************************************************
 * Copyright (C) 1999-2010 VLC authors and VideoLAN
 * $Id$
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
    p_style->i_karaoke_background_color = 0xffffff;
    p_style->i_karaoke_background_alpha = STYLE_ALPHA_OPAQUE;
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
        MERGE(i_karaoke_background_color, STYLE_HAS_K_BACKGROUND_COLOR);
        MERGE(i_karaoke_background_alpha, STYLE_HAS_K_BACKGROUND_ALPHA);
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

void text_segment_Delete( text_segment_t* segment )
{
    if ( segment != NULL )
    {
        free( segment->psz_text );
        text_style_Delete( segment->style );
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

    if( psz_hex != psz_value || isxdigit(*psz_hex) )
    {
        uint32_t i_value = strtol( psz_hex, &psz_end, 16 );
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
