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
#import "misc.h"
#import "controls.h" // TODO: remove me
#import "SideBarItem.h"
#import <vlc_playlist.h>
#import <vlc_aout_intf.h>
#import <vlc_url.h>
#import <vlc_strings.h>
#import <vlc_services_discovery.h>

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
//     styleMask ^= NSTexturedBackgroundWindowMask;

    self = [super initWithContentRect:contentRect styleMask:styleMask //& ~NSTitledWindowMask
                              backing:backingType defer:flag];

    [[VLCMain sharedInstance] updateTogglePlaylistState];

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
    [o_sidebaritems release];
    [super dealloc];
}

- (void)awakeFromNib
{
    /* setup the styled interface */
    b_dark_interface = config_GetInt( VLCIntf, "macosx-interfacestyle" );
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

    if (!b_dark_interface) {
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
        [o_time_sld_left_view setImage: [NSImage imageNamed:@"progression-track-wrapper-left"]];
        [o_time_sld_middle_view setImage: [NSImage imageNamed:@"progression-track-wrapper-middle"]];
        [o_time_sld_right_view setImage: [NSImage imageNamed:@"progression-track-wrapper-right"]];
        [o_volume_down_btn setImage: [NSImage imageNamed:@"volume-low"]];
        [o_volume_track_view setImage: [NSImage imageNamed:@"volume-slider-track"]];
        [o_volume_up_btn setImage: [NSImage imageNamed:@"volume-high"]];
        [o_effects_btn setImage: [NSImage imageNamed:@"effects-double-buttons"]];
        [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-double-buttons-pressed"]];
        [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-double-buttons"]];
        [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-double-buttons-pressed"]];
        [o_time_sld_fancygradient_view loadImagesInDarkStyle:NO];
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
        [o_time_sld_fancygradient_view loadImagesInDarkStyle:YES];
    }
    [o_repeat_btn setImage: o_repeat_img];
    [o_repeat_btn setAlternateImage: o_repeat_pressed_img];
    [o_shuffle_btn setImage: o_shuffle_img];
    [o_shuffle_btn setAlternateImage: o_shuffle_pressed_img];
    [o_play_btn setImage: o_play_img];
    [o_play_btn setAlternateImage: o_play_pressed_img];

    /* interface builder action */
    [o_video_view setFrame: [o_split_view frame]];
    [self setDelegate: self];
    [self setExcludedFromWindowsMenu: YES];
    // Set that here as IB seems to be buggy
    [self setContentMinSize:NSMakeSize(500., 200.)];
    [self setTitle: _NS("VLC media player")];
    [o_playlist_btn setEnabled:NO];

    /* reset the interface */
    [self updateVolumeSlider];
    [self updateTimeSlider];

    /* create the sidebar */
    o_sidebaritems = [[NSMutableArray alloc] init];
    SideBarItem *libraryItem = [SideBarItem itemWithTitle:_NS("LIBRARY") identifier:@"library"];
    SideBarItem *playlistItem = [SideBarItem itemWithTitle:_NS("Playlist") identifier:@"playlist"];
    SideBarItem *mycompItem = [SideBarItem itemWithTitle:_NS("MY COMPUTER") identifier:@"mycomputer"];
    SideBarItem *devicesItem = [SideBarItem itemWithTitle:_NS("DEVICES") identifier:@"devices"];
    SideBarItem *lanItem = [SideBarItem itemWithTitle:_NS("LOCAL NETWORK") identifier:@"localnetwork"];
    SideBarItem *internetItem = [SideBarItem itemWithTitle:_NS("INTERNET") identifier:@"internet"];

    /* SD subnodes, inspired by the Qt4 intf */
    char **ppsz_longnames;
    int *p_categories;
    char **ppsz_names = vlc_sd_GetNames( pl_Get( VLCIntf ), &ppsz_longnames, &p_categories );
    if (!ppsz_names)
        msg_Err( VLCIntf, "no sd item found" ); //TODO
    char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
    int *p_category = p_categories;
    NSMutableArray *internetItems = [[NSMutableArray alloc] init];
    NSMutableArray *devicesItems = [[NSMutableArray alloc] init];
    NSMutableArray *lanItems = [[NSMutableArray alloc] init];
    NSMutableArray *mycompItems = [[NSMutableArray alloc] init];
    for (; *ppsz_name; ppsz_name++, ppsz_longname++, p_category++)
    {
        switch (*p_category) {
            case SD_CAT_INTERNET:
                [internetItems addObject: [SideBarItem itemWithTitle: [NSString stringWithCString: *ppsz_longname encoding: NSUTF8StringEncoding] identifier: [NSString stringWithCString: *ppsz_name encoding: NSUTF8StringEncoding]]];
                break;
            case SD_CAT_DEVICES:
                [devicesItems addObject: [SideBarItem itemWithTitle: [NSString stringWithCString: *ppsz_longname encoding: NSUTF8StringEncoding] identifier: [NSString stringWithCString: *ppsz_name encoding: NSUTF8StringEncoding]]];
                break;
            case SD_CAT_LAN:
                [lanItems addObject: [SideBarItem itemWithTitle: [NSString stringWithCString: *ppsz_longname encoding: NSUTF8StringEncoding] identifier: [NSString stringWithCString: *ppsz_name encoding: NSUTF8StringEncoding]]];
                break;
            case SD_CAT_MYCOMPUTER:
                [mycompItems addObject: [SideBarItem itemWithTitle: [NSString stringWithCString: *ppsz_longname encoding: NSUTF8StringEncoding] identifier: [NSString stringWithCString: *ppsz_name encoding: NSUTF8StringEncoding]]];
                break;
            default:
                msg_Warn( VLCIntf, "unknown SD type found, skipping (%s)", *ppsz_name );
                break;
        }

        free( *ppsz_name );
        free( *ppsz_longname );
    }
    [mycompItem setChildren: [NSArray arrayWithArray: mycompItems]];
    [devicesItem setChildren: [NSArray arrayWithArray: devicesItems]];
    [lanItem setChildren: [NSArray arrayWithArray: lanItems]];
    [internetItem setChildren: [NSArray arrayWithArray: internetItems]];
    [mycompItems release];
    [devicesItems release];
    [lanItems release];
    [internetItems release];
    free( ppsz_names );
    free( ppsz_longnames );
    free( p_categories );

    [libraryItem setChildren: [NSArray arrayWithObject: playlistItem]];
    [o_sidebaritems addObject: libraryItem];
    if ([mycompItem hasChildren])
        [o_sidebaritems addObject: mycompItem];
    if ([devicesItem hasChildren])
        [o_sidebaritems addObject: devicesItem];
    if ([lanItem hasChildren])
        [o_sidebaritems addObject: lanItem];
    if ([internetItem hasChildren])
        [o_sidebaritems addObject: internetItem];

    msg_Dbg( VLCIntf, "side bar should contain %lu items", [o_sidebaritems count] );
    [o_sidebar_view reloadData];
}

#pragma mark -
#pragma mark Button Actions

- (IBAction)play:(id)sender
{
    [[VLCCoreInteraction sharedInstance] play];
}

- (void)resetPreviousButton
{
    if (([NSDate timeIntervalSinceReferenceDate] - last_bwd_event) >= 0.35) {
        // seems like no further event occured, so let's switch the playback item
        [[VLCCoreInteraction sharedInstance] previous];
        just_triggered_previous = NO;
    }
}

- (void)resetBackwardSkip
{
    // the user stopped skipping, so let's allow him to change the item
    if (([NSDate timeIntervalSinceReferenceDate] - last_bwd_event) >= 0.35)
        just_triggered_previous = NO;
}

- (IBAction)bwd:(id)sender
{
    if(!just_triggered_previous)
    {
        just_triggered_previous = YES;
        [self performSelector:@selector(resetPreviousButton)
                   withObject: NULL
                   afterDelay:0.40];
    }
    else
    {
        if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) > 0.12 )
        {
            // we just skipped 3 "continous" events, otherwise we are too fast
            [[VLCCoreInteraction sharedInstance] backward];
            last_bwd_event = [NSDate timeIntervalSinceReferenceDate];
            [self performSelector:@selector(resetBackwardSkip)
                       withObject: NULL
                       afterDelay:0.40];
        }
    }
}

- (void)resetNextButton
{
    if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) >= 0.35) {
        // seems like no further event occured, so let's switch the playback item
        [[VLCCoreInteraction sharedInstance] next];
        just_triggered_next = NO;
    }
}

- (void)resetForwardSkip
{
    // the user stopped skipping, so let's allow him to change the item
    if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) >= 0.35)
        just_triggered_next = NO;
}

- (IBAction)fwd:(id)sender
{
   if(!just_triggered_next)
    {
        just_triggered_next = YES;
        [self performSelector:@selector(resetNextButton)
                   withObject: NULL
                   afterDelay:0.40];
    }
    else
    {
        if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) > 0.12 )
        {
            // we just skipped 3 "continous" events, otherwise we are too fast
            [[VLCCoreInteraction sharedInstance] forward];
            last_fwd_event = [NSDate timeIntervalSinceReferenceDate];
            [self performSelector:@selector(resetForwardSkip)
                       withObject: NULL
                       afterDelay:0.40];
        }
    }
}

- (IBAction)stop:(id)sender
{
    [[VLCCoreInteraction sharedInstance] stop];
}

- (IBAction)togglePlaylist:(id)sender
{
    if ([o_video_view isHidden] && [o_playlist_btn isEnabled]) {
        [o_video_view setHidden: NO];
        [o_playlist_table setHidden: YES];
    }
    else
    {
        [o_playlist_table setHidden: NO];
        [o_video_view setHidden: YES];
    }
}

- (void)setRepeatOne
{
    [o_repeat_btn setImage: o_repeat_one_img];
    [o_repeat_btn setAlternateImage: o_repeat_one_pressed_img];   
}

- (void)setRepeatAll
{
    [o_repeat_btn setImage: o_repeat_all_img];
    [o_repeat_btn setAlternateImage: o_repeat_all_pressed_img];
}

- (void)setRepeatOff
{
    [o_repeat_btn setImage: o_repeat_img];
    [o_repeat_btn setAlternateImage: o_repeat_pressed_img];
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
        [self setRepeatOne];
    }
    else if( repeating.b_bool && !looping.b_bool )
    {
        /* was: Repeat One, switching to Repeat All */
        [[VLCCoreInteraction sharedInstance] repeatAll];
        [self setRepeatAll];
    }
    else
    {
        /* was: Repeat All or bug in VLC, switching to Repeat Off */
        [[VLCCoreInteraction sharedInstance] repeatOff];
        [self setRepeatOff];
    }
}

- (void)setShuffle
{
    bool b_value;
    playlist_t *p_playlist = pl_Get( VLCIntf );
    b_value = var_GetBool( p_playlist, "random" );
	if(b_value) {
        [o_shuffle_btn setImage: o_shuffle_on_img];
        [o_shuffle_btn setAlternateImage: o_shuffle_on_pressed_img];
    }
    else
    {
        [o_shuffle_btn setImage: o_shuffle_img];
        [o_shuffle_btn setAlternateImage: o_shuffle_pressed_img];
    }
}

- (IBAction)shuffle:(id)sender
{
    [[VLCCoreInteraction sharedInstance] shuffle];
    [self setShuffle];
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
        if( [o_time_fld timeRemaining] && dur != -1 )
        {
            o_time = [NSString stringWithFormat: @"-%s", secstotimestr( psz_time, ((dur - time.i_time) / 1000000) )];
        }
        else
            o_time = [NSString stringWithUTF8String: secstotimestr( psz_time, (time.i_time / 1000000) )];

        [o_time_fld setStringValue: o_time];
        [[[[VLCMain sharedInstance] controls] fspanel] setStreamPos: f_updated andTime: o_time];
        vlc_object_release( p_input );
    }
    [self drawFancyGradientEffectForTimeSlider];
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
        if( [o_time_fld timeRemaining] && dur != -1 )
        {
            o_time = [NSString stringWithFormat: @"-%s", secstotimestr( psz_time, ((dur - time.i_time) / 1000000))];
        }
        else
            o_time = [NSString stringWithUTF8String: secstotimestr( psz_time, (time.i_time / 1000000) )];

        if (dur == -1) {
            [o_time_sld setEnabled: NO];
            [o_time_sld setHidden: YES];
        } else {
            [o_time_sld setEnabled: YES];
            [o_time_sld setHidden: NO];
        }

        [o_time_fld setStringValue: o_time];
        [o_time_fld setNeedsDisplay:YES];
//        [[[[VLCMain sharedInstance] controls] fspanel] setStreamPos: f_updated andTime: o_time];
        vlc_object_release( p_input );
    }
    else
    {
        [o_time_sld setFloatValue: 0.0];
        [o_time_fld setStringValue: @"00:00"];
        [o_time_sld setEnabled: NO];
        [o_time_sld setHidden: YES];
    }
        
    [self performSelectorOnMainThread:@selector(drawFancyGradientEffectForTimeSlider) withObject:nil waitUntilDone:NO];
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

- (void)updateName
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    input_thread_t * p_input;
    p_input = pl_CurrentInput( VLCIntf );
    if( p_input )
    {
        NSString *aString;
        char *format = var_InheritString( VLCIntf, "input-title-format" );
        char *formated = str_format_meta( p_input, format );
        free( format );
        aString = [NSString stringWithUTF8String:formated];
        free( formated );

        char *uri = input_item_GetURI( input_GetItem( p_input ) );

        if ([aString isEqualToString:@""])
        {

            char *file = uri ? strrchr( uri, '/' ) : NULL;
            if( file != NULL )
            {
                decode_URI( ++file );
                aString = [NSString stringWithUTF8String:file];
            }
            else
                aString = [NSString stringWithUTF8String:uri];
        }

        NSMutableString *o_mrl = [NSMutableString stringWithUTF8String: decode_URI(uri)];
        free( uri );
        NSRange prefix_range = [o_mrl rangeOfString: @"file:"];
        if( prefix_range.location != NSNotFound )
            [o_mrl deleteCharactersInRange: prefix_range];

        if( [o_mrl characterAtIndex:0] == '/' )
        {
            /* it's a local file */
            [self setRepresentedFilename: o_mrl];
        }
        else
        {
            /* it's from the network or somewhere else,
             * we clear the previous path */
            [self setRepresentedFilename: @""];
        }

        [self setTitle: aString];
        [[[[VLCMain sharedInstance] controls] fspanel] setStreamTitle: aString];
    }
    else
    {
        [self setTitle: _NS("VLC media player")];
        [self setRepresentedFilename: @""];
    }
    [o_pool release];
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


    [o_time_sld setEnabled: b_seekable];
    [self updateTimeSlider];
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

- (void)drawFancyGradientEffectForTimeSlider
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    float f_value = ([o_time_sld_middle_view frame].size.width -5) * ([o_time_sld intValue] / [o_time_sld maxValue]);
    if (f_value > 5.0)
    {
        if (f_value != [o_time_sld_fancygradient_view frame].size.width)
        {
            [o_time_sld_fancygradient_view setHidden: NO];
            [o_time_sld_fancygradient_view setFrame: NSMakeRect( [o_time_sld_fancygradient_view frame].origin.x, [o_time_sld_fancygradient_view frame].origin.y, f_value, [o_time_sld_fancygradient_view frame].size.height )];
            [o_time_sld_fancygradient_view setNeedsDisplay:YES];
            [o_time_sld_middle_view setNeedsDisplay:YES];
        }
    }
    else
    {
        [o_time_sld_fancygradient_view setHidden: YES];
    }
    [o_pool release];
}

#pragma mark -
#pragma mark Video Output handling

- (id)videoView
{
    return o_video_view;
}

- (void)setVideoplayEnabled
{
    [o_playlist_btn setEnabled: [[VLCMain sharedInstance] activeVideoPlayback]];
}

#pragma mark -
#pragma mark Side Bar Data handling
/* taken under BSD-new from the PXSourceList sample project, adapted for VLC */
- (NSUInteger)sourceList:(PXSourceList*)sourceList numberOfChildrenOfItem:(id)item
{
	//Works the same way as the NSOutlineView data source: `nil` means a parent item
	if(item==nil) {
		return [o_sidebaritems count];
	}
	else {
		return [[item children] count];
	}
}


- (id)sourceList:(PXSourceList*)aSourceList child:(NSUInteger)index ofItem:(id)item
{
    //Works the same way as the NSOutlineView data source: `nil` means a parent item
	if(item==nil) {
		return [o_sidebaritems objectAtIndex:index];
	}
	else {
		return [[item children] objectAtIndex:index];
	}
}


- (id)sourceList:(PXSourceList*)aSourceList objectValueForItem:(id)item
{
	return [item title];
}

- (void)sourceList:(PXSourceList*)aSourceList setObjectValue:(id)object forItem:(id)item
{
	[item setTitle:object];
}

- (BOOL)sourceList:(PXSourceList*)aSourceList isItemExpandable:(id)item
{
	return [item hasChildren];
}


- (BOOL)sourceList:(PXSourceList*)aSourceList itemHasBadge:(id)item
{
	return [item hasBadge];
}


- (NSInteger)sourceList:(PXSourceList*)aSourceList badgeValueForItem:(id)item
{
	return [item badgeValue];
}


- (BOOL)sourceList:(PXSourceList*)aSourceList itemHasIcon:(id)item
{
	return [item hasIcon];
}


- (NSImage*)sourceList:(PXSourceList*)aSourceList iconForItem:(id)item
{
	return [item icon];
}

- (NSMenu*)sourceList:(PXSourceList*)aSourceList menuForEvent:(NSEvent*)theEvent item:(id)item
{
	if ([theEvent type] == NSRightMouseDown || ([theEvent type] == NSLeftMouseDown && ([theEvent modifierFlags] & NSControlKeyMask) == NSControlKeyMask)) {
		NSMenu * m = [[NSMenu alloc] init];
		if (item != nil)
			[m addItemWithTitle:[item title] action:nil keyEquivalent:@""];
		return [m autorelease];
	}
	return nil;
}

#pragma mark -
#pragma mark Side Bar Delegate Methods
/* taken under BSD-new from the PXSourceList sample project, adapted for VLC */
- (BOOL)sourceList:(PXSourceList*)aSourceList isGroupAlwaysExpanded:(id)group
{
	if([[group identifier] isEqualToString:@"library"])
		return YES;

	return NO;
}

- (void)sourceListSelectionDidChange:(NSNotification *)notification
{
	NSIndexSet *selectedIndexes = [o_sidebar_view selectedRowIndexes];

	//Set the label text to represent the new selection
    if([selectedIndexes count]==1) {
		NSString *title = [[o_sidebar_view itemAtRow:[selectedIndexes firstIndex]] title];

		[o_chosen_category_lbl setStringValue:title];
	}
	else {
		[o_chosen_category_lbl setStringValue:@"(none)"];
	}
}

@end

@implementation VLCProgressBarGradientEffect
- (void)dealloc
{
    [o_time_sld_gradient_left_img release];
    [o_time_sld_gradient_middle_img release];
    [o_time_sld_gradient_right_img release];
    [super dealloc];
}

- (void)loadImagesInDarkStyle: (BOOL)b_value
{
    if (b_value)
    {
        o_time_sld_gradient_left_img = [[NSImage imageNamed:@"progressbar-fill-left_dark"] retain];
        o_time_sld_gradient_middle_img = [[NSImage imageNamed:@"progressbar-fill-middle_dark"] retain];
        o_time_sld_gradient_right_img = [[NSImage imageNamed:@"progressbar-fill-right_dark"] retain];
    }
    else
    {
        o_time_sld_gradient_left_img = [[NSImage imageNamed:@"progression-fill-left"] retain];
        o_time_sld_gradient_middle_img = [[NSImage imageNamed:@"progression-fill-middle"] retain];
        o_time_sld_gradient_right_img = [[NSImage imageNamed:@"progression-fill-right"] retain];
    }
}

- (void)drawRect:(NSRect)rect
{
    NSRect bnds = [self bounds];
    NSDrawThreePartImage( bnds, o_time_sld_gradient_left_img, o_time_sld_gradient_middle_img, o_time_sld_gradient_right_img, NO, NSCompositeSourceOver, 1, NO );
}
@end
