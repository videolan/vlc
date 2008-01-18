/*****************************************************************************
 * vlc_internal.c: vlcinternal python binding module
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

#include "vlc_internal.h"
#include "../../src/libvlc.h"

/**************************************************************************
 * VLC Module
 **************************************************************************/

#ifndef vlcinternalMODINIT_FUNC /* declarations for DLL import/export */
#define vlcinternalMODINIT_FUNC void
#endif

static PyMethodDef vlcinternal_methods[] = {
    { NULL }  /* Sentinel */
};

vlcinternalMODINIT_FUNC
initvlcinternal( void )
{
    PyObject* p_module;

    p_module = Py_InitModule3( "vlcinternal", vlcinternal_methods,
                               "VLC media player internal module" );

    if( !p_module )
      return;

    if( PyType_Ready( &vlcObject_Type ) < 0 )
        return;

    /* Types */
    Py_INCREF( &vlcObject_Type );
    PyModule_AddObject( p_module, "Object",
                        ( PyObject * )&vlcObject_Type );
}


/* Make libpostproc happy... */
void * fast_memcpy( void * to, const void * from, size_t len )
{
  return memcpy( to, from, len );
}

/*****************************************************************************
 * VLCObject implementation
 *****************************************************************************/

static PyObject
*vlcObject_new( PyTypeObject *p_type, PyObject *p_args, PyObject *p_kwds )
{
    vlcObject *self;
    vlc_object_t *p_object;
    int i_id;

    self = PyObject_New( vlcObject, &vlcObject_Type );

    if( !PyArg_ParseTuple( p_args, "i", &i_id ) )
      return NULL;

    /* Maybe we were already initialized */
    p_object = ( vlc_object_t* )vlc_current_object( i_id );

    if( !p_object )
    {
        /* Try to initialize */
        i_id = VLC_Create();
        if( i_id < 0 )
        {
            PyErr_SetString( PyExc_StandardError, "Unable to create a VLC instance." );
            return NULL;
        }
        p_object = ( vlc_object_t* )vlc_current_object( i_id );
    }

    if( !p_object )
    {
        PyErr_SetString( PyExc_StandardError, "Unable to get object." );
        return NULL;
    }

    self->p_object = p_object;
    self->b_released = 0;

    Py_INCREF(  self ); /* Ah bon ? */
    return ( PyObject * )self;
}

static PyObject *
vlcObject_release(  PyObject *self, PyObject *p_args )
{
    if( VLCSELF->b_released == 0 )
    {
        vlc_object_release( VLCSELF->p_object );
        VLCSELF->b_released = 1;
    }
    Py_INCREF(  Py_None );
    return Py_None;
}

static void
vlcObject_dealloc( PyObject *self )
{
    vlcObject_release( self, NULL );
    PyObject_DEL( self );
}

static PyObject *
vlcObject_find_object( PyObject *self, PyObject *args )
{
    vlcObject *p_retval;
    vlc_object_t *p_obj;
    char *psz_name;
    int i_object_type;

    if( !PyArg_ParseTuple( args, "s", &psz_name ) )
        return NULL;

    /* psz_name is in
       ( aout, decoder, input, httpd, intf, playlist, root, vlc, vout )
    */
    if( !strncmp( psz_name, "aout", 4 ) )
    {
        i_object_type = VLC_OBJECT_AOUT;
    }
    else if (! strncmp( psz_name, "decoder", 7 ) )
    {
        i_object_type = VLC_OBJECT_DECODER;
    }
    else if (! strncmp( psz_name, "httpd", 5 ) )
    {
            i_object_type = VLC_OBJECT_HTTPD;
    }
    else if (! strncmp( psz_name, "intf", 4 ) )
    {
        i_object_type = VLC_OBJECT_INTF;
    }
    else if (! strncmp( psz_name, "input", 5 ) )
    {
        i_object_type = VLC_OBJECT_INPUT;
    }
    else if (! strncmp( psz_name, "playlist", 8 ) )
    {
        i_object_type = VLC_OBJECT_PLAYLIST;
    }
    else if (! strncmp( psz_name, "libvlc", 6 ) )
    {
        i_object_type = VLC_OBJECT_LIBVLC;
    }
    else if (! strncmp( psz_name, "vout", 4 ) )
    {
        i_object_type = VLC_OBJECT_VOUT;
    }
    else if (! strncmp( psz_name, "sout", 4 ) )
    {
        i_object_type = VLC_OBJECT_SOUT;
    }
    else if (! strncmp( psz_name, "global", 6 ) )
    {
        i_object_type = VLC_OBJECT_GLOBAL;
    }
    else if (! strncmp( psz_name, "packetizer", 10 ) )
    {
        i_object_type = VLC_OBJECT_PACKETIZER;
    }
    else if (! strncmp( psz_name, "encoder", 7 ) )
    {
        i_object_type = VLC_OBJECT_ENCODER;
    }
    else if (! strncmp( psz_name, "vlm", 3 ) )
    {
        i_object_type = VLC_OBJECT_VLM;
    }
    else if (! strncmp( psz_name, "announce", 8 ) )
    {
        i_object_type = VLC_OBJECT_ANNOUNCE;
    }
    else if (! strncmp( psz_name, "demux", 5 ) )
    {
        i_object_type = VLC_OBJECT_DEMUX;
    }
    else if (! strncmp( psz_name, "access", 6 ) )
    {
        i_object_type = VLC_OBJECT_ACCESS;
    }
    else if (! strncmp( psz_name, "stream", 6 ) )
    {
        i_object_type = VLC_OBJECT_STREAM;
    }
    else if (! strncmp( psz_name, "filter", 6 ) )
    {
        i_object_type = VLC_OBJECT_FILTER;
    }
    else if (! strncmp( psz_name, "vod", 3 ) )
    {
        i_object_type = VLC_OBJECT_VOD;
    }
    else if (! strncmp( psz_name, "xml", 3 ) )
    {
        i_object_type = VLC_OBJECT_XML;
    }
    else if (! strncmp( psz_name, "osdmenu", 7 ) )
    {
        i_object_type = VLC_OBJECT_OSDMENU;
    }
    else if (! strncmp( psz_name, "stats", 5 ) )
    {
        i_object_type = VLC_OBJECT_STATS;
    }
    else if (! strncmp( psz_name, "metaengine", 10 ) )
    {
        i_object_type = VLC_OBJECT_META_ENGINE;
    }
    else
    {
        /* FIXME: raise an exception ? */
        Py_INCREF( Py_None );
        return Py_None;
    }

    p_obj = vlc_object_find( VLCSELF->p_object, i_object_type, FIND_ANYWHERE );

    if( !p_obj )
    {
        Py_INCREF( Py_None );
        return Py_None;
    }

    p_retval = PyObject_New( vlcObject, &vlcObject_Type );

    p_retval->p_object = p_obj;

    return ( PyObject * )p_retval;
}

static PyObject *
vlcObject_find_name( PyObject *self, PyObject *args )
{
    vlcObject *p_retval;
    vlc_object_t *p_obj;
    char *psz_name;

    if( !PyArg_ParseTuple( args, "s", &psz_name ) )
        return NULL;

    p_obj = vlc_object_find_name( VLCSELF->p_object, psz_name, FIND_ANYWHERE );

    if( !p_obj )
    {
        Py_INCREF( Py_None );
        return Py_None;
    }

    p_retval = PyObject_New( vlcObject, &vlcObject_Type );

    p_retval->p_object = p_obj;

    return ( PyObject * )p_retval;
}

static PyObject *
vlcObject_info( PyObject *self, PyObject *args )
{
    PyObject *p_retval;
    vlc_object_t *p_obj;
    vlc_object_internals_t *p_priv;
    
    p_obj = VLCSELF->p_object;
    p_priv = vlc_internals( p_obj );

    /* Return information about the object as a dict. */
    p_retval = PyDict_New();

    PyDict_SetItemString( p_retval, "object-id",
                          Py_BuildValue( "l", p_obj->i_object_id ) );
    PyDict_SetItemString( p_retval, "object-type",
                          Py_BuildValue( "s", p_obj->psz_object_type ) );
    PyDict_SetItemString( p_retval, "object-name",
                          Py_BuildValue( "s", p_obj->psz_object_name ) );
    PyDict_SetItemString( p_retval, "thread",
                          PyBool_FromLong( p_priv->b_thread ) );
    PyDict_SetItemString( p_retval, "thread-id",
                          PyLong_FromLongLong( p_priv->thread_id ) );
    PyDict_SetItemString( p_retval, "refcount",
                          PyInt_FromLong( p_priv->i_refcount ) );
    return p_retval;
}

static PyObject *
vlcObject_find_id( PyObject *self, PyObject *args )
{
    vlcObject *p_retval;
    vlc_object_t* p_object;
    int i_id;

    if( !PyArg_ParseTuple( args, "i", &i_id ) )
        return NULL;

    p_object = ( vlc_object_t* )vlc_current_object( i_id );

    if( !p_object )
    {
        Py_INCREF( Py_None );
        return Py_None;
    }

    p_retval = PyObject_NEW( vlcObject, &vlcObject_Type );

    p_retval->p_object = p_object;

    return ( PyObject * )p_retval;
}

/* Do a var_Get call on the object. Parameter: the variable name. */
static PyObject *
vlcObject_var_get( PyObject *self, PyObject *args )
{
    PyObject *p_retval;
    vlc_value_t value;
    char *psz_name;
    int i_type;

    if( !PyArg_ParseTuple( args, "s", &psz_name ) )
        return NULL;

    if( var_Get( VLCSELF->p_object, psz_name, &value ) != VLC_SUCCESS )
    {
        PyErr_SetString( PyExc_StandardError,
                         "Error: variable does not exist.\n" );
        return NULL;
    }

    i_type = var_Type ( VLCSELF->p_object, psz_name );

    switch ( i_type )
    {
    case VLC_VAR_VOID      :
        p_retval = PyString_FromString( "A void variable" );
        break;
    case VLC_VAR_BOOL      :
        p_retval = PyBool_FromLong( value.b_bool );
        break;
    case VLC_VAR_INTEGER   :
        p_retval = PyInt_FromLong( ( long )value.i_int );
        break;
    case VLC_VAR_HOTKEY    :
        p_retval = PyString_FromFormat( "A hotkey variable ( %d )", value.i_int );
        break;
    case VLC_VAR_FILE      :
    case VLC_VAR_STRING    :
    case VLC_VAR_DIRECTORY :
    case VLC_VAR_VARIABLE  :
        p_retval = PyString_FromString( value.psz_string );
        break;
    case VLC_VAR_MODULE   :
        p_retval = ( PyObject* )PyObject_New( vlcObject, &vlcObject_Type );
        ( ( vlcObject* )p_retval )->p_object = value.p_object;
        break;
    case VLC_VAR_FLOAT     :
        p_retval = PyFloat_FromDouble( ( double )value.f_float );
        break;
    case VLC_VAR_TIME      :
        p_retval = PyLong_FromLongLong( value.i_time );
        break;
    case VLC_VAR_ADDRESS   :
        p_retval = PyString_FromString( "A VLC address ( not handled yet )" );
        break;
    case VLC_VAR_LIST      :
        p_retval = PyString_FromString( "A VLC list ( not handled yet )" );
        break;
    case VLC_VAR_MUTEX :
        p_retval = PyString_FromString( "A mutex" );
        break;
    default:
        p_retval = Py_None;
    }

    Py_INCREF( p_retval );
    return p_retval;
}

static PyObject *
vlcObject_var_type( PyObject *self, PyObject *args )
{
    char *psz_name;
    PyObject *p_retval;
    int i_type;

    if( !PyArg_ParseTuple( args, "s", &psz_name ) )
        return NULL;

    i_type = var_Type( VLCSELF->p_object, psz_name );

    switch ( i_type )
    {
    case VLC_VAR_VOID   :
        p_retval = PyString_FromString( "Void" );
        break;
    case VLC_VAR_BOOL      :
        p_retval = PyString_FromString( "Boolean" );
        break;
    case VLC_VAR_INTEGER   :
        p_retval = PyString_FromString( "Integer" );
        break;
    case VLC_VAR_HOTKEY   :
        p_retval = PyString_FromString( "Hotkey" );
        break;
    case VLC_VAR_FILE      :
        p_retval = PyString_FromString( "File" );
        break;
    case VLC_VAR_STRING    :
        p_retval = PyString_FromString( "String" );
        break;
    case VLC_VAR_DIRECTORY :
        p_retval = PyString_FromString( "Directory" );
        break;
    case VLC_VAR_VARIABLE  :
        p_retval = PyString_FromString( "Variable" );
        break;
    case VLC_VAR_MODULE   :
        p_retval = PyString_FromString( "Module" );
        break;
    case VLC_VAR_FLOAT     :
        p_retval = PyString_FromString( "Float" );
        break;
    case VLC_VAR_TIME      :
        p_retval = PyString_FromString( "Time" );
        break;
    case VLC_VAR_ADDRESS   :
        p_retval = PyString_FromString( "Address" );
        break;
    case VLC_VAR_LIST      :
        p_retval = PyString_FromString( "List" );
        break;
    case VLC_VAR_MUTEX :
        p_retval = PyString_FromString( "Mutex" );
        break;
    default:
        p_retval = PyString_FromString( "Unknown" );
    }
    return p_retval;
}

/* Do a var_Set call on the object. Parameter: the variable name. */
static PyObject *
vlcObject_var_set( PyObject *self, PyObject *args )
{
    vlc_value_t value;
    char *psz_name;
    PyObject *py_value;
    int i_type;
    vlc_object_t *p_obj;

    if( !PyArg_ParseTuple( args, "sO", &psz_name, &py_value ) )
        return NULL;

    p_obj = VLCSELF->p_object;
    i_type = var_Type( p_obj, psz_name );

    switch ( i_type )
    {
    case VLC_VAR_VOID   :
        break;
    case VLC_VAR_BOOL      :
        value.b_bool = PyInt_AsLong( py_value );
        break;
    case VLC_VAR_INTEGER   :
    case VLC_VAR_HOTKEY   :
        value.i_int = PyInt_AsLong( py_value );
        break;
    case VLC_VAR_FILE      :
    case VLC_VAR_STRING    :
    case VLC_VAR_DIRECTORY :
    case VLC_VAR_VARIABLE  :
        value.psz_string = strdup( PyString_AsString( py_value ) );
        break;
    case VLC_VAR_MODULE   :
        /* FIXME: we should check the PyObject type and get its p_object */
        value.p_object = ( ( vlcObject* )p_obj )->p_object;
        break;
    case VLC_VAR_FLOAT     :
        value.f_float = PyFloat_AsDouble( py_value );
        break;
    case VLC_VAR_TIME      :
        value.i_time = PyLong_AsLongLong( py_value );
        break;
    case VLC_VAR_ADDRESS   :
        value.p_address = ( char* )PyLong_AsVoidPtr( py_value );
        break;
    case VLC_VAR_LIST      :
        /* FIXME */
        value.p_list = NULL;
        break;
    case VLC_VAR_MUTEX :
        break;
    }

    var_Set( p_obj, psz_name, value );

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcObject_var_list( PyObject *self, PyObject *args )
{
    PyObject *p_retval;
    Py_ssize_t i_size;
    Py_ssize_t i_index;
    vlc_object_internals_t *p_priv;

    p_priv = vlc_internals( VLCSELF->p_object );
    i_size = p_priv->i_vars;
    p_retval = PyTuple_New( i_size );


    for ( i_index = 0 ; i_index < i_size ; i_index++ )
    {
        PyTuple_SetItem( p_retval, i_index,
                         Py_BuildValue( "s", p_priv->p_vars[i_index].psz_name ) );
    }

    return p_retval;
}

/* Do a config_Get call on the object. Parameter: the variable name. */
static PyObject *
vlcObject_config_get( PyObject *self, PyObject *args )
{
    PyObject *p_retval;
    vlc_value_t value;
    char *psz_name;
    module_config_t *p_config;

    if( !PyArg_ParseTuple( args, "s", &psz_name ) )
        return NULL;

    p_config = config_FindConfig( VLCSELF->p_object, psz_name );

    if( !p_config )
    {
        PyErr_SetString( PyExc_StandardError,
                         "Error: config variable does not exist.\n" );
        return NULL;
    }

    switch ( p_config->i_type )
    {
    case CONFIG_ITEM_BOOL      :
        p_retval = PyBool_FromLong( p_config->value.i );
        break;
    case CONFIG_ITEM_INTEGER   :
        p_retval = PyInt_FromLong( ( long )p_config->value.i );
        break;
    case CONFIG_ITEM_KEY   :
        p_retval = PyString_FromFormat( "A hotkey variable ( %d )", p_config->value.i );
        break;
    case CONFIG_ITEM_FILE      :
    case CONFIG_ITEM_STRING    :
    case CONFIG_ITEM_DIRECTORY :
    case CONFIG_ITEM_MODULE    :
        vlc_mutex_lock( p_config->p_lock );
        if( p_config->value.psz )
            p_retval = PyString_FromString( p_config->value.psz );
        else
            p_retval = PyString_FromString( "" );
        vlc_mutex_unlock( p_config->p_lock );
        break;
        p_retval = ( PyObject* )PyObject_New( vlcObject, &vlcObject_Type );
        ( ( vlcObject* )p_retval )->p_object = value.p_object;
        break;
    case CONFIG_ITEM_FLOAT     :
        p_retval = PyFloat_FromDouble( ( double )p_config->value.f );
        break;
    default:
        p_retval = Py_None;
        Py_INCREF( p_retval );
    }

    return p_retval;
}

/* Do a config_put* call on the object. Parameter: the variable name. */
static PyObject *
vlcObject_config_set( PyObject *self, PyObject *args )
{
    char *psz_name;
    PyObject *py_value;
    vlc_object_t *p_obj;
    module_config_t *p_config;


    if( !PyArg_ParseTuple( args, "sO", &psz_name, &py_value ) )
        return NULL;

    p_obj = VLCSELF->p_object;
    p_config = config_FindConfig( p_obj, psz_name );
    /* sanity checks */
    if( !p_config )
    {
        PyErr_SetString( PyExc_StandardError,
                         "Error: option does not exist.\n" );
        return NULL;
    }

    switch ( p_config->i_type )
    {
    case CONFIG_ITEM_BOOL      :
    case CONFIG_ITEM_INTEGER   :
    case CONFIG_ITEM_KEY       :
        config_PutInt( p_obj, psz_name, PyInt_AsLong( py_value ) );
        break;
    case CONFIG_ITEM_FILE      :
    case CONFIG_ITEM_STRING    :
    case CONFIG_ITEM_DIRECTORY :
    case CONFIG_ITEM_MODULE   :
        config_PutPsz( p_obj, psz_name, PyString_AsString( py_value ) );
        break;
    case CONFIG_ITEM_FLOAT     :
        config_PutFloat( p_obj, psz_name, PyFloat_AsDouble( py_value ) );
        break;
    }
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *
vlcObject_children( PyObject *self, PyObject *args )
{
    PyObject *p_retval;
    Py_ssize_t i_size;
    Py_ssize_t i_index;

    i_size = VLCSELF->p_object->i_children;
    p_retval = PyTuple_New( i_size );

    for ( i_index = 0 ; i_index < i_size ; i_index++ )
    {
        PyTuple_SetItem( p_retval, i_index,
                         Py_BuildValue( "i",
                                        VLCSELF->p_object->pp_children[i_index]->i_object_id ) );
    }

    return p_retval;
}


/* Method table */
static PyMethodDef vlcObject_methods[] =
{
    { "get", vlcObject_var_get, METH_VARARGS,
      "get( str ) -> value   Get a variable value."},
    { "set", vlcObject_var_set, METH_VARARGS,
      "set( str, value )     Set a variable value" },
    { "config_get", vlcObject_config_get, METH_VARARGS,
      "config_get( str ) -> value   Get a configuration option." },
    { "config_set", vlcObject_config_set, METH_VARARGS,
      "config_set( str, value )     Set a configuration option" },
    { "type", vlcObject_var_type, METH_VARARGS,
      "type( str ) -> str     Get a variable type" },
    { "list", vlcObject_var_list, METH_NOARGS,
      "list( )             List the available variables" },
    { "children", vlcObject_children, METH_NOARGS,
      "children( )             List the children ids" },
    { "find_object", vlcObject_find_object, METH_VARARGS,
      "find_object( str ) -> Object     Find the object of a given type.\n\nAvailable types are : aout, decoder, input, httpd, intf, playlist, root, vlc, vout"},
    { "find_id", vlcObject_find_id, METH_VARARGS,
      "find_id( int ) -> Object      Find an object by id" },
    { "find_name", vlcObject_find_name, METH_VARARGS,
      "find_name( str ) -> Object      Find an object by name" },
    { "info", vlcObject_info, METH_NOARGS,
       "info( ) -> dict    Return information about the object" },
    { "release", vlcObject_release, METH_NOARGS,
      "release( ) ->     Release the VLC Object" },
    { NULL, NULL, 0, NULL },
};

static PyTypeObject vlcObject_Type =
{
    PyObject_HEAD_INIT( NULL )
    0,                         /*ob_size*/
    "vlc.Object",       /*tp_name*/
    sizeof( vlcObject_Type ), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    ( destructor )vlcObject_dealloc,      /*tp_dealloc*/
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
    "Expose VLC object internal infrastructure.\n\nConstructor: vlc.Object(object_id)\n\nPLEASE BE AWARE that accessing internal features of VLC voids the guarantee for the product and is not advised except if you know what you are doing.",  /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    vlcObject_methods,             /* tp_methods */
    0,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    vlcObject_new,          /* tp_new */
};
