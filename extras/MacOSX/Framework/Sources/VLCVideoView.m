/*****************************************************************************
 * VLCVideoView.h: VLC.framework VLCVideoView implementation
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
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

#import "VLCVideoView.h"
#import "VLCLibrary.h"
#import "VLCEventManager.h"

/* Libvlc */
#include <vlc/vlc.h>
#include <vlc/libvlc.h>

/* Notifications */
NSString *VLCVideoViewEnteredFullScreen = @"VLCVideoViewEnteredFullScreen";
NSString *VLCVideoViewLeftFullScreen = @"VLCVideoViewLeftFullScreen";

/* This is a forward reference to VLCOpenGLVoutView specified in minimal_macosx
   library.  We could get rid of this, but it prevents warnings from the 
   compiler. */
@interface VLCOpenGLVoutView : NSView

- (void)detachFromVout;

@end

@implementation VLCVideoView

- (id)initWithFrame:(NSRect)rect
{
    if (self = [super initWithFrame:rect]) 
    {
        delegate = nil;
        [self setBackColor:[NSColor blackColor]];
        [self setStretchesVideo:NO];
        [self setAutoresizesSubviews:YES];
    }
    return self;
}

- (void)dealloc
{
    delegate = nil;
    [backColor release];
    [super dealloc];
}

- (void)setDelegate:(id)value
{
    delegate = value;
}

- (id)delegate
{
    return delegate;
}

- (void)setBackColor:(NSColor *)value
{
    if (backColor != value)
    {
        [backColor release];
        backColor = [value retain];
    }
}

- (NSColor *)backColor
{
    return backColor;
}

- (void)setStretchesVideo:(BOOL)value
{
    stretchesVideo = value;
}

- (BOOL)stretchesVideo
{
    return stretchesVideo;
}

/* This is called by the libvlc module 'minimal_macosx' as soon as there is one 
 * vout available 
 */
- (void)addVoutSubview:(NSView *)aView
{
    if( [[self subviews] count] )
    {
        /* XXX: This is a hack until core gets fixed */
        int i;
        for( i = 0; i < [[self subviews] count]; i++ )
        {
            [[[self subviews] objectAtIndex:i] detachFromVout];
            [[[self subviews] objectAtIndex:i] retain];
            [[[self subviews] objectAtIndex:i] removeFromSuperview];
        }
    }
    [self addSubview:aView];
    [aView setFrame:[self bounds]];
    
    // TODO: Should we let the media player specify these values?
    [aView setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
}

- (void)removeVoutSubview:(NSView *)view
{
    // Should we do something?  I don't know, however the protocol requires
    // this to be implemented
}

/* This is a LibVLC notification that we're about to enter into full screen,
   there is no other place where I can see where we can trap this event */
- (void)enterFullscreen
{
    // Go ahead and send a notification to the world we're going into full screen
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject:self 
                                                   withDelegateMethod:nil 
                                                 withNotificationName:VLCVideoViewEnteredFullScreen];
    
    // There is nothing else to do, as this object strictly displays the video feed
}

/* This is a LibVLC notification that we're about to enter leaving full screen,
   there is no other place where I can see where we can trap this event */
- (void)leaveFullscreen
{
    // Go ahead and send a notification to the world we're leaving full screen
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject:self 
                                                   withDelegateMethod:nil 
                                                 withNotificationName:VLCVideoViewLeftFullScreen];
    
    // There is nothing else to do, as this object strictly displays the video feed
}

- (void)drawRect:(NSRect)aRect
{
    [self lockFocus];
    [backColor set];
    NSRectFill(aRect);
    [self unlockFocus];
}

- (BOOL)isOpaque
{
    return YES;
}

@end
