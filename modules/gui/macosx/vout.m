/*****************************************************************************
 * vout.m: MacOS X video output module
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
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
 * VLCWindow implementation
 *****************************************************************************/
@implementation VLCWindow

- (id)initWithVout:(vout_thread_t *)_p_vout frame:(NSRect *)s_frame
{
    int i_timeout;

    p_vout = _p_vout;

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

    NSScreen * o_screen;
    vlc_bool_t b_main_screen;

    var_Create( p_vout, "macosx-vdev", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-fill", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-stretch", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "macosx-opaqueness", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );

    /* Setup the menuitem for the multiple displays. Read the vlc preference (macosx-vdev) for the primary display */
    NSArray * o_screens = [NSScreen screens];
    if( [o_screens count] > 0 && var_Type( p_real_vout, "video-device" ) == 0 )
    {
        int i = 1;
        vlc_value_t val, val2, text;
        NSScreen * o_screen;

        var_Get( p_real_vout, "macosx-vdev", &val );

        var_Create( p_real_vout, "video-device", VLC_VAR_INTEGER |
                                            VLC_VAR_HASCHOICE );
        text.psz_string = _("Video device");
        var_Change( p_real_vout, "video-device", VLC_VAR_SETTEXT, &text, NULL );

        NSEnumerator * o_enumerator = [o_screens objectEnumerator];

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

            if( ( i - 1 ) == val.i_int )
            {
                var_Set( p_real_vout, "video-device", val2 );
            }
            i++;
        }

        var_AddCallback( p_real_vout, "video-device", vout_VarCallback,
                         NULL );

        val2.b_bool = VLC_TRUE;
        var_Set( p_real_vout, "intf-change", val2 );
    }

    /* Find out on which screen to open the window */
    int i_device = var_GetInteger( p_real_vout, "video-device" );
    if( i_device < 0 )
    {
         /* No preference specified. Use the main screen */
        o_screen = [NSScreen mainScreen];
        b_main_screen = 1;
    }
    else
    {
        NSArray *o_screens = [NSScreen screens];
        
        if( [o_screens count] < (unsigned) i_device )
        {
            o_screen = [NSScreen mainScreen];
            b_main_screen = 1;
        }
        else
        {
            i_device--;
            o_screen = [o_screens objectAtIndex: i_device];
            var_SetInteger( p_real_vout, "macosx-vdev", i_device );
            b_main_screen = ( i_device == 0 );
        }
    }

    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    if( p_vout->b_fullscreen )
    {
        NSRect screen_rect = [o_screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0;

        /* Creates a window with size: screen_rect on o_screen */
        [self initWithContentRect: screen_rect
              styleMask: NSBorderlessWindowMask
              backing: NSBackingStoreBuffered
              defer: YES screen: o_screen];

        if( b_main_screen )
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
    return self;
}

- (void)close
{
    if( p_fullscreen_state )
    {
        EndFullScreen( p_fullscreen_state, 0 );
    }
    [super close];
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
    if( var_Get( p_vout, "video-on-top", &val )>=0 && val.b_bool)
    {
        val.b_bool = VLC_FALSE;
        var_Set( p_vout, "video-on-top", val );
    }
    else
    {
        val.b_bool = VLC_TRUE;
        var_Set( p_vout, "video-on-top", val );
    }
}

- (void)toggleFullscreen
{
    vlc_value_t val;
    val.b_bool = !p_real_vout->b_fullscreen;
    var_Set( p_real_vout, "fullscreen", val );
}

- (BOOL)isFullscreen
{
    return( p_vout->b_fullscreen );
}

- (void)snapshot
{
    vout_Control( p_vout, VOUT_SNAPSHOT );
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
    NSMutableString * o_title,* o_mrl;
    vlc_bool_t b_file = VLC_FALSE;
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

    if( ! strcmp( p_input->input.p_access->p_module->psz_shortname, "File" ) )
        b_file = VLC_TRUE;
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
        if( b_file == VLC_TRUE )
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
        int i_width, i_height, i_x, i_y;

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
