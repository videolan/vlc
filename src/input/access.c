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
     * to hard-code the list of schemes that use file paths in vlc_uri2path().
     */
    if (asprintf(&url, "file://%s", location) == -1)
        return NULL;

    path = vlc_uri2path (url);
    free (url);
    return path;
}

static void vlc_access_Destroy(stream_t *access)
{
    module_unneed(access, access->p_module);
    free(access->psz_filepath);
    free(access->psz_name);
}

#define MAX_REDIR 5

/*****************************************************************************
 * access_New:
 *****************************************************************************/
static stream_t *access_New(vlc_object_t *parent, input_thread_t *input,
                            bool preparsing, const char *mrl)
{
    char *redirv[MAX_REDIR];
    unsigned redirc = 0;

    stream_t *access = vlc_stream_CommonNew(parent, vlc_access_Destroy);
    if (unlikely(access == NULL))
        return NULL;

    access->p_input = input;
    access->psz_name = NULL;
    access->psz_url = strdup(mrl);
    access->psz_filepath = NULL;
    access->b_preparsing = preparsing;

    if (unlikely(access->psz_url == NULL))
        goto error;

    while (redirc < MAX_REDIR)
    {
        char *url = access->psz_url;
        msg_Dbg(access, "creating access: %s", url);

        const char *p = strstr(url, "://");
        if (p == NULL)
            goto error;

        access->psz_name = strndup(url, p - url);
        if (unlikely(access->psz_name == NULL))
            goto error;

        access->psz_location = p + 3;
        access->psz_filepath = get_path(access->psz_location);
        if (access->psz_filepath != NULL)
            msg_Dbg(access, " (path: %s)", access->psz_filepath);

        access->p_module = module_need(access, "access", access->psz_name,
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

        free(access->psz_filepath);
        free(access->psz_name);
        access->psz_filepath = access->psz_name = NULL;
    }

    msg_Err(access, "too many redirections");
error:
    while (redirc > 0)
        free(redirv[--redirc]);
    free(access->psz_filepath);
    free(access->psz_name);
    stream_CommonDelete(access);
    return NULL;
}

stream_t *vlc_access_NewMRL(vlc_object_t *parent, const char *mrl)
{
    return access_New(parent, NULL, false, mrl);
}

/*****************************************************************************
 * access_vaDirectoryControlHelper:
 *****************************************************************************/
int access_vaDirectoryControlHelper( stream_t *p_access, int i_query, va_list args )
{
    VLC_UNUSED( p_access );

    switch( i_query )
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = false;
            break;
        case STREAM_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = 0;
            break;
        case STREAM_IS_DIRECTORY:
            break;
        default:
            return VLC_EGENERIC;
     }
     return VLC_SUCCESS;
}

static int AStreamNoReadDir(stream_t *s, input_item_node_t *p_node)
{
    (void) s; (void) p_node;
    return VLC_EGENERIC;;
}

/* Block access */
static block_t *AStreamReadBlock(stream_t *s, bool *restrict eof)
{
    stream_t *access = s->p_sys;
    input_thread_t *input = s->p_input;
    block_t * block;

    if (vlc_stream_Eof(access))
    {
        *eof = true;
        return NULL;
    }
    if (vlc_killed())
        return NULL;

    block = vlc_stream_ReadBlock(access);

    if (block != NULL && input != NULL)
    {
        uint64_t total;

        vlc_mutex_lock(&input_priv(input)->counters.counters_lock);
        stats_Update(input_priv(input)->counters.p_read_bytes,
                     block->i_buffer, &total);
        stats_Update(input_priv(input)->counters.p_input_bitrate, total, NULL);
        stats_Update(input_priv(input)->counters.p_read_packets, 1, NULL);
        vlc_mutex_unlock(&input_priv(input)->counters.counters_lock);
    }

    return block;
}

/* Read access */
static ssize_t AStreamReadStream(stream_t *s, void *buf, size_t len)
{
    stream_t *access = s->p_sys;
    input_thread_t *input = s->p_input;

    if (vlc_stream_Eof(access))
        return 0;
    if (vlc_killed())
        return -1;

    ssize_t val = vlc_stream_ReadPartial(access, buf, len);

    if (val > 0 && input != NULL)
    {
        uint64_t total;

        vlc_mutex_lock(&input_priv(input)->counters.counters_lock);
        stats_Update(input_priv(input)->counters.p_read_bytes, val, &total);
        stats_Update(input_priv(input)->counters.p_input_bitrate, total, NULL);
        stats_Update(input_priv(input)->counters.p_read_packets, 1, NULL);
        vlc_mutex_unlock(&input_priv(input)->counters.counters_lock);
    }

    return val;
}

/* Directory */
static int AStreamReadDir(stream_t *s, input_item_node_t *p_node)
{
    stream_t *access = s->p_sys;

    return access->pf_readdir(access, p_node);
}

/* Common */
static int AStreamSeek(stream_t *s, uint64_t offset)
{
    stream_t *access = s->p_sys;

    return vlc_stream_Seek(access, offset);
}

static int AStreamControl(stream_t *s, int cmd, va_list args)
{
    stream_t *access = s->p_sys;

    return vlc_stream_vaControl(access, cmd, args);
}

static void AStreamDestroy(stream_t *s)
{
    stream_t *access = s->p_sys;

    vlc_stream_Delete(access);
}

stream_t *stream_AccessNew(vlc_object_t *parent, input_thread_t *input,
                           bool preparsing, const char *url)
{
    stream_t *s = vlc_stream_CommonNew(parent, AStreamDestroy);
    if (unlikely(s == NULL))
        return NULL;

    stream_t *access = access_New(VLC_OBJECT(s), input, preparsing, url);
    if (access == NULL)
    {
        stream_CommonDelete(s);
        return NULL;
    }

    s->p_input = input;
    s->psz_url = strdup(access->psz_url);

    const char *cachename;

    if (access->pf_block != NULL)
    {
        s->pf_block = AStreamReadBlock;
        cachename = "prefetch,cache_block";
    }
    else
    if (access->pf_read != NULL)
    {
        s->pf_read = AStreamReadStream;
        cachename = "prefetch,cache_read";
    }
    else
    {
        cachename = NULL;
    }

    if (access->pf_readdir != NULL)
        s->pf_readdir = AStreamReadDir;
    else
        s->pf_readdir = AStreamNoReadDir;

    s->pf_seek    = AStreamSeek;
    s->pf_control = AStreamControl;
    s->p_sys      = access;

    if (cachename != NULL)
        s = stream_FilterChainNew(s, cachename);
    return stream_FilterAutoNew(s);
}
