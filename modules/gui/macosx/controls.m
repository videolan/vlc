/*****************************************************************************
 * controls.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2011 the VideoLAN team
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
}


- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    [o_fs_panel release];

    [super dealloc];
}

- (IBAction)play:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );
    bool empty;

    PL_LOCK;
    empty = playlist_IsEmpty( p_playlist );
    PL_UNLOCK;

    if( empty )
        [[VLCOpen sharedInstance] openFileGeneric];

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
    playlist_t * p_playlist = pl_Get( p_intf );

    var_Get( p_playlist, "random", &val );
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "random", val );
    if( val.b_bool )
    {
        //vout_OSDMessage( p_intf, SPU_DEFAULT_CHANNEL, "%s", _( "Random On" ) );
        config_PutInt( p_playlist, "random", 1 );
    }
    else
    {
        //vout_OSDMessage( p_intf, SPU_DEFAULT_CHANNEL, "%s", _( "Random Off" ) );
        config_PutInt( p_playlist, "random", 0 );
    }
    [self shuffle];

    p_intf->p_sys->b_playmode_update = true;
    p_intf->p_sys->b_intf_update = true;
}

/* three little ugly helpers */
- (void)repeatOne
{
    [o_btn_repeat setImage: [NSImage imageNamed:@"repeat-one"]];
    [o_btn_repeat setAlternateImage: [NSImage imageNamed:@"repeat-one-pressed"]];
}
- (void)repeatAll
{
    [o_btn_repeat setImage: [NSImage imageNamed:@"repeat-all"]];
    [o_btn_repeat setAlternateImage: [NSImage imageNamed:@"repeat-all-pressed"]];
}
- (void)repeatOff
{
    [o_btn_repeat setImage: [NSImage imageNamed:@"repeat"]];
    [o_btn_repeat setAlternateImage: [NSImage imageNamed:@"repeat-pressed"]];
}
- (void)shuffle
{
    vlc_value_t val;
    playlist_t *p_playlist = pl_Get( VLCIntf );
    var_Get( p_playlist, "random", &val );
	if(val.b_bool) {
        [o_btn_shuffle setImage: [NSImage imageNamed:@"shuffle-on"]];
        [o_btn_shuffle setAlternateImage: [NSImage imageNamed:@"shuffle-blue-pressed"]];
    }
    else
    {
        [o_btn_shuffle setImage: [NSImage imageNamed:@"shuffle"]];
        [o_btn_shuffle setAlternateImage: [NSImage imageNamed:@"shuffle-pressed"]];
    }
}

- (IBAction)repeatButtonAction:(id)sender
{
    vlc_value_t looping,repeating;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );

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
        //vout_OSDMessage( p_intf, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat One" ) );
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
        //vout_OSDMessage( p_intf, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat All" ) );
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
        //vout_OSDMessage( p_intf, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat Off" ) );
    }

    /* communicate with core and the main intf loop */
    var_Set( p_playlist, "repeat", repeating );
    var_Set( p_playlist, "loop", looping );
    p_intf->p_sys->b_playmode_update = true;
    p_intf->p_sys->b_intf_update = true;
}


- (IBAction)repeat:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );

    var_Get( p_playlist, "repeat", &val );
    if (!val.b_bool)
    {
        var_Set( p_playlist, "loop", val );
    }
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "repeat", val );
    if( val.b_bool )
    {
        //vout_OSDMessage( p_intf, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat One" ) );
        config_PutInt( p_playlist, "repeat", 1 );
    }
    else
    {
        //vout_OSDMessage( p_intf, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat Off" ) );
        config_PutInt( p_playlist, "repeat", 0 );
    }

    p_intf->p_sys->b_playmode_update = true;
    p_intf->p_sys->b_intf_update = true;
}

- (IBAction)loop:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );

    var_Get( p_playlist, "loop", &val );
    if (!val.b_bool)
    {
        var_Set( p_playlist, "repeat", val );
    }
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "loop", val );
    if( val.b_bool )
    {
        //vout_OSDMessage( p_intf, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat All" ) );
        config_PutInt( p_playlist, "loop", 1 );
    }
    else
    {
        //vout_OSDMessage( p_intf, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat Off" ) );
        config_PutInt( p_playlist, "loop", 0 );
    }

    p_intf->p_sys->b_playmode_update = true;
    p_intf->p_sys->b_intf_update = true;
}

- (IBAction)quitAfterPlayback:(id)sender
{
    vlc_value_t val;
    playlist_t * p_playlist = pl_Get( VLCIntf );
    var_ToggleBool( p_playlist, "play-and-exit" );
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
    playlist_t * p_playlist = pl_Get( p_intf );
    audio_volume_t i_volume = (audio_volume_t)[sender intValue];
    int i_volume_step;

    i_volume_step = config_GetInt( p_intf->p_libvlc, "volume-step" );
    aout_VolumeSet( p_playlist, i_volume * i_volume_step );
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
            playlist_t * p_playlist = pl_Get( VLCIntf );

            if( [o_title isEqualToString: _NS("Fullscreen")] ||
                [sender isKindOfClass:[NSButton class]] )
            {
                var_ToggleBool( p_playlist, "fullscreen" );
            }
        }
        vlc_object_release( p_input );
    }
}

- (IBAction)telxTransparent:(id)sender
{
    vlc_object_t *p_vbi;
    p_vbi = (vlc_object_t *) vlc_object_find_name( pl_Get( VLCIntf ), "zvbi" );
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

    p_vbi = (vlc_object_t *) vlc_object_find_name( pl_Get( VLCIntf ), "zvbi" );
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
    [openPanel setAllowedFileTypes: [NSArray arrayWithObjects: @"cdg",@"@idx",@"srt",@"sub",@"utf",@"ass",@"ssa",@"aqt",@"jss",@"psb",@"rt",@"smi",@"txt",@"smil", nil]];
    [openPanel setDirectoryURL:[NSURL fileURLWithPath:[[NSString stringWithUTF8String:path] stringByExpandingTildeInPath]]];
    i_returnValue = [openPanel runModal];
    free( path );

    if( i_returnValue == NSOKButton )
    {
        NSUInteger c = 0;
        if( !p_input ) return;

        c = [[openPanel URLs] count];

        for (int i = 0; i < c ; i++)
        {
            msg_Dbg( VLCIntf, "loading subs from %s", [[[[openPanel URLs] objectAtIndex: i] path] UTF8String] );
            if( input_AddSubtitle( p_input, [[[[openPanel URLs] objectAtIndex: i] path] UTF8String], TRUE ) )
                msg_Warn( VLCIntf, "unable to load subtitles from '%s'",
                         [[[[openPanel URLs] objectAtIndex: i] path] UTF8String] );
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
            int64_t timeInSec = 0;
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
