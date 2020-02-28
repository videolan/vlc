/*****************************************************************************
 * player_track.c: Player track and program implementation
 *****************************************************************************
 * Copyright © 2018-2019 VLC authors and VideoLAN
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
#include "player.h"

static char *
vlc_player_program_DupTitle(int id, const char *title)
{
    char *dup;
    if (title)
        dup = strdup(title);
    else if (asprintf(&dup, "%d", id) == -1)
        dup = NULL;
    return dup;
}

struct vlc_player_program *
vlc_player_program_New(int id, const char *name)
{
    struct vlc_player_program *prgm = malloc(sizeof(*prgm));
    if (!prgm)
        return NULL;
    prgm->name = vlc_player_program_DupTitle(id, name);
    if (!prgm->name)
    {
        free(prgm);
        return NULL;
    }
    prgm->group_id = id;
    prgm->selected = prgm->scrambled = false;

    return prgm;
}

int
vlc_player_program_Update(struct vlc_player_program *prgm, int id,
                          const char *name)
{
    free((char *)prgm->name);
    prgm->name = vlc_player_program_DupTitle(id, name);
    return prgm->name != NULL ? VLC_SUCCESS : VLC_ENOMEM;
}

struct vlc_player_program *
vlc_player_program_Dup(const struct vlc_player_program *src)
{
    struct vlc_player_program *dup =
        vlc_player_program_New(src->group_id, src->name);

    if (!dup)
        return NULL;
    dup->selected = src->selected;
    dup->scrambled = src->scrambled;
    return dup;
}

void
vlc_player_program_Delete(struct vlc_player_program *prgm)
{
    free((char *)prgm->name);
    free(prgm);
}

struct vlc_player_program *
vlc_player_program_vector_FindById(vlc_player_program_vector *vec, int id,
                                   size_t *idx)
{
    for (size_t i = 0; i < vec->size; ++i)
    {
        struct vlc_player_program *prgm = vec->data[i];
        if (prgm->group_id == id)
        {
            if (idx)
                *idx = i;
            return prgm;
        }
    }
    return NULL;
}

struct vlc_player_track_priv *
vlc_player_track_priv_New(vlc_es_id_t *id, const char *name, const es_format_t *fmt)
{
    struct vlc_player_track_priv *trackpriv = malloc(sizeof(*trackpriv));
    if (!trackpriv)
        return NULL;
    struct vlc_player_track *track = &trackpriv->t;

    trackpriv->delay = INT64_MAX;
    trackpriv->vout = NULL;
    trackpriv->vout_order = VLC_VOUT_ORDER_NONE;
    trackpriv->selected_by_user = false;

    track->name = strdup(name);
    if (!track->name)
    {
        free(track);
        return NULL;
    }

    int ret = es_format_Copy(&track->fmt, fmt);
    if (ret != VLC_SUCCESS)
    {
        free((char *)track->name);
        free(track);
        return NULL;
    }
    track->es_id = vlc_es_id_Hold(id);
    track->selected = false;

    return trackpriv;
}

void
vlc_player_track_priv_Delete(struct vlc_player_track_priv *trackpriv)
{
    struct vlc_player_track *track = &trackpriv->t;
    es_format_Clean(&track->fmt);
    free((char *)track->name);
    vlc_es_id_Release(track->es_id);
    free(trackpriv);
}

void
vlc_player_track_Delete(struct vlc_player_track *track)
{
    struct vlc_player_track_priv *trackpriv =
        container_of(track, struct vlc_player_track_priv, t);
    vlc_player_track_priv_Delete(trackpriv);
}

struct vlc_player_track *
vlc_player_track_Dup(const struct vlc_player_track *src)
{
    struct vlc_player_track_priv *duppriv =
        vlc_player_track_priv_New(src->es_id, src->name, &src->fmt);

    if (!duppriv)
        return NULL;
    duppriv->t.selected = src->selected;
    return &duppriv->t;
}

int
vlc_player_track_priv_Update(struct vlc_player_track_priv *trackpriv,
                             const char *name, const es_format_t *fmt)
{
    struct vlc_player_track *track = &trackpriv->t;

    if (strcmp(name, track->name) != 0)
    {
        char *dup = strdup(name);
        if (!dup)
            return VLC_ENOMEM;
        free((char *)track->name);
        track->name = dup;
    }

    es_format_t fmtdup;
    int ret = es_format_Copy(&fmtdup, fmt);
    if (ret != VLC_SUCCESS)
        return ret;

    es_format_Clean(&track->fmt);
    track->fmt = fmtdup;
    return VLC_SUCCESS;
}

struct vlc_player_track_priv *
vlc_player_track_vector_FindById(vlc_player_track_vector *vec, vlc_es_id_t *id,
                                 size_t *idx)
{
    for (size_t i = 0; i < vec->size; ++i)
    {
        struct vlc_player_track_priv *trackpriv = vec->data[i];
        if (trackpriv->t.es_id == id)
        {
            if (idx)
                *idx = i;
            return trackpriv;
        }
    }
    return NULL;
}


int
vlc_player_GetFirstSelectedTrackId(const vlc_player_track_vector* tracks)
{
    struct vlc_player_track_priv* t;
    vlc_vector_foreach(t, tracks)
    {
        if (t->t.selected)
            return t->t.fmt.i_id;
    }
    return -1;
}
