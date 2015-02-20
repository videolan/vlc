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
#include <sys/queue.h>

#include <vlc_atomic.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_threads.h>

/* Maximum VLC buffers queued by the internal queue in microseconds. This delay
 * doesn't include audiotrack delay */
#define MAX_QUEUE_US INT64_C(1000000) // 1000ms

static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

struct thread_cmd;
typedef TAILQ_HEAD(, thread_cmd) THREAD_CMD_QUEUE;

struct aout_sys_t {
    /* sw gain */
    float soft_gain;
    bool soft_mute;

    /* Owned by JNIThread */
    jobject p_audiotrack; /* AudioTrack ref */
    jobject p_audioTimestamp; /* AudioTimestamp ref */
    jbyteArray p_bytearray; /* ByteArray ref */
    size_t i_bytearray_size; /* size of the ByteArray */
    audio_sample_format_t fmt; /* fmt setup by Start */
    uint32_t i_pos_initial; /* initial position set by getPlaybackHeadPosition */
    uint32_t i_samples_written; /* number of samples written since last flush */
    mtime_t i_play_time; /* time when play was called */

    /* JNIThread control */
    vlc_mutex_t mutex;
    vlc_cond_t cond;
    vlc_thread_t thread;

    /* Shared between two threads, must be locked */
    bool b_thread_run; /* is thread alive */
    THREAD_CMD_QUEUE thread_cmd_queue; /* thread cmd queue */
    uint32_t i_samples_queued; /* number of samples queued */
    uint32_t i_audiotrack_delay; /* audiotrack delay in samples */
};

/* Soft volume helper */
#include "audio_output/volume.h"

//#define AUDIOTRACK_USE_FLOAT

vlc_module_begin ()
    set_shortname( "AudioTrack" )
    set_description( N_( "Android AudioTrack audio output" ) )
    set_capability( "audio output", 180 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_sw_gain()
    add_shortcut( "audiotrack" )
    set_callbacks( Open, Close )
vlc_module_end ()

struct thread_cmd
{
    TAILQ_ENTRY(thread_cmd) next;
    enum {
        CMD_START,
        CMD_STOP,
        CMD_PLAY,
        CMD_PAUSE,
        CMD_FLUSH,
        CMD_DONE,
    } id;
    union {
        struct {
            audio_sample_format_t *p_fmt;
        } start;
        struct {
            block_t *p_buffer;
        } play;
        struct {
            bool b_pause;
            mtime_t i_date;
        } pause;
        struct {
            bool b_wait;
        } flush;
    } in;
    union {
        struct {
            int i_ret;
            audio_sample_format_t *p_fmt;
        } start;
    } out;
    void ( *pf_destroy )( struct thread_cmd * );
};

#define THREAD_NAME "android_audiotrack"

extern int jni_attach_thread(JNIEnv **env, const char *thread_name);
extern void jni_detach_thread();
extern int jni_get_env(JNIEnv **env);

static struct
{
    struct {
        jclass clazz;
        jmethodID ctor;
        jmethodID release;
        jmethodID play;
        jmethodID stop;
        jmethodID flush;
        jmethodID pause;
        jmethodID write;
        jmethodID getPlaybackHeadPosition;
        jmethodID getTimestamp;
        jmethodID getMinBufferSize;
        jint MODE_STREAM;
        jint ERROR;
        jint ERROR_BAD_VALUE;
        jint ERROR_INVALID_OPERATION;
    } AudioTrack;
    struct {
        jint ENCODING_PCM_8BIT;
        jint ENCODING_PCM_16BIT;
        jint ENCODING_PCM_FLOAT;
        bool has_ENCODING_PCM_FLOAT;
        jint CHANNEL_OUT_MONO;
        jint CHANNEL_OUT_STEREO;
    } AudioFormat;
    struct {
        jint ERROR_DEAD_OBJECT;
        bool has_ERROR_DEAD_OBJECT;
        jint STREAM_MUSIC;
    } AudioManager;
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
    bool ret, b_attached = false;
    jclass clazz;
    jfieldID field;
    JNIEnv* env = NULL;

    vlc_mutex_lock( &lock );

    if( i_init_state != -1 )
        goto end;

    if( jni_get_env(&env) < 0 )
    {
        jni_attach_thread( &env, THREAD_NAME );
        if( !env )
        {
            i_init_state = 0;
            goto end;
        }
        b_attached = true;
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
    GET_ID( GetMethodID, AudioTrack.play, "play", "()V", true );
    GET_ID( GetMethodID, AudioTrack.stop, "stop", "()V", true );
    GET_ID( GetMethodID, AudioTrack.flush, "flush", "()V", true );
    GET_ID( GetMethodID, AudioTrack.pause, "pause", "()V", true );
    GET_ID( GetMethodID, AudioTrack.write, "write", "([BII)I", true );

    GET_ID( GetMethodID, AudioTrack.getTimestamp,
            "getTimestamp", "(Landroid/media/AudioTimestamp;)Z", false );
    GET_ID( GetMethodID, AudioTrack.getPlaybackHeadPosition,
            "getPlaybackHeadPosition", "()I", true );

    GET_ID( GetStaticMethodID, AudioTrack.getMinBufferSize, "getMinBufferSize",
            "(III)I", true );
    GET_CONST_INT( AudioTrack.MODE_STREAM, "MODE_STREAM", true );
    GET_CONST_INT( AudioTrack.ERROR, "ERROR", true );
    GET_CONST_INT( AudioTrack.ERROR_BAD_VALUE , "ERROR_BAD_VALUE", true );
    GET_CONST_INT( AudioTrack.ERROR_INVALID_OPERATION ,
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
    } else
    {
        jfields.AudioTimestamp.clazz = NULL;
        jfields.AudioTimestamp.ctor = NULL;
        jfields.AudioTimestamp.framePosition = NULL;
        jfields.AudioTimestamp.nanoTime = NULL;
    }

    /* AudioFormat class init */
    GET_CLASS( "android/media/AudioFormat", true );
    GET_CONST_INT( AudioFormat.ENCODING_PCM_8BIT, "ENCODING_PCM_8BIT", true );
    GET_CONST_INT( AudioFormat.ENCODING_PCM_16BIT, "ENCODING_PCM_16BIT", true );
#ifdef AUDIOTRACK_USE_FLOAT
    GET_CONST_INT( AudioFormat.ENCODING_PCM_FLOAT, "ENCODING_PCM_FLOAT",
                   false );
    jfields.AudioFormat.has_ENCODING_PCM_FLOAT = field != NULL;
#else
    jfields.AudioFormat.has_ENCODING_PCM_FLOAT = false;
#endif
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_MONO, "CHANNEL_OUT_MONO", true );
    GET_CONST_INT( AudioFormat.CHANNEL_OUT_STEREO, "CHANNEL_OUT_STEREO", true );

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
    if( b_attached )
        jni_detach_thread();
    vlc_mutex_unlock( &lock );
    return ret;
}

static inline bool
check_exception( JNIEnv *env, bool *p_error, audio_output_t *p_aout,
                 const char *method )
{
    if( (*env)->ExceptionOccurred( env ) )
    {
        (*env)->ExceptionClear( env );
        *p_error = true;
        msg_Err( p_aout, "AudioTrack.%s triggered an exception !", method );
        return true;
    } else
        return false;
}
#define CHECK_EXCEPTION( method ) check_exception( env, p_error, p_aout, method )

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

#define JNI_AUDIOTIMESTAMP_GET_LONG( field ) JNI_CALL( GetLongField, p_sys->p_audioTimestamp, jfields.AudioTimestamp.field )

static inline mtime_t
frames_to_us( aout_sys_t *p_sys, uint32_t i_nb_frames )
{
    return  i_nb_frames * CLOCK_FREQ / p_sys->fmt.i_rate;
}
#define FRAMES_TO_US(x) frames_to_us( p_sys, (x) )

static struct thread_cmd *
ThreadCmd_New( int id )
{
    struct thread_cmd *p_cmd = calloc( 1, sizeof(struct thread_cmd) );

    if( p_cmd )
        p_cmd->id = id;

    return p_cmd;
}

static void
ThreadCmd_InsertHead( aout_sys_t *p_sys, struct thread_cmd *p_cmd )
{
    TAILQ_INSERT_HEAD( &p_sys->thread_cmd_queue, p_cmd, next);
    vlc_cond_signal( &p_sys->cond );
}

static void
ThreadCmd_InsertTail( aout_sys_t *p_sys, struct thread_cmd *p_cmd )
{
    TAILQ_INSERT_TAIL( &p_sys->thread_cmd_queue, p_cmd, next);
    vlc_cond_signal( &p_sys->cond );
}

static bool
ThreadCmd_Wait( aout_sys_t *p_sys, struct thread_cmd *p_cmd )
{
    while( p_cmd->id != CMD_DONE && p_sys->b_thread_run  )
        vlc_cond_wait( &p_sys->cond, &p_sys->mutex );

    return p_cmd->id == CMD_DONE;
}

static void
ThreadCmd_FlushQueue( aout_sys_t *p_sys )
{
    struct thread_cmd *p_cmd, *p_cmd_next;

    for ( p_cmd = TAILQ_FIRST( &p_sys->thread_cmd_queue );
          p_cmd != NULL; p_cmd = p_cmd_next )
    {
        p_cmd_next = TAILQ_NEXT( p_cmd, next );
        TAILQ_REMOVE( &p_sys->thread_cmd_queue, p_cmd, next );
        if( p_cmd->pf_destroy )
            p_cmd->pf_destroy( p_cmd );
    }
}

static void
JNIThread_InitDelay( JNIEnv *env, audio_output_t *p_aout, uint32_t *p_delay )
{
    aout_sys_t *p_sys = p_aout->sys;

    p_sys->i_pos_initial = JNI_AT_CALL_INT( getPlaybackHeadPosition );

    /* HACK: On some broken devices, head position is still moving after a
     * flush or a stop. So, wait for the head position to be stabilized. */
    if( unlikely( p_sys->i_pos_initial != 0 ) )
    {
        uint32_t i_last_pos;
        do {
            i_last_pos = p_sys->i_pos_initial;
            msleep( 50000 );
            p_sys->i_pos_initial = JNI_AT_CALL_INT( getPlaybackHeadPosition );
        } while( p_sys->i_pos_initial != i_last_pos );
    }
    p_sys->i_samples_written = 0;
    *p_delay = 0;
}

static void
JNIThread_SetDelay( JNIEnv *env, audio_output_t *p_aout, uint32_t *p_delay )
{
    aout_sys_t *p_sys = p_aout->sys;
    bool b_frame_delay_set = false;
    jlong i_frame_pos;
    mtime_t i_current_time = mdate();

    if( p_sys->p_audioTimestamp )
    {
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

        if( JNI_AT_CALL_BOOL( getTimestamp, p_sys->p_audioTimestamp ) )
        {
            jlong i_frame_time = JNI_AUDIOTIMESTAMP_GET_LONG( nanoTime ) / 1000;
            /* frame time should be after last play time
             * frame time shouldn't be in the future
             * frame time should be less than 10 seconds old */
            if( i_frame_time > p_sys->i_play_time
                && i_current_time > i_frame_time
                && ( i_current_time - i_frame_time ) <= INT64_C(10000000) )
            {
                jlong i_time_diff = i_current_time - i_frame_time;
                jlong i_frames_diff = i_time_diff *  p_sys->fmt.i_rate
                                      / CLOCK_FREQ;
                i_frame_pos = JNI_AUDIOTIMESTAMP_GET_LONG( framePosition )
                              + i_frames_diff;
                b_frame_delay_set = true;
            }
        }
    }
    if( !b_frame_delay_set )
    {
        /* Android doc:
         * getPlaybackHeadPosition: Returns the playback head position
         * expressed in frames. Though the "int" type is signed 32-bits, the
         * value should be reinterpreted as if it is unsigned 32-bits. That is,
         * the next position after 0x7FFFFFFF is (int) 0x80000000. This is a
         * continuously advancing counter. It will wrap (overflow)
         * periodically, for example approximately once every 27:03:11
         * hours:minutes:seconds at 44.1 kHz. It is reset to zero by flush(),
         * reload(), and stop().
         */

        uint32_t i_head_pos = JNI_AT_CALL_INT( getPlaybackHeadPosition );
        i_frame_pos = i_head_pos - p_sys->i_pos_initial;
        b_frame_delay_set = true;
    }

    if( b_frame_delay_set && p_sys->i_samples_written > i_frame_pos )
        *p_delay = p_sys->i_samples_written - i_frame_pos;
}

static int
JNIThread_Start( JNIEnv *env, bool *p_error, audio_output_t *p_aout )
{
    struct aout_sys_t *p_sys = p_aout->sys;
    int i_size, i_min_buffer_size, i_channel_config, i_rate, i_format,
        i_format_size, i_nb_channels;
    jobject p_audiotrack;

    /* 4000 <= frequency <= 48000 */
    i_rate = p_sys->fmt.i_rate;
    if( i_rate < 4000 )
        i_rate = 4000;
    if( i_rate > 48000 )
        i_rate = 48000;

    /* We can only accept U8, S16N, and FL32 (depending on Android version) */
    if( p_sys->fmt.i_format != VLC_CODEC_U8
        && p_sys->fmt.i_format != VLC_CODEC_S16N
        && p_sys->fmt.i_format != VLC_CODEC_FL32 )
        p_sys->fmt.i_format = VLC_CODEC_S16N;

    if( p_sys->fmt.i_format == VLC_CODEC_FL32
        && !jfields.AudioFormat.has_ENCODING_PCM_FLOAT )
        p_sys->fmt.i_format = VLC_CODEC_S16N;

    if( p_sys->fmt.i_format == VLC_CODEC_S16N )
    {
        i_format = jfields.AudioFormat.ENCODING_PCM_16BIT;
        i_format_size = 2;
    } else if( p_sys->fmt.i_format == VLC_CODEC_FL32 )
    {
        i_format = jfields.AudioFormat.ENCODING_PCM_FLOAT;
        i_format_size = 4;
    } else
    {
        i_format = jfields.AudioFormat.ENCODING_PCM_8BIT;
        i_format_size = 1;
    }
    p_sys->fmt.i_original_channels = p_sys->fmt.i_physical_channels;

    i_nb_channels = aout_FormatNbChannels( &p_sys->fmt );
    switch( i_nb_channels )
    {
    case 1:
        i_channel_config = jfields.AudioFormat.CHANNEL_OUT_MONO;
        p_sys->fmt.i_physical_channels = AOUT_CHAN_CENTER;
        break;
    default:
        i_nb_channels = 2; // XXX: AudioTrack handle only stereo for now
    case 2:
        i_channel_config = jfields.AudioFormat.CHANNEL_OUT_STEREO;
        p_sys->fmt.i_physical_channels = AOUT_CHANS_STEREO;
        break;
    }

    i_min_buffer_size = JNI_AT_CALL_STATIC_INT( getMinBufferSize, i_rate,
                                                i_channel_config, i_format );
    if( i_min_buffer_size <= 0 )
    {
        msg_Warn( p_aout, "getMinBufferSize returned an invalid size" ) ;
        /* use a defaut min buffer size (shouldn't happen) */
        i_min_buffer_size = i_nb_channels * i_format_size * 2024;
    }

    i_size = i_min_buffer_size * 2; // double buffering

    /* create AudioTrack object */
    p_audiotrack = JNI_AT_NEW( jfields.AudioManager.STREAM_MUSIC, i_rate,
                               i_channel_config, i_format, i_size,
                               jfields.AudioTrack.MODE_STREAM );
    if( CHECK_EXCEPTION( "AudioTrack<init>" ) || !p_audiotrack )
        return VLC_EGENERIC;
    p_sys->p_audiotrack = (*env)->NewGlobalRef( env, p_audiotrack );
    (*env)->DeleteLocalRef( env, p_audiotrack );
    if( !p_sys->p_audiotrack )
        return VLC_EGENERIC;

    if( jfields.AudioTimestamp.clazz )
    {
        /* create AudioTimestamp object */
        jobject p_audioTimestamp = JNI_CALL( NewObject,
                                             jfields.AudioTimestamp.clazz,
                                             jfields.AudioTimestamp.ctor );
        if( CHECK_EXCEPTION( "AudioTimestamp<init>" ) || !p_audioTimestamp )
            goto error;
        p_sys->p_audioTimestamp = (*env)->NewGlobalRef( env, p_audioTimestamp );
        (*env)->DeleteLocalRef( env, p_audioTimestamp );
        if( !p_sys->p_audioTimestamp )
            goto error;
    }

    p_sys->fmt.i_rate = i_rate;

    JNI_AT_CALL_VOID( play );
    CHECK_EXCEPTION( "play" );
    p_sys->i_play_time = mdate();

    return VLC_SUCCESS;
error:
    if( p_sys->p_audiotrack )
    {
        JNI_AT_CALL_VOID( release );
        (*env)->DeleteGlobalRef( env, p_sys->p_audiotrack );
        p_sys->p_audiotrack = NULL;
    }
    return VLC_EGENERIC;
}

static void
JNIThread_Stop( JNIEnv *env, bool *p_error, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    JNI_AT_CALL_VOID( stop );
    CHECK_EXCEPTION( "stop" );

    JNI_AT_CALL_VOID( release );
    (*env)->DeleteGlobalRef( env, p_sys->p_audiotrack );
    p_sys->p_audiotrack = NULL;
}

static void
JNIThread_Play( JNIEnv *env, bool *p_error, audio_output_t *p_aout,
                block_t *p_buffer )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_offset = 0;

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
    {
        *p_error = true;
        return;
    }

    /* copy p_buffer in to ByteArray */
    (*env)->SetByteArrayRegion( env, p_sys->p_bytearray, 0,
                                p_buffer->i_buffer,
                                (jbyte *)p_buffer->p_buffer);

    while ( p_buffer->i_buffer > (unsigned int) i_offset )
    {
        int i_ret;

        /* write ByteArray */
        i_ret = JNI_AT_CALL_INT( write, p_sys->p_bytearray, i_offset,
                                 p_buffer->i_buffer - i_offset);
        if( i_ret < 0 ) {
            if( jfields.AudioManager.has_ERROR_DEAD_OBJECT
                && i_ret == jfields.AudioManager.ERROR_DEAD_OBJECT )
            {
                msg_Warn( p_aout, "ERROR_DEAD_OBJECT: "
                                  "try recreating AudioTrack" );
                JNIThread_Stop( env, p_error, p_aout );
                i_ret = JNIThread_Start( env, p_error, p_aout );
                if( i_ret == VLC_SUCCESS )
                    continue;
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
            *p_error = true;
            break;
        }

        i_offset += i_ret;
    }
    p_sys->i_samples_written += p_buffer->i_nb_samples;
}

static void
JNIThread_Pause( JNIEnv *env, bool *p_error, audio_output_t *p_aout,
                 bool b_pause, mtime_t i_date )
{
    VLC_UNUSED( i_date );

    aout_sys_t *p_sys = p_aout->sys;

    if( b_pause )
    {
        JNI_AT_CALL_VOID( pause );
        CHECK_EXCEPTION( "pause" );
    } else
    {
        JNI_AT_CALL_VOID( play );
        CHECK_EXCEPTION( "play" );
        p_sys->i_play_time = mdate();
    }
}

static void
JNIThread_Flush( JNIEnv *env, bool *p_error, audio_output_t *p_aout,
                 bool b_wait )
{
    aout_sys_t *p_sys = p_aout->sys;

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
        if( CHECK_EXCEPTION( "stop" ) )
            return;
    } else
    {
        JNI_AT_CALL_VOID( pause );
        if( CHECK_EXCEPTION( "pause" ) )
            return;
        JNI_AT_CALL_VOID( flush );
    }
    JNI_AT_CALL_VOID( play );
    CHECK_EXCEPTION( "play" );
    p_sys->i_play_time = mdate();
}

static void *
JNIThread( void *data )
{
    audio_output_t *p_aout = data;
    aout_sys_t *p_sys = p_aout->sys;
    bool b_error = false;
    bool b_paused = false;
    uint32_t i_audiotrack_delay = 0;
    JNIEnv* env;

    jni_attach_thread( &env, THREAD_NAME );

    vlc_mutex_lock( &p_sys->mutex );
    if( !env )
        goto end;

    while( p_sys->b_thread_run )
    {
        struct thread_cmd *p_cmd;

        /* wait to process a command */
        while( ( p_cmd = TAILQ_FIRST( &p_sys->thread_cmd_queue ) ) == NULL
               && p_sys->b_thread_run )
            vlc_cond_wait( &p_sys->cond, &p_sys->mutex );

        if( !p_sys->b_thread_run || p_cmd == NULL )
            break;

        if( b_paused && p_cmd->id == CMD_PLAY )
        {
            vlc_cond_wait( &p_sys->cond, &p_sys->mutex );
            continue;
        }

        TAILQ_REMOVE( &p_sys->thread_cmd_queue, p_cmd, next );

        if( p_cmd->id == CMD_PLAY )
        {
            p_sys->i_samples_queued -= p_cmd->in.play.p_buffer->i_nb_samples;
            vlc_cond_signal( &p_sys->cond );
        }

        vlc_mutex_unlock( &p_sys->mutex );
        /* process a command */
        switch( p_cmd->id )
        {
            case CMD_START:
                p_sys->fmt = *p_cmd->in.start.p_fmt;
                p_cmd->out.start.i_ret =
                        JNIThread_Start( env, &b_error, p_aout );
                JNIThread_InitDelay( env, p_aout, &i_audiotrack_delay );
                p_cmd->out.start.p_fmt = &p_sys->fmt;
                b_paused = false;
                break;
            case CMD_STOP:
                JNIThread_Stop( env, &b_error, p_aout );
                b_paused = false;
                break;
            case CMD_PLAY:
                JNIThread_Play( env, &b_error, p_aout,
                                p_cmd->in.play.p_buffer );
                JNIThread_SetDelay( env, p_aout, &i_audiotrack_delay );
                break;
            case CMD_PAUSE:
                JNIThread_Pause( env, &b_error, p_aout,
                                 p_cmd->in.pause.b_pause,
                                 p_cmd->in.pause.i_date );
                b_paused = p_cmd->in.pause.b_pause;
                break;
            case CMD_FLUSH:
                JNIThread_Flush( env, &b_error, p_aout,
                                 p_cmd->in.flush.b_wait );
                JNIThread_InitDelay( env, p_aout, &i_audiotrack_delay );
                break;
            default:
                vlc_assert_unreachable();
        }

        vlc_mutex_lock( &p_sys->mutex );

        p_sys->i_audiotrack_delay = i_audiotrack_delay;

        p_cmd->id = CMD_DONE;
        if( p_cmd->pf_destroy )
            p_cmd->pf_destroy( p_cmd );

        if( b_error )
            p_sys->b_thread_run = false;

        /* signal that command is processed */
        vlc_cond_signal( &p_sys->cond );
    }
end:
    if( env )
    {
        if( p_sys->p_bytearray )
            (*env)->DeleteGlobalRef( env, p_sys->p_bytearray );
        jni_detach_thread();
    }
    p_sys->b_thread_run = false;
    vlc_cond_signal( &p_sys->cond );
    vlc_mutex_unlock( &p_sys->mutex );
    return NULL;
}

static int
Start( audio_output_t *p_aout, audio_sample_format_t *restrict p_fmt )
{
    int i_ret = VLC_EGENERIC;
    struct thread_cmd *p_cmd;
    aout_sys_t *p_sys = p_aout->sys;

    vlc_mutex_lock( &p_sys->mutex );

    assert( !p_sys->b_thread_run );

    /* create JNIThread */
    p_sys->b_thread_run = true;
    if( vlc_clone( &p_sys->thread,
                   JNIThread, p_aout, VLC_THREAD_PRIORITY_AUDIO ) )
    {
        msg_Err( p_aout, "JNIThread creation failed" );
        vlc_mutex_unlock( &p_sys->mutex );
        return VLC_EGENERIC;
    }

    p_cmd = ThreadCmd_New( CMD_START );
    if( p_cmd )
    {
        /* ask the thread to process the Start command */
        p_cmd->in.start.p_fmt = p_fmt;

        ThreadCmd_InsertHead( p_sys, p_cmd );
        if( ThreadCmd_Wait( p_sys, p_cmd ) )
        {
            i_ret = p_cmd->out.start.i_ret;
            if( i_ret == VLC_SUCCESS )
                *p_fmt = *p_cmd->out.start.p_fmt;
        }
        free( p_cmd );
    }
    vlc_mutex_unlock( &p_sys->mutex );

    if( i_ret == VLC_SUCCESS )
        aout_SoftVolumeStart( p_aout );

    return i_ret;
}

static void
Stop( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    vlc_mutex_lock( &p_sys->mutex );

    if( p_sys->b_thread_run )
    {
        struct thread_cmd *p_cmd;

        p_sys->i_samples_queued = 0;
        ThreadCmd_FlushQueue( p_sys );

        p_cmd = ThreadCmd_New( CMD_STOP );
        if( p_cmd )
        {
            /* ask the thread to process the Stop command */
            ThreadCmd_InsertHead( p_sys, p_cmd );
            ThreadCmd_Wait( p_sys, p_cmd );

            free( p_cmd );
        }
        /* kill the thread */
        p_sys->b_thread_run = false;
        vlc_cond_signal( &p_sys->cond );
    }
    vlc_mutex_unlock( &p_sys->mutex );

    vlc_join( p_sys->thread, NULL );
}

static void
PlayCmd_Destroy( struct thread_cmd *p_cmd )
{
    block_Release( p_cmd->in.play.p_buffer );
    free( p_cmd );
}

static void
Play( audio_output_t *p_aout, block_t *p_buffer )
{
    aout_sys_t *p_sys = p_aout->sys;

    vlc_mutex_lock( &p_sys->mutex );

    if( p_sys->b_thread_run )
    {
        struct thread_cmd *p_cmd;

        while( p_sys->i_samples_queued != 0 && p_sys->b_thread_run
               && FRAMES_TO_US( p_sys->i_samples_queued +
                                p_buffer->i_nb_samples ) >= MAX_QUEUE_US )
            vlc_cond_wait( &p_sys->cond, &p_sys->mutex );

        p_cmd = ThreadCmd_New( CMD_PLAY );

        if( p_cmd )
        {
            /* ask the thread to process the Play command */
            p_cmd->in.play.p_buffer = p_buffer;
            p_cmd->pf_destroy = PlayCmd_Destroy;

            ThreadCmd_InsertTail( p_sys, p_cmd );

            p_sys->i_samples_queued += p_buffer->i_nb_samples;
        } else
             block_Release( p_cmd->in.play.p_buffer );
    }
    vlc_mutex_unlock( &p_sys->mutex );
}

static void
Pause( audio_output_t *p_aout, bool b_pause, mtime_t i_date )
{
    aout_sys_t *p_sys = p_aout->sys;

    vlc_mutex_lock( &p_sys->mutex );

    if( p_sys->b_thread_run )
    {
        struct thread_cmd *p_cmd = ThreadCmd_New( CMD_PAUSE );

        if( p_cmd )
        {
            /* ask the thread to process the Pause command */
            p_cmd->in.pause.b_pause = b_pause;
            p_cmd->in.pause.i_date = i_date;

            ThreadCmd_InsertHead( p_sys, p_cmd );
            ThreadCmd_Wait( p_sys, p_cmd );

            free( p_cmd );
        }
    }
    vlc_mutex_unlock( &p_sys->mutex );
}

static void
Flush( audio_output_t *p_aout, bool b_wait )
{
    aout_sys_t *p_sys = p_aout->sys;

    vlc_mutex_lock( &p_sys->mutex );

    if( p_sys->b_thread_run )
    {
        struct thread_cmd *p_cmd;

        p_sys->i_samples_queued = 0;
        ThreadCmd_FlushQueue( p_sys );

        p_cmd = ThreadCmd_New( CMD_FLUSH );
        if( p_cmd)
        {
            /* ask the thread to process the Flush command */
            p_cmd->in.flush.b_wait = b_wait;

            ThreadCmd_InsertHead( p_sys, p_cmd );
            ThreadCmd_Wait( p_sys, p_cmd );

            free( p_cmd );
        }
    }
    vlc_mutex_unlock( &p_sys->mutex );
}

static int
TimeGet( audio_output_t *p_aout, mtime_t *restrict p_delay )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_ret;

    vlc_mutex_lock( &p_sys->mutex );
    if( p_sys->i_samples_queued != 0 )
    {
        *p_delay = FRAMES_TO_US( p_sys->i_samples_queued +
                                 p_sys->i_audiotrack_delay );
        i_ret = 0;
    } else
        i_ret = -1;
    vlc_mutex_unlock( &p_sys->mutex );

    return i_ret;
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

    vlc_mutex_init( &p_sys->mutex );
    vlc_cond_init( &p_sys->cond );
    TAILQ_INIT( &p_sys->thread_cmd_queue );

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

    vlc_mutex_destroy( &p_sys->mutex );
    vlc_cond_destroy( &p_sys->cond );

    free( p_sys );
}
