/*****************************************************************************
 * vlc_mediacontrol.c: vlc.MediaControl binding
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

/*****************************************************************************
 * VLC MediaControl object implementation
 *****************************************************************************/

/* The MediaControl constructor takes either an existing vlc.Instance or a
   list of strings */
static PyObject *
MediaControl_new( PyTypeObject *type, PyObject *args, PyObject *kwds )
{
    MediaControl *self;
    mediacontrol_Exception *exception = NULL;
    PyObject* py_param = NULL;
    char** ppsz_args = NULL;
    libvlc_instance_t* p_instance = NULL;
    Py_ssize_t i_size = 0;

    self = PyObject_New( MediaControl, &MediaControl_Type );

    fprintf (stderr, "Instantiating mediacontrol\n");
    if( PyArg_ParseTuple( args, "O", &py_param ) )
    {
        if( PyObject_TypeCheck( py_param, &vlcInstance_Type ) == 1 )
        {
            p_instance = ((vlcInstance*)py_param)->p_instance;
        }
        else
        {
            Py_ssize_t i_index;

            Py_INCREF( py_param );
            if( ! PySequence_Check( py_param ) )
            {
                PyErr_SetString( PyExc_TypeError, "Parameter must be a vlc.Instance or a sequence of strings." );
                Py_DECREF( py_param );
                return NULL;
            }
            i_size = PySequence_Size( py_param );
            ppsz_args = malloc( ( i_size + 1 ) * sizeof( char * ) );
            if( ! ppsz_args )
            {
                PyErr_SetString( PyExc_MemoryError, "Out of memory" );
                Py_DECREF( py_param );
                return NULL;
            }

            for ( i_index = 0; i_index < i_size; i_index++ )
            {
                ppsz_args[i_index] =
                    strdup( PyString_AsString( PyObject_Str(
                                                   PySequence_GetItem( py_param,
                                                                       i_index ) ) ) );
            }
            ppsz_args[i_size] = NULL;
            Py_DECREF( py_param );
        }
    }
    else
    {
        /* No arguments were given. Clear the exception raised
           by PyArg_ParseTuple. */
        PyErr_Clear( );
    }

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    if( p_instance )
    {
        self->mc = mediacontrol_new_from_instance( p_instance, exception );
        Py_INCREF( py_param );
        self->vlc_instance = ( vlcInstance* ) py_param;
    }
    else
    {
        self->mc = mediacontrol_new( i_size, ppsz_args, exception );
        self->vlc_instance = PyObject_New( vlcInstance, &vlcInstance_Type );
        self->vlc_instance->p_instance = mediacontrol_get_libvlc_instance( LIBVLC_MC(self) );
    }
    MC_EXCEPT;
    Py_END_ALLOW_THREADS

    Py_INCREF( self );
    return ( PyObject * )self;
}

static void
MediaControl_dealloc( PyObject *self )
{
    fprintf(stderr, "MC dealloc\n");
    Py_DECREF( ((MediaControl*)self)->vlc_instance );
    PyObject_DEL( self );
}

static PyObject *
MediaControl_get_vlc_instance( PyObject *self, PyObject *args )
{
    vlcInstance *p_ret;

    p_ret = ((MediaControl*)self)->vlc_instance;
    Py_INCREF( p_ret );
    return ( PyObject * )p_ret;
}

static PyObject *
MediaControl_get_mediaplayer( PyObject *self, PyObject *args )
{
    vlcMediaPlayer *p_ret;

    p_ret = PyObject_New( vlcMediaPlayer, &vlcMediaPlayer_Type );
    p_ret->p_mp = mediacontrol_get_media_player( LIBVLC_MC(self) );
    Py_INCREF( p_ret );
    return ( PyObject * )p_ret;
}

/**
 *  Return the current position in the stream. The returned value can
   be relative or absolute ( according to PositionOrigin ) and the unit
   is set by PositionKey
 */
static PyObject *
MediaControl_get_media_position( PyObject *self, PyObject *args )
{
    mediacontrol_Position* pos;
    mediacontrol_Exception* exception = NULL;
    PyObject *py_origin;
    PyObject *py_key;
    PyObject *py_retval;
    mediacontrol_PositionOrigin origin;
    mediacontrol_PositionKey key;

    if( !PyArg_ParseTuple( args, "OO", &py_origin, &py_key ) )
        return NULL;

    origin = positionOrigin_py_to_c( py_origin );
    key    = positionKey_py_to_c( py_key );

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    pos = mediacontrol_get_media_position( LIBVLC_MC(self), origin, key, exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    py_retval = ( PyObject* )position_c_to_py( pos );
    free( pos );
    return py_retval;
}

/** Set the media position */
static PyObject *
MediaControl_set_media_position( PyObject *self, PyObject *args )
{
    mediacontrol_Exception* exception = NULL;
    mediacontrol_Position *a_position;
    PyObject *py_pos;

    if( !PyArg_ParseTuple( args, "O", &py_pos ) )
        return NULL;

    a_position = position_py_to_c( py_pos );
    if( !a_position )
    {
        PyErr_SetString( PyExc_MemoryError, "Out of memory" );
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_set_media_position( LIBVLC_MC(self), a_position, exception );
    free( a_position );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
MediaControl_start( PyObject *self, PyObject *args )
{
    mediacontrol_Position *a_position;
    mediacontrol_Exception *exception = NULL;
    PyObject *py_pos;

    if( !PyArg_ParseTuple( args, "O", &py_pos ) )
    {
        /* No argument. Use a default 0 value. */
        PyErr_Clear( );
        py_pos = NULL;
    }
    a_position = position_py_to_c( py_pos );
    if( !a_position )
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_start( LIBVLC_MC(self), a_position, exception );
    free( a_position );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
MediaControl_pause( PyObject *self, PyObject *args )
{
    mediacontrol_Exception *exception = NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_pause( LIBVLC_MC(self), exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

  Py_INCREF( Py_None );
  return Py_None;
}

static PyObject *
MediaControl_resume( PyObject *self, PyObject *args )
{
    mediacontrol_Exception *exception = NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_resume( LIBVLC_MC(self), exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
MediaControl_stop( PyObject *self, PyObject *args )
{
    mediacontrol_Exception *exception = NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_stop( LIBVLC_MC(self), exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
MediaControl_exit( PyObject *self, PyObject *args )
{
    mediacontrol_exit( LIBVLC_MC(self) );
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
MediaControl_set_mrl( PyObject *self, PyObject *args )
{
    char *psz_file;
    mediacontrol_Exception *exception = NULL;

    if( !PyArg_ParseTuple( args, "s", &psz_file ) )
      return NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_set_mrl( LIBVLC_MC(self), psz_file, exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
MediaControl_get_mrl( PyObject *self, PyObject *args )
{
    PyObject *py_retval;
    char* psz_file;
    mediacontrol_Exception *exception = NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    psz_file = mediacontrol_get_mrl( LIBVLC_MC(self), exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    py_retval = Py_BuildValue( "s", psz_file );
    free( psz_file );
    return py_retval;
}

static PyObject *
MediaControl_snapshot( PyObject *self, PyObject *args )
{
    mediacontrol_RGBPicture *p_retval = NULL;
    mediacontrol_Exception* exception = NULL;
    mediacontrol_Position *a_position = NULL;
    PyObject *py_pos = NULL;
    PyObject *py_obj = NULL;

    if( !PyArg_ParseTuple( args, "O", &py_pos ) )
      return NULL;

    a_position = position_py_to_c( py_pos );

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    p_retval = mediacontrol_snapshot( LIBVLC_MC(self), a_position, exception );
    free( a_position );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    if( !p_retval )
    {
        Py_INCREF( Py_None );
        return Py_None;
    }

    /* FIXME: create a real RGBPicture object */
    py_obj = PyDict_New();

    PyDict_SetItemString( py_obj, "width",
                          Py_BuildValue( "i", p_retval->width ) );
    PyDict_SetItemString( py_obj, "height",
                          Py_BuildValue( "i", p_retval->height ) );
    PyDict_SetItemString( py_obj, "type",
                          Py_BuildValue( "i", p_retval->type ) );
    PyDict_SetItemString( py_obj, "data",
                          Py_BuildValue( "s#", p_retval->data, p_retval->size ) );
    PyDict_SetItemString( py_obj, "date",
                          Py_BuildValue( "L", p_retval->date ) );

    mediacontrol_RGBPicture__free( p_retval );

    return py_obj;
}

static PyObject*
MediaControl_display_text( PyObject *self, PyObject *args )
{
    mediacontrol_Exception* exception = NULL;
    PyObject *py_begin, *py_end;
    char* message;
    mediacontrol_Position * begin;
    mediacontrol_Position * end;

    if( !PyArg_ParseTuple( args, "sOO", &message, &py_begin, &py_end ) )
        return NULL;

    begin = position_py_to_c( py_begin );
    end   = position_py_to_c( py_end );

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_display_text( LIBVLC_MC(self), message, begin, end, exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    free( begin );
    free( end );

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject*
MediaControl_get_stream_information( PyObject *self, PyObject *args )
{
    mediacontrol_StreamInformation *retval  = NULL;
    mediacontrol_Exception* exception = NULL;
    PyObject *py_obj;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    retval = mediacontrol_get_stream_information(
        LIBVLC_MC(self), mediacontrol_MediaTime, exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    py_obj = PyDict_New( );

     /* FIXME: create a real StreamInformation object */
    PyDict_SetItemString( py_obj, "status",
                  Py_BuildValue( "i", retval->streamstatus ) );
    PyDict_SetItemString( py_obj, "url",
                  Py_BuildValue( "s", retval->url ) );
    PyDict_SetItemString( py_obj, "position",
                  Py_BuildValue( "L", retval->position ) );
    PyDict_SetItemString( py_obj, "length",
                  Py_BuildValue( "L", retval->length ) );

    mediacontrol_StreamInformation__free( retval );

    return py_obj;
}

static PyObject*
MediaControl_sound_set_volume( PyObject *self, PyObject *args )
{
    mediacontrol_Exception* exception = NULL;
    unsigned short volume;

    if( !PyArg_ParseTuple( args, "H", &volume ) )
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_sound_set_volume( LIBVLC_MC(self), volume, exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject*
MediaControl_sound_get_volume( PyObject *self, PyObject *args )
{
    mediacontrol_Exception* exception = NULL;
    PyObject *py_retval;
    unsigned short volume;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    volume = mediacontrol_sound_get_volume( LIBVLC_MC(self), exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    py_retval = Py_BuildValue( "H", volume );
    return py_retval;
}

static PyObject*
MediaControl_set_rate( PyObject *self, PyObject *args )
{
    mediacontrol_Exception* exception = NULL;
    int rate;

    if( !PyArg_ParseTuple( args, "i", &rate ) )
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_set_rate( LIBVLC_MC(self), rate, exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject*
MediaControl_get_rate( PyObject *self, PyObject *args )
{
    mediacontrol_Exception* exception = NULL;
    PyObject *py_retval;
    int rate;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    rate = mediacontrol_get_rate( LIBVLC_MC(self), exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    py_retval = Py_BuildValue( "i", rate );
    return py_retval;
}

static PyObject*
MediaControl_set_fullscreen( PyObject *self, PyObject *args )
{
    mediacontrol_Exception* exception = NULL;
    int fs;

    if( !PyArg_ParseTuple( args, "i", &fs ) )
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_set_fullscreen( LIBVLC_MC(self), fs, exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject*
MediaControl_get_fullscreen( PyObject *self, PyObject *args )
{
    mediacontrol_Exception* exception = NULL;
    PyObject *py_retval;
    int fs;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    fs = mediacontrol_get_fullscreen( LIBVLC_MC(self), exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    py_retval = Py_BuildValue( "i", fs );
    return py_retval;
}

static PyObject*
MediaControl_set_visual( PyObject *self, PyObject *args )
{
    mediacontrol_Exception* exception = NULL;
    WINDOWHANDLE visual;

    if( !PyArg_ParseTuple( args, "i", &visual ) )
       return NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_set_visual( LIBVLC_MC(self), visual, exception );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyMethodDef MediaControl_methods[] =
{
    { "get_vlc_instance", MediaControl_get_vlc_instance, METH_VARARGS,
      "get_vlc_instance( ) -> Instance    Get embedded vlc.Instance." },
    { "get_mediaplayer", MediaControl_get_mediaplayer, METH_VARARGS,
      "get_mediaplayer( ) -> MediaPlayer    Get embedded vlc.MediaPlayer." },
    { "get_media_position", MediaControl_get_media_position, METH_VARARGS,
      "get_media_position( origin, key ) -> Position    Get current media position." },
    { "set_media_position", MediaControl_set_media_position, METH_VARARGS,
      "set_media_position( Position )            Set media position" },
    { "start", MediaControl_start, METH_VARARGS,
      "start( Position )         Start the player." },
    { "pause", MediaControl_pause, METH_VARARGS,
      "pause( Position )         Pause the player." },
    { "resume", MediaControl_resume, METH_VARARGS,
      "resume( Position )        Resume the player" },
    { "stop", MediaControl_stop, METH_VARARGS,
      "stop( Position )              Stop the player" },
    { "exit", MediaControl_exit, METH_VARARGS,
      "exit( )                     Exit the player" },
    { "set_mrl", MediaControl_set_mrl, METH_VARARGS,
      "set_mrl( str )               Set the file to be played" },
    { "get_mrl", MediaControl_get_mrl, METH_VARARGS,
      "get_mrl( ) -> str       Get the played file" },
    { "snapshot", MediaControl_snapshot, METH_VARARGS,
      "snapshot( Position ) -> dict        Take a snapshot" },
    { "display_text", MediaControl_display_text, METH_VARARGS,
      "display_text( str, Position, Position )    Display a text on the video" },
    { "get_stream_information", MediaControl_get_stream_information,
      METH_VARARGS,
      "get_stream_information( ) -> dict      Get information about the stream"},
    { "sound_get_volume", MediaControl_sound_get_volume, METH_VARARGS,
      "sound_get_volume( ) -> int       Get the volume" },
    { "sound_set_volume", MediaControl_sound_set_volume, METH_VARARGS,
      "sound_set_volume( int )           Set the volume" },
    { "set_visual", MediaControl_set_visual, METH_VARARGS,
      "set_visual( int )           Set the embedding window visual ID" },
    { "get_rate", MediaControl_get_rate, METH_VARARGS,
      "get_rate( ) -> int       Get the rate" },
    { "set_rate", MediaControl_set_rate, METH_VARARGS,
      "set_rate( int )              Set the rate" },
    { "get_fullscreen", MediaControl_get_fullscreen, METH_VARARGS,
      "get_fullscreen( ) -> int       Get the fullscreen status" },
    { "set_fullscreen", MediaControl_set_fullscreen, METH_VARARGS,
      "set_fullscreen( int )              Set the fullscreen status" },
    { NULL, NULL, 0, NULL },
};

static PyTypeObject MediaControl_Type =
{
    PyObject_HEAD_INIT( NULL )
    0,                         /*ob_size*/
    "vlc.MediaControl",        /*tp_name*/
    sizeof( MediaControl_Type ), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    ( destructor )MediaControl_dealloc,      /*tp_dealloc*/
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
    "Control of a VLC instance.\n\nvlc.MediaControl(args): initialisation with a list of VLC parameters.\nvlc.MediaControl(instance): initialisation with an existing vlc.Instance",  /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    MediaControl_methods,             /* tp_methods */
    0,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    MediaControl_new,          /* tp_new */
};
