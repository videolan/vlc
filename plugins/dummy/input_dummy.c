/*****************************************************************************
 * input_dummy.c: dummy input plugin, to manage "vlc:***" special options
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: input_dummy.c,v 1.14 2002/01/10 04:11:25 sam Exp $
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

#include <videolan/vlc.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#include "interface.h"
#include "intf_playlist.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  DummyProbe     ( probedata_t * );
static void DummyInit      ( struct input_thread_s * );
static void DummyOpen      ( struct input_thread_s * );
static void DummyClose     ( struct input_thread_s * );
static void DummyEnd       ( struct input_thread_s * );
static int  DummyRead      ( struct input_thread_s *, data_packet_t ** );

/*****************************************************************************
 * dummy_data_t: private input data
 *****************************************************************************/
typedef struct dummy_data_s
{
    /* The real command */
    int i_command;

    /* Used for the pause command */
    mtime_t expiration;

} dummy_data_t;

#define COMMAND_NOP   0
#define COMMAND_QUIT  1
#define COMMAND_LOOP  2
#define COMMAND_PAUSE 3

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list )
{
#define input p_function_list->functions.input
    p_function_list->pf_probe = DummyProbe;
    input.pf_init             = DummyInit;
    input.pf_open             = DummyOpen;
    input.pf_close            = DummyClose;
    input.pf_end              = DummyEnd;
    input.pf_set_area         = NULL;
    input.pf_read             = DummyRead;
    input.pf_demux            = NULL;
    input.pf_new_packet       = NULL;
    input.pf_new_pes          = NULL;
    input.pf_delete_packet    = NULL;
    input.pf_delete_pes       = NULL;
    input.pf_rewind           = NULL;
    input.pf_seek             = NULL;
#undef input
}

/*****************************************************************************
 * DummyProbe: verifies that the input is a vlc command
 *****************************************************************************/
static int DummyProbe( probedata_t *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_data;
    char *psz_name = p_input->p_source;

    if( ( strlen(psz_name) > 4 ) && !strncasecmp( psz_name, "vlc:", 4 ) )
    {
        /* If the user specified "vlc:" then it's probably a file */
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * DummyOpen: open the target, ie. do nothing
 *****************************************************************************/
static void DummyOpen( input_thread_t * p_input )
{
    p_input->stream.i_method = INPUT_METHOD_NONE;
}

/*****************************************************************************
 * DummyClose: close the target, ie. do nothing
 *****************************************************************************/
static void DummyClose( input_thread_t * p_input )
{
    ;
}

/*****************************************************************************
 * DummyOpen: initialize the target, ie. parse the command
 *****************************************************************************/
static void DummyInit( struct input_thread_s *p_input )
{
    dummy_data_t* p_method;
    char *psz_name = p_input->p_source;
    int   i_len = strlen( psz_name );
    int   i_arg;
    
    p_input->stream.b_seekable = 0;

    if( ( i_len <= 4 ) || strncasecmp( psz_name, "vlc:", 4 ) )
    {
        /* If the command doesn't start with "vlc:" then it's not for us */
        p_input->b_error = 1;
        return;
    }

    /* We don't need the "vlc:" stuff any more */
    psz_name += 4;
    i_len -= 4;

    p_method = malloc( sizeof( dummy_data_t ) );
    if( p_method == NULL )
    {
        intf_ErrMsg( "input: out of memory" );
        p_input->b_error = 1;
        return;
    }

    p_input->p_plugin_data = (void *)p_method;
    p_input->stream.p_demux_data = NULL;

    /* Check for a "vlc:nop" command */
    if( i_len == 3 && !strncasecmp( psz_name, "nop", 3 ) )
    {
        intf_WarnMsg( 2, "input: command `nop'" );
        p_method->i_command = COMMAND_NOP;
        return;
    }

    /* Check for a "vlc:quit" command */
    if( i_len == 4 && !strncasecmp( psz_name, "quit", 4 ) )
    {
        intf_WarnMsg( 2, "input: command `quit'" );
        p_method->i_command = COMMAND_QUIT;
        return;
    }

    /* Check for a "vlc:loop" command */
    if( i_len == 4 && !strncasecmp( psz_name, "loop", 4 ) )
    {
        intf_WarnMsg( 2, "input: command `loop'" );
        p_method->i_command = COMMAND_LOOP;
        return;
    }

    /* Check for a "vlc:pause:***" command */
    if( i_len > 6 && !strncasecmp( psz_name, "pause:", 6 ) )
    {
        i_arg = atoi( psz_name + 6 );
        intf_WarnMsg( 2, "input: command `pause %i'", i_arg );
        p_method->i_command = COMMAND_PAUSE;
        p_method->expiration = mdate() + (mtime_t)i_arg * (mtime_t)1000000;
        return;
    }

    intf_ErrMsg( "input error: unknown command `%s'", psz_name );
    free( p_input->p_plugin_data );
    p_input->b_error = 1;

    return;
}

/*****************************************************************************
 * DummyEnd: end the target, ie. do nothing
 *****************************************************************************/
static void DummyEnd( struct input_thread_s *p_input )
{
    free( p_input->p_plugin_data );
}

/*****************************************************************************
 * DummyRead: do what the command says
 *****************************************************************************/
static int DummyRead( struct input_thread_s *p_input, data_packet_t **pp_data )
{
    dummy_data_t* p_method = (dummy_data_t *)p_input->p_plugin_data;

    switch( p_method->i_command )
    {
        case COMMAND_QUIT:
            p_input->b_die = 1;
            break;

        case COMMAND_LOOP:
            intf_PlaylistJumpto( p_main->p_playlist, -1 );
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

    *pp_data = NULL;

    return 0;
}

