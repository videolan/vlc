/*****************************************************************************
 * rc.c : remote control stdin/stdout plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: rc.c,v 1.14 2002/12/06 16:34:06 sam Exp $
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

#if defined( WIN32 )
#include <winsock2.h>                                            /* select() */
#endif

#include "error.h"

#define MAX_LINE_LENGTH 256

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void Run          ( intf_thread_t *p_intf );

static int  Playlist     ( vlc_object_t *, char *, char * );
static int  Quit         ( vlc_object_t *, char *, char * );
static int  Intf         ( vlc_object_t *, char *, char * );
static int  Volume       ( vlc_object_t *, char *, char * );
static int  VolumeMove   ( vlc_object_t *, char *, char * );
static int  AudioConfig  ( vlc_object_t *, char *, char * );

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

    p_input = NULL;
    p_playlist = NULL;

    /* Register commands that will be cleaned up upon object destruction */
    var_Create( p_intf, "quit", VLC_VAR_COMMAND );
    var_Set( p_intf, "quit", (vlc_value_t)(void*)Quit );
    var_Create( p_intf, "intf", VLC_VAR_COMMAND );
    var_Set( p_intf, "intf", (vlc_value_t)(void*)Intf );

    var_Create( p_intf, "play", VLC_VAR_COMMAND );
    var_Set( p_intf, "play", (vlc_value_t)(void*)Playlist );
    var_Create( p_intf, "stop", VLC_VAR_COMMAND );
    var_Set( p_intf, "stop", (vlc_value_t)(void*)Playlist );
    var_Create( p_intf, "pause", VLC_VAR_COMMAND );
    var_Set( p_intf, "pause", (vlc_value_t)(void*)Playlist );
    var_Create( p_intf, "seek", VLC_VAR_COMMAND );
    var_Set( p_intf, "seek", (vlc_value_t)(void*)Playlist );
    var_Create( p_intf, "prev", VLC_VAR_COMMAND );
    var_Set( p_intf, "prev", (vlc_value_t)(void*)Playlist );
    var_Create( p_intf, "next", VLC_VAR_COMMAND );
    var_Set( p_intf, "next", (vlc_value_t)(void*)Playlist );

    var_Create( p_intf, "title", VLC_VAR_COMMAND );
    var_Set( p_intf, "title", (vlc_value_t)(void*)Playlist );
    var_Create( p_intf, "title_n", VLC_VAR_COMMAND );
    var_Set( p_intf, "title_n", (vlc_value_t)(void*)Playlist );
    var_Create( p_intf, "title_p", VLC_VAR_COMMAND );
    var_Set( p_intf, "title_p", (vlc_value_t)(void*)Playlist );
    var_Create( p_intf, "chapter", VLC_VAR_COMMAND );
    var_Set( p_intf, "chapter", (vlc_value_t)(void*)Playlist );
    var_Create( p_intf, "chapter_n", VLC_VAR_COMMAND );
    var_Set( p_intf, "chapter_n", (vlc_value_t)(void*)Playlist );
    var_Create( p_intf, "chapter_p", VLC_VAR_COMMAND );
    var_Set( p_intf, "chapter_p", (vlc_value_t)(void*)Playlist );

    var_Create( p_intf, "volume", VLC_VAR_COMMAND );
    var_Set( p_intf, "volume", (vlc_value_t)(void*)Volume );
    var_Create( p_intf, "volup", VLC_VAR_COMMAND );
    var_Set( p_intf, "volup", (vlc_value_t)(void*)VolumeMove );
    var_Create( p_intf, "voldown", VLC_VAR_COMMAND );
    var_Set( p_intf, "voldown", (vlc_value_t)(void*)VolumeMove );
    var_Create( p_intf, "adev", VLC_VAR_COMMAND );
    var_Set( p_intf, "adev", (vlc_value_t)(void*)AudioConfig );
    var_Create( p_intf, "achan", VLC_VAR_COMMAND );
    var_Set( p_intf, "achan", (vlc_value_t)(void*)AudioConfig );

    while( !p_intf->b_die )
    {
        fd_set         fds;
        struct timeval tv;
        vlc_bool_t     b_complete = VLC_FALSE;

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
            if( var_Type( p_intf, psz_cmd ) == VLC_VAR_COMMAND )
            {
                vlc_value_t val;
                int i_ret;

                val.psz_string = psz_arg;
                i_ret = var_Get( p_intf, psz_cmd, &val );
                printf( "%s: returned %i (%s)\n",
                        psz_cmd, i_ret, vlc_error( i_ret ) );
            }
            /* Or maybe it's a global command */
            else if( var_Type( p_intf->p_libvlc, psz_cmd ) == VLC_VAR_COMMAND )
            {
                vlc_value_t val;
                int i_ret;

                val.psz_string = psz_arg;
                /* FIXME: it's a global command, but we should pass the
                 * local object as an argument, not p_intf->p_libvlc. */
                i_ret = var_Get( p_intf->p_libvlc, psz_cmd, &val );
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
            case 'a':
            case 'A':
                if( psz_cmd[1] == ' ' && p_playlist )
                {
                    playlist_Add( p_playlist, psz_cmd + 2,
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

static int Playlist( vlc_object_t *p_this, char *psz_cmd, char *psz_arg )
{
    input_thread_t * p_input;
    playlist_t *     p_playlist;

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
        input_Seek( p_input, atoi( psz_arg ),
                    INPUT_SEEK_SECONDS | INPUT_SEEK_SET );
    }
    else if( !strcmp( psz_cmd, "chapter" ) ||
             !strcmp( psz_cmd, "chapter_n" ) ||
             !strcmp( psz_cmd, "chapter_p" ) )
    {
        unsigned int i_chapter = 0;

        if( !strcmp( psz_cmd, "chapter" ) )
        {
            if ( *psz_arg )
            {
                /* Set. */
                i_chapter = atoi( psz_arg );
            }
            else
            {
                /* Get. */
                vlc_mutex_lock( &p_input->stream.stream_lock );
                printf( "Currently playing chapter %d\n",
                        p_input->stream.p_selected_area->i_part );
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
        if( ( i_chapter > 0 ) && ( i_chapter <=
            p_input->stream.p_selected_area->i_part_nb ) )
        {
          p_input->stream.p_selected_area->i_part = i_chapter;
          vlc_mutex_unlock( &p_input->stream.stream_lock );
          input_ChangeArea( p_input,
                            (input_area_t*)p_input->stream.p_selected_area );
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
            if ( *psz_arg )
            {
                /* Set. */
                i_title = atoi( psz_arg );
            }
            else
            {
                /* Get. */
                vlc_mutex_lock( &p_input->stream.stream_lock );
                printf( "Currently playing title %d\n",
                        p_input->stream.p_selected_area->i_id );
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
        if( ( i_title > 0 ) && ( i_title <=
            p_input->stream.p_selected_area->i_part_nb ) )
        {
          p_input->stream.p_selected_area->i_part = i_title;
          vlc_mutex_unlock( &p_input->stream.stream_lock );
          input_ChangeArea( p_input,
                            (input_area_t*)p_input->stream.pp_areas[i_title] );
          input_SetStatus( p_input, INPUT_STATUS_PLAY );
          vlc_mutex_lock( &p_input->stream.stream_lock );
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }


    p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                           FIND_PARENT );
    vlc_object_release( p_input );

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

    vlc_object_release( p_playlist );
    return VLC_SUCCESS;
}

static int Quit( vlc_object_t *p_this, char *psz_cmd, char *psz_arg )
{
    p_this->p_vlc->b_die = VLC_TRUE;
    return VLC_SUCCESS;
}

static int Intf( vlc_object_t *p_this, char *psz_cmd, char *psz_arg )
{
    intf_thread_t *p_newintf;
    char *psz_oldmodule = config_GetPsz( p_this->p_vlc, "intf" );

    config_PutPsz( p_this->p_vlc, "intf", psz_arg );
    p_newintf = intf_Create( p_this->p_vlc );
    config_PutPsz( p_this->p_vlc, "intf", psz_oldmodule );

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

    return VLC_SUCCESS;
}

static int Signal( vlc_object_t *p_this, char *psz_cmd, char *psz_arg )
{
    raise( atoi(psz_arg) );
    return VLC_SUCCESS;
}

static int Volume( vlc_object_t *p_this, char *psz_cmd, char *psz_arg )
{
    aout_instance_t * p_aout;
    int i_error;
    p_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT, FIND_ANYWHERE );
    if ( p_aout == NULL ) return VLC_ENOOBJ;

    if ( *psz_arg )
    {
        /* Set. */
        audio_volume_t i_volume = atoi( psz_arg );
        if ( i_volume > AOUT_VOLUME_MAX )
        {
            printf( "Volume must be in the range %d-%d\n", AOUT_VOLUME_MIN,
                    AOUT_VOLUME_MAX );
            i_error = VLC_EBADVAR;
        }
        else i_error = aout_VolumeSet( p_aout, i_volume );
    }
    else
    {
        /* Get. */
        audio_volume_t i_volume;
        if ( aout_VolumeGet( p_aout, &i_volume ) < 0 )
        {
            i_error = VLC_EGENERIC;
        }
        else
        {
            printf( "Volume is %d\n", i_volume );
            i_error = VLC_SUCCESS;
        }
    }
    vlc_object_release( (vlc_object_t *)p_aout );

    return i_error;
}

static int VolumeMove( vlc_object_t * p_this, char * psz_cmd, char * psz_arg )
{
    aout_instance_t * p_aout;
    audio_volume_t i_volume;
    int i_nb_steps = atoi(psz_arg);
    int i_error = VLC_SUCCESS;

    if ( i_nb_steps <= 0 || i_nb_steps > (AOUT_VOLUME_MAX/AOUT_VOLUME_STEP) )
    {
        i_nb_steps = 1;
    }

    p_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT, FIND_ANYWHERE );
    if ( p_aout == NULL ) return VLC_ENOOBJ;

    if ( !strcmp(psz_cmd, "volup") )
    {
        if ( aout_VolumeUp( p_aout, i_nb_steps, &i_volume ) < 0 )
            i_error = VLC_EGENERIC;
    }
    else
    {
        if ( aout_VolumeDown( p_aout, i_nb_steps, &i_volume ) < 0 )
            i_error = VLC_EGENERIC;
    }
    vlc_object_release( (vlc_object_t *)p_aout );

    if ( !i_error ) printf( "Volume is %d\n", i_volume );
    return i_error;
}

static int AudioConfig( vlc_object_t * p_this, char * psz_cmd, char * psz_arg )
{
    aout_instance_t * p_aout;
    const char * psz_variable;
    const char * psz_name;
    int i_error;

    p_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT, FIND_ANYWHERE );
    if ( p_aout == NULL ) return VLC_ENOOBJ;

    if ( !strcmp( psz_cmd, "adev" ) )
    {
        psz_variable = "audio-device";
        psz_name = "audio devices";
    }
    else
    {
        psz_variable = "audio-channels";
        psz_name = "audio channels";
    }

    if ( !*psz_arg )
    {
        /* Retrieve all registered ***. */
        vlc_value_t val;
        int i, i_vals;
        vlc_value_t * p_vals;
        char * psz_value;

        if ( var_Get( (vlc_object_t *)p_aout, psz_variable, &val ) < 0 )
        {
            vlc_object_release( (vlc_object_t *)p_aout );
            return VLC_EGENERIC;
        }
        psz_value = val.psz_string;

        if ( var_Change( (vlc_object_t *)p_aout, psz_variable,
                         VLC_VAR_GETLIST, &val ) < 0 )
        {
            free( psz_value );
            vlc_object_release( (vlc_object_t *)p_aout );
            return VLC_EGENERIC;
        }

        printf( "+----[ %s ]\n", psz_name );
        i_vals = ((vlc_value_t *)val.p_address)[0].i_int;
        p_vals = &((vlc_value_t *)val.p_address)[1]; /* Starts at index 1 */
        for ( i = 0; i < i_vals; i++ )
        {
            if ( !strcmp( psz_value, p_vals[i].psz_string ) )
                printf( "| %s *\n", p_vals[i].psz_string );
            else
                printf( "| %s\n", p_vals[i].psz_string );
        }
        var_Change( (vlc_object_t *)p_aout, psz_variable, VLC_VAR_FREELIST,
                    &val );
        printf( "+----[ end of %s ]\n", psz_name );

        free( psz_value );
        i_error = VLC_SUCCESS;
    }
    else
    {
        vlc_value_t val;
        val.psz_string = psz_arg;

        i_error = var_Set( (vlc_object_t *)p_aout, psz_variable, val );
    }
    vlc_object_release( (vlc_object_t *)p_aout );

    return i_error;
}

