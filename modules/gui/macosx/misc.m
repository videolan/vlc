/*****************************************************************************
 * misc.m: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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
#import <Carbon/Carbon.h>

#import "CompatibilityFixes.h"
#import "intf.h"                                          /* VLCApplication */
#import "MainWindow.h"
#import "misc.h"
#import "playlist.h"
#import "controls.h"
#import <vlc_url.h>

/*****************************************************************************
 * NSAnimation (VLCAdditions)
 *
 *  Missing extension to NSAnimation
 *****************************************************************************/

@implementation NSAnimation (VLCAdditions)
/* fake class attributes  */
static NSMapTable *VLCAdditions_userInfo = NULL;

+ (void)load
{
    /* init our fake object attribute */
    VLCAdditions_userInfo = NSCreateMapTable(NSNonRetainedObjectMapKeyCallBacks, NSObjectMapValueCallBacks, 16);
}

- (void)dealloc
{
    NSMapRemove(VLCAdditions_userInfo, self);
    [super dealloc];
}

- (void)setUserInfo: (void *)userInfo
{
    NSMapInsert(VLCAdditions_userInfo, self, (void*)userInfo);
}

- (void *)userInfo
{
    return NSMapGet(VLCAdditions_userInfo, self);
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
    NSUInteger count = [[NSScreen screens] count];

    for( NSUInteger i = 0; i < count; i++ )
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
	return (CGDirectDisplayID)[[[self deviceDescription] objectForKey: @"NSScreenNumber"] intValue];
}

- (void)blackoutOtherScreens
{
    /* Free our previous blackout window (follow blackoutWindow alloc strategy) */
    [blackoutWindows makeObjectsPerformSelector:@selector(close)];
    [blackoutWindows removeAllObjects];

    NSUInteger screenCount = [[NSScreen screens] count];
    for(NSUInteger i = 0; i < screenCount; i++)
    {
        NSScreen *screen = [[NSScreen screens] objectAtIndex: i];
        VLCWindow *blackoutWindow;
        NSRect screen_rect;

        if([self isScreen: screen])
            continue;

        screen_rect = [screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0;

        /* blackoutWindow alloc strategy
            - The NSMutableArray blackoutWindows has the blackoutWindow references
            - blackoutOtherDisplays is responsible for alloc/releasing its Windows
        */
        blackoutWindow = [[VLCWindow alloc] initWithContentRect: screen_rect styleMask: NSBorderlessWindowMask
                backing: NSBackingStoreBuffered defer: NO screen: screen];
        [blackoutWindow setBackgroundColor:[NSColor blackColor]];
        [blackoutWindow setLevel: NSFloatingWindowLevel]; /* Disappear when Expose is triggered */

        [blackoutWindow displayIfNeeded];
        [blackoutWindow orderFront: self animate: YES];

        [blackoutWindows addObject: blackoutWindow];
        [blackoutWindow release];

        if( [screen isMainScreen ] )
        {
            if ([screen isMainScreen])
            {
                if (OSX_LEOPARD)
                    SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
                else
                    [NSApp setPresentationOptions:(NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar)];
            }
        }
    }
}

+ (void)unblackoutScreens
{
    NSUInteger blackoutWindowCount = [blackoutWindows count];

    for(NSUInteger i = 0; i < blackoutWindowCount; i++)
    {
        VLCWindow *blackoutWindow = [blackoutWindows objectAtIndex: i];
        [blackoutWindow closeAndAnimate: YES];
    }

    if (OSX_LEOPARD)
        SetSystemUIMode( kUIModeNormal, kUIOptionAutoShowMenuBar);
    else
        [NSApp setPresentationOptions:(NSApplicationPresentationDefault)];
}

@end

/*****************************************************************************
 * VLCWindow
 *
 *  Missing extension to NSWindow
 *****************************************************************************/

@implementation VLCWindow
- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask
    backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    self = [super initWithContentRect:contentRect styleMask:styleMask backing:backingType defer:flag];
    if( self )
    {
        b_isset_canBecomeKeyWindow = NO;
        /* we don't want this window to be restored on relaunch */
        if ([self respondsToSelector:@selector(setRestorable:)])
            [self setRestorable:NO];
    }
    return self;
}
- (void)setCanBecomeKeyWindow: (BOOL)canBecomeKey
{
    b_isset_canBecomeKeyWindow = YES;
    b_canBecomeKeyWindow = canBecomeKey;
}

- (BOOL)canBecomeKeyWindow
{
    if(b_isset_canBecomeKeyWindow)
        return b_canBecomeKeyWindow;

    return [super canBecomeKeyWindow];
}

- (void)closeAndAnimate: (BOOL)animate
{
    NSInvocation *invoc;

    if (!animate)
    {
        [super close];
        return;
    }

    invoc = [NSInvocation invocationWithMethodSignature:[super methodSignatureForSelector:@selector(close)]];
    [invoc setTarget: self];

    if (![self isVisible] || [self alphaValue] == 0.0)
    {
        [super close];
        return;
    }

    [self orderOut: self animate: YES callback: invoc];
}

- (void)orderOut: (id)sender animate: (BOOL)animate
{
    NSInvocation *invoc = [NSInvocation invocationWithMethodSignature:[super methodSignatureForSelector:@selector(orderOut:)]];
    [invoc setTarget: self];
    [invoc setArgument: sender atIndex: 0];
    [self orderOut: sender animate: animate callback: invoc];
}

- (void)orderOut: (id)sender animate: (BOOL)animate callback:(NSInvocation *)callback
{
    NSViewAnimation *anim;
    NSViewAnimation *current_anim;
    NSMutableDictionary *dict;

    if (!animate)
    {
        [self orderOut: sender];
        return;
    }

    dict = [[NSMutableDictionary alloc] initWithCapacity:2];

    [dict setObject:self forKey:NSViewAnimationTargetKey];

    [dict setObject:NSViewAnimationFadeOutEffect forKey:NSViewAnimationEffectKey];
    anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict, nil]];
    [dict release];

    [anim setAnimationBlockingMode:NSAnimationNonblocking];
    [anim setDuration:0.9];
    [anim setFrameRate:30];
    [anim setUserInfo: callback];

    @synchronized(self) {
        current_anim = self->animation;

        if ([[[current_anim viewAnimations] objectAtIndex:0] objectForKey: NSViewAnimationEffectKey] == NSViewAnimationFadeOutEffect && [current_anim isAnimating])
        {
            [anim release];
        }
        else
        {
            if (current_anim)
            {
                [current_anim stopAnimation];
                [anim setCurrentProgress:1.0-[current_anim currentProgress]];
                [current_anim release];
            }
            else
                [anim setCurrentProgress:1.0 - [self alphaValue]];
            self->animation = anim;
            [self setDelegate: self];
            [anim startAnimation];
        }
    }
}

- (void)orderFront: (id)sender animate: (BOOL)animate
{
    NSViewAnimation *anim;
    NSViewAnimation *current_anim;
    NSMutableDictionary *dict;

    if (!animate)
    {
        [super orderFront: sender];
        [self setAlphaValue: 1.0];
        return;
    }

    if (![self isVisible])
    {
        [self setAlphaValue: 0.0];
        [super orderFront: sender];
    }
    else if ([self alphaValue] == 1.0)
    {
        [super orderFront: self];
        return;
    }

    dict = [[NSMutableDictionary alloc] initWithCapacity:2];

    [dict setObject:self forKey:NSViewAnimationTargetKey];

    [dict setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];
    anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict, nil]];
    [dict release];

    [anim setAnimationBlockingMode:NSAnimationNonblocking];
    [anim setDuration:0.5];
    [anim setFrameRate:30];

    @synchronized(self) {
        current_anim = self->animation;

        if ([[[current_anim viewAnimations] objectAtIndex:0] objectForKey: NSViewAnimationEffectKey] == NSViewAnimationFadeInEffect && [current_anim isAnimating])
        {
            [anim release];
        }
        else
        {
            if (current_anim)
            {
                [current_anim stopAnimation];
                [anim setCurrentProgress:1.0 - [current_anim currentProgress]];
                [current_anim release];
            }
            else
                [anim setCurrentProgress:[self alphaValue]];
            self->animation = anim;
            [self setDelegate: self];
            [self orderFront: sender];
            [anim startAnimation];
        }
    }
}

- (void)animationDidEnd:(NSAnimation*)anim
{
    if ([self alphaValue] <= 0.0)
    {
        NSInvocation * invoc;
        [super orderOut: nil];
        [self setAlphaValue: 1.0];
        if ((invoc = [anim userInfo]))
            [invoc invoke];
    }
}
@end

/*****************************************************************************
 * VLCControllerView
 *****************************************************************************/

@implementation VLCControllerView

- (void)dealloc
{
    [self unregisterDraggedTypes];
    [super dealloc];
}

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects:NSTIFFPboardType,
        NSFilenamesPboardType, nil]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask])
                == NSDragOperationGeneric)
    {
        return NSDragOperationGeneric;
    }
    else
    {
        return NSDragOperationNone;
    }
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *o_paste = [sender draggingPasteboard];
    NSArray *o_types = [NSArray arrayWithObjects: NSFilenamesPboardType, nil];
    NSString *o_desired_type = [o_paste availableTypeFromArray:o_types];
    NSData *o_carried_data = [o_paste dataForType:o_desired_type];

    if( o_carried_data )
    {
        if ([o_desired_type isEqualToString:NSFilenamesPboardType])
        {
            NSArray *o_array = [NSArray array];
            NSArray *o_values = [[o_paste propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
            NSUInteger count = [o_values count];

            for( NSUInteger i = 0; i < count; i++)
            {
                NSDictionary *o_dic;
                char *psz_uri = make_URI([[o_values objectAtIndex:i] UTF8String], NULL);
                if( !psz_uri )
                    continue;

                o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];

                free( psz_uri );
                o_array = [o_array arrayByAddingObject: o_dic];
            }
            [(VLCPlaylist *)[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:NO];
            return YES;
        }
    }
    [self setNeedsDisplay:YES];
    return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end

/*****************************************************************************
 * VLBrushedMetalImageView
 *****************************************************************************/

@implementation VLBrushedMetalImageView

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (void)dealloc
{
    [self unregisterDraggedTypes];
    [super dealloc];
}

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects:NSTIFFPboardType,
        NSFilenamesPboardType, nil]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask])
                == NSDragOperationGeneric)
    {
        return NSDragOperationGeneric;
    }
    else
    {
        return NSDragOperationNone;
    }
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *o_paste = [sender draggingPasteboard];
    NSArray *o_types = [NSArray arrayWithObjects: NSFilenamesPboardType, nil];
    NSString *o_desired_type = [o_paste availableTypeFromArray:o_types];
    NSData *o_carried_data = [o_paste dataForType:o_desired_type];
    BOOL b_autoplay = config_GetInt( VLCIntf, "macosx-autoplay" );

    if( o_carried_data )
    {
        if ([o_desired_type isEqualToString:NSFilenamesPboardType])
        {
            NSArray *o_array = [NSArray array];
            NSArray *o_values = [[o_paste propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
            NSUInteger count = [o_values count];

            for( NSUInteger i = 0; i < count; i++)
            {
                NSDictionary *o_dic;
                char *psz_uri = make_URI([[o_values objectAtIndex:i] UTF8String], NULL);
                if( !psz_uri )
                    continue;

                o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];
                free( psz_uri );

                o_array = [o_array arrayByAddingObject: o_dic];
            }
            if( b_autoplay )
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:NO];
            else
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:YES];
            return YES;
        }
    }
    [self setNeedsDisplay:YES];
    return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end


/*****************************************************************************
 * MPSlider
 *****************************************************************************/
@implementation MPSlider

void _drawKnobInRect(NSRect knobRect)
{
    // Center knob in given rect
    knobRect.origin.x += (int)((float)(knobRect.size.width - 7)/2.0);
    knobRect.origin.y += (int)((float)(knobRect.size.height - 7)/2.0);

    // Draw diamond
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 3, knobRect.origin.y + 6, 1, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 2, knobRect.origin.y + 5, 3, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 1, knobRect.origin.y + 4, 5, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 0, knobRect.origin.y + 3, 7, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 1, knobRect.origin.y + 2, 5, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 2, knobRect.origin.y + 1, 3, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 3, knobRect.origin.y + 0, 1, 1), NSCompositeSourceOver);
}

void _drawFrameInRect(NSRect frameRect)
{
    // Draw frame
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x, frameRect.origin.y, frameRect.size.width, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x, frameRect.origin.y + frameRect.size.height-1, frameRect.size.width, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x, frameRect.origin.y, 1, frameRect.size.height), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x+frameRect.size.width-1, frameRect.origin.y, 1, frameRect.size.height), NSCompositeSourceOver);
}

- (void)drawRect:(NSRect)rect
{
    // Draw default to make sure the slider behaves correctly
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSRectClip(NSZeroRect);
    [super drawRect:rect];
    [[NSGraphicsContext currentContext] restoreGraphicsState];

    // Full size
    rect = [self bounds];
    int diff = (int)(([[self cell] knobThickness] - 7.0)/2.0) - 1;
    rect.origin.x += diff-1;
    rect.origin.y += diff;
    rect.size.width -= 2*diff-2;
    rect.size.height -= 2*diff;

    // Draw dark
    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    [[[NSColor blackColor] colorWithAlphaComponent:0.6] set];
    _drawFrameInRect(rect);
    _drawKnobInRect(knobRect);

    // Draw shadow
    [[[NSColor blackColor] colorWithAlphaComponent:0.1] set];
    rect.origin.x++;
    rect.origin.y++;
    knobRect.origin.x++;
    knobRect.origin.y++;
    _drawFrameInRect(rect);
    _drawKnobInRect(knobRect);
}

@end

/*****************************************************************************
 * TimeLineSlider
 *****************************************************************************/

@implementation TimeLineSlider

- (void)drawKnobInRect:(NSRect)knobRect
{
    NSRect image_rect;
    NSImage *img = [NSImage imageNamed:@"progression-knob"];
    image_rect.size = [img size];
    image_rect.origin.x = 0;
    image_rect.origin.y = 0;
    knobRect.origin.x += (knobRect.size.width - image_rect.size.width) / 2;
    knobRect.size.width = image_rect.size.width;
    knobRect.size.height = image_rect.size.height;
    [img drawInRect:knobRect fromRect:image_rect operation:NSCompositeSourceOver fraction:1];
}

- (void)drawRect:(NSRect)rect
{
    /* Draw default to make sure the slider behaves correctly */
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSRectClip(NSZeroRect);
    [super drawRect:rect];
    [[NSGraphicsContext currentContext] restoreGraphicsState];

    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    knobRect.origin.y+=1;
    [self drawKnobInRect: knobRect];
}

@end

/*****************************************************************************
 * ITSlider
 *****************************************************************************/

@implementation ITSlider

- (void)drawKnobInRect:(NSRect)knobRect
{
    NSRect image_rect;
    NSImage *img = [NSImage imageNamed:@"volume-slider-knob"];
    image_rect.size = [img size];
    image_rect.origin.x = 0;
    image_rect.origin.y = 0;
    knobRect.origin.x += (knobRect.size.width - image_rect.size.width) / 2;
    knobRect.size.width = image_rect.size.width;
    knobRect.size.height = image_rect.size.height;
    [img drawInRect:knobRect fromRect:image_rect operation:NSCompositeSourceOver fraction:1];
}

- (void)drawRect:(NSRect)rect
{
    /* Draw default to make sure the slider behaves correctly */
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSRectClip(NSZeroRect);
    [super drawRect:rect];
    [[NSGraphicsContext currentContext] restoreGraphicsState];

    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    knobRect.origin.y+=2;
    [self drawKnobInRect: knobRect];
}

@end

/*****************************************************************************
 * VLCTimeField implementation
 *****************************************************************************
 * we need this to catch our click-event in the controller window
 *****************************************************************************/

@implementation VLCTimeField
+ (void)initialize{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:@"NO" forKey:@"DisplayTimeAsTimeRemaining"];

    [defaults registerDefaults:appDefaults];
}

- (void)mouseDown: (NSEvent *)ourEvent
{
    if( [ourEvent clickCount] > 1 )
        [[[VLCMain sharedInstance] controls] goToSpecificTime: nil];
    else
    {
        if ([[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayTimeAsTimeRemaining"])
            [[NSUserDefaults standardUserDefaults] setObject:@"NO" forKey:@"DisplayTimeAsTimeRemaining"];
        else
            [[NSUserDefaults standardUserDefaults] setObject:@"YES" forKey:@"DisplayTimeAsTimeRemaining"];
    }
}

- (BOOL)timeRemaining
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayTimeAsTimeRemaining"];
}
@end

/*****************************************************************************
 * VLCMainWindowSplitView implementation
 * comments taken from NSSplitView.h (10.7 SDK)
 *****************************************************************************/
@implementation VLCMainWindowSplitView : NSSplitView
/* Return the color of the dividers that the split view is drawing between subviews. The default implementation of this method returns [NSColor clearColor] for the thick divider style. It will also return [NSColor clearColor] for the thin divider style when the split view is in a textured window. All other thin dividers are drawn with a color that looks good between two white panes. You can override this method to change the color of dividers.
 */
- (NSColor *)dividerColor
{
    return [NSColor blackColor];
}

/* Return the thickness of the dividers that the split view is drawing between subviews. The default implementation returns a value that depends on the divider style. You can override this method to change the size of dividers.
 */
- (CGFloat)dividerThickness
{
    return 0.01;
}
@end
