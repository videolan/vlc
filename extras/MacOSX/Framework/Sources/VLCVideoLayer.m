/*****************************************************************************
 * VLCVideoLayer.m: VLC.framework VLCVideoLayer implementation
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id: VLCVideoView.m 24023 2008-01-02 02:52:35Z pdherbemont $
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

#import "VLCVideoLayer.h"
#import "VLCLibrary.h"
#import "VLCEventManager.h"
#import "VLCVideoCommon.h"

/* Libvlc */
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
    self.hasVideo = YES;
}

- (void)removeVoutLayer:(CALayer*)voutLayer
{
    [CATransaction begin];
    [voutLayer removeFromSuperlayer];
    [CATransaction commit];
    self.hasVideo = NO;
}

@end
