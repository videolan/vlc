/*****************************************************************************
 * ncurses.c : NCurses plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ncurses.c,v 1.16 2002/06/01 12:32:00 sam Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>
#include <time.h>

#include <curses.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void intf_getfunctions ( function_list_t * );
static int  intf_Open         ( intf_thread_t * );
static void intf_Close        ( intf_thread_t * );
static void intf_Run          ( intf_thread_t * );

static void FullScreen   ( intf_thread_t * );
static void Play         ( intf_thread_t * );
static void Stop         ( intf_thread_t * );
static void Next           ( intf_thread_t * );
static void Eject          ( intf_thread_t * );
static void Pause          ( intf_thread_t * );
static void PrevTitle      ( intf_thread_t * );
static void NextTitle      ( intf_thread_t * );
static void PrevChapter    ( intf_thread_t * );
static void NextChapter    ( intf_thread_t * );

static int  HandleKey      ( intf_thread_t *, int );
static void Redraw           ( intf_thread_t *, time_t * );
static int  PrintFullLine  ( const char *p_fmt, ... );
static void ManageSlider   ( intf_thread_t * );

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("ncurses interface module") )
    ADD_CAPABILITY( INTF, 10 )
    ADD_SHORTCUT( "curses" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    intf_getfunctions( &p_module->p_functions->intf );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * intf_sys_t: description and status of ncurses interface
 *****************************************************************************/
struct intf_sys_s
{
    /* special actions */
    vlc_mutex_t         change_lock;                      /* the change lock */

    float               f_slider_state;
    float               f_slider_state_old;
};

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void intf_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Open: initialize and create window
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return( 1 );
    }

    memset ( p_intf->p_sys, 0, sizeof ( intf_sys_t ) );

    /* Initialize the curses library */
    initscr();
    /* Don't do NL -> CR/NL */
    nonl();
    /* Take input chars one at a time */
    cbreak();
    /* Don't echo */
    noecho();

    curs_set(0);
    timeout(0);

    clear();

    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface window
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Close the ncurses interface */
    endwin();

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: ncurses thread
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    signed char i_key;
    time_t t_last_refresh;

    /*
     * force drawing the interface for the first time
     */
    t_last_refresh = ( time( 0 ) - 1);

    while( !p_intf->p_vlc->b_die )
    {
        p_intf->pf_manage( p_intf );

        msleep( INTF_IDLE_SLEEP );

        while( (i_key = getch()) != -1 )
        {
            /*
             * HandleKey returns 1 if the screen needs to be redrawn
             */
            if ( HandleKey( p_intf, i_key ) )
            {
                Redraw( p_intf, &t_last_refresh );
            }
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
    switch( i_key )
    {
        case 'q':
        case 'Q':
            p_intf->b_die = 1;
            return 0;

        case 'f':
            FullScreen( p_intf );
            return 1;

        case 'p':
            Play( p_intf );
            return 1;

        case ' ':
            Pause( p_intf );
            return 1;

        case 's':
            Stop( p_intf );
            return 1;

        case 'n':
            Next( p_intf );
            return 1;

        case 'e':
            Eject( p_intf );
            return 1;

        case '[':
            PrevTitle( p_intf );
            break;

        case ']':
            NextTitle( p_intf );
            break;

        case '<':
            PrevChapter( p_intf );
            break;

        case '>':
            NextChapter( p_intf );
            break;

        case KEY_RIGHT:
            p_intf->p_sys->f_slider_state += 100;
            ManageSlider ( p_intf );
            break;

        case KEY_LEFT:
            p_intf->p_sys->f_slider_state--;
            ManageSlider ( p_intf );
            break;

        /*
         * ^l should clear and redraw the screen
         */
        case 0x0c:
            clear();
            return 1;

        default:
            break;
    }

    return 0;
}

static int PrintFullLine ( const char *p_fmt, ... )
{
    va_list  vl_args;
    char *    p_buf        = NULL;
    int       i_len;

    va_start ( vl_args, p_fmt );
    vasprintf ( &p_buf, p_fmt, vl_args );
    va_end ( vl_args );

    if ( p_buf == NULL )
    {
//X        msg_Err( p_input, "intf error: %s", strerror ( ENOMEM ) );
        return ( -1 );
    }

    i_len = strlen( p_buf );

    /*
     * make sure we don't exceed the border on the right side
     */
    if ( i_len > COLS )
    {
        p_buf[COLS] = '\0';
        i_len = COLS;
        printw( "%s", p_buf );
    }
    else
    {
        printw( "%s", p_buf );
        hline( ' ', COLS - i_len );
    }

    free ( p_buf );

    return i_len;
}

static void
Redraw ( intf_thread_t *p_intf, time_t *t_last_refresh )
{
    int row = 0;

    move ( row, 0 );

    attrset ( A_REVERSE );
    PrintFullLine( VOUT_TITLE " (ncurses interface)" );
    attroff ( A_REVERSE );

    row++;

    row++;
    move ( row, 0 );

    if ( p_intf->p_vlc->p_input_bank->pp_input[0] != NULL )
    {
        PrintFullLine ( " DVD Chapter:%3d     DVD Title:%3d",
            p_intf->p_vlc->p_input_bank->pp_input[0]->stream.p_selected_area->i_part,
            p_intf->p_vlc->p_input_bank->pp_input[0]->stream.p_selected_area->i_id );
    }

    row++;
    mvaddch ( row, 0, ACS_ULCORNER );
    mvhline ( row, 1, ACS_HLINE, COLS-2 );
    mvaddch ( row, COLS-1, ACS_URCORNER );

    row++;
    mvaddch ( row, 0, ACS_VLINE );
    attrset ( A_REVERSE );
    mvhline ( row, 1, ' ', ( (int) p_intf->p_sys->f_slider_state % COLS-2) );
    attroff ( A_REVERSE );
    mvaddch ( row, COLS-1, ACS_VLINE );

    row++;
    mvaddch ( row, 0, ACS_LLCORNER );
    mvhline ( row, 1, ACS_HLINE, COLS-2 );
    mvaddch ( row, COLS-1, ACS_LRCORNER );

    refresh();

    *t_last_refresh = time( 0 );
}

static void FullScreen( intf_thread_t *p_intf )
{
    vlc_mutex_lock( &p_intf->p_vlc->p_vout_bank->pp_vout[0]->change_lock );

    p_intf->p_vlc->p_vout_bank->pp_vout[0]->i_changes |= VOUT_FULLSCREEN_CHANGE;

    vlc_mutex_unlock( &p_intf->p_vlc->p_vout_bank->pp_vout[0]->change_lock );
}

static void Eject ( intf_thread_t *p_intf )
{
    char *psz_device = NULL;
    char *psz_parser;

    /*
     * Get the active input
     * Determine whether we can eject a media, ie it's a VCD or DVD
     * If it's neither a VCD nor a DVD, then return
     */

    /*
     * Don't really know if I must lock the stuff here, we're using it read-only
     */

    if( p_intf->p_vlc->p_playlist->current.psz_name != NULL)
    {
        if( !strncmp(p_intf->p_vlc->p_playlist->current.psz_name, "dvd:", 4) )
        {
            switch( p_intf->p_vlc->p_playlist->current.psz_name[4] )
            {
            case '\0':
            case '@':
                psz_device = config_GetPsz( p_intf, "dvd_device" );
                break;
            default:
                /* Omit the first 4 characters */
                psz_device = strdup( p_intf->p_vlc->p_playlist->current.psz_name + 4 );
                break;
            }
        }
        else if( !strncmp(p_intf->p_vlc->p_playlist->current.psz_name, "vcd:", 4) )
        {
            switch( p_intf->p_vlc->p_playlist->current.psz_name[4] )
            {
            case '\0':
            case '@':
                psz_device = config_GetPsz( p_intf, "vcd_device" );
                break;
            default:
                /* Omit the first 4 characters */
                psz_device = strdup( p_intf->p_vlc->p_playlist->current.psz_name + 4 );
                break;
            }
        }
        else
        {
            psz_device = strdup( p_intf->p_vlc->p_playlist->current.psz_name );
        }
    }

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
    if( p_intf->p_vlc->p_input_bank->pp_input[0] == NULL )
    {
//X        msg_Dbg( p_input, "ejecting %s", psz_device );

        intf_Eject( p_intf->p_this, psz_device );
    }

    free(psz_device);
    return;
}

static void Play ( intf_thread_t *p_intf )
{
    if( p_intf->p_vlc->p_input_bank->pp_input[0] != NULL )
    {
        input_SetStatus( p_intf->p_vlc->p_input_bank->pp_input[0], INPUT_STATUS_PLAY );
        p_intf->p_vlc->p_playlist->b_stopped = 0;
    }
    else
    {
        vlc_mutex_lock( &p_intf->p_vlc->p_playlist->change_lock );

        if( p_intf->p_vlc->p_playlist->b_stopped )
        {
            if( p_intf->p_vlc->p_playlist->i_size )
            {
                vlc_mutex_unlock( &p_intf->p_vlc->p_playlist->change_lock );
                intf_PlaylistJumpto( p_intf->p_vlc->p_playlist,
                                     p_intf->p_vlc->p_playlist->i_index );
            }
            else
            {
                vlc_mutex_unlock( &p_intf->p_vlc->p_playlist->change_lock );
            }
        }
        else
        {

            vlc_mutex_unlock( &p_intf->p_vlc->p_playlist->change_lock );
        }

    }
}

static void Pause ( intf_thread_t *p_intf )
{
    if( p_intf->p_vlc->p_input_bank->pp_input[0] != NULL )
    {
        if ( p_intf->p_vlc->p_input_bank->pp_input[0]->i_status & INPUT_STATUS_PLAY )
        {
            input_SetStatus( p_intf->p_vlc->p_input_bank->pp_input[0], INPUT_STATUS_PAUSE );

            vlc_mutex_lock( &p_intf->p_vlc->p_playlist->change_lock );
            p_intf->p_vlc->p_playlist->b_stopped = 0;
            vlc_mutex_unlock( &p_intf->p_vlc->p_playlist->change_lock );
        }
        else
        {
            Play ( p_intf );
        }
    }
}

static void Stop ( intf_thread_t *p_intf )
{
    if( p_intf->p_vlc->p_input_bank->pp_input[0] != NULL )
    {
        /* end playing item */
        p_intf->p_vlc->p_input_bank->pp_input[0]->b_eof = 1;

        /* update playlist */
        vlc_mutex_lock( &p_intf->p_vlc->p_playlist->change_lock );

        p_intf->p_vlc->p_playlist->i_index--;
        p_intf->p_vlc->p_playlist->b_stopped = 1;

        vlc_mutex_unlock( &p_intf->p_vlc->p_playlist->change_lock );

    }
}

static void Next ( intf_thread_t *p_intf )
{
    int i_id;
    input_area_t * p_area;

    i_id = p_intf->p_vlc->p_input_bank->pp_input[0]->stream.p_selected_area->i_id+1;

    if ( i_id < p_intf->p_vlc->p_input_bank->pp_input[0]->stream.i_area_nb )
    {
        p_area = p_intf->p_vlc->p_input_bank->pp_input[0]->stream.pp_areas[i_id];

        input_ChangeArea( p_intf->p_vlc->p_input_bank->pp_input[0],
                (input_area_t *) p_area );

        input_SetStatus( p_intf->p_vlc->p_input_bank->pp_input[0], INPUT_STATUS_PLAY );
    }
}

static void ManageSlider ( intf_thread_t *p_intf )
{
    if( p_intf->p_vlc->p_input_bank->pp_input[0] != NULL )
    {
        vlc_mutex_lock( &p_intf->p_vlc->p_input_bank->pp_input[0]->stream.stream_lock );

        if( p_intf->p_vlc->p_input_bank->pp_input[0]->stream.b_seekable &&
            p_intf->p_vlc->p_input_bank->pp_input[0]->i_status & INPUT_STATUS_PLAY )
        {
            float newvalue = p_intf->p_sys->f_slider_state;

#define p_area p_intf->p_vlc->p_input_bank->pp_input[0]->stream.p_selected_area

            /* If the user hasn't touched the slider since the last time,
             * then the input can safely change it */
            if( newvalue == p_intf->p_sys->f_slider_state_old )
            {
                /* Update the value */
                p_intf->p_sys->f_slider_state =
                    p_intf->p_sys->f_slider_state_old =
                    ( 100 * p_area->i_tell ) / p_area->i_size;
            }
            /* Otherwise, send message to the input if the user has
             * finished dragging the slider */
            else
            {
                off_t i_seek = ( newvalue * p_area->i_size ) / 100;

                /* release the lock to be able to seek */
                vlc_mutex_unlock( &p_intf->p_vlc->p_input_bank->pp_input[0]->stream.stream_lock );
                input_Seek( p_intf->p_vlc->p_input_bank->pp_input[0]->p_this, i_seek, INPUT_SEEK_SET );
                vlc_mutex_lock( &p_intf->p_vlc->p_input_bank->pp_input[0]->stream.stream_lock );

                /* Update the old value */
                p_intf->p_sys->f_slider_state_old = newvalue;
            }
#    undef p_area
        }

        vlc_mutex_unlock( &p_intf->p_vlc->p_input_bank->pp_input[0]->stream.stream_lock );
    }
}

static void PrevTitle ( intf_thread_t *p_intf )
{
    input_area_t *  p_area;
    int             i_id;

    i_id = p_intf->p_vlc->p_input_bank->pp_input[0]->stream.p_selected_area->i_id - 1;

    /* Disallow area 0 since it is used for video_ts.vob */
    if ( i_id > 0 )
    {
        p_area = p_intf->p_vlc->p_input_bank->pp_input[0]->stream.pp_areas[i_id];
        input_ChangeArea( p_intf->p_vlc->p_input_bank->pp_input[0], (input_area_t*)p_area );

        input_SetStatus( p_intf->p_vlc->p_input_bank->pp_input[0], INPUT_STATUS_PLAY );
    }
}

static void NextTitle ( intf_thread_t *p_intf )
{
    input_area_t *  p_area;
    int             i_id;

    i_id = p_intf->p_vlc->p_input_bank->pp_input[0]->stream.p_selected_area->i_id + 1;

    if ( i_id < p_intf->p_vlc->p_input_bank->pp_input[0]->stream.i_area_nb )
    {
        p_area = p_intf->p_vlc->p_input_bank->pp_input[0]->stream.pp_areas[i_id];
        input_ChangeArea( p_intf->p_vlc->p_input_bank->pp_input[0], (input_area_t*)p_area );

        input_SetStatus( p_intf->p_vlc->p_input_bank->pp_input[0], INPUT_STATUS_PLAY );
    }
}

static void PrevChapter ( intf_thread_t *p_intf )
{
    input_area_t *  p_area;

    p_area = p_intf->p_vlc->p_input_bank->pp_input[0]->stream.p_selected_area;

    if ( p_area->i_part > 0 )
    {
        p_area->i_part--;
        input_ChangeArea( p_intf->p_vlc->p_input_bank->pp_input[0], (input_area_t*)p_area );

        input_SetStatus( p_intf->p_vlc->p_input_bank->pp_input[0], INPUT_STATUS_PLAY );
    }
}

static void NextChapter( intf_thread_t *p_intf )
{
    input_area_t *  p_area;

    p_area = p_intf->p_vlc->p_input_bank->pp_input[0]->stream.p_selected_area;

    if ( p_area->i_part < p_area->i_part_nb )
    {
        p_area->i_part++;
        input_ChangeArea( p_intf->p_vlc->p_input_bank->pp_input[0], (input_area_t*)p_area );

        input_SetStatus( p_intf->p_vlc->p_input_bank->pp_input[0], INPUT_STATUS_PLAY );
    }
}

