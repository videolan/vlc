/*****************************************************************************
 * file.c: file input (file: access plug-in)
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: file.c,v 1.4 2002/04/10 16:26:21 jobi Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void input_getfunctions( function_list_t * );
static int  FileOpen       ( struct input_thread_s * );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP
 
MODULE_INIT_START
    SET_DESCRIPTION( "Standard filesystem file reading" )
    ADD_CAPABILITY( ACCESS, 50 )
    ADD_SHORTCUT( "file" )
    ADD_SHORTCUT( "stream" )
MODULE_INIT_STOP
 
MODULE_ACTIVATE_START
    input_getfunctions( &p_module->p_functions->access );
MODULE_ACTIVATE_STOP
 
MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list )
{
#define input p_function_list->functions.access
    input.pf_open             = FileOpen;
    input.pf_read             = input_FDRead;
    input.pf_close            = input_FDClose;
    input.pf_set_program      = input_SetProgram;
    input.pf_set_area         = NULL;
    input.pf_seek             = input_FDSeek;
#undef input
}

/*****************************************************************************
 * FileOpen: open the file
 *****************************************************************************/
static int FileOpen( input_thread_t * p_input )
{
    char *              psz_name = p_input->psz_name;
    int                 i_stat;
    struct stat         stat_info;                                              
    input_socket_t *    p_access_data;
    boolean_t           b_stdin;

    p_input->i_mtu = 0;

    b_stdin = ( strlen( p_input->psz_name ) == 1 )
                && *p_input->psz_name == '-';

    if( !b_stdin && (i_stat = stat( psz_name, &stat_info )) == (-1) )
    {
        intf_ErrMsg( "input error: cannot stat() file `%s' (%s)",
                     psz_name, strerror(errno));
        return( -1 );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    if( *p_input->psz_access && !strncmp( p_input->psz_access, "stream", 7 ) )
    {
        /* stream:%s */
        p_input->stream.b_pace_control = 0;
        p_input->stream.b_seekable = 0;
        p_input->stream.p_selected_area->i_size = 0;
    }
    else
    {
        /* file:%s or %s */
        p_input->stream.b_pace_control = 1;

        if( b_stdin )
        {
            p_input->stream.b_seekable = 0;
            p_input->stream.p_selected_area->i_size = 0;
        }
        else if( S_ISREG(stat_info.st_mode) || S_ISCHR(stat_info.st_mode)
                  || S_ISBLK(stat_info.st_mode) )
        {
            p_input->stream.b_seekable = 1;
            p_input->stream.p_selected_area->i_size = stat_info.st_size;
        }
        else if( S_ISFIFO(stat_info.st_mode)
#if !defined( SYS_BEOS ) && !defined( WIN32 )
                  || S_ISSOCK(stat_info.st_mode)
#endif
               )
        {
            p_input->stream.b_seekable = 0;
            p_input->stream.p_selected_area->i_size = 0;
        }
        else
        {
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            intf_ErrMsg( "input error: unknown file type for `%s'",
                         psz_name );
            return( -1 );
        }
    }
 
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_FILE;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
 
    intf_WarnMsg( 2, "input: opening file `%s'", psz_name );
    p_access_data = malloc( sizeof(input_socket_t) );
    p_input->p_access_data = (void *)p_access_data;
    if( p_access_data == NULL )
    {
        intf_ErrMsg( "input error: Out of memory" );
        return( -1 );
    }

    if( b_stdin )
    {
        p_access_data->i_handle = 0;
    }
    else if( (p_access_data->i_handle = open( psz_name,
                                   /*O_NONBLOCK | O_LARGEFILE*/ 0 )) == (-1) )
    {
        intf_ErrMsg( "input error: cannot open file %s (%s)", psz_name,
                     strerror(errno) );
        free( p_access_data );
        return( -1 );
    }

    return( 0 );
}
