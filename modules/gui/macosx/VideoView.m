/*****************************************************************************
 * VideoView.m: MacOS X video output module
 *****************************************************************************
 * Copyright (C) 2002-2013 VLC authors and VideoLAN
 * $Id$
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
#import <string.h>

#import "intf.h"
#import "VideoView.h"
#import "CoreInteraction.h"
#import "MainMenu.h"
#import "MainWindow.h"

#import <vlc_common.h>
#import <vlc_keys.h>


/*****************************************************************************
 * VLCVoutView implementation
 *****************************************************************************/
@implementation VLCVoutView

#pragma mark -
#pragma mark drag & drop support

- (void)dealloc
{
    if (p_vout)
        vlc_object_release(p_vout);

    [self unregisterDraggedTypes];
    [super dealloc];
}

-(id)initWithFrame:(NSRect)frameRect
{
    if (self = [super initWithFrame:frameRect]) {
        [self registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];
    }

    i_lastScrollWheelDirection = 0;
    f_cumulated_magnification = 0.0;

    return self;
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
    BOOL b_returned;
    b_returned = [[VLCCoreInteraction sharedInstance] performDragOperation: sender];

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
                playlist_t * p_playlist = pl_Get(VLCIntf);
                 if (var_GetBool(p_playlist, "fullscreen"))
                     [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            }
            /* handle Lion's default key combo for fullscreen-toggle in addition to our own hotkeys */
            else if (key == 'f' && i_pressed_modifiers & NSControlKeyMask && i_pressed_modifiers & NSCommandKeyMask)
                [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            else if (p_vout) {
                val.i_int |= (int)CocoaKeyToVLC(key);
                var_Set(p_vout->p_libvlc, "key-pressed", val);
            }
            else
                msg_Dbg(VLCIntf, "could not send keyevent to VLC core");

            return;
        }
    }
    [super keyDown: o_event];
}

- (BOOL)performKeyEquivalent:(NSEvent *)o_event
{
    return [[VLCMainWindow sharedInstance] performKeyEquivalent: o_event];
}

- (void)mouseDown:(NSEvent *)o_event
{
    if (([o_event type] == NSLeftMouseDown) && (! ([o_event modifierFlags] &  NSControlKeyMask))) {
        if ([o_event clickCount] > 1)
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
    } else if (([o_event type] == NSRightMouseDown) ||
               (([o_event type] == NSLeftMouseDown) &&
               ([o_event modifierFlags] &  NSControlKeyMask)))
        [NSMenu popUpContextMenu: [[VLCMainMenu sharedInstance] voutMenu] withEvent: o_event forView: self];

    [super mouseDown: o_event];
}

- (void)rightMouseDown:(NSEvent *)o_event
{
    if ([o_event type] == NSRightMouseDown)
        [NSMenu popUpContextMenu: [[VLCMainMenu sharedInstance] voutMenu] withEvent: o_event forView: self];

    [super mouseDown: o_event];
}

- (void)rightMouseUp:(NSEvent *)o_event
{
    if ([o_event type] == NSRightMouseUp)
        [NSMenu popUpContextMenu: [[VLCMainMenu sharedInstance] voutMenu] withEvent: o_event forView: self];

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
    /* release the scroll direction 0.8 secs after the last event */
    if (([NSDate timeIntervalSinceReferenceDate] - t_lastScrollEvent) >= 0.80)
        i_lastScrollWheelDirection = 0;
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    intf_thread_t * p_intf = VLCIntf;
    CGFloat f_deltaX = [theEvent deltaX];
    CGFloat f_deltaY = [theEvent deltaY];

    if (!OSX_SNOW_LEOPARD && [theEvent isDirectionInvertedFromDevice]) {
        f_deltaX = -f_deltaX;
        f_deltaY = -f_deltaY;
    }

    CGFloat f_yabsvalue = f_deltaY > 0.0f ? f_deltaY : -f_deltaY;
    CGFloat f_xabsvalue = f_deltaX > 0.0f ? f_deltaX : -f_deltaX;

    int i_yvlckey, i_xvlckey = 0;
    if (f_deltaY < 0.0f)
        i_yvlckey = KEY_MOUSEWHEELDOWN;
    else
        i_yvlckey = KEY_MOUSEWHEELUP;

    if (f_deltaX < 0.0f)
        i_xvlckey = KEY_MOUSEWHEELRIGHT;
    else
        i_xvlckey = KEY_MOUSEWHEELLEFT;

    /* in the following, we're forwarding either a x or a y event */
    /* Multiple key events are send depending on the intensity of the event */
    /* the opposite direction is being blocked for 0.8 secs */
    if (f_yabsvalue > 0.05) {
        if (i_lastScrollWheelDirection < 0) // last was a X
            return;

        i_lastScrollWheelDirection = 1; // Y
        for (NSUInteger i = 0; i < (int)(f_yabsvalue/4.+1.); i++)
            var_SetInteger(p_intf->p_libvlc, "key-pressed", i_yvlckey);

        t_lastScrollEvent = [NSDate timeIntervalSinceReferenceDate];
        [self performSelector:@selector(resetScrollWheelDirection)
                   withObject: NULL
                   afterDelay:1.00];
        return;
    }
    if (f_xabsvalue > 0.05) {
        if (i_lastScrollWheelDirection > 0) // last was a Y
            return;

        i_lastScrollWheelDirection = -1; // X
        for (NSUInteger i = 0; i < (int)(f_xabsvalue/6.+1.); i++)
            var_SetInteger(p_intf->p_libvlc, "key-pressed", i_xvlckey);

        t_lastScrollEvent = [NSDate timeIntervalSinceReferenceDate];
        [self performSelector:@selector(resetScrollWheelDirection)
                   withObject: NULL
                   afterDelay:1.00];
    }
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
