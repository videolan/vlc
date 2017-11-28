/*****************************************************************************
 * imem-access.c: In-memory bit stream input for VLC
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
#include <vlc_access.h>
#include <vlc_plugin.h>

struct access_sys_t
{
    void *opaque;
    ssize_t (*read_cb)(void *, unsigned char *, size_t);
    int (*seek_cb)(void *, uint64_t);
    void (*close_cb)(void *);
    uint64_t size;
};

static ssize_t Read(stream_t *access, void *buf, size_t len)
{
    access_sys_t *sys = access->p_sys;

    ssize_t val = sys->read_cb(sys->opaque, buf, len);

    if (val < 0) {
        msg_Err(access, "read error");
        val = 0;
    }

    return val;
}

static int Seek(stream_t *access, uint64_t offset)
{
    access_sys_t *sys = access->p_sys;

    assert(sys->seek_cb != NULL);

    if (sys->seek_cb(sys->opaque, offset) != 0)
        return VLC_EGENERIC;
   return VLC_SUCCESS;
}

static int Control(stream_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;

    switch (query)
    {
        case STREAM_CAN_SEEK:
            *va_arg(args, bool *) = sys->seek_cb != NULL;
            break;

        case STREAM_CAN_FASTSEEK:
            *va_arg(args, bool *) = false;
            break;

        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = sys->seek_cb != NULL;
            break;

        case STREAM_GET_SIZE:
            if (sys->size == UINT64_MAX)
                return VLC_EGENERIC;
            *va_arg(args, uint64_t *) = sys->size;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg(args, int64_t *) = DEFAULT_PTS_DELAY;
            break;

        case STREAM_SET_PAUSE_STATE:
            break;

        default:
            return VLC_EGENERIC;
    }
    (void) access;
    return VLC_SUCCESS;
}

static int open_cb_default(void *opaque, void **datap, uint64_t *sizep)
{
    *datap = opaque;
    (void) sizep;
    return 0;
}

static int Open(vlc_object_t *object)
{
    stream_t *access = (stream_t *)object;

    access_sys_t *sys = vlc_obj_malloc(object, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    int (*open_cb)(void *, void **, uint64_t *);
    void *opaque;

    opaque = var_InheritAddress(access, "imem-data");
    open_cb = var_InheritAddress(access, "imem-open");
    sys->opaque = NULL;
    sys->read_cb = var_InheritAddress(access, "imem-read");
    sys->seek_cb = var_InheritAddress(access, "imem-seek");
    sys->close_cb = var_InheritAddress(access, "imem-close");
    sys->size = UINT64_MAX;

    if (open_cb == NULL)
        open_cb = open_cb_default;
    if (sys->read_cb == NULL)
        return VLC_EGENERIC;

    if (open_cb(opaque, &sys->opaque, &sys->size)) {
        msg_Err(access, "open error");
        return VLC_EGENERIC;
    }

    access->pf_read = Read;
    access->pf_block = NULL;
    access->pf_seek = (sys->seek_cb != NULL) ? Seek : NULL;
    access->pf_control = Control;

    access->p_sys = sys;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    stream_t *access = (stream_t *)object;
    access_sys_t *sys = access->p_sys;

    if (sys->close_cb != NULL)
        sys->close_cb(sys->opaque);
}

vlc_module_begin()
    set_shortname(N_("Memory stream"))
    set_description(N_("In-memory stream input"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)

    add_shortcut("imem")
    set_capability("access", 0)
    set_callbacks(Open, Close)
vlc_module_end()
