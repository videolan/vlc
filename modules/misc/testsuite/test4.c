/*****************************************************************************
 * test4.c : Miscellaneous stress tests module for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>

#include <stdlib.h>
#include <signal.h>

/*****************************************************************************
 * Defines
 *****************************************************************************/
#define MAXVAR        50                    /* Number of variables to create */
#define MAXSET      2000                       /* Number of variables to set */
#define MAXOBJ      1000                      /* Number of objects to create */
#define MAXLOOK    10000                      /* Number of objects to lookup */
#define MAXTH          4                       /* Number of threads to spawn */

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int    Foo       ( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int    Callback  ( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int    MyCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static void * MyThread  ( vlc_object_t * );

static int    Stress    ( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static void * Dummy     ( vlc_object_t * );

static int    Signal    ( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Miscellaneous stress tests") );
    var_Create( p_module->p_libvlc, "foo-test",
                VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_module->p_libvlc, "foo-test", Foo, NULL );
    var_Create( p_module->p_libvlc, "callback-test",
                VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_module->p_libvlc, "callback-test", Callback, NULL );
    var_Create( p_module->p_libvlc, "stress-test",
                VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_module->p_libvlc, "stress-test", Stress, NULL );
    var_Create( p_module->p_libvlc, "signal",
                VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_module->p_libvlc, "signal", Signal, NULL );
vlc_module_end();

/*****************************************************************************
 * Foo: put anything here
 *****************************************************************************/
static int Foo( vlc_object_t *p_this, char const *psz_cmd,
                vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vlc_value_t val;
    int i;

    var_Create( p_this, "honk", VLC_VAR_STRING | VLC_VAR_HASCHOICE );

    val.psz_string = "foo";
    var_Change( p_this, "honk", VLC_VAR_ADDCHOICE, &val, NULL );
    val.psz_string = "bar";
    var_Change( p_this, "honk", VLC_VAR_ADDCHOICE, &val, NULL );
    val.psz_string = "baz";
    var_Change( p_this, "honk", VLC_VAR_ADDCHOICE, &val, NULL );
    var_Change( p_this, "honk", VLC_VAR_SETDEFAULT, &val, NULL );

    var_Get( p_this, "honk", &val ); printf( "value: %s\n", val.psz_string );

    val.psz_string = "foo";
    var_Set( p_this, "honk", val );

    var_Get( p_this, "honk", &val ); printf( "value: %s\n", val.psz_string );

    val.psz_string = "blork";
    var_Set( p_this, "honk", val );

    var_Get( p_this, "honk", &val ); printf( "value: %s\n", val.psz_string );

    val.psz_string = "baz";
    var_Change( p_this, "honk", VLC_VAR_DELCHOICE, &val, NULL );

    var_Get( p_this, "honk", &val ); printf( "value: %s\n", val.psz_string );

    var_Change( p_this, "honk", VLC_VAR_GETLIST, &val, NULL );
    for( i = 0 ; i < val.p_list->i_count ; i++ )
    {
        printf( "value %i: %s\n", i, val.p_list->p_values[i].psz_string );
    }
    var_Change( p_this, "honk", VLC_VAR_FREELIST, &val, NULL );

    var_Destroy( p_this, "honk" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Callback: test callback functions
 *****************************************************************************/
static int Callback( vlc_object_t *p_this, char const *psz_cmd,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    int i;
    char psz_var[20];
    vlc_object_t *pp_objects[10];
    vlc_value_t val;

    /* Allocate a few variables */
    for( i = 0; i < 1000; i++ )
    {
        sprintf( psz_var, "blork-%i", i );
        var_Create( p_this, psz_var, VLC_VAR_INTEGER );
        var_AddCallback( p_this, psz_var, MyCallback, NULL );
    }

    /*
     *  Test #1: callback loop detection
     */
    printf( "Test #1: callback loop detection\n" );

    printf( " - without boundary check (vlc should print an error)\n" );
    val.i_int = 1234567;
    var_Set( p_this, "blork-12", val );

    printf( " - with boundary check\n" );
    val.i_int = 12;
    var_Set( p_this, "blork-12", val );

    /*
     *  Test #2: concurrent access
     */
    printf( "Test #2: concurrent access\n" );

    printf( " - launch worker threads\n" );

    for( i = 0; i < 10; i++ )
    {
        pp_objects[i] = vlc_object_create( p_this, VLC_OBJECT_GENERIC );
        vlc_object_attach( pp_objects[i], p_this );
        vlc_thread_create( pp_objects[i], "foo", MyThread, 0, VLC_TRUE );
    }

    msleep( 3000000 );

    printf( " - kill worker threads\n" );

    for( i = 0; i < 10; i++ )
    {
        pp_objects[i]->b_die = VLC_TRUE;
        vlc_thread_join( pp_objects[i] );
        vlc_object_detach( pp_objects[i] );
        vlc_object_destroy( pp_objects[i] );
    }

    /* Clean our mess */
    for( i = 0; i < 1000; i++ )
    {
        sprintf( psz_var, "blork-%i", i );
        var_DelCallback( p_this, psz_var, MyCallback, NULL );
        var_Destroy( p_this, psz_var );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * MyCallback: used by callback-test
 *****************************************************************************/
static int MyCallback( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    int i_var = 1 + atoi( psz_var + strlen("blork-") );
    char psz_newvar[20];

    if( i_var == 1000 )
    {
        i_var = 0;
    }

    /* If we are requested to stop, then stop. */
    if( i_var == newval.i_int )
    {
        return VLC_SUCCESS;
    }

    /* If we're the blork-3 callback, set blork-4, and so on. */
    sprintf( psz_newvar, "blork-%i", i_var );
    var_Set( p_this, psz_newvar, newval );

    return VLC_SUCCESS;   
}

/*****************************************************************************
 * MyThread: used by callback-test, creates objects and then do nothing.
 *****************************************************************************/
static void * MyThread( vlc_object_t *p_this )
{
    char psz_var[20];
    vlc_value_t val;
    vlc_object_t *p_parent = p_this->p_parent;

    vlc_thread_ready( p_this );

    val.i_int = 42;

    while( !p_this->b_die )
    {
        int i = (int) (100.0 * rand() / (RAND_MAX));

        sprintf( psz_var, "blork-%i", i );
        val.i_int = i + 200;
        var_Set( p_parent, psz_var, val );

        /* This is quite heavy, but we only have 10 threads. Keep cool. */
        msleep( 1000 );
    }

    return NULL;
}

/*****************************************************************************
 * Stress: perform various stress tests
 *****************************************************************************/
static int Stress( vlc_object_t *p_this, char const *psz_cmd,
                   vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vlc_object_t **pp_objects;
    mtime_t start;
    char ** ppsz_name;
    char *  psz_blob;
    int     i, i_level;

    if( *newval.psz_string )
    {
        i_level = atoi( newval.psz_string );
        if( i_level <= 0 )
        {
            i_level = 1;
        }
        else if( i_level > 200 )
        {
            /* It becomes quite dangerous above 150 */
            i_level = 200;
        }
    }
    else
    {
        i_level = 10;
    }

    /* Allocate required data */
    ppsz_name = malloc( MAXVAR * i_level * sizeof(char*) );
    psz_blob = malloc( 20 * MAXVAR * i_level * sizeof(char) );
    for( i = 0; i < MAXVAR * i_level; i++ )
    {
        ppsz_name[i] = psz_blob + 20 * i;
    }

    pp_objects = malloc( MAXOBJ * i_level * sizeof(void*) );

    /*
     *  Test #1: objects
     */
    printf( "Test #1: objects\n" );

    printf( " - creating %i objects\n", MAXOBJ * i_level );
    start = mdate();
    for( i = 0; i < MAXOBJ * i_level; i++ )
    {
        pp_objects[i] = vlc_object_create( p_this, VLC_OBJECT_GENERIC );
    }

    printf( " - randomly looking up %i objects\n", MAXLOOK * i_level );
    for( i = MAXLOOK * i_level; i--; )
    {
        int id = (int) (MAXOBJ * i_level * 1.0 * rand() / (RAND_MAX));
        vlc_object_get( p_this, pp_objects[id]->i_object_id );
        vlc_object_release( p_this );
    }

    printf( " - destroying the objects (LIFO)\n" );
    for( i = MAXOBJ * i_level; i--; )
    {
        vlc_object_destroy( pp_objects[i] );
    }

    printf( "done (%fs).\n", (mdate() - start) / 1000000.0 );

    /*
     *  Test #2: integer variables
     */
    printf( "Test #2: integer variables\n" );

    printf( " - creating %i integer variables\n", MAXVAR * i_level );
    start = mdate();
    for( i = 0; i < MAXVAR * i_level; i++ )
    {
        sprintf( ppsz_name[i], "foo-%04i-bar-%04x", i, i * 11 );
        var_Create( p_this, ppsz_name[i], VLC_VAR_INTEGER );
    }

    printf( " - randomly assigning %i values\n", MAXSET * i_level );
    for( i = 0; i < MAXSET * i_level; i++ )
    {
        int v = (int) (MAXVAR * i_level * 1.0 * rand() / (RAND_MAX));
        var_Set( p_this, ppsz_name[v], (vlc_value_t)i );
    }

    printf( " - destroying the variables\n" );
    for( i = 0; i < MAXVAR * i_level; i++ )
    {
        var_Destroy( p_this, ppsz_name[i] );
    }

    printf( "done (%fs).\n", (mdate() - start) / 1000000.0 );

    /*
     *  Test #3: string variables
     */
    printf( "Test #3: string variables\n" );

    printf( " - creating %i string variables\n", MAXVAR * i_level );
    start = mdate();
    for( i = 0; i < MAXVAR * i_level; i++ )
    {
        sprintf( ppsz_name[i], "foo-%04i-bar-%04x", i, i * 11 );
        var_Create( p_this, ppsz_name[i], VLC_VAR_STRING );
    }

    printf( " - randomly assigning %i values\n", MAXSET * i_level );
    for( i = 0; i < MAXSET * i_level; i++ )
    {
        int v = (int) (MAXVAR * i_level * 1.0 * rand() / (RAND_MAX));
        var_Set( p_this, ppsz_name[v], (vlc_value_t)ppsz_name[v] );
    }

    printf( " - destroying the variables\n" );
    for( i = 0; i < MAXVAR * i_level; i++ )
    {
        var_Destroy( p_this, ppsz_name[i] );
    }

    printf( "done (%fs).\n", (mdate() - start) / 1000000.0 );

    /*
     *  Test #4: threads
     */
    printf( "Test #4: threads\n" );
    start = mdate();

    printf( " - spawning %i threads that will each create %i objects\n",
            MAXTH * i_level, MAXOBJ/MAXTH );
    for( i = 0; i < MAXTH * i_level; i++ )
    {
        pp_objects[i] = vlc_object_create( p_this, VLC_OBJECT_GENERIC );
        vlc_thread_create( pp_objects[i], "foo", Dummy, 0, VLC_TRUE );
    }

    printf( " - killing the threads (LIFO)\n" );
    for( i = MAXTH * i_level; i--; )
    {
        pp_objects[i]->b_die = VLC_TRUE;
        vlc_thread_join( pp_objects[i] );
        vlc_object_destroy( pp_objects[i] );
    }

    printf( "done (%fs).\n", (mdate() - start) / 1000000.0 );

    /* Free required data */
    free( pp_objects );
    free( psz_blob );
    free( ppsz_name );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Dummy: used by stress-test, creates objects and then do nothing.
 *****************************************************************************/
static void * Dummy( vlc_object_t *p_this )
{
    int i;
    vlc_object_t *pp_objects[MAXOBJ/MAXTH];

    for( i = 0; i < MAXOBJ/MAXTH; i++ )
    {
        pp_objects[i] = vlc_object_create( p_this, VLC_OBJECT_GENERIC );
    }

    vlc_thread_ready( p_this );

    while( !p_this->b_die )
    {
        msleep( 10000 );
    }

    for( i = MAXOBJ/MAXTH; i--; )
    {
        vlc_object_destroy( pp_objects[i] );
    }

    return NULL;
}

/*****************************************************************************
 * Signal: send a signal to the current thread.
 *****************************************************************************/
static int Signal( vlc_object_t *p_this, char const *psz_cmd,
                   vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    raise( atoi(newval.psz_string) );
    return VLC_SUCCESS;
}
