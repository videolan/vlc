/******************************************************************************
 * intf_3dfx.c: 3dfx interface
 * (c)2000 VideoLAN
 ******************************************************************************/

/******************************************************************************
 * Preamble
 ******************************************************************************/
#include <errno.h>
#include <signal.h>
#include <stdio.h> /* stderr */
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>                                                /* close() */
#include <sys/uio.h>                                           /* for input.h */

#include <sys/types.h>              /* open() */
#include <sys/stat.h>
#include <fcntl.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "input.h"
#include "video.h"
#include "video_output.h"
#include "intf_sys.h"
#include "intf_msg.h"
#include "interface.h"
#include "main.h"

/******************************************************************************
 * intf_sys_t: description and status of 3dfx interface
 ******************************************************************************/
typedef struct intf_sys_s
{
   
} intf_sys_t;

/******************************************************************************
 * intf_SysCreate: initialize 3dfx interface
 ******************************************************************************/
int intf_SysCreate( intf_thread_t *p_intf )
{
    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };
    intf_DbgMsg("0x%x\n", p_intf );

    /* Spawn video output thread */
    if( p_main->b_video )
    {
        p_intf->p_vout = vout_CreateThread( NULL, 0, 0, 0, NULL);
        if( p_intf->p_vout == NULL )                                /* error */
        {
            intf_ErrMsg("intf error: can't create output thread\n" );
            return( 1 );
        }
    }
    return( 0 );
}

/******************************************************************************
 * intf_SysDestroy: destroy 3dfx interface
 ******************************************************************************/
void intf_SysDestroy( intf_thread_t *p_intf )
{
    /* Close input thread, if any (blocking) */
    if( p_intf->p_input )
    {
	input_DestroyThread( p_intf->p_input, NULL );
    }

    /* Close video output thread, if any (blocking) */
    if( p_intf->p_vout )
    {
	vout_DestroyThread( p_intf->p_vout, NULL );
    }

    /* Destroy structure */
    free( p_intf->p_sys );
}


/******************************************************************************
 * intf_SysManage: event loop
 ******************************************************************************/
void intf_SysManage( intf_thread_t *p_intf )
{
    ;
}

