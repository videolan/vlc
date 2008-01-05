/*****************************************************************************
 * VLCAppAdditions.m: Helpful additions to NS* classes
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

#import <Cocoa/Cocoa.h>

@interface NSIndexPath (VLCAppAddition)
- (NSIndexPath *)indexPathByRemovingFirstIndex;
- (NSUInteger)lastIndex;
@end

@interface NSArray (VLCAppAddition)
- (id)objectAtIndexPath:(NSIndexPath *)path withNodeKeyPath:(NSString *)nodeKeyPath;
@end

@interface NSView (VLCAppAdditions)
- (void)moveSubviewsToVisible;
@end

/* Split view that supports slider animation */
@interface VLCOneSplitView : NSSplitView
{
    BOOL fixedCursorDuringResize;
}
@property (assign) BOOL fixedCursorDuringResize;
- (float)sliderPosition;
- (void)setSliderPosition:(float)newPosition;
@end
