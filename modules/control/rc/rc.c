/*****************************************************************************
 * rc.c : remote control stdin/stdout plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: rc.c,v 1.8 2002/10/11 22:32:56 sam Exp $
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
#include <vlc/vout.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <sys/types.h>

#if defined( WIN32 )
#include <winsock2.h>                                            /* select() */
#endif

#define MAX_LINE_LENGTH 256

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void Run          ( intf_thread_t *p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define POS_TEXT N_("show stream position")
#define POS_LONGTEXT N_("Show the current position in seconds within the stream from time to time.")

#define TTY_TEXT N_("fake TTY")
#define TTY_LONGTEXT N_("Force the rc plugin to use stdin as if it was a TTY.")

vlc_module_begin();
    add_category_hint( N_("Remote control"), NULL );
    add_bool( "rc-show-pos", 0, NULL, POS_TEXT, POS_LONGTEXT );
#ifdef HAVE_ISATTY
    add_bool( "fake-tty", 0, NULL, TTY_TEXT, TTY_LONGTEXT );
#endif
    set_description( _("remote control interface module") );
    set_capability( "interface", 20 );
    set_callbacks( Activate, NULL );
vlc_module_end();

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;

#ifdef HAVE_ISATTY
    /* Check that stdin is a TTY */
    if( !config_GetInt( p_intf, "fake-tty" ) && !isatty( 0 ) )
    {
        msg_Warn( p_intf, "fd 0 is not a TTY" );
        return 1;
    }
#endif

    /* Non-buffered stdout */
    setvbuf( stdout, (char *)NULL, _IOLBF, 0 );

    p_intf->pf_run = Run;

    CONSOLE_INTRO_MSG;

    printf( "remote control interface initialized, `h' for help\n" );
    return 0;
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
    vlc_bool_t b_complete = 0;
    vlc_bool_t b_showpos = config_GetInt( p_intf, "rc-show-pos" );
    input_info_category_t * p_category;
    input_info_t * p_info;

    int        i_dummy;
    off_t      i_oldpos = 0;
    off_t      i_newpos;
    fd_set     fds;                                        /* stdin changed? */
    struct timeval tv;                                   /* how long to wait */

    double     f_ratio = 1;

    p_input = NULL;
    p_playlist = NULL;

    var_Create( p_intf, "foo", VLC_VAR_STRING );
    var_Set( p_intf, "foo", (vlc_value_t)"test" );

    while( !p_intf->b_die )
    {
        b_complete = 0;

        /* Check stdin */
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        FD_ZERO( &fds );
        FD_SET( STDIN_FILENO, &fds );

        i_dummy = select( 32, &fds, NULL, NULL, &tv );
        if( i_dummy > 0 )
        {
            int i_size = 0;

            while( !p_intf->b_die
                    && i_size < MAX_LINE_LENGTH
                    && read( STDIN_FILENO, p_buffer + i_size, 1 ) > 0
                    && p_buffer[ i_size ] != '\r'
                    && p_buffer[ i_size ] != '\n' )
            {
                i_size++;
            }

            if( i_size == MAX_LINE_LENGTH
                 || p_buffer[ i_size ] == '\r'
                 || p_buffer[ i_size ] == '\n' )
            {
                p_buffer[ i_size ] = 0;
                b_complete = 1;
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
        if( b_complete == 1 )
        {
            char *p_cmd = p_buffer;

            if( !strcmp( p_cmd, "quit" ) )
            {
                p_intf->p_vlc->b_die = VLC_TRUE;
            }
            else if( !strcmp( p_cmd, "segfault" ) )
            {
                raise( SIGSEGV );
            }
            else if( !strcmp( p_cmd, "prev" ) )
            {
                if( p_playlist ) playlist_Prev( p_playlist );
            }
            else if( !strcmp( p_cmd, "next" ) )
            {
                if( p_playlist ) playlist_Next( p_playlist );
            }
            else if( !strcmp( p_cmd, "play" ) )
            {
                if( p_playlist ) playlist_Play( p_playlist );
            }
            else if( !strcmp( p_cmd, "stop" ) )
            {
                if( p_playlist ) playlist_Stop( p_playlist );
            }
            else if( !strcmp( p_cmd, "pause" ) )
            {
                if( p_input ) input_SetStatus( p_input, INPUT_STATUS_PAUSE );
            }
            else if( !strcmp( p_cmd, "tree" ) )
            {
                vlc_dumpstructure( p_intf->p_vlc );
            }
            else if( !strcmp( p_cmd, "list" ) )
            {
                vlc_liststructure( p_intf->p_vlc );
            }
            else if( !strncmp( p_cmd, "setfoo ", 7 ) )
            {
                vlc_value_t value;
                value.psz_string = p_cmd + 7;
                var_Set( p_intf, "foo", value );
            }
            else if( !strncmp( p_cmd, "getfoo", 6 ) )
            {
                vlc_value_t value;
                var_Get( p_intf, "foo", &value );
                printf( "current value is '%s'\n", value.psz_string );
            }
            else if( !strncmp( p_cmd, "intf ", 5 ) )
            {
                intf_thread_t *p_newintf;
                char *psz_oldmodule = config_GetPsz( p_intf->p_vlc, "intf" );

                config_PutPsz( p_intf->p_vlc, "intf", p_cmd + 5 );
                p_newintf = intf_Create( p_intf->p_vlc );
                config_PutPsz( p_intf->p_vlc, "intf", psz_oldmodule );

                if( psz_oldmodule )
                {
                    free( psz_oldmodule );
                }

                if( p_newintf )
                {
                    p_newintf->b_block = VLC_FALSE;
                    if( intf_RunThread( p_newintf ) )
                    {
                        vlc_object_detach( p_newintf );
                        intf_Destroy( p_newintf );
                    }
                }
            }
            else if( !strcmp( p_cmd, "info" ) )
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
            else switch( p_cmd[0] )
            {
            case 'a':
            case 'A':
                if( p_cmd[1] == ' ' && p_playlist )
                {
                    playlist_Add( p_playlist, p_cmd + 2,
                                  PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
                }
                break;

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

            case 'r':
            case 'R':
                if( p_input )
                {
                    for( i_dummy = 1;
                         i_dummy < MAX_LINE_LENGTH && p_cmd[ i_dummy ] >= '0'
                                                   && p_cmd[ i_dummy ] <= '9';
                         i_dummy++ )
                    {
                        ;
                    }

                    p_cmd[ i_dummy ] = 0;
                    input_Seek( p_input, (off_t)atoi( p_cmd + 1 ),
                                INPUT_SEEK_SECONDS | INPUT_SEEK_SET );
                    /* rcreseek(f_cpos); */
                }
                break;

            case '?':
            case 'h':
            case 'H':
                printf("+----[ remote control commands ]\n");
                printf("| \n");
                printf("| a XYZ  . . . . . . . . . . . add XYZ to playlist\n");
                printf("| play . . . . . . . . . . . . . . . . play stream\n");
                printf("| stop . . . . . . . . . . . . . . . . stop stream\n");
                printf("| next . . . . . . . . . . . .  next playlist item\n");
                printf("| prev . . . . . . . . . .  previous playlist item\n");
                printf("| \n");
                printf("| r X  . . . seek in seconds, for instance `r 3.5'\n");
                printf("| pause  . . . . . . . . . . . . . .  toggle pause\n");
                printf("| f  . . . . . . . . . . . . . . toggle fullscreen\n");
                printf("| info . . .  information about the current stream\n");
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
                printf( "unknown command `%s', type `help' for help\n", p_cmd );
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

    var_Destroy( p_intf, "foo" );
}

