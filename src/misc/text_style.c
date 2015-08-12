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
    p_style->i_font_alpha = 0xff;
    p_style->i_outline_color = 0x000000;
    p_style->i_outline_alpha = 0xff;
    p_style->i_shadow_color = 0x000000;
    p_style->i_shadow_alpha = 0xff;
    p_style->i_background_color = 0xffffff;
    p_style->i_background_alpha = 0x80;
    p_style->i_karaoke_background_color = 0xffffff;
    p_style->i_karaoke_background_alpha = 0xff;
    p_style->i_outline_width = 1;
    p_style->i_shadow_width = 0;
    p_style->i_spacing = -1;

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

    while( p_src && p_src->p_next ) {
        text_segment_t *p_next = text_segment_New( p_src->psz_text );
        p_src = p_src->p_next;

        if( p_dst == NULL )
            p_dst = p_dst0 = p_next;
        else
            p_dst->p_next = p_next;
    }

    return p_dst0;
}

