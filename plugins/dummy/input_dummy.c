/*****************************************************************************
 * input_dummy.c: dummy input plugin, to manage "vlc:***" special options
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: input_dummy.c,v 1.18 2002/06/01 12:31:58 sam Exp $
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
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/input.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  DummyInit   ( input_thread_t * );
static int  DummyOpen   ( input_thread_t * );
static void DummyClose  ( input_thread_t * );
static void DummyEnd    ( input_thread_t * );
static int  DummyDemux  ( input_thread_t * );

/*****************************************************************************
 * access_sys_t: private input data
 *****************************************************************************/
struct demux_sys_s
{
    /* The real command */
    int i_command;

    /* Used for the pause command */
    mtime_t expiration;
};

#define COMMAND_NOP   0
#define COMMAND_QUIT  1
#define COMMAND_LOOP  2
#define COMMAND_PAUSE 3

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( access_getfunctions )( function_list_t * p_function_list )
{
#define input p_function_list->functions.access
    input.pf_open             = DummyOpen;
    input.pf_read             = NULL;
    input.pf_close            = DummyClose;
    input.pf_set_program      = NULL;
    input.pf_set_area         = NULL;
    input.pf_seek             = NULL;
#undef input
}

void _M( demux_getfunctions )( function_list_t * p_function_list )
{
#define input p_function_list->functions.demux
    input.pf_init             = DummyInit;
    input.pf_end              = DummyEnd;
    input.pf_demux            = DummyDemux;
    input.pf_rewind           = NULL;
#undef input
}

/*****************************************************************************
 * DummyOpen: open the target, ie. do nothing
 *****************************************************************************/
static int DummyOpen( input_thread_t * p_input )
{
    p_input->stream.i_method = INPUT_METHOD_NONE;

    /* Force dummy demux plug-in */
    p_input->psz_demux = "vlc";
    return( 0 );
}

/*****************************************************************************
 * DummyClose: close the target, ie. do nothing
 *****************************************************************************/
static void DummyClose( input_thread_t * p_input )
{
}

/*****************************************************************************
 * DummyInit: initialize the target, ie. parse the command
 *****************************************************************************/
static int DummyInit( input_thread_t *p_input )
{
    char * psz_name = p_input->psz_name;
    int i_len = strlen( psz_name );
    struct demux_sys_s * p_method;
    int   i_arg;
    
    p_input->stream.b_seekable = 0;

    p_method = malloc( sizeof( struct demux_sys_s ) );
    if( p_method == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }

    p_input->p_demux_data = p_method;
    p_input->stream.p_demux_data = NULL;

    /* Check for a "vlc:nop" command */
    if( i_len == 3 && !strncasecmp( psz_name, "nop", 3 ) )
    {
        msg_Info( p_input, "command `nop'" );
        p_method->i_command = COMMAND_NOP;
        return( 0 );
    }

    /* Check for a "vlc:quit" command */
    if( i_len == 4 && !strncasecmp( psz_name, "quit", 4 ) )
    {
        msg_Info( p_input, "command `quit'" );
        p_method->i_command = COMMAND_QUIT;
        return( 0 );
    }

    /* Check for a "vlc:loop" command */
    if( i_len == 4 && !strncasecmp( psz_name, "loop", 4 ) )
    {
        msg_Info( p_input, "command `loop'" );
        p_method->i_command = COMMAND_LOOP;
        return( 0 );
    }

    /* Check for a "vlc:pause:***" command */
    if( i_len > 6 && !strncasecmp( psz_name, "pause:", 6 ) )
    {
        i_arg = atoi( psz_name + 6 );
        msg_Info( p_input, "command `pause %i'", i_arg );
        p_method->i_command = COMMAND_PAUSE;
        p_method->expiration = mdate() + (mtime_t)i_arg * (mtime_t)1000000;
        return( 0 );
    }

    msg_Err( p_input, "unknown command `%s'", psz_name );
    free( p_input->p_demux_data );
    p_input->b_error = 1;

    return( -1 );
}

/*****************************************************************************
 * DummyEnd: end the target, ie. do nothing
 *****************************************************************************/
static void DummyEnd( input_thread_t *p_input )
{
    free( p_input->p_demux_data );
}

/*****************************************************************************
 * DummyDemux: do what the command says
 *****************************************************************************/
static int DummyDemux( input_thread_t *p_input )
{
    struct demux_sys_s * p_method = p_input->p_demux_data;

    switch( p_method->i_command )
    {
        case COMMAND_QUIT:
            p_input->p_vlc->b_die = 1;
            p_input->b_die = 1;
            break;

        case COMMAND_LOOP:
            //playlist_Jumpto( p_input->p_vlc->p_playlist, -1 );
            p_input->b_eof = 1;
            break;

        case COMMAND_PAUSE:
            if( mdate() < p_method->expiration )
            {
                msleep( 10000 );
            }
            else
            {
                p_input->b_eof = 1;
            }
            break;

        case COMMAND_NOP:
        default:
            p_input->b_eof = 1;
            break;
    }

    return 1;
}

