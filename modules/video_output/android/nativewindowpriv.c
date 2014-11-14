/*****************************************************************************
 * nativewindowpriv.c: Wrapper to android native window private api
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 *
 * Authors: Thomas Guillem <guillem@archos.com>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#if ANDROID_API <= 13
#include <ui/android_native_buffer.h>
#include <ui/egl/android_natives.h>
#else
#include <system/window.h>
#endif

#include <hardware/gralloc.h>

#include <android/log.h>

#define NO_ERROR 0
typedef int32_t status_t;

#if ANDROID_API <= 13
typedef android_native_buffer_t ANativeWindowBuffer_t;
#endif
typedef struct native_window_priv native_window_priv;

struct native_window_priv
{
    ANativeWindow *anw;
};

#define LOG_TAG "VLC/ANW"

#define LOGD(...) __android_log_print( ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__ )
#define LOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

#define CHECK_ERR() do {\
    if( err != NO_ERROR ) {\
        LOGE( "error %d in %s  line %d\n", err, __FUNCTION__, __LINE__  );\
        return err;\
    }\
} while (0)

#define CHECK_ANB() do {\
    if( anb->common.magic != ANDROID_NATIVE_BUFFER_MAGIC &&\
            anb->common.version != sizeof(ANativeWindowBuffer_t) ) {\
        LOGE( "error, buffer not valid\n"  );\
        return -EINVAL;\
    }\
} while (0)

native_window_priv *ANativeWindowPriv_connect( void *window )
{
    native_window_priv *priv;
    ANativeWindow *anw = (ANativeWindow *)window;

    if( anw->common.magic != ANDROID_NATIVE_WINDOW_MAGIC &&
            anw->common.version != sizeof(ANativeWindow) ) {
        LOGE( "error, window not valid\n"  );
        return NULL;
    }

#if ANDROID_API >= 14
    if (native_window_api_connect( anw, NATIVE_WINDOW_API_MEDIA ) != 0) {
        LOGE( "native_window_api_connect FAIL"  );
        return NULL;
    }
#endif

    priv = calloc( 1, sizeof(native_window_priv) );

    if( !priv ) {
#if ANDROID_API >= 14
        native_window_api_disconnect( anw, NATIVE_WINDOW_API_MEDIA );
#endif
        return NULL;
    }
    priv->anw = anw;

    return priv;
}

int ANativeWindowPriv_disconnect( native_window_priv *priv )
{
#if ANDROID_API >= 14
    native_window_api_disconnect( priv->anw, NATIVE_WINDOW_API_MEDIA );
#endif
    free(priv);

    return 0;
}

int ANativeWindowPriv_setup( native_window_priv *priv, int w, int h, int hal_format, int hw_usage )
{
    int usage = 0;
    status_t err;

    LOGD( "setup: %p, %d, %d, %X, %X\n",
          priv->anw, w, h, hal_format, hw_usage );

    usage |= hw_usage | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
#if ANDROID_API >= 11
    usage |= GRALLOC_USAGE_EXTERNAL_DISP;
#endif

    err = native_window_set_usage( priv->anw, usage );
    CHECK_ERR();

#if ANDROID_API <= 13
    err = native_window_set_buffers_geometry( priv->anw, w, h, hal_format );
    CHECK_ERR();
#else
    err = native_window_set_scaling_mode( priv->anw, NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW );
    CHECK_ERR();

    err = native_window_set_buffers_dimensions( priv->anw, w, h );
    CHECK_ERR();

    err = native_window_set_buffers_format( priv->anw, hal_format );
    CHECK_ERR();
#endif

    return 0;
}

int ANativeWindowPriv_getMinUndequeued( native_window_priv *priv, unsigned int *min_undequeued )
{
    status_t err;

#if ANDROID_API >= 11
    err = priv->anw->query( priv->anw, NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, min_undequeued );
    CHECK_ERR();
#endif
    /* set a minimum value of min_undequeued in case query fails */
    if( *min_undequeued == 0 )
        *min_undequeued = 2;

    LOGD( "getMinUndequeued: %p %u", priv->anw, *min_undequeued );

    return 0;
}

int ANativeWindowPriv_setBufferCount(native_window_priv *priv, unsigned int count )
{
    status_t err;

    LOGD( "setBufferCount: %p %u", priv->anw, count );

    err = native_window_set_buffer_count( priv->anw, count );
    CHECK_ERR();

    return 0;
}

int ANativeWindowPriv_setCrop( native_window_priv *priv, int ofs_x, int ofs_y, int w, int h )
{
    android_native_rect_t crop;

    crop.left = ofs_x;
    crop.top = ofs_y;
    crop.right = ofs_x + w;
    crop.bottom = ofs_y + h;
    return native_window_set_crop( priv->anw, &crop );
}

int ANativeWindowPriv_dequeue( native_window_priv *priv, void **pp_handle )
{
    ANativeWindowBuffer_t *anb;
    status_t err = NO_ERROR;

#if ANDROID_API >= 18
    err = priv->anw->dequeueBuffer_DEPRECATED( priv->anw, &anb );
#else
    err = priv->anw->dequeueBuffer( priv->anw, &anb );
#endif
    CHECK_ERR();

    *pp_handle = anb;

    return 0;
}

int ANativeWindowPriv_lock( native_window_priv *priv, void *p_handle )
{
    ANativeWindowBuffer_t *anb = (ANativeWindowBuffer_t *)p_handle;
    status_t err = NO_ERROR;

    CHECK_ANB();

#if ANDROID_API >= 18
    err = priv->anw->lockBuffer_DEPRECATED( priv->anw, anb );
#else
    err = priv->anw->lockBuffer( priv->anw, anb );
#endif
    CHECK_ERR();

    return 0;
}

int ANativeWindowPriv_queue( native_window_priv *priv, void *p_handle )
{
    ANativeWindowBuffer_t *anb = (ANativeWindowBuffer_t *)p_handle;
    status_t err = NO_ERROR;

    CHECK_ANB();

#if ANDROID_API >= 18
    err = priv->anw->queueBuffer_DEPRECATED( priv->anw, anb );
#else
    err = priv->anw->queueBuffer( priv->anw, anb );
#endif
    CHECK_ERR();

    return 0;
}

int ANativeWindowPriv_cancel( native_window_priv *priv, void *p_handle )
{
    ANativeWindowBuffer_t *anb = (ANativeWindowBuffer_t *)p_handle;
    status_t err = NO_ERROR;

    CHECK_ANB();

#if ANDROID_API >= 18
    err = priv->anw->cancelBuffer_DEPRECATED( priv->anw, anb );
#else
    err = priv->anw->cancelBuffer( priv->anw, anb );
#endif
    CHECK_ERR();

    return 0;
}

int ANativeWindowPriv_setOrientation( native_window_priv *priv, int orientation )
{
    status_t err = NO_ERROR;
    int transform;

    switch( orientation )
    {
        case 90:
            transform = NATIVE_WINDOW_TRANSFORM_ROT_90;
            break;
        case 180:
            transform = NATIVE_WINDOW_TRANSFORM_ROT_180;
            break;
        case 270:
            transform = NATIVE_WINDOW_TRANSFORM_ROT_270;
            break;
        default:
            transform = 0;
    }

    err = native_window_set_buffers_transform( priv->anw, transform );
    CHECK_ERR();

    return 0;
}
