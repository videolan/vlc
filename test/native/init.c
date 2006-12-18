#include "../pyunit.h"
#include "tests.h"

PyObject *init( PyObject *self, PyObject *args )
{
    (void)setvbuf (stdout, NULL, _IONBF, 0);
    Py_INCREF( Py_None );
    return Py_None;
}

static PyMethodDef native_libvlc_test_methods[] = {
   DEF_METHOD( init, "Init some stuff" )
   DEF_METHOD( create_destroy, "Create and destroy" )
   DEF_METHOD( exception_test, "Test Exception handling" )
   DEF_METHOD( playlist_test, "Test Playlist interaction" )
   DEF_METHOD( vlm_test, "Test VLM" )
   DEF_METHOD( timers_test, "Test timers" )
   DEF_METHOD( i18n_atof_test, "Test i18n_atof" )
   DEF_METHOD( url_test, "URL decoding" )
   DEF_METHOD( chains_test, "Test building of chains" )
   DEF_METHOD( gui_chains_test, "Test interactions between chains and GUI" )
   DEF_METHOD( psz_chains_test, "Test building of chain strings" )
   DEF_METHOD( arrays_test, "Test arrays")
   DEF_METHOD( bsearch_direct_test, "Test Bsearch without structure" )
   DEF_METHOD( bsearch_member_test, "Test Bsearch with structure" )
   DEF_METHOD( dict_test, "Test dictionnaries" )
   DEF_METHOD( threadvar_test, "Test TLS" )
   { NULL, NULL, 0, NULL }
};

asserts =0;

DECLARE_MODULE( native_libvlc_test )
