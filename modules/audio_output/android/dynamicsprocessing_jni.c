/*****************************************************************************
 * android/dynamicsprocessing_jni.c: Android DynamicsProcessing
 *****************************************************************************
 * Copyright © 2012-2023 VLC authors and VideoLAN, VideoLabs
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_fourcc.h>

#include <jni.h>
#include <math.h>

#include "dynamicsprocessing_jni.h"
#include "../../video_output/android/env.h"

#define THREAD_NAME "android_audio"
#define GET_ENV() android_getEnv( obj, THREAD_NAME )
#define JNI_CALL( what, obj, method, ... ) (*env)->what( env, obj, method, ##__VA_ARGS__ )
#define JNI_CALL_INT( obj, method, ... ) JNI_CALL( CallIntMethod, obj, method, ##__VA_ARGS__ )
#define JNI_CALL_VOID( obj, method, ... ) JNI_CALL( CallVoidMethod, obj, method, ##__VA_ARGS__ )

struct DynamicsProcessing_fields
{
    /* Needed to probe for PCM 32bit support */
    struct {
        jclass clazz;
        jint ENCODING_PCM_32BIT;
    } AudioFormat;

    struct {
        jclass clazz;
        jmethodID ctor;
        jmethodID setInputGainAllChannelsTo;
        jmethodID setEnabled;
    } DynamicsProcessing;
};
static struct DynamicsProcessing_fields jfields;

int DynamicsProcessing_InitJNI( vlc_object_t *obj )
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static int i_init_state = -1;

    jclass clazz;
    jfieldID field;
    JNIEnv *env = android_getEnv( obj, THREAD_NAME );

    if( env == NULL )
        return VLC_EGENERIC;

    vlc_mutex_lock(&lock);
    if( i_init_state != -1 )
        goto end;

#define CHECK_EXCEPTION( what, critical ) do { \
    if( (*env)->ExceptionCheck( env ) ) \
    { \
        msg_Err( obj, "%s failed", what ); \
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

    /* Don't use DynamicsProcessing before Android 12 since it may crash
     * randomly, cf. videolan/vlc-android#2221.
     *
     * ENCODING_PCM_32BIT is available on API 31, so test its availability to
     * check if we are running Android 12 */
    GET_CLASS( "android/media/AudioFormat", true );
    GET_CONST_INT( AudioFormat.ENCODING_PCM_32BIT, "ENCODING_PCM_32BIT", true );

    GET_CLASS( "android/media/audiofx/DynamicsProcessing", true );

    jfields.DynamicsProcessing.clazz = (jclass) (*env)->NewGlobalRef( env, clazz );
    CHECK_EXCEPTION( "NewGlobalRef", true );
    GET_ID( GetMethodID, DynamicsProcessing.ctor, "<init>", "(I)V", true );
    GET_ID( GetMethodID, DynamicsProcessing.setInputGainAllChannelsTo,
            "setInputGainAllChannelsTo", "(F)V", true );
    GET_ID( GetMethodID, DynamicsProcessing.setEnabled,
            "setEnabled", "(Z)I", true );

#undef CHECK_EXCEPTION

    i_init_state = 1;
end:
    vlc_mutex_unlock(&lock);
    return i_init_state == 1 ? VLC_SUCCESS : VLC_EGENERIC;
}

static inline bool
check_exception( JNIEnv *env, vlc_object_t *obj,
                 const char *class, const char *method )
{
    if( (*env)->ExceptionCheck( env ) )
    {
        (*env)->ExceptionDescribe( env );
        (*env)->ExceptionClear( env );
        msg_Err( obj, "%s.%s triggered an exception !", class, method );
        return true;
    } else
        return false;
}
#define CHECK_EXCEPTION( class, method ) check_exception( env, obj, class, method )


jobject
(DynamicsProcessing_New)( vlc_object_t *obj, int session_id )
{
    JNIEnv *env;
    if( !( env = GET_ENV() ) )
        return NULL;

    int ret = DynamicsProcessing_InitJNI( obj );
    if (ret == VLC_EGENERIC)
        return NULL;

    jobject dp = JNI_CALL( NewObject, jfields.DynamicsProcessing.clazz,
                           jfields.DynamicsProcessing.ctor, session_id );

    if( CHECK_EXCEPTION( "DynamicsProcessing", "ctor" ) )
        return NULL;

    jobject global_dp = (*env)->NewGlobalRef( env, dp );
    (*env)->DeleteLocalRef( env, dp );

    return global_dp;
}

void
(DynamicsProcessing_Disable)( vlc_object_t *obj, jobject dp )
{
    JNIEnv *env;
    if( !( env = GET_ENV() ) )
        return;

    JNI_CALL_INT( dp, jfields.DynamicsProcessing.setEnabled, false );
    CHECK_EXCEPTION( "DynamicsProcessing", "setEnabled" );
}

int
(DynamicsProcessing_SetVolume)( vlc_object_t *obj, jobject dp, float volume )
{
    JNIEnv *env;
    if( !( env = GET_ENV() ) )
        return VLC_EGENERIC;

    /* convert linear gain to dB */
    float dB = volume == 0.0f ? -144 : 20.0f * log10f( volume );

    JNI_CALL_VOID( dp, jfields.DynamicsProcessing.setInputGainAllChannelsTo, dB );
    if( CHECK_EXCEPTION( "DynamicsProcessing", "setInputGainAllChannelsTo" ) )
        return VLC_EGENERIC;

    int ret = JNI_CALL_INT( dp, jfields.DynamicsProcessing.setEnabled, volume != 1.0f );
    if( CHECK_EXCEPTION( "DynamicsProcessing", "setEnabled" ) || ret != 0 )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

void
(DynamicsProcessing_Delete)( vlc_object_t *obj, jobject dp )
{
    JNIEnv *env;
    if( !( env = GET_ENV() ) )
        return;

    JNI_CALL_INT( dp, jfields.DynamicsProcessing.setEnabled, false );
    CHECK_EXCEPTION( "DynamicsProcessing", "setEnabled" );

    (*env)->DeleteGlobalRef( env, dp );
}
