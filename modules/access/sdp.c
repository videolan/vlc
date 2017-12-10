/*****************************************************************************
 * sdp.c: Fake input for sdp:// scheme
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
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
#include <vlc_access.h>

static int Open (vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("SDP"))
    set_description (N_("Session Description Protocol"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACCESS)

    set_capability ("access", 0)
    set_callbacks (Open, NULL)
    add_shortcut ("sdp")
vlc_module_end()

static ssize_t Read (stream_t *, void *, size_t);
static int Seek (stream_t *, uint64_t);
static int Control (stream_t *, int, va_list);

struct access_sys_t
{
    size_t offset;
    size_t length;
    char   data[];
};

static int Open (vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    size_t len = strlen (access->psz_location);

    access_sys_t *sys = vlc_obj_malloc(obj, sizeof(*sys) + len);
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* NOTE: This copy is not really needed. Better safe than sorry. */
    sys->offset = 0;
    sys->length = len;
    memcpy (sys->data, access->psz_location, len);

    access->pf_read = Read;
    access->pf_block = NULL;
    access->pf_seek = Seek;
    access->pf_control = Control;
    access->p_sys = sys;

    return VLC_SUCCESS;
}

static ssize_t Read (stream_t *access, void *buf, size_t len)
{
    access_sys_t *sys = access->p_sys;

    if (sys->offset >= sys->length)
        return 0;

    if (len > sys->length - sys->offset)
        len = sys->length - sys->offset;
    memcpy (buf, sys->data + sys->offset, len);
    sys->offset += len;
    return len;
}

static int Seek (stream_t *access, uint64_t position)
{
    access_sys_t *sys = access->p_sys;

    if (position > sys->length)
        position = sys->length;

    sys->offset = position;
    return VLC_SUCCESS;
}

static int Control (stream_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;

    switch (query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
        {
            bool *b = va_arg(args, bool*);
            *b = true;
            return VLC_SUCCESS;
        }

        case STREAM_GET_SIZE:
            *va_arg(args, uint64_t *) = sys->length;
            return VLC_SUCCESS;

        case STREAM_GET_PTS_DELAY:
        {
            int64_t *dp = va_arg(args, int64_t *);
            *dp = DEFAULT_PTS_DELAY;
            return VLC_SUCCESS;
        }
    
        case STREAM_SET_PAUSE_STATE:
            return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}
