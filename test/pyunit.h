#include <Python.h>

#define ASSERT( a, message ) if( !(a) ) { fprintf( stderr, "ASSERTION FAILED\n" ); PyErr_SetString( PyExc_AssertionError, message ); return NULL; }

#define DECLARE_MODULE( module ) PyMODINIT_FUNC init##module( void ) {  \
        Py_InitModule( #module, module##_methods );                     \
}

#define ASSERT_EXCEPTION if( libvlc_exception_raised( &exception ) ) { \
         if( libvlc_exception_get_message( &exception ) )  PyErr_SetString( PyExc_AssertionError, libvlc_exception_get_message( &exception ) ); \
         else PyErr_SetString( PyExc_AssertionError, "Exception raised" ); return NULL; }

#define DEF_METHOD( method, desc ) { #method, method, METH_VARARGS, desc},
