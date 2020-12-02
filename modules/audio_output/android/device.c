/*****************************************************************************
 * android/device.c: Android AudioTrack/AAudio device handler
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
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_aout.h>
#include "device.h"
#include "../video_output/android/utils.h"

/* There is an undefined behavior when configuring AudioTrack with SPDIF or
 * more than 2 channels when there is no HDMI out. It may succeed and the
 * Android ressampler will be used to downmix to stereo. It may fails cleanly,
 * and this module will be able to recover and fallback to stereo. Finally, in
 * some rare cases, it may crash during init or while ressampling. Because of
 * the last case we don't try up to 8 channels and we use
 * ANDROID_AUDIO_DEVICE_STEREO device per default */
#define ANDROID_AUDIO_DEVICE_DEFAULT ANDROID_AUDIO_DEVICE_STEREO

static const struct {
    const char *id;
    const char *name;
    enum android_audio_device_type adev;
} adevs[] = {
    { "stereo", "Up to 2 channels (compat mode).", ANDROID_AUDIO_DEVICE_STEREO },
    { "pcm", "Up to 8 channels.", ANDROID_AUDIO_DEVICE_PCM },

    /* With "encoded", the module will try to play every audio codecs via
     * passthrough.
     *
     * With "encoded:ENCODING_FLAGS_MASK", the module will try to play only
     * codecs specified by ENCODING_FLAGS_MASK. This extra value is a long long
     * that contains binary-shifted AudioFormat.ENCODING_* values. */
    { "encoded", "Up to 8 channels, passthrough if available.", ANDROID_AUDIO_DEVICE_ENCODED },
    {  NULL, NULL, ANDROID_AUDIO_DEVICE_DEFAULT },
};

static struct DynamicsProcessing_fields dp_fields;

struct sys {
    aout_stream_t *stream;

    enum android_audio_device_type adev;
    long long encoding_flags;

    bool mute;
    float volume;
};

static int
Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    struct sys *sys = aout->sys;

    if (!AudioTrack_HasEncoding(sys->encoding_flags, fmt->i_format))
        return VLC_EGENERIC;

    aout_stream_t *s = vlc_object_create(aout, sizeof (*s));
    if (unlikely(s == NULL))
        return VLC_EGENERIC;
    s->aout = aout;

    module_t **mods;
    ssize_t total = vlc_module_match("aout android stream", NULL, false, &mods, NULL);
    int ret = VLC_EGENERIC;
    for (ssize_t i = 0; i < total; i++)
    {
        aout_stream_start start = vlc_module_map(vlc_object_logger(aout), mods[i]);
        if (start == NULL)
            continue;
        ret = start(s, fmt, sys->adev);
        if (ret == VLC_SUCCESS)
        {
            sys->stream = s;

            assert(s->stop != NULL && s->time_get != NULL && s->play != NULL &&
                   s->pause != NULL && s->flush != NULL);

            if (s->volume_set != NULL)
                s->volume_set(s, sys->volume);
            if (s->mute_set != NULL && sys->mute)
                s->mute_set(s, true);
            break;
        }
    }

    free(mods);

    return ret;
}

static void
Stop(audio_output_t *aout)
{
    struct sys *sys = aout->sys;
    assert(sys->stream != NULL);

    sys->stream->stop(sys->stream);

    vlc_object_delete(sys->stream);
    sys->stream = NULL;
}

static int
TimeGet(audio_output_t *aout, vlc_tick_t *restrict delay)
{
    struct sys *sys = aout->sys;
    assert(sys->stream != NULL);

    return sys->stream->time_get(sys->stream, delay);
}

static void
Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    struct sys *sys = aout->sys;
    assert(sys->stream != NULL);

    sys->stream->play(sys->stream, block, date);
}

static void
Pause(audio_output_t *aout, bool paused, vlc_tick_t date)
{
    struct sys *sys = aout->sys;
    assert(sys->stream != NULL);

    sys->stream->pause(sys->stream, paused, date);
}

static void
Flush(audio_output_t *aout)
{
    struct sys *sys = aout->sys;
    assert(sys->stream != NULL);

    sys->stream->flush(sys->stream);
}

static int
VolumeSet(audio_output_t *aout, float vol)
{
    struct sys *sys = aout->sys;

    sys->volume = vol;
    if (sys->stream != NULL && sys->stream->volume_set != NULL)
        sys->stream->volume_set(sys->stream, vol);

    aout_VolumeReport(aout, vol);
    return 0;
}

static int
MuteSet(audio_output_t *aout, bool mute)
{
    struct sys *sys = aout->sys;

    sys->mute = mute;
    if (sys->stream != NULL && sys->stream->mute_set != NULL)
        sys->stream->mute_set(sys->stream, mute);

    aout_MuteReport(aout, mute);
    return 0;
}

static int DeviceSelect(audio_output_t *aout, const char *id)
{
    struct sys *sys = aout->sys;
    enum android_audio_device_type adev = ANDROID_AUDIO_DEVICE_DEFAULT;

    if (id)
    {
        for (unsigned int i = 0; adevs[i].id; ++i)
        {
            if (strncmp(id, adevs[i].id, strlen(adevs[i].id))== 0)
            {
                adev = adevs[i].adev;
                break;
            }
        }
    }

    long long encoding_flags = 0;
    if (adev == ANDROID_AUDIO_DEVICE_ENCODED)
    {
        const size_t prefix_size = strlen("encoded:");
        if (strncmp(id, "encoded:", prefix_size)== 0)
            encoding_flags = atoll(id + prefix_size);
    }

    if (adev != sys->adev || encoding_flags != sys->encoding_flags)
    {
        sys->adev = adev;
        sys->encoding_flags = encoding_flags;
        aout_RestartRequest(aout, AOUT_RESTART_OUTPUT);
        msg_Dbg(aout, "selected device: %s", id);

        if (adev == ANDROID_AUDIO_DEVICE_ENCODED)
        {
            static const vlc_fourcc_t enc_fourccs[] = {
                VLC_CODEC_DTS, VLC_CODEC_DTSHD, VLC_CODEC_A52, VLC_CODEC_EAC3,
                VLC_CODEC_TRUEHD,
            };
            for (size_t i = 0;
                 i < sizeof(enc_fourccs)/ sizeof(enc_fourccs[0]); ++i)
            {
                if (AudioTrack_HasEncoding(sys->encoding_flags, enc_fourccs[i]))
                    msg_Dbg(aout, "device has %4.4s passthrough support",
                             (const char *)&enc_fourccs[i]);
            }
        }
    }
    aout_DeviceReport(aout, id);
    return VLC_SUCCESS;
}

static int
Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    int ret = AudioTrack_InitJNI(aout, &dp_fields);
    if (ret != VLC_SUCCESS)
        return ret;

    struct sys *sys = aout->sys = vlc_obj_malloc(obj, sizeof(*sys));
    if (sys == NULL)
        return VLC_ENOMEM;

    sys->adev = ANDROID_AUDIO_DEVICE_DEFAULT;
    sys->encoding_flags = 0;
    sys->volume = 1.f;
    sys->mute = false;

    aout->start = Start;
    aout->stop = Stop;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->time_get = TimeGet;
    aout->device_select = DeviceSelect;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;

    for (unsigned int i = 0; adevs[i].id; ++i)
        aout_HotplugReport(aout, adevs[i].id, adevs[i].name);

    if (var_InheritBool(aout, "spdif"))
        DeviceSelect(aout, "encoded");

    return VLC_SUCCESS;
}

#define THREAD_NAME "android_audio"
#define GET_ENV() android_getEnv( VLC_OBJECT(stream), THREAD_NAME )
#define JNI_CALL( what, obj, method, ... ) (*env)->what( env, obj, method, ##__VA_ARGS__ )
#define JNI_CALL_INT( obj, method, ... ) JNI_CALL( CallIntMethod, obj, method, ##__VA_ARGS__ )
#define JNI_CALL_VOID( obj, method, ... ) JNI_CALL( CallVoidMethod, obj, method, ##__VA_ARGS__ )
static inline bool
check_exception( JNIEnv *env, aout_stream_t *stream,
                 const char *class, const char *method )
{
    if( (*env)->ExceptionCheck( env ) )
    {
        (*env)->ExceptionDescribe( env );
        (*env)->ExceptionClear( env );
        msg_Err( stream, "%s.%s triggered an exception !", class, method );
        return true;
    } else
        return false;
}
#define CHECK_EXCEPTION( class, method ) check_exception( env, stream, class, method )

jobject
DynamicsProcessing_New( aout_stream_t *stream, int session_id )
{
    JNIEnv *env;
    if( !( env = GET_ENV() ) )
        return NULL;

    if( !dp_fields.clazz )
        return NULL;

    jobject dp = JNI_CALL( NewObject, dp_fields.clazz,
                           dp_fields.ctor, session_id );

    if( CHECK_EXCEPTION( "DynamicsProcessing", "ctor" ) )
        return NULL;

    jobject global_dp = (*env)->NewGlobalRef( env, dp );
    (*env)->DeleteLocalRef( env, dp );

    return global_dp;
}

void
DynamicsProcessing_Disable( aout_stream_t *stream, jobject dp )
{
    JNIEnv *env;
    if( !( env = GET_ENV() ) )
        return;

    JNI_CALL_INT( dp, dp_fields.setEnabled, false );
    CHECK_EXCEPTION( "DynamicsProcessing", "setEnabled" );
}

int
DynamicsProcessing_SetVolume( aout_stream_t *stream, jobject dp, float volume )
{
    JNIEnv *env;
    if( !( env = GET_ENV() ) )
        return VLC_EGENERIC;

    /* convert linear gain to dB */
    float dB = volume == 0.0f ? -144 : 20.0f * log10f( volume );

    JNI_CALL_VOID( dp, dp_fields.setInputGainAllChannelsTo, dB );
    int ret = JNI_CALL_INT( dp, dp_fields.setEnabled, volume != 1.0f );

    if( CHECK_EXCEPTION( "DynamicsProcessing", "setEnabled" ) || ret != 0 )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

void
DynamicsProcessing_Delete( aout_stream_t *stream, jobject dp )
{
    JNIEnv *env;
    if( !( env = GET_ENV() ) )
        return;

    JNI_CALL_INT( dp, dp_fields.setEnabled, false );
    CHECK_EXCEPTION( "DynamicsProcessing", "setEnabled" );

    (*env)->DeleteGlobalRef( env, dp );
}

#define AUDIOTRACK_SESSION_ID_TEXT " Id of audio session the AudioTrack must be attached to"

vlc_module_begin ()
    set_shortname("Android Audio")
    set_description("Android audio output")
    set_capability("audio output", 200)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_integer("audiotrack-session-id", 0,
            AUDIOTRACK_SESSION_ID_TEXT, NULL )
        change_private()
    set_callback(Open)
vlc_module_end ()
