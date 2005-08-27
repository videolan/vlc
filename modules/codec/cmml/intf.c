/*****************************************************************************
 * intf.c: interface for CMML annotations/hyperlinks
 *****************************************************************************
 * Copyright (C) 2003-2004 Commonwealth Scientific and Industrial Research
 *                         Organisation (CSIRO) Australia
 * Copyright (C) 2004 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Andre Pang <Andre.Pang@csiro.au>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#include <vlc_osd.h>

#include "vlc_keys.h"

#include "browser_open.h"
#include "history.h"
#include "xstrcat.h"
#include "xurl.h"

#undef  CMML_INTF_USE_TIMED_URIS

#undef  CMML_INTF_DEBUG
#undef  CMML_INTF_HISTORY_DEBUG

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    decoder_t *         p_cmml_decoder;
    input_thread_t *    p_input;

    vlc_bool_t          b_key_pressed;
};

struct navigation_history_t
{
    int i_history_size;
    int i_last_item;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int   InitThread                 ( intf_thread_t * );
static int   MouseEvent                 ( vlc_object_t *, char const *,
                                          vlc_value_t, vlc_value_t, void * );
static int   KeyEvent                   ( vlc_object_t *, char const *,
                                          vlc_value_t, vlc_value_t, void * );

static void  FollowAnchor               ( intf_thread_t * );
static void  GoBack                     ( intf_thread_t * );
static void  GoForward                  ( intf_thread_t * );

static int   FollowAnchorCallback       ( vlc_object_t *, char const *,
                                          vlc_value_t, vlc_value_t, void * );
static int   GoBackCallback             ( vlc_object_t *, char const *,
                                          vlc_value_t, vlc_value_t, void * );
static int   GoForwardCallback          ( vlc_object_t *, char const *,
                                          vlc_value_t, vlc_value_t, void * );

static char *GetTimedURLFromPlaylistItem( intf_thread_t *, playlist_item_t * );
static char *GetTimedURIFragmentForTime ( int );
static int   GetCurrentTimeInSeconds    ( input_thread_t * );
static int   DisplayAnchor              ( intf_thread_t *, vout_thread_t *,
                                          char *, char * );
static int   DisplayPendingAnchor       ( intf_thread_t *, vout_thread_t * );
static history_t * GetHistory           ( playlist_t * );
static void  ReplacePlaylistItem        ( playlist_t *, char * );

/* Exported functions */
static void RunIntf        ( intf_thread_t *p_intf );

/*****************************************************************************
 * OpenIntf: initialize CMML interface
 *****************************************************************************/
int E_(OpenIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };

    p_intf->pf_run = RunIntf;

    var_AddCallback( p_intf->p_vlc, "key-pressed", KeyEvent, p_intf );
    /* we also need to add the callback for "mouse-clicked", but do that later
     * when we've found a p_vout */

    var_Create( p_intf->p_vlc, "browse-go-back", VLC_VAR_VOID );
    var_AddCallback( p_intf->p_vlc, "browse-go-back",
                     GoBackCallback, p_intf );
    var_Create( p_intf->p_vlc, "browse-go-forward", VLC_VAR_VOID );
    var_AddCallback( p_intf->p_vlc, "browse-go-forward",
                     GoForwardCallback, p_intf );
    var_Create( p_intf->p_vlc, "browse-follow-anchor", VLC_VAR_VOID );
    var_AddCallback( p_intf->p_vlc, "browse-follow-anchor",
                     FollowAnchorCallback, p_intf );

    return( 0 );
}

/*****************************************************************************
 * CloseIntf: destroy dummy interface
 *****************************************************************************/
void E_(CloseIntf) ( vlc_object_t *p_this )
{
    intf_thread_t * p_intf = (intf_thread_t *)p_this;
    vout_thread_t * p_vout;

#ifdef CMML_INTF_DEBUG
    msg_Dbg( p_intf, "freeing CMML interface" );
#endif

    /* erase the anchor text description from the video output if it exists */
    p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout )
    {
        /* enable CMML as a subtitle track */
        spu_Control( p_vout->p_spu, SPU_CHANNEL_CLEAR, DEFAULT_CHAN );
        vlc_object_release( p_vout );
    }

    var_DelCallback( p_intf->p_vlc, "key-pressed", KeyEvent, p_intf );

    vlc_object_release( p_intf->p_sys->p_cmml_decoder );

    free( p_intf->p_sys );
}


/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
static void RunIntf( intf_thread_t *p_intf )
{
    vout_thread_t * p_vout = NULL;

    if( InitThread( p_intf ) < 0 )
    {
        msg_Err( p_intf, "can't initialize CMML interface" );
        return;
    }
#ifdef CMML_INTF_DEBUG
    msg_Dbg( p_intf, "CMML intf initialized" );
#endif

    /* if video output is dying, disassociate ourselves from it */
    if( p_vout && p_vout->b_die )
    {
        var_DelCallback( p_vout, "mouse-clicked", MouseEvent, p_intf );
        vlc_object_release( p_vout );
        p_vout = NULL;
    }

    /* Main loop */
    while( !p_intf->b_die )
    {
        
        /* find a video output if we currently don't have one */
        if( p_vout == NULL )
        {
            p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                      VLC_OBJECT_VOUT, FIND_CHILD );
            if( p_vout )
            {
#ifdef CMML_INTF_DEBUG
                msg_Dbg( p_intf, "found vout thread" );
#endif
                var_AddCallback( p_vout, "mouse-clicked", MouseEvent, p_intf );
            }
        }

        vlc_mutex_lock( &p_intf->change_lock );

        /*
         * keyboard event
         */
        if( p_intf->p_sys->b_key_pressed )
        {
            vlc_value_t val;
            int i, i_action = -1;
            struct hotkey *p_hotkeys = p_intf->p_vlc->p_hotkeys;

            /* Find action triggered by hotkey (if any) */
            var_Get( p_intf->p_vlc, "key-pressed", &val );

            /* Acknowledge that we've handled the b_key_pressed event */
            p_intf->p_sys->b_key_pressed = VLC_FALSE;

#ifdef CMML_INTF_DEBUG
            msg_Dbg( p_intf, "Got a keypress: %d", val.i_int );
#endif

            for( i = 0; p_hotkeys[i].psz_action != NULL; i++ )
            {
                if( p_hotkeys[i].i_key == val.i_int )
                    i_action = p_hotkeys[i].i_action;
            }

            /* What did the user do? */
            if( i_action != -1 )
            {
                switch( i_action )
                {
                    case ACTIONID_NAV_ACTIVATE:
                        FollowAnchor( p_intf );
                        break;
                    case ACTIONID_HISTORY_BACK:
                        GoBack( p_intf );
                        break;
                    case ACTIONID_HISTORY_FORWARD:
                        GoForward( p_intf );
                        break;
                    default:
                        break;
                }
            }
        }

        vlc_mutex_unlock( &p_intf->change_lock );

        (void) DisplayPendingAnchor( p_intf, p_vout );

        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }

    /* if we're here, the video output is dying: release the vout object */

    if( p_vout )
    {
        var_DelCallback( p_vout, "mouse-clicked", MouseEvent, p_intf );
        vlc_object_release( p_vout );
    }

    vlc_object_release( p_intf->p_sys->p_input );
}

/*****************************************************************************
 * DisplayPendingAnchor: get a pending anchor description/URL from the CMML
 * decoder and display it on screen
 *****************************************************************************/
static int DisplayPendingAnchor( intf_thread_t *p_intf, vout_thread_t *p_vout )
{
    decoder_t *p_cmml_decoder;
    char *psz_description = NULL;
    char *psz_url = NULL;

    intf_thread_t *p_primary_intf;
    vlc_value_t val;

    p_cmml_decoder = p_intf->p_sys->p_cmml_decoder;
    if( var_Get( p_cmml_decoder, "psz-current-anchor-description", &val )
            != VLC_SUCCESS )
    {
        return VLC_TRUE;
    }

    if( !val.p_address )
        return VLC_TRUE;

    psz_description = val.p_address;

    if( var_Get( p_cmml_decoder, "psz-current-anchor-url", &val )
            == VLC_SUCCESS )
    {
        psz_url = val.p_address;
    }

    if( p_vout != NULL )
    {
        /* don't display anchor if main interface can display it */
        p_primary_intf = vlc_object_find( p_intf->p_vlc, VLC_OBJECT_INTF,
                FIND_CHILD );

        if( p_primary_intf )
        {
            if( var_Get( p_primary_intf, "intf-displays-cmml-description", &val )
                    == VLC_SUCCESS )
            {
                if( val.b_bool == VLC_TRUE )
                {
                    vlc_object_release( p_primary_intf );
                    return VLC_TRUE;
                }
            }

            vlc_object_release( p_primary_intf );
        }

        /* display anchor as subtitle on-screen */
        if( DisplayAnchor( p_intf, p_vout, psz_description, psz_url )
                != VLC_SUCCESS )
        {
            /* text render unsuccessful: do nothing */
            return VLC_FALSE;
        }

        /* text render successful: clear description */
        val.p_address = NULL;
        if( var_Set( p_cmml_decoder, "psz-current-anchor-description", val )
                != VLC_SUCCESS )
        {
            msg_Dbg( p_intf,
                     "reset of psz-current-anchor-description failed" );
        }
        free( psz_description );
        psz_url = NULL;
    }

    return VLC_TRUE;
}


/*****************************************************************************
 * InitThread:
 *****************************************************************************/
static int InitThread( intf_thread_t * p_intf )
{
    /* We might need some locking here */
    if( !p_intf->b_die )
    {
        input_thread_t * p_input;
        decoder_t *p_cmml_decoder;

        p_cmml_decoder = vlc_object_find( p_intf, VLC_OBJECT_DECODER, FIND_PARENT );
        p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_PARENT );

#ifdef CMML_INTF_DEBUG
        msg_Dbg( p_intf, "cmml decoder at %p, input thread at %p",
                 p_cmml_decoder, p_input );
#endif

        /* Maybe the input just died */
        if( p_input == NULL )
        {
            return VLC_EGENERIC;
        }

        vlc_mutex_lock( &p_intf->change_lock );

        p_intf->p_sys->p_input = p_input;
        p_intf->p_sys->p_cmml_decoder = p_cmml_decoder;

        p_intf->p_sys->b_key_pressed = VLC_FALSE;

        vlc_mutex_unlock( &p_intf->change_lock );

        return VLC_SUCCESS;
    }
    else
    {
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    /* TODO: handle mouse clicks on the anchor text */

    return VLC_SUCCESS;
}

/*****************************************************************************
 * KeyEvent: callback for keyboard events
 *****************************************************************************/
static int KeyEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    vlc_mutex_lock( &p_intf->change_lock );

    p_intf->p_sys->b_key_pressed = VLC_TRUE;
    
    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * FollowAnchor: follow the current anchor being displayed to the user
 *****************************************************************************/
static void FollowAnchor ( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys;
    decoder_t *p_cmml_decoder;
    char *psz_url = NULL;
    vlc_value_t val;

    msg_Dbg( p_intf, "User followed anchor" );

    p_sys = p_intf->p_sys;
    p_cmml_decoder = p_sys->p_cmml_decoder;

    if( var_Get( p_cmml_decoder, "psz-current-anchor-url", &val ) ==
            VLC_SUCCESS )
    {
        if( val.p_address ) psz_url = val.p_address;
    }

#ifdef CMML_INTF_DEBUG
    msg_Dbg( p_intf, "Current URL is \"%s\"", psz_url );
#endif

    if( psz_url )
    {
        playlist_t *p_playlist;
        playlist_item_t *p_current_item;
        char *psz_uri_to_load;
        mtime_t i_seconds;
        vlc_value_t time;

        p_playlist = (playlist_t *) vlc_object_find( p_intf, 
                VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        if ( !p_playlist )
        {
            msg_Warn( p_intf, "can't find playlist" );
            return;
        }

        /* Get new URL */
        p_current_item = p_playlist->pp_items[p_playlist->i_index];
#ifdef CMML_INTF_DEBUG
        msg_Dbg( p_intf, "Current playlist item URL is \"%s\"",
                p_current_item->input.psz_uri );
#endif

        psz_uri_to_load = XURL_Concat( p_current_item->input.psz_uri,
                                       psz_url );

#ifdef CMML_INTF_DEBUG
        msg_Dbg( p_intf, "URL to load is \"%s\"", psz_uri_to_load );
#endif

        if( var_Get( p_intf->p_sys->p_input, "time", &time ) )
        {
            msg_Dbg( p_intf, "couldn't get time from current clip" );
            time.i_time = 0;
        }
        i_seconds = time.i_time / 1000000;
#ifdef CMML_INTF_DEBUG
        msg_Dbg( p_intf, "Current time is \"%lld\"", i_seconds );
#endif

        /* TODO: we need a (much) more robust way of detecting whether
         * the file's a media file ... */
        if( strstr( psz_uri_to_load, ".anx" ) != NULL )
        {
            history_t *p_history = NULL;
            history_item_t *p_history_item = NULL;
            char *psz_timed_url;

            p_history = GetHistory( p_playlist );

            /* create history item */
            psz_timed_url = GetTimedURLFromPlaylistItem( p_intf, p_current_item );
            p_history_item = historyItem_New( psz_timed_url, psz_timed_url );
            free( psz_timed_url );

            if( !p_history_item )
            {
                msg_Warn( p_intf, "could not initialise history item" );
            }
            else
            {
#ifdef CMML_INTF_DEBUG
                msg_Dbg( p_intf, "history pre-index %d", p_history->i_index );
#endif
                history_PruneAndInsert( p_history, p_history_item );
#ifdef CMML_INTF_DEBUG
                msg_Dbg( p_intf, "new history item at %p, uri is \"%s\"",
                         p_history_item, p_history_item->psz_uri );
                msg_Dbg( p_intf, "history index now %d", p_history->i_index );
#endif
            }

            /* free current-anchor-url */
            free( psz_url );
            val.p_address = NULL;
            if( var_Set( p_cmml_decoder, "psz-current-anchor-url", val ) !=
                    VLC_SUCCESS )
            {
                msg_Dbg( p_intf, "couldn't reset psz-current-anchor-url" );
            }

            ReplacePlaylistItem( p_playlist, psz_uri_to_load );
        }
        else
        {
#ifdef CMML_INTF_DEBUG
            msg_Dbg( p_intf, "calling browser_Open with \"%s\"", psz_url );
#endif
            (void) browser_Open( psz_url );
            playlist_Control( p_playlist, PLAYLIST_PAUSE, 0 );
        }

        free( psz_uri_to_load );

        vlc_object_release( p_playlist );
    }
}

static
char *GetTimedURLFromPlaylistItem( intf_thread_t *p_intf,
        playlist_item_t *p_current_item )
{
#ifdef CMML_INTF_USE_TIMED_URIS
    char *psz_url = NULL;
    char *psz_return_value = NULL;
    char *psz_seconds = NULL;
    int i_seconds;
    
    psz_url = XURL_GetWithoutFragment( p_current_item->input->psz_uri );

    /* Get current time as a string */
    if( XURL_IsFileURL( psz_url ) == VLC_TRUE )
        psz_url = xstrcat( psz_url, "#" );
    else
        psz_url = xstrcat( psz_url, "?" );

    /* jump back to 2 seconds before where we are now */
    i_seconds = GetCurrentTimeInSeconds( p_intf->p_sys->p_input ) - 2;
    psz_seconds = GetTimedURIFragmentForTime( i_seconds < 0 ? 0 : i_seconds );
    if( psz_seconds )
    {
        psz_url = xstrcat( psz_url, psz_seconds );
        free( psz_seconds );
        psz_return_value = psz_url;
    }

    return psz_return_value;
#else
    void *p;

    /* Suppress warning messages about unused functions */
    p = GetTimedURIFragmentForTime; /* unused */
    p = GetCurrentTimeInSeconds;    /* unused */

    return strdup( p_current_item->input.psz_uri );
#endif
}


/*
 * Get the current time, rounded down to the nearest second
 *
 * http://www.ietf.org/internet-drafts/draft-pfeiffer-temporal-fragments-02.txt
 */
static
int GetCurrentTimeInSeconds( input_thread_t *p_input )
{
    vlc_value_t time;
    mtime_t i_seconds;

    var_Get( p_input, "time", &time );
    i_seconds = time.i_time / 1000000;

    return i_seconds;
}

static
char *GetTimedURIFragmentForTime( int seconds )
{
    char *psz_time;

    asprintf( &psz_time, "%d", seconds );

    return psz_time;
}

static
int GoBackCallback( vlc_object_t *p_this, char const *psz_var,
                    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *) p_data;
    GoBack( p_intf );
    return VLC_SUCCESS;
}

static
int GoForwardCallback( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *) p_data;
    GoForward( p_intf );
    return VLC_SUCCESS;
}

static
int FollowAnchorCallback( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *) p_data;
    FollowAnchor( p_intf );
    return VLC_SUCCESS;
}

static
void GoBack( intf_thread_t *p_intf )
{
    vlc_value_t history;
    history_t *p_history = NULL;
    history_item_t *p_history_item = NULL;
    history_item_t *p_new_history_item = NULL;
    playlist_t *p_playlist = NULL;
    char *psz_timed_url = NULL;
    playlist_item_t *p_current_item;

#ifdef CMML_INTF_DEBUG
    msg_Dbg( p_intf, "Going back in navigation history" );
#endif

    /* Find the playlist */
    p_playlist = (playlist_t *) vlc_object_find( p_intf, 
            VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if ( !p_playlist )
    {
        msg_Warn( p_intf, "can't find playlist" );
        return;
    }

    /* Retrieve navigation history from playlist */
    if( var_Get( p_playlist, "navigation-history", &history ) != VLC_SUCCESS ||
        !history.p_address )
    {
        /* History doesn't exist yet: ignore user's request */
        msg_Warn( p_intf, "can't go back: no history exists yet" );
        vlc_object_release( p_playlist );
        return;
    }

    p_history = history.p_address;
#ifdef CMML_INTF_DEBUG
    msg_Dbg( p_intf, "back: nav history retrieved from %p", p_history );
    msg_Dbg( p_intf, "nav history index:%d, p_xarray:%p", p_history->i_index,
             p_history->p_xarray );
#endif

    /* Check whether we can go back in the history */
    if( history_CanGoBack( p_history ) == VLC_FALSE )
    {
        msg_Warn( p_intf, "can't go back: already at beginning of history" );
        vlc_object_release( p_playlist );
        return;
    }

    p_current_item = p_playlist->pp_items[p_playlist->i_index];

    /* Save the currently-playing media in a new history item */
    psz_timed_url = GetTimedURLFromPlaylistItem( p_intf, p_current_item );
    p_new_history_item = historyItem_New( psz_timed_url, psz_timed_url );
    free( psz_timed_url );

    if( !p_new_history_item )
    {
#ifdef CMML_INTF_DEBUG
        msg_Dbg( p_intf, "back: could not initialise new history item" );
#endif
        vlc_object_release( p_playlist );
        return;
    }

    /* Go back in the history, saving the currently-playing item */
    (void) history_GoBackSavingCurrentItem( p_history, p_new_history_item );
    p_history_item = history_Item( p_history );

#ifdef CMML_INTF_DEBUG
    msg_Dbg( p_intf, "retrieving item from h index %d", p_history->i_index );
    msg_Dbg( p_intf, "got previous history item: %p", p_history_item );
    msg_Dbg( p_intf, "prev history item URL: \"%s\"", p_history_item->psz_uri );
#endif

    ReplacePlaylistItem( p_playlist, p_history_item->psz_uri );
    vlc_object_release( p_playlist );
}

static
void GoForward( intf_thread_t *p_intf )
{
    vlc_value_t history;
    history_t *p_history = NULL;
    history_item_t *p_history_item = NULL;
    history_item_t *p_new_history_item = NULL;
    playlist_t *p_playlist = NULL;
    playlist_item_t *p_current_item;

#ifdef CMML_INTF_DEBUG
    msg_Dbg( p_intf, "Going forward in navigation history" );
#endif

    /* Find the playlist */
    p_playlist = (playlist_t *) vlc_object_find( p_intf, 
            VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if ( !p_playlist )
    {
        msg_Warn( p_intf, "can't find playlist" );
        return;
    }

    /* Retrieve navigation history from playlist */
    if( var_Get( p_playlist, "navigation-history", &history ) != VLC_SUCCESS ||
        !history.p_address )
    {
        /* History doesn't exist yet: ignore user's request */
        msg_Warn( p_intf, "can't go back: no history exists yet" );
        vlc_object_release( p_playlist );
        return;
    }

    p_history = history.p_address;
#ifdef CMML_INTF_DEBUG
    msg_Dbg( p_intf, "forward: nav history retrieved from %p", p_history );
    msg_Dbg( p_intf, "nav history index:%d, p_xarray:%p", p_history->i_index,
             p_history->p_xarray );
#endif

    /* Check whether we can go forward in the history */
    if( history_CanGoForward( p_history ) == VLC_FALSE )
    {
        msg_Warn( p_intf, "can't go forward: already at end of history" );
        vlc_object_release( p_playlist );
        return;
    }

    /* Save the currently-playing media in a new history item */
    p_new_history_item = malloc( sizeof(history_item_t) );
    if( !p_new_history_item )
    {
#ifdef CMML_INTF_DEBUG
        msg_Dbg( p_intf, "forward: could not initialise new history item" );
#endif
        vlc_object_release( p_playlist );
        return;
    }
    p_current_item = p_playlist->pp_items[p_playlist->i_index];
    p_new_history_item->psz_uri = GetTimedURLFromPlaylistItem( p_intf, 
            p_current_item );
    p_new_history_item->psz_name = p_new_history_item->psz_uri;

    /* Go forward in the history, saving the currently-playing item */
    (void) history_GoForwardSavingCurrentItem( p_history, p_new_history_item );
    p_history_item = history_Item( p_history );

#ifdef CMML_INTF_DEBUG
    msg_Dbg( p_intf, "retrieving item from h index %d", p_history->i_index );
    msg_Dbg( p_intf, "got next history item: %p", p_history_item );
    msg_Dbg( p_intf, "next history item URL: \"%s\"", p_history_item->psz_uri );
#endif

    ReplacePlaylistItem( p_playlist, p_history_item->psz_uri );
    vlc_object_release( p_playlist );
}

static void ReplacePlaylistItem( playlist_t *p_playlist, char *psz_uri )
{
    playlist_Stop( p_playlist );
    (void) playlist_Add( p_playlist, psz_uri, psz_uri,
                         PLAYLIST_REPLACE, p_playlist->i_index );
    playlist_Goto( p_playlist, p_playlist->i_index );
}

/****************************************************************************
 * DisplayAnchor: displays an anchor on the given video output
 ****************************************************************************/
static int DisplayAnchor( intf_thread_t *p_intf,
        vout_thread_t *p_vout,
        char *psz_anchor_description,
        char *psz_anchor_url )
{
    int i_margin_h, i_margin_v;
    mtime_t i_now;

    i_margin_h = 0;
    i_margin_v = 10;

    i_now = mdate();

    if( p_vout )
    {
        text_style_t *p_style = NULL;

        text_style_t blue_with_underline = default_text_style;
        blue_with_underline.b_underline = VLC_TRUE;
        blue_with_underline.i_color = 0x22ff22;

        if( psz_anchor_url )
        {
            /* Should display subtitle underlined and in blue, but it looks
             * like VLC doesn't implement any text styles yet.  D'oh! */
            p_style = &blue_with_underline;

        }

        /* TODO: p_subpicture doesn't have the proper i_x and i_y
         * coordinates.  Need to look at the subpicture display system to
         * work out why. */
        if ( vout_ShowTextAbsolute( p_vout, DEFAULT_CHAN,
                psz_anchor_description, p_style, OSD_ALIGN_BOTTOM,
                i_margin_h, i_margin_v, i_now, 0 ) == VLC_SUCCESS )
        {
            /* Displayed successfully */
        }
        else
        {
            return VLC_EGENERIC;
        }

    }
    else
    {
        msg_Dbg( p_intf, "DisplayAnchor couldn't find a video output" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static history_t * GetHistory( playlist_t *p_playlist )
{
    vlc_value_t val;
    history_t *p_history = NULL;

    if( var_Get( p_playlist, "navigation-history", &val ) != VLC_SUCCESS )
    {
        /* history doesn't exist yet: need to create it */
        history_t *new_history = history_New();
        val.p_address = new_history;
        var_Create( p_playlist, "navigation-history",
                VLC_VAR_ADDRESS|VLC_VAR_DOINHERIT );
        if( var_Set( p_playlist, "navigation-history", val ) != VLC_SUCCESS )
        {
            msg_Warn( p_playlist, "could not initialise history" );
        }
        else
        {
            p_history = new_history;
#ifdef CMML_INTF_HISTORY_DEBUG
            msg_Dbg( p_playlist, "nav history created at %p", new_history );
            msg_Dbg( p_playlist, "nav history index:%d, p_xarray:%p",
                     p_history->i_index, p_history->p_xarray );
#endif
        }
    }
    else
    {
        p_history = val.p_address;
#ifdef CMML_INTF_HISTORY_DEBUG
        msg_Dbg( p_playlist, "nav history retrieved from %p", p_history );
#endif
    }

    return p_history;
}

