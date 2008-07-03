/*****************************************************************************
 * bandwidth.c
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <assert.h>
#include <errno.h>

#include <vlc_access.h>

#define BANDWIDTH_TEXT N_("Bandwidth limit (bytes/s)")
#define BANDWIDTH_LONGTEXT N_( \
    "The bandwidth module will drop any data in excess of that many bytes " \
    "per seconds." )

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/* TODO: burst support */

vlc_module_begin ();
    set_shortname (N_("Bandwidth"));
    set_description (N_("Bandwidth limiter"));
    set_category (CAT_INPUT);
    set_subcategory (SUBCAT_INPUT_ACCESS_FILTER);
    set_capability ("access_filter", 0);
    add_shortcut ("bandwidth");
    set_callbacks (Open, Close);

    add_integer ("access-bandwidth", 65536, NULL, BANDWIDTH_TEXT,
                 BANDWIDTH_LONGTEXT, false);
vlc_module_end();

static ssize_t Read (access_t *access, uint8_t *buffer, size_t len);
static int Seek (access_t *access, int64_t offset);
static int Control (access_t *access, int cmd, va_list ap);

struct access_sys_t
{
    mtime_t last_time;
    size_t  last_size;
    size_t  bandwidth;
};

/**
 * Open()
 */
static int Open (vlc_object_t *obj)
{
    access_t *access = (access_t*)obj;
    access_t *src = access->p_source;

    if (src->pf_read != NULL)
        access->pf_read = Read;
    else
    {
        msg_Err (obj, "block bandwidth limit not implemented");
        return VLC_EGENERIC;
    }

    if (src->pf_seek != NULL)
        access->pf_seek = Seek;

    access->pf_control = Control;
    access->info = src->info;

    access_sys_t *p_sys = access->p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    memset (p_sys, 0, sizeof (*p_sys));
    p_sys->bandwidth = var_CreateGetInteger (access, "access-bandwidth");
    p_sys->last_time = mdate ();

    msg_Dbg (obj, "bandwidth limit: %lu bytes/s",
             (unsigned long)p_sys->bandwidth);
    return VLC_SUCCESS;
}


/**
 * Close()
 */
static void Close (vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *p_sys = access->p_sys;
    free (p_sys);
}


static ssize_t Read (access_t *access, uint8_t *buffer, size_t len)
{
    access_t *src = access->p_source;
    access_sys_t *p_sys = access->p_sys;
    mtime_t now;

    if (len == 0)
        return 0;

retry:
    now = mdate ();
    if (now <= p_sys->last_time)
    {
        msg_Err (access, "*** ALERT *** System clock is going backward! ***");
        return 0; /* Uho, your clock is broken. */
    }

    mtime_t delta = now - p_sys->last_time;
    p_sys->last_time = now;

    delta *= p_sys->bandwidth;
    delta /= 1000000u;

    if (delta == 0)
    {
        now += 1000000u / p_sys->bandwidth;
        mwait (now);
        goto retry;
    }

    if (len > delta)
    {
        msg_Dbg (access, "reading %"PRIu64" bytes instead of %zu", delta,
                 len);
        len = (int)delta;
    }


    src->info.i_update = access->info.i_update;
    len = src->pf_read (src, buffer, len);
    access->info = src->info;

    msg_Dbg (access, "read %zu bytes", len);
    return len;
}


static int Control (access_t *access, int cmd, va_list ap)
{
    access_t *src = access->p_source;
    return src->pf_control (src, cmd, ap);
}


static int Seek (access_t *access, int64_t offset)
{
    access_t *src = access->p_source;

    src->info.i_update = access->info.i_update;
    int ret = src->pf_seek (src, offset);
    access->info = src->info;
    return ret;
}
