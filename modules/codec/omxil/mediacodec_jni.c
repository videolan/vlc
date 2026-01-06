/*****************************************************************************
 * mediacodec_jni.c: mc_api implementation using JNI
 *****************************************************************************
 * Copyright © 2015 VLC authors and VideoLAN, VideoLabs
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <jni.h>
#include <stdint.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_threads.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include "omxil_utils.h"

#include "mediacodec.h"
#include "../../video_output/android/env.h"

char* MediaCodec_GetName(vlc_object_t *p_obj, vlc_fourcc_t codec,
                         const char *psz_mime, int profile, int *p_quirks);

#define THREAD_NAME "mediacodec"

/*****************************************************************************
 * JNI Initialisation
 *****************************************************************************/

struct jfields
{
    jclass media_codec_list_class;
    jmethodID get_codec_count, get_codec_info_at, is_encoder, get_capabilities_for_type;
    jmethodID is_feature_supported;
    jfieldID profile_levels_field, profile_field, level_field;
    jmethodID get_supported_types, get_name;
};
static struct jfields jfields;

enum Types
{
    METHOD, STATIC_METHOD, FIELD
};

#define OFF(x) offsetof(struct jfields, x)
struct classname
{
    const char *name;
    int offset;
};
static const struct classname classes[] = {
    { "android/media/MediaCodecList", OFF(media_codec_list_class) },
    { NULL, 0 },
};

struct member
{
    const char *name;
    const char *sig;
    const char *class;
    int offset;
    int type;
    bool critical;
};
static const struct member members[] = {
    { "getCodecCount", "()I", "android/media/MediaCodecList", OFF(get_codec_count), STATIC_METHOD, true },
    { "getCodecInfoAt", "(I)Landroid/media/MediaCodecInfo;", "android/media/MediaCodecList", OFF(get_codec_info_at), STATIC_METHOD, true },

    { "isEncoder", "()Z", "android/media/MediaCodecInfo", OFF(is_encoder), METHOD, true },
    { "getSupportedTypes", "()[Ljava/lang/String;", "android/media/MediaCodecInfo", OFF(get_supported_types), METHOD, true },
    { "getName", "()Ljava/lang/String;", "android/media/MediaCodecInfo", OFF(get_name), METHOD, true },
    { "getCapabilitiesForType", "(Ljava/lang/String;)Landroid/media/MediaCodecInfo$CodecCapabilities;", "android/media/MediaCodecInfo", OFF(get_capabilities_for_type), METHOD, true },
    { "isFeatureSupported", "(Ljava/lang/String;)Z", "android/media/MediaCodecInfo$CodecCapabilities", OFF(is_feature_supported), METHOD, false },
    { "profileLevels", "[Landroid/media/MediaCodecInfo$CodecProfileLevel;", "android/media/MediaCodecInfo$CodecCapabilities", OFF(profile_levels_field), FIELD, true },
    { "profile", "I", "android/media/MediaCodecInfo$CodecProfileLevel", OFF(profile_field), FIELD, true },
    { "level", "I", "android/media/MediaCodecInfo$CodecProfileLevel", OFF(level_field), FIELD, true },


    { NULL, NULL, NULL, 0, 0, false },
};

static int jstrcmp(JNIEnv* env, jobject str, const char* str2)
{
    jsize len = (*env)->GetStringUTFLength(env, str);
    if (len != (jsize) strlen(str2))
        return -1;
    const char *ptr = (*env)->GetStringUTFChars(env, str, NULL);
    int ret = memcmp(ptr, str2, len);
    (*env)->ReleaseStringUTFChars(env, str, ptr);
    return ret;
}

static inline bool check_exception(JNIEnv *env)
{
    if ((*env)->ExceptionCheck(env))
    {
        (*env)->ExceptionClear(env);
        return true;
    }
    else
        return false;
}
#define CHECK_EXCEPTION() check_exception(env)

static inline jstring jni_new_string(JNIEnv *env, const char *psz_string)
{
    jstring jstring = (*env)->NewStringUTF(env, psz_string);
    return !CHECK_EXCEPTION() ? jstring : NULL;
}
#define JNI_NEW_STRING(psz_string) jni_new_string(env, psz_string)


/* Initialize all jni fields.
 * Done only one time during the first initialisation */
static bool
InitJNIFields (vlc_object_t *p_obj, JNIEnv *env)
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static int i_init_state = -1;
    bool ret;

    vlc_mutex_lock(&lock);

    if (i_init_state != -1)
        goto end;

    i_init_state = 0;

    for (int i = 0; classes[i].name; i++)
    {
        jclass clazz = (*env)->FindClass(env, classes[i].name);
        if (CHECK_EXCEPTION())
        {
            msg_Warn(p_obj, "Unable to find class %s", classes[i].name);
            goto end;
        }
        *(jclass*)((uint8_t*)&jfields + classes[i].offset) =
            (jclass) (*env)->NewGlobalRef(env, clazz);
        (*env)->DeleteLocalRef(env, clazz);
    }

    jclass last_class = NULL;
    for (int i = 0; members[i].name; i++)
    {
        if (i == 0 || strcmp(members[i].class, members[i - 1].class))
        {
            if (last_class != NULL)
                (*env)->DeleteLocalRef(env, last_class);
            last_class = (*env)->FindClass(env, members[i].class);
        }

        if (CHECK_EXCEPTION())
        {
            msg_Warn(p_obj, "Unable to find class %s", members[i].class);
            goto end;
        }

        switch (members[i].type) {
        case METHOD:
            *(jmethodID*)((uint8_t*)&jfields + members[i].offset) =
                (*env)->GetMethodID(env, last_class, members[i].name, members[i].sig);
            break;
        case STATIC_METHOD:
            *(jmethodID*)((uint8_t*)&jfields + members[i].offset) =
                (*env)->GetStaticMethodID(env, last_class, members[i].name, members[i].sig);
            break;
        case FIELD:
            *(jfieldID*)((uint8_t*)&jfields + members[i].offset) =
                (*env)->GetFieldID(env, last_class, members[i].name, members[i].sig);
            break;
        }
        if (CHECK_EXCEPTION())
        {
            msg_Warn(p_obj, "Unable to find the member %s in %s",
                     members[i].name, members[i].class);
            if (members[i].critical)
                goto end;
        }
    }
    if (last_class != NULL)
        (*env)->DeleteLocalRef(env, last_class);

    i_init_state = 1;
end:
    ret = i_init_state == 1;
    if (!ret)
        msg_Err(p_obj, "MediaCodec jni init failed");

    vlc_mutex_unlock(&lock);
    return ret;
}

static char *GetManufacturer(JNIEnv *env)
{
    char *manufacturer = NULL;

    jclass clazz = (*env)->FindClass(env, "android/os/Build");
    if (CHECK_EXCEPTION())
        return NULL;

    jfieldID id = (*env)->GetStaticFieldID(env, clazz, "MANUFACTURER",
                                           "Ljava/lang/String;");
    if (CHECK_EXCEPTION())
        goto end;

    jstring jstr = (*env)->GetStaticObjectField(env, clazz, id);

    if (CHECK_EXCEPTION())
        goto end;

    const char *str = (*env)->GetStringUTFChars(env, jstr, 0);
    if (str)
    {
        manufacturer = strdup(str);
        (*env)->ReleaseStringUTFChars(env, jstr, str);
    }

end:
    (*env)->DeleteLocalRef(env, clazz);
    return manufacturer;
}

/*****************************************************************************
 * MediaCodec_GetName
 *****************************************************************************/
char* MediaCodec_GetName(vlc_object_t *p_obj, vlc_fourcc_t codec,
                         const char *psz_mime, int profile, int *p_quirks)
{
    JNIEnv *env;
    int num_codecs;
    jstring jmime;
    char *psz_name = NULL;

    if (!(env = android_getEnv(p_obj, THREAD_NAME)))
        return NULL;

    if (!InitJNIFields(p_obj, env))
        return NULL;

    jmime = JNI_NEW_STRING(psz_mime);
    if (!jmime)
        return NULL;

    num_codecs = (*env)->CallStaticIntMethod(env,
                                             jfields.media_codec_list_class,
                                             jfields.get_codec_count);

    for (int i = 0; i < num_codecs; i++)
    {
        jobject codec_capabilities = NULL;
        jobject profile_levels = NULL;
        jobject info = NULL;
        jobject name = NULL;
        jobject types = NULL;
        jsize name_len = 0;
        int profile_levels_len = 0, num_types = 0;
        const char *name_ptr = NULL;
        bool found = false;
        bool b_adaptive = false;

        info = (*env)->CallStaticObjectMethod(env, jfields.media_codec_list_class,
                                              jfields.get_codec_info_at, i);

        name = (*env)->CallObjectMethod(env, info, jfields.get_name);
        name_len = (*env)->GetStringUTFLength(env, name);
        name_ptr = (*env)->GetStringUTFChars(env, name, NULL);

        if (OMXCodec_IsBlacklisted(name_ptr, name_len))
            goto loopclean;

        if ((*env)->CallBooleanMethod(env, info, jfields.is_encoder))
            goto loopclean;

        codec_capabilities = (*env)->CallObjectMethod(env, info,
                                                      jfields.get_capabilities_for_type,
                                                      jmime);
        if (CHECK_EXCEPTION())
        {
            msg_Warn(p_obj, "Exception occurred in MediaCodecInfo.getCapabilitiesForType");
            goto loopclean;
        }
        else if (codec_capabilities)
        {
            profile_levels = (*env)->GetObjectField(env, codec_capabilities, jfields.profile_levels_field);
            if (profile_levels)
                profile_levels_len = (*env)->GetArrayLength(env, profile_levels);
            if (jfields.is_feature_supported)
            {
                jstring jfeature = JNI_NEW_STRING("adaptive-playback");
                b_adaptive =
                    (*env)->CallBooleanMethod(env, codec_capabilities,
                                              jfields.is_feature_supported,
                                              jfeature);
                CHECK_EXCEPTION();
                (*env)->DeleteLocalRef(env, jfeature);
            }
        }
        msg_Dbg(p_obj, "Number of profile levels: %d", profile_levels_len);

        types = (*env)->CallObjectMethod(env, info, jfields.get_supported_types);
        num_types = (*env)->GetArrayLength(env, types);
        found = false;

        for (int j = 0; j < num_types && !found; j++)
        {
            jobject type = (*env)->GetObjectArrayElement(env, types, j);
            if (!jstrcmp(env, type, psz_mime))
            {
                /* The mime type is matching for this component. We
                   now check if the capabilities of the codec is
                   matching the video format. */
                if (profile > 0)
                {
                    /* This decoder doesn't expose its profiles and is high
                     * profile capable */
                    if (!strncmp(name_ptr, "OMX.LUMEVideoDecoder", __MIN(20, name_len)))
                        found = true;

                    for (int i = 0; i < profile_levels_len && !found; ++i)
                    {
                        jobject profile_level = (*env)->GetObjectArrayElement(env, profile_levels, i);

                        int omx_profile = (*env)->GetIntField(env, profile_level, jfields.profile_field);
                        (*env)->DeleteLocalRef(env, profile_level);

                        int codec_profile =
                            convert_omx_to_profile_idc(codec, omx_profile);
                        if (codec_profile != profile)
                            continue;
                        /* Some encoders set the level too high, thus we ignore it for the moment.
                           We could try to guess the actual profile based on the resolution. */
                        found = true;
                    }
                }
                else
                    found = true;
            }
            (*env)->DeleteLocalRef(env, type);
        }
        if (found)
        {
            msg_Dbg(p_obj, "using %.*s", name_len, name_ptr);
            psz_name = malloc(name_len + 1);
            if (psz_name)
            {
                memcpy(psz_name, name_ptr, name_len);
                psz_name[name_len] = '\0';

                bool ignore_size = false;

                /* The AVC/HEVC MediaCodec implementation on Amazon fire TV
                 * seems to report the Output surface size instead of the Video
                 * size. This bug is specific to Amazon devices since other MTK
                 * implementations report the correct size. The manufacturer is
                 * checked only if the codec matches the MTK one in order to
                 * avoid extra manufacturer check for other every devices.
                 * */
                static const char mtk_dec[] = "OMX.MTK.VIDEO.DECODER.";
                if (strncmp(psz_name, mtk_dec, sizeof(mtk_dec) - 1) == 0)
                {
                    char *manufacturer = GetManufacturer(env);
                    if (manufacturer && strcmp(manufacturer, "Amazon") == 0)
                        ignore_size = true;
                    free(manufacturer);
                }

                if (ignore_size)
                {
                    *p_quirks |= MC_API_VIDEO_QUIRKS_IGNORE_SIZE;
                    /* If the MediaCodec size is ignored, the adaptive mode
                     * should be disabled in order to trigger the hxxx_helper
                     * parsers that will parse the correct video size. Hence
                     * the following 'else if' */
                }
                else if (b_adaptive)
                    *p_quirks |= MC_API_VIDEO_QUIRKS_ADAPTIVE;
            }
        }
loopclean:
        if (name)
        {
            (*env)->ReleaseStringUTFChars(env, name, name_ptr);
            (*env)->DeleteLocalRef(env, name);
        }
        if (profile_levels)
            (*env)->DeleteLocalRef(env, profile_levels);
        if (types)
            (*env)->DeleteLocalRef(env, types);
        if (codec_capabilities)
            (*env)->DeleteLocalRef(env, codec_capabilities);
        if (info)
            (*env)->DeleteLocalRef(env, info);
        if (found)
            break;
    }
    (*env)->DeleteLocalRef(env, jmime);

    return psz_name;
}