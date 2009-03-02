/*****************************************************************************
 * vlc_media.c: vlc.Media binding
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <oaubert at liris.cnrs.fr>
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
 * vlc.Media
 ***********************************************************************/

static PyObject *
vlcMedia_new( PyTypeObject *type, PyObject *args, PyObject *kwds )
{
    fprintf(stderr, "vlcMedia_new called\n");
    PyErr_SetString( PyExc_TypeError, "vlc.Media can be instantiated by itself. You should use vlc.Instance().media_new(mrl)." );
    return NULL;
}

static void
vlcMedia_dealloc( PyObject *self )
{
    libvlc_media_release( LIBVLC_MEDIA(self) );
    PyObject_DEL( self );
}

static PyObject *
vlcMedia_add_option( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char* psz_options = NULL;

    if( !PyArg_ParseTuple( args, "s", &psz_options ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_media_add_option( LIBVLC_MEDIA(self), psz_options, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcMedia_get_mrl( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char * psz_mrl;
    PyObject * o_ret;

    LIBVLC_TRY;
    psz_mrl = libvlc_media_get_mrl( LIBVLC_MEDIA(self), &ex);
    LIBVLC_EXCEPT;

    o_ret = Py_BuildValue( "s", psz_mrl );
    free( psz_mrl );
    return o_ret;
}

static PyObject *
vlcMedia_get_state( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    libvlc_state_t i_state;

    LIBVLC_TRY;
    i_state = libvlc_media_get_state( LIBVLC_MEDIA(self), &ex);
    LIBVLC_EXCEPT;
    /* FIXME: return the defined state constant */
    return Py_BuildValue( "i", i_state );
}

static PyObject *
vlcMedia_get_duration( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    libvlc_time_t i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_get_duration( LIBVLC_MEDIA(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "L", i_ret );
}

static PyObject *
vlcMedia_media_player_new( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    libvlc_media_player_t *p_mp;
    vlcMediaPlayer *p_ret;

    LIBVLC_TRY;
    p_mp = libvlc_media_player_new_from_media( LIBVLC_MEDIA(self), &ex);
    LIBVLC_EXCEPT;

    p_ret = PyObject_New( vlcMediaPlayer, &vlcMediaPlayer_Type );
    p_ret->p_mp = p_mp;
    Py_INCREF( p_ret ); /* Ah bon ? */
    return ( PyObject * )p_ret;
}

static PyObject *
vlcMedia_is_preparsed( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_media_is_preparsed( LIBVLC_MEDIA(self), &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "L", i_ret );
}

static PyObject *
vlcMedia_get_meta( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    char * psz_meta = NULL;
    char * psz_ret = NULL;
    PyObject* o_ret;
    int i_index = -1;
    int i_loop = 0;
    static const char * meta_names[] = { "Title", "Artist", "Genre", "Copyright", "Album", "TrackNumber", "Description", "Rating", "Date", "Setting", "URL", "Language", "NowPlaying", "Publisher", "EncodedBy", "ArtworkURL", "TrackID", NULL };

    if( !PyArg_ParseTuple( args, "s", &psz_meta ) )
        return NULL;
    while( meta_names[i_loop] )
    {
        if( !strncmp(meta_names[i_loop], psz_meta, strlen(meta_names[i_loop])) )
        {
            i_index = i_loop;
            break;
        }
        i_loop++;
    }
    if( i_index < 0 )
    {
        PyObject *py_exc = vlc_Exception;
        PyErr_SetString( py_exc, "Unknown meta attribute" );
        return NULL;
    }

    LIBVLC_TRY;
    psz_ret = libvlc_media_get_meta( LIBVLC_MEDIA(self), i_index, &ex);
    LIBVLC_EXCEPT;

    o_ret = Py_BuildValue( "s", psz_ret );
    free( psz_ret );
    return o_ret;
}

static PyMethodDef vlcMedia_methods[] =
{
    { "add_option", vlcMedia_add_option, METH_VARARGS,
      "add_option(str) Add an option to the media." },
    { "get_mrl", vlcMedia_get_mrl, METH_VARARGS,
      "get_mrl() -> str" },
    { "get_state", vlcMedia_get_state, METH_VARARGS,
      "get_state() -> int" },
    { "get_duration", vlcMedia_get_duration, METH_VARARGS,
      "get_duration() -> int" },
    { "mediaplayer_new", vlcMedia_media_player_new, METH_VARARGS,
      "mediaplayer_new() -> vlc.MediaPlayer   Create a MediaPlayer object from a Media" },
    { "is_preparsed", vlcMedia_is_preparsed, METH_VARARGS,
      "is_preparsed() -> int" },
    { "get_meta", vlcMedia_get_meta, METH_VARARGS,
      "get_meta(str) -> str   Read the meta of the media." },

    { NULL }  /* Sentinel */
};

static PyTypeObject vlcMedia_Type =
{
    PyObject_HEAD_INIT( NULL )
    0,                         /*ob_size*/
    "vlc.Media",            /*tp_name*/
    sizeof( vlcMedia_Type ),   /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    vlcMedia_dealloc, /*tp_dealloc*/
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
    "vlc.Media object.",  /* tp_doc */
    0,                        /* tp_traverse */
    0,                        /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                          /* tp_iternext */
    vlcMedia_methods,          /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    vlcMedia_new,              /* tp_new */
};

