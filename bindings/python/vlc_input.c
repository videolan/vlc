/*****************************************************************************
 * vlc_input.c: vlc.Input binding
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

/***********************************************************************
 * vlc.Input
 ***********************************************************************/

static PyObject *
vlcMediaInstance_get_length( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    vlc_int64_t i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_instance_get_length( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "L", i_ret );
}

static PyObject *
vlcMediaInstance_get_time( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    vlc_int64_t i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_instance_get_time( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "L", i_ret );
}

static PyObject *
vlcMediaInstance_set_time( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    vlc_int64_t i_time;

    if( !PyArg_ParseTuple( args, "L", &i_time ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_instance_set_time( LIBVLC_MEDIAINSTANCE->p_mi, i_time, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_get_position( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_ret;
    LIBVLC_TRY;
    f_ret = libvlc_media_instance_get_position( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "f", f_ret );
}

static PyObject *
vlcMediaInstance_set_position( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_pos;

    if( !PyArg_ParseTuple( args, "f", &f_pos ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_instance_set_position( LIBVLC_MEDIAINSTANCE->p_mi, f_pos, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_will_play( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_instance_will_play( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_get_rate( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_ret;
    LIBVLC_TRY;
    f_ret = libvlc_media_instance_get_rate( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "f", f_ret );
}

static PyObject *
vlcMediaInstance_set_rate( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_rate;

    if( !PyArg_ParseTuple( args, "f", &f_rate ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_instance_set_rate( LIBVLC_MEDIAINSTANCE->p_mi, f_rate, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_get_state( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_instance_get_state( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_has_vout( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_instance_has_vout( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_get_fps( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_ret;
    LIBVLC_TRY;
    f_ret = libvlc_media_instance_get_fps( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "f", f_ret );
}

static PyObject *
vlcMediaInstance_audio_get_track( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_audio_get_track( LIBVLC_MEDIAINSTANCE->p_mi, &ex );
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_audio_set_track( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_track;

    if( !PyArg_ParseTuple( args, "i", &i_track ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_audio_set_track( LIBVLC_MEDIAINSTANCE->p_mi, i_track, &ex );
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_get_chapter( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_media_instance_get_chapter( LIBVLC_MEDIAINSTANCE->p_mi, &ex );
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_get_chapter_count( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_media_instance_get_chapter_count( LIBVLC_MEDIAINSTANCE->p_mi, &ex );
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_set_chapter( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_chapter;

    if( !PyArg_ParseTuple( args, "i", &i_chapter ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_instance_set_chapter( LIBVLC_MEDIAINSTANCE->p_mi, i_chapter, &ex );
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}


static PyObject *
vlcMediaInstance_toggle_fullscreen( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;

    LIBVLC_TRY;
    libvlc_toggle_fullscreen( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_set_fullscreen( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_fullscreen;

    if( !PyArg_ParseTuple( args, "i", &i_fullscreen ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_set_fullscreen( LIBVLC_MEDIAINSTANCE->p_mi, i_fullscreen, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_get_fullscreen( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_get_fullscreen( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_get_height( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_video_get_height( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_get_width( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_video_get_width( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_get_aspect_ratio( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_ret;
    PyObject* o_ret;

    LIBVLC_TRY;
    psz_ret = libvlc_video_get_aspect_ratio( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    o_ret=Py_BuildValue( "s", psz_ret );
    free( psz_ret );
    return o_ret;
}

static PyObject *
vlcMediaInstance_set_aspect_ratio( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_ratio;

    if( !PyArg_ParseTuple( args, "s", &psz_ratio ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_video_set_aspect_ratio( LIBVLC_MEDIAINSTANCE->p_mi, psz_ratio, &ex);
    LIBVLC_EXCEPT;
    free( psz_ratio );
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_video_take_snapshot( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_filename;

    if( !PyArg_ParseTuple( args, "s", &psz_filename ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_video_take_snapshot( LIBVLC_MEDIAINSTANCE->p_mi, psz_filename, 0, 0, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_video_resize( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_width;
    int i_height;

    if( !PyArg_ParseTuple( args, "ii", &i_width, &i_height ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_video_resize( LIBVLC_MEDIAINSTANCE->p_mi, i_width, i_height, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_video_reparent( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    WINDOWHANDLE i_visual;
    int i_ret;

    if( !PyArg_ParseTuple( args, "i", &i_visual ) )
        return NULL;

    LIBVLC_TRY;
    i_ret = libvlc_video_reparent( LIBVLC_MEDIAINSTANCE->p_mi, i_visual, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_is_seekable( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_instance_is_seekable( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_can_pause( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_instance_can_pause( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_play( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;

    LIBVLC_TRY;
    libvlc_media_instance_play( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_pause( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;

    LIBVLC_TRY;
    libvlc_media_instance_pause( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_stop( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;

    LIBVLC_TRY;
    libvlc_media_instance_stop( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_set_drawable( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_drawable;

    if( !PyArg_ParseTuple( args, "i", &i_drawable ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_instance_set_drawable( LIBVLC_MEDIAINSTANCE->p_mi, (libvlc_drawable_t) i_drawable, &ex );
    LIBVLC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaInstance_set_media_descriptor( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    PyObject* py_param = NULL;

    if( !PyArg_ParseTuple( args, "O", &py_param ) )
        return NULL;
    if( PyObject_TypeCheck( py_param, &vlcMediaDescriptor_Type ) == 1 )
    {
        LIBVLC_TRY;
        libvlc_media_instance_set_media_descriptor( LIBVLC_MEDIAINSTANCE->p_mi, ((vlcMediaDescriptor*)py_param)->p_md, &ex );
        LIBVLC_EXCEPT;
    }
    else
    {
        PyObject *py_exc = vlcInstance_Exception;
        PyErr_SetString( py_exc, "vlc.MediaDescriptor parameter needed" );
        return NULL;
    }
    return NULL;
}

static PyObject *
vlcMediaInstance_get_media_descriptor( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    libvlc_media_descriptor_t *p_md;
    vlcMediaDescriptor *p_ret;

    LIBVLC_TRY;
    p_md = libvlc_media_instance_get_media_descriptor( LIBVLC_MEDIAINSTANCE->p_mi, &ex );
    LIBVLC_EXCEPT;

    if( !p_md )
    {
        Py_INCREF( Py_None );
        return Py_None;
    }
    else
    {
        p_ret = PyObject_New( vlcMediaDescriptor, &vlcMediaDescriptor_Type );
        p_ret->p_md = p_md;
        Py_INCREF( p_ret ); /* Ah bon ? */
        return ( PyObject * )p_ret;
    }
}

static PyObject *
vlcMediaInstance_get_spu( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_video_get_spu( LIBVLC_MEDIAINSTANCE->p_mi, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaInstance_set_spu( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_spu;

    if( !PyArg_ParseTuple( args, "i", &i_spu ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_video_set_spu( LIBVLC_MEDIAINSTANCE->p_mi, i_spu, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}


static PyMethodDef vlcMediaInstance_methods[] =
{
    { "get_length", vlcMediaInstance_get_length, METH_VARARGS,
      "get_length() -> long    " },
    { "get_time", vlcMediaInstance_get_time, METH_VARARGS,
      "get_time() -> long" },
    { "set_time", vlcMediaInstance_set_time, METH_VARARGS,
      "set_time(long)" },
    { "get_position", vlcMediaInstance_get_position, METH_VARARGS,
      "get_position() -> float" },
    { "set_position", vlcMediaInstance_set_position, METH_VARARGS,
      "set_position(float)" },
    { "will_play", vlcMediaInstance_will_play, METH_VARARGS,
      "will_play() -> int" },
    { "is_seekable", vlcMediaInstance_is_seekable, METH_VARARGS,
      "is_seekable() -> int" },
    { "can_pause", vlcMediaInstance_can_pause, METH_VARARGS,
      "can_pause() -> int" },
    { "get_rate", vlcMediaInstance_get_rate, METH_VARARGS,
      "get_rate() -> float" },
    { "set_rate", vlcMediaInstance_set_rate, METH_VARARGS,
      "set_rate(float)" },
    { "get_state", vlcMediaInstance_get_state, METH_VARARGS,
      "get_state() -> int" },
    { "has_vout", vlcMediaInstance_has_vout, METH_VARARGS,
      "has_vout() -> int" },
    { "get_fps", vlcMediaInstance_get_fps, METH_VARARGS,
      "get_fps() -> float" },
    { "audio_get_track", vlcMediaInstance_audio_get_track, METH_VARARGS,
      "audio_get_track() -> int    Get current audio track" },
    { "audio_set_track", vlcMediaInstance_audio_set_track, METH_VARARGS,
      "audio_set_track(int)        Set current audio track" },
    { "toggle_fullscreen", vlcMediaInstance_toggle_fullscreen, METH_VARARGS,
      "toggle_fullscreen()    Toggle fullscreen status on video output" },
    { "set_fullscreen", vlcMediaInstance_set_fullscreen, METH_VARARGS,
      "set_fullscreen(bool)    Enable or disable fullscreen on a video output" },
    { "get_fullscreen", vlcMediaInstance_get_fullscreen, METH_VARARGS,
      "get_fullscreen() -> bool    Get current fullscreen status" },
    { "get_height", vlcMediaInstance_get_height, METH_VARARGS,
      "get_height() -> int           Get current video height" },
    { "get_width", vlcMediaInstance_get_width, METH_VARARGS,
      "get_width() -> int           Get current video width" },
    { "get_aspect_ratio", vlcMediaInstance_get_aspect_ratio, METH_VARARGS,
      "get_aspect_ratio() -> str    Get current video aspect ratio" },
    { "set_aspect_ratio", vlcMediaInstance_set_aspect_ratio, METH_VARARGS,
      "set_aspect_ratio(str)        Set new video aspect ratio" },
    { "video_take_snapshot", vlcMediaInstance_video_take_snapshot, METH_VARARGS,
      "video_take_snapshot(filename=str)        Take a snapshot of the current video window" },
    { "video_resize", vlcMediaInstance_video_resize, METH_VARARGS,
      "video_resize(width=int, height=int)      Resize the current video output window" },
    { "video_reparent", vlcMediaInstance_video_reparent, METH_VARARGS,
      "video_reparent(visual=int)               change the parent for the current video output" },

    { "play", vlcMediaInstance_play, METH_VARARGS,
      "play()    Play the media instance" },
    { "pause", vlcMediaInstance_pause, METH_VARARGS,
      "pause()   Pause the media instance" },
    { "stop", vlcMediaInstance_stop, METH_VARARGS,
      "stop()    Stop the media instance" },
    { "set_drawable", vlcMediaInstance_set_drawable, METH_VARARGS,
      "set_drawable()    Set the drawable id" },

    { "get_chapter", vlcMediaInstance_get_chapter, METH_VARARGS,
      "get_chapter() -> int    Get current chapter" },
    { "set_chapter", vlcMediaInstance_set_chapter, METH_VARARGS,
      "set_chapter(int)        Set current chapter" },
    { "get_chapter_count", vlcMediaInstance_get_chapter_count, METH_VARARGS,
      "get_chapter_count() -> int    Get current chapter count" },

    { "set_mediadescriptor", vlcMediaInstance_set_media_descriptor, METH_VARARGS,
      "set_mediadescriptor(vlc.MediaDescriptor)        Set the media descriptor that will be used by the media_instance" },
    { "get_mediadescriptor", vlcMediaInstance_get_media_descriptor, METH_VARARGS,
      "get_mediadescriptor() -> vlc.MediaDescriptor    Get the media descriptor used by the media_instance (if any)." },

    { "get_spu", vlcMediaInstance_get_spu, METH_VARARGS,
      "get_spu() -> int   Get current video subtitle" },
    { "set_spu", vlcMediaInstance_set_spu, METH_VARARGS,
      "set_spu(int)      Set new video subtitle" },

    { NULL }  /* Sentinel */
};

static PyTypeObject vlcMediaInstance_Type =
{
    PyObject_HEAD_INIT( NULL )
    0,                         /*ob_size*/
    "vlc.MediaInstance",            /*tp_name*/
    sizeof( vlcMediaInstance_Type ),   /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "vlc.MediaInstance object\n\nIt cannot be instanciated standalone, it must be obtained from an existing vlc.Instance object",  /* tp_doc */
    0,                        /* tp_traverse */
    0,                        /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                          /* tp_iternext */
    vlcMediaInstance_methods,          /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

