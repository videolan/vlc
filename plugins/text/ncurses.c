/*****************************************************************************
 * ncurses.c : NCurses plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ncurses.c,v 1.11 2002/02/15 13:32:53 sam Exp $
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

#include <curses.h>

#include <videolan/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void intf_getfunctions ( function_list_t * p_function_list );
static int  intf_Open         ( intf_thread_t *p_intf );
static void intf_Close        ( intf_thread_t *p_intf );
static void intf_Run          ( intf_thread_t *p_intf );

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for NCurses module" )
    ADD_COMMENT( "For now, the NCurses module cannot be configured" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_INTF;
    p_module->psz_longname = "ncurses interface module";
    ADD_SHORTCUT( "ncurses" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    intf_getfunctions( &p_module->p_functions->intf );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * intf_sys_t: description and status of ncurses interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    /* special actions */
    vlc_mutex_t         change_lock;                      /* the change lock */

} intf_sys_t;

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
        intf_ErrMsg( "intf error: %s", strerror(ENOMEM) );
        return( 1 );
    }

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

    while( !p_intf->b_die )
    {
        p_intf->pf_manage( p_intf );

        msleep( INTF_IDLE_SLEEP );

        mvaddstr( 1, 2, VOUT_TITLE " (ncurses interface)" );
        mvaddstr( 3, 2, "keys:" );
        mvaddstr( 4, 2, "Q,q.......quit" );
        //mvaddstr( 5, 2, "No other keys are active yet." );

        while( (i_key = getch()) != -1 )
        {
            switch( i_key )
            {
                case 'q':
                case 'Q':
                    p_intf->b_die = 1;
                    break;

                default:
                    break;
            }
        }
    }
}

/* following functions are local */

