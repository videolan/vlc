/*******************************************************************************
 * control.c: user control functions
 * (c)1999 VideoLAN
 *******************************************************************************
 * Library of functions common to all threads, allowing access to various
 * structures and settings. Interfaces should only use those functions
 * to read or write informations from other threads.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <stdio.h>
#include <netinet/in.h>
#include <sys/soundcard.h>
#include <sys/uio.h>
#include <X11/Xlib.h> 
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "input.h"
#include "input_vlan.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"

#include "xconsole.h"
#include "interface.h"
#include "intf_msg.h"
#include "control.h"

#include "pgm_data.h"

/*******************************************************************************
 * intf_CreateVoutThread: create video output thread in interface
 *******************************************************************************
 * This function creates - if possible - a new video output thread in the
 * interface registery, using interface default settings. It returns the
 * thread number for the interface, or a negative number.
 * If video is desactivated, nothing will be done. If psz_title is not NULL, it
 * will be used as window's title, and width and height will also be used if
 * they are positive.
 *******************************************************************************/
int intf_CreateVoutThread( intf_thread_t *p_intf, char *psz_title, int i_width, int i_height )
{
    int             i_thread;                                  /* thread index */
    video_cfg_t     cfg;                               /* thread configuration */    

    /* Verify that video is enabled */
    if( !p_program_data->cfg.b_video )
    {
        return( -1 );
    }

    /* Set configuration */
    memcpy( &cfg, &p_program_data->vout_cfg, sizeof( cfg ) );
    if( psz_title != NULL )
    {
        cfg.i_properties |= VIDEO_CFG_TITLE;
        cfg.psz_title = psz_title;        
    }
    if( i_width > 0 )
    {
        cfg.i_properties |= VIDEO_CFG_WIDTH;
        cfg.i_width = i_width;        
    }
    if( i_height > 0 )
    {
        cfg.i_properties |= VIDEO_CFG_HEIGHT;
        cfg.i_height = i_height;
    }

    /* Find an empty place */
    for( i_thread = 0; i_thread < VOUT_MAX_THREADS; i_thread++ )
    {
        if( p_intf->pp_vout[i_thread] == NULL )
        {
            /* The current place is empty: create a thread */
            p_intf->pp_vout[i_thread] = vout_CreateThread( &cfg, NULL );
            if( p_intf->pp_vout[i_thread] == NULL )                   /* error */
            {                    
                return( -1 );
            }
        }
    }

    /* No empty place has been found */
    return( -1 );
}


/*******************************************************************************
 * intf_DestroyVoutThread: destroy video output thread in interface
 *******************************************************************************
 * This function destroy a video output thread created with
 * intf_CreateVoutThread().
 *******************************************************************************/
void intf_DestroyVoutThread( intf_thread_t *p_intf, int i_thread )
{
#ifdef DEBUG
    /* Check if thread still exists */
    if( p_intf->pp_vout[i_thread] == NULL )
    {
        intf_DbgMsg("intf error: destruction of an inexistant vout thread\n");
        return;
    }
#endif

    /* Destroy thread and marks its place as empty */
    vout_DestroyThread( p_intf->pp_vout[i_thread], NULL );
    p_intf->pp_vout[i_thread] = NULL;
}


/*******************************************************************************
 * intf_CreateInputThread: create input thread in interface
 *******************************************************************************
 * This function creates - if possible - a new input thread in the
 * interface registery, using interface default settings. It returns the
 * thread number for the interface, or a negative number.
 *******************************************************************************/
int intf_CreateInputThread( intf_thread_t *p_intf, input_cfg_t* p_cfg )
{
    int             i_thread;                                  /* thread index */

    /* Find an empty place */
    for( i_thread = 0; i_thread < INPUT_MAX_THREADS; i_thread++ )
    {
        if( p_intf->pp_input[i_thread] == NULL )
        {
            /* The current place is empty: create a thread and return */
            p_intf->pp_input[i_thread] = input_CreateThread( p_cfg );
            return( (p_intf->pp_input[i_thread] != NULL) ? i_thread : -1 );
        }
    }

    /* No empty place has been found */
    return( -1 );    
}

/*******************************************************************************
 * intf_DestroyInputThread: destroy input thread in interface
 *******************************************************************************
 * This function destroy aa input thread created with
 * intf_CreateInputThread().
 *******************************************************************************/
void intf_DestroyInputThread( intf_thread_t *p_intf, int i_thread )
{
#ifdef DEBUG
    /* Check if thread still exists */
    if( p_intf->pp_input[i_thread] == NULL )
    {
        intf_DbgMsg("intf error: destruction of an inexistant input thread\n");
        return;        
    }
#endif

    /* Destroy thread and marks its place as empty */
    input_DestroyThread( p_intf->pp_input[i_thread] );
    p_intf->pp_input[i_thread] = NULL;
}
