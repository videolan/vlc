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

#include <OMX_Core.h>
#include <OMX_Component.h>
#include "omxil_utils.h"
#include "../../packetizer/hevc_nal.h"

#include "mediacodec.h"

char* MediaCodec_GetName(vlc_object_t *p_obj, const char *psz_mime,
                         int profile, bool *p_adaptive);

#define THREAD_NAME "mediacodec_jni"

#define BUFFER_FLAG_CODEC_CONFIG  2
#define BUFFER_FLAG_END_OF_STREAM 4
#define INFO_OUTPUT_BUFFERS_CHANGED -3
#define INFO_OUTPUT_FORMAT_CHANGED  -2
#define INFO_TRY_AGAIN_LATER        -1

/*****************************************************************************
 * JNI Initialisation
 *****************************************************************************/

struct jfields
{
    jclass media_codec_list_class, media_codec_class, media_format_class;
    jclass buffer_info_class, byte_buffer_class;
    jmethodID tostring;
    jmethodID get_codec_count, get_codec_info_at, is_encoder, get_capabilities_for_type;
    jmethodID is_feature_supported;
    jfieldID profile_levels_field, profile_field, level_field;
    jmethodID get_supported_types, get_name;
    jmethodID create_by_codec_name, configure, start, stop, flush, release;
    jmethodID get_output_format;
    jmethodID get_input_buffers, get_input_buffer;
    jmethodID get_output_buffers, get_output_buffer;
    jmethodID dequeue_input_buffer, dequeue_output_buffer, queue_input_buffer;
    jmethodID release_output_buffer;
    jmethodID create_video_format, create_audio_format;
    jmethodID set_integer, set_bytebuffer, get_integer;
    jmethodID buffer_info_ctor;
    jfieldID size_field, offset_field, pts_field, flags_field;
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
    { "android/media/MediaCodec", OFF(media_codec_class) },
    { "android/media/MediaFormat", OFF(media_format_class) },
    { "android/media/MediaFormat", OFF(media_format_class) },
    { "android/media/MediaCodec$BufferInfo", OFF(buffer_info_class) },
    { "java/nio/ByteBuffer", OFF(byte_buffer_class) },
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
    { "toString", "()Ljava/lang/String;", "java/lang/Object", OFF(tostring), METHOD, true },

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

    { "createByCodecName", "(Ljava/lang/String;)Landroid/media/MediaCodec;", "android/media/MediaCodec", OFF(create_by_codec_name), STATIC_METHOD, true },
    { "configure", "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V", "android/media/MediaCodec", OFF(configure), METHOD, true },
    { "start", "()V", "android/media/MediaCodec", OFF(start), METHOD, true },
    { "stop", "()V", "android/media/MediaCodec", OFF(stop), METHOD, true },
    { "flush", "()V", "android/media/MediaCodec", OFF(flush), METHOD, true },
    { "release", "()V", "android/media/MediaCodec", OFF(release), METHOD, true },
    { "getOutputFormat", "()Landroid/media/MediaFormat;", "android/media/MediaCodec", OFF(get_output_format), METHOD, true },
    { "getInputBuffers", "()[Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_input_buffers), METHOD, false },
    { "getInputBuffer", "(I)Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_input_buffer), METHOD, false },
    { "getOutputBuffers", "()[Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_output_buffers), METHOD, false },
    { "getOutputBuffer", "(I)Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_output_buffer), METHOD, false },
    { "dequeueInputBuffer", "(J)I", "android/media/MediaCodec", OFF(dequeue_input_buffer), METHOD, true },
    { "dequeueOutputBuffer", "(Landroid/media/MediaCodec$BufferInfo;J)I", "android/media/MediaCodec", OFF(dequeue_output_buffer), METHOD, true },
    { "queueInputBuffer", "(IIIJI)V", "android/media/MediaCodec", OFF(queue_input_buffer), METHOD, true },
    { "releaseOutputBuffer", "(IZ)V", "android/media/MediaCodec", OFF(release_output_buffer), METHOD, true },

    { "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;", "android/media/MediaFormat", OFF(create_video_format), STATIC_METHOD, true },
    { "createAudioFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;", "android/media/MediaFormat", OFF(create_audio_format), STATIC_METHOD, true },
    { "setInteger", "(Ljava/lang/String;I)V", "android/media/MediaFormat", OFF(set_integer), METHOD, true },
    { "getInteger", "(Ljava/lang/String;)I", "android/media/MediaFormat", OFF(get_integer), METHOD, true },
    { "setByteBuffer", "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V", "android/media/MediaFormat", OFF(set_bytebuffer), METHOD, true },

    { "<init>", "()V", "android/media/MediaCodec$BufferInfo", OFF(buffer_info_ctor), METHOD, true },
    { "size", "I", "android/media/MediaCodec$BufferInfo", OFF(size_field), FIELD, true },
    { "offset", "I", "android/media/MediaCodec$BufferInfo", OFF(offset_field), FIELD, true },
    { "presentationTimeUs", "J", "android/media/MediaCodec$BufferInfo", OFF(pts_field), FIELD, true },
    { "flags", "I", "android/media/MediaCodec$BufferInfo", OFF(flags_field), FIELD, true },
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
#define GET_ENV() if (!(env = android_getEnv(api->p_obj, THREAD_NAME))) return MC_API_ERROR;

static inline jstring jni_new_string(JNIEnv *env, const char *psz_string)
{
    jstring jstring = (*env)->NewStringUTF(env, psz_string);
    return !CHECK_EXCEPTION() ? jstring : NULL;
}
#define JNI_NEW_STRING(psz_string) jni_new_string(env, psz_string)

static inline int get_integer(JNIEnv *env, jobject obj, const char *psz_name)
{
    jstring jname = JNI_NEW_STRING(psz_name);
    if (jname)
    {
        int i_ret = (*env)->CallIntMethod(env, obj, jfields.get_integer, jname);
        (*env)->DeleteLocalRef(env, jname);
        /* getInteger can throw NullPointerException (when fetching the
         * "channel-mask" property for example) */
        if (CHECK_EXCEPTION())
            return 0;
        return i_ret;
    }
    else
        return 0;
}
#define GET_INTEGER(obj, name) get_integer(env, obj, name)

static inline void set_integer(JNIEnv *env, jobject jobj, const char *psz_name,
                               int i_value)
{
    jstring jname = JNI_NEW_STRING(psz_name);
    if (jname)
    {
        (*env)->CallVoidMethod(env, jobj, jfields.set_integer, jname, i_value);
        (*env)->DeleteLocalRef(env, jname);
    }
}
#define SET_INTEGER(obj, name, value) set_integer(env, obj, name, value)

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
    /* getInputBuffers and getOutputBuffers are deprecated if API >= 21
     * use getInputBuffer and getOutputBuffer instead. */
    if (jfields.get_input_buffer && jfields.get_output_buffer)
    {
        jfields.get_output_buffers =
        jfields.get_input_buffers = NULL;
    }
    else if (!jfields.get_output_buffers && !jfields.get_input_buffers)
    {
        msg_Err(p_obj, "Unable to find get Output/Input Buffer/Buffers");
        goto end;
    }

    i_init_state = 1;
end:
    ret = i_init_state == 1;
    if (!ret)
        msg_Err(p_obj, "MediaCodec jni init failed");

    vlc_mutex_unlock(&lock);
    return ret;
}

/****************************************************************************
 * Local prototypes
 ****************************************************************************/

struct mc_api_sys
{
    jobject codec;
    jobject buffer_info;
    jobject input_buffers, output_buffers;
};

/*****************************************************************************
 * MediaCodec_GetName
 *****************************************************************************/
char* MediaCodec_GetName(vlc_object_t *p_obj, const char *psz_mime,
                         int profile, bool *p_adaptive)
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

                        int codec_profile = 0;
                        if (strcmp(psz_mime, "video/avc") == 0)
                            codec_profile = convert_omx_to_profile_idc(omx_profile);
                        else if (strcmp(psz_mime, "video/hevc") == 0)
                        {
                            switch (omx_profile)
                            {
                                case 0x1: /* OMX_VIDEO_HEVCProfileMain */
                                    codec_profile = HEVC_PROFILE_MAIN;
                                    break;
                                case 0x2:    /* OMX_VIDEO_HEVCProfileMain10 */
                                case 0x1000: /* OMX_VIDEO_HEVCProfileMain10HDR10 */
                                    codec_profile = HEVC_PROFILE_MAIN_10;
                                    break;
                            }
                        }
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
            }
            *p_adaptive = b_adaptive;
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

/*****************************************************************************
 * Stop
 *****************************************************************************/
static int Stop(mc_api *api)
{
    mc_api_sys *p_sys = api->p_sys;
    JNIEnv *env;

    api->b_direct_rendering = false;

    GET_ENV();

    if (p_sys->input_buffers)
    {
        (*env)->DeleteGlobalRef(env, p_sys->input_buffers);
        p_sys->input_buffers = NULL;
    }
    if (p_sys->output_buffers)
    {
        (*env)->DeleteGlobalRef(env, p_sys->output_buffers);
        p_sys->output_buffers = NULL;
    }
    if (p_sys->codec)
    {
        if (api->b_started)
        {
            (*env)->CallVoidMethod(env, p_sys->codec, jfields.stop);
            if (CHECK_EXCEPTION())
                msg_Err(api->p_obj, "Exception in MediaCodec.stop");
            api->b_started = false;
        }

        (*env)->CallVoidMethod(env, p_sys->codec, jfields.release);
        if (CHECK_EXCEPTION())
            msg_Err(api->p_obj, "Exception in MediaCodec.release");
        (*env)->DeleteGlobalRef(env, p_sys->codec);
        p_sys->codec = NULL;
    }
    if (p_sys->buffer_info)
    {
        (*env)->DeleteGlobalRef(env, p_sys->buffer_info);
        p_sys->buffer_info = NULL;
    }
    msg_Dbg(api->p_obj, "MediaCodec via JNI closed");
    return 0;
}

/*****************************************************************************
 * Start
 *****************************************************************************/
static int Start(mc_api *api, union mc_api_args *p_args)
{
    mc_api_sys *p_sys = api->p_sys;
    JNIEnv* env = NULL;
    int i_ret = MC_API_ERROR;
    bool b_direct_rendering = false;
    jstring jmime = NULL;
    jstring jcodec_name = NULL;
    jobject jcodec = NULL;
    jobject jformat = NULL;
    jobject jinput_buffers = NULL;
    jobject joutput_buffers = NULL;
    jobject jbuffer_info = NULL;
    jobject jsurface = NULL;

    assert(api->psz_mime && api->psz_name);

    GET_ENV();

    jmime = JNI_NEW_STRING(api->psz_mime);
    jcodec_name = JNI_NEW_STRING(api->psz_name);
    if (!jmime || !jcodec_name)
        goto error;

    /* This method doesn't handle errors nicely, it crashes if the codec isn't
     * found.  (The same goes for createDecoderByType.) This is fixed in latest
     * AOSP and in 4.2, but not in 4.1 devices. */
    jcodec = (*env)->CallStaticObjectMethod(env, jfields.media_codec_class,
                                            jfields.create_by_codec_name,
                                            jcodec_name);
    if (CHECK_EXCEPTION())
    {
        msg_Warn(api->p_obj, "Exception occurred in MediaCodec.createByCodecName");
        goto error;
    }
    p_sys->codec = (*env)->NewGlobalRef(env, jcodec);

    if (api->i_cat == VIDEO_ES)
    {
        assert(p_args->video.i_angle == 0 || api->b_support_rotation);
        jformat = (*env)->CallStaticObjectMethod(env,
                                                 jfields.media_format_class,
                                                 jfields.create_video_format,
                                                 jmime,
                                                 p_args->video.i_width,
                                                 p_args->video.i_height);
        jsurface = p_args->video.p_jsurface;
        b_direct_rendering = !!jsurface;

        if (p_args->video.i_angle != 0)
            SET_INTEGER(jformat, "rotation-degrees", p_args->video.i_angle);

        if (b_direct_rendering)
        {
            /* feature-tunneled-playback available since API 21 */
            if (jfields.get_input_buffer && p_args->video.b_tunneled_playback)
                SET_INTEGER(jformat, "feature-tunneled-playback", 1);

            if (p_args->video.b_adaptive_playback)
                SET_INTEGER(jformat, "feature-adaptive-playback", 1);
        }
    }
    else
    {
        jformat = (*env)->CallStaticObjectMethod(env,
                                                 jfields.media_format_class,
                                                 jfields.create_audio_format,
                                                 jmime,
                                                 p_args->audio.i_sample_rate,
                                                 p_args->audio.i_channel_count);
    }
    /* No limits for input size */
    SET_INTEGER(jformat, "max-input-size", 0);

    if (b_direct_rendering)
    {
        // Configure MediaCodec with the Android surface.
        (*env)->CallVoidMethod(env, p_sys->codec, jfields.configure,
                               jformat, jsurface, NULL, 0);
        if (CHECK_EXCEPTION())
        {
            msg_Warn(api->p_obj, "Exception occurred in MediaCodec.configure "
                                 "with an output surface.");
            goto error;
        }
    }
    else
    {
        (*env)->CallVoidMethod(env, p_sys->codec, jfields.configure,
                               jformat, NULL, NULL, 0);
        if (CHECK_EXCEPTION())
        {
            msg_Warn(api->p_obj, "Exception occurred in MediaCodec.configure");
            goto error;
        }
    }

    (*env)->CallVoidMethod(env, p_sys->codec, jfields.start);
    if (CHECK_EXCEPTION())
    {
        msg_Warn(api->p_obj, "Exception occurred in MediaCodec.start");
        goto error;
    }
    api->b_started = true;

    if (jfields.get_input_buffers && jfields.get_output_buffers)
    {

        jinput_buffers = (*env)->CallObjectMethod(env, p_sys->codec,
                                                  jfields.get_input_buffers);
        if (CHECK_EXCEPTION())
        {
            msg_Err(api->p_obj, "Exception in MediaCodec.getInputBuffers");
            goto error;
        }
        p_sys->input_buffers = (*env)->NewGlobalRef(env, jinput_buffers);

        joutput_buffers = (*env)->CallObjectMethod(env, p_sys->codec,
                                                   jfields.get_output_buffers);
        if (CHECK_EXCEPTION())
        {
            msg_Err(api->p_obj, "Exception in MediaCodec.getOutputBuffers");
            goto error;
        }
        p_sys->output_buffers = (*env)->NewGlobalRef(env, joutput_buffers);
    }
    jbuffer_info = (*env)->NewObject(env, jfields.buffer_info_class,
                                     jfields.buffer_info_ctor);
    p_sys->buffer_info = (*env)->NewGlobalRef(env, jbuffer_info);

    api->b_direct_rendering = b_direct_rendering;
    i_ret = 0;
    msg_Dbg(api->p_obj, "MediaCodec via JNI opened");

error:
    if (jmime)
        (*env)->DeleteLocalRef(env, jmime);
    if (jcodec_name)
        (*env)->DeleteLocalRef(env, jcodec_name);
    if (jcodec)
        (*env)->DeleteLocalRef(env, jcodec);
    if (jformat)
        (*env)->DeleteLocalRef(env, jformat);
    if (jinput_buffers)
        (*env)->DeleteLocalRef(env, jinput_buffers);
    if (joutput_buffers)
        (*env)->DeleteLocalRef(env, joutput_buffers);
    if (jbuffer_info)
        (*env)->DeleteLocalRef(env, jbuffer_info);

    if (i_ret != 0)
        Stop(api);
    return i_ret;
}

/*****************************************************************************
 * Flush
 *****************************************************************************/
static int Flush(mc_api *api)
{
    mc_api_sys *p_sys = api->p_sys;
    JNIEnv *env = NULL;

    GET_ENV();

    (*env)->CallVoidMethod(env, p_sys->codec, jfields.flush);
    if (CHECK_EXCEPTION())
    {
        msg_Warn(api->p_obj, "Exception occurred in MediaCodec.flush");
        return MC_API_ERROR;
    }
    return 0;
}

/*****************************************************************************
 * DequeueInput
 *****************************************************************************/
static int DequeueInput(mc_api *api, mtime_t i_timeout)
{
    mc_api_sys *p_sys = api->p_sys;
    JNIEnv *env;
    int i_index;

    GET_ENV();

    i_index = (*env)->CallIntMethod(env, p_sys->codec,
                                    jfields.dequeue_input_buffer, i_timeout);
    if (CHECK_EXCEPTION())
    {
        msg_Err(api->p_obj, "Exception occurred in MediaCodec.dequeueInputBuffer");
        return MC_API_ERROR;
    }
    if (i_index >= 0)
        return i_index;
    else
        return MC_API_INFO_TRYAGAIN;

}

/*****************************************************************************
 * QueueInput
 *****************************************************************************/
static int QueueInput(mc_api *api, int i_index, const void *p_buf,
                      size_t i_size, mtime_t i_ts, bool b_config)
{
    mc_api_sys *p_sys = api->p_sys;
    JNIEnv *env;
    uint8_t *p_mc_buf;
    jobject j_mc_buf;
    jsize j_mc_size;
    jint jflags = (b_config ? BUFFER_FLAG_CODEC_CONFIG : 0)
                | (p_buf == NULL ? BUFFER_FLAG_END_OF_STREAM : 0);

    assert(i_index >= 0);

    GET_ENV();

    if (jfields.get_input_buffers)
        j_mc_buf = (*env)->GetObjectArrayElement(env, p_sys->input_buffers,
                                                 i_index);
    else
    {
        j_mc_buf = (*env)->CallObjectMethod(env, p_sys->codec,
                                            jfields.get_input_buffer, i_index);
        if (CHECK_EXCEPTION())
        {
            msg_Err(api->p_obj, "Exception in MediaCodec.getInputBuffer");
            return MC_API_ERROR;
        }
    }
    j_mc_size = (*env)->GetDirectBufferCapacity(env, j_mc_buf);
    p_mc_buf = (*env)->GetDirectBufferAddress(env, j_mc_buf);
    if (j_mc_size < 0)
    {
        msg_Err(api->p_obj, "Java buffer has invalid size");
        (*env)->DeleteLocalRef(env, j_mc_buf);
        return MC_API_ERROR;
    }
    if ((size_t) j_mc_size > i_size)
        j_mc_size = i_size;
    memcpy(p_mc_buf, p_buf, j_mc_size);

    (*env)->CallVoidMethod(env, p_sys->codec, jfields.queue_input_buffer,
                           i_index, 0, j_mc_size, i_ts, jflags);
    (*env)->DeleteLocalRef(env, j_mc_buf);
    if (CHECK_EXCEPTION())
    {
        msg_Err(api->p_obj, "Exception in MediaCodec.queueInputBuffer");
        return MC_API_ERROR;
    }

    return 0;
}

/*****************************************************************************
 * DequeueOutput
 *****************************************************************************/
static int DequeueOutput(mc_api *api, mtime_t i_timeout)
{
    mc_api_sys *p_sys = api->p_sys;
    JNIEnv *env;
    int i_index;

    GET_ENV();
    i_index = (*env)->CallIntMethod(env, p_sys->codec,
                                    jfields.dequeue_output_buffer,
                                    p_sys->buffer_info, i_timeout);
    if (CHECK_EXCEPTION())
    {
        msg_Warn(api->p_obj, "Exception in MediaCodec.dequeueOutputBuffer");
        return MC_API_ERROR;
    }
    if (i_index >= 0)
        return i_index;
    else if (i_index == INFO_OUTPUT_FORMAT_CHANGED)
        return MC_API_INFO_OUTPUT_FORMAT_CHANGED;
    else if (i_index == INFO_OUTPUT_BUFFERS_CHANGED)
        return MC_API_INFO_OUTPUT_BUFFERS_CHANGED;
    else
        return MC_API_INFO_TRYAGAIN;
}

/*****************************************************************************
 * GetOutput
 *****************************************************************************/
static int GetOutput(mc_api *api, int i_index, mc_api_out *p_out)
{
    mc_api_sys *p_sys = api->p_sys;
    JNIEnv *env;

    GET_ENV();

    if (i_index >= 0)
    {
        p_out->type = MC_OUT_TYPE_BUF;
        p_out->buf.i_index = i_index;
        p_out->buf.i_ts = (*env)->GetLongField(env, p_sys->buffer_info,
                                                 jfields.pts_field);

        int flags = (*env)->GetIntField(env, p_sys->buffer_info,
                                        jfields.flags_field);
        p_out->b_eos = flags & BUFFER_FLAG_END_OF_STREAM;

        if (api->b_direct_rendering)
        {
            p_out->buf.p_ptr = NULL;
            p_out->buf.i_size = 0;
        }
        else
        {
            jobject buf;
            uint8_t *ptr = NULL;
            int offset = 0;

            if (jfields.get_output_buffers)
                buf = (*env)->GetObjectArrayElement(env, p_sys->output_buffers,
                                                    i_index);
            else
            {
                buf = (*env)->CallObjectMethod(env, p_sys->codec,
                                               jfields.get_output_buffer,
                                               i_index);
                if (CHECK_EXCEPTION())
                {
                    msg_Err(api->p_obj, "Exception in MediaCodec.getOutputBuffer");
                    return MC_API_ERROR;
                }
            }
            //jsize buf_size = (*env)->GetDirectBufferCapacity(env, buf);
            /* buf can be NULL in case of EOS */
            if (buf)
            {
                ptr = (*env)->GetDirectBufferAddress(env, buf);

                offset = (*env)->GetIntField(env, p_sys->buffer_info,
                                             jfields.offset_field);
            }
            p_out->buf.p_ptr = ptr + offset;
            p_out->buf.i_size = (*env)->GetIntField(env, p_sys->buffer_info,
                                                       jfields.size_field);
            (*env)->DeleteLocalRef(env, buf);
        }
        return 1;
    } else if (i_index == MC_API_INFO_OUTPUT_FORMAT_CHANGED)
    {
        jobject format = NULL;
        jobject format_string = NULL;
        jsize format_len;
        const char *format_ptr;

        format = (*env)->CallObjectMethod(env, p_sys->codec,
                                          jfields.get_output_format);
        if (CHECK_EXCEPTION())
        {
            msg_Err(api->p_obj, "Exception in MediaCodec.getOutputFormat");
            return MC_API_ERROR;
        }

        format_string = (*env)->CallObjectMethod(env, format, jfields.tostring);

        format_len = (*env)->GetStringUTFLength(env, format_string);
        format_ptr = (*env)->GetStringUTFChars(env, format_string, NULL);
        msg_Dbg(api->p_obj, "output format changed: %.*s", format_len,
                format_ptr);
        (*env)->ReleaseStringUTFChars(env, format_string, format_ptr);

        p_out->type = MC_OUT_TYPE_CONF;
        p_out->b_eos = false;
        if (api->i_cat == VIDEO_ES)
        {
            p_out->conf.video.width         = GET_INTEGER(format, "width");
            p_out->conf.video.height        = GET_INTEGER(format, "height");
            p_out->conf.video.stride        = GET_INTEGER(format, "stride");
            p_out->conf.video.slice_height  = GET_INTEGER(format, "slice-height");
            p_out->conf.video.pixel_format  = GET_INTEGER(format, "color-format");
            p_out->conf.video.crop_left     = GET_INTEGER(format, "crop-left");
            p_out->conf.video.crop_top      = GET_INTEGER(format, "crop-top");
            p_out->conf.video.crop_right    = GET_INTEGER(format, "crop-right");
            p_out->conf.video.crop_bottom   = GET_INTEGER(format, "crop-bottom");
        }
        else
        {
            p_out->conf.audio.channel_count = GET_INTEGER(format, "channel-count");
            p_out->conf.audio.channel_mask = GET_INTEGER(format, "channel-mask");
            p_out->conf.audio.sample_rate = GET_INTEGER(format, "sample-rate");
        }

        (*env)->DeleteLocalRef(env, format);
        return 1;
    }
    else if (i_index == MC_API_INFO_OUTPUT_BUFFERS_CHANGED)
    {
        jobject joutput_buffers;

        msg_Dbg(api->p_obj, "output buffers changed");
        if (!jfields.get_output_buffers)
            return 0;
        (*env)->DeleteGlobalRef(env, p_sys->output_buffers);

        joutput_buffers = (*env)->CallObjectMethod(env, p_sys->codec,
                                                   jfields.get_output_buffers);
        if (CHECK_EXCEPTION())
        {
            msg_Err(api->p_obj, "Exception in MediaCodec.getOutputBuffer");
            p_sys->output_buffers = NULL;
            return MC_API_ERROR;
        }
        p_sys->output_buffers = (*env)->NewGlobalRef(env, joutput_buffers);
        (*env)->DeleteLocalRef(env, joutput_buffers);
    }
    return 0;
}

/*****************************************************************************
 * ReleaseOutput
 *****************************************************************************/
static int ReleaseOutput(mc_api *api, int i_index, bool b_render)
{
    mc_api_sys *p_sys = api->p_sys;
    JNIEnv *env;

    assert(i_index >= 0);

    GET_ENV();

    (*env)->CallVoidMethod(env, p_sys->codec, jfields.release_output_buffer,
                           i_index, b_render);
    if (CHECK_EXCEPTION())
    {
        msg_Err(api->p_obj, "Exception in MediaCodec.releaseOutputBuffer");
        return MC_API_ERROR;
    }
    return 0;
}

/*****************************************************************************
 * SetOutputSurface
 *****************************************************************************/
static int SetOutputSurface(mc_api *api, void *p_surface, void *p_jsurface)
{
    (void) api; (void) p_surface; (void) p_jsurface;

    return MC_API_ERROR;
}

/*****************************************************************************
 * Clean
 *****************************************************************************/
static void Clean(mc_api *api)
{
    free(api->psz_name);
    free(api->p_sys);
}

/*****************************************************************************
 * Configure
 *****************************************************************************/
static int Configure(mc_api *api, int i_profile)
{
    free(api->psz_name);
    bool b_adaptive;
    api->psz_name = MediaCodec_GetName(api->p_obj, api->psz_mime,
                                       i_profile, &b_adaptive);
    if (!api->psz_name)
        return MC_API_ERROR;
    api->i_quirks = OMXCodec_GetQuirks(api->i_cat, api->i_codec, api->psz_name,
                                       strlen(api->psz_name));

    /* Allow interlaced picture after API 21 */
    if (jfields.get_input_buffer && jfields.get_output_buffer)
        api->i_quirks |= MC_API_VIDEO_QUIRKS_SUPPORT_INTERLACED;
    if (b_adaptive)
        api->i_quirks |= MC_API_VIDEO_QUIRKS_ADAPTIVE;
    return 0;
}

/*****************************************************************************
 * MediaCodecJni_New
 *****************************************************************************/
int MediaCodecJni_Init(mc_api *api)
{
    JNIEnv *env;

    GET_ENV();

    if (!InitJNIFields(api->p_obj, env))
        return MC_API_ERROR;

    api->p_sys = calloc(1, sizeof(mc_api_sys));
    if (!api->p_sys)
        return MC_API_ERROR;

    api->clean = Clean;
    api->configure = Configure;
    api->start = Start;
    api->stop = Stop;
    api->flush = Flush;
    api->dequeue_in = DequeueInput;
    api->queue_in = QueueInput;
    api->dequeue_out = DequeueOutput;
    api->get_out = GetOutput;
    api->release_out = ReleaseOutput;
    api->release_out_ts = NULL;
    api->set_output_surface = SetOutputSurface;

    /* Allow rotation only after API 21 */
    if (jfields.get_input_buffer && jfields.get_output_buffer)
        api->b_support_rotation = true;
    return 0;
}
