/*****************************************************************************
 * vlcglue.h: Main header for the Python binding
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <olivier.aubert at liris.cnrs.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef _VLCGLUE_H
#define _VLCGLUE_H 1

#include <Python.h>
#include "structmember.h"

#include <stdio.h>
#include <vlc/vlc.h>
#include <vlc/libvlc.h>
#include <vlc/mediacontrol_structures.h>
#include <vlc/mediacontrol.h>

/* Python 2.5 64-bit support compatibility define */
#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif


/**********************************************************************
 * Exceptions handling
 **********************************************************************/

#define MC_TRY exception=mediacontrol_exception_create( )

#define MC_EXCEPT  \
  if( exception && exception->code ) { \
    PyObject *py_exc = MediaControl_InternalException; \
    switch( exception->code ) { \
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
    PyErr_SetString( py_exc, exception->message ); \
    mediacontrol_exception_free( exception ); \
    return NULL; \
  } else if( exception ) { mediacontrol_exception_free( exception ); }

PyObject *MediaControl_InternalException;
PyObject *MediaControl_PositionKeyNotSupported;
PyObject *MediaControl_PositionOriginNotSupported;
PyObject *MediaControl_InvalidPosition;
PyObject *MediaControl_PlaylistException;
PyObject *vlc_Exception;

/**********************************************************************
 * vlc.Instance Object
 **********************************************************************/
typedef struct
{
    PyObject_HEAD
    libvlc_instance_t* p_instance;
} vlcInstance;

/**********************************************************************
 * MediaControl Object
 **********************************************************************/
typedef struct
{
    PyObject_HEAD
    mediacontrol_Instance* mc;
    vlcInstance *vlc_instance;
} MediaControl;

/**********************************************************************
 * Position Object
 **********************************************************************/
typedef struct
{
    PyObject_HEAD
    int origin;
    int key;
    PY_LONG_LONG value;
} PyPosition;

/**********************************************************************
 * vlc.MediaPlayer Object
 **********************************************************************/
typedef struct
{
    PyObject_HEAD
    libvlc_media_player_t* p_mp;
} vlcMediaPlayer;

/**********************************************************************
 * vlc.Media Object
 **********************************************************************/
typedef struct
{
    PyObject_HEAD
    libvlc_media_t* p_media;
} vlcMedia;

/* Forward declarations */
staticforward PyTypeObject MediaControl_Type;
staticforward PyTypeObject PyPosition_Type;
staticforward PyTypeObject vlcInstance_Type;
staticforward PyTypeObject vlcMediaPlayer_Type;
staticforward PyTypeObject vlcMedia_Type;

#define LIBVLC_INSTANCE(self) (((vlcInstance*)self)->p_instance)
#define LIBVLC_MEDIAPLAYER(self) (((vlcMediaPlayer*)self)->p_mp)
#define LIBVLC_MEDIA(self) (((vlcMedia*)self)->p_media)
#define LIBVLC_MC(self) (((MediaControl*)self)->mc)

#define LIBVLC_TRY libvlc_exception_init( &ex );

#define LIBVLC_EXCEPT if( libvlc_exception_raised( &ex ) ) { \
    PyObject *py_exc = vlc_Exception; \
    PyErr_SetString( py_exc, libvlc_errmsg() );	\
    return NULL; \
  }

mediacontrol_PositionKey positionKey_py_to_c( PyObject * py_key );
mediacontrol_PositionOrigin positionOrigin_py_to_c( PyObject * py_origin );
mediacontrol_Position * position_py_to_c( PyObject * py_position );
PyPosition * position_c_to_py( mediacontrol_Position * position );

/* Long long conversion on Mac os X/ppc */
#if defined (__ppc__) || defined(__ppc64__)
#define ntohll(x) ((long long) x >> 64)
#else
#define ntohll(x) (x)
#endif

#endif
