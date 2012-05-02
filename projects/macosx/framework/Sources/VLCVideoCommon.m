/*****************************************************************************
 * VLCVideoCommon.m: VLCKit.framework VLCVideoCommon implementation
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

#import "VLCVideoCommon.h"

/******************************************************************************
 * Implementation VLCVideoLayoutManager
 *
 * Manage the size of the video layer 
 */

@implementation VLCVideoLayoutManager 

/* Factories */
+ (id)layoutManager
{
    return [[[self alloc] init] autorelease];
}

/* CALayoutManager Impelmentation */
- (void)layoutSublayersOfLayer:(CALayer *)layer
{
    /* After having done everything normally resize the vlcopengllayer */
    if( [[layer sublayers] count] > 0 && [[[[layer sublayers] objectAtIndex:0] name] isEqualToString:@"vlcopengllayer"])
    {
        CALayer * videolayer = [[layer sublayers] objectAtIndex:0];
        CGRect bounds = layer.bounds;
        CGRect videoRect = bounds;
        
        CGFloat xRatio = CGRectGetWidth(bounds)/originalVideoSize.width;
        CGFloat yRatio = CGRectGetHeight(bounds)/originalVideoSize.height;
        CGFloat ratio = fillScreenEntirely ? MAX(xRatio, yRatio) : MIN(xRatio, yRatio);
        
        videoRect.size.width = ratio*originalVideoSize.width;
        videoRect.size.height = ratio*originalVideoSize.height;
        videoRect.origin.x += (CGRectGetWidth(bounds) - CGRectGetWidth(videoRect))/2.0;
        videoRect.origin.y += (CGRectGetHeight(bounds) - CGRectGetHeight(videoRect))/2.0;
        
        videolayer.frame = videoRect;
    }
}

/* Properties */
@synthesize fillScreenEntirely;
@synthesize originalVideoSize;
@end
