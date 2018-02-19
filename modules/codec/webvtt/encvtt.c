/*****************************************************************************
 * encvtt.c: Encoder for WEBVTT as ISO1446-30 payload
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VLC authors and VideoLAN
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
#include <vlc_boxes.h>
#include <vlc_charset.h>
#include "webvtt.h"

static block_t *Encode ( encoder_t *, subpicture_t * );

int webvtt_OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_WEBVTT )
        return VLC_EGENERIC;

    p_enc->p_sys = NULL;

    p_enc->pf_encode_sub = Encode;
    p_enc->fmt_out.i_cat = SPU_ES;
    return VLC_SUCCESS;
}


void webvtt_CloseEncoder( vlc_object_t *p_this )
{
    (void)p_this;
}

static void WriteText( const char *psz, bo_t *box, char *c_last )
{
    /* We need to break any double newline sequence
     * in or over segments */
    while(*psz)
    {
        const char *p = strchr( psz, '\n' );
        if( p )
        {
            bo_add_mem( box, p - psz, psz );
            if( *c_last == '\n' )
                bo_add_8( box, '!' ); /* Add space */
            bo_add_8( box, '\n' );
            *c_last = '\n';
            psz = p + 1;
        }
        else
        {
            size_t len = strlen(psz);
            bo_add_mem( box, len, psz );
            *c_last = (len > 0) ? psz[len - 1] : '\0';
            break;
        }
    }
}

static block_t *Encode( encoder_t *p_enc, subpicture_t *p_spu )
{
    VLC_UNUSED( p_enc );

    if( p_spu == NULL )
        return NULL;

    bo_t box;
    if( !bo_init( &box, 8 ) )
        return NULL;

    for( subpicture_region_t *p_region = p_spu->p_region;
                              p_region; p_region = p_region->p_next )
    {
        if( p_region->fmt.i_chroma != VLC_CODEC_TEXT ||
            p_region->p_text == NULL ||
            p_region->p_text->psz_text == NULL )
            continue;

        size_t i_offset = bo_size( &box );

        bo_add_32be( &box, 0 );
        bo_add_fourcc( &box, "vttc" );

        /* Payload */

        bo_add_32be( &box, 0 );
        bo_add_fourcc( &box, "payl" );

        char prevchar = '\0';
        /* This should already be UTF-8 encoded, so not much effort... */
        for( const text_segment_t *p_segment = p_region->p_text;
             p_segment; p_segment = p_segment->p_next )
        {
            if( p_segment->psz_text == NULL )
                continue;

            if( p_segment->p_ruby )
            {
                bo_add_mem( &box, 6, "<ruby>" );
                for( const text_segment_ruby_t *p_ruby = p_segment->p_ruby;
                                                p_ruby; p_ruby = p_ruby->p_next )
                {
                    WriteText( p_ruby->psz_base, &box, &prevchar );
                    bo_add_mem( &box, 4, "<rt>" );
                    WriteText( p_ruby->psz_rt, &box, &prevchar );
                    bo_add_mem( &box, 5, "</rt>" );
                }
                bo_add_mem( &box, 7, "</ruby>" );
                continue;
            }

            const text_style_t *style = p_segment->style;
            if( style && style->i_features )
            {
                if( style->i_features & STYLE_HAS_FLAGS )
                {
                    if( style->i_style_flags & STYLE_BOLD )
                        bo_add_mem( &box, 3, "<b>" );
                    if( style->i_style_flags & STYLE_UNDERLINE )
                        bo_add_mem( &box, 3, "<u>" );
                    if( style->i_style_flags & STYLE_ITALIC )
                        bo_add_mem( &box, 3, "<i>" );
                }
            }

            WriteText( p_segment->psz_text, &box, &prevchar );

            if( style && style->i_features )
            {
                if( style->i_features & STYLE_HAS_FLAGS )
                {
                    if( style->i_style_flags & STYLE_BOLD )
                        bo_add_mem( &box, 4, "</b>" );
                    if( style->i_style_flags & STYLE_UNDERLINE )
                        bo_add_mem( &box, 4, "</u>" );
                    if( style->i_style_flags & STYLE_ITALIC )
                        bo_add_mem( &box, 4, "</i>" );
                }
            }
        }

        bo_set_32be( &box, i_offset + 8, bo_size( &box ) - i_offset - 8 );

        /* Settings */

        if( (p_region->i_text_align & (SUBPICTURE_ALIGN_LEFT|SUBPICTURE_ALIGN_RIGHT)) ||
                (p_region->i_align & SUBPICTURE_ALIGN_TOP) )
        {
            size_t i_start = bo_size( &box );

            bo_add_32be( &box, 0 );
            bo_add_fourcc( &box, "sttg" );

            if( p_region->i_text_align & SUBPICTURE_ALIGN_LEFT )
                bo_add_mem( &box, 10, "align:left" );
            else if( p_region->i_text_align & SUBPICTURE_ALIGN_RIGHT )
                bo_add_mem( &box, 11, "align:right" );

            if( p_region->i_align & SUBPICTURE_ALIGN_TOP )
            {
                float offset = 100.0;
                if( p_spu->i_original_picture_height > 0 )
                    offset = offset * p_region->i_y / p_spu->i_original_picture_height;
                if( bo_size( &box ) != i_start + 8 )
                    bo_add_8( &box, ' ' );
                char *psz;
                int i_printed = us_asprintf( &psz, "line:%2.2f%%", offset );
                if( i_printed >= 0 )
                {
                    if( i_printed > 0 )
                        bo_add_mem( &box, i_printed, psz );
                    free( psz );
                }
            }
            bo_set_32be( &box, i_start, bo_size( &box ) - i_start );
        }


        bo_set_32be( &box, i_offset, bo_size( &box ) - i_offset );
    }

    if( bo_size( &box ) == 0 ) /* No cue */
    {
        bo_add_32be( &box, 8 );
        bo_add_fourcc( &box, "vtte" );
    }

    block_t *p_block = box.b;
    box.b = NULL;
    bo_deinit( &box );

    if( p_block )
    {
        p_block->i_pts = p_block->i_dts = p_spu->i_start;
        if( p_spu->i_stop > p_spu->i_start )
            p_block->i_length = p_spu->i_stop - p_spu->i_start;
    }

    return p_block;
}
