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
#include <string.h>
#include <sys/types.h>
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
#include "video.h"
#include "video_output.h"

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
        intf_ErrMsg("error: %s\n", strerror( ENOMEM ) );        
	return( NULL );
    }

    /* Initialize structure */
    p_intf->b_die =     0;    
    p_intf->p_vout =    NULL;
    p_intf->p_input =   NULL;   

    /* Start interfaces */
    p_intf->p_console = intf_ConsoleCreate();
    if( p_intf->p_console == NULL )
    {
        intf_ErrMsg("error: can't create control console\n");
	free( p_intf );
        return( NULL );
    }
    if( intf_SysCreate( p_intf ) )
    {
	intf_ErrMsg("error: can't create interface\n");
	intf_ConsoleDestroy( p_intf->p_console );
	free( p_intf );
	return( NULL );
    }   

    intf_Msg("Interface initialized\n");    
    return( p_intf );
}

/*******************************************************************************
 * intf_Run
 *******************************************************************************
 * Initialization script and main interface loop.
 *******************************************************************************/
void intf_Run( intf_thread_t *p_intf )
{ 
    /* Execute the initialization script - if a positive number is returned, 
     * the script could be executed but failed */
    if( intf_ExecScript( main_GetPszVariable( INTF_INIT_SCRIPT_VAR, INTF_INIT_SCRIPT_DEFAULT ) ) > 0 )
    {
	intf_ErrMsg("warning: error(s) during startup script\n");
    }

    /* Main loop */
    while(!p_intf->b_die)
    {
        /* Flush waiting messages */
        intf_FlushMsg();

        /* Manage specific interface */
        intf_SysManage( p_intf );

        /* Check attached threads status */
        if( (p_intf->p_vout != NULL) && p_intf->p_vout->b_error )
        {
            //?? add aout error detection
            p_intf->b_die = 1;            
        }    
        if( (p_intf->p_input != NULL) && p_intf->p_input->b_error )
        {
            input_DestroyThread( p_intf->p_input, NULL );            
            p_intf->p_input = NULL;            
            intf_DbgMsg("Input thread destroyed\n");            
        }

        /* Sleep to avoid using all CPU - since some interfaces needs to access 
         * keyboard events, a 100ms delay is a good compromise */
        msleep( INTF_IDLE_SLEEP );
    }
}

/*******************************************************************************
 * intf_Destroy: clean interface after main loop
 *******************************************************************************
 * This function destroys specific interfaces and close output devices.
 *******************************************************************************/
void intf_Destroy( intf_thread_t *p_intf )
{
    /* Destroy interfaces */
    intf_SysDestroy( p_intf );
    intf_ConsoleDestroy( p_intf->p_console );

    /* Free structure */
    free( p_intf );
}

/*******************************************************************************
 * intf_SelectInput: change input stream
 *******************************************************************************
 * Kill existing input, if any, and try to open a new one, using an input
 * configuration table.
 *******************************************************************************/
int intf_SelectInput( intf_thread_t * p_intf, int i_index )
{
    intf_DbgMsg("\n");
    
    /* Kill existing input, if any */
    if( p_intf->p_input != NULL )
    {        
        input_DestroyThread( p_intf->p_input, NULL );
    }

    /* Open a new input */
    intf_Msg("Switching to channel %d\n", i_index );    
    p_intf->p_input = input_CreateThread( INPUT_METHOD_TS_VLAN_BCAST, NULL, 0, i_index, 
                                          p_intf->p_vout, p_main->p_aout, NULL );        
    return( p_intf->p_input == NULL );    
}

/*******************************************************************************
 * intf_ProcessKey: process standard keys
 *******************************************************************************
 * This function will process standard keys and return non 0 if the key was
 * unknown.
 *******************************************************************************/
int intf_ProcessKey( intf_thread_t *p_intf, int i_key )
{
    switch( i_key )
    {
    case 'Q':                                                    /* quit order */
    case 'q':
    case 27: /* escape key */
        p_intf->b_die = 1;
        break;  
    case '0':                                                 /* source change */
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':                    
        if( intf_SelectInput( p_intf, i_key - '0' ) )
        {
            intf_ErrMsg("error: can not open channel %d\n", i_key - '0');            
        }        
        break;
    case '+':                                                      /* volume + */
        // ??
        break;
    case '-':                                                      /* volume - */
        // ??
        break;
    case 'M':                                                   /* toggle mute */
    case 'm':                    
        // ??
        break;	    
    case 'g':                                                       /* gamma - */
        if( (p_intf->p_vout != NULL) && (p_intf->p_vout->f_gamma > -INTF_GAMMA_LIMIT) )
        {
            vlc_mutex_lock( &p_intf->p_vout->change_lock );
            p_intf->p_vout->f_gamma   -= INTF_GAMMA_STEP;                        
            p_intf->p_vout->i_changes |= VOUT_GAMMA_CHANGE;
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );
        }                    
        break;                                        
    case 'G':                                                       /* gamma + */
        if( (p_intf->p_vout != NULL) && (p_intf->p_vout->f_gamma < INTF_GAMMA_LIMIT) )
        {       
            vlc_mutex_lock( &p_intf->p_vout->change_lock );
            p_intf->p_vout->f_gamma   += INTF_GAMMA_STEP;
            p_intf->p_vout->i_changes |= VOUT_GAMMA_CHANGE;
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );
        }                    
        break;  
    case 'c':                                              /* toggle grayscale */
        if( p_intf->p_vout != NULL )
        {
            vlc_mutex_lock( &p_intf->p_vout->change_lock );                        
            p_intf->p_vout->b_grayscale = !p_intf->p_vout->b_grayscale;                    
            p_intf->p_vout->i_changes  |= VOUT_GRAYSCALE_CHANGE;                        
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );      
        }
        break;  
    case ' ':                                                   /* toggle info */
        if( p_intf->p_vout != NULL )
        {
            vlc_mutex_lock( &p_intf->p_vout->change_lock );                        
            p_intf->p_vout->b_info     = !p_intf->p_vout->b_info;                    
            p_intf->p_vout->i_changes |= VOUT_INFO_CHANGE;                        
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );      
        }
        break;                                
    default:                                                    /* unknown key */
        return( 1 );        
    }

    return( 0 );    
}

    
                
