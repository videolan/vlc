/*****************************************************************************
 * rc.c : remote control stdin/stdout plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: rc.c,v 1.12 2002/04/19 13:56:11 sam Exp $
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
#include <videolan/vlc.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>
#include <ctype.h>

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

#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_playlist.h"
#include "interface.h"

#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * intf_sys_t: description and status of rc interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    vlc_mutex_t         change_lock;

} intf_sys_t;

#define MAX_LINE_LENGTH 256

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void intf_getfunctions ( function_list_t * p_function_list );
static int  intf_Open         ( intf_thread_t *p_intf );
static void intf_Close        ( intf_thread_t *p_intf );
static void intf_Run          ( intf_thread_t *p_intf );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("remote control interface module") )
    ADD_CAPABILITY( INTF, 20 )
    ADD_SHORTCUT( "rc" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    intf_getfunctions( &p_module->p_functions->intf );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

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
 * intf_Open: initialize and create stuff
 *****************************************************************************/
static int intf_Open( intf_thread_t *p_intf )
{
    /* Non-buffered stdout */
    setvbuf( stdout, (char *)NULL, _IOLBF, 0 );

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg( "intf error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    intf_Msg( "rc: remote control interface initialized, `h' for help" );
    return( 0 );
}

/*****************************************************************************
 * intf_Close: destroy interface stuff
 *****************************************************************************/
static void intf_Close( intf_thread_t *p_intf )
{
    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_Run: rc thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
static void intf_Run( intf_thread_t *p_intf )
{
    char      p_cmd[ MAX_LINE_LENGTH + 1 ];
    int       i_cmd_pos;
    boolean_t b_complete = 0;

    int       i_dummy;
    off_t     i_oldpos = 0;
    off_t     i_newpos;
    fd_set    fds;                                         /* stdin changed? */
    struct timeval tv;                                   /* how long to wait */

    double    f_cpos;
    double    f_ratio = 1;

    while( !p_intf->b_die )
    {
        vlc_mutex_lock( &p_input_bank->lock );
#define S p_input_bank->pp_input[0]->stream
        if( p_input_bank->pp_input[0] != NULL )
        {
            /* Get position */
            if( S.i_mux_rate )
            {
                f_ratio = 1.0 / ( 50 * S.i_mux_rate );
                i_newpos = S.p_selected_area->i_tell * f_ratio;

                if( i_oldpos != i_newpos )
                {
                    i_oldpos = i_newpos;
                    intf_Msg( "rc: pos: %li s / %li s", (long int)i_newpos,
                              (long int)( f_ratio *
                                          S.p_selected_area->i_size ) );
                }
            }
        }
#undef S
        vlc_mutex_unlock( &p_input_bank->lock );

        b_complete = 0;
        i_cmd_pos = 0;

        /* Check stdin */
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        FD_ZERO( &fds );
        FD_SET( STDIN_FILENO, &fds );

        if( select( 32, &fds, NULL, NULL, &tv ) )
        {
            while( !p_intf->b_die
                    && i_cmd_pos < MAX_LINE_LENGTH
                    && read( STDIN_FILENO, p_cmd + i_cmd_pos, 1 ) > 0
                    && p_cmd[ i_cmd_pos ] != '\r'
                    && p_cmd[ i_cmd_pos ] != '\n' )
            {
                i_cmd_pos++;
            }

            if( i_cmd_pos == MAX_LINE_LENGTH
                 || p_cmd[ i_cmd_pos ] == '\r'
                 || p_cmd[ i_cmd_pos ] == '\n' )
            {
                p_cmd[ i_cmd_pos ] = 0;
                b_complete = 1;
            }
        }

        vlc_mutex_lock( &p_input_bank->lock );

        /* Is there something to do? */
        if( b_complete == 1 )
        {
            switch( p_cmd[ 0 ] )
            {
            case 'a':
            case 'A':
                if( p_cmd[ 1 ] == ' ' )
                {
                    intf_PlaylistAdd( p_main->p_playlist,
                                      PLAYLIST_END, p_cmd + 2 );
                    if( p_input_bank->pp_input[0] != NULL )
                    {
                        p_input_bank->pp_input[0]->b_eof = 1;
                    }
                    intf_PlaylistJumpto( p_main->p_playlist,
                                         p_main->p_playlist->i_size - 2 );
                }
                break;

            case 'p':
            case 'P':
                if( p_input_bank->pp_input[0] != NULL )
                {
                    input_SetStatus( p_input_bank->pp_input[0],
                                     INPUT_STATUS_PAUSE );
                }
                break;

            case 'f':
            case 'F':
                vlc_mutex_lock( &p_vout_bank->lock );
                /* XXX: only fullscreen the first video output */
                if( p_vout_bank->i_count )
                {
                    p_vout_bank->pp_vout[0]->i_changes
                                      |= VOUT_FULLSCREEN_CHANGE;
                }
                vlc_mutex_unlock( &p_vout_bank->lock );
                break;

            case 'm':
            case 'M':
#if 0
                double picratio = p_intf->p_input->p_default_vout->i_width 
                    / p_intf->p_input->p_default_vout->i_height;
                if (picratio
                p_intf->p_input->p_default_vout->i_width=800
                p_intf->p_input->p_default_vout->i_changes |= 
                    VOUT_FULLSCREEN_CHANGE;
#endif
                break;

            case 's':
            case 'S':
                ;
                break;

            case 'q':
            case 'Q':
                p_intf->b_die = 1;
                break;

            case 'r':
            case 'R':
                if( p_input_bank->pp_input[0] != NULL )
                {
                    for( i_dummy = 1;
                         i_dummy < MAX_LINE_LENGTH && p_cmd[ i_dummy ] >= '0'
                                                   && p_cmd[ i_dummy ] <= '9';
                         i_dummy++ )
                    {
                        ;
                    }

                    p_cmd[ i_dummy ] = 0;
                    f_cpos = atof( p_cmd + 1 );
                    input_Seek( p_input_bank->pp_input[0],
                                (off_t) (f_cpos / f_ratio) );
                    /* rcreseek(f_cpos); */
                }
                break;

            case '?':
            case 'h':
            case 'H':
                intf_Msg( "rc: help for remote control commands" );
                intf_Msg( "rc: h                                       help" );
                intf_Msg( "rc: a XYZ                 append XYZ to playlist" );
                intf_Msg( "rc: p                               toggle pause" );
                intf_Msg( "rc: f                          toggle fullscreen" );
                intf_Msg( "rc: r X    seek in seconds, for instance `r 3.5'" );
                intf_Msg( "rc: q                                       quit" );
                intf_Msg( "rc: end of help" );
                break;

            default:
                intf_Msg( "rc: unknown command `%s'", p_cmd );
                break;
            }
        }

        vlc_mutex_unlock( &p_input_bank->lock );

        p_intf->pf_manage( p_intf );
        msleep( INTF_IDLE_SLEEP );
    }
}

