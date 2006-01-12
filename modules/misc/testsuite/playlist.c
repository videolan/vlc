/*****************************************************************************
 * playlist.c : Playlist testsuite
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors : Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/* To run these tests, run vlc with
 *  --playlist-test --quiet --no-plugins-cache -I dummy */

/* greek tree used for tests, borrowed from the SubVersion project 
 *

                                   A                       iota
          _______________________//|\
         /         ______________/ | \_____________
        mu        /                |               \
                 /                 |                \
                B                  C                 D
          _____/|\_____                _____________/|\
         /      |      \              /              | \
        /       |       \            /              /   \___
     lambda     |        F          /              /        \
                E                gamma            /          \
               / \                               /            |
              /   \                     ________/             |
          alpha   beta                 /                      H
                                      /               _______/|\______
                                     /               /        |       \
                                    G               /         |        \
                           ________/|\_______     chi        psi      omega
                          /         |        \
                         /          |         \
                        /           |          \
                       pi          rho         tau

 */
 
/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>

#include <vlc_input.h>
#include <vlc_playlist.h>

#include <stdlib.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
int    PlaylistTest    ( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );

static int Callback( vlc_object_t *, char *, vlc_value_t, vlc_value_t,void*);

static void Assert( intf_sys_t *p_sys,const char* psz_text, int condition );

static void StressTest (vlc_object_t *);

#define MAX_ITEMS 100

#define CREATE_GT 1000

struct intf_sys_t
{
    char *ppsz_names[MAX_ITEMS];
    playlist_t *p_playlist;
    int i_names;
    int i_current;
    vlc_bool_t b_error;
};

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Playlist stress tests") );
    add_bool( "playlist-test" , VLC_FALSE , PlaylistTest , "" , "" ,
  	      VLC_TRUE );
vlc_module_end();

/*****************************************************************************
 * PlaylistTest: callback function, spawns the tester thread
 *****************************************************************************/
int PlaylistTest( vlc_object_t *p_this, char const *psz_cmd,
                   vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    if( newval.b_bool == VLC_FALSE ) return VLC_SUCCESS;

    if( vlc_thread_create( p_this, "playlist tester", StressTest,
                    VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Err( p_this, "unable to spawn playlist test thread" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Tester thread 
 *****************************************************************************/
static void StressTest( vlc_object_t *p_this )
{
    playlist_t *p_playlist = NULL;
    playlist_view_t *p_view;
    int i;
    mtime_t i_start;
    playlist_item_t *p_item,*p_a,*p_b,*p_c,*p_d,*p_e,*p_f,*p_g,*p_h, *p_gamma;

    intf_sys_t *p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    p_sys->b_error = VLC_FALSE;

    fprintf( stderr, "beginning playlist test\n" );
    while( p_playlist == NULL )
    {
        msleep( INTF_IDLE_SLEEP );
        p_playlist = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                        FIND_ANYWHERE );
    }
    fprintf( stderr, "Attached to playlist\n");

    p_sys->p_playlist = p_playlist;

    /* let time for vlc initialization */
    sleep( 3 );

    var_AddCallback( p_playlist, "playlist-current", Callback, p_sys );


    /* Test 1 : Create a greek tree */
    fprintf(stderr,"1/ Creating greek tree pattern (except H)\n");

    p_view = playlist_ViewFind( p_playlist, VIEW_CATEGORY );
    i_start = mdate();

    playlist_Stop( p_playlist );
    Assert( p_sys, "Checking playlist status STOPPED ...",
            p_playlist->status.i_status == PLAYLIST_STOPPED );

    p_a = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "A", p_view->p_root );
    p_item = playlist_ItemNew(p_playlist, "mu","mu" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_a, PLAYLIST_APPEND, PLAYLIST_END );
    p_b = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "B", p_a );
    p_c = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "C", p_a );
    p_d = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "D", p_a );
    p_item = playlist_ItemNew(p_playlist, "lambda","lambda" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_b, PLAYLIST_APPEND, PLAYLIST_END );
    p_e = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "E", p_b );
    p_f = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "F", p_b );
    p_gamma = p_item = playlist_ItemNew(p_playlist, "gamma","gamma" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_d, PLAYLIST_APPEND, PLAYLIST_END );
    p_g = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "G", p_d );
    p_item = playlist_ItemNew(p_playlist, "beta","beta" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_e, PLAYLIST_INSERT, 0 );
    p_item = playlist_ItemNew(p_playlist, "alpha","alpha" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_e, PLAYLIST_INSERT, 0 );
    p_item = playlist_ItemNew(p_playlist, "pi","pi" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_g, PLAYLIST_APPEND, PLAYLIST_END );
    p_item = playlist_ItemNew(p_playlist, "tau","tau" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_g, PLAYLIST_APPEND, PLAYLIST_END );
    p_item = playlist_ItemNew(p_playlist, "rho","rho" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_g, PLAYLIST_INSERT, 1 );
    /* Create H as an item, we'll expand it later */
    p_item = playlist_ItemNew(p_playlist, "H","H" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_d, PLAYLIST_INSERT, 1 );

    fprintf( stderr, "Created in "I64Fi " us\n", mdate() - i_start );

    fprintf(stderr,"Dumping category view\n" );
    playlist_ViewDump( p_playlist, p_view );

    Assert( p_sys, "Checking playlist status STOPPED ...",
            p_playlist->status.i_status == PLAYLIST_STOPPED );

//    Assert( p_sys, "Checking 0 items in VIEW_SIMPLE ...",
  //          playlist_ViewItemsCount( p_playlist, VIEW_SIMPLE ) == 0 );
 //   Assert( p_sys, "Checking 9 items in VIEW_ALL ...",
   //         playlist_ViewItemsCount( p_playlist, VIEW_ALL ) == 9 );


    p_sys->ppsz_names[0] = strdup("mu");
    p_sys->ppsz_names[1]= strdup("lambda");
    p_sys->ppsz_names[2] = strdup("beta");
    p_sys->ppsz_names[3] = strdup("alpha");
    p_sys->ppsz_names[4] = strdup("gamma");
    p_sys->ppsz_names[5] = strdup("pi");
    p_sys->ppsz_names[6] = strdup("tau");
    p_sys->ppsz_names[7] = strdup("rho");
    p_sys->ppsz_names[8] = strdup("H");
    p_sys->i_names = 9;
    p_sys->i_current = 0;

    fprintf( stderr, "Starting playlist\n");

    playlist_Play( p_playlist );

    sleep( 1 );

    Assert( p_sys, "Checking nothing was played ...",
                    (p_sys->i_current == 0) );
    fprintf(stderr,"played : %i\n",p_sys->i_current );

    playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, VIEW_CATEGORY,
                      p_view->p_root, NULL );

    Assert( p_sys, "Checking playlist RUNNING ...",
                     p_playlist->status.i_status == 1 );

    /* Wait for everything to have played */
#if 0
     while( p_sys->i_current != 8 );// && p_sys->b_error == VLC_FALSE )
     {
         msleep( INTF_IDLE_SLEEP );
     }
     fprintf(stderr,"finished\n" );
#endif

    /* Let some more time */
    sleep( 5 );
    Assert( p_sys, "Checking playlist status STOPPED ...",
            (p_playlist->status.i_status == PLAYLIST_STOPPED) );

    p_sys->i_names = -1;

    /* Test 2 : Repeat */
    fprintf( stderr, "2/ Checking repeat\n" );
    var_SetBool( p_playlist, "repeat", VLC_TRUE );
    playlist_Goto( p_playlist, 4 );
    msleep( 100 );
    Assert( p_sys, "Checking playing of gamma ...",
            p_playlist->status.p_item == p_gamma );
    sleep( 2 );
    Assert( p_sys, "Checking still playing gamma ...", 
	    p_playlist->status.p_item == p_gamma );
    Assert( p_sys, "Checking playlist still running ...",
            p_playlist->status.i_status == PLAYLIST_RUNNING );

    /* Test 3: Play and stop */
    fprintf( stderr, "3/ Checking play and stop\n" );
    playlist_Stop( p_playlist );
    var_SetBool( p_playlist, "repeat", VLC_FALSE );
    var_SetBool( p_playlist, "play-and-stop", VLC_TRUE );
    playlist_Skip( p_playlist, 1 );
    Assert( p_sys, "Checking playlist status RUNNING ...",
            p_playlist->status.i_status == PLAYLIST_RUNNING );
    sleep( 2 );
    Assert( p_sys, "Checking playlist stopped  ...",
            p_playlist->status.i_status == PLAYLIST_STOPPED );

    /* Test 4 : Simple adding of iota */
    fprintf( stderr, "4/ Checking simple add\n" );
    p_item = playlist_ItemNew( p_playlist, "iota","iota" );
    playlist_AddItem( p_playlist, p_item, PLAYLIST_APPEND, PLAYLIST_END ); 

    /* Check items counts */
//    Assert( p_sys, "Checking 1 item in VIEW_SIMPLE ...",
  //          playlist_ViewItemsCount( p_playlist, VIEW_SIMPLE ) == 1 );
//    Assert( p_sys, "Checking 10 items in VIEW_CATEGORY ...",
  //          playlist_ViewItemsCount( p_playlist, VIEW_CATEGORY ) == 10 );

    /* Test 5:Expand H : it was added only to view_category so the children 
     * should not appear in VIEW_SIMPLE */
    fprintf( stderr, "5/ ItemToNode - Parent inheritance\n" );

    
    /* Test 6 : Add many items */
    fprintf( stderr, "6/ Adding %i items", 12*CREATE_GT );

    i_start = mdate();

    for( i = CREATE_GT ; i >= 0 ; i-- )
    {
	GreekTree( p_playlist, p_view->p_root );
    }
    
    fprintf( stderr, "Created in "I64Fi " us\n", mdate() - i_start );

    vlc_object_release( p_playlist );

    if( p_sys->b_error == VLC_FALSE )
    {
        p_this->p_vlc->b_die = VLC_TRUE;
    }
    else
    {
	exit( 1 );
    }
    return;
}

static inline void GreekTree( playlist_t *p_playlist, playlist_item_t *p_node )
{
    playlist_item_t *p_item,*p_a,*p_b,*p_c,*p_d,*p_e,*p_f,*p_g,*p_h;	
    p_a = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "A", p_node );
    p_item = playlist_ItemNew(p_playlist, "mu","mu" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_a, PLAYLIST_APPEND, PLAYLIST_END );
    p_b = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "B", p_a );
    p_c = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "C", p_a );
    p_d = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "D", p_a );
    p_item = playlist_ItemNew(p_playlist, "lambda","lambda" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_b, PLAYLIST_APPEND, PLAYLIST_END );
    p_e = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "E", p_b );
    p_f = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "F", p_b );
    p_item = playlist_ItemNew(p_playlist, "gamma","gamma" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_d, PLAYLIST_APPEND, PLAYLIST_END );
    p_g = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "G", p_d );
    p_item = playlist_ItemNew(p_playlist, "beta","beta" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_e, PLAYLIST_INSERT, 0 );
    p_item = playlist_ItemNew(p_playlist, "alpha","alpha" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_e, PLAYLIST_INSERT, 0 );
    p_item = playlist_ItemNew(p_playlist, "pi","pi" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_g, PLAYLIST_APPEND, PLAYLIST_END );
    p_item = playlist_ItemNew(p_playlist, "tau","tau" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_g, PLAYLIST_APPEND, PLAYLIST_END );
    p_item = playlist_ItemNew(p_playlist, "rho","rho" );
    playlist_NodeAddItem( p_playlist, p_item, VIEW_CATEGORY,
                          p_g, PLAYLIST_INSERT, 1 );
    p_g = playlist_NodeCreate( p_playlist, VIEW_CATEGORY, "H", p_d );
}

static void Assert( intf_sys_t *p_sys,const char* psz_text, int condition )
{
    fprintf( stderr, "%s", psz_text );
    if( condition == 0 )
    {
        fprintf( stderr, "Fail\n" );
        p_sys->b_error = VLC_TRUE;
    }
    else
    {
        fprintf(stderr,"Pass\n" );
    }
    return;
}


static int Callback( vlc_object_t *p_this, char *psz_cmd,
                vlc_value_t ov, vlc_value_t nv,void *param)
{
    intf_sys_t *p_sys = (intf_sys_t*) param;
    playlist_t *p_playlist = (playlist_t*)p_this;
    char *psz_name;
    playlist_item_t *p_item;
    
    if( p_sys->i_names == -1 ) return;
    
    p_item= playlist_ItemGetById( p_sys->p_playlist,nv.i_int );
    psz_name = strdup (p_item->input.psz_name );

    if( p_sys->i_current >= p_sys->i_names )
    {
        fprintf( stderr,"Error, we read too many items\n" );
        p_sys->b_error = VLC_TRUE;
	return;
    }

    if( !strcmp( p_sys->ppsz_names[p_sys->i_current], psz_name ) )
    {
        p_sys->i_current++;
        fprintf(stderr,"playing %s\n",p_sys->ppsz_names[p_sys->i_current-1]);
    }
    else
    {
        fprintf( stderr, "Error, we read %s, %s expected",
                 psz_name ,  p_sys->ppsz_names[p_sys->i_current] );
        p_sys->b_error = VLC_TRUE;
    }

    return VLC_SUCCESS;
}
