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

/*****************************************************************************
 * Preamble
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_description(N_("Uncompressed RAR"))
    set_capability("access", 0)
    set_callbacks(Open, Close)
    add_shortcut("rar")
vlc_module_end()

/*****************************************************************************
 * Local definitions/prototypes
 *****************************************************************************/
struct access_sys_t {
    stream_t               *s;
    rar_file_t             *file;
    const rar_file_chunk_t *chunk;
};

static int Seek(access_t *access, uint64_t position)
{
    access_sys_t *sys = access->p_sys;
    const rar_file_t *file = sys->file;

    if (position > file->real_size)
        position = file->real_size;

    /* Search the chunk */
    const rar_file_chunk_t *old_chunk = sys->chunk;
    for (int i = 0; i < file->chunk_count; i++) {
        sys->chunk = file->chunk[i];
        if (position < sys->chunk->cummulated_size + sys->chunk->size)
            break;
    }
    access->info.i_pos = position;
    access->info.b_eof = false;

    const uint64_t offset = sys->chunk->offset +
                            (position - sys->chunk->cummulated_size);

    if (strcmp(old_chunk->mrl, sys->chunk->mrl)) {
        if (sys->s)
            stream_Delete(sys->s);
        sys->s = stream_UrlNew(access, sys->chunk->mrl);
    }
    return sys->s ? stream_Seek(sys->s, offset) : VLC_EGENERIC;
}

static ssize_t Read(access_t *access, uint8_t *data, size_t size)
{
    access_sys_t *sys = access->p_sys;

    size_t total = 0;
    while (total < size) {
        const uint64_t chunk_end = sys->chunk->cummulated_size + sys->chunk->size;
        int max = __MIN(__MIN((int64_t)(size - total), (int64_t)(chunk_end - access->info.i_pos)), INT_MAX);
        if (max <= 0)
            break;

        int r = sys->s ? stream_Read(sys->s, data, max) : -1;
        if (r <= 0)
            break;

        total += r;
        if( data )
            data += r;
        access->info.i_pos += r;
        if (access->info.i_pos >= chunk_end &&
            Seek(access, access->info.i_pos))
            break;
    }
    if (size > 0 && total <= 0)
        access->info.b_eof = true;
    return total;

}

static int Control(access_t *access, int query, va_list args)
{
    stream_t *s = access->p_sys->s;
    if (!s)
        return VLC_EGENERIC;

    switch (query) {
    case ACCESS_CAN_SEEK: {
        bool *b = va_arg(args, bool *);
        return stream_Control(s, STREAM_CAN_SEEK, b);
    }
    case ACCESS_CAN_FASTSEEK: {
        bool *b = va_arg(args, bool *);
        return stream_Control(s, STREAM_CAN_FASTSEEK, b);
    }
    /* FIXME the following request should ask the underlying access object */
    case ACCESS_CAN_PAUSE:
    case ACCESS_CAN_CONTROL_PACE: {
        bool *b = va_arg(args, bool *);
        *b = true;
        return VLC_SUCCESS;
    }
    case ACCESS_GET_PTS_DELAY: {
        int64_t *delay = va_arg(args, int64_t *);
        *delay = DEFAULT_PTS_DELAY;
        return VLC_SUCCESS;
    }
    case ACCESS_SET_PAUSE_STATE:
        return VLC_SUCCESS;

    default:
        return VLC_EGENERIC;
    }
}

static int Open(vlc_object_t *object)
{
    access_t *access = (access_t*)object;

    if (!strchr(access->psz_location, '|'))
        return VLC_EGENERIC;

    char *base = strdup(access->psz_location);
    if (!base)
        return VLC_EGENERIC;
    char *name = strchr(base, '|');
    *name++ = '\0';
    decode_URI(base);

    stream_t *s = stream_UrlNew(access, base);
    if (!s)
        goto error;
    int count;
    rar_file_t **files;
    if (RarProbe(s) || RarParse(s, &count, &files) || count <= 0)
        goto error;
    rar_file_t *file = NULL;
    for (int i = 0; i < count; i++) {
        if (!file && !strcmp(files[i]->name, name))
            file = files[i];
        else
            RarFileDelete(files[i]);
    }
    free(files);
    if (!file)
        goto error;

    access_sys_t *sys = access->p_sys = malloc(sizeof(*sys));
    sys->s    = s;
    sys->file = file;

    access->pf_read    = Read;
    access->pf_block   = NULL;
    access->pf_control = Control;
    access->pf_seek    = Seek;

    access_InitFields(access);
    access->info.i_size = file->size;

    rar_file_chunk_t dummy = {
        .mrl = base,
    };
    sys->chunk = &dummy;
    Seek(access, 0);

    free(base);
    return VLC_SUCCESS;

error:
    if (s)
        stream_Delete(s);
    free(base);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *object)
{
    access_t *access = (access_t*)object;
    access_sys_t *sys = access->p_sys;

    if (sys->s)
        stream_Delete(sys->s);
    RarFileDelete(sys->file);
    free(sys);
}

