#include "../pyunit.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

PyObject *threadvar_test( PyObject *self, PyObject *args )
{
    void *p_foo = malloc(1);
    vlc_threadvar_t key, key2;

    vlc_threadvar_create( NULL, &key );
    vlc_threadvar_set( &key, p_foo );
    ASSERT( vlc_threadvar_get( &key ) == p_foo, "key does not match" );

    vlc_threadvar_create( NULL, &key2 );
    vlc_threadvar_set( &key2, NULL );
    ASSERT( vlc_threadvar_get( &key2 ) == NULL, "key2 does not match" );
 
    Py_INCREF( Py_None );
    return Py_None;
}
