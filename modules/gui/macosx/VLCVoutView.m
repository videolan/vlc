/*****************************************************************************
 * VLCVoutView.m: MacOS X video output module
 *****************************************************************************
 * Copyright (C) 2002-2014 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import <stdlib.h>                                                 /* free() */

#import "CompatibilityFixes.h"
#import "VLCMain.h"
#import "VLCCoreInteraction.h"
#import "VLCVoutView.h"
#import "VLCMainMenu.h"

#import <QuartzCore/QuartzCore.h>

#import <vlc_actions.h>


/*****************************************************************************
 * VLCVoutView implementation
 *****************************************************************************/
@interface VLCVoutView()
{
    NSTimer *p_scrollTimer;
    NSInteger i_lastScrollWheelDirection;

    CGFloat f_cumulatedXScrollValue;
    CGFloat f_cumulatedYScrollValue;

    CGFloat f_cumulated_magnification;

    vout_thread_t *p_vout;
}
@end

@implementation VLCVoutView

#pragma mark -
#pragma mark drag & drop support

- (void)dealloc
{
    if (p_vout)
        vlc_object_release(p_vout);

    [self unregisterDraggedTypes];
}

-(id)initWithFrame:(NSRect)frameRect
{
    if (self = [super initWithFrame:frameRect]) {
        [self registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];
        i_lastScrollWheelDirection = 0;
        f_cumulated_magnification = 0.0;
    }

    return self;
}

- (void)drawRect:(NSRect)rect
{
    // Draw black area in case first frame is not drawn yet
    [[NSColor blackColor] setFill];
    NSRectFill(rect);
}

- (void)addVoutLayer:(CALayer *)aLayer
{
    if (self.layer == nil) {
        [self setLayer:[CALayer layer]];
        [self setWantsLayer:YES];
    }

    [CATransaction begin];
    aLayer.opaque = 1.;
    aLayer.hidden = NO;
    aLayer.bounds = self.layer.bounds;
    [self.layer addSublayer:aLayer];
    [self setNeedsDisplay:YES];
    [aLayer setNeedsDisplay];
    CGRect frame = aLayer.bounds;
    frame.origin.x = frame.origin.y = 0.;
    aLayer.frame = frame;
    [CATransaction commit];
}

- (void)removeVoutLayer:(CALayer *)aLayer
{
    [CATransaction begin];
    [aLayer removeFromSuperlayer];
    [CATransaction commit];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric)
        return NSDragOperationGeneric;
    return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    // FIXME: re-implement drag and drop of new input items to the playlist
    BOOL b_returned = NO;

    [self setNeedsDisplay:YES];
    return b_returned;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

#pragma mark -
#pragma mark vout actions

- (void)keyDown:(NSEvent *)o_event
{
    unichar key = 0;
    vlc_value_t val;
    unsigned int i_pressed_modifiers = 0;
    val.i_int = 0;

    i_pressed_modifiers = [o_event modifierFlags];

    if (i_pressed_modifiers & NSShiftKeyMask)
        val.i_int |= KEY_MODIFIER_SHIFT;
    if (i_pressed_modifiers & NSControlKeyMask)
        val.i_int |= KEY_MODIFIER_CTRL;
    if (i_pressed_modifiers & NSAlternateKeyMask)
        val.i_int |= KEY_MODIFIER_ALT;
    if (i_pressed_modifiers & NSCommandKeyMask)
        val.i_int |= KEY_MODIFIER_COMMAND;

    NSString * characters = [o_event charactersIgnoringModifiers];
    if ([characters length] > 0) {
        key = [[characters lowercaseString] characterAtIndex: 0];

        if (key) {
            /* Escape should always get you out of fullscreen */
            if (key == (unichar) 0x1b) {
                playlist_t * p_playlist = pl_Get(getIntf());
                 if (var_GetBool(p_playlist, "fullscreen"))
                     [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            }
            /* handle Lion's default key combo for fullscreen-toggle in addition to our own hotkeys */
            else if (key == 'f' && i_pressed_modifiers & NSControlKeyMask && i_pressed_modifiers & NSCommandKeyMask)
                [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            else if (p_vout) {
                val.i_int |= (int)CocoaKeyToVLC(key);
                var_Set(p_vout->obj.libvlc, "key-pressed", val);
            }
            else
                msg_Dbg(getIntf(), "could not send keyevent to VLC core");

            return;
        }
    }
    [super keyDown: o_event];
}

- (BOOL)performKeyEquivalent:(NSEvent *)o_event
{
    return [[[VLCMain sharedInstance] mainWindow] performKeyEquivalent: o_event];
}

- (void)mouseDown:(NSEvent *)o_event
{
    if (([o_event type] == NSLeftMouseDown) && (! ([o_event modifierFlags] &  NSControlKeyMask))) {
        if ([o_event clickCount] == 2)
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];

    } else if (([o_event type] == NSRightMouseDown) ||
               (([o_event type] == NSLeftMouseDown) &&
               ([o_event modifierFlags] &  NSControlKeyMask)))
        [NSMenu popUpContextMenu: [[[VLCMain sharedInstance] mainMenu] voutMenu] withEvent: o_event forView: self];

    [super mouseDown: o_event];
}

- (void)rightMouseDown:(NSEvent *)o_event
{
    if ([o_event type] == NSRightMouseDown)
        [NSMenu popUpContextMenu: [[[VLCMain sharedInstance] mainMenu] voutMenu] withEvent: o_event forView: self];

    [super mouseDown: o_event];
}

- (void)rightMouseUp:(NSEvent *)o_event
{
    if ([o_event type] == NSRightMouseUp)
        [NSMenu popUpContextMenu: [[[VLCMain sharedInstance] mainMenu] voutMenu] withEvent: o_event forView: self];

    [super mouseUp: o_event];
}

- (void)mouseMoved:(NSEvent *)o_event
{
    NSPoint ml = [self convertPoint: [o_event locationInWindow] fromView: nil];
    if ([self mouse: ml inRect: [self bounds]])
        [[VLCMain sharedInstance] showFullscreenController];

    [super mouseMoved: o_event];
}

- (void)resetScrollWheelDirection
{
    i_lastScrollWheelDirection = 0;
    f_cumulatedXScrollValue = f_cumulatedYScrollValue = 0.;
    msg_Dbg(getIntf(), "Reset scrolling timer");
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    const CGFloat f_xThreshold = 0.8;
    const CGFloat f_yThreshold = 1.0;
    const CGFloat f_directionThreshold = 0.05;

    intf_thread_t * p_intf = getIntf();
    CGFloat f_deltaX = [theEvent deltaX];
    CGFloat f_deltaY = [theEvent deltaY];

    if ([theEvent isDirectionInvertedFromDevice]) {
        f_deltaX = -f_deltaX;
        f_deltaY = -f_deltaY;
    }

    CGFloat f_deltaXAbs = ABS(f_deltaX);
    CGFloat f_deltaYAbs = ABS(f_deltaY);

    // A mouse scroll wheel has lower sensitivity. We want to scroll at least
    // with every event here.
    BOOL isMouseScrollWheel = ([theEvent subtype] == NSMouseEventSubtype);
    if (isMouseScrollWheel && f_deltaYAbs < f_yThreshold)
        f_deltaY = f_deltaY > 0. ? f_yThreshold : -f_yThreshold;

    if (isMouseScrollWheel && f_deltaXAbs < f_xThreshold)
        f_deltaX = f_deltaX > 0. ? f_xThreshold : -f_xThreshold;

    /* in the following, we're forwarding either a x or a y event */
    /* Multiple key events are send depending on the intensity of the event */
    /* the opposite direction is being blocked for a couple of milli seconds */
    if (f_deltaYAbs > f_directionThreshold) {
        if (i_lastScrollWheelDirection < 0) // last was a X
            return;
        i_lastScrollWheelDirection = 1; // Y

        f_cumulatedYScrollValue += f_deltaY;
        int key = f_cumulatedYScrollValue < 0.0f ? KEY_MOUSEWHEELDOWN : KEY_MOUSEWHEELUP;

        while (ABS(f_cumulatedYScrollValue) >= f_yThreshold) {
            f_cumulatedYScrollValue -= (f_cumulatedYScrollValue > 0 ? f_yThreshold : -f_yThreshold);
            var_SetInteger(p_intf->obj.libvlc, "key-pressed", key);
            msg_Dbg(p_intf, "Scrolling in y direction");
        }

    } else if (f_deltaXAbs > f_directionThreshold) {
        if (i_lastScrollWheelDirection > 0) // last was a Y
            return;
        i_lastScrollWheelDirection = -1; // X

        f_cumulatedXScrollValue += f_deltaX;
        int key = f_cumulatedXScrollValue < 0.0f ? KEY_MOUSEWHEELRIGHT : KEY_MOUSEWHEELLEFT;

        while (ABS(f_cumulatedXScrollValue) >= f_xThreshold) {
            f_cumulatedXScrollValue -= (f_cumulatedXScrollValue > 0 ? f_xThreshold : -f_xThreshold);
            var_SetInteger(p_intf->obj.libvlc, "key-pressed", key);
            msg_Dbg(p_intf, "Scrolling in x direction");
        }
    }

    if (p_scrollTimer) {
        [p_scrollTimer invalidate];
        p_scrollTimer = nil;
    }
    p_scrollTimer = [NSTimer scheduledTimerWithTimeInterval:0.4 target:self selector:@selector(resetScrollWheelDirection) userInfo:nil repeats:NO];
}

#pragma mark -
#pragma mark Handling of vout related actions

- (void)setVoutThread:(vout_thread_t *)p_vout_thread
{
    assert(p_vout == NULL);
    p_vout = p_vout_thread;
    vlc_object_hold(p_vout);
}

- (vout_thread_t *)voutThread
{
    if (p_vout) {
        vlc_object_hold(p_vout);
        return p_vout;
    }

    return NULL;
}

- (void)releaseVoutThread
{
    if (p_vout) {
        vlc_object_release(p_vout);
        p_vout = NULL;
    }
}

#pragma mark -
#pragma mark Basic view behaviour and touch events handling

- (BOOL)mouseDownCanMoveWindow
{
    if (p_vout) {
        bool b_vrnav_can_change = var_GetBool(p_vout, "viewpoint-changeable");
        return !b_vrnav_can_change;
    }

    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    return YES;
}

- (BOOL)resignFirstResponder
{
    /* while we need to be the first responder most of the time, we need to give up that status when toggling the playlist */
    return YES;
}

-(void)didAddSubview:(NSView *)subview
{
    [[self window] makeFirstResponder: subview];
}

- (void)magnifyWithEvent:(NSEvent *)event
{
    f_cumulated_magnification += [event magnification];

    // This is the result of [NSEvent standardMagnificationThreshold].
    // Unfortunately, this is a private API, currently.
    CGFloat f_threshold = 0.3;
    BOOL b_fullscreen = [(VLCVideoWindowCommon *)[self window] fullscreen];

    if ((f_cumulated_magnification > f_threshold && !b_fullscreen) || (f_cumulated_magnification < -f_threshold && b_fullscreen)) {
        f_cumulated_magnification = 0.0;
        [[VLCCoreInteraction sharedInstance] toggleFullscreen];
    }
}

- (void)beginGestureWithEvent:(NSEvent *)event
{
    f_cumulated_magnification = 0.0;
}

@end
