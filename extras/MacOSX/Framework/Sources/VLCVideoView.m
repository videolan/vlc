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

#import <QuartzCore/QuartzCore.h>

/* Notifications */
NSString * VLCVideoViewEnteredFullScreen    = @"VLCVideoViewEnteredFullScreen";
NSString * VLCVideoViewLeftFullScreen       = @"VLCVideoViewLeftFullScreen";

/******************************************************************************
 * Soon deprecated stuff 
 */

/* This is a forward reference to VLCOpenGLVoutView specified in minimal_macosx
   library.  We could get rid of this, but it prevents warnings from the 
   compiler. (Scheduled to deletion) */
@interface VLCOpenGLVoutView : NSView
- (void)detachFromVout;
@end

/* Depreacted methods */
@interface VLCVideoView (Deprecated)
- (void)setStretchesVideo:(BOOL)value;
- (BOOL)stretchesVideo;

- (void)addVoutSubview:(NSView *)aView;  /* (Scheduled to deletion) */
- (void)removeVoutSubview:(NSView *)aView;  /* (Scheduled to deletion) */
@end

/******************************************************************************
 * VLCVideoView (Private) 
 */

@interface VLCVideoView (Private)
/* Method */
- (void)addVoutLayer:(CALayer *)aLayer;
@end

/******************************************************************************
 * Interface & Implementation VLCConstraintLayoutManager
 *
 * Manage the size of the video layer 
 */
@interface VLCConstraintLayoutManager : CAConstraintLayoutManager
{
    CGSize originalVideoSize;
    BOOL  fillScreenEntirely;
}
@property BOOL  fillScreenEntirely;
@property CGSize originalVideoSize;
@end

@implementation VLCConstraintLayoutManager 
@synthesize  fillScreenEntirely;
@synthesize  originalVideoSize;
- (id)init
{
    if( self = [super init] )
    {
        self.originalVideoSize = CGSizeMake(0., 0.);
        self.fillScreenEntirely = NO;
    }
    return self;

}
+ (id)layoutManager
{
    return [[[VLCConstraintLayoutManager alloc] init] autorelease];
}
- (void)layoutSublayersOfLayer:(CALayer *)layer
{
    /* Called by CA, when our rootLayer needs layout */
    [super layoutSublayersOfLayer:layer];

    /* After having done everything normally resize the vlcopengllayer */
    if( [[layer sublayers] count] > 0 && [[[[layer sublayers] objectAtIndex:0] name] isEqualToString:@"vlcopengllayer"])
    {
        CALayer * videolayer = [[layer sublayers] objectAtIndex:0];
        CGRect bounds = layer.bounds;
        float new_height = (bounds.size.width * originalVideoSize.height) / originalVideoSize.width;

        if( fillScreenEntirely )
        {
            if( bounds.size.height > new_height )
                bounds.size.height = new_height;
            else
                bounds.size.width = (bounds.size.height * originalVideoSize.width) / originalVideoSize.height;
        }
        else
        {
            if( bounds.size.height > new_height )
                bounds.size.width = (bounds.size.height * originalVideoSize.width) / originalVideoSize.height;
            else
                bounds.size.height = new_height;
        }

        bounds.origin = CGPointMake( 0.0, 0.0 );
        videolayer.bounds = bounds;
        videolayer.position = CGPointMake((layer.bounds.size.width - layer.bounds.origin.x)/2, (layer.bounds.size.height - layer.bounds.origin.y)/2);
    }
}
@end

/******************************************************************************
 * Implementation VLCVideoView 
 */

@implementation VLCVideoView

- (BOOL)fillScreen
{
    return [layoutManager fillScreenEntirely];
}
- (void)setFillScreen:(BOOL)fillScreen
{
    [layoutManager setFillScreenEntirely:fillScreen];
    [[self layer] setNeedsLayout];
}

- (BOOL)fullScreen
{
    return fullScreen;
}

- (void)setFullScreen:(BOOL)newFullScreen
{
    if( newFullScreen )
    {
        fullScreen = YES;
        [self enterFullscreen];
    }
    else
    {
        fullScreen = NO;
        [self leaveFullscreen];
    }
}


- (id)initWithFrame:(NSRect)rect
{
    if (self = [super initWithFrame:rect]) 
    {
        delegate = nil;
        [self setBackColor:[NSColor blackColor]];
        [self setStretchesVideo:NO];
        [self setAutoresizesSubviews:YES];
        [self setFillScreen: NO];
        layoutManager = [[VLCConstraintLayoutManager layoutManager] retain];
    }
    return self;
}

- (void)dealloc
{
    [layoutManager release];
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

/* This is a LibVLC notification that we're about to enter into full screen,
   there is no other place where I can see where we can trap this event */
- (void)enterFullscreen
{
    // Go ahead and send a notification to the world we're going into full screen
    [[VLCEventManager sharedManager] callOnMainThreadDelegateOfObject:self 
                                                   withDelegateMethod:nil 
                                                 withNotificationName:VLCVideoViewEnteredFullScreen];
    
    [super enterFullScreenMode:[[self window] screen] withOptions:nil];
    if( !self.fullScreen ) self.fullScreen = YES;
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
    [super exitFullScreenModeWithOptions:nil];
    if( self.fullScreen ) self.fullScreen = NO;
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

- (void)mouseDown:(NSEvent *)theEvent
{
    if([theEvent clickCount] != 2)
        return;
    if(self.fullScreen)
        [self leaveFullscreen];
    else
        [self enterFullscreen];
}
@end

/******************************************************************************
 * Implementation VLCVideoView  (Private)
 */

@implementation VLCVideoView (Private)

/* This is called by the libvlc module 'opengllayer' as soon as there is one 
 * vout available
 */
- (void)addVoutLayer:(CALayer *)aLayer
{
    [CATransaction begin];
    [self setWantsLayer: YES];
	CALayer * rootLayer = [self layer];

    aLayer.name = @"vlcopengllayer";

    [layoutManager setOriginalVideoSize:aLayer.bounds.size];
    [rootLayer setLayoutManager:layoutManager];
    [rootLayer insertSublayer:aLayer atIndex:0];

    [aLayer setNeedsLayout];
    [aLayer setNeedsDisplay];
    [rootLayer setNeedsDisplay];
    [rootLayer layoutIfNeeded];
    [CATransaction commit];
}

@end

/******************************************************************************
 * Implementation VLCVideoView  (Deprecated)
 */

@implementation VLCVideoView (Deprecated)

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
- (void)addVoutSubview:(NSView *)aView /* (Scheduled to deletion) */
{
    /* This is where the real video comes from */
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

    [aView setFrame:[self bounds]];
    
    [self addSubview:aView];

    // TODO: Should we let the media player specify these values?
    [aView setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
}

- (void)removeVoutSubview:(NSView *)view /* (Scheduled to deletion) */
{
    // Should we do something?  I don't know, however the protocol requires
    // this to be implemented
}
@end
