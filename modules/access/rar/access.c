/*****************************************************************************
 * access.c: uncompressed RAR access
 *****************************************************************************
 * Copyright (C) 2008-2010 Laurent Aimar
 * $Id$
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <vlc_access.h>
#include <vlc_stream.h>
#include <vlc_url.h>

#include <assert.h>
#include <limits.h>

#include "rar.h"

struct access_sys_t {
    stream_t               *s;
    rar_file_t             *file;
    const rar_file_chunk_t *chunk;
    uint64_t                position;
};

static int Seek(access_t *access, uint64_t position)
{
    access_sys_t *sys = access->p_sys;
    const rar_file_t *file = sys->file;

    if (position > file->real_size)
        position = file->real_size;
    sys->position = position;

    /* Search the chunk */
    const rar_file_chunk_t *old_chunk = sys->chunk;
    for (int i = 0; i < file->chunk_count; i++) {
        sys->chunk = file->chunk[i];
        if (position < sys->chunk->cummulated_size + sys->chunk->size)
            break;
    }

    const uint64_t offset = sys->chunk->offset +
                            (position - sys->chunk->cummulated_size);

    if (strcmp(old_chunk->mrl, sys->chunk->mrl)) {
        if (sys->s)
            vlc_stream_Delete(sys->s);
        sys->s = vlc_stream_NewMRL(access, sys->chunk->mrl);
    }
    return sys->s ? vlc_stream_Seek(sys->s, offset) : VLC_EGENERIC;
}

static ssize_t Read(access_t *access, void *data, size_t size)
{
    access_sys_t *sys = access->p_sys;

    size_t total = 0;
    while (total < size) {
        const uint64_t chunk_end = sys->chunk->cummulated_size + sys->chunk->size;
        int max = __MIN(__MIN((int64_t)(size - total), (int64_t)(chunk_end - sys->position)), INT_MAX);
        if (max <= 0)
            break;

        int r = sys->s ? vlc_stream_Read(sys->s, data, max) : -1;
        if (r <= 0)
            break;

        total += r;
        if( data )
            data = ((char *)data) + r;
        sys->position += r;
        if (sys->position >= chunk_end &&
            Seek(access, sys->position))
            break;
    }
    return total;

}

static int Control(access_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;
    stream_t *s = sys->s;
    if (!s)
        return VLC_EGENERIC;

    switch (query) {
    case STREAM_CAN_SEEK: {
        bool *b = va_arg(args, bool *);
        return vlc_stream_Control(s, STREAM_CAN_SEEK, b);
    }
    case STREAM_CAN_FASTSEEK: {
        bool *b = va_arg(args, bool *);
        return vlc_stream_Control(s, STREAM_CAN_FASTSEEK, b);
    }
    /* FIXME the following request should ask the underlying access object */
    case STREAM_CAN_PAUSE:
    case STREAM_CAN_CONTROL_PACE: {
        bool *b = va_arg(args, bool *);
        *b = true;
        return VLC_SUCCESS;
    }
    case STREAM_GET_SIZE:
        *va_arg(args, uint64_t *) = sys->file->size;
        return VLC_SUCCESS;
    case STREAM_GET_PTS_DELAY: {
        int64_t *delay = va_arg(args, int64_t *);
        *delay = DEFAULT_PTS_DELAY;
        return VLC_SUCCESS;
    }
    case STREAM_SET_PAUSE_STATE:
        return VLC_SUCCESS;

    default:
        return VLC_EGENERIC;
    }
}

int RarAccessOpen(vlc_object_t *object)
{
    access_t *access = (access_t*)object;

    const char *name = strchr(access->psz_location, '|');
    if (name == NULL)
        return VLC_EGENERIC;

    char *base = strndup(access->psz_location, name - access->psz_location);
    if (unlikely(base == NULL))
        return VLC_ENOMEM;

    name++;
    vlc_uri_decode(base);

    stream_t *s = vlc_stream_NewMRL(access, base);
    if (!s || RarProbe(s))
        goto error;

    struct
    {
        int filescount;
        rar_file_t **files;
        unsigned int i_nbvols;
    } newscheme = { 0, NULL, 0 }, oldscheme = { 0, NULL, 0 }, *p_scheme;

    if (RarParse(s, &newscheme.filescount, &newscheme.files, &newscheme.i_nbvols, false)
            || newscheme.filescount < 1 || newscheme.i_nbvols < 2 )
    {
        /* We might want to lookup old naming scheme, could be a part1.rar,part1.r00 */
        vlc_stream_Seek(s, 0);
        RarParse(s, &oldscheme.filescount, &oldscheme.files, &oldscheme.i_nbvols, true);
    }

    if (oldscheme.filescount >= newscheme.filescount && oldscheme.i_nbvols > newscheme.i_nbvols)
    {
        for (int i = 0; i < newscheme.filescount; i++)
            RarFileDelete(newscheme.files[i]);
        free(newscheme.files);
        p_scheme = &oldscheme;
        msg_Dbg(s, "using rar old naming for %d files nbvols %u", p_scheme->filescount, oldscheme.i_nbvols);
    }
    else if (newscheme.filescount)
    {
        for (int i = 0; i < oldscheme.filescount; i++)
            RarFileDelete(oldscheme.files[i]);
        free(oldscheme.files);
        p_scheme = &newscheme;
        msg_Dbg(s, "using rar new naming for %d files nbvols %u", p_scheme->filescount, oldscheme.i_nbvols);
    }
    else
    {
        msg_Info(s, "Invalid or unsupported RAR archive");
        for (int i = 0; i < oldscheme.filescount; i++)
            RarFileDelete(oldscheme.files[i]);
        free(oldscheme.files);
        for (int i = 0; i < newscheme.filescount; i++)
            RarFileDelete(newscheme.files[i]);
        free(newscheme.files);
        goto error;
    }

    rar_file_t *file = NULL;
    for (int i = 0; i < p_scheme->filescount; i++) {
        if (!file && !strcmp(p_scheme->files[i]->name, name))
            file = p_scheme->files[i];
        else
            RarFileDelete(p_scheme->files[i]);
    }
    free(p_scheme->files);
    if (!file)
        goto error;

    access_sys_t *sys = access->p_sys = malloc(sizeof(*sys));
    sys->s    = s;
    sys->file = file;

    access->pf_read    = Read;
    access->pf_block   = NULL;
    access->pf_control = Control;
    access->pf_seek    = Seek;

    rar_file_chunk_t dummy = {
        .mrl = base,
    };
    sys->chunk = &dummy;
    Seek(access, 0);

    free(base);
    return VLC_SUCCESS;

error:
    if (s)
        vlc_stream_Delete(s);
    free(base);
    return VLC_EGENERIC;
}

void RarAccessClose(vlc_object_t *object)
{
    access_t *access = (access_t*)object;
    access_sys_t *sys = access->p_sys;

    if (sys->s)
        vlc_stream_Delete(sys->s);
    RarFileDelete(sys->file);
    free(sys);
}
