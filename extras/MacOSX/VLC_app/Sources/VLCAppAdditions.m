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

#import "VLCAppAdditions.h"
#import <QuartzCore/QuartzCore.h>

@implementation NSIndexPath (VLCAppAddition)
- (NSIndexPath *)indexPathByRemovingFirstIndex
{
    if( [self length] <= 1 )
        return [[[NSIndexPath alloc] init] autorelease];

    NSIndexPath * ret;
    NSUInteger * ints = malloc(sizeof(NSUInteger)*[self length]);
    if( !ints ) return nil;
    [self getIndexes:ints];

    ret = [NSIndexPath indexPathWithIndexes:ints+1 length:[self length]-1];

    free(ints);
    return ret;
}
- (NSUInteger)lastIndex
{
    if(![self length])
        return 0;
    return [self indexAtPosition:[self length]-1];
}
@end

@implementation NSArray (VLCAppAddition)
- (id)objectAtIndexPath:(NSIndexPath *)path withNodeKeyPath:(NSString *)nodeKeyPath
{
    if( ![path length] || !nodeKeyPath )
        return self;

    id object = [self objectAtIndex:[path indexAtPosition:0]];
    id subarray = [object valueForKeyPath:nodeKeyPath];
    if([path length] == 1)
        return subarray ? subarray : object;

    if(!subarray)
        return object;
    return [subarray objectAtIndexPath:[path indexPathByRemovingFirstIndex] withNodeKeyPath:nodeKeyPath];
}
@end

@implementation NSView (VLCAppAdditions)
- (void)moveSubviewsToVisible
{
    for(NSView * view in [self subviews])
    {
        if( ([view autoresizingMask] & NSViewHeightSizable) &&
            !NSContainsRect([view frame], [self bounds]) )
        {
            NSRect newFrame = NSIntersectionRect( [self bounds], [view frame] );
            if( !NSIsEmptyRect(newFrame) )
                [view setFrame:NSIntersectionRect( [self bounds], [view frame] )];
        }
    }
}
@end

/* Split view that supports slider animation */
@implementation VLCOneSplitView
- (float)sliderPosition
{
    return [[[self subviews] objectAtIndex:0] frame].size.height;
}
- (void)setSliderPosition:(float)newPosition
{
    [self setPosition:newPosition ofDividerAtIndex:0];
}
+ (id)defaultAnimationForKey:(NSString *)key
{
    if([key isEqualToString:@"sliderPosition"])
    {
        return [CABasicAnimation animation];
    }
    return [super defaultAnimationForKey: key];
}
@end

