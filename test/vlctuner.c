/*****************************************************************************
 * Copyright (C) 2020 Videolabs
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
 *          Alexandre Janniaux <ajanni@videolabs.io>
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

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_variables.h>
#include <vlc_plugin.h>
#include <vlc_tick.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <vlc/vlc.h>
#include <vlc_network.h>
#include "vlc_list.h"

#include "libvlc/test.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define TP_PACKET_SIZE                          188
#define NUMBER_PACKETS                          (128)
#define BUFFER_SIZE                             (TP_PACKET_SIZE*NUMBER_PACKETS)

#define MIN(a, b)   ( ((a) < (b)) ? (a) : (b) )

typedef enum
{
    INJECT_EOS, /*!< EOS */
    //    ...TBD.
}
InjectCmdType;
struct media_owner;

static pthread_t read_thread_hnd;

struct media_ctx
{
    struct media_owner *owner;
    unsigned ref;

    const uint8_t *buffer;
    size_t available_size;
    bool should_flush;

    struct vlc_list node;
};

struct media_owner
{
    pthread_mutex_t lock;
    pthread_cond_t ctx_wait;
    pthread_cond_t append_wait;
    pthread_cond_t read_wait;
    pthread_cond_t flush_wait;
    struct vlc_list ctx_list;

    bool stopped;
    struct media_ctx *active_ctx;
};

static int open_cb(void *opaque, void **datap, uint64_t *sizep)
{
    struct media_owner *owner = opaque;

    pthread_mutex_lock(&owner->lock);

    if (owner->active_ctx != NULL)
    {
        pthread_mutex_unlock(&owner->lock);
        return -1;
    }

    struct media_ctx *ctx = malloc(sizeof(*ctx));
    if (ctx == NULL)
    {
        pthread_mutex_unlock(&owner->lock);
        return -1;
    }

    ctx->ref = 1;
    ctx->owner = owner;
    ctx->buffer = NULL;
    ctx->available_size = 0;
    ctx->should_flush = false;
    vlc_list_append(&ctx->node, &owner->ctx_list);
    owner->active_ctx = ctx;
    pthread_cond_broadcast(&owner->ctx_wait);

    *datap = ctx;
    *sizep = UINT64_MAX;

    pthread_mutex_unlock(&owner->lock);
    return 0;
}

static ssize_t read_cb(void *opaque, unsigned char *buf, size_t len)
{
    struct media_ctx *ctx = opaque;

    pthread_mutex_lock(&ctx->owner->lock);

    while (ctx->available_size == 0 && ctx->owner->active_ctx == ctx && !ctx->should_flush)
        pthread_cond_wait(&ctx->owner->read_wait, &ctx->owner->lock);

    if (ctx->should_flush && ctx->available_size == 0)
    {
        ctx->should_flush = false;
        pthread_cond_broadcast(&ctx->owner->flush_wait);
        pthread_mutex_unlock(&ctx->owner->lock);
        return 0;
    }

    if (ctx->owner->active_ctx != ctx)
    {
        pthread_mutex_unlock(&ctx->owner->lock);
        return -1;
    }

    len = MIN(len, ctx->available_size);

    memcpy(buf, ctx->buffer, len);
    ctx->buffer += len;

    ctx->available_size -= len;

    if (ctx->available_size == 0)
        pthread_cond_broadcast(&ctx->owner->append_wait);
    pthread_mutex_unlock(&ctx->owner->lock);

    return len;
}

static int seek_cb(void *opaque, uint64_t offset)
{
   return 0;
}

static void close_cb(void *opaque)
{
    struct media_ctx *ctx = opaque;
    struct media_owner *owner = ctx->owner;

    pthread_mutex_lock(&ctx->owner->lock);
    if (ctx->owner->active_ctx == ctx)
        ctx->owner->active_ctx = NULL;
    vlc_list_remove(&ctx->node);
    ctx->ref--;
    if (ctx->ref == 0)
        free(ctx);

    pthread_cond_broadcast(&owner->flush_wait);

    pthread_mutex_unlock(&owner->lock);
}

static void append_data_locked(struct media_ctx *ctx, const uint8_t *buffer, size_t size)
{
    ctx->available_size = size;
    ctx->buffer = buffer;
    pthread_cond_broadcast(&ctx->owner->read_wait);

    while ((ctx->available_size > 0 || ctx->should_flush)
            && ctx->owner->active_ctx == ctx)
        pthread_cond_wait(&ctx->owner->append_wait, &ctx->owner->lock);
}

static struct media_ctx *media_owner_lock_active_ctx(struct media_owner *owner)
{
    pthread_mutex_lock(&owner->lock);
    while (owner->active_ctx == NULL && !owner->stopped)
        pthread_cond_wait(&owner->ctx_wait, &owner->lock);

    if (owner->active_ctx == NULL)
    {
        pthread_mutex_unlock(&owner->lock);
        return NULL;
    }

    owner->active_ctx->ref++;
    return owner->active_ctx;
}

static void media_owner_unlock_ctx(struct media_owner *owner, struct media_ctx *ctx)
{
    ctx->ref--;
    if (ctx->ref == 0)
        free(ctx);
    pthread_mutex_unlock(&owner->lock);
}

static libvlc_instance_t *_libvlc;
static libvlc_media_player_t *_mediaPlayer;

static struct media_owner _media_owner;

static bool _keepLastFrame;

static void init(void)
{
    pthread_mutex_init(&_media_owner.lock, NULL);
    pthread_cond_init(&_media_owner.ctx_wait, NULL);
    pthread_cond_init(&_media_owner.append_wait, NULL);
    pthread_cond_init(&_media_owner.read_wait, NULL);
    pthread_cond_init(&_media_owner.flush_wait, NULL);
    vlc_list_init(&_media_owner.ctx_list);
    _media_owner.active_ctx = NULL;
    _media_owner.stopped = false;
    _keepLastFrame = false;
}

static void destroy(void)
{
    pthread_mutex_destroy(&_media_owner.lock);
    pthread_cond_destroy(&_media_owner.ctx_wait);
    pthread_cond_destroy(&_media_owner.append_wait);
    pthread_cond_destroy(&_media_owner.read_wait);
    pthread_cond_destroy(&_media_owner.flush_wait);
}

static int start(void)
{
    pthread_mutex_lock(&_media_owner.lock);
    _media_owner.stopped = false;
    pthread_mutex_unlock(&_media_owner.lock);

    libvlc_media_t *media =
        libvlc_media_new_callbacks(open_cb, read_cb, seek_cb, close_cb,
                                   &_media_owner);
    assert(media);

    libvlc_media_player_set_media(_mediaPlayer, media);
    libvlc_media_release(media);

    libvlc_media_player_play(_mediaPlayer);

    return 0;
}

static int stop(void)
{
    libvlc_media_player_stop_async(_mediaPlayer);

    pthread_mutex_lock(&_media_owner.lock);
    _media_owner.active_ctx = NULL;
    _media_owner.stopped = true;
    pthread_cond_broadcast(&_media_owner.ctx_wait);
    pthread_cond_broadcast(&_media_owner.append_wait);
    pthread_cond_broadcast(&_media_owner.read_wait);
    pthread_cond_broadcast(&_media_owner.flush_wait);

    pthread_mutex_unlock(&_media_owner.lock);
    return 0;
}

static int injectTS(void *bytes, size_t size)
{
    struct media_ctx *ctx = media_owner_lock_active_ctx(&_media_owner);
    if (ctx == NULL)
        return -1;

    append_data_locked(ctx, bytes, size);

    media_owner_unlock_ctx(&_media_owner, ctx);
    return size;
}

static int injectCommand(int command)
{
    struct media_ctx *ctx = media_owner_lock_active_ctx(&_media_owner);
    if (ctx == NULL)
        return -1;

    switch (command) {
        case INJECT_EOS:
            ctx->should_flush = true;
            pthread_cond_broadcast(&ctx->owner->read_wait);
            break;
        default:
            media_owner_unlock_ctx(&_media_owner, ctx);
            return 1;
    }
    media_owner_unlock_ctx(&_media_owner, ctx);
    return 0;
}

static int clearBuffer(void)
{
    libvlc_media_player_set_position(_mediaPlayer, 0, 0);
    return 0;
}

static int mpause(void)
{
    libvlc_media_player_pause(_mediaPlayer);
    return 0;
}

static int resume(void)
{
    libvlc_media_player_play(_mediaPlayer);
    return 0;
}

static int flushTS(void)
{
    struct media_ctx *ctx = media_owner_lock_active_ctx(&_media_owner);
    if (ctx == NULL)
        return -1;

    ctx->should_flush = true;
    pthread_cond_broadcast(&ctx->owner->read_wait);

    while (ctx->should_flush && _media_owner.active_ctx == ctx)
        pthread_cond_wait(&_media_owner.flush_wait, &_media_owner.lock);
    media_owner_unlock_ctx(&_media_owner, ctx);
    return 0;
}

static const char *file_path = "/home/tom/Downloads/ch15_jtbc_(FHD_H264).ts";
static bool terminate = false;

static void *read_thread(void *data)
{
    vlc_object_t *obj = (vlc_object_create)(NULL, sizeof(vlc_object_t));
    assert(obj);

    char *port_str = getenv("VLCTUNER_PORT");
    assert(port_str);
    int port = atoi(port_str);

    char *addr = getenv("VLCTUNER_ADDR");
    assert(addr);

    int fd = net_ListenUDP1(obj, addr, port);

    assert(fd >= 0);

    char readBuffer[BUFFER_SIZE];
    unsigned long lastFileOffset = 0;
    int dataLength = 0;
    int dataSize = 0;
    unsigned int written = 0;
    while (!terminate)
    {
        dataLength = (int) read (fd, readBuffer, TP_PACKET_SIZE * NUMBER_PACKETS);
        dataSize = dataLength;
        if (dataLength > 0)
        {
            written = injectTS(readBuffer, dataSize);
            assert(written > 0);

            lastFileOffset += written;
        }
    }

    close(fd);

    return NULL;
}

static void lock(void)
{
    terminate = false;

    int ret = pthread_create(&read_thread_hnd, NULL, read_thread, NULL);
    assert(ret == 0);
}

static void unlock(void)
{
    terminate = true;
    pthread_join(read_thread_hnd, NULL);
}

static long getCurrentSec()
{
    return SEC_FROM_VLC_TICK( vlc_tick_now() );
}


int main(int argc, char * argv[])
{
    setenv( "VLC_PLUGIN_PATH", "../modules", 1 );
    _libvlc = libvlc_new(argc, argv);
    assert(_libvlc);

    _mediaPlayer = libvlc_media_player_new(_libvlc);
    assert(_mediaPlayer);

    init();

    start();

    lock();

    while(getchar() != 'q');

    unlock();

    stop();
}
