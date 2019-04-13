/*****************************************************************************
 * intf.m: macOS minimal interface module
 *****************************************************************************
 * Copyright (C) 2002-2017 VLC authors and VideoLAN
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
#import <stdlib.h>
#import <string.h>
#import <unistd.h>

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_interface.h>
#import <vlc_vout_window.h>

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

    p_intf->p_sys = NULL;

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

static int WindowEnable(vout_window_t *p_wnd, const vout_window_cfg_t *cfg)
{
    @autoreleasepool {
        VLCMinimalVoutWindow __block *o_window;
        NSRect proposedVideoViewPosition = NSMakeRect(cfg->x, cfg->y, cfg->width, cfg->height);

        dispatch_sync(dispatch_get_main_queue(), ^{
            o_window = [[VLCMinimalVoutWindow alloc] initWithContentRect:proposedVideoViewPosition];
            [o_window makeKeyAndOrderFront:nil];
        });

        if (!o_window) {
            msg_Err(p_wnd, "window creation failed");
            return VLC_EGENERIC;
        }

        msg_Dbg(p_wnd, "returning video window with proposed position x=%i, y=%i, width=%i, height=%i", cfg->x, cfg->y, cfg->width, cfg->height);
        p_wnd->handle.nsobject = (void *)CFBridgingRetain([o_window contentView]);
    }

    if (cfg->is_fullscreen)
        vout_window_SetFullScreen(p_wnd, NULL);
    return VLC_SUCCESS;
}

static void WindowDisable(vout_window_t *p_wnd)
{
    @autoreleasepool {
        NSWindow * o_window = [(__bridge id)p_wnd->handle.nsobject window];
        if (o_window)
            o_window = nil;
    }
}

static void WindowResize(vout_window_t *p_wnd,
                         unsigned i_width, unsigned i_height)
{
    NSWindow* o_window = [(__bridge id)p_wnd->handle.nsobject window];

    @autoreleasepool {
        dispatch_sync(dispatch_get_main_queue(), ^{
            NSRect theFrame = [o_window frame];
            theFrame.size.width = i_width;
            theFrame.size.height = i_height;
            [o_window setFrame:theFrame display:YES animate:YES];
        });
    }
}

static void WindowSetState(vout_window_t *p_wnd, unsigned state)
{
    NSWindow* o_window = [(__bridge id)p_wnd->handle.nsobject window];

    [o_window setLevel:state];
}

static void WindowUnsetFullscreen(vout_window_t *p_wnd)
{
    NSWindow* o_window = [(__bridge id)p_wnd->handle.nsobject window];

    @autoreleasepool {
        dispatch_sync(dispatch_get_main_queue(), ^{
            [(VLCMinimalVoutWindow*)o_window leaveFullscreen];
        });
    }
}

static void WindowSetFullscreen(vout_window_t *p_wnd, const char *psz_id)
{
    NSWindow* o_window = [(__bridge id)p_wnd->handle.nsobject window];

    @autoreleasepool {
        dispatch_sync(dispatch_get_main_queue(), ^{
            [(VLCMinimalVoutWindow*)o_window enterFullscreen];
        });
    }
}

static void WindowClose(vout_window_t *);

static const struct vout_window_operations ops = {
    WindowEnable,
    WindowDisable,
    WindowResize,
    NULL,
    WindowSetState,
    WindowUnsetFullscreen,
    WindowSetFullscreen,
    NULL,
};

int WindowOpen(vout_window_t *p_wnd)
{
    p_wnd->type = VOUT_WINDOW_TYPE_NSOBJECT;
    p_wnd->ops = &ops;
    return VLC_SUCCESS;
}
