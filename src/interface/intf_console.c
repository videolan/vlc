/*******************************************************************************
 * intf_console.c: generic console for interface
 * (c)1998 VideoLAN
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <stdlib.h>

#include "config.h"

/*******************************************************************************
 * intf_console_t: console descriptor
 *******************************************************************************
 * Generic console object. This object will have a representation depending of
 * the interface.
 *******************************************************************************/
typedef struct intf_console_s
{
    /* Text and history arrays - last line/command has indice 0 */
    char *                  psz_text[INTF_CONSOLE_MAX_TEXT];
    char *                  psz_history[INTF_CONSOLE_MAX_HISTORY]; 
} intf_console_t;

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/

/*******************************************************************************
 * intf_ConsoleCreate: create console
 *******************************************************************************
 * This function will initialize the console object.
 * It returns NULL on error.
 *******************************************************************************/
intf_console_t *intf_ConsoleCreate( void )
{
    intf_console_t *p_console;

    p_console = malloc( sizeof( intf_console_t ) );
    return( p_console );
}

/*******************************************************************************
 * intf_ConsoleDestroy
 *******************************************************************************
 * Destroy the console instance initialized by intf_ConsoleCreate.
 *******************************************************************************/
void intf_ConsoleDestroy( intf_console_t *p_console )
{
    free( p_console );
}

/*******************************************************************************
 * intf_ConsoleClear: clear console
 *******************************************************************************
 * Empty all text.
 *******************************************************************************/
void intf_ConsoleClear( intf_console_t *p_console )
{
    //??
}

/*******************************************************************************
 * intf_ConsolePrint: print a message to console
 *******************************************************************************
 * Print a message to the console.
 *******************************************************************************/
void intf_ConsolePrint( intf_console_t *p_console, char *psz_str )
{
    //??
}


/*******************************************************************************
 * intf_ConsoleExec: execute a command in console
 *******************************************************************************
 * This function will run a command and print its result in console.
 *******************************************************************************/
void intf_ConsoleExec( intf_console_t *p_console, char *psz_str )
{
    //??
}

/* following functions are local */
