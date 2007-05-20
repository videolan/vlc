/*****************************************************************************
 * callback-jni.cc: JNI native callback functions for VLC Java Bindings
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *
 *
 * $Id $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* These are a must*/
#include <jni.h>
#include <vlc/libvlc.h>
#ifdef WIN32
#include <windows.h>
#undef usleep
#define usleep(var) Sleep(var/1000)
#else
#include <unistd.h>
#endif
#include <stdio.h>

#include "utils.h"

#include "../includes/Audio.h"

static JavaVM *jvm;
static jclass audioClass;
static jmethodID wakeupListenersMethod;

void volumeChangedCallback( libvlc_instance_t *p_instance, libvlc_event_t *event, void *user_data );


JNIEXPORT void JNICALL Java_org_videolan_jvlc_Audio__1install_1callback( JNIEnv *env, jobject _this )
{
    INIT_FUNCTION ;
    if (jvm == NULL)
    {
        env->GetJavaVM( &jvm );
	audioClass = env->GetObjectClass( _this );
	wakeupListenersMethod = env->GetStaticMethodID(audioClass, "wakeupListeners", "()V");
    }

    libvlc_callback_register_for_event( ( libvlc_instance_t* ) instance,
					VOLUME_CHANGED,
					volumeChangedCallback,
					NULL,
					exception );
    CHECK_EXCEPTION_FREE ;
}

void volumeChangedCallback( struct libvlc_instance_t *p_instance, libvlc_event_t *event, void *user_data )
{
    JNIEnv *env;
    jvm->AttachCurrentThread( ( void ** ) &env, NULL );

    env->CallStaticVoidMethod( audioClass, wakeupListenersMethod);
}
