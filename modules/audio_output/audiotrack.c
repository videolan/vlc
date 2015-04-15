/*****************************************************************************
 * audiotrack.c: Android Java AudioTrack audio output module
 *****************************************************************************
 * Copyright © 2012-2015 VLC authors and VideoLAN, VideoLabs
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
 *          Ming Hu <tewilove@gmail.com>
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

#include <assert.h>
#include <jni.h>
#include <dlfcn.h>
#include <stdbool.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

#define MIN_AUDIOTRACK_BUFFER_US INT64_C(250000)  // 250ms
#define MAX_AUDIOTRACK_BUFFER_US INT64_C(1000000) // 1000ms

#define SMOOTHPOS_SAMPLE_COUNT 10
#define SMOOTHPOS_INTERVAL_US INT64_C(30000) // 30ms

#define AUDIOTIMESTAMP_INTERVAL_US INT64_C(500000) // 500ms

static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );
static void Stop( audio_output_t * );
static int Start( audio_output_t *, audio_sample_format_t * );

struct aout_sys_t {
    /* sw gain */
    float soft_gain;
    bool soft_mute;

    jobject p_audiotrack; /* AudioTrack ref */
    jbyteArray p_bytearray; /* ByteArray ref (for Write) */
    size_t i_bytearray_size; /* size of the ByteArray */
    jfloatArray p_floatarray; /* FloatArray ref (for WriteFloat) */
    size_t i_floatarray_size; /* size of the FloatArray */
    jobject p_bytebuffer; /* ByteBuffer ref (for WriteV21) */
    audio_sample_format_t fmt; /* fmt setup by Start */

    /* Used by AudioTrack_GetPosition / AudioTrack_getPlaybackHeadPosition */
    struct {
        uint64_t i_initial;
        uint32_t i_wrap_count;
        uint32_t i_last;
    } headpos;

    /* Used by AudioTrack_GetTimestampPositionUs */
    struct {
        jobject p_obj; /* AudioTimestamp ref */
        jlong i_frame_us;
        jlong i_frame_pos;
        mtime_t i_play_time; /* time when play was called */
        mtime_t i_last_time;
    } timestamp;

    /* Used by AudioTrack_GetSmoothPositionUs */
    struct {
        uint32_t i_idx;
        uint32_t i_count;
        mtime_t p_us[SMOOTHPOS_SAMPLE_COUNT];
        mtime_t i_us;
        mtime_t i_last_time;
        mtime_t i_latency_us;
    } smoothpos;

    uint64_t i_samples_written; /* number of samples written since last flush */
    uint32_t i_bytes_per_frame; /* byte per frame */
    uint32_t i_max_audiotrack_samples;
    bool b_audiotrack_exception; /* true if audiotrack throwed an exception */
    bool b_error; /* generic error */
    bool b_spdif;
    uint8_t i_chans_to_reorder; /* do we need channel reordering */
    uint8_t p_chan_table[AOUT_CHAN_MAX];
    enum {
        WRITE,
        WRITE_V21,
        WRITE_FLOAT
    } i_write_type;
};

/* Soft volume helper */
#include "audio_output/volume.h"

// Don't use Float for now since 5.1/7.1 Float is down sampled to Stereo Float
//#define AUDIOTRACK_USE_FLOAT
// TODO: activate getTimestamp for new android versions
//#define AUDIOTRACK_USE_TIMESTAMP

#define AUDIO_CHAN_TEXT N_("Audio output channels")
#define AUDIO_CHAN_LONGTEXT N_("Channels available for audio output. " \
    "If the input has more channels than the output, it will be down-mixed. " \
    "This parameter is ignored when digital pass-through is active.")

vlc_module_begin ()
    set_shortname( "AudioTrack" )
    set_description( N_( "Android AudioTrack audio output" ) )
    set_capability( "audio output", 180 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_sw_gain()
    add_shortcut( "audiotrack" )
    add_integer( "audiotrack-audio-channels", 2,
                 AUDIO_CHAN_TEXT, AUDIO_CHAN_LONGTEXT, true)
    set_callbacks( Open, Close )
vlc_module_end ()

#define THREAD_NAME "android_audiotrack"
extern JNIEnv *jni_get_env(const char *name);

static struct
{
    struct {
        jclass clazz;
        jmethodID ctor;
        jmethodID release;
        jmethodID getState;
        jmethodID play;
        jmethodID stop;
        jmethodID flush;
        jmethodID pause;
        jmethodID write;
        jmethodID writeV21;
        jmethodID writeFloat;
        jmethodID getPlaybackHeadPosition;
        jmethodID getTimestamp;
        jmethodID getMinBufferSize;
        jmethodID getNativeOutputSampleRate;
        jint STATE_INITIALIZED;
        jint MODE_STREAM;
        jint ERROR;
        jint ERROR_BAD_VALUE;
        jint ERROR_INVALID_OPERATION;
        jint WRITE_NON_BLOCKING;
    } AudioTrack;
    struct {
        jint ENCODING_PCM_8BIT;
        jint ENCODING_PCM_16BIT;
        jint ENCODING_PCM_FLOAT;
        bool has_ENCODING_PCM_FLOAT;
        jint ENCODING_AC3;
        jint ENCODING_E_AC3;
        bool has_ENCODING_AC3;
        jint CHANNEL_OUT_MONO;
        jint CHANNEL_OUT_STEREO;
        jint CHANNEL_OUT_FRONT_LEFT;
        jint CHANNEL_OUT_FRONT_RIGHT;
        jint CHANNEL_OUT_BACK_LEFT;
        jint CHANNEL_OUT_BACK_RIGHT;
        jint CHANNEL_OUT_FRONT_CENTER;
        jint CHANNEL_OUT_LOW_FREQUENCY;
        jint CHANNEL_OUT_BACK_CENTER;
        jint CHANNEL_OUT_5POINT1;
        jint CHANNEL_OUT_SIDE_LEFT;
        jint CHANNEL_OUT_SIDE_RIGHT;
        bool has_CHANNEL_OUT_SIDE;
    } AudioFormat;
    struct {
        jint ERROR_DEAD_OBJECT;
        bool has_ERROR_DEAD_OBJECT;
        jint STREAM_MUSIC;
    } AudioManager;
    struct {
        jclass clazz;
        jmethodID getOutputLatency;
    } AudioSystem;
    struct {
        jclass clazz;
        jmethodID ctor;
        jfieldID framePosition;
        jfieldID nanoTime;
    } AudioTimestamp;
} jfields;

/* init all jni fields.
 * Done only one time during the first initialisation */
static bool
InitJNIFields( audio_output_t *p_aout )
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static int i_init_state = -1;
    bool ret;
    jclass clazz;
    jfieldID field;
    JNIEnv* env = NULL;

    vlc_mutex_lock( &lock );

    if( i_init_state != -1 )
        goto end;

    if (!(env = jni_get_env(THREAD_NAME)))
    {
        i_init_state = 0;
        goto end;
    }

#define CHECK_EXCEPTION( what, critical ) do { \
    if( (*env)->ExceptionOccurred( env ) ) \
    { \
        msg_Err( p_aout, "%s failed", what ); \
        (*env)->ExceptionClear( env ); \
        if( (critical) ) \
        { \
            i_init_state = 0; \
            goto end; \
        } \
    } \
} while( 0 )
#define GET_CLASS( str, critical ) do { \
    clazz = (*env)->FindClass( env, (str) ); \
    CHECK_EXCEPTION( str, critical ); \
} while( 0 )
#define GET_ID( get, id, str, args, critical ) do { \
    jfields.id = (*env)->get( env, clazz, (str), (args) ); \
    CHECK_EXCEPTION( #get, critical ); \
} while( 0 )
#define GET_CONST_INT( id, str, critical ) do { \
    field = NULL; \
    field = (*env)->GetStaticFieldID( env, clazz, (str), "I" ); \
    CHECK_EXCEPTION( #id, critical ); \
    if( field ) \
    { \
        jfields.id = (*env)->GetStaticIntField( env, clazz, field ); \
        CHECK_EXCEPTION( #id, critical ); \
    } \
} while( 0 )

    /* AudioTrack class init */
    GET_CLASS( "android/media/AudioTrack", true );
    jfields.AudioTrack.clazz = (jclass) (*env)->NewGlobalRef( env, clazz );
    CHECK_EXCEPTION( "NewGlobalRef", true );

    GET_ID( GetMethodID, AudioTrack.ctor, "<init>", "(IIIIII)V", true );
    GET_ID( GetMethodID, AudioTrack.release, "release", "()V", true );
    GET_ID( GetMethodID, AudioTrack.getState, "getState", "()I", true );
    GET_ID( GetMethodID, AudioTrack.play, "play", "()V", true );
    GET_ID( GetMethodID, AudioTrack.stop, "stop", "()V", true );
    GET_ID( GetMethodID, AudioTrack.flush, "flush", "()V", true );
    GET_ID( GetMethodID, AudioTrack.pause, "pause", "()V", true );

    GET_ID( GetMethodID, AudioTrack.writeV21, "write", "(Ljava/nio/ByteBuffer;II)I", false );
    if( jfields.AudioTrack.writeV21 )
    {
        GET_CONST_INT( AudioTrack.WRITE_NON_BLOCKING, "WRITE_NON_BLOCKING", true );
#ifdef AUDIOTRACK_USE_FLOAT
        GET_ID( GetMethodID, AudioTrack.writeFloat, "write", "([FIII)I", true );
#endif
    } else
        GET_ID( GetMethodID, AudioTrack.write, "write", "([BII)I", true );

    GET_ID( GetMethodID, AudioTrack.getTimestamp,
            "getTimestamp", "(Landroid/media/AudioTimestamp;)Z", false );
    GET_ID( GetMethodID, AudioTrack.getPlaybackHeadPosition,
            "getPlaybackHeadPosition", "()I", true );

    GET_ID( GetStaticMethodID, AudioTrack.getMinBufferSize, "getMinBufferSize",
            "(III)I", true );
    GET_ID( GetStaticMethodID, AudioTrack.getNativeOutputSampleRate,
            "getNativeOutputSampleRate",  "(I)I", true );
    GET_CONST_INT( AudioTrack.STATE_INITIALIZED, "STATE_INITIALIZED", true );
    GET_CONST_INT( AudioTrack.MODE_STREAM, "MODE_STREAM", true );
    GET_CONST_INT( AudioTrack.ERROR, "ERROR", true );
    GET_CONST_INT( AudioTrack.ERROR_BAD_VALUE , "ERROR_BAD_VALUE", true );
    GET_CONST_INT( AudioTrack.ERROR_INVALID_OPERATION,
                   "ERROR_INVALID_OPERATION", true );

    /* AudioTimestamp class init (if any) */
    if( jfields.AudioTrack.getTimestamp )
    {
        GET_CLASS( "android/media/AudioTimestamp", true );
        jfields.AudioTimestamp.clazz = (jclass) (*env)->NewGlobalRef( env,
                                                                      clazz );
        CHECK_EXCEPTION( "NewGlobalRef", true );

        GET_ID( GetMethodID, AudioTimestamp.ctor, "<init>", "()V", true );
        GET_ID( GetFieldID, AudioTimestamp.framePosition,
                "framePosition", "J", true );
        GET_ID( GetFieldID, AudioTimestamp.nanoTime,
                "nanoTime", "J", true );
    }

    /* AudioSystem class init */
    GET_CLASS( "android/media/AudioSystem", false );
    if( clazz )
    {
        jfields.AudioSystem.clazz = (jclass) (*env)->NewGlobalRef( env, clazz );
        GET_ID( GetStaticMethodID, AudioSystem.getOutputLatency,
                "getOutputLatency", "(I)I", false );
    }

    /* AudioFormat class init */
    GET_CLASS( "android/media/AudioFormat", true );
    GET_CONST_INT( AudioFormat.ENCODING_PCM_8BIT, "ENCODING_PCM_8BIT", true );
    GET_CONST_INT( AudioFormat.ENCODING_PCM_16BIT, "ENCODING_PCM_16BIT", true );
#ifdef AUDIOTRACK_USE_FLOAT
    GET_CONST_INT( AudioFormat.ENCODING_PCM_FLOAT, "ENCODING_PCM_FLOAT",
                   false );
    jfields.AudioFormat.has_ENCODING_PCM_FLOAT = field != NULL &&
                                                 jfields.AudioTrack.writeFloat;
#else
    jfields.AudioFormat.has_ENCODING_PCM_FLOAT = false;
#endif
    GET_CONST_INT( AudioFormat.ENCODING_AC3, "ENCODING_AC3", false );
    if( field != NULL )
    {
        GET_CONST_INT( AudioFormat.ENCODING_E_AC3, "ENCODING_E_AC3", false );
        jfields.AudioFormat.has_ENCODING_AC3 = field != NULL;
    } else
        jfields.AudioFormat.has_ENCODING_AC3 = false;

    GET_CONST_INT( AudioFormat.CHANNEL_OUT_MONO, "CHANNEL_OUT_MONO", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_STEREO, "CHANNEL_OUT_STEREO", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_FRONT_LEFT, "CHANNEL_OUT_FRONT_LEFT", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_FRONT_RIGHT, "CHANNEL_OUT_FRONT_RIGHT", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_5POINT1, "CHANNEL_OUT_5POINT1", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_BACK_LEFT, "CHANNEL_OUT_BACK_LEFT", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_BACK_RIGHT, "CHANNEL_OUT_BACK_RIGHT", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_FRONT_CENTER, "CHANNEL_OUT_FRONT_CENTER", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_LOW_FREQUENCY, "CHANNEL_OUT_LOW_FREQUENCY", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_BACK_CENTER, "CHANNEL_OUT_BACK_CENTER", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_SIDE_LEFT, "CHANNEL_OUT_SIDE_LEFT", false );
    if( field != NULL )
    {
        GET_CONST_INT( AudioFormat.CHANNEL_OUT_SIDE_RIGHT, "CHANNEL_OUT_SIDE_RIGHT", true );
        jfields.AudioFormat.has_CHANNEL_OUT_SIDE = true;
    } else
        jfields.AudioFormat.has_CHANNEL_OUT_SIDE = false;

    /* AudioManager class init */
    GET_CLASS( "android/media/AudioManager", true );
    GET_CONST_INT( AudioManager.ERROR_DEAD_OBJECT, "ERROR_DEAD_OBJECT", false );
    jfields.AudioManager.has_ERROR_DEAD_OBJECT = field != NULL;
    GET_CONST_INT( AudioManager.STREAM_MUSIC, "STREAM_MUSIC", true );

#undef CHECK_EXCEPTION
#undef GET_CLASS
#undef GET_ID
#undef GET_CONST_INT

    i_init_state = 1;
end:
    ret = i_init_state == 1;
    if( !ret )
        msg_Err( p_aout, "AudioTrack jni init failed" );
    vlc_mutex_unlock( &lock );
    return ret;
}

static inline bool
check_exception( JNIEnv *env, audio_output_t *p_aout,
                 const char *method )
{
    if( (*env)->ExceptionOccurred( env ) )
    {
        aout_sys_t *p_sys = p_aout->sys;

        p_sys->b_audiotrack_exception = true;
        p_sys->b_error = true;
        (*env)->ExceptionClear( env );
        msg_Err( p_aout, "AudioTrack.%s triggered an exception !", method );
        return true;
    } else
        return false;
}
#define CHECK_AT_EXCEPTION( method ) check_exception( env, p_aout, method )

#define JNI_CALL( what, obj, method, ... ) (*env)->what( env, obj, method, ##__VA_ARGS__ )

#define JNI_CALL_INT( obj, method, ... ) JNI_CALL( CallIntMethod, obj, method, ##__VA_ARGS__ )
#define JNI_CALL_BOOL( obj, method, ... ) JNI_CALL( CallBooleanMethod, obj, method, ##__VA_ARGS__ )
#define JNI_CALL_VOID( obj, method, ... ) JNI_CALL( CallVoidMethod, obj, method, ##__VA_ARGS__ )
#define JNI_CALL_STATIC_INT( clazz, method, ... ) JNI_CALL( CallStaticIntMethod, clazz, method, ##__VA_ARGS__ )

#define JNI_AT_NEW( ... ) JNI_CALL( NewObject, jfields.AudioTrack.clazz, jfields.AudioTrack.ctor, ##__VA_ARGS__ )
#define JNI_AT_CALL_INT( method, ... ) JNI_CALL_INT( p_sys->p_audiotrack, jfields.AudioTrack.method, ##__VA_ARGS__ )
#define JNI_AT_CALL_BOOL( method, ... ) JNI_CALL_BOOL( p_sys->p_audiotrack, jfields.AudioTrack.method, ##__VA_ARGS__ )
#define JNI_AT_CALL_VOID( method, ... ) JNI_CALL_VOID( p_sys->p_audiotrack, jfields.AudioTrack.method, ##__VA_ARGS__ )
#define JNI_AT_CALL_STATIC_INT( method, ... ) JNI_CALL( CallStaticIntMethod, jfields.AudioTrack.clazz, jfields.AudioTrack.method, ##__VA_ARGS__ )

#define JNI_AUDIOTIMESTAMP_GET_LONG( field ) JNI_CALL( GetLongField, p_sys->timestamp.p_obj, jfields.AudioTimestamp.field )

static inline mtime_t
frames_to_us( aout_sys_t *p_sys, uint64_t i_nb_frames )
{
    return  i_nb_frames * CLOCK_FREQ / p_sys->fmt.i_rate;
}
#define FRAMES_TO_US(x) frames_to_us( p_sys, (x) )

static inline uint64_t
bytes_to_frames( aout_sys_t *p_sys, size_t i_bytes )
{
    if( p_sys->b_spdif )
        return i_bytes * A52_FRAME_NB / p_sys->i_bytes_per_frame;
    else
        return i_bytes / p_sys->i_bytes_per_frame;
}
#define BYTES_TO_FRAMES(x) bytes_to_frames( p_sys, (x) )

static inline size_t
frames_to_bytes( aout_sys_t *p_sys, uint64_t i_frames )
{
    if( p_sys->b_spdif )
        return i_frames * p_sys->i_bytes_per_frame / A52_FRAME_NB;
    else
        return i_frames * p_sys->i_bytes_per_frame;
}
#define FRAMES_TO_BYTES(x) frames_to_bytes( p_sys, (x) )

/**
 * Get the AudioTrack position
 *
 * The doc says that the position is reset to zero after flush but it's not
 * true for all devices or Android versions. Use AudioTrack_GetPosition instead.
 */
static uint64_t
AudioTrack_getPlaybackHeadPosition( JNIEnv *env, audio_output_t *p_aout )
{
    /* Android doc:
     * getPlaybackHeadPosition: Returns the playback head position expressed in
     * frames. Though the "int" type is signed 32-bits, the value should be
     * reinterpreted as if it is unsigned 32-bits. That is, the next position
     * after 0x7FFFFFFF is (int) 0x80000000. This is a continuously advancing
     * counter. It will wrap (overflow) periodically, for example approximately
     * once every 27:03:11 hours:minutes:seconds at 44.1 kHz. It is reset to
     * zero by flush(), reload(), and stop().
     */

    aout_sys_t *p_sys = p_aout->sys;
    uint32_t i_pos;

    /* int32_t to uint32_t */
    i_pos = 0xFFFFFFFFL & JNI_AT_CALL_INT( getPlaybackHeadPosition );

    /* uint32_t to uint64_t */
    if( p_sys->headpos.i_last > i_pos )
        p_sys->headpos.i_wrap_count++;
    p_sys->headpos.i_last = i_pos;
    return p_sys->headpos.i_last + ((uint64_t)p_sys->headpos.i_wrap_count << 32);
}

/**
 * Get the AudioTrack position since the last flush or stop
 */
static uint64_t
AudioTrack_GetPosition( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    return AudioTrack_getPlaybackHeadPosition( env, p_aout )
        - p_sys->headpos.i_initial;
}

/**
 * Reset AudioTrack position
 *
 * Called after flush, or start
 */
static void
AudioTrack_ResetPlaybackHeadPosition( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    if( p_sys->p_audiotrack )
        p_sys->headpos.i_initial = AudioTrack_getPlaybackHeadPosition( env, p_aout );
    else
        p_sys->headpos.i_initial = 0;

    /* HACK: On some broken devices, head position is still moving after a
     * flush or a stop. So, wait for the head position to be stabilized. */
    if( unlikely( p_sys->headpos.i_initial != 0 ) )
    {
        uint64_t i_last_pos;
        do {
            i_last_pos = p_sys->headpos.i_initial;
            msleep( 50000 );
            p_sys->headpos.i_initial = JNI_AT_CALL_INT( getPlaybackHeadPosition );
        } while( p_sys->headpos.i_initial != i_last_pos );
    }
    p_sys->headpos.i_last = 0;
    p_sys->headpos.i_wrap_count = 0;
}

/**
 * Reset AudioTrack SmoothPosition and TimestampPosition
 */
static void
AudioTrack_ResetPositions( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    VLC_UNUSED( env );

    p_sys->timestamp.i_play_time = mdate();
    p_sys->timestamp.i_last_time = 0;
    p_sys->timestamp.i_frame_us = 0;
    p_sys->timestamp.i_frame_pos = 0;

    p_sys->smoothpos.i_count = 0;
    p_sys->smoothpos.i_idx = 0;
    p_sys->smoothpos.i_last_time = 0;
    p_sys->smoothpos.i_us = 0;
    p_sys->smoothpos.i_latency_us = 0;
}

/**
 * Get a smooth AudioTrack position
 *
 * This function smooth out the AudioTrack position since it has a very bad
 * precision (+/- 20ms on old devices).
 */
static mtime_t
AudioTrack_GetSmoothPositionUs( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    uint64_t i_audiotrack_us;
    mtime_t i_now = mdate();

    /* Fetch an AudioTrack position every SMOOTHPOS_INTERVAL_US (30ms) */
    if( i_now - p_sys->smoothpos.i_last_time >= SMOOTHPOS_INTERVAL_US )
    {
        i_audiotrack_us = FRAMES_TO_US( AudioTrack_GetPosition( env, p_aout ) );

        p_sys->smoothpos.i_last_time = i_now;

        /* Base the position off the current time */
        p_sys->smoothpos.p_us[p_sys->smoothpos.i_idx] = i_audiotrack_us - i_now;
        p_sys->smoothpos.i_idx = (p_sys->smoothpos.i_idx + 1)
                                 % SMOOTHPOS_SAMPLE_COUNT;
        if( p_sys->smoothpos.i_count < SMOOTHPOS_SAMPLE_COUNT )
            p_sys->smoothpos.i_count++;

        /* Calculate the average position based off the current time */
        p_sys->smoothpos.i_us = 0;
        for( uint32_t i = 0; i < p_sys->smoothpos.i_count; ++i )
            p_sys->smoothpos.i_us += p_sys->smoothpos.p_us[i];
        p_sys->smoothpos.i_us /= p_sys->smoothpos.i_count;

        if( jfields.AudioSystem.getOutputLatency )
        {
            int i_latency_ms = JNI_CALL( CallStaticIntMethod,
                                         jfields.AudioSystem.clazz,
                                         jfields.AudioSystem.getOutputLatency,
                                         jfields.AudioManager.STREAM_MUSIC );

            p_sys->smoothpos.i_latency_us = i_latency_ms > 0 ?
                                            i_latency_ms * 1000L : 0;
        }
    }
    if( p_sys->smoothpos.i_us != 0 )
        return p_sys->smoothpos.i_us + i_now - p_sys->smoothpos.i_latency_us;
    else
        return 0;
}

static mtime_t
AudioTrack_GetTimestampPositionUs( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    mtime_t i_now;

    if( !p_sys->timestamp.p_obj )
        return 0;

    i_now = mdate();

    /* Android doc:
     * getTimestamp: Poll for a timestamp on demand.
     *
     * If you need to track timestamps during initial warmup or after a
     * routing or mode change, you should request a new timestamp once per
     * second until the reported timestamps show that the audio clock is
     * stable. Thereafter, query for a new timestamp approximately once
     * every 10 seconds to once per minute. Calling this method more often
     * is inefficient. It is also counter-productive to call this method
     * more often than recommended, because the short-term differences
     * between successive timestamp reports are not meaningful. If you need
     * a high-resolution mapping between frame position and presentation
     * time, consider implementing that at application level, based on
     * low-resolution timestamps.
     */

    /* Fetch an AudioTrack timestamp every AUDIOTIMESTAMP_INTERVAL_US (500ms) */
    if( i_now - p_sys->timestamp.i_last_time >= AUDIOTIMESTAMP_INTERVAL_US )
    {
        p_sys->timestamp.i_last_time = i_now;

        if( JNI_AT_CALL_BOOL( getTimestamp, p_sys->timestamp.p_obj ) )
        {
            p_sys->timestamp.i_frame_us = JNI_AUDIOTIMESTAMP_GET_LONG( nanoTime ) / 1000;
            p_sys->timestamp.i_frame_pos = JNI_AUDIOTIMESTAMP_GET_LONG( framePosition );
        }
        else
        {
            p_sys->timestamp.i_frame_us = 0;
            p_sys->timestamp.i_frame_pos = 0;
        }
    }

    /* frame time should be after last play time
     * frame time shouldn't be in the future
     * frame time should be less than 10 seconds old */
    if( p_sys->timestamp.i_frame_us != 0 && p_sys->timestamp.i_frame_pos != 0
     && p_sys->timestamp.i_frame_us > p_sys->timestamp.i_play_time
     && i_now > p_sys->timestamp.i_frame_us
     && ( i_now - p_sys->timestamp.i_frame_us ) <= INT64_C(10000000) )
    {
        jlong i_time_diff = i_now - p_sys->timestamp.i_frame_us;
        jlong i_frames_diff = i_time_diff * p_sys->fmt.i_rate / CLOCK_FREQ;
        return FRAMES_TO_US( p_sys->timestamp.i_frame_pos + i_frames_diff );
    } else
        return 0;
}

static int
TimeGet( audio_output_t *p_aout, mtime_t *restrict p_delay )
{
    aout_sys_t *p_sys = p_aout->sys;
    mtime_t i_audiotrack_us;
    JNIEnv *env;

    if( p_sys->b_error || !( env = jni_get_env( THREAD_NAME ) ) )
        return -1;

    if( p_sys->b_spdif )
        return -1;

    i_audiotrack_us = AudioTrack_GetTimestampPositionUs( env, p_aout );

    if( i_audiotrack_us <= 0 )
        i_audiotrack_us = AudioTrack_GetSmoothPositionUs(env, p_aout );

    if( i_audiotrack_us > 0 )
    {
        mtime_t i_delay = FRAMES_TO_US( p_sys->i_samples_written )
                          - i_audiotrack_us;
        if( i_delay >= 0 )
        {
            *p_delay = i_delay;
            return 0;
        }
        else
            msg_Warn( p_aout, "Negative delay, Should not happen !" );
    }
    return -1;
}

static void
AudioTrack_GetChanOrder( uint16_t i_physical_channels, uint32_t p_chans_out[] )
{
#define HAS_CHAN( x ) ( ( i_physical_channels & (x) ) == (x) )
    /* samples will be in the following order: FL FR FC LFE BL BR BC SL SR */
    int i = 0;

    if( HAS_CHAN( AOUT_CHAN_LEFT ) )
        p_chans_out[i++] = AOUT_CHAN_LEFT;
    if( HAS_CHAN( AOUT_CHAN_RIGHT ) )
        p_chans_out[i++] = AOUT_CHAN_RIGHT;

    if( HAS_CHAN( AOUT_CHAN_CENTER ) )
        p_chans_out[i++] = AOUT_CHAN_CENTER;

    if( HAS_CHAN( AOUT_CHAN_LFE ) )
        p_chans_out[i++] = AOUT_CHAN_LFE;

    if( HAS_CHAN( AOUT_CHAN_REARLEFT ) )
        p_chans_out[i++] = AOUT_CHAN_REARLEFT;
    if( HAS_CHAN( AOUT_CHAN_REARRIGHT ) )
        p_chans_out[i++] = AOUT_CHAN_REARRIGHT;

    if( HAS_CHAN( AOUT_CHAN_REARCENTER ) )
        p_chans_out[i++] = AOUT_CHAN_REARCENTER;

    if( HAS_CHAN( AOUT_CHAN_MIDDLELEFT ) )
        p_chans_out[i++] = AOUT_CHAN_MIDDLELEFT;
    if( HAS_CHAN( AOUT_CHAN_MIDDLERIGHT ) )
        p_chans_out[i++] = AOUT_CHAN_MIDDLERIGHT;

    assert( i <= AOUT_CHAN_MAX );
#undef HAS_CHAN
}

/**
 * Configure and create an Android AudioTrack.
 * returns NULL on configuration error
 */
static jobject
AudioTrack_New( JNIEnv *env, audio_output_t *p_aout,
                unsigned int i_rate,
                vlc_fourcc_t i_vlc_format,
                uint16_t i_physical_channels,
                int i_bytes_per_frame,
                int *p_audiotrack_size )
{
    int i_size, i_min_buffer_size, i_channel_config, i_format;
    jobject p_audiotrack;

    switch( i_vlc_format )
    {
        case VLC_CODEC_U8:
            i_format = jfields.AudioFormat.ENCODING_PCM_8BIT;
            break;
        case VLC_CODEC_S16N:
            i_format = jfields.AudioFormat.ENCODING_PCM_16BIT;
            break;
        case VLC_CODEC_FL32:
            i_format = jfields.AudioFormat.ENCODING_PCM_FLOAT;
            break;
        case VLC_CODEC_SPDIFB:
            i_format = jfields.AudioFormat.ENCODING_AC3;
            break;
        default:
            vlc_assert_unreachable();
    }

    switch( i_physical_channels )
    {
        case AOUT_CHANS_7_1:
            /* bitmask of CHANNEL_OUT_7POINT1 doesn't correspond to 5POINT1 and
             * SIDES */
            i_channel_config = jfields.AudioFormat.CHANNEL_OUT_5POINT1 |
                               jfields.AudioFormat.CHANNEL_OUT_SIDE_LEFT |
                               jfields.AudioFormat.CHANNEL_OUT_SIDE_RIGHT;
            break;
        case AOUT_CHANS_5_1:
            i_channel_config = jfields.AudioFormat.CHANNEL_OUT_5POINT1;
            break;
        case AOUT_CHAN_LEFT:
            i_channel_config = jfields.AudioFormat.CHANNEL_OUT_MONO;
            break;
        default:
        case AOUT_CHANS_STEREO:
            i_channel_config = jfields.AudioFormat.CHANNEL_OUT_STEREO;
            break;
    }

    i_min_buffer_size = JNI_AT_CALL_STATIC_INT( getMinBufferSize, i_rate,
                                                i_channel_config, i_format );
    if( i_min_buffer_size <= 0 )
    {
        msg_Warn( p_aout, "getMinBufferSize returned an invalid size" ) ;
        return NULL;
    }
    if( i_vlc_format == VLC_CODEC_SPDIFB )
        i_size = ( i_min_buffer_size / AOUT_SPDIF_SIZE + 1 ) * AOUT_SPDIF_SIZE;
    else
    {
        /* Optimal buffer size: i_min_buffer_size * 4 but between 250ms and
         * 1000ms */
        mtime_t i_time, i_clipped_time;

        i_size = i_min_buffer_size * 4;
        i_time = (i_size / i_bytes_per_frame) * CLOCK_FREQ / i_rate;

        i_clipped_time = VLC_CLIP( i_time, MIN_AUDIOTRACK_BUFFER_US,
                         MAX_AUDIOTRACK_BUFFER_US );
        if( i_clipped_time != i_time )
            i_size = i_rate * i_clipped_time * i_bytes_per_frame / CLOCK_FREQ;
    }

    /* create AudioTrack object */
    p_audiotrack = JNI_AT_NEW( jfields.AudioManager.STREAM_MUSIC, i_rate,
                               i_channel_config, i_format, i_size,
                               jfields.AudioTrack.MODE_STREAM );
    if( CHECK_AT_EXCEPTION( "AudioTrack<init>" ) || !p_audiotrack )
    {
        msg_Warn( p_aout, "AudioTrack Init failed" ) ;
        return NULL;
    }
    if( JNI_CALL_INT( p_audiotrack, jfields.AudioTrack.getState )
        != jfields.AudioTrack.STATE_INITIALIZED )
    {
        JNI_CALL_VOID( p_audiotrack, jfields.AudioTrack.release );
        (*env)->DeleteLocalRef( env, p_audiotrack );
        msg_Err( p_aout, "AudioTrack getState failed" );
        return NULL;
    }
    *p_audiotrack_size = i_size;

    return p_audiotrack;
}

static int
Start( audio_output_t *p_aout, audio_sample_format_t *restrict p_fmt )
{
    aout_sys_t *p_sys = p_aout->sys;
    JNIEnv *env;
    jobject p_audiotrack = NULL;
    int i_nb_channels, i_max_channels, i_audiotrack_size, i_bytes_per_frame,
        i_native_rate;
    unsigned int i_rate;
    bool b_spdif;

    b_spdif = var_InheritBool( p_aout, "spdif" );
    i_max_channels = var_InheritInteger( p_aout, "audiotrack-audio-channels" );

    if( !( env = jni_get_env( THREAD_NAME ) ) )
        return VLC_EGENERIC;

    p_sys->fmt = *p_fmt;

    aout_FormatPrint( p_aout, "VLC is looking for:", &p_sys->fmt );

    p_sys->fmt.i_original_channels = p_sys->fmt.i_physical_channels;

    i_native_rate = JNI_AT_CALL_STATIC_INT( getNativeOutputSampleRate,
                                            jfields.AudioManager.STREAM_MUSIC );
    if( i_native_rate <= 0 )
    {
        msg_Warn( p_aout, "negative native rate ? Should not happen !" );
        i_native_rate = VLC_CLIP( p_sys->fmt.i_rate, 4000, 48000 );
    }

    /* We can only accept U8, S16N, FL32, and AC3 */
    switch( p_sys->fmt.i_format )
    {
        case VLC_CODEC_U8:
            break;
        case VLC_CODEC_S16N:
            break;
        case VLC_CODEC_FL32:
            if( !jfields.AudioFormat.has_ENCODING_PCM_FLOAT )
                p_sys->fmt.i_format = VLC_CODEC_S16N;
            break;
        case VLC_CODEC_A52:
            if( jfields.AudioFormat.has_ENCODING_AC3 && b_spdif )
                p_sys->fmt.i_format = VLC_CODEC_SPDIFB;
            else if( jfields.AudioFormat.has_ENCODING_PCM_FLOAT )
                p_sys->fmt.i_format = VLC_CODEC_FL32;
            else
                p_sys->fmt.i_format = VLC_CODEC_S16N;
            break;
        default:
            p_sys->fmt.i_format = VLC_CODEC_S16N;
            break;
    }

    /* Android AudioTrack supports only mono, stereo, 5.1 and 7.1.
     * Android will downmix to stereo if audio output doesn't handle 5.1 or 7.1
     */
    i_nb_channels = aout_FormatNbChannels( &p_sys->fmt );
    if( p_sys->fmt.i_format != VLC_CODEC_SPDIFB )
        i_nb_channels = __MIN( i_max_channels, i_nb_channels );
    if( i_nb_channels > 5 )
    {
        if( i_nb_channels > 7 && jfields.AudioFormat.has_CHANNEL_OUT_SIDE )
            p_sys->fmt.i_physical_channels = AOUT_CHANS_7_1;
        else
            p_sys->fmt.i_physical_channels = AOUT_CHANS_5_1;
    } else
    {
        if( i_nb_channels == 1 )
            p_sys->fmt.i_physical_channels = AOUT_CHAN_LEFT;
        else
            p_sys->fmt.i_physical_channels = AOUT_CHANS_STEREO;
    }
    i_nb_channels = aout_FormatNbChannels( &p_sys->fmt );

    do
    {
        i_bytes_per_frame = i_nb_channels *
                            aout_BitsPerSample( p_sys->fmt.i_format ) / 8;
        i_rate = p_sys->fmt.i_format == VLC_CODEC_SPDIFB ?
                                        VLC_CLIP( p_sys->fmt.i_rate, 32000, 48000 )
                                        : (unsigned int) i_native_rate;

        /* Try to create an AudioTrack with the most advanced channel and
         * format configuration. If AudioTrack_New fails, try again with a less
         * advanced format (PCM S16N). If it fails again, try again with Stereo
         * channels. */
        p_audiotrack = AudioTrack_New( env, p_aout, i_rate,
                                       p_sys->fmt.i_format,
                                       p_sys->fmt.i_physical_channels,
                                       i_bytes_per_frame,
                                       &i_audiotrack_size );
        if( !p_audiotrack )
        {
            if( p_sys->fmt.i_format == VLC_CODEC_SPDIFB )
            {
                msg_Warn( p_aout, "SPDIF configuration failed, "
                                  "fallback to PCM" );
                if( jfields.AudioFormat.has_ENCODING_PCM_FLOAT )
                    p_sys->fmt.i_format = VLC_CODEC_FL32;
                else
                    p_sys->fmt.i_format = VLC_CODEC_S16N;
            }
            else if( p_sys->fmt.i_format == VLC_CODEC_FL32 )
            {
                msg_Warn( p_aout, "FL32 configuration failed, "
                                  "fallback to S16N PCM" );
                p_sys->fmt.i_format = VLC_CODEC_S16N;
            }
            else if( i_nb_channels > 5 )
            {
                msg_Warn( p_aout, "5.1 or 7.1 configuration failed, "
                                  "fallback to Stereo" );
                p_sys->fmt.i_physical_channels = AOUT_CHANS_STEREO;
                i_nb_channels = aout_FormatNbChannels( &p_sys->fmt );
            }
            else
                break;
        }
    } while( !p_audiotrack );

    if( !p_audiotrack )
        return VLC_EGENERIC;

    p_sys->p_audiotrack = (*env)->NewGlobalRef( env, p_audiotrack );
    (*env)->DeleteLocalRef( env, p_audiotrack );
    if( !p_sys->p_audiotrack )
        return VLC_EGENERIC;

    p_sys->fmt.i_rate = i_rate;
    p_sys->b_spdif = p_sys->fmt.i_format == VLC_CODEC_SPDIFB;
    if( p_sys->b_spdif )
    {
        p_sys->fmt.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_sys->fmt.i_frame_length = A52_FRAME_NB;
        p_sys->i_bytes_per_frame = p_sys->fmt.i_bytes_per_frame;
    }
    else
    {
        uint32_t p_chans_out[AOUT_CHAN_MAX];

        memset( p_chans_out, 0, sizeof(p_chans_out) );
        AudioTrack_GetChanOrder( p_sys->fmt.i_physical_channels, p_chans_out );
        p_sys->i_chans_to_reorder =
            aout_CheckChannelReorder( NULL, p_chans_out,
                                      p_sys->fmt.i_physical_channels,
                                      p_sys->p_chan_table );
        p_sys->i_bytes_per_frame = i_bytes_per_frame;
    }
    p_sys->i_max_audiotrack_samples = BYTES_TO_FRAMES( i_audiotrack_size );

#ifdef AUDIOTRACK_USE_TIMESTAMP
    if( jfields.AudioTimestamp.clazz )
    {
        /* create AudioTimestamp object */
        jobject p_obj = JNI_CALL( NewObject, jfields.AudioTimestamp.clazz,
                                 jfields.AudioTimestamp.ctor );
        if( p_obj )
        {
            p_sys->timestamp.p_obj = (*env)->NewGlobalRef( env, p_obj );
            (*env)->DeleteLocalRef( env, p_obj );
        }
        if( !p_sys->timestamp.p_obj )
        {
            Stop( p_aout );
            return VLC_EGENERIC;
        }
    }
#endif

    if( p_sys->fmt.i_format == VLC_CODEC_FL32 )
    {
        msg_Dbg( p_aout, "using WRITE_FLOAT");
        p_sys->i_write_type = WRITE_FLOAT;
    }
    else if( jfields.AudioTrack.writeV21 )
    {
        msg_Dbg( p_aout, "using WRITE_V21");
        p_sys->i_write_type = WRITE_V21;
    }
    else
    {
        msg_Dbg( p_aout, "using WRITE");
        p_sys->i_write_type = WRITE;
    }

    JNI_AT_CALL_VOID( play );
    CHECK_AT_EXCEPTION( "play" );

    AudioTrack_ResetPositions( env, p_aout );
    AudioTrack_ResetPlaybackHeadPosition( env, p_aout );
    p_sys->i_samples_written = 0;
    *p_fmt = p_sys->fmt;
    aout_SoftVolumeStart( p_aout );

    aout_FormatPrint( p_aout, "VLC will output:", &p_sys->fmt );

    return VLC_SUCCESS;
}

static void
Stop( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    JNIEnv *env;

    if( !( env = jni_get_env( THREAD_NAME ) ) )
        return;

    if( p_sys->p_audiotrack )
    {
        if( !p_sys->b_audiotrack_exception )
        {
            JNI_AT_CALL_VOID( stop );
            if( !CHECK_AT_EXCEPTION( "stop" ) )
                JNI_AT_CALL_VOID( release );
        }
        (*env)->DeleteGlobalRef( env, p_sys->p_audiotrack );
        p_sys->p_audiotrack = NULL;
    }
    if( p_sys->timestamp.p_obj )
    {
        (*env)->DeleteGlobalRef( env, p_sys->timestamp.p_obj );
        p_sys->timestamp.p_obj = NULL;
    }

    p_sys->b_audiotrack_exception = false;
    p_sys->b_error = false;
}

/**
 * Non blocking write function.
 * Do a calculation between current position and audiotrack position and assure
 * that we won't wait in AudioTrack.write() method
 */
static int
AudioTrack_Write( JNIEnv *env, audio_output_t *p_aout, block_t *p_buffer,
                  size_t i_buffer_offset, bool b_force )
{
    aout_sys_t *p_sys = p_aout->sys;
    size_t i_data;
    uint64_t i_samples;
    uint64_t i_audiotrack_pos;
    uint64_t i_samples_pending;

    i_data = p_buffer->i_buffer - i_buffer_offset;
    i_audiotrack_pos = AudioTrack_GetPosition( env, p_aout );
    if( i_audiotrack_pos > p_sys->i_samples_written )
    {
        msg_Warn( p_aout, "audiotrack position is ahead. Should NOT happen" );
        AudioTrack_ResetPlaybackHeadPosition( env, p_aout );
        p_sys->i_samples_written = 0;
        return 0;
    }
    i_samples_pending = p_sys->i_samples_written - i_audiotrack_pos;

    /* check if audiotrack buffer is not full before writing on it. */
    if( b_force )
    {
        msg_Warn( p_aout, "Force write. It may block..." );
        i_samples_pending = 0;
    } else if( i_samples_pending >= p_sys->i_max_audiotrack_samples )
        return 0;

    i_samples = __MIN( p_sys->i_max_audiotrack_samples - i_samples_pending,
                       BYTES_TO_FRAMES( i_data ) );

    i_data = FRAMES_TO_BYTES( i_samples );

    return JNI_AT_CALL_INT( write, p_sys->p_bytearray,
                            i_buffer_offset, i_data );
}

/**
 * Non blocking write function for Lollipop and after.
 * It calls a new write method with WRITE_NON_BLOCKING flags.
 */
static int
AudioTrack_WriteV21( JNIEnv *env, audio_output_t *p_aout, block_t *p_buffer,
                     size_t i_buffer_offset )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_ret;
    size_t i_data = p_buffer->i_buffer - i_buffer_offset;
    uint8_t *p_data = p_buffer->p_buffer + i_buffer_offset;

    if( !p_sys->p_bytebuffer )
    {
        jobject p_bytebuffer;

        p_bytebuffer = (*env)->NewDirectByteBuffer( env, p_data, i_data );
        if( !p_bytebuffer )
            return jfields.AudioTrack.ERROR_BAD_VALUE;

        p_sys->p_bytebuffer = (*env)->NewGlobalRef( env, p_bytebuffer );
        (*env)->DeleteLocalRef( env, p_bytebuffer );

        if( !p_sys->p_bytebuffer || (*env)->ExceptionOccurred( env ) )
        {
            p_sys->p_bytebuffer = NULL;
            (*env)->ExceptionClear( env );
            return jfields.AudioTrack.ERROR_BAD_VALUE;
        }
    }

    i_ret = JNI_AT_CALL_INT( writeV21, p_sys->p_bytebuffer, i_data,
                             jfields.AudioTrack.WRITE_NON_BLOCKING );
    if( i_ret > 0 )
    {
        /* don't delete the bytebuffer if we wrote nothing, keep it for next
         * call */
        (*env)->DeleteGlobalRef( env, p_sys->p_bytebuffer );
        p_sys->p_bytebuffer = NULL;
    }
    return i_ret;
}

/**
 * Non blocking write float function for Lollipop and after.
 * It calls a new write method with WRITE_NON_BLOCKING flags.
 */
static int
AudioTrack_WriteFloat( JNIEnv *env, audio_output_t *p_aout, block_t *p_buffer,
                       size_t i_buffer_offset )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_ret;
    size_t i_data;

    i_buffer_offset /= 4;
    i_data = p_buffer->i_buffer / 4 - i_buffer_offset;

    i_ret = JNI_AT_CALL_INT( writeFloat, p_sys->p_floatarray,
                             i_buffer_offset, i_data,
                             jfields.AudioTrack.WRITE_NON_BLOCKING );
    if( i_ret < 0 )
        return i_ret;
    else
        return i_ret * 4;
}

static int
AudioTrack_PreparePlay( JNIEnv *env, audio_output_t *p_aout,
                         block_t *p_buffer )
{
    aout_sys_t *p_sys = p_aout->sys;

    if( p_sys->i_chans_to_reorder )
       aout_ChannelReorder( p_buffer->p_buffer, p_buffer->i_buffer,
                            p_sys->i_chans_to_reorder, p_sys->p_chan_table,
                            p_sys->fmt.i_format );

    switch( p_sys->i_write_type )
    {
    case WRITE:
        /* check if we need to realloc a ByteArray */
        if( p_buffer->i_buffer > p_sys->i_bytearray_size )
        {
            jbyteArray p_bytearray;

            if( p_sys->p_bytearray )
            {
                (*env)->DeleteGlobalRef( env, p_sys->p_bytearray );
                p_sys->p_bytearray = NULL;
            }

            p_bytearray = (*env)->NewByteArray( env, p_buffer->i_buffer );
            if( p_bytearray )
            {
                p_sys->p_bytearray = (*env)->NewGlobalRef( env, p_bytearray );
                (*env)->DeleteLocalRef( env, p_bytearray );
            }
            p_sys->i_bytearray_size = p_buffer->i_buffer;
        }
        if( !p_sys->p_bytearray )
            return VLC_EGENERIC;

        /* copy p_buffer in to ByteArray */
        (*env)->SetByteArrayRegion( env, p_sys->p_bytearray, 0,
                                    p_buffer->i_buffer,
                                    (jbyte *)p_buffer->p_buffer);
        break;
    case WRITE_FLOAT:
    {
        size_t i_data = p_buffer->i_buffer / 4;

        /* check if we need to realloc a floatArray */
        if( i_data > p_sys->i_floatarray_size )
        {
            jfloatArray p_floatarray;

            if( p_sys->p_floatarray )
            {
                (*env)->DeleteGlobalRef( env, p_sys->p_floatarray );
                p_sys->p_floatarray = NULL;
            }

            p_floatarray = (*env)->NewFloatArray( env, i_data );
            if( p_floatarray )
            {
                p_sys->p_floatarray = (*env)->NewGlobalRef( env, p_floatarray );
                (*env)->DeleteLocalRef( env, p_floatarray );
            }
            p_sys->i_floatarray_size = i_data;
        }
        if( !p_sys->p_floatarray )
            return VLC_EGENERIC;

        /* copy p_buffer in to FloatArray */
        (*env)->SetFloatArrayRegion( env, p_sys->p_floatarray, 0, i_data,
                                    (jfloat *)p_buffer->p_buffer);

        break;
    }
    case WRITE_V21:
        break;
    }

    return VLC_SUCCESS;
}

static int
AudioTrack_Play( JNIEnv *env, audio_output_t *p_aout,
                 block_t *p_buffer, size_t *p_buffer_offset, mtime_t *p_wait,
                 bool b_force )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_ret;

    switch( p_sys->i_write_type )
    {
    case WRITE_V21:
        i_ret = AudioTrack_WriteV21( env, p_aout, p_buffer, *p_buffer_offset );
        break;
    case WRITE:
        i_ret = AudioTrack_Write( env, p_aout, p_buffer, *p_buffer_offset,
                                  b_force );
        break;
    case WRITE_FLOAT:
        i_ret = AudioTrack_WriteFloat( env, p_aout, p_buffer, *p_buffer_offset );
        break;
    default:
        vlc_assert_unreachable();
    }

    if( i_ret < 0 ) {
        if( jfields.AudioManager.has_ERROR_DEAD_OBJECT
            && i_ret == jfields.AudioManager.ERROR_DEAD_OBJECT )
        {
            msg_Warn( p_aout, "ERROR_DEAD_OBJECT: "
                              "try recreating AudioTrack" );
            Stop( p_aout );
            i_ret = Start( p_aout, &p_sys->fmt );
        } else
        {
            const char *str;
            if( i_ret == jfields.AudioTrack.ERROR_INVALID_OPERATION )
                str = "ERROR_INVALID_OPERATION";
            else if( i_ret == jfields.AudioTrack.ERROR_BAD_VALUE )
                str = "ERROR_BAD_VALUE";
            else
                str = "ERROR";
            msg_Err( p_aout, "Write failed: %s", str );
        }
    } else if( i_ret == 0 )
    {
        /* audiotrack internal buffer is full, wait a little: between 10ms and
         * 20ms depending on devices or rate */
        *p_wait = FRAMES_TO_US( p_sys->i_max_audiotrack_samples / 20 );
    } else
    {
        uint64_t i_samples = BYTES_TO_FRAMES( i_ret );

        p_sys->i_samples_written += i_samples;

        *p_buffer_offset += i_ret;
    }
    return i_ret;
}

static void
Play( audio_output_t *p_aout, block_t *p_buffer )
{
    JNIEnv *env;
    size_t i_buffer_offset = 0;
    int i_nb_try = 0;
    mtime_t i_play_wait = 0;
    aout_sys_t *p_sys = p_aout->sys;

    if( p_sys->b_error || !( env = jni_get_env( THREAD_NAME ) ) )
        goto bailout;

    p_sys->b_error = AudioTrack_PreparePlay( env, p_aout, p_buffer )
                        != VLC_SUCCESS;

    while( i_buffer_offset < p_buffer->i_buffer && !p_sys->b_error )
    {
        int i_ret;

        if( i_play_wait != 0 )
            msleep( i_play_wait );

        i_play_wait = 0;
        i_ret = AudioTrack_Play( env, p_aout, p_buffer,
                                 &i_buffer_offset,
                                 &i_play_wait,
                                 i_nb_try > 100 );
        if( i_ret < 0 )
            p_sys->b_error = true;
        else if( p_sys->i_write_type == WRITE )
        {
            /* HACK: AudioFlinger can drop frames without notifying us and
             * there is no way to know it. It it happens, i_audiotrack_pos
             * won't move and the current code will be stuck because it'll
             * assume that audiotrack internal buffer is full when it's not. It
             * can happen only after Android 4.4.2 if we send frames too
             * quickly. */
            i_nb_try = i_ret == 0 ? i_nb_try + 1 : 0;
        }
    }
bailout:
    block_Release( p_buffer );
}

static void
Pause( audio_output_t *p_aout, bool b_pause, mtime_t i_date )
{
    aout_sys_t *p_sys = p_aout->sys;
    JNIEnv *env;
    VLC_UNUSED( i_date );

    if( p_sys->b_error || !( env = jni_get_env( THREAD_NAME ) ) )
        return;

    if( b_pause )
    {
        JNI_AT_CALL_VOID( pause );
        CHECK_AT_EXCEPTION( "pause" );
    } else
    {
        JNI_AT_CALL_VOID( play );
        CHECK_AT_EXCEPTION( "play" );
        AudioTrack_ResetPositions( env, p_aout );
    }
}

static void
Flush( audio_output_t *p_aout, bool b_wait )
{
    aout_sys_t *p_sys = p_aout->sys;
    JNIEnv *env;

    if( p_sys->b_error || !( env = jni_get_env( THREAD_NAME ) ) )
        return;

    /* Android doc:
     * stop(): Stops playing the audio data. When used on an instance created
     * in MODE_STREAM mode, audio will stop playing after the last buffer that
     * was written has been played. For an immediate stop, use pause(),
     * followed by flush() to discard audio data that hasn't been played back
     * yet.
     *
     * flush(): Flushes the audio data currently queued for playback. Any data
     * that has not been played back will be discarded.  No-op if not stopped
     * or paused, or if the track's creation mode is not MODE_STREAM.
     */
    if( b_wait )
    {
        JNI_AT_CALL_VOID( stop );
        if( CHECK_AT_EXCEPTION( "stop" ) )
            return;
    } else
    {
        JNI_AT_CALL_VOID( pause );
        if( CHECK_AT_EXCEPTION( "pause" ) )
            return;
        JNI_AT_CALL_VOID( flush );
    }
    JNI_AT_CALL_VOID( play );
    CHECK_AT_EXCEPTION( "play" );

    if( p_sys->p_bytebuffer )
    {
        (*env)->DeleteGlobalRef( env, p_sys->p_bytebuffer );
        p_sys->p_bytebuffer = NULL;
    }

    AudioTrack_ResetPositions( env, p_aout );
    AudioTrack_ResetPlaybackHeadPosition( env, p_aout );
    p_sys->i_samples_written = 0;
}

static int
Open( vlc_object_t *obj )
{
    audio_output_t *p_aout = (audio_output_t *) obj;
    aout_sys_t *p_sys;

    if( !InitJNIFields( p_aout ) )
        return VLC_EGENERIC;

    p_sys = calloc( 1, sizeof (aout_sys_t) );

    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_aout->sys = p_sys;
    p_aout->start = Start;
    p_aout->stop = Stop;
    p_aout->play = Play;
    p_aout->pause = Pause;
    p_aout->flush = Flush;
    p_aout->time_get = TimeGet;

    aout_SoftVolumeInit( p_aout );

    return VLC_SUCCESS;
}

static void
Close( vlc_object_t *obj )
{
    audio_output_t *p_aout = (audio_output_t *) obj;
    aout_sys_t *p_sys = p_aout->sys;
    JNIEnv *env;

    if( ( env = jni_get_env( THREAD_NAME ) ) )
    {
        if( p_sys->p_bytearray )
            (*env)->DeleteGlobalRef( env, p_sys->p_bytearray );
        if( p_sys->p_floatarray )
            (*env)->DeleteGlobalRef( env, p_sys->p_floatarray );
        if( p_sys->p_bytebuffer )
            (*env)->DeleteGlobalRef( env, p_sys->p_bytebuffer );
    }

    free( p_sys );
}
