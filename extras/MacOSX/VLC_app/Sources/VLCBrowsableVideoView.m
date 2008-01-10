/*****************************************************************************
 * VLCBrowsableVideoView.h: VideoView subclasses that allow fullScreen
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

/* DisableScreenUpdates, SetSystemUIMode, ... */
#import <QuickTime/QuickTime.h>

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


@interface VLCBrowsableVideoView (FullScreenTransition)
- (void)hasEndedFullScreen;
- (void)hasBecomeFullScreen;

- (void)enterFullScreen:(NSScreen *)screen;
- (void)leaveFullScreen;
- (void)leaveFullScreenAndFadeOut: (BOOL)fadeout;

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
    return [self isInFullScreenMode];
}

- (void)setFullScreen:(BOOL)newFullScreen
{
    if( newFullScreen == self.fullScreen )
        return;
    
    if( newFullScreen )
    {
        [self enterFullScreenMode:[[self window] screen] withOptions:
                    [NSDictionary dictionaryWithObject: [NSNumber numberWithInt:1]
                                                forKey: NSFullScreenModeWindowLevel]];
    }
    else
    {
        [self exitFullScreenModeWithOptions:nil];
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
    tempFullScreenView = [[NSView alloc] init];
    fullScreen = NO;
    /* Observe our bindings */
    //[self displayMenu];
    //[self changeSelectedIndex:0];
}

- (void)dealloc
{
    [tempFullScreenView release];
    [selectedPath release];
    [super dealloc];
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

- (void)drawRect:(NSRect)rect
{    
    NSColor * bottomGradient = [NSColor colorWithCalibratedWhite:0.10 alpha:1.0];
    NSColor * topGradient    = [NSColor colorWithCalibratedWhite:0.45 alpha:1.0];
	NSGradient * gradient = [[NSGradient alloc] initWithStartingColor:bottomGradient endingColor:topGradient];
    [gradient drawInRect:self.bounds angle:90.0];
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

- (void)enterFullScreenMode:(NSScreen *)screen withOptions:(NSDictionary *)options
{
    [self enterFullScreen: screen];
}

- (void)exitFullScreenModeWithOptions:(NSDictionary *)options
{
    [self leaveFullScreen];

}

- (BOOL)isInFullScreenMode
{
    return fullScreen;
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
    if( [items count] <= 0 )
        return;
    if( i >= [items count] ) i = [items count] - 1;
    if( i < 0 ) i = 0;

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

@implementation VLCBrowsableVideoView (FullScreenTransition)

- (void)enterFullScreen:(NSScreen *)screen
{
    NSMutableDictionary *dict1,*dict2;
    NSRect screenRect;
    NSRect aRect;
            
    screenRect = [screen frame];
        
    [NSCursor setHiddenUntilMouseMoves: YES];
    
    /* Only create the o_fullScreen_window if we are not in the middle of the zooming animation */
    if (!fullScreenWindow)
    {
        /* We can't change the styleMask of an already created NSWindow, so we create an other window, and do eye catching stuff */
        
        aRect = [[self superview] convertRect: [self frame] toView: nil]; /* Convert to Window base coord */
        aRect.origin.x += [[self window] frame].origin.x;
        aRect.origin.y += [[self window] frame].origin.y;
        fullScreenWindow = [[VLCWindow alloc] initWithContentRect:aRect styleMask: NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
        [fullScreenWindow setBackgroundColor: [NSColor blackColor]];
        [fullScreenWindow setCanBecomeKeyWindow: YES];

        if (![[self window] isVisible] || [[self window] alphaValue] == 0.0 || [self isHiddenOrHasHiddenAncestor] )
        {
            /* We don't animate if we are not visible or if we are running on
             * Mac OS X <10.4 which doesn't support NSAnimation, instead we
             * simply fade the display */
            CGDisplayFadeReservationToken token;
 
            [fullScreenWindow setFrame:screenRect display:NO];
 
            CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
            CGDisplayFade( token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );
 
            if ([screen isMainScreen])
                SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
 
            [self retain];
            [[self superview] replaceSubview:self with:tempFullScreenView];
            [tempFullScreenView setFrame:[self frame]];
            [fullScreenWindow setContentView:self];
            [fullScreenWindow makeKeyAndOrderFront:self];
            [self release];
            [[tempFullScreenView window] orderOut: self];

            CGDisplayFade( token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO );
            CGReleaseDisplayFadeReservation( token);

            [self hasBecomeFullScreen];

            return;
        }
 
        /* Make sure we don't see the o_view disappearing of the screen during this operation */
        DisableScreenUpdates();
        [self retain]; /* Removing from a view, make sure we won't be released */
        /* Make sure our layer won't disappear */
        CALayer * layer = [[self layer] retain];
        id alayoutManager = layer.layoutManager;
        [[self superview] replaceSubview:self with:tempFullScreenView];
        [tempFullScreenView setFrame:[self frame]];
        [fullScreenWindow setContentView:self];
        [self setWantsLayer:YES];
        [self setLayer:layer];
        layer.layoutManager = alayoutManager;

        [fullScreenWindow makeKeyAndOrderFront:self];
        EnableScreenUpdates();
    }

    /* We are in fullScreen (and no animation is running) */
    if (fullScreen)
    {
        /* Make sure we are hidden */
        [[tempFullScreenView window] orderOut: self];
        return;
    }

    if (fullScreenAnim1)
    {
        [fullScreenAnim1 stopAnimation];
        [fullScreenAnim1 release];
    }
    if (fullScreenAnim2)
    {
        [fullScreenAnim2 stopAnimation];
        [fullScreenAnim2 release];
    }
 
    if ([screen isMainScreen])
        SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:2];
    dict2 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:[tempFullScreenView window] forKey:NSViewAnimationTargetKey];
    [dict1 setObject:NSViewAnimationFadeOutEffect forKey:NSViewAnimationEffectKey];

    [dict2 setObject:fullScreenWindow forKey:NSViewAnimationTargetKey];
    [dict2 setObject:[NSValue valueWithRect:[fullScreenWindow frame]] forKey:NSViewAnimationStartFrameKey];
    [dict2 setObject:[NSValue valueWithRect:screenRect] forKey:NSViewAnimationEndFrameKey];

    /* Strategy with NSAnimation allocation:
        - Keep at most 2 animation at a time
        - leaveFullScreen/enterFullScreen are the only responsible for releasing and alloc-ing
    */
    fullScreenAnim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict1, nil]];
    fullScreenAnim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict2, nil]];

    [dict1 release];
    [dict2 release];

    [fullScreenAnim1 setAnimationBlockingMode: NSAnimationNonblocking];
    [fullScreenAnim1 setDuration: 0.3];
    [fullScreenAnim1 setFrameRate: 30];
    [fullScreenAnim2 setAnimationBlockingMode: NSAnimationNonblocking];
    [fullScreenAnim2 setDuration: 0.3];
    [fullScreenAnim2 setFrameRate: 30];

    [fullScreenAnim2 setDelegate: self];
    [fullScreenAnim2 startWhenAnimation: fullScreenAnim1 reachesProgress: 1.0];

    [fullScreenAnim1 startAnimation];
}

- (void)hasBecomeFullScreen
{
    [fullScreenWindow makeFirstResponder: self];

    [fullScreenWindow makeKeyWindow];
    [fullScreenWindow setAcceptsMouseMovedEvents: TRUE];
 
    [[tempFullScreenView window] orderOut: self];
    [self willChangeValueForKey:@"fullScreen"];
    fullScreen = YES;
    [self didChangeValueForKey:@"fullScreen"];
}

- (void)leaveFullScreen
{
    [self leaveFullScreenAndFadeOut: NO];
}

- (void)leaveFullScreenAndFadeOut: (BOOL)fadeout
{
    NSMutableDictionary *dict1, *dict2;
    NSRect frame;

    [self willChangeValueForKey:@"fullScreen"];
    fullScreen = NO;
    [self didChangeValueForKey:@"fullScreen"];

    /* Don't do anything if o_fullScreen_window is already closed */
    if (!fullScreenWindow)
        return;

    if (fadeout || [tempFullScreenView isHiddenOrHasHiddenAncestor])
    {
        /* We don't animate if we are not visible or if we are running on
        * Mac OS X <10.4 which doesn't support NSAnimation, instead we
        * simply fade the display */
        CGDisplayFadeReservationToken token;

        CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
        CGDisplayFade( token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );

        SetSystemUIMode( kUIModeNormal, kUIOptionAutoShowMenuBar);

        [self hasEndedFullScreen];

        CGDisplayFade( token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO );
        CGReleaseDisplayFadeReservation( token);
        return;
    }

    [[tempFullScreenView window] setAlphaValue: 0.0];
    [[tempFullScreenView window] orderFront: self];

    SetSystemUIMode( kUIModeNormal, kUIOptionAutoShowMenuBar);

    if (fullScreenAnim1)
    {
        [fullScreenAnim1 stopAnimation];
        [fullScreenAnim1 release];
    }
    if (fullScreenAnim2)
    {
        [fullScreenAnim2 stopAnimation];
        [fullScreenAnim2 release];
    }

    frame = [[tempFullScreenView superview] convertRect: [tempFullScreenView frame] toView: nil]; /* Convert to Window base coord */
    frame.origin.x += [tempFullScreenView window].frame.origin.x;
    frame.origin.y += [tempFullScreenView window].frame.origin.y;

    dict2 = [[NSMutableDictionary alloc] initWithCapacity:2];
    [dict2 setObject:[tempFullScreenView window] forKey:NSViewAnimationTargetKey];
    [dict2 setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];

    fullScreenAnim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict2, nil]];
    [dict2 release];

    [fullScreenAnim2 setAnimationBlockingMode: NSAnimationNonblocking];
    [fullScreenAnim2 setDuration: 0.3];
    [fullScreenAnim2 setFrameRate: 30];

    [fullScreenAnim2 setDelegate: self];

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:fullScreenWindow forKey:NSViewAnimationTargetKey];
    [dict1 setObject:[NSValue valueWithRect:[fullScreenWindow frame]] forKey:NSViewAnimationStartFrameKey];
    [dict1 setObject:[NSValue valueWithRect:frame] forKey:NSViewAnimationEndFrameKey];

    fullScreenAnim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict1, nil]];
    [dict1 release];

    [fullScreenAnim1 setAnimationBlockingMode: NSAnimationNonblocking];
    [fullScreenAnim1 setDuration: 0.2];
    [fullScreenAnim1 setFrameRate: 30];
    [fullScreenAnim2 startWhenAnimation: fullScreenAnim1 reachesProgress: 1.0];

    /* Make sure o_fullScreen_window is the frontmost window */
    [fullScreenWindow orderFront: self];

    [fullScreenAnim1 startAnimation];
}

- (void)hasEndedFullScreen
{
    /* This function is private and should be only triggered at the end of the fullScreen change animation */
    /* Make sure we don't see the o_view disappearing of the screen during this operation */
    DisableScreenUpdates();
    [self retain];
    /* Make sure we don't loose the layer */
    CALayer * layer = [[self layer] retain];
    id alayoutManager = layer.layoutManager;
    [self removeFromSuperviewWithoutNeedingDisplay];
    [[tempFullScreenView superview] replaceSubview:tempFullScreenView with:self];
    [self release];
    [self setWantsLayer:YES];
    [self setLayer:layer];
    layer.layoutManager = alayoutManager;

    [self setFrame:[tempFullScreenView frame]];
    [[self window] makeFirstResponder: self];
    if ([[self window] isVisible])
        [[self window] makeKeyAndOrderFront:self];
    [fullScreenWindow orderOut: self];
    EnableScreenUpdates();

    [fullScreenWindow release];
    fullScreenWindow = nil;
}

- (void)animationDidEnd:(NSAnimation*)animation
{
    NSArray *viewAnimations;

    if ([animation currentValue] < 1.0)
        return;

    /* FullScreen ended or started (we are a delegate only for leaveFullScreen's/enterFullscren's anim2) */
    viewAnimations = [fullScreenAnim2 viewAnimations];
    if ([viewAnimations count] >=1 &&
        [[[viewAnimations objectAtIndex: 0] objectForKey: NSViewAnimationEffectKey] isEqualToString:NSViewAnimationFadeInEffect])
    {
        /* FullScreen ended */
        [self hasEndedFullScreen];
    }
    else
    {
        /* FullScreen started */
        [self hasBecomeFullScreen];
    }
}

@end

