/*****************************************************************************
 * VideoView.m: MacOS X video output module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
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
 * DeviceCallback: Callback triggered when the video-device variable is changed
 *****************************************************************************/
int DeviceCallback( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    vlc_value_t val;
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    msg_Dbg( p_vout, "set %"PRId64, new_val.i_int );
    var_Create( p_vout->p_libvlc, "video-device", VLC_VAR_INTEGER );
    var_Set( p_vout->p_libvlc, "video-device", new_val );

    val.b_bool = true;
    var_Set( p_vout, "intf-change", val );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLCVoutView implementation
 *****************************************************************************/
@implementation VLCVoutView

#pragma mark -
#pragma mark drag & drop support

- (void)dealloc
{
    [self unregisterDraggedTypes];
    [super dealloc];
}

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObject: NSFilenamesPboardType]];

    f_cumulated_magnification = 0.0;
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

- (void)closeVout
{
    vout_thread_t * p_vout = getVout();
    if( p_vout )
    {
        var_DelCallback( p_vout, "video-device", DeviceCallback, NULL );
        vlc_object_release( p_vout );
    }
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    VLCControls * o_controls = (VLCControls *)[[NSApp delegate] controls];
    [o_controls scrollWheel: theEvent];
}

- (void)keyDown:(NSEvent *)o_event
{
    unichar key = 0;
    vlc_value_t val;
    unsigned int i_pressed_modifiers = 0;
    val.i_int = 0;

    i_pressed_modifiers = [o_event modifierFlags];

    if( i_pressed_modifiers & NSShiftKeyMask )
        val.i_int |= KEY_MODIFIER_SHIFT;
    if( i_pressed_modifiers & NSControlKeyMask )
        val.i_int |= KEY_MODIFIER_CTRL;
    if( i_pressed_modifiers & NSAlternateKeyMask )
        val.i_int |= KEY_MODIFIER_ALT;
    if( i_pressed_modifiers & NSCommandKeyMask )
        val.i_int |= KEY_MODIFIER_COMMAND;

    key = [[[o_event charactersIgnoringModifiers] lowercaseString] characterAtIndex: 0];

    if( key )
    {
        vout_thread_t * p_vout = getVout();
        /* Escape should always get you out of fullscreen */
        if( key == (unichar) 0x1b )
        {
            playlist_t * p_playlist = pl_Get( VLCIntf );
             if( var_GetBool( p_playlist, "fullscreen") )
                 [[VLCCoreInteraction sharedInstance] toggleFullscreen];
        }
        /* handle Lion's default key combo for fullscreen-toggle in addition to our own hotkeys */
        else if( key == 'f' && i_pressed_modifiers & NSControlKeyMask && i_pressed_modifiers & NSCommandKeyMask )
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
        else if ( p_vout )
        {
            if( key == ' ' )
            {
                [[VLCCoreInteraction sharedInstance] play];
            }
            else
            {
                val.i_int |= (int)CocoaKeyToVLC( key );
                var_Set( p_vout->p_libvlc, "key-pressed", val );
            }
        }
        else
            msg_Dbg( VLCIntf, "could not send keyevent to VLC core" );

        if (p_vout)
            vlc_object_release( p_vout );
    }
    else
        [super keyDown: o_event];
}

- (BOOL)performKeyEquivalent:(NSEvent *)o_event
{
    return [[VLCMainWindow sharedInstance] performKeyEquivalent: o_event];
}

- (void)mouseDown:(NSEvent *)o_event
{
    if( ( [o_event type] == NSLeftMouseDown ) &&
       ( ! ( [o_event modifierFlags] &  NSControlKeyMask ) ) )
    {
        if( [o_event clickCount] > 1 )
        {
            /* multiple clicking */
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
        }
    }
    else if( ( [o_event type] == NSRightMouseDown ) ||
            ( ( [o_event type] == NSLeftMouseDown ) &&
             ( [o_event modifierFlags] &  NSControlKeyMask ) ) )
    {
        [NSMenu popUpContextMenu: [[VLCMainMenu sharedInstance] voutMenu] withEvent: o_event forView: self];
    }

    [super mouseDown: o_event];
}

- (void)rightMouseDown:(NSEvent *)o_event
{
    if( [o_event type] == NSRightMouseDown )
        [NSMenu popUpContextMenu: [[VLCMainMenu sharedInstance] voutMenu] withEvent: o_event forView: self];

    [super mouseDown: o_event];
}

- (void)rightMouseUp:(NSEvent *)o_event
{
    if( [o_event type] == NSRightMouseUp )
        [NSMenu popUpContextMenu: [[VLCMainMenu sharedInstance] voutMenu] withEvent: o_event forView: self];

    [super mouseUp: o_event];
}

- (void)mouseMoved:(NSEvent *)o_event
{
    NSPoint ml = [self convertPoint: [o_event locationInWindow] fromView: nil];
    if( [self mouse: ml inRect: [self bounds]] )
        [[VLCMain sharedInstance] showFullscreenController];

    [super mouseMoved: o_event];
}

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
    BOOL b_fullscreen = [[VLCMainWindow sharedInstance] isFullscreen];

    if( ( f_cumulated_magnification > f_threshold && !b_fullscreen ) || ( f_cumulated_magnification < -f_threshold && b_fullscreen ) )
    {
        f_cumulated_magnification = 0.0;
        [[VLCCoreInteraction sharedInstance] toggleFullscreen];
    }
}

- (void)beginGestureWithEvent:(NSEvent *)event
{
    f_cumulated_magnification = 0.0;
}

@end
