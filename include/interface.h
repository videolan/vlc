/*****************************************************************************
 * interface.h: interface access for other threads
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
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
 * Required headers:
 *  <sys/uio.h>
 *  <X11/Xlib.h>
 *  <X11/extensions/XShm.h>
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "threads.h"
 *  "input.h"
 *  "video.h"
 *  "video_output.h"
 *  "audio_output.h"
 *  "xconsole.h"
 *****************************************************************************/

/*****************************************************************************
 * intf_thread_t: describe an interface thread
 *****************************************************************************
 * This structe describes all interface-specific data of the main (interface)
 * thread.
 *****************************************************************************/
typedef int   ( intf_sys_create_t )   ( p_intf_thread_t p_intf );
typedef void  ( intf_sys_destroy_t )  ( p_intf_thread_t p_intf );
typedef void  ( intf_sys_manage_t )   ( p_intf_thread_t p_intf );

typedef struct _key
{
    int received_key;
    int forwarded_key;
    struct _key *  next;
} intf_key;

typedef intf_key * p_intf_key;

typedef struct intf_thread_s
{
    boolean_t           b_die;                                 /* `die' flag */

    /* Specific interfaces */
    p_intf_console_t    p_console;                                /* console */
    p_intf_sys_t        p_sys;                           /* system interface */
    p_intf_key          p_keys;
    
    /* Plugin */
    intf_sys_create_t *     p_sys_create;         /* create interface thread */
    intf_sys_manage_t *     p_sys_manage;                       /* main loop */
    intf_sys_destroy_t *    p_sys_destroy;              /* destroy interface */

    /* XXX: Channels array - new API */
  //p_intf_channel_t *  p_channel[INTF_MAX_CHANNELS];/* channel descriptions */

    /* Channels array - NULL if not used */
    p_intf_channel_t    p_channel;                /* description of channels */

    /* Main threads - NULL if not active */
    p_vout_thread_t     p_vout;
    p_input_thread_t    p_input;

} intf_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
intf_thread_t * intf_Create             ( void );
void            intf_Run                ( intf_thread_t * p_intf );
void            intf_Destroy            ( intf_thread_t * p_intf );

int             intf_SelectChannel      ( intf_thread_t * p_intf, int i_channel );
int             intf_ProcessKey         ( intf_thread_t * p_intf, int i_key );

void intf_AssignKey( intf_thread_t *p_intf, int r_key, int f_key);

int intf_getKey( intf_thread_t *p_intf, int r_key);

void intf_AssignNormalKeys( intf_thread_t *p_intf);

