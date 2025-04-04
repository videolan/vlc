/*****************************************************************************
 * musicbrainz.c : Musicbrainz API lookup
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_messages.h>

#include "json_helper.h"
#include "musicbrainz.h"

typedef struct
{
    struct json_object json;
} musicbrainz_lookup_t;

static void musicbrainz_lookup_release(musicbrainz_lookup_t *p)
{
    json_free(&p->json);
    free(p);
}

static musicbrainz_lookup_t * musicbrainz_lookup_new(void)
{
    return calloc(1, sizeof(musicbrainz_lookup_t));
}

static musicbrainz_lookup_t * musicbrainz_lookup(vlc_object_t *p_obj, const char *psz_url)
{
    msg_Dbg(p_obj, "Querying MB for %s", psz_url);
    size_t i_buffer;
    void *p_buffer = json_retrieve_document(p_obj, psz_url, &i_buffer);
    if(!p_buffer)
        return NULL;

    musicbrainz_lookup_t *p_lookup = musicbrainz_lookup_new();
    if (!p_lookup)
    {
        free(p_buffer);
        return NULL;
    }

    struct json_helper_sys sys;
    sys.logger = p_obj->logger;
    sys.buffer = p_buffer;
    sys.size = i_buffer;

    int val = json_parse(&sys, &p_lookup->json);
    free(p_buffer);
    if (val)
    {
        msg_Dbg( p_obj, "error: could not parse json!");
        musicbrainz_lookup_release(p_lookup);
        return NULL;
    }

    return p_lookup;
}

static bool musicbrainz_fill_track(const struct json_object *trackobj, musicbrainz_track_t *t)
{
    t->psz_title = json_dupstring(trackobj, "title");

    const struct json_array *array = json_get_array(trackobj, "artist-credit");
    if (array != NULL && array->size >= 1) {
        const struct json_value *artist = &array->entries[0];
        if (artist->type == JSON_OBJECT)
            t->psz_artist = json_dupstring(&artist->object, "name");
    }

    double pos = json_get_num(trackobj, "position");
    if (pos != NAN) {
        t->i_index = pos;
    }

    return true;
}

static bool musicbrainz_has_cover_in_releasegroup(const struct json_value *p_nodes,
                                                  size_t i_nodes,
                                                  const char *psz_group_id)
{
    /* FIXME, not sure it does what I want */
    for(size_t i=0; i<i_nodes; i++)
    {
        if (p_nodes[i].type != JSON_OBJECT)
            continue;
        const struct json_object *obj = &p_nodes[i].object;

        const struct json_object *rgobj = json_get_object(obj, "release-group");
        if (rgobj == NULL)
            continue;
        const char *psz_id = json_get_str(rgobj, "id");
        if(!psz_id || strcmp(psz_id, psz_group_id))
            continue;
        
        const struct json_object *cover = json_get_object(obj, "cover-art-archive");
        if (cover == NULL)
            continue;
        const struct json_value *node = json_get(cover, "front");
        if (node == NULL)
            continue;
        if(node->type != JSON_BOOLEAN || !node->boolean)
            continue;

        return true;
    }

    return false;
}

static char *musicbrainz_fill_artists(const struct json_array *array)
{
    char *psz = NULL;

    for(size_t i = 0; i < array->size; i++)
    {
        if (array->entries[i].type != JSON_OBJECT)
            continue;
        const struct json_object *obj = &array->entries[i].object;

        const char *name = json_get_str(obj, "name");
        if (name == NULL)
            continue;
        if(psz == NULL) {
            psz = strdup(name);
        }
        else {
            char *p = NULL;
            if (asprintf(&p, "%s, %s", psz, name) > 0) {
                free(psz);
                psz = p;
            }
        }
    }

    return psz;
}

static bool musicbrainz_fill_release(const struct json_object *release,
                                     musicbrainz_release_t *r)
{
    const struct json_array *media_array = json_get_array(release, "media");
    if (media_array == NULL || media_array->size == 0)
        return false;
    /* we always use first media */
    const struct json_value *media = &media_array->entries[0];
    if (media->type != JSON_OBJECT)
        return false;

    const struct json_array *tracks = json_get_array(&media->object, "tracks");
    if (tracks == NULL || tracks->size == 0)
        return false;

    r->p_tracks = calloc(tracks->size, sizeof(*r->p_tracks));
    if(!r->p_tracks)
        return false;

    for(size_t i = 0; i < tracks->size; i++)
    {
        if (tracks->entries[i].type != JSON_OBJECT)
            continue;
        const struct json_object *trackobj = &tracks->entries[i].object;
        if(musicbrainz_fill_track(trackobj, &r->p_tracks[r->i_tracks]))
            r->i_tracks++;
    }

    r->psz_title = json_dupstring(release, "title");
    r->psz_id = json_dupstring(release, "id");

    const struct json_object *rg = json_get_object(release,
                                                   "release-group");
    if (rg != NULL) {
        r->psz_date = json_dupstring(rg, "first-release-date");
        r->psz_group_id = json_dupstring(rg, "id");

        const struct json_array *artists = json_get_array(rg, "artist-credit");
        if (artists != NULL)
            r->psz_artist = musicbrainz_fill_artists(artists);
    }
    const struct json_array *events = json_get_array(release,
                                                     "release-events");
    if (events != NULL && events->size > 0) {
        if (events->entries[0].type == JSON_OBJECT)
            r->psz_date = json_dupstring(&events->entries[0].object, "date");
    }

    return true;
}

void musicbrainz_recording_release(musicbrainz_recording_t *mbr)
{
    for(size_t i=0; i<mbr->i_release; i++)
    {
        free(mbr->p_releases[i].psz_id);
        free(mbr->p_releases[i].psz_group_id);
        free(mbr->p_releases[i].psz_artist);
        free(mbr->p_releases[i].psz_title);
        free(mbr->p_releases[i].psz_date);
        free(mbr->p_releases[i].psz_coverart_url);
        for(size_t j=0; j<mbr->p_releases[i].i_tracks; j++)
        {
            free(mbr->p_releases[i].p_tracks[j].psz_title);
            free(mbr->p_releases[i].p_tracks[j].psz_artist);
        }
        free(mbr->p_releases[i].p_tracks);
    }
    free(mbr->p_releases);
    free(mbr);
}

static musicbrainz_recording_t *musicbrainz_lookup_recording_by_apiurl(vlc_object_t *obj,
                                                                       const char *psz_url)
{
    musicbrainz_recording_t *r = calloc(1, sizeof(*r));
    if(!r)
        return NULL;

    musicbrainz_lookup_t *lookup = musicbrainz_lookup(obj, psz_url);
    if(!lookup)
    {
        free(r);
        return NULL;
    }

    const struct json_array *releases = json_get_array(&lookup->json, "releases");
    if (releases != NULL && releases->size)
    {
        r->p_releases = calloc(releases->size, sizeof(*r->p_releases));
        if(r->p_releases)
        {
            for(unsigned i = 0; i < releases->size; i++)
            {
                musicbrainz_release_t *p_mbrel = &r->p_releases[r->i_release];
                const struct json_value *node = &releases->entries[i];
                if (node->type != JSON_OBJECT ||
                    !musicbrainz_fill_release(&node->object, p_mbrel))
                    continue;

                /* Try to find cover from other releases from the same group */
                if(p_mbrel->psz_group_id && !p_mbrel->psz_coverart_url &&
                   musicbrainz_has_cover_in_releasegroup(releases->entries,
                                                         releases->size,
                                                         p_mbrel->psz_group_id))
                {
                    char *psz_art = coverartarchive_make_releasegroup_arturl(
                                        COVERARTARCHIVE_DEFAULT_SERVER,
                                        p_mbrel->psz_group_id );
                    if(psz_art)
                        p_mbrel->psz_coverart_url = psz_art;
                }

                r->i_release++;
            }
        }
    }

    musicbrainz_lookup_release(lookup);

    return r;
}

static char *musicbrainz_build_discid_json_url(const char *psz_server,
                                               const char *psz_disc_id,
                                               const char *psz_tail)
{
    char *psz_url;
    if(asprintf(&psz_url,
                "https://%s/ws/2/discid/%s?"
                "fmt=json"
                "&inc=artist-credits+recordings+release-groups"
                "&cdstubs=no"
                "%s%s",
                psz_server ? psz_server : MUSICBRAINZ_DEFAULT_SERVER,
                psz_disc_id,
                psz_tail ? "&" : "",
                psz_tail ? psz_tail : "" ) > -1 )
    {
        return psz_url;
    }
    return NULL;
}

musicbrainz_recording_t *musicbrainz_lookup_recording_by_toc(musicbrainz_config_t *cfg,
                                                             const char *psz_toc)
{
    char *psz_url = musicbrainz_build_discid_json_url(cfg->psz_mb_server, "-", psz_toc);
    if(!psz_url)
        return NULL;

    musicbrainz_recording_t *r = musicbrainz_lookup_recording_by_apiurl(cfg->obj, psz_url);
    free(psz_url);
    return r;
}

musicbrainz_recording_t *musicbrainz_lookup_recording_by_discid(musicbrainz_config_t *cfg,
                                                                const char *psz_disc_id)
{
    char *psz_url = musicbrainz_build_discid_json_url(cfg->psz_mb_server, psz_disc_id, NULL);
    if(!psz_url)
        return NULL;

    musicbrainz_recording_t *r = musicbrainz_lookup_recording_by_apiurl(cfg->obj, psz_url);
    free(psz_url);
    return r;
}

char * coverartarchive_make_releasegroup_arturl(const char *psz_server, const char *psz_group_id)
{
    char *psz_art;
    if(-1 < asprintf(&psz_art, "https://%s/release-group/%s/front",
                     psz_server ? psz_server : COVERARTARCHIVE_DEFAULT_SERVER,
                     psz_group_id))
        return psz_art;
    return NULL;
}

void musicbrainz_release_covert_art(coverartarchive_t *c)
{
    free(c);
}

coverartarchive_t * coverartarchive_lookup_releasegroup(musicbrainz_config_t *cfg, const char *psz_id)
{
    coverartarchive_t *c = calloc(1, sizeof(*c));
    if(!c)
        return NULL;

    char *psz_url;
    if(asprintf(&psz_url, "https://%s/release-group/%s", cfg->psz_coverart_server, psz_id) < 0)
    {
        free(c);
        return NULL;
    }

     musicbrainz_lookup_t *p_lookup = musicbrainz_lookup(cfg->obj, psz_url);
     free(psz_url);

     if(!p_lookup)
     {
         free(c);
         return NULL;
     }

    return c;
}

