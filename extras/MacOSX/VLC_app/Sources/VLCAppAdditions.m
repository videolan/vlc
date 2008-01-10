/*****************************************************************************
 * VLCAppAdditions.m: Helpful additions to NS* classes
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Felix Kühne <fkuehne at videolan dot org>
 *          Jérôme Decoodt <djc at videolan dot org>
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
@synthesize fixedCursorDuringResize;
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
- (void)adjustSubviews
{
    if( !fixedCursorDuringResize )
    {
        [super adjustSubviews];
        return;
    }
    NSRect frame0 = [[[self subviews] objectAtIndex:0] frame];
    NSRect frame1 = [[[self subviews] objectAtIndex:1] frame];
    frame1.size.height = [self bounds].size.height - frame0.size.height - [self dividerThickness];
    if( frame1.size.height < 0. )
    {
        float delta = -frame1.size.height;
        frame1.size.height = 0.;
        frame0.size.height -= delta;
        frame1.origin.y = frame0.size.height + [self dividerThickness];
        [[[self subviews] objectAtIndex:1] setFrame: frame0];
    }
    [[[self subviews] objectAtIndex:1] setFrame: frame1];
}
@end

/*****************************************************************************
 * NSScreen (VLCAdditions)
 *
 *  Missing extension to NSScreen
 *****************************************************************************/

@implementation NSScreen (VLCAdditions)

static NSMutableArray *blackoutWindows = NULL;

+ (void)load
{
    /* init our fake object attribute */
    blackoutWindows = [[NSMutableArray alloc] initWithCapacity:1];
}

+ (NSScreen *)screenWithDisplayID: (CGDirectDisplayID)displayID
{
    int i;
 
    for( i = 0; i < [[NSScreen screens] count]; i++ )
    {
        NSScreen *screen = [[NSScreen screens] objectAtIndex: i];
        if([screen displayID] == displayID)
            return screen;
    }
    return nil;
}

- (BOOL)isMainScreen
{
    return ([self displayID] == [[[NSScreen screens] objectAtIndex:0] displayID]);
}

- (BOOL)isScreen: (NSScreen*)screen
{
    return ([self displayID] == [screen displayID]);
}

- (CGDirectDisplayID)displayID
{
    return (CGDirectDisplayID)_screenNumber;
}

- (void)blackoutOtherScreens
{
    unsigned int i;

    /* Free our previous blackout window (follow blackoutWindow alloc strategy) */
    [blackoutWindows makeObjectsPerformSelector:@selector(close)];
    [blackoutWindows removeAllObjects];

 
    for(i = 0; i < [[NSScreen screens] count]; i++)
    {
        NSScreen *screen = [[NSScreen screens] objectAtIndex: i];
        VLCWindow *blackoutWindow;
        NSRect screen_rect;
 
        if([self isScreen: screen])
            continue;

        screen_rect = [screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0.0f;

        /* blackoutWindow alloc strategy
            - The NSMutableArray blackoutWindows has the blackoutWindow references
            - blackoutOtherDisplays is responsible for alloc/releasing its Windows
        */
        blackoutWindow = [[VLCWindow alloc] initWithContentRect: screen_rect styleMask: NSBorderlessWindowMask
                backing: NSBackingStoreBuffered defer: NO screen: screen];
        [blackoutWindow setBackgroundColor:[NSColor blackColor]];
        [blackoutWindow setLevel: NSFloatingWindowLevel]; /* Disappear when Expose is triggered */
 
        [blackoutWindow orderFront: self];

        [blackoutWindows addObject: blackoutWindow];
        [blackoutWindow release];
    }
}

+ (void)unblackoutScreens
{
    unsigned int i;

    for(i = 0; i < [blackoutWindows count]; i++)
    {
        VLCWindow *blackoutWindow = [blackoutWindows objectAtIndex: i];
        [blackoutWindow close];
    }
}

@end

/*****************************************************************************
 * VLCWindow
 *
 *  Missing extension to NSWindow
 *****************************************************************************/

@implementation VLCWindow
- (id)initWithContentRect:(NSRect)contentRect styleMask:(unsigned int)styleMask
    backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    self = [super initWithContentRect:contentRect styleMask:styleMask backing:backingType defer:flag];
    if( self )
        isset_canBecomeKeyWindow = NO;
    return self;
}
- (void)setCanBecomeKeyWindow: (BOOL)canBecomeKey
{
    isset_canBecomeKeyWindow = YES;
    canBecomeKeyWindow = canBecomeKey;
}

- (BOOL)canBecomeKeyWindow
{
    if(isset_canBecomeKeyWindow)
        return canBecomeKeyWindow;

    return [super canBecomeKeyWindow];
}
@end

/*****************************************************************************
 * VLCImageCustomizedSlider
 *
 *  Slider personalized by backgroundImage and knobImage
 *****************************************************************************/
@implementation VLCImageCustomizedSlider
@synthesize backgroundImage;
@synthesize knobImage;

- (id)initWithFrame:(NSRect)frame
{
    if(self = [super initWithFrame:frame])
    {
        knobImage = nil;
        backgroundImage = nil;
    }
    return self;
}

- (void)dealloc
{
    [knobImage release];
    [knobImage release];
    [super dealloc];
}

- (void)drawKnobInRect:(NSRect) knobRect
{
    NSRect imageRect;
    imageRect.size = [self.knobImage size];
    imageRect.origin.x = 0;
    imageRect.origin.y = 0;
    knobRect.origin.x += (knobRect.size.width - imageRect.size.width) / 2;
    knobRect.origin.y += (knobRect.size.width - imageRect.size.width) / 2;
    knobRect.size.width = imageRect.size.width;
    knobRect.size.height = imageRect.size.height;
    [self.knobImage drawInRect:knobRect fromRect:imageRect operation:NSCompositeSourceOver fraction:1];
}

- (void)drawBackgroundInRect:(NSRect) drawRect
{
    NSRect imageRect = drawRect;
    imageRect.origin.y += ([self.backgroundImage size].height - [self bounds].size.height ) / 2;
    [self.backgroundImage drawInRect:drawRect fromRect:imageRect operation:NSCompositeSourceOver fraction:1];
}

- (void)drawRect:(NSRect)rect
{
    /* Draw default to make sure the slider behaves correctly */
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSRectClip(NSZeroRect);
    [super drawRect:rect];
    [[NSGraphicsContext currentContext] restoreGraphicsState];
    if( self.backgroundImage ) 
        [self drawBackgroundInRect: rect];
    if( self.knobImage ) 
    {
        NSRect knobRect = [[self cell] knobRectFlipped:NO];
        [[[NSColor blackColor] colorWithAlphaComponent:0.6] set];
        [self drawKnobInRect: knobRect];
    }
}

@end

/*****************************************************************************
 * NSImageView (VLCAppAdditions)
 *
 *  Make the image view  move the window by mouse down by default
 *****************************************************************************/

@implementation NSImageView (VLCAppAdditions)
- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}
@end

