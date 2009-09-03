/*****************************************************************************
 * vlc_instance.c: vlc.Instance binding
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

/* Helper functions */
static Py_ssize_t
pyoptions_to_args(PyObject *py_options, char*** pppsz_args)
{
    Py_ssize_t i_size;
    Py_ssize_t  i_index;

    Py_INCREF( py_options );
    if( ! PySequence_Check( py_options ) )
    {
        PyErr_SetString( PyExc_TypeError, "Parameter must be a sequence." );
        return -1;
    }
    i_size = PySequence_Size( py_options );

    char **ppsz_args = *pppsz_args = malloc( ( i_size + 1 ) * sizeof( char * ) );

    if( ! ppsz_args )
    {
        PyErr_SetString( PyExc_MemoryError, "Out of memory" );
        return -1;
    }

    for ( i_index = 0; i_index < i_size; i_index++ )
    {
        ppsz_args[i_index] =
            strdup( PyString_AsString( PyObject_Str(
                                           PySequence_GetItem( py_options,
                                                               i_index ) ) ) );
    }
    ppsz_args[i_size] = NULL;
    Py_DECREF( py_options );
    return i_size;
}

static void
free_args(int i_size, char** ppsz_args)
{
    int i_index;

    for ( i_index = 0; i_index < i_size; i_index++ )
        free( ppsz_args[i_index] );
    free( ppsz_args );
}

/*****************************************************************************
 * Instance object implementation
 *****************************************************************************/

static PyObject *
vlcInstance_new( PyTypeObject *type, PyObject *args, PyObject *kwds )
{
    vlcInstance *self;
    libvlc_exception_t ex;
    PyObject* py_list = NULL;
    char** ppsz_args = NULL;
    int i_size = 0;

    fprintf(stderr, "Instantiating\n");
    if( PyArg_ParseTuple( args, "|O", &py_list ) )
    {
        i_size = pyoptions_to_args( py_list, &ppsz_args );
        if( i_size < 0 )
            return NULL;
    }
    else
    {
        /* No arguments were given. Clear the exception raised
           by PyArg_ParseTuple. */
        PyErr_Clear( );
    }

    self = PyObject_New( vlcInstance, &vlcInstance_Type );

    Py_BEGIN_ALLOW_THREADS
    LIBVLC_TRY
    LIBVLC_INSTANCE(self) = libvlc_new( i_size, ppsz_args, &ex );
    free_args( i_size, ppsz_args );
    LIBVLC_EXCEPT
    Py_END_ALLOW_THREADS

    Py_INCREF( self );
    return ( PyObject * )self;
}

static void
vlcInstance_dealloc( PyObject *self )
{
    libvlc_release( LIBVLC_INSTANCE(self) );
    PyObject_DEL( self );
}

static PyObject *
vlcInstance_new_media_player( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    libvlc_media_player_t *p_mp;
    vlcMediaPlayer *p_ret;

    LIBVLC_TRY;
    p_mp = libvlc_media_player_new( LIBVLC_INSTANCE(self), &ex );
    LIBVLC_EXCEPT;

    p_ret = PyObject_New( vlcMediaPlayer, &vlcMediaPlayer_Type );
    p_ret->p_mp = p_mp;
    Py_INCREF( p_ret ); /* Ah bon ? */
    return ( PyObject * )p_ret;
}

static PyObject *
vlcInstance_audio_toggle_mute( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    LIBVLC_TRY;
    libvlc_audio_toggle_mute( LIBVLC_INSTANCE(self), &ex );
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_audio_get_mute( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_audio_get_mute( LIBVLC_INSTANCE(self), &ex );
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcInstance_audio_set_mute( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_mute;

    if( !PyArg_ParseTuple( args, "i", &i_mute ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_audio_set_mute( LIBVLC_INSTANCE(self), i_mute, &ex );
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_audio_get_volume( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_audio_get_volume( LIBVLC_INSTANCE(self), &ex );
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcInstance_audio_set_volume( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_volume;

    if( !PyArg_ParseTuple( args, "i", &i_volume ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_audio_set_volume( LIBVLC_INSTANCE(self), i_volume, &ex );
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_audio_get_channel( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;

    LIBVLC_TRY;
    i_ret = libvlc_audio_get_channel( LIBVLC_INSTANCE(self), &ex );
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcInstance_audio_set_channel( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_channel;

    if( !PyArg_ParseTuple( args, "i", &i_channel ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_audio_set_channel( LIBVLC_INSTANCE(self), i_channel, &ex );
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

/* vlm_add_broadcast : name, input MRL, output MRL
   Keywords: options, enable, loop */
static PyObject *
vlcInstance_vlm_add_broadcast( PyObject *self, PyObject *args, PyObject *kwds )
{
    libvlc_exception_t ex;
    static char *kwlist[] = { "name", "input", "output",
                              "options", "enable", "loop", NULL};
    char* psz_name = NULL;
    char* psz_input = NULL;
    char* psz_output = NULL;
    PyObject* py_options = NULL;
    int i_enable = 1;
    int i_loop = 0;
    int i_size = 0;
    char** ppsz_args = NULL;

    if( !PyArg_ParseTupleAndKeywords( args, kwds, "sss|Oii", kwlist,
                                      &psz_name,
                      &psz_input, &psz_output,
                      &py_options, &i_enable, &i_loop ) )
        return NULL;

    if( py_options )
    {
        i_size = pyoptions_to_args( py_options, &ppsz_args );
    }

    LIBVLC_TRY;
    libvlc_vlm_add_broadcast( LIBVLC_INSTANCE(self),
                              psz_name, psz_input, psz_output,
                              i_size, ppsz_args, i_enable, i_loop, &ex);
    free_args( i_size, ppsz_args );
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_del_media( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;

    if( !PyArg_ParseTuple( args, "s", &psz_name ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_vlm_del_media( LIBVLC_INSTANCE(self), psz_name, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_set_enabled( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;
    int i_enabled;

    if( !PyArg_ParseTuple( args, "si", &psz_name, &i_enabled ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_vlm_set_enabled( LIBVLC_INSTANCE(self), psz_name, i_enabled, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_set_output( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;
    char* psz_output;

    if( !PyArg_ParseTuple( args, "ss", &psz_name, &psz_output ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_vlm_set_output( LIBVLC_INSTANCE(self), psz_name, psz_output, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_set_input( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;
    char* psz_input;

    if( !PyArg_ParseTuple( args, "ss", &psz_name, &psz_input ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_vlm_set_input( LIBVLC_INSTANCE(self), psz_name, psz_input, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_add_input( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;
    char* psz_input;

    if( !PyArg_ParseTuple( args, "ss", &psz_name, &psz_input ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_vlm_add_input( LIBVLC_INSTANCE(self), psz_name, psz_input, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_set_loop( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;
    int i_loop;

    if( !PyArg_ParseTuple( args, "si", &psz_name, &i_loop ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_vlm_set_loop( LIBVLC_INSTANCE(self), psz_name, i_loop, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_change_media( PyObject *self, PyObject *args, PyObject *kwds )
{
    libvlc_exception_t ex;
    static char *kwlist[] = { "name", "input", "output",
                              "options", "enable", "loop", NULL};
    char* psz_name = NULL;
    char* psz_input = NULL;
    char* psz_output = NULL;
    PyObject* py_options = NULL;
    int i_enable = 1;
    int i_loop = 0;
    int i_size = 0;
    char** ppsz_args = NULL;

    if( !PyArg_ParseTupleAndKeywords( args, kwds, "sss|Oii", kwlist,
                                      &psz_name,
                      &psz_input, &psz_output,
                      &py_options, &i_enable, &i_loop ) )
        return NULL;

    if( py_options )
    {
        i_size = pyoptions_to_args( py_options, &ppsz_args );
    }

    LIBVLC_TRY;
    libvlc_vlm_change_media( LIBVLC_INSTANCE(self),
                              psz_name, psz_input, psz_output,
                              i_size, ppsz_args, i_enable, i_loop, &ex);
    free_args( i_size, ppsz_args );
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_play_media( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;

    if( !PyArg_ParseTuple( args, "s", &psz_name ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_vlm_play_media( LIBVLC_INSTANCE(self), psz_name, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_stop_media( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;

    if( !PyArg_ParseTuple( args, "s", &psz_name ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_vlm_stop_media( LIBVLC_INSTANCE(self), psz_name, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_pause_media( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;

    if( !PyArg_ParseTuple( args, "s", &psz_name ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_vlm_pause_media( LIBVLC_INSTANCE(self), psz_name, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_seek_media( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;
    float f_percentage;

    if( !PyArg_ParseTuple( args, "sf", &psz_name, &f_percentage ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_vlm_seek_media( LIBVLC_INSTANCE(self), psz_name, f_percentage, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInstance_vlm_show_media( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_name;
    char* psz_ret;
    PyObject* o_ret;

    if( !PyArg_ParseTuple( args, "s", &psz_name ) )
        return NULL;
    LIBVLC_TRY;
    psz_ret = libvlc_vlm_show_media( LIBVLC_INSTANCE(self), psz_name, &ex );
    LIBVLC_EXCEPT;
    o_ret = Py_BuildValue( "s", psz_ret );
    free( psz_ret );
    return o_ret;
}

static PyObject *
vlcInstance_media_new( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    libvlc_media_t *p_media;
    char* psz_mrl = NULL;
    vlcMedia *p_ret;

    if( !PyArg_ParseTuple( args, "s", &psz_mrl ) )
        return NULL;

    LIBVLC_TRY;
    p_media = libvlc_media_new( LIBVLC_INSTANCE(self), psz_mrl, &ex );
    LIBVLC_EXCEPT;

    p_ret = PyObject_New( vlcMedia, &vlcMedia_Type );
    p_ret->p_media = p_media;
    Py_INCREF( p_ret ); /* Ah bon ? */
    return ( PyObject * )p_ret;
}

/* Method table */
static PyMethodDef vlcInstance_methods[] =
{
    { "get_vlc_id", vlcInstance_get_vlc_id, METH_NOARGS,
      "get_vlc_id( ) -> int        Get the instance id."},
    { "audio_toggle_mute", vlcInstance_audio_toggle_mute, METH_NOARGS,
      "audio_toggle_mute()         Toggle the mute state"},
    { "audio_get_mute", vlcInstance_audio_get_mute, METH_NOARGS,
      "audio_get_mute() -> int     Get the mute state"},
    { "audio_set_mute", vlcInstance_audio_set_mute, METH_VARARGS,
      "audio_set_mute(state=int)         Set the mute state"},
    { "audio_get_volume", vlcInstance_audio_get_volume, METH_NOARGS,
      "audio_get_volume() -> int   Get the audio volume"},
    { "audio_set_volume", vlcInstance_audio_set_volume, METH_VARARGS,
      "audio_set_volume(volume=int)       Set the audio volume"},
    { "audio_get_channel", vlcInstance_audio_get_channel, METH_NOARGS,
      "audio_get_channel() -> int  Get current audio channel" },
    { "audio_set_channel", vlcInstance_audio_set_channel, METH_VARARGS,
      "audio_set_channel(int)      Set current audio channel" },

    { "media_new", vlcInstance_media_new, METH_VARARGS,
      "media_new(str) -> object   Create a media object with the given mrl."},

    { "mediaplayer_new", vlcInstance_new_media_player, METH_NOARGS,
      "mediaplayer_new() -> object   Create a media player."},

    { "vlm_add_broadcast", vlcInstance_vlm_add_broadcast, METH_VARARGS | METH_KEYWORDS,
      "vlm_add_broadcast(name=str, input=str, output=str, options=list, enable=int, loop=int)   Add a new broadcast" },
    { "vlm_del_media", vlcInstance_vlm_del_media, METH_VARARGS,
      "vlm_del_media(name=str)    Delete a media" },
    { "vlm_set_enabled", vlcInstance_vlm_set_enabled, METH_VARARGS,
      "vlm_set_enabled(name=str, enabled=int)    Enable/disable a media" },
    { "vlm_set_output", vlcInstance_vlm_set_output, METH_VARARGS,
      "vlm_set_output(name=str, output=str)      Set the output" },
    { "vlm_set_input", vlcInstance_vlm_set_input, METH_VARARGS,
      "vlm_set_input(name=str, output=str)       Set the input" },
    { "vlm_add_input", vlcInstance_vlm_add_input, METH_VARARGS,
      "vlm_add_input(name=str, output=str)       Add a media's input MRL" },
    { "vlm_set_loop", vlcInstance_vlm_set_loop, METH_VARARGS,
      "vlm_set_loop(name=str, loop=int)          Change the looping value" },
    { "vlm_change_media", vlcInstance_vlm_change_media, METH_VARARGS | METH_KEYWORDS,
      "vlm_change_media(name=str, input=str, output=str, options=list, enable=int, loop=int)   Change the broadcast parameters" },
    { "vlm_play_media", vlcInstance_vlm_play_media, METH_VARARGS,
      "vlm_play_media(name=str)       Plays the named broadcast." },
    { "vlm_stop_media", vlcInstance_vlm_stop_media, METH_VARARGS,
      "vlm_stop_media(name=str)       Stops the named broadcast." },
    { "vlm_pause_media", vlcInstance_vlm_pause_media, METH_VARARGS,
      "vlm_pause_media(name=str)      Pauses the named broadcast." },
    { "vlm_seek_media", vlcInstance_vlm_seek_media, METH_VARARGS,
      "vlm_seek_media(name=str, percentage=float)  Seeks in the named broadcast." },
    { "vlm_show_media", vlcInstance_vlm_show_media, METH_VARARGS,
      "vlm_show_media(name=str)       Return information of the named broadcast." },

    { NULL, NULL, 0, NULL },
};

static PyTypeObject vlcInstance_Type =
{
    PyObject_HEAD_INIT( NULL )
    0,                          /*ob_size*/
    "vlc.Instance",             /*tp_name*/
    sizeof( vlcInstance_Type ), /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    ( destructor )vlcInstance_dealloc,      /*tp_dealloc*/
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
    "VLC Instance(args)",  /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    vlcInstance_methods,             /* tp_methods */
    0,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    vlcInstance_new,          /* tp_new */
};
