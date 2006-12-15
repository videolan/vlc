#include <Python.h>

extern int asserts;

#define ASSERT( a, message ) asserts++;if( !(a) ) { fprintf( stderr, "Assert failed at %s:%i\n", __FILE__, __LINE__); PyErr_SetString( PyExc_AssertionError, message  " - " #a ); return NULL; }

#define DECLARE_MODULE( module ) PyMODINIT_FUNC init##module( void ) {  \
        Py_InitModule( #module, module##_methods );                     \
}

#define ASSERT_NOEXCEPTION asserts++; if( libvlc_exception_raised( &exception ) ) { \
         if( libvlc_exception_get_message( &exception ) )  PyErr_SetString( PyExc_AssertionError, libvlc_exception_get_message( &exception ) ); \
         else PyErr_SetString( PyExc_AssertionError, "Exception raised" ); return NULL; }

#define ASSERT_EXCEPTION asserts ++; if( !libvlc_exception_raised( &exception ) ) { \
         if( libvlc_exception_get_message( &exception ) )  PyErr_SetString( PyExc_AssertionError, libvlc_exception_get_message( &exception ) ); \
         else PyErr_SetString( PyExc_AssertionError, "Exception not raised" ); return NULL; }


#define DEF_METHOD( method, desc ) { #method, method, METH_VARARGS, desc},
