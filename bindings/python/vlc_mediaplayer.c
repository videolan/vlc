/*****************************************************************************
 * vlc_mediaplayer.c: vlc.MediaPlayer binding
 *****************************************************************************
 * Copyright (C) 2006,2007,2008,2009 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <olivier.aubert at liris.cnrs.fr>
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
vlcMediaPlayer_get_length( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int64_t i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_player_get_length( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "L", i_ret );
}

static PyObject *
vlcMediaPlayer_get_time( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int64_t i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_player_get_time( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "L", i_ret );
}

static PyObject *
vlcMediaPlayer_set_time( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int64_t i_time;

    if( !PyArg_ParseTuple( args, "L", &i_time ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_player_set_time( LIBVLC_MEDIAPLAYER(self), i_time, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_get_position( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_ret;
    LIBVLC_TRY;
    f_ret = libvlc_media_player_get_position( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "f", f_ret );
}

static PyObject *
vlcMediaPlayer_set_position( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_pos;

    if( !PyArg_ParseTuple( args, "f", &f_pos ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_player_set_position( LIBVLC_MEDIAPLAYER(self), f_pos, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_will_play( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_player_will_play( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_get_rate( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_ret;
    LIBVLC_TRY;
    f_ret = libvlc_media_player_get_rate( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "f", f_ret );
}

static PyObject *
vlcMediaPlayer_set_rate( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_rate;

    if( !PyArg_ParseTuple( args, "f", &f_rate ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_player_set_rate( LIBVLC_MEDIAPLAYER(self), f_rate, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_get_state( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_player_get_state( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_has_vout( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_player_has_vout( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_get_fps( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_ret;
    LIBVLC_TRY;
    f_ret = libvlc_media_player_get_fps( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "f", f_ret );
}

static PyObject *
vlcMediaPlayer_audio_get_track( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_audio_get_track( LIBVLC_MEDIAPLAYER(self), &ex );
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_audio_set_track( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_track;

    if( !PyArg_ParseTuple( args, "i", &i_track ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_audio_set_track( LIBVLC_MEDIAPLAYER(self), i_track, &ex );
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_get_chapter( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_media_player_get_chapter( LIBVLC_MEDIAPLAYER(self), &ex );
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_get_chapter_count( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_media_player_get_chapter_count( LIBVLC_MEDIAPLAYER(self), &ex );
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_set_chapter( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_chapter;

    if( !PyArg_ParseTuple( args, "i", &i_chapter ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_player_set_chapter( LIBVLC_MEDIAPLAYER(self), i_chapter, &ex );
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}


static PyObject *
vlcMediaPlayer_toggle_fullscreen( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;

    LIBVLC_TRY;
    libvlc_toggle_fullscreen( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_set_fullscreen( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_fullscreen;

    if( !PyArg_ParseTuple( args, "i", &i_fullscreen ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_set_fullscreen( LIBVLC_MEDIAPLAYER(self), i_fullscreen, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_get_fullscreen( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_get_fullscreen( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_get_height( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_video_get_height( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_get_width( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_video_get_width( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_get_aspect_ratio( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_ret;
    PyObject* o_ret;

    LIBVLC_TRY;
    psz_ret = libvlc_video_get_aspect_ratio( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    o_ret=Py_BuildValue( "s", psz_ret );
    free( psz_ret );
    return o_ret;
}

static PyObject *
vlcMediaPlayer_set_aspect_ratio( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_ratio;

    if( !PyArg_ParseTuple( args, "s", &psz_ratio ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_video_set_aspect_ratio( LIBVLC_MEDIAPLAYER(self), psz_ratio, &ex);
    LIBVLC_EXCEPT;
    free( psz_ratio );
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_video_take_snapshot( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_filename;

    if( !PyArg_ParseTuple( args, "s", &psz_filename ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_video_take_snapshot( LIBVLC_MEDIAPLAYER(self), psz_filename, 0, 0, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_is_seekable( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_player_is_seekable( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_can_pause( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_player_can_pause( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_play( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;

    LIBVLC_TRY;
    libvlc_media_player_play( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_pause( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;

    LIBVLC_TRY;
    libvlc_media_player_pause( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_stop( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;

    LIBVLC_TRY;
    libvlc_media_player_stop( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_set_xwindow( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    uint32_t i_drawable;

    if( !PyArg_ParseTuple( args, "i", &i_drawable ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_player_set_xwindow( LIBVLC_MEDIAPLAYER(self), i_drawable, &ex );
    LIBVLC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_get_xwindow( PyObject *self, PyObject *args )
{
    uint32_t i_ret;

    i_ret = libvlc_media_player_get_xwindow( LIBVLC_MEDIAPLAYER(self));
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_set_hwnd( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    void* i_drawable;

    if( !PyArg_ParseTuple( args, "l", &i_drawable ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_player_set_hwnd( LIBVLC_MEDIAPLAYER(self), (void*) i_drawable, &ex );
    LIBVLC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_get_hwnd( PyObject *self, PyObject *args )
{
    void* i_ret;

    i_ret = libvlc_media_player_get_hwnd( LIBVLC_MEDIAPLAYER(self));
    return Py_BuildValue( "l", i_ret );
}

static PyObject *
vlcMediaPlayer_set_agl( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    uint32_t i_drawable;

    if( !PyArg_ParseTuple( args, "i", &i_drawable ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_player_set_agl( LIBVLC_MEDIAPLAYER(self), i_drawable, &ex );
    LIBVLC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_get_agl( PyObject *self, PyObject *args )
{
    uint32_t i_ret;

    i_ret = libvlc_media_player_get_agl( LIBVLC_MEDIAPLAYER(self));
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_set_nsobject( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    void* i_drawable;

    if( !PyArg_ParseTuple( args, "l", &i_drawable ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_player_set_nsobject( LIBVLC_MEDIAPLAYER(self), (void*) i_drawable, &ex );
    LIBVLC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMediaPlayer_get_nsobject( PyObject *self, PyObject *args )
{
    void* i_ret;

    i_ret = libvlc_media_player_get_nsobject( LIBVLC_MEDIAPLAYER(self));
    return Py_BuildValue( "l", i_ret );
}

static PyObject *
vlcMediaPlayer_set_media( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    PyObject* py_param = NULL;

    if( !PyArg_ParseTuple( args, "O", &py_param ) )
        return NULL;
    if( PyObject_TypeCheck( py_param, &vlcMedia_Type ) == 1 )
    {
        LIBVLC_TRY;
        libvlc_media_player_set_media( LIBVLC_MEDIAPLAYER(self), ((vlcMedia*)py_param)->p_media, &ex );
        LIBVLC_EXCEPT;
    }
    else
    {
        PyObject *py_exc = vlc_Exception;
        PyErr_SetString( py_exc, "vlc.Media parameter needed" );
        return NULL;
    }
    return NULL;
}

static PyObject *
vlcMediaPlayer_get_media( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    libvlc_media_t *p_media;
    vlcMedia *p_ret;

    LIBVLC_TRY;
    p_media = libvlc_media_player_get_media( LIBVLC_MEDIAPLAYER(self), &ex );
    LIBVLC_EXCEPT;

    if( !p_media )
    {
        Py_INCREF( Py_None );
        return Py_None;
    }
    else
    {
        p_ret = PyObject_New( vlcMedia, &vlcMedia_Type );
        p_ret->p_media = p_media;
        Py_INCREF( p_ret ); /* Ah bon ? */
        return ( PyObject * )p_ret;
    }
}

static PyObject *
vlcMediaPlayer_get_spu( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_video_get_spu( LIBVLC_MEDIAPLAYER(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcMediaPlayer_set_spu( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_spu;

    if( !PyArg_ParseTuple( args, "i", &i_spu ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_video_set_spu( LIBVLC_MEDIAPLAYER(self), i_spu, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}


static PyMethodDef vlcMediaPlayer_methods[] =
{
    { "get_length", vlcMediaPlayer_get_length, METH_VARARGS,
      "get_length() -> long    " },
    { "get_time", vlcMediaPlayer_get_time, METH_VARARGS,
      "get_time() -> long" },
    { "set_time", vlcMediaPlayer_set_time, METH_VARARGS,
      "set_time(long)" },
    { "get_position", vlcMediaPlayer_get_position, METH_VARARGS,
      "get_position() -> float" },
    { "set_position", vlcMediaPlayer_set_position, METH_VARARGS,
      "set_position(float)" },
    { "will_play", vlcMediaPlayer_will_play, METH_VARARGS,
      "will_play() -> int" },
    { "is_seekable", vlcMediaPlayer_is_seekable, METH_VARARGS,
      "is_seekable() -> int" },
    { "can_pause", vlcMediaPlayer_can_pause, METH_VARARGS,
      "can_pause() -> int" },
    { "get_rate", vlcMediaPlayer_get_rate, METH_VARARGS,
      "get_rate() -> float" },
    { "set_rate", vlcMediaPlayer_set_rate, METH_VARARGS,
      "set_rate(float)" },
    { "get_state", vlcMediaPlayer_get_state, METH_VARARGS,
      "get_state() -> int" },
    { "has_vout", vlcMediaPlayer_has_vout, METH_VARARGS,
      "has_vout() -> int" },
    { "get_fps", vlcMediaPlayer_get_fps, METH_VARARGS,
      "get_fps() -> float" },
    { "audio_get_track", vlcMediaPlayer_audio_get_track, METH_VARARGS,
      "audio_get_track() -> int    Get current audio track" },
    { "audio_set_track", vlcMediaPlayer_audio_set_track, METH_VARARGS,
      "audio_set_track(int)        Set current audio track" },
    { "toggle_fullscreen", vlcMediaPlayer_toggle_fullscreen, METH_VARARGS,
      "toggle_fullscreen()    Toggle fullscreen status on video output" },
    { "set_fullscreen", vlcMediaPlayer_set_fullscreen, METH_VARARGS,
      "set_fullscreen(bool)    Enable or disable fullscreen on a video output" },
    { "get_fullscreen", vlcMediaPlayer_get_fullscreen, METH_VARARGS,
      "get_fullscreen() -> bool    Get current fullscreen status" },
    { "get_height", vlcMediaPlayer_get_height, METH_VARARGS,
      "get_height() -> int           Get current video height" },
    { "get_width", vlcMediaPlayer_get_width, METH_VARARGS,
      "get_width() -> int           Get current video width" },
    { "get_aspect_ratio", vlcMediaPlayer_get_aspect_ratio, METH_VARARGS,
      "get_aspect_ratio() -> str    Get current video aspect ratio" },
    { "set_aspect_ratio", vlcMediaPlayer_set_aspect_ratio, METH_VARARGS,
      "set_aspect_ratio(str)        Set new video aspect ratio" },
    { "video_take_snapshot", vlcMediaPlayer_video_take_snapshot, METH_VARARGS,
      "video_take_snapshot(filename=str)        Take a snapshot of the current video window" },

    { "play", vlcMediaPlayer_play, METH_VARARGS,
      "play()    Play the media instance" },
    { "pause", vlcMediaPlayer_pause, METH_VARARGS,
      "pause()   Pause the media instance" },
    { "stop", vlcMediaPlayer_stop, METH_VARARGS,
      "stop()    Stop the media instance" },

    { "set_xwindow", vlcMediaPlayer_set_xwindow, METH_VARARGS,
      "set_xwindow()    Set the X-Window id" },
    { "set_nsobject", vlcMediaPlayer_set_nsobject, METH_VARARGS,
      "set_nsobject()    Set the NSObject" },
    { "set_agl", vlcMediaPlayer_set_agl, METH_VARARGS,
      "set_agl()    Set the AGL" },
    { "set_hwnd", vlcMediaPlayer_set_hwnd, METH_VARARGS,
      "set_hwndl()    Set the HWND" },

    { "get_xwindow", vlcMediaPlayer_get_xwindow, METH_VARARGS,
      "get_xwindow()    Set the X-Window id" },
    { "get_nsobject", vlcMediaPlayer_get_nsobject, METH_VARARGS,
      "get_nsobject()    Set the NSObject" },
    { "get_agl", vlcMediaPlayer_get_agl, METH_VARARGS,
      "get_agl()    Set the AGL" },
    { "get_hwnd", vlcMediaPlayer_get_hwnd, METH_VARARGS,
      "get_hwndl()    Set the HWND" },

    { "get_chapter", vlcMediaPlayer_get_chapter, METH_VARARGS,
      "get_chapter() -> int    Get current chapter" },
    { "set_chapter", vlcMediaPlayer_set_chapter, METH_VARARGS,
      "set_chapter(int)        Set current chapter" },
    { "get_chapter_count", vlcMediaPlayer_get_chapter_count, METH_VARARGS,
      "get_chapter_count() -> int    Get current chapter count" },

    { "set_media", vlcMediaPlayer_set_media, METH_VARARGS,
      "set_media(vlc.Media)        Set the media that will be used by the media_player" },
    { "get_media", vlcMediaPlayer_get_media, METH_VARARGS,
      "get_media() -> vlc.Media    Get the media used by the media_player (if any)." },

    { "get_spu", vlcMediaPlayer_get_spu, METH_VARARGS,
      "get_spu() -> int   Get current video subtitle" },
    { "set_spu", vlcMediaPlayer_set_spu, METH_VARARGS,
      "set_spu(int)      Set new video subtitle" },

    { NULL }  /* Sentinel */
};

static PyTypeObject vlcMediaPlayer_Type =
{
    PyObject_HEAD_INIT( NULL )
    0,                         /*ob_size*/
    "vlc.MediaPlayer",            /*tp_name*/
    sizeof( vlcMediaPlayer_Type ),   /*tp_basicsize*/
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
    "vlc.MediaPlayer object\n\nIt cannot be instantiated standalone, it must be obtained from an existing vlc.Instance object",  /* tp_doc */
    0,                        /* tp_traverse */
    0,                        /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                          /* tp_iternext */
    vlcMediaPlayer_methods,          /* tp_methods */
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

