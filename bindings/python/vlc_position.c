/*****************************************************************************
 * vlc_position.c: vlc.Position binding
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
 * Position
 ***********************************************************************/

static PyObject *
PyPosition_new( PyTypeObject *type, PyObject *args, PyObject *kwds )
{
    PyPosition *self;
    static char *kwlist[] = { "value", "origin", "key", NULL};

    self = PyObject_New( PyPosition, &PyPosition_Type );

    self->value=0;
    self->origin=mediacontrol_AbsolutePosition;
    self->key=mediacontrol_MediaTime;

    if(! PyArg_ParseTupleAndKeywords( args, kwds, "|lii", kwlist,
                                      &(self->value),
                                      &(self->origin),
                                      &(self->key) ) )
    {
        return NULL;
    }

    if( self->key != mediacontrol_MediaTime
    && self->key != mediacontrol_ByteCount
    && self->key != mediacontrol_SampleCount )
    {
        PyErr_SetString ( MediaControl_InternalException, "Invalid key value" );
        return NULL;
    }

    if( self->origin != mediacontrol_AbsolutePosition
    && self->origin != mediacontrol_RelativePosition
    && self->origin != mediacontrol_ModuloPosition )
    {
        PyErr_SetString ( MediaControl_InternalException, "Invalid origin value" );
        return NULL;
    }

    Py_INCREF( self );
    return ( PyObject * )self;
}

mediacontrol_PositionKey
positionKey_py_to_c( PyObject * py_key )
{
    mediacontrol_PositionKey key_position = mediacontrol_MediaTime;
    int key;

    if( !PyArg_Parse( py_key, "i", &key ) )
    {
        PyErr_SetString ( MediaControl_InternalException, "Invalid key value" );
        return key_position;
    }

    switch ( key )
    {
    case 0: key = mediacontrol_ByteCount;   break;
    case 1: key = mediacontrol_SampleCount; break;
    case 2: key = mediacontrol_MediaTime;   break;
    }
    return key_position;
}

mediacontrol_PositionOrigin
positionOrigin_py_to_c( PyObject * py_origin )
{
    mediacontrol_PositionOrigin  origin_position = mediacontrol_AbsolutePosition;
    int origin;

    if( !PyArg_Parse( py_origin,"i", &origin ) )
    {
        PyErr_SetString( MediaControl_InternalException,
                         "Invalid origin value" );
        return origin_position;
    }

    switch ( origin )
    {
    case 0: origin_position = mediacontrol_AbsolutePosition; break;
    case 1: origin_position = mediacontrol_RelativePosition; break;
    case 2: origin_position = mediacontrol_ModuloPosition;   break;
    }

    return origin_position;
}

/* Methods for transforming the Position Python object to Position structure*/
mediacontrol_Position*
position_py_to_c( PyObject * py_position )
{
    mediacontrol_Position * a_position = NULL;
    PyPosition *pos = ( PyPosition* )py_position;

    a_position = ( mediacontrol_Position* )malloc( sizeof( mediacontrol_Position ) );
    if( !a_position )
    {
        PyErr_SetString( PyExc_MemoryError, "Out of memory" );
        return NULL;
    }

    if( !py_position )
    {
        /* If we give a NULL value, it will be considered as
           a 0 relative position in mediatime */
        a_position->origin = mediacontrol_RelativePosition;
        a_position->key    = mediacontrol_MediaTime;
        a_position->value  = 0;
    }
    else if( PyObject_IsInstance( py_position, ( PyObject* )&PyPosition_Type ) )
    {
        a_position->origin = pos->origin;
        a_position->key    = pos->key;
        a_position->value  = ntohll(pos->value);
    }
    else
    {
        /* Feature: if we give an integer, it will be considered as
           a relative position in mediatime */
        a_position->origin = mediacontrol_RelativePosition;
        a_position->key    = mediacontrol_MediaTime;
        a_position->value  = PyLong_AsLongLong( py_position );
    }
    return a_position;
}

PyPosition*
position_c_to_py( mediacontrol_Position *position )
{
    PyPosition* py_retval;

    py_retval = PyObject_New( PyPosition, &PyPosition_Type );
    py_retval->origin = position->origin;
    py_retval->key    = position->key;
    py_retval->value  = position->value;

    return py_retval;
}

static PyMethodDef PyPosition_methods[] =
{
    { NULL }  /* Sentinel */
};

static PyMemberDef PyPosition_members[] =
{
    { "origin", T_INT, offsetof( PyPosition, origin ), 0, "Position origin" },
    { "key",    T_INT, offsetof( PyPosition, key ),    0, "Position key" },
    { "value",  T_ULONG, offsetof( PyPosition, value ), 0, "Position value" },
    { NULL }  /* Sentinel */
};

static PyTypeObject PyPosition_Type =
{
    PyObject_HEAD_INIT( NULL )
    0,                         /*ob_size*/
    "vlc.Position",            /*tp_name*/
    sizeof( PyPosition_Type ),   /*tp_basicsize*/
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
    "Represent a Position with value, origin and key",  /* tp_doc */
    0,                        /* tp_traverse */
    0,                        /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                          /* tp_iternext */
    PyPosition_methods,             /* tp_methods */
    PyPosition_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    PyPosition_new,            /* tp_new */
};
