/*****************************************************************************
 * ncurses.c : NCurses plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Yoann Peronneau <yoann@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>
#include <time.h>

#include <curses.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#if (!defined( WIN32 ) || defined(__MINGW32__))
/* Mingw has its own version of dirent */
#   include <dirent.h>
#endif

#ifdef HAVE_CDDAX
#define CDDA_MRL "cddax://"
#else
#define CDDA_MRL "cdda://"
#endif

#ifdef HAVE_VCDX
#define VCD_MRL "vcdx://"
#else
#define VCD_MRL "vcdx://"
#endif

#define SEARCH_CHAIN_SIZE 20
#define OPEN_CHAIN_SIZE 50

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open           ( vlc_object_t * );
static void Close          ( vlc_object_t * );

static void Run            ( intf_thread_t * );
static void PlayPause      ( intf_thread_t * );
static void Eject          ( intf_thread_t * );

static int  HandleKey      ( intf_thread_t *, int );
static void Redraw         ( intf_thread_t *, time_t * );
static void PlaylistRebuild( intf_thread_t * );
static void PlaylistAddNode( intf_thread_t *, playlist_item_t *, int, char *);
static void PlaylistDestroy( intf_thread_t * );
static int  PlaylistChanged( vlc_object_t *, const char *, vlc_value_t,
                             vlc_value_t, void * );
static void FindIndex      ( intf_thread_t * );
static void SearchPlaylist ( intf_thread_t *, char * );
static int  SubSearchPlaylist( intf_thread_t *, char *, int, int );
static void ManageSlider   ( intf_thread_t * );
static void ReadDir        ( intf_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define BROWSE_TEXT N_("Filebrowser starting point")
#define BROWSE_LONGTEXT N_( \
    "This option allows you to specify the directory the ncurses filebrowser " \
    "will show you initially.")

vlc_module_begin();
    set_shortname( "Ncurses" );
    set_description( _("Ncurses interface") );
    set_capability( "interface", 10 );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_GENERAL );
    set_callbacks( Open, Close );
    add_shortcut( "curses" );
    add_directory( "browse-dir", NULL, NULL, BROWSE_TEXT, BROWSE_LONGTEXT, VLC_FALSE );
vlc_module_end();

/*****************************************************************************
 * intf_sys_t: description and status of ncurses interface
 *****************************************************************************/
enum
{
    BOX_NONE,
    BOX_HELP,
    BOX_INFO,
    BOX_LOG,
    BOX_PLAYLIST,
    BOX_SEARCH,
    BOX_OPEN,
    BOX_BROWSE
};
struct dir_entry_t
{
    vlc_bool_t  b_file;
    char        *psz_path;
};
struct pl_item_t
{
    playlist_item_t *p_item;
    char            *psz_display;
};
struct intf_sys_t
{
    playlist_t     *p_playlist;
    input_thread_t *p_input;

    float           f_slider;
    float           f_slider_old;

    WINDOW          *w;

    int             i_box_type;
    int             i_box_y;
    int             i_box_lines;
    int             i_box_lines_total;
    int             i_box_start;

    int             i_box_plidx;    /* Playlist index */
    int             b_box_plidx_follow;
    playlist_item_t *p_plnode;      /* Playlist node */
    int             i_box_bidx;     /* browser index */

    int             b_box_cleared;

    msg_subscription_t* p_sub;                  /* message bank subscription */

    char            *psz_search_chain;          /* for playlist searching    */
    char            *psz_old_search;            /* for searching next        */
    int             i_before_search;

    char            *psz_open_chain;

    char            *psz_current_dir;
    int             i_dir_entries;
    struct dir_entry_t  **pp_dir_entries;

    int             i_current_view;             /* playlist view             */
    struct pl_item_t    **pp_plist;
    int             i_plist_entries;
    vlc_bool_t      b_need_update;              /* for playlist view */
};

static void DrawBox( WINDOW *win, int y, int x, int h, int w, char *title );
static void DrawLine( WINDOW *win, int y, int x, int w );
static void DrawEmptyLine( WINDOW *win, int y, int x, int w );

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t    *p_sys;
    vlc_value_t    val;

    /* Allocate instance and initialize some members */
    p_sys = p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    p_sys->p_playlist = NULL;
    p_sys->p_input = NULL;
    p_sys->f_slider = 0.0;
    p_sys->f_slider_old = 0.0;
    p_sys->i_box_type = BOX_PLAYLIST;
    p_sys->i_box_lines = 0;
    p_sys->i_box_start= 0;
    p_sys->i_box_lines_total = 0;
    p_sys->b_box_plidx_follow = VLC_TRUE;
    p_sys->b_box_cleared = VLC_FALSE;
    p_sys->i_box_plidx = 0;
    p_sys->p_plnode = NULL;
    p_sys->i_box_bidx = 0;
    p_sys->p_sub = msg_Subscribe( p_intf );

    /* Initialize the curses library */
    p_sys->w = initscr();
    keypad( p_sys->w, TRUE );
    /* Don't do NL -> CR/NL */
    nonl();
    /* Take input chars one at a time */
    cbreak();
    /* Don't echo */
    noecho();

    curs_set(0);
    timeout(0);

    clear();

    /* exported function */
    p_intf->pf_run = Run;

    /* Set quiet mode */
    val.i_int = -1;
    var_Set( p_intf->p_vlc, "verbose", val );

    /* Set defaul playlist view */
    p_sys->i_current_view = VIEW_CATEGORY;
    p_sys->pp_plist = NULL;
    p_sys->i_plist_entries = 0;
    p_sys->b_need_update = VLC_FALSE;

    /* Initialize search chain */
    p_sys->psz_search_chain = (char *)malloc( SEARCH_CHAIN_SIZE + 1 );
    p_sys->psz_old_search = NULL;
    p_sys->i_before_search = 0;

    /* Initialize open chain */
    p_sys->psz_open_chain = (char *)malloc( OPEN_CHAIN_SIZE + 1 );

    /* Initialize browser options */
    var_Create( p_intf, "browse-dir", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_intf, "browse-dir", &val);

    if( val.psz_string && *val.psz_string )
    {
        p_sys->psz_current_dir = strdup( val.psz_string);
        free( val.psz_string );
    }
    else
    {
        p_sys->psz_current_dir = strdup( p_intf->p_vlc->psz_homedir );
    }

    p_sys->i_dir_entries = 0;
    p_sys->pp_dir_entries = NULL;
    ReadDir( p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t    *p_sys = p_intf->p_sys;
    int i;

    var_DelCallback( p_sys->p_playlist, "intf-change", PlaylistChanged,
                     p_intf );
    var_DelCallback( p_sys->p_playlist, "item-append", PlaylistChanged,
                     p_intf );

    PlaylistDestroy( p_intf );

    for( i = 0; i < p_sys->i_dir_entries; i++ )
    {
        struct dir_entry_t *p_dir_entry = p_sys->pp_dir_entries[i];
        if( p_dir_entry->psz_path ) free( p_dir_entry->psz_path );
        REMOVE_ELEM( p_sys->pp_dir_entries, p_sys->i_dir_entries, i );
        if( p_dir_entry ) free( p_dir_entry );
    }
    p_sys->pp_dir_entries = NULL;

    if( p_sys->psz_current_dir ) free( p_sys->psz_current_dir );
    if( p_sys->psz_search_chain ) free( p_sys->psz_search_chain );
    if( p_sys->psz_old_search ) free( p_sys->psz_old_search );
    if( p_sys->psz_open_chain ) free( p_sys->psz_open_chain );

    if( p_sys->p_input )
    {
        vlc_object_release( p_sys->p_input );
    }
    if( p_sys->p_playlist )
    {
        vlc_object_release( p_sys->p_playlist );
    }

    /* Close the ncurses interface */
    endwin();

    msg_Unsubscribe( p_intf, p_sys->p_sub );

    /* Destroy structure */
    free( p_sys );
}

/*****************************************************************************
 * Run: ncurses thread
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    intf_sys_t    *p_sys = p_intf->p_sys;

    int i_key;
    time_t t_last_refresh;

    /*
     * force drawing the interface for the first time
     */
    t_last_refresh = ( time( 0 ) - 1);

    while( !p_intf->b_die )
    {
        msleep( INTF_IDLE_SLEEP );

        /* Update the input */
        if( p_sys->p_playlist == NULL )
        {
            p_sys->p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
            if( p_sys->p_playlist )
            {
                var_AddCallback( p_sys->p_playlist, "intf-change",
                                 PlaylistChanged, p_intf );
                var_AddCallback( p_sys->p_playlist, "item-append",
                                 PlaylistChanged, p_intf );
            }
        }
        if( p_sys->p_playlist )
        {
            vlc_mutex_lock( &p_sys->p_playlist->object_lock );
            if( p_sys->p_input == NULL )
            {
                p_sys->p_input = p_sys->p_playlist->p_input;
                if( p_sys->p_input )
                {
                    if( !p_sys->p_input->b_dead )
                    {
                        vlc_object_yield( p_sys->p_input );
                    }
                }
            }
            else if( p_sys->p_input->b_dead )
            {
                vlc_object_release( p_sys->p_input );
                p_sys->p_input = NULL;
                p_sys->f_slider = p_sys->f_slider_old = 0.0;
                p_sys->b_box_cleared = VLC_FALSE;
            }
            vlc_mutex_unlock( &p_sys->p_playlist->object_lock );
        }

        if( p_sys->b_box_plidx_follow && p_sys->p_playlist->i_index >= 0 )
        {
            FindIndex( p_intf );
        }

        while( ( i_key = getch()) != -1 )
        {
            /*
             * HandleKey returns 1 if the screen needs to be redrawn
             */
            if( HandleKey( p_intf, i_key ) )
            {
                Redraw( p_intf, &t_last_refresh );
            }
        }
        /* Hack */
        if( p_sys->f_slider > 0.0001 && !p_sys->b_box_cleared )
        {
            clear();
            Redraw( p_intf, &t_last_refresh );
            p_sys->b_box_cleared = VLC_TRUE;
        }

        /*
         * redraw the screen every second
         */
        if( (time(0) - t_last_refresh) >= 1 )
        {
            ManageSlider( p_intf );
            Redraw( p_intf, &t_last_refresh );
        }
    }
}

/* following functions are local */

static int HandleKey( intf_thread_t *p_intf, int i_key )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    vlc_value_t val;

    if( p_sys->i_box_type == BOX_PLAYLIST && p_sys->p_playlist )
    {
        int b_ret = VLC_TRUE;

        switch( i_key )
        {
            vlc_value_t val;
            /* Playlist Settings */
            case 'r':
                var_Get( p_sys->p_playlist, "random", &val );
                val.b_bool = !val.b_bool;
                var_Set( p_sys->p_playlist, "random", val );
                return 1;
            case 'l':
                var_Get( p_sys->p_playlist, "loop", &val );
                val.b_bool = !val.b_bool;
                var_Set( p_sys->p_playlist, "loop", val );
                return 1;
            case 'R':
                var_Get( p_sys->p_playlist, "repeat", &val );
                val.b_bool = !val.b_bool;
                var_Set( p_sys->p_playlist, "repeat", val );
                return 1;

            /* Playlist sort */
            case 'o':
                playlist_Sort( p_sys->p_playlist, SORT_TITLE, ORDER_NORMAL );
                return 1;
            case 'O':
                playlist_Sort( p_sys->p_playlist, SORT_TITLE, ORDER_REVERSE );
                return 1;

            /* Playlist view */
            case 'v':
                switch( p_sys->i_current_view )
                {
                    case VIEW_CATEGORY:
                        p_sys->i_current_view = VIEW_ALL;
                        break;
                    default:
                        p_sys->i_current_view = VIEW_CATEGORY;
                }
                PlaylistRebuild( p_intf );
                FindIndex( p_intf );
                return 1;

            /* Playlist navigation */
            case KEY_HOME:
                p_sys->i_box_plidx = 0;
                break;
            case KEY_END:
                p_sys->i_box_plidx = p_sys->p_playlist->i_size - 1;
                break;
            case KEY_UP:
                p_sys->i_box_plidx--;
                break;
            case KEY_DOWN:
                p_sys->i_box_plidx++;
                break;
            case KEY_PPAGE:
                p_sys->i_box_plidx -= p_sys->i_box_lines;
                break;
            case KEY_NPAGE:
                p_sys->i_box_plidx += p_sys->i_box_lines;
                break;
            case 'D':
            case KEY_BACKSPACE:
            case KEY_DC:
            {
                int i_item = p_sys->p_playlist->i_index;

                playlist_LockDelete( p_sys->p_playlist, p_sys->i_box_plidx );
                if( i_item < p_sys->p_playlist->i_size &&
                    i_item != p_sys->p_playlist->i_index )
                {
                    playlist_Goto( p_sys->p_playlist, i_item );
                }
                break;
            }

            case KEY_ENTER:
            case 0x0d:
                if( p_sys->i_current_view == VIEW_ALL )
                {
                    playlist_Goto( p_sys->p_playlist, p_sys->i_box_plidx );
                }
                else
                {
                    if( p_sys->pp_plist[p_sys->i_box_plidx]->p_item->i_children
                                == -1 )
                    {
                        playlist_Control( p_sys->p_playlist, PLAYLIST_ITEMPLAY,
                            p_sys->pp_plist[p_sys->i_box_plidx]->p_item );
                    }
                    else
                    {
                        playlist_Control( p_sys->p_playlist, PLAYLIST_VIEWPLAY,
                            p_sys->i_current_view,
                            p_sys->pp_plist[p_sys->i_box_plidx]->p_item,
                            NULL );
                    }
                }
                p_sys->b_box_plidx_follow = VLC_TRUE;
                break;
            default:
                b_ret = VLC_FALSE;
                break;
        }

        if( b_ret )
        {
            int i_max = p_sys->i_plist_entries;
            if( p_sys->i_current_view == VIEW_ALL )
                i_max = p_sys->p_playlist->i_size;
            if( p_sys->i_box_plidx >= i_max ) p_sys->i_box_plidx = i_max - 1;
            if( p_sys->i_box_plidx < 0 ) p_sys->i_box_plidx = 0;
            if( p_sys->i_current_view == VIEW_ALL )
            {
                if( p_sys->i_box_plidx == p_sys->p_playlist->i_index )
                    p_sys->b_box_plidx_follow = VLC_TRUE;
                else
                    p_sys->b_box_plidx_follow = VLC_FALSE;
            }
            else
            {
                if( p_sys->pp_plist[p_sys->i_box_plidx]->p_item ==
                        p_sys->p_playlist->status.p_item )
                    p_sys->b_box_plidx_follow = VLC_TRUE;
                else
                    p_sys->b_box_plidx_follow = VLC_FALSE;
            }
            return 1;
        }
    }
    if( p_sys->i_box_type == BOX_BROWSE )
    {
        vlc_bool_t b_ret = VLC_TRUE;
        /* Browser navigation */
        switch( i_key )
        {
            case KEY_HOME:
                p_sys->i_box_bidx = 0;
                break;
            case KEY_END:
                p_sys->i_box_bidx = p_sys->i_dir_entries - 1;
                break;
            case KEY_UP:
                p_sys->i_box_bidx--;
                break;
            case KEY_DOWN:
                p_sys->i_box_bidx++;
                break;
            case KEY_PPAGE:
                p_sys->i_box_bidx -= p_sys->i_box_lines;
                break;
            case KEY_NPAGE:
                p_sys->i_box_bidx += p_sys->i_box_lines;
                break;

            case KEY_ENTER:
            case 0x0d:
                if( p_sys->pp_dir_entries[p_sys->i_box_bidx]->b_file )
                {
                    int i_size_entry = strlen( p_sys->psz_current_dir ) +
                                       strlen( p_sys->pp_dir_entries[p_sys->i_box_bidx]->psz_path ) + 2;
                    char *psz_uri = (char *)malloc( sizeof(char)*i_size_entry);

                    sprintf( psz_uri, "%s/%s", p_sys->psz_current_dir, p_sys->pp_dir_entries[p_sys->i_box_bidx]->psz_path );
                    playlist_Add( p_sys->p_playlist, psz_uri,
                                  psz_uri,
                                  PLAYLIST_APPEND, PLAYLIST_END );
                    p_sys->i_box_type = BOX_PLAYLIST;
                    free( psz_uri );
                }
                else
                {
                    int i_size_entry = strlen( p_sys->psz_current_dir ) +
                                       strlen( p_sys->pp_dir_entries[p_sys->i_box_bidx]->psz_path ) + 2;
                    char *psz_uri = (char *)malloc( sizeof(char)*i_size_entry);

                    sprintf( psz_uri, "%s/%s", p_sys->psz_current_dir, p_sys->pp_dir_entries[p_sys->i_box_bidx]->psz_path );

                    p_sys->psz_current_dir = strdup( psz_uri );
                    ReadDir( p_intf );
                    free( psz_uri );
                }
                break;
            default:
                b_ret = VLC_FALSE;
                break;
        }
        if( b_ret )
        {
            if( p_sys->i_box_bidx >= p_sys->i_dir_entries ) p_sys->i_box_bidx = p_sys->i_dir_entries - 1;
            if( p_sys->i_box_bidx < 0 ) p_sys->i_box_bidx = 0;
            return 1;
        }
    }
    else if( p_sys->i_box_type == BOX_HELP || p_sys->i_box_type == BOX_INFO )
    {
        switch( i_key )
        {
            case KEY_HOME:
                p_sys->i_box_start = 0;
                return 1;
            case KEY_END:
                p_sys->i_box_start = p_sys->i_box_lines_total - 1;
                return 1;
            case KEY_UP:
                if( p_sys->i_box_start > 0 ) p_sys->i_box_start--;
                return 1;
            case KEY_DOWN:
                if( p_sys->i_box_start < p_sys->i_box_lines_total - 1 )
                {
                    p_sys->i_box_start++;
                }
                return 1;
            case KEY_PPAGE:
                p_sys->i_box_start -= p_sys->i_box_lines;
                if( p_sys->i_box_start < 0 ) p_sys->i_box_start = 0;
                return 1;
            case KEY_NPAGE:
                p_sys->i_box_start += p_sys->i_box_lines;
                if( p_sys->i_box_start >= p_sys->i_box_lines_total )
                {
                    p_sys->i_box_start = p_sys->i_box_lines_total - 1;
                }
                return 1;
            default:
                break;
        }
    }
    else if( p_sys->i_box_type == BOX_NONE )
    {
        switch( i_key )
        {
            case KEY_HOME:
                p_sys->f_slider = 0;
                ManageSlider( p_intf );
                return 1;
            case KEY_END:
                p_sys->f_slider = 99.9;
                ManageSlider( p_intf );
                return 1;
            case KEY_UP:
                p_sys->f_slider += 5.0;
                if( p_sys->f_slider >= 99.0 ) p_sys->f_slider = 99.0;
                ManageSlider( p_intf );
                return 1;
            case KEY_DOWN:
                p_sys->f_slider -= 5.0;
                if( p_sys->f_slider < 0.0 ) p_sys->f_slider = 0.0;
                ManageSlider( p_intf );
                return 1;

            default:
                break;
        }
    }
    else if( p_sys->i_box_type == BOX_SEARCH && p_sys->psz_search_chain )
    {
        int i_chain_len;
        i_chain_len = strlen( p_sys->psz_search_chain );
        switch( i_key )
        {
            case 0x0c:      /* ^l */
                clear();
                return 1;
            case KEY_ENTER:
            case 0x0d:
                if( i_chain_len > 0 )
                {
                    p_sys->psz_old_search = strdup( p_sys->psz_search_chain );
                }
                else if( p_sys->psz_old_search )
                {
                    SearchPlaylist( p_intf, p_sys->psz_old_search );
                }
                p_sys->i_box_type = BOX_PLAYLIST;
                return 1;
            case 0x1b:      /* Esc. */
                p_sys->i_box_plidx = p_sys->i_before_search;
                p_sys->i_box_type = BOX_PLAYLIST;
                return 1;
            case KEY_BACKSPACE:
                if( i_chain_len > 0 )
                {
                    p_sys->psz_search_chain[ i_chain_len - 1 ] = '\0';
                }
                break;
            default:
                if( i_chain_len < SEARCH_CHAIN_SIZE )
                {
                    p_sys->psz_search_chain[ i_chain_len++ ] = i_key;
                    p_sys->psz_search_chain[ i_chain_len ] = 0;
                }
                break;
        }
        if( p_sys->psz_old_search )
        {
            free( p_sys->psz_old_search );
            p_sys->psz_old_search = NULL;
        }
        SearchPlaylist( p_intf, p_sys->psz_search_chain );
        return 1;
    }
    else if( p_sys->i_box_type == BOX_OPEN && p_sys->psz_open_chain )
    {
        int i_chain_len = strlen( p_sys->psz_open_chain );
        playlist_t *p_playlist = p_sys->p_playlist;

        switch( i_key )
        {
            case 0x0c:      /* ^l */
                clear();
                return 1;
            case KEY_ENTER:
            case 0x0d:
                if( p_playlist && i_chain_len > 0 )
                {
                    playlist_Add( p_playlist, p_sys->psz_open_chain,
                                  p_sys->psz_open_chain,
                                  PLAYLIST_GO|PLAYLIST_APPEND, PLAYLIST_END );
                    p_sys->b_box_plidx_follow = VLC_TRUE;
                }
                p_sys->i_box_type = BOX_PLAYLIST;
                return 1;
            case 0x1b:      /* Esc. */
                p_sys->i_box_type = BOX_PLAYLIST;
                return 1;
            case KEY_BACKSPACE:
                if( i_chain_len > 0 )
                {
                    p_sys->psz_open_chain[ i_chain_len - 1 ] = '\0';
                }
                break;
            default:
                if( i_chain_len < OPEN_CHAIN_SIZE )
                {
                    p_sys->psz_open_chain[ i_chain_len++ ] = i_key;
                    p_sys->psz_open_chain[ i_chain_len ] = 0;
                }
                break;
        }
        return 1;
    }


    /* Common keys */
    switch( i_key )
    {
        case 'q':
        case 'Q':
        case 0x1b:  /* Esc */
            p_intf->p_vlc->b_die = VLC_TRUE;
            return 0;

        /* Box switching */
        case 'i':
            if( p_sys->i_box_type == BOX_INFO )
                p_sys->i_box_type = BOX_NONE;
            else
                p_sys->i_box_type = BOX_INFO;
            p_sys->i_box_lines_total = 0;
            return 1;
        case 'L':
            if( p_sys->i_box_type == BOX_LOG )
                p_sys->i_box_type = BOX_NONE;
            else
                p_sys->i_box_type = BOX_LOG;
            return 1;
        case 'P':
            if( p_sys->i_box_type == BOX_PLAYLIST )
                p_sys->i_box_type = BOX_NONE;
            else
                p_sys->i_box_type = BOX_PLAYLIST;
            return 1;
        case 'B':
            if( p_sys->i_box_type == BOX_BROWSE )
                p_sys->i_box_type = BOX_NONE;
            else
                p_sys->i_box_type = BOX_BROWSE;
            return 1;
        case 'h':
        case 'H':
            if( p_sys->i_box_type == BOX_HELP )
                p_sys->i_box_type = BOX_NONE;
            else
                p_sys->i_box_type = BOX_HELP;
            p_sys->i_box_lines_total = 0;
            return 1;
        case '/':
            if( p_sys->i_box_type != BOX_SEARCH )
            {
                if( p_sys->psz_search_chain == NULL )
                {
                    return 1;
                }
                p_sys->psz_search_chain[0] = '\0';
                p_sys->b_box_plidx_follow = VLC_FALSE;
                p_sys->i_before_search = p_sys->i_box_plidx;
                p_sys->i_box_type = BOX_SEARCH;
            }
            return 1;
        case 'A': /* Open */
            if( p_sys->i_box_type != BOX_OPEN )
            {
                if( p_sys->psz_open_chain == NULL )
                {
                    return 1;
                }
                p_sys->psz_open_chain[0] = '\0';
                p_sys->i_box_type = BOX_OPEN;
            }
            return 1;

        /* Navigation */
        case KEY_RIGHT:
            p_sys->f_slider += 1.0;
            if( p_sys->f_slider > 99.9 ) p_sys->f_slider = 99.9;
            ManageSlider( p_intf );
            return 1;

        case KEY_LEFT:
            p_sys->f_slider -= 1.0;
            if( p_sys->f_slider < 0.0 ) p_sys->f_slider = 0.0;
            ManageSlider( p_intf );
            return 1;

        /* Common control */
        case 'f':
        {
            if( p_intf->p_sys->p_input )
            {
                vout_thread_t *p_vout;
                p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                          VLC_OBJECT_VOUT, FIND_CHILD );
                if( p_vout )
                {
                    var_Get( p_vout, "fullscreen", &val );
                    val.b_bool = !val.b_bool;
                    var_Set( p_vout, "fullscreen", val );
                    vlc_object_release( p_vout );
                }
                else
                {
                    playlist_t *p_playlist;
                    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                  FIND_ANYWHERE );
                    if( p_playlist )
                    {
                        var_Get( p_playlist, "fullscreen", &val );
                        val.b_bool = !val.b_bool;
                        var_Set( p_playlist, "fullscreen", val );
                        vlc_object_release( p_playlist );
                    }
                }
            }
            return 0;
        }

        case ' ':
            PlayPause( p_intf );
            return 1;

        case 's':
            if( p_intf->p_sys->p_playlist )
            {
                playlist_Stop( p_intf->p_sys->p_playlist );
            }
            return 1;

        case 'e':
            Eject( p_intf );
            return 1;

        case '[':
            if( p_sys->p_input )
            {
                val.b_bool = VLC_TRUE;
                var_Set( p_sys->p_input, "prev-title", val );
            }
            return 1;

        case ']':
            if( p_sys->p_input )
            {
                val.b_bool = VLC_TRUE;
                var_Set( p_sys->p_input, "next-title", val );
            }
            return 1;

        case '<':
            if( p_sys->p_input )
            {
                val.b_bool = VLC_TRUE;
                var_Set( p_sys->p_input, "prev-chapter", val );
            }
            return 1;

        case '>':
            if( p_sys->p_input )
            {
                val.b_bool = VLC_TRUE;
                var_Set( p_sys->p_input, "next-chapter", val );
            }
            return 1;

        case 'p':
            if( p_intf->p_sys->p_playlist )
            {
                playlist_Prev( p_intf->p_sys->p_playlist );
            }
            clear();
            return 1;

        case 'n':
            if( p_intf->p_sys->p_playlist )
            {
                playlist_Next( p_intf->p_sys->p_playlist );
            }
            clear();
            return 1;

        case 'a':
            aout_VolumeUp( p_intf, 1, NULL );
            clear();
            return 1;

        case 'z':
            aout_VolumeDown( p_intf, 1, NULL );
            clear();
            return 1;

        /*
         * ^l should clear and redraw the screen
         */
        case 0x0c:
            clear();
            return 1;

        default:
            return 0;
    }
}

static void ManageSlider( intf_thread_t *p_intf )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    vlc_value_t     val;

    if( p_input == NULL )
    {
        return;
    }
    var_Get( p_input, "state", &val );
    if( val.i_int != PLAYING_S )
    {
        return;
    }

    var_Get( p_input, "position", &val );
    if( p_sys->f_slider == p_sys->f_slider_old )
    {
        p_sys->f_slider =
        p_sys->f_slider_old = 100 * val.f_float;
    }
    else
    {
        p_sys->f_slider_old = p_sys->f_slider;

        val.f_float = p_sys->f_slider / 100.0;
        var_Set( p_input, "position", val );
    }
}

static void SearchPlaylist( intf_thread_t *p_intf, char *psz_searchstring )
{
    int i_max;
    int i_first = 0 ;
    int i_item = -1;
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = p_sys->p_playlist;

    if( p_sys->i_before_search >= 0 )
    {
        i_first = p_sys->i_before_search;
    }

    if( ( ! psz_searchstring ) ||  strlen( psz_searchstring ) <= 0 )
    {
        p_sys->i_box_plidx = p_sys->i_before_search;
        return;
    }

    i_max = p_sys->i_current_view == VIEW_ALL ?
                p_playlist->i_size : p_sys->i_plist_entries;

    i_item = SubSearchPlaylist( p_intf, psz_searchstring, i_first + 1, i_max );
    if( i_item < 0 )
    {
        i_item = SubSearchPlaylist( p_intf, psz_searchstring, 0, i_first );
    }

    if( i_item < 0 || i_item >= i_max ) return;

    p_sys->i_box_plidx = i_item;
}

static int SubSearchPlaylist( intf_thread_t *p_intf, char *psz_searchstring,
                              int i_start, int i_stop )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = p_sys->p_playlist;
    int i, i_item = -1;

    if( p_sys->i_current_view == VIEW_ALL )
    {
        for( i = i_start + 1; i < i_stop; i++ )
        {
            if( strcasestr( p_playlist->pp_items[i]->input.psz_name,
                            psz_searchstring ) != NULL
                || strcasestr( p_playlist->pp_items[i]->input.psz_uri,
                               psz_searchstring ) != NULL )
            {
                i_item = i;
                break;
            }
        }
    }
    else
    {
        for( i = i_start + 1; i < i_stop; i++ )
        {
            if( strcasestr( p_sys->pp_plist[i]->psz_display,
                            psz_searchstring ) != NULL )
            {
                i_item = i;
                break;
            }
        }
    }

    return i_item;
}


static void mvnprintw( int y, int x, int w, const char *p_fmt, ... )
{
    va_list  vl_args;
    char    *p_buf = NULL;
    int      i_len;

    va_start( vl_args, p_fmt );
    vasprintf( &p_buf, p_fmt, vl_args );
    va_end( vl_args );

    if( p_buf == NULL )
    {
        return;
    }
    if(  w > 0 )
    {
        if( ( i_len = strlen( p_buf ) ) > w )
        {
            int i_cut = i_len - w;
            int x1 = i_len/2 - i_cut/2;
            int x2 = x1 + i_cut;

            if( i_len > x2 )
            {
                memmove( &p_buf[x1], &p_buf[x2], i_len - x2 );
            }
            p_buf[w] = '\0';
            if( w > 7 )
            {
                p_buf[w/2-1] = '.';
                p_buf[w/2  ] = '.';
                p_buf[w/2+1] = '.';
            }
            mvprintw( y, x, "%s", p_buf );
        }
        else
        {
            mvprintw( y, x, "%s", p_buf );
            mvhline( y, x + i_len, ' ', w - i_len );
        }
    }
}
static void MainBoxWrite( intf_thread_t *p_intf, int l, int x, const char *p_fmt, ... )
{
    intf_sys_t     *p_sys = p_intf->p_sys;

    va_list  vl_args;
    char    *p_buf = NULL;

    if( l < p_sys->i_box_start || l - p_sys->i_box_start >= p_sys->i_box_lines )
    {
        return;
    }

    va_start( vl_args, p_fmt );
    vasprintf( &p_buf, p_fmt, vl_args );
    va_end( vl_args );

    if( p_buf == NULL )
    {
        return;
    }

    mvnprintw( p_sys->i_box_y + l - p_sys->i_box_start, x, COLS - x - 1, "%s", p_buf );
}

static void Redraw( intf_thread_t *p_intf, time_t *t_last_refresh )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    int y = 0;
    int h;
    int y_end;

    //clear();

    /* Title */
    attrset( A_REVERSE );
    mvnprintw( y, 0, COLS, "VLC media player" " (ncurses interface) [ h for help ]" );
    attroff( A_REVERSE );
    y += 2;

    /* Infos */
    if( p_input && !p_input->b_dead )
    {
        char buf1[MSTRTIME_MAX_SIZE];
        char buf2[MSTRTIME_MAX_SIZE];
        vlc_value_t val;
        vlc_value_t val_list;

        /* Source */
        mvnprintw( y++, 0, COLS, " Source   : %s",
                   p_input->input.p_item->psz_uri );

        /* State */
        var_Get( p_input, "state", &val );
        if( val.i_int == PLAYING_S )
        {
            mvnprintw( y++, 0, COLS, " State    : Playing" );
        }
        else if( val.i_int == PAUSE_S )
        {
            mvnprintw( y++, 0, COLS, " State    : Paused" );
        }
        else
        {
            y++;
        }
        if( val.i_int != INIT_S && val.i_int != END_S )
        {
            audio_volume_t i_volume;

            /* Position */
            var_Get( p_input, "time", &val );
            msecstotimestr( buf1, val.i_time / 1000 );

            var_Get( p_input, "length", &val );
            msecstotimestr( buf2, val.i_time / 1000 );

            mvnprintw( y++, 0, COLS, " Position : %s/%s (%.2f%%)", buf1, buf2, p_sys->f_slider );

            /* Volume */
            aout_VolumeGet( p_intf, &i_volume );
            mvnprintw( y++, 0, COLS, " Volume   : %i%%", i_volume*200/AOUT_VOLUME_MAX );

            /* Title */
            if( !var_Get( p_input, "title", &val ) )
            {
                var_Change( p_input, "title", VLC_VAR_GETCHOICES, &val_list, NULL );
                if( val_list.p_list->i_count > 0 )
                {
                    mvnprintw( y++, 0, COLS, " Title    : %d/%d", val.i_int, val_list.p_list->i_count );
                }
                var_Change( p_input, "title", VLC_VAR_FREELIST, &val_list, NULL );
            }

            /* Chapter */
            if( !var_Get( p_input, "chapter", &val ) )
            {
                var_Change( p_input, "chapter", VLC_VAR_GETCHOICES, &val_list, NULL );
                if( val_list.p_list->i_count > 0 )
                {
                    mvnprintw( y++, 0, COLS, " Chapter  : %d/%d", val.i_int, val_list.p_list->i_count );
                }
                var_Change( p_input, "chapter", VLC_VAR_FREELIST, &val_list, NULL );
            }
        }
        else
        {
            y += 2;
        }
    }
    else
    {
        mvnprintw( y++, 0, COLS, "Source: <no current item>" );
        DrawEmptyLine( p_sys->w, y++, 0, COLS );
        DrawEmptyLine( p_sys->w, y++, 0, COLS );
        DrawEmptyLine( p_sys->w, y++, 0, COLS );
    }

    DrawBox( p_sys->w, y, 0, 3, COLS, "" );
    DrawEmptyLine( p_sys->w, y+1, 1, COLS-2);
    DrawLine( p_sys->w, y+1, 1, (int)(p_intf->p_sys->f_slider/100.0 * (COLS -2)) );
    y += 3;

    p_sys->i_box_y = y + 1;
    p_sys->i_box_lines = LINES - y - 2;

    h = LINES - y;
    y_end = y + h - 1;

    if( p_sys->i_box_type == BOX_HELP )
    {
        /* Help box */
        int l = 0;
        DrawBox( p_sys->w, y++, 0, h, COLS, " Help " );

        MainBoxWrite( p_intf, l++, 1, "[Display]" );
        MainBoxWrite( p_intf, l++, 1, "     h,H         Show/Hide help box" );
        MainBoxWrite( p_intf, l++, 1, "     i           Show/Hide info box" );
        MainBoxWrite( p_intf, l++, 1, "     L           Show/Hide messages box" );
        MainBoxWrite( p_intf, l++, 1, "     P           Show/Hide playlist box" );
        MainBoxWrite( p_intf, l++, 1, "     B           Show/Hide filebrowser" );
        MainBoxWrite( p_intf, l++, 1, "" );

        MainBoxWrite( p_intf, l++, 1, "[Global]" );
        MainBoxWrite( p_intf, l++, 1, "     q, Q        Quit" );
        MainBoxWrite( p_intf, l++, 1, "     s           Stop" );
        MainBoxWrite( p_intf, l++, 1, "     <space>     Pause/Play" );
        MainBoxWrite( p_intf, l++, 1, "     f           Toggle Fullscreen" );
        MainBoxWrite( p_intf, l++, 1, "     n, p        Next/Previous playlist item" );
        MainBoxWrite( p_intf, l++, 1, "     [, ]        Next/Previous title" );
        MainBoxWrite( p_intf, l++, 1, "     <, >        Next/Previous chapter" );
        MainBoxWrite( p_intf, l++, 1, "     <right>     Seek +1%%" );
        MainBoxWrite( p_intf, l++, 1, "     <left>      Seek -1%%" );
        MainBoxWrite( p_intf, l++, 1, "     a           Volume Up" );
        MainBoxWrite( p_intf, l++, 1, "     z           Volume Down" );
        MainBoxWrite( p_intf, l++, 1, "" );

        MainBoxWrite( p_intf, l++, 1, "[Playlist]" );
        MainBoxWrite( p_intf, l++, 1, "     r           Random" );
        MainBoxWrite( p_intf, l++, 1, "     l           Loop Playlist" );
        MainBoxWrite( p_intf, l++, 1, "     R           Repeat item" );
        MainBoxWrite( p_intf, l++, 1, "     o           Order Playlist by title" );
        MainBoxWrite( p_intf, l++, 1, "     O           Reverse order Playlist by title" );
        MainBoxWrite( p_intf, l++, 1, "     /           Look for an item" );
        MainBoxWrite( p_intf, l++, 1, "     A           Add an entry" );
        MainBoxWrite( p_intf, l++, 1, "     D, <del>    Delete an entry" );
        MainBoxWrite( p_intf, l++, 1, "     <backspace> Delete an entry" );
        MainBoxWrite( p_intf, l++, 1, "" );

        MainBoxWrite( p_intf, l++, 1, "[Boxes]" );
        MainBoxWrite( p_intf, l++, 1, "     <up>,<down>     Navigate through the box line by line" );
        MainBoxWrite( p_intf, l++, 1, "     <pgup>,<pgdown> Navigate through the box page by page" );
        MainBoxWrite( p_intf, l++, 1, "" );

        MainBoxWrite( p_intf, l++, 1, "[Player]" );
        MainBoxWrite( p_intf, l++, 1, "     <up>,<down>     Seek +/-5%%" );
        MainBoxWrite( p_intf, l++, 1, "" );

        MainBoxWrite( p_intf, l++, 1, "[Miscellaneous]" );
        MainBoxWrite( p_intf, l++, 1, "     Ctrl-l          Refresh the screen" );

        p_sys->i_box_lines_total = l;
        if( p_sys->i_box_start >= p_sys->i_box_lines_total )
        {
            p_sys->i_box_start = p_sys->i_box_lines_total - 1;
        }

        if( l - p_sys->i_box_start < p_sys->i_box_lines )
        {
            y += l - p_sys->i_box_start;
        }
        else
        {
            y += p_sys->i_box_lines;
        }
    }
    else if( p_sys->i_box_type == BOX_INFO )
    {
        /* Info box */
        int l = 0;
        DrawBox( p_sys->w, y++, 0, h, COLS, " Information " );

        if( p_input )
        {
            int i,j;
            vlc_mutex_lock( &p_input->input.p_item->lock );
            for( i = 0; i < p_input->input.p_item->i_categories; i++ )
            {
                info_category_t *p_category = p_input->input.p_item->pp_categories[i];
                if( y >= y_end ) break;
                MainBoxWrite( p_intf, l++, 1, "  [%s]", p_category->psz_name );
                for( j = 0; j < p_category->i_infos; j++ )
                {
                    info_t *p_info = p_category->pp_infos[j];
                    if( y >= y_end ) break;
                    MainBoxWrite( p_intf, l++, 1, "      %s: %s", p_info->psz_name, p_info->psz_value );
                }
            }
            vlc_mutex_unlock( &p_input->input.p_item->lock );
        }
        else
        {
            MainBoxWrite( p_intf, l++, 1, "No item currently playing" );
        }
        p_sys->i_box_lines_total = l;
        if( p_sys->i_box_start >= p_sys->i_box_lines_total )
        {
            p_sys->i_box_start = p_sys->i_box_lines_total - 1;
        }

        if( l - p_sys->i_box_start < p_sys->i_box_lines )
        {
            y += l - p_sys->i_box_start;
        }
        else
        {
            y += p_sys->i_box_lines;
        }
    }
    else if( p_sys->i_box_type == BOX_LOG )
    {
        int i_line = 0;
        int i_stop;
        int i_start;

        DrawBox( p_sys->w, y++, 0, h, COLS, " Logs " );

        i_start = p_intf->p_sys->p_sub->i_start;

        vlc_mutex_lock( p_intf->p_sys->p_sub->p_lock );
        i_stop = *p_intf->p_sys->p_sub->pi_stop;
        vlc_mutex_unlock( p_intf->p_sys->p_sub->p_lock );

        for( ;; )
        {
            static const char *ppsz_type[4] = { "", "error", "warning", "debug" };
            if( i_line >= h - 2 )
            {
                break;
            }
            i_stop--;
            i_line++;
            if( i_stop < 0 ) i_stop += VLC_MSG_QSIZE;
            if( i_stop == i_start )
            {
                break;
            }
            mvnprintw( y + h-2-i_line, 1, COLS - 2, "   [%s] %s",
                      ppsz_type[p_sys->p_sub->p_msg[i_stop].i_type],
                      p_sys->p_sub->p_msg[i_stop].psz_msg );
        }

        vlc_mutex_lock( p_intf->p_sys->p_sub->p_lock );
        p_intf->p_sys->p_sub->i_start = i_stop;
        vlc_mutex_unlock( p_intf->p_sys->p_sub->p_lock );
        y = y_end;
    }
    else if( p_sys->i_box_type == BOX_BROWSE )
    {
        /* Filebrowser box */
        int        i_start, i_stop;
        int        i_item;
        DrawBox( p_sys->w, y++, 0, h, COLS, " Browse " );

        if( p_sys->i_box_bidx >= p_sys->i_dir_entries ) p_sys->i_box_plidx = p_sys->i_dir_entries - 1;
        if( p_sys->i_box_bidx < 0 ) p_sys->i_box_bidx = 0;

        if( p_sys->i_box_bidx < (h - 2)/2 )
        {
            i_start = 0;
            i_stop = h - 2;
        }
        else if( p_sys->i_dir_entries - p_sys->i_box_bidx > (h - 2)/2 )
        {
            i_start = p_sys->i_box_bidx - (h - 2)/2;
            i_stop = i_start + h - 2;
        }
        else
        {
            i_stop = p_sys->i_dir_entries;
            i_start = p_sys->i_dir_entries - (h - 2);
        }
        if( i_start < 0 )
        {
            i_start = 0;
        }
        if( i_stop > p_sys->i_dir_entries )
        {
            i_stop = p_sys->i_dir_entries;
        }

        for( i_item = i_start; i_item < i_stop; i_item++ )
        {
            vlc_bool_t b_selected = ( p_sys->i_box_bidx == i_item );

            if( y >= y_end ) break;
            if( b_selected )
            {
                attrset( A_REVERSE );
            }
            mvnprintw( y++, 1, COLS - 2, "%c %s", p_sys->pp_dir_entries[i_item]->b_file == VLC_TRUE ? '-' : '+',
                            p_sys->pp_dir_entries[i_item]->psz_path );
            if( b_selected )
            {
                attroff( A_REVERSE );
            }
        }

    }
    else if( ( p_sys->i_box_type == BOX_PLAYLIST ||
               p_sys->i_box_type == BOX_SEARCH ||
               p_sys->i_box_type == BOX_OPEN  ) && p_sys->p_playlist )
    {
        /* Playlist box */
        playlist_t *p_playlist = p_sys->p_playlist;
        int        i_start, i_stop, i_max = p_sys->i_plist_entries;
        int        i_item;
        char       *psz_title;

        switch( p_sys->i_current_view )
        {
            case VIEW_ALL:
                psz_title = strdup( " Playlist (All, unsorted) " );
                i_max = p_playlist->i_size;
                break;
            case VIEW_CATEGORY:
                psz_title = strdup( " Playlist (By category) " );
                break;
            default:
                psz_title = strdup( " Playlist (Manually added) " );
        }

        DrawBox( p_sys->w, y++, 0, h, COLS, psz_title );

        if( p_sys->i_current_view != VIEW_ALL &&
                ( p_sys->b_need_update || p_sys->pp_plist == NULL ) )
        {
            PlaylistRebuild( p_intf );
        }
        if( p_sys->b_box_plidx_follow )
        {
            FindIndex( p_intf );
        }

        if( p_sys->i_box_plidx < 0 ) p_sys->i_box_plidx = 0;

        if( p_sys->i_box_plidx < (h - 2)/2 )
        {
            i_start = 0;
            i_stop = h - 2;
        }
        else if( i_max - p_sys->i_box_plidx > (h - 2)/2 )
        {
            i_start = p_sys->i_box_plidx - (h - 2)/2;
            i_stop = i_start + h - 2;
        }
        else
        {
            i_stop = i_max;
            i_start = i_max - (h - 2);
        }
        if( i_start < 0 )
        {
            i_start = 0;
        }
        if( i_stop > i_max )
        {
            i_stop = i_max;
        }

       if( p_sys->i_current_view == VIEW_ALL )
       {

        for( i_item = i_start; i_item < i_stop; i_item++ )
        {
            vlc_bool_t b_selected = ( p_sys->i_box_plidx == i_item );
            int c = p_playlist->i_index == i_item ? '>' : ' ';

            if( y >= y_end ) break;
            if( b_selected )
            {
                attrset( A_REVERSE );
            }
            if( !strcmp( p_playlist->pp_items[i_item]->input.psz_name,
                         p_playlist->pp_items[i_item]->input.psz_uri ) )
            {
                mvnprintw( y++, 1, COLS - 2, "%c %d - '%s'",
                           c,
                           i_item,
                           p_playlist->pp_items[i_item]->input.psz_uri );
            }
            else
            {
                mvnprintw( y++, 1, COLS - 2, "%c %d - '%s' (%s)",
                          c,
                          i_item,
                          p_playlist->pp_items[i_item]->input.psz_uri,
                          p_playlist->pp_items[i_item]->input.psz_name );
            }
            if( b_selected )
            {
                attroff( A_REVERSE );
            }
        }

       }
       else
       {

        for( i_item = i_start; i_item < i_stop; i_item++ )
        {
            vlc_bool_t b_selected = ( p_sys->i_box_plidx == i_item );
            int c = ( p_playlist->status.p_item ==
                          p_sys->pp_plist[i_item]->p_item ) ? '>' : ' ';

            if( y >= y_end ) break;
            if( b_selected )
            {
                attrset( A_REVERSE );
            }
            mvnprintw( y++, 1, COLS - 2, "%c%s", c,
                       p_sys->pp_plist[i_item]->psz_display );
            if( b_selected )
            {
                attroff( A_REVERSE );
            }
        }

       }
    }
    else
    {
        y++;
    }
    if( p_sys->i_box_type == BOX_SEARCH )
    {
        DrawEmptyLine( p_sys->w, 7, 1, COLS-2 );
        if( p_sys->psz_search_chain )
        {
            if( strlen( p_sys->psz_search_chain ) == 0 &&
                p_sys->psz_old_search != NULL )
            {
                /* Searching next entry */
                mvnprintw( 7, 1, COLS-2, "Find: %s", p_sys->psz_old_search );
            }
            else
            {
                mvnprintw( 7, 1, COLS-2, "Find: %s", p_sys->psz_search_chain );
            }
        }
    }
    if( p_sys->i_box_type == BOX_OPEN )
    {
        if( p_sys->psz_open_chain )
        {
            DrawEmptyLine( p_sys->w, 7, 1, COLS-2 );
            mvnprintw( 7, 1, COLS-2, "Open: %s", p_sys->psz_open_chain );
        }
    }

    while( y < y_end )
    {
        DrawEmptyLine( p_sys->w, y++, 1, COLS - 2 );
    }

    refresh();

    *t_last_refresh = time( 0 );
}

static void PlaylistRebuild( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_view_t *p_view;
    playlist_t *p_playlist = p_sys->p_playlist;

    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    p_view = playlist_ViewFind( p_playlist, p_intf->p_sys->i_current_view );

    /* First clear the old one */
    PlaylistDestroy( p_intf );

    /* Build the new one */
    PlaylistAddNode( p_intf, p_view->p_root, 0, "" );

    p_sys->b_need_update = VLC_FALSE;

    vlc_mutex_unlock( &p_playlist->object_lock );
}

static void PlaylistAddNode( intf_thread_t *p_intf, playlist_item_t *p_node,
                             int i, char *c )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_item_t *p_child;
    char *psz_tmp;
    int k;

    psz_tmp = (char *)malloc( strlen( c ) + 4 );
    if( psz_tmp == NULL ) return;
    for( k = 0; k < p_node->i_children; k++ )
    {
        struct pl_item_t *p_pl_item;
        char *buff;
        int i_size;

        p_child = p_node->pp_children[k];
        i_size = strlen( c ) + strlen( p_child->input.psz_name ) + 4;
        buff = (char *)malloc( sizeof( char ) * i_size );
        p_pl_item = (struct pl_item_t *)malloc( sizeof( struct pl_item_t ) );
        if(  p_pl_item == NULL || buff == NULL ) return;

        if( strlen( c ) )
        {
            sprintf( buff, "%s%c-%s", c, k == p_node->i_children - 1 ?
                     '`' : '|', p_child->input.psz_name );
        }
        else
        {
            sprintf( buff, " %s", p_child->input.psz_name );
        }
        p_pl_item->psz_display = strdup( buff );
        p_pl_item->p_item = p_child;
        INSERT_ELEM( p_sys->pp_plist, p_sys->i_plist_entries,
                     p_sys->i_plist_entries, p_pl_item );
        free( buff );
        i++;

        if( p_child->i_children > 0 )
        {
            sprintf( psz_tmp, "%s%c ", c,
                     k == p_node->i_children - 1 ? ' ' : '|' );
            PlaylistAddNode( p_intf, p_child, i,
                             strlen( c ) ? psz_tmp : " " );
        }
    }
    free( psz_tmp );
}

static int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                            vlc_value_t oval, vlc_value_t nval, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->b_need_update = VLC_TRUE;
    return VLC_SUCCESS;
}

static void FindIndex( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    int i;

    if( p_sys->i_current_view == VIEW_ALL )
    {
         p_sys->i_box_plidx = p_sys->p_playlist->i_index;
    }
    else if( ( p_sys->i_box_plidx < p_sys->i_plist_entries &&
               p_sys->pp_plist[p_sys->i_box_plidx]->p_item !=
               p_sys->p_playlist->status.p_item ) )
    {
        for( i = 0; i < p_sys->i_plist_entries; i++ )
        {
            if( p_sys->pp_plist[i]->p_item ==
                p_sys->p_playlist->status.p_item )
            {
                p_sys->i_box_plidx = i;
                break;
            }
        }
    }
}

static void PlaylistDestroy( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    int i;

    for( i = 0; i < p_sys->i_plist_entries; i++ )
    {
        struct pl_item_t *p_pl_item = p_sys->pp_plist[i];
        free( p_pl_item->psz_display );
        REMOVE_ELEM( p_sys->pp_plist, p_sys->i_plist_entries, i );
        free( p_pl_item );
    }
    p_sys->pp_plist = NULL;
    p_sys->i_plist_entries = 0;
}

static void Eject( intf_thread_t *p_intf )
{
    char *psz_device = NULL, *psz_parser, *psz_name;

    /*
     * Get the active input
     * Determine whether we can eject a media, ie it's a DVD, VCD or CD-DA
     * If it's neither of these, then return
     */

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    if( p_playlist->i_index < 0 )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        return;
    }

    psz_name = p_playlist->pp_items[ p_playlist->i_index ]->input.psz_name;

    if( psz_name )
    {
        if( !strncmp(psz_name, "dvd://", 4) )
        {
            switch( psz_name[strlen("dvd://")] )
            {
            case '\0':
            case '@':
                psz_device = config_GetPsz( p_intf, "dvd" );
                break;
            default:
                /* Omit the first MRL-selector characters */
                psz_device = strdup( psz_name + strlen("dvd://" ) );
                break;
            }
        }
        else if( !strncmp(psz_name, VCD_MRL, strlen(VCD_MRL)) )
        {
            switch( psz_name[strlen(VCD_MRL)] )
            {
            case '\0':
            case '@':
                psz_device = config_GetPsz( p_intf, VCD_MRL );
                break;
            default:
                /* Omit the beginning MRL-selector characters */
                psz_device = strdup( psz_name + strlen(VCD_MRL) );
                break;
            }
        }
        else if( !strncmp(psz_name, CDDA_MRL, strlen(CDDA_MRL) ) )
        {
            switch( psz_name[strlen(CDDA_MRL)] )
            {
            case '\0':
            case '@':
                psz_device = config_GetPsz( p_intf, "cd-audio" );
                break;
            default:
                /* Omit the beginning MRL-selector characters */
                psz_device = strdup( psz_name + strlen(CDDA_MRL) );
                break;
            }
        }
        else
        {
            psz_device = strdup( psz_name );
        }
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );

    if( psz_device == NULL )
    {
        return;
    }

    /* Remove what we have after @ */
    psz_parser = psz_device;
    for( psz_parser = psz_device ; *psz_parser ; psz_parser++ )
    {
        if( *psz_parser == '@' )
        {
            *psz_parser = '\0';
            break;
        }
    }

    /* If there's a stream playing, we aren't allowed to eject ! */
    if( p_intf->p_sys->p_input == NULL )
    {
        msg_Dbg( p_intf, "ejecting %s", psz_device );

        intf_Eject( p_intf, psz_device );
    }

    free(psz_device);
    return;
}

static void ReadDir( intf_thread_t *p_intf )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    DIR *                       p_current_dir;
    struct dirent *             p_dir_content;
    int i;

    if( p_sys->psz_current_dir && *p_sys->psz_current_dir )
    {
        /* Open the dir */
        p_current_dir = opendir( p_sys->psz_current_dir );

        if( p_current_dir == NULL )
        {
            /* something went bad, get out of here ! */
#ifdef HAVE_ERRNO_H
            msg_Warn( p_intf, "cannot open directory `%s' (%s)",
                      p_sys->psz_current_dir, strerror(errno));
#else
            msg_Warn( p_intf, "cannot open directory `%s'", p_sys->psz_current_dir );
#endif
            return;
        }

        /* Clean the old shit */
        for( i = 0; i < p_sys->i_dir_entries; i++ )
        {
            struct dir_entry_t *p_dir_entry = p_sys->pp_dir_entries[i];
            free( p_dir_entry->psz_path );
            REMOVE_ELEM( p_sys->pp_dir_entries, p_sys->i_dir_entries, i );
            free( p_dir_entry );
        }
        p_sys->pp_dir_entries = NULL;
        p_sys->i_dir_entries = 0;

        /* get the first directory entry */
        p_dir_content = readdir( p_current_dir );

        /* while we still have entries in the directory */
        while( p_dir_content != NULL )
        {
#if defined( S_ISDIR )
            struct stat stat_data;
#endif
            struct dir_entry_t *p_dir_entry;
            int i_size_entry = strlen( p_sys->psz_current_dir ) +
                               strlen( p_dir_content->d_name ) + 2;
            char *psz_uri = (char *)malloc( sizeof(char)*i_size_entry);

            sprintf( psz_uri, "%s/%s", p_sys->psz_current_dir,
                     p_dir_content->d_name );

            if( !( p_dir_entry = malloc( sizeof( struct dir_entry_t) ) ) )
            {
                free( psz_uri);
                return;
            }

#if defined( S_ISDIR )
            stat( psz_uri, &stat_data );
            if( S_ISDIR(stat_data.st_mode) )
#elif defined( DT_DIR )
            if( p_dir_content->d_type & DT_DIR )
#else
            if( 0 )
#endif
            {
                p_dir_entry->psz_path = strdup( p_dir_content->d_name );
                p_dir_entry->b_file = VLC_FALSE;
                INSERT_ELEM( p_sys->pp_dir_entries, p_sys->i_dir_entries,
                     p_sys->i_dir_entries, p_dir_entry );
            }
            else
            {
                p_dir_entry->psz_path = strdup( p_dir_content->d_name );
                p_dir_entry->b_file = VLC_TRUE;
                INSERT_ELEM( p_sys->pp_dir_entries, p_sys->i_dir_entries,
                     p_sys->i_dir_entries, p_dir_entry );
            }

            free( psz_uri );
            /* Read next entry */
            p_dir_content = readdir( p_current_dir );
        }
        closedir( p_current_dir );
        return;
    }
    else
    {
        msg_Dbg( p_intf, "no current dir set" );
        return;
    }
}

static void PlayPause( intf_thread_t *p_intf )
{
    input_thread_t *p_input = p_intf->p_sys->p_input;
    vlc_value_t val;

    if( p_input )
    {
        var_Get( p_input, "state", &val );
        if( val.i_int != PAUSE_S )
        {
            val.i_int = PAUSE_S;
        }
        else
        {
            val.i_int = PLAYING_S;
        }
        var_Set( p_input, "state", val );
    }
    else if( p_intf->p_sys->p_playlist )
    {
        playlist_Play( p_intf->p_sys->p_playlist );
    }
}

/****************************************************************************
 *
 ****************************************************************************/
static void DrawBox( WINDOW *win, int y, int x, int h, int w, char *title )
{
    int i;
    int i_len;

    if( w > 3 && h > 2 )
    {
        if( title == NULL ) title = "";
        i_len = strlen( title );

        if( i_len > w - 2 ) i_len = w - 2;

        mvwaddch( win, y, x,    ACS_ULCORNER );
        mvwhline( win, y, x+1,  ACS_HLINE, ( w-i_len-2)/2 );
        mvwprintw( win,y, x+1+(w-i_len-2)/2, "%s", title );
        mvwhline( win, y, x+(w-i_len)/2+i_len,  ACS_HLINE, w - 1 - ((w-i_len)/2+i_len) );
        mvwaddch( win, y, x+w-1,ACS_URCORNER );

        for( i = 0; i < h-2; i++ )
        {
            mvwaddch( win, y+i+1, x,     ACS_VLINE );
            mvwaddch( win, y+i+1, x+w-1, ACS_VLINE );
        }

        mvwaddch( win, y+h-1, x,     ACS_LLCORNER );
        mvwhline( win, y+h-1, x+1,   ACS_HLINE, w - 2 );
        mvwaddch( win, y+h-1, x+w-1, ACS_LRCORNER );
    }
}

static void DrawEmptyLine( WINDOW *win, int y, int x, int w )
{
    if( w > 0 )
    {
        mvhline( y, x, ' ', w );
    }
}

static void DrawLine( WINDOW *win, int y, int x, int w )
{
    if( w > 0 )
    {
        attrset( A_REVERSE );
        mvhline( y, x, ' ', w );
        attroff( A_REVERSE );
    }
}
