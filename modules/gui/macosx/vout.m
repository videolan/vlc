/*****************************************************************************
 * vout.m: MacOS X video output module
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
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
#include <string.h>                                            /* strerror() */

/* BeginFullScreen, EndFullScreen */
#include <QuickTime/QuickTime.h>

#include <vlc_keys.h>

#include "intf.h"
#include "vout.h"


/*****************************************************************************
 * DeviceCallback: Callback triggered when the video-device variable is changed
 *****************************************************************************/
int DeviceCallback( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    vlc_value_t val;
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    msg_Dbg( p_vout, "set %d", new_val.i_int );
    var_Create( p_vout->p_vlc, "video-device", VLC_VAR_INTEGER );
    var_Set( p_vout->p_vlc, "video-device", new_val );

    val.b_bool = VLC_TRUE;
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
/*    if( ![[o_window className] isEqualToString: @"VLCWindow"] )
    {
        NSLog( @"We were not given a VLCWindow" );
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

- (id)initWithFrame:(NSRect)frameRect
{
    [super initWithFrame: frameRect];
    p_vout = NULL;
    o_view = nil;
    s_frame = &frameRect;

    p_real_vout = NULL;
    o_window = nil;
    return self;
}

- (BOOL)setVout: (vout_thread_t *) vout subView: (NSView *) view
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
    if( var_Type( p_real_vout->p_vlc, "video-device" ) == 0 )
    {
        i_device = var_GetInteger( p_vout, "macosx-vdev" );
    }
    else
    {
        i_device = var_GetInteger( p_real_vout->p_vlc, "video-device" );
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
            val2.i_int = i;
            var_Change( p_real_vout, "video-device",
                        VLC_VAR_ADDCHOICE, &val2, &text );
            if( i == i_device )
            {
                var_Set( p_real_vout, "video-device", val2 );
            }
            i++;
        }

        var_AddCallback( p_real_vout, "video-device", DeviceCallback,
                         NULL );

        val2.b_bool = VLC_TRUE;
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

- (void)closeVout
{
    [o_view removeFromSuperview];
    o_view = nil;
    p_vout = NULL;
    s_frame = nil;
    o_window = nil;
    p_real_vout = NULL;
}

- (void)updateTitle
{
    NSMutableString * o_title = nil, * o_mrl = nil;
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

    if( p_input->input.p_item->psz_name != NULL )
        o_title = [NSMutableString stringWithUTF8String:
            p_input->input.p_item->psz_name];
    if( p_input->input.p_item->psz_uri != NULL )
        o_mrl = [NSMutableString stringWithUTF8String:
            p_input->input.p_item->psz_uri];
    if( o_title == nil )
        o_title = o_mrl;

    if( o_mrl != nil )
    {
        if( p_input->input.p_access && !strcmp( p_input->input.p_access->p_module->psz_shortname, "File" ) )
        {
            NSRange prefix_range = [o_mrl rangeOfString: @"file:"];
            if( prefix_range.location != NSNotFound )
                [o_mrl deleteCharactersInRange: prefix_range];
            [o_window setRepresentedFilename: o_mrl];
        }
        [o_window setTitle: o_title];
    }
    else
    {
        [o_window setTitle: [NSString stringWithCString: VOUT_TITLE]];
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

- (void)scaleWindowWithFactor: (float)factor
{
    NSSize newsize;
    int i_corrected_height, i_corrected_width;
    NSPoint topleftbase;
    NSPoint topleftscreen;

    if ( !p_vout->b_fullscreen )
    {
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

        /* Calculate the window's new size */
        new_frame.size.width = [o_window frame].size.width -
                                    [self frame].size.width + newsize.width;
        new_frame.size.height = [o_window frame].size.height -
                                    [self frame].size.height + newsize.height;

        new_frame.origin.x = topleftscreen.x;
        new_frame.origin.y = topleftscreen.y - new_frame.size.height;

        [o_window setFrame: new_frame display: YES];

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }
}

- (void)toggleFloatOnTop
{
    vlc_value_t val;

    if( !p_real_vout ) return;
    if( var_Get( p_real_vout, "video-on-top", &val )>=0 && val.b_bool)
    {
        val.b_bool = VLC_FALSE;
    }
    else
    {
        val.b_bool = VLC_TRUE;
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
    var_Get( p_real_vout, "fullscreen", &val );
    return( val.b_bool );
}

- (void)snapshot
{
    vout_Control( p_real_vout, VOUT_SNAPSHOT );
}

- (void)manage
{
    /* Disable Screensaver */
    UpdateSystemActivity( UsrActivity );
}

- (id)getWindow
{
    return o_window;
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
            var_Set( p_vout->p_vlc, "key-pressed", val );
        }
        else
        {
            val.i_int |= CocoaKeyToVLC( key );
            var_Set( p_vout->p_vlc, "key-pressed", val );
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
        switch( [o_event type] )
        {
            case NSLeftMouseDown:
            {
                var_Get( p_vout, "mouse-button-down", &val );
                val.i_int |= 1;
                var_Set( p_vout, "mouse-button-down", val );
            }
            break;

            default:
                [super mouseDown: o_event];
            break;
        }
    }
}

- (void)otherMouseDown:(NSEvent *)o_event
{
    vlc_value_t val;

    if( p_vout )
    {
        switch( [o_event type] )
        {
            case NSOtherMouseDown:
            {
                var_Get( p_vout, "mouse-button-down", &val );
                val.i_int |= 2;
                var_Set( p_vout, "mouse-button-down", val );
            }
            break;

            default:
                [super mouseDown: o_event];
            break;
        }
    }
}

- (void)rightMouseDown:(NSEvent *)o_event
{
    vlc_value_t val;

    if( p_vout )
    {
        switch( [o_event type] )
        {
            case NSRightMouseDown:
            {
                var_Get( p_vout, "mouse-button-down", &val );
                val.i_int |= 4;
                var_Set( p_vout, "mouse-button-down", val );
            }
            break;

            default:
                [super mouseDown: o_event];
            break;
        }
    }
}

- (void)mouseUp:(NSEvent *)o_event
{
    vlc_value_t val;

    if( p_vout )
    {
        switch( [o_event type] )
        {
            case NSLeftMouseUp:
            {
                vlc_value_t b_val;
                b_val.b_bool = VLC_TRUE;
                var_Set( p_vout, "mouse-clicked", b_val );

                var_Get( p_vout, "mouse-button-down", &val );
                val.i_int &= ~1;
                var_Set( p_vout, "mouse-button-down", val );
            }
            break;

            default:
                [super mouseUp: o_event];
            break;
        }
    }
}

- (void)otherMouseUp:(NSEvent *)o_event
{
    vlc_value_t val;

    if( p_vout )
    {
        switch( [o_event type] )
        {
            case NSOtherMouseUp:
            {
                var_Get( p_vout, "mouse-button-down", &val );
                val.i_int &= ~2;
                var_Set( p_vout, "mouse-button-down", val );
            }
            break;

            default:
                [super mouseUp: o_event];
            break;
        }
    }
}

- (void)rightMouseUp:(NSEvent *)o_event
{
    vlc_value_t val;

    if( p_vout )
    {
        switch( [o_event type] )
        {
            case NSRightMouseUp:
            {
                var_Get( p_vout, "mouse-button-down", &val );
                val.i_int &= ~4;
                var_Set( p_vout, "mouse-button-down", val );
            }
            break;

            default:
                [super mouseUp: o_event];
            break;
        }
    }
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

            val.b_bool = VLC_TRUE;
            var_Set( p_vout, "mouse-moved", val );
        }
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
    vout_thread_t * p_real_vout = [VLCVoutView getRealVout: p_vout];

    var_Get( p_vout->p_vlc, "drawable", &value_drawable );

    var_Create( p_vout, "macosx-vdev", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-fill", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-stretch", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-opaqueness", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-background", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-black", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-embedded", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );


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
                        !(var_GetBool( p_real_vout, "macosx-background" )) &&
                        var_GetBool( p_vout, "macosx-embedded") )
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
    o_window = [[VLCWindow alloc] initWithVout: p_arg_vout view: self
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
    if( p_vout->b_fullscreen )
    {
        if( mdate() - i_time_mouse_last_moved > 3000000 )
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
                     frame: (NSRect *) s_arg_frame

{
    BOOL b_return;
    b_return = [super setVout: p_arg_vout subView: view frame: s_arg_frame];
    if( b_return )
    {
        o_window = [self window];
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

- (BOOL)setVout: (vout_thread_t *) p_arg_vout subView: (NSView *) view
                     frame: (NSRect *) s_arg_frame
{
    BOOL b_return = [super setVout: p_arg_vout subView: view frame: s_arg_frame];

    if( b_return )
    {
        [o_window setAlphaValue: var_GetFloat( p_vout, "macosx-opaqueness" )];
        [self updateTitle];
        [self scaleWindowWithFactor: 1.0];
        [o_window makeKeyAndOrderFront: self];
    }
    return b_return;
}

- (void)closeVout
{
    [o_window orderOut: self];
    [super closeVout];
}

@end

/*****************************************************************************
 * VLCWindow implementation
 *****************************************************************************/
@implementation VLCWindow

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
    vlc_bool_t b_menubar_screen = VLC_FALSE;
    int i_device;

    b_init_ok = VLC_FALSE;

    p_fullscreen_state = NULL;
    p_real_vout = [VLCVoutView getRealVout: p_vout];
    i_device = var_GetInteger( p_real_vout->p_vlc, "video-device" );
    b_black = var_GetBool( p_real_vout->p_vlc, "macosx-black" );

    /* Find out on which screen to open the window */
    if( i_device <= 0 || i_device > (int)[o_screens count] )
    {
         /* No preference specified. Use the main screen */
        o_screen = [NSScreen mainScreen];
        i_device = [o_screens indexOfObject: o_screen];
        if( o_screen == [o_screens objectAtIndex: 0] )
            b_menubar_screen = VLC_TRUE;
    }
    else
    {
        i_device--;
        o_screen = [o_screens objectAtIndex: i_device];
        b_menubar_screen = ( i_device == 0 );
    }

    if( p_vout->b_fullscreen )
    {
        CGDisplayFadeReservationToken token;
        NSRect screen_rect = [o_screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0;

        /* Creates a window with size: screen_rect on o_screen */
        [self initWithContentRect: screen_rect
              styleMask: NSBorderlessWindowMask
              backing: NSBackingStoreBuffered
              defer: YES screen: o_screen];

        if( var_GetBool( p_real_vout, "macosx-black" ) )
        if( b_black == VLC_TRUE )
        {
            CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
            CGDisplayFade( token, 0.5, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, true );
            CGReleaseDisplayFadeReservation( token );
            unsigned int i;
            for( i = 0 ; i < [o_screens count]; i++)
            {
                struct
                {
                    CGDirectDisplayID displayID;
                    CGGammaValue redMin, redMax, redGamma,
                                 greenMin, greenMax, greenGamma,
                                 blueMin, blueMax, blueGamma;
                } dispSettings;
                CGDisplayCount dspyCnt;
                CGPoint gPoint;

                if( i == (unsigned int)i_device ) continue;

                screen_rect = [[o_screens objectAtIndex: i] frame];

                gPoint.x = screen_rect.origin.x;
                gPoint.y = screen_rect.origin.y;
                CGGetDisplaysWithPoint( gPoint, 1, &(dispSettings.displayID), &dspyCnt);
                CGGetDisplayTransferByFormula(
                    dispSettings.displayID,
                    &dispSettings.redMin, &dispSettings.redMax, &dispSettings.redGamma,
                    &dispSettings.greenMin, &dispSettings.greenMax, &dispSettings.greenGamma,
                    &dispSettings.blueMin, &dispSettings.blueMax, &dispSettings.blueGamma );
                CGSetDisplayTransferByFormula(
                    dispSettings.displayID,
                    dispSettings.redMin,   0, dispSettings.redGamma,
                    dispSettings.greenMin, 0, dispSettings.greenGamma,
                    dispSettings.blueMin,  0, dispSettings.blueGamma );
            }
        }
        if( b_menubar_screen )
        {
            BeginFullScreen( &p_fullscreen_state, NULL, 0, 0,
                             NULL, NULL, fullScreenAllowEvents );
        }
        if( b_black == VLC_TRUE )
        {
            CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
            CGDisplayFade( token, 2 , kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, false );
            CGReleaseDisplayFadeReservation( token);
        }
    }
    else if( var_GetBool( p_real_vout, "macosx-background" ) )
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

    b_init_ok = VLC_TRUE;
    return self;
}

- (void)close
{
    [o_view closeVout];
}

- (void) closeWindow
{
    /* XXX waitUntilDone = NO to avoid a possible deadlock when hitting
       Command-Q */
    [self setContentView: NULL];
    [self performSelectorOnMainThread: @selector(closeReal:)
        withObject: NULL waitUntilDone: NO];
}

- (id) closeReal: (id) sender
{
    if( p_fullscreen_state )
        EndFullScreen( p_fullscreen_state, 0 );
    if( b_black == VLC_TRUE )
    {
        CGDisplayFadeReservationToken token;
        CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
        CGDisplayFade( token, 2, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, false );
        CGReleaseDisplayFadeReservation( token);
        CGDisplayRestoreColorSyncSettings();
    }
    [super close];
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
