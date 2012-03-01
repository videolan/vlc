/*****************************************************************************
 * MainWindow.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
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

#import "CompatibilityFixes.h"
#import "MainWindow.h"
#import "intf.h"
#import "CoreInteraction.h"
#import "AudioEffects.h"
#import "MainMenu.h"
#import "open.h"
#import "controls.h" // TODO: remove me
#import "playlist.h"
#import "SideBarItem.h"
#import <vlc_playlist.h>
#import <vlc_aout_intf.h>
#import <vlc_url.h>
#import <vlc_strings.h>
#import <vlc_services_discovery.h>
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
    {
        o_fspanel = [[VLCFSPanel alloc] init];
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask
                  backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    b_dark_interface = config_GetInt( VLCIntf, "macosx-interfacestyle" );

    if (b_dark_interface)
    {
#ifdef MAC_OS_X_VERSION_10_7
        if (OSX_LION)
            styleMask = NSBorderlessWindowMask | NSResizableWindowMask;
        else
            styleMask = NSBorderlessWindowMask;
#else
        styleMask = NSBorderlessWindowMask;
#endif
    }

    self = [super initWithContentRect:contentRect styleMask:styleMask
                              backing:backingType defer:flag];

    [[VLCMain sharedInstance] updateTogglePlaylistState];

    /* we want to be moveable regardless of our style */
    [self setMovableByWindowBackground: YES];

    /* we don't want this window to be restored on relaunch */
    if (OSX_LION)
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
    if (b_dark_interface)
        [o_color_backdrop release];

    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [o_sidebaritems release];
    [super dealloc];
}

- (void)awakeFromNib
{
    BOOL b_splitviewShouldBeHidden = NO;
    /* setup the styled interface */
    b_nativeFullscreenMode = NO;
#ifdef MAC_OS_X_VERSION_10_7
    if( config_GetInt( VLCIntf, "embedded-video" ))
        b_nativeFullscreenMode = config_GetInt( VLCIntf, "macosx-nativefullscreenmode" );
#endif
    i_lastShownVolume = -1;
    t_hide_mouse_timer = nil;

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
        [o_bottombar_view setImagesLeft: [NSImage imageNamed:@"bottom-background"] middle: [NSImage imageNamed:@"bottom-background"] right: [NSImage imageNamed:@"bottom-background"]];
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
        [o_time_sld_background setImagesLeft: [NSImage imageNamed:@"progression-track-wrapper-left"] middle: [NSImage imageNamed:@"progression-track-wrapper-middle"] right: [NSImage imageNamed:@"progression-track-wrapper-right"]];
        [o_volume_down_btn setImage: [NSImage imageNamed:@"volume-low"]];
        [o_volume_track_view setImage: [NSImage imageNamed:@"volume-slider-track"]];
        [o_volume_up_btn setImage: [NSImage imageNamed:@"volume-high"]];
        if (OSX_LION && b_nativeFullscreenMode)
        {
            [o_effects_btn setImage: [NSImage imageNamed:@"effects-one-button"]];
            [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-one-button-blue"]];
        }
        else
        {
            [o_effects_btn setImage: [NSImage imageNamed:@"effects-double-buttons"]];
            [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-double-buttons-pressed"]];
        }
        [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-double-buttons"]];
        [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-double-buttons-pressed"]];
        [o_time_sld_fancygradient_view setImagesLeft:[NSImage imageNamed:@"progression-fill-left"] middle:[NSImage imageNamed:@"progression-fill-middle"] right:[NSImage imageNamed:@"progression-fill-right"]];
    }
    else
    {
        [o_bottombar_view setImagesLeft: [NSImage imageNamed:@"bottomdark-left"] middle: [NSImage imageNamed:@"bottom-background_dark"] right: [NSImage imageNamed:@"bottomdark-right"]];
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
        [o_time_sld_background setImagesLeft: [NSImage imageNamed:@"progression-track-wrapper-left_dark"] middle: [NSImage imageNamed:@"progression-track-wrapper-middle_dark"] right: [NSImage imageNamed:@"progression-track-wrapper-right_dark"]];
        [o_volume_down_btn setImage: [NSImage imageNamed:@"volume-low_dark"]];
        [o_volume_track_view setImage: [NSImage imageNamed:@"volume-slider-track_dark"]];
        [o_volume_up_btn setImage: [NSImage imageNamed:@"volume-high_dark"]];
        if (OSX_LION && b_nativeFullscreenMode)
        {
            [o_effects_btn setImage: [NSImage imageNamed:@"effects-one-button_dark"]];
            [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-one-button-blue_dark"]];
        }
        else
        {
            [o_effects_btn setImage: [NSImage imageNamed:@"effects-double-buttons_dark"]];
            [o_effects_btn setAlternateImage: [NSImage imageNamed:@"effects-double-buttons-pressed_dark"]];
        }
        [o_fullscreen_btn setImage: [NSImage imageNamed:@"fullscreen-double-buttons_dark"]];
        [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"fullscreen-double-buttons-pressed_dark"]];
        [o_time_sld_fancygradient_view setImagesLeft:[NSImage imageNamed:@"progressbar-fill-left_dark"] middle:[NSImage imageNamed:@"progressbar-fill-middle_dark"] right:[NSImage imageNamed:@"progressbar-fill-right_dark"]];
    }
    [o_repeat_btn setImage: o_repeat_img];
    [o_repeat_btn setAlternateImage: o_repeat_pressed_img];
    [o_shuffle_btn setImage: o_shuffle_img];
    [o_shuffle_btn setAlternateImage: o_shuffle_pressed_img];
    [o_play_btn setImage: o_play_img];
    [o_play_btn setAlternateImage: o_play_pressed_img];
    BOOL b_mute = ![[VLCCoreInteraction sharedInstance] isMuted];
    [o_volume_sld setEnabled: b_mute];
    [o_volume_up_btn setEnabled: b_mute];

    /* interface builder action */
    if ([self frame].size.height < 100)
        b_splitviewShouldBeHidden = YES;
    [self setDelegate: self];
    [self setExcludedFromWindowsMenu: YES];
    [self setAcceptsMouseMovedEvents: YES];
    // Set that here as IB seems to be buggy
    if (b_dark_interface)
        [self setContentMinSize:NSMakeSize(604., (288. + [o_titlebar_view frame].size.height))];
    else
        [self setContentMinSize:NSMakeSize(604., 288.)];
    [self setTitle: _NS("VLC media player")];
    [o_time_fld setAlignment: NSCenterTextAlignment];
    [o_time_fld setNeedsDisplay:YES];
    b_dropzone_active = YES;
    o_temp_view = [[NSView alloc] init];
    [o_temp_view setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
    [o_dropzone_view setFrame: [o_playlist_table frame]];
    [o_left_split_view setFrame: [o_sidebar_view frame]];
    if (OSX_LION && b_nativeFullscreenMode)
    {
        NSRect frame;
        [self setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary];
        float f_width = [o_fullscreen_btn frame].size.width;

        #define moveItem( item ) \
        frame = [item frame]; \
        frame.origin.x = f_width + frame.origin.x; \
        [item setFrame: frame]

        moveItem( o_effects_btn );
        moveItem( o_volume_up_btn );
        moveItem( o_volume_sld );
        moveItem( o_volume_track_view );
        moveItem( o_volume_down_btn );
        moveItem( o_time_fld );
        #undef moveItem

        #define enlargeItem( item ) \
        frame = [item frame]; \
        frame.size.width = f_width + frame.size.width; \
        [item setFrame: frame]

        enlargeItem( o_time_sld );
        enlargeItem( o_progress_bar );
        enlargeItem( o_time_sld_background );
        enlargeItem( o_time_sld_fancygradient_view );
        #undef enlargeItem

        [o_fullscreen_btn removeFromSuperviewWithoutNeedingDisplay];
    }
    else
        [o_titlebar_view setFullscreenButtonHidden: YES];

    if (OSX_LION)
    {
        /* the default small size of the search field is slightly different on Lion, let's work-around that */
        NSRect frame;
        frame = [o_search_fld frame];
        frame.origin.y = frame.origin.y + 2.0;
        frame.size.height = frame.size.height - 1.0;
        [o_search_fld setFrame: frame];
    }

    /* create the sidebar */
    o_sidebaritems = [[NSMutableArray alloc] init];
    SideBarItem *libraryItem = [SideBarItem itemWithTitle:_NS("LIBRARY") identifier:@"library"];
    SideBarItem *playlistItem = [SideBarItem itemWithTitle:_NS("Playlist") identifier:@"playlist"];
    [playlistItem setIcon: [NSImage imageNamed:@"sidebar-playlist"]];
    SideBarItem *medialibraryItem = [SideBarItem itemWithTitle:_NS("Media Library") identifier:@"medialibrary"];
    [medialibraryItem setIcon: [NSImage imageNamed:@"sidebar-playlist"]];
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
        switch (*p_category) {
            case SD_CAT_INTERNET:
                {
                    [internetItems addObject: [SideBarItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                    if (!strncmp( *ppsz_name, "podcast", 7 ))
                        [internetItems removeLastObject]; // we don't support podcasts at this point (see #6017)
//                        [[internetItems lastObject] setIcon: [NSImage imageNamed:@"sidebar-podcast"]];
                    else
                        [[internetItems lastObject] setIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
                    [[internetItems lastObject] setSdtype: SD_CAT_INTERNET];
                    [[internetItems lastObject] setUntranslatedTitle: [NSString stringWithUTF8String: *ppsz_longname]];
                }
                break;
            case SD_CAT_DEVICES:
                {
                    [devicesItems addObject: [SideBarItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                    [[devicesItems lastObject] setIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
                    [[devicesItems lastObject] setSdtype: SD_CAT_DEVICES];
                    [[devicesItems lastObject] setUntranslatedTitle: [NSString stringWithUTF8String: *ppsz_longname]];
                }
                break;
            case SD_CAT_LAN:
                {
                    [lanItems addObject: [SideBarItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                    [[lanItems lastObject] setIcon: [NSImage imageNamed:@"sidebar-local"]];
                    [[lanItems lastObject] setSdtype: SD_CAT_LAN];
                    [[lanItems lastObject] setUntranslatedTitle: [NSString stringWithUTF8String: *ppsz_longname]];
                }
                break;
            case SD_CAT_MYCOMPUTER:
                {
                    [mycompItems addObject: [SideBarItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                    if (!strncmp( *ppsz_name, "video_dir", 9 ))
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"sidebar-movie"]];
                    else if (!strncmp( *ppsz_name, "audio_dir", 9 ))
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"sidebar-music"]];
                    else if (!strncmp( *ppsz_name, "picture_dir", 11 ))
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"sidebar-pictures"]];
                    else
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
                    [[mycompItems lastObject] setUntranslatedTitle: [NSString stringWithUTF8String: *ppsz_longname]];
                    [[mycompItems lastObject] setSdtype: SD_CAT_MYCOMPUTER];
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

    [libraryItem setChildren: [NSArray arrayWithObjects: playlistItem, medialibraryItem, nil]];
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
    NSUInteger i_sidebaritem_count = [o_sidebaritems count];
    for (NSUInteger x = 0; x < i_sidebaritem_count; x++)
        [o_sidebar_view expandItem: [o_sidebaritems objectAtIndex: x] expandChildren: YES];
    [o_sidebar_view selectRowIndexes:[NSIndexSet indexSetWithIndex:1] byExtendingSelection:NO];

    if( b_dark_interface )
    {
        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowResizedOrMoved:) name: NSWindowDidResizeNotification object: nil];
        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowResizedOrMoved:) name: NSWindowDidMoveNotification object: nil];

        [self setBackgroundColor: [NSColor clearColor]];
        [self setOpaque: NO];
        [self display];
        [self setHasShadow:NO];
        [self setHasShadow:YES];

        NSRect winrect = [self frame];
        CGFloat f_titleBarHeight = [o_titlebar_view frame].size.height;

        [o_titlebar_view setFrame: NSMakeRect( 0, winrect.size.height - f_titleBarHeight,
                                              winrect.size.width, f_titleBarHeight )];
        [[self contentView] addSubview: o_titlebar_view positioned: NSWindowAbove relativeTo: o_split_view];

        if (winrect.size.height > 100)
        {
            [self setFrame: winrect display:YES animate:YES];
            previousSavedFrame = winrect;
        }

        winrect = [o_split_view frame];
        winrect.size.height = winrect.size.height - f_titleBarHeight;
        [o_split_view setFrame: winrect];
        [o_video_view setFrame: winrect];

        o_color_backdrop = [[VLCColorView alloc] initWithFrame: [o_split_view frame]];
        [[self contentView] addSubview: o_color_backdrop positioned: NSWindowBelow relativeTo: o_split_view];
        [o_color_backdrop setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
    }
    else
    {
        NSRect frame;
        frame = [o_time_sld_fancygradient_view frame];
        frame.size.height = frame.size.height - 1;
        frame.origin.y = frame.origin.y + 1;
        [o_time_sld_fancygradient_view setFrame: frame];

        [o_video_view setFrame: [o_split_view frame]];
        [o_playlist_table setBorderType: NSNoBorder];
        [o_sidebar_scrollview setBorderType: NSNoBorder];
    }

    NSRect frame;
    frame = [o_time_sld_fancygradient_view frame];
    frame.size.width = 0;
    [o_time_sld_fancygradient_view setFrame: frame];

    if (OSX_LION)
        [o_resize_view setImage: NULL];

    if ([self styleMask] & NSResizableWindowMask)
        [o_resize_view removeFromSuperviewWithoutNeedingDisplay];

    if (OSX_LEOPARD)
        [o_time_sld_fancygradient_view removeFromSuperviewWithoutNeedingDisplay];

    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(someWindowWillClose:) name: NSWindowWillCloseNotification object: nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(someWindowWillMiniaturize:) name: NSWindowWillMiniaturizeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(applicationWillTerminate:) name: NSApplicationWillTerminateNotification object: nil];
    [[VLCMain sharedInstance] playbackModeUpdated];

    if (b_splitviewShouldBeHidden)
    {
        i_lastSplitViewHeight = [o_split_view frame].size.height;
        [self hideSplitView];
    }
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
        // seems like no further event occurred, so let's switch the playback item
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
        if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) > 0.16 )
        {
            // we just skipped 4 "continous" events, otherwise we are too fast
            [[VLCCoreInteraction sharedInstance] backwardExtraShort];
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
        // seems like no further event occurred, so let's switch the playback item
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
        if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) > 0.16 )
        {
            // we just skipped 4 "continous" events, otherwise we are too fast
            [[VLCCoreInteraction sharedInstance] forwardExtraShort];
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

- (void)resizePlaylistAfterCollapse
{
    NSRect plrect;
    plrect = [[o_playlist_table animator] frame];
    plrect.size.height = i_lastSplitViewHeight - 19.0; // actual pl top bar height, which differs from its frame
    [[o_playlist_table animator] setFrame: plrect];
}

- (IBAction)togglePlaylist:(id)sender
{
    if ( ((([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask) != 0) && !b_splitview_removed && ![[VLCMain sharedInstance] activeVideoPlayback]) || (b_nonembedded && [[VLCMain sharedInstance] activeVideoPlayback] && sender != nil) )
    {
        [self hideSplitView];
    }
    else
    {
        if (b_splitview_removed)
        {
            if( !b_nonembedded ||( sender != nil && b_nonembedded))
                [self showSplitView];
        }

        if (b_dropzone_active && ![[VLCMain sharedInstance] activeVideoPlayback])
        {
            b_dropzone_active = NO;
            [self hideDropZone];
        }

        if (!b_nonembedded)
        {
            if ([o_video_view isHidden] && [[VLCMain sharedInstance] activeVideoPlayback]) {
                [o_split_view setHidden: YES];
                [o_video_view setHidden: NO];
                [self makeFirstResponder: o_video_view];
            }
            else
            {
                [o_video_view setHidden: YES];
                [o_split_view setHidden: NO];
                [self makeFirstResponder: nil];
            }
        }
        else
        {
            [o_split_view setHidden: NO];
            [o_playlist_table setHidden: NO];
            [o_video_view setHidden: ![[VLCMain sharedInstance] activeVideoPlayback]];
        }
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
}

- (IBAction)volumeAction:(id)sender
{
    if (sender == o_volume_sld)
        [[VLCCoreInteraction sharedInstance] setVolume: [sender intValue]];
    else if (sender == o_volume_down_btn)
    {
        [[VLCCoreInteraction sharedInstance] mute];
    }
    else
        [[VLCCoreInteraction sharedInstance] setVolume: AOUT_VOLUME_MAX];
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
#pragma mark overwritten default functionality
- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL s_menuAction = [menuItem action];

    if ((s_menuAction == @selector(performClose:)) || (s_menuAction == @selector(performMiniaturize:)) || (s_menuAction == @selector(performZoom:)))
            return YES;

    return [super validateMenuItem:menuItem];
}

- (void)setTitle:(NSString *)title
{
    if (b_dark_interface)
        [o_titlebar_view setWindowTitle: title];
    if (b_nonembedded && [[VLCMain sharedInstance] activeVideoPlayback])
        [o_nonembedded_window setTitle: title];
    [super setTitle: title];
}

- (void)performClose:(id)sender
{
    NSWindow *o_key_window = [NSApp keyWindow];

    if (b_dark_interface)
    {
        [o_key_window orderOut: sender];
        if ( [[VLCMain sharedInstance] activeVideoPlayback] && ( !b_nonembedded || o_key_window != self ))
            [[VLCCoreInteraction sharedInstance] stop];
    }
    else
    {
        if( b_nonembedded && o_key_window != self )
            [o_nonembedded_window performClose: sender];
        else
            [super performClose: sender];
    }
}

- (void)performMiniaturize:(id)sender
{
    if (b_dark_interface)
    {
        [self miniaturize: sender];
        if (config_GetInt( VLCIntf, "macosx-pause-minimized" ))
        {
            if ([[VLCMain sharedInstance] activeVideoPlayback])
                [[VLCCoreInteraction sharedInstance] pause];
        }
    }
    else
        [super performMiniaturize: sender];
}

- (void)performZoom:(id)sender
{
    if (b_dark_interface)
        [self customZoom: sender];
    else
        [super performZoom: sender];
}

- (void)zoom:(id)sender
{
    if (b_dark_interface)
        [self customZoom: sender];
    else
        [super zoom: sender];
}

/**
 * Given a proposed frame rectangle, return a modified version
 * which will fit inside the screen.
 *
 * This method is based upon NSWindow.m, part of the GNUstep GUI Library, licensed under LGPLv2+.
 *    Authors:  Scott Christley <scottc@net-community.com>, Venkat Ajjanagadde <venkat@ocbi.com>,   
 *              Felipe A. Rodriguez <far@ix.netcom.com>, Richard Frith-Macdonald <richard@brainstorm.co.uk>
 *    Copyright (C) 1996 Free Software Foundation, Inc.
 */
- (NSRect) customConstrainFrameRect: (NSRect)frameRect toScreen: (NSScreen*)screen
{
    NSRect screenRect = [screen visibleFrame];
    float difference;

    /* Move top edge of the window inside the screen */
    difference = NSMaxY (frameRect) - NSMaxY (screenRect);
    if (difference > 0)
    {
        frameRect.origin.y -= difference;
    }

    /* If the window is resizable, resize it (if needed) so that the
     bottom edge is on the screen or can be on the screen when the user moves
     the window */
    difference = NSMaxY (screenRect) - NSMaxY (frameRect);
    if (_styleMask & NSResizableWindowMask)
    {
        float difference2;

        difference2 = screenRect.origin.y - frameRect.origin.y;
        difference2 -= difference;
        // Take in account the space between the top of window and the top of the 
        // screen which can be used to move the bottom of the window on the screen
        if (difference2 > 0)
        {
            frameRect.size.height -= difference2;
            frameRect.origin.y += difference2;
        }

        /* Ensure that resizing doesn't makewindow smaller than minimum */
        difference2 = [self minSize].height - frameRect.size.height;
        if (difference2 > 0)
        {
            frameRect.size.height += difference2;
            frameRect.origin.y -= difference2;
        }
    }

    return frameRect;
}

#define DIST 3

/**
 Zooms the receiver.   This method calls the delegate method
 windowShouldZoom:toFrame: to determine if the window should
 be allowed to zoom to full screen.
 *
 * This method is based upon NSWindow.m, part of the GNUstep GUI Library, licensed under LGPLv2+.
 *    Authors:  Scott Christley <scottc@net-community.com>, Venkat Ajjanagadde <venkat@ocbi.com>,   
 *              Felipe A. Rodriguez <far@ix.netcom.com>, Richard Frith-Macdonald <richard@brainstorm.co.uk>
 *    Copyright (C) 1996 Free Software Foundation, Inc.
 */
- (void) customZoom: (id)sender
{
    NSRect maxRect = [[self screen] visibleFrame];
    NSRect currentFrame = [self frame];

    if ([[self delegate] respondsToSelector: @selector(windowWillUseStandardFrame:defaultFrame:)])
    {
        maxRect = [[self delegate] windowWillUseStandardFrame: self defaultFrame: maxRect];
    }

    maxRect = [self customConstrainFrameRect: maxRect toScreen: [self screen]];

    // Compare the new frame with the current one
    if ((abs(NSMaxX(maxRect) - NSMaxX(currentFrame)) < DIST)
        && (abs(NSMaxY(maxRect) - NSMaxY(currentFrame)) < DIST)
        && (abs(NSMinX(maxRect) - NSMinX(currentFrame)) < DIST)
        && (abs(NSMinY(maxRect) - NSMinY(currentFrame)) < DIST))
    {
        // Already in zoomed mode, reset user frame, if stored
        if ([self frameAutosaveName] != nil)
        {
            [self setFrame: previousSavedFrame display: YES animate: YES];
            [self saveFrameUsingName: [self frameAutosaveName]];
        }
        return;
    }

    if ([self frameAutosaveName] != nil)
    {
        [self saveFrameUsingName: [self frameAutosaveName]];
        previousSavedFrame = [self frame];
    }

    [self setFrame: maxRect display: YES animate: YES];
}

- (void)windowResizedOrMoved:(NSNotification *)notification
{
    [self saveFrameUsingName: [self frameAutosaveName]];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    if( config_GetInt( VLCIntf, "macosx-autosave-volume" ))
        config_PutInt( VLCIntf->p_libvlc, "volume", i_lastShownVolume );

    [self saveFrameUsingName: [self frameAutosaveName]];
}

- (void)someWindowWillClose:(NSNotification *)notification
{
    if([notification object] == o_nonembedded_window || ([notification object] == self && !b_nonembedded))
    {
        if ([[VLCMain sharedInstance] activeVideoPlayback])
            [[VLCCoreInteraction sharedInstance] stop];
    }
}

- (void)someWindowWillMiniaturize:(NSNotification *)notification
{
    if (config_GetInt( VLCIntf, "macosx-pause-minimized" ))
    {
        if([notification object] == o_nonembedded_window || [notification object] == self)
        {
            if([[VLCMain sharedInstance] activeVideoPlayback])
                [[VLCCoreInteraction sharedInstance] pause];
        }
    }
}

#pragma mark -
#pragma mark Update interface and respond to foreign events
- (void)showDropZone
{
    b_dropzone_active = YES;
    [o_right_split_view addSubview: o_dropzone_view];
    [o_dropzone_view setFrame: [o_playlist_table frame]];
    [[o_playlist_table animator] setHidden:YES];
}

- (void)hideDropZone
{
    [o_dropzone_view removeFromSuperview];
    [[o_playlist_table animator] setHidden: NO];
}

- (void)hideSplitView
{
    NSRect winrect = [self frame];
    i_lastSplitViewHeight = [o_split_view frame].size.height;
    winrect.size.height = winrect.size.height - i_lastSplitViewHeight;
    winrect.origin.y = winrect.origin.y + i_lastSplitViewHeight;
    [self setFrame: winrect display: YES animate: YES];
    [self performSelector:@selector(hideDropZone) withObject:nil afterDelay:0.1];
    if (b_dark_interface)
    {
        [self setContentMinSize: NSMakeSize( 604., [o_bottombar_view frame].size.height + [o_titlebar_view frame].size.height )];
        [self setContentMaxSize: NSMakeSize( FLT_MAX, [o_bottombar_view frame].size.height + [o_titlebar_view frame].size.height )];
    }
    else
    {
        [self setContentMinSize: NSMakeSize( 604., [o_bottombar_view frame].size.height )];
        [self setContentMaxSize: NSMakeSize( FLT_MAX, [o_bottombar_view frame].size.height )];
    }
    if (i_lastSplitViewHeight < 100)
        i_lastSplitViewHeight = 300; // random reasonable size
    b_splitview_removed = YES;
}

- (void)showSplitView
{
    if (b_dark_interface)
        [self setContentMinSize:NSMakeSize( 604., 288. + [o_titlebar_view frame].size.height )];
    else
        [self setContentMinSize:NSMakeSize( 604., 288. )];
    [self setContentMaxSize: NSMakeSize( FLT_MAX, FLT_MAX )];

    NSRect winrect;
    winrect = [self frame];
    winrect.size.height = winrect.size.height + i_lastSplitViewHeight;
    winrect.origin.y = winrect.origin.y - i_lastSplitViewHeight;
    [self setFrame: winrect display: YES animate: YES];

    [self performSelector:@selector(resizePlaylistAfterCollapse) withObject: nil afterDelay:0.75];
    [self performSelector:@selector(updateWindow) withObject: nil afterDelay:0.3];

    b_splitview_removed = NO;
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
            [o_time_sld_fancygradient_view setHidden: YES];
        } else {
            [o_time_sld setEnabled: YES];
            [o_time_sld setHidden: NO];
            [o_time_sld_fancygradient_view setHidden: NO];
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
        [o_time_sld_fancygradient_view setHidden: YES];
    }
}

- (void)updateVolumeSlider
{
    audio_volume_t i_volume;
    playlist_t * p_playlist = pl_Get( VLCIntf );

    i_volume = aout_VolumeGet( p_playlist );
    BOOL b_muted = [[VLCCoreInteraction sharedInstance] isMuted];

    if( !b_muted )
    {
        i_lastShownVolume = i_volume;
        [o_volume_sld setIntValue: i_volume];
        [o_fspanel setVolumeLevel: i_volume];
    }
    else
        [o_volume_sld setIntValue: 0];

    [o_volume_sld setEnabled: !b_muted];
    [o_volume_up_btn setEnabled: !b_muted];
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
        vlc_object_release( p_input );
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

        if (( cachedInputState == PLAYING_S || b_buffering == YES ) && [[VLCMain sharedInstance] activeVideoPlayback] )
            [[o_video_view window] makeKeyAndOrderFront: nil];

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
    if ([[[VLCMain sharedInstance] playlist] currentPlaylistRoot] != p_playlist->p_local_category || p_playlist->p_local_category->i_children > 0)
        [self hideDropZone];
    else
        [self showDropZone];
    PL_UNLOCK;
    [o_sidebar_view setNeedsDisplay:YES];
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
    if (OSX_LEOPARD)
        return;

    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    CGFloat f_value = [o_time_sld knobPosition];
    if (f_value > 7.5)
    {
        NSRect oldFrame = [o_time_sld_fancygradient_view frame];
        if (f_value != oldFrame.size.width)
        {
            if ([o_time_sld_fancygradient_view isHidden])
                [o_time_sld_fancygradient_view setHidden: NO];
            [o_time_sld_fancygradient_view setFrame: NSMakeRect( oldFrame.origin.x, oldFrame.origin.y, f_value, oldFrame.size.height )];
        }
    }
    else
    {
        NSRect frame;
        frame = [o_time_sld_fancygradient_view frame];
        if (frame.size.width > 0)
        {
            frame.size.width = 0;
            [o_time_sld_fancygradient_view setFrame: frame];
        }
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
        if ([o_video_view superview] != NULL)
            [o_video_view removeFromSuperviewWithoutNeedingDisplay];
        if (o_nonembedded_window)
            [o_nonembedded_window release];

        o_nonembedded_window = [[VLCWindow alloc] initWithContentRect:[o_video_view frame] styleMask: NSTitledWindowMask|NSClosableWindowMask|NSResizableWindowMask|NSMiniaturizableWindowMask backing:NSBackingStoreBuffered defer:YES];
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
    BOOL b_videoPlayback = [[VLCMain sharedInstance] activeVideoPlayback];

    if (!b_videoPlayback)
        [o_nonembedded_window orderOut: nil];
    if( OSX_LION && b_nativeFullscreenMode )
    {
        if( [NSApp presentationOptions] & NSApplicationPresentationFullScreen )
            [o_bottombar_view setHidden: b_videoPlayback];
        else
            [o_bottombar_view setHidden: NO];
        if (!b_videoPlayback)
            [o_fspanel setNonActive: nil];
    }
    if (b_videoPlayback)
        [self makeFirstResponder: o_video_view];
    else
        [self makeFirstResponder: nil];

    if (!b_videoPlayback && b_fullscreen)
    {
        if (!b_nativeFullscreenMode || !OSX_LION)
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
    }
}

- (void)resizeWindow
{
    if ( b_fullscreen || (OSX_LION && [NSApp presentationOptions] & NSApplicationPresentationFullScreen && b_nativeFullscreenMode) )
        return;

    NSPoint topleftbase = NSMakePoint(0, [self frame].size.height);
    NSPoint topleftscreen = [self convertBaseToScreen: topleftbase];

    /* Calculate the window's new size */
    float w = [self frame].size.width  - [o_video_view frame].size.width
        + nativeVideoSize.width;
    float h = [self frame].size.height - [o_video_view frame].size.height
        + nativeVideoSize.height;

    if (b_dark_interface)
        h += [o_titlebar_view frame].size.height;

    NSRect new_frame = NSMakeRect(topleftscreen.x, topleftscreen.y - h, w, h);

    [[self animator] setFrame:new_frame display:YES];
}

- (void)setNativeVideoSize:(NSSize)size
{
    if (size.width != nativeVideoSize.width || size.height != nativeVideoSize.height )
    {
        nativeVideoSize = size;
        [self resizeWindow];
    }
}

//  Called automatically if window's acceptsMouseMovedEvents property is true
- (void)mouseMoved:(NSEvent *)theEvent
{
    if (b_fullscreen)
        [self recreateHideMouseTimer];

    [super mouseMoved: theEvent];
}

- (void)recreateHideMouseTimer
{
    if (t_hide_mouse_timer != nil) {
        [t_hide_mouse_timer invalidate];
        [t_hide_mouse_timer release];
    }

    t_hide_mouse_timer = [NSTimer scheduledTimerWithTimeInterval:2
                                                          target:self
                                                        selector:@selector(hideMouseCursor:)
                                                        userInfo:nil
                                                         repeats:NO];
    [t_hide_mouse_timer retain];
}

//  NSTimer selectors require this function signature as per Apple's docs
- (void)hideMouseCursor:(NSTimer *)timer
{
    [NSCursor setHiddenUntilMouseMoves: YES];
}

#pragma mark -
#pragma mark Fullscreen support
- (void)showFullscreenController
{
     if (b_fullscreen && [[VLCMain sharedInstance] activeVideoPlayback] )
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
    id o_videoWindow = b_nonembedded ? o_nonembedded_window : self;

    if( p_vout )
        screen = [NSScreen screenWithDisplayID:(CGDirectDisplayID)config_GetInt( VLCIntf, "macosx-vdev" )];

    [self lockFullscreenAnimation];

    if (!screen)
    {
        msg_Dbg( VLCIntf, "chosen screen isn't present, using current screen for fullscreen mode" );
        screen = [o_videoWindow screen];
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

    [self recreateHideMouseTimer];

    if( blackout_other_displays )
        [screen blackoutOtherScreens];

    /* Make sure we don't see the window flashes in float-on-top mode */
    i_originalLevel = [o_videoWindow level];
    [o_videoWindow setLevel:NSNormalWindowLevel];

    /* Only create the o_fullscreen_window if we are not in the middle of the zooming animation */
    if (!o_fullscreen_window)
    {
        /* We can't change the styleMask of an already created NSWindow, so we create another window, and do eye catching stuff */

        rect = [[o_video_view superview] convertRect: [o_video_view frame] toView: nil]; /* Convert to Window base coord */
        rect.origin.x += [o_videoWindow frame].origin.x;
        rect.origin.y += [o_videoWindow frame].origin.y;
        o_fullscreen_window = [[VLCWindow alloc] initWithContentRect:rect styleMask: NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
        [o_fullscreen_window setFullscreen: YES];
        [o_fullscreen_window setBackgroundColor: [NSColor blackColor]];
        [o_fullscreen_window setCanBecomeKeyWindow: YES];

        if (![o_videoWindow isVisible] || [o_videoWindow alphaValue] == 0.0)
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
                if (OSX_LEOPARD)
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
        if( b_nonembedded )
            [o_nonembedded_window orderOut: self];
        else
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
        if (OSX_LEOPARD)
            SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
        else
            [NSApp setPresentationOptions:(NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar)];
    }

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:2];
    dict2 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:o_videoWindow forKey:NSViewAnimationTargetKey];
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

    if( !b_nonembedded && [self isVisible] )
        [super orderOut: self];

    if( b_nonembedded && [o_nonembedded_window isVisible] )
        [o_nonembedded_window orderOut: self];

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
        if (OSX_LEOPARD)
            SetSystemUIMode( kUIModeNormal, kUIOptionAutoShowMenuBar);
        else
            [NSApp setPresentationOptions: NSApplicationPresentationDefault];

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

    id o_videoWindow = b_nonembedded ? o_nonembedded_window : self;

    [o_videoWindow setAlphaValue: 0.0];
    [o_videoWindow orderFront: self];
    [[o_video_view window] orderFront: self];

    [o_fspanel setNonActive: nil];
    if (OSX_LEOPARD)
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
    frame.origin.x += [o_videoWindow frame].origin.x;
    frame.origin.y += [o_videoWindow frame].origin.y;

    dict2 = [[NSMutableDictionary alloc] initWithCapacity:2];
    [dict2 setObject:o_videoWindow forKey:NSViewAnimationTargetKey];
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
    [[o_video_view window] makeFirstResponder: o_video_view];
    if( [[o_video_view window] isVisible] )
    {
        if( !b_nonembedded )
            [super makeKeyAndOrderFront:self]; /* our version contains a workaround */
        else
            [[o_video_view window] makeKeyAndOrderFront: self];
    }
    [o_fullscreen_window orderOut: self];
    NSEnableScreenUpdates();

    [o_fullscreen_window release];
    o_fullscreen_window = nil;
    [[o_video_view window] setLevel:i_originalLevel];

    // if we quit fullscreen because there is no video anymore, make sure non-embedded window is not visible
    if( ![[VLCMain sharedInstance] activeVideoPlayback] && b_nonembedded )
        [o_nonembedded_window orderOut: self];

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
    /* Make sure we leave fullscreen */
    if (!(OSX_LION || !b_nativeFullscreenMode))
        [self leaveFullscreenAndFadeOut: YES];

    [super orderOut: sender];
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

        [anim setAnimationBlockingMode: NSAnimationNonblocking];
        [anim setDuration: 0.4];
        [anim setFrameRate: 30];
        [anim startAnimation];

        [anim release];
    }
    else {
        [super setFrame:args->frame display:args->display animate:args->animate];
    }
}

#pragma mark -
#pragma mark Lion's native fullscreen handling
- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    [o_video_view setFrame: [[self contentView] frame]];
    b_fullscreen = YES;
    [o_fspanel setVoutWasUpdated: (int)[[self screen] displayID]];
    [o_fspanel setActive: nil];

    [self recreateHideMouseTimer];
    i_originalLevel = [self level];
    [self setLevel:NSNormalWindowLevel];

    if (b_dark_interface)
    {
        [o_titlebar_view removeFromSuperviewWithoutNeedingDisplay];

        NSRect winrect;
        CGFloat f_titleBarHeight = [o_titlebar_view frame].size.height;
        winrect = [self frame];

        winrect.size.height = winrect.size.height - f_titleBarHeight;
        [self setFrame: winrect display:NO animate:NO];
        winrect = [o_split_view frame];
        winrect.size.height = winrect.size.height + f_titleBarHeight;
        [o_split_view setFrame: winrect];
    }

    if ([[VLCMain sharedInstance] activeVideoPlayback])
        [o_bottombar_view setHidden: YES];
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
    [o_video_view setFrame: [o_split_view frame]];
    [NSCursor setHiddenUntilMouseMoves: NO];
    [o_fspanel setNonActive: nil];
    [self setLevel:i_originalLevel];
    b_fullscreen = NO;

    if (b_dark_interface)
    {
        NSRect winrect;
        CGFloat f_titleBarHeight = [o_titlebar_view frame].size.height;
        winrect = [self frame];

        [o_titlebar_view setFrame: NSMakeRect( 0, winrect.size.height - f_titleBarHeight,
                                              winrect.size.width, f_titleBarHeight )];
        [[self contentView] addSubview: o_titlebar_view];

        winrect.size.height = winrect.size.height + f_titleBarHeight;
        [self setFrame: winrect display:NO animate:NO];
        winrect = [o_split_view frame];
        winrect.size.height = winrect.size.height - f_titleBarHeight;
        [o_split_view setFrame: winrect];
        [o_video_view setFrame: winrect];
    }

    if ([[VLCMain sharedInstance] activeVideoPlayback])
        [o_bottombar_view setHidden: NO];
}

#pragma mark -
#pragma mark split view delegate
- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMin ofSubviewAt:(NSInteger)dividerIndex
{
    if (dividerIndex == 0)
        return 200.0;
    else
        return proposedMin;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMax ofSubviewAt:(NSInteger)dividerIndex
{
    if (dividerIndex == 0)
        return ([self frame].size.width - 300.0);
    else
        return proposedMax;
}

#pragma mark -
#pragma mark Side Bar Data handling
/* taken under BSD-new from the PXSourceList sample project, adapted for VLC */
- (NSUInteger)sourceList:(PXSourceList*)sourceList numberOfChildrenOfItem:(id)item
{
    //Works the same way as the NSOutlineView data source: `nil` means a parent item
    if(item==nil)
        return [o_sidebaritems count];
    else
        return [[item children] count];
}


- (id)sourceList:(PXSourceList*)aSourceList child:(NSUInteger)index ofItem:(id)item
{
    //Works the same way as the NSOutlineView data source: `nil` means a parent item
    if(item==nil)
        return [o_sidebaritems objectAtIndex:index];
    else
        return [[item children] objectAtIndex:index];
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
    if ([[item identifier] isEqualToString: @"playlist"] || [[item identifier] isEqualToString: @"medialibrary"])
        return YES;

    return [item hasBadge];
}


- (NSInteger)sourceList:(PXSourceList*)aSourceList badgeValueForItem:(id)item
{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    NSInteger i_playlist_size;

    if ([[item identifier] isEqualToString: @"playlist"])
    {
        PL_LOCK;
        i_playlist_size = p_playlist->p_local_category->i_children;
        PL_UNLOCK;

        return i_playlist_size;
    }
    if ([[item identifier] isEqualToString: @"medialibrary"])
    {
        PL_LOCK;
        i_playlist_size = p_playlist->p_ml_category->i_children;
        PL_UNLOCK;

        return i_playlist_size;
    }

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
    if ([theEvent type] == NSRightMouseDown || ([theEvent type] == NSLeftMouseDown && ([theEvent modifierFlags] & NSControlKeyMask) == NSControlKeyMask))
    {
        if (item != nil)
        {
            NSMenu * m;
            if ([item sdtype] > 0)
            {
                m = [[NSMenu alloc] init];
                playlist_t * p_playlist = pl_Get( VLCIntf );
                BOOL sd_loaded = playlist_IsServicesDiscoveryLoaded( p_playlist, [[item identifier] UTF8String] );
                if (!sd_loaded)
                    [m addItemWithTitle:_NS("Enable") action:@selector(sdmenuhandler:) keyEquivalent:@""];
                else
                    [m addItemWithTitle:_NS("Disable") action:@selector(sdmenuhandler:) keyEquivalent:@""];
                [[m itemAtIndex:0] setRepresentedObject: [item identifier]];
            }
            return [m autorelease];
        }
    }

    return nil;
}

- (IBAction)sdmenuhandler:(id)sender
{
    NSString * identifier = [sender representedObject];
    if ([identifier length] > 0 && ![identifier isEqualToString:@"lua{sd='freebox',longname='Freebox TV'}"])
    {
        playlist_t * p_playlist = pl_Get( VLCIntf );
        BOOL sd_loaded = playlist_IsServicesDiscoveryLoaded( p_playlist, [identifier UTF8String] );

        if (!sd_loaded)
            playlist_ServicesDiscoveryAdd( p_playlist, [identifier UTF8String] );
        else
            playlist_ServicesDiscoveryRemove( p_playlist, [identifier UTF8String] );
    }
}

#pragma mark -
#pragma mark Side Bar Delegate Methods
/* taken under BSD-new from the PXSourceList sample project, adapted for VLC */
- (BOOL)sourceList:(PXSourceList*)aSourceList isGroupAlwaysExpanded:(id)group
{
    if ([[group identifier] isEqualToString:@"library"])
        return YES;

    return NO;
}

- (void)sourceListSelectionDidChange:(NSNotification *)notification
{
    playlist_t * p_playlist = pl_Get( VLCIntf );

    NSIndexSet *selectedIndexes = [o_sidebar_view selectedRowIndexes];
    id item = [o_sidebar_view itemAtRow:[selectedIndexes firstIndex]];


    //Set the label text to represent the new selection
    if ([item sdtype] > -1 && [[item identifier] length] > 0)
    {
        BOOL sd_loaded = playlist_IsServicesDiscoveryLoaded( p_playlist, [[item identifier] UTF8String] );
        if (!sd_loaded)
        {
            playlist_ServicesDiscoveryAdd( p_playlist, [[item identifier] UTF8String] );
        }
    }

    [o_chosen_category_lbl setStringValue:[item title]];

    if ([[item identifier] isEqualToString:@"playlist"])
    {
        [[[VLCMain sharedInstance] playlist] setPlaylistRoot:p_playlist->p_local_category];
    }
    else if([[item identifier] isEqualToString:@"medialibrary"])
    {
        [[[VLCMain sharedInstance] playlist] setPlaylistRoot:p_playlist->p_ml_category];
    }
    else
    {
        playlist_item_t * pl_item;
        PL_LOCK;
        pl_item = playlist_ChildSearchName( p_playlist->p_root, [[item untranslatedTitle] UTF8String] );
        PL_UNLOCK;
        [[[VLCMain sharedInstance] playlist] setPlaylistRoot: pl_item];
    }

    PL_LOCK;
    if ([[[VLCMain sharedInstance] playlist] currentPlaylistRoot] != p_playlist->p_local_category || p_playlist->p_local_category->i_children > 0)
        [self hideDropZone];
    else
        [self showDropZone];
    PL_UNLOCK;
}

@end
