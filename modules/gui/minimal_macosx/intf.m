/*****************************************************************************
 * intf.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import <stdlib.h>                                      /* malloc(), free() */
#import <sys/param.h>                                    /* for MAXPATHLEN */
#import <string.h>
#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_playlist.h>
#import <vlc_vout_window.h>

#import "intf.h"
#import "VLCMinimalVoutWindow.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run (intf_thread_t *p_intf);

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int OpenIntf (vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    p_intf->p_sys = malloc(sizeof(intf_sys_t));
    if (p_intf->p_sys == NULL)
        return VLC_ENOMEM;

    memset(p_intf->p_sys, 0, sizeof(*p_intf->p_sys));

    Run(p_intf);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void CloseIntf (vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    free(p_intf->p_sys);
}

/* Dock Connection */
typedef struct CPSProcessSerNum
{
        UInt32                lo;
        UInt32                hi;
} CPSProcessSerNum;

extern OSErr    CPSGetCurrentProcess(CPSProcessSerNum *psn);
extern OSErr    CPSEnableForegroundOperation(CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr    CPSSetFrontProcess(CPSProcessSerNum *psn);

/*****************************************************************************
 * KillerThread: Thread that kill the application
 *****************************************************************************/
static void * KillerThread(void *user_data)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    intf_thread_t *p_intf = user_data;

    vlc_mutex_init(&p_intf->p_sys->lock);
    vlc_cond_init(&p_intf->p_sys->wait);

    vlc_mutex_lock (&p_intf->p_sys->lock);
    while(vlc_object_alive(p_intf))
        vlc_cond_wait(&p_intf->p_sys->wait, &p_intf->p_sys->lock);
    vlc_mutex_unlock(&p_intf->p_sys->lock);

    vlc_mutex_destroy(&p_intf->p_sys->lock);
    vlc_cond_destroy(&p_intf->p_sys->wait);

    /* We are dead, terminate */
    [NSApp terminate: nil];
    [o_pool release];
    return NULL;
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run(intf_thread_t *p_intf)
{
    sigset_t set;

    /* Make sure the "force quit" menu item does quit instantly.
     * VLC overrides SIGTERM which is sent by the "force quit"
     * menu item to make sure deamon mode quits gracefully, so
     * we un-override SIGTERM here. */
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    /* Setup a thread that will monitor the module killing */
    pthread_t killer_thread;
    pthread_create(&killer_thread, NULL, KillerThread, p_intf);

    CPSProcessSerNum PSN;
    NSAutoreleasePool   *pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    if (!CPSGetCurrentProcess(&PSN))
        if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
            if (!CPSSetFrontProcess(&PSN))
                [NSApplication sharedApplication];
    [NSApp run];

    pthread_join(killer_thread, NULL);

    [pool release];
}

/*****************************************************************************
 * Vout window management
 *****************************************************************************/
static int WindowControl(vout_window_t *, int i_query, va_list);

int WindowOpen(vout_window_t *p_wnd, const vout_window_cfg_t *cfg)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    NSRect proposedVideoViewPosition = NSMakeRect(cfg->x, cfg->y, cfg->width, cfg->height);

    VLCMinimalVoutWindow *o_window = [[VLCMinimalVoutWindow alloc] initWithContentRect:proposedVideoViewPosition];
    [o_window makeKeyAndOrderFront:nil];

    if (!o_window) {
        msg_Err(p_wnd, "window creation failed");
        [o_pool release];
        return VLC_EGENERIC;
    }

    msg_Dbg(p_wnd, "returning video window with proposed position x=%i, y=%i, width=%i, height=%i", cfg->x, cfg->y, cfg->width, cfg->height);
    p_wnd->handle.nsobject = [o_window contentView];

    // TODO: find a cleaner way for "start in fullscreen"
    if (var_GetBool(pl_Get(p_wnd), "fullscreen"))
        [o_window performSelectorOnMainThread:@selector(enterFullscreen) withObject:nil waitUntilDone:NO];

    p_wnd->control = WindowControl;

    [o_pool release];
    return VLC_SUCCESS;
}

static int WindowControl(vout_window_t *p_wnd, int i_query, va_list args)
{
    NSWindow * o_window = [(id)p_wnd->handle.nsobject window];
    if (!o_window) {
        msg_Err(p_wnd, "failed to recover cocoa window");
        return VLC_EGENERIC;
    }

    switch (i_query) {
        case VOUT_WINDOW_SET_STATE:
        {
            unsigned i_state = va_arg(args, unsigned);

            [o_window setLevel: i_state];

            return VLC_SUCCESS;
        }
        case VOUT_WINDOW_SET_SIZE:
        {
            NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

            NSRect theFrame = [o_window frame];
            unsigned int i_width  = va_arg(args, unsigned int);
            unsigned int i_height = va_arg(args, unsigned int);
            theFrame.size.width = i_width;
            theFrame.size.height = i_height;
            [o_window setFrame: theFrame display: YES animate: YES];

            [o_pool release];
            return VLC_SUCCESS;
        }
        case VOUT_WINDOW_SET_FULLSCREEN:
        {
            NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
            int i_full = va_arg(args, int);

            if (i_full)
                [o_window performSelectorOnMainThread:@selector(enterFullscreen) withObject:nil waitUntilDone:NO];
            else
                [o_window performSelectorOnMainThread:@selector(leaveFullscreen) withObject:nil waitUntilDone:NO];

            [o_pool release];
            return VLC_SUCCESS;
        }
        default:
            msg_Warn(p_wnd, "unsupported control query");
            return VLC_EGENERIC;
    }
}

void WindowClose(vout_window_t *p_wnd)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    NSWindow * o_window = [(id)p_wnd->handle.nsobject window];
    if (o_window)
        [o_window release];

    [o_pool release];
}

