#include "../pyunit.h"

/* Libvlc */
PyObject *exception_test( PyObject *self, PyObject *args );
PyObject *create_destroy( PyObject *self, PyObject *args );
PyObject *playlist_test( PyObject *self, PyObject *args );
PyObject *vlm_test( PyObject *self, PyObject *args );

PyObject *threadvar_test( PyObject *self, PyObject *args );

/* Stats */
PyObject *timers_test( PyObject *self, PyObject *args );

PyObject *url_test( PyObject *self, PyObject *args );

PyObject *i18n_atof_test( PyObject *self, PyObject *args );

/* Profiles */
PyObject *chains_test( PyObject *self, PyObject *args );
PyObject *gui_chains_test( PyObject *self, PyObject *args );
PyObject *psz_chains_test( PyObject *self, PyObject *args );

/* Algo */
PyObject *arrays_test( PyObject *self, PyObject *args );
PyObject *bsearch_direct_test( PyObject *self, PyObject *args );
PyObject *bsearch_member_test( PyObject *self, PyObject *args );
PyObject *dict_test( PyObject *self, PyObject *args );

