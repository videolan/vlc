/*****************************************************************************
 * VLCVideoLayer.m: VLCKit.framework VLCVideoLayer implementation
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

#import "VLCVideoLayer.h"
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
 * VLCVideoView (Private) 
 */

@interface VLCVideoLayer (Private)
/* Method */
- (void)addVoutLayer:(CALayer *)aLayer;
@end

@interface VLCVideoLayer ()
@property (readwrite) BOOL hasVideo;
@end

/******************************************************************************
 * Implementation VLCVideoLayer 
 */

@implementation VLCVideoLayer
@synthesize hasVideo;

- (BOOL)fillScreen
{
    return [self.layoutManager fillScreenEntirely];
}

- (void)setFillScreen:(BOOL)fillScreen
{
    [self.layoutManager setFillScreenEntirely:fillScreen];
    [self setNeedsLayout];
}

@end

/******************************************************************************
 * Implementation VLCVideoLayer  (Private)
 */

@implementation VLCVideoLayer (Private)


/* This is called by the libvlc module 'opengllayer' as soon as there is one 
 * vout available
 */
- (void)addVoutLayer:(CALayer *)voutLayer
{
    [CATransaction begin];
 
    voutLayer.name = @"vlcopengllayer";
    
    VLCVideoLayoutManager * layoutManager = [VLCVideoLayoutManager layoutManager];
    layoutManager.originalVideoSize = voutLayer.bounds.size;
    self.layoutManager = layoutManager;
    
    [self insertSublayer:voutLayer atIndex:0];
    [self setNeedsDisplayOnBoundsChange:YES];

    [CATransaction commit];

    /* Trigger by hand, as it doesn't go through else. Assumed bug from Cocoa */
    [self willChangeValueForKey:@"hasVideo"];
    self.hasVideo = YES;
    [self didChangeValueForKey:@"hasVideo"];
}

- (void)removeVoutLayer:(CALayer*)voutLayer
{
    [CATransaction begin];
    [voutLayer removeFromSuperlayer];
    [CATransaction commit];
    
    /* Trigger by hand, as it doesn't go through else. Assumed bug from Cocoa */
    [self willChangeValueForKey:@"hasVideo"];
    self.hasVideo = NO;
    [self didChangeValueForKey:@"hasVideo"];
}

@end
