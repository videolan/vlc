/*****************************************************************************
 * VLCVoutView.m: MacOS X video output module
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
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

#import "VLCVoutView.h"

#import <QuartzCore/QuartzCore.h>

#import "coreinteraction/VLCHotkeysController.h"
#import "main/VLCMain.h"
#import "menus/VLCMainMenu.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "windows/video/VLCVideoWindowCommon.h"

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
    vlc_window_t *_wnd;
    dispatch_queue_t _eventQueue;
    NSTrackingArea *_trackingArea;
    VLCPlayerController *_playerController;
    VLCHotkeysController *_hotkeysController;
}
@end

@implementation VLCVoutView

#pragma mark -
#pragma mark drag & drop support

- (void)dealloc
{
    dispatch_sync(_eventQueue, ^{
        _wnd = NULL;
    });
    if (p_vout)
        vout_Release(p_vout);

    [self unregisterDraggedTypes];
}

-(id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)awakeFromNib
{
    [self setup];
}

- (void)setup
{
    [self registerForDraggedTypes:@[NSFilenamesPboardType]];
    i_lastScrollWheelDirection = 0;
    f_cumulated_magnification = 0.0;

    _playerController = VLCMain.sharedInstance.playlistController.playerController;
    _hotkeysController = [[VLCHotkeysController alloc] init];
    _eventQueue = dispatch_queue_create("org.videolan.vlc.vout.mouseevents", DISPATCH_QUEUE_SERIAL);
}

- (void)layout {
    NSRect bounds = [self convertRectToBacking:self.bounds];
    // dispatch the event async to prevent potential deadlock 
    // with video output's RenderPicture's display lock
    dispatch_async(_eventQueue, ^{
        if (self->_wnd == NULL) {
            return;
        }
        vlc_window_ReportSize(self->_wnd, bounds.size.width, bounds.size.height);
    });
    [super layout];
}

- (void)drawRect:(NSRect)rect
{
    // Draw black area in case first frame is not drawn yet
    [[NSColor blackColor] setFill];
    NSRectFill(rect);
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
    if (![_hotkeysController handleVideoOutputKeyDown:o_event forVideoOutput:p_vout]) {
        [super keyDown: o_event];
    }
}

- (BOOL)performKeyEquivalent:(NSEvent *)o_event
{
    return [_hotkeysController performKeyEquivalent:o_event];
}

- (void)updateTrackingAreas {
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }

    _trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.bounds
        options: NSTrackingMouseMoved | NSTrackingActiveAlways
        owner:self
        userInfo:nil
    ];

    [self addTrackingArea:_trackingArea];
}

- (void)mouseDown:(NSEvent *)o_event
{
    if (([o_event type] == NSLeftMouseDown) && (! ([o_event modifierFlags] &  NSControlKeyMask))) {
        if ([o_event clickCount] == 1)
            dispatch_sync(_eventQueue, ^{
                if (_wnd)
                    vlc_window_ReportMousePressed(_wnd, MOUSE_BUTTON_LEFT);
            });
        if ([o_event clickCount] == 2)
            [_playerController toggleFullscreen];

    } else if (([o_event type] == NSRightMouseDown) ||
               (([o_event type] == NSLeftMouseDown) &&
               ([o_event modifierFlags] &  NSControlKeyMask)))
        [NSMenu popUpContextMenu: VLCMain.sharedInstance.mainMenu.voutMenu withEvent: o_event forView: self];

    [super mouseDown: o_event];
}

- (void)mouseUp:(NSEvent *)event
{
    if (event.type == NSLeftMouseUp) {
        dispatch_sync(_eventQueue, ^{
            if (_wnd)
                vlc_window_ReportMouseReleased(_wnd, MOUSE_BUTTON_LEFT);
        });
    }

    [super mouseUp:event];
}


- (void)rightMouseDown:(NSEvent *)o_event
{
    if ([o_event type] == NSRightMouseDown)
        [NSMenu popUpContextMenu: VLCMain.sharedInstance.mainMenu.voutMenu withEvent: o_event forView: self];

    [super mouseDown: o_event];
}

- (void)rightMouseUp:(NSEvent *)o_event
{
    if ([o_event type] == NSRightMouseUp)
        [NSMenu popUpContextMenu: VLCMain.sharedInstance.mainMenu.voutMenu withEvent: o_event forView: self];

    [super mouseUp: o_event];
}

- (void)mouseMoved:(NSEvent *)event
{
    NSPoint pointInView = 
        [self convertPoint:event.locationInWindow fromView:nil];
    if ([self mouse:pointInView inRect:self.bounds]) {
        [NSNotificationCenter.defaultCenter postNotificationName:VLCVideoWindowShouldShowFullscreenController
                      object:self];
        // Invert Y coordinates
        CGPoint pointInWindow = 
            CGPointMake(pointInView.x, self.bounds.size.height - pointInView.y);
        NSPoint pointInBacking = [self convertPointToBacking:pointInWindow];
        dispatch_sync(_eventQueue, ^{
            if (_wnd == NULL)
                return;
            vlc_window_ReportMouseMoved(_wnd, pointInBacking.x, pointInBacking.y);
        });
    }
    [super mouseMoved:event];
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
            var_SetInteger(vlc_object_instance(p_intf), "key-pressed", key);
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
            var_SetInteger(vlc_object_instance(p_intf), "key-pressed", key);
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

- (void)setVoutWindow:(vlc_window_t *)p_wnd {
    dispatch_sync(_eventQueue, ^{
        _wnd = p_wnd;
    });
}

- (vlc_window_t *)voutWindow {
    __block vlc_window_t *p_wnd = NULL;
    dispatch_sync(_eventQueue, ^{
        p_wnd = _wnd;
    });
    return p_wnd;
}

- (void)setVoutThread:(vout_thread_t *)p_vout_thread
{
    assert(p_vout == NULL);
    p_vout = p_vout_thread;
    vout_Hold(p_vout);
}

- (vout_thread_t *)voutThread
{
    if (p_vout) {
        vout_Hold(p_vout);
        return p_vout;
    }

    return NULL;
}

- (void)releaseVoutThread
{
    if (p_vout) {
        vout_Release(p_vout);
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
        [_playerController toggleFullscreen];
    }
}

- (void)beginGestureWithEvent:(NSEvent *)event
{
    f_cumulated_magnification = 0.0;
}

@end
