
// TODO: Ugly, split correctly 
#include "libvlc.c"
#include "stats.c"
#include "i18n.c"
#include "url.c"

static PyMethodDef native_libvlc_test_methods[] = {
   DEF_METHOD( create_destroy, "Create and destroy" )
   DEF_METHOD( exception_test, "Test Exception handling" )
   DEF_METHOD( playlist_test, "Test Playlist interaction" )
   DEF_METHOD( vlm_test, "Test VLM" )
   DEF_METHOD( timers_test, "Test timers" )
   DEF_METHOD( i18n_atof_test, "Test i18n_atof" )
   DEF_METHOD( url_decode_test, "URL decoding" )
   { NULL, NULL, 0, NULL }
};

DECLARE_MODULE( native_libvlc_test )
