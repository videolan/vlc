/*****************************************************************************
 * stream.c: uncompressed RAR stream filter
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
#include <vlc_stream.h>
#include <vlc_url.h>

#include <assert.h>
#include <limits.h>

#include "rar.h"

static ssize_t Read(stream_t *s, void *data, size_t size)
{
    return vlc_stream_Read(s->p_sys, data, size);
}

static int Seek(stream_t *s, uint64_t offset)
{
    return vlc_stream_Seek(s->p_sys, offset);
}

static int Control(stream_t *s, int query, va_list args)
{
    switch (query) {
    case STREAM_GET_CONTENT_TYPE: {
        char **mime = va_arg(args, char **);
        *mime = strdup("audio/x-mpegurl");
        return VLC_SUCCESS;
    }
    default:
        return vlc_stream_vaControl(s->p_sys, query, args);
    }
}

int RarStreamOpen(vlc_object_t *object)
{
    stream_t *s = (stream_t*)object;

    if( s->psz_url == NULL )
        return VLC_EGENERIC;

    if (RarProbe(s->p_source))
        return VLC_EGENERIC;

    struct
    {
        int filescount;
        rar_file_t **files;
        unsigned int i_nbvols;
    } newscheme = { 0, NULL, 0 }, oldscheme = { 0, NULL, 0 }, *p_scheme;

    const int64_t position = vlc_stream_Tell(s->p_source);

    if (RarParse(s->p_source, &newscheme.filescount, &newscheme.files, &newscheme.i_nbvols, false)
            || (newscheme.filescount < 2 && newscheme.i_nbvols < 2) )
    {
        /* We might want to lookup old naming scheme, could be a part1.rar,part1.r00 */
        vlc_stream_Seek(s->p_source, 0);
        RarParse(s->p_source, &oldscheme.filescount, &oldscheme.files, &oldscheme.i_nbvols, true );
    }

    if (oldscheme.filescount >= newscheme.filescount && oldscheme.i_nbvols > newscheme.i_nbvols)
    {
        free(newscheme.files);
        p_scheme = &oldscheme;
    }
    else if (newscheme.filescount)
    {
        free(oldscheme.files);
        p_scheme = &newscheme;
    }
    else
    {
        vlc_stream_Seek(s->p_source, position);
        msg_Info(s, "Invalid or unsupported RAR archive");
        free(oldscheme.files);
        free(newscheme.files);
        return VLC_EGENERIC;
    }

    /* TODO use xspf to have node for directories
     * Reusing WriteXSPF from the zip access is probably a good idea
     * (becareful about '\' and '/'.
     */
    char *base;
    char *encoded = vlc_uri_encode(s->psz_url);
    if (!encoded || asprintf(&base, "rar://%s", encoded) < 0)
        base = NULL;
    free(encoded);

    char *data = strdup("#EXTM3U\n");
    for (int i = 0; i < p_scheme->filescount; i++) {
        rar_file_t *f = p_scheme->files[i];
        char *next;
        if (base && data &&
            asprintf(&next, "%s"
                            "#EXTINF:,,%s\n"
                            "%s|%s\n",
                            data, f->name, base, f->name) >= 0) {
            free(data);
            data = next;
        }
        RarFileDelete(f);
    }
    free(base);
    free(p_scheme->files);
    if (!data)
        return VLC_EGENERIC;
    stream_t *payload = vlc_stream_MemoryNew(s, (uint8_t*)data, strlen(data), false);
    if (!payload) {
        free(data);
        return VLC_EGENERIC;
    }

    s->pf_read = Read;
    s->pf_seek = Seek;
    s->pf_control = Control;
    s->p_sys = payload;

    char *tmp;
    if (asprintf(&tmp, "%s.m3u", s->psz_url) < 0) {
        RarStreamClose(object);
        return VLC_ENOMEM;
    }
    free(s->psz_url);
    s->psz_url = tmp;

    return VLC_SUCCESS;
}

void RarStreamClose(vlc_object_t *object)
{
    stream_t *s = (stream_t*)object;

    vlc_stream_Delete(s->p_sys);
}
