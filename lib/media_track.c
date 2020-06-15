/*****************************************************************************
 * media_track.c: Libvlc API media track
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#include <assert.h>
#include <errno.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_events.h>

#include "libvlc_internal.h"
#include "media_internal.h"

struct libvlc_media_tracklist_t
{
    size_t count;
    libvlc_media_trackpriv_t *tracks[];
};

void
libvlc_media_trackpriv_from_es( libvlc_media_trackpriv_t *trackpriv,
                                const es_format_t *es  )
{
    libvlc_media_track_t *track = &trackpriv->t;

    track->i_codec = es->i_codec;
    track->i_original_fourcc = es->i_original_fourcc;
    track->i_id = es->i_id;

    track->i_profile = es->i_profile;
    track->i_level = es->i_level;

    track->i_bitrate = es->i_bitrate;
    track->psz_language = es->psz_language != NULL ? strdup(es->psz_language) : NULL;
    track->psz_description = es->psz_description != NULL ? strdup(es->psz_description) : NULL;
    track->psz_id = NULL;
    track->id_stable = false;
    track->psz_name = NULL;
    track->selected = false;

    switch( es->i_cat )
    {
    case UNKNOWN_ES:
    default:
        track->i_type = libvlc_track_unknown;
        break;
    case VIDEO_ES:
        track->video = &trackpriv->video;
        track->i_type = libvlc_track_video;
        track->video->i_height = es->video.i_visible_height;
        track->video->i_width = es->video.i_visible_width;
        track->video->i_sar_num = es->video.i_sar_num;
        track->video->i_sar_den = es->video.i_sar_den;
        track->video->i_frame_rate_num = es->video.i_frame_rate;
        track->video->i_frame_rate_den = es->video.i_frame_rate_base;

        assert( es->video.orientation >= ORIENT_TOP_LEFT &&
                es->video.orientation <= ORIENT_RIGHT_BOTTOM );
        track->video->i_orientation = (int) es->video.orientation;

        assert( ( es->video.projection_mode >= PROJECTION_MODE_RECTANGULAR &&
                es->video.projection_mode <= PROJECTION_MODE_EQUIRECTANGULAR ) ||
                ( es->video.projection_mode == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD ) );
        track->video->i_projection = (int) es->video.projection_mode;

        track->video->pose.f_yaw = es->video.pose.yaw;
        track->video->pose.f_pitch = es->video.pose.pitch;
        track->video->pose.f_roll = es->video.pose.roll;
        track->video->pose.f_field_of_view = es->video.pose.fov;

        assert( es->video.multiview_mode >= MULTIVIEW_2D &&
                es->video.multiview_mode <= MULTIVIEW_STEREO_CHECKERBOARD );
        track->video->i_multiview = (int) es->video.multiview_mode;
        break;
    case AUDIO_ES:
        track->audio = &trackpriv->audio;
        track->i_type = libvlc_track_audio;
        track->audio->i_channels = es->audio.i_channels;
        track->audio->i_rate = es->audio.i_rate;
        break;
    case SPU_ES:
        track->subtitle = &trackpriv->subtitle;
        track->i_type = libvlc_track_text;
        track->subtitle->psz_encoding = es->subs.psz_encoding != NULL ?
                                        strdup(es->subs.psz_encoding) : NULL;
        break;
    }
}

static libvlc_media_trackpriv_t *
libvlc_media_trackpriv_new( void )
{
    libvlc_media_trackpriv_t *trackpriv =
        malloc( sizeof (libvlc_media_trackpriv_t ) );

    if( trackpriv == NULL )
        return NULL;

    trackpriv->es_id = NULL;
    vlc_atomic_rc_init( &trackpriv->rc );
    return trackpriv;
}

void
libvlc_media_track_clean( libvlc_media_track_t *track )
{
    free( track->psz_language );
    free( track->psz_description );
    free( track->psz_name );
    switch( track->i_type )
    {
    case libvlc_track_audio:
        break;
    case libvlc_track_video:
        break;
    case libvlc_track_text:
        free( track->subtitle->psz_encoding );
        break;
    case libvlc_track_unknown:
    default:
        break;
    }
}

libvlc_media_track_t *
libvlc_media_track_hold( libvlc_media_track_t *track )
{
    libvlc_media_trackpriv_t *trackpriv =
        container_of( track, libvlc_media_trackpriv_t, t );
    vlc_atomic_rc_inc( &trackpriv->rc );
    return track;
}

void
libvlc_media_track_release( libvlc_media_track_t *track )
{
    libvlc_media_trackpriv_t *trackpriv =
        container_of( track, libvlc_media_trackpriv_t, t );

    if( vlc_atomic_rc_dec( &trackpriv->rc ) )
    {
        libvlc_media_track_clean( track );
        if( trackpriv->es_id )
            vlc_es_id_Release( trackpriv->es_id );
        free( trackpriv );
    }
}

static libvlc_media_tracklist_t *
libvlc_media_tracklist_alloc( size_t count )
{
    size_t size;
    if( mul_overflow( count, sizeof(libvlc_media_trackpriv_t *), &size) )
        return NULL;
    if( add_overflow( size, sizeof(libvlc_media_tracklist_t), &size) )
        return NULL;

    libvlc_media_tracklist_t *list = malloc( size );
    if( list == NULL )
        return NULL;

    list->count = 0;
    return list;
}

libvlc_media_tracklist_t *
libvlc_media_tracklist_from_es_array( es_format_t **es_array,
                                      size_t es_count,
                                      libvlc_track_type_t type )
{
    size_t count = 0;
    const enum es_format_category_e cat = libvlc_track_type_to_escat( type );

    for( size_t i = 0; i < es_count; ++i )
    {
        if( es_array[i]->i_cat == cat )
            count++;
    }

    libvlc_media_tracklist_t *list = libvlc_media_tracklist_alloc( count );

    if( count == 0 || list == NULL )
        return list;

    for( size_t i = 0; i < es_count; ++i )
    {
        if( es_array[i]->i_cat == cat )
        {
            libvlc_media_trackpriv_t *trackpriv = libvlc_media_trackpriv_new();
            if( trackpriv == NULL )
            {
                libvlc_media_tracklist_delete( list );
                return NULL;
            }
            list->tracks[list->count++] = trackpriv;
            libvlc_media_trackpriv_from_es( trackpriv, es_array[i] );
        }
    }

    return list;
}

static void
libvlc_media_trackpriv_from_player_track( libvlc_media_trackpriv_t *trackpriv,
                                          const struct vlc_player_track *track )
{
    libvlc_media_trackpriv_from_es( trackpriv, &track->fmt );

    trackpriv->es_id = vlc_es_id_Hold( track->es_id );

    trackpriv->t.psz_id = vlc_es_id_GetStrId( track->es_id );
    trackpriv->t.id_stable = vlc_es_id_IsStrIdStable( track->es_id );
    trackpriv->t.psz_name = strdup( track->name );
    trackpriv->t.selected = track->selected;
}

libvlc_media_track_t *
libvlc_media_track_create_from_player_track( const struct vlc_player_track *track )
{
    libvlc_media_trackpriv_t *trackpriv = libvlc_media_trackpriv_new();
    if( trackpriv == NULL )
        return NULL;
    libvlc_media_trackpriv_from_player_track( trackpriv, track );
    return &trackpriv->t;
}

libvlc_media_tracklist_t *
libvlc_media_tracklist_from_player( vlc_player_t *player,
                                    libvlc_track_type_t type )
{
    const enum es_format_category_e cat = libvlc_track_type_to_escat( type );

    size_t count = vlc_player_GetTrackCount( player, cat );
    libvlc_media_tracklist_t *list = libvlc_media_tracklist_alloc( count );

    if( count == 0 || list == NULL )
        return NULL;

    for( size_t i = 0; i < count; ++i )
    {
        const struct vlc_player_track *track =
            vlc_player_GetTrackAt( player, cat, i );
        assert( track );

        libvlc_media_trackpriv_t *trackpriv = libvlc_media_trackpriv_new();
        if( trackpriv == NULL )
        {
            libvlc_media_tracklist_delete( list );
            return NULL;
        }
        list->tracks[list->count++] = trackpriv;
        libvlc_media_trackpriv_from_player_track( trackpriv, track );
    }

    return list;
}

size_t
libvlc_media_tracklist_count( const libvlc_media_tracklist_t *list )
{
    return list->count;
}

libvlc_media_track_t *
libvlc_media_tracklist_at( libvlc_media_tracklist_t *list, size_t idx )
{
    assert( idx < list->count );
    return &list->tracks[idx]->t;
}

void
libvlc_media_tracklist_delete( libvlc_media_tracklist_t *list )
{
    for( size_t i = 0; i < list->count; ++i )
    {
        libvlc_media_trackpriv_t *trackpriv = list->tracks[i];
        libvlc_media_track_release( &trackpriv->t );
    }
    free( list );
}
