/*****************************************************************************
 * android/audioformat_jni.c: Android AudioFormat JNI implementation
 *****************************************************************************
 * Copyright © 2012-2022 VLC authors and VideoLAN, VideoLabs
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
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
#include <vlc_modules.h>
#include <vlc_fourcc.h>
#include "../video_output/android/env.h"
#include "audioformat_jni.h"

#define THREAD_NAME "android_audio"
#define GET_ENV() android_getEnv(VLC_OBJECT(stream), THREAD_NAME)
#define JNI_CALL(what, obj, method, ...) (*env)->what(env, obj, method, ##__VA_ARGS__)
#define JNI_CALL_INT(obj, method, ...) JNI_CALL(CallIntMethod, obj, method, ##__VA_ARGS__)
#define JNI_CALL_VOID(obj, method, ...) JNI_CALL(CallVoidMethod, obj, method, ##__VA_ARGS__)

static struct {
    struct {
        jint ENCODING_AC3;
        bool has_ENCODING_AC3;
        jint ENCODING_E_AC3;
        bool has_ENCODING_E_AC3;
        jint ENCODING_DOLBY_TRUEHD;
        bool has_ENCODING_DOLBY_TRUEHD;
        jint ENCODING_DTS;
        bool has_ENCODING_DTS;
        jint ENCODING_DTS_HD;
        bool has_ENCODING_DTS_HD;
    } AudioFormat;
} jfields;

bool vlc_android_AudioFormat_HasEncoding(long long encoding_flags,
                                         vlc_fourcc_t i_format)
{
#define MATCH_ENCODING_FLAG(x) jfields.AudioFormat.has_##x && \
    (encoding_flags == 0 || encoding_flags & (1 << jfields.AudioFormat.x))

    switch(i_format)
    {
        case VLC_CODEC_DTSHD:
            return MATCH_ENCODING_FLAG(ENCODING_DTS_HD);
        case VLC_CODEC_DTS:
            return MATCH_ENCODING_FLAG(ENCODING_DTS);
        case VLC_CODEC_A52:
            return MATCH_ENCODING_FLAG(ENCODING_AC3);
        case VLC_CODEC_EAC3:
            return MATCH_ENCODING_FLAG(ENCODING_E_AC3);
        case VLC_CODEC_TRUEHD:
        case VLC_CODEC_MLP:
            return MATCH_ENCODING_FLAG(ENCODING_DOLBY_TRUEHD);
        default:
            return true;
    }
}

/* init all jni fields.
 * Done only one time during the first initialisation */
int vlc_android_AudioFormat_InitJNI(vlc_object_t *p_aout)
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static int i_init_state = -1;
    jclass clazz;
    jfieldID field;
    JNIEnv *env = android_getEnv(p_aout, THREAD_NAME);

    if(env == NULL)
        return VLC_EGENERIC;

    vlc_mutex_lock(&lock);

    if(i_init_state != -1)
        goto end;

#define CHECK_EXCEPTION(what, critical) do { \
    if((*env)->ExceptionCheck(env)) \
    { \
        msg_Err(p_aout, "%s failed", what); \
        (*env)->ExceptionClear(env); \
        if((critical)) \
        { \
            i_init_state = 0; \
            goto end; \
        } \
    } \
} while(0)
#define GET_CLASS(str, critical) do { \
    clazz = (*env)->FindClass(env, (str)); \
    CHECK_EXCEPTION("FindClass(" str ")", critical); \
} while(0)
#define GET_ID(get, id, str, args, critical) do { \
    jfields.id = (*env)->get(env, clazz, (str), (args)); \
    CHECK_EXCEPTION(#get "(" #id ")", critical); \
} while(0)
#define GET_CONST_INT(id, str, critical) do { \
    field = NULL; \
    field = (*env)->GetStaticFieldID(env, clazz, (str), "I"); \
    CHECK_EXCEPTION("GetStaticFieldID(" #id ")", critical); \
    if(field) \
    { \
        jfields.id = (*env)->GetStaticIntField(env, clazz, field); \
        CHECK_EXCEPTION(#id, critical); \
    } \
} while(0)


    /* AudioFormat class init */
    GET_CLASS("android/media/AudioFormat", true);
    GET_CONST_INT(AudioFormat.ENCODING_AC3, "ENCODING_AC3", false);
    jfields.AudioFormat.has_ENCODING_AC3 = field != NULL;
    GET_CONST_INT(AudioFormat.ENCODING_E_AC3, "ENCODING_E_AC3", false);
    jfields.AudioFormat.has_ENCODING_E_AC3 = field != NULL;

    GET_CONST_INT(AudioFormat.ENCODING_DTS, "ENCODING_DTS", false);
    jfields.AudioFormat.has_ENCODING_DTS = field != NULL;
    GET_CONST_INT(AudioFormat.ENCODING_DTS_HD, "ENCODING_DTS_HD", false);
    jfields.AudioFormat.has_ENCODING_DTS_HD = field != NULL;

    GET_CONST_INT(AudioFormat.ENCODING_DOLBY_TRUEHD, "ENCODING_DOLBY_TRUEHD",
                  false);
    jfields.AudioFormat.has_ENCODING_DOLBY_TRUEHD = field != NULL;

    i_init_state = 1;
end:
    vlc_mutex_unlock(&lock);
    return i_init_state == 1 ? VLC_SUCCESS : VLC_EGENERIC;
}
