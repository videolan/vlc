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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block_helper.h>
#include <vlc_cpu.h>

#include "../h264_nal.h"
#include <OMX_Core.h>
#include <OMX_Component.h>
#include "omxil_utils.h"

#define INFO_OUTPUT_BUFFERS_CHANGED -3
#define INFO_OUTPUT_FORMAT_CHANGED  -2
#define INFO_TRY_AGAIN_LATER        -1

extern JavaVM *myVm;

struct decoder_sys_t
{
    jclass media_codec_list_class, media_codec_class, media_format_class;
    jclass buffer_info_class, byte_buffer_class;
    jmethodID tostring;
    jmethodID get_codec_count, get_codec_info_at, is_encoder;
    jmethodID get_supported_types, get_name;
    jmethodID create_by_codec_name, configure, start, stop, flush, release;
    jmethodID get_output_format, get_input_buffers, get_output_buffers;
    jmethodID dequeue_input_buffer, dequeue_output_buffer, queue_input_buffer;
    jmethodID release_output_buffer;
    jmethodID create_video_format, set_integer, set_bytebuffer, get_integer;
    jmethodID buffer_info_ctor;
    jmethodID allocate_direct, limit;
    jfieldID size_field, offset_field, pts_field;

    uint32_t nal_size;

    jobject codec;
    jobject buffer_info;
    jobject input_buffers, output_buffers;
    int pixel_format;
    int stride, slice_height;
    int crop_top, crop_left;
    char *name;

    int started;
    int decoded;
};

enum Types
{
    METHOD, STATIC_METHOD, FIELD
};

#define OFF(x) offsetof(struct decoder_sys_t, x)
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
};
static const struct member members[] = {
    { "toString", "()Ljava/lang/String;", "java/lang/Object", OFF(tostring), METHOD },

    { "getCodecCount", "()I", "android/media/MediaCodecList", OFF(get_codec_count), STATIC_METHOD },
    { "getCodecInfoAt", "(I)Landroid/media/MediaCodecInfo;", "android/media/MediaCodecList", OFF(get_codec_info_at), STATIC_METHOD },

    { "isEncoder", "()Z", "android/media/MediaCodecInfo", OFF(is_encoder), METHOD },
    { "getSupportedTypes", "()[Ljava/lang/String;", "android/media/MediaCodecInfo", OFF(get_supported_types), METHOD },
    { "getName", "()Ljava/lang/String;", "android/media/MediaCodecInfo", OFF(get_name), METHOD },

    { "createByCodecName", "(Ljava/lang/String;)Landroid/media/MediaCodec;", "android/media/MediaCodec", OFF(create_by_codec_name), STATIC_METHOD },
    { "configure", "(Landroid/media/MediaFormat;Landroid/view/Surface;Landroid/media/MediaCrypto;I)V", "android/media/MediaCodec", OFF(configure), METHOD },
    { "start", "()V", "android/media/MediaCodec", OFF(start), METHOD },
    { "stop", "()V", "android/media/MediaCodec", OFF(stop), METHOD },
    { "flush", "()V", "android/media/MediaCodec", OFF(flush), METHOD },
    { "release", "()V", "android/media/MediaCodec", OFF(release), METHOD },
    { "getOutputFormat", "()Landroid/media/MediaFormat;", "android/media/MediaCodec", OFF(get_output_format), METHOD },
    { "getInputBuffers", "()[Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_input_buffers), METHOD },
    { "getOutputBuffers", "()[Ljava/nio/ByteBuffer;", "android/media/MediaCodec", OFF(get_output_buffers), METHOD },
    { "dequeueInputBuffer", "(J)I", "android/media/MediaCodec", OFF(dequeue_input_buffer), METHOD },
    { "dequeueOutputBuffer", "(Landroid/media/MediaCodec$BufferInfo;J)I", "android/media/MediaCodec", OFF(dequeue_output_buffer), METHOD },
    { "queueInputBuffer", "(IIIJI)V", "android/media/MediaCodec", OFF(queue_input_buffer), METHOD },
    { "releaseOutputBuffer", "(IZ)V", "android/media/MediaCodec", OFF(release_output_buffer), METHOD },

    { "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;", "android/media/MediaFormat", OFF(create_video_format), STATIC_METHOD },
    { "setInteger", "(Ljava/lang/String;I)V", "android/media/MediaFormat", OFF(set_integer), METHOD },
    { "getInteger", "(Ljava/lang/String;)I", "android/media/MediaFormat", OFF(get_integer), METHOD },
    { "setByteBuffer", "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V", "android/media/MediaFormat", OFF(set_bytebuffer), METHOD },

    { "<init>", "()V", "android/media/MediaCodec$BufferInfo", OFF(buffer_info_ctor), METHOD },
    { "size", "I", "android/media/MediaCodec$BufferInfo", OFF(size_field), FIELD },
    { "offset", "I", "android/media/MediaCodec$BufferInfo", OFF(offset_field), FIELD },
    { "presentationTimeUs", "J", "android/media/MediaCodec$BufferInfo", OFF(pts_field), FIELD },

    { "allocateDirect", "(I)Ljava/nio/ByteBuffer;", "java/nio/ByteBuffer", OFF(allocate_direct), STATIC_METHOD },
    { "limit", "(I)Ljava/nio/Buffer;", "java/nio/ByteBuffer", OFF(limit), METHOD },

    { NULL, NULL, NULL, 0, 0 },
};

#define GET_INTEGER(obj, name) (*env)->CallIntMethod(env, obj, p_sys->get_integer, (*env)->NewStringUTF(env, name))

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);

static picture_t *DecodeVideo(decoder_t *, block_t **);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Video decoder using Android MediaCodec") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_section( N_("Decoding") , NULL )
    set_capability( "decoder", 0 ) /* Only enabled via commandline arguments */
    set_callbacks( OpenDecoder, CloseDecoder )
vlc_module_end ()

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

/*****************************************************************************
 * OpenDecoder: Create the decoder instance
 *****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if (p_dec->fmt_in.i_cat != VIDEO_ES && !p_dec->b_force)
        return VLC_EGENERIC;

    const char *mime = NULL;
    switch (p_dec->fmt_in.i_codec) {
    case VLC_CODEC_H264: mime = "video/avc"; break;
    case VLC_CODEC_H263: mime = "video/3gpp"; break;
    case VLC_CODEC_MP4V: mime = "video/mp4v-es"; break;
    case VLC_CODEC_VC1:  mime = "video/wvc1"; break;
    default:
        msg_Dbg(p_dec, "codec %d not supported", p_dec->fmt_in.i_codec);
        return VLC_EGENERIC;
    }
    /* Allocate the memory needed to store the decoder's structure */
    if ((p_dec->p_sys = p_sys = calloc(1, sizeof(*p_sys))) == NULL)
        return VLC_ENOMEM;

    p_dec->pf_decode_video = DecodeVideo;


    p_dec->fmt_out.i_cat = p_dec->fmt_in.i_cat;
    p_dec->fmt_out.video = p_dec->fmt_in.video;
    p_dec->fmt_out.audio = p_dec->fmt_in.audio;
    p_dec->b_need_packetized = true;

    JNIEnv* env = NULL;
    (*myVm)->AttachCurrentThread(myVm, &env, NULL);

    for (int i = 0; classes[i].name; i++) {
        *(jclass*)((uint8_t*)p_sys + classes[i].offset) =
            (*env)->FindClass(env, classes[i].name);

        if ((*env)->ExceptionOccurred(env)) {
            msg_Warn(p_dec, "Unable to find class %s", classes[i].name);
            (*env)->ExceptionClear(env);
            goto error;
        }
    }

    jclass last_class;
    for (int i = 0; members[i].name; i++) {
        if (i == 0 || strcmp(members[i].class, members[i - 1].class))
            last_class = (*env)->FindClass(env, members[i].class);

        if ((*env)->ExceptionOccurred(env)) {
            msg_Warn(p_dec, "Unable to find class %s", members[i].class);
            (*env)->ExceptionClear(env);
            goto error;
        }

        switch (members[i].type) {
        case METHOD:
            *(jmethodID*)((uint8_t*)p_sys + members[i].offset) =
                (*env)->GetMethodID(env, last_class, members[i].name, members[i].sig);
            break;
        case STATIC_METHOD:
            *(jmethodID*)((uint8_t*)p_sys + members[i].offset) =
                (*env)->GetStaticMethodID(env, last_class, members[i].name, members[i].sig);
            break;
        case FIELD:
            *(jfieldID*)((uint8_t*)p_sys + members[i].offset) =
                (*env)->GetFieldID(env, last_class, members[i].name, members[i].sig);
            break;
        }
        if ((*env)->ExceptionOccurred(env)) {
            msg_Warn(p_dec, "Unable to find the member %s in %s",
                     members[i].name, members[i].class);
            (*env)->ExceptionClear(env);
            goto error;
        }
    }

    int num_codecs = (*env)->CallStaticIntMethod(env, p_sys->media_codec_list_class,
                                                 p_sys->get_codec_count);
    jobject codec_name = NULL;

    for (int i = 0; i < num_codecs; i++) {
        jobject info = (*env)->CallStaticObjectMethod(env, p_sys->media_codec_list_class,
                                                      p_sys->get_codec_info_at, i);
        if ((*env)->CallBooleanMethod(env, info, p_sys->is_encoder)) {
            (*env)->DeleteLocalRef(env, info);
            continue;
        }
        jobject types = (*env)->CallObjectMethod(env, info, p_sys->get_supported_types);
        int num_types = (*env)->GetArrayLength(env, types);
        bool found = false;
        for (int j = 0; j < num_types && !found; j++) {
            jobject type = (*env)->GetObjectArrayElement(env, types, j);
            if (!jstrcmp(env, type, mime))
                found = true;
            (*env)->DeleteLocalRef(env, type);
        }
        if (found) {
            jobject name = (*env)->CallObjectMethod(env, info, p_sys->get_name);
            jsize name_len = (*env)->GetStringUTFLength(env, name);
            const char *name_ptr = (*env)->GetStringUTFChars(env, name, NULL);
            msg_Dbg(p_dec, "using %.*s", name_len, name_ptr);
            p_sys->name = malloc(name_len + 1);
            memcpy(p_sys->name, name_ptr, name_len);
            p_sys->name[name_len] = '\0';
            (*env)->ReleaseStringUTFChars(env, name, name_ptr);
            codec_name = name;
            break;
        }
        (*env)->DeleteLocalRef(env, info);
    }

    if (!codec_name) {
        msg_Dbg(p_dec, "No suitable codec matching %s was found", mime);
        goto error;
    }

    // This method doesn't handle errors nicely, it crashes if the codec isn't found.
    // (The same goes for createDecoderByType.) This is fixed in latest AOSP and in 4.2,
    // but not in 4.1 devices.
    p_sys->codec = (*env)->CallStaticObjectMethod(env, p_sys->media_codec_class,
                                                  p_sys->create_by_codec_name, codec_name);
    p_sys->codec = (*env)->NewGlobalRef(env, p_sys->codec);

    jobject format = (*env)->CallStaticObjectMethod(env, p_sys->media_format_class,
                         p_sys->create_video_format, (*env)->NewStringUTF(env, mime),
                         p_dec->fmt_in.video.i_width, p_dec->fmt_in.video.i_height);

    if (p_dec->fmt_in.i_extra) {
        // Allocate a byte buffer via allocateDirect in java instead of NewDirectByteBuffer,
        // since the latter doesn't allocate storage of its own, and we don't know how long
        // the codec uses the buffer.
        int buf_size = p_dec->fmt_in.i_extra + 20;
        jobject bytebuf = (*env)->CallStaticObjectMethod(env, p_sys->byte_buffer_class,
                                                         p_sys->allocate_direct, buf_size);
        uint32_t size = p_dec->fmt_in.i_extra;
        uint8_t *ptr = (*env)->GetDirectBufferAddress(env, bytebuf);
        if (p_dec->fmt_in.i_codec == VLC_CODEC_H264 && ((uint8_t*)p_dec->fmt_in.p_extra)[0] == 1) {
            convert_sps_pps(p_dec, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra,
                            ptr, buf_size,
                            &size, &p_sys->nal_size);
        } else {
            memcpy(ptr, p_dec->fmt_in.p_extra, size);
        }
        (*env)->CallObjectMethod(env, bytebuf, p_sys->limit, size);
        (*env)->CallVoidMethod(env, format, p_sys->set_bytebuffer,
                               (*env)->NewStringUTF(env, "csd-0"), bytebuf);
        (*env)->DeleteLocalRef(env, bytebuf);
    }

    (*env)->CallVoidMethod(env, p_sys->codec, p_sys->configure, format, NULL, NULL, 0);
    if ((*env)->ExceptionOccurred(env)) {
        msg_Warn(p_dec, "Exception occurred in MediaCodec.configure");
        (*env)->ExceptionClear(env);
        goto error;
    }
    (*env)->CallVoidMethod(env, p_sys->codec, p_sys->start);
    if ((*env)->ExceptionOccurred(env)) {
        msg_Warn(p_dec, "Exception occurred in MediaCodec.start");
        (*env)->ExceptionClear(env);
        goto error;
    }
    p_sys->started = 1;

    p_sys->input_buffers = (*env)->CallObjectMethod(env, p_sys->codec, p_sys->get_input_buffers);
    p_sys->output_buffers = (*env)->CallObjectMethod(env, p_sys->codec, p_sys->get_output_buffers);
    p_sys->buffer_info = (*env)->NewObject(env, p_sys->buffer_info_class, p_sys->buffer_info_ctor);
    p_sys->input_buffers = (*env)->NewGlobalRef(env, p_sys->input_buffers);
    p_sys->output_buffers = (*env)->NewGlobalRef(env, p_sys->output_buffers);
    p_sys->buffer_info = (*env)->NewGlobalRef(env, p_sys->buffer_info);
    (*env)->DeleteLocalRef(env, format);

    (*myVm)->DetachCurrentThread(myVm);
    return VLC_SUCCESS;

 error:
    (*myVm)->DetachCurrentThread(myVm);
    CloseDecoder(p_this);
    return VLC_EGENERIC;
}

static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    JNIEnv *env = NULL;

    if (!p_sys)
        return;

    (*myVm)->AttachCurrentThread(myVm, &env, NULL);
    if (p_sys->input_buffers)
        (*env)->DeleteGlobalRef(env, p_sys->input_buffers);
    if (p_sys->output_buffers)
        (*env)->DeleteGlobalRef(env, p_sys->output_buffers);
    if (p_sys->codec) {
        if (p_sys->started)
            (*env)->CallVoidMethod(env, p_sys->codec, p_sys->stop);
        (*env)->CallVoidMethod(env, p_sys->codec, p_sys->release);
        (*env)->DeleteGlobalRef(env, p_sys->codec);
    }
    if (p_sys->buffer_info)
        (*env)->DeleteGlobalRef(env, p_sys->buffer_info);
    (*myVm)->DetachCurrentThread(myVm);

    free(p_sys->name);
    free(p_sys);
}

static void GetOutput(decoder_t *p_dec, JNIEnv *env, picture_t **pp_pic, int loop)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    do {
        int index = (*env)->CallIntMethod(env, p_sys->codec, p_sys->dequeue_output_buffer,
                                          p_sys->buffer_info, (jlong) 0);
        if (index >= 0) {
            jobject buf = (*env)->GetObjectArrayElement(env, p_sys->output_buffers, index);
            jsize buf_size = (*env)->GetDirectBufferCapacity(env, buf);
            uint8_t *ptr = (*env)->GetDirectBufferAddress(env, buf);
            if (!*pp_pic)
                *pp_pic = decoder_NewPicture(p_dec);
            if (*pp_pic) {
                picture_t *p_pic = *pp_pic;
                int size = (*env)->GetIntField(env, p_sys->buffer_info, p_sys->size_field);
                int offset = (*env)->GetIntField(env, p_sys->buffer_info, p_sys->offset_field);
                ptr += offset; // Check the size parameter as well
                // TODO: Use crop_top/crop_left as well? Or is that already taken into account?
                // On OMX_TI_COLOR_FormatYUV420PackedSemiPlanar the offset already incldues
                // the cropping, so the top/left cropping params should just be ignored.
                unsigned int chroma_div;
                p_pic->date = (*env)->GetLongField(env, p_sys->buffer_info, p_sys->pts_field);
                GetVlcChromaSizes(p_dec->fmt_out.i_codec, p_dec->fmt_out.video.i_width,
                                  p_dec->fmt_out.video.i_height, NULL, NULL, &chroma_div);
                CopyOmxPicture(p_sys->pixel_format, p_pic, p_sys->slice_height, p_sys->stride,
                               ptr, chroma_div);
            }
            (*env)->CallVoidMethod(env, p_sys->codec, p_sys->release_output_buffer, index, false);
            jthrowable exception = (*env)->ExceptionOccurred(env);
            if(exception != NULL) {
                jclass illegalStateException = (*env)->FindClass(env, "java/lang/IllegalStateException");
                if((*env)->IsInstanceOf(env, exception, illegalStateException)) {
                    msg_Err(p_dec, "Codec error (IllegalStateException) in MediaCodec.releaseOutputBuffer");
                    (*env)->ExceptionClear(env);
                    (*env)->DeleteLocalRef(env, illegalStateException);
                }
            }
            (*env)->DeleteLocalRef(env, buf);
        } else if (index == INFO_OUTPUT_BUFFERS_CHANGED) {
            msg_Dbg(p_dec, "output buffers changed");
            (*env)->DeleteGlobalRef(env, p_sys->output_buffers);
            p_sys->output_buffers = (*env)->CallObjectMethod(env, p_sys->codec,
                                                             p_sys->get_output_buffers);
            p_sys->output_buffers = (*env)->NewGlobalRef(env, p_sys->output_buffers);
            continue;
        } else if (index == INFO_OUTPUT_FORMAT_CHANGED) {

            jobject format = (*env)->CallObjectMethod(env, p_sys->codec, p_sys->get_output_format);
            jobject format_string = (*env)->CallObjectMethod(env, format, p_sys->tostring);

            jsize format_len = (*env)->GetStringUTFLength(env, format_string);
            const char *format_ptr = (*env)->GetStringUTFChars(env, format_string, NULL);
            msg_Dbg(p_dec, "output format changed: %.*s", format_len, format_ptr);
            (*env)->ReleaseStringUTFChars(env, format_string, format_ptr);

            int width           = GET_INTEGER(format, "width");
            int height          = GET_INTEGER(format, "height");
            p_sys->stride       = GET_INTEGER(format, "stride");
            p_sys->slice_height = GET_INTEGER(format, "slice-height");
            p_sys->pixel_format = GET_INTEGER(format, "color-format");
            p_sys->crop_left    = GET_INTEGER(format, "crop-left");
            p_sys->crop_top     = GET_INTEGER(format, "crop-top");
            int crop_right      = GET_INTEGER(format, "crop-right");
            int crop_bottom     = GET_INTEGER(format, "crop-bottom");

            const char *name = "unknown";
            GetVlcChromaFormat(p_sys->pixel_format, &p_dec->fmt_out.i_codec, &name);
            msg_Dbg(p_dec, "output: %d %s, %dx%d stride %d %d, crop %d %d %d %d",
                    p_sys->pixel_format, name, width, height, p_sys->stride, p_sys->slice_height,
                    p_sys->crop_left, p_sys->crop_top, crop_right, crop_bottom);

            p_dec->fmt_out.video.i_width = crop_right + 1 - p_sys->crop_left;
            p_dec->fmt_out.video.i_height = crop_bottom + 1 - p_sys->crop_top;
            if (p_sys->stride <= 0)
                p_sys->stride = width;
            if (p_sys->slice_height <= 0)
                p_sys->slice_height = height;
            if ((*env)->ExceptionOccurred(env))
                (*env)->ExceptionClear(env);
            if (p_sys->pixel_format == OMX_TI_COLOR_FormatYUV420PackedSemiPlanar) {
                p_sys->slice_height -= p_sys->crop_top/2;
                /* Reset crop top/left here, since the offset parameter already includes this.
                 * If we'd ignore the offset parameter in the BufferInfo, we could just keep
                 * the original slice height and apply the top/left cropping instead. */
                p_sys->crop_top = 0;
                p_sys->crop_left = 0;
            }
            if (IgnoreOmxDecoderPadding(p_sys->name)) {
                p_sys->slice_height = 0;
                p_sys->stride = p_dec->fmt_out.video.i_width;
            }

            continue;
        } else {
            break;
        }
    } while (loop);
}

static picture_t *DecodeVideo(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic = NULL;
    JNIEnv *env = NULL;
    struct H264ConvertState convert_state = { 0, 0 };

    if (!pp_block || !*pp_block)
        return NULL;

    block_t *p_block = *pp_block;

    (*myVm)->AttachCurrentThread(myVm, &env, NULL);

    if (p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED)) {
        block_Release(p_block);
        if (p_sys->decoded) {
            (*env)->CallVoidMethod(env, p_sys->codec, p_sys->flush);
            if ((*env)->ExceptionOccurred(env)) {
                msg_Warn(p_dec, "Exception occurred in MediaCodec.flush");
                (*env)->ExceptionClear(env);
            }
        }
        p_sys->decoded = 0;
        (*myVm)->DetachCurrentThread(myVm);
        return NULL;
    }

    jlong timeout = 0;
    while (true) {
        int index = (*env)->CallIntMethod(env, p_sys->codec, p_sys->dequeue_input_buffer, timeout);
        if (index < 0) {
            GetOutput(p_dec, env, &p_pic, timeout > 0);
            timeout = 30;
            continue;
        }
        jobject buf = (*env)->GetObjectArrayElement(env, p_sys->input_buffers, index);
        jsize size = (*env)->GetDirectBufferCapacity(env, buf);
        uint8_t *bufptr = (*env)->GetDirectBufferAddress(env, buf);
        if (size > p_block->i_buffer)
            size = p_block->i_buffer;
        memcpy(bufptr, p_block->p_buffer, size);

        convert_h264_to_annexb(bufptr, size, p_sys->nal_size, &convert_state);

        int64_t ts = p_block->i_pts;
        if (!ts && p_block->i_dts)
            ts = p_block->i_dts;
        (*env)->CallVoidMethod(env, p_sys->codec, p_sys->queue_input_buffer, index, 0, size, ts, 0);
        (*env)->DeleteLocalRef(env, buf);
        p_sys->decoded = 1;
        break;
    }
    if (!p_pic)
        GetOutput(p_dec, env, &p_pic, 0);
    (*myVm)->DetachCurrentThread(myVm);

    block_Release(p_block);
    *pp_block = NULL;

    return p_pic;
}
