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
#import <vlc_vout_window.h>
#import <vlc_vout_display.h>
#import <vlc_keys.h>
#import <vlc_mouse.h>
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
 * VLCOpenGLVideoView interface excerpt
 * full implementation in modules/video_output/macosx.m:95
 *****************************************************************************/
@interface VLCOpenGLVideoView : NSOpenGLView
{
}
- (vout_display_t *)voutDisplay;
@end

/*****************************************************************************
 * VLCVoutView implementation
 *****************************************************************************/
@implementation VLCVoutView
- (void)closeVout
{
    vout_thread_t * p_vout = getVout();
    if( !p_vout )
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
            vlc_object_release( p_vout );
        }
        else
            msg_Dbg( VLCIntf, "could not send keyevent to VLC core" );
    }
    else
        [super keyDown: o_event];
}

- (void)mouseDown:(NSEvent *)o_event
{
    vout_thread_t * p_vout = getVout();
    vlc_value_t val;
    if( p_vout )
    {
        if( ( [o_event type] == NSLeftMouseDown ) &&
          ( ! ( [o_event modifierFlags] &  NSControlKeyMask ) ) )
        {
            if( [o_event clickCount] <= 1 )
            {
                /* single clicking */
                vout_display_SendEventMousePressed( [[[self subviews] objectAtIndex:0] voutDisplay], MOUSE_BUTTON_LEFT );
            }
            else
            {
                /* multiple clicking */
                [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            }
        }
        else if( ( [o_event type] == NSRightMouseDown ) ||
               ( ( [o_event type] == NSLeftMouseDown ) &&
                 ( [o_event modifierFlags] &  NSControlKeyMask ) ) )
        {
            msg_Dbg( p_vout, "received NSRightMouseDown (generic method) or Ctrl clic" );
            [NSMenu popUpContextMenu: [[VLCMainMenu sharedInstance] voutMenu] withEvent: o_event forView: self];
        }
        vlc_object_release( p_vout );
    }

    [super mouseDown: o_event];
}

- (void)otherMouseDown:(NSEvent *)o_event
{
    if( [o_event type] == NSOtherMouseDown )
    {
        vout_thread_t * p_vout = getVout();
        vlc_value_t val;

        if (p_vout)
        {
            vout_display_SendEventMousePressed( [[[self subviews] objectAtIndex:0] voutDisplay], MOUSE_BUTTON_CENTER );
        }
        vlc_object_release( p_vout );
    }

    [super mouseDown: o_event];
}

- (void)rightMouseDown:(NSEvent *)o_event
{
    if( [o_event type] == NSRightMouseDown )
    {
        vout_thread_t * p_vout = getVout();
        if (p_vout)
            [NSMenu popUpContextMenu: [[VLCMainMenu sharedInstance] voutMenu] withEvent: o_event forView: self];
        vlc_object_release( p_vout );
    }

    [super mouseDown: o_event];
}

- (void)mouseUp:(NSEvent *)o_event
{
    if( [o_event type] == NSLeftMouseUp )
    {
        vout_thread_t * p_vout = getVout();
        if (p_vout)
        {
            vout_display_SendEventMouseReleased( [[[self subviews] objectAtIndex:0] voutDisplay], MOUSE_BUTTON_LEFT );
            vlc_object_release( p_vout );
        }
    }

    [super mouseUp: o_event];
}

- (void)otherMouseUp:(NSEvent *)o_event
{
    if( [o_event type] == NSOtherMouseUp )
    {
        vout_thread_t * p_vout = getVout();
        if (p_vout)
        {
            vout_display_SendEventMouseReleased( [[[self subviews] objectAtIndex:0] voutDisplay], MOUSE_BUTTON_CENTER );
            vlc_object_release( p_vout );
        }
    }

    [super mouseUp: o_event];
}

- (void)rightMouseUp:(NSEvent *)o_event
{
    if( [o_event type] == NSRightMouseUp )
    {
        vout_thread_t * p_vout = getVout();
        if (p_vout)
        {
            [NSMenu popUpContextMenu: [[VLCMainMenu sharedInstance] voutMenu] withEvent: o_event forView: self];
            vlc_object_release( p_vout );
        }
    }

    [super mouseUp: o_event];
}

- (void)mouseDragged:(NSEvent *)o_event
{
    [self mouseMoved: o_event];
}

- (void)otherMouseDragged:(NSEvent *)o_event
{
    [self mouseMoved: o_event];
}

- (void)rightMouseDragged:(NSEvent *)o_event
{
    [self mouseMoved: o_event];
}

- (void)mouseMoved:(NSEvent *)o_event
{
    vout_thread_t * p_vout = getVout();
    if (p_vout)
    {
        NSPoint ml;
        NSRect s_rect;
        BOOL b_inside;

        s_rect = [self bounds];
        ml = [self convertPoint: [o_event locationInWindow] fromView: nil];
        b_inside = [self mouse: ml inRect: s_rect];

        if( b_inside )
        {
            vout_display_SendEventMouseMoved( [[[self subviews] objectAtIndex:0] voutDisplay], ((int)ml.x), ((int)s_rect.size.height - ((int)ml.y)) );
        }
        [[VLCMain sharedInstance] showFullscreenController];
        vlc_object_release( p_vout );
    }

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
    /* We need to stay the first responder or we'll miss some
       events */
    return NO;
}
@end
