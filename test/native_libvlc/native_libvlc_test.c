#include "../pyunit.h"
#include <vlc/libvlc.h>

static PyObject *exception_test( PyObject *self, PyObject *args )
{
     libvlc_exception_t exception;

     libvlc_exception_init( &exception );
     ASSERT( !libvlc_exception_raised( &exception) , "Exception raised" );
     ASSERT( !libvlc_exception_get_message( &exception) , "Exception raised" );

     libvlc_exception_raise( &exception, NULL );
     ASSERT( !libvlc_exception_get_message( &exception), "Unexpected message" );
     ASSERT( libvlc_exception_raised( &exception), "Exception not raised" );

     libvlc_exception_raise( &exception, "test" );
     ASSERT( libvlc_exception_get_message( &exception), "No Message" );
     ASSERT( libvlc_exception_raised( &exception), "Exception not raised" );

     Py_INCREF( Py_None );
     return Py_None;
}

static PyObject *create_destroy( PyObject *self, PyObject *args )
{
     libvlc_instance_t *p_instance;
     char *argv[] = {};

     libvlc_exception_t exception;
     libvlc_exception_init( &exception );

     p_instance = libvlc_new( 0, argv, &exception );

     ASSERT( p_instance != NULL, "Instance creation failed" );

     ASSERT( !libvlc_exception_raised( &exception ),
             "Exception raised while creating instance" );

     libvlc_destroy( p_instance );
     
     Py_INCREF( Py_None );
     return Py_None;
}

static PyMethodDef native_libvlc_test_methods[] = {
   DEF_METHOD( create_destroy, "Create and destroy" )
   DEF_METHOD( exception_test, "Test Exception handling" )
   { NULL, NULL, 0, NULL }
};

DECLARE_MODULE( native_libvlc_test )
