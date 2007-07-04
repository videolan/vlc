/* These are a must*/
#include <jni.h>
#include <jawt.h>
#include <jawt_md.h>

#include <vlc/libvlc.h>
#include <stdlib.h> // for free

void handle_vlc_exception( JNIEnv*, libvlc_exception_t* );
jlong getInstance ( JNIEnv* , jobject );

#define CHECK_EXCEPTION_FREE \
    if ( libvlc_exception_raised( exception )) \
    { \
        handle_vlc_exception( env, exception ); \
    } \
    free( exception );

#define CHECK_EXCEPTION \
    if ( libvlc_exception_raised( exception )) \
    { \
        handle_vlc_exception( env, exception ); \
    }


#define INIT_FUNCTION \
    long instance; \
    libvlc_exception_t *exception = ( libvlc_exception_t * ) malloc( sizeof( libvlc_exception_t )); \
    libvlc_exception_init( exception ); \
    instance = getInstance( env, _this );

#define GET_INPUT_THREAD \
    libvlc_media_instance_t *input; \
    input = libvlc_playlist_get_media_instance( ( libvlc_instance_t *) instance, exception ); \
    CHECK_EXCEPTION ;

#define ATTACH_JVM \
    JNIEnv *env; \
    jvm->AttachCurrentThread( ( void ** ) &env, NULL );

