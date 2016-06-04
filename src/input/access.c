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
#include <ctype.h>

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

#define MAX_REDIR 5

/*****************************************************************************
 * access_New:
 *****************************************************************************/
static access_t *access_New(vlc_object_t *parent, input_thread_t *input,
                            bool preparsing, const char *mrl)
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
    access->b_preparsing = preparsing;
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
    return access_New(parent, NULL, false, mrl);
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

static int AStreamNoReadDir(stream_t *s, input_item_node_t *p_node)
{
    (void) s; (void) p_node;
    return VLC_EGENERIC;;
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
    {
        block_Release(block);
        sys->block = NULL;
    }
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
static int AStreamReadDir(stream_t *s, input_item_node_t *p_node)
{
    stream_sys_t *sys = s->p_sys;

    return sys->access->pf_readdir(sys->access, p_node);
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
                           bool preparsing, const char *url)
{
    stream_t *s = stream_CommonNew(parent, AStreamDestroy);
    if (unlikely(s == NULL))
        return NULL;

    stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        goto error;

    sys->access = access_New(VLC_OBJECT(s), input, preparsing, url);
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

static int compar_type(input_item_t *p1, input_item_t *p2)
{
    if (p1->i_type != p2->i_type)
    {
        if (p1->i_type == ITEM_TYPE_DIRECTORY)
            return -1;
        if (p2->i_type == ITEM_TYPE_DIRECTORY)
            return 1;
    }
    return 0;
}

static int compar_collate(input_item_t *p1, input_item_t *p2)
{
    int i_ret = compar_type(p1, p2);

    if (i_ret != 0)
        return i_ret;

#ifdef HAVE_STRCOLL
    /* The program's LOCAL defines if case is ignored */
    return strcoll(p1->psz_name, p2->psz_name);
#else
    return strcasecmp(p1->psz_name, p2->psz_name);
#endif
}

static int compar_version(input_item_t *p1, input_item_t *p2)
{
    int i_ret = compar_type(p1, p2);

    if (i_ret != 0)
        return i_ret;

    return strverscmp(p1->psz_name, p2->psz_name);
}

static void fsdir_sort(struct access_fsdir *p_fsdir)
{
    input_item_compar_cb pf_compar = NULL;

    if (p_fsdir->psz_sort != NULL)
    {
        if (!strcasecmp(p_fsdir->psz_sort, "version"))
            pf_compar = compar_version;
        else if(strcasecmp(p_fsdir->psz_sort, "none"))
            pf_compar = compar_collate;

        if (pf_compar != NULL)
            input_item_node_Sort(p_fsdir->p_node, pf_compar);
    }
}

/**
 * Does the provided file name has one of the extension provided ?
 */
static bool fsdir_has_ext(const char *psz_filename,
                          const char *psz_ignored_exts)
{
    if (psz_ignored_exts == NULL)
        return false;

    const char *ext = strrchr(psz_filename, '.');
    if (ext == NULL)
        return false;

    size_t extlen = strlen(++ext);

    for (const char *type = psz_ignored_exts, *end; type[0]; type = end + 1)
    {
        end = strchr(type, ',');
        if (end == NULL)
            end = type + strlen(type);

        if (type + extlen == end && !strncasecmp(ext, type, extlen))
            return true;

        if (*end == '\0')
            break;
    }

    return false;
}

static bool fsdir_is_ignored(struct access_fsdir *p_fsdir,
                             const char *psz_filename)
{
    return (psz_filename[0] == '\0'
         || strcmp(psz_filename, ".") == 0
         || strcmp(psz_filename, "..") == 0
         || (!p_fsdir->b_show_hiddenfiles && psz_filename[0] == '.')
         || fsdir_has_ext(psz_filename, p_fsdir->psz_ignored_exts));
}

struct fsdir_slave
{
    input_item_slave_t *p_slave;
    char *psz_filename;
    input_item_node_t *p_node;
};

static char *fsdir_name_from_filename(const char *psz_filename)
{
    /* remove leading white spaces */
    while (*psz_filename != '\0' && *psz_filename == ' ')
        psz_filename++;

    char *psz_name = strdup(psz_filename);
    if (!psz_name)
        return NULL;

    /* remove extension */
    char *psz_ptr = strrchr(psz_name, '.');
    if (psz_ptr)
        *psz_ptr = '\0';

    /* remove trailing white spaces */
    int i = strlen(psz_name) - 1;
    while (psz_name[i] == ' ' && i >= 0)
        psz_name[i--] = '\0';

    /* convert to lower case */
    psz_ptr = psz_name;
    while (*psz_ptr != '\0')
    {
        *psz_ptr = tolower(*psz_ptr);
        psz_ptr++;
    }

    return psz_name;
}

static uint8_t fsdir_get_slave_priority(input_item_t *p_item,
                                        input_item_slave_t *p_slave,
                                        const char *psz_slave_filename)
{
    uint8_t i_priority = SLAVE_PRIORITY_MATCH_NONE;
    char *psz_item_name = fsdir_name_from_filename(p_item->psz_name);
    char *psz_slave_name = fsdir_name_from_filename(psz_slave_filename);

    if (!psz_item_name || !psz_slave_name)
        goto done;

    /* check if the names match exactly */
    if (!strcmp(psz_item_name, psz_slave_name))
    {
        i_priority = SLAVE_PRIORITY_MATCH_ALL;
        goto done;
    }

    /* "cdg" slaves have to be a full match */
    if (p_slave->i_type == SLAVE_TYPE_SPU)
    {
        char *psz_ext = strrchr(psz_slave_name, '.');
        if (psz_ext != NULL && strcasecmp(++psz_ext, "cdg") == 0)
            goto done;
    }

    /* check if the item name is a substring of the slave name */
    const char *psz_sub = strstr(psz_slave_name, psz_item_name);

    if (psz_sub)
    {
        /* check if the item name was found at the end of the slave name */
        if (strlen(psz_sub + strlen(psz_item_name)) == 0)
        {
            i_priority = SLAVE_PRIORITY_MATCH_RIGHT;
            goto done;
        }
        else
        {
            i_priority = SLAVE_PRIORITY_MATCH_LEFT;
            goto done;
        }
    }

done:
    free(psz_item_name);
    free(psz_slave_name);
    return i_priority;
}

static int fsdir_should_match_idx(struct access_fsdir *p_fsdir,
                                  struct fsdir_slave *p_fsdir_sub)
{
    char *psz_ext = strrchr(p_fsdir_sub->psz_filename, '.');
    if (!psz_ext)
        return false;
    psz_ext++;

    if (strcasecmp(psz_ext, "sub") != 0)
        return false;

    for (unsigned int i = 0; i < p_fsdir->i_slaves; i++)
    {
        struct fsdir_slave *p_fsdir_slave = p_fsdir->pp_slaves[i];

        if (p_fsdir_slave == NULL || p_fsdir_slave == p_fsdir_sub)
            continue;

        /* check that priorities match */
        if (p_fsdir_slave->p_slave->i_priority !=
            p_fsdir_sub->p_slave->i_priority)
            continue;

        /* check that the filenames without extension match */
        if (strncasecmp(p_fsdir_sub->psz_filename, p_fsdir_slave->psz_filename,
                        strlen(p_fsdir_sub->psz_filename) - 3 ) != 0)
            continue;

        /* check that we have an idx file */
        char *psz_ext_idx = strrchr(p_fsdir_slave->psz_filename, '.');
        if (psz_ext_idx == NULL)
            continue;
        psz_ext_idx++;
        if (strcasecmp(psz_ext_idx, "idx" ) == 0)
            return true;
    }
    return false;
}

static void fsdir_attach_slaves(struct access_fsdir *p_fsdir)
{
    if (p_fsdir->i_sub_autodetect_fuzzy == 0)
        return;

    /* Try to match slaves for each items of the node */
    for (int i = 0; i < p_fsdir->p_node->i_children; i++)
    {
        input_item_node_t *p_node = p_fsdir->p_node->pp_children[i];
        input_item_t *p_item = p_node->p_item;

        for (unsigned int j = 0; j < p_fsdir->i_slaves; j++)
        {
            struct fsdir_slave *p_fsdir_slave = p_fsdir->pp_slaves[j];

            /* Don't try to match slaves with themselves or slaves already
             * attached with the higher priority */
            if (p_fsdir_slave->p_node == p_node
             || p_fsdir_slave->p_slave->i_priority == SLAVE_PRIORITY_MATCH_ALL)
                continue;

            uint8_t i_priority =
                fsdir_get_slave_priority(p_item, p_fsdir_slave->p_slave,
                                         p_fsdir_slave->psz_filename);

            if (i_priority < p_fsdir->i_sub_autodetect_fuzzy)
                continue;

            /* Drop the ".sub" slave if a ".idx" slave matches */
            if (p_fsdir_slave->p_slave->i_type == SLAVE_TYPE_SPU
             && fsdir_should_match_idx(p_fsdir, p_fsdir_slave))
                continue;

            input_item_slave_t *p_slave =
                input_item_slave_New(p_fsdir_slave->p_slave->psz_uri,
                                     p_fsdir_slave->p_slave->i_type,
                                     i_priority);
            if (p_slave == NULL)
                break;

            if (input_item_AddSlave(p_item, p_slave) != VLC_SUCCESS)
            {
                input_item_slave_Delete(p_slave);
                break;
            }

            /* Remove the corresponding node if any: This slave won't be
             * added in the parent node */
            if (p_fsdir_slave->p_node != NULL)
            {
                input_item_node_Delete(p_fsdir_slave->p_node);
                p_fsdir_slave->p_node = NULL;
            }

            p_fsdir_slave->p_slave->i_priority = i_priority;
        }
    }
}

void access_fsdir_init(struct access_fsdir *p_fsdir,
                       access_t *p_access, input_item_node_t *p_node)
{
    p_fsdir->p_node = p_node;
    p_fsdir->b_show_hiddenfiles = var_InheritBool(p_access, "show-hiddenfiles");
    p_fsdir->psz_ignored_exts = var_InheritString(p_access, "ignore-filetypes");
    p_fsdir->psz_sort = var_InheritString(p_access, "directory-sort");
    bool b_autodetect = var_InheritBool(p_access, "sub-autodetect-file");
    p_fsdir->i_sub_autodetect_fuzzy = !b_autodetect ? 0 : 
        var_InheritInteger(p_access, "sub-autodetect-fuzzy");
    TAB_INIT(p_fsdir->i_slaves, p_fsdir->pp_slaves);
}

void access_fsdir_finish(struct access_fsdir *p_fsdir, bool b_success)
{
    if (b_success)
    {
        fsdir_attach_slaves(p_fsdir);
        fsdir_sort(p_fsdir);
    }
    free(p_fsdir->psz_ignored_exts);
    free(p_fsdir->psz_sort);

    /* Remove unmatched slaves */
    for (unsigned int i = 0; i < p_fsdir->i_slaves; i++)
    {
        struct fsdir_slave *p_fsdir_slave = p_fsdir->pp_slaves[i];
        if (p_fsdir_slave != NULL)
        {
            input_item_slave_Delete(p_fsdir_slave->p_slave);
            free(p_fsdir_slave->psz_filename);
            free(p_fsdir_slave);
        }
    }
    TAB_CLEAN(p_fsdir->i_slaves, p_fsdir->pp_slaves);
}

int access_fsdir_additem(struct access_fsdir *p_fsdir,
                         const char *psz_uri, const char *psz_filename,
                         int i_type, int i_net)
{
    enum slave_type i_slave_type;
    struct fsdir_slave *p_fsdir_slave = NULL;
    input_item_node_t *p_node;

    if (p_fsdir->i_sub_autodetect_fuzzy != 0
     && input_item_slave_GetType(psz_filename, &i_slave_type))
    {
        p_fsdir_slave = malloc(sizeof(*p_fsdir_slave));
        if (!p_fsdir_slave)
            return VLC_ENOMEM;

        p_fsdir_slave->p_node = NULL;
        p_fsdir_slave->psz_filename = strdup(psz_filename);
        p_fsdir_slave->p_slave = input_item_slave_New(psz_uri, i_slave_type,
                                                      SLAVE_PRIORITY_MATCH_NONE);
        if (!p_fsdir_slave->p_slave || !p_fsdir_slave->psz_filename)
        {
            free(p_fsdir_slave->psz_filename);
            free(p_fsdir_slave);
            return VLC_ENOMEM;
        }

        INSERT_ELEM(p_fsdir->pp_slaves, p_fsdir->i_slaves,
                    p_fsdir->i_slaves, p_fsdir_slave);
    }

    if (fsdir_is_ignored(p_fsdir, psz_filename))
        return VLC_SUCCESS;

    input_item_t *p_item = input_item_NewExt(psz_uri, psz_filename, -1,
                                             i_type, i_net);
    if (p_item == NULL)
        return VLC_ENOMEM;

    input_item_CopyOptions(p_item, p_fsdir->p_node->p_item);
    p_node = input_item_node_AppendItem(p_fsdir->p_node, p_item);
    input_item_Release(p_item);

    /* A slave can also be an item. If there is a match, this item will be
     * removed from the parent node. This is not a common case, since most
     * slaves will be ignored by fsdir_is_ignored() */
    if (p_fsdir_slave != NULL)
        p_fsdir_slave->p_node = p_node;
    return VLC_SUCCESS;
}
