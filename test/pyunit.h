#include <Python.h>

#define ASSERT( a, message ) if( !(a) ) { fprintf( stderr, "ASSERTION FAILED\n" ); PyErr_SetString( PyExc_AssertionError, message ); return NULL; }

#define DECLARE_MODULE( module ) PyMODINIT_FUNC init##module( void ) {  \
        Py_InitModule( #module, module##_methods );                     \
}

#define DEF_METHOD( method, desc ) { #method, method, METH_VARARGS, desc},
