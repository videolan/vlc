/*****************************************************************************
 * CoreInteraction.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2012 Felix Paul Kühne
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
#import <vlc/vlc.h>
#import <vlc_strings.h>

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

- (void)pause
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_PAUSE );
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

- (void)toggleRecord
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    input_thread_t * p_input;
    p_input = pl_CurrentInput( p_intf );
    if( p_input )
    {
        var_ToggleBool( p_input, "record" );
        vlc_object_release( p_input );
    }
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
    float f_rate;

    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return 0;

    input_thread_t * p_input;
    p_input = pl_CurrentInput( p_intf );
    if (p_input)
    {
        f_rate = var_GetFloat( p_input, "rate" );
        vlc_object_release( p_input );
    }
    else
    {
        playlist_t * p_playlist = pl_Get( VLCIntf );
        f_rate = var_GetFloat( p_playlist, "rate" );
    }

    double value = 17 * log( f_rate ) / log( 2. );
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

- (BOOL)isPlaying
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return NO;

    input_thread_t * p_input = pl_CurrentInput( p_intf );

    if (!p_input) return NO;

    input_state_e i_state = ERROR_S;
    input_Control( p_input, INPUT_GET_STATE, &i_state);
    vlc_object_release( p_input );

    return ((i_state == OPENING_S) || (i_state == PLAYING_S));
}

- (int)currentTime
{
    input_thread_t * p_input = pl_CurrentInput( VLCIntf );
    int64_t i_currentTime = -1;

    if (!p_input) return i_currentTime;

    input_Control( p_input, INPUT_GET_TIME, &i_currentTime);
    vlc_object_release( p_input );

    return (int)( i_currentTime / 1000000 );
}

- (void)setCurrentTime:(int)i_value
{
    int64_t i64_value = (int64_t)i_value;
    input_thread_t * p_input = pl_CurrentInput( VLCIntf );

    if (!p_input) return;

    input_Control( p_input, INPUT_SET_TIME, (int64_t)(i64_value * 1000000));
    vlc_object_release( p_input );
}

- (int)durationOfCurrentPlaylistItem
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return 0;

    input_thread_t * p_input = pl_CurrentInput( p_intf );
    int64_t i_duration = -1;
    if (!p_input) return i_duration;


    input_Control( p_input, INPUT_GET_LENGTH, &i_duration);
    vlc_object_release( p_input );

    return (int)(i_duration / 1000000);
}

- (NSURL*)URLOfCurrentPlaylistItem
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return nil;

    input_thread_t *p_input = pl_CurrentInput( p_intf );
    if (!p_input) return nil;

    input_item_t *p_item = input_GetItem( p_input );
    if (!p_item)
    {
        vlc_object_release( p_input );
        return nil;
    }

    char *psz_uri = input_item_GetURI( p_item );
    if (!psz_uri)
    {
        vlc_object_release( p_input );
        return nil;
    }

    NSURL *o_url;
    o_url = [NSURL URLWithString:[NSString stringWithUTF8String:psz_uri]];
    vlc_object_release( p_input );

    return o_url;
}

- (NSString*)nameOfCurrentPlaylistItem
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return nil;

    input_thread_t *p_input = pl_CurrentInput( p_intf );
    if (!p_input) return nil;

    input_item_t *p_item = input_GetItem( p_input );
    if (!p_item)
    {
        vlc_object_release( p_input );
        return nil;
    }

    char *psz_uri = input_item_GetURI( p_item );
    if (!psz_uri)
    {
        vlc_object_release( p_input );
        return nil;
    }

    NSString *o_name;
    char *format = var_InheritString( VLCIntf, "input-title-format" );
    char *formated = str_format_meta( p_input, format );
    free( format );
    o_name = [NSString stringWithUTF8String:formated];
    free( formated );

    NSURL * o_url = [NSURL URLWithString: [NSString stringWithUTF8String: psz_uri]];
    free( psz_uri );

    if ([o_name isEqualToString:@""])
    {
        if ([o_url isFileURL]) 
            o_name = [[NSFileManager defaultManager] displayNameAtPath: [o_url path]];
        else
            o_name = [o_url absoluteString];
    }
    vlc_object_release( p_input );
    return o_name;
}

- (void)forward
{
    //LEGACY SUPPORT
    [self forwardShort];
}

- (void)backward
{
    //LEGACY SUPPORT
    [self backwardShort];
}

- (void)forwardExtraShort
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_EXTRASHORT );
}

- (void)backwardExtraShort
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_EXTRASHORT );
}

- (void)forwardShort
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_SHORT );
}

- (void)backwardShort
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_SHORT );
}

- (void)forwardMedium
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_MEDIUM );
}

- (void)backwardMedium
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_MEDIUM );
}

- (void)forwardLong
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_LONG );
}

- (void)backwardLong
{
    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_LONG );
}

- (void)shuffle
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    vlc_value_t val;
    playlist_t * p_playlist = pl_Get( p_intf );
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
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    playlist_t * p_playlist = pl_Get( p_intf );

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
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    playlist_t * p_playlist = pl_Get( p_intf );

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
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    playlist_t * p_playlist = pl_Get( p_intf );

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

- (void)displayVolume
{
    vout_thread_t *p_vout = getVout();
    if (p_vout)
    {
        vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, _( "Volume %d%%" ),
                       [self volume]*100/AOUT_VOLUME_DEFAULT );
        vlc_object_release( p_vout );
    }
}

- (void)volumeUp
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    aout_VolumeUp( pl_Get( p_intf ), 1, NULL );
    [self displayVolume];
}

- (void)volumeDown
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    aout_VolumeDown( pl_Get( p_intf ), 1, NULL );
    [self displayVolume];
}

- (void)mute
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    aout_ToggleMute( pl_Get( p_intf ), NULL );

    vout_thread_t *p_vout = getVout();
    if( p_vout )
    {
        if( [self isMuted] )
        {
            vout_OSDIcon( p_vout, SPU_DEFAULT_CHANNEL, OSD_MUTE_ICON );
        }
        else
            [self displayVolume];

        vlc_object_release( p_vout );
    }
}

- (BOOL)isMuted
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return NO;

    BOOL b_is_muted = NO;
    b_is_muted = aout_IsMuted( VLC_OBJECT(pl_Get( p_intf )) );

    return b_is_muted;
}

- (int)volume
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return 0;

    audio_volume_t i_volume = aout_VolumeGet( pl_Get( p_intf ) );

    return (int)i_volume;
}

- (void)setVolume: (int)i_value
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    aout_VolumeSet( pl_Get( p_intf ), i_value );
}

#pragma mark -
#pragma mark video output stuff

- (void)setAspectRatioLocked:(BOOL)b_value
{
    config_PutInt( VLCIntf, "macosx-lock-aspect-ratio", b_value );
}

- (BOOL)aspectRatioIsLocked
{
    return config_GetInt( VLCIntf, "macosx-lock-aspect-ratio" );
}

- (void)toggleFullscreen
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    var_ToggleBool( pl_Get( p_intf ), "fullscreen" );
}

@end
