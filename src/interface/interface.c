/*******************************************************************************
 * interface.c: interface access for other threads
 * (c)1998 VideoLAN
 *******************************************************************************
 * This library provides basic functions for threads to interact with user
 * interface, such as command line.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>                                        /* for input.h */

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "input.h"
#include "intf_msg.h"
#include "interface.h"
#include "intf_cmd.h"
#include "intf_console.h"
#include "main.h"

#include "intf_sys.h"


/*******************************************************************************
 * intf_Create: prepare interface before main loop
 *******************************************************************************
 * This function opens output devices and create specific interfaces. It send
 * it's own error messages.
 *******************************************************************************/
intf_thread_t* intf_Create( void )
{    
    intf_thread_t *p_intf;                                        

    /* Allocate structure */
    p_intf = malloc( sizeof( intf_thread_t ) );
    if( !p_intf )
    {
	errno = ENOMEM;
	return( NULL );
    }
    p_intf->b_die = 0;
    intf_DbgMsg( "0x%x\n", p_intf );

    /* Initialize structure */
    p_intf->p_vout = NULL;
    p_intf->p_input = NULL;   

    /* Start interfaces */
    p_intf->p_console = intf_ConsoleCreate();
    if( p_intf->p_console == NULL )
    {
        intf_ErrMsg("intf error: can't create control console\n");
	free( p_intf );
        return( NULL );
    }
    if( intf_SysCreate( p_intf ) )
    {
	intf_ErrMsg("intf error: can't create interface\n");
	intf_ConsoleDestroy( p_intf->p_console );
	free( p_intf );
	return( NULL );
    }   

    return( p_intf );
}

/*******************************************************************************
 * intf_Run
 *******************************************************************************
 * Initialization script and main interface loop.
 *******************************************************************************/
void intf_Run( intf_thread_t *p_intf )
{ 
    intf_DbgMsg("0x%x begin\n", p_intf );

    /* Execute the initialization script - if a positive number is returned, 
     * the script could be executed but failed */
    if( intf_ExecScript( main_GetPszVariable( INTF_INIT_SCRIPT_VAR, INTF_INIT_SCRIPT_DEFAULT ) ) > 0 )
    {
	intf_ErrMsg("intf error: error during initialization script\n");
    }

    /* Main loop */
    while(!p_intf->b_die)
    {
        /* Flush waiting messages */
        intf_FlushMsg();

        /* Manage specific interface */
        intf_SysManage( p_intf );

        /* Sleep to avoid using all CPU - since some interfaces needs to access 
         * keyboard events, a 100ms delay is a good compromise */
        msleep( INTF_IDLE_SLEEP );
    }

    intf_DbgMsg("0x%x end\n", p_intf );
}

/*******************************************************************************
 * intf_Destroy: clean interface after main loop
 *******************************************************************************
 * This function destroys specific interfaces and close output devices.
 *******************************************************************************/
void intf_Destroy( intf_thread_t *p_intf )
{
    intf_DbgMsg("0x%x\n", p_intf );

    /* Destroy interfaces */
    intf_SysDestroy( p_intf );
    intf_ConsoleDestroy( p_intf->p_console );

    /* Free structure */
    free( p_intf );
}

/*******************************************************************************
 * intf_SelectInput: change input stream
 *******************************************************************************
 * Kill existing input, if any, and try to open a new one. If p_cfg is NULL,
 * no new input will be openned.
 *******************************************************************************/
int intf_SelectInput( intf_thread_t * p_intf, input_cfg_t *p_cfg )
{
    intf_DbgMsg("0x%x\n", p_intf );
    
    /* Kill existing input, if any */
    if( p_intf->p_input != NULL )
    {        
        input_DestroyThread( p_intf->p_input /*??, NULL*/ );
        p_intf->p_input = NULL;        
    }
    
    /* Open new one */
    if( p_cfg != NULL )
    {        
        p_intf->p_input = input_CreateThread( p_cfg /*??, NULL*/ );    
    }
    
    return( (p_cfg != NULL) && (p_intf->p_input == NULL) );    
}


