/*****************************************************************************
 * vout.m: MacOS X video output module
 *****************************************************************************
 * Copyright (C) 2001-2005 VideoLAN
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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
 * VLCWindow implementation
 *****************************************************************************/
@implementation VLCWindow

- (id) initWithVout: (vout_thread_t *) vout frame: (NSRect *) frame
{
    p_vout  = vout;
    s_frame = frame;

    if( MACOS_VERSION >= 10.2 )
    {
        [self performSelectorOnMainThread: @selector(initReal:)
            withObject: NULL waitUntilDone: YES];
    }
    else
    {
        [self initReal: NULL];
    }

    if( !b_init_ok )
    {
        return NULL;
    }
    
    return self;
}

- (id) initReal: (id) sender
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    NSArray *o_screens = [NSScreen screens];
    NSScreen *o_screen;
    vlc_bool_t b_menubar_screen = VLC_FALSE;
    int i_timeout, i_device;
    vlc_value_t value_drawable;

    b_init_ok = VLC_FALSE;

    var_Get( p_vout->p_vlc, "drawable", &value_drawable );

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
            return NULL;
        }
    }
    
    if( [o_screens count] <= 0 )
    {
        msg_Err( p_vout, "no OSX screens available" );
        return NULL;
    }

    /* p_real_vout: the vout we have to use to check for video-on-top
       and a few other things. If we are the QuickTime output, it's us.
       It we are the OpenGL provider, it is our parent. */
    if( p_vout->i_object_type == VLC_OBJECT_OPENGL )
    {
        p_real_vout = (vout_thread_t *) p_vout->p_parent;
    }
    else
    {
        p_real_vout = p_vout;
    }

    p_fullscreen_state = NULL;
    i_time_mouse_last_moved = mdate();

    var_Create( p_vout, "macosx-vdev", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-fill", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-stretch", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-opaqueness", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );

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

    /* Find out on which screen to open the window */
    if( i_device <= 0 || i_device > (int)[o_screens count] )
    {
         /* No preference specified. Use the main screen */
        o_screen = [NSScreen mainScreen];
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
        NSRect screen_rect = [o_screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0;

        /* Creates a window with size: screen_rect on o_screen */
        [self initWithContentRect: screen_rect
              styleMask: NSBorderlessWindowMask
              backing: NSBackingStoreBuffered
              defer: YES screen: o_screen];

        if( b_menubar_screen )
        {
            BeginFullScreen( &p_fullscreen_state, NULL, 0, 0,
                             NULL, NULL, fullScreenAllowEvents );
        }
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

        if( var_GetBool( p_real_vout, "video-on-top" ) )
        {
            [self setLevel: NSStatusWindowLevel];
        }

        if( !s_frame )
        {
            [self center];
        }
    }

    [self updateTitle];
    [self makeKeyAndOrderFront: nil];
    [self setReleasedWhenClosed: YES];

    /* We'll catch mouse events */
    [self setAcceptsMouseMovedEvents: YES];
    [self makeFirstResponder: self];
    
    [o_pool release];

    b_init_ok = VLC_TRUE;
    return self;
}

- (void)close
{
    [super close];
    if( p_fullscreen_state )
    {
        EndFullScreen( p_fullscreen_state, 0 );
    }
}

- (void)setOnTop:(BOOL)b_on_top
{
    if( b_on_top )
    {
        [self setLevel: NSStatusWindowLevel];
    }
    else
    {
        [self setLevel: NSNormalWindowLevel];
    }
}

- (void)hideMouse:(BOOL)b_hide
{
    BOOL b_inside;
    NSPoint ml;
    NSView *o_contents = [self contentView];
    
    ml = [self convertScreenToBase:[NSEvent mouseLocation]];
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
    if( p_fullscreen_state )
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

    /* Disable screensaver */
    UpdateSystemActivity( UsrActivity );
}

- (void)scaleWindowWithFactor: (float)factor
{
    NSSize newsize;
    int i_corrected_height, i_corrected_width;
    NSPoint topleftbase;
    NSPoint topleftscreen;
    
    if ( !p_vout->b_fullscreen )
    {
        topleftbase.x = 0;
        topleftbase.y = [self frame].size.height;
        topleftscreen = [self convertBaseToScreen: topleftbase];
        
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
    
        [self setContentSize: newsize];
        
        [self setFrameTopLeftPoint: topleftscreen];
        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }
}

- (void)toggleFloatOnTop
{
    vlc_value_t val;

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

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

/* Sometimes crashes VLC....
- (BOOL)performKeyEquivalent:(NSEvent *)o_event
{
        return [[VLCMain sharedInstance] hasDefinedShortcutKey:o_event];
}*/

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
             if( [self isFullscreen] )
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

- (void)updateTitle
{
    NSMutableString * o_title = NULL, * o_mrl = NULL;
    input_thread_t * p_input;
    
    if( p_vout == NULL )
    {
        return;
    }
    
    p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT,
                                                FIND_PARENT );
    
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

    vlc_object_release( p_input );
    if( o_mrl != nil )
    {
        if( p_input->input.p_access && !strcmp( p_input->input.p_access->p_module->psz_shortname, "File" ) )
        {
            NSRange prefix_range = [o_mrl rangeOfString: @"file:"];
            if( prefix_range.location != NSNotFound )
                [o_mrl deleteCharactersInRange: prefix_range];
            [self setRepresentedFilename: o_mrl];
        }
        [self setTitle: o_title];
    }
    else
    {
        [self setTitle: [NSString stringWithCString: VOUT_TITLE]];
    }
}

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

- (void)mouseDown:(NSEvent *)o_event
{
    vlc_value_t val;

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

- (void)otherMouseDown:(NSEvent *)o_event
{
    vlc_value_t val;

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

- (void)rightMouseDown:(NSEvent *)o_event
{
    vlc_value_t val;

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

- (void)mouseUp:(NSEvent *)o_event
{
    vlc_value_t val;

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

- (void)otherMouseUp:(NSEvent *)o_event
{
    vlc_value_t val;

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

- (void)rightMouseUp:(NSEvent *)o_event
{
    vlc_value_t val;

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
    NSView * o_view;

    i_time_mouse_last_moved = mdate();

    o_view = [self contentView];
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

    [super mouseMoved: o_event];
}

@end
