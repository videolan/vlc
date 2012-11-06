/*****************************************************************************
 * mpls.c: BluRay Disc MPLS
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <limits.h>

#include <vlc_common.h>
#include <vlc_bits.h>
#include "mpls.h"


/* MPLS */
void bd_mpls_stream_Parse( bd_mpls_stream_t *p_stream, bs_t *s, int i_class )
{
    /* Stream entry parsing */
    const int i_entry_length = bs_read( s, 8 );
    const int i_entry_start = bs_pos( s ) / 8;

    p_stream->i_type = bs_read( s, 8 );
    p_stream->i_class = i_class;
    if( p_stream->i_type == BD_MPLS_STREAM_TYPE_PLAY_ITEM )
    {
        p_stream->play_item.i_pid = bs_read( s, 16 );
    }
    else if( p_stream->i_type == BD_MPLS_STREAM_TYPE_SUB_PATH )
    {
        p_stream->sub_path.i_sub_path_id = bs_read( s, 8 );
        p_stream->sub_path.i_sub_clip_id = bs_read( s, 8 );
        p_stream->sub_path.i_pid = bs_read( s, 16 );
    }
    else if( p_stream->i_type == BD_MPLS_STREAM_TYPE_IN_MUX_SUB_PATH )
    {
        p_stream->in_mux_sub_path.i_sub_path_id = bs_read( s, 8 );
        p_stream->in_mux_sub_path.i_pid = bs_read( s, 16 );
    }
    bs_skip( s, 8 * ( i_entry_start + i_entry_length ) - bs_pos( s ) );

    /* Stream attributes parsing */
    const int i_attributes_length = bs_read( s, 8 );
    const int i_attributes_start = bs_pos( s ) / 8;

    p_stream->i_stream_type = bs_read( s, 8 );
    strcpy( p_stream->psz_language, "" );
    p_stream->i_charset = -1;

    if( p_stream->i_stream_type == 0x02 || /* MPEG-I/II */
        p_stream->i_stream_type == 0x1b || /* AVC */
        p_stream->i_stream_type == 0xea )  /* VC-1 */
    {
        /* Video */
    }
    else if( ( p_stream->i_stream_type >= 0x80 && p_stream->i_stream_type <= 0x8f ) ||
             ( p_stream->i_stream_type >= 0xa0 && p_stream->i_stream_type <= 0xaf ) )
    {
        /* Audio */
        bs_skip( s, 4 );
        bs_skip( s, 4 );
        for( int i = 0; i < 3; i++ )
            p_stream->psz_language[i] = bs_read( s, 8 );
        p_stream->psz_language[3] = '\0';
    }
    else if( p_stream->i_stream_type == 0x90 ||   /* PG stream */
             p_stream->i_stream_type == 0x91 )    /* IG stream */
    {
        for( int i = 0; i < 3; i++ )
            p_stream->psz_language[i] = bs_read( s, 8 );
        p_stream->psz_language[3] = '\0';
    }
    else if( p_stream->i_stream_type == 0x92 )    /* Text stream */
    {
        p_stream->i_charset = bs_read( s, 8 );
        for( int i = 0; i < 3; i++ )
            p_stream->psz_language[i] = bs_read( s, 8 );
        p_stream->psz_language[3] = '\0';
    }

    bs_skip( s, 8 * ( i_attributes_start + i_attributes_length ) - bs_pos( s ) );
}

void bd_mpls_play_item_Clean( bd_mpls_play_item_t *p_item )
{
    free( p_item->p_clpi );
    free( p_item->p_stream );
}
void bd_mpls_play_item_Parse( bd_mpls_play_item_t *p_item, bs_t *s )
{
    const int i_length = bs_read( s, 16 );
    const int i_start = bs_pos( s ) / 8;

    char psz_name[5+1];
    for( int j = 0; j < 5; j++ )
        psz_name[j] = bs_read( s, 8 );
    psz_name[5] = '\0';

    p_item->clpi.i_id = strtol( psz_name, NULL, 10 );

    bs_skip( s, 32 );

    bs_skip( s, 11 );
    const bool b_angle = bs_read( s, 1 );

    p_item->i_connection = bs_read( s, 4 );
    p_item->clpi.i_stc_id = bs_read( s, 8 );
    p_item->i_in_time = bs_read( s, 32 );
    p_item->i_out_time = bs_read( s, 32 );
    bs_skip( s, 64 );
    bs_skip( s, 1 );
    bs_skip( s, 7 );
    p_item->i_still = bs_read( s, 8 );
    p_item->i_still_time = bs_read( s, 16 );
    if( p_item->i_still == BD_MPLS_PLAY_ITEM_STILL_NONE )
        p_item->i_still_time = 0;
    else if( p_item->i_still == BD_MPLS_PLAY_ITEM_STILL_INFINITE )
        p_item->i_still_time = INT_MAX;

    if( b_angle )
    {
        const int i_angle = bs_read( s, 8 );
        bs_skip( s, 6 );
        p_item->b_angle_different_audio = bs_read( s, 1 );
        p_item->b_angle_seamless = bs_read( s, 1 );

        p_item->p_clpi = calloc( i_angle, sizeof(*p_item->p_clpi) );
        for( p_item->i_clpi = 0; p_item->i_clpi < i_angle; p_item->i_clpi++ )
        {
            if( !p_item->p_clpi )
                break;

            bd_mpls_clpi_t *p_clpi = &p_item->p_clpi[p_item->i_clpi];

            char psz_name[5+1];
            for( int j = 0; j < 5; j++ )
                psz_name[j] = bs_read( s, 8 );
            psz_name[5] = '\0';

            p_clpi->i_id = strtol( psz_name, NULL, 10 );

            bs_skip( s, 32 );

            p_clpi->i_stc_id = bs_read( s, 8 );
        }
    }
    else
    {
        p_item->i_clpi = 0;
        p_item->p_clpi = NULL;
        p_item->b_angle_different_audio = false;
        p_item->b_angle_seamless = true;
    }

    /* STN Table */
    bs_skip( s, 16 );  /* Length */
    bs_skip( s, 16 );

    const int i_video = bs_read( s, 8 );
    const int i_audio = bs_read( s, 8 );
    const int i_pg = bs_read( s, 8 );
    const int i_ig = bs_read( s, 8 );
    const int i_audio_2 = bs_read( s, 8 );
    const int i_video_2 = bs_read( s, 8 );
    const int i_pip_pg = bs_read( s, 8 );
    bs_skip( s, 40 );

    p_item->i_stream = 0;
    p_item->p_stream = calloc( i_video + i_audio + i_pg + i_ig,
                               sizeof(*p_item->p_stream) );

    for( int j = 0; j < i_video; j++, p_item->i_stream++ )
    {
        if( !p_item->p_stream )
            break;

        bd_mpls_stream_Parse( &p_item->p_stream[p_item->i_stream], s,
                              BD_MPLS_STREAM_CLASS_PRIMARY_VIDEO );
    }
    for( int j = 0; j < i_audio; j++, p_item->i_stream++ )
    {
        if( !p_item->p_stream )
            break;

        bd_mpls_stream_Parse( &p_item->p_stream[p_item->i_stream], s,
                              BD_MPLS_STREAM_CLASS_PRIMARY_AUDIO );
    }
    for( int j = 0; j < i_pg; j++, p_item->i_stream++ )
    {
        if( !p_item->p_stream )
            break;

        bd_mpls_stream_Parse( &p_item->p_stream[p_item->i_stream], s,
                              BD_MPLS_STREAM_CLASS_PG );
    }
    for( int j = 0; j < i_ig; j++, p_item->i_stream++ )
    {
        if( !p_item->p_stream )
            break;

        bd_mpls_stream_Parse( &p_item->p_stream[p_item->i_stream], s,
                              BD_MPLS_STREAM_CLASS_IG );
    }
    for( int j = 0; j < i_audio_2; j++ )
    {
        /* TODO I need samples */
    }
    for( int j = 0; j < i_video_2; j++ )
    {
        /* TODO I need samples */
    }
    for( int j = 0; j < i_pip_pg; j++ )
    {
        /* TODO I need samples */
    }

    bs_skip( s, 8 * ( i_start + i_length ) - bs_pos( s ) );
}

void bd_mpls_sub_path_Parse( bd_mpls_sub_path_t *p_path, bs_t *s )
{
    const uint32_t i_length = bs_read( s, 32 );
    const int i_start = bs_pos( s ) / 8;

    bs_skip( s, 8 );
    p_path->i_type = bs_read( s, 8 );
    bs_skip( s, 15 );
    p_path->b_repeat = bs_read( s, 1 );
    bs_skip( s, 8 );
    p_path->i_item = bs_read( s, 8 );

    for( int j = 0; j < p_path->i_item; j++ )
    {
        const int i_length = bs_read( s, 16 );
        const int i_start = bs_pos( s ) / 8;

        /* TODO */

        bs_skip( s, 8 * ( i_start + i_length ) - bs_pos( s ) );
    }

    bs_skip( s, 8 * ( i_start + i_length ) - bs_pos( s ) );
}

void bd_mpls_mark_Parse( bd_mpls_mark_t *p_mark, bs_t *s )
{
    bs_skip( s, 8 );
    p_mark->i_type = bs_read( s, 8 );
    p_mark->i_play_item_id = bs_read( s, 16 );
    p_mark->i_time = bs_read( s, 32 );
    p_mark->i_entry_es_pid = bs_read( s, 16 );
    bs_skip( s, 32 );
}

void bd_mpls_Clean( bd_mpls_t *p_mpls )
{
    for( int i = 0; i < p_mpls->i_play_item; i++ )
        bd_mpls_play_item_Clean( &p_mpls->p_play_item[i] );
    free( p_mpls->p_play_item );

    free( p_mpls->p_sub_path );

    free( p_mpls->p_mark );
}

int bd_mpls_Parse( bd_mpls_t *p_mpls, bs_t *s, int i_id )
{
    const int i_start = bs_pos( s ) / 8;

    /* */
    if( bs_read( s, 32 ) != 0x4d504c53 )
        return VLC_EGENERIC;
    const uint32_t i_version = bs_read( s, 32 );
    if( i_version != 0x30313030 && i_version != 0x30323030 )
        return VLC_EGENERIC;

    const uint32_t i_play_item_start = bs_read( s, 32 );
    const uint32_t i_mark_start = bs_read( s, 32 );
    bs_skip( s, 32 );   /* Extension start */

    /* */
    p_mpls->i_id = i_id;

    /* Read AppInfo: ignored */

    /* Read Playlist */
    bs_t ps = *s;
    bs_skip( &ps, 8 * ( i_start + i_play_item_start ) - bs_pos( s ) );
    bs_skip( &ps, 32 ); /* Length */
    bs_skip( &ps, 16 );
    const int i_play_item = bs_read( &ps, 16 );
    const int i_sub_path = bs_read( &ps, 16 );

    p_mpls->p_play_item = calloc( i_play_item, sizeof(*p_mpls->p_play_item) );
    for( p_mpls->i_play_item = 0; p_mpls->i_play_item < i_play_item; p_mpls->i_play_item++ )
    {
        if( !p_mpls->p_play_item )
            break;
        bd_mpls_play_item_t *p_item = &p_mpls->p_play_item[p_mpls->i_play_item];

        bd_mpls_play_item_Parse( p_item, &ps );
    }
    p_mpls->p_sub_path = calloc( i_sub_path, sizeof(*p_mpls->p_sub_path) );
    for( p_mpls->i_sub_path = 0; p_mpls->i_sub_path < i_sub_path; p_mpls->i_sub_path++ )
    {
        if( !p_mpls->p_sub_path )
            break;
        bd_mpls_sub_path_t *p_sub = &p_mpls->p_sub_path[p_mpls->i_sub_path];

        bd_mpls_sub_path_Parse( p_sub, &ps );
    }

    /* Read Mark */
    bs_t ms = *s;
    bs_skip( &ms, 8 * ( i_start + i_mark_start ) - bs_pos( s ) );
    bs_skip( &ms, 32 );
    const int i_mark = bs_read( &ms, 16 );

    p_mpls->p_mark = calloc( i_mark, sizeof(*p_mpls->p_mark) );
    for( p_mpls->i_mark = 0; p_mpls->i_mark < i_mark; p_mpls->i_mark++ )
    {
        if( !p_mpls->p_mark )
            break;
        bd_mpls_mark_t *p_mark = &p_mpls->p_mark[p_mpls->i_mark];

        bd_mpls_mark_Parse( p_mark, &ms );
    }

    /* Read Extension: ignored */

    return VLC_SUCCESS;
}
