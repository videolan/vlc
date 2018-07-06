/*****************************************************************************
 * concat.c: Concatenated inputs
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
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
#include <stdint.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>

struct access_entry
{
    struct access_entry *next;
    char mrl[1];
};

typedef struct
{
    stream_t *access;
    struct access_entry *next;
    struct access_entry *first;
    bool can_seek;
    bool can_seek_fast;
    bool can_pause;
    bool can_control_pace;
    uint64_t size;
    vlc_tick_t caching;
} access_sys_t;

static stream_t *GetAccess(stream_t *access)
{
    access_sys_t *sys = access->p_sys;
    stream_t *a = sys->access;

    if (a != NULL)
    {
        if (!vlc_stream_Eof(a))
            return a;

        vlc_stream_Delete(a);
        sys->access = NULL;
    }

    if (sys->next == NULL)
        return NULL;

    a = vlc_access_NewMRL(VLC_OBJECT(access), sys->next->mrl);
    if (a == NULL)
        return NULL;

    sys->access = a;
    sys->next = sys->next->next;
    return a;
}

static ssize_t Read(stream_t *access, void *buf, size_t len)
{
    stream_t *a = GetAccess(access);
    if (a == NULL)
        return 0;

    /* NOTE: Since we recreate the underlying access, the access method can
     * change. We need to check it. For instance, a path could point to a
     * regular file during Open() yet point to a directory here and now. */
    if (unlikely(a->pf_read == NULL))
        return 0;

    return vlc_stream_ReadPartial(a, buf, len);
}

static block_t *Block(stream_t *access, bool *restrict eof)
{
    stream_t *a = GetAccess(access);
    if (a == NULL)
    {
        *eof = true;
        return NULL;
    }

    return vlc_stream_ReadBlock(a);
}

static int Seek(stream_t *access, uint64_t position)
{
    access_sys_t *sys = access->p_sys;

    if (sys->access != NULL)
    {
        vlc_stream_Delete(sys->access);
        sys->access = NULL;
    }

    sys->next = sys->first;

    for (uint64_t offset = 0;;)
    {
        stream_t *a = GetAccess(access);
        if (a == NULL)
            break;

        bool can_seek;
        vlc_stream_Control(a, STREAM_CAN_SEEK, &can_seek);
        if (!can_seek)
            break;

        uint64_t size;

        if (vlc_stream_GetSize(a, &size))
            break;
        if (position - offset < size)
        {
            if (vlc_stream_Seek(a, position - offset))
                break;
            return VLC_SUCCESS;
        }

        offset += size;
        vlc_stream_Delete(a);
        sys->access = NULL;
    }

    return VLC_EGENERIC;
}

static int Control(stream_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;

    switch (query)
    {
        case STREAM_CAN_SEEK:
            *va_arg(args, bool *) = sys->can_seek;
            break;
        case STREAM_CAN_FASTSEEK:
            *va_arg(args, bool *) = sys->can_seek_fast;
            break;
        case STREAM_CAN_PAUSE:
            *va_arg(args, bool *) = sys->can_pause;
            break;
        case STREAM_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = sys->can_control_pace;
            break;
        case STREAM_GET_SIZE:
            if (sys->size == UINT64_MAX)
                return VLC_EGENERIC;
            *va_arg(args, uint64_t *) = sys->size;
            break;
        case STREAM_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t *) = sys->caching;
            break;

        case STREAM_GET_SIGNAL:
        case STREAM_SET_PAUSE_STATE:
            return vlc_stream_vaControl(sys->access, query, args);

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;

    char *list = var_CreateGetNonEmptyString(access, "concat-list");
    if (list == NULL)
        return VLC_EGENERIC;

    access_sys_t *sys = vlc_obj_malloc(obj, sizeof (*sys));
    if (unlikely(sys == NULL))
    {
        free(list);
        return VLC_ENOMEM;
    }

    var_SetString(access, "concat-list", ""); /* prevent recursion */

    bool read_cb = true;

    sys->access = NULL;
    sys->can_seek = true;
    sys->can_seek_fast = true;
    sys->can_pause = true;
    sys->can_control_pace = true;
    sys->size = 0;
    sys->caching = 0;

    struct access_entry **pp = &sys->first;

    for (char *buf, *mrl = strtok_r(list, ",", &buf);
         mrl != NULL;
         mrl = strtok_r(NULL, ",", &buf))
    {
        size_t mlen = strlen(mrl);
        struct access_entry *e = malloc(sizeof (*e) + mlen);
        if (unlikely(e == NULL))
            break;

        stream_t *a = vlc_access_NewMRL(obj, mrl);
        if (a == NULL)
        {
            msg_Err(access, "cannot concatenate location %s", mrl);
            free(e);
            continue;
        }

        if (a->pf_read == NULL)
        {
            if (a->pf_block == NULL)
            {
                msg_Err(access, "cannot concatenate directory %s", mrl);
                vlc_stream_Delete(a);
                free(e);
                continue;
            }
            read_cb = false;
        }

        *pp = e;
        e->next = NULL;
        memcpy(e->mrl, mrl, mlen + 1);

        if (sys->can_seek)
            vlc_stream_Control(a, STREAM_CAN_SEEK, &sys->can_seek);
        if (sys->can_seek_fast)
            vlc_stream_Control(a, STREAM_CAN_FASTSEEK, &sys->can_seek_fast);
        if (sys->can_pause)
            vlc_stream_Control(a, STREAM_CAN_PAUSE, &sys->can_pause);
        if (sys->can_control_pace)
            vlc_stream_Control(a, STREAM_CAN_CONTROL_PACE,
                               &sys->can_control_pace);
        if (sys->size != UINT64_MAX)
        {
            uint64_t size;

            if (vlc_stream_GetSize(a, &size))
                sys->size = UINT64_MAX;
            else
                sys->size += size;
        }

        vlc_tick_t caching;
        vlc_stream_Control(a, STREAM_GET_PTS_DELAY, &caching);
        if (caching > sys->caching)
            sys->caching = caching;

        vlc_stream_Delete(a);
        pp = &e->next;
    }

    free(list);
    *pp = NULL;
    sys->next = sys->first;

    access->pf_read = read_cb ? Read : NULL;
    access->pf_block = read_cb ? NULL : Block;
    access->pf_seek = Seek;
    access->pf_control = Control;
    access->p_sys = sys;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = access->p_sys;

    if (sys->access != NULL)
        vlc_stream_Delete(sys->access);

    for (struct access_entry *e = sys->first, *next; e != NULL; e = next)
    {
        next = e->next;
        free(e);
    }

    var_Destroy(access, "concat-list");
}

#define INPUT_LIST_TEXT N_("Inputs list")
#define INPUT_LIST_LONGTEXT N_( \
    "Comma-separated list of input URLs to concatenate.")

vlc_module_begin()
    set_shortname(N_("Concatenation"))
    set_description(N_("Concatenated inputs"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    add_string("concat-list", NULL, INPUT_LIST_TEXT, INPUT_LIST_LONGTEXT, true)
    set_capability("access", 0)
    set_callbacks(Open, Close)
    add_shortcut("concast", "list")
vlc_module_end()
