/**
 * @file androidnativewindow.c
 * @brief Android native window provider module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2013 VLC authors and VideoLAN
 *
 * Author: Adrien Maglo <magsoft@videolan.org>
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
# include <config.h>
#endif

#include <stdarg.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

#include <dlfcn.h>
#include <jni.h>

#include "utils.h"

#define THREAD_NAME "ANativeWindow"
extern int jni_attach_thread(JNIEnv **env, const char *thread_name);
extern void jni_detach_thread();
extern jobject jni_LockAndGetAndroidJavaSurface();
extern void jni_UnlockAndroidSurface();
extern void  jni_SetAndroidSurfaceSize(int width, int height, int visible_width, int visible_height, int sar_num, int sar_den);

static int Open(vout_window_t *, const vout_window_cfg_t *);
static void Close(vout_window_t *);
static int Control(vout_window_t *, int, va_list ap);

/*
 * Module descriptor
 */
vlc_module_begin()
    set_shortname(N_("ANativeWindow"))
    set_description(N_("Android native window"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window anative", 10)
    set_callbacks(Open, Close)
vlc_module_end()


struct vout_window_sys_t
{
    void *p_library;
    native_window_api_t native_window;

    ANativeWindow *window;
};

/**
 * Create an Android native window.
 */
static int Open(vout_window_t *wnd, const vout_window_cfg_t *cfg)
{
    vout_window_sys_t *p_sys = malloc(sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_sys->p_library = LoadNativeWindowAPI(&p_sys->native_window);
    if (p_sys->p_library == NULL)
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    // Create the native window by first getting the Java surface.
    jobject javaSurface = jni_LockAndGetAndroidJavaSurface();
    if (javaSurface == NULL)
        goto error;

    JNIEnv *p_env;
    jni_attach_thread(&p_env, THREAD_NAME);
    p_sys->window = p_sys->native_window.winFromSurface(p_env, javaSurface); // ANativeWindow_fromSurface call.
    jni_detach_thread();

    jni_UnlockAndroidSurface();

    if (p_sys->window == NULL)
        goto error;

    wnd->handle.anativewindow = p_sys->window;
    wnd->control = Control;
    wnd->sys = p_sys;

    // Set the Java surface size.
    jni_SetAndroidSurfaceSize(cfg->width, cfg->height, cfg->width, cfg->height, 1, 1);

    return VLC_SUCCESS;

error:
    dlclose(p_sys->p_library);
    free(p_sys);
    return VLC_EGENERIC;
}


/**
 * Destroys the Android native window.
 */
static void Close(vout_window_t *wnd)
{
    vout_window_sys_t *p_sys = wnd->sys;
    p_sys->native_window.winRelease(p_sys->window); // Release the native window.
    dlclose(p_sys->p_library); // Close the library.
    free (p_sys);
}


/**
 * Window control.
 */
static int Control(vout_window_t *wnd, int cmd, va_list ap)
{
    switch (cmd)
    {
        case VOUT_WINDOW_SET_SIZE:
        {
            unsigned width = va_arg(ap, unsigned);
            unsigned height = va_arg(ap, unsigned);
            jni_SetAndroidSurfaceSize(width, height, width, height, 1, 1);
            break;
        }
        case VOUT_WINDOW_SET_STATE:
        case VOUT_WINDOW_SET_FULLSCREEN:
            return VLC_EGENERIC;
        default:
            msg_Err (wnd, "request %d not implemented", cmd);
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
