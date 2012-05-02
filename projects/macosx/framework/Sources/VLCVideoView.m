/*****************************************************************************
 * VLCVideoView.m: VLCKit.framework VLCVideoView implementation
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import "VLCVideoView.h"
#import "VLCLibrary.h"
#import "VLCEventManager.h"
#import "VLCVideoCommon.h"

/* Libvlc */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc/libvlc.h>

#import <QuartzCore/QuartzCore.h>

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

@interface VLCVideoView ()
/* Proeprties */
@property (readwrite) BOOL hasVideo;
@end

/******************************************************************************
 * Implementation VLCVideoView 
 */

@implementation VLCVideoView

/* Initializers */
- (id)initWithFrame:(NSRect)rect
{
    if (self = [super initWithFrame:rect]) 
    {
        self.delegate = nil;
        self.backColor = [NSColor blackColor];
        self.fillScreen = NO;
        self.hasVideo = NO;
        
        [self setStretchesVideo:NO];
        [self setAutoresizesSubviews:YES];
        layoutManager = [[VLCVideoLayoutManager layoutManager] retain];
    }
    return self;
}

- (void)dealloc
{
    self.delegate = nil;
    self.backColor = nil;
    [layoutManager release];
    [super dealloc];
}

/* NSView Overrides */
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

/* Properties */
@synthesize delegate;
@synthesize backColor;
@synthesize hasVideo;

- (BOOL)fillScreen
{
    return [layoutManager fillScreenEntirely];
}

- (void)setFillScreen:(BOOL)fillScreen
{
    [(VLCVideoLayoutManager *)layoutManager setFillScreenEntirely:fillScreen];
    [[self layer] setNeedsLayout];
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
    [aLayer setNeedsDisplayOnBoundsChange:YES];

    [CATransaction commit];
    self.hasVideo = YES;
}

- (void)removeVoutLayer:(CALayer *)voutLayer
{
    [CATransaction begin];
    [voutLayer removeFromSuperlayer];
    [CATransaction commit];
    self.hasVideo = NO;
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
