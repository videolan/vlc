/*****************************************************************************
 * MainWindow.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2011 VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
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

#import "MainWindow.h"
#import "intf.h"
#import "CoreInteraction.h"
#import "AudioEffects.h"
#import "MainMenu.h"
#import "controls.h" // TODO: remove me
#import <vlc_playlist.h>
#import <vlc_aout_intf.h>

@implementation VLCMainWindow
static VLCMainWindow *_o_sharedInstance = nil;

+ (VLCMainWindow *)sharedInstance
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
        _o_sharedInstance = [super init];

    return _o_sharedInstance;
}

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask
                  backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    /* FIXME: this should enable the SnowLeopard window style, however, it leads to ugly artifacts
     *        needs some further investigation! -- feepk */
     BOOL b_useTextured = YES;

     if( [[NSWindow class] instancesRespondToSelector:@selector(setContentBorderThickness:forEdge:)] )
     {
     b_useTextured = NO;
     styleMask ^= NSTexturedBackgroundWindowMask;
     }

    self = [super initWithContentRect:contentRect styleMask:styleMask //& ~NSTitledWindowMask
                              backing:backingType defer:flag];

    [[VLCMain sharedInstance] updateTogglePlaylistState];

    // FIXME: see above...
    if(! b_useTextured )
     [self setContentBorderThickness:28.0 forEdge:NSMinYEdge];

    /* we want to be moveable regardless of our style */
    [self setMovableByWindowBackground: YES];

    /* we don't want this window to be restored on relaunch */
    if ([self respondsToSelector:@selector(setRestorable:)])
        [self setRestorable:NO];

    return self;
}

- (BOOL)performKeyEquivalent:(NSEvent *)o_event
{
    /* We indeed want to prioritize Cocoa key equivalent against libvlc,
     so we perform the menu equivalent now. */
    if([[NSApp mainMenu] performKeyEquivalent:o_event])
        return TRUE;

    return [[VLCMain sharedInstance] hasDefinedShortcutKey:o_event] || [(VLCControls *)[[VLCMain sharedInstance] controls] keyEvent:o_event];
}

- (void)dealloc
{
    config_PutInt( VLCIntf->p_libvlc, "volume", i_lastShownVolume );
    [super dealloc];
}

- (void)awakeFromNib
{
    b_gray_interface = YES; //TODO
    i_lastShownVolume = -1;

    [o_play_btn setToolTip: _NS("Play/Pause")];
    [o_bwd_btn setToolTip: _NS("Backward")];
    [o_fwd_btn setToolTip: _NS("Forward")];
    [o_stop_btn setToolTip: _NS("Stop")];
    [o_playlist_btn setToolTip: _NS("Show/Hide Playlist")];
    [o_repeat_btn setToolTip: _NS("Repeat")];
    [o_shuffle_btn setToolTip: _NS("Shuffle")];
    [o_effects_btn setToolTip: _NS("Effects")];
    [o_fullscreen_btn setToolTip: _NS("Toggle Fullscreen mode")];
    [[o_search_fld cell] setPlaceholderString: _NS("Search")];
    [o_volume_sld setToolTip: _NS("Volume")];
    [o_volume_down_btn setToolTip: _NS("Mute")];
    [o_volume_up_btn setToolTip: _NS("Full Volume")];
    [o_time_sld setToolTip: _NS("Position")];

    if (b_gray_interface) {
        [o_bottombar_view setImage: [NSImage imageNamed:@"bottom-background"]];
        [o_bwd_btn setImage: [NSImage imageNamed:@"back"]];
        [o_bwd_btn setAlternateImage: [NSImage imageNamed:@"back-pressed"]];
        o_play_img = [[NSImage imageNamed:@"play"] retain];
        o_play_pressed_img = [[NSImage imageNamed:@"play-pressed"] retain];
        o_pause_img = [[NSImage imageNamed:@"pause"] retain];
        o_pause_pressed_img = [[NSImage imageNamed:@"pause-pressed"] retain];
        [o_fwd_btn setImage: [NSImage imageNamed:@"forward"]];
        [o_fwd_btn setAlternateImage: [NSImage imageNamed:@"forward-pressed"]];
        [o_stop_btn setImage: [NSImage imageNamed:@"stop"]];
        [o_stop_btn setAlternateImage: [NSImage imageNamed:@"stop-pressed"]];
        [o_playlist_btn setImage: [NSImage imageNamed:@"playlist"]];
        [o_playlist_btn setAlternateImage: [NSImage imageNamed:@"playlist-pressed"]];
        o_repeat_img = [[NSImage imageNamed:@"repeat"] retain];
        o_repeat_pressed_img = [[NSImage imageNamed:@"repeat-pressed"] retain];
        o_repeat_all_img  = [[NSImage imageNamed:@"repeat-all"] retain];
        o_repeat_all_pressed_img = [[NSImage imageNamed:@"repeat-all-pressed"] retain];
        o_repeat_one_img = [[NSImage imageNamed:@"repeat-one"] retain];
        o_repeat_one_pressed_img = [[NSImage imageNamed:@"repeat-one-pressed"] retain];
        o_shuffle_img = [[NSImage imageNamed:@"shuffle"] retain];
        o_shuffle_pressed_img = [[NSImage imageNamed:@"shuffle-pressed"] retain];
        o_shuffle_on_img = [[NSImage imageNamed:@"shuffle-blue"] retain];
        o_shuffle_on_pressed_img = [[NSImage imageNamed:@"shuffle-blue-pressed"] retain];
        [o_time_fld setTextColor: [NSColor colorWithCalibratedRed:64.0 green:64.0 blue:64.0 alpha:100.0]];
        [o_time_sld_left_view setImage: [NSImage imageNamed:@"progression-track-wrapper-left"]];
        [o_time_sld_middle_view setImage: [NSImage imageNamed:@"progression-track-wrapper-middle"]];
        [o_time_sld_right_view setImage: [NSImage imageNamed:@"progression-track-wrapper-right"]];
        [o_volume_down_btn setImage: [NSImage imageNamed:@"volume-low"]];
        [o_volume_track_view setImage: [NSImage imageNamed:@"volumetrack"]];
        [o_volume_up_btn setImage: [NSImage imageNamed:@"volume-high"]];
        [o_effects_btn setImage: [NSImage imageNamed:@"effects-double-buttons"]];
        [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-double-buttons-pressed"]];
        [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-double-buttons"]];
        [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-double-buttons-pressed"]];
    }
    else
    {
        /* TODO: we also need to change the window style here... */
        [o_bottombar_view setImage: [NSImage imageNamed:@"bottom-background_dark"]];
        [o_bwd_btn setImage: [NSImage imageNamed:@"back_dark"]];
        [o_bwd_btn setAlternateImage: [NSImage imageNamed:@"back-pressed_dark"]];
        o_play_img = [[NSImage imageNamed:@"play_dark"] retain];
        o_play_pressed_img = [[NSImage imageNamed:@"play-pressed_dark"] retain];
        o_pause_img = [[NSImage imageNamed:@"pause_dark"] retain];
        o_pause_pressed_img = [[NSImage imageNamed:@"pause-pressed_dark"] retain];
        [o_fwd_btn setImage: [NSImage imageNamed:@"forward_dark"]];
        [o_fwd_btn setAlternateImage: [NSImage imageNamed:@"forward-pressed_dark"]];
        [o_stop_btn setImage: [NSImage imageNamed:@"stop_dark"]];
        [o_stop_btn setAlternateImage: [NSImage imageNamed:@"stop-pressed_dark"]];
        [o_playlist_btn setImage: [NSImage imageNamed:@"playlist_dark"]];
        [o_playlist_btn setAlternateImage: [NSImage imageNamed:@"playlist-pressed_dark"]];
        o_repeat_img = [[NSImage imageNamed:@"repeat_dark"] retain];
        o_repeat_pressed_img = [[NSImage imageNamed:@"repeat-pressed_dark"] retain];
        o_repeat_all_img  = [[NSImage imageNamed:@"repeat-all-blue_dark"] retain];
        o_repeat_all_pressed_img = [[NSImage imageNamed:@"repeat-all-blue-pressed_dark"] retain];
        o_repeat_one_img = [[NSImage imageNamed:@"repeat-one-blue_dark"] retain];
        o_repeat_one_pressed_img = [[NSImage imageNamed:@"repeat-one-blue-pressed_dark"] retain];
        o_shuffle_img = [[NSImage imageNamed:@"shuffle_dark"] retain];
        o_shuffle_pressed_img = [[NSImage imageNamed:@"shuffle-pressed_dark"] retain];
        o_shuffle_on_img = [[NSImage imageNamed:@"shuffle-blue_dark"] retain];
        o_shuffle_on_pressed_img = [[NSImage imageNamed:@"shuffle-blue-pressed_dark"] retain];
        [o_time_fld setTextColor: [NSColor colorWithCalibratedRed:229.0 green:229.0 blue:229.0 alpha:100.0]];
        [o_time_sld_left_view setImage: [NSImage imageNamed:@"progression-track-wrapper-left_dark"]];
        [o_time_sld_middle_view setImage: [NSImage imageNamed:@"progression-track-wrapper-middle_dark"]];
        [o_time_sld_right_view setImage: [NSImage imageNamed:@"progression-track-wrapper-right_dark"]];
        [o_volume_down_btn setImage: [NSImage imageNamed:@"volume-low_dark"]];
        [o_volume_track_view setImage: [NSImage imageNamed:@"volume-slider-track_dark"]];
        [o_volume_up_btn setImage: [NSImage imageNamed:@"volume-high_dark"]];
        [o_effects_btn setImage: [NSImage imageNamed:@"effects-double-buttons_dark"]];
        [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-double-buttons-pressed_dark"]];
        [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-double-buttons_dark"]];
        [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-double-buttons-pressed_dark"]];        
    }
    [o_repeat_btn setImage: o_repeat_img];
    [o_repeat_btn setAlternateImage: o_repeat_pressed_img];
    [o_shuffle_btn setImage: o_shuffle_img];
    [o_shuffle_btn setAlternateImage: o_shuffle_pressed_img];

    [o_play_btn setImage: o_play_img];
    [o_play_btn setAlternateImage: o_play_pressed_img];

    [o_video_view setFrame: [o_playlist_table frame]];
    [self setDelegate: self];
    [self setExcludedFromWindowsMenu: YES];
    // Set that here as IB seems to be buggy
    [self setContentMinSize:NSMakeSize(500., 200.)];
    [self setTitle: _NS("VLC media player")];
    [o_playlist_btn setEnabled:NO];

    [self updateVolumeSlider];
}

#pragma mark -
#pragma mark Button Actions

- (IBAction)play:(id)sender
{
    [[VLCCoreInteraction sharedInstance] play];
}

// TODO: we need more advanced handling for the next 2 actions to handling skipping as well
- (IBAction)bwd:(id)sender
{
    [[VLCCoreInteraction sharedInstance] previous];
}

- (IBAction)fwd:(id)sender
{
    [[VLCCoreInteraction sharedInstance] next];
}

- (IBAction)stop:(id)sender
{
    [[VLCCoreInteraction sharedInstance] stop];
}

- (IBAction)togglePlaylist:(id)sender
{
    NSLog( @"b_videoplayen %i", b_video_playback_enabled );
    if (b_video_playback_enabled && [o_video_view isHidden]) {
        [o_video_view setHidden: NO];
        [o_playlist_table setHidden: YES];
    }
    else
    {
        [o_playlist_table setHidden: NO];
        [o_video_view setHidden: YES];
    }
}

- (IBAction)repeat:(id)sender
{
    vlc_value_t looping,repeating;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get( p_intf );

    var_Get( p_playlist, "repeat", &repeating );
    var_Get( p_playlist, "loop", &looping );

    if( !repeating.b_bool && !looping.b_bool )
    {
        /* was: no repeating at all, switching to Repeat One */
        [[VLCCoreInteraction sharedInstance] repeatOne];

        [o_repeat_btn setImage: o_repeat_one_img];
        [o_repeat_btn setAlternateImage: o_repeat_one_pressed_img];
    }
    else if( repeating.b_bool && !looping.b_bool )
    {
        /* was: Repeat One, switching to Repeat All */
        [[VLCCoreInteraction sharedInstance] repeatAll];

        [o_repeat_btn setImage: o_repeat_all_img];
        [o_repeat_btn setAlternateImage: o_repeat_all_pressed_img];
    }
    else
    {
        /* was: Repeat All or bug in VLC, switching to Repeat Off */
        [[VLCCoreInteraction sharedInstance] repeatOff];

        [o_repeat_btn setImage: o_repeat_img];
        [o_repeat_btn setAlternateImage: o_repeat_pressed_img];
    }
}

- (IBAction)shuffle:(id)sender
{
    [[VLCCoreInteraction sharedInstance] shuffle];

    vlc_value_t val;
    playlist_t *p_playlist = pl_Get( VLCIntf );
    var_Get( p_playlist, "random", &val );
	if(val.b_bool) {
        [o_shuffle_btn setImage: o_shuffle_on_img];
        [o_shuffle_btn setAlternateImage: o_shuffle_on_pressed_img];
    }
    else
    {
        [o_shuffle_btn setImage: o_shuffle_img];
        [o_shuffle_btn setAlternateImage: o_shuffle_pressed_img];
    }
}

- (IBAction)timeSliderAction:(id)sender
{
    float f_updated;
    input_thread_t * p_input;

    switch( [[NSApp currentEvent] type] )
    {
        case NSLeftMouseUp:
        case NSLeftMouseDown:
        case NSLeftMouseDragged:
            f_updated = [sender floatValue];
            break;

        default:
            return;
    }
    p_input = pl_CurrentInput( VLCIntf );
    if( p_input != NULL )
    {
        vlc_value_t time;
        vlc_value_t pos;
        NSString * o_time;
        char psz_time[MSTRTIME_MAX_SIZE];

        pos.f_float = f_updated / 10000.;
        var_Set( p_input, "position", pos );
        [o_time_sld setFloatValue: f_updated];

        var_Get( p_input, "time", &time );

        mtime_t dur = input_item_GetDuration( input_GetItem( p_input ) );
        if( b_time_remaining && dur != -1 )
        {
            o_time = [NSString stringWithFormat: @"-%s", secstotimestr( psz_time, ((dur - time.i_time) / 1000000) )];
        }
        else
            o_time = [NSString stringWithUTF8String: secstotimestr( psz_time, (time.i_time / 1000000) )];

        [o_time_fld setStringValue: o_time];
        [[[[VLCMain sharedInstance] controls] fspanel] setStreamPos: f_updated andTime: o_time];
        vlc_object_release( p_input );
    }
}

- (IBAction)timeFieldWasClicked:(id)sender
{
    b_time_remaining = !b_time_remaining;
    NSLog( @"b_time_remaining %i", b_time_remaining );
}

- (IBAction)volumeAction:(id)sender
{
    if (sender == o_volume_sld)
        [[VLCCoreInteraction sharedInstance] setVolume: [sender intValue]];
    else if (sender == o_volume_down_btn)
        [[VLCCoreInteraction sharedInstance] mute];
    else
        [[VLCCoreInteraction sharedInstance] setVolume: 400];
}

- (IBAction)effects:(id)sender
{
    [[VLCMainMenu sharedInstance] showAudioEffects: sender];
}

- (IBAction)fullscreen:(id)sender
{
    NSLog( @"fullscreen mode not yet implemented" );
    [[VLCCoreInteraction sharedInstance] toggleFullscreen];
}

#pragma mark -
#pragma mark Update interface and respond to foreign events
- (void)updateTimeSlider
{
    if ([o_time_sld isEnabled])
    {
        input_thread_t * p_input;
        p_input = pl_CurrentInput( VLCIntf );
        if( p_input )
        {
            vlc_value_t time;
            NSString * o_time;
            vlc_value_t pos;
            char psz_time[MSTRTIME_MAX_SIZE];
            float f_updated;

            var_Get( p_input, "position", &pos );
            f_updated = 10000. * pos.f_float;
            [o_time_sld setFloatValue: f_updated];

            var_Get( p_input, "time", &time );

            mtime_t dur = input_item_GetDuration( input_GetItem( p_input ) );
            if( b_time_remaining && dur != -1 )
            {
                o_time = [NSString stringWithFormat: @"-%s", secstotimestr( psz_time, ((dur - time.i_time) / 1000000))];
            }
            else
                o_time = [NSString stringWithUTF8String: secstotimestr( psz_time, (time.i_time / 1000000) )];

            [o_time_fld setStringValue: o_time];
    //        [[[[VLCMain sharedInstance] controls] fspanel] setStreamPos: f_updated andTime: o_time];
        }
    }
}

- (void)updateVolumeSlider
{
    audio_volume_t i_volume;
    playlist_t * p_playlist = pl_Get( VLCIntf );

    i_volume = aout_VolumeGet( p_playlist );

    if( i_volume != i_lastShownVolume )
    {
        i_lastShownVolume = i_volume;
        int i_volume_step = 0;
        i_volume_step = config_GetInt( VLCIntf->p_libvlc, "volume-step" );
        [o_volume_sld setFloatValue: (float)i_lastShownVolume / i_volume_step];
//        [[[[VLCMain sharedInstance] controls] fspanel] setVolumeLevel: (float)i_lastShownVolume / i_volume_step];
    }
}

- (void)updateWindow
{
    bool b_input = false;
    bool b_plmul = false;
    bool b_control = false;
    bool b_seekable = false;
    bool b_chapters = false;

    playlist_t * p_playlist = pl_Get( VLCIntf );

    PL_LOCK;
    b_plmul = playlist_CurrentSize( p_playlist ) > 1;
    PL_UNLOCK;

    input_thread_t * p_input = playlist_CurrentInput( p_playlist );

    bool b_buffering = NO;

    if( ( b_input = ( p_input != NULL ) ) )
    {
        /* seekable streams */
        cachedInputState = input_GetState( p_input );
        if ( cachedInputState == INIT_S || cachedInputState == OPENING_S )
            b_buffering = YES;

        /* seekable streams */
        b_seekable = var_GetBool( p_input, "can-seek" );

        /* check whether slow/fast motion is possible */
        b_control = var_GetBool( p_input, "can-rate" );

        /* chapters & titles */
        //FIXME! b_chapters = p_input->stream.i_area_nb > 1;
        vlc_object_release( p_input );
    }

    if( b_buffering )
    {
        [o_progress_bar startAnimation:self];
        [o_progress_bar setIndeterminate:YES];
        [o_progress_bar setHidden:NO];
    } else {
        [o_progress_bar stopAnimation:self];
        [o_progress_bar setHidden:YES];
    }

    [o_stop_btn setEnabled: b_input];
    [o_fwd_btn setEnabled: (b_seekable || b_plmul || b_chapters)];
    [o_bwd_btn setEnabled: (b_seekable || b_plmul || b_chapters)];
    [[VLCMainMenu sharedInstance] setRateControlsEnabled: b_control];

    [o_time_sld setFloatValue: 0.0];
    [o_time_sld setEnabled: b_seekable];
    [o_time_fld setStringValue: @"00:00"];
    [[[[VLCMain sharedInstance] controls] fspanel] setStreamPos: 0 andTime: @"00:00"];
    [[[[VLCMain sharedInstance] controls] fspanel] setSeekable: b_seekable];
}

- (void)setPause
{
    [o_play_btn setImage: o_pause_img];
    [o_play_btn setAlternateImage: o_pause_pressed_img];
    [o_play_btn setToolTip: _NS("Pause")];
}

- (void)setPlay
{
    [o_play_btn setImage: o_play_img];
    [o_play_btn setAlternateImage: o_play_pressed_img];
    [o_play_btn setToolTip: _NS("Play")];
}

#pragma mark -
#pragma mark Video Output handling

- (id)videoView
{
    return o_video_view;
}

- (void)setVideoplayEnabled:(BOOL)b_value
{
    NSLog( @"setVideoplayEnabled:%i", b_value );
    if (b_value) {
        b_video_playback_enabled = YES;
        [o_playlist_btn setEnabled: YES];
    }
    else
    {
        b_video_playback_enabled = NO;
        [o_playlist_btn setEnabled: NO];
    }
}

@end