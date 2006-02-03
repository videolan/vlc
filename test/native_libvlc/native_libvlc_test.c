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
    char *argv[] = { "vlc", "--quiet" };

    libvlc_exception_t exception;
    libvlc_exception_init( &exception );

    p_instance = libvlc_new( 2, argv, &exception );

    ASSERT( p_instance != NULL, "Instance creation failed" );

    ASSERT( !libvlc_exception_raised( &exception ),
             "Exception raised while creating instance" );

    libvlc_destroy( p_instance );
     
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *playlist_test( PyObject *self, PyObject *args )
{
    libvlc_instance_t *p_instance;
    char *argv[] = { "vlc", "--quiet" };
    int i_id;

    libvlc_exception_t exception;
    libvlc_exception_init( &exception );

    p_instance = libvlc_new( 2, argv, &exception );

    libvlc_playlist_play( p_instance, 0, 0, argv, &exception );

    ASSERT( libvlc_exception_raised( &exception ), 
            "Playlist empty and exception not raised" );

    libvlc_exception_clear( &exception );
    i_id = libvlc_playlist_add( p_instance, "test" , NULL , &exception );

    ASSERT_EXCEPTION;

    ASSERT( i_id > 0 , "Returned identifier is <= 0" );
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyMethodDef native_libvlc_test_methods[] = {
   DEF_METHOD( create_destroy, "Create and destroy" )
   DEF_METHOD( exception_test, "Test Exception handling" )
   DEF_METHOD( playlist_test, "Test Playlist interaction" )
   { NULL, NULL, 0, NULL }
};

DECLARE_MODULE( native_libvlc_test )
