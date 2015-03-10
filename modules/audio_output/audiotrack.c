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
    jbyteArray p_bytearray; /* ByteArray ref (for Write) */
    size_t i_bytearray_size; /* size of the ByteArray */
    jobject p_bytebuffer; /* ByteBuffer ref (for WriteV21) */
    audio_sample_format_t fmt; /* fmt setup by Start */
    uint32_t i_pos_initial; /* initial position set by getPlaybackHeadPosition */
    uint32_t i_samples_written; /* number of samples written since last flush */
    uint32_t i_bytes_per_frame; /* byte per frame */
    uint32_t i_max_audiotrack_samples;
    mtime_t i_play_time; /* time when play was called */
    bool b_audiotrack_exception; /* true if audiotrack throwed an exception */
    int i_audiotrack_stuck_count;
    uint8_t i_chans_to_reorder; /* do we need channel reordering */
    uint8_t p_chan_table[AOUT_CHAN_MAX];

    /* JNIThread control */
    vlc_mutex_t mutex;
    vlc_cond_t cond;
    vlc_thread_t thread;

    /* Shared between two threads, must be locked */
    bool b_thread_run; /* is thread alive */
    THREAD_CMD_QUEUE thread_cmd_queue; /* thread cmd queue */
    uint32_t i_samples_queued; /* number of samples queued */
};

/* Soft volume helper */
#include "audio_output/volume.h"

//#define AUDIOTRACK_USE_FLOAT
// TODO: activate getTimestamp for new android versions
//#define AUDIOTRACK_USE_TIMESTAMP

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
        CMD_TIME_GET,
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
        struct {
            int i_ret;
            mtime_t i_delay;
        } time_get;
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
        jmethodID getState;
        jmethodID play;
        jmethodID stop;
        jmethodID flush;
        jmethodID pause;
        jmethodID write;
        jmethodID writeV21;
        jmethodID getPlaybackHeadPosition;
        jmethodID getTimestamp;
        jmethodID getMinBufferSize;
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
        jint CHANNEL_OUT_MONO;
        jint CHANNEL_OUT_STEREO;
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
    GET_ID( GetMethodID, AudioTrack.getState, "getState", "()I", true );
    GET_ID( GetMethodID, AudioTrack.play, "play", "()V", true );
    GET_ID( GetMethodID, AudioTrack.stop, "stop", "()V", true );
    GET_ID( GetMethodID, AudioTrack.flush, "flush", "()V", true );
    GET_ID( GetMethodID, AudioTrack.pause, "pause", "()V", true );

    GET_ID( GetMethodID, AudioTrack.writeV21, "write", "(Ljava/nio/ByteBuffer;II)I", false );
    if( jfields.AudioTrack.writeV21 )
    {
        jfields.AudioTrack.write = NULL;
        GET_CONST_INT( AudioTrack.WRITE_NON_BLOCKING, "WRITE_NON_BLOCKING", true );
    } else
        GET_ID( GetMethodID, AudioTrack.write, "write", "([BII)I", true );

    GET_ID( GetMethodID, AudioTrack.getTimestamp,
            "getTimestamp", "(Landroid/media/AudioTimestamp;)Z", false );
    GET_ID( GetMethodID, AudioTrack.getPlaybackHeadPosition,
            "getPlaybackHeadPosition", "()I", true );

    GET_ID( GetStaticMethodID, AudioTrack.getMinBufferSize, "getMinBufferSize",
            "(III)I", true );
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
    if( b_attached )
        jni_detach_thread();
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
    while( p_cmd->id != CMD_DONE )
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
JNIThread_InitDelay( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    if( p_sys->p_audiotrack )
        p_sys->i_pos_initial = JNI_AT_CALL_INT( getPlaybackHeadPosition );
    else
        p_sys->i_pos_initial = 0;

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
    p_sys->i_samples_queued = 0;
}

static uint32_t
JNIThread_GetAudioTrackPos( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    /* Android doc:
     * getPlaybackHeadPosition: Returns the playback head position expressed in
     * frames. Though the "int" type is signed 32-bits, the value should be
     * reinterpreted as if it is unsigned 32-bits. That is, the next position
     * after 0x7FFFFFFF is (int) 0x80000000. This is a continuously advancing
     * counter. It will wrap (overflow) periodically, for example approximately
     * once every 27:03:11 hours:minutes:seconds at 44.1 kHz. It is reset to
     * zero by flush(), reload(), and stop().
     */

    return JNI_AT_CALL_INT( getPlaybackHeadPosition ) - p_sys->i_pos_initial;
}

static int
JNIThread_TimeGet( JNIEnv *env, audio_output_t *p_aout, mtime_t *p_delay )
{
    aout_sys_t *p_sys = p_aout->sys;
    jlong i_frame_pos;
    uint32_t i_audiotrack_delay = 0;

    if( p_sys->i_samples_queued == 0 )
        return -1;
    if( p_sys->p_audioTimestamp )
    {
        mtime_t i_current_time = mdate();
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
                if( p_sys->i_samples_written > i_frame_pos )
                    i_audiotrack_delay =  p_sys->i_samples_written - i_frame_pos;
            }
        }
    }
    if( i_audiotrack_delay == 0 )
    {
        uint32_t i_audiotrack_pos = JNIThread_GetAudioTrackPos( env, p_aout );

        if( p_sys->i_samples_written > i_audiotrack_pos )
            i_audiotrack_delay = p_sys->i_samples_written - i_audiotrack_pos;
    }

    if( i_audiotrack_delay > 0 )
    {
        *p_delay = FRAMES_TO_US( p_sys->i_samples_queued + i_audiotrack_delay );
        return 0;
    } else
        return -1;
}

static bool
AudioTrack_GetChanOrder( int i_mask, uint32_t p_chans_out[] )
{
#define HAS_CHAN( x ) ( ( i_mask & (jfields.AudioFormat.x) ) == (jfields.AudioFormat.x) )
    const int i_sides = jfields.AudioFormat.CHANNEL_OUT_SIDE_LEFT |
                        jfields.AudioFormat.CHANNEL_OUT_SIDE_RIGHT;
    const int i_backs = jfields.AudioFormat.CHANNEL_OUT_BACK_LEFT |
                        jfields.AudioFormat.CHANNEL_OUT_BACK_RIGHT;

    /* verify has FL/FR */
    if ( !HAS_CHAN( CHANNEL_OUT_STEREO ) )
        return false;

    /* verify uses SIDE as a pair (ok if not using SIDE at all) */
    bool b_has_sides = false;
    if( jfields.AudioFormat.has_CHANNEL_OUT_SIDE && ( i_mask & i_sides ) != 0 )
    {
        if( ( i_mask & i_sides ) != i_sides )
            return false;
        b_has_sides = true;
    }
    /* verify uses BACK as a pair (ok if not using BACK at all) */
    bool b_has_backs = false;
    if( ( i_mask & i_backs ) != 0 )
    {
        if( ( i_mask & i_backs ) != i_backs )
            return false;
        b_has_backs = true;
    }

    const bool b_has_FC   = HAS_CHAN( CHANNEL_OUT_FRONT_CENTER );
    const bool b_has_LFE  = HAS_CHAN( CHANNEL_OUT_LOW_FREQUENCY );
    const bool b_has_BC   = HAS_CHAN( CHANNEL_OUT_BACK_CENTER );

    /* compute at what index each channel is: samples will be in the following
     * order: FL FR FC LFE BL BR BC SL SR
     * when a channel is not present, its index is set to the same as the index
     * of the preceding channel. */

    const int i_FC  = b_has_FC    ? 2           : 1;
    const int i_LFE = b_has_LFE   ? i_FC + 1    : i_FC;
    const int i_BL  = b_has_backs ? i_LFE + 1   : i_LFE;
    const int i_BR  = b_has_backs ? i_BL + 1    : i_BL;
    const int i_BC  = b_has_BC    ? i_BR + 1    : i_BR;
    const int i_SL  = b_has_sides ? i_BC + 1    : i_BC;
    const int i_SR  = b_has_sides ? i_SL + 1    : i_SL;

    p_chans_out[0] = AOUT_CHAN_LEFT;
    p_chans_out[1] = AOUT_CHAN_RIGHT;
    if( b_has_FC )
        p_chans_out[i_FC] = AOUT_CHAN_CENTER;
    if( b_has_LFE )
        p_chans_out[i_LFE] = AOUT_CHAN_LFE;
    if( b_has_backs )
    {
        p_chans_out[i_BL] = AOUT_CHAN_REARLEFT;
        p_chans_out[i_BR] = AOUT_CHAN_REARRIGHT;
    }
    if( b_has_BC )
        p_chans_out[i_BC] = AOUT_CHAN_REARCENTER;
    if( b_has_sides )
    {
        p_chans_out[i_SL] = AOUT_CHAN_MIDDLELEFT;
        p_chans_out[i_SR] = AOUT_CHAN_MIDDLERIGHT;
    }

#undef HAS_CHAN
    return true;
}


/**
 * Configure and create an Android AudioTrack.
 * returns -1 on critical error, 0 on success, 1 on configuration error
 */
static int
JNIThread_Configure( JNIEnv *env, audio_output_t *p_aout )
{
    struct aout_sys_t *p_sys = p_aout->sys;
    int i_size, i_min_buffer_size, i_channel_config, i_format,
        i_format_size, i_nb_channels;
    uint8_t i_chans_to_reorder = 0;
    uint8_t p_chan_table[AOUT_CHAN_MAX];
    jobject p_audiotrack;
    audio_sample_format_t fmt = p_sys->fmt;

    /* 4000 <= frequency <= 48000 */
    fmt.i_rate = VLC_CLIP( fmt.i_rate, 4000, 48000 );

    /* We can only accept U8, S16N, FL32 */
    switch( fmt.i_format )
    {
        case VLC_CODEC_U8:
            break;
        case VLC_CODEC_S16N:
            break;
        case VLC_CODEC_FL32:
            if( !jfields.AudioFormat.has_ENCODING_PCM_FLOAT )
                fmt.i_format = VLC_CODEC_S16N;
            break;
        default:
            fmt.i_format = VLC_CODEC_S16N;
            break;
    }
    switch( fmt.i_format )
    {
        case VLC_CODEC_U8:
            i_format = jfields.AudioFormat.ENCODING_PCM_8BIT;
            i_format_size = 1;
            break;
        case VLC_CODEC_S16N:
            i_format = jfields.AudioFormat.ENCODING_PCM_16BIT;
            i_format_size = 2;
            break;
        case VLC_CODEC_FL32:
            i_format = jfields.AudioFormat.ENCODING_PCM_FLOAT;
            i_format_size = 4;
            break;
        default:
            vlc_assert_unreachable();
    }

    i_nb_channels = aout_FormatNbChannels( &fmt );

    /* Android AudioTrack supports only mono, stereo, 5.1 and 7.1.
     * Android will downmix to stereo if audio output doesn't handle 5.1 or 7.1
     */
    if( i_nb_channels > 5 )
    {
        uint32_t p_chans_out[AOUT_CHAN_MAX];

        if( i_nb_channels > 7 && jfields.AudioFormat.has_CHANNEL_OUT_SIDE )
        {
            fmt.i_physical_channels = AOUT_CHANS_7_1;
            /* bitmask of CHANNEL_OUT_7POINT1 doesn't correspond to 5POINT1 and
             * SIDES */
            i_channel_config = jfields.AudioFormat.CHANNEL_OUT_5POINT1 |
                               jfields.AudioFormat.CHANNEL_OUT_SIDE_LEFT |
                               jfields.AudioFormat.CHANNEL_OUT_SIDE_RIGHT;
        } else
        {
            fmt.i_physical_channels = AOUT_CHANS_5_1;
            i_channel_config = jfields.AudioFormat.CHANNEL_OUT_5POINT1;
        }
        if( AudioTrack_GetChanOrder( i_channel_config, p_chans_out ) )
            i_chans_to_reorder =
                aout_CheckChannelReorder( NULL, p_chans_out,
                                          fmt.i_physical_channels,
                                          p_chan_table );
        else
            return 1;
    } else
    {
        if( i_nb_channels == 1 )
        {
            i_channel_config = jfields.AudioFormat.CHANNEL_OUT_MONO;
        } else
        {
            fmt.i_physical_channels = AOUT_CHANS_STEREO;
            i_channel_config = jfields.AudioFormat.CHANNEL_OUT_STEREO;
        }
    }
    i_nb_channels = aout_FormatNbChannels( &fmt );

    i_min_buffer_size = JNI_AT_CALL_STATIC_INT( getMinBufferSize, fmt.i_rate,
                                                i_channel_config, i_format );
    if( i_min_buffer_size <= 0 )
    {
        msg_Warn( p_aout, "getMinBufferSize returned an invalid size" ) ;
        return 1;
    }
    i_size = i_min_buffer_size * 4;

    /* create AudioTrack object */
    p_audiotrack = JNI_AT_NEW( jfields.AudioManager.STREAM_MUSIC, fmt.i_rate,
                               i_channel_config, i_format, i_size,
                               jfields.AudioTrack.MODE_STREAM );
    if( CHECK_AT_EXCEPTION( "AudioTrack<init>" ) || !p_audiotrack )
        return 1;
    p_sys->p_audiotrack = (*env)->NewGlobalRef( env, p_audiotrack );
    (*env)->DeleteLocalRef( env, p_audiotrack );
    if( !p_sys->p_audiotrack )
        return -1;

    p_sys->i_chans_to_reorder = i_chans_to_reorder;
    if( i_chans_to_reorder )
        memcpy( p_sys->p_chan_table, p_chan_table, sizeof(p_sys->p_chan_table) );
    p_sys->i_bytes_per_frame = i_nb_channels * i_format_size;
    p_sys->i_max_audiotrack_samples = i_size / p_sys->i_bytes_per_frame;
    p_sys->fmt = fmt;

    return 0;
}

static int
JNIThread_Start( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_ret;

    aout_FormatPrint( p_aout, "VLC is looking for:", &p_sys->fmt );
    p_sys->fmt.i_original_channels = p_sys->fmt.i_physical_channels;

    i_ret = JNIThread_Configure( env, p_aout );
    if( i_ret == 1 )
    {
        /* configuration error, try to fallback to stereo */
        if( ( p_sys->fmt.i_format != VLC_CODEC_U8 &&
              p_sys->fmt.i_format != VLC_CODEC_S16N ) ||
            aout_FormatNbChannels( &p_sys->fmt ) > 2 )
        {
            msg_Warn( p_aout,
                      "AudioTrack configuration failed, try again in stereo" );
            p_sys->fmt.i_format = VLC_CODEC_S16N;
            p_sys->fmt.i_physical_channels = AOUT_CHANS_STEREO;

            i_ret = JNIThread_Configure( env, p_aout );
        }
    }
    if( i_ret != 0 )
        return VLC_EGENERIC;

    aout_FormatPrint( p_aout, "VLC will output:", &p_sys->fmt );

    if( JNI_AT_CALL_INT( getState ) != jfields.AudioTrack.STATE_INITIALIZED )
    {
        msg_Err( p_aout, "AudioTrack init failed" );
        goto error;
    }

#ifdef AUDIOTRACK_USE_TIMESTAMP
    if( jfields.AudioTimestamp.clazz )
    {
        /* create AudioTimestamp object */
        jobject p_audioTimestamp = JNI_CALL( NewObject,
                                             jfields.AudioTimestamp.clazz,
                                             jfields.AudioTimestamp.ctor );
        if( !p_audioTimestamp )
            goto error;
        p_sys->p_audioTimestamp = (*env)->NewGlobalRef( env, p_audioTimestamp );
        (*env)->DeleteLocalRef( env, p_audioTimestamp );
        if( !p_sys->p_audioTimestamp )
            goto error;
    }
#endif

    JNI_AT_CALL_VOID( play );
    CHECK_AT_EXCEPTION( "play" );
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
JNIThread_Stop( JNIEnv *env, audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    if( !p_sys->b_audiotrack_exception )
    {
        JNI_AT_CALL_VOID( stop );
        if( !CHECK_AT_EXCEPTION( "stop" ) )
            JNI_AT_CALL_VOID( release );
    }
    p_sys->b_audiotrack_exception = false;
    (*env)->DeleteGlobalRef( env, p_sys->p_audiotrack );
    p_sys->p_audiotrack = NULL;

    if( p_sys->p_audioTimestamp )
    {
        (*env)->DeleteGlobalRef( env, p_sys->p_audioTimestamp );
        p_sys->p_audioTimestamp = NULL;
    }
}

/**
 * Non blocking write function.
 * Do a calculation between current position and audiotrack position and assure
 * that we won't wait in AudioTrack.write() method
 */
static int
JNIThread_Write( JNIEnv *env, audio_output_t *p_aout, block_t *p_buffer )
{
    aout_sys_t *p_sys = p_aout->sys;
    uint8_t *p_data = p_buffer->p_buffer;
    size_t i_data;
    uint32_t i_samples;
    uint32_t i_audiotrack_pos;
    uint32_t i_samples_pending;

    i_audiotrack_pos = JNIThread_GetAudioTrackPos( env, p_aout );
    if( i_audiotrack_pos > p_sys->i_samples_written )
    {
        msg_Warn( p_aout, "audiotrack position is ahead. Should NOT happen" );
        JNIThread_InitDelay( env, p_aout );
        return 0;
    }
    i_samples_pending = p_sys->i_samples_written - i_audiotrack_pos;

    /* check if audiotrack buffer is not full before writing on it. */
    if( i_samples_pending >= p_sys->i_max_audiotrack_samples )
    {

        /* HACK: AudioFlinger can drop frames without notifying us and there is
         * no way to know it. It it happens, i_audiotrack_pos won't move and
         * the current code will be stuck because it'll assume that audiotrack
         * internal buffer is full when it's not. It can happen only after
         * Android 4.4.2 if we send frames too quickly. This HACK is just an
         * other precaution since it shouldn't happen anymore thanks to the
         * HACK in JNIThread_Play */

        p_sys->i_audiotrack_stuck_count++;
        if( p_sys->i_audiotrack_stuck_count > 100 )
        {
            msg_Warn( p_aout, "AudioFlinger underrun, force write" );
            i_samples_pending = 0;
            p_sys->i_audiotrack_stuck_count = 0;
        }
    } else
        p_sys->i_audiotrack_stuck_count = 0;
    i_samples = __MIN( p_sys->i_max_audiotrack_samples - i_samples_pending,
                       p_buffer->i_nb_samples );

    i_data = i_samples * p_sys->i_bytes_per_frame;

    /* check if we need to realloc a ByteArray */
    if( i_data > p_sys->i_bytearray_size )
    {
        jbyteArray p_bytearray;

        if( p_sys->p_bytearray )
        {
            (*env)->DeleteGlobalRef( env, p_sys->p_bytearray );
            p_sys->p_bytearray = NULL;
        }

        p_bytearray = (*env)->NewByteArray( env, i_data );
        if( p_bytearray )
        {
            p_sys->p_bytearray = (*env)->NewGlobalRef( env, p_bytearray );
            (*env)->DeleteLocalRef( env, p_bytearray );
        }
        p_sys->i_bytearray_size = i_data;
    }
    if( !p_sys->p_bytearray )
        return jfields.AudioTrack.ERROR_BAD_VALUE;

    /* copy p_buffer in to ByteArray */
    (*env)->SetByteArrayRegion( env, p_sys->p_bytearray, 0, i_data,
                                (jbyte *)p_data);

    return JNI_AT_CALL_INT( write, p_sys->p_bytearray, 0, i_data );
}

/**
 * Non blocking write function for Lollipop and after.
 * It calls a new write method with WRITE_NON_BLOCKING flags.
 */
static int
JNIThread_WriteV21( JNIEnv *env, audio_output_t *p_aout, block_t *p_buffer )
{
    aout_sys_t *p_sys = p_aout->sys;
    int i_ret;

    if( !p_sys->p_bytebuffer )
    {
        jobject p_bytebuffer;

        p_bytebuffer = (*env)->NewDirectByteBuffer( env, p_buffer->p_buffer,
                                                    p_buffer->i_buffer );
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

    i_ret = JNI_AT_CALL_INT( writeV21, p_sys->p_bytebuffer, p_buffer->i_buffer,
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

static int
JNIThread_Play( JNIEnv *env, audio_output_t *p_aout,
                block_t **pp_buffer, mtime_t *p_wait )
{
    aout_sys_t *p_sys = p_aout->sys;
    block_t *p_buffer = *pp_buffer;
    int i_ret;

    if( jfields.AudioTrack.writeV21 )
        i_ret = JNIThread_WriteV21( env, p_aout, p_buffer );
    else
        i_ret = JNIThread_Write( env, p_aout, p_buffer );

    if( i_ret < 0 ) {
        if( jfields.AudioManager.has_ERROR_DEAD_OBJECT
            && i_ret == jfields.AudioManager.ERROR_DEAD_OBJECT )
        {
            msg_Warn( p_aout, "ERROR_DEAD_OBJECT: "
                              "try recreating AudioTrack" );
            JNIThread_Stop( env, p_aout );
            i_ret = JNIThread_Start( env, p_aout );
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
        uint32_t i_samples = i_ret / p_sys->i_bytes_per_frame;
        p_sys->i_samples_queued -= i_samples;
        p_sys->i_samples_written += i_samples;

        p_buffer->p_buffer += i_ret;
        p_buffer->i_buffer -= i_ret;
        p_buffer->i_nb_samples -= i_samples;
        if( p_buffer->i_buffer == 0 )
            *pp_buffer = NULL;

        /* HACK: There is a known issue in audiotrack, "due to an internal
         * timeout within the AudioTrackThread". It happens after android
         * 4.4.2, it's not a problem for Android 5.0 since we use an other way
         * to write samples. A working hack is to wait a little between each
         * write. This hack is done only for API 19 (AudioTimestamp was added
         * in API 19). */

        if( jfields.AudioTimestamp.clazz && !jfields.AudioTrack.writeV21 )
            *p_wait = FRAMES_TO_US( i_samples ) / 2;
    }
    return i_ret >= 0 ? VLC_SUCCESS : VLC_EGENERIC;
}

static void
JNIThread_Pause( JNIEnv *env, audio_output_t *p_aout,
                 bool b_pause, mtime_t i_date )
{
    VLC_UNUSED( i_date );

    aout_sys_t *p_sys = p_aout->sys;

    if( b_pause )
    {
        JNI_AT_CALL_VOID( pause );
        CHECK_AT_EXCEPTION( "pause" );
    } else
    {
        JNI_AT_CALL_VOID( play );
        CHECK_AT_EXCEPTION( "play" );
        p_sys->i_play_time = mdate();
    }
}

static void
JNIThread_Flush( JNIEnv *env, audio_output_t *p_aout,
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
    p_sys->i_play_time = mdate();

    if( p_sys->p_bytebuffer )
    {
        (*env)->DeleteGlobalRef( env, p_sys->p_bytebuffer );
        p_sys->p_bytebuffer = NULL;
    }
}

static void *
JNIThread( void *data )
{
    audio_output_t *p_aout = data;
    aout_sys_t *p_sys = p_aout->sys;
    bool b_error = false;
    bool b_paused = false;
    block_t *p_buffer = NULL;
    mtime_t i_play_deadline = 0;
    JNIEnv* env;

    jni_attach_thread( &env, THREAD_NAME );

    vlc_mutex_lock( &p_sys->mutex );
    if( !env )
        goto end;

    while( p_sys->b_thread_run )
    {
        struct thread_cmd *p_cmd;
        bool b_remove_cmd = true;

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

        if( p_cmd->id == CMD_PLAY && i_play_deadline > 0 )
        {
            if( mdate() > i_play_deadline )
                i_play_deadline = 0;
            else
            {
                int i_ret = 0;
                while( p_cmd == TAILQ_FIRST( &p_sys->thread_cmd_queue )
                       && i_ret != ETIMEDOUT && p_sys->b_thread_run )
                    i_ret = vlc_cond_timedwait( &p_sys->cond, &p_sys->mutex,
                                                i_play_deadline );
                continue;
            }
        }

        /* process a command */
        switch( p_cmd->id )
        {
            case CMD_START:
                assert( !p_sys->p_audiotrack );
                if( b_error ) {
                    p_cmd->out.start.i_ret = -1;
                    break;
                }
                p_sys->fmt = *p_cmd->in.start.p_fmt;
                p_cmd->out.start.i_ret =
                        JNIThread_Start( env, p_aout );
                JNIThread_InitDelay( env, p_aout );
                p_cmd->out.start.p_fmt = &p_sys->fmt;
                b_paused = false;
                break;
            case CMD_STOP:
                assert( p_sys->p_audiotrack );
                JNIThread_Stop( env, p_aout );
                JNIThread_InitDelay( env, p_aout );
                b_paused = false;
                b_error = false;
                p_buffer = NULL;
                break;
            case CMD_PLAY:
            {
                mtime_t i_play_wait = 0;

                assert( p_sys->p_audiotrack );
                if( b_error )
                    break;
                if( p_buffer == NULL )
                {
                    p_buffer = p_cmd->in.play.p_buffer;
                    if( p_sys->i_chans_to_reorder )
                       aout_ChannelReorder( p_buffer->p_buffer,
                                            p_buffer->i_buffer,
                                            p_sys->i_chans_to_reorder,
                                            p_sys->p_chan_table,
                                            p_sys->fmt.i_format );

                }
                b_error = JNIThread_Play( env, p_aout, &p_buffer,
                                          &i_play_wait ) != VLC_SUCCESS;
                if( p_buffer != NULL )
                    b_remove_cmd = false;
                if( i_play_wait > 0 )
                    i_play_deadline = mdate() + i_play_wait;
                break;
            }
            case CMD_PAUSE:
                assert( p_sys->p_audiotrack );
                if( b_error )
                    break;
                JNIThread_Pause( env, p_aout,
                                 p_cmd->in.pause.b_pause,
                                 p_cmd->in.pause.i_date );
                b_paused = p_cmd->in.pause.b_pause;
                break;
            case CMD_TIME_GET:
                assert( p_sys->p_audiotrack );
                if( b_error )
                    break;
                p_cmd->out.time_get.i_ret =
                        JNIThread_TimeGet( env, p_aout,
                                           &p_cmd->out.time_get.i_delay );
                break;
            case CMD_FLUSH:
                assert( p_sys->p_audiotrack );
                if( b_error )
                    break;
                JNIThread_Flush( env, p_aout,
                                 p_cmd->in.flush.b_wait );
                JNIThread_InitDelay( env, p_aout );
                p_buffer = NULL;
                break;
            default:
                vlc_assert_unreachable();
        }
        if( p_sys->b_audiotrack_exception )
            b_error = true;

        if( b_remove_cmd )
        {
            TAILQ_REMOVE( &p_sys->thread_cmd_queue, p_cmd, next );
            p_cmd->id = CMD_DONE;
            if( p_cmd->pf_destroy )
                p_cmd->pf_destroy( p_cmd );
        }

        /* signal that command is processed */
        vlc_cond_signal( &p_sys->cond );
    }
end:
    if( env )
    {
        if( p_sys->p_bytearray )
            (*env)->DeleteGlobalRef( env, p_sys->p_bytearray );
        if( p_sys->p_bytebuffer )
            (*env)->DeleteGlobalRef( env, p_sys->p_bytebuffer );
        jni_detach_thread();
    }
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
    struct thread_cmd *p_cmd;

    vlc_mutex_lock( &p_sys->mutex );

    ThreadCmd_FlushQueue( p_sys );

    p_cmd = ThreadCmd_New( CMD_STOP );
    if( p_cmd )
    {
        /* ask the thread to process the Stop command */
        ThreadCmd_InsertHead( p_sys, p_cmd );
        ThreadCmd_Wait( p_sys, p_cmd );

        free( p_cmd );
    }

    vlc_mutex_unlock( &p_sys->mutex );
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
    struct thread_cmd *p_cmd;

    vlc_mutex_lock( &p_sys->mutex );

    while( p_sys->i_samples_queued != 0
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

    vlc_mutex_unlock( &p_sys->mutex );
}

static void
Pause( audio_output_t *p_aout, bool b_pause, mtime_t i_date )
{
    aout_sys_t *p_sys = p_aout->sys;
    struct thread_cmd *p_cmd;

    vlc_mutex_lock( &p_sys->mutex );

    p_cmd = ThreadCmd_New( CMD_PAUSE );
    if( p_cmd )
    {
        /* ask the thread to process the Pause command */
        p_cmd->in.pause.b_pause = b_pause;
        p_cmd->in.pause.i_date = i_date;

        ThreadCmd_InsertHead( p_sys, p_cmd );
        ThreadCmd_Wait( p_sys, p_cmd );

        free( p_cmd );
    }

    vlc_mutex_unlock( &p_sys->mutex );
}

static void
Flush( audio_output_t *p_aout, bool b_wait )
{
    aout_sys_t *p_sys = p_aout->sys;
    struct thread_cmd *p_cmd;

    vlc_mutex_lock( &p_sys->mutex );

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

    vlc_mutex_unlock( &p_sys->mutex );
}

static int
TimeGet( audio_output_t *p_aout, mtime_t *restrict p_delay )
{
    aout_sys_t *p_sys = p_aout->sys;
    struct thread_cmd *p_cmd;
    int i_ret = -1;

    vlc_mutex_lock( &p_sys->mutex );

    p_cmd = ThreadCmd_New( CMD_TIME_GET );
    if( p_cmd)
    {
        ThreadCmd_InsertHead( p_sys, p_cmd );
        ThreadCmd_Wait( p_sys, p_cmd );

        i_ret = p_cmd->out.time_get.i_ret;
        *p_delay = p_cmd->out.time_get.i_delay;
        free( p_cmd );
    }

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

    /* create JNIThread */
    p_sys->b_thread_run = true;
    if( vlc_clone( &p_sys->thread,
                   JNIThread, p_aout, VLC_THREAD_PRIORITY_AUDIO ) )
    {
        msg_Err( p_aout, "JNIThread creation failed" );
        p_sys->b_thread_run = false;
        Close( obj );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void
Close( vlc_object_t *obj )
{
    audio_output_t *p_aout = (audio_output_t *) obj;
    aout_sys_t *p_sys = p_aout->sys;

    /* kill the thread */
    vlc_mutex_lock( &p_sys->mutex );
    if( p_sys->b_thread_run )
    {
        p_sys->b_thread_run = false;
        vlc_cond_signal( &p_sys->cond );
        vlc_mutex_unlock( &p_sys->mutex );
        vlc_join( p_sys->thread, NULL );
    } else
        vlc_mutex_unlock( &p_sys->mutex );

    vlc_mutex_destroy( &p_sys->mutex );
    vlc_cond_destroy( &p_sys->cond );

    free( p_sys );
}
