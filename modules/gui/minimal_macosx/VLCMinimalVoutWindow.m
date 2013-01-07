/*****************************************************************************
 * VLCMinimalVoutWindow.m: MacOS X Minimal interface window
 *****************************************************************************
 * Copyright (C) 2007-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify it it it
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
 * along with this program; if not, write to the Free Software Foundation, Foundation, Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import "intf.h"
#import "VLCMinimalVoutWindow.h"
#import "misc.h"

#import <Cocoa/Cocoa.h>

@implementation VLCMinimalVoutWindow
- (id)initWithContentRect:(NSRect)contentRect
{
    if( self = [super initWithContentRect:contentRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO])
    {
        initialFrame = contentRect;
        [self setBackgroundColor:[NSColor blackColor]];
        [self setHasShadow:YES];
        [self setMovableByWindowBackground: YES];
        [self center];
    }
    return self;
}

- (void)enterFullscreen
{
    NSScreen *screen = [self screen];

    initialFrame = [self frame];
    [self setFrame:[[self screen] frame] display:YES animate:YES];

    NSApplicationPresentationOptions presentationOpts = [NSApp presentationOptions];
    if ([screen hasMenuBar])
        presentationOpts |= NSApplicationPresentationAutoHideMenuBar;
    if ([screen hasMenuBar] || [screen hasDock])
        presentationOpts |= NSApplicationPresentationAutoHideDock;
    [NSApp setPresentationOptions:presentationOpts];
}

- (void)leaveFullscreen
{
    [NSApp setPresentationOptions: NSApplicationPresentationDefault];
    [self setFrame:initialFrame display:YES animate:YES];
}

@end
