/*****************************************************************************
 * VLCMinimalVoutWindow.m: MacOS X Minimal interface window
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include "intf.h"
#include "voutgl.h"
#include "VLCOpenGLVoutView.h"
#include "VLCMinimalVoutWindow.h"

/* SetSystemUIMode, ... */
#import <QuickTime/QuickTime.h>

#import <Cocoa/Cocoa.h>

@implementation VLCMinimalVoutWindow
- (id)initWithContentRect:(NSRect)contentRect
{
    if( self = [super initWithContentRect:contentRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO])
    {
        rect = contentRect;
        [self setBackgroundColor:[NSColor blackColor]];
        [self setMovableByWindowBackground: YES];
    }
    return self;
}
- (void)addVoutSubview:(NSView *)view
{
    [[self contentView] addSubview:view];
    [view setFrame:[[self contentView] bounds]];
}

- (void)removeVoutSubview:(NSView *)view
{
    [self close];
    [self release];
}

- (void)enterFullscreen
{
    SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
    [self setFrame:[[self screen] frame] display: YES];
}

- (void)leaveFullscreen
{
    SetSystemUIMode( kUIModeNormal, kUIOptionAutoShowMenuBar);
    [self setFrame:rect display: YES];
}

- (BOOL)stretchesVideo
{
    return NO;
}

- (void)setOnTop: (BOOL)ontop
{

}
@end

