/*****************************************************************************
 * vlcglue.h: Main header for the Python binding
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <oaubert at bat710.univ-lyon1.fr>
 *          Clément Stenac <zorglub@videolan.org>
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

#include <Python.h>
#include "structmember.h"

/* Undefine the following define to disable low-level vlc Object support */
#define VLCOBJECT_SUPPORT 0

#define __VLC__

#include <stdio.h>
#include <vlc/vlc.h>
#include <vlc/control_structures.h>
#include <vlc/control.h>

#define SELF ((MediaControl*)self)

/**********************************************************************
 * Exceptions handling
 **********************************************************************/

#define MC_TRY exception=mediacontrol_exception_init(exception)

#define MC_EXCEPT  \
  if (exception->code) { \
    PyObject *py_exc = MediaControl_InternalException; \
    switch (exception->code) { \
    case mediacontrol_InternalException: \
      py_exc = MediaControl_InternalException; \
      break; \
    case mediacontrol_PlaylistException: \
      py_exc = MediaControl_PlaylistException; \
      break; \
    case mediacontrol_InvalidPosition: \
      py_exc = MediaControl_InvalidPosition; \
      break; \
    case mediacontrol_PositionKeyNotSupported: \
      py_exc = MediaControl_PositionKeyNotSupported; \
      break; \
    case mediacontrol_PositionOriginNotSupported: \
      py_exc = MediaControl_PositionOriginNotSupported; \
      break; \
    } \
    PyErr_SetString(py_exc, exception->message); \
    mediacontrol_exception_free(exception); \
    return NULL; \
  } else { mediacontrol_exception_free(exception); }

PyObject *MediaControl_InternalException;
PyObject *MediaControl_PositionKeyNotSupported;
PyObject *MediaControl_PositionOriginNotSupported;
PyObject *MediaControl_InvalidPosition;
PyObject *MediaControl_PlaylistException;

/**********************************************************************
 * VLC Object
 **********************************************************************/
#ifdef VLCOBJECT_SUPPORT

#define VLCSELF ((vlcObject*)self)

/**********************************************************************
 * VLCObject Object
 **********************************************************************/
typedef struct
{
    PyObject_HEAD
    vlc_object_t* p_object;
    int b_released;
} vlcObject;

staticforward PyTypeObject vlcObject_Type;

#endif

/**********************************************************************
 * MediaControl Object
 **********************************************************************/
typedef struct
{
    PyObject_HEAD
    mediacontrol_Instance* mc;
}MediaControl;

staticforward PyTypeObject MediaControl_Type;

/**********************************************************************
 * Position Object
 **********************************************************************/
typedef struct
{
    PyObject_HEAD
    int origin;
    int key;
    long long value;
} PyPosition;

staticforward PyTypeObject PyPosition_Type;

mediacontrol_PositionKey positionKey_py_to_c( PyObject * py_key );
mediacontrol_PositionOrigin positionOrigin_py_to_c( PyObject * py_origin );
mediacontrol_Position* position_py_to_c( PyObject * py_position );
PyPosition* position_c_to_py(mediacontrol_Position *position);
