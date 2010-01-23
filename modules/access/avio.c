/*****************************************************************************
 * avio.c: access using libavformat library
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_avcodec.h>

#include "avio.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#ifndef MERGE_FFMPEG
vlc_module_begin()
    AVIO_MODULE
vlc_module_end()
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t Read   (access_t *, uint8_t *, size_t);
static int     Seek   (access_t *, uint64_t);
static int     Control(access_t *, int, va_list);

static int     SetupAvio(access_t *);

struct access_sys_t {
    URLContext *context;
};

/* */
int OpenAvio(vlc_object_t *object)
{
    access_t *access = (access_t*)object;
    access_sys_t *sys;

    /* */
    access->p_sys = sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    /* We can either accept only one user (actually) or multiple ones
     * with an exclusive lock */
    if (SetupAvio(access)) {
        msg_Err(access, "Module aready in use");
        return VLC_EGENERIC;
    }

    /* */
    vlc_avcodec_lock();
    av_register_all();
    vlc_avcodec_unlock();

    /* We accept:
     * - avio://full_url
     * - url (only a subset of available protocols).
     */
    char *url;
    if (!strcmp(access->psz_access, "avio"))
        url = strdup(access->psz_path);
    else if (asprintf(&url, "%s://%s", access->psz_access, access->psz_path) < 0)
        url = NULL;

    if (!url)
        goto error;

    msg_Dbg(access, "Opening '%s'", url);
    if (url_open(&sys->context, url, URL_RDONLY) < 0 )
        sys->context = NULL;
    free(url);

    if (!sys->context) {
        msg_Err(access, "Failed to open url using libavformat");
        goto error;
    }
    const int64_t size = url_filesize(sys->context);
    msg_Dbg(access, "is_streamed=%d size=%"PRIi64, sys->context->is_streamed, size);

    /* */
    access_InitFields(access);
    access->info.i_size = size > 0 ? size : 0;

    access->pf_read = Read;
    access->pf_block = NULL;
    access->pf_control = Control;
    access->pf_seek = Seek;
    access->p_sys = sys;

    return VLC_SUCCESS;

error:
    SetupAvio(NULL);
    free(sys);
    return VLC_EGENERIC;
}


void CloseAvio(vlc_object_t *object)
{
    access_t *access = (access_t*)object;
    access_sys_t *sys = access->p_sys;

    url_close(sys->context);

    SetupAvio(NULL);

    free(sys);
}


static ssize_t Read(access_t *access, uint8_t *data, size_t size)
{
    /* FIXME I am unsure of the meaning of the return value in case
     * of error.
     */
    int r = url_read(access->p_sys->context, data, size);
    access->info.b_eof = r <= 0;
    if (r < 0)
        return -1;
    access->info.i_pos += r;
    return r;
}


static int Seek(access_t *access, uint64_t position)
{
    access_sys_t *sys = access->p_sys;

    if (position > INT64_MAX ||
        url_seek(sys->context, position, SEEK_SET) < 0) {
        msg_Err(access, "Seek to %"PRIu64" failed\n", position);
        if (access->info.i_size <= 0 || position != access->info.i_size)
            return VLC_EGENERIC;
    }
    access->info.i_pos = position;
    access->info.b_eof = false;
    return VLC_SUCCESS;
}


static int Control(access_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;

    switch (query) {
    case ACCESS_CAN_SEEK:
    case ACCESS_CAN_FASTSEEK: { /* FIXME how to do that ? */
        bool *b = va_arg(args, bool *);
        *b = !sys->context->is_streamed;
        return VLC_SUCCESS;
    }
    case ACCESS_CAN_PAUSE: {
        bool *b = va_arg(args, bool *);
        *b = sys->context->prot->url_read_pause != NULL; /* FIXME Unsure */
        return VLC_SUCCESS;
    }
    case ACCESS_CAN_CONTROL_PACE: {
        bool *b = va_arg(args, bool *);
        *b = true; /* FIXME */
        return VLC_SUCCESS;
    }
    case ACCESS_GET_PTS_DELAY: {
        int64_t *delay = va_arg(args, int64_t *);
        *delay = DEFAULT_PTS_DELAY; /* FIXME */
    }
    case ACCESS_SET_PAUSE_STATE: {
        bool is_paused = va_arg(args, int);
        if (av_url_read_pause(sys->context, is_paused)< 0)
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }
    case ACCESS_GET_TITLE_INFO:
    case ACCESS_GET_META:
    case ACCESS_GET_CONTENT_TYPE:
    case ACCESS_GET_SIGNAL:
    case ACCESS_SET_TITLE:
    case ACCESS_SET_SEEKPOINT:
    case ACCESS_SET_PRIVATE_ID_STATE:
    case ACCESS_SET_PRIVATE_ID_CA:
    case ACCESS_GET_PRIVATE_ID_STATE:
    default:
        return VLC_EGENERIC;
    }
}

/* */
static vlc_mutex_t avio_lock = VLC_STATIC_MUTEX;
static access_t *current_access = NULL;


static int UrlInterruptCallback(void)
{
    assert(current_access);
    return !vlc_object_alive(current_access);
}


static int SetupAvio(access_t *access)
{
    vlc_mutex_lock(&avio_lock);
    assert(!access != !current_access);
    if (access && current_access) {
        vlc_mutex_unlock(&avio_lock);
        return VLC_EGENERIC;
    }
    url_set_interrupt_cb(access ? UrlInterruptCallback : NULL);
    current_access = access;
    vlc_mutex_unlock(&avio_lock);

    return VLC_SUCCESS;
}

