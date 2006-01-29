#include "../pyunit.h"

static PyObject *create_destroy( PyObject *self, PyObject *args )
{
     /* Test stuff here */
     
     Py_INCREF( Py_None );
     return Py_None;
}

static PyMethodDef native_libvlc_test_methods[] = {
   DEF_METHOD( create_destroy, "Create and destroy" )
   { NULL, NULL, 0, NULL }
};

DECLARE_MODULE( native_libvlc_test )
