/*****************************************************************************
 * avio.c: access using libavformat library
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <vlc_sout.h>
#include <vlc_avcodec.h>
#include <vlc_interrupt.h>

#include "avio.h"
#include "../codec/avcodec/avcommon.h"

#if LIBAVFORMAT_VERSION_MAJOR < 54
# define AVIOContext URLContext

# define avio_open url_open
# define avio_close url_close
# define avio_read url_read
# define avio_seek url_seek
# define avio_pause av_url_read_pause

# define AVIO_FLAG_READ URL_RDONLY
# define AVIO_FLAG_WRITE URL_WRONLY
# define avio_size url_filesize
#endif

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
static ssize_t Read   (access_t *, void *, size_t);
static int     Seek   (access_t *, uint64_t);
static int     Control(access_t *, int, va_list);
static ssize_t Write(sout_access_out_t *, block_t *);
static int     OutControl(sout_access_out_t *, int, va_list);
static int     OutSeek (sout_access_out_t *, off_t);

static int UrlInterruptCallback(void *access)
{
    /* NOTE: This works so long as libavformat invokes the callback from the
     * same thread that invokes libavformat. Currently libavformat does not
     * create internal threads at all. This is not proper event handling in any
     * case; libavformat needs fixing. */
    (void) access;
    return vlc_killed();
}

struct access_sys_t
{
    AVIOContext *context;
    int64_t size;
};

struct sout_access_out_sys_t {
    AVIOContext *context;
};


/* */

#if LIBAVFORMAT_VERSION_MAJOR < 54
static vlc_object_t *current_access = NULL;

static int UrlInterruptCallbackSingle(void)
{
    return UrlInterruptCallback(current_access);
}

static int SetupAvioCb(vlc_object_t *access)
{
    static vlc_mutex_t avio_lock = VLC_STATIC_MUTEX;
    vlc_mutex_lock(&avio_lock);
    assert(!access != !current_access);
    if (access && current_access) {
        vlc_mutex_unlock(&avio_lock);
        return VLC_EGENERIC;
    }
    url_set_interrupt_cb(access ? UrlInterruptCallbackSingle : NULL);

    current_access = access;

    vlc_mutex_unlock(&avio_lock);

    return VLC_SUCCESS;
}
#endif

/* */

int OpenAvio(vlc_object_t *object)
{
    access_t *access = (access_t*)object;
    access_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    sys->context = NULL;

    /* We accept:
     * - avio://full_url
     * - url (only a subset of available protocols).
     */
    char *url;
    if (!strcmp(access->psz_name, "avio"))
        url = strdup(access->psz_location);
    else if (asprintf(&url, "%s://%s", access->psz_name,
                      access->psz_location) < 0)
        url = NULL;

    if (!url) {
        free(sys);
        return VLC_ENOMEM;
    }

    /* */
    vlc_init_avformat(object);

    int ret;
#if LIBAVFORMAT_VERSION_MAJOR < 54
    ret = avio_open(&sys->context, url, AVIO_FLAG_READ);
#else
    AVIOInterruptCB cb = {
        .callback = UrlInterruptCallback,
        .opaque = access,
    };
    AVDictionary *options = NULL;
    char *psz_opts = var_InheritString(access, "avio-options");
    if (psz_opts && *psz_opts) {
        options = vlc_av_get_options(psz_opts);
        free(psz_opts);
    }
    ret = avio_open2(&sys->context, url, AVIO_FLAG_READ, &cb, &options);
    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(options, "", t, AV_DICT_IGNORE_SUFFIX)))
        msg_Err( access, "unknown option \"%s\"", t->key );
    av_dict_free(&options);
#endif
    if (ret < 0) {
        msg_Err(access, "Failed to open %s: %s", url,
                vlc_strerror_c(AVUNERROR(ret)));
        free(url);
        goto error;
    }
    free(url);

#if LIBAVFORMAT_VERSION_MAJOR < 54
    /* We can accept only one active user at any time */
    if (SetupAvioCb(VLC_OBJECT(access))) {
        msg_Err(access, "Module already in use");
        avio_close(sys->context);
        goto error;
    }
#endif

    int64_t size = avio_size(sys->context);
    bool seekable;
#if LIBAVFORMAT_VERSION_MAJOR < 54
    seekable = !sys->context->is_streamed;
#else
    seekable = sys->context->seekable;
#endif
    msg_Dbg(access, "%sseekable, size=%"PRIi64, seekable ? "" : "not ", size);

    /* */
    access->pf_read = Read;
    access->pf_block = NULL;
    access->pf_control = Control;
    access->pf_seek = Seek;
    access->p_sys = sys;

    return VLC_SUCCESS;

error:
    free(sys);
    return VLC_EGENERIC;
}

/* */

static const char *const ppsz_sout_options[] = {
    "options",
    NULL,
};

int OutOpenAvio(vlc_object_t *object)
{
    sout_access_out_t *access = (sout_access_out_t*)object;

    config_ChainParse( access, "sout-avio-", ppsz_sout_options, access->p_cfg );

    sout_access_out_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    sys->context = NULL;

    /* */
    vlc_init_avformat(object);

    if (!access->psz_path)
        goto error;

    int ret;
#if LIBAVFORMAT_VERSION_MAJOR < 54
    ret = avio_open(&sys->context, access->psz_path, AVIO_FLAG_WRITE);
#else
    AVDictionary *options = NULL;
    char *psz_opts = var_InheritString(access, "sout-avio-options");
    if (psz_opts && *psz_opts) {
        options = vlc_av_get_options(psz_opts);
        free(psz_opts);
    }
    ret = avio_open2(&sys->context, access->psz_path, AVIO_FLAG_WRITE,
                     NULL, &options);
    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(options, "", t, AV_DICT_IGNORE_SUFFIX)))
        msg_Err( access, "unknown option \"%s\"", t->key );
    av_dict_free(&options);
#endif
    if (ret < 0) {
        errno = AVUNERROR(ret);
        msg_Err(access, "Failed to open %s", access->psz_path);
        goto error;
    }

#if LIBAVFORMAT_VERSION_MAJOR < 54
    /* We can accept only one active user at any time */
    if (SetupAvioCb(VLC_OBJECT(access))) {
        msg_Err(access, "Module already in use");
        goto error;
    }
#endif

    access->pf_write = Write;
    access->pf_control = OutControl;
    access->pf_seek = OutSeek;
    access->p_sys = sys;

    return VLC_SUCCESS;

error:
    free(sys);
    return VLC_EGENERIC;
}

void CloseAvio(vlc_object_t *object)
{
    access_t *access = (access_t*)object;
    access_sys_t *sys = access->p_sys;

#if LIBAVFORMAT_VERSION_MAJOR < 54
    SetupAvioCb(NULL);
#endif

    avio_close(sys->context);
    free(sys);
}

void OutCloseAvio(vlc_object_t *object)
{
    sout_access_out_t *access = (sout_access_out_t*)object;
    sout_access_out_sys_t *sys = access->p_sys;

#if LIBAVFORMAT_VERSION_MAJOR < 54
    SetupAvioCb(NULL);
#endif

    avio_close(sys->context);
    free(sys);
}

static ssize_t Read(access_t *access, void *data, size_t size)
{
    access_sys_t *sys = access->p_sys;

    int r = avio_read(sys->context, data, size);
    if (r < 0)
        r = 0;
    return r;
}

/*****************************************************************************
 * Write:
 *****************************************************************************/
static ssize_t Write(sout_access_out_t *p_access, block_t *p_buffer)
{
    sout_access_out_sys_t *p_sys = (sout_access_out_sys_t*)p_access->p_sys;
    size_t i_write = 0;
    int val;

    while (p_buffer != NULL) {
        block_t *p_next = p_buffer->p_next;

#if LIBAVFORMAT_VERSION_MAJOR < 54
        val = url_write(p_sys->context, p_buffer->p_buffer, p_buffer->i_buffer);
        if (val < 0)
            goto error;
        i_write += val;
#else
        avio_write(p_sys->context, p_buffer->p_buffer, p_buffer->i_buffer);
        avio_flush(p_sys->context);
        if ((val = p_sys->context->error) != 0) {
            p_sys->context->error = 0; /* FIXME? */
            goto error;
        }
        i_write += p_buffer->i_buffer;
#endif

        block_Release(p_buffer);

        p_buffer = p_next;
    }

    return i_write;

error:
    msg_Err(p_access, "Wrote only %zu bytes: %s", i_write,
            vlc_strerror_c(AVUNERROR(val)));
    block_ChainRelease( p_buffer );
    return i_write;
}

static int Seek(access_t *access, uint64_t position)
{
    access_sys_t *sys = access->p_sys;
    int ret;

#ifndef EOVERFLOW
# define EOVERFLOW EFBIG
#endif

    if (position > INT64_MAX)
        ret = AVERROR(EOVERFLOW);
    else
        ret = avio_seek(sys->context, position, SEEK_SET);
    if (ret < 0) {
        msg_Err(access, "Seek to %"PRIu64" failed: %s", position,
                vlc_strerror_c(AVUNERROR(ret)));
        if (sys->size < 0 || position != sys->size)
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int OutSeek(sout_access_out_t *p_access, off_t i_pos)
{
    sout_access_out_sys_t *sys = p_access->p_sys;

    if (avio_seek(sys->context, i_pos, SEEK_SET) < 0)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int OutControl(sout_access_out_t *p_access, int i_query, va_list args)
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    VLC_UNUSED(p_sys);
    switch (i_query) {
    case ACCESS_OUT_CONTROLS_PACE: {
        bool *pb = va_arg(args, bool *);
        //*pb = strcmp(p_access->psz_name, "stream");
        *pb = false;
        break;
    }

    default:
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Control(access_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;
    bool *b;

    switch (query) {
    case STREAM_CAN_SEEK:
    case STREAM_CAN_FASTSEEK: /* FIXME how to do that ? */
        b = va_arg(args, bool *);
#if LIBAVFORMAT_VERSION_MAJOR < 54
        *b = !sys->context->is_streamed;
#else
        *b = sys->context->seekable;
#endif
        return VLC_SUCCESS;
    case STREAM_CAN_PAUSE:
        b = va_arg(args, bool *);
#if LIBAVFORMAT_VERSION_MAJOR < 54
        *b = sys->context->prot->url_read_pause != NULL;
#else
        *b = sys->context->read_pause != NULL;
#endif
        return VLC_SUCCESS;
    case STREAM_CAN_CONTROL_PACE:
        b = va_arg(args, bool *);
        *b = true; /* FIXME */
        return VLC_SUCCESS;
    case STREAM_GET_SIZE:
        if (sys->size < 0)
            return VLC_EGENERIC;
        *va_arg(args, uint64_t *) = sys->size;
        return VLC_SUCCESS;
    case STREAM_GET_PTS_DELAY: {
        int64_t *delay = va_arg(args, int64_t *);
        *delay = INT64_C(1000) * var_InheritInteger(access, "network-caching");
        return VLC_SUCCESS;
    }
    case STREAM_SET_PAUSE_STATE: {
        bool is_paused = va_arg(args, int);
        if (avio_pause(sys->context, is_paused)< 0)
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }
    default:
        return VLC_EGENERIC;
    }
}
