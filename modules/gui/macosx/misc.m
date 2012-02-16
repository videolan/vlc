/*****************************************************************************
 * misc.m: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003-2011 VLC authors and VideoLAN
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
        b_isFullscreen = NO;
        b_isset_canBecomeKeyWindow = NO;
        /* we don't want this window to be restored on relaunch */
        if (OSX_LION)
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

- (void)setFullscreen:(BOOL)b_var
{
    b_isFullscreen = b_var;
}

- (BOOL)isFullscreen
{
    return b_isFullscreen;
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

            input_thread_t * p_input = pl_CurrentInput( VLCIntf );
            BOOL b_returned = NO;

            if (count == 1 && p_input)
            {
                b_returned = input_AddSubtitle( p_input, make_URI([[o_values objectAtIndex:0] UTF8String], NULL), true );
                vlc_object_release( p_input );
                if(!b_returned)
                    return YES;
            }
            else if( p_input )
                vlc_object_release( p_input );

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
    [self setImageScaling: NSScaleToFit];
    [self setImageFrameStyle: NSImageFrameNone];
    [self setImageAlignment: NSImageAlignCenter];
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

            input_thread_t * p_input = pl_CurrentInput( VLCIntf );
            BOOL b_returned = NO;

            if (count == 1 && p_input)
            {
                b_returned = input_AddSubtitle( p_input, make_URI([[o_values objectAtIndex:0] UTF8String], NULL), true );
                vlc_object_release( p_input );
                if(!b_returned)
                    return YES;
            }
            else if( p_input )
                vlc_object_release( p_input );

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

- (void)awakeFromNib
{
    if (config_GetInt( VLCIntf, "macosx-interfacestyle" ))
        o_knob_img = [NSImage imageNamed:@"progression-knob_dark"];
    else
        o_knob_img = [NSImage imageNamed:@"progression-knob"];
    img_rect.size = [o_knob_img size];
    img_rect.origin.x = img_rect.origin.y = 0;
}

- (void)dealloc
{
    [o_knob_img release];
    [super dealloc];
}

- (CGFloat)knobPosition
{
    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    knobRect.origin.x += knobRect.size.width / 2;
    return knobRect.origin.x;
}

- (void)drawKnobInRect:(NSRect)knobRect
{
    knobRect.origin.x += (knobRect.size.width - img_rect.size.width) / 2;
    knobRect.size.width = img_rect.size.width;
    knobRect.size.height = img_rect.size.height;
    [o_knob_img drawInRect:knobRect fromRect:img_rect operation:NSCompositeSourceOver fraction:1];
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

- (void)awakeFromNib
{
    BOOL b_dark = config_GetInt( VLCIntf, "macosx-interfacestyle" );
    if (b_dark)
        img = [NSImage imageNamed:@"volume-slider-knob_dark"];
    else
        img = [NSImage imageNamed:@"volume-slider-knob"];

    image_rect.size = [img size];
    image_rect.origin.x = 0;

    if (b_dark)
        image_rect.origin.y = -1;
    else
        image_rect.origin.y = 0;
}

- (void)drawKnobInRect:(NSRect)knobRect
{
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

- (void)awakeFromNib
{
    NSColor *o_string_color;
    if (!config_GetInt( VLCIntf, "macosx-interfacestyle"))
        o_string_color = [NSColor colorWithCalibratedRed:0.229 green:0.229 blue:0.229 alpha:100.0];
    else
        o_string_color = [NSColor colorWithCalibratedRed:0.64 green:0.64 blue:0.64 alpha:100.0];

    textAlignment = NSCenterTextAlignment;
    o_string_attributes_dict = [[NSDictionary dictionaryWithObjectsAndKeys: o_string_color, NSForegroundColorAttributeName, [NSFont titleBarFontOfSize:10.0], NSFontAttributeName, nil] retain];
}

- (void)setAlignment:(NSTextAlignment)alignment
{
    textAlignment = alignment;
    [self setStringValue:[self stringValue]];
}

- (void)dealloc
{
    [o_string_shadow release];
    [o_string_attributes_dict release];
}

- (void)setStringValue:(NSString *)string
{
    if (!o_string_shadow)
    {
        o_string_shadow = [[NSShadow alloc] init];
        [o_string_shadow setShadowColor: [NSColor colorWithCalibratedWhite:1.0 alpha:0.5]];
        [o_string_shadow setShadowOffset:NSMakeSize(0.0, -1.5)];
        [o_string_shadow setShadowBlurRadius:0.0];
    }

    NSMutableAttributedString *o_attributed_string = [[NSMutableAttributedString alloc] initWithString:string attributes: o_string_attributes_dict];
    NSUInteger i_stringLength = [string length];

    [o_attributed_string addAttribute: NSShadowAttributeName value: o_string_shadow range: NSMakeRange(0, i_stringLength)];
    [o_attributed_string setAlignment: textAlignment range: NSMakeRange(0, i_stringLength)];
    [self setAttributedStringValue: o_attributed_string];
    [o_attributed_string release];
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
 * comment 1 + 2 taken from NSSplitView.h (10.7 SDK)
 *****************************************************************************/
@implementation VLCMainWindowSplitView : NSSplitView
/* Return the color of the dividers that the split view is drawing between subviews. The default implementation of this method returns [NSColor clearColor] for the thick divider style. It will also return [NSColor clearColor] for the thin divider style when the split view is in a textured window. All other thin dividers are drawn with a color that looks good between two white panes. You can override this method to change the color of dividers.
 */
- (NSColor *)dividerColor
{
    return [NSColor colorWithCalibratedRed:.60 green:.60 blue:.60 alpha:1.];
}

/* Return the thickness of the dividers that the split view is drawing between subviews. The default implementation returns a value that depends on the divider style. You can override this method to change the size of dividers.
 */
- (CGFloat)dividerThickness
{
    return 1.0;
}

- (void)adjustSubviews
{
    NSArray *o_subviews = [self subviews];
    NSRect viewDimensions = [self frame];
    NSRect leftViewDimensions = [[o_subviews objectAtIndex:0] frame];
    NSRect rightViewDimensions = [[o_subviews objectAtIndex:1] frame];
    CGFloat f_dividerThickness = [self dividerThickness];

    leftViewDimensions.size.height = viewDimensions.size.height;
    [[o_subviews objectAtIndex:0] setFrame: leftViewDimensions];

    rightViewDimensions.origin.x = leftViewDimensions.size.width + f_dividerThickness;
    rightViewDimensions.size.width = viewDimensions.size.width - leftViewDimensions.size.width - f_dividerThickness;
    rightViewDimensions.size.height = viewDimensions.size.height;
    [[o_subviews objectAtIndex:1] setFrame: rightViewDimensions];
}
@end

/*****************************************************************************
 * VLCThreePartImageView interface
 *****************************************************************************/
@implementation VLCThreePartImageView
- (void)dealloc
{
    [o_left_img release];
    [o_middle_img release];
    [o_right_img release];

    [super dealloc];
}

- (void)setImagesLeft:(NSImage *)left middle: (NSImage *)middle right:(NSImage *)right
{
    if (o_left_img)
        [o_left_img release];
    if (o_middle_img)
        [o_middle_img release];
    if (o_right_img)
        [o_right_img release];

    o_left_img = [left retain];
    o_middle_img = [middle retain];
    o_right_img = [right retain];
}

- (void)drawRect:(NSRect)rect
{
    NSRect bnds = [self bounds];
    NSDrawThreePartImage( bnds, o_left_img, o_middle_img, o_right_img, NO, NSCompositeSourceOver, 1, NO );
}

@end
