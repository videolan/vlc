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
#include <unistd.h>

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_vout_window.h>

#import "VLCMain.h"
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
    msg_Dbg(p_intf, "Using minimal macosx interface");

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
 * Run: main loop
 *****************************************************************************/
static void Run(intf_thread_t *p_intf)
{
    CPSProcessSerNum PSN;
    @autoreleasepool {
        [NSApplication sharedApplication];
        if (!CPSGetCurrentProcess(&PSN))
            if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
                if (!CPSSetFrontProcess(&PSN))
                    [NSApplication sharedApplication];
    }
}

/*****************************************************************************
 * Vout window management
 *****************************************************************************/
static int WindowControl(vout_window_t *, int i_query, va_list);

int WindowOpen(vout_window_t *p_wnd, const vout_window_cfg_t *cfg)
{
    if (cfg->type != VOUT_WINDOW_TYPE_INVALID
     && cfg->type != VOUT_WINDOW_TYPE_NSOBJECT)
        return VLC_EGENERIC;

    @autoreleasepool {
        NSRect proposedVideoViewPosition = NSMakeRect(cfg->x, cfg->y, cfg->width, cfg->height);

        VLCMinimalVoutWindow *o_window = [[VLCMinimalVoutWindow alloc] initWithContentRect:proposedVideoViewPosition];
        [o_window makeKeyAndOrderFront:nil];

        if (!o_window) {
            msg_Err(p_wnd, "window creation failed");
            return VLC_EGENERIC;
        }

        msg_Dbg(p_wnd, "returning video window with proposed position x=%i, y=%i, width=%i, height=%i", cfg->x, cfg->y, cfg->width, cfg->height);
        p_wnd->handle.nsobject = (void *)CFBridgingRetain([o_window contentView]);

        p_wnd->type = VOUT_WINDOW_TYPE_NSOBJECT;
        p_wnd->control = WindowControl;
    }

    vout_window_SetFullScreen(p_wnd, cfg->is_fullscreen);
    return VLC_SUCCESS;
}

static int WindowControl(vout_window_t *p_wnd, int i_query, va_list args)
{
    NSWindow * o_window = [(__bridge id)p_wnd->handle.nsobject window];
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
            @autoreleasepool {
                NSRect theFrame = [o_window frame];
                unsigned int i_width  = va_arg(args, unsigned int);
                unsigned int i_height = va_arg(args, unsigned int);
                theFrame.size.width = i_width;
                theFrame.size.height = i_height;
                [o_window setFrame: theFrame display: YES animate: YES];
            }
            return VLC_SUCCESS;
        }
        case VOUT_WINDOW_SET_FULLSCREEN:
        {
            @autoreleasepool {
                int i_full = va_arg(args, int);

                if (i_full)
                    [o_window performSelectorOnMainThread:@selector(enterFullscreen) withObject:nil waitUntilDone:NO];
                else
                    [o_window performSelectorOnMainThread:@selector(leaveFullscreen) withObject:nil waitUntilDone:NO];
            }
            return VLC_SUCCESS;
        }
        default:
            msg_Warn(p_wnd, "unsupported control query");
            return VLC_EGENERIC;
    }
}

void WindowClose(vout_window_t *p_wnd)
{
    @autoreleasepool {
        NSWindow * o_window = [(__bridge id)p_wnd->handle.nsobject window];
        if (o_window)
            o_window = nil;
    }
}
