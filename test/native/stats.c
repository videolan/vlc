#include "../pyunit.h"
#include <vlc/vlc.h>

static PyObject *timers_test( PyObject *self, PyObject *args )
{
     Py_INCREF( Py_None );
     return Py_None;
}

static PyMethodDef native_stats_test_methods[] = {
   DEF_METHOD( timers_test, "Test timers" )
   { NULL, NULL, 0, NULL }
};

DECLARE_MODULE( native_stats_test )
