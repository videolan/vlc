/*****************************************************************************
 * test4.c : Miscellaneous stress tests module for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: test4.c,v 1.2 2002/10/14 19:04:51 sam Exp $
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
static int    Stress    ( vlc_object_t *, char *, char * );
static void * Dummy     ( vlc_object_t * );

static int    Signal    ( vlc_object_t *, char *, char * );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Miscellaneous stress tests") );
    var_Create( p_module->p_libvlc, "stress", VLC_VAR_COMMAND );
    var_Set( p_module->p_libvlc, "stress", (vlc_value_t)(void*)Stress );
    var_Create( p_module->p_libvlc, "signal", VLC_VAR_COMMAND );
    var_Set( p_module->p_libvlc, "signal", (vlc_value_t)(void*)Signal );
vlc_module_end();

/*****************************************************************************
 * Stress: perform various tests
 *****************************************************************************/
static int Stress( vlc_object_t *p_this, char *psz_cmd, char *psz_arg )
{
    vlc_object_t **pp_objects;
    mtime_t start;
    char ** ppsz_name;
    char *  psz_blob;
    int     i, i_level;

    if( *psz_arg )
    {
        i_level = atoi( psz_arg );
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
 * Dummy: used by test, creates objects and then do nothing.
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
static int Signal( vlc_object_t *p_this, char *psz_cmd, char *psz_arg )
{
    raise( atoi(psz_arg) );
    return VLC_SUCCESS;
}

