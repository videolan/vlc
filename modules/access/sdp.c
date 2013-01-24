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
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("SDP"))
    set_description (N_("Session Description Protocol"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACCESS)

    set_capability ("access", 0)
    set_callbacks (Open, Close)
    add_shortcut ("sdp")
vlc_module_end()

static ssize_t Read (access_t *, uint8_t *, size_t);
static int Seek (access_t *, uint64_t);
static int Control (access_t *, int, va_list);

struct access_sys_t
{
    size_t length;
    char   data[];
};

static int Open (vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    size_t len = strlen (access->psz_location);

    access_sys_t *sys = malloc (sizeof(*sys) + len);
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* NOTE: This copy is not really needed. Better safe than sorry. */
    sys->length = len;
    memcpy (sys->data, access->psz_location, len);

    access_InitFields (access);
    access->pf_read = Read;
    access->pf_block = NULL;
    access->pf_seek = Seek;
    access->pf_control = Control;
    access->p_sys = sys;

    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = access->p_sys;

    free (sys);
}

static ssize_t Read (access_t *access, uint8_t *buf, size_t len)
{
    access_sys_t *sys = access->p_sys;

    if (access->info.i_pos >= sys->length)
    {
        access->info.b_eof = true;
        return 0;
    }

    if (len > sys->length)
        len = sys->length;
    memcpy (buf, sys->data + access->info.i_pos, len);
    access->info.i_pos += len;
    return len;
}

static int Seek (access_t *access, uint64_t position)
{
    access->info.i_pos = position;
    access->info.b_eof = false;
    return VLC_SUCCESS;
}

static int Control (access_t *access, int query, va_list args)
{
    switch (query)
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
        {
            bool *b = va_arg(args, bool*);
            *b = true;
            return VLC_SUCCESS;
        }

        case ACCESS_GET_PTS_DELAY:
        {
            int64_t *dp = va_arg(args, int64_t *);
            *dp = DEFAULT_PTS_DELAY;
            return VLC_SUCCESS;
        }
    
        case ACCESS_SET_PAUSE_STATE:
            return VLC_SUCCESS;
    }
    (void) access;
    return VLC_EGENERIC;
}
