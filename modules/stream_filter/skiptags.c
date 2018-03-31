/*****************************************************************************
 * skiptags.c:
 *****************************************************************************
 * Copyright © 2005-2008 VLC authors and VideoLAN
 * Copyright © 2016-2017 Rémi Denis-Courmont
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
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_block.h>

#define MAX_TAGS 16
#define MAX_TAG_SIZE (1<<17)

struct skiptags_sys_t
{
    uint64_t header_skip;
    /* TODO? also discard trailer tags? */
    block_t *p_tags;
};

static struct skiptags_sys_t * skiptags_sys_New(void)
{
    struct skiptags_sys_t *sys = malloc(sizeof (*sys));
    if(sys)
    {
        sys->header_skip = 0;
        sys->p_tags = NULL;
    }
    return sys;
}

static void skiptags_sys_Delete(struct skiptags_sys_t *sys)
{
    block_ChainRelease(sys->p_tags);
    free(sys);
}

static uint_fast32_t SkipID3Tag(stream_t *s)
{
    const uint8_t *peek;

    /* Get 10-bytes ID3 header */
    if (vlc_stream_Peek(s, &peek, 10) < 10)
        return 0;
    if (memcmp(peek, "ID3", 3))
        return 0;

    uint_fast8_t version = peek[3];
    uint_fast8_t revision = peek[4];
    bool has_footer = (peek[5] & 0x10) != 0;
    uint_fast32_t size = 10u + (peek[6] << 21) + (peek[7] << 14)
                         + (peek[8] << 7) + peek[9] + (has_footer ? 10u : 0u);

    /* Skip the entire tag */
    msg_Dbg(s, "ID3v2.%"PRIuFAST8" revision %"PRIuFAST8" tag found, "
            "skipping %"PRIuFAST32" bytes", version, revision, size);

    return size;
}

static uint_fast32_t SkipAPETag(stream_t *s)
{
    const uint8_t *peek;

    /* Get 32-bytes APE header */
    if (vlc_stream_Peek(s, &peek, 32) < 32)
        return 0;
    if (memcmp(peek, "APETAGEX", 8))
        return 0;

    uint_fast32_t version = GetDWLE(peek + 8);
    if (version != 1000 && version != 2000)
        return 0;

    uint_fast32_t size = GetDWLE(peek + 12);
    if (size > SSIZE_MAX - 32u)
        return 0; /* impossibly long tag */

    uint_fast32_t flags = GetDWLE(peek + 16);
    if ((flags & (1u << 29)) == 0)
        return 0;

    if (flags & (1u << 30))
        size += 32;

    msg_Dbg(s, "AP2 v%"PRIuFAST32" tag found, "
            "skipping %"PRIuFAST32" bytes", version / 1000, size);
    return size;
}

static bool SkipTag(stream_t *s, uint_fast32_t (*skipper)(stream_t *),
                    block_t **pp_block, unsigned *pi_tags_count)
{
    uint_fast64_t offset = vlc_stream_Tell(s);
    uint_fast32_t size = skipper(s);
    if(size> 0)
    {
        /* Skip the entire tag */
        ssize_t read;
        if(*pi_tags_count < MAX_TAGS && size <= MAX_TAG_SIZE)
        {
            *pp_block = vlc_stream_Block(s, size);
            read = *pp_block ? (ssize_t)(*pp_block)->i_buffer : -1;
        }
        else
        {
            read = vlc_stream_Read(s, NULL, size);
        }

        if(read < (ssize_t)size)
        {
            block_ChainRelease(*pp_block);
            *pp_block = NULL;
            if (unlikely(read < 0))
            {   /* I/O error, try to restore offset. If it fails, screwed. */
                if (vlc_stream_Seek(s, offset))
                    msg_Err(s, "seek failure");
                return false;
            }
        }
        else (*pi_tags_count)++;
    }
    return size != 0;
}

static ssize_t Read(stream_t *stream, void *buf, size_t buflen)
{
    return vlc_stream_Read(stream->s, buf, buflen);
}

static int Seek(stream_t *stream, uint64_t offset)
{
    const struct skiptags_sys_t *sys = stream->p_sys;

    if (unlikely(offset + sys->header_skip < offset))
        return -1;
    return vlc_stream_Seek(stream->s, sys->header_skip + offset);
}

static int Control(stream_t *stream, int query, va_list args)
{
    const struct skiptags_sys_t *sys = stream->p_sys;

    /* In principles, we should return the meta-data embedded in the skipped
     * tags in STREAM_GET_META. But the meta engine is devoted to that already.
     */
    switch (query)
    {
        case STREAM_GET_TAGS:
            if (sys->p_tags == NULL)
                break;
            *va_arg(args, const block_t **) = sys->p_tags;
            return VLC_SUCCESS;

        case STREAM_GET_SIZE:
        {
            uint64_t size;
            int ret = vlc_stream_GetSize(stream->s, &size);
            if (ret == VLC_SUCCESS)
                *va_arg(args, uint64_t *) = size - sys->header_skip;
            return ret;
        }
    }

    return vlc_stream_vaControl(stream->s, query, args);
}

static int Open(vlc_object_t *obj)
{
    stream_t *stream = (stream_t *)obj;
    stream_t *s = stream->s;
    struct skiptags_sys_t *sys;

    block_t *p_tags = NULL, *p_tag = NULL;
    unsigned i_tagscount = 0;

    while (SkipTag(s, SkipID3Tag, &p_tag, &i_tagscount)||
           SkipTag(s, SkipAPETag, &p_tag, &i_tagscount))
    {
        if(p_tag)
        {
            p_tag->p_next = p_tags;
            p_tags = p_tag;
            p_tag = NULL;
        }
    }

    uint_fast64_t offset = vlc_stream_Tell(s);
    if (offset == 0 || !(sys = skiptags_sys_New()))
    {
        block_ChainRelease( p_tags );
        return VLC_EGENERIC; /* nothing to do */
    }

    sys->header_skip = offset;
    sys->p_tags = p_tags;
    stream->p_sys = sys;
    stream->pf_read = Read;
    stream->pf_seek = Seek;
    stream->pf_control = Control;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    stream_t *stream = (stream_t *)obj;
    struct skiptags_sys_t *sys = (struct skiptags_sys_t *) stream->p_sys;

    skiptags_sys_Delete(sys);
}

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_capability("stream_filter", 330)

    set_description(N_("APE/ID3 tags-skipping filter"))
    set_callbacks(Open, Close)
vlc_module_end()
