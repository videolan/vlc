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
#include "../video_output/android/utils.h"

#define SMOOTHPOS_SAMPLE_COUNT 10
#define SMOOTHPOS_INTERVAL_US INT64_C(30000) // 30ms

#define AUDIOTIMESTAMP_INTERVAL_US INT64_C(500000) // 500ms

static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );
static void Stop( audio_output_t * );
static int Start( audio_output_t *, audio_sample_format_t * );
static void *AudioTrack_Thread( void * );

/* There is an undefined behavior when configuring AudioTrack with SPDIF or
 * more than 2 channels when there is no HDMI out. It may succeed and the
 * Android ressampler will be used to downmix to stereo. It may fails cleanly,
 * and this module will be able to recover and fallback to stereo. Finally, in
 * some rare cases, it may crash during init or while ressampling. Because of
 * the last case we don't try up to 8 channels and we use AT_DEV_STEREO device
 * per default */
enum at_dev {
    AT_DEV_STEREO = 0,
    AT_DEV_PCM,
    AT_DEV_ENCODED,
};
#define AT_DEV_DEFAULT AT_DEV_STEREO
#define AT_DEV_MAX_CHANNELS 8

static const struct {
    const char *id;
    const char *name;
    enum at_dev at_dev;
} at_devs[] = {
    { "stereo", "Up to 2 channels (compat mode).", AT_DEV_STEREO },
    { "pcm", "Up to 8 channels.", AT_DEV_PCM },

    /* With "encoded", the module will try to play every audio codecs via
     * passthrough.
     *
     * With "encoded:ENCODING_FLAGS_MASK", the module will try to play only
     * codecs specified by ENCODING_FLAGS_MASK. This extra value is a long long
     * that contains binary-shifted AudioFormat.ENCODING_* values. */
    { "encoded", "Up to 8 channels, passthrough if available.", AT_DEV_ENCODED },
    {  NULL, NULL, AT_DEV_DEFAULT },
};

struct aout_sys_t {
    /* sw gain */
    float soft_gain;
    bool soft_mute;

    enum at_dev at_dev;

    jobject p_audiotrack; /* AudioTrack ref */

    audio_sample_format_t fmt; /* fmt setup by Start */

    struct {
        unsigned int i_rate;
        int i_channel_config;
        int i_format;
        int i_size;
    } audiotrack_args;

    /* Used by AudioTrack_getPlaybackHeadPosition */
    struct {
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

    uint32_t i_max_audiotrack_samples;
    long long i_encoding_flags;
    bool b_passthrough;
    uint8_t i_chans_to_reorder; /* do we need channel reordering */
    uint8_t p_chan_table[AOUT_CHAN_MAX];

    enum {
        WRITE_BYTEARRAY,
        WRITE_BYTEARRAYV23,
        WRITE_SHORTARRAYV23,
        WRITE_BYTEBUFFER,
        WRITE_FLOATARRAY
    } i_write_type;

    vlc_thread_t thread;    /* AudioTrack_Thread */
    vlc_mutex_t lock;
    vlc_cond_t aout_cond;   /* cond owned by aout */
    vlc_cond_t thread_cond; /* cond owned by AudioTrack_Thread */

    /* These variables need locking on read and write */
    bool b_thread_running;  /* Set to false by aout to stop the thread */
    bool b_thread_paused;   /* If true, the thread won't process any data, see
                             * Pause() */
    bool b_thread_waiting;  /* If true, the thread is waiting for enough spaces
                             * in AudioTrack internal buffers */

    uint64_t i_samples_written; /* Number of samples written since last flush */
    bool b_audiotrack_exception; /* True if audiotrack threw an exception */
    bool b_error; /* generic error */

    struct {
        uint64_t i_read;    /* Number of bytes read */
        uint64_t i_write;   /* Number of bytes written */
        size_t i_size;      /* Size of the circular buffer in bytes */
        union {
            jbyteArray p_bytearray;
            jfloatArray p_floatarray;
            jshortArray p_shortarray;
            struct {
                uint8_t *p_data;
                jobject p_obj;
            } bytebuffer;
        } u;
    } circular;
};

/* Soft volume helper */
#include "audio_output/volume.h"

// Don't use Float for now since 5.1/7.1 Float is down sampled to Stereo Float
//#define AUDIOTRACK_USE_FLOAT
//#define AUDIOTRACK_HW_LATENCY

/* Get AudioTrack native sample rate: if activated, most of  the resampling
 * will be done by VLC */
#define AUDIOTRACK_NATIVE_SAMPLERATE

#define AUDIOTRACK_SESSION_ID_TEXT " Id of audio session the AudioTrack must be attached to"

vlc_module_begin ()
    set_shortname( "AudioTrack" )
    set_description( "Android AudioTrack audio output" )
    set_capability( "audio output", 180 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_integer( "audiotrack-session-id", 0,
            AUDIOTRACK_SESSION_ID_TEXT, NULL, true )
        change_private()
    add_sw_gain()
    add_shortcut( "audiotrack" )
    set_callbacks( Open, Close )
vlc_module_end ()

#define THREAD_NAME "android_audiotrack"
#define GET_ENV() android_getEnv( VLC_OBJECT(p_aout), THREAD_NAME )

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
        jmethodID writeV23;
        jmethodID writeShortV23;
        jmethodID writeBufferV21;
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
        bool has_ENCODING_AC3;
        jint ENCODING_E_AC3;
        bool has_ENCODING_E_AC3;
        jint ENCODING_DOLBY_TRUEHD;
        bool has_ENCODING_DOLBY_TRUEHD;
        jint ENCODING_DTS;
        bool has_ENCODING_DTS;
        jint ENCODING_DTS_HD;
        bool has_ENCODING_DTS_HD;
        jint ENCODING_IEC61937;
        bool has_ENCODING_IEC61937;
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
InitJNIFields( audio_output_t *p_aout, JNIEnv* env )
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static int i_init_state = -1;
    bool ret;
    jclass clazz;
    jfieldID field;

    vlc_mutex_lock( &lock );

    if( i_init_state != -1 )
        goto end;

#define CHECK_EXCEPTION( what, critical ) do { \
    if( (*env)->ExceptionCheck( env ) ) \
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
    CHECK_EXCEPTION( "FindClass(" str ")", critical ); \
} while( 0 )
#define GET_ID( get, id, str, args, critical ) do { \
    jfields.id = (*env)->get( env, clazz, (str), (args) ); \
    CHECK_EXCEPTION( #get "(" #id ")", critical ); \
} while( 0 )
#define GET_CONST_INT( id, str, critical ) do { \
    field = NULL; \
    field = (*env)->GetStaticFieldID( env, clazz, (str), "I" ); \
    CHECK_EXCEPTION( "GetStaticFieldID(" #id ")", critical ); \
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

    GET_ID( GetMethodID, AudioTrack.ctor, "<init>", "(IIIIIII)V", true );
    GET_ID( GetMethodID, AudioTrack.release, "release", "()V", true );
    GET_ID( GetMethodID, AudioTrack.getState, "getState", "()I", true );
    GET_ID( GetMethodID, AudioTrack.play, "play", "()V", true );
    GET_ID( GetMethodID, AudioTrack.stop, "stop", "()V", true );
    GET_ID( GetMethodID, AudioTrack.flush, "flush", "()V", true );
    GET_ID( GetMethodID, AudioTrack.pause, "pause", "()V", true );

    GET_ID( GetMethodID, AudioTrack.writeV23, "write", "([BIII)I", false );
    GET_ID( GetMethodID, AudioTrack.writeShortV23, "write", "([SIII)I", false );
    if( !jfields.AudioTrack.writeV23 )
        GET_ID( GetMethodID, AudioTrack.writeBufferV21, "write", "(Ljava/nio/ByteBuffer;II)I", false );

    if( jfields.AudioTrack.writeV23 || jfields.AudioTrack.writeBufferV21 )
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
#ifdef AUDIOTRACK_NATIVE_SAMPLERATE
    GET_ID( GetStaticMethodID, AudioTrack.getNativeOutputSampleRate,
            "getNativeOutputSampleRate",  "(I)I", true );
#endif
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

#ifdef AUDIOTRACK_HW_LATENCY
    /* AudioSystem class init */
    GET_CLASS( "android/media/AudioSystem", false );
    if( clazz )
    {
        jfields.AudioSystem.clazz = (jclass) (*env)->NewGlobalRef( env, clazz );
        GET_ID( GetStaticMethodID, AudioSystem.getOutputLatency,
                "getOutputLatency", "(I)I", false );
    }
#endif

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

    if( jfields.AudioTrack.writeShortV23 )
    {
        GET_CONST_INT( AudioFormat.ENCODING_IEC61937, "ENCODING_IEC61937", false );
        jfields.AudioFormat.has_ENCODING_IEC61937 = field != NULL;
    }
    else
        jfields.AudioFormat.has_ENCODING_IEC61937 = false;

    GET_CONST_INT( AudioFormat.ENCODING_AC3, "ENCODING_AC3", false );
    jfields.AudioFormat.has_ENCODING_AC3 = field != NULL;
    GET_CONST_INT( AudioFormat.ENCODING_E_AC3, "ENCODING_E_AC3", false );
    jfields.AudioFormat.has_ENCODING_E_AC3 = field != NULL;

    GET_CONST_INT( AudioFormat.ENCODING_DTS, "ENCODING_DTS", false );
    jfields.AudioFormat.has_ENCODING_DTS = field != NULL;
    GET_CONST_INT( AudioFormat.ENCODING_DTS_HD, "ENCODING_DTS_HD", false );
    jfields.AudioFormat.has_ENCODING_DTS_HD = field != NULL;

    GET_CONST_INT( AudioFormat.ENCODING_DOLBY_TRUEHD, "ENCODING_DOLBY_TRUEHD",
                   false );
    jfields.AudioFormat.has_ENCODING_DOLBY_TRUEHD = field != NULL;

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
    if( (*env)->ExceptionCheck( env ) )
    {
        aout_sys_t *p_sys = p_aout->sys;

        p_sys->b_audiotrack_exception = true;
        p_sys->b_error = true;
        (*env)->ExceptionDescribe( env );
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
    return i_bytes * p_sys->fmt.i_frame_length / p_sys->fmt.i_bytes_per_frame;
}
#define BYTES_TO_FRAMES(x) bytes_to_frames( p_sys, (x) )
#define BYTES_TO_US(x) frames_to_us( p_sys, bytes_to_frames( p_sys, (x) ) )

static inline size_t
frames_to_bytes( aout_sys_t *p_sys, uint64_t i_frames )
{
    return i_frames * p_sys->fmt.i_bytes_per_frame / p_sys->fmt.i_frame_length;
}
#define FRAMES_TO_BYTES(x) frames_to_bytes( p_sys, (x) )

/**
 * Get the AudioTrack position
 *
 * The doc says that the position is reset to zero after flush but it's not
 * true for all devices or Android versions.
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
 * Reset AudioTrack position
 *
 * Called after flush, or start
 */
static void
AudioTrack_ResetPlaybackHeadPosition( JNIEnv *env, audio_output_t *p_aout )
{
    (void) env;
    aout_sys_t *p_sys = p_aout->sys;

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
 * Reset all AudioTrack positions and internal state
 */
static void
AudioTrack_Reset( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    AudioTrack_ResetPositions( env, p_aout );
    AudioTrack_ResetPlaybackHeadPosition( env, p_aout );
    p_sys->i_samples_written = 0;
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
        i_audiotrack_us = FRAMES_TO_US( AudioTrack_getPlaybackHeadPosition( env, p_aout ) );

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

    if( p_sys->b_passthrough )
        return -1;

    vlc_mutex_lock( &p_sys->lock );

    if( p_sys->b_error || !p_sys->i_samples_written || !( env = GET_ENV() ) )
        goto bailout;

    i_audiotrack_us = AudioTrack_GetTimestampPositionUs( env, p_aout );

    if( i_audiotrack_us <= 0 )
        i_audiotrack_us = AudioTrack_GetSmoothPositionUs(env, p_aout );

/* Debug log for both delays */
#if 0
{
    mtime_t i_written_us = FRAMES_TO_US( p_sys->i_samples_written );
    mtime_t i_ts_us = AudioTrack_GetTimestampPositionUs( env, p_aout );
    mtime_t i_smooth_us = 0;

    if( i_ts_us > 0 )
        i_smooth_us = AudioTrack_GetSmoothPositionUs(env, p_aout );
    else if ( p_sys->smoothpos.i_us != 0 )
        i_smooth_us = p_sys->smoothpos.i_us + mdate()
            - p_sys->smoothpos.i_latency_us;

    msg_Err( p_aout, "TimeGet: TimeStamp: %lld, Smooth: %lld (latency: %lld)",
                    i_ts_us ? i_written_us - i_ts_us : 0,
                    i_smooth_us ? i_written_us - i_smooth_us : 0,
                    p_sys->smoothpos.i_latency_us );
}
#endif

    if( i_audiotrack_us > 0 )
    {
        /* AudioTrack delay */
        mtime_t i_delay = FRAMES_TO_US( p_sys->i_samples_written )
                        - i_audiotrack_us;
        if( i_delay >= 0 )
        {
            /* Circular buffer delay */
            i_delay += BYTES_TO_US( p_sys->circular.i_write
                                    - p_sys->circular.i_read );
            *p_delay = i_delay;
            vlc_mutex_unlock( &p_sys->lock );
            return 0;
        }
        else
        {
            msg_Warn( p_aout, "timing screwed, reset positions" );
            AudioTrack_ResetPositions( env, p_aout );
        }
    }

bailout:
    vlc_mutex_unlock( &p_sys->lock );
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
 * Create an Android AudioTrack.
 * returns -1 on error, 0 on success.
 */
static int
AudioTrack_New( JNIEnv *env, audio_output_t *p_aout, unsigned int i_rate,
                int i_channel_config, int i_format, int i_size )
{
    aout_sys_t *p_sys = p_aout->sys;
    jint session_id = var_InheritInteger( p_aout, "audiotrack-session-id" );
    jobject p_audiotrack = JNI_AT_NEW( jfields.AudioManager.STREAM_MUSIC,
                                       i_rate, i_channel_config, i_format,
                                       i_size, jfields.AudioTrack.MODE_STREAM,
                                       session_id );
    if( CHECK_AT_EXCEPTION( "AudioTrack<init>" ) || !p_audiotrack )
    {
        msg_Warn( p_aout, "AudioTrack Init failed" ) ;
        return -1;
    }
    if( JNI_CALL_INT( p_audiotrack, jfields.AudioTrack.getState )
        != jfields.AudioTrack.STATE_INITIALIZED )
    {
        JNI_CALL_VOID( p_audiotrack, jfields.AudioTrack.release );
        (*env)->DeleteLocalRef( env, p_audiotrack );
        msg_Err( p_aout, "AudioTrack getState failed" );
        return -1;
    }

    p_sys->p_audiotrack = (*env)->NewGlobalRef( env, p_audiotrack );
    (*env)->DeleteLocalRef( env, p_audiotrack );
    if( !p_sys->p_audiotrack )
        return -1;

    return 0;
}

/**
 * Destroy and recreate an Android AudioTrack using the same arguments.
 * returns -1 on error, 0 on success.
 */
static int
AudioTrack_Recreate( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    JNI_AT_CALL_VOID( release );
    (*env)->DeleteGlobalRef( env, p_sys->p_audiotrack );
    p_sys->p_audiotrack = NULL;
    return AudioTrack_New( env, p_aout, p_sys->audiotrack_args.i_rate,
                           p_sys->audiotrack_args.i_channel_config,
                           p_sys->audiotrack_args.i_format,
                           p_sys->audiotrack_args.i_size );
}

/**
 * Configure and create an Android AudioTrack.
 * returns -1 on configuration error, 0 on success.
 */
static int
AudioTrack_Create( JNIEnv *env, audio_output_t *p_aout,
                   unsigned int i_rate,
                   int i_format,
                   uint16_t i_physical_channels )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_size, i_min_buffer_size, i_channel_config;

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
        case AOUT_CHANS_STEREO:
            i_channel_config = jfields.AudioFormat.CHANNEL_OUT_STEREO;
            break;
        default:
            vlc_assert_unreachable();
    }

    i_min_buffer_size = JNI_AT_CALL_STATIC_INT( getMinBufferSize, i_rate,
                                                i_channel_config, i_format );
    if( i_min_buffer_size <= 0 )
    {
        msg_Warn( p_aout, "getMinBufferSize returned an invalid size" ) ;
        return -1;
    }
    i_size = i_min_buffer_size * 2;

    /* create AudioTrack object */
    if( AudioTrack_New( env, p_aout, i_rate, i_channel_config,
                        i_format , i_size ) != 0 )
        return -1;

    p_sys->audiotrack_args.i_rate = i_rate;
    p_sys->audiotrack_args.i_channel_config = i_channel_config;
    p_sys->audiotrack_args.i_format = i_format;
    p_sys->audiotrack_args.i_size = i_size;

    return 0;
}

static bool
AudioTrack_HasEncoding( audio_output_t *p_aout, vlc_fourcc_t i_format,
                        bool *p_dtshd )
{
    aout_sys_t *p_sys = p_aout->sys;

#define MATCH_ENCODING_FLAG(x) jfields.AudioFormat.has_##x && \
    ( p_sys->i_encoding_flags == 0 || p_sys->i_encoding_flags & (1 << jfields.AudioFormat.x) )

    *p_dtshd = false;
    switch( i_format )
    {
        case VLC_CODEC_DTS:
            if( MATCH_ENCODING_FLAG( ENCODING_DTS_HD )
             && var_GetBool( p_aout, "dtshd" ) )
            {
                *p_dtshd = true;
                return true;
            }
            return MATCH_ENCODING_FLAG( ENCODING_DTS );
        case VLC_CODEC_A52:
            return MATCH_ENCODING_FLAG( ENCODING_AC3 );
        case VLC_CODEC_EAC3:
            return MATCH_ENCODING_FLAG( ENCODING_E_AC3 );
        case VLC_CODEC_TRUEHD:
        case VLC_CODEC_MLP:
            return MATCH_ENCODING_FLAG( ENCODING_DOLBY_TRUEHD );
        default:
            return false;
    }
}

static int
StartPassthrough( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_at_format;

    if( jfields.AudioFormat.has_ENCODING_IEC61937 )
    {
        bool b_dtshd;
        if( !AudioTrack_HasEncoding( p_aout, p_sys->fmt.i_format, &b_dtshd ) )
            return VLC_EGENERIC;
        i_at_format = jfields.AudioFormat.ENCODING_IEC61937;
        switch( p_sys->fmt.i_format )
        {
            case VLC_CODEC_TRUEHD:
            case VLC_CODEC_MLP:
                p_sys->fmt.i_rate = 192000;
                p_sys->fmt.i_bytes_per_frame = 16;

                /* AudioFormat.ENCODING_IEC61937 documentation says that the
                 * channel layout must be stereo. Well, not for TrueHD
                 * apparently */
                p_sys->fmt.i_physical_channels = AOUT_CHANS_7_1;
                break;
            case VLC_CODEC_DTS:
                p_sys->fmt.i_bytes_per_frame = 4;
                p_sys->fmt.i_physical_channels = AOUT_CHANS_STEREO;
                if( b_dtshd )
                {
                    p_sys->fmt.i_rate = 192000;
                    p_sys->fmt.i_bytes_per_frame = 16;
                }
                break;
            case VLC_CODEC_EAC3:
                p_sys->fmt.i_rate = 192000;
            case VLC_CODEC_A52:
                p_sys->fmt.i_physical_channels = AOUT_CHANS_STEREO;
                p_sys->fmt.i_bytes_per_frame = 4;
                break;
            default:
                return VLC_EGENERIC;
        }
        p_sys->fmt.i_frame_length = 1;
        p_sys->fmt.i_channels = aout_FormatNbChannels( &p_sys->fmt );
        p_sys->fmt.i_format = VLC_CODEC_SPDIFL;
    }
    else
    {
        switch( p_sys->fmt.i_format )
        {
            case VLC_CODEC_A52:
                if( !jfields.AudioFormat.has_ENCODING_AC3 )
                    return VLC_EGENERIC;
                i_at_format = jfields.AudioFormat.ENCODING_AC3;
                break;
            case VLC_CODEC_DTS:
                if( !jfields.AudioFormat.has_ENCODING_DTS )
                    return VLC_EGENERIC;
                i_at_format = jfields.AudioFormat.ENCODING_DTS;
                break;
            default:
                return VLC_EGENERIC;
        }
        p_sys->fmt.i_bytes_per_frame = 4;
        p_sys->fmt.i_frame_length = 1;
        p_sys->fmt.i_physical_channels = AOUT_CHANS_STEREO;
        p_sys->fmt.i_channels = 2;
        p_sys->fmt.i_format = VLC_CODEC_SPDIFB;
    }

    int i_ret = AudioTrack_Create( env, p_aout, p_sys->fmt.i_rate, i_at_format,
                                   p_sys->fmt.i_physical_channels );
    if( i_ret != VLC_SUCCESS )
        msg_Warn( p_aout, "SPDIF configuration failed" );
    else
    {
        p_sys->b_passthrough = true;
        p_sys->i_chans_to_reorder = 0;
    }

    return i_ret;
}

static int
StartPCM( JNIEnv *env, audio_output_t *p_aout, unsigned i_max_channels )
{
    aout_sys_t *p_sys = p_aout->sys;
    unsigned i_nb_channels;
    int i_at_format, i_ret;

    if (jfields.AudioTrack.getNativeOutputSampleRate)
        p_sys->fmt.i_rate =
            JNI_AT_CALL_STATIC_INT( getNativeOutputSampleRate,
                                    jfields.AudioManager.STREAM_MUSIC );
    else
        p_sys->fmt.i_rate = VLC_CLIP( p_sys->fmt.i_rate, 4000, 48000 );

    do
    {
        /* We can only accept U8, S16N, FL32, and AC3 */
        switch( p_sys->fmt.i_format )
        {
        case VLC_CODEC_U8:
            i_at_format = jfields.AudioFormat.ENCODING_PCM_8BIT;
            break;
        case VLC_CODEC_S16N:
            i_at_format = jfields.AudioFormat.ENCODING_PCM_16BIT;
            break;
        case VLC_CODEC_FL32:
            if( jfields.AudioFormat.has_ENCODING_PCM_FLOAT )
                i_at_format = jfields.AudioFormat.ENCODING_PCM_FLOAT;
            else
            {
                p_sys->fmt.i_format = VLC_CODEC_S16N;
                i_at_format = jfields.AudioFormat.ENCODING_PCM_16BIT;
            }
            break;
        default:
            p_sys->fmt.i_format = VLC_CODEC_S16N;
            i_at_format = jfields.AudioFormat.ENCODING_PCM_16BIT;
            break;
        }

        /* Android AudioTrack supports only mono, stereo, 5.1 and 7.1.
         * Android will downmix to stereo if audio output doesn't handle 5.1 or 7.1
         */

        i_nb_channels = aout_FormatNbChannels( &p_sys->fmt );
        if( i_nb_channels == 0 )
            return VLC_EGENERIC;
        if( AOUT_FMT_LINEAR( &p_sys->fmt ) )
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

        /* Try to create an AudioTrack with the most advanced channel and
         * format configuration. If AudioTrack_Create fails, try again with a
         * less advanced format (PCM S16N). If it fails again, try again with
         * Stereo channels. */
        i_ret = AudioTrack_Create( env, p_aout, p_sys->fmt.i_rate, i_at_format,
                                   p_sys->fmt.i_physical_channels );
        if( i_ret != 0 )
        {
            if( p_sys->fmt.i_format == VLC_CODEC_FL32 )
            {
                msg_Warn( p_aout, "FL32 configuration failed, "
                                  "fallback to S16N PCM" );
                p_sys->fmt.i_format = VLC_CODEC_S16N;
            }
            else if( p_sys->fmt.i_physical_channels & AOUT_CHANS_5_1 )
            {
                msg_Warn( p_aout, "5.1 or 7.1 configuration failed, "
                                  "fallback to Stereo" );
                p_sys->fmt.i_physical_channels = AOUT_CHANS_STEREO;
            }
            else
                break;
        }
    } while( i_ret != 0 );

    if( i_ret != VLC_SUCCESS )
        return i_ret;

    uint32_t p_chans_out[AOUT_CHAN_MAX];
    memset( p_chans_out, 0, sizeof(p_chans_out) );
    AudioTrack_GetChanOrder( p_sys->fmt.i_physical_channels, p_chans_out );
    p_sys->i_chans_to_reorder =
        aout_CheckChannelReorder( NULL, p_chans_out,
                                  p_sys->fmt.i_physical_channels,
                                  p_sys->p_chan_table );
    aout_FormatPrepare( &p_sys->fmt );
    return VLC_SUCCESS;
}

static int
Start( audio_output_t *p_aout, audio_sample_format_t *restrict p_fmt )
{
    aout_sys_t *p_sys = p_aout->sys;
    JNIEnv *env;
    int i_ret;
    bool b_try_passthrough;
    unsigned i_max_channels;

    if( p_sys->at_dev == AT_DEV_ENCODED )
    {
        b_try_passthrough = true;
        i_max_channels = AT_DEV_MAX_CHANNELS;
    }
    else
    {
        b_try_passthrough = var_InheritBool( p_aout, "spdif" );
        i_max_channels = p_sys->at_dev == AT_DEV_STEREO ? 2 : AT_DEV_MAX_CHANNELS;
    }

    if( !( env = GET_ENV() ) )
        return VLC_EGENERIC;

    p_sys->fmt = *p_fmt;

    aout_FormatPrint( p_aout, "VLC is looking for:", &p_sys->fmt );

    bool low_latency = false;
    if (p_sys->fmt.channel_type == AUDIO_CHANNEL_TYPE_AMBISONICS)
    {
        p_sys->fmt.channel_type = AUDIO_CHANNEL_TYPE_BITMAP;

        /* TODO: detect sink channel layout */
        p_sys->fmt.i_physical_channels = AOUT_CHANS_STEREO;
        aout_FormatPrepare(&p_sys->fmt);
        low_latency = true;
    }

    if( AOUT_FMT_LINEAR( &p_sys->fmt ) )
        i_ret = StartPCM( env, p_aout, i_max_channels );
    else if( b_try_passthrough )
        i_ret = StartPassthrough( env, p_aout );
    else
        return VLC_EGENERIC;

    if( i_ret != 0 )
        return VLC_EGENERIC;

    p_sys->i_max_audiotrack_samples = BYTES_TO_FRAMES( p_sys->audiotrack_args.i_size );

#ifdef AUDIOTRACK_HW_LATENCY
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
            goto error;
    }
#endif

    AudioTrack_Reset( env, p_aout );

    if( p_sys->fmt.i_format == VLC_CODEC_FL32 )
    {
        msg_Dbg( p_aout, "using WRITE_FLOATARRAY");
        p_sys->i_write_type = WRITE_FLOATARRAY;
    }
    else if( p_sys->fmt.i_format == VLC_CODEC_SPDIFL )
    {
        assert( jfields.AudioFormat.has_ENCODING_IEC61937 );
        msg_Dbg( p_aout, "using WRITE_SHORTARRAYV23");
        p_sys->i_write_type = WRITE_SHORTARRAYV23;
    }
    else if( jfields.AudioTrack.writeV23 )
    {
        msg_Dbg( p_aout, "using WRITE_BYTEARRAYV23");
        p_sys->i_write_type = WRITE_BYTEARRAYV23;
    }
    else if( jfields.AudioTrack.writeBufferV21 )
    {
        msg_Dbg( p_aout, "using WRITE_BYTEBUFFER");
        p_sys->i_write_type = WRITE_BYTEBUFFER;
    }
    else
    {
        msg_Dbg( p_aout, "using WRITE_BYTEARRAY");
        p_sys->i_write_type = WRITE_BYTEARRAY;
    }

    p_sys->circular.i_read = p_sys->circular.i_write = 0;
    p_sys->circular.i_size = (int)p_sys->fmt.i_rate
                           * p_sys->fmt.i_bytes_per_frame
                           / p_sys->fmt.i_frame_length;
    if (low_latency)
    {
        /* 40 ms of buffering */
        p_sys->circular.i_size = p_sys->circular.i_size / 25;
    }
    else
    {
        /* 2 seconds of buffering */
        p_sys->circular.i_size = p_sys->circular.i_size * AOUT_MAX_PREPARE_TIME
                               / CLOCK_FREQ;
    }

    /* Allocate circular buffer */
    switch( p_sys->i_write_type )
    {
        case WRITE_BYTEARRAY:
        case WRITE_BYTEARRAYV23:
        {
            jbyteArray p_bytearray;

            p_bytearray = (*env)->NewByteArray( env, p_sys->circular.i_size );
            if( p_bytearray )
            {
                p_sys->circular.u.p_bytearray = (*env)->NewGlobalRef( env, p_bytearray );
                (*env)->DeleteLocalRef( env, p_bytearray );
            }

            if( !p_sys->circular.u.p_bytearray )
            {
                msg_Err(p_aout, "byte array allocation failed");
                goto error;
            }
            break;
        }
        case WRITE_SHORTARRAYV23:
        {
            jshortArray p_shortarray;

            p_shortarray = (*env)->NewShortArray( env,
                                                  p_sys->circular.i_size / 2 );
            if( p_shortarray )
            {
                p_sys->circular.u.p_shortarray = (*env)->NewGlobalRef( env, p_shortarray );
                (*env)->DeleteLocalRef( env, p_shortarray );
            }
            if( !p_sys->circular.u.p_shortarray )
            {
                msg_Err(p_aout, "short array allocation failed");
                goto error;
            }
            break;
        }
        case WRITE_FLOATARRAY:
        {
            jfloatArray p_floatarray;

            p_floatarray = (*env)->NewFloatArray( env,
                                                  p_sys->circular.i_size / 4 );
            if( p_floatarray )
            {
                p_sys->circular.u.p_floatarray = (*env)->NewGlobalRef( env, p_floatarray );
                (*env)->DeleteLocalRef( env, p_floatarray );
            }
            if( !p_sys->circular.u.p_floatarray )
            {
                msg_Err(p_aout, "float array allocation failed");
                goto error;
            }
            break;
        }
        case WRITE_BYTEBUFFER:
            p_sys->circular.u.bytebuffer.p_data = malloc( p_sys->circular.i_size );
            if( !p_sys->circular.u.bytebuffer.p_data )
            {
                msg_Err(p_aout, "bytebuffer allocation failed");
                goto error;
            }
            break;
    }

    /* Run AudioTrack_Thread */
    p_sys->b_thread_running = true;
    p_sys->b_thread_paused = false;
    if ( vlc_clone( &p_sys->thread, AudioTrack_Thread, p_aout,
                    VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err(p_aout, "vlc clone failed");
        goto error;
    }

    JNI_AT_CALL_VOID( play );
    CHECK_AT_EXCEPTION( "play" );

    *p_fmt = p_sys->fmt;
    aout_SoftVolumeStart( p_aout );

    aout_FormatPrint( p_aout, "VLC will output:", &p_sys->fmt );

    return VLC_SUCCESS;

error:
    Stop( p_aout );
    return VLC_EGENERIC;
}

static void
Stop( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    JNIEnv *env;

    if( !( env = GET_ENV() ) )
        return;

    /* Stop the AudioTrack thread */
    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->b_thread_running )
    {
        p_sys->b_thread_running = false;
        vlc_cond_signal( &p_sys->thread_cond );
        vlc_mutex_unlock( &p_sys->lock );
        vlc_join( p_sys->thread, NULL );
    }
    else
        vlc_mutex_unlock(  &p_sys->lock );

    /* Release the AudioTrack object */
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

    /* Release the timestamp object */
    if( p_sys->timestamp.p_obj )
    {
        (*env)->DeleteGlobalRef( env, p_sys->timestamp.p_obj );
        p_sys->timestamp.p_obj = NULL;
    }

    /* Release the Circular buffer data */
    switch( p_sys->i_write_type )
    {
    case WRITE_BYTEARRAY:
    case WRITE_BYTEARRAYV23:
        if( p_sys->circular.u.p_bytearray )
        {
            (*env)->DeleteGlobalRef( env, p_sys->circular.u.p_bytearray );
            p_sys->circular.u.p_bytearray = NULL;
        }
        break;
    case WRITE_SHORTARRAYV23:
        if( p_sys->circular.u.p_shortarray )
        {
            (*env)->DeleteGlobalRef( env, p_sys->circular.u.p_shortarray );
            p_sys->circular.u.p_shortarray = NULL;
        }
        break;
    case WRITE_FLOATARRAY:
        if( p_sys->circular.u.p_floatarray )
        {
            (*env)->DeleteGlobalRef( env, p_sys->circular.u.p_floatarray );
            p_sys->circular.u.p_floatarray = NULL;
        }
        break;
    case WRITE_BYTEBUFFER:
        free( p_sys->circular.u.bytebuffer.p_data );
        p_sys->circular.u.bytebuffer.p_data = NULL;
        break;
    }

    p_sys->b_audiotrack_exception = false;
    p_sys->b_error = false;
    p_sys->b_passthrough = false;
}

/**
 * Non blocking write function, run from AudioTrack_Thread.
 * Do a calculation between current position and audiotrack position and assure
 * that we won't wait in AudioTrack.write() method.
 */
static int
AudioTrack_PlayByteArray( JNIEnv *env, audio_output_t *p_aout,
                          size_t i_data_size, size_t i_data_offset,
                          bool b_force )
{
    aout_sys_t *p_sys = p_aout->sys;
    uint64_t i_samples;
    uint64_t i_audiotrack_pos;
    uint64_t i_samples_pending;

    i_audiotrack_pos = AudioTrack_getPlaybackHeadPosition( env, p_aout );

    assert( i_audiotrack_pos <= p_sys->i_samples_written );
    if( i_audiotrack_pos > p_sys->i_samples_written )
    {
        msg_Err( p_aout, "audiotrack position is ahead. Should NOT happen" );
        p_sys->i_samples_written = 0;
        p_sys->b_error = true;
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
                       BYTES_TO_FRAMES( i_data_size ) );

    i_data_size = FRAMES_TO_BYTES( i_samples );

    return JNI_AT_CALL_INT( write, p_sys->circular.u.p_bytearray,
                            i_data_offset, i_data_size );
}

/**
 * Non blocking write function for Android M and after, run from
 * AudioTrack_Thread. It calls a new write method with WRITE_NON_BLOCKING
 * flags.
 */
static int
AudioTrack_PlayByteArrayV23( JNIEnv *env, audio_output_t *p_aout,
                             size_t i_data_size, size_t i_data_offset )
{
    aout_sys_t *p_sys = p_aout->sys;

    return JNI_AT_CALL_INT( writeV23, p_sys->circular.u.p_bytearray,
                            i_data_offset, i_data_size,
                            jfields.AudioTrack.WRITE_NON_BLOCKING );
}

/**
 * Non blocking play function for Lollipop and after, run from
 * AudioTrack_Thread. It calls a new write method with WRITE_NON_BLOCKING
 * flags.
 */
static int
AudioTrack_PlayByteBuffer( JNIEnv *env, audio_output_t *p_aout,
                           size_t i_data_size, size_t i_data_offset )
{
    aout_sys_t *p_sys = p_aout->sys;

    /* The same DirectByteBuffer will be used until the data_offset reaches 0.
     * The internal position of this buffer is moved by the writeBufferV21
     * wall. */
    if( i_data_offset == 0 )
    {
        /* No need to get a global ref, this object will be only used from the
         * same Thread */
        if( p_sys->circular.u.bytebuffer.p_obj )
            (*env)->DeleteLocalRef( env, p_sys->circular.u.bytebuffer.p_obj );

        p_sys->circular.u.bytebuffer.p_obj = (*env)->NewDirectByteBuffer( env,
                                            p_sys->circular.u.bytebuffer.p_data,
                                            p_sys->circular.i_size );
        if( !p_sys->circular.u.bytebuffer.p_obj )
        {
            if( (*env)->ExceptionCheck( env ) )
                (*env)->ExceptionClear( env );
            return jfields.AudioTrack.ERROR;
        }
    }

    return JNI_AT_CALL_INT( writeBufferV21, p_sys->circular.u.bytebuffer.p_obj,
                            i_data_size,
                            jfields.AudioTrack.WRITE_NON_BLOCKING );
}

/**
 * Non blocking short write function for Android M and after, run from
 * AudioTrack_Thread. It calls a new write method with WRITE_NON_BLOCKING
 * flags.
 */
static int
AudioTrack_PlayShortArrayV23( JNIEnv *env, audio_output_t *p_aout,
                               size_t i_data_size, size_t i_data_offset )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_ret;

    i_ret = JNI_AT_CALL_INT( writeShortV23, p_sys->circular.u.p_shortarray,
                             i_data_offset / 2, i_data_size / 2,
                             jfields.AudioTrack.WRITE_NON_BLOCKING );
    if( i_ret < 0 )
        return i_ret;
    else
        return i_ret * 2;
}

/**
 * Non blocking play float function for Lollipop and after, run from
 * AudioTrack_Thread. It calls a new write method with WRITE_NON_BLOCKING
 * flags.
 */
static int
AudioTrack_PlayFloatArray( JNIEnv *env, audio_output_t *p_aout,
                           size_t i_data_size, size_t i_data_offset )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_ret;

    i_ret = JNI_AT_CALL_INT( writeFloat, p_sys->circular.u.p_floatarray,
                             i_data_offset / 4, i_data_size / 4,
                             jfields.AudioTrack.WRITE_NON_BLOCKING );
    if( i_ret < 0 )
        return i_ret;
    else
        return i_ret * 4;
}

static int
AudioTrack_Play( JNIEnv *env, audio_output_t *p_aout, size_t i_data_size,
                 size_t i_data_offset, bool b_force )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_ret;

    switch( p_sys->i_write_type )
    {
    case WRITE_BYTEARRAYV23:
        i_ret = AudioTrack_PlayByteArrayV23( env, p_aout, i_data_size,
                                             i_data_offset );
        break;
    case WRITE_BYTEBUFFER:
        i_ret = AudioTrack_PlayByteBuffer( env, p_aout, i_data_size,
                                           i_data_offset );
        break;
    case WRITE_SHORTARRAYV23:
        i_ret = AudioTrack_PlayShortArrayV23( env, p_aout, i_data_size,
                                              i_data_offset );
        break;
    case WRITE_BYTEARRAY:
        i_ret = AudioTrack_PlayByteArray( env, p_aout, i_data_size,
                                          i_data_offset, b_force );
        break;
    case WRITE_FLOATARRAY:
        i_ret = AudioTrack_PlayFloatArray( env, p_aout, i_data_size,
                                           i_data_offset );
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
            if( ( i_ret = AudioTrack_Recreate( env, p_aout ) ) == 0 )
            {
                AudioTrack_Reset( env, p_aout );
                JNI_AT_CALL_VOID( play );
                CHECK_AT_EXCEPTION( "play" );
            }
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
            p_sys->b_error = true;
        }
    } else
        p_sys->i_samples_written += BYTES_TO_FRAMES( i_ret );
    return i_ret;
}

/**
 * This thread will play the data coming from the circular buffer.
 */
static void *
AudioTrack_Thread( void *p_data )
{
    audio_output_t *p_aout = p_data;
    aout_sys_t *p_sys = p_aout->sys;
    JNIEnv *env = GET_ENV();
    mtime_t i_play_deadline = 0;
    mtime_t i_last_time_blocked = 0;

    if( !env )
        return NULL;

    for( ;; )
    {
        int i_ret = 0;
        bool b_forced;
        size_t i_data_offset;
        size_t i_data_size;

        vlc_mutex_lock( &p_sys->lock );

        /* Wait for free space in Audiotrack internal buffer */
        if( i_play_deadline != 0 && mdate() < i_play_deadline )
        {
            /* Don't wake up the thread when there is new data since we are
             * waiting for more space */
            p_sys->b_thread_waiting = true;
            while( p_sys->b_thread_running && i_ret != ETIMEDOUT )
                i_ret = vlc_cond_timedwait( &p_sys->thread_cond,
                                            &p_sys->lock,
                                            i_play_deadline );
            i_play_deadline = 0;
            p_sys->b_thread_waiting = false;
        }

        /* Wait for not paused state */
        while( p_sys->b_thread_running && p_sys->b_thread_paused )
        {
            i_last_time_blocked = 0;
            vlc_cond_wait( &p_sys->thread_cond, &p_sys->lock );
        }

        /* Wait for more data in the circular buffer */
        while( p_sys->b_thread_running
            && p_sys->circular.i_read >= p_sys->circular.i_write )
            vlc_cond_wait( &p_sys->thread_cond, &p_sys->lock );

        if( !p_sys->b_thread_running || p_sys->b_error )
        {
            vlc_mutex_unlock( &p_sys->lock );
            break;
        }

        /* HACK: AudioFlinger can drop frames without notifying us and there is
         * no way to know it. If it happens, i_audiotrack_pos won't move and
         * the current code will be stuck because it'll assume that audiotrack
         * internal buffer is full when it's not. It may happen only after
         * Android 4.4.2 if we send frames too quickly. To fix this issue,
         * force the writing of the buffer after a certain delay. */
        if( i_last_time_blocked != 0 )
            b_forced = mdate() - i_last_time_blocked >
                       FRAMES_TO_US( p_sys->i_max_audiotrack_samples ) * 2;
        else
            b_forced = false;

        i_data_offset = p_sys->circular.i_read % p_sys->circular.i_size;
        i_data_size = __MIN( p_sys->circular.i_size - i_data_offset,
                             p_sys->circular.i_write - p_sys->circular.i_read );

        i_ret = AudioTrack_Play( env, p_aout, i_data_size, i_data_offset,
                                 b_forced );
        if( i_ret >= 0 )
        {
            if( p_sys->i_write_type == WRITE_BYTEARRAY )
            {
                if( i_ret != 0 )
                    i_last_time_blocked = 0;
                else if( i_last_time_blocked == 0 )
                    i_last_time_blocked = mdate();
            }

            if( i_ret == 0 )
                i_play_deadline = mdate() + __MAX( 10000, FRAMES_TO_US(
                                  p_sys->i_max_audiotrack_samples / 5 ) );
            else
                p_sys->circular.i_read += i_ret;
        }

        vlc_cond_signal( &p_sys->aout_cond );
        vlc_mutex_unlock( &p_sys->lock );
    }

    if( p_sys->circular.u.bytebuffer.p_obj )
    {
        (*env)->DeleteLocalRef( env, p_sys->circular.u.bytebuffer.p_obj );
        p_sys->circular.u.bytebuffer.p_obj = NULL;
    }

    return NULL;
}

static void
Play( audio_output_t *p_aout, block_t *p_buffer )
{
    JNIEnv *env = NULL;
    size_t i_buffer_offset = 0;
    aout_sys_t *p_sys = p_aout->sys;

    vlc_mutex_lock( &p_sys->lock );

    if( p_sys->b_error || !( env = GET_ENV() ) )
        goto bailout;

    if( p_sys->i_chans_to_reorder )
       aout_ChannelReorder( p_buffer->p_buffer, p_buffer->i_buffer,
                            p_sys->i_chans_to_reorder, p_sys->p_chan_table,
                            p_sys->fmt.i_format );

    while( i_buffer_offset < p_buffer->i_buffer && !p_sys->b_error )
    {
        size_t i_circular_free;
        size_t i_data_offset;
        size_t i_data_size;

        /* Wait for enough room in circular buffer */
        while( !p_sys->b_error && ( i_circular_free = p_sys->circular.i_size -
               ( p_sys->circular.i_write - p_sys->circular.i_read ) ) == 0 )
            vlc_cond_wait( &p_sys->aout_cond, &p_sys->lock );
        if( p_sys->b_error )
            goto bailout;

        i_data_offset = p_sys->circular.i_write % p_sys->circular.i_size;
        i_data_size = __MIN( p_buffer->i_buffer - i_buffer_offset,
                             p_sys->circular.i_size - i_data_offset );
        i_data_size = __MIN( i_data_size, i_circular_free );

        switch( p_sys->i_write_type )
        {
        case WRITE_BYTEARRAY:
        case WRITE_BYTEARRAYV23:
            (*env)->SetByteArrayRegion( env, p_sys->circular.u.p_bytearray,
                                        i_data_offset, i_data_size,
                                        (jbyte *)p_buffer->p_buffer
                                        + i_buffer_offset);
            break;
        case WRITE_SHORTARRAYV23:
            i_data_offset &= ~1;
            i_data_size &= ~1;
            (*env)->SetShortArrayRegion( env, p_sys->circular.u.p_shortarray,
                                         i_data_offset / 2, i_data_size / 2,
                                         (jshort *)p_buffer->p_buffer
                                         + i_buffer_offset / 2);
            break;
        case WRITE_FLOATARRAY:
            i_data_offset &= ~3;
            i_data_size &= ~3;
            (*env)->SetFloatArrayRegion( env, p_sys->circular.u.p_floatarray,
                                         i_data_offset / 4, i_data_size / 4,
                                         (jfloat *)p_buffer->p_buffer
                                         + i_buffer_offset / 4);

            break;
        case WRITE_BYTEBUFFER:
            memcpy( p_sys->circular.u.bytebuffer.p_data + i_data_offset,
                    p_buffer->p_buffer + i_buffer_offset, i_data_size );
            break;
        }

        i_buffer_offset += i_data_size;
        p_sys->circular.i_write += i_data_size;

        if( !p_sys->b_thread_waiting )
            vlc_cond_signal( &p_sys->thread_cond );
    }

bailout:
    vlc_mutex_unlock( &p_sys->lock );
    block_Release( p_buffer );
}

static void
Pause( audio_output_t *p_aout, bool b_pause, mtime_t i_date )
{
    aout_sys_t *p_sys = p_aout->sys;
    JNIEnv *env;
    VLC_UNUSED( i_date );

    vlc_mutex_lock( &p_sys->lock );

    if( p_sys->b_error || !( env = GET_ENV() ) )
        goto bailout;

    if( b_pause )
    {
        p_sys->b_thread_paused = true;
        JNI_AT_CALL_VOID( pause );
        CHECK_AT_EXCEPTION( "pause" );
    } else
    {
        p_sys->b_thread_paused = false;
        AudioTrack_ResetPositions( env, p_aout );
        JNI_AT_CALL_VOID( play );
        CHECK_AT_EXCEPTION( "play" );
    }

bailout:
    vlc_mutex_unlock( &p_sys->lock );
}

static void
Flush( audio_output_t *p_aout, bool b_wait )
{
    aout_sys_t *p_sys = p_aout->sys;
    JNIEnv *env;

    vlc_mutex_lock( &p_sys->lock );

    if( p_sys->b_error || !( env = GET_ENV() ) )
        goto bailout;

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
        /* Wait for the thread to process the circular buffer */
        while( !p_sys->b_error
            && p_sys->circular.i_read != p_sys->circular.i_write )
            vlc_cond_wait( &p_sys->aout_cond, &p_sys->lock );
        if( p_sys->b_error )
            goto bailout;

        JNI_AT_CALL_VOID( stop );
        if( CHECK_AT_EXCEPTION( "stop" ) )
            goto bailout;
    } else
    {
        JNI_AT_CALL_VOID( pause );
        if( CHECK_AT_EXCEPTION( "pause" ) )
            goto bailout;
        JNI_AT_CALL_VOID( flush );
    }
    p_sys->circular.i_read = p_sys->circular.i_write = 0;

    /* HACK: Before Android 4.4, the head position is not reset to zero and is
     * still moving after a flush or a stop. This prevents to get a precise
     * head position and there is no way to know when it stabilizes. Therefore
     * recreate an AudioTrack object in that case. The AudioTimestamp class was
     * added in API Level 19, so if this class is not found, the Android
     * Version is 4.3 or before */
    if( !jfields.AudioTimestamp.clazz && p_sys->i_samples_written > 0 )
    {
        if( AudioTrack_Recreate( env, p_aout ) != 0 )
        {
            p_sys->b_error = true;
            goto bailout;
        }
    }
    AudioTrack_Reset( env, p_aout );
    JNI_AT_CALL_VOID( play );
    CHECK_AT_EXCEPTION( "play" );

bailout:
    vlc_mutex_unlock( &p_sys->lock );
}

static int DeviceSelect(audio_output_t *p_aout, const char *p_id)
{
    aout_sys_t *p_sys = p_aout->sys;
    enum at_dev at_dev = AT_DEV_DEFAULT;

    if( p_id )
    {
        for( unsigned int i = 0; at_devs[i].id; ++i )
        {
            if( strncmp( p_id, at_devs[i].id, strlen( at_devs[i].id ) ) == 0 )
            {
                at_dev = at_devs[i].at_dev;
                break;
            }
        }
    }

    long long i_encoding_flags = 0;
    if( at_dev == AT_DEV_ENCODED )
    {
        const size_t i_prefix_size = strlen( "encoded:" );
        if( strncmp( p_id, "encoded:", i_prefix_size ) == 0 )
            i_encoding_flags = atoll( p_id + i_prefix_size );
    }

    if( at_dev != p_sys->at_dev || i_encoding_flags != p_sys->i_encoding_flags )
    {
        p_sys->at_dev = at_dev;
        p_sys->i_encoding_flags = i_encoding_flags;
        aout_RestartRequest( p_aout, AOUT_RESTART_OUTPUT );
        msg_Dbg( p_aout, "selected device: %s", p_id );

        if( at_dev == AT_DEV_ENCODED )
        {
            static const vlc_fourcc_t enc_fourccs[] = {
                VLC_CODEC_DTS, VLC_CODEC_A52, VLC_CODEC_EAC3, VLC_CODEC_TRUEHD,
            };
            for( size_t i = 0;
                 i < sizeof( enc_fourccs ) / sizeof( enc_fourccs[0] ); ++i )
            {
                bool b_dtshd;
                if( AudioTrack_HasEncoding( p_aout, enc_fourccs[i], &b_dtshd ) )
                    msg_Dbg( p_aout, "device has %4.4s passthrough support",
                             b_dtshd ? "dtsh" : (const char *)&enc_fourccs[i] );
            }
        }
    }
    aout_DeviceReport( p_aout, p_id );
    return VLC_SUCCESS;
}

static int
Open( vlc_object_t *obj )
{
    audio_output_t *p_aout = (audio_output_t *) obj;
    aout_sys_t *p_sys;
    JNIEnv *env = GET_ENV();

    if( !env || !InitJNIFields( p_aout, env ) )
        return VLC_EGENERIC;

    p_sys = calloc( 1, sizeof (aout_sys_t) );

    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_sys->at_dev = AT_DEV_DEFAULT;
    vlc_mutex_init(&p_sys->lock);
    vlc_cond_init(&p_sys->aout_cond);
    vlc_cond_init(&p_sys->thread_cond);

    p_aout->sys = p_sys;
    p_aout->start = Start;
    p_aout->stop = Stop;
    p_aout->play = Play;
    p_aout->pause = Pause;
    p_aout->flush = Flush;
    p_aout->time_get = TimeGet;
    p_aout->device_select = DeviceSelect;

    for( unsigned int i = 0; at_devs[i].id; ++i )
        aout_HotplugReport(p_aout, at_devs[i].id, at_devs[i].name);

    aout_SoftVolumeInit( p_aout );

    return VLC_SUCCESS;
}

static void
Close( vlc_object_t *obj )
{
    audio_output_t *p_aout = (audio_output_t *) obj;
    aout_sys_t *p_sys = p_aout->sys;

    vlc_mutex_destroy(&p_sys->lock);
    vlc_cond_destroy(&p_sys->aout_cond);
    vlc_cond_destroy(&p_sys->thread_cond);
    free( p_sys );
}
