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
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/soundcard.h>
#include <sys/uio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "thread.h"
#include "debug.h"

#include "intf_msg.h"

#include "input.h"
#include "input_netlist.h"
#include "input_vlan.h"
#include "decoder_fifo.h"

#include "audio_output.h"
#include "audio_decoder.h"

#include "video.h"
#include "video_output.h"
#include "video_decoder.h"

#include "xconsole.h"
#include "interface.h"
#include "intf_cmd.h"

#include "pgm_data.h"
/* ?? remove useless headers */

/*
 * Local prototypes
 */
static int  StartInterface  ( intf_thread_t *p_intf );
static void EndInterface    ( intf_thread_t *p_intf );

/*******************************************************************************
 * intf_Run
 *******************************************************************************
 * what it does:
 *     - Create an X11 console
 *     - wait for a command and try to execute it
 *     - interpret the order returned after the command execution
 *     - print the messages of the message queue (intf_FlushMsg)
 * return value: 0 if successful, < 0 otherwise
 *******************************************************************************/
int intf_Run( intf_thread_t *p_intf )
{
    /* When it is started, interface won't die immediatly */
    p_intf->b_die = 0;
    if( StartInterface( p_intf ) )                                    /* error */
    {
        return( 1 );
    }
    
    /* Main loop */
    while(!p_intf->b_die)
    {
        /* Flush waiting messages */
        intf_FlushMsg();

        /* Manage specific interfaces */
        intf_ManageXConsole( &p_intf->xconsole );               /* X11 console */

        /* Sleep to avoid using all CPU - since some interfaces needs to access 
         * keyboard events, a 100ms delay is a good compromise */
        msleep( INTF_IDLE_SLEEP );
    }

    /* End of interface thread - the main() function will close all remaining
     * output threads */
    EndInterface( p_intf );
    return ( 0 );
}

/* following functions are local */

/*******************************************************************************
 * StartInterface: prepare interface before main loop
 *******************************************************************************
 * This function opens output devices and create specific interfaces. It send
 * it's own error messages.
 *******************************************************************************/
static int StartInterface( intf_thread_t *p_intf )
{
    int i_thread;                                              /* thread index */
#ifdef INIT_SCRIPT
    int fd;
#endif

    /* Empty all threads array */
    for( i_thread = 0; i_thread < VOUT_MAX_THREADS; i_thread++ )
    {
        p_intf->pp_vout[i_thread] = NULL;        
    }
    for( i_thread = 0; i_thread < INPUT_MAX_THREADS; i_thread++ )
    {
        p_intf->pp_input[i_thread] = NULL;        
    }    

    /* Start X11 Console*/
    if( intf_OpenXConsole( &p_intf->xconsole ) )
    {
        intf_ErrMsg("intf error: can't open X11 console\n");
        return( 1 );
    }

#ifdef INIT_SCRIPT
    /* Execute the initialization script (typically spawn an input thread) */
    if ( (fd = open( INIT_SCRIPT, O_RDONLY )) != -1 )
    {
        /* Startup script does exist */
        close( fd );
        intf_ExecScript( INIT_SCRIPT );
    }
#endif

    return( 0 );
}

/*******************************************************************************
 * EndInterface: clean interface after main loop
 *******************************************************************************
 * This function destroys specific interfaces and close output devices.
 *******************************************************************************/
static void EndInterface( intf_thread_t *p_intf )
{
    int         i_thread;                                      /* thread index */
    boolean_t   b_thread;                          /* flag for remaing threads */
    int         pi_vout_status[VOUT_MAX_THREADS];       /* vout threads status */
    
    
    
    /* Close X11 console */
    intf_CloseXConsole( &p_intf->xconsole );        

    /* Destroy all remaining input threads */
    for( i_thread = 0; i_thread < INPUT_MAX_THREADS; i_thread++ )
    {
        if( p_intf->pp_input[i_thread] != NULL )
        {
            input_DestroyThread( p_intf->pp_input[i_thread] );
        }        
    }

    /* Destroy all remaining video output threads - all destruction orders are send,
     * then all THREAD_OVER status are received */
    for( i_thread = 0, b_thread = 0; i_thread < VOUT_MAX_THREADS; i_thread++ )
    {
        if( p_intf->pp_vout[i_thread] != NULL )
        {
            vout_DestroyThread( p_intf->pp_vout[i_thread], &pi_vout_status[i_thread] );
            b_thread = 1;            
        }
    }
    while( b_thread )
    {
        msleep( INTF_IDLE_SLEEP );        
        b_thread = 0;        
        for( i_thread = 0; i_thread < VOUT_MAX_THREADS; i_thread++ )
        {
            if( (p_intf->pp_vout[i_thread] != NULL) 
                && (pi_vout_status[i_thread] != THREAD_OVER) )
            {
                b_thread = 1;
            }     
        }
    }
    

}
