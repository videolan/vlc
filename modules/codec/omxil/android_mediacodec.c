/*****************************************************************************
 * android_mediacodec.c: Video decoder module using the Android MediaCodec API
 *****************************************************************************
 * Copyright (C) 2012 Martin Storsjo
 *
 * Authors: Martin Storsjo <martin@martin.st>
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
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block_helper.h>
#include <vlc_cpu.h>

#include "../../packetizer/h264_nal.h"
#include "../../packetizer/hevc_nal.h"
#include <OMX_Core.h>
#include <OMX_Component.h>
#include "omxil_utils.h"
#include "android_opaque.h"
#include "../../video_output/android/android_window.h"

#define BUFFER_FLAG_CODEC_CONFIG  2

#define INFO_OUTPUT_BUFFERS_CHANGED -3
#define INFO_OUTPUT_FORMAT_CHANGED  -2
#define INFO_TRY_AGAIN_LATER        -1

#define THREAD_NAME "android_mediacodec"
extern JNIEnv *jni_get_env(const char *name);

/* JNI functions to get/set an Android Surface object. */
extern jobject jni_LockAndGetAndroidJavaSurface();
extern void jni_UnlockAndroidSurface();
extern void jni_EventHardwareAccelerationError();

/* Implementation of a circular buffer of timestamps with overwriting
 * of older values. MediaCodec has only one type of timestamp, if a
 * block has no PTS, we send the DTS instead. Some hardware decoders
 * cannot cope with this situation and output the frames in the wrong
 * order. As a workaround in this case, we use a FIFO of timestamps in
 * order to remember which input packets had no PTS.  Since an
 * hardware decoder can silently drop frames, this might cause a
 * growing desynchronization with the actual timestamp. Thus the
 * circular buffer has a limited size and will overwrite older values.
 */
typedef struct
{
    uint32_t          begin;
    uint32_t          size;
    uint32_t          capacity;
    int64_t           *buffer;
} timestamp_fifo_t;

static timestamp_fifo_t *timestamp_FifoNew(uint32_t capacity)
{
    timestamp_fifo_t *fifo = calloc(1, sizeof(*fifo));
    if (!fifo)
        return NULL;
    fifo->buffer = malloc(capacity * sizeof(*fifo->buffer));
    if (!fifo->buffer) {
        free(fifo);
        return NULL;
    }
    fifo->capacity = capacity;
    return fifo;
}

static void timestamp_FifoRelease(timestamp_fifo_t *fifo)
{
    free(fifo->buffer);
    free(fifo);
}

static bool timestamp_FifoIsEmpty(timestamp_fifo_t *fifo)
{
    return fifo->size == 0;
}

static bool timestamp_FifoIsFull(timestamp_fifo_t *fifo)
{
    return fifo->size == fifo->capacity;
}

static void timestamp_FifoEmpty(timestamp_fifo_t *fifo)
{
    fifo->size = 0;
}

static void timestamp_FifoPut(timestamp_fifo_t *fifo, int64_t ts)
{
    uint32_t end = (fifo->begin + fifo->size) % fifo->capacity;
    fifo->buffer[end] = ts;
    if (!timestamp_FifoIsFull(fifo))
        fifo->size += 1;
    else
        fifo->begin = (fifo->begin + 1) % fifo->capacity;
}

static int64_t timestamp_FifoGet(timestamp_fifo_t *fifo)
{
    if (timestamp_FifoIsEmpty(fifo))
        return VLC_TS_INVALID;

    int64_t result = fifo->buffer[fifo->begin];
    fifo->begin = (fifo->begin + 1) % fifo->capacity;
    fifo->size -= 1;
    return result;
}

struct decoder_sys_t
{
    uint32_t nal_size;

    jobject codec;
    jobject buffer_info;
    jobject input_buffers, output_buffers;
    int pixel_format;
    int stride, slice_height;
    char *name;

    /* "csd-0" buffer */
    void *p_csd0_buffer;
    size_t i_csd0_buffer;
    /* or buffer sent via BUFFER_FLAG_CODEC_CONFIG flag */
    uint8_t *p_config_buffer;
    size_t i_config_buffer;
    bool b_config_resend;

    bool b_update_format;
    int i_width;
    int i_height;

    bool allocated;
    bool started;
    bool decoded;
    bool error_state;
    bool b_new_block;

    ArchitectureSpecificCopyData architecture_specific_data;

    /* Direct rendering members. */
    bool direct_rendering;
    picture_t** pp_inflight_pictures; /**< stores the inflight picture for each output buffer or NULL */
    unsigned int i_inflight_pictures;

    timestamp_fifo_t *timestamp_fifo;

    int64_t i_preroll_end;
};

struct jfields
{
    jclass media_codec_list_class, media_codec_class, media_format_class;
    jclass buffer_info_class, byte_buffer_class;
    jmethodID tostring;
    jmethodID get_codec_count, get_codec_info_at, is_encoder, get_capabilities_for_type;
    jfieldID profile_levels_field, profile_field, level_field;
    jmethodID get_supported_types, get_name;
    jmethodID create_by_codec_name, configure, start, stop, flush, release;
    jmethodID get_output_format;
    jmethodID get_input_buffers, get_input_buffer;
    jmethodID get_output_buffers, get_output_buffer;
    jmethodID dequeue_input_buffer, dequeue_output_buffer, queue_input_buffer;
    jmethodID release_output_buffer;
    jmethodID create_video_format, set_integer, set_bytebuffer, get_integer;
    jmethodID buffer_info_ctor;
    jfieldID size_field, offset_field, pts_field;
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
    { "setInteger", "(Ljava/lang/String;I)V", "android/media/MediaFormat", OFF(set_integer), METHOD, true },
    { "getInteger", "(Ljava/lang/String;)I", "android/media/MediaFormat", OFF(get_integer), METHOD, true },
    { "setByteBuffer", "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V", "android/media/MediaFormat", OFF(set_bytebuffer), METHOD, true },

    { "<init>", "()V", "android/media/MediaCodec$BufferInfo", OFF(buffer_info_ctor), METHOD, true },
    { "size", "I", "android/media/MediaCodec$BufferInfo", OFF(size_field), FIELD, true },
    { "offset", "I", "android/media/MediaCodec$BufferInfo", OFF(offset_field), FIELD, true },
    { "presentationTimeUs", "J", "android/media/MediaCodec$BufferInfo", OFF(pts_field), FIELD, true },
    { NULL, NULL, NULL, 0, 0, false },
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);
static void CloseMediaCodec(decoder_t *p_dec, JNIEnv *env);

static picture_t *DecodeVideo(decoder_t *, block_t **);

static void InvalidateAllPictures(decoder_t *);
static int InsertInflightPicture(decoder_t *, picture_t *, unsigned int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DIRECTRENDERING_TEXT N_("Android direct rendering")
#define DIRECTRENDERING_LONGTEXT N_(\
        "Enable Android direct rendering using opaque buffers.")

#define CFG_PREFIX "mediacodec-"

vlc_module_begin ()
    set_description( N_("Video decoder using Android MediaCodec") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_section( N_("Decoding") , NULL )
    set_capability( "decoder", 0 ) /* Only enabled via commandline arguments */
    add_bool(CFG_PREFIX "dr", true,
             DIRECTRENDERING_TEXT, DIRECTRENDERING_LONGTEXT, true)
    set_callbacks( OpenDecoder, CloseDecoder )
vlc_module_end ()

static inline int get_integer(JNIEnv *env, jobject obj, const char *psz_name)
{
    int i_ret;
    jstring jname = (*env)->NewStringUTF(env, psz_name);
    i_ret = (*env)->CallIntMethod(env, obj, jfields.get_integer, jname);
    (*env)->DeleteLocalRef(env, jname);
    return i_ret;
}
#define GET_INTEGER(obj, name) get_integer(env, obj, name)

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

static inline bool check_exception( JNIEnv *env )
{
    if ((*env)->ExceptionOccurred(env)) {
        (*env)->ExceptionClear(env);
        return true;
    }
    else
        return false;
}
#define CHECK_EXCEPTION() check_exception( env )

/* Initialize all jni fields.
 * Done only one time during the first initialisation */
static bool
InitJNIFields( decoder_t *p_dec, JNIEnv *env )
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static int i_init_state = -1;
    bool ret;

    vlc_mutex_lock( &lock );

    if( i_init_state != -1 )
        goto end;

    i_init_state = 0;

    for (int i = 0; classes[i].name; i++) {
        jclass clazz = (*env)->FindClass(env, classes[i].name);
        if (CHECK_EXCEPTION()) {
            msg_Warn(p_dec, "Unable to find class %s", classes[i].name);
            goto end;
        }
        *(jclass*)((uint8_t*)&jfields + classes[i].offset) =
            (jclass) (*env)->NewGlobalRef(env, clazz);
    }

    jclass last_class;
    for (int i = 0; members[i].name; i++) {
        if (i == 0 || strcmp(members[i].class, members[i - 1].class))
            last_class = (*env)->FindClass(env, members[i].class);

        if (CHECK_EXCEPTION()) {
            msg_Warn(p_dec, "Unable to find class %s", members[i].class);
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
        if (CHECK_EXCEPTION()) {
            msg_Warn(p_dec, "Unable to find the member %s in %s",
                     members[i].name, members[i].class);
            if (members[i].critical)
                goto end;
        }
    }
    /* getInputBuffers and getOutputBuffers are deprecated if API >= 21
     * use getInputBuffer and getOutputBuffer instead. */
    if (jfields.get_input_buffer && jfields.get_output_buffer) {
        jfields.get_output_buffers =
        jfields.get_input_buffers = NULL;
    } else if (!jfields.get_output_buffers && !jfields.get_input_buffers) {
        msg_Err(p_dec, "Unable to find get Output/Input Buffer/Buffers");
        goto end;
    }

    i_init_state = 1;
end:
    ret = i_init_state == 1;
    if( !ret )
        msg_Err( p_dec, "MediaCodec jni init failed" );

    vlc_mutex_unlock( &lock );
    return ret;
}

static int H264GetSPSPPS(uint8_t *p_buf, size_t i_buf,
                         uint8_t **pp_sps_buf, size_t *p_sps_size,
                         uint8_t **pp_pps_buf, size_t *p_pps_size,
                         struct nal_sps *p_sps)
{
    uint8_t *p_sps_buf, *p_pps_buf;
    size_t i_sps_size, i_pps_size;

    if (h264_get_spspps(p_buf, i_buf, &p_sps_buf, &i_sps_size,
                        &p_pps_buf, &i_pps_size) == 0)
    {
        if (pp_sps_buf && p_sps_size )
        {
            *pp_sps_buf = p_sps_buf;
            *p_sps_size = i_sps_size;
        }
        if (pp_pps_buf && p_pps_size )
        {
            *pp_pps_buf = p_pps_buf;
            *p_pps_size = i_pps_size;
        }
        return h264_parse_sps(p_sps_buf, i_sps_size, p_sps);
    } else
        return -1;
}

static jstring GetMediaCodecName(decoder_t *p_dec, JNIEnv *env,
                                 const char *mime, jstring jmime)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int num_codecs;
    size_t fmt_profile = 0;
    jstring jcodec_name = NULL;

    if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
        h264_get_profile_level(&p_dec->fmt_in, &fmt_profile, NULL, NULL);

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

        info = (*env)->CallStaticObjectMethod(env, jfields.media_codec_list_class,
                                              jfields.get_codec_info_at, i);

        name = (*env)->CallObjectMethod(env, info, jfields.get_name);
        name_len = (*env)->GetStringUTFLength(env, name);
        name_ptr = (*env)->GetStringUTFChars(env, name, NULL);

        if (OMXCodec_IsBlacklisted( name_ptr, name_len))
            goto loopclean;

        if ((*env)->CallBooleanMethod(env, info, jfields.is_encoder))
            goto loopclean;

        codec_capabilities = (*env)->CallObjectMethod(env, info,
                                                      jfields.get_capabilities_for_type,
                                                      jmime);
        if (CHECK_EXCEPTION()) {
            msg_Warn(p_dec, "Exception occurred in MediaCodecInfo.getCapabilitiesForType");
            goto loopclean;
        } else if (codec_capabilities) {
            profile_levels = (*env)->GetObjectField(env, codec_capabilities, jfields.profile_levels_field);
            if (profile_levels)
                profile_levels_len = (*env)->GetArrayLength(env, profile_levels);
        }
        msg_Dbg(p_dec, "Number of profile levels: %d", profile_levels_len);

        types = (*env)->CallObjectMethod(env, info, jfields.get_supported_types);
        num_types = (*env)->GetArrayLength(env, types);
        found = false;

        for (int j = 0; j < num_types && !found; j++) {
            jobject type = (*env)->GetObjectArrayElement(env, types, j);
            if (!jstrcmp(env, type, mime)) {
                /* The mime type is matching for this component. We
                   now check if the capabilities of the codec is
                   matching the video format. */
                if (p_dec->fmt_in.i_codec == VLC_CODEC_H264 && fmt_profile) {
                    /* This decoder doesn't expose its profiles and is high
                     * profile capable */
                    if (!strncmp(name_ptr, "OMX.LUMEVideoDecoder", __MIN(20, name_len)))
                        found = true;

                    for (int i = 0; i < profile_levels_len && !found; ++i) {
                        jobject profile_level = (*env)->GetObjectArrayElement(env, profile_levels, i);

                        int omx_profile = (*env)->GetIntField(env, profile_level, jfields.profile_field);
                        size_t codec_profile = convert_omx_to_profile_idc(omx_profile);
                        (*env)->DeleteLocalRef(env, profile_level);
                        if (codec_profile != fmt_profile)
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
        if (found) {
            msg_Dbg(p_dec, "using %.*s", name_len, name_ptr);
            p_sys->name = malloc(name_len + 1);
            memcpy(p_sys->name, name_ptr, name_len);
            p_sys->name[name_len] = '\0';
            jcodec_name = name;
        }
loopclean:
        if (name)
        {
            (*env)->ReleaseStringUTFChars(env, name, name_ptr);
            if (jcodec_name != name)
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
    return jcodec_name;
}

/*****************************************************************************
 * OpenMediaCodec: Create the mediacodec instance
 *****************************************************************************/
static int OpenMediaCodec(decoder_t *p_dec, JNIEnv *env)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    const char *mime = NULL;
    int i_ret = VLC_EGENERIC;
    jstring jmime = NULL;
    jstring jcodec_name = NULL;
    jobject jcodec = NULL;
    jobject jcsd0_buffer = NULL;
    jstring jcsd0_string = NULL;
    jobject jformat = NULL;
    jstring jrotation_string = NULL;
    jobject jinput_buffers = NULL;
    jobject joutput_buffers = NULL;
    jobject jbuffer_info = NULL;

    switch (p_dec->fmt_in.i_codec) {
    case VLC_CODEC_HEVC: mime = "video/hevc"; break;
    case VLC_CODEC_H264: mime = "video/avc"; break;
    case VLC_CODEC_H263: mime = "video/3gpp"; break;
    case VLC_CODEC_MP4V: mime = "video/mp4v-es"; break;
    case VLC_CODEC_WMV3: mime = "video/x-ms-wmv"; break;
    case VLC_CODEC_VC1:  mime = "video/wvc1"; break;
    case VLC_CODEC_VP8:  mime = "video/x-vnd.on2.vp8"; break;
    case VLC_CODEC_VP9:  mime = "video/x-vnd.on2.vp9"; break;
    default:
        vlc_assert_unreachable();
    }

    jmime = (*env)->NewStringUTF(env, mime);
    if (!jmime)
        return VLC_EGENERIC;

    jcodec_name = GetMediaCodecName(p_dec, env, mime, jmime);
    if (!jcodec_name) {
        msg_Dbg(p_dec, "No suitable codec matching %s was found", mime);
        goto error;
    }

    // This method doesn't handle errors nicely, it crashes if the codec isn't found.
    // (The same goes for createDecoderByType.) This is fixed in latest AOSP and in 4.2,
    // but not in 4.1 devices.
    jcodec = (*env)->CallStaticObjectMethod(env, jfields.media_codec_class,
                                            jfields.create_by_codec_name,
                                            jcodec_name);
    if (CHECK_EXCEPTION()) {
        msg_Warn(p_dec, "Exception occurred in MediaCodec.createByCodecName.");
        goto error;
    }
    p_sys->allocated = true;
    p_sys->codec = (*env)->NewGlobalRef(env, jcodec);

    /* Either we use a "csd-0" buffer that is provided before codec
     * initialisation via the MediaFormat class, or use a CODEC_CONFIG buffer
     * that can be provided during playback (and must be provided after a flush
     * and a start). */
    if (p_dec->fmt_in.i_extra && !p_sys->p_config_buffer) {
        uint32_t size = p_dec->fmt_in.i_extra;
        int buf_size = p_dec->fmt_in.i_extra + 20;

        /* Don't free p_csd0_buffer until Format use it, so until MediaCodec
         * is closed */
        p_sys->p_csd0_buffer = malloc(buf_size);
        if (!p_sys->p_csd0_buffer)
        {
            msg_Warn(p_dec, "extra buffer allocation failed");
            goto error;
        }
        if (p_dec->fmt_in.i_codec == VLC_CODEC_H264 && ((uint8_t*)p_dec->fmt_in.p_extra)[0] == 1) {
            convert_sps_pps(p_dec, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra,
                            p_sys->p_csd0_buffer, buf_size,
                            &size, &p_sys->nal_size);
        } else if (p_dec->fmt_in.i_codec == VLC_CODEC_HEVC) {
            convert_hevc_nal_units(p_dec, p_dec->fmt_in.p_extra,
                                   p_dec->fmt_in.i_extra,
                                   p_sys->p_csd0_buffer, buf_size,
                                   &size, &p_sys->nal_size);
        } else {
            memcpy(p_sys->p_csd0_buffer, p_dec->fmt_in.p_extra, size);
        }
        p_sys->i_csd0_buffer = size;

        if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
        {
            struct nal_sps sps;

            if (H264GetSPSPPS(p_sys->p_csd0_buffer, p_sys->i_csd0_buffer,
                              NULL, NULL, NULL, NULL, &sps) == 0)
            {
                msg_Warn(p_dec, "SPS found, id: %d size: %dx%d (vs %dx%d)",
                         sps.i_id, sps.i_width, sps.i_height,
                         p_sys->i_width, p_sys->i_height);
                p_sys->i_width = sps.i_width;
                p_sys->i_height = sps.i_height;
            }
        }
    }

    if (!p_sys->i_width && !p_sys->i_height)
    {
        msg_Err(p_dec, "invalid size, abort MediaCodec");
        goto error;
    }
    jformat = (*env)->CallStaticObjectMethod(env, jfields.media_format_class,
                                             jfields.create_video_format, jmime,
                                             p_sys->i_width, p_sys->i_height);

    if (p_sys->p_csd0_buffer)
    {
        jcsd0_buffer = (*env)->NewDirectByteBuffer( env,
                                                    p_sys->p_csd0_buffer,
                                                    p_sys->i_csd0_buffer);
        if (CHECK_EXCEPTION() || !jcsd0_buffer)
        {
            msg_Warn(p_dec, "java extra buffer allocation failed");
            free(p_sys->p_csd0_buffer);
            p_sys->p_csd0_buffer = NULL;
            goto error;
        }
        jcsd0_string = (*env)->NewStringUTF(env, "csd-0");
        (*env)->CallVoidMethod(env, jformat, jfields.set_bytebuffer,
                               jcsd0_string, jcsd0_buffer);
    }

    p_sys->direct_rendering = var_InheritBool(p_dec, CFG_PREFIX "dr");

    /* There is no way to rotate the video using direct rendering (and using a
     * SurfaceView) before  API 21 (Lollipop). Therefore, we deactivate direct
     * rendering if video doesn't have a normal rotation and if
     * get_input_buffer method is not present (This method exists since API
     * 21). */
    if (p_sys->direct_rendering
        && p_dec->fmt_in.video.orientation != ORIENT_NORMAL
        && !jfields.get_input_buffer)
        p_sys->direct_rendering = false;

    if (p_sys->direct_rendering) {
        if (p_dec->fmt_in.video.orientation != ORIENT_NORMAL) {
            int i_angle;

            switch (p_dec->fmt_in.video.orientation) {
                case ORIENT_ROTATED_90:
                    i_angle = 90;
                    break;
                case ORIENT_ROTATED_180:
                    i_angle = 180;
                    break;
                case ORIENT_ROTATED_270:
                    i_angle = 270;
                    break;
                default:
                    i_angle = 0;
            }
            jrotation_string = (*env)->NewStringUTF(env, "rotation-degrees");
            (*env)->CallVoidMethod(env, jformat, jfields.set_integer,
                                   jrotation_string, i_angle);
        }

        jobject surf = jni_LockAndGetAndroidJavaSurface();
        if (surf) {
            // Configure MediaCodec with the Android surface.
            (*env)->CallVoidMethod(env, p_sys->codec, jfields.configure,
                                   jformat, surf, NULL, 0);
            if (CHECK_EXCEPTION()) {
                msg_Warn(p_dec, "Exception occurred in MediaCodec.configure with an output surface.");
                jni_UnlockAndroidSurface();
                goto error;
            }
            p_dec->fmt_out.i_codec = VLC_CODEC_ANDROID_OPAQUE;
            jni_UnlockAndroidSurface();
        } else {
            msg_Warn(p_dec, "Failed to get the Android Surface, disabling direct rendering.");
            p_sys->direct_rendering = false;
        }
    }
    if (!p_sys->direct_rendering) {
        (*env)->CallVoidMethod(env, p_sys->codec, jfields.configure,
                               jformat, NULL, NULL, 0);
        if (CHECK_EXCEPTION()) {
            msg_Warn(p_dec, "Exception occurred in MediaCodec.configure");
            goto error;
        }
    }

    (*env)->CallVoidMethod(env, p_sys->codec, jfields.start);
    if (CHECK_EXCEPTION()) {
        msg_Warn(p_dec, "Exception occurred in MediaCodec.start");
        goto error;
    }
    p_sys->started = true;

    if (jfields.get_input_buffers && jfields.get_output_buffers) {

        jinput_buffers = (*env)->CallObjectMethod(env, p_sys->codec,
                                                  jfields.get_input_buffers);
        if (CHECK_EXCEPTION()) {
            msg_Err(p_dec, "Exception in MediaCodec.getInputBuffers (OpenDecoder)");
            goto error;
        }
        p_sys->input_buffers = (*env)->NewGlobalRef(env, jinput_buffers);

        joutput_buffers = (*env)->CallObjectMethod(env, p_sys->codec,
                                                   jfields.get_output_buffers);
        if (CHECK_EXCEPTION()) {
            msg_Err(p_dec, "Exception in MediaCodec.getOutputBuffers (OpenDecoder)");
            goto error;
        }
        p_sys->output_buffers = (*env)->NewGlobalRef(env, joutput_buffers);
    }
    jbuffer_info = (*env)->NewObject(env, jfields.buffer_info_class,
                                     jfields.buffer_info_ctor);
    p_sys->buffer_info = (*env)->NewGlobalRef(env, jbuffer_info);
    p_sys->b_update_format = true;

    i_ret = VLC_SUCCESS;

error:
    if (jmime)
        (*env)->DeleteLocalRef(env, jmime);
    if (jcodec_name)
        (*env)->DeleteLocalRef(env, jcodec_name);
    if (jcodec)
        (*env)->DeleteLocalRef(env, jcodec);
    if (jcsd0_buffer)
        (*env)->DeleteLocalRef(env, jcsd0_buffer);
    if (jcsd0_string)
        (*env)->DeleteLocalRef(env, jcsd0_string);
    if (jformat)
        (*env)->DeleteLocalRef(env, jformat);
    if (jrotation_string)
        (*env)->DeleteLocalRef(env, jrotation_string);
    if (jinput_buffers)
        (*env)->DeleteLocalRef(env, jinput_buffers);
    if (joutput_buffers)
        (*env)->DeleteLocalRef(env, joutput_buffers);
    if (jbuffer_info)
        (*env)->DeleteLocalRef(env, jbuffer_info);

    if (i_ret != VLC_SUCCESS)
        CloseMediaCodec(p_dec, env);
    return i_ret;
}

/*****************************************************************************
 * CloseMediaCodec: Close the mediacodec instance
 *****************************************************************************/
static void CloseMediaCodec(decoder_t *p_dec, JNIEnv *env)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (!p_sys)
        return;

    free(p_sys->name);
    p_sys->name = NULL;

    /* Invalidate all pictures that are currently in flight in order
     * to prevent the vout from using destroyed output buffers. */
    if (p_sys->direct_rendering) {
        InvalidateAllPictures(p_dec);
        p_sys->direct_rendering = false;
    }

    if (p_sys->input_buffers) {
        (*env)->DeleteGlobalRef(env, p_sys->input_buffers);
        p_sys->input_buffers = NULL;
    }
    if (p_sys->output_buffers) {
        (*env)->DeleteGlobalRef(env, p_sys->output_buffers);
        p_sys->output_buffers = NULL;
    }
    if (p_sys->codec) {
        if (p_sys->started)
        {
            (*env)->CallVoidMethod(env, p_sys->codec, jfields.stop);
            if (CHECK_EXCEPTION())
                msg_Err(p_dec, "Exception in MediaCodec.stop");
            p_sys->started = false;
        }
        if (p_sys->allocated)
        {
            (*env)->CallVoidMethod(env, p_sys->codec, jfields.release);
            if (CHECK_EXCEPTION())
                msg_Err(p_dec, "Exception in MediaCodec.release");
            p_sys->allocated = false;
        }
        (*env)->DeleteGlobalRef(env, p_sys->codec);
        p_sys->codec = NULL;
    }
    if (p_sys->buffer_info) {
        (*env)->DeleteGlobalRef(env, p_sys->buffer_info);
        p_sys->buffer_info = NULL;
    }
}

/*****************************************************************************
 * OpenDecoder: Create the decoder instance
 *****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;
    JNIEnv* env = NULL;

    if (p_dec->fmt_in.i_cat != VIDEO_ES && !p_dec->b_force)
        return VLC_EGENERIC;

    switch (p_dec->fmt_in.i_codec) {
    case VLC_CODEC_H264:
        /* We can handle h264 without a valid video size */
        break;
    case VLC_CODEC_HEVC:
    case VLC_CODEC_H263:
    case VLC_CODEC_MP4V:
    case VLC_CODEC_WMV3:
    case VLC_CODEC_VC1:
    case VLC_CODEC_VP8:
    case VLC_CODEC_VP9:
        if (p_dec->fmt_in.video.i_width && p_dec->fmt_in.video.i_height)
            break;
    default:
        msg_Dbg(p_dec, "codec %4.4s or resolution (%dx%d) not supported",
                (char *)&p_dec->fmt_in.i_codec,
                p_dec->fmt_in.video.i_width, p_dec->fmt_in.video.i_height);
        return VLC_EGENERIC;
    }

    if (!(env = jni_get_env(THREAD_NAME)))
        goto error;

    if (!InitJNIFields(p_dec, env))
        goto error;

    /* Allocate the memory needed to store the decoder's structure */
    if ((p_dec->p_sys = calloc(1, sizeof(*p_dec->p_sys))) == NULL)
        return VLC_ENOMEM;

    p_dec->pf_decode_video = DecodeVideo;

    p_dec->fmt_out.i_cat = p_dec->fmt_in.i_cat;
    p_dec->fmt_out.video = p_dec->fmt_in.video;
    p_dec->fmt_out.audio = p_dec->fmt_in.audio;
    p_dec->b_need_packetized = true;

    p_dec->p_sys->i_width = p_dec->fmt_in.video.i_width;
    p_dec->p_sys->i_height = p_dec->fmt_in.video.i_height;

    p_dec->p_sys->timestamp_fifo = timestamp_FifoNew(32);
    if (!p_dec->p_sys->timestamp_fifo)
        goto error;

    p_dec->p_sys->b_new_block = true;

    switch (p_dec->fmt_in.i_codec)
    {
    case VLC_CODEC_H264:
        if (!p_dec->p_sys->i_width || !p_dec->p_sys->i_height)
        {
            msg_Warn(p_dec, "waiting for sps/pps for codec %4.4s",
                     (const char *)&p_dec->fmt_in.i_codec);
            return VLC_SUCCESS;
        }
    case VLC_CODEC_VC1:
        if (!p_dec->fmt_in.i_extra)
        {
            msg_Warn(p_dec, "waiting for extra data for codec %4.4s",
                     (const char *)&p_dec->fmt_in.i_codec);
            return VLC_SUCCESS;
        }
        break;
    }
    return OpenMediaCodec(p_dec, env);

 error:
    CloseDecoder(p_this);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CloseDecoder: Close the decoder instance
 *****************************************************************************/
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    JNIEnv *env = NULL;

    if (!p_sys)
        return;

    if ((env = jni_get_env(THREAD_NAME)))
        CloseMediaCodec(p_dec, env);
    else
        msg_Warn(p_dec, "Can't get a JNIEnv, can't close mediacodec !");

    free(p_sys->p_csd0_buffer);
    free(p_sys->p_config_buffer);
    free(p_sys->name);
    ArchitectureSpecificCopyHooksDestroy(p_sys->pixel_format, &p_sys->architecture_specific_data);
    free(p_sys->pp_inflight_pictures);
    if (p_sys->timestamp_fifo)
        timestamp_FifoRelease(p_sys->timestamp_fifo);
    free(p_sys);
}

/*****************************************************************************
 * ReleaseOutputBuffer
 *****************************************************************************/
static int ReleaseOutputBuffer(decoder_t *p_dec, JNIEnv *env, int i_index,
                               bool b_render)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    (*env)->CallVoidMethod(env, p_sys->codec, jfields.release_output_buffer,
                           i_index, b_render);
    if (CHECK_EXCEPTION()) {
        msg_Err(p_dec, "Exception in MediaCodec.releaseOutputBuffer");
        return -1;
    }
    return 0;
}

/*****************************************************************************
 * vout callbacks
 *****************************************************************************/
static void UnlockPicture(picture_t* p_pic, bool b_render)
{
    picture_sys_t *p_picsys = p_pic->p_sys;
    decoder_t *p_dec = p_picsys->priv.hw.p_dec;

    if (!p_picsys->priv.hw.b_valid)
        return;

    vlc_mutex_lock(get_android_opaque_mutex());

    /* Picture might have been invalidated while waiting on the mutex. */
    if (!p_picsys->priv.hw.b_valid) {
        vlc_mutex_unlock(get_android_opaque_mutex());
        return;
    }

    uint32_t i_index = p_picsys->priv.hw.i_index;
    InsertInflightPicture(p_dec, NULL, i_index);

    /* Release the MediaCodec buffer. */
    JNIEnv *env = NULL;
    if ((env = jni_get_env(THREAD_NAME)))
        ReleaseOutputBuffer(p_dec, env, i_index, b_render);
    p_picsys->priv.hw.b_valid = false;

    vlc_mutex_unlock(get_android_opaque_mutex());
}

static void InvalidateAllPictures(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock(get_android_opaque_mutex());

    for (unsigned int i = 0; i < p_sys->i_inflight_pictures; ++i) {
        picture_t *p_pic = p_sys->pp_inflight_pictures[i];
        if (p_pic) {
            p_pic->p_sys->priv.hw.b_valid = false;
            p_sys->pp_inflight_pictures[i] = NULL;
        }
    }
    vlc_mutex_unlock(get_android_opaque_mutex());
}

static int InsertInflightPicture(decoder_t *p_dec, picture_t *p_pic,
                                 unsigned int i_index)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (i_index >= p_sys->i_inflight_pictures) {
        picture_t **pp_pics = realloc(p_sys->pp_inflight_pictures,
                                      (i_index + 1) * sizeof (picture_t *));
        if (!pp_pics)
            return -1;
        if (i_index - p_sys->i_inflight_pictures > 0)
            memset(&pp_pics[p_sys->i_inflight_pictures], 0,
                   (i_index - p_sys->i_inflight_pictures) * sizeof (picture_t *));
        p_sys->pp_inflight_pictures = pp_pics;
        p_sys->i_inflight_pictures = i_index + 1;
    }
    p_sys->pp_inflight_pictures[i_index] = p_pic;
    return 0;
}

static int PutInput(decoder_t *p_dec, JNIEnv *env, block_t *p_block, jlong timeout)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int index;
    int64_t ts = 0;
    uint8_t *p_mc_buf, *p_buf;
    size_t i_buf;
    jobject j_mc_buf;
    jsize j_mc_size;
    jint j_flags = 0;

    index = (*env)->CallIntMethod(env, p_sys->codec,
                                  jfields.dequeue_input_buffer, timeout);
    if (CHECK_EXCEPTION()) {
        msg_Err(p_dec, "Exception occurred in MediaCodec.dequeueInputBuffer");
        return -1;
    }
    if (index < 0)
        return 0;

    if (p_sys->b_config_resend)
    {
        p_buf = p_sys->p_config_buffer;
        i_buf = p_sys->i_config_buffer;
        j_flags = BUFFER_FLAG_CODEC_CONFIG;
        msg_Dbg(p_dec, "sending codec specific data of size %d "
                       "via BUFFER_FLAG_CODEC_CONFIG flag", i_buf);
    } else
    {
        p_buf = p_block->p_buffer;
        i_buf = p_block->i_buffer;
    }

    if (jfields.get_input_buffers)
        j_mc_buf = (*env)->GetObjectArrayElement(env, p_sys->input_buffers,
                                                 index);
    else
        j_mc_buf = (*env)->CallObjectMethod(env, p_sys->codec,
                                            jfields.get_input_buffer, index);
    j_mc_size = (*env)->GetDirectBufferCapacity(env, j_mc_buf);
    p_mc_buf = (*env)->GetDirectBufferAddress(env, j_mc_buf);
    if (j_mc_size < 0) {
        msg_Err(p_dec, "Java buffer has invalid size");
        (*env)->DeleteLocalRef(env, j_mc_buf);
        return -1;
    }
    if ((size_t) j_mc_size > i_buf)
        j_mc_size = i_buf;
    memcpy(p_mc_buf, p_buf, j_mc_size);

    if (!p_sys->b_config_resend)
    {
        ts = p_block->i_pts;
        if (!ts && p_block->i_dts)
            ts = p_block->i_dts;
        if (p_block->i_flags & BLOCK_FLAG_PREROLL )
            p_sys->i_preroll_end = ts;
        timestamp_FifoPut(p_sys->timestamp_fifo,
                          p_block->i_pts ? VLC_TS_INVALID : p_block->i_dts);
    }
    (*env)->CallVoidMethod(env, p_sys->codec, jfields.queue_input_buffer,
                           index, 0, j_mc_size, ts, j_flags);
    (*env)->DeleteLocalRef(env, j_mc_buf);
    if (CHECK_EXCEPTION()) {
        msg_Err(p_dec, "Exception in MediaCodec.queueInputBuffer");
        return -1;
    }
    p_sys->decoded = true;

    if (p_sys->b_config_resend)
    {
        p_sys->b_config_resend = false;
        return 0; /* 0 since the p_block is not processed */
    } else
        return 1;
}

static int GetOutput(decoder_t *p_dec, JNIEnv *env, picture_t *p_pic, jlong timeout)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int index = (*env)->CallIntMethod(env, p_sys->codec, jfields.dequeue_output_buffer,
                                      p_sys->buffer_info, timeout);
    if (CHECK_EXCEPTION()) {
        msg_Err(p_dec, "Exception in MediaCodec.dequeueOutputBuffer (GetOutput)");
        return -1;
    }

    if (index >= 0) {
        int64_t i_buffer_pts;

        /* If the oldest input block had no PTS, the timestamp of the frame
         * returned by MediaCodec might be wrong so we overwrite it with the
         * corresponding dts. Call FifoGet first in order to avoid a gap if
         * buffers are released due to an invalid format or a preroll */
        int64_t forced_ts = timestamp_FifoGet(p_sys->timestamp_fifo);

        if (!p_sys->pixel_format || !p_pic) {
            msg_Warn(p_dec, "Buffers returned before output format is set, dropping frame");
            return ReleaseOutputBuffer(p_dec, env, index, false);
        }

        i_buffer_pts = (*env)->GetLongField(env, p_sys->buffer_info, jfields.pts_field);
        if (i_buffer_pts <= p_sys->i_preroll_end)
            return ReleaseOutputBuffer(p_dec, env, index, false);

        if (forced_ts == VLC_TS_INVALID)
            p_pic->date = i_buffer_pts;
        else
            p_pic->date = forced_ts;

        if (p_sys->direct_rendering) {
            picture_sys_t *p_picsys = p_pic->p_sys;
            p_picsys->pf_lock_pic = NULL;
            p_picsys->pf_unlock_pic = UnlockPicture;
            p_picsys->priv.hw.p_dec = p_dec;
            p_picsys->priv.hw.i_index = index;
            p_picsys->priv.hw.b_valid = true;

            vlc_mutex_lock(get_android_opaque_mutex());
            InsertInflightPicture(p_dec, p_pic, index);
            vlc_mutex_unlock(get_android_opaque_mutex());
        } else {
            jobject buf;
            if (jfields.get_output_buffers)
                buf = (*env)->GetObjectArrayElement(env, p_sys->output_buffers, index);
            else
                buf = (*env)->CallObjectMethod(env, p_sys->codec,
                                               jfields.get_output_buffer, index);
            //jsize buf_size = (*env)->GetDirectBufferCapacity(env, buf);
            uint8_t *ptr = (*env)->GetDirectBufferAddress(env, buf);

            //int size = (*env)->GetIntField(env, p_sys->buffer_info, jfields.size_field);
            int offset = (*env)->GetIntField(env, p_sys->buffer_info, jfields.offset_field);
            ptr += offset; // Check the size parameter as well

            unsigned int chroma_div;
            GetVlcChromaSizes(p_dec->fmt_out.i_codec, p_dec->fmt_out.video.i_width,
                              p_dec->fmt_out.video.i_height, NULL, NULL, &chroma_div);
            CopyOmxPicture(p_sys->pixel_format, p_pic, p_sys->slice_height, p_sys->stride,
                           ptr, chroma_div, &p_sys->architecture_specific_data);
            (*env)->CallVoidMethod(env, p_sys->codec, jfields.release_output_buffer, index, false);

            jthrowable exception = (*env)->ExceptionOccurred(env);
            if (exception != NULL) {
                jclass illegalStateException = (*env)->FindClass(env, "java/lang/IllegalStateException");
                if((*env)->IsInstanceOf(env, exception, illegalStateException)) {
                    msg_Err(p_dec, "Codec error (IllegalStateException) in MediaCodec.releaseOutputBuffer");
                    (*env)->ExceptionClear(env);
                    (*env)->DeleteLocalRef(env, illegalStateException);
                    (*env)->DeleteLocalRef(env, buf);
                    return -1;
                }
            }
            (*env)->DeleteLocalRef(env, buf);
        }
        return 1;
    } else if (index == INFO_OUTPUT_BUFFERS_CHANGED) {
        jobject joutput_buffers;

        msg_Dbg(p_dec, "output buffers changed");
        if (!jfields.get_output_buffers)
            return 0;
        (*env)->DeleteGlobalRef(env, p_sys->output_buffers);

        joutput_buffers = (*env)->CallObjectMethod(env, p_sys->codec,
                                                   jfields.get_output_buffers);
        if (CHECK_EXCEPTION()) {
            msg_Err(p_dec, "Exception in MediaCodec.getOutputBuffer (GetOutput)");
            p_sys->output_buffers = NULL;
            return -1;
        }
        p_sys->output_buffers = (*env)->NewGlobalRef(env, joutput_buffers);
        (*env)->DeleteLocalRef(env, joutput_buffers);
    } else if (index == INFO_OUTPUT_FORMAT_CHANGED) {
        jobject format = NULL;
        jobject format_string = NULL;
        jsize format_len;
        const char *format_ptr;

        format = (*env)->CallObjectMethod(env, p_sys->codec,
                                          jfields.get_output_format);
        if (CHECK_EXCEPTION()) {
            msg_Err(p_dec, "Exception in MediaCodec.getOutputFormat (GetOutput)");
            return -1;
        }

        format_string = (*env)->CallObjectMethod(env, format, jfields.tostring);

        format_len = (*env)->GetStringUTFLength(env, format_string);
        format_ptr = (*env)->GetStringUTFChars(env, format_string, NULL);
        msg_Dbg(p_dec, "output format changed: %.*s", format_len, format_ptr);
        (*env)->ReleaseStringUTFChars(env, format_string, format_ptr);

        ArchitectureSpecificCopyHooksDestroy(p_sys->pixel_format, &p_sys->architecture_specific_data);

        int width           = GET_INTEGER(format, "width");
        int height          = GET_INTEGER(format, "height");
        p_sys->stride       = GET_INTEGER(format, "stride");
        p_sys->slice_height = GET_INTEGER(format, "slice-height");
        p_sys->pixel_format = GET_INTEGER(format, "color-format");
        int crop_left       = GET_INTEGER(format, "crop-left");
        int crop_top        = GET_INTEGER(format, "crop-top");
        int crop_right      = GET_INTEGER(format, "crop-right");
        int crop_bottom     = GET_INTEGER(format, "crop-bottom");
        (*env)->DeleteLocalRef(env, format);

        const char *name = "unknown";
        if (!p_sys->direct_rendering) {
            if (!GetVlcChromaFormat(p_sys->pixel_format,
                                    &p_dec->fmt_out.i_codec, &name)) {
                msg_Err(p_dec, "color-format not recognized");
                return -1;
            }
        }

        msg_Err(p_dec, "output: %d %s, %dx%d stride %d %d, crop %d %d %d %d",
                p_sys->pixel_format, name, width, height, p_sys->stride, p_sys->slice_height,
                crop_left, crop_top, crop_right, crop_bottom);

        p_dec->fmt_out.video.i_width = crop_right + 1 - crop_left;
        p_dec->fmt_out.video.i_height = crop_bottom + 1 - crop_top;
        if (p_dec->fmt_out.video.i_width <= 1
            || p_dec->fmt_out.video.i_height <= 1) {
            p_dec->fmt_out.video.i_width = width;
            p_dec->fmt_out.video.i_height = height;
        }
        p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_width;
        p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height;

        if (p_sys->stride <= 0)
            p_sys->stride = width;
        if (p_sys->slice_height <= 0)
            p_sys->slice_height = height;

        ArchitectureSpecificCopyHooks(p_dec, p_sys->pixel_format, p_sys->slice_height,
                                      p_sys->stride, &p_sys->architecture_specific_data);
        if (p_sys->pixel_format == OMX_TI_COLOR_FormatYUV420PackedSemiPlanar)
            p_sys->slice_height -= crop_top/2;
        if (IgnoreOmxDecoderPadding(p_sys->name)) {
            p_sys->slice_height = 0;
            p_sys->stride = p_dec->fmt_out.video.i_width;
        }
        p_sys->b_update_format = true;
    }
    return 0;
}

static bool spsppscmp(decoder_t *p_dec, uint8_t *p_sps_buf, size_t i_sps_size,
                      uint8_t *p_pps_buf, size_t i_pps_size)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->i_config_buffer != i_sps_size + i_pps_size)
        return false;
    if (p_sps_buf && memcmp(p_sys->p_config_buffer, p_sps_buf, i_sps_size) != 0)
        return false;
    if (p_pps_buf && memcmp(p_sys->p_config_buffer + i_sps_size,
                            p_pps_buf, i_pps_size) != 0)
        return false;
    return true;
}

static void H264ProcessBlock(decoder_t *p_dec, JNIEnv *env, block_t *p_block,
                             bool *p_delayed_open)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_sps_buf, *p_pps_buf;
    size_t i_sps_size, i_pps_size;
    struct nal_sps sps;
    struct H264ConvertState convert_state = { 0, 0 };

    assert(p_dec->fmt_in.i_codec == VLC_CODEC_H264 && p_block);

    if (p_sys->p_csd0_buffer)
    {
        convert_h264_to_annexb(p_block->p_buffer, p_block->i_buffer,
                               p_sys->nal_size, &convert_state);
    } else if (H264GetSPSPPS(p_block->p_buffer, p_block->i_buffer,
                             &p_sps_buf, &i_sps_size,
                             &p_pps_buf, &i_pps_size, &sps) == 0
        && sps.i_width && sps.i_height
        && !spsppscmp(p_dec, p_sps_buf, i_sps_size, p_pps_buf, i_pps_size))
    {
        void *p_config_buffer;

        msg_Warn(p_dec, "New SPS/PPS found, id: %d size: %dx%d (vs %dx%d) %d %d",
                 sps.i_id, sps.i_width, sps.i_height,
                 p_sys->i_width, p_sys->i_height,
                 i_sps_size, i_pps_size);

        if (p_sys->codec && (sps.i_width != p_sys->i_width ||
                             sps.i_height != p_sys->i_height))
        {
            msg_Err(p_dec, "SPS/PPS changed during the stream and "
                    "MediaCodec configured with a different video size. "
                    "Restart it !");
            CloseMediaCodec(p_dec, env);
        }
        if (!p_sys->codec)
            *p_delayed_open = true;

        p_config_buffer = realloc(p_sys->p_config_buffer,
                                  i_sps_size + i_pps_size);
        if (!p_config_buffer)
            free(p_sys->p_config_buffer);
        p_sys->p_config_buffer = p_config_buffer;

        if (p_sys->p_config_buffer)
        {
            if (p_sps_buf && i_sps_size)
                memcpy(p_sys->p_config_buffer, p_sps_buf, i_sps_size);
            if (p_pps_buf && i_pps_size)
                memcpy(p_sys->p_config_buffer + i_sps_size,
                       p_pps_buf, i_pps_size);
            p_sys->i_config_buffer = i_sps_size + i_pps_size;
            p_sys->b_config_resend = true;
        }

        p_sys->i_width = sps.i_width;
        p_sys->i_height = sps.i_height;
    }
}

static void HEVCProcessBlock(decoder_t *p_dec, JNIEnv *env, block_t *p_block,
                             bool *p_delayed_open)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct H264ConvertState convert_state = { 0, 0 };

    assert(p_dec->fmt_in.i_codec == VLC_CODEC_HEVC && p_block);

    if (p_sys->p_csd0_buffer)
    {
        convert_h264_to_annexb(p_block->p_buffer, p_block->i_buffer,
                               p_sys->nal_size, &convert_state);
    }

    /* TODO */
    VLC_UNUSED(env);
    VLC_UNUSED(p_delayed_open);
}

static picture_t *DecodeVideo(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic = NULL;
    JNIEnv *env = NULL;
    block_t *p_block = pp_block ? *pp_block : NULL;
    unsigned int i_attempts = 0;
    jlong timeout = 0;
    int i_output_ret = 0;
    int i_input_ret = 0;
    bool b_error = false;
    bool b_delayed_open = false;
    bool b_new_block = p_block ? p_sys->b_new_block : false;

    if (p_sys->error_state)
        goto endclean;

    /* Allow interlaced picture only after API 21 */
    if (p_block && p_block->i_flags & BLOCK_FLAG_INTERLACED_MASK
        && !(jfields.get_input_buffer && jfields.get_output_buffer))
    {
        b_error = true;
        goto endclean;
    }

    if (!(env = jni_get_env(THREAD_NAME)))
    {
        b_error = true;
        goto endclean;
    }

    if (p_block && p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED)) {
        p_sys->i_preroll_end = 0;
        timestamp_FifoEmpty(p_sys->timestamp_fifo);
        if (p_sys->decoded) {
            /* Invalidate all pictures that are currently in flight
             * since flushing make all previous indices returned by
             * MediaCodec invalid. */
            if (p_sys->direct_rendering)
                InvalidateAllPictures(p_dec);

            (*env)->CallVoidMethod(env, p_sys->codec, jfields.flush);
            if (CHECK_EXCEPTION()) {
                msg_Warn(p_dec, "Exception occurred in MediaCodec.flush");
                b_error = true;
            }
        }
        p_sys->decoded = false;

        /* resend CODEC_CONFIG buffer after a flush */
        if (p_sys->p_config_buffer)
            p_sys->b_config_resend = true;
        goto endclean;
    }

    if (b_new_block)
    {
        p_sys->b_new_block = false;
        if (p_dec->fmt_in.i_codec == VLC_CODEC_H264)
            H264ProcessBlock(p_dec, env, p_block, &b_delayed_open);
        else if (p_dec->fmt_in.i_codec == VLC_CODEC_HEVC)
            HEVCProcessBlock(p_dec, env, p_block, &b_delayed_open);
    }

    /* try delayed opening if there is a new extra data */
    if (!p_sys->codec)
    {
        switch (p_dec->fmt_in.i_codec)
        {
        case VLC_CODEC_VC1:
            if (p_dec->fmt_in.i_extra)
                b_delayed_open = true;
        default:
            break;
        }
        if (b_delayed_open && OpenMediaCodec(p_dec, env) != VLC_SUCCESS)
        {
            b_error = true;
            goto endclean;
        }
    }
    if (!p_sys->codec)
        goto endclean;

    /* Use the aspect ratio provided by the input (ie read from packetizer).
     * Don't check the current value of the aspect ratio in fmt_out, since we
     * want to allow changes in it to propagate. */
    if (p_dec->fmt_in.video.i_sar_num != 0 && p_dec->fmt_in.video.i_sar_den != 0
     && (p_dec->fmt_out.video.i_sar_num != p_dec->fmt_in.video.i_sar_num ||
         p_dec->fmt_out.video.i_sar_den != p_dec->fmt_in.video.i_sar_den))
    {
        p_dec->fmt_out.video.i_sar_num = p_dec->fmt_in.video.i_sar_num;
        p_dec->fmt_out.video.i_sar_den = p_dec->fmt_in.video.i_sar_den;
        p_sys->b_update_format = true;
    }

    do {
        if ((p_sys->b_config_resend || p_block) && i_input_ret == 0)
            i_input_ret = PutInput(p_dec, env, p_block, timeout);

        if (i_input_ret != -1 && i_output_ret == 0)
        {
            /* FIXME: A new picture shouldn't be created each time.
             * If decoder_NewPicture fails because the decoder is
             * flushing/exiting, GetOutput will either fail (or crash in
             * function of devices), or never return an output buffer. Indeed,
             * if the Decoder is flushing, MediaCodec can be stalled since the
             * input is waiting for the output or vice-versa.  Therefore, call
             * decoder_NewPicture before GetOutput as a safeguard. */

            if (p_sys->pixel_format)
            {
                if (p_sys->b_update_format)
                {
                    p_sys->b_update_format = false;
                    if (decoder_UpdateVideoFormat(p_dec) != 0)
                    {
                        msg_Err(p_dec, "decoder_UpdateVideoFormat failed");
                        b_error = true;
                        break;
                    }
                }
                p_pic = decoder_NewPicture(p_dec);
                if (!p_pic) {
                    msg_Warn(p_dec, "NewPicture failed");
                    break;
                }
            }
            i_output_ret = GetOutput(p_dec, env, p_pic, timeout);
            if (p_pic)
            {
                if (i_output_ret != 1) {
                    picture_Release(p_pic);
                    p_pic = NULL;
                }
            } else
            {
                if (i_output_ret == 0 && i_input_ret == 0 && ++i_attempts > 100)
                {
                    /* No p_pic, so no pixel_format, thereforce mediacodec
                     * didn't produce any output or events yet. Don't wait
                     * indefinitely and abort after 2seconds (100 * 2 * 10ms)
                     * without any data. Indeed, MediaCodec can fail without
                     * throwing any exception or error returns... */
                    b_error = true;
                    break;
                }
            }
        }
        timeout = 10 * 1000; // 10 ms
        /* loop until either the input or the output are processed (i_input_ret
         * or i_output_ret == 1 ) or caused an error (i_input_ret or
         * i_output_ret == -1 )*/
    } while (p_block && i_input_ret == 0 && i_output_ret == 0);

    if (i_input_ret == -1 || i_output_ret == -1)
        b_error = true;

endclean:

    /* If pf_decode returns NULL, we'll get a new p_block from the next
     * pf_decode call. Therefore we need to release the current one even if we
     * couldn't process it (it happens in case or error or if MediaCodec is
     * still not opened). We also must release the current p_block if we were
     * able to process it. */
    if (p_block && (p_pic == NULL || i_input_ret != 0))
    {
        block_Release(p_block);
        *pp_block = NULL;
        p_sys->b_new_block = true;
    }
    if (b_error && !p_sys->error_state) {
        /* Signal the error to the Java. */
        jni_EventHardwareAccelerationError();
        p_sys->error_state = true;
    }

    return p_pic;
}
