/*****************************************************************************
 * ncurses.c : NCurses plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN
 * $Id: ncurses.c,v 1.11 2004/01/11 14:53:36 garf Exp $
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open           ( vlc_object_t * );
static void Close          ( vlc_object_t * );

static void Run            ( intf_thread_t * );
static void PlayPause      ( intf_thread_t * );
static void Eject          ( intf_thread_t * );

static int  HandleKey      ( intf_thread_t *, int );
static void Redraw           ( intf_thread_t *, time_t * );
static void ManageSlider   ( intf_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("ncurses interface") );
    set_capability( "interface", 10 );
    set_callbacks( Open, Close );
    add_shortcut( "curses" );
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
    BOX_PLAYLIST
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

    int             b_box_cleared;

    msg_subscription_t* p_sub;                  /* message bank subscription */
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

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t    *p_sys = p_intf->p_sys;

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
            p_sys->p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
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

        if( p_sys->b_box_plidx_follow )
        {
            p_sys->i_box_plidx = p_sys->p_playlist->i_index;
        }


        while( ( i_key = getch()) != -1 )
        {
            /*
             * HandleKey returns 1 if the screen needs to be redrawn
             */
            if ( HandleKey( p_intf, i_key ) )
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
        if ( (time(0) - t_last_refresh) >= 1 )
        {
            ManageSlider ( p_intf );
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
            /* Playlist sort */
            case 'r':
                playlist_Sort( p_sys->p_playlist, SORT_RANDOM, ORDER_NORMAL );
                return 1;
            case 'o':
                playlist_Sort( p_sys->p_playlist, SORT_TITLE, ORDER_NORMAL );
                return 1;
            case 'O':
                playlist_Sort( p_sys->p_playlist, SORT_TITLE, ORDER_REVERSE );
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
            case KEY_BACKSPACE:
            case KEY_DC:
            {
                int i_item = p_sys->p_playlist->i_index;

                playlist_Delete( p_sys->p_playlist, p_sys->i_box_plidx );
                if( i_item < p_sys->p_playlist->i_size && i_item != p_sys->p_playlist->i_index )
                {
                    playlist_Goto( p_sys->p_playlist, i_item );
                }
                break;
            }

            case KEY_ENTER:
            case 0x0d:
                playlist_Goto( p_sys->p_playlist, p_sys->i_box_plidx );
                break;
            default:
                b_ret = VLC_FALSE;
                break;
        }

        if( b_ret )
        {
            if( p_sys->i_box_plidx >= p_sys->p_playlist->i_size ) p_sys->i_box_plidx = p_sys->p_playlist->i_size - 1;
            if( p_sys->i_box_plidx < 0 ) p_sys->i_box_plidx = 0;
            if( p_sys->i_box_plidx == p_sys->p_playlist->i_index )
                p_sys->b_box_plidx_follow = VLC_TRUE;
            else
                p_sys->b_box_plidx_follow = VLC_FALSE;
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
                ManageSlider ( p_intf );
                return 1;
            case KEY_END:
                p_sys->f_slider = 99.9;
                ManageSlider ( p_intf );
                return 1;
            case KEY_UP:
                p_sys->f_slider += 20.0;
                if( p_sys->f_slider >= 99.0 ) p_sys->f_slider = 99.0;
                ManageSlider ( p_intf );
                return 1;
            case KEY_DOWN:
                p_sys->f_slider -= 20.0;
                if( p_sys->f_slider < 0.0 ) p_sys->f_slider = 0.0;
                ManageSlider ( p_intf );
                return 1;

            default:
                break;
        }
    }

    /* Common keys */
    switch( i_key )
    {
        case 'q':
        case 'Q':
        case 0x1b:  /* Esc */
            p_intf->b_die = 1;
            return 0;

        /* Box switching */
        case 'i':
            if( p_sys->i_box_type == BOX_INFO )
                p_sys->i_box_type = BOX_NONE;
            else
                p_sys->i_box_type = BOX_INFO;
            p_sys->i_box_lines_total = 0;
            return 1;
        case 'l':
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
        case 'h':
        case 'H':
            if( p_sys->i_box_type == BOX_HELP )
                p_sys->i_box_type = BOX_NONE;
            else
                p_sys->i_box_type = BOX_HELP;
            p_sys->i_box_lines_total = 0;
            return 1;

        /* Navigation */
        case KEY_RIGHT:
            p_sys->f_slider += 1.0;
            if( p_sys->f_slider > 99.9 ) p_sys->f_slider = 99.9;
            ManageSlider ( p_intf );
            return 1;

        case KEY_LEFT:
            p_sys->f_slider -= 1.0;
            if( p_sys->f_slider < 0.0 ) p_sys->f_slider = 0.0;
            ManageSlider ( p_intf );
            return 1;

        /* Common control */
        case 'f':
        {
            vout_thread_t *p_vout;
            if( p_intf->p_sys->p_input )
            {
                p_vout = vlc_object_find( p_intf->p_sys->p_input,
                                          VLC_OBJECT_VOUT, FIND_CHILD );
                if( p_vout )
                {
                    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                    vlc_object_release( p_vout );
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
            break;

        case ']':
            if( p_sys->p_input )
            {
                val.b_bool = VLC_TRUE;
                var_Set( p_sys->p_input, "next-title", val );
            }
            break;

        case '<':
            if( p_sys->p_input )
            {
                val.b_bool = VLC_TRUE;
                var_Set( p_sys->p_input, "prev-chapter", val );
            }
            break;

        case '>':
            if( p_sys->p_input )
            {
                val.b_bool = VLC_TRUE;
                var_Set( p_sys->p_input, "next-chapter", val );
            }
            break;

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

static void ManageSlider ( intf_thread_t *p_intf )
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

static void mvnprintw( int y, int x, int w, const char *p_fmt, ... )
{
    va_list  vl_args;
    char    *p_buf = NULL;
    int      i_len;

    va_start ( vl_args, p_fmt );
    vasprintf ( &p_buf, p_fmt, vl_args );
    va_end ( vl_args );

    if ( p_buf == NULL )
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

    va_start ( vl_args, p_fmt );
    vasprintf ( &p_buf, p_fmt, vl_args );
    va_end ( vl_args );

    if( p_buf == NULL )
    {
        return;
    }

    mvnprintw( p_sys->i_box_y + l - p_sys->i_box_start, x, COLS - x - 1, "%s", p_buf );
}

static void Redraw ( intf_thread_t *p_intf, time_t *t_last_refresh )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    int y = 0;
    int h;
    int y_end;

    //clear();

    /* Title */
    attrset ( A_REVERSE );
    mvnprintw( y, 0, COLS, VOUT_TITLE " (ncurses interface) [ h for help ]" );
    attroff ( A_REVERSE );
    y += 2;

    /* Infos */
    if( p_input && !p_input->b_dead )
    {
        char buf1[MSTRTIME_MAX_SIZE];
        char buf2[MSTRTIME_MAX_SIZE];
        vlc_value_t val;
        vlc_value_t val_list;

        /* Source */
        mvnprintw( y++, 0, COLS, " Source   : %s", p_input->psz_source );

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
            /* Position */
            var_Get( p_input, "time", &val );
            msecstotimestr( buf1, val.i_time / 1000 );

            var_Get( p_input, "length", &val );
            msecstotimestr( buf2, val.i_time / 1000 );

            mvnprintw( y++, 0, COLS, " Position : %s/%s (%.2f%%)", buf1, buf2, p_sys->f_slider );

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
            y++;
        }
    }
    else
    {
        mvnprintw( y++, 0, COLS, "Source: <no current item>" );
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
        MainBoxWrite( p_intf, l++, 1, "     i           Show/Hide informations box" );
        MainBoxWrite( p_intf, l++, 1, "     l           Show/Hide logs box" );
        MainBoxWrite( p_intf, l++, 1, "     P           Show/Hide playlist box" );
        MainBoxWrite( p_intf, l++, 1, "" );

        MainBoxWrite( p_intf, l++, 1, "[Global]" );
        MainBoxWrite( p_intf, l++, 1, "     q, Q        Quit" );
        MainBoxWrite( p_intf, l++, 1, "     s           Stop" );
        MainBoxWrite( p_intf, l++, 1, "   <space>       Pause/Play" );
        MainBoxWrite( p_intf, l++, 1, "     n, p        Next/Previous item" );
        MainBoxWrite( p_intf, l++, 1, "     [, ]        Next/Previous title" );
        MainBoxWrite( p_intf, l++, 1, "     <, >        Next/Previous title" );
        MainBoxWrite( p_intf, l++, 1, "     <right>     Seek +5%%" );
        MainBoxWrite( p_intf, l++, 1, "     <left>      Seek -5%%" );
        MainBoxWrite( p_intf, l++, 1, "" );

        MainBoxWrite( p_intf, l++, 1, "[Playlist]" );
        MainBoxWrite( p_intf, l++, 1, "     r           Randomize playlist" );
        MainBoxWrite( p_intf, l++, 1, "     o           Order Playlist" );
        MainBoxWrite( p_intf, l++, 1, "     O           Reverse order Playlist" );
        MainBoxWrite( p_intf, l++, 1, "   <del>         Delete an entry" );
        MainBoxWrite( p_intf, l++, 1, "  <backspace>    Delete an entry" );
        MainBoxWrite( p_intf, l++, 1, "" );

        MainBoxWrite( p_intf, l++, 1, "[Boxes]" );
        MainBoxWrite( p_intf, l++, 1, "  <up>,<down>    Navigate through the box line by line" );
        MainBoxWrite( p_intf, l++, 1, " <pgup>,<pgdown> Navigate through the box page by page" );
        MainBoxWrite( p_intf, l++, 1, "" );

        MainBoxWrite( p_intf, l++, 1, "[Player]" );
        MainBoxWrite( p_intf, l++, 1, "  <up>,<down>    Seek +/-20%%" );
        MainBoxWrite( p_intf, l++, 1, "" );

        MainBoxWrite( p_intf, l++, 1, "[Miscellaneous]" );
        MainBoxWrite( p_intf, l++, 1, "   Ctrl-L        Refresh the screen" );

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
        DrawBox( p_sys->w, y++, 0, h, COLS, " Informations " );

        if( p_input )
        {
            input_info_category_t * p_category;
            input_info_t * p_info;

            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_category = p_input->stream.p_info;
            while ( p_category )
            {
                if( y >= y_end ) break;
                MainBoxWrite( p_intf, l++, 1, "  [%s]", p_category->psz_name );
                p_info = p_category->p_info;
                while ( p_info )
                {
                    if( y >= y_end ) break;
                    MainBoxWrite( p_intf, l++, 1, "      %s: %s", p_info->psz_name, p_info->psz_value );
                    p_info = p_info->p_next;
                }
                p_category = p_category->p_next;
            }
            vlc_mutex_unlock( &p_input->stream.stream_lock );
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
    else if( p_sys->i_box_type == BOX_PLAYLIST && p_sys->p_playlist )
    {
        /* Playlist box */
        playlist_t *p_playlist = p_sys->p_playlist;
        int        i_start, i_stop;
        int        i_item;
        DrawBox( p_sys->w, y++, 0, h, COLS, " Playlist " );

        if( p_sys->i_box_plidx >= p_playlist->i_size ) p_sys->i_box_plidx = p_playlist->i_size - 1;
        if( p_sys->i_box_plidx < 0 ) p_sys->i_box_plidx = 0;

        if( p_sys->i_box_plidx < (h - 2)/2 )
        {
            i_start = 0;
            i_stop = h - 2;
        }
        else if( p_playlist->i_size - p_sys->i_box_plidx > (h - 2)/2 )
        {
            i_start = p_sys->i_box_plidx - (h - 2)/2;
            i_stop = i_start + h - 2;
        }
        else
        {
            i_stop = p_playlist->i_size;
            i_start = p_playlist->i_size - (h - 2);
        }
        if( i_start < 0 )
        {
            i_start = 0;
        }
        if( i_stop > p_playlist->i_size )
        {
            i_stop = p_playlist->i_size;
        }

        for( i_item = i_start; i_item < i_stop; i_item++ )
        {
            vlc_bool_t b_selected = ( p_sys->i_box_plidx == i_item );
            int c = p_playlist->i_index == i_item ? '>' : ' ';

            if( y >= y_end ) break;
            if( b_selected )
            {
                attrset( A_REVERSE );
            }
            if( !strcmp( p_playlist->pp_items[i_item]->psz_name, p_playlist->pp_items[i_item]->psz_uri ) )
            {
                mvnprintw( y++, 1, COLS - 2, "%c %d - '%s'",
                           c,
                           i_item,
                           p_playlist->pp_items[i_item]->psz_uri );
            }
            else
            {
                mvnprintw( y++, 1, COLS - 2, "%c %d - '%s' (%s)",
                          c,
                          i_item,
                          p_playlist->pp_items[i_item]->psz_uri,
                          p_playlist->pp_items[i_item]->psz_name );
            }
            if( b_selected )
            {
                attroff ( A_REVERSE );
            }
        }
    }
    else
    {
        y++;
    }

    while( y < y_end )
    {
        DrawEmptyLine( p_sys->w, y++, 1, COLS - 2 );
    }

    refresh();

    *t_last_refresh = time( 0 );
}

static void Eject ( intf_thread_t *p_intf )
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

    psz_name = p_playlist->pp_items[ p_playlist->i_index ]->psz_name;

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

static void PlayPause ( intf_thread_t *p_intf )
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

    if(  w > 3 && h > 2 )
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
        attroff ( A_REVERSE );
    }
}
