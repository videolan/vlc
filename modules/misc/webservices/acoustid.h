/*****************************************************************************
 * acoustid.h: AcoustId webservice parser
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
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
#define ACOUSTID_ANON_SERVER "fingerprint.videolan.org"
#define ACOUSTID_ANON_SERVER_PATH   "/acoustid.php"
#define MB_ID_SIZE 36

struct acoustid_mb_result_t
{
    char *psz_artist;
    char *psz_title;
    char s_musicbrainz_id[MB_ID_SIZE];
};
typedef struct acoustid_mb_result_t acoustid_mb_result_t;

struct acoustid_result_t
{
    double d_score;
    char *psz_id;
    struct
    {
        unsigned int count;
        acoustid_mb_result_t *p_recordings;
    } recordings;
};
typedef struct acoustid_result_t acoustid_result_t;

struct acoustid_results_t
{
    acoustid_result_t * p_results;
    unsigned int count;
};
typedef struct acoustid_results_t acoustid_results_t;

struct acoustid_fingerprint_t
{
    char *psz_fingerprint;
    unsigned int i_duration;
    acoustid_results_t results;
};
typedef struct acoustid_fingerprint_t acoustid_fingerprint_t;

typedef struct
{
    vlc_object_t *p_obj;
    char *psz_server;
    char *psz_apikey;
} acoustid_config_t;

int acoustid_lookup_fingerprint( const acoustid_config_t *, acoustid_fingerprint_t * );
void acoustid_result_release( acoustid_result_t * );
