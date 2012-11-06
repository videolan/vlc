/*****************************************************************************
 * rar.c: uncompressed RAR parser
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

#include <assert.h>
#include <limits.h>

#include "rar.h"

static const uint8_t rar_marker[] = {
    0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00
};
static const int rar_marker_size = sizeof(rar_marker);

void RarFileDelete(rar_file_t *file)
{
    for (int i = 0; i < file->chunk_count; i++) {
        free(file->chunk[i]->mrl);
        free(file->chunk[i]);
    }
    free(file->chunk);
    free(file->name);
    free(file);
}

typedef struct {
    uint16_t crc;
    uint8_t  type;
    uint16_t flags;
    uint16_t size;
    uint32_t add_size;
} rar_block_t;

enum {
    RAR_BLOCK_MARKER = 0x72,
    RAR_BLOCK_ARCHIVE = 0x73,
    RAR_BLOCK_FILE = 0x74,
    RAR_BLOCK_SUBBLOCK = 0x7a,
    RAR_BLOCK_END = 0x7b,
};
enum {
    RAR_BLOCK_END_HAS_NEXT = 0x0001,
};
enum {
    RAR_BLOCK_FILE_HAS_PREVIOUS = 0x0001,
    RAR_BLOCK_FILE_HAS_NEXT     = 0x0002,
    RAR_BLOCK_FILE_HAS_HIGH     = 0x0100,
};

static int PeekBlock(stream_t *s, rar_block_t *hdr)
{
    const uint8_t *peek;
    int peek_size = stream_Peek(s, &peek, 11);

    if (peek_size < 7)
        return VLC_EGENERIC;

    hdr->crc   = GetWLE(&peek[0]);
    hdr->type  = peek[2];
    hdr->flags = GetWLE(&peek[3]);
    hdr->size  = GetWLE(&peek[5]);
    hdr->add_size = 0;
    if ((hdr->flags & 0x8000) ||
        hdr->type == RAR_BLOCK_FILE ||
        hdr->type == RAR_BLOCK_SUBBLOCK) {
        if (peek_size < 11)
            return VLC_EGENERIC;
        hdr->add_size = GetDWLE(&peek[7]);
    }

    if (hdr->size < 7)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}
static int SkipBlock(stream_t *s, const rar_block_t *hdr)
{
    uint64_t size = (uint64_t)hdr->size + hdr->add_size;

    while (size > 0) {
        int skip = __MIN(size, INT_MAX);
        if (stream_Read(s, NULL, skip) < skip)
            return VLC_EGENERIC;

        size -= skip;
    }
    return VLC_SUCCESS;
}

static int IgnoreBlock(stream_t *s, int block)
{
    /* */
    rar_block_t bk;
    if (PeekBlock(s, &bk) || bk.type != block)
        return VLC_EGENERIC;
    return SkipBlock(s, &bk);
}

static int SkipEnd(stream_t *s, const rar_block_t *hdr)
{
    if (!(hdr->flags & RAR_BLOCK_END_HAS_NEXT))
        return VLC_EGENERIC;

    if (SkipBlock(s, hdr))
        return VLC_EGENERIC;

    /* Now, we need to look for a marker block,
     * It seems that there is garbage at EOF */
    for (;;) {
        const uint8_t *peek;

        if (stream_Peek(s, &peek, rar_marker_size) < rar_marker_size)
            return VLC_EGENERIC;

        if (!memcmp(peek, rar_marker, rar_marker_size))
            break;

        if (stream_Read(s, NULL, 1) != 1)
            return VLC_EGENERIC;
    }

    /* Skip marker and archive blocks */
    if (IgnoreBlock(s, RAR_BLOCK_MARKER))
        return VLC_EGENERIC;
    if (IgnoreBlock(s, RAR_BLOCK_ARCHIVE))
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int SkipFile(stream_t *s, int *count, rar_file_t ***file,
                    const rar_block_t *hdr, const char *volume_mrl)
{
    const uint8_t *peek;

    int min_size = 7+21;
    if (hdr->flags & RAR_BLOCK_FILE_HAS_HIGH)
        min_size += 8;
    if (hdr->size < (unsigned)min_size)
        return VLC_EGENERIC;

    if (stream_Peek(s, &peek, min_size) < min_size)
        return VLC_EGENERIC;

    /* */
    uint32_t file_size_low = GetDWLE(&peek[7+4]);
    uint8_t  method = peek[7+18];
    uint16_t name_size = GetWLE(&peek[7+19]);
    uint32_t file_size_high = 0;
    if (hdr->flags & RAR_BLOCK_FILE_HAS_HIGH)
        file_size_high = GetDWLE(&peek[7+29]);
    const uint64_t file_size = ((uint64_t)file_size_high << 32) | file_size_low;

    char *name = calloc(1, name_size + 1);
    if (!name)
        return VLC_EGENERIC;

    const int name_offset = (hdr->flags & RAR_BLOCK_FILE_HAS_HIGH) ? (7+33) : (7+25);
    if (name_offset + name_size <= hdr->size) {
        const int max_size = name_offset + name_size;
        if (stream_Peek(s, &peek, max_size) < max_size) {
            free(name);
            return VLC_EGENERIC;
        }
        memcpy(name, &peek[name_offset], name_size);
    }

    rar_file_t *current = NULL;
    if (method != 0x30) {
        msg_Warn(s, "Ignoring compressed file %s (method=0x%2.2x)", name, method);
        goto exit;
    }

    /* */
    if( *count > 0 )
        current = (*file)[*count - 1];

    if (current &&
        (current->is_complete ||
          strcmp(current->name, name) ||
          (hdr->flags & RAR_BLOCK_FILE_HAS_PREVIOUS) == 0))
        current = NULL;

    if (!current) {
        if (hdr->flags & RAR_BLOCK_FILE_HAS_PREVIOUS)
            goto exit;
        current = malloc(sizeof(*current));
        if (!current)
            goto exit;
        TAB_APPEND(*count, *file, current);

        current->name = name;
        current->size = file_size;
        current->is_complete = false;
        current->real_size = 0;
        TAB_INIT(current->chunk_count, current->chunk);

        name = NULL;
    }

    /* Append chunks */
    rar_file_chunk_t *chunk = malloc(sizeof(*chunk));
    if (chunk) {
        chunk->mrl = strdup(volume_mrl);
        chunk->offset = stream_Tell(s) + hdr->size;
        chunk->size = hdr->add_size;
        chunk->cummulated_size = 0;
        if (current->chunk_count > 0) {
            rar_file_chunk_t *previous = current->chunk[current->chunk_count-1];

            chunk->cummulated_size += previous->cummulated_size +
                                      previous->size;
        }

        TAB_APPEND(current->chunk_count, current->chunk, chunk);

        current->real_size += hdr->add_size;
    }
    if ((hdr->flags & RAR_BLOCK_FILE_HAS_NEXT) == 0)
        current->is_complete = true;

exit:
    /* */
    free(name);

    /* We stop on the first non empty file if we cannot seek */
    if (current) {
        bool can_seek = false;
        stream_Control(s, STREAM_CAN_SEEK, &can_seek);
        if (!can_seek && current->size > 0)
            return VLC_EGENERIC;
    }

    if (SkipBlock(s, hdr))
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

int RarProbe(stream_t *s)
{
    const uint8_t *peek;
    if (stream_Peek(s, &peek, rar_marker_size) < rar_marker_size)
        return VLC_EGENERIC;
    if (memcmp(peek, rar_marker, rar_marker_size))
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

typedef struct {
    const char *match;
    const char *format;
    int start;
    int stop;
} rar_pattern_t;

static const rar_pattern_t *FindVolumePattern(const char *location)
{
    static const rar_pattern_t patterns[] = {
        { ".part1.rar",   "%s.part%.1d.rar", 2,   9 },
        { ".part01.rar",  "%s.part%.2d.rar", 2,  99, },
        { ".part001.rar", "%s.part%.3d.rar", 2, 999 },
        { ".rar",         "%s.%c%.2d",       0, 999 },
        { NULL, NULL, 0, 0 },
    };

    const size_t location_size = strlen(location);
    for (int i = 0; patterns[i].match != NULL; i++) {
        const size_t match_size = strlen(patterns[i].match);

        if (location_size < match_size)
            continue;
        if (!strcmp(&location[location_size - match_size], patterns[i].match))
            return &patterns[i];
    }
    return NULL;
}

int RarParse(stream_t *s, int *count, rar_file_t ***file)
{
    *count = 0;
    *file = NULL;

    const rar_pattern_t *pattern = FindVolumePattern(s->psz_path);
    int volume_offset = 0;

    char *volume_mrl;
    if (asprintf(&volume_mrl, "%s://%s",
                 s->psz_access, s->psz_path) < 0)
        return VLC_EGENERIC;

    stream_t *vol = s;
    for (;;) {
        /* Skip marker & archive */
        if (IgnoreBlock(vol, RAR_BLOCK_MARKER) ||
            IgnoreBlock(vol, RAR_BLOCK_ARCHIVE)) {
            if (vol != s)
                stream_Delete(vol);
            free(volume_mrl);
            return VLC_EGENERIC;
        }

        /* */
        int has_next = -1;
        for (;;) {
            rar_block_t bk;
            int ret;

            if (PeekBlock(vol, &bk))
                break;

            switch(bk.type) {
            case RAR_BLOCK_END:
                ret = SkipEnd(vol, &bk);
                has_next = ret && (bk.flags & RAR_BLOCK_END_HAS_NEXT);
                break;
            case RAR_BLOCK_FILE:
                ret = SkipFile(vol, count, file, &bk, volume_mrl);
                break;
            default:
                ret = SkipBlock(vol, &bk);
                break;
            }
            if (ret)
                break;
        }
        if (has_next < 0 && *count > 0 && !(*file)[*count -1]->is_complete)
            has_next = 1;
        if (vol != s)
            stream_Delete(vol);

        if (!has_next || !pattern) {
            free(volume_mrl);
            return VLC_SUCCESS;
        }

        /* Open next volume */
        const int volume_index = pattern->start + volume_offset++;
        if (volume_index > pattern->stop) {
            free(volume_mrl);
            return VLC_SUCCESS;
        }

        char *volume_base;
        if (asprintf(&volume_base, "%s://%.*s",
                     s->psz_access,
                     (int)(strlen(s->psz_path) - strlen(pattern->match)), s->psz_path) < 0) {
            free(volume_mrl);
            return VLC_SUCCESS;
        }

        free(volume_mrl);
        if (pattern->start) {
            if (asprintf(&volume_mrl, pattern->format, volume_base, volume_index) < 0)
                volume_mrl = NULL;
        } else {
            if (asprintf(&volume_mrl, pattern->format, volume_base,
                         'r' + volume_index / 100, volume_index % 100) < 0)
                volume_mrl = NULL;
        }
        free(volume_base);

        if (!volume_mrl)
            return VLC_SUCCESS;

        const int s_flags = s->i_flags;
        if (has_next < 0)
            s->i_flags |= OBJECT_FLAGS_NOINTERACT;
        vol = stream_UrlNew(s, volume_mrl);
        s->i_flags = s_flags;

        if (!vol) {
            free(volume_mrl);
            return VLC_SUCCESS;
        }
    }
}

