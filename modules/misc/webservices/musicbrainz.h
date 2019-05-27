/*****************************************************************************
 * musicbrainz.h : Musicbrainz API lookup
 *****************************************************************************
 * Copyright (C) 2019 VideoLabs, VLC authors and VideoLAN
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
#ifndef VLC_MUSICBRAINZ_H_
#define VLC_MUSICBRAINZ_H_

#define MUSICBRAINZ_DEFAULT_SERVER "musicbrainz.org"
#define COVERARTARCHIVE_DEFAULT_SERVER "coverartarchive.org"

typedef struct
{
    unsigned i_index;
    char *psz_title;
    char *psz_artist;
} musicbrainz_track_t;

typedef struct
{
    char *psz_id;
    char *psz_group_id;
    char *psz_title;
    char *psz_artist;
    /* https://github.com/metabrainz/mmd-schema/blob/master/schema/musicbrainz_mmd-2.0.rng
       "def_incomplete-date" [0-9]{4}(-[0-9]{2})?(-[0-9]{2})? */
    char *psz_date;
    char *psz_coverart_url;
    size_t i_tracks;
    musicbrainz_track_t *p_tracks;
} musicbrainz_release_t;

typedef struct
{
    size_t i_release;
    musicbrainz_release_t *p_releases;
} musicbrainz_recording_t;

typedef struct
{
    vlc_object_t *obj;
    char *psz_mb_server;
    char *psz_coverart_server;
} musicbrainz_config_t;

void musicbrainz_recording_release(musicbrainz_recording_t *);
musicbrainz_recording_t * musicbrainz_lookup_recording_by_toc(musicbrainz_config_t *, const char *);
musicbrainz_recording_t * musicbrainz_lookup_recording_by_discid(musicbrainz_config_t *, const char *);

typedef struct
{
    char *psz_url;
} coverartarchive_t;

void musicbrainz_release_covert_art(coverartarchive_t *c);
coverartarchive_t * coverartarchive_lookup_releasegroup(musicbrainz_config_t *, const char *);
char * coverartarchive_make_releasegroup_arturl(const char *, const char *);

#endif
