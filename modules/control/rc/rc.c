/*****************************************************************************
 * rc.c : remote control stdin/stdout plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: rc.c,v 1.37 2003/07/28 07:16:50 fenrir Exp $
 *
 * Authors: Peter Surda <shurdeek@panorama.sth.ac.at>
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
#include <ctype.h>
#include <signal.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/aout.h>
#include <vlc/vout.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <sys/types.h>

#include "vlc_error.h"

#define MAX_LINE_LENGTH 256

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void Run          ( intf_thread_t *p_intf );

static int  Input        ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Playlist     ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Quit         ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Intf         ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Volume       ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  VolumeMove   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  AudioConfig  ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define POS_TEXT N_("Show stream position")
#define POS_LONGTEXT N_("Show the current position in seconds within the stream from time to time.")

#define TTY_TEXT N_("Fake TTY")
#define TTY_LONGTEXT N_("Force the rc plugin to use stdin as if it was a TTY.")

vlc_module_begin();
    add_category_hint( N_("Remote control"), NULL, VLC_TRUE );
    add_bool( "rc-show-pos", 0, NULL, POS_TEXT, POS_LONGTEXT, VLC_TRUE );
#ifdef HAVE_ISATTY
    add_bool( "fake-tty", 0, NULL, TTY_TEXT, TTY_LONGTEXT, VLC_TRUE );
#endif
    set_description( _("remote control interface") );
    set_capability( "interface", 20 );
    set_callbacks( Activate, NULL );
vlc_module_end();

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;

#if defined(HAVE_ISATTY) && !defined(WIN32)
    /* Check that stdin is a TTY */
    if( !config_GetInt( p_intf, "fake-tty" ) && !isatty( 0 ) )
    {
        msg_Warn( p_intf, "fd 0 is not a TTY" );
        return VLC_EGENERIC;
    }
#endif

    /* Non-buffered stdout */
    setvbuf( stdout, (char *)NULL, _IOLBF, 0 );

    p_intf->pf_run = Run;

    CONSOLE_INTRO_MSG;

    printf( "remote control interface initialized, `h' for help\n" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Run: rc thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    input_thread_t * p_input;
    playlist_t *     p_playlist;

    char       p_buffer[ MAX_LINE_LENGTH + 1 ];
    vlc_bool_t b_showpos = config_GetInt( p_intf, "rc-show-pos" );
    input_info_category_t * p_category;
    input_info_t * p_info;

    int        i_dummy;
    off_t      i_oldpos = 0;
    off_t      i_newpos;

    double     f_ratio = 1.0;

#ifdef WIN32
    HANDLE hConsoleIn;
    INPUT_RECORD input_record;
    DWORD i_dummy2;
#endif

    p_input = NULL;
    p_playlist = NULL;

    /* Register commands that will be cleaned up upon object destruction */
    var_Create( p_intf, "quit", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "quit", Quit, NULL );
    var_Create( p_intf, "intf", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "intf", Intf, NULL );

    var_Create( p_intf, "add", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "add", Playlist, NULL );
    var_Create( p_intf, "playlist", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "playlist", Playlist, NULL );
    var_Create( p_intf, "play", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "play", Playlist, NULL );
    var_Create( p_intf, "stop", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "stop", Playlist, NULL );
    var_Create( p_intf, "prev", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "prev", Playlist, NULL );
    var_Create( p_intf, "next", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "next", Playlist, NULL );

    var_Create( p_intf, "pause", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "pause", Input, NULL );
    var_Create( p_intf, "seek", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "seek", Input, NULL );
    var_Create( p_intf, "title", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "title", Input, NULL );
    var_Create( p_intf, "title_n", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "title_n", Input, NULL );
    var_Create( p_intf, "title_p", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "title_p", Input, NULL );
    var_Create( p_intf, "chapter", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "chapter", Input, NULL );
    var_Create( p_intf, "chapter_n", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "chapter_n", Input, NULL );
    var_Create( p_intf, "chapter_p", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "chapter_p", Input, NULL );

    var_Create( p_intf, "volume", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "volume", Volume, NULL );
    var_Create( p_intf, "volup", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "volup", VolumeMove, NULL );
    var_Create( p_intf, "voldown", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "voldown", VolumeMove, NULL );
    var_Create( p_intf, "adev", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "adev", AudioConfig, NULL );
    var_Create( p_intf, "achan", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "achan", AudioConfig, NULL );

#ifdef WIN32
    /* Get the file descriptor of the console input */
    hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    if( hConsoleIn == INVALID_HANDLE_VALUE )
    {
        msg_Err( p_intf, "Couldn't open STD_INPUT_HANDLE" ); 
        p_intf->b_die = VLC_TRUE;
    }
#endif

    while( !p_intf->b_die )
    {
        vlc_bool_t     b_complete = VLC_FALSE;

#ifndef WIN32
        fd_set         fds;
        struct timeval tv;

        /* Check stdin */
        tv.tv_sec = 0;
        tv.tv_usec = (long)INTF_IDLE_SLEEP;
        FD_ZERO( &fds );
        FD_SET( STDIN_FILENO, &fds );

        i_dummy = select( STDIN_FILENO + 1, &fds, NULL, NULL, &tv );
#else
        /* On Win32, select() only works on socket descriptors */
        i_dummy = ( WaitForSingleObject( hConsoleIn, INTF_IDLE_SLEEP/1000 )
                    == WAIT_OBJECT_0 );
#endif
        if( i_dummy > 0 )
        {
            int i_size = 0;

            while( !p_intf->b_die
                    && i_size < MAX_LINE_LENGTH
#ifndef WIN32
                    && read( STDIN_FILENO, p_buffer + i_size, 1 ) > 0
#else
                    && ReadConsoleInput( hConsoleIn, &input_record, 1,
                                         &i_dummy2 )
#endif
                   )
            {
#ifdef WIN32
                if( input_record.EventType != KEY_EVENT ||
                    !input_record.Event.KeyEvent.bKeyDown ||
                    input_record.Event.KeyEvent.wVirtualKeyCode == VK_SHIFT ||
                    input_record.Event.KeyEvent.wVirtualKeyCode == VK_CONTROL||
                    input_record.Event.KeyEvent.wVirtualKeyCode == VK_MENU ||
                    input_record.Event.KeyEvent.wVirtualKeyCode == VK_CAPITAL )
                {
                    /* nothing interesting */
                    continue;
                }

                p_buffer[ i_size ] =
                    input_record.Event.KeyEvent.uChar.AsciiChar;

                /* Echo out the command */
                putc( p_buffer[ i_size ], stdout );

                /* Handle special keys */
                if( p_buffer[ i_size ] == '\r' || p_buffer[ i_size ] == '\n' )
                {
                    putc( '\n', stdout );
                    break;
                }
                switch( p_buffer[ i_size ] )
                {
                case '\b':
                    if( i_size )
                    {
                        i_size -= 2;
                        putc( ' ', stdout );
                        putc( '\b', stdout );
                    }
                    break;
                case '\r':
                    i_size --;
                    break;
                }

                i_size++;
#else

                if( p_buffer[ i_size ] == '\r' || p_buffer[ i_size ] == '\n' )
                {
                    break;
                }

                i_size++;
#endif
            }

            if( i_size == MAX_LINE_LENGTH
                 || p_buffer[ i_size ] == '\r'
                 || p_buffer[ i_size ] == '\n' )
            {
                p_buffer[ i_size ] = 0;
                b_complete = VLC_TRUE;
            }
        }

        /* Manage the input part */
        if( p_input == NULL )
        {
            if( p_playlist )
            {
                p_input = vlc_object_find( p_playlist, VLC_OBJECT_INPUT,
                                                       FIND_CHILD );
            }
            else
            {
                p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                   FIND_ANYWHERE );
                if( p_input )
                {
                    p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                                           FIND_PARENT );
                }
            }
        }
        else if( p_input->b_dead )
        {
            vlc_object_release( p_input );
            p_input = NULL;
        }

        if( p_input && b_showpos )
        {
            /* Get position */
            vlc_mutex_lock( &p_input->stream.stream_lock );
            if( !p_input->b_die && p_input->stream.i_mux_rate )
            {
#define A p_input->stream.p_selected_area
                f_ratio = 1.0 / ( 50 * p_input->stream.i_mux_rate );
                i_newpos = A->i_tell * f_ratio;

                if( i_oldpos != i_newpos )
                {
                    i_oldpos = i_newpos;
                    printf( "pos: %li s / %li s\n", (long int)i_newpos,
                            (long int)(f_ratio * A->i_size) );
                }
#undef S
            }
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        /* Is there something to do? */
        if( b_complete )
        {
            char *psz_cmd, *psz_arg;

            /* Skip heading spaces */
            psz_cmd = p_buffer;
            while( *psz_cmd == ' ' )
            {
                psz_cmd++;
            }

            /* Split psz_cmd at the first space and make sure that
             * psz_arg is valid */
            psz_arg = strchr( psz_cmd, ' ' );
            if( psz_arg )
            {
                *psz_arg++ = 0;
                while( *psz_arg == ' ' )
                {
                    psz_arg++;
                }
            }
            else
            {
                psz_arg = "";
            }

            /* If the user typed a registered local command, try it */
            if( var_Type( p_intf, psz_cmd ) & VLC_VAR_ISCOMMAND )
            {
                vlc_value_t val;
                int i_ret;

                val.psz_string = psz_arg;
                i_ret = var_Set( p_intf, psz_cmd, val );
                printf( "%s: returned %i (%s)\n",
                        psz_cmd, i_ret, vlc_error( i_ret ) );
            }
            /* Or maybe it's a global command */
            else if( var_Type( p_intf->p_libvlc, psz_cmd ) & VLC_VAR_ISCOMMAND )
            {
                vlc_value_t val;
                int i_ret;

                val.psz_string = psz_arg;
                /* FIXME: it's a global command, but we should pass the
                 * local object as an argument, not p_intf->p_libvlc. */
                i_ret = var_Set( p_intf->p_libvlc, psz_cmd, val );
                printf( "%s: returned %i (%s)\n",
                        psz_cmd, i_ret, vlc_error( i_ret ) );
            }
            else if( !strcmp( psz_cmd, "info" ) )
            {
                if ( p_input )
                {
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    p_category = p_input->stream.p_info;
                    while ( p_category )
                    {
                        printf( "+----[ %s ]\n", p_category->psz_name );
                        printf( "| \n" );
                        p_info = p_category->p_info;
                        while ( p_info )
                        {
                            printf( "| %s: %s\n", p_info->psz_name,
                                    p_info->psz_value );
                            p_info = p_info->p_next;
                        }
                        p_category = p_category->p_next;
                        printf( "| \n" );
                    }
                    printf( "+----[ end of stream info ]\n" );
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                }
                else
                {
                    printf( "no input\n" );
                }
            }
            else switch( psz_cmd[0] )
            {
            case 'f':
            case 'F':
                if( p_input )
                {
                    vout_thread_t *p_vout;
                    p_vout = vlc_object_find( p_input,
                                              VLC_OBJECT_VOUT, FIND_CHILD );

                    if( p_vout )
                    {
                        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                        vlc_object_release( p_vout );
                    }
                }
                break;

            case 's':
            case 'S':
                ;
                break;

            case '?':
            case 'h':
            case 'H':
                printf("+----[ remote control commands ]\n");
                printf("| \n");
                printf("| add XYZ  . . . . . . . . . . add XYZ to playlist\n");
                printf("| playlist . . .  show items currently in playlist\n");
                printf("| play . . . . . . . . . . . . . . . . play stream\n");
                printf("| stop . . . . . . . . . . . . . . . . stop stream\n");
                printf("| next . . . . . . . . . . . .  next playlist item\n");
                printf("| prev . . . . . . . . . .  previous playlist item\n");
                printf("| title [X]  . . . . set/get title in current item\n");
                printf("| title_n  . . . . . .  next title in current item\n");
                printf("| title_p  . . . .  previous title in current item\n");
                printf("| chapter [X]  . . set/get chapter in current item\n");
                printf("| chapter_n  . . . .  next chapter in current item\n");
                printf("| chapter_p  . .  previous chapter in current item\n");
                printf("| \n");
                printf("| seek X . seek in seconds, for instance `seek 12'\n");
                printf("| pause  . . . . . . . . . . . . . .  toggle pause\n");
                printf("| f  . . . . . . . . . . . . . . toggle fullscreen\n");
                printf("| info . . .  information about the current stream\n");
                printf("| \n");
                printf("| volume [X] . . . . . . . .  set/get audio volume\n");
                printf("| volup [X]  . . . . .  raise audio volume X steps\n");
                printf("| voldown [X]  . . . .  lower audio volume X steps\n");
                printf("| adev [X] . . . . . . . . .  set/get audio device\n");
                printf("| achan [X]. . . . . . . .  set/get audio channels\n");
                printf("| \n");
                printf("| help . . . . . . . . . . . . . this help message\n");
                printf("| quit . . . . . . . . . . . . . . . . .  quit vlc\n");
                printf("| \n");
                printf("+----[ end of help ]\n");
                break;
            case '\0':
                /* Ignore empty lines */
                break;
            default:
                printf( "unknown command `%s', type `help' for help\n", psz_cmd );
                break;
            }
        }
    }

    if( p_input )
    {
        vlc_object_release( p_input );
        p_input = NULL;
    }

    if( p_playlist )
    {
        vlc_object_release( p_playlist );
        p_playlist = NULL;
    }
}

static int Input( vlc_object_t *p_this, char const *psz_cmd,
                  vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t * p_input;

    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_ANYWHERE );

    if( !p_input )
    {
        return VLC_ENOOBJ;
    }

    /* Parse commands that only require an input */
    if( !strcmp( psz_cmd, "pause" ) )
    {
        input_SetStatus( p_input, INPUT_STATUS_PAUSE );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if( !strcmp( psz_cmd, "seek" ) )
    {
        if( strlen( newval.psz_string ) > 0 &&
            newval.psz_string[strlen( newval.psz_string ) - 1] == '%' )
        {
            input_Seek( p_input, atoi( newval.psz_string ),
                        INPUT_SEEK_PERCENT | INPUT_SEEK_SET );
        }
        else
        {
            input_Seek( p_input, atoi( newval.psz_string ),
                        INPUT_SEEK_SECONDS | INPUT_SEEK_SET );
        }
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if( !strcmp( psz_cmd, "chapter" ) ||
             !strcmp( psz_cmd, "chapter_n" ) ||
             !strcmp( psz_cmd, "chapter_p" ) )
    {
        unsigned int i_chapter = 0;

        if( !strcmp( psz_cmd, "chapter" ) )
        {
            if ( *newval.psz_string )
            {
                /* Set. */
                i_chapter = atoi( newval.psz_string );
            }
            else
            {
                /* Get. */
                vlc_mutex_lock( &p_input->stream.stream_lock );
                printf( "Currently playing chapter %d/%d\n",
                        p_input->stream.p_selected_area->i_part,
                        p_input->stream.p_selected_area->i_part_nb - 1 );
                vlc_mutex_unlock( &p_input->stream.stream_lock );

                vlc_object_release( p_input );
                return VLC_SUCCESS;
            }
        }
        else if( !strcmp( psz_cmd, "chapter_n" ) )
        {
            vlc_mutex_lock( &p_input->stream.stream_lock );
            i_chapter = p_input->stream.p_selected_area->i_part + 1;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
        else if( !strcmp( psz_cmd, "chapter_p" ) )
        {
            vlc_mutex_lock( &p_input->stream.stream_lock );
            i_chapter = p_input->stream.p_selected_area->i_part - 1;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        vlc_mutex_lock( &p_input->stream.stream_lock );
        if( ( i_chapter > 0 ) && ( i_chapter <
            p_input->stream.p_selected_area->i_part_nb ) )
        {
            input_area_t *p_area = p_input->stream.p_selected_area;
            p_input->stream.p_selected_area->i_part = i_chapter;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            input_ChangeArea( p_input, p_area );
            input_SetStatus( p_input, INPUT_STATUS_PLAY );
            vlc_mutex_lock( &p_input->stream.stream_lock );
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if( !strcmp( psz_cmd, "title" ) ||
             !strcmp( psz_cmd, "title_n" ) ||
             !strcmp( psz_cmd, "title_p" ) )
    {
        unsigned int i_title = 0;

        if( !strcmp( psz_cmd, "title" ) )
        {
            if ( *newval.psz_string )
            {
                /* Set. */
                i_title = atoi( newval.psz_string );
            }
            else
            {
                /* Get. */
                vlc_mutex_lock( &p_input->stream.stream_lock );
                printf( "Currently playing title %d/%d\n",
                        p_input->stream.p_selected_area->i_id,
                        p_input->stream.i_area_nb - 1 );
                vlc_mutex_unlock( &p_input->stream.stream_lock );

                vlc_object_release( p_input );
                return VLC_SUCCESS;
            }
        }
        else if( !strcmp( psz_cmd, "title_n" ) )
        {
            vlc_mutex_lock( &p_input->stream.stream_lock );
            i_title = p_input->stream.p_selected_area->i_id + 1;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
        else if( !strcmp( psz_cmd, "title_p" ) )
        {
            vlc_mutex_lock( &p_input->stream.stream_lock );
            i_title = p_input->stream.p_selected_area->i_id - 1;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        vlc_mutex_lock( &p_input->stream.stream_lock );
        if( ( i_title > 0 ) && ( i_title < p_input->stream.i_area_nb ) )
        {
            input_area_t *p_area = p_input->stream.pp_areas[i_title];
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            input_ChangeArea( p_input, p_area );
            input_SetStatus( p_input, INPUT_STATUS_PLAY );
            vlc_mutex_lock( &p_input->stream.stream_lock );
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    /* Never reached. */
    return VLC_EGENERIC;
}

static int Playlist( vlc_object_t *p_this, char const *psz_cmd,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    playlist_t *     p_playlist;

    p_playlist = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
    if( !p_playlist )
    {
        return VLC_ENOOBJ;
    }

    /* Parse commands that require a playlist */
    if( !strcmp( psz_cmd, "prev" ) )
    {
        playlist_Prev( p_playlist );
    }
    else if( !strcmp( psz_cmd, "next" ) )
    {
        playlist_Next( p_playlist );
    }
    else if( !strcmp( psz_cmd, "play" ) )
    {
        playlist_Play( p_playlist );
    }
    else if( !strcmp( psz_cmd, "stop" ) )
    {
        playlist_Stop( p_playlist );
    }
    else if( !strcmp( psz_cmd, "add" ) )
    {
        printf( "trying to add %s to playlist\n", newval.psz_string );
        playlist_Add( p_playlist, newval.psz_string, NULL, 0,
                      PLAYLIST_GO|PLAYLIST_APPEND, PLAYLIST_END );
    }
    else if( !strcmp( psz_cmd, "playlist" ) )
    {
        int i;
        for ( i = 0; i < p_playlist->i_size; i++ )
        {
            printf( "|%s%s   %s|\n", i == p_playlist->i_index?"*":" ",
                    p_playlist->pp_items[i]->psz_name,
                    p_playlist->pp_items[i]->psz_uri );
        }
        if ( i == 0 )
        {
            printf( "| no entries\n" );
        }
    }
    /*
     * sanity check
     */
    else
    {
        printf( "unknown command!\n" );
    }

    vlc_object_release( p_playlist );
    return VLC_SUCCESS;
}

static int Quit( vlc_object_t *p_this, char const *psz_cmd,
                 vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    p_this->p_vlc->b_die = VLC_TRUE;
    return VLC_SUCCESS;
}

static int Intf( vlc_object_t *p_this, char const *psz_cmd,
                 vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_newintf;

    p_newintf = intf_Create( p_this->p_vlc, newval.psz_string );

    if( p_newintf )
    {
        p_newintf->b_block = VLC_FALSE;
        if( intf_RunThread( p_newintf ) )
        {
            vlc_object_detach( p_newintf );
            intf_Destroy( p_newintf );
        }
    }

    return VLC_SUCCESS;
}

static int Volume( vlc_object_t *p_this, char const *psz_cmd,
                   vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    int i_error;

    if ( *newval.psz_string )
    {
        /* Set. */
        audio_volume_t i_volume = atoi( newval.psz_string );
        if ( i_volume > AOUT_VOLUME_MAX )
        {
            printf( "Volume must be in the range %d-%d\n", AOUT_VOLUME_MIN,
                    AOUT_VOLUME_MAX );
            i_error = VLC_EBADVAR;
        }
        else i_error = aout_VolumeSet( p_this, i_volume );
    }
    else
    {
        /* Get. */
        audio_volume_t i_volume;
        if ( aout_VolumeGet( p_this, &i_volume ) < 0 )
        {
            i_error = VLC_EGENERIC;
        }
        else
        {
            printf( "Volume is %d\n", i_volume );
            i_error = VLC_SUCCESS;
        }
    }

    return i_error;
}

static int VolumeMove( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    audio_volume_t i_volume;
    int i_nb_steps = atoi(newval.psz_string);
    int i_error = VLC_SUCCESS;

    if ( i_nb_steps <= 0 || i_nb_steps > (AOUT_VOLUME_MAX/AOUT_VOLUME_STEP) )
    {
        i_nb_steps = 1;
    }

    if ( !strcmp(psz_cmd, "volup") )
    {
        if ( aout_VolumeUp( p_this, i_nb_steps, &i_volume ) < 0 )
            i_error = VLC_EGENERIC;
    }
    else
    {
        if ( aout_VolumeDown( p_this, i_nb_steps, &i_volume ) < 0 )
            i_error = VLC_EGENERIC;
    }

    if ( !i_error ) printf( "Volume is %d\n", i_volume );
    return i_error;
}

static int AudioConfig( vlc_object_t *p_this, char const *psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    aout_instance_t * p_aout;
    const char * psz_variable;
    vlc_value_t val_name;
    int i_error;

    p_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT, FIND_ANYWHERE );
    if ( p_aout == NULL ) return VLC_ENOOBJ;

    if ( !strcmp( psz_cmd, "adev" ) )
    {
        psz_variable = "audio-device";
    }
    else
    {
        psz_variable = "audio-channels";
    }

    /* Get the descriptive name of the variable */
    var_Change( (vlc_object_t *)p_aout, psz_variable, VLC_VAR_GETTEXT,
                 &val_name, NULL );
    if( !val_name.psz_string ) val_name.psz_string = strdup(psz_variable);

    if ( !*newval.psz_string )
    {
        /* Retrieve all registered ***. */
        vlc_value_t val, text;
        int i, i_value;

        if ( var_Get( (vlc_object_t *)p_aout, psz_variable, &val ) < 0 )
        {
            vlc_object_release( (vlc_object_t *)p_aout );
            return VLC_EGENERIC;
        }
        i_value = val.i_int;

        if ( var_Change( (vlc_object_t *)p_aout, psz_variable,
                         VLC_VAR_GETLIST, &val, &text ) < 0 )
        {
            vlc_object_release( (vlc_object_t *)p_aout );
            return VLC_EGENERIC;
        }

        printf( "+----[ %s ]\n", val_name.psz_string );
        for ( i = 0; i < val.p_list->i_count; i++ )
        {
            if ( i_value == val.p_list->p_values[i].i_int )
                printf( "| %i - %s *\n", val.p_list->p_values[i].i_int,
			text.p_list->p_values[i].psz_string );
            else
                printf( "| %i - %s\n", val.p_list->p_values[i].i_int,
			text.p_list->p_values[i].psz_string );
        }
        var_Change( (vlc_object_t *)p_aout, psz_variable, VLC_VAR_FREELIST,
                    &val, &text );
        printf( "+----[ end of %s ]\n", val_name.psz_string );

        if( val_name.psz_string ) free( val_name.psz_string );
        i_error = VLC_SUCCESS;
    }
    else
    {
        vlc_value_t val;
        val.i_int = atoi( newval.psz_string );

        i_error = var_Set( (vlc_object_t *)p_aout, psz_variable, val );
    }
    vlc_object_release( (vlc_object_t *)p_aout );

    return i_error;
}
