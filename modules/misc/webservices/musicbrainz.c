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

#include "json_helper.h"
#include "musicbrainz.h"

typedef struct
{
    json_value *root;
} musicbrainz_lookup_t;

static void musicbrainz_lookup_release(musicbrainz_lookup_t *p)
{
    if(p && p->root)
        json_value_free(p->root);
    free(p);
}

static musicbrainz_lookup_t * musicbrainz_lookup_new(void)
{
    return calloc(1, sizeof(musicbrainz_lookup_t));
}

static musicbrainz_lookup_t * musicbrainz_lookup(vlc_object_t *p_obj, const char *psz_url)
{
    msg_Dbg(p_obj, "Querying MB for %s", psz_url);
    void *p_buffer = json_retrieve_document(p_obj, psz_url);
    if(!p_buffer)
        return NULL;

    musicbrainz_lookup_t *p_lookup = musicbrainz_lookup_new();
    if(p_lookup)
    {
        p_lookup->root = json_parse_document(p_obj, p_buffer);
        if (!p_lookup->root)
            msg_Dbg(p_obj, "No results");
    }
    free(p_buffer);
    return p_lookup;
}

static bool musicbrainz_fill_track(const json_value *tracknode, musicbrainz_track_t *t)
{
    t->psz_title = json_dupstring(tracknode, "title");

    const json_value *node = json_getbyname(tracknode, "artist-credit");
    if (node && node->type == json_array && node->u.array.length)
        t->psz_artist = json_dupstring(node->u.array.values[0], "name");

    node = json_getbyname(tracknode, "position");
    if (node && node->type == json_integer)
        t->i_index = node->u.integer;

    return true;
}

static bool musicbrainz_has_cover_in_releasegroup(json_value ** const p_nodes,
                                                  size_t i_nodes,
                                                  const char *psz_group_id)
{
    for(size_t i=0; i<i_nodes; i++)
    {
        const json_value *rgnode = json_getbyname(p_nodes[i], "release-group");
        if(rgnode)
        {
            const char *psz_id = jsongetstring(rgnode, "id");
            if(!psz_id || strcmp(psz_id, psz_group_id))
                continue;

            const json_value *node = json_getbyname(p_nodes[i], "cover-art-archive");
            if(!node)
                continue;

            node = json_getbyname(node, "front");
            if(!node || node->type != json_boolean || !node->u.boolean)
                continue;

            return true;
        }
    }

    return false;
}

static char *musicbrainz_fill_artists(const json_value *arraynode)
{
    char *psz = NULL;
    if(arraynode->type != json_array || arraynode->u.array.length < 1)
        return psz;

    size_t i_total = 1;
    for(size_t i=0; i<arraynode->u.array.length; i++)
    {
        const json_value *name = json_getbyname(arraynode->u.array.values[i], "name");
        if(name->type != json_string)
            continue;

        if(psz == NULL)
        {
            psz = strdup(name->u.string.ptr);
            i_total = name->u.string.length + 1;
        }
        else
        {
            char *p = realloc(psz, i_total + name->u.string.length + 2);
            if(p)
            {
                psz = p;
                psz = strcat(psz, ", ");
                psz = strncat(psz, name->u.string.ptr, name->u.string.length);
                i_total += name->u.string.length + 2;
            }
        }
    }

    return psz;
}

static bool musicbrainz_fill_release(const json_value *releasenode, musicbrainz_release_t *r)
{
    const json_value *media = json_getbyname(releasenode, "media");
    if(!media || media->type != json_array ||
       media->u.array.length == 0)
        return false;
    /* we always use first media */
    media = media->u.array.values[0];

    const json_value *tracks = json_getbyname(media, "tracks");
    if(!tracks || tracks->type != json_array ||
       tracks->u.array.length == 0)
        return false;

    r->p_tracks = calloc(tracks->u.array.length, sizeof(*r->p_tracks));
    if(!r->p_tracks)
        return false;

    for(size_t i=0; i<tracks->u.array.length; i++)
    {
        if(musicbrainz_fill_track(tracks->u.array.values[i], &r->p_tracks[r->i_tracks]))
            r->i_tracks++;
    }

    r->psz_title = json_dupstring(releasenode, "title");
    r->psz_id = json_dupstring(releasenode, "id");

    const json_value *rgnode = json_getbyname(releasenode, "release-group");
    if(rgnode)
    {
        r->psz_date = json_dupstring(rgnode, "first-release-date");
        r->psz_group_id = json_dupstring(rgnode, "id");

        const json_value *node = json_getbyname(rgnode, "artist-credit");
        if(node)
            r->psz_artist = musicbrainz_fill_artists(node);
    }
    else
    {
        const json_value *node = json_getbyname(releasenode, "artist-credit");
        if(node)
            r->psz_artist = musicbrainz_fill_artists(node);

        node = json_getbyname(releasenode, "release-events");
        if(node && node->type == json_array && node->u.array.length)
            r->psz_date = json_dupstring(node->u.array.values[0], "date");
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

    const json_value *releases = json_getbyname(lookup->root, "releases");
    if (releases && releases->type == json_array &&
        releases->u.array.length)
    {
        r->p_releases = calloc(releases->u.array.length, sizeof(*r->p_releases));
        if(r->p_releases)
        {
            for(unsigned i=0; i<releases->u.array.length; i++)
            {
                json_value *node = releases->u.array.values[i];
                musicbrainz_release_t *p_mbrel = &r->p_releases[r->i_release];
                if (!node || node->type != json_object ||
                    !musicbrainz_fill_release(node, p_mbrel))
                    continue;

                /* Try to find cover from other releases from the same group */
                if(p_mbrel->psz_group_id && !p_mbrel->psz_coverart_url &&
                   musicbrainz_has_cover_in_releasegroup(releases->u.array.values,
                                                         releases->u.array.length,
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
    if(0 < asprintf(&psz_url, "https://%s/releasegroup/%s", cfg->psz_coverart_server, psz_id ))
    {
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
