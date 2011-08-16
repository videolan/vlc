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
#import "open.h"
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
    {
        o_fspanel = [[VLCFSPanel alloc] init];
        _o_sharedInstance = [super init];
    }

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
    [o_dropzone_btn setTitle: _NS("Open media...")];
    [o_dropzone_lbl setStringValue: _NS("Drop media here")];

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
    [self setDelegate: self];
    [self setExcludedFromWindowsMenu: YES];
    // Set that here as IB seems to be buggy
    [self setContentMinSize:NSMakeSize(500., 288.)];
    [self setTitle: _NS("VLC media player")];
    [o_playlist_btn setEnabled:NO];
    [o_video_view setFrame: [o_split_view frame]];
    o_temp_view = [[NSView alloc] init];
    [o_temp_view setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
    [o_dropzone_view setFrame: [o_playlist_table frame]];
    if (NSAppKitVersionNumber >= 1115.2)
        [self setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary];

    /* create the sidebar */
    o_sidebaritems = [[NSMutableArray alloc] init];
    SideBarItem *libraryItem = [SideBarItem itemWithTitle:_NS("LIBRARY") identifier:@"library"];
    SideBarItem *playlistItem = [SideBarItem itemWithTitle:_NS("Playlist") identifier:@"playlist"];
    [playlistItem setIcon: [NSImage imageNamed:@"document-music-playlist"]];
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
    NSString *o_identifier;
    for (; *ppsz_name; ppsz_name++, ppsz_longname++, p_category++)
    {
        o_identifier = [NSString stringWithCString: *ppsz_name encoding: NSUTF8StringEncoding];
        o_identifier = [[o_identifier componentsSeparatedByString:@"{"] objectAtIndex:0];
        switch (*p_category) {
            case SD_CAT_INTERNET:
                {
                    [internetItems addObject: [SideBarItem itemWithTitle: [NSString stringWithCString: *ppsz_longname encoding: NSUTF8StringEncoding] identifier: o_identifier]];
                    if (!strncmp( *ppsz_name, "podcast", 7 ))
                        [[internetItems lastObject] setIcon: [NSImage imageNamed:@"film-cast"]];
                    else
                        [[internetItems lastObject] setIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
                }
                break;
            case SD_CAT_DEVICES:
                {
                    [devicesItems addObject: [SideBarItem itemWithTitle: [NSString stringWithCString: *ppsz_longname encoding: NSUTF8StringEncoding] identifier: o_identifier]];
                    [[devicesItems lastObject] setIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
                }
                break;
            case SD_CAT_LAN:
                {
                    [lanItems addObject: [SideBarItem itemWithTitle: [NSString stringWithCString: *ppsz_longname encoding: NSUTF8StringEncoding] identifier: o_identifier]];
                    [[lanItems lastObject] setIcon: [NSImage imageNamed:@"network-cloud"]];
                }
                break;
            case SD_CAT_MYCOMPUTER:
                {
                    [mycompItems addObject: [SideBarItem itemWithTitle: [NSString stringWithCString: *ppsz_longname encoding: NSUTF8StringEncoding] identifier: o_identifier]];
                    if (!strncmp( *ppsz_name, "video_dir", 9 ))
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"film"]];
                    else if (!strncmp( *ppsz_name, "audio_dir", 9 ))
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"music-beam"]];
                    else if (!strncmp( *ppsz_name, "picture_dir", 11 ))
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"picture"]];
                    else
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
                }
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

    [o_sidebar_view reloadData];
    [o_sidebar_view selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:YES];
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
    if (!b_nonembedded)
    {
        if ([o_video_view isHidden] && [o_playlist_btn isEnabled]) {
            [o_playlist_table setHidden: YES];
            [o_video_view setHidden: NO];
        }
        else
        {
            [o_video_view setHidden: YES];
            [o_playlist_table setHidden: NO];
        }
    }
    else
    {
        [o_playlist_table setHidden: NO];
        [o_video_view setHidden: ![[VLCMain sharedInstance] activeVideoPlayback]];
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
        [o_fspanel setStreamPos: f_updated andTime: o_time];
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
    [[VLCCoreInteraction sharedInstance] toggleFullscreen];
}

- (IBAction)dropzoneButtonAction:(id)sender
{
    [[[VLCMain sharedInstance] open] openFileGeneric];
}

#pragma mark -
#pragma mark Update interface and respond to foreign events
- (void)showDropZone
{
    [o_right_split_view addSubview: o_dropzone_view];
    [o_dropzone_view setFrame: [o_playlist_table frame]];
    [[o_playlist_table animator] setHidden:YES];
}

- (void)hideDropZone
{
    [o_dropzone_view removeFromSuperview];
    [[o_playlist_table animator] setHidden: NO];
}

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
        [o_fspanel setStreamPos: f_updated andTime: o_time];
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
        [o_fspanel setVolumeLevel: (float)i_lastShownVolume / i_volume_step];
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

        NSURL * o_url = [NSURL URLWithString: [NSString stringWithUTF8String: uri]];
        if ([o_url isFileURL])
            [self setRepresentedURL: o_url];
        else
            [self setRepresentedURL: nil];
        free( uri );

        if ([aString isEqualToString:@""])
        {
            if ([o_url isFileURL])
                aString = [[NSFileManager defaultManager] displayNameAtPath: [o_url path]];
            else
                aString = [o_url absoluteString];
        }

        [self setTitle: aString];
        [o_fspanel setStreamTitle: aString];
    }
    else
    {
        [self setTitle: _NS("VLC media player")];
        [self setRepresentedURL: nil];
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
    [o_fspanel setSeekable: b_seekable];

    PL_LOCK;
    if (playlist_CurrentSize( p_playlist ) >= 1)
        [self hideDropZone];
    else
        [self showDropZone];
    PL_UNLOCK;
}

- (void)setPause
{
    [o_play_btn setImage: o_pause_img];
    [o_play_btn setAlternateImage: o_pause_pressed_img];
    [o_play_btn setToolTip: _NS("Pause")];
    [o_fspanel setPause];
}

- (void)setPlay
{
    [o_play_btn setImage: o_play_img];
    [o_play_btn setAlternateImage: o_play_pressed_img];
    [o_play_btn setToolTip: _NS("Play")];
    [o_fspanel setPlay];
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
    vout_thread_t *p_vout = getVout();
    if (config_GetInt( VLCIntf, "embedded-video" ))
    {
        if ([o_video_view window] != self)
        {
            [o_video_view removeFromSuperviewWithoutNeedingDisplay];
            [o_video_view setFrame: [o_split_view frame]];
            [[self contentView] addSubview:o_video_view positioned:NSWindowAbove relativeTo:nil];
        }
        b_nonembedded = NO;
    }
    else
    {
        [o_video_view removeFromSuperviewWithoutNeedingDisplay];
        if (o_nonembedded_window)
            [o_nonembedded_window release];

        o_nonembedded_window = [[VLCWindow alloc] initWithContentRect:[o_video_view frame] styleMask: NSBorderlessWindowMask|NSResizableWindowMask backing:NSBackingStoreBuffered defer:YES];
        [o_nonembedded_window setFrame:[o_video_view frame] display:NO];
        [o_nonembedded_window setBackgroundColor: [NSColor blackColor]];
        [o_nonembedded_window setMovableByWindowBackground: YES];
        [o_nonembedded_window setCanBecomeKeyWindow: YES];
        [o_nonembedded_window setHasShadow:YES];
        [o_nonembedded_window setContentView: o_video_view];
        [o_nonembedded_window setLevel:NSNormalWindowLevel];
        [o_nonembedded_window useOptimizedDrawing: YES];
        [o_nonembedded_window center];
        [o_nonembedded_window makeKeyAndOrderFront:self];
        [o_nonembedded_window orderFront:self animate:YES];
        [o_nonembedded_window setReleasedWhenClosed:NO];
        b_nonembedded = YES;
    }

    if (p_vout)
    {
        if( var_GetBool( p_vout, "video-on-top" ) )
            [[o_video_view window] setLevel: NSStatusWindowLevel];
        else
            [[o_video_view window] setLevel: NSNormalWindowLevel];
        vlc_object_release( p_vout );
    }
    return o_video_view;
}

- (void)setVideoplayEnabled
{
    if (!b_nonembedded)
        [o_playlist_btn setEnabled: [[VLCMain sharedInstance] activeVideoPlayback]];
    else
    {
        [o_playlist_btn setEnabled: NO];
        if (![[VLCMain sharedInstance] activeVideoPlayback])
            [o_nonembedded_window orderOut: nil];
    }
}

- (void)resizeWindow
{
    if ( !b_fullscreen && !(NSAppKitVersionNumber >= 1115.2 && [NSApp currentSystemPresentationOptions] == NSApplicationPresentationFullScreen) )
    {
        NSPoint topleftbase;
        NSPoint topleftscreen;
        NSRect new_frame;
        topleftbase.x = 0;
        topleftbase.y = [self frame].size.height;
        topleftscreen = [self convertBaseToScreen: topleftbase];

        /* Calculate the window's new size */
        new_frame.size.width = [self frame].size.width - [o_video_view frame].size.width + nativeVideoSize.width;
        new_frame.size.height = [self frame].size.height - [o_video_view frame].size.height + nativeVideoSize.height;

        new_frame.origin.x = topleftscreen.x;
        new_frame.origin.y = topleftscreen.y - new_frame.size.height;

        [[self animator] setFrame:new_frame display:YES];
    }
}

- (void)setNativeVideoSize:(NSSize)size
{
    if (size.width != nativeVideoSize.width || size.height != nativeVideoSize.height )
    {
        nativeVideoSize = size;
        [self resizeWindow];
    }
}

#pragma mark -
#pragma mark Fullscreen support
- (void)showFullscreenController
{
    if (b_fullscreen)
        [o_fspanel fadeIn];
}

- (BOOL)isFullscreen
{
    return b_fullscreen;
}

- (void)lockFullscreenAnimation
{
    [o_animation_lock lock];
}

- (void)unlockFullscreenAnimation
{
    [o_animation_lock unlock];
}

- (void)enterFullscreen
{
    NSMutableDictionary *dict1, *dict2;
    NSScreen *screen;
    NSRect screen_rect;
    NSRect rect;
    vout_thread_t *p_vout = getVout();
    BOOL blackout_other_displays = config_GetInt( VLCIntf, "macosx-black" );

    if( p_vout )
        screen = [NSScreen screenWithDisplayID:(CGDirectDisplayID)var_GetInteger( p_vout, "video-device" )];

    [self lockFullscreenAnimation];

    if (!screen)
    {
        msg_Dbg( VLCIntf, "chosen screen isn't present, using current screen for fullscreen mode" );
        screen = [self screen];
    }
    if (!screen)
    {
        msg_Dbg( VLCIntf, "Using deepest screen" );
        screen = [NSScreen deepestScreen];
    }

    if( p_vout )
        vlc_object_release( p_vout );

    screen_rect = [screen frame];

    [o_fullscreen_btn setState: YES];

    [NSCursor setHiddenUntilMouseMoves: YES];

    if( blackout_other_displays )
        [screen blackoutOtherScreens];

    /* Make sure we don't see the window flashes in float-on-top mode */
    i_originalLevel = [self level];
    [self setLevel:NSNormalWindowLevel];

    /* Only create the o_fullscreen_window if we are not in the middle of the zooming animation */
    if (!o_fullscreen_window)
    {
        /* We can't change the styleMask of an already created NSWindow, so we create another window, and do eye catching stuff */

        rect = [[o_video_view superview] convertRect: [o_video_view frame] toView: nil]; /* Convert to Window base coord */
        rect.origin.x += [self frame].origin.x;
        rect.origin.y += [self frame].origin.y;
        o_fullscreen_window = [[VLCWindow alloc] initWithContentRect:rect styleMask: NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
        [o_fullscreen_window setBackgroundColor: [NSColor blackColor]];
        [o_fullscreen_window setCanBecomeKeyWindow: YES];

        if (![self isVisible] || [self alphaValue] == 0.0)
        {
            /* We don't animate if we are not visible, instead we
             * simply fade the display */
            CGDisplayFadeReservationToken token;

            if( blackout_other_displays )
            {
                CGAcquireDisplayFadeReservation( kCGMaxDisplayReservationInterval, &token );
                CGDisplayFade( token, 0.5, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );
            }

            if ([screen isMainScreen])
            {
                if (NSAppKitVersionNumber < 1038) // Leopard
                    SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
                else
                    [NSApp setPresentationOptions:(NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar)];
            }

            [[o_video_view superview] replaceSubview:o_video_view with:o_temp_view];
            [o_temp_view setFrame:[o_video_view frame]];
            [o_fullscreen_window setContentView:o_video_view];

            [o_fullscreen_window makeKeyAndOrderFront:self];
            [o_fullscreen_window orderFront:self animate:YES];

            [o_fullscreen_window setFrame:screen_rect display:YES animate:YES];
            [o_fullscreen_window setLevel:NSNormalWindowLevel];

            if( blackout_other_displays )
            {
                CGDisplayFade( token, 0.3, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO );
                CGReleaseDisplayFadeReservation( token );
            }

            /* Will release the lock */
            [self hasBecomeFullscreen];

            return;
        }

        /* Make sure we don't see the o_video_view disappearing of the screen during this operation */
        NSDisableScreenUpdates();
        [[o_video_view superview] replaceSubview:o_video_view with:o_temp_view];
        [o_temp_view setFrame:[o_video_view frame]];
        [o_fullscreen_window setContentView:o_video_view];
        [o_fullscreen_window makeKeyAndOrderFront:self];
        NSEnableScreenUpdates();
    }

    /* We are in fullscreen (and no animation is running) */
    if (b_fullscreen)
    {
        /* Make sure we are hidden */
        [super orderOut: self];
        [self unlockFullscreenAnimation];
        return;
    }

    if (o_fullscreen_anim1)
    {
        [o_fullscreen_anim1 stopAnimation];
        [o_fullscreen_anim1 release];
    }
    if (o_fullscreen_anim2)
    {
        [o_fullscreen_anim2 stopAnimation];
        [o_fullscreen_anim2 release];
    }

    if ([screen isMainScreen])
    {
        if (NSAppKitVersionNumber < 1038) // Leopard
            SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
        else
            [NSApp setPresentationOptions:(NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar)];
    }

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:2];
    dict2 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:self forKey:NSViewAnimationTargetKey];
    [dict1 setObject:NSViewAnimationFadeOutEffect forKey:NSViewAnimationEffectKey];

    [dict2 setObject:o_fullscreen_window forKey:NSViewAnimationTargetKey];
    [dict2 setObject:[NSValue valueWithRect:[o_fullscreen_window frame]] forKey:NSViewAnimationStartFrameKey];
    [dict2 setObject:[NSValue valueWithRect:screen_rect] forKey:NSViewAnimationEndFrameKey];

    /* Strategy with NSAnimation allocation:
     - Keep at most 2 animation at a time
     - leaveFullscreen/enterFullscreen are the only responsible for releasing and alloc-ing
     */
    o_fullscreen_anim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict1]];
    o_fullscreen_anim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict2]];

    [dict1 release];
    [dict2 release];

    [o_fullscreen_anim1 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim1 setDuration: 0.3];
    [o_fullscreen_anim1 setFrameRate: 30];
    [o_fullscreen_anim2 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim2 setDuration: 0.2];
    [o_fullscreen_anim2 setFrameRate: 30];

    [o_fullscreen_anim2 setDelegate: self];
    [o_fullscreen_anim2 startWhenAnimation: o_fullscreen_anim1 reachesProgress: 1.0];

    [o_fullscreen_anim1 startAnimation];
    /* fullscreenAnimation will be unlocked when animation ends */
}

- (void)hasBecomeFullscreen
{
    [o_fullscreen_window makeFirstResponder: o_video_view];

    [o_fullscreen_window makeKeyWindow];
    [o_fullscreen_window setAcceptsMouseMovedEvents: TRUE];

    /* tell the fspanel to move itself to front next time it's triggered */
    [o_fspanel setVoutWasUpdated: (int)[[o_fullscreen_window screen] displayID]];
    [o_fspanel setActive: nil];

    if([self isVisible])
        [super orderOut: self];

    [o_fspanel setActive: nil];

    b_fullscreen = YES;
    [self unlockFullscreenAnimation];
}

- (void)leaveFullscreen
{
    [self leaveFullscreenAndFadeOut: NO];
}

- (void)leaveFullscreenAndFadeOut: (BOOL)fadeout
{
    NSMutableDictionary *dict1, *dict2;
    NSRect frame;
    BOOL blackout_other_displays = config_GetInt( VLCIntf, "macosx-black" );

    [self lockFullscreenAnimation];

    b_fullscreen = NO;
    [o_fullscreen_btn setState: NO];

    /* We always try to do so */
    [NSScreen unblackoutScreens];
    vout_thread_t *p_vout = getVout();
    if (p_vout)
    {
        if( var_GetBool( p_vout, "video-on-top" ) )
            [[o_video_view window] setLevel: NSStatusWindowLevel];
        else
            [[o_video_view window] setLevel: NSNormalWindowLevel];
        vlc_object_release( p_vout );
    }
    [[o_video_view window] makeKeyAndOrderFront: nil];

    /* Don't do anything if o_fullscreen_window is already closed */
    if (!o_fullscreen_window)
    {
        [self unlockFullscreenAnimation];
        return;
    }

    if (fadeout)
    {
        /* We don't animate if we are not visible, instead we
         * simply fade the display */
        CGDisplayFadeReservationToken token;

        if( blackout_other_displays )
        {
            CGAcquireDisplayFadeReservation( kCGMaxDisplayReservationInterval, &token );
            CGDisplayFade( token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );
        }

        [o_fspanel setNonActive: nil];
        if (NSAppKitVersionNumber < 1038) // Leopard
            SetSystemUIMode( kUIModeNormal, kUIOptionAutoShowMenuBar);
        else
            [NSApp setPresentationOptions:(NSApplicationPresentationDefault)];

        /* Will release the lock */
        [self hasEndedFullscreen];

        /* Our window is hidden, and might be faded. We need to workaround that, so note it
         * here */
        b_window_is_invisible = YES;

        if( blackout_other_displays )
        {
            CGDisplayFade( token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO );
            CGReleaseDisplayFadeReservation( token );
        }

        return;
    }

    [self setAlphaValue: 0.0];
    [self orderFront: self];
    [[o_video_view window] orderFront: self];

    [o_fspanel setNonActive: nil];
    if (NSAppKitVersionNumber < 1038) // Leopard
        SetSystemUIMode( kUIModeNormal, kUIOptionAutoShowMenuBar);
    else
        [NSApp setPresentationOptions:(NSApplicationPresentationDefault)];

    if (o_fullscreen_anim1)
    {
        [o_fullscreen_anim1 stopAnimation];
        [o_fullscreen_anim1 release];
    }
    if (o_fullscreen_anim2)
    {
        [o_fullscreen_anim2 stopAnimation];
        [o_fullscreen_anim2 release];
    }

    frame = [[o_temp_view superview] convertRect: [o_temp_view frame] toView: nil]; /* Convert to Window base coord */
    frame.origin.x += [self frame].origin.x;
    frame.origin.y += [self frame].origin.y;

    dict2 = [[NSMutableDictionary alloc] initWithCapacity:2];
    [dict2 setObject:self forKey:NSViewAnimationTargetKey];
    [dict2 setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];

    o_fullscreen_anim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict2, nil]];
    [dict2 release];

    [o_fullscreen_anim2 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim2 setDuration: 0.3];
    [o_fullscreen_anim2 setFrameRate: 30];

    [o_fullscreen_anim2 setDelegate: self];

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:o_fullscreen_window forKey:NSViewAnimationTargetKey];
    [dict1 setObject:[NSValue valueWithRect:[o_fullscreen_window frame]] forKey:NSViewAnimationStartFrameKey];
    [dict1 setObject:[NSValue valueWithRect:frame] forKey:NSViewAnimationEndFrameKey];

    o_fullscreen_anim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict1, nil]];
    [dict1 release];

    [o_fullscreen_anim1 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim1 setDuration: 0.2];
    [o_fullscreen_anim1 setFrameRate: 30];
    [o_fullscreen_anim2 startWhenAnimation: o_fullscreen_anim1 reachesProgress: 1.0];

    /* Make sure o_fullscreen_window is the frontmost window */
    [o_fullscreen_window orderFront: self];

    [o_fullscreen_anim1 startAnimation];
    /* fullscreenAnimation will be unlocked when animation ends */
}

- (void)hasEndedFullscreen
{
    /* This function is private and should be only triggered at the end of the fullscreen change animation */
    /* Make sure we don't see the o_video_view disappearing of the screen during this operation */
    NSDisableScreenUpdates();
    [o_video_view retain];
    [o_video_view removeFromSuperviewWithoutNeedingDisplay];
    [[o_temp_view superview] replaceSubview:o_temp_view with:o_video_view];
    [o_video_view release];
    [o_video_view setFrame:[o_temp_view frame]];
    [self makeFirstResponder: o_video_view];
    if ([self isVisible])
        [super makeKeyAndOrderFront:self]; /* our version contains a workaround */
    [o_fullscreen_window orderOut: self];
    NSEnableScreenUpdates();

    [o_fullscreen_window release];
    o_fullscreen_window = nil;
    [self setLevel:i_originalLevel];

    [self unlockFullscreenAnimation];
}

- (void)animationDidEnd:(NSAnimation*)animation
{
    NSArray *viewAnimations;
    if( o_makekey_anim == animation )
    {
        [o_makekey_anim release];
        return;
    }
    if ([animation currentValue] < 1.0)
        return;

    /* Fullscreen ended or started (we are a delegate only for leaveFullscreen's/enterFullscren's anim2) */
    viewAnimations = [o_fullscreen_anim2 viewAnimations];
    if ([viewAnimations count] >=1 &&
        [[[viewAnimations objectAtIndex: 0] objectForKey: NSViewAnimationEffectKey] isEqualToString:NSViewAnimationFadeInEffect])
    {
        /* Fullscreen ended */
        [self hasEndedFullscreen];
    }
    else
    {
        /* Fullscreen started */
        [self hasBecomeFullscreen];
    }
}

- (void)orderOut: (id)sender
{
    [super orderOut: sender];

    /* Make sure we leave fullscreen */
    [self leaveFullscreenAndFadeOut: YES];
}

- (void)makeKeyAndOrderFront: (id)sender
{
    /* Hack
     * when we exit fullscreen and fade out, we may endup in
     * having a window that is faded. We can't have it fade in unless we
     * animate again. */

    if(!b_window_is_invisible)
    {
        /* Make sure we don't do it too much */
        [super makeKeyAndOrderFront: sender];
        return;
    }

    [super setAlphaValue:0.0f];
    [super makeKeyAndOrderFront: sender];

    NSMutableDictionary * dict = [[NSMutableDictionary alloc] initWithCapacity:2];
    [dict setObject:self forKey:NSViewAnimationTargetKey];
    [dict setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];

    o_makekey_anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict]];
    [dict release];

    [o_makekey_anim setAnimationBlockingMode: NSAnimationNonblocking];
    [o_makekey_anim setDuration: 0.1];
    [o_makekey_anim setFrameRate: 30];
    [o_makekey_anim setDelegate: self];

    [o_makekey_anim startAnimation];
    b_window_is_invisible = NO;

    /* fullscreenAnimation will be unlocked when animation ends */
}

/* Make sure setFrame gets executed on main thread especially if we are animating.
 * (Thus we won't block the video output thread) */
- (void)setFrame:(NSRect)frame display:(BOOL)display animate:(BOOL)animate
{
    struct { NSRect frame; BOOL display; BOOL animate;} args;
    NSData *packedargs;

    args.frame = frame;
    args.display = display;
    args.animate = animate;

    packedargs = [NSData dataWithBytes:&args length:sizeof(args)];

    [self performSelectorOnMainThread:@selector(setFrameOnMainThread:)
                           withObject: packedargs waitUntilDone: YES];
}

- (void)setFrameOnMainThread:(NSData*)packedargs
{
    struct args { NSRect frame; BOOL display; BOOL animate; } * args = (struct args*)[packedargs bytes];

    if( args->animate )
    {
        /* Make sure we don't block too long and set up a non blocking animation */
        NSDictionary * dict = [NSDictionary dictionaryWithObjectsAndKeys:
                               self, NSViewAnimationTargetKey,
                               [NSValue valueWithRect:[self frame]], NSViewAnimationStartFrameKey,
                               [NSValue valueWithRect:args->frame], NSViewAnimationEndFrameKey, nil];

        NSViewAnimation * anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict]];
        [dict release];

        [anim setAnimationBlockingMode: NSAnimationNonblocking];
        [anim setDuration: 0.4];
        [anim setFrameRate: 30];
        [anim startAnimation];
    }
    else {
        [super setFrame:args->frame display:args->display animate:args->animate];
    }
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
