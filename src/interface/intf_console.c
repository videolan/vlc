/*****************************************************************************
 * intf_console.c: generic console for interface
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 *
 * Authors:
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
#include "defs.h"

#include <stdlib.h>                                              /* malloc() */

#include "config.h"

/*****************************************************************************
 * intf_console_t: console descriptor
 *****************************************************************************
 * Generic console object. This object will have a representation depending of
 * the interface.
 *****************************************************************************/
typedef struct intf_console_s
{
    /* Text and history arrays - last line/command has indice 0 */
    char *                  psz_text[INTF_CONSOLE_MAX_TEXT];
    char *                  psz_history[INTF_CONSOLE_MAX_HISTORY];
} intf_console_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/*****************************************************************************
 * intf_ConsoleCreate: create console
 *****************************************************************************
 * This function will initialize the console object.
 * It returns NULL on error.
 *****************************************************************************/
intf_console_t *intf_ConsoleCreate( void )
{
    intf_console_t *p_console;

    p_console = malloc( sizeof( intf_console_t ) );
    return( p_console );
}

/*****************************************************************************
 * intf_ConsoleDestroy
 *****************************************************************************
 * Destroy the console instance initialized by intf_ConsoleCreate.
 *****************************************************************************/
void intf_ConsoleDestroy( intf_console_t *p_console )
{
    free( p_console );
}

/*****************************************************************************
 * intf_ConsoleClear: clear console
 *****************************************************************************
 * Empty all text.
 *****************************************************************************/
void intf_ConsoleClear( intf_console_t *p_console )
{
    /* XXX?? */
}

/*****************************************************************************
 * intf_ConsolePrint: print a message to console
 *****************************************************************************
 * Print a message to the console.
 *****************************************************************************/
void intf_ConsolePrint( intf_console_t *p_console, char *psz_str )
{
    /* XXX?? */
}


/*****************************************************************************
 * intf_ConsoleExec: execute a command in console
 *****************************************************************************
 * This function will run a command and print its result in console.
 *****************************************************************************/
void intf_ConsoleExec( intf_console_t *p_console, char *psz_str )
{
    /* XXX?? */
}

/* following functions are local */
