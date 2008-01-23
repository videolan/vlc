#include "../pyunit.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

PyObject *timers_test( PyObject *self, PyObject *args )
{
     Py_INCREF( Py_None );
     return Py_None;
}
