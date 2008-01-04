/*****************************************************************************
 * VLCBrowsableVideoView.h: VideoView subclasses that allow fullscreen
 * browsing
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

#import "VLCBrowsableVideoView.h"
#import "VLCAppAdditions.h"

/* TODO: We may want to clean up the private functions a bit... */

@interface VLCBrowsableVideoView ()
/* Property */
@property (readwrite, retain) id selectedObject;
@end

@interface VLCBrowsableVideoView (Private)

/* Methods */
+ (CAScrollLayer *)menuLayer;
+ (CALayer *)backLayer;

- (void)loadItemsAtIndexPath:(NSIndexPath *)path inLayer:(CALayer *)layer;
- (void)changeSelectedIndex:(NSInteger)i;
- (void)changeSelectedPath:(NSIndexPath *)newPath withSelectedIndex:(NSUInteger)newIndex;

- (void)displayEmptyView;
@end


/******************************************************************************
 * VLCBrowsableVideoView
 */
@implementation VLCBrowsableVideoView

/* Property */
@synthesize nodeKeyPath;
@synthesize contentKeyPath;
@synthesize selectedObject;
@synthesize target;
@synthesize action;

- (NSArray *)itemsTree {
    return itemsTree;
}

- (void)setItemsTree:(NSArray *)newItemsTree
{
    [itemsTree release];
    itemsTree = [newItemsTree retain];
    [self changeSelectedPath:[[[NSIndexPath alloc] init] autorelease] withSelectedIndex:0];
}

- (BOOL)fullScreen
{
    return [super isInFullScreenMode];
}

- (void)setFullScreen:(BOOL)newFullScreen
{
    if( newFullScreen == self.fullScreen )
        return;
    
    if( newFullScreen )
    {
        [super enterFullScreenMode:[[self window] screen] withOptions:nil];
    }
    else
    {
        [super exitFullScreenModeWithOptions:nil];
    }
}

/* Initializer */
- (void)awakeFromNib
{
    // FIXME: do that in -initWithFrame:
    [self setWantsLayer:YES];
    menuDisplayed = NO;
    displayedItems = NSMakeRange( -1, 0 );
    selectedIndex = -1;
    selectionLayer = backLayer = nil;
    menuLayer = nil;
    selectedPath = [[NSIndexPath alloc] init];
    /* Observe our bindings */
    //[self displayMenu];
    //[self changeSelectedIndex:0];
}

/* Hiding/Displaying the menu */

- (void)hideMenu
{
    if( !menuDisplayed )
        return; /* Nothing to do */

    [menuLayer removeFromSuperlayer];
    [selectionLayer removeFromSuperlayer];
    [backLayer removeFromSuperlayer];
    //[menuLayer autorelease]; /* Need gc for that */
    //[selectionLayer autorelease];
    //[backLayer autorelease];
    selectionLayer = backLayer = nil;
    menuLayer = nil;
    menuDisplayed = NO;
}

- (void)displayMenu
{
    if( menuDisplayed || !self.itemsTree )
        return; /* Nothing to do */

    if( !menuLayer )
    {
        CALayer * rootLayer = [self layer];
        rootLayer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
        rootLayer.layoutManager = [CAConstraintLayoutManager layoutManager];
        menuLayer = [VLCBrowsableVideoView menuLayer];
        [self loadItemsAtIndexPath: selectedPath inLayer: menuLayer];
    }
    if( !backLayer )
    {
        backLayer = [[VLCBrowsableVideoView backLayer] retain];
    }
    [[self layer] addSublayer:backLayer];
    [[self layer] addSublayer:menuLayer];

    [[self layer] setNeedsLayout];
    [[self layer] setNeedsDisplay];

    menuDisplayed = YES;
    [self changeSelectedPath:selectedPath withSelectedIndex:selectedIndex];
}

- (void)toggleMenu
{
    if( menuDisplayed )
        [self hideMenu];
    else
        [self displayMenu];
}


/* Event handling */

- (BOOL)acceptsFirstResponder
{
    return YES;
}

-(void)moveUp:(id)sender
{
    [self changeSelectedIndex:selectedIndex-1];
}

-(void)moveDown:(id)sender
{
    [self changeSelectedIndex:selectedIndex+1];
}

- (void)mouseDown:(NSEvent *)theEvent
{
    if([theEvent clickCount] != 2)
        return;

    self.fullScreen = !self.fullScreen;
}

- (void)keyDown:(NSEvent *)theEvent
{
    if(([[theEvent charactersIgnoringModifiers] characterAtIndex:0] == 13) && menuDisplayed)
    {
        [self changeSelectedPath:[selectedPath indexPathByAddingIndex:selectedIndex] withSelectedIndex:0];
    }
    else if([[theEvent charactersIgnoringModifiers] characterAtIndex:0] ==  NSLeftArrowFunctionKey && menuDisplayed)
    {
        if( [selectedPath length] > 0 )
            [self changeSelectedPath:[selectedPath indexPathByRemovingLastIndex] withSelectedIndex:[selectedPath lastIndex]];
        else
            [self hideMenu];
    }
    else if(!menuDisplayed)
    {
        [self displayMenu];
    }
    else
        [super keyDown: theEvent];

}
@end

/******************************************************************************
 * VLCBrowsableVideoView (Private)
 */
@implementation VLCBrowsableVideoView (Private)
+ (CAScrollLayer *)menuLayer
{
    CAScrollLayer * layer = [CAScrollLayer layer];
    layer.scrollMode = kCAScrollVertically;
            
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxY
                                       relativeTo:@"superlayer" attribute:kCAConstraintMaxY]];
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxX
                                       relativeTo:@"superlayer" attribute:kCAConstraintMaxX]];
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinX
                                       relativeTo:@"superlayer" attribute:kCAConstraintMinX]];
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinY
                                       relativeTo:@"superlayer" attribute:kCAConstraintMinY]];
    return layer;
}

+ (CALayer *)backLayer
{
    CALayer * layer = [CALayer layer];
            
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxY
                                       relativeTo:@"superlayer" attribute:kCAConstraintMaxY]];
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxX
                                       relativeTo:@"superlayer" attribute:kCAConstraintMaxX]];
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinX
                                       relativeTo:@"superlayer" attribute:kCAConstraintMinX]];
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinY
                                       relativeTo:@"superlayer" attribute:kCAConstraintMinY]];

    layer.opacity = 1.0;
    layer.backgroundColor = CGColorCreateGenericRGB(0., 0., 0., .5);
    return layer;
}


- (void)loadItemsAtIndexPath:(NSIndexPath *)path inLayer:(CALayer *)layer
{
    const CGFloat height=70.0;
    const CGFloat fontSize=48.0;
    NSArray * items = [self.itemsTree objectAtIndexPath:path withNodeKeyPath:self.nodeKeyPath]; 
    int i;

    for( i = 0; i < [items count]; i++ )
    {
        CATextLayer *menuItemLayer=[CATextLayer layer];
        id item = [items objectAtIndex: i];
        menuItemLayer.string = self.contentKeyPath ? [item valueForKeyPath:self.contentKeyPath] : @"No content Key path set";
        menuItemLayer.font = @"BankGothic-Light";
        menuItemLayer.fontSize = fontSize;
        menuItemLayer.foregroundColor = CGColorCreateGenericRGB(1.0,1.0,1.0,1.0);
        menuItemLayer.shadowColor = CGColorCreateGenericRGB(0.0,0.0,0.0,1.0);
        menuItemLayer.shadowOpacity = 0.7;
        menuItemLayer.shadowRadius = 2.0;

        menuItemLayer.frame = CGRectMake( 40., height*(-i) + layer.visibleRect.size.height, 500.0f,70.);
        [layer addSublayer: menuItemLayer];
    }

/*    for(i=0; i < [[layer sublayers] count]; i++)
        NSLog(@"%d, %@", i, [[[layer sublayers] objectAtIndex: i] string]);
    NSLog(@"---");*/
}

- (void)changeSelectedIndex:(NSInteger)i
{
    BOOL justCreatedSelectionLayer = NO;
    if( !menuDisplayed )
    {
        selectedIndex = i;
        return;
    }

    if( !selectionLayer )
    {
        justCreatedSelectionLayer = YES;
        /* Rip-off from Apple's Sample code */
        selectionLayer=[[CALayer layer] retain];
        
        selectionLayer.borderWidth=2.0;
        selectionLayer.borderColor=CGColorCreateGenericRGB(1.0f,1.0f,1.0f,1.0f);
        selectionLayer.backgroundColor=CGColorCreateGenericRGB(.9f,1.0f,1.0f,.1f);
        
        CIFilter *filter = [CIFilter filterWithName:@"CIBloom"];
        [filter setDefaults];
        [filter setValue:[NSNumber numberWithFloat:5.0] forKey:@"inputRadius"];
        
        [filter setName:@"pulseFilter"];
        
        [selectionLayer setFilters:[NSArray arrayWithObject:filter]];
        
        CABasicAnimation* pulseAnimation = [CABasicAnimation animation];
        
        pulseAnimation.keyPath = @"filters.pulseFilter.inputIntensity";
        
        pulseAnimation.fromValue = [NSNumber numberWithFloat: 0.0];
        pulseAnimation.toValue = [NSNumber numberWithFloat: 3.0];
        
        pulseAnimation.duration = 2.0;
        pulseAnimation.repeatCount = 1e100f;
        pulseAnimation.autoreverses = YES;
        
        pulseAnimation.timingFunction = [CAMediaTimingFunction functionWithName:
                                         kCAMediaTimingFunctionEaseInEaseOut];
        
        [selectionLayer addAnimation:pulseAnimation forKey:@"pulseAnimation"];
        [[self layer] addSublayer:selectionLayer];
    }
    NSArray * items = [self.itemsTree objectAtIndexPath:selectedPath withNodeKeyPath:self.nodeKeyPath];
    if( i < 0 ) i = 0;
    if( i >= [items count] ) i = [items count] - 1;

    CALayer * layer = [[menuLayer sublayers] objectAtIndex: i];
    CGRect frame = layer.frame;
    if( i == 0 )
    {
        frame.origin.y -= [self layer].bounds.size.height - frame.size.height;
        frame.size.height = [self layer].bounds.size.height;
    }
    [(CAScrollLayer*)menuLayer scrollToRect:frame];

    if( !justCreatedSelectionLayer ) /* Get around an artifact on first launch */
        [CATransaction flush]; /* Make sure we get the "right" layer.frame */

    frame = [[self layer] convertRect:layer.frame fromLayer:[layer superlayer]];
    frame.size.width += 200.;
    frame.origin.x -= 100.f;
    selectionLayer.frame = frame;
    
    selectionLayer.cornerRadius = selectionLayer.bounds.size.height / 2.;
    selectedIndex = i;
}

- (void)changeSelectedPath:(NSIndexPath *)newPath withSelectedIndex:(NSUInteger)newIndex
{
    if( menuDisplayed )
    {
        id object = [itemsTree objectAtIndexPath:newPath withNodeKeyPath:nodeKeyPath];
        /* Make sure we are in a node */
        if( ![object isKindOfClass:[NSArray class]] )
        {
            self.selectedObject = object;
            if( !self.target || !self.action )
            {
                [NSException raise:@"VLCBrowsableVideoViewNoActionSpecified" format:@"*** Exception [%@]: No action specified.", [self class]];
                return;
            }
            void (*method)(id, SEL, id) = (void (*)(id, SEL, id))[self.target methodForSelector: self.action];

            method( self.target, self.action, self);

            [self hideMenu];
            return;
        }
        
        /* Make sure the node isn't empty */
        if( ![object count] )
        {
            [self displayEmptyView];
        }
        else
        {
            CALayer * newMenuLayer = [VLCBrowsableVideoView menuLayer];
            if( menuLayer )
                newMenuLayer.bounds = menuLayer.bounds; /* Get around some artifacts */
            [self loadItemsAtIndexPath:newPath inLayer:newMenuLayer];
            if( menuLayer )
                [[self layer] replaceSublayer:menuLayer with:newMenuLayer];
            else
                [[self layer] addSublayer:newMenuLayer];
            //[menuLayer autorelease]; /* warn: we need gc for that */
            menuLayer = [newMenuLayer retain];
        }
    }
    [selectedPath release];
    selectedPath = [newPath retain];
    [self changeSelectedIndex:newIndex];
}

- (void)displayEmptyView
{
    CALayer * layer = [CALayer layer];
    layer.layoutManager = [CAConstraintLayoutManager layoutManager];
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxY
                                       relativeTo:@"superlayer" attribute:kCAConstraintMaxY]];
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxX
                                       relativeTo:@"superlayer" attribute:kCAConstraintMaxX]];
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinX
                                       relativeTo:@"superlayer" attribute:kCAConstraintMinX]];
    [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinY
                                       relativeTo:@"superlayer" attribute:kCAConstraintMinY]];
        
    CATextLayer *menuItemLayer=[CATextLayer layer];
    menuItemLayer.string = @"Empty";
    menuItemLayer.font = @"BankGothic-Light";
    menuItemLayer.fontSize = 48.f;
    menuItemLayer.foregroundColor = CGColorCreateGenericRGB(1.0,1.0,1.0,1.0);
    menuItemLayer.shadowColor = CGColorCreateGenericRGB(0.0,0.0,0.0,1.0);
    menuItemLayer.shadowOpacity = 0.7;
    menuItemLayer.shadowRadius = 2.0;

    [menuItemLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMidX
                                       relativeTo:@"superlayer" attribute:kCAConstraintMidX]];
    [menuItemLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMidY
                                       relativeTo:@"superlayer" attribute:kCAConstraintMidY]];
    [layer addSublayer:menuItemLayer];

    if( menuLayer )
        [[self layer] replaceSublayer:menuLayer with:layer];
    else
        [[self layer] addSublayer:layer];
    [selectionLayer removeFromSuperlayer];
    //[selectionLayer autorelease] /* need gc */
    //[menuLayer autorelease] /* need gc */
    menuLayer = layer;
    selectionLayer = nil;
}
@end