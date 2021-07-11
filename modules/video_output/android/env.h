/*****************************************************************************
 * env.h: shared code between Android modules
 *****************************************************************************
 * Copyright (C) 2014-2015 VLC authors and VideoLAN
 *
 * Authors: Felix Abecassis <felix.abecassis@gmail.com>
 *          Thomas Guillem <thomas@gllm.fr>
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

#include <jni.h>

JNIEnv *
android_getEnvCommon(vlc_object_t *p_obj, JavaVM *jvm, const char *psz_name);

static inline JNIEnv *
android_getEnv(vlc_object_t *p_obj, const char *psz_name)
{
    return android_getEnvCommon(p_obj, NULL, psz_name);
}
