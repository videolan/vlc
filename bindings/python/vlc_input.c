/*****************************************************************************
 * vlc_input.c: vlc.Input binding
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: $
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
vlcInput_get_length( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    vlc_int64_t i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_input_get_length( LIBVLC_INPUT->p_input, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "L", i_ret );
}

static PyObject *
vlcInput_get_time( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    vlc_int64_t i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_input_get_time( LIBVLC_INPUT->p_input, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "L", i_ret );
}

static PyObject *
vlcInput_set_time( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    vlc_int64_t i_time;

    if( !PyArg_ParseTuple( args, "L", &i_time ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_input_set_time( LIBVLC_INPUT->p_input, i_time, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInput_get_position( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_ret;
    LIBVLC_TRY;
    f_ret = libvlc_input_get_position( LIBVLC_INPUT->p_input, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "f", f_ret );
}

static PyObject *
vlcInput_set_position( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_pos;

    if( !PyArg_ParseTuple( args, "f", &f_pos ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_input_set_position( LIBVLC_INPUT->p_input, f_pos, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInput_will_play( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_input_will_play( LIBVLC_INPUT->p_input, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcInput_get_rate( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_ret;
    LIBVLC_TRY;
    f_ret = libvlc_input_get_rate( LIBVLC_INPUT->p_input, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "f", f_ret );
}

static PyObject *
vlcInput_set_rate( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_rate;

    if( !PyArg_ParseTuple( args, "f", &f_rate ) )
        return NULL;

    LIBVLC_TRY;
    libvlc_input_set_rate( LIBVLC_INPUT->p_input, f_rate, &ex);
    LIBVLC_EXCEPT;
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcInput_get_state( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_input_get_state( LIBVLC_INPUT->p_input, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcInput_has_vout( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    int i_ret;
    LIBVLC_TRY;
    i_ret = libvlc_input_has_vout( LIBVLC_INPUT->p_input, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "i", i_ret );
}

static PyObject *
vlcInput_get_fps( PyObject *self, PyObject *args )
{
    libvlc_exception_t ex;
    float f_ret;
    LIBVLC_TRY;
    f_ret = libvlc_input_get_fps( LIBVLC_INPUT->p_input, &ex);
    LIBVLC_EXCEPT;
    return Py_BuildValue( "f", f_ret );
}

static PyMethodDef vlcInput_methods[] =
{
    { "get_length", vlcInput_get_length, METH_VARARGS,
      "get_length() -> long" },
    { "get_time", vlcInput_get_time, METH_VARARGS,
      "get_time() -> long" },
    { "set_time", vlcInput_set_time, METH_VARARGS,
      "set_time(long)" },
    { "get_position", vlcInput_get_position, METH_VARARGS,
      "get_position() -> float" },
    { "set_position", vlcInput_set_position, METH_VARARGS,
      "set_position(float)" },
    { "will_play", vlcInput_will_play, METH_VARARGS,
      "will_play() -> int" },
    { "get_rate", vlcInput_get_rate, METH_VARARGS,
      "get_rate() -> float" },
    { "set_rate", vlcInput_set_rate, METH_VARARGS,
      "set_rate(float)" },
    { "get_state", vlcInput_get_state, METH_VARARGS,
      "get_state() -> int" },
    { "has_vout", vlcInput_has_vout, METH_VARARGS,
      "has_vout() -> int" },
    { "get_fps", vlcInput_get_fps, METH_VARARGS,
      "get_fps() -> float" },
    { NULL }  /* Sentinel */
};

static PyTypeObject vlcInput_Type =
{
    PyObject_HEAD_INIT( NULL )
    0,                         /*ob_size*/
    "vlc.Input",            /*tp_name*/
    sizeof( vlcInput_Type ),   /*tp_basicsize*/
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
    "vlc.Input object",  /* tp_doc */
    0,                        /* tp_traverse */
    0,                        /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                          /* tp_iternext */
    vlcInput_methods,          /* tp_methods */
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

