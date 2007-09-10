/*****************************************************************************
 * eyetv.c : Access module to connect to our plugin running within EyeTV
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Author: Felix Kühne <fkuehne at videolan dot org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>
#include <vlc_access.h>

#include <CoreFoundation/CoreFoundation.h>

/* TODO:
 * watch for PluginQuit or DeviceRemoved to stop output to VLC's core then */

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( "EyeTV" );
    set_description( _("EyeTV access module") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    set_capability( "access2", 0 );
    add_shortcut( "eyetv" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
typedef struct
{
    VLC_COMMON_MEMBERS
 
    vlc_mutex_t     lock;
    vlc_cond_t      wait;
 
    CFMessagePortRef    inputMessagePortFromEyeTV;
} eyetv_thread_t;

struct access_sys_t
{
    eyetv_thread_t      *p_thread;
};

CFDataRef dataFromEyetv;
int lastPacketId;
int lastForwardedPacketId;

static int Read( access_t *, uint8_t *, int );
static int Control( access_t *, int, va_list );
static void Thread( vlc_object_t * );
CFDataRef msgPortCallback( CFMessagePortRef local, SInt32 msgid, CFDataRef data, void *info );

/*****************************************************************************
 * Open: sets up the module and its threads
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t        *p_access = (access_t *)p_this;
    access_sys_t    *p_sys;
    eyetv_thread_t  *p_thread;
    CFMessagePortContext context;
    memset(&context, 0, sizeof(context));
 
    /* Init p_access */
    access_InitFields( p_access ); \
    ACCESS_SET_CALLBACKS( Read, NULL, Control, NULL ); \
    MALLOC_ERR( p_access->p_sys, access_sys_t ); \
    p_sys = p_access->p_sys; memset( p_sys, 0, sizeof( access_sys_t ) );

    msg_Dbg( p_access, "coming up" );

    /* create receiving thread which will keep the message port alive without blocking */
    p_sys->p_thread = p_thread = vlc_object_create( p_access, sizeof( eyetv_thread_t ) );
    vlc_object_attach( p_thread, p_this );
    vlc_mutex_init( p_access, &p_thread->lock );
    vlc_cond_init( p_access, &p_thread->wait );
    msg_Dbg( p_access, "thread created, msg port following now" );
 
    /* set up our own msg port
    * we may give the msgport such a generic name, because EyeTV may only run
    * once per entire machine, so we can't interfere with other instances.
    * we just trust the user no to launch multiple VLC instances trying to
    * access EyeTV at the same time. If this happens, the latest launched
    * instance will win. */
    p_sys->p_thread->inputMessagePortFromEyeTV =  CFMessagePortCreateLocal( kCFAllocatorDefault,
                                                                  CFSTR("VLCEyeTVMsgPort"),
                                                                  &msgPortCallback,
                                                                  &context,
                                                                  /* no info to free */ NULL );
    if( p_sys->p_thread->inputMessagePortFromEyeTV == NULL )
    {
        msg_Err( p_access, "opening local msg port failed" );
        free( p_sys->p_thread->inputMessagePortFromEyeTV );
        vlc_mutex_destroy( &p_thread->lock );
        vlc_cond_destroy( &p_thread->wait );
        vlc_object_detach( p_thread );
        vlc_object_destroy( p_thread );
        free( p_sys );
        return VLC_EGENERIC;
    }
    else
        msg_Dbg( p_access, "remote msg port opened" );
 
    /* let the thread run */
    if( vlc_thread_create( p_thread, "EyeTV Receiver Thread", Thread,
                           VLC_THREAD_PRIORITY_HIGHEST, VLC_FALSE ) )
    {
        msg_Err( p_access, "couldn't launch eyetv receiver thread" );
        vlc_mutex_destroy( &p_thread->lock );
        vlc_cond_destroy( &p_thread->wait );
        vlc_object_detach( p_thread );
        vlc_object_destroy( p_thread );
        free( p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_access, "receiver thread created and launched" );
 
    /* tell the EyeTV plugin to open up its msg port and start sending */
    CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter (),
                                          CFSTR("VLCAccessStartDataSending"),
                                          CFSTR("VLCEyeTVSupport"),
                                          /*userInfo*/ NULL,
                                          TRUE );
 
    msg_Dbg( p_access, "plugin notified" );
 
    /* we don't need such a high priority */
    //vlc_thread_set_priority( p_access, VLC_THREAD_PRIORITY_LOW );
 
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: closes msg-port, free resources
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t *)p_this;
    access_sys_t *p_sys = p_access->p_sys;
 
    msg_Dbg( p_access, "closing" );
 
    /* tell the EyeTV plugin to close its msg port and stop sending */
    CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter (),
                                          CFSTR("VLCAccessStopDataSending"),
                                          CFSTR("VLCEyeTVSupport"),
                                          /*userInfo*/ NULL,
                                          TRUE );
 
    msg_Dbg( p_access, "plugin notified" );
 
    /* stop receiver thread */
    vlc_object_kill( p_sys->p_thread );
    vlc_mutex_lock( &p_sys->p_thread->lock );
    vlc_cond_signal( &p_sys->p_thread->wait );
    vlc_mutex_unlock( &p_sys->p_thread->lock );
    vlc_thread_join( p_sys->p_thread );
 
    /* close msg port */
    CFMessagePortInvalidate( p_sys->p_thread->inputMessagePortFromEyeTV );
    free( p_sys->p_thread->inputMessagePortFromEyeTV );
    msg_Dbg( p_access, "msg port closed and freed" );
 
    /* free thread */
    vlc_mutex_destroy( &p_sys->p_thread->lock );
    vlc_cond_destroy( &p_sys->p_thread->wait );
    vlc_object_detach( p_sys->p_thread );
    vlc_object_destroy( p_sys->p_thread );
 
    free( p_sys );
}

static void Thread( vlc_object_t *p_this )
{
    eyetv_thread_t *p_thread= (eyetv_thread_t*)p_this;
    CFRunLoopSourceRef runLoopSource;
 
    /* create our run loop source for the port and attach it to our current loop */
    runLoopSource = CFMessagePortCreateRunLoopSource( kCFAllocatorDefault,
                                                      p_thread->inputMessagePortFromEyeTV,
                                                      0 );
    CFRunLoopAddSource( CFRunLoopGetCurrent(),
                        runLoopSource,
                        kCFRunLoopDefaultMode );
 
    CFRunLoopRun();
}


/*****************************************************************************
* msgPortCallback: receives data from the EyeTV plugin
*****************************************************************************/
CFDataRef msgPortCallback( CFMessagePortRef local, SInt32 msgid, CFDataRef data, void *info )
{
    extern CFDataRef dataFromEyetv;
    extern int lastPacketId;
 
    /* copy callback data to module data */
    dataFromEyetv = CFDataCreateCopy( kCFAllocatorDefault, data );
#if 0
    printf( "packet %i contained %i bytes, forwarding %i bytes\n",
            (int)msgid,
            (int)CFDataGetLength( data ),
            (int)CFDataGetLength( dataFromEyetv ) );
#endif

    lastPacketId = msgid;
 
    return NULL; /* we've got nothing to return */
}

/*****************************************************************************
* Read: forwarding data from EyeTV plugin which was received above
*****************************************************************************/
static int Read( access_t *p_access, uint8_t *p_buffer, int i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    extern CFDataRef dataFromEyetv;
    extern int lastPacketId;
    extern int lastForwardedPacketId;
 
    /* wait for a new buffer before forwarding */
    while( lastPacketId == lastForwardedPacketId && !p_access->b_die )
    {
        msleep( INPUT_ERROR_SLEEP );
    }
 
    /* read data here, copy it to p_buffer, fill i_len with respective length
     * and return info with i_read; i_read = 0 == EOF */
    if( dataFromEyetv )
    {
        CFDataGetBytes( dataFromEyetv,
                        CFRangeMake( 0, CFDataGetLength( dataFromEyetv ) ),
                        (uint8_t *)p_buffer );
        i_len = (int)CFDataGetLength( dataFromEyetv );
#if 0
        msg_Dbg( p_access, "%i bytes with id %i received in read function, pushing to core",
             (int)CFDataGetLength( dataFromEyetv ), lastPacketId );
#endif
        lastForwardedPacketId = lastPacketId;
        if( i_len == 0)
        {
            msg_Err( p_access, "you looosed!" );
            return 0;
        }
    }
 
    if( p_access->b_die )
        return 0;
 
    return i_len;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{/*
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;
 
    switch( i_query )
    {
        * *
        case ACCESS_SET_PAUSE_STATE:
            * Nothing to do *
            break;

        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
        case ACCESS_GET_MTU:
        case ACCESS_GET_PTS_DELAY:
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;
 
        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;
 
    }
    return VLC_SUCCESS;*/
    return VLC_EGENERIC;
}
