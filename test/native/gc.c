#include "../pyunit.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

struct mygc
{
    VLC_GC_MEMBERS;
    int i;
};

typedef struct mygc mygc;

static void mygc_destructor( gc_object_t *p_gc )
{
    free( p_gc );
    p_gc = NULL;
};

static PyObject *gc_test( PyObject *self, PyObject *args )
{
     mygc *gc = (mygc *)malloc( sizeof( mygc ) );

     vlc_gc_init( gc, mygc_destructor, NULL );
     ASSERT( gc->i_gc_refcount == 0, "Refcount should be 0" );
     vlc_gc_incref( gc );
     ASSERT( gc->i_gc_refcount == 1, "Refcount should be 1" );
     vlc_gc_incref( gc );
     ASSERT( gc->i_gc_refcount == 2, "Refcount should be 2" );
     gc->i++;
     vlc_gc_decref( gc );
     ASSERT( gc->i_gc_refcount == 1, "Refcount should be 1" );
     vlc_gc_decref( gc );

     Py_INCREF( Py_None );
     return Py_None;
};

static PyMethodDef native_gc_test_methods[] = {
   DEF_METHOD( gc_test, "Test GC" )
   { NULL, NULL, 0, NULL }
};

asserts = 0;

DECLARE_MODULE( native_gc_test )
