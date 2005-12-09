/*****************************************************************************
 * vlcglue.c: VLC Module
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <oaubert at bat710.univ-lyon1.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include "vlcglue.h"

/**************************************************************************
 * VLC Module
 **************************************************************************/

#ifndef vlcMODINIT_FUNC /* declarations for DLL import/export */
#define vlcMODINIT_FUNC void
#endif

static PyMethodDef vlc_methods[] = {
    {NULL}  /* Sentinel */
};

/* Module globals */
PyObject* MediaControl_InternalException          = NULL;
PyObject* MediaControl_PositionKeyNotSupported    = NULL;
PyObject *MediaControl_PositionOriginNotSupported = NULL;
PyObject* MediaControl_InvalidPosition            = NULL;
PyObject *MediaControl_PlaylistException          = NULL;

vlcMODINIT_FUNC  initvlc(void)
{
    PyObject* m;

    PyPosition_Type.tp_new = PyType_GenericNew;
    PyPosition_Type.tp_alloc = PyType_GenericAlloc;

    if (PyType_Ready(&PyPosition_Type) < 0)
        return;
    if (PyType_Ready(&MediaControl_Type) < 0)
        return;
#ifdef VLCOBJECT_SUPPORT
    if (PyType_Ready(&vlcObject_Type) < 0)
        return;
#endif

        /*  PyEval_InitThreads(); */

        /* Have a look at
http://base.bel-epa.com/pyapache/Python/MySQL-python/MySQL-python-0.3.0/_mysqlmodule.c */

    m = Py_InitModule3( "vlc", vlc_methods,
                        "VLC media player embedding module.");

    /* Exceptions */
    MediaControl_InternalException =
            PyErr_NewException("vlc.InternalException", NULL, NULL);
    PyModule_AddObject(m, "InternalException", MediaControl_InternalException);

    MediaControl_PositionKeyNotSupported =
            PyErr_NewException("vlc.PositionKeyNotSupported", NULL, NULL);
    PyModule_AddObject(m, "PositionKeyNotSupported",
                    MediaControl_PositionKeyNotSupported);

    MediaControl_PositionOriginNotSupported=
            PyErr_NewException("vlc.InvalidPosition", NULL, NULL);
    PyModule_AddObject(m, "PositionOriginNotSupported",
                    MediaControl_PositionOriginNotSupported);

    MediaControl_InvalidPosition =
            PyErr_NewException("vlc.InvalidPosition", NULL, NULL);
    PyModule_AddObject(m, "InvalidPosition", MediaControl_InvalidPosition);

    MediaControl_PlaylistException =
            PyErr_NewException("vlc.PlaylistException", NULL, NULL);
    PyModule_AddObject(m, "PlaylistException", MediaControl_PlaylistException);

    /* Types */
    Py_INCREF(&PyPosition_Type);
    PyModule_AddObject(m, "Position", (PyObject *)&PyPosition_Type);
    Py_INCREF(&MediaControl_Type);
    PyModule_AddObject(m, "MediaControl", (PyObject *)&MediaControl_Type);
#ifdef VLCOBJECT_SUPPORT
    Py_INCREF(&vlcObject_Type);
    PyModule_AddObject(m, "Object", (PyObject *)&vlcObject_Type);
#endif

    /* Constants */
    PyModule_AddIntConstant(m, "AbsolutePosition",
                    mediacontrol_AbsolutePosition);
    PyModule_AddIntConstant(m, "RelativePosition",
                    mediacontrol_RelativePosition);
    PyModule_AddIntConstant(m, "ModuloPosition",
                    mediacontrol_ModuloPosition);

    PyModule_AddIntConstant(m, "ByteCount",        mediacontrol_ByteCount);
    PyModule_AddIntConstant(m, "SampleCount",      mediacontrol_SampleCount);
    PyModule_AddIntConstant(m, "MediaTime",        mediacontrol_MediaTime);
    PyModule_AddIntConstant(m, "PlayingStatus",    mediacontrol_PlayingStatus);
    PyModule_AddIntConstant(m, "PauseStatus", mediacontrol_PauseStatus);
    PyModule_AddIntConstant(m, "ForwardStatus", mediacontrol_ForwardStatus);
    PyModule_AddIntConstant(m, "BackwardStatus", mediacontrol_BackwardStatus);
    PyModule_AddIntConstant(m, "InitStatus", mediacontrol_InitStatus);
    PyModule_AddIntConstant(m, "EndStatus", mediacontrol_EndStatus);
    PyModule_AddIntConstant(m, "UndefinedStatus", mediacontrol_UndefinedStatus);
}


/* Make libpostproc happy... */
void * fast_memcpy(void * to, const void * from, size_t len)
{
  return memcpy(to, from, len);
}


/*****************************************************************************
 * VLCObject implementation
 *****************************************************************************/

#ifdef VLCOBJECT_SUPPORT

static PyObject *vlcObject_new(
                PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    vlcObject *self;
    vlc_object_t *p_object;
    int i_id;

    self = PyObject_New(vlcObject, &vlcObject_Type);

    if ( !PyArg_ParseTuple(args, "i", &i_id) )
      return NULL;

    /* Maybe we were already initialized */
    p_object = (vlc_object_t*)vlc_current_object(i_id);

    if (! p_object)
    {
        /* Try to initialize */
        i_id = VLC_Create();
        p_object = (vlc_object_t*)vlc_current_object(i_id);
    }

    if (! p_object)
    {
        PyErr_SetString(PyExc_StandardError, "Unable to get object.");
        return NULL;
    }

    self->p_object = p_object;
    self->b_released = 0;

    Py_INCREF( self ); /* Ah bon ? */
    return (PyObject *)self;
}

static PyObject * vlcObject_release( PyObject *self )
{
    if( VLCSELF->b_released == 0 )
    {
        vlc_object_release( VLCSELF->p_object );
        VLCSELF->b_released = 1;
    }
    Py_INCREF( Py_None);
    return Py_None;
}

static void  vlcObject_dealloc(PyObject *self)
{
    vlcObject_release( self );
    PyMem_DEL(self);
}

static PyObject * vlcObject_find_object(PyObject *self, PyObject *args)
{
    vlcObject *retval;
    vlc_object_t *p_obj;
    char *psz_name;
    int i_object_type;

    if ( !PyArg_ParseTuple(args, "s", &psz_name) )
      return NULL;

    /* psz_name is in
       (aout, decoder, input, httpd, intf, playlist, root, vlc, vout)
    */
    switch (psz_name[0])
    {
        case 'a':
            i_object_type = VLC_OBJECT_AOUT;
            break;
        case 'd':
            i_object_type = VLC_OBJECT_DECODER;
            break;
        case 'h':
            i_object_type = VLC_OBJECT_HTTPD;
            break;
        case 'i':
            if (strlen(psz_name) < 3)
                    i_object_type = VLC_OBJECT_INTF;
            else if (psz_name[2] == 't')
                    i_object_type = VLC_OBJECT_INTF;
            else
                    i_object_type = VLC_OBJECT_INPUT;
            break;
        case 'p':
            i_object_type = VLC_OBJECT_PLAYLIST;
            break;
        case 'r':
            i_object_type = VLC_OBJECT_ROOT;
            break;
        case 'v':
            if (strlen(psz_name) < 3)
                    i_object_type = VLC_OBJECT_VLC;
            else if (psz_name[1] == 'l')
                    i_object_type = VLC_OBJECT_VLC;
            else
                    i_object_type = VLC_OBJECT_VOUT;
            break;
        default:
            /* FIXME: raise an exception */
            return Py_None;
    }

    p_obj = vlc_object_find( VLCSELF->p_object, i_object_type, FIND_ANYWHERE );

    if (! p_obj)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    retval = PyObject_New(vlcObject, &vlcObject_Type);

    retval->p_object = p_obj;

    return (PyObject *)retval;
}

static PyObject * vlcObject_info(PyObject *self, PyObject *args)
{
    PyObject *retval;
    vlc_object_t *p_obj;

    p_obj = VLCSELF->p_object;

    /* Return information about the object as a dict. */
    retval = PyDict_New();

    PyDict_SetItemString(retval, "object-id",
                    Py_BuildValue("l", p_obj->i_object_id));
    PyDict_SetItemString(retval, "object-type",
                  Py_BuildValue("s", p_obj->psz_object_type));
    PyDict_SetItemString(retval, "object-name",
                  Py_BuildValue("s", p_obj->psz_object_name));
    PyDict_SetItemString(retval, "thread",
                  PyBool_FromLong(p_obj->b_thread));
    PyDict_SetItemString(retval, "thread-id",
                  PyLong_FromLongLong(p_obj->thread_id));
    PyDict_SetItemString(retval, "refcount",
                  PyInt_FromLong(p_obj->i_refcount));

    return retval;
}

static PyObject * vlcObject_find_id(PyObject *self, PyObject *args)
{
    vlcObject *retval;
    vlc_object_t* p_object;
    int i_id;

    if ( !PyArg_ParseTuple(args, "i", &i_id) )
    {
        PyErr_SetString(PyExc_StandardError, "Error: no id was given.\n");
        return Py_None;
    }

    p_object = (vlc_object_t*)vlc_current_object(i_id);

    if (! p_object)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    retval = PyObject_NEW(vlcObject, &vlcObject_Type);

    retval->p_object = p_object;

    return (PyObject *)retval;
}

/* Do a var_Get call on the object. Parameter: the variable name. */
/* FIXME: We should make them attributes */
static PyObject * vlcObject_var_get(PyObject *self, PyObject *args)
{
    PyObject *retval;
    vlc_value_t value;
    char *psz_name;
    int i_type;

    if ( !PyArg_ParseTuple(args, "s", &psz_name) )
    {
        PyErr_SetString(PyExc_StandardError, "Error: no variable name was given.\n");
        Py_INCREF(Py_None);
        return Py_None;
    }

    if( var_Get( VLCSELF->p_object, psz_name, &value ) != VLC_SUCCESS )
    {
            PyErr_SetString(PyExc_StandardError, "Error: no variable name was given.\n");
            Py_INCREF(Py_None);
            return Py_None;
    }

    i_type = var_Type (VLCSELF->p_object, psz_name);

    switch (i_type)
    {
            case VLC_VAR_VOID   :
                    retval = PyString_FromString("A void variable");
                    break;
            case VLC_VAR_BOOL      :
                    retval = PyBool_FromLong(value.b_bool);
                    break;
            case VLC_VAR_INTEGER   :
                    retval = PyInt_FromLong((long)value.i_int);
                    break;
            case VLC_VAR_HOTKEY   :
                    retval = PyString_FromFormat("A hotkey variable (%d)", value.i_int);
                    break;
            case VLC_VAR_FILE      :
            case VLC_VAR_STRING    :
            case VLC_VAR_DIRECTORY :
            case VLC_VAR_VARIABLE  :
                    retval = PyString_FromString(value.psz_string);
                    break;
            case VLC_VAR_MODULE   :
                    retval = (PyObject*)PyObject_New(vlcObject, &vlcObject_Type);
                    ((vlcObject*)retval)->p_object = value.p_object;
                    break;
            case VLC_VAR_FLOAT     :
                    retval = PyFloat_FromDouble((double)value.f_float);
                    break;
            case VLC_VAR_TIME      :
                    retval = PyLong_FromLongLong(value.i_time);
                    break;
            case VLC_VAR_ADDRESS   :
                    retval = PyString_FromString("A VLC address (not handled yet)");
                    break;
            case VLC_VAR_LIST      :
                    retval = PyString_FromString("A VLC list (not handled yet)");
                    break;
            case VLC_VAR_MUTEX :
                    retval = PyString_FromString("A mutex");
                    break;
            default:
                    retval = Py_None;
    }

    Py_INCREF(retval);
    return retval;
}

static PyObject * vlcObject_var_type(PyObject *self,
                PyObject *args)
{
        char *psz_name;
        PyObject *retval;
        int i_type;

        if ( !PyArg_ParseTuple(args, "s", &psz_name))
        {
                PyErr_SetString(PyExc_StandardError, "Error: no variable name was given.\n");
                Py_INCREF(Py_None);
                return Py_None;
        }

        i_type = var_Type(VLCSELF->p_object, psz_name);

        switch (i_type)
        {
                case VLC_VAR_VOID   :
                        retval = PyString_FromString("Void");
                        break;
                case VLC_VAR_BOOL      :
                        retval = PyString_FromString("Boolean");
                        break;
                case VLC_VAR_INTEGER   :
                        retval = PyString_FromString("Integer");
                        break;
                case VLC_VAR_HOTKEY   :
                        retval = PyString_FromString("Hotkey");
                        break;
                case VLC_VAR_FILE      :
                        retval = PyString_FromString("File");
                        break;
                case VLC_VAR_STRING    :
                        retval = PyString_FromString("String");
                        break;
                case VLC_VAR_DIRECTORY :
                        retval = PyString_FromString("Directory");
                        break;
                case VLC_VAR_VARIABLE  :
                        retval = PyString_FromString("Variable");
                        break;
                case VLC_VAR_MODULE   :
                        retval = PyString_FromString("Module");
                        break;
                case VLC_VAR_FLOAT     :
                        retval = PyString_FromString("Float");
                        break;
                case VLC_VAR_TIME      :
                        retval = PyString_FromString("Time");
                        break;
                case VLC_VAR_ADDRESS   :
                        retval = PyString_FromString("Address");
                        break;
                case VLC_VAR_LIST      :
                        retval = PyString_FromString("List");
                        break;
                case VLC_VAR_MUTEX :
                        retval = PyString_FromString("Mutex");
                        break;
                default:
                        retval = PyString_FromString("Unknown");
        }
        return retval;
}

/* Do a var_Set call on the object. Parameter: the variable name. */
/* FIXME: We should make them attributes */
static PyObject * vlcObject_var_set(PyObject *self,
                PyObject *args)
{
        vlc_value_t value;
        char *psz_name;
        PyObject *py_value;
        int i_type;
        vlc_object_t *p_obj;

        if ( !PyArg_ParseTuple(args, "sO", &psz_name, &py_value) )
        {
                PyErr_SetString(PyExc_StandardError, "Error: no variable name was given.\n");
                Py_INCREF(Py_None);
                return Py_None;
        }

        p_obj = VLCSELF->p_object;
        i_type = var_Type(p_obj, psz_name);

        switch (i_type)
        {
                case VLC_VAR_VOID   :
                        break;
                case VLC_VAR_BOOL      :
                        value.b_bool = PyInt_AsLong(py_value);
                        break;
                case VLC_VAR_INTEGER   :
                case VLC_VAR_HOTKEY   :
                        value.i_int = PyInt_AsLong(py_value);
                        break;
                case VLC_VAR_FILE      :
                case VLC_VAR_STRING    :
                case VLC_VAR_DIRECTORY :
                case VLC_VAR_VARIABLE  :
                        value.psz_string = strdup(PyString_AsString(py_value));
                        break;
                case VLC_VAR_MODULE   :
                        /* FIXME: we should check the PyObject type and get its p_object */
                        value.p_object = ((vlcObject*)p_obj)->p_object;
                        break;
                case VLC_VAR_FLOAT     :
                        value.f_float = PyFloat_AsDouble(py_value);
                        break;
                case VLC_VAR_TIME      :
                        value.i_time = PyLong_AsLongLong(py_value);
                        break;
                case VLC_VAR_ADDRESS   :
                        value.p_address = (char*)PyLong_AsVoidPtr(py_value);
                        break;
                case VLC_VAR_LIST      :
                        /* FIXME */
                        value.p_list = NULL;
                        break;
                case VLC_VAR_MUTEX :
                        break;
        }

        var_Set(p_obj, psz_name, value);

        Py_INCREF(Py_None);
        return Py_None;
}

static PyObject * vlcObject_var_list(PyObject *self,
                PyObject *args)
{
        PyObject *retval;
        int i_size;
        int i_index;

        i_size = VLCSELF->p_object->i_vars;
        retval = PyTuple_New(i_size);

        for (i_index = 0 ; i_index < i_size ; i_index++)
        {
                PyTuple_SetItem(retval, i_index, 
                                Py_BuildValue("s", VLCSELF->p_object->p_vars[i_index].psz_name));
        }

        /* Py_INCREF(retval); */
        return retval;
}

/* Do a config_Get call on the object. Parameter: the variable name. */
static PyObject * vlcObject_config_get(PyObject *self,
                PyObject *args)
{
        PyObject *retval;
        vlc_value_t value;
        char *psz_name;
        module_config_t *p_config;

        if ( !PyArg_ParseTuple(args, "s", &psz_name) )
        {
                PyErr_SetString(PyExc_StandardError, "Error: no config variable name was given.\n");
                Py_INCREF(Py_None);
                return Py_None;
        }

        p_config = config_FindConfig( VLCSELF->p_object, psz_name );

        if( !p_config )
        {
                PyErr_SetString(PyExc_StandardError, "Error: config variable does not exist.\n");
                Py_INCREF(Py_None);
                return Py_None;
        }

        switch (p_config->i_type)
        {
                case CONFIG_ITEM_BOOL      :
                        retval = PyBool_FromLong(p_config->i_value);
                        break;
                case CONFIG_ITEM_INTEGER   :
                        retval = PyInt_FromLong((long)p_config->i_value);
                        break;
                case CONFIG_ITEM_KEY   :
                        retval = PyString_FromFormat("A hotkey variable (%d)", p_config->i_value);
                        break;
                case CONFIG_ITEM_FILE      :
                case CONFIG_ITEM_STRING    :
                case CONFIG_ITEM_DIRECTORY :
                case CONFIG_ITEM_MODULE    :
                        vlc_mutex_lock( p_config->p_lock );
                        if( p_config->psz_value )
                                retval = PyString_FromString( p_config->psz_value );
                        else
                                retval = PyString_FromString( "" );
                        vlc_mutex_unlock( p_config->p_lock );
                        break;
                        retval = (PyObject*)PyObject_New(vlcObject, &vlcObject_Type);
                        ((vlcObject*)retval)->p_object = value.p_object;
                        break;
                case CONFIG_ITEM_FLOAT     :
                        retval = PyFloat_FromDouble((double)p_config->f_value);
                        break;
                default:
                        retval = Py_None;
                        Py_INCREF(retval);
        }

        return retval;
}

/* Do a var_Set call on the object. Parameter: the variable name. */
/* FIXME: We should make them attributes */
static PyObject * vlcObject_config_set(PyObject *self,
                PyObject *args)
{
        char *psz_name;
        PyObject *py_value;
        vlc_object_t *p_obj;
        module_config_t *p_config;


        if ( !PyArg_ParseTuple(args, "sO", &psz_name, &py_value) )
        {
            PyErr_SetString(PyExc_StandardError,
                            "Error: no variable name was given.\n");
            Py_INCREF(Py_None);
            return Py_None;
        }

        p_obj = VLCSELF->p_object;
        p_config = config_FindConfig( p_obj, psz_name );
        /* sanity checks */
        if( !p_config )
        {
            PyErr_SetString(PyExc_StandardError,
                            "Error: option does not exist.\n");
            Py_INCREF(Py_None);
            return Py_None;
        }

        switch (p_config->i_type)
        {
                case CONFIG_ITEM_BOOL      :
                case CONFIG_ITEM_INTEGER   :
                case CONFIG_ITEM_KEY       :
                        config_PutInt(p_obj, psz_name, PyInt_AsLong(py_value));
                        break;
                case CONFIG_ITEM_FILE      :
                case CONFIG_ITEM_STRING    :
                case CONFIG_ITEM_DIRECTORY :
                case CONFIG_ITEM_MODULE   :
                        config_PutPsz(p_obj, psz_name, PyString_AsString(py_value));
                        break;
                case CONFIG_ITEM_FLOAT     :
                        config_PutFloat(p_obj, psz_name, PyFloat_AsDouble(py_value));
                        break;
        }
        Py_INCREF(Py_None);
        return Py_None;
}

static PyObject * vlcObject_children(PyObject *self,
                PyObject *args)
{
        PyObject *retval;
        int i_size;
        int i_index;

        i_size = VLCSELF->p_object->i_children;
        retval = PyTuple_New(i_size);

        for (i_index = 0 ; i_index < i_size ; i_index++)
        {
            PyTuple_SetItem(retval, i_index,
                 Py_BuildValue("i",
                      VLCSELF->p_object->pp_children[i_index]->i_object_id));
        }

        /* Py_INCREF(retval); */
        return retval;
}


/* Method table */
static PyMethodDef vlcObject_methods[] =
{
    { "get", vlcObject_var_get, METH_VARARGS,
      "get(str) -> value   Get a variable value."},
    { "set", vlcObject_var_set, METH_VARARGS,
      "set(str, value)     Set a variable value" },
    { "config_get", vlcObject_config_get, METH_VARARGS,
      "config_get(str) -> value   Get a configuration option." },
    { "config_set", vlcObject_config_set, METH_VARARGS,
      "config_set(str, value)     Set a configuration option" },
    { "type", vlcObject_var_type, METH_VARARGS,
      "type(str) -> str     Get a variable type" },
    { "list", vlcObject_var_list, METH_VARARGS,
      "list()             List the available variables" },
    { "children", vlcObject_children, METH_VARARGS,
      "children()             List the children ids" },
    { "find_object", vlcObject_find_object, METH_VARARGS,
      "find_object(str) -> Object     Find the object of a given type.\n\nAvailable types are : aout, decoder, input, httpd, intf, playlist, root, vlc, vout"},
    { "find_id", vlcObject_find_id, METH_VARARGS,
      "find_id(int) -> Object      Find an object by id" },
    { "info", vlcObject_info, METH_VARARGS,
       "info() -> dict    Return information about the object" },
    { "release", vlcObject_release, METH_VARARGS,
      "release() ->     Release the VLC Object" },
    { NULL, NULL, 0, NULL },
};

static PyTypeObject vlcObject_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "vlc.Object",       /*tp_name*/
    sizeof(vlcObject_Type), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)vlcObject_dealloc,      /*tp_dealloc*/
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
    "Explore VLC objects.",  /* tp_doc */
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

#endif


/*****************************************************************************
 * VLC MediaControl object implementation
 *****************************************************************************/

static PyObject *MediaControl_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MediaControl *self;
    mediacontrol_Exception *exception = NULL;
    PyObject* py_list = NULL;
    char** ppsz_args = NULL;

    self = PyObject_New(MediaControl, &MediaControl_Type);

    if (PyArg_ParseTuple(args, "O", &py_list))
    {
        int i_size;
        int i_index;

        Py_INCREF(py_list);
        if (! PySequence_Check(py_list))
        {
            PyErr_SetString(PyExc_TypeError, "Parameter must be a sequence.");
            return NULL;
        }
        i_size = PySequence_Size(py_list);
        ppsz_args = malloc(i_size + 1);
        if (! ppsz_args)
        {
            PyErr_SetString(PyExc_MemoryError, "Out of memory");
            return NULL;
        }

        for ( i_index = 0; i_index < i_size; i_index++ )
        {
            ppsz_args[i_index] =
               strdup( PyString_AsString( PyObject_Str(
                                               PySequence_GetItem(py_list,
                                                    i_index ) ) ) );
        }
        ppsz_args[i_size] = NULL;
        Py_DECREF(py_list);
    }
    else
    {
        /* No arguments were given. Clear the exception raised
	 * by PyArg_ParseTuple. */
        PyErr_Clear();
    }

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    self->mc = mediacontrol_new( ppsz_args, exception );
    MC_EXCEPT;
    Py_END_ALLOW_THREADS

    Py_INCREF(self);
    return (PyObject *)self;
}

static void MediaControl_dealloc(PyObject *self)
{
    PyMem_DEL(self);
}

/**
 *  Returns the current position in the stream. The returned value can
   be relative or absolute (according to PositionOrigin) and the unit
   is set by PositionKey
 */
static PyObject * MediaControl_get_media_position(
                                      PyObject *self, PyObject *args)
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

    origin = positionOrigin_py_to_c(py_origin);
    key    = positionKey_py_to_c(py_key);

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    pos = mediacontrol_get_media_position(SELF->mc, origin, key, exception);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    py_retval = (PyObject*)position_c_to_py(pos);
    free( pos );
    return py_retval;
}

/** Sets the media position */
static PyObject *MediaControl_set_media_position(
                                        PyObject *self, PyObject *args )
{
    mediacontrol_Exception* exception = NULL;
    mediacontrol_Position *a_position;
    PyObject *py_pos;

    if( !PyArg_ParseTuple( args, "O", &py_pos ) )
        return NULL;

    a_position = position_py_to_c(py_pos);
    if (!a_position )
    {
        PyErr_SetString(PyExc_MemoryError, "Out of memory");
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_set_media_position( SELF->mc, a_position, exception );
    free( a_position );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *MediaControl_start(PyObject *self, PyObject *args)
{
    mediacontrol_Position *a_position;
    mediacontrol_Exception *exception = NULL;
    PyObject *py_pos;

    if( !PyArg_ParseTuple(args, "O", &py_pos ) )
        return NULL;

    a_position = position_py_to_c(py_pos);
    if ( !a_position )
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_start(SELF->mc, a_position, exception);
    free(a_position);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *MediaControl_pause(PyObject *self, PyObject *args)
{
    mediacontrol_Position *a_position;
    mediacontrol_Exception *exception = NULL;
    PyObject *py_pos;

    if( !PyArg_ParseTuple(args, "O", &py_pos ) )
        return NULL;

    a_position = position_py_to_c(py_pos);

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_pause(SELF->mc, a_position, exception);
    free(a_position);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

  Py_INCREF( Py_None );
  return Py_None;
}

static PyObject * MediaControl_resume(PyObject *self, PyObject *args)
{
    mediacontrol_Position *a_position;
    mediacontrol_Exception *exception = NULL;
    PyObject *py_pos;

    if( !PyArg_ParseTuple(args, "O", &py_pos ) )
        return NULL;

    a_position = position_py_to_c(py_pos);

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_start(SELF->mc, a_position, exception);
    free(a_position);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *MediaControl_stop(PyObject *self, PyObject *args)
{
    mediacontrol_Position *a_position;
    mediacontrol_Exception *exception = NULL;
    PyObject *py_pos;

    if( !PyArg_ParseTuple(args, "O", &py_pos ) )
        return NULL;

    a_position = position_py_to_c(py_pos);

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_stop(SELF->mc, a_position, exception);
    free(a_position);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *MediaControl_exit(PyObject *self, PyObject *args)
{
    mediacontrol_exit(SELF->mc);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *MediaControl_playlist_add_item(PyObject *self, PyObject *args)
{
    char *psz_file;
    mediacontrol_Exception *exception = NULL;

    if ( !PyArg_ParseTuple(args, "s", &psz_file) )
      return NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_playlist_add_item(SELF->mc, psz_file, exception);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * MediaControl_playlist_clear( PyObject *self,
                                                PyObject *args)
{
    mediacontrol_Exception *exception = NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_playlist_clear(SELF->mc, exception);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * MediaControl_playlist_get_list( PyObject *self,
                                                  PyObject *args )
{
    PyObject *py_retval;
    mediacontrol_Exception *exception = NULL;
    mediacontrol_PlaylistSeq* pl;
    int i_index;
    int i_playlist_size;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    pl = mediacontrol_playlist_get_list(SELF->mc, exception);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    i_playlist_size = pl->size;

    py_retval = PyList_New( i_playlist_size );

    for (i_index = 0 ; i_index < i_playlist_size ; i_index++)
    {
        PyList_SetItem( py_retval, i_index,
                        Py_BuildValue("s", pl->data[i_index] ) );
    }
    mediacontrol_PlaylistSeq__free(pl);

    return py_retval;
}


static PyObject * MediaControl_snapshot(PyObject *self, PyObject *args)
{
    mediacontrol_RGBPicture *retval  = NULL;
    mediacontrol_Exception* exception = NULL;
    mediacontrol_Position *a_position    = NULL;
    PyObject *py_pos        = NULL;
    PyObject *py_obj        = NULL;

    if( !PyArg_ParseTuple(args, "O", &py_pos))
      return NULL;

    a_position = position_py_to_c(py_pos);

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    retval = mediacontrol_snapshot(SELF->mc, a_position, exception);
    free( a_position );
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    if( !retval )
    {
        Py_INCREF( Py_None );
        return Py_None;
    }

    /* FIXME: create a real RGBPicture object */
    py_obj = PyDict_New();

    PyDict_SetItemString(py_obj, "width",
                    Py_BuildValue("i", retval->width) );
    PyDict_SetItemString(py_obj, "height",
                    Py_BuildValue("i", retval->height) );
    PyDict_SetItemString(py_obj, "type",
                    Py_BuildValue("i", retval->type) );
    PyDict_SetItemString(py_obj, "data",
                  Py_BuildValue("s#", retval->data, retval->size) );

    /*  Py_INCREF(py_obj); */
    return py_obj;
}

static PyObject* MediaControl_display_text(PyObject *self, PyObject *args)
{
    mediacontrol_Exception* exception = NULL;
    PyObject *py_begin, *py_end;
    char* message;
    mediacontrol_Position * begin;
    mediacontrol_Position * end;

    if( !PyArg_ParseTuple(args, "sOO", &message, &py_begin, &py_end))
      return NULL;

    begin = position_py_to_c(py_begin);
    end   = position_py_to_c(py_end);

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_display_text(SELF->mc, message, begin, end, exception);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    free(begin);
    free(end);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* MediaControl_get_stream_information(
                        PyObject *self, PyObject *args)
{
    mediacontrol_StreamInformation *retval  = NULL;
    mediacontrol_Exception* exception = NULL;
    PyObject *py_obj;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    retval = mediacontrol_get_stream_information(
                                SELF->mc, mediacontrol_MediaTime, exception);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    py_obj = PyDict_New();

     /* FIXME: create a real StreamInformation object */
    PyDict_SetItemString(py_obj, "status",
                  Py_BuildValue("i", retval->streamstatus));
    PyDict_SetItemString(py_obj, "url",
                  Py_BuildValue("s", retval->url));
    PyDict_SetItemString(py_obj, "position",
                  Py_BuildValue("L", retval->position));
    PyDict_SetItemString(py_obj, "length",
                  Py_BuildValue("L", retval->length));

    free(retval->url);
    free(retval);

    /* Py_INCREF(py_obj); */
    return py_obj;
}

static PyObject* MediaControl_sound_set_volume(PyObject *self, PyObject *args)
{
    mediacontrol_Exception* exception = NULL;
    unsigned short volume;

    if (!PyArg_ParseTuple(args, "H", &volume))
       return NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_sound_set_volume(SELF->mc, volume, exception);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* MediaControl_sound_get_volume(PyObject *self, PyObject *args)
{
    mediacontrol_Exception* exception = NULL;
    PyObject *py_retval;
    unsigned short volume;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    volume=mediacontrol_sound_get_volume(SELF->mc, exception);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    py_retval = Py_BuildValue("H", volume);
    return py_retval;
}

static PyObject* MediaControl_set_visual(PyObject *self, PyObject *args)
{
    mediacontrol_Exception* exception = NULL;
    WINDOWHANDLE visual;

    if (!PyArg_ParseTuple(args, "i", &visual))
       return NULL;

    Py_BEGIN_ALLOW_THREADS
    MC_TRY;
    mediacontrol_set_visual(SELF->mc, visual, exception);
    Py_END_ALLOW_THREADS
    MC_EXCEPT;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef MediaControl_methods[] =
{
    {"get_media_position", MediaControl_get_media_position, METH_VARARGS,
     "get_media_position(origin, key) -> Position    Get current media position." },
    { "set_media_position", MediaControl_set_media_position, METH_VARARGS,
      "set_media_position(Position)            Set media position" },
    { "start", MediaControl_start, METH_VARARGS, 
      "start(Position)         Start the player." },
    { "pause", MediaControl_pause, METH_VARARGS,
      "pause(Position)         Pause the player." },
    { "resume", MediaControl_resume, METH_VARARGS,
      "resume(Position)        Resume the player" },
    { "stop", MediaControl_stop, METH_VARARGS,
      "stop(Position)              Stop the player" },
    { "exit", MediaControl_exit, METH_VARARGS,
      "exit()                     Exit the player" },
    { "playlist_add_item", MediaControl_playlist_add_item, METH_VARARGS,
      "playlist_add_item(str)               Add an item to the playlist" },
    { "playlist_get_list", MediaControl_playlist_get_list, METH_VARARGS,
      "playlist_get_list() -> list       Get the contents of the playlist" },
    { "playlist_clear", MediaControl_playlist_clear, METH_VARARGS,
      "clear()         Clear the playlist." },
    { "snapshot", MediaControl_snapshot, METH_VARARGS,
      "snapshot(Position) -> dict        Take a snapshot" },
    { "display_text", MediaControl_display_text, METH_VARARGS,
      "display_text(str, Position, Position)    Display a text on the video" },
    { "get_stream_information", MediaControl_get_stream_information,
      METH_VARARGS,
      "get_stream_information() -> dict      Get information about the stream"},
    { "sound_get_volume", MediaControl_sound_get_volume, METH_VARARGS,
      "sound_get_volume() -> int       Get the volume" },
    { "sound_set_volume", MediaControl_sound_set_volume, METH_VARARGS,
      "sound_set_volume(int)           Set the volume" },
    { "set_visual", MediaControl_set_visual, METH_VARARGS,
      "set_visual(int)           Set the embedding window visual ID" },  
    { NULL, NULL, 0, NULL },
};

static PyTypeObject MediaControl_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "vlc.MediaControl",        /*tp_name*/
    sizeof(MediaControl_Type), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)MediaControl_dealloc,      /*tp_dealloc*/
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
    "Control of a VLC instance.",  /* tp_doc */
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

/***********************************************************************
 * Position
 ***********************************************************************/


static int PyPosition_init( PyPosition *self, PyObject *args, PyObject *kwds )
{
    self->origin = mediacontrol_AbsolutePosition;
    self->key    = mediacontrol_MediaTime;
    self->value  = 0;
    return 0;
}

mediacontrol_PositionKey positionKey_py_to_c( PyObject * py_key )
{
    mediacontrol_PositionKey key_position = mediacontrol_MediaTime;
    int key;

    if( !PyArg_Parse( py_key, "i", &key ) )
    {
        PyErr_SetString (MediaControl_InternalException, "Invalid key value"); 
        return key_position;
    }

    switch (key)
    {
        case 0: key = mediacontrol_ByteCount;   break;
        case 1: key = mediacontrol_SampleCount; break;
        case 2: key = mediacontrol_MediaTime;   break;
    }
    return key_position;
}

mediacontrol_PositionOrigin positionOrigin_py_to_c( PyObject * py_origin )
{
    mediacontrol_PositionOrigin  origin_position =
            mediacontrol_AbsolutePosition;
    int origin;

    if(!PyArg_Parse(py_origin,"i", &origin))
    {
        PyErr_SetString( MediaControl_InternalException,
                         "Invalid origin value");
        return origin_position;
    }

    switch (origin)
    {
       case 0: origin_position = mediacontrol_AbsolutePosition; break;
       case 1: origin_position = mediacontrol_RelativePosition; break;
       case 2: origin_position = mediacontrol_ModuloPosition;   break;
    }

    return origin_position;
}

/* Methods for transforming the Position Python object to Position structure*/
mediacontrol_Position* position_py_to_c( PyObject * py_position )
{
    mediacontrol_Position * a_position = NULL;
    PyPosition *pos = (PyPosition*)py_position;

    a_position = (mediacontrol_Position*)malloc(sizeof(mediacontrol_Position));
    if (! a_position)
    {
        PyErr_SetString(PyExc_MemoryError, "Out of memory");
        return NULL;
    }

    if (PyObject_IsInstance(py_position, (PyObject*)&PyPosition_Type))
    {
        a_position->origin = pos->origin;
        a_position->key    = pos->key;
        a_position->value  = pos->value;
    }
    else
    {
        /* Feature: if we give an integer, it will be considered as
           a relative position in mediatime */
        a_position->origin = mediacontrol_RelativePosition;
        a_position->key    = mediacontrol_MediaTime;
        a_position->value  = PyLong_AsLongLong(py_position);
    }
    return a_position;
}

PyPosition* position_c_to_py(mediacontrol_Position *position)
{
    PyPosition* py_retval;

    py_retval = PyObject_New(PyPosition, &PyPosition_Type);
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
    { "origin", T_INT, offsetof(PyPosition, origin), 0, "Position origin" },
    { "key",    T_INT, offsetof(PyPosition, key),    0, "Position key" },
    { "value",  T_ULONG, offsetof(PyPosition, value), 0, "Position value" },
    { NULL }  /* Sentinel */
};

static PyTypeObject PyPosition_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "vlc.Position",            /*tp_name*/
    sizeof(PyPosition_Type),   /*tp_basicsize*/
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
    "Represent a Position with origin, key and value",  /* tp_doc */
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
    (initproc)PyPosition_init, /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};
