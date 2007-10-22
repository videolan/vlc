#include "../pyunit.h"
#include <vlc/libvlc.h>

PyObject *exception_test( PyObject *self, PyObject *args )
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

    libvlc_exception_clear( &exception );
    ASSERT( !libvlc_exception_raised( &exception ), "Exception not cleared" );

    Py_INCREF( Py_None );
    return Py_None;
}

PyObject *create_destroy( PyObject *self, PyObject *args )
{
    libvlc_instance_t *p_i1, *p_i2;
    char *argv1[] = { "vlc", "--quiet" };
    char *argv2[]= { "vlc", "-vvv" };
    int id1,id2;

    printf( "\n" );

    libvlc_exception_t exception;
    libvlc_exception_init( &exception );

    /* Create and destroy a single instance */
    fprintf( stderr, "Create 1\n" );
    p_i1 = libvlc_new( 2, argv1, &exception );
    ASSERT( p_i1 != NULL, "Instance creation failed" );
    ASSERT_NOEXCEPTION;
    id1 = libvlc_get_vlc_id( p_i1 );
    libvlc_release( p_i1, &exception );
    ASSERT_NOEXCEPTION;

    /* Create and destroy two instances */
    fprintf( stderr, "Create 2\n" );
    p_i1 = libvlc_new( 2, argv1, &exception );
    ASSERT( p_i1 != NULL, "Instance creation failed" );
    ASSERT_NOEXCEPTION;

    fprintf( stderr, "Create 3\n" );
    p_i2 = libvlc_new( 2, argv2, &exception );
    ASSERT( p_i2 != NULL, "Instance creation failed" );
    ASSERT_NOEXCEPTION;

    fprintf( stderr, "Destroy 1\n" );
    libvlc_release( p_i1, &exception );
    ASSERT_NOEXCEPTION;
    fprintf( stderr, "Destroy 2\n" );
    libvlc_release( p_i2, &exception );
    ASSERT_NOEXCEPTION;

    /* Deinit */
    fprintf( stderr, "Create 4\n" );
    p_i1 = libvlc_new( 2, argv1, &exception );
    ASSERT_NOEXCEPTION;
    id2 = libvlc_get_vlc_id( p_i1 );

    ASSERT( id1 == id2, "libvlc object ids do not match after deinit" );

    Py_INCREF( Py_None );
    return Py_None;
}

 PyObject *playlist_test( PyObject *self, PyObject *args )
{
    libvlc_instance_t *p_instance;
    char *argv[] = { "vlc", "--quiet" };
    int i_id, i_playing, i_items;

    libvlc_exception_t exception;
    libvlc_exception_init( &exception );

    p_instance = libvlc_new( 2, argv, &exception );
    ASSERT_NOEXCEPTION;

    /* Initial status */
    libvlc_playlist_play( p_instance, 0, 0, argv, &exception );
    ASSERT( libvlc_exception_raised( &exception ),
            "Playlist empty and exception not raised" );

    libvlc_exception_clear( &exception );

    i_playing  = libvlc_playlist_isplaying( p_instance, &exception  );
    ASSERT_NOEXCEPTION;
    ASSERT( i_playing == 0, "Playlist shouldn't be running" );
    i_items = libvlc_playlist_items_count( p_instance, &exception );
    ASSERT_NOEXCEPTION;
    ASSERT( i_items == 0, "Playlist should be empty" );

    /* Add 1 item */
    libvlc_exception_clear( &exception );
    i_id = libvlc_playlist_add( p_instance, "test" , NULL , &exception );
    ASSERT_NOEXCEPTION;
    ASSERT( i_id > 0 , "Returned identifier is <= 0" );
    i_items = libvlc_playlist_items_count( p_instance, &exception );
    ASSERT_NOEXCEPTION;
    ASSERT( i_items == 1, "Playlist should have 1 item" );
    i_playing  = libvlc_playlist_isplaying( p_instance, &exception  );
    ASSERT_NOEXCEPTION;
    ASSERT( i_playing == 0, "Playlist shouldn't be running" );

    /* */
 
    Py_INCREF( Py_None );
    return Py_None;
}

 PyObject *vlm_test( PyObject *self, PyObject *args )
{
    libvlc_instance_t *p_instance;
    char *argv[] = { "vlc", "--quiet" };
    char *ppsz_empty[] = {};
    libvlc_exception_t exception;
    libvlc_exception_init( &exception );

    p_instance = libvlc_new( 2, argv, &exception );
    ASSERT_NOEXCEPTION;
 
    /* Test that working on unexisting streams fail */
    libvlc_vlm_set_enabled( p_instance, "test", 1, &exception );
    ASSERT_EXCEPTION;
    libvlc_exception_clear( &exception );
    libvlc_vlm_set_input( p_instance, "test", "input", &exception );
    ASSERT_EXCEPTION;
    libvlc_exception_clear( &exception );
    libvlc_vlm_del_media( p_instance, "test", &exception );
    ASSERT_EXCEPTION;
    libvlc_exception_clear( &exception );

    /*******  Broadcast *******/
    /* Now create a media */
    libvlc_vlm_add_broadcast( p_instance, "test", "input_test", "output_test",
                              0, ppsz_empty, 1, 1, &exception );
    ASSERT_NOEXCEPTION;
    libvlc_exception_clear( &exception );

    /* Change its parameters */
    libvlc_vlm_set_enabled( p_instance, "test", 0, &exception );
    ASSERT_NOEXCEPTION;
    libvlc_exception_clear( &exception );
    libvlc_vlm_set_output( p_instance, "test", "output_test2", &exception );
    ASSERT_NOEXCEPTION;
    libvlc_exception_clear( &exception );

    /* Check the parameters */
    fprintf( stderr, "The code for this is not written yet\n");

    /* Control it a bit */
    fprintf( stderr, "The code for this is not written yet\n");

    /* Try to delete it */
    libvlc_vlm_del_media( p_instance, "test", &exception );
    ASSERT_NOEXCEPTION;
    libvlc_exception_clear( &exception );

    libvlc_vlm_del_media( p_instance, "test", &exception );
    ASSERT_EXCEPTION;
    libvlc_exception_clear( &exception );

    /*******  VOD *******/

    Py_INCREF( Py_None );
    return Py_None;
}
