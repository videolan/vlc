/*****************************************************************************
 * utils.cc: Utility functions for VLC Java Bindings
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

#include "utils.h"

JAWT awt;
JAWT_DrawingSurfaceInfo* dsi;

void handle_vlc_exception( JNIEnv* env, libvlc_exception_t* exception ) {
  jclass newExcCls;

  // raise a Java exception
  newExcCls = env->FindClass("org/videolan/jvlc/VLCException");
  if (newExcCls == 0) { /* Unable to find the new exception class, give up. */
      return;
  }
  env->ThrowNew(newExcCls, libvlc_exception_get_message(exception));
	
}

jlong getInstance (JNIEnv *env, jobject _this) {
    /* get the id field of object */
    jclass    cls   = env->GetObjectClass(_this);
    jmethodID mid   = env->GetMethodID(cls, "getInstance", "()J");
    jlong     field = env->CallLongMethod(_this, mid);
    return field;
}
