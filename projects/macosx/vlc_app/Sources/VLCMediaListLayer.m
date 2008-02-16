//
//  VLCMediaListLayer.m
//  VLC
//
//  Created by Pierre d'Herbemont on 1/14/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "VLCMediaListLayer.h"

/*****************************************************************************
 * @implementation VLCMediaListLayer
 */

@interface VLCMediaListLayer (Private)
- (VLCMediaLayer *)selectedLayer;
- (VLCMediaLayer *)previousLayer;
- (VLCMediaLayer *)nextLayer;
- (void)changeSelectedLayerToNextIndex;
- (void)changeSelectedLayerToPreviousIndex;
- (void)resetLayers;


- (void)setSelectedLayer:(VLCMediaLayer *)layer;
- (void)setPreviousLayer:(VLCMediaLayer *)layer;
- (void)setNextLayer:(VLCMediaLayer *)layer;
@end

/*****************************************************************************
 * @implementation VLCMediaListLayer
 */

@implementation VLCMediaListLayer
@synthesize selectedIndex;
@synthesize content;
@synthesize controller;

+ (id)layer
{
    VLCMediaListLayer * me = [super layer];

    me.layoutManager = [CAConstraintLayoutManager layoutManager];

    [CATransaction commit];

    me->selectedIndex = NSNotFound;
    return me;
}

+ (id)layerWithMediaArrayController:(VLCMediaArrayController *)aController
{
    VLCMediaListLayer * me = [VLCMediaListLayer layer];
    me.controller = aController;
    
    /* The following will trigger -observeValueForKeyPath: ofObject: change: context: */
    [me.controller addObserver:me forKeyPath:@"arrangedObjects" options:NSKeyValueObservingOptionInitial|NSKeyValueObservingOptionNew|NSKeyValueObservingOptionOld context:nil];
    [me.controller addObserver:me forKeyPath:@"selectionIndex" options:NSKeyValueObservingOptionInitial|NSKeyValueObservingOptionNew|NSKeyValueObservingOptionOld context:nil];
    [me.controller addObserver:me forKeyPath:@"contentMediaList" options:NSKeyValueObservingOptionInitial|NSKeyValueObservingOptionNew|NSKeyValueObservingOptionOld context:nil];

    return me;
}

- (void)dealloc
{
    /* Previously registered in +layerWithMediaArrayController: */
    [self.controller removeObserver:self forKeyPath:@"arrangedObjects"];
    [self.controller removeObserver:self forKeyPath:@"contentMediaList"];
    [self.controller removeObserver:self forKeyPath:@"selectionIndex"];
    [super dealloc];
}
@end

/*****************************************************************************
 * @implementation VLCMediaListLayer (Private)
 */

@implementation VLCMediaListLayer (Private)
+ (NSSet *)keyPathsForValuesAffectingSelectedLayer
{
    return [NSSet setWithObjects:@"selectedLayer", @"content", nil];
}

- (VLCMediaLayer *)selectedLayer
{
    VLCMedia * media = (self.selectedIndex != NSNotFound) ? [self.content objectAtIndex:self.selectedIndex ] : nil;
    if( !media )
    {
        CATextLayer * layer = [CATextLayer layer];
        CALayer * container = [CALayer layer];
        container.layoutManager = [CAConstraintLayoutManager layoutManager];
        if([self.controller.contentMediaList isReadOnly])
            layer.string = @"Empty";
        else if ([self.content count])
            layer.string = @"Empty search.";
        else
            layer.string = @"Drag and Drop a movie or a music here.";
        layer.alignmentMode = kCAAlignmentCenter;
        layer.wrapped = YES;
        [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMidY relativeTo:@"superlayer" attribute:kCAConstraintMidY]];
        [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMidX relativeTo:@"superlayer" attribute:kCAConstraintMidX]];
        [layer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintWidth relativeTo:@"superlayer" attribute:kCAConstraintWidth]];
        [container addSublayer:layer];
        return (VLCMediaLayer *)container;
    }

    if( [selectedLayer isKindOfClass:[VLCMediaLayer class]] && [media compare:[selectedLayer media]] == NSOrderedSame )
        return [[selectedLayer retain] autorelease];

    return [VLCMediaLayer layerWithMedia:[self.content objectAtIndex:self.selectedIndex]];
}

- (VLCMediaLayer *)previousLayer
{
    if( self.selectedIndex == NSNotFound )
        return nil;
    VLCMedia * media = self.selectedIndex > 0 ? [self.content objectAtIndex:self.selectedIndex - 1] : nil;
    if( !media )
        return nil;

    if( [previousLayer isKindOfClass:[VLCMediaLayer class]] && [media compare:[previousLayer media]] == NSOrderedSame )
        return [[previousLayer retain] autorelease];
    
    return [VLCMediaLayer layerWithMedia: media ];
}

- (VLCMediaLayer *)nextLayer
{
    if( self.selectedIndex == NSNotFound )
        return nil;
    VLCMedia * media = self.selectedIndex + 1 < [content count] ? [self.content objectAtIndex:self.selectedIndex + 1] : nil;
    if( !media )
        return nil;

    if( [nextLayer isKindOfClass:[VLCMediaLayer class]] && [media compare:[nextLayer media]] == NSOrderedSame )
        return [[nextLayer retain] autorelease];
    
    return [VLCMediaLayer layerWithMedia: media ];
}

- (void)changeSelectedLayerToNextIndex
{
    if(!nextLayer)
    {
        /* Can't do anything */
        return;
    }
    selectedIndex++;

    /* Remove offscreen layer. Without actions */
    if( previousLayer )
    {
        [CATransaction begin];
        [CATransaction setValue:[NSNumber numberWithBool:YES] forKey:kCATransactionDisableActions];
            [previousLayer removeFromSuperlayer];
        [CATransaction commit];
    }

    [CATransaction begin];
        if ( [[NSApp currentEvent] modifierFlags] & NSShiftKeyMask )
            [CATransaction setValue:[NSNumber numberWithFloat:1.5] forKey:kCATransactionAnimationDuration];

        [self setPreviousLayer: selectedLayer];        
        [self setSelectedLayer: nextLayer];
        [self setNextLayer: [self nextLayer]];
    [CATransaction commit];

    /* Move the new nextLayer layer on screen. Without Actions */
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithBool:YES] forKey:kCATransactionDisableActions];
        [self addSublayer: nextLayer];
    [CATransaction commit];
}

- (void)changeSelectedLayerToPreviousIndex
{
    if(!previousLayer)
    {
        /* Can't do anything */
        return;
    }
    selectedIndex--;

    /* Remove offscreen layer. Without actions */
    if( nextLayer )
    {
        [CATransaction begin];
        [CATransaction setValue:[NSNumber numberWithBool:YES] forKey:kCATransactionDisableActions];
            [nextLayer removeFromSuperlayer];
        [CATransaction commit];
    }

    [CATransaction begin];
        if ( [[NSApp currentEvent] modifierFlags] & NSShiftKeyMask )
            [CATransaction setValue:[NSNumber numberWithFloat:1.5] forKey:kCATransactionAnimationDuration];

        [self setNextLayer: selectedLayer];        
        [self setSelectedLayer: previousLayer];
        [self setPreviousLayer: [self previousLayer]];
    [CATransaction commit];

    /* Move the new previous layer on screen. Without Actions */
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithBool:YES] forKey:kCATransactionDisableActions];
        [self addSublayer:previousLayer];
    [CATransaction commit];
}

- (void)gotoIndex:(NSUInteger)index;
{
    if( selectedIndex == index )
        return;

    if( selectedIndex > index )
    {
        /* It is ok to scroll five layers */
        if( selectedIndex - index < 5  )
        {
            while( index < selectedIndex  )
                [self changeSelectedLayerToPreviousIndex];
            return;
        }
        [self changeSelectedLayerToPreviousIndex];
        [self changeSelectedLayerToPreviousIndex];
        selectedIndex = index;
        [self resetLayers];
    }
    else
    {
        if( index - selectedIndex < 5  )
        {
            while( index > selectedIndex  )
                [self changeSelectedLayerToNextIndex];
            return;
        }
        [self changeSelectedLayerToNextIndex];
        [self changeSelectedLayerToNextIndex];
        selectedIndex = index;
        [self resetLayers];
    }
}

- (void)resetLayers
{
    VLCMediaLayer * layer;
    [CATransaction begin];
    layer = [self previousLayer];
    if( previousLayer != layer )
    {
        if( previousLayer ) [self replaceSublayer:previousLayer with:layer];
        else [self addSublayer:layer];
        [self setPreviousLayer:layer];
    }
    layer = [self selectedLayer];
    if( selectedLayer != layer )
    {
        if( selectedLayer ) [self replaceSublayer:selectedLayer with:layer];
        else [self addSublayer:layer];
        [self setSelectedLayer:layer];
    }
    layer = [self nextLayer];
    if( nextLayer != layer )
    {
        if( nextLayer ) [self replaceSublayer:nextLayer with:layer];
        else [self addSublayer:layer];
        [self setNextLayer:layer];
    }
    [CATransaction commit];
}


- (void)setSelectedLayer:(VLCMediaLayer *)layer
{
    [selectedLayer autorelease];
    if( !layer )
    {
        selectedLayer = nil;
        return;
    }
    selectedLayer = [layer retain];
    selectedLayer.frame = [self bounds];
    [selectedLayer setAutoresizingMask:kCALayerWidthSizable|kCALayerHeightSizable];
}

- (void)setPreviousLayer:(VLCMediaLayer *)layer
{
    [previousLayer autorelease];
    if( !layer )
    {
        previousLayer = nil;
        return;
    }
    previousLayer = [layer retain];
    CGRect frame = [self bounds];
    frame.origin.x -= frame.size.width;
    previousLayer.frame = frame;
    [previousLayer setAutoresizingMask:kCALayerMaxXMargin|kCALayerHeightSizable];
}

- (void)setNextLayer:(VLCMediaLayer *)layer
{
    [nextLayer autorelease];
    if( !layer )
    {
        nextLayer = nil;
        return;
    }
    nextLayer = [layer retain];
    CGRect frame = [self bounds];
    frame.origin.x += frame.size.width;
    nextLayer.frame = frame;
    [nextLayer setAutoresizingMask:kCALayerMinXMargin|kCALayerHeightSizable];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if([keyPath isEqualToString:@"selectionIndex"])
    {
        if ( selectedIndex == NSNotFound || [object selectionIndex] == NSNotFound )
        {
            selectedIndex = [object selectionIndex];
            if(selectedIndex == NSNotFound  && [content count])
            {
                selectedIndex = 0;
            }
            [self resetLayers];
            return;
        }

        [self gotoIndex: [object selectionIndex]];
        return;
    }
    if([keyPath isEqualToString:@"arrangedObjects"] || [keyPath isEqualToString:@"contentMediaList"])
    {
        selectedIndex = [object selectionIndex];
        if(selectedIndex == NSNotFound  && [[object arrangedObjects] count])
        {
            selectedIndex = 0;
        }
        [content release];
        content = [[object arrangedObjects] retain];
        [self resetLayers];
        return;
    }
    [self observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

@end
