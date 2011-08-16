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
#import "open.h"
#import <vlc_playlist.h>
#import <vlc_input.h>
#import <vlc_keys.h>
#import <vlc_osd.h>
#import <vlc_aout_intf.h>

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

- (void)setPlaybackRate:(int)i_value
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    double speed = pow( 2, (double)i_value / 17 );
    int rate = INPUT_RATE_DEFAULT / speed;
    if( i_currentPlaybackRate != rate )
        var_SetFloat( p_playlist, "rate", (float)INPUT_RATE_DEFAULT / (float)rate );
    i_currentPlaybackRate = rate;
}

- (int)playbackRate
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    float rate = var_GetFloat( p_playlist, "rate" );
    double value = 17 * log( rate ) / log( 2. );
    int returnValue = (int) ( ( value > 0 ) ? value + .5 : value - .5 );

    if( returnValue < -34 )
        returnValue = -34;
    else if( returnValue > 34 )
        returnValue = 34;

    i_currentPlaybackRate = returnValue;
    return returnValue;
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
    vout_thread_t *p_vout = getVout();

    var_Get( p_playlist, "random", &val );
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "random", val );
    if( val.b_bool )
    {
        if (p_vout)
        {
            vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Random On" ) );
            vlc_object_release( p_vout );
        }
        config_PutInt( p_playlist, "random", 1 );
    }
    else
    {
        if (p_vout)
        {
            vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Random Off" ) );
            vlc_object_release( p_vout );
        }
        config_PutInt( p_playlist, "random", 0 );
    }
}

- (void)repeatAll
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    var_SetBool( p_playlist, "repeat", NO );
    var_SetBool( p_playlist, "loop", YES );
    config_PutInt( p_playlist, "repeat", NO );
    config_PutInt( p_playlist, "loop", YES );

    vout_thread_t *p_vout = getVout();
    if (p_vout)
    {
        vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat All" ) );
        vlc_object_release( p_vout );
    }
}

- (void)repeatOne
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    var_SetBool( p_playlist, "repeat", YES );
    var_SetBool( p_playlist, "loop", NO );
    config_PutInt( p_playlist, "repeat", YES );
    config_PutInt( p_playlist, "loop", NO );

    vout_thread_t *p_vout = getVout();
    if (p_vout)
    {
        vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat One" ) );
        vlc_object_release( p_vout );
    }
}

- (void)repeatOff
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    var_SetBool( p_playlist, "repeat", NO );
    var_SetBool( p_playlist, "loop", NO );
    config_PutInt( p_playlist, "repeat", NO );
    config_PutInt( p_playlist, "loop", NO );

    vout_thread_t *p_vout = getVout();
    if (p_vout)
    {
        vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", _( "Repeat Off" ) );
        vlc_object_release( p_vout );
    }
}

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
    playlist_t * p_playlist = pl_Get( VLCIntf );
    var_ToggleBool( p_playlist, "fullscreen" );
}
@end
