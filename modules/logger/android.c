/*****************************************************************************
 * android.c: Android logger using logcat
 *****************************************************************************
 * Copyright © 2015 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef __ANDROID__
#error __ANDROID__ not defined
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <android/log.h>

#include <stdarg.h>
#include <stdio.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

static const int ptr_width = 2 * /* hex digits */ sizeof (uintptr_t);

static void AndroidPrintMsg(void *opaque, int type, const vlc_log_t *p_item,
                            const char *format, va_list ap)
{
    int prio;
    char *format2;
    int verbose = (intptr_t)opaque;

    if (verbose < type)
        return;

    if (asprintf(&format2, "[%0*"PRIxPTR"/%lx] %s %s: %s",
                 ptr_width, p_item->i_object_id, p_item->tid, p_item->psz_module,
                 p_item->psz_object_type, format) < 0)
        return;
    switch (type) {
        case VLC_MSG_INFO:
            prio = ANDROID_LOG_INFO;
            break;
        case VLC_MSG_ERR:
            prio = ANDROID_LOG_ERROR;
            break;
        case VLC_MSG_WARN:
            prio = ANDROID_LOG_WARN;
            break;
        default:
        case VLC_MSG_DBG:
            prio = ANDROID_LOG_DEBUG;
    }
    __android_log_vprint(prio, "VLC", format2, ap);
    free(format2);
}

static const struct vlc_logger_operations ops = { AndroidPrintMsg, NULL };

static const struct vlc_logger_operations *Open(vlc_object_t *obj, void **sysp)
{
    int verbosity = var_InheritInteger(obj, "verbose");

    if (verbosity < 0)
        return NULL;

    verbosity += VLC_MSG_ERR;
    *sysp = (void *)(uintptr_t)verbosity;

    return &ops;
}

vlc_module_begin()
    set_shortname(N_("Android log"))
    set_description(N_("Android log using logcat"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("logger", 30)
    set_callback(Open)
vlc_module_end ()
