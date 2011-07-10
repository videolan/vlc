/*****************************************************************************
 * CoreInteraction.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011 Felix Paul Kühne
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import "CoreInteraction.h"
#import "intf.h"
#import "vout.h"
#import "open.h"
#import <vlc_playlist.h>
#import <vlc_input.h>
#import <vlc_keys.h>
#import <vlc_osd.h>

@implementation VLCCoreInteraction
static VLCCoreInteraction *_o_sharedInstance = nil;

+ (VLCCoreInteraction *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

#pragma mark -
#pragma mark Initialization

- (id)init
{
    if( _o_sharedInstance)
    {
        [self dealloc];
        return _o_sharedInstance;
    }
    else
    {
        _o_sharedInstance = [super init];
        b_lockAspectRatio = YES;
    }
    
    return _o_sharedInstance;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [super dealloc];
}

- (void)awakeFromNib
{
    [[NSNotificationCenter defaultCenter] addObserver: self 
                                             selector: @selector(applicationWillFinishLaunching:)
                                                 name: NSApplicationWillFinishLaunchingNotification
                                               object: nil];
}

#pragma mark -
#pragma mark Playback Controls

- (void)play
{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    bool empty;
    
    PL_LOCK;
    empty = playlist_IsEmpty( p_playlist );
    PL_UNLOCK;
    
    if( empty )
        [[[VLCMain sharedInstance] open] openFileGeneric];
    
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_PLAY_PAUSE );
}

- (void)stop
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_STOP );
    /* Close the window directly, because we do know that there
     * won't be anymore video. It's currently waiting a bit. */
    [[[self voutView] window] orderOut:self];
}

- (void)faster
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_FASTER );
}

- (void)slower
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_SLOWER );
}

- (void)normalSpeed
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_RATE_NORMAL );
}

- (void)previous
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_PREV );
}

- (void)next
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_NEXT );
}

- (void)forward
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_SHORT );
}

- (void)backward
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_SHORT );
}

- (void)shuffle
{
    vlc_value_t val;
    playlist_t * p_playlist = pl_Get( VLCIntf );
    
    var_Get( p_playlist, "random", &val );
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "random", val );
    if( val.b_bool )
    {
        //vout_OSDMessage( VLCIntf, SPU_DEFAULT_CHANNEL, "%s", _( "Random On" ) );
        config_PutInt( p_playlist, "random", 1 );
    }
    else
    {
        //vout_OSDMessage( VLCIntf, SPU_DEFAULT_CHANNEL, "%s", _( "Random Off" ) );
        config_PutInt( p_playlist, "random", 0 );
    }
    
    VLCIntf->p_sys->b_playmode_update = true;
    VLCIntf->p_sys->b_intf_update = true;
}

- (void)repeatAll
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    var_SetBool( p_playlist, "repeat", NO );
    var_SetBool( p_playlist, "loop", YES );
    config_PutInt( p_playlist, "repeat", NO );
    config_PutInt( p_playlist, "loop", YES );

    //vout_OSDMessage( VLCIntf, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat All" ) );

    VLCIntf->p_sys->b_playmode_update = true;
    VLCIntf->p_sys->b_intf_update = true;
}

- (void)repeatOne
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    var_SetBool( p_playlist, "repeat", YES );
    var_SetBool( p_playlist, "loop", NO );
    config_PutInt( p_playlist, "repeat", YES );
    config_PutInt( p_playlist, "loop", NO );

    //vout_OSDMessage( VLCIntf, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat One" ) );

    VLCIntf->p_sys->b_playmode_update = true;
    VLCIntf->p_sys->b_intf_update = true;
}

- (void)repeatOff
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    var_SetBool( p_playlist, "repeat", NO );
    var_SetBool( p_playlist, "loop", NO );
    config_PutInt( p_playlist, "repeat", NO );
    config_PutInt( p_playlist, "loop", NO );

    //vout_OSDMessage( VLCIntf, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat Off" ) );

    VLCIntf->p_sys->b_playmode_update = true;
    VLCIntf->p_sys->b_intf_update = true;
}

// CAVE: [o_main manageVolumeSlider]

- (void)volumeUp
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_VOL_UP );
}

- (void)volumeDown
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_VOL_DOWN );
}

- (void)mute
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_VOL_MUTE );
}

- (void)setVolume: (int)i_value
{
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( VLCIntf );
    audio_volume_t i_volume = (audio_volume_t)i_value;
    int i_volume_step;

    i_volume_step = config_GetInt( VLCIntf->p_libvlc, "volume-step" );
    aout_VolumeSet( p_playlist, i_volume * i_volume_step );
}

#pragma mark -
#pragma mark video output stuff

- (void)setAspectRatioLocked:(BOOL)b_value
{
    b_lockAspectRatio = b_value;
}

- (BOOL)aspectRatioIsLocked
{
    return b_lockAspectRatio;
}

- (void)toggleFullscreen
{
    input_thread_t * p_input = pl_CurrentInput( VLCIntf );
    
    if( p_input != NULL )
    {
        vout_thread_t *p_vout = input_GetVout( p_input );
        if( p_vout != NULL )
        {
            id o_vout_view = [self voutView];
            if( o_vout_view )
                [o_vout_view toggleFullscreen];
            vlc_object_release( p_vout );
        }
        else
        {
            playlist_t * p_playlist = pl_Get( VLCIntf );
            var_ToggleBool( p_playlist, "fullscreen" );
        }
        vlc_object_release( p_input );
    }
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
@end
