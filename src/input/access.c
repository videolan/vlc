/*****************************************************************************
 * access.c
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_modules.h>
#include <vlc_interrupt.h>

#include <libvlc.h>
#include "stream.h"
#include "input_internal.h"

/* Decode URL (which has had its scheme stripped earlier) to a file path. */
char *get_path(const char *location)
{
    char *url, *path;

    /* Prepending "file://" is a bit hackish. But then again, we do not want
     * to hard-code the list of schemes that use file paths in make_path().
     */
    if (asprintf(&url, "file://%s", location) == -1)
        return NULL;

    path = make_path (url);
    free (url);
    return path;
}

#define MAX_REDIR 5

/*****************************************************************************
 * access_New:
 *****************************************************************************/
static access_t *access_New(vlc_object_t *parent, input_thread_t *input,
                            const char *mrl)
{
    char *redirv[MAX_REDIR];
    unsigned redirc = 0;

    access_t *access = vlc_custom_create(parent, sizeof (*access), "access");
    if (unlikely(access == NULL))
        return NULL;

    access->p_input = input;
    access->psz_access = NULL;
    access->psz_url = strdup(mrl);
    access->psz_filepath = NULL;
    access->pf_read = NULL;
    access->pf_block = NULL;
    access->pf_readdir = NULL;
    access->pf_seek = NULL;
    access->pf_control = NULL;
    access->p_sys = NULL;
    access_InitFields(access);

    if (unlikely(access->psz_url == NULL))
        goto error;

    while (redirc < MAX_REDIR)
    {
        char *url = access->psz_url;
        msg_Dbg(access, "creating access: %s", url);

        const char *p = strstr(url, "://");
        if (p == NULL)
            goto error;

        access->psz_access = strndup(url, p - url);
        if (unlikely(access->psz_access == NULL))
            goto error;

        access->psz_location = p + 3;
        access->psz_filepath = get_path(access->psz_location);
        if (access->psz_filepath != NULL)
            msg_Dbg(access, " (path: %s)", access->psz_filepath);

        access->p_module = module_need(access, "access", access->psz_access,
                                       true);
        if (access->p_module != NULL) /* success */
        {
            while (redirc > 0)
                free(redirv[--redirc]);

            assert(access->pf_control != NULL);
            return access;
        }

        if (access->psz_url == url) /* failure (no redirection) */
            goto error;

        /* redirection */
        msg_Dbg(access, "redirecting to: %s", access->psz_url);
        redirv[redirc++] = url;

        for (unsigned j = 0; j < redirc; j++)
            if (!strcmp(redirv[j], access->psz_url))
            {
                msg_Err(access, "redirection loop");
                goto error;
            }
    }

    msg_Err(access, "too many redirections");
error:
    while (redirc > 0)
        free(redirv[--redirc]);
    free(access->psz_filepath);
    free(access->psz_url);
    free(access->psz_access);
    vlc_object_release(access);
    return NULL;
}

access_t *vlc_access_NewMRL(vlc_object_t *parent, const char *mrl)
{
    return access_New(parent, NULL, mrl);
}

void vlc_access_Delete(access_t *access)
{
    module_unneed(access, access->p_module);

    free(access->psz_filepath);
    free(access->psz_url);
    free(access->psz_access);
    vlc_object_release(access);
}

/*****************************************************************************
 * access_vaDirectoryControlHelper:
 *****************************************************************************/
int access_vaDirectoryControlHelper( access_t *p_access, int i_query, va_list args )
{
    VLC_UNUSED( p_access );

    switch( i_query )
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = false;
            break;
        case ACCESS_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = 0;
            break;
        case ACCESS_IS_DIRECTORY:
            *va_arg( args, bool * ) = false;
            *va_arg( args, bool * ) = false;
            break;
        default:
            return VLC_EGENERIC;
     }
     return VLC_SUCCESS;
}

struct stream_sys_t
{
    access_t *access;
    block_t  *block;
};

static ssize_t AStreamNoRead(stream_t *s, void *buf, size_t len)
{
    (void) s; (void) buf; (void) len;
    return -1;
}

static input_item_t *AStreamNoReadDir(stream_t *s)
{
    (void) s;
    return NULL;
}

/* Block access */
static ssize_t AStreamReadBlock(stream_t *s, void *buf, size_t len)
{
    stream_sys_t *sys = s->p_sys;
    input_thread_t *input = s->p_input;
    block_t *block = sys->block;

    while (block == NULL)
    {
        if (vlc_access_Eof(sys->access))
            return 0;
        if (vlc_killed())
            return -1;

        block = vlc_access_Block(sys->access);
    }

    if (input != NULL)
    {
        uint64_t total;

        vlc_mutex_lock(&input->p->counters.counters_lock);
        stats_Update(input->p->counters.p_read_bytes, block->i_buffer, &total);
        stats_Update(input->p->counters.p_input_bitrate, total, NULL);
        stats_Update(input->p->counters.p_read_packets, 1, NULL);
        vlc_mutex_unlock(&input->p->counters.counters_lock);
    }

    size_t copy = block->i_buffer < len ? block->i_buffer : len;

    if (likely(copy > 0) && buf != NULL /* skipping data? */)
        memcpy(buf, block->p_buffer, copy);

    block->p_buffer += copy;
    block->i_buffer -= copy;

    if (block->i_buffer == 0)
        block_Release(block);
    else
        sys->block = block;

    return copy;
}

/* Read access */
static ssize_t AStreamReadStream(stream_t *s, void *buf, size_t len)
{
    stream_sys_t *sys = s->p_sys;
    input_thread_t *input = s->p_input;
    ssize_t val = 0;

    do
    {
        if (vlc_access_Eof(sys->access))
            return 0;
        if (vlc_killed())
            return -1;

        val = vlc_access_Read(sys->access, buf, len);
        if (val == 0)
            return 0; /* EOF */
    }
    while (val < 0);

    if (input != NULL)
    {
        uint64_t total;

        vlc_mutex_lock(&input->p->counters.counters_lock);
        stats_Update(input->p->counters.p_read_bytes, val, &total);
        stats_Update(input->p->counters.p_input_bitrate, total, NULL);
        stats_Update(input->p->counters.p_read_packets, 1, NULL);
        vlc_mutex_unlock(&input->p->counters.counters_lock);
    }

    return val;
}

/* Directory */
static input_item_t *AStreamReadDir(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;

    return sys->access->pf_readdir(sys->access);
}

/* Common */
static int AStreamSeek(stream_t *s, uint64_t offset)
{
    stream_sys_t *sys = s->p_sys;

    if (sys->block != NULL)
    {
        block_Release(sys->block);
        sys->block = NULL;
    }

    return vlc_access_Seek(sys->access, offset);
}

#define static_control_match(foo) \
    static_assert((unsigned) STREAM_##foo == ACCESS_##foo, "Mismatch")

static int AStreamControl(stream_t *s, int cmd, va_list args)
{
    stream_sys_t *sys = s->p_sys;
    access_t *access = sys->access;

    static_control_match(CAN_SEEK);
    static_control_match(CAN_FASTSEEK);
    static_control_match(CAN_PAUSE);
    static_control_match(CAN_CONTROL_PACE);
    static_control_match(GET_SIZE);
    static_control_match(IS_DIRECTORY);
    static_control_match(GET_PTS_DELAY);
    static_control_match(GET_TITLE_INFO);
    static_control_match(GET_TITLE);
    static_control_match(GET_SEEKPOINT);
    static_control_match(GET_META);
    static_control_match(GET_CONTENT_TYPE);
    static_control_match(GET_SIGNAL);
    static_control_match(SET_PAUSE_STATE);
    static_control_match(SET_TITLE);
    static_control_match(SET_SEEKPOINT);
    static_control_match(SET_PRIVATE_ID_STATE);
    static_control_match(SET_PRIVATE_ID_CA);
    static_control_match(GET_PRIVATE_ID_STATE);

    switch (cmd)
    {
        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
        {
            int ret = access_vaControl(access, cmd, args);
            if (ret != VLC_SUCCESS)
                return ret;

            if (sys->block != NULL)
            {
                block_Release(sys->block);
                sys->block = NULL;
            }
            break;
        }

        case STREAM_GET_PRIVATE_BLOCK:
        {
            block_t **b = va_arg(args, block_t **);
            bool *eof = va_arg(args, bool *);

            if (access->pf_block == NULL)
                return VLC_EGENERIC;

            *b = vlc_access_Eof(access) ? NULL : vlc_access_Block(access);
            *eof = (*b == NULL) && vlc_access_Eof(access);
            break;
        }

        default:
            return access_vaControl(access, cmd, args);
    }
    return VLC_SUCCESS;
}

static void AStreamDestroy(stream_t *s)
{
    stream_sys_t *sys = s->p_sys;

    if (sys->block != NULL)
        block_Release(sys->block);
    vlc_access_Delete(sys->access);
    free(sys);
}

stream_t *stream_AccessNew(vlc_object_t *parent, input_thread_t *input,
                           const char *url)
{
    stream_t *s = stream_CommonNew(parent, AStreamDestroy);
    if (unlikely(s == NULL))
        return NULL;

    stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        goto error;

    sys->access = access_New(VLC_OBJECT(s), input, url);
    if (sys->access == NULL)
        goto error;

    sys->block = NULL;
    s->p_input = input;
    s->psz_url = strdup(sys->access->psz_url);

    const char *cachename;

    if (sys->access->pf_block != NULL)
    {
        s->pf_read = AStreamReadBlock;
        cachename = "cache_block";
    }
    else
    if (sys->access->pf_read != NULL)
    {
        s->pf_read = AStreamReadStream;
        cachename = "prefetch,cache_read";
    }
    else
    {
        s->pf_read = AStreamNoRead;
        cachename = NULL;
    }

    if (sys->access->pf_readdir != NULL)
        s->pf_readdir = AStreamReadDir;
    else
        s->pf_readdir = AStreamNoReadDir;

    s->pf_seek    = AStreamSeek;
    s->pf_control = AStreamControl;
    s->p_sys      = sys;

    if (cachename != NULL)
        s = stream_FilterChainNew(s, cachename);
    return s;
error:
    free(sys);
    stream_CommonDelete(s);
    return NULL;
}
