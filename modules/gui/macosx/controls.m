/*****************************************************************************
 * controls.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Benjamin Pracht <bigben at videolan doit org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>

#import "intf.h"
#import "vout.h"
#import "open.h"
#import "controls.h"
#import "playlist.h"
#include <vlc_osd.h>
#include <vlc_keys.h>

#pragma mark -
/*****************************************************************************
 * VLCControls implementation
 *****************************************************************************/
@implementation VLCControls

- (id)init
{
    [super init];
    o_fs_panel = [[VLCFSPanel alloc] init];
    b_lockAspectRatio = YES;
    return self;
}

- (void)awakeFromNib
{
    [o_specificTime_mi setTitle: _NS("Jump To Time")];
    [o_specificTime_cancel_btn setTitle: _NS("Cancel")];
    [o_specificTime_ok_btn setTitle: _NS("OK")];
    [o_specificTime_sec_lbl setStringValue: _NS("sec.")];
    [o_specificTime_goTo_lbl setStringValue: _NS("Jump to time")];

    o_repeat_off = [NSImage imageNamed:@"repeat_embedded"];

    [self controlTintChanged];

    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector( controlTintChanged )
                                                 name: NSControlTintDidChangeNotification
                                               object: nil];
}

- (void)controlTintChanged
{
    int i_repeat = 0;
    if( [o_btn_repeat image] == o_repeat_single )
        i_repeat = 1;
    else if( [o_btn_repeat image] == o_repeat_all )
        i_repeat = 2;

    if( [NSColor currentControlTint] == NSGraphiteControlTint )
    {
        o_repeat_single = [NSImage imageNamed:@"repeat_single_embedded_graphite"];
        o_repeat_all = [NSImage imageNamed:@"repeat_embedded_graphite"];
        
        [o_btn_shuffle setAlternateImage: [NSImage imageNamed: @"shuffle_embedded_graphite"]];
        [o_btn_addNode setAlternateImage: [NSImage imageNamed: @"add_embedded_graphite"]];
    }
    else
    {
        o_repeat_single = [NSImage imageNamed:@"repeat_single_embedded_blue"];
        o_repeat_all = [NSImage imageNamed:@"repeat_embedded_blue"];
        
        [o_btn_shuffle setAlternateImage: [NSImage imageNamed: @"shuffle_embedded_blue"]];
        [o_btn_addNode setAlternateImage: [NSImage imageNamed: @"add_embedded_blue"]];
    }
    
    /* update the repeat button, but keep its state */
    if( i_repeat == 1 )
        [self repeatOne];
    else if( i_repeat == 2 )
        [self repeatAll];
    else
        [self repeatOff];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    
    [o_fs_panel release];
    [o_repeat_single release];
    [o_repeat_all release];
    [o_repeat_off release];
    
    [super dealloc];
}

- (IBAction)play:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Hold( p_intf );
    bool empty;

    PL_LOCK;
    empty = playlist_IsEmpty( p_playlist );
    PL_UNLOCK;

    pl_Release( p_intf );

    if( empty )
        [o_main intfOpenFileGeneric: (id)sender];

    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_PLAY_PAUSE );
}

- (id)voutView
{
    id o_window;
    id o_voutView = nil;
    id o_embeddedViewList = [[VLCMain sharedInstance] embeddedList];
    NSEnumerator *o_enumerator = [[NSApp orderedWindows] objectEnumerator];
    while( !o_voutView && ( o_window = [o_enumerator nextObject] ) )
    {
        /* We have an embedded vout */
        if( [o_embeddedViewList windowContainsEmbedded: o_window] )
        {
            o_voutView = [o_embeddedViewList viewForWindow: o_window];
        }
        /* We have a detached vout */
        else if( [[o_window className] isEqualToString: @"VLCVoutWindow"] )
        {
            o_voutView = [o_window voutView];
        }
    }
    return [[o_voutView retain] autorelease];
}

- (BOOL)aspectRatioIsLocked
{
    return b_lockAspectRatio;
}

- (IBAction)stop:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_STOP );
    /* Close the window directly, because we do know that there
     * won't be anymore video. It's currently waiting a bit. */
    [[[self voutView] window] orderOut:self];
}

- (IBAction)faster:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_FASTER );
}

- (IBAction)slower:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_SLOWER );
}

- (IBAction)normalSpeed:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_RATE_NORMAL );
}

- (IBAction)prev:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_PREV );
}

- (IBAction)next:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_NEXT );
}

- (IBAction)random:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Hold( p_intf );

    var_Get( p_playlist, "random", &val );
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "random", val );
    if( val.b_bool )
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, "%s", _( "Random On" ) );
        config_PutInt( p_playlist, "random", 1 );
    }
    else
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, "%s", _( "Random Off" ) );
        config_PutInt( p_playlist, "random", 0 );
    }

    p_intf->p_sys->b_playmode_update = true;
    p_intf->p_sys->b_intf_update = true;
    pl_Release( p_intf );
}

/* three little ugly helpers */
- (void)repeatOne
{
    [o_btn_repeat setImage: o_repeat_single];
    [o_btn_repeat setAlternateImage: o_repeat_all];
    [o_btn_repeat_embed setImage: [NSImage imageNamed:@"sidebarRepeatOneOn"]];
}
- (void)repeatAll
{
    [o_btn_repeat setImage: o_repeat_all];
    [o_btn_repeat setAlternateImage: o_repeat_off];
    [o_btn_repeat_embed setImage: [NSImage imageNamed:@"sidebarRepeatOn"]];
}
- (void)repeatOff
{
    [o_btn_repeat setImage: o_repeat_off];
    [o_btn_repeat setAlternateImage: o_repeat_single];
    [o_btn_repeat_embed setImage: [NSImage imageNamed:@"sidebarRepeat"]];
}
- (void)shuffle
{
    vlc_value_t val;
    playlist_t *p_playlist = pl_Hold( VLCIntf );
    var_Get( p_playlist, "random", &val );
    [o_btn_shuffle setState: val.b_bool];
	if(val.b_bool)
        [o_btn_shuffle_embed setImage: [NSImage imageNamed:@"sidebarShuffleOn"]];
	else
        [o_btn_shuffle_embed setImage: [NSImage imageNamed:@"sidebarShuffle"]];    
    pl_Release( VLCIntf );
}

- (IBAction)repeatButtonAction:(id)sender
{
    vlc_value_t looping,repeating;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Hold( p_intf );

    var_Get( p_playlist, "repeat", &repeating );
    var_Get( p_playlist, "loop", &looping );

    if( !repeating.b_bool && !looping.b_bool )
    {
        /* was: no repeating at all, switching to Repeat One */
 
        /* set our button's look */
        [self repeatOne];
 
        /* prepare core communication */
        repeating.b_bool = true;
        looping.b_bool = false;
        config_PutInt( p_playlist, "repeat", 1 );
        config_PutInt( p_playlist, "loop", 0 );
 
        /* show the change */
        vout_OSDMessage( p_intf, DEFAULT_CHAN, "%s", _( "Repeat One" ) );
    }
    else if( repeating.b_bool && !looping.b_bool )
    {
        /* was: Repeat One, switching to Repeat All */
 
        /* set our button's look */
        [self repeatAll];
 
        /* prepare core communication */
        repeating.b_bool = false;
        looping.b_bool = true;
        config_PutInt( p_playlist, "repeat", 0 );
        config_PutInt( p_playlist, "loop", 1 );
 
        /* show the change */
        vout_OSDMessage( p_intf, DEFAULT_CHAN, "%s", _( "Repeat All" ) );
    }
    else
    {
        /* was: Repeat All or bug in VLC, switching to Repeat Off */
 
        /* set our button's look */
        [self repeatOff];
 
        /* prepare core communication */
        repeating.b_bool = false;
        looping.b_bool = false;
        config_PutInt( p_playlist, "repeat", 0 );
        config_PutInt( p_playlist, "loop", 0 );
 
        /* show the change */
        vout_OSDMessage( p_intf, DEFAULT_CHAN, "%s", _( "Repeat Off" ) );
    }

    /* communicate with core and the main intf loop */
    var_Set( p_playlist, "repeat", repeating );
    var_Set( p_playlist, "loop", looping );
    p_intf->p_sys->b_playmode_update = true;
    p_intf->p_sys->b_intf_update = true;

    pl_Release( p_intf );
}


- (IBAction)repeat:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Hold( p_intf );

    var_Get( p_playlist, "repeat", &val );
    if (!val.b_bool)
    {
        var_Set( p_playlist, "loop", val );
    }
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "repeat", val );
    if( val.b_bool )
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, "%s", _( "Repeat One" ) );
        config_PutInt( p_playlist, "repeat", 1 );
    }
    else
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, "%s", _( "Repeat Off" ) );
        config_PutInt( p_playlist, "repeat", 0 );
    }
 
    p_intf->p_sys->b_playmode_update = true;
    p_intf->p_sys->b_intf_update = true;
    pl_Release( p_intf );
}

- (IBAction)loop:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Hold( p_intf );

    var_Get( p_playlist, "loop", &val );
    if (!val.b_bool)
    {
        var_Set( p_playlist, "repeat", val );
    }
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "loop", val );
    if( val.b_bool )
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, "%s", _( "Repeat All" ) );
        config_PutInt( p_playlist, "loop", 1 );
    }
    else
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, "%s", _( "Repeat Off" ) );
        config_PutInt( p_playlist, "loop", 0 );
    }

    p_intf->p_sys->b_playmode_update = true;
    p_intf->p_sys->b_intf_update = true;
    pl_Release( p_intf );
}

- (IBAction)quitAfterPlayback:(id)sender
{
    vlc_value_t val;
    playlist_t * p_playlist = pl_Hold( VLCIntf );
    var_Get( p_playlist, "play-and-exit", &val );
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "play-and-exit", val );
    pl_Release( VLCIntf );
}

- (IBAction)forward:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_SHORT );
}

- (IBAction)backward:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_SHORT );
}


- (IBAction)volumeUp:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_VOL_UP );
    /* Manage volume status */
    [o_main manageVolumeSlider];
}

- (IBAction)volumeDown:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_VOL_DOWN );
    /* Manage volume status */
    [o_main manageVolumeSlider];
}

- (IBAction)mute:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_VOL_MUTE );
    /* Manage volume status */
    [o_main manageVolumeSlider];
}

- (IBAction)volumeSliderUpdated:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Hold( p_intf );
    audio_volume_t i_volume = (audio_volume_t)[sender intValue];
    int i_volume_step;

    i_volume_step = config_GetInt( p_intf->p_libvlc, "volume-step" );
    aout_VolumeSet( p_playlist, i_volume * i_volume_step );
    pl_Release( p_intf );
    /* Manage volume status */
    [o_main manageVolumeSlider];
}

- (IBAction)showPosition: (id)sender
{
    input_thread_t * p_input = pl_CurrentInput( VLCIntf );
    if( p_input != NULL )
    {
        vout_thread_t *p_vout = input_GetVout( p_input );
        if( p_vout != NULL )
        {
            var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_POSITION );
            vlc_object_release( (vlc_object_t *)p_vout );
        }
        vlc_object_release( p_input );
    }
}

- (IBAction)toogleFullscreen:(id)sender {
    NSMenuItem *o_mi = [[NSMenuItem alloc] initWithTitle: _NS("Fullscreen") action: nil keyEquivalent:@""];
    [self windowAction: [o_mi autorelease]];
}

- (BOOL) isFullscreen {
    id o_vout_view = [self voutView];
    if( o_vout_view )
    {
        return [o_vout_view isFullscreen];
    }
    return NO;
}

- (IBAction)windowAction:(id)sender
{
    NSString *o_title = [sender title];
    input_thread_t * p_input = pl_CurrentInput( VLCIntf );

    if( p_input != NULL )
    {
        vout_thread_t *p_vout = input_GetVout( p_input );
        if( p_vout != NULL )
        {
            id o_vout_view = [self voutView];
            if( o_vout_view )
            {
                if( [o_title isEqualToString: _NS("Half Size") ] )
                    [o_vout_view scaleWindowWithFactor: 0.5 animate: YES];
                else if( [o_title isEqualToString: _NS("Normal Size") ] )
                    [o_vout_view scaleWindowWithFactor: 1.0 animate: YES];
                else if( [o_title isEqualToString: _NS("Double Size") ] )
                    [o_vout_view scaleWindowWithFactor: 2.0 animate: YES];
                else if( [o_title isEqualToString: _NS("Float on Top") ] )
                    [o_vout_view toggleFloatOnTop];
                else if( [o_title isEqualToString: _NS("Fit to Screen") ] )
                {
                    id o_window = [o_vout_view voutWindow];
                    if( ![o_window isZoomed] )
                        [o_window performZoom:self];
                }
                else if( [o_title isEqualToString: _NS("Snapshot") ] )
                {
                    [o_vout_view snapshot];
                }
                else
                {
                    /* Fullscreen state for next time will be saved here too */
                    [o_vout_view toggleFullscreen];
                }
            }
            vlc_object_release( (vlc_object_t *)p_vout );
        }
        else
        {
            playlist_t * p_playlist = pl_Hold( VLCIntf );

            if( [o_title isEqualToString: _NS("Fullscreen")] ||
                [sender isKindOfClass:[NSButton class]] )
            {
                var_ToggleBool( p_playlist, "fullscreen" );
            }

            pl_Release( VLCIntf );
        }
        vlc_object_release( p_input );
    }
}

- (IBAction)telxTransparent:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    vlc_object_t *p_vbi;
    p_vbi = (vlc_object_t *) vlc_object_find_name( p_intf,
                    "zvbi", FIND_ANYWHERE );
    if( p_vbi )
    {
        var_SetBool( p_vbi, "vbi-opaque", [sender state] );
        [sender setState: ![sender state]];
        vlc_object_release( p_vbi );
    }
}

- (IBAction)telxNavLink:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    vlc_object_t *p_vbi;
    int i_page = 0;

    if( [[sender title] isEqualToString: _NS("Index")] )
        i_page = 'i' << 16;
    else if( [[sender title] isEqualToString: _NS("Red")] )
        i_page = 'r' << 16;
    else if( [[sender title] isEqualToString: _NS("Green")] )
        i_page = 'g' << 16;
    else if( [[sender title] isEqualToString: _NS("Yellow")] )
        i_page = 'y' << 16;
    else if( [[sender title] isEqualToString: _NS("Blue")] )
        i_page = 'b' << 16;
    if( i_page == 0 ) return;

    p_vbi = (vlc_object_t *) vlc_object_find_name( p_intf,
                "zvbi", FIND_ANYWHERE );
    if( p_vbi )
    {
        var_SetInteger( p_vbi, "vbi-page", i_page );
        vlc_object_release( p_vbi );
    }
}

- (IBAction)lockVideosAspectRatio:(id)sender
{
    if( [sender state] == NSOffState )
        [sender setState: NSOnState];
    else
        [sender setState: NSOffState];

    b_lockAspectRatio = !b_lockAspectRatio;
}

- (IBAction)addSubtitleFile:(id)sender
{
    NSInteger i_returnValue = 0;
    input_thread_t * p_input = pl_CurrentInput( VLCIntf );
    if( !p_input ) return;

    input_item_t *p_item = input_GetItem( p_input );
    if( !p_item ) return;

    char *path = input_item_GetURI( p_item );
    if( !path ) path = strdup( "" );

    NSOpenPanel * openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseFiles: YES];
    [openPanel setCanChooseDirectories: NO];
    [openPanel setAllowsMultipleSelection: YES];
    i_returnValue = [openPanel runModalForDirectory: [NSString stringWithUTF8String: path] file: nil types: [NSArray arrayWithObjects: @"cdg",@"@idx",@"srt",@"sub",@"utf",@"ass",@"ssa",@"aqt",@"jss",@"psb",@"rt",@"smi", nil]];
    free( path );

    if( i_returnValue == NSOKButton )
    {
        NSUInteger c = 0;
        if( !p_input ) return;
        
        c = [[openPanel filenames] count];

        for (int i = 0; i < [[openPanel filenames] count] ; i++)
        {
            msg_Dbg( VLCIntf, "loading subs from %s", [[[openPanel filenames] objectAtIndex: i] UTF8String] );
            if( input_AddSubtitle( p_input, [[[openPanel filenames] objectAtIndex: i] UTF8String], TRUE ) )
                msg_Warn( VLCIntf, "unable to load subtitles from '%s'",
                         [[[openPanel filenames] objectAtIndex: i] UTF8String] );
        }
    }
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    intf_thread_t * p_intf = VLCIntf;
    float f_yabsvalue = [theEvent deltaY] > 0.0f ? [theEvent deltaY] : -[theEvent deltaY];
    float f_xabsvalue = [theEvent deltaX] > 0.0f ? [theEvent deltaX] : -[theEvent deltaX];
    int i, i_yvlckey, i_xvlckey;

    if ([theEvent deltaY] < 0.0f)
        i_yvlckey = KEY_MOUSEWHEELDOWN;
    else
        i_yvlckey = KEY_MOUSEWHEELUP;

    if ([theEvent deltaX] < 0.0f)
        i_xvlckey = KEY_MOUSEWHEELRIGHT;
    else
        i_xvlckey = KEY_MOUSEWHEELLEFT;

    /* Send multiple key event, depending on the intensity of the event */
    for (i = 0; i < (int)(f_yabsvalue/4.+1.) && f_yabsvalue > 0.05 ; i++)
        var_SetInteger( p_intf->p_libvlc, "key-pressed", i_yvlckey );

    /* Prioritize Y event (sound volume) over X event */
    if (f_yabsvalue < 0.05)
    {
        for (i = 0; i < (int)(f_xabsvalue/6.+1.) && f_xabsvalue > 0.05; i++)
         var_SetInteger( p_intf->p_libvlc, "key-pressed", i_xvlckey );
    }
}

- (BOOL)keyEvent:(NSEvent *)o_event
{
    BOOL eventHandled = NO;
    unichar key = [[o_event charactersIgnoringModifiers] characterAtIndex: 0];

    if( key )
    {
        input_thread_t * p_input = pl_CurrentInput( VLCIntf );
        if( p_input != NULL )
        {
            vout_thread_t *p_vout = input_GetVout( p_input );

            if( p_vout != NULL )
            {
                /* Escape */
                if( key == (unichar) 0x1b )
                {
                    id o_vout_view = [self voutView];
                    if( o_vout_view && [o_vout_view isFullscreen] )
                    {
                        [o_vout_view toggleFullscreen];
                        eventHandled = YES;
                    }
                }
                else if( key == ' ' )
                {
                    [self play:self];
                    eventHandled = YES;
                }
                vlc_object_release( (vlc_object_t *)p_vout );
            }
            vlc_object_release( p_input );
        }
    }
    return eventHandled;
}

- (void)setupVarMenuItem:(NSMenuItem *)o_mi
                    target:(vlc_object_t *)p_object
                    var:(const char *)psz_variable
                    selector:(SEL)pf_callback
{
    vlc_value_t val, text;
    int i_type = var_Type( p_object, psz_variable );

    switch( i_type & VLC_VAR_TYPE )
    {
    case VLC_VAR_VOID:
    case VLC_VAR_BOOL:
    case VLC_VAR_VARIABLE:
    case VLC_VAR_STRING:
    case VLC_VAR_INTEGER:
        break;
    default:
        /* Variable doesn't exist or isn't handled */
        return;
    }
 
    /* Make sure we want to display the variable */
    if( i_type & VLC_VAR_HASCHOICE )
    {
        var_Change( p_object, psz_variable, VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int == 0 ) return;
        if( (i_type & VLC_VAR_TYPE) != VLC_VAR_VARIABLE && val.i_int == 1 )
            return;
    }
 
    /* Get the descriptive name of the variable */
    var_Change( p_object, psz_variable, VLC_VAR_GETTEXT, &text, NULL );
    [o_mi setTitle: [[VLCMain sharedInstance] localizedString: text.psz_string ?
                                        text.psz_string : psz_variable ]];

    if( i_type & VLC_VAR_HASCHOICE )
    {
        NSMenu *o_menu = [o_mi submenu];

        [self setupVarMenu: o_menu forMenuItem: o_mi target:p_object
                        var:psz_variable selector:pf_callback];
 
        free( text.psz_string );
        return;
    }
    if( var_Get( p_object, psz_variable, &val ) < 0 )
    {
        return;
    }

    VLCAutoGeneratedMenuContent *o_data;
    switch( i_type & VLC_VAR_TYPE )
    {
    case VLC_VAR_VOID:
        o_data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                andValue: val ofType: i_type];
        [o_mi setRepresentedObject: [o_data autorelease]];
        break;

    case VLC_VAR_BOOL:
        o_data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                andValue: val ofType: i_type];
        [o_mi setRepresentedObject: [o_data autorelease]];
        if( !( i_type & VLC_VAR_ISCOMMAND ) )
            [o_mi setState: val.b_bool ? TRUE : FALSE ];
        break;

    default:
        break;
    }

    if( ( i_type & VLC_VAR_TYPE ) == VLC_VAR_STRING ) free( val.psz_string );
    free( text.psz_string );
}


- (void)setupVarMenu:(NSMenu *)o_menu
                    forMenuItem: (NSMenuItem *)o_parent
                    target:(vlc_object_t *)p_object
                    var:(const char *)psz_variable
                    selector:(SEL)pf_callback
{
    vlc_value_t val, val_list, text_list;
    int i_type, i, i_nb_items;

    /* remove previous items */
    i_nb_items = [o_menu numberOfItems];
    for( i = 0; i < i_nb_items; i++ )
    {
        [o_menu removeItemAtIndex: 0];
    }

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_variable );

    /* Make sure we want to display the variable */
    if( i_type & VLC_VAR_HASCHOICE )
    {
        var_Change( p_object, psz_variable, VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int == 0 ) return;
        if( (i_type & VLC_VAR_TYPE) != VLC_VAR_VARIABLE && val.i_int == 1 )
            return;
    }
    else
    {
        return;
    }

    switch( i_type & VLC_VAR_TYPE )
    {
    case VLC_VAR_VOID:
    case VLC_VAR_BOOL:
    case VLC_VAR_VARIABLE:
    case VLC_VAR_STRING:
    case VLC_VAR_INTEGER:
        break;
    default:
        /* Variable doesn't exist or isn't handled */
        return;
    }

    if( var_Get( p_object, psz_variable, &val ) < 0 )
    {
        return;
    }

    if( var_Change( p_object, psz_variable, VLC_VAR_GETLIST,
                    &val_list, &text_list ) < 0 )
    {
        if( (i_type & VLC_VAR_TYPE) == VLC_VAR_STRING ) free( val.psz_string );
        return;
    }

    /* make (un)sensitive */
    [o_parent setEnabled: ( val_list.p_list->i_count > 1 )];

    /* Aspect Ratio */
    if( [[o_parent title] isEqualToString: _NS("Aspect-ratio")] == YES )
    {
        NSMenuItem *o_lmi_tmp2;
        o_lmi_tmp2 = [o_menu addItemWithTitle: _NS("Lock Aspect Ratio") action: @selector(lockVideosAspectRatio:) keyEquivalent: @""];
        [o_lmi_tmp2 setTarget: self];
        [o_lmi_tmp2 setEnabled: YES];
        [o_lmi_tmp2 setState: b_lockAspectRatio];
        [o_parent setEnabled: YES];
        [o_menu addItem: [NSMenuItem separatorItem]];
    }

    /* special case for the subtitles items */
    if( [[o_parent title] isEqualToString: _NS("Subtitles Track")] == YES )
    {
        NSMenuItem * o_lmi_tmp;
        o_lmi_tmp = [o_menu addItemWithTitle: _NS("Open File...") action: @selector(addSubtitleFile:) keyEquivalent: @""];
        [o_lmi_tmp setTarget: self];
        [o_lmi_tmp setEnabled: YES];
        [o_parent setEnabled: YES];
        [o_menu addItem: [NSMenuItem separatorItem]];
    }

    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        NSMenuItem * o_lmi;
        NSString *o_title = @"";
        VLCAutoGeneratedMenuContent *o_data;

        switch( i_type & VLC_VAR_TYPE )
        {
        case VLC_VAR_STRING:

            o_title = [[VLCMain sharedInstance] localizedString: text_list.p_list->p_values[i].psz_string ?
                text_list.p_list->p_values[i].psz_string : val_list.p_list->p_values[i].psz_string ];

            o_lmi = [o_menu addItemWithTitle: o_title action: pf_callback keyEquivalent: @""];
            o_data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                    andValue: val_list.p_list->p_values[i] ofType: i_type];
            [o_lmi setRepresentedObject: [o_data autorelease]];
            [o_lmi setTarget: self];

            if( !strcmp( val.psz_string, val_list.p_list->p_values[i].psz_string ) && !( i_type & VLC_VAR_ISCOMMAND ) )
                [o_lmi setState: TRUE ];

            break;

        case VLC_VAR_INTEGER:

             o_title = text_list.p_list->p_values[i].psz_string ?
                                 [[VLCMain sharedInstance] localizedString: text_list.p_list->p_values[i].psz_string] :
                                 [NSString stringWithFormat: @"%d",
                                 val_list.p_list->p_values[i].i_int];

            o_lmi = [o_menu addItemWithTitle: o_title action: pf_callback keyEquivalent: @""];
            o_data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                    andValue: val_list.p_list->p_values[i] ofType: i_type];
            [o_lmi setRepresentedObject: [o_data autorelease]];
            [o_lmi setTarget: self];

            if( val_list.p_list->p_values[i].i_int == val.i_int && !( i_type & VLC_VAR_ISCOMMAND ) )
                [o_lmi setState: TRUE ];
            break;

        default:
          break;
        }
    }

    /* special case for the subtitles sub-menu
     * In case that we don't have any subs, we don't want a separator item at the end */
    if( [[o_parent title] isEqualToString: _NS("Subtitles Track")] == YES )
    {
        if( [o_menu numberOfItems] == 2 )
            [o_menu removeItemAtIndex: 1];
    }

    /* clean up everything */
    if( (i_type & VLC_VAR_TYPE) == VLC_VAR_STRING ) free( val.psz_string );
    var_FreeList( &val_list, &text_list );
}

- (IBAction)toggleVar:(id)sender
{
    NSMenuItem *o_mi = (NSMenuItem *)sender;
    VLCAutoGeneratedMenuContent *o_data = [o_mi representedObject];
    [NSThread detachNewThreadSelector: @selector(toggleVarThread:)
        toTarget: self withObject: o_data];

    return;
}

- (int)toggleVarThread: (id)data
{
    vlc_object_t *p_object;
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    assert([data isKindOfClass:[VLCAutoGeneratedMenuContent class]]);
    VLCAutoGeneratedMenuContent *menuContent = (VLCAutoGeneratedMenuContent *)data;

    vlc_thread_set_priority( VLCIntf , VLC_THREAD_PRIORITY_LOW );

    p_object = [menuContent vlcObject];

    if( p_object != NULL )
    {
        var_Set( p_object, [menuContent name], [menuContent value] );
        vlc_object_release( p_object );
        [o_pool release];
        return true;
    }
    [o_pool release];
    return VLC_EGENERIC;
}

- (IBAction)goToSpecificTime:(id)sender
{
    if( sender == o_specificTime_cancel_btn )
    {
        [NSApp endSheet: o_specificTime_win];
        [o_specificTime_win close];
    }
    else if( sender == o_specificTime_ok_btn )
    {
        input_thread_t * p_input = pl_CurrentInput( VLCIntf );
        if( p_input )
        {
            unsigned int timeInSec = 0;
            NSString * fieldContent = [o_specificTime_enter_fld stringValue];
            if( [[fieldContent componentsSeparatedByString: @":"] count] > 1 &&
                [[fieldContent componentsSeparatedByString: @":"] count] <= 3 )
            {
                NSArray * ourTempArray = \
                    [fieldContent componentsSeparatedByString: @":"];

                if( [[fieldContent componentsSeparatedByString: @":"] count] == 3 )
                {
                    timeInSec += ([[ourTempArray objectAtIndex: 0] intValue] * 3600); //h
                    timeInSec += ([[ourTempArray objectAtIndex: 1] intValue] * 60); //m
                    timeInSec += [[ourTempArray objectAtIndex: 2] intValue];        //s
                }
                else
                {
                    timeInSec += ([[ourTempArray objectAtIndex: 0] intValue] * 60); //m
                    timeInSec += [[ourTempArray objectAtIndex: 1] intValue]; //s
                }
            }
            else
                timeInSec = [fieldContent intValue];

            input_Control( p_input, INPUT_SET_TIME, (int64_t)(timeInSec * 1000000));
            vlc_object_release( p_input );
        }

        [NSApp endSheet: o_specificTime_win];
        [o_specificTime_win close];
    }
    else
    {
        input_thread_t * p_input = pl_CurrentInput( VLCIntf );
        if( p_input )
        {
            /* we can obviously only do that if an input is available */
            vlc_value_t pos, length;
            var_Get( p_input, "time", &pos );
            [o_specificTime_enter_fld setIntValue: (pos.i_time / 1000000)];
            var_Get( p_input, "length", &length );
            [o_specificTime_stepper setMaxValue: (length.i_time / 1000000)];

            [NSApp beginSheet: o_specificTime_win modalForWindow: \
                [NSApp mainWindow] modalDelegate: self didEndSelector: nil \
                contextInfo: nil];
            [o_specificTime_win makeKeyWindow];
            vlc_object_release( p_input );
        }
    }
}

- (id)fspanel
{
    if( o_fs_panel )
        return o_fs_panel;
    else
    {
        msg_Err( VLCIntf, "FSPanel is nil" );
        return NULL;
    }
}

@end

@implementation VLCControls (NSMenuValidation)

- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    BOOL bEnabled = TRUE;
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Hold( p_intf );
    input_thread_t * p_input = playlist_CurrentInput( p_playlist );

    if( [[o_mi title] isEqualToString: _NS("Faster")] ||
        [[o_mi title] isEqualToString: _NS("Slower")] ||
        [[o_mi title] isEqualToString: _NS("Normal rate")] )
    {
        if( p_input != NULL )
        {
            bEnabled = var_GetBool( p_input, "can-rate" );
        }
        else
        {
            bEnabled = FALSE;
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Stop")] )
    {
        if( p_input == NULL )
        {
            bEnabled = FALSE;
        }
        [o_main setupMenus]; /* Make sure input menu is up to date */
    }
    else if( [[o_mi title] isEqualToString: _NS("Previous")] ||
             [[o_mi title] isEqualToString: _NS("Next")] )
    {
        PL_LOCK;
        bEnabled = playlist_CurrentSize( p_playlist ) > 1;
        PL_UNLOCK;
    }
    else if( [[o_mi title] isEqualToString: _NS("Random")] )
    {
        int i_state;
        var_Get( p_playlist, "random", &val );
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    }
    else if( [[o_mi title] isEqualToString: _NS("Repeat One")] )
    {
        int i_state;
        var_Get( p_playlist, "repeat", &val );
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    }
    else if( [[o_mi title] isEqualToString: _NS("Repeat All")] )
    {
        int i_state;
        var_Get( p_playlist, "loop", &val );
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    }
    else if( [[o_mi title] isEqualToString: _NS("Quit after Playback")] )
    {
        int i_state;
        var_Get( p_playlist, "play-and-exit", &val );
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    }
    else if( [[o_mi title] isEqualToString: _NS("Step Forward")] ||
             [[o_mi title] isEqualToString: _NS("Step Backward")] ||
             [[o_mi title] isEqualToString: _NS("Jump To Time")])
    {
        if( p_input != NULL )
        {
            var_Get( p_input, "can-seek", &val);
            bEnabled = val.b_bool;
        }
        else bEnabled = FALSE;
    }
    else if( [[o_mi title] isEqualToString: _NS("Mute")] )
    {
        [o_mi setState: p_intf->p_sys->b_mute ? NSOnState : NSOffState];
        [o_main setupMenus]; /* Make sure audio menu is up to date */
    }
    else if( [[o_mi title] isEqualToString: _NS("Half Size")] ||
                [[o_mi title] isEqualToString: _NS("Normal Size")] ||
                [[o_mi title] isEqualToString: _NS("Double Size")] ||
                [[o_mi title] isEqualToString: _NS("Fit to Screen")] ||
                [[o_mi title] isEqualToString: _NS("Snapshot")] ||
                [[o_mi title] isEqualToString: _NS("Fullscreen")] ||
                [[o_mi title] isEqualToString: _NS("Float on Top")] )
    {
        id o_window;
        NSArray *o_windows = [NSApp orderedWindows];
        NSEnumerator *o_enumerator = [o_windows objectEnumerator];
        bEnabled = FALSE;
 
        if( p_input != NULL )
        {
            vout_thread_t *p_vout = input_GetVout( p_input );
            if( p_vout != NULL )
            {
                if( [[o_mi title] isEqualToString: _NS("Float on Top")] )
                {
                    var_Get( p_vout, "video-on-top", &val );
                    [o_mi setState: val.b_bool ?  NSOnState : NSOffState];
                }
    
                while( (o_window = [o_enumerator nextObject]))
                {
                    if( [[o_window className] isEqualToString: @"VLCVoutWindow"] ||
                                [[[VLCMain sharedInstance] embeddedList]
                                windowContainsEmbedded: o_window])
                    {
                        bEnabled = TRUE;
                        break;
                    }
                }
    
                vlc_object_release( (vlc_object_t *)p_vout );
            }
        }
        if( [[o_mi title] isEqualToString: _NS("Fullscreen")] )
        {
            var_Get( p_playlist, "fullscreen", &val );
            [o_mi setState: val.b_bool];
            bEnabled = TRUE;
        }
        [o_main setupMenus]; /* Make sure video menu is up to date */
    }

    /* Special case for telx menu */
    if( [[o_mi title] isEqualToString: _NS("Normal Size")] )
    {
        NSMenuItem *item = [[o_mi menu] itemWithTitle:_NS("Teletext")];
		bool b_telx = p_input && var_GetInteger( p_input, "teletext-es" ) >= 0;

        [[item submenu] setAutoenablesItems:NO];
        for( int k=0; k < [[item submenu] numberOfItems]; k++ )
        {
            [[[item submenu] itemAtIndex:k] setEnabled: b_telx];
        }
    }

    if( p_input ) vlc_object_release( p_input );
    pl_Release( p_intf );

    return( bEnabled );
}

@end

/*****************************************************************************
 * VLCAutoGeneratedMenuContent implementation
 *****************************************************************************
 * Object connected to a playlistitem which remembers the data belonging to
 * the variable of the autogenerated menu
 *****************************************************************************/
@implementation VLCAutoGeneratedMenuContent

-(id) initWithVariableName:(const char *)name ofObject:(vlc_object_t *)object
        andValue:(vlc_value_t)val ofType:(int)type
{
    self = [super init];

    if( self != nil )
    {
        _vlc_object = vlc_object_hold( object );
        psz_name = strdup( name );
        i_type = type;
        value = val;
        if( (i_type & VLC_VAR_TYPE) == VLC_VAR_STRING )
            value.psz_string = strdup( val.psz_string );
    }

    return( self );
}

- (void)dealloc
{
    vlc_object_release( _vlc_object );
    if( (i_type & VLC_VAR_TYPE) == VLC_VAR_STRING )
        free( value.psz_string );
    free( psz_name );
    [super dealloc];
}

- (const char *)name
{
    return psz_name;
}

- (vlc_value_t)value
{
    return value;
}

- (vlc_object_t *)vlcObject
{
    return vlc_object_hold( _vlc_object );
}


- (int)type
{
    return i_type;
}

@end


/*****************************************************************************
 * VLCTimeField implementation
 *****************************************************************************
 * we need this to catch our click-event in the controller window
 *****************************************************************************/

@implementation VLCTimeField
- (void)mouseDown: (NSEvent *)ourEvent
{
    if( [ourEvent clickCount] > 1 )
        [[[VLCMain sharedInstance] controls] goToSpecificTime: nil];
    else
        [[VLCMain sharedInstance] timeFieldWasClicked: self];
}
@end
