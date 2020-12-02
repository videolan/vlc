/*****************************************************************************
 * android/device.h: Android AudioTrack/AAudio device handler
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

#include <jni.h>

typedef struct aout_stream aout_stream_t;

enum android_audio_device_type
{
    ANDROID_AUDIO_DEVICE_STEREO = 0,
    ANDROID_AUDIO_DEVICE_PCM,
    ANDROID_AUDIO_DEVICE_ENCODED,
};
#define ANDROID_AUDIO_DEVICE_MAX_CHANNELS 8

struct DynamicsProcessing_fields
{
    jclass clazz;
    jmethodID ctor;
    jmethodID setInputGainAllChannelsTo;
    jmethodID setEnabled;
};

int
AudioTrack_InitJNI(audio_output_t *aout,
                   struct DynamicsProcessing_fields *dp_fields);

bool
AudioTrack_HasEncoding(long long encoding_flags, vlc_fourcc_t format);

jobject
DynamicsProcessing_New(aout_stream_t *stream, int32_t session_id);

int
DynamicsProcessing_SetVolume(aout_stream_t *stream, jobject dp, float volume);

void
DynamicsProcessing_Disable(aout_stream_t *stream, jobject dp);

void
DynamicsProcessing_Delete(aout_stream_t *stream, jobject dp);

struct aout_stream
{
    struct vlc_object_t obj;
    void *sys;

    void (*stop)(aout_stream_t *);
    int (*time_get)(aout_stream_t *, vlc_tick_t *);
    void (*play)(aout_stream_t *, block_t *, vlc_tick_t);
    void (*pause)(aout_stream_t *, bool, vlc_tick_t);
    void (*flush)(aout_stream_t *);
    void (*volume_set)(aout_stream_t *, float volume);
    void (*mute_set)(aout_stream_t *, bool mute);

    audio_output_t *aout;
};

static inline void
aout_stream_GainRequest(aout_stream_t *s, float gain)
{
    aout_GainRequest(s->aout, gain);
}

typedef int (*aout_stream_start)(aout_stream_t *s, audio_sample_format_t *fmt,
                                 enum android_audio_device_type dev);
