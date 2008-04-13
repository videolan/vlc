/*****************************************************************************
 * vout.m: MacOS X video output module
 *****************************************************************************
 * Copyright (C) 2001-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>

/* BeginFullScreen, EndFullScreen */
#include <QuickTime/QuickTime.h>

#include <vlc_keys.h>

#include "intf.h"
#include "fspanel.h"
#include "vout.h"
#import "controls.h"
#import "embeddedwindow.h"

/*****************************************************************************
 * DeviceCallback: Callback triggered when the video-device variable is changed
 *****************************************************************************/
int DeviceCallback( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    vlc_value_t val;
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    msg_Dbg( p_vout, "set %d", new_val.i_int );
    var_Create( p_vout->p_libvlc, "video-device", VLC_VAR_INTEGER );
    var_Set( p_vout->p_libvlc, "video-device", new_val );

    val.b_bool = true;
    var_Set( p_vout, "intf-change", val );
    return VLC_SUCCESS;
}


/*****************************************************************************
 * VLCEmbeddedList implementation
 *****************************************************************************/
@implementation VLCEmbeddedList

- (id)init
{
    [super init];
    o_embedded_array = [NSMutableArray array];
    return self;
}

- (id)getEmbeddedVout
{
    unsigned int i;

    for( i = 0; i < [o_embedded_array count]; i++ )
    {
        id o_vout_view = [o_embedded_array objectAtIndex: i];
        if( ![o_vout_view isUsed] )
        {
            [o_vout_view setUsed: YES];
            return o_vout_view;
        }
    }
    return nil;
}

- (void)releaseEmbeddedVout: (id)o_vout_view
{
    if( [o_embedded_array containsObject: o_vout_view] )
    {
        [o_vout_view setUsed: NO];
    }
    else
    {
        msg_Warn( VLCIntf, "cannot find Video Output");
    }
}

- (void)addEmbeddedVout: (id)o_vout_view
{
    if( ![o_embedded_array containsObject: o_vout_view] )
    {
        [o_embedded_array addObject: o_vout_view];
    }
}

- (BOOL)windowContainsEmbedded: (id)o_window
{
/*    if( ![[o_window className] isEqualToString: @"VLCVoutWindow"] )
    {
        NSLog( @"We were not given a VLCVoutWindow" );
    }*/
    return ([self getViewForWindow: o_window] == nil ? NO : YES );
}

- (id)getViewForWindow: (id)o_window
{
    id o_enumerator = [o_embedded_array objectEnumerator];
    id o_current_embedded;

    while( (o_current_embedded = [o_enumerator nextObject]) )
    {
        if( [o_current_embedded getWindow] == o_window )
        {
            return o_current_embedded;
        }
    }
    return nil;
}

@end

/*****************************************************************************
 * VLCVoutView implementation
 *****************************************************************************/
@implementation VLCVoutView

- (id)initWithFrame: (NSRect)frameRect
{
    [super initWithFrame: frameRect];
    p_vout = NULL;
    o_view = nil;
    s_frame = &frameRect;

    p_real_vout = NULL;
    o_window = nil;
    return self;
}

- (BOOL)setVout: (vout_thread_t *) vout
        subView: (NSView *) view
          frame: (NSRect *) frame
{
    int i_device;
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    NSArray *o_screens = [NSScreen screens];

    p_vout  = vout;
    o_view  = view;
    s_frame = frame;

    if( [o_screens count] <= 0 )
    {
        msg_Err( p_vout, "no OSX screens available" );
        return NO;
    }

    p_real_vout = [VLCVoutView getRealVout: p_vout];

    /* Get the pref value when this is the first time, otherwise retrieve the device from the top level video-device var */
    if( var_Type( p_real_vout->p_libvlc, "video-device" ) == 0 )
    {
        i_device = var_GetInteger( p_vout, "macosx-vdev" );
    }
    else
    {
        i_device = var_GetInteger( p_real_vout->p_libvlc, "video-device" );
    }

    /* Setup the menuitem for the multiple displays. */
    if( var_Type( p_real_vout, "video-device" ) == 0 )
    {
        int i = 1;
        vlc_value_t val2, text;
        NSScreen * o_screen;

        var_Create( p_real_vout, "video-device", VLC_VAR_INTEGER |
                                            VLC_VAR_HASCHOICE );
        text.psz_string = _("Video Device");
        var_Change( p_real_vout, "video-device", VLC_VAR_SETTEXT, &text, NULL );

        NSEnumerator * o_enumerator = [o_screens objectEnumerator];

        val2.i_int = 0;
        text.psz_string = _("Default");
        var_Change( p_real_vout, "video-device",
                        VLC_VAR_ADDCHOICE, &val2, &text );
        var_Set( p_real_vout, "video-device", val2 );

        while( (o_screen = [o_enumerator nextObject]) != NULL )
        {
            char psz_temp[255];
            NSRect s_rect = [o_screen frame];

            snprintf( psz_temp, sizeof(psz_temp)/sizeof(psz_temp[0])-1,
                      "%s %d (%dx%d)", _("Screen"), i,
                      (int)s_rect.size.width, (int)s_rect.size.height );

            text.psz_string = psz_temp;
            val2.i_int = (int)[o_screen displayID];
            var_Change( p_real_vout, "video-device",
                        VLC_VAR_ADDCHOICE, &val2, &text );
            if( (int)[o_screen displayID] == i_device )
            {
                var_Set( p_real_vout, "video-device", val2 );
            }
            i++;
        }

        var_AddCallback( p_real_vout, "video-device", DeviceCallback,
                         NULL );

        val2.b_bool = true;
        var_Set( p_real_vout, "intf-change", val2 );
    }

    /* Add the view. It's automatically resized to fit the window */
    [self addSubview: o_view];
    [self setAutoresizesSubviews: YES];
    [o_pool release];

    return YES;
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldBoundsSize
{
    [super resizeSubviewsWithOldSize: oldBoundsSize];
    [o_view setFrameSize: [self frame].size];
}

- (void)drawRect:(NSRect)rect
{
    /* When there is no subview we draw a black background */
    [self lockFocus];
    [[NSColor blackColor] set];
    NSRectFill(rect);
    [self unlockFocus];
}

- (void)closeVout
{
    [[[[VLCMain sharedInstance] getControls] getFSPanel] fadeOut];

    /* Make sure we don't see a white flash */
    [[self window] disableScreenUpdatesUntilFlush];
    [o_view removeFromSuperview];
    o_view = nil;
    p_vout = NULL;
    s_frame = nil;
    o_window = nil;
    p_real_vout = NULL;
}

- (void)updateTitle
{
    NSString * o_title = nil; 
    NSMutableString * o_mrl = nil;
    input_thread_t * p_input;

    if( p_vout == NULL )
    {
        return;
    }

    p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT, FIND_PARENT );

    if( p_input == NULL )
    {
        return;
    }

    char *psz_nowPlaying = input_item_GetNowPlaying ( input_GetItem( p_input ) );
    char *psz_name = input_item_GetName( input_GetItem( p_input ) );
    char *psz_uri = input_item_GetURI( input_GetItem( p_input ) );
    if( psz_nowPlaying != NULL )
        o_title = [NSString stringWithUTF8String: psz_nowPlaying];
    else if( psz_name != NULL )
        o_title = [NSString stringWithUTF8String: psz_name];

    if( psz_uri != NULL )
        o_mrl = [NSMutableString stringWithUTF8String: psz_uri];

    FREENULL( psz_nowPlaying );
    FREENULL( psz_name );
    FREENULL( psz_uri );

    if( o_title == nil )
        o_title = o_mrl;

    if( o_mrl != nil )
    {
        /* FIXME once psz_access is exported, we could check if we are
         * reading from a file in a smarter way. */

        NSRange prefix_range = [o_mrl rangeOfString: @"file:"];
        if( prefix_range.location != NSNotFound )
            [o_mrl deleteCharactersInRange: prefix_range];

        if( [o_mrl characterAtIndex:0] == '/' )
        {
            /* it's a local file */
            [o_window setRepresentedFilename: o_mrl];
        }
        else
        {
            /* it's from the network or somewhere else,
             * we clear the previous path */
            [o_window setRepresentedFilename: @""];
        }
        [o_window setTitle: o_title];
    }
    else
    {
        [o_window setTitle: [NSString stringWithUTF8String: VOUT_TITLE]];
    }
    vlc_object_release( p_input );
}


- (void)setOnTop:(BOOL)b_on_top
{
    if( b_on_top )
    {
        [o_window setLevel: NSStatusWindowLevel];
    }
    else
    {
        [o_window setLevel: NSNormalWindowLevel];
    }
}

- (void)scaleWindowWithFactor: (float)factor animate: (BOOL)animate
{
    NSSize newsize;
    int i_corrected_height, i_corrected_width;
    NSPoint topleftbase;
    NSPoint topleftscreen;

    if ( !p_vout->b_fullscreen )
    {
        NSView *mainView;
        NSRect new_frame;
        topleftbase.x = 0;
        topleftbase.y = [o_window frame].size.height;
        topleftscreen = [o_window convertBaseToScreen: topleftbase];

        if( p_vout->render.i_height * p_vout->render.i_aspect >
                        p_vout->render.i_width * VOUT_ASPECT_FACTOR )
        {
            i_corrected_width = p_vout->render.i_height * p_vout->render.i_aspect /
                                            VOUT_ASPECT_FACTOR;
            newsize.width = (int) ( i_corrected_width * factor );
            newsize.height = (int) ( p_vout->render.i_height * factor );
        }
        else
        {
            i_corrected_height = p_vout->render.i_width * VOUT_ASPECT_FACTOR /
                                            p_vout->render.i_aspect;
            newsize.width = (int) ( p_vout->render.i_width * factor );
            newsize.height = (int) ( i_corrected_height * factor );
        }

        /* In fullscreen mode we need to use a view that is different from
         * ourselves, with the VLCEmbeddedWindow */
        if([o_window isKindOfClass:[VLCEmbeddedWindow class]])
            mainView = [o_window mainView];
        else
            mainView = self;

        /* Calculate the window's new size */
        new_frame.size.width = [o_window frame].size.width -
                                    [mainView frame].size.width + newsize.width;
        new_frame.size.height = [o_window frame].size.height -
                                    [mainView frame].size.height + newsize.height;

        new_frame.origin.x = topleftscreen.x;
        new_frame.origin.y = topleftscreen.y - new_frame.size.height;

        [o_window setFrame: new_frame display: animate animate: animate];

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }
}

- (void)toggleFloatOnTop
{
    vlc_value_t val;

    if( !p_real_vout ) return;
    if( var_Get( p_real_vout, "video-on-top", &val )>=0 && val.b_bool)
    {
        val.b_bool = false;
    }
    else
    {
        val.b_bool = true;
    }
    var_Set( p_real_vout, "video-on-top", val );
}

- (void)toggleFullscreen
{
    vlc_value_t val;
    if( !p_real_vout ) return;
    var_Get( p_real_vout, "fullscreen", &val );
    val.b_bool = !val.b_bool;
    var_Set( p_real_vout, "fullscreen", val );
}

- (BOOL)isFullscreen
{
    vlc_value_t val;
    if( !p_real_vout ) return NO;
    var_Get( p_real_vout, "fullscreen", &val );
    return( val.b_bool );
}

- (void)snapshot
{
    vout_Control( p_real_vout, VOUT_SNAPSHOT );
}

- (void)manage
{
    /* Disable Screensaver, when we're playing something, but allow it on pause */
    if( !VLCIntf || !VLCIntf->p_sys || !VLCIntf->p_sys->i_play_status )
        return;

    if( VLCIntf->p_sys->i_play_status == PLAYING_S )
        UpdateSystemActivity( UsrActivity );
}

- (id)getWindow
{
    return o_window;
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    VLCControls * o_controls = (VLCControls *)[[NSApp delegate] getControls];
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

    key = [[o_event charactersIgnoringModifiers] characterAtIndex: 0];

    if( key )
    {
        /* Escape should always get you out of fullscreen */
        if( key == (unichar) 0x1b )
        {
             if( p_real_vout && [self isFullscreen] )
             {
                 [self toggleFullscreen];
             }
        }
        else if ( key == ' ' )
        {
            vlc_value_t val;
            val.i_int = config_GetInt( p_vout, "key-play-pause" );
            var_Set( p_vout->p_libvlc, "key-pressed", val );
        }
        else
        {
            val.i_int |= CocoaKeyToVLC( key );
            var_Set( p_vout->p_libvlc, "key-pressed", val );
        }
    }
    else
    {
        [super keyDown: o_event];
    }
}

- (void)mouseDown:(NSEvent *)o_event
{
    vlc_value_t val;

    if( p_vout )
    {
        if( ( [o_event type] == NSLeftMouseDown ) &&
          ( ! ( [o_event modifierFlags] &  NSControlKeyMask ) ) )
        {
            if( [o_event clickCount] <= 1 )
            {
                /* single clicking */
                var_Get( p_vout, "mouse-button-down", &val );
                val.i_int |= 1;
                var_Set( p_vout, "mouse-button-down", val );
            }
            else
            {
                /* multiple clicking */
                [self toggleFullscreen];
            }
        }
        else if( ( [o_event type] == NSRightMouseDown ) ||
               ( ( [o_event type] == NSLeftMouseDown ) &&
                 ( [o_event modifierFlags] &  NSControlKeyMask ) ) )
        {
            msg_Dbg( p_vout, "received NSRightMouseDown (generic method) or Ctrl clic" );
            [NSMenu popUpContextMenu: [[VLCMain sharedInstance] getVoutMenu] withEvent: o_event forView: [[[VLCMain sharedInstance] getControls] getVoutView]];
        }
    }

    [super mouseDown: o_event];
}

- (void)otherMouseDown:(NSEvent *)o_event
{
    vlc_value_t val;

    if( p_vout && [o_event type] == NSOtherMouseDown )
    {
        var_Get( p_vout, "mouse-button-down", &val );
        val.i_int |= 2;
        var_Set( p_vout, "mouse-button-down", val );
    }

    [super mouseDown: o_event];
}

- (void)rightMouseDown:(NSEvent *)o_event
{
    if( p_vout && [o_event type] == NSRightMouseDown )
    {
        msg_Dbg( p_vout, "received NSRightMouseDown (specific method)" );
        [NSMenu popUpContextMenu: [[VLCMain sharedInstance] getVoutMenu] withEvent: o_event forView: [[[VLCMain sharedInstance] getControls] getVoutView]];
    }

    [super mouseDown: o_event];
}

- (void)mouseUp:(NSEvent *)o_event
{
    vlc_value_t val;

    if( p_vout && [o_event type] == NSLeftMouseUp )
    {
        vlc_value_t b_val;
        b_val.b_bool = true;
        var_Set( p_vout, "mouse-clicked", b_val );

        var_Get( p_vout, "mouse-button-down", &val );
        val.i_int &= ~1;
        var_Set( p_vout, "mouse-button-down", val );
    }

    [super mouseUp: o_event];
}

- (void)otherMouseUp:(NSEvent *)o_event
{
    vlc_value_t val;

    if( p_vout && [o_event type] == NSOtherMouseUp )
    {
        var_Get( p_vout, "mouse-button-down", &val );
        val.i_int &= ~2;
        var_Set( p_vout, "mouse-button-down", val );
    }

    [super mouseUp: o_event];
}

- (void)rightMouseUp:(NSEvent *)o_event
{
    if( p_vout && [o_event type] == NSRightMouseUp )
    {
        /* FIXME: this isn't the appropriate place, but we can't receive
         * NSRightMouseDown some how */
        msg_Dbg( p_vout, "received NSRightMouseUp" );
        [NSMenu popUpContextMenu: [[VLCMain sharedInstance] getVoutMenu] withEvent: o_event forView: [[[VLCMain sharedInstance] getControls] getVoutView]];
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
    NSPoint ml;
    NSRect s_rect;
    BOOL b_inside;

    if( p_vout )
    {
        s_rect = [o_view bounds];
        ml = [o_view convertPoint: [o_event locationInWindow] fromView: nil];
        b_inside = [o_view mouse: ml inRect: s_rect];

        if( b_inside )
        {
            vlc_value_t val;
            unsigned int i_width, i_height, i_x, i_y;

            vout_PlacePicture( p_vout, (unsigned int)s_rect.size.width,
                                       (unsigned int)s_rect.size.height,
                                       &i_x, &i_y, &i_width, &i_height );

            val.i_int = ( ((int)ml.x) - i_x ) *
                        p_vout->render.i_width / i_width;
            var_Set( p_vout, "mouse-x", val );

            if( [[o_view className] isEqualToString: @"VLCGLView"] )
            {
                val.i_int = ( ((int)(s_rect.size.height - ml.y)) - i_y ) *
                            p_vout->render.i_height / i_height;
            }
            else
            {
                val.i_int = ( ((int)ml.y) - i_y ) *
                            p_vout->render.i_height / i_height;
            }
            var_Set( p_vout, "mouse-y", val );

            val.b_bool = true;
            var_Set( p_vout, "mouse-moved", val );
        }
        if( [self isFullscreen] )
            [[[[VLCMain sharedInstance] getControls] getFSPanel] fadeIn];
    }

    [super mouseMoved: o_event];
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

/* Class methods used by the different vout modules */

+ (vout_thread_t *)getRealVout: (vout_thread_t *)p_vout
{
    /* p_real_vout: the vout we have to use to check for video-on-top
       and a few other things. If we are the QuickTime output, it's us.
       It we are the OpenGL provider, it is our parent. */
    if( p_vout->i_object_type == VLC_OBJECT_OPENGL )
    {
        return (vout_thread_t *) p_vout->p_parent;
    }
    else
    {
        return p_vout;
    }

}

+ (id)getVoutView: (vout_thread_t *)p_vout subView: (NSView *)view
                                    frame: (NSRect *)s_frame
{
    vlc_value_t value_drawable;
    int i_timeout;
    id o_return = nil;

    var_Get( p_vout->p_libvlc, "drawable", &value_drawable );

    var_Create( p_vout, "macosx-vdev", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-stretch", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-opaqueness", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-background", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-black", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "embedded-video", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* We only wait for NSApp to initialise if we're not embedded (as in the
     * case of the Mozilla plugin).  We can tell whether we're embedded or not
     * by examining the "drawable" value: if it's zero, we're running in the
     * main Mac intf; if it's non-zero, we're embedded. */
    if( value_drawable.i_int == 0 )
    {
        /* Wait for a MacOS X interface to appear. Timeout is 2 seconds. */
        for( i_timeout = 20 ; i_timeout-- ; )
        {
            if( NSApp == NULL )
            {
                msleep( INTF_IDLE_SLEEP );
            }
        }

        if( NSApp == NULL )
        {
            /* No MacOS X intf, unable to communicate with MT */
            msg_Err( p_vout, "no MacOS X interface present" );
            return nil;
        }
        else
        {
            if ( VLCIntf && !(p_vout->b_fullscreen) &&
                        !(var_GetBool( p_vout, "macosx-background" )) &&
                        var_GetBool( p_vout, "embedded-video") )
            {
                o_return = [[[VLCMain sharedInstance] getEmbeddedList]
                                                            getEmbeddedVout];
            }
        }
    }

    /* No embedded vout is available */
    if( o_return == nil )
    {
        NSRect null_rect;
        bzero( &null_rect, sizeof( NSRect ) );
        o_return = [[VLCDetachedVoutView alloc] initWithFrame: null_rect ];
    }
    [o_return setVout: p_vout subView: view frame: s_frame];
    return o_return;
}

- (void)enterFullscreen
{
    [[o_view class] performSelectorOnMainThread:@selector(resetVout:) withObject:[NSValue valueWithPointer:p_vout] waitUntilDone:YES];
    [[[[VLCMain sharedInstance] getControls] getFSPanel] setActive: nil];
}

- (void)leaveFullscreen
{
    [[o_view class] performSelectorOnMainThread:@selector(resetVout:) withObject:[NSValue valueWithPointer:p_vout] waitUntilDone:YES];
    [[[[VLCMain sharedInstance] getControls] getFSPanel] setNonActive: nil];
}

@end

/*****************************************************************************
 * VLCDetachedVoutView implementation
 *****************************************************************************/
@implementation VLCDetachedVoutView

- (id)initWithFrame: (NSRect)frameRect
{
    [super initWithFrame: frameRect];
    i_time_mouse_last_moved = 0;
    return self;
}

- (bool)setVout: (vout_thread_t *) p_arg_vout subView: (NSView *) view
                     frame: (NSRect *) s_arg_frame
{
    BOOL b_return = [super setVout: p_arg_vout subView: view frame:s_arg_frame];
    i_time_mouse_last_moved = mdate();
    o_window = [[VLCVoutWindow alloc] initWithVout: p_arg_vout view: self
                                                    frame: s_arg_frame];
    [self updateTitle];
    [view setFrame: [self frame]];

    if( var_GetBool( p_real_vout, "video-on-top" ) )
    {
        [o_window setLevel: NSStatusWindowLevel];
    }


    [o_window setAcceptsMouseMovedEvents: TRUE];
    return b_return;
}

- (void)closeVout
{
    [o_window closeWindow];
    [o_window setAcceptsMouseMovedEvents: NO];
    i_time_mouse_last_moved = 0;
    [super closeVout];
}

- (void)mouseMoved:(NSEvent *)o_event
{
    i_time_mouse_last_moved = mdate();
    [super mouseMoved: o_event];
}

- (void)hideMouse:(BOOL)b_hide
{
    BOOL b_inside;
    NSPoint ml;
    NSView *o_contents = [o_window contentView];

    ml = [o_window convertScreenToBase:[NSEvent mouseLocation]];
    ml = [o_contents convertPoint:ml fromView:nil];
    b_inside = [o_contents mouse: ml inRect: [o_contents bounds]];

    if( b_hide && b_inside )
    {
        [NSCursor setHiddenUntilMouseMoves: YES];
    }
    else if( !b_hide )
    {
        [NSCursor setHiddenUntilMouseMoves: NO];
    }
}

- (void)manage
{
    [super manage];
    unsigned int i_mouse_hide_timeout =
        var_GetInteger(p_vout, "mouse-hide-timeout") * 1000;
    if( p_vout->b_fullscreen )
    {
        if( mdate() - i_time_mouse_last_moved > i_mouse_hide_timeout )
        {
            [self hideMouse: YES];
        }
    }
    else
    {
        [self hideMouse: NO];
    }
}

@end

/*****************************************************************************
 * VLCEmbeddedVoutView implementation
 *****************************************************************************/

@implementation VLCEmbeddedVoutView

- (id)initWithFrame: (NSRect)frameRect
{
    [super initWithFrame: frameRect];
    b_used = NO;
    [[[VLCMain sharedInstance] getEmbeddedList] addEmbeddedVout: self];
    return self;
}

- (BOOL)setVout: (vout_thread_t *) p_arg_vout subView: (NSView *) view
                 frame: (NSRect *)s_arg_frame showWindow: (BOOL)b_show_window
{
    BOOL b_return;
    b_return = [super setVout: p_arg_vout subView: view frame: s_arg_frame];
    if( b_return )
    {
        o_window = [self window];
        if (b_show_window)
            [o_window makeKeyAndOrderFront: self];
        [o_window setAcceptsMouseMovedEvents: TRUE];

        if( var_GetBool( p_real_vout, "video-on-top" ) )
        {
            [o_window setLevel: NSStatusWindowLevel];
        }

        [view setFrameSize: [self frame].size];
    }
    return b_return;
}

- (BOOL)setVout: (vout_thread_t *) p_arg_vout subView: (NSView *) view
                     frame: (NSRect *) s_arg_frame

{
    return [self setVout: p_arg_vout subView: view frame:s_arg_frame showWindow: YES];
}

- (void)setUsed: (BOOL)b_new_used
{
    b_used = b_new_used;
}

- (BOOL)isUsed
{
    return b_used;
}

- (void)closeVout
{
    [super closeVout];
    [o_window setAcceptsMouseMovedEvents: NO];
    [[[VLCMain sharedInstance] getEmbeddedList] releaseEmbeddedVout: self];
}


@end

@implementation VLCDetachedEmbeddedVoutView
- (void)awakeFromNib
{
    o_embeddedwindow = [self window];
}

- (BOOL)setVout: (vout_thread_t *) p_arg_vout subView: (NSView *) view
                     frame: (NSRect *) s_arg_frame
{
    BOOL b_return = [super setVout: p_arg_vout subView: view frame: s_arg_frame showWindow: NO];

    /* o_window needs to point to our o_embeddedwindow, super might have set it
     * to the fullscreen window that o_embeddedwindow setups during fullscreen */
    o_window = o_embeddedwindow;
 
    if( b_return )
    {
        [o_window lockFullscreenAnimation];

        [o_window setAlphaValue: var_GetFloat( p_vout, "macosx-opaqueness" )];

        [self updateTitle];

        /* Make the window the front and key window before animating */
        if ([o_window isVisible] && (![o_window isFullscreen]))
            [o_window makeKeyAndOrderFront: self];

        [self scaleWindowWithFactor: 1.0 animate: [o_window isVisible] && (![o_window isFullscreen])];

        [o_window setAspectRatio:NSMakeSize([o_window frame].size.width, [o_window frame].size.height)];

        /* Make sure our window is visible, if we are not in fullscreen */
        if (![o_window isFullscreen])
            [o_window makeKeyAndOrderFront: self];
        [o_window unlockFullscreenAnimation];

    }
    return b_return;
}

- (void)closeVout
{
    playlist_t * p_playlist = pl_Yield( VLCIntf );

    if(!playlist_IsPlaying( p_playlist ))
        [o_window performSelectorOnMainThread: @selector(orderOut:) withObject: self waitUntilDone: YES];
 
    vlc_object_release( p_playlist );

    [super closeVout];
}

- (void)enterFullscreen
{
    /* We are in a VLCEmbeddedWindow */
    [o_embeddedwindow performSelectorOnMainThread: @selector(enterFullscreen) withObject: NULL waitUntilDone: YES];
}

- (void)leaveFullscreen
{
    /* We are in a VLCEmbeddedWindow */
    [o_embeddedwindow performSelectorOnMainThread: @selector(leaveFullscreen) withObject: NULL waitUntilDone: YES];
}
@end

/*****************************************************************************
 * VLCVoutWindow implementation
 *****************************************************************************/
@implementation VLCVoutWindow

- (id) initWithVout: (vout_thread_t *) vout view: (VLCVoutView *) view
                     frame: (NSRect *) frame
{
    p_vout  = vout;
    o_view  = view;
    s_frame = frame;

    [self performSelectorOnMainThread: @selector(initReal:)
        withObject: NULL waitUntilDone: YES];

    if( !b_init_ok )
    {
        return NULL;
    }

    return self;
}

- (id)initReal: (id) sender
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    NSArray *o_screens = [NSScreen screens];
    NSScreen *o_screen;
    bool b_menubar_screen = false;
    int i_device;

    b_init_ok = false;

    p_real_vout = [VLCVoutView getRealVout: p_vout];
    i_device = var_GetInteger( p_real_vout->p_libvlc, "video-device" );
    b_black = NO;
    b_embedded = var_GetBool( p_vout, "embedded-video" );

    /* Find out on which screen to open the window */
    o_screen = [NSScreen screenWithDisplayID: (CGDirectDisplayID)i_device];

    if( !o_screen )
        o_screen = [NSScreen mainScreen];

    if( [o_screen isMainScreen] )
        b_menubar_screen = true;

    if( p_vout->b_fullscreen )
    {
        CGDisplayFadeReservationToken token;
        NSRect screen_rect = [o_screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0;

        b_black = var_GetBool( p_vout, "macosx-black" );

        /* move the FSPanel to front in case that it is currently shown
         * this won't and is not supposed to work when it's fading right now */
        if( [[[[VLCMain sharedInstance] getControls] getFSPanel] isDisplayed] )
            [[[[VLCMain sharedInstance] getControls] getFSPanel] setActive: nil];
 
        /* tell the fspanel to move itself to front next time it's triggered */
        [[[[VLCMain sharedInstance] getControls] getFSPanel] setVoutWasUpdated: i_device];

        /* Creates a window with size: screen_rect on o_screen */
        [self initWithContentRect: screen_rect
              styleMask: NSBorderlessWindowMask
              backing: NSBackingStoreBuffered
              defer: YES screen: o_screen];
 
        if( b_menubar_screen )
        {
            SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
        }
        if( b_black == true )
        {
            CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
            CGDisplayFade( token, 0.6 , kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );

            [o_screen blackoutOtherScreens];

            CGDisplayFade( token, 0.3 , kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO );
            CGReleaseDisplayFadeReservation( token);
        }
    }
    else if( var_GetBool( p_vout, "macosx-background" ) )
    {
        NSRect screen_rect = [o_screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0;

        /* Creates a window with size: screen_rect on o_screen */
        [self initWithContentRect: screen_rect
              styleMask: NSBorderlessWindowMask
              backing: NSBackingStoreBuffered
              defer: YES screen: o_screen];

        [self setLevel: CGWindowLevelForKey(kCGDesktopWindowLevelKey)];
    }
    else
    {
        unsigned int i_stylemask = NSTitledWindowMask |
                                   NSMiniaturizableWindowMask |
                                   NSClosableWindowMask |
                                   NSResizableWindowMask;

        NSRect s_rect;
        if( !s_frame )
        {
            s_rect.size.width  = p_vout->i_window_width;
            s_rect.size.height = p_vout->i_window_height;
        }
        else
        {
            s_rect = *s_frame;
        }

        [self initWithContentRect: s_rect
              styleMask: i_stylemask
              backing: NSBackingStoreBuffered
              defer: YES screen: o_screen];

        [self setAlphaValue: var_GetFloat( p_vout, "macosx-opaqueness" )];

        if( !s_frame )
        {
            [self center];
        }
    }

    [self makeKeyAndOrderFront: nil];
    [self setReleasedWhenClosed: YES];

    /* We'll catch mouse events */
    [self makeFirstResponder: o_view];

    /* Add the view. It's automatically resized to fit the window */
    [self setContentView: o_view];

    [o_pool release];

    b_init_ok = true;
    return self;
}

- (void)close
{
    [o_view closeVout];
}

- (void)closeWindow
{
    /* XXX waitUntilDone = NO to avoid a possible deadlock when hitting
       Command-Q */
    [self performSelectorOnMainThread: @selector(closeReal:)
        withObject: NULL waitUntilDone: NO];
}

- (id)closeReal:(id)sender
{
    if( b_black == true )
    {
        CGDisplayFadeReservationToken token;
        CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
        CGDisplayFade( token, 0.3 , kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );
 
        [self disableScreenUpdatesUntilFlush];
        [self orderOut: self];

        CGDisplayFade( token, 0.6 , kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, YES );
        CGReleaseDisplayFadeReservation( token);
        CGDisplayRestoreColorSyncSettings();
    }
    [NSScreen unblackoutScreens];

    SetSystemUIMode( kUIModeNormal, 0);
    [super close];
    /* this does only work in embedded mode */
    if( b_embedded == true )
        [[[[VLCMain sharedInstance] getControls] getFSPanel] orderOut: self];

    return NULL;
}

- (id)getVoutView
{
    return o_view;
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

/* Sometimes crashes VLC....
- (BOOL)performKeyEquivalent:(NSEvent *)o_event
{
        return [[VLCMain sharedInstance] hasDefinedShortcutKey:o_event];
}*/

/* This is actually the same as VLCControls::stop. */

- (BOOL)windowShouldClose:(id)sender
{
    playlist_t * p_playlist = vlc_object_find( p_vout, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return NO;
    }

    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );

    /* The window will be closed by the intf later. */
    return NO;
}


@end
