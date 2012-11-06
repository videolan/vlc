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

/*****************************************************************************
 * Preamble
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_description(N_("Uncompressed RAR"))
    set_capability("stream_filter", 1)
    set_callbacks(Open, Close)
    add_shortcut("rar")
vlc_module_end()

/****************************************************************************
 * Local definitions/prototypes
 ****************************************************************************/
struct stream_sys_t {
    stream_t *payload;
};

static int Read(stream_t *s, void *data, unsigned size)
{
    return stream_Read(s->p_sys->payload, data, size);
}

static int Peek( stream_t *s, const uint8_t **data, unsigned size)
{
    return stream_Peek(s->p_sys->payload, data, size);
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
        return stream_vaControl(s->p_sys->payload, query, args);
    }
}

static int Open(vlc_object_t *object)
{
    stream_t *s = (stream_t*)object;

    if (RarProbe(s->p_source))
        return VLC_EGENERIC;

    int count;
    rar_file_t **files;
    const int64_t position = stream_Tell(s->p_source);
    if (RarParse(s->p_source, &count, &files)) {
        stream_Seek(s->p_source, position);
        msg_Err(s, "Invalid or unsupported RAR archive");
        free(files);
        return VLC_EGENERIC;
    }

    /* TODO use xspf to have node for directories
     * Reusing WriteXSPF from the zip access is probably a good idea
     * (becareful about '\' and '/'.
     */
    char *mrl;
    if (asprintf(&mrl, "%s://%s", s->psz_access, s->psz_path)< 0)
        mrl = NULL;
    char *base;
    char *encoded = mrl ? encode_URI_component(mrl) : NULL;
    free(mrl);

    if (!encoded || asprintf(&base, "rar://%s", encoded) < 0)
        base = NULL;
    free(encoded);

    char *data = strdup("#EXTM3U\n");
    for (int i = 0; i < count; i++) {
        rar_file_t *f = files[i];
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
    free(files);
    if (!data)
        return VLC_EGENERIC;
    stream_t *payload = stream_MemoryNew(s, (uint8_t*)data, strlen(data), false);
    if (!payload) {
        free(data);
        return VLC_EGENERIC;
    }

    s->pf_read = Read;
    s->pf_peek = Peek;
    s->pf_control = Control;

    stream_sys_t *sys = s->p_sys = malloc(sizeof(*sys));
    if (!sys) {
        stream_Delete(payload);
        return VLC_ENOMEM;
    }
    sys->payload = payload;

    char *tmp;
    if (asprintf(&tmp, "%s.m3u", s->psz_path) < 0) {
        Close(object);
        return VLC_ENOMEM;
    }
    free(s->psz_path);
    s->psz_path = tmp;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    stream_t *s = (stream_t*)object;
    stream_sys_t *sys = s->p_sys;

    stream_Delete(sys->payload);
    free(sys);
}

