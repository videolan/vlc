/*****************************************************************************
 * intf_rc.cpp: remote control interface
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: intf_rc.cpp,v 0.1 2001/04/27 shurdeek
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

#define MODULE_NAME rc
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#if defined( WIN32 )
#include <winsock2.h>                                            /* select() */
#endif

#include "config.h"
#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_playlist.h"
#include "interface.h"

#include "video.h"
#include "video_output.h"

#include "modules.h"
#include "modules_export.h"

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
static int  intf_Probe     ( probedata_t *p_data );
static int  intf_Open      ( intf_thread_t *p_intf );
static void intf_Close     ( intf_thread_t *p_intf );
static void intf_Run       ( intf_thread_t *p_intf );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( intf_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = intf_Probe;
    p_function_list->functions.intf.pf_open  = intf_Open;
    p_function_list->functions.intf.pf_close = intf_Close;
    p_function_list->functions.intf.pf_run   = intf_Run;
}

/*****************************************************************************
 * intf_Probe: probe the interface and return a score
 *****************************************************************************
 * This function tries to initialize rc and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int intf_Probe( probedata_t *p_data )
{
    if( TestMethod( INTF_METHOD_VAR, "rc" ) )
    {
        return( 999 );
    }

    return( 20 );
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
#define S p_intf->p_input->stream
        if( p_intf->p_input != NULL )
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

        /* Is there something to do? */
        if( b_complete == 1 )
        {
            switch( p_cmd[ 0 ] )
            {
            case 'p':
            case 'P':
                if( p_intf->p_input != NULL )
                {
                    input_SetStatus( p_intf->p_input, INPUT_STATUS_PAUSE );
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
                if( p_intf->p_input != NULL )
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
                    input_Seek( p_intf->p_input, (off_t) (f_cpos / f_ratio) );
                    /* rcreseek(f_cpos); */
                }
                break;

            default:
                intf_Msg( "rc: unknown command `%s'", p_cmd );
                break;
            }
        }

        p_intf->pf_manage( p_intf );
        msleep( INTF_IDLE_SLEEP );
    }
}

