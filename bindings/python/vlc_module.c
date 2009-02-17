/*****************************************************************************
 * vlc_module.c: vlc python binding module
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <oaubert at bat710.univ-lyon1.fr>
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

#include "vlcglue.h"

/**************************************************************************
 * VLC Module
 **************************************************************************/

#ifndef vlcMODINIT_FUNC /* declarations for DLL import/export */
#define vlcMODINIT_FUNC void
#endif

static PyMethodDef vlc_methods[] = {
    { NULL }  /* Sentinel */
};

/* Module globals */
PyObject* MediaControl_InternalException          = NULL;
PyObject* MediaControl_PositionKeyNotSupported    = NULL;
PyObject *MediaControl_PositionOriginNotSupported = NULL;
PyObject* MediaControl_InvalidPosition            = NULL;
PyObject *MediaControl_PlaylistException          = NULL;

vlcMODINIT_FUNC
initvlc( void )
{
    PyObject* p_module;

    /* vlcMediaPlayer_Type.tp_new = PyType_GenericNew; */
    vlcMediaPlayer_Type.tp_alloc = PyType_GenericAlloc;
    /* vlcMedia_Type.tp_new = PyType_GenericNew; */
    vlcMedia_Type.tp_alloc = PyType_GenericAlloc;

    vlcInstance_Type.tp_alloc = PyType_GenericAlloc;
    MediaControl_Type.tp_alloc = PyType_GenericAlloc;

    p_module = Py_InitModule3( "vlc", vlc_methods,
                               "VLC media player embedding module." );

    if( !p_module )
      return;

    if( PyType_Ready( &PyPosition_Type ) < 0 )
        return;
    if( PyType_Ready( &MediaControl_Type ) < 0 )
        return;
    if( PyType_Ready( &vlcInstance_Type ) < 0 )
        return;
    if( PyType_Ready( &vlcMediaPlayer_Type ) < 0 )
        return;
    if( PyType_Ready( &vlcMedia_Type ) < 0 )
        return;

    /* Exceptions */
    MediaControl_InternalException =
            PyErr_NewException( "vlc.InternalException", NULL, NULL );
    Py_INCREF( MediaControl_InternalException );
    PyModule_AddObject( p_module, "InternalException",
                        MediaControl_InternalException );

    MediaControl_PositionKeyNotSupported =
            PyErr_NewException( "vlc.PositionKeyNotSupported", NULL, NULL );
    Py_INCREF( MediaControl_PositionKeyNotSupported );
    PyModule_AddObject( p_module, "PositionKeyNotSupported",
                        MediaControl_PositionKeyNotSupported );

    MediaControl_PositionOriginNotSupported=
            PyErr_NewException( "vlc.InvalidPosition", NULL, NULL );
    Py_INCREF( MediaControl_PositionOriginNotSupported );
    PyModule_AddObject( p_module, "PositionOriginNotSupported",
                        MediaControl_PositionOriginNotSupported );

    MediaControl_InvalidPosition =
            PyErr_NewException( "vlc.InvalidPosition", NULL, NULL );
    Py_INCREF( MediaControl_InvalidPosition );
    PyModule_AddObject( p_module, "InvalidPosition",
                        MediaControl_InvalidPosition );

    MediaControl_PlaylistException =
            PyErr_NewException( "vlc.PlaylistException", NULL, NULL );
    Py_INCREF( MediaControl_PlaylistException );
    PyModule_AddObject( p_module, "PlaylistException",
                        MediaControl_PlaylistException );

    /* Exceptions */
    vlc_Exception =
        PyErr_NewException( "vlc.InstanceException", NULL, NULL );
    Py_INCREF( vlc_Exception );
    PyModule_AddObject( p_module, "InstanceException",
                        vlc_Exception );

    /* Types */
    Py_INCREF( &PyPosition_Type );
    PyModule_AddObject( p_module, "Position",
                        ( PyObject * )&PyPosition_Type );

    Py_INCREF( &MediaControl_Type );
    PyModule_AddObject( p_module, "MediaControl",
                        ( PyObject * )&MediaControl_Type );

    Py_INCREF( &vlcInstance_Type );
    PyModule_AddObject( p_module, "Instance",
                        ( PyObject * )&vlcInstance_Type );

    Py_INCREF( &vlcMediaPlayer_Type );
    PyModule_AddObject( p_module, "MediaPlayer",
                        ( PyObject * )&vlcMediaPlayer_Type );

    Py_INCREF( &vlcMedia_Type );
    PyModule_AddObject( p_module, "Media",
                        ( PyObject * )&vlcMedia_Type );

    /* Constants */
    PyModule_AddIntConstant( p_module, "AbsolutePosition",
                             mediacontrol_AbsolutePosition );
    PyModule_AddIntConstant( p_module, "RelativePosition",
                             mediacontrol_RelativePosition );
    PyModule_AddIntConstant( p_module, "ModuloPosition",
                             mediacontrol_ModuloPosition );

    PyModule_AddIntConstant( p_module, "ByteCount",
                             mediacontrol_ByteCount );
    PyModule_AddIntConstant( p_module, "SampleCount",
                             mediacontrol_SampleCount );
    PyModule_AddIntConstant( p_module, "MediaTime",
                             mediacontrol_MediaTime );

    PyModule_AddIntConstant( p_module, "PlayingStatus",
                             mediacontrol_PlayingStatus );
    PyModule_AddIntConstant( p_module, "PauseStatus",
                             mediacontrol_PauseStatus );
    PyModule_AddIntConstant( p_module, "InitStatus",
                             mediacontrol_InitStatus );
    PyModule_AddIntConstant( p_module, "EndStatus",
                             mediacontrol_EndStatus );
    PyModule_AddIntConstant( p_module, "UndefinedStatus",
                             mediacontrol_UndefinedStatus );

}

/* Horrible hack... Please do not look.  Temporary workaround for the
   forward declaration mess of python types (cf vlcglue.h). If we do a
   separate compilation, we have to declare some types as extern. But
   the recommended way to forward declared types in python is
   static... I am sorting the mess but in the meantime, this will
   produce a working python module.
*/
#include "vlc_mediacontrol.c"
#include "vlc_position.c"
#include "vlc_instance.c"
#include "vlc_mediaplayer.c"
#include "vlc_media.c"
