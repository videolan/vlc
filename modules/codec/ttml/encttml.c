/*****************************************************************************
 * encttml.c : TTML encoder
 *****************************************************************************
 * Copyright (C) 2018-2024 VideoLabs, VLC authors and VideoLAN
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
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_subpicture.h>

#include "../codec/ttml/ttml.h"

#define HEX_COLOR_MAX 10
static void FillHexColor( uint32_t argb, bool withalpha, char text[HEX_COLOR_MAX] )
{
    if( withalpha )
        snprintf( text, HEX_COLOR_MAX, "#%08x", (argb << 8) | (argb >> 24) );
    else
        snprintf( text, HEX_COLOR_MAX, "#%06x", argb & 0x00FFFFFF );
}

static void AddTextNode( tt_node_t *p_parent, const char *psz_text )
{
    const char *psz = psz_text;
    const char *nl;
    do
    {
        nl = strchr( psz, '\n' );
        if( nl )
        {
            tt_subtextnode_New( p_parent, psz, nl - psz );
            tt_node_New( p_parent, "br", NULL );
            psz += nl - psz + 1;
            if( *psz == '\0' )
                break;
        }
        else
        {
            tt_textnode_New( p_parent, psz );
        }
    } while ( nl );
}

static block_t *Encode( encoder_t *p_enc, subpicture_t *p_spu )
{
    VLC_UNUSED( p_enc );

    if( p_spu == NULL )
        return NULL;

    tt_node_t *p_root = tt_node_New( NULL, "tt", TT_NS );
    if( !p_root )
        return NULL;

    tt_node_AddAttribute( p_root, "xmlns", TT_NS );
    tt_node_AddAttribute( p_root, "xmlns:tts", TT_NS_STYLING );

    tt_node_t *p_body = tt_node_New( p_root, "body", NULL );
    if( !p_body )
    {
        tt_node_RecursiveDelete( p_root );
        return NULL;
    }

    tt_node_t *p_div = tt_node_New( p_body, "div", NULL );
    if( !p_div )
    {
        tt_node_RecursiveDelete( p_root );
        return NULL;
    }

    subpicture_region_t *p_region;
    vlc_spu_regions_foreach(p_region, &p_spu->regions)
    {
        if( !subpicture_region_IsText( p_region ) ||
            p_region->p_text == NULL ||
            p_region->p_text->psz_text == NULL )
            continue;

        tt_node_t *p_par = tt_node_New( p_div, "p", NULL );
        if( !p_par )
            continue;

        if( p_spu->i_start != VLC_TICK_INVALID )
        {
            p_par->timings.begin = tt_time_Create( p_spu->i_start - VLC_TICK_0 );
            if( p_spu->i_stop != VLC_TICK_INVALID &&  p_spu->i_stop > p_spu->i_start )
                p_par->timings.end = tt_time_Create( p_spu->i_stop - VLC_TICK_0 );
            tt_node_AddAttribute( p_par, "begin", "" );
        }

        for( const text_segment_t *p_segment = p_region->p_text;
             p_segment; p_segment = p_segment->p_next )
        {
            if( p_segment->psz_text == NULL )
                continue;

            const text_style_t *style = p_segment->style;
            if( style && style->i_features )
            {
                tt_node_t *p_span = tt_node_New( p_par, "span", NULL );
                if( !p_span )
                    continue;

                if( style->f_font_relsize && p_spu->i_original_picture_height )
                {
                    char fontsize[10];
                    unsigned relem = p_spu->i_original_picture_height * style->f_font_relsize / 16;
                    snprintf( fontsize, 10, "%u%%", relem );
                    tt_node_AddAttribute( p_span, "tts:fontSize", fontsize );
                }
                else if ( style->i_font_size )
                {
                    char fontsize[10];
                    snprintf( fontsize, 10, "%upx", style->i_font_size );
                    tt_node_AddAttribute( p_span, "tts:fontSize", fontsize );
                }

                if( style->psz_fontname )
                    tt_node_AddAttribute( p_span, "tts:fontFamily", style->psz_fontname );

                if( style->i_features & STYLE_HAS_FLAGS )
                {
                    if( style->i_style_flags & STYLE_BOLD )
                        tt_node_AddAttribute( p_span, "tts:fontWeight", "bold" );
                    if( style->i_style_flags & STYLE_ITALIC )
                        tt_node_AddAttribute( p_span, "tts:fontStyle", "italic" );
                    if( style->i_style_flags & STYLE_UNDERLINE )
                        tt_node_AddAttribute( p_span, "tts:textDecoration", "underline" );
                    if( style->i_style_flags & STYLE_STRIKEOUT )
                        tt_node_AddAttribute( p_span, "tts:textDecoration", "lineThrough" );
                    if( style->i_style_flags & STYLE_OUTLINE )
                    {
                        char color[HEX_COLOR_MAX];
                        uint32_t argb = style->i_outline_color;
                        if( style->i_features & STYLE_HAS_OUTLINE_ALPHA )
                            argb |= style->i_outline_alpha << 24;
                        FillHexColor( argb, style->i_features & STYLE_HAS_OUTLINE_ALPHA, color );
                        tt_node_AddAttribute( p_span, "tts:textOutline", color );
                    }
                }

                if( style->i_features & STYLE_HAS_FONT_COLOR )
                {
                    char color[HEX_COLOR_MAX];
                    uint32_t argb = style->i_font_color;
                    if( style->i_features & STYLE_HAS_FONT_ALPHA )
                        argb |= style->i_font_alpha << 24;
                    FillHexColor( argb, style->i_features & STYLE_HAS_FONT_ALPHA, color );
                    tt_node_AddAttribute( p_span, "tts:color", color );
                }

                if( style->i_features & STYLE_HAS_BACKGROUND_COLOR )
                {
                    char color[HEX_COLOR_MAX];
                    uint32_t argb = style->i_background_color;
                    if( style->i_features & STYLE_HAS_BACKGROUND_ALPHA )
                        argb |= style->i_background_alpha << 24;
                    FillHexColor( argb, style->i_features & STYLE_HAS_BACKGROUND_ALPHA, color );
                    tt_node_AddAttribute( p_span, "tts:backgroundColor", color );
                }

                AddTextNode( p_span, p_segment->psz_text );
            }
            else
            {
                AddTextNode( p_par, p_segment->psz_text );
            }
        }
    }

    block_t* p_block = NULL;
    struct vlc_memstream stream;

    if( !vlc_memstream_open( &stream ) )
    {
        tt_time_t playbacktime = tt_time_Create( p_spu->i_start );

        tt_node_ToText( &stream, (tt_basenode_t *)p_root, &playbacktime );
        if( !vlc_memstream_close( &stream ) )
        {
            p_block = block_heap_Alloc( stream.ptr, stream.length );
            if( p_block )
            {
                p_block->i_dts = p_block->i_pts = VLC_TICK_0 + p_spu->i_start;
                if( p_spu->i_stop != VLC_TICK_INVALID && p_spu->i_stop > p_spu->i_start )
                    p_block->i_length = p_spu->i_stop - p_spu->i_start;
            }
        }
    }

    tt_node_RecursiveDelete( p_root );

    return p_block;
}

int tt_OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_TTML )
        return VLC_EGENERIC;

    p_enc->p_sys = NULL;

    static const struct vlc_encoder_operations ops =
        { .encode_sub = Encode };
    p_enc->ops = &ops;

    p_enc->fmt_out.i_cat = SPU_ES;
    return VLC_SUCCESS;
}
