/*****************************************************************************
 * iomx_hwbuffer.c: Wrapper to android native window api used for IOMX
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

#if ANDROID_API <= 11
#include <ui/android_native_buffer.h>
#include <ui/egl/android_natives.h>
#else
#include <system/window.h>
#endif

#include <hardware/gralloc.h>

#include <android/log.h>

#define NO_ERROR 0
typedef int32_t status_t;

#if ANDROID_API <= 11
typedef android_native_buffer_t ANativeWindowBuffer_t;
#endif

#define LOG_TAG "VLC/IOMX"

#define LOGD(...) __android_log_print( ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__ )
#define LOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

#define CHECK_ERR() do {\
    if( err != NO_ERROR ) {\
        LOGE( "IOMXHWBuffer: error %d in %s  line %d\n", err, __FUNCTION__, __LINE__  );\
        return err;\
    }\
} while (0)

#define CHECK_ANW() do {\
    if( anw->common.magic != ANDROID_NATIVE_WINDOW_MAGIC &&\
            anw->common.version != sizeof(ANativeWindow) ) {\
        LOGE( "IOMXHWBuffer: error, window not valid\n"  );\
        return -EINVAL;\
    }\
} while (0)

#define CHECK_ANB() do {\
    if( anb->common.magic != ANDROID_NATIVE_BUFFER_MAGIC &&\
            anb->common.version != sizeof(ANativeWindowBuffer_t) ) {\
        LOGE( "IOMXHWBuffer: error, buffer not valid\n"  );\
        return -EINVAL;\
    }\
} while (0)

int IOMXHWBuffer_Connect( void *window )
{
    ANativeWindow *anw = (ANativeWindow *)window;
    CHECK_ANW();

#if ANDROID_API > 11
    if (native_window_api_connect( anw, NATIVE_WINDOW_API_MEDIA ) != 0) {
        LOGE( "native_window_api_connect FAIL"  );
        return -EINVAL;
    }
#endif
}

int IOMXHWBuffer_Disconnect( void *window )
{
    ANativeWindow *anw = (ANativeWindow *)window;

    CHECK_ANW();

#if ANDROID_API > 11
    native_window_api_disconnect( anw, NATIVE_WINDOW_API_MEDIA );
#endif

    return 0;
}


int IOMXHWBuffer_Setup( void *window, int w, int h, int hal_format, int hw_usage,
                        unsigned int *num_frames, unsigned int *min_undequeued )
{
    ANativeWindow *anw = (ANativeWindow *)window;
    int usage = 0;
    status_t err;

    CHECK_ANW();

    LOGD( "IOMXHWBuffer_setup: %p, %d, %d, %X, %X\n",
          anw, w, h, hal_format, hw_usage );

    usage |= hw_usage | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
#if ANDROID_API >= 11
    usage |= GRALLOC_USAGE_EXTERNAL_DISP;
#endif

    err = native_window_set_usage( anw, usage );
    CHECK_ERR();

#if ANDROID_API <= 11
    err = native_window_set_buffers_geometry( anw, w, h, hal_format );
    CHECK_ERR();
#else
    err = native_window_set_scaling_mode( anw, NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW );
    CHECK_ERR();

    err = native_window_set_buffers_dimensions( anw, w, h );
    CHECK_ERR();

    err = native_window_set_buffers_format( anw, hal_format );
    CHECK_ERR();
#endif

#if ANDROID_API >= 11
    if( *min_undequeued == 0 )
    {
        anw->query( anw, NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, min_undequeued );
        CHECK_ERR();
    }
#endif
    /* set a minimum value of min_undequeued in case query fails */
    if( *min_undequeued == 0 )
        *min_undequeued = 2;
    *num_frames += *min_undequeued;

    err = native_window_set_buffer_count( anw, *num_frames );
    CHECK_ERR();

    return 0;
}

int IOMXHWBuffer_Setcrop( void *window, int ofs_x, int ofs_y, int w, int h )
{
    ANativeWindow *anw = (ANativeWindow *)window;
    android_native_rect_t crop;

    CHECK_ANW();

    crop.left = ofs_x;
    crop.top = ofs_y;
    crop.right = ofs_x + w;
    crop.bottom = ofs_y + h;
    return native_window_set_crop( anw, &crop );
}

int IOMXHWBuffer_Dequeue( void *window, void **pp_handle )
{
    ANativeWindow *anw = (ANativeWindow *)window;
    ANativeWindowBuffer_t *anb;
    status_t err = NO_ERROR;

    CHECK_ANW();

#if ANDROID_API >= 18
    err = anw->dequeueBuffer_DEPRECATED( anw, &anb );
#else
    err = anw->dequeueBuffer( anw, &anb );
#endif
    CHECK_ERR();

    *pp_handle = anb;

    return 0;
}

int IOMXHWBuffer_Lock( void *window, void *p_handle )
{
    ANativeWindow *anw = (ANativeWindow *)window;
    ANativeWindowBuffer_t *anb = (ANativeWindowBuffer_t *)p_handle;
    status_t err = NO_ERROR;

    CHECK_ANW();
    CHECK_ANB();

#if ANDROID_API >= 18
    err = anw->lockBuffer_DEPRECATED( anw, anb );
#else
    err = anw->lockBuffer( anw, anb );
#endif
    CHECK_ERR();

    return 0;
}

int IOMXHWBuffer_Queue( void *window, void *p_handle )
{
    ANativeWindow *anw = (ANativeWindow *)window;
    ANativeWindowBuffer_t *anb = (ANativeWindowBuffer_t *)p_handle;
    status_t err = NO_ERROR;

    CHECK_ANW();
    CHECK_ANB();

#if ANDROID_API >= 18
    err = anw->queueBuffer_DEPRECATED( anw, anb );
#else
    err = anw->queueBuffer( anw, anb );
#endif
    CHECK_ERR();

    return 0;
}

int IOMXHWBuffer_Cancel( void *window, void *p_handle )
{
    ANativeWindow *anw = (ANativeWindow *)window;
    ANativeWindowBuffer_t *anb = (ANativeWindowBuffer_t *)p_handle;
    status_t err = NO_ERROR;

    CHECK_ANW();
    CHECK_ANB();

#if ANDROID_API >= 18
    err = anw->cancelBuffer_DEPRECATED( anw, anb );
#else
    err = anw->cancelBuffer( anw, anb );
#endif
    CHECK_ERR();

    return 0;
}
