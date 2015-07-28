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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>

struct access_entry
{
    struct access_entry *next;
    char mrl[1];
};

struct access_sys_t
{
    access_t *access;
    struct access_entry *next;
    struct access_entry *first;
    bool can_seek;
    bool can_seek_fast;
    bool can_pause;
    bool can_control_pace;
    uint64_t size;
    int64_t caching;
};

static access_t *GetAccess(access_t *access)
{
    access_sys_t *sys = access->p_sys;
    access_t *a = sys->access;

    if (a != NULL)
    {
        if (!vlc_access_Eof(a))
            return a;

        vlc_access_Delete(a);
        sys->access = NULL;
    }

    if (sys->next == NULL)
    {
error:
        access->info.b_eof = true;
        return NULL;
    }

    a = vlc_access_NewMRL(VLC_OBJECT(access), sys->next->mrl);
    if (a == NULL)
        goto error;

    sys->access = a;
    sys->next = sys->next->next;
    return a;
}

static ssize_t Read(access_t *access, uint8_t *buf, size_t len)
{
    access_t *a = GetAccess(access);
    if (a == NULL)
        return 0;

    /* NOTE: Since we recreate the underlying access, the access method can
     * change. We need to check it. For instance, a path could point to a
     * regular file during Open() yet point to a directory here and now. */
    if (unlikely(a->pf_read == NULL))
    {
        access->info.b_eof = true;
        return 0;
    }

    ssize_t ret = vlc_access_Read(a, buf, len);
    if (ret >= 0)
        access->info.i_pos += ret;
    return ret;
}

static block_t *Block(access_t *access)
{
    access_t *a = GetAccess(access);
    if (a == NULL)
        return NULL;

    if (likely(a->pf_block != NULL))
        return vlc_access_Block(a);

    if (a->pf_read == NULL)
    {
        access->info.b_eof = true;
        return NULL;
    }

    /* Emulate pf_block in case of mixed pf_read and bf_block */
    block_t *block = block_Alloc(4096);
    if (unlikely(block == NULL))
        return NULL;

    ssize_t ret = vlc_access_Read(a, block->p_buffer, block->i_buffer);
    if (ret >= 0)
    {
        block->i_buffer = ret;
        access->info.i_pos += ret;
    }
    else
    {
        block_Release(block);
        block = NULL;
    }
    return block;
}

static int Seek(access_t *access, uint64_t position)
{
    access_sys_t *sys = access->p_sys;

    if (sys->access != NULL)
    {
        vlc_access_Delete(sys->access);
        sys->access = NULL;
    }

    sys->next = sys->first;
    access->info.i_pos = 0;

    for (;;)
    {
        access_t *a = GetAccess(access);
        if (a == NULL)
            break;

        bool can_seek;
        access_Control(a, ACCESS_CAN_SEEK, &can_seek);
        if (!can_seek)
            break;

        uint64_t size = access_GetSize(a);

        if (position - access->info.i_pos < size)
        {
            if (vlc_access_Seek(a, position - access->info.i_pos))
                break;

            access->info.i_pos = position;
            return VLC_SUCCESS;
        }

        access->info.i_pos += size;
        vlc_access_Delete(a);
        sys->access = NULL;
    }

    return VLC_EGENERIC;
}

static int Control(access_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;

    switch (query)
    {
        case ACCESS_CAN_SEEK:
            *va_arg(args, bool *) = sys->can_seek;
            break;
        case ACCESS_CAN_FASTSEEK:
            *va_arg(args, bool *) = sys->can_seek_fast;
            break;
        case ACCESS_CAN_PAUSE:
            *va_arg(args, bool *) = sys->can_pause;
            break;
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = sys->can_control_pace;
            break;
        case ACCESS_GET_SIZE:
            *va_arg(args, uint64_t *) = sys->size;
            break;
        case ACCESS_GET_PTS_DELAY:
            *va_arg(args, int64_t *) = sys->caching;
            break;

        case ACCESS_GET_SIGNAL:
        case ACCESS_SET_PAUSE_STATE:
            return access_vaControl(sys->access, query, args);

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;

    char *list = var_CreateGetNonEmptyString(access, "concat-list");
    if (list == NULL)
        return VLC_EGENERIC;

    access_sys_t *sys = malloc(sizeof (*sys));
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

        access_t *a = vlc_access_NewMRL(obj, mrl);
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
                vlc_access_Delete(a);
                free(e);
                continue;
            }
            read_cb = false;
        }

        *pp = e;
        e->next = NULL;
        memcpy(e->mrl, mrl, mlen + 1);

        if (sys->can_seek)
            access_Control(a, ACCESS_CAN_SEEK, &sys->can_seek);
        if (sys->can_seek_fast)
            access_Control(a, ACCESS_CAN_FASTSEEK, &sys->can_seek_fast);
        if (sys->can_pause)
            access_Control(a, ACCESS_CAN_PAUSE, &sys->can_pause);
        if (sys->can_control_pace)
            access_Control(a, ACCESS_CAN_CONTROL_PACE, &sys->can_control_pace);

        sys->size += access_GetSize(a);

        int64_t caching;
        access_Control(a, ACCESS_GET_PTS_DELAY, &caching);
        if (caching > sys->caching)
            sys->caching = caching;

        vlc_access_Delete(a);
        pp = &e->next;
    }

    free(list);
    *pp = NULL;
    sys->next = sys->first;

    access_InitFields(access);
    access->pf_read = read_cb ? Read : NULL;
    access->pf_block = read_cb ? NULL : Block;
    access->pf_seek = Seek;
    access->pf_control = Control;
    access->p_sys = sys;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = access->p_sys;

    if (sys->access != NULL)
        vlc_access_Delete(sys->access);

    for (struct access_entry *e = sys->first, *next; e != NULL; e = next)
    {
        next = e->next;
        free(e);
    }

    var_Destroy(access, "concat-list");
    free(sys);
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
