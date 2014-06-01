/*****************************************************************************
 * MainWindow.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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
#import <math.h>
#import <vlc_playlist.h>
#import <vlc_url.h>
#import <vlc_strings.h>
#import <vlc_services_discovery.h>

#import "ControlsBar.h"
#import "VideoView.h"
#import "VLCVoutWindowController.h"


@interface VLCMainWindow (Internal)
- (void)resizePlaylistAfterCollapse;
- (void)makeSplitViewVisible;
- (void)makeSplitViewHidden;
- (void)showPodcastControls;
- (void)hidePodcastControls;
@end


@implementation VLCMainWindow

@synthesize nativeFullscreenMode=b_nativeFullscreenMode;
@synthesize nonembedded=b_nonembedded;
@synthesize fsPanel=o_fspanel;

static VLCMainWindow *_o_sharedInstance = nil;

+ (VLCMainWindow *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

#pragma mark -
#pragma mark Initialization

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
        return _o_sharedInstance;
    } else
        _o_sharedInstance = [super init];

    return _o_sharedInstance;
}

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask
                  backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
    self = [super initWithContentRect:contentRect styleMask:styleMask
                              backing:backingType defer:flag];
    _o_sharedInstance = self;

    [[VLCMain sharedInstance] updateTogglePlaylistState];

    return self;
}

- (BOOL)isEvent:(NSEvent *)o_event forKey:(const char *)keyString
{
    char *key;
    NSString *o_key;

    key = config_GetPsz(VLCIntf, keyString);
    o_key = [NSString stringWithFormat:@"%s", key];
    FREENULL(key);

    unsigned int i_keyModifiers = [[VLCStringUtility sharedInstance] VLCModifiersToCocoa:o_key];

    NSString * characters = [o_event charactersIgnoringModifiers];
    if ([characters length] > 0) {
        return [[characters lowercaseString] isEqualToString: [[VLCStringUtility sharedInstance] VLCKeyToString: o_key]] &&
                (i_keyModifiers & NSShiftKeyMask)     == ([o_event modifierFlags] & NSShiftKeyMask) &&
                (i_keyModifiers & NSControlKeyMask)   == ([o_event modifierFlags] & NSControlKeyMask) &&
                (i_keyModifiers & NSAlternateKeyMask) == ([o_event modifierFlags] & NSAlternateKeyMask) &&
                (i_keyModifiers & NSCommandKeyMask)   == ([o_event modifierFlags] & NSCommandKeyMask);
    }
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)o_event
{
    BOOL b_force = NO;
    // these are key events which should be handled by vlc core, but are attached to a main menu item
    if (![self isEvent: o_event forKey: "key-vol-up"] &&
        ![self isEvent: o_event forKey: "key-vol-down"] &&
        ![self isEvent: o_event forKey: "key-vol-mute"] &&
        ![self isEvent: o_event forKey: "key-prev"] &&
        ![self isEvent: o_event forKey: "key-next"] &&
        ![self isEvent: o_event forKey: "key-jump+short"] &&
        ![self isEvent: o_event forKey: "key-jump-short"]) {
        /* We indeed want to prioritize some Cocoa key equivalent against libvlc,
         so we perform the menu equivalent now. */
        if ([[NSApp mainMenu] performKeyEquivalent:o_event])
            return TRUE;
    }
    else
        b_force = YES;

    return [[VLCMain sharedInstance] hasDefinedShortcutKey:o_event force:b_force] ||
           [(VLCControls *)[[VLCMain sharedInstance] controls] keyEvent:o_event];
}

- (void)dealloc
{
    if (b_dark_interface)
        [o_color_backdrop release];

    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [o_sidebaritems release];
    [o_fspanel release];

    [super dealloc];
}

- (void)awakeFromNib
{
    // sets lion fullscreen behaviour
    [super awakeFromNib];

    BOOL b_splitviewShouldBeHidden = NO;

    if (!OSX_SNOW_LEOPARD)
        [self setRestorable: NO];
    [self setFrameAutosaveName:@"mainwindow"];

    /* setup the styled interface */
    b_nativeFullscreenMode = NO;
#ifdef MAC_OS_X_VERSION_10_7
    if (!OSX_SNOW_LEOPARD)
        b_nativeFullscreenMode = var_InheritBool(VLCIntf, "macosx-nativefullscreenmode");
#endif
    [self useOptimizedDrawing: YES];

    [[o_search_fld cell] setPlaceholderString: _NS("Search")];
    [[o_search_fld cell] accessibilitySetOverrideValue:_NS("Enter a term to search the playlist. Results will be selected in the table.") forAttribute:NSAccessibilityDescriptionAttribute];

    [o_dropzone_btn setTitle: _NS("Open media...")];
    [[o_dropzone_btn cell] accessibilitySetOverrideValue:_NS("Click to open an advanced dialog to select the media to play. You can also drop files here to play.") forAttribute:NSAccessibilityDescriptionAttribute];
    [o_dropzone_lbl setStringValue: _NS("Drop media here")];

    [o_podcast_add_btn setTitle: _NS("Subscribe")];
    [o_podcast_remove_btn setTitle: _NS("Unsubscribe")];
    [o_podcast_subscribe_title_lbl setStringValue: _NS("Subscribe to a podcast")];
    [o_podcast_subscribe_subtitle_lbl setStringValue: _NS("Enter URL of the podcast to subscribe to:")];
    [o_podcast_subscribe_cancel_btn setTitle: _NS("Cancel")];
    [o_podcast_subscribe_ok_btn setTitle: _NS("Subscribe")];
    [o_podcast_unsubscribe_title_lbl setStringValue: _NS("Unsubscribe from a podcast")];
    [o_podcast_unsubscribe_subtitle_lbl setStringValue: _NS("Select the podcast you would like to unsubscribe from:")];
    [o_podcast_unsubscribe_ok_btn setTitle: _NS("Unsubscribe")];
    [o_podcast_unsubscribe_cancel_btn setTitle: _NS("Cancel")];

    /* interface builder action */
    CGFloat f_threshold_height = f_min_video_height + [o_controls_bar height];
    if (b_dark_interface)
        f_threshold_height += [o_titlebar_view frame].size.height;
    if ([[self contentView] frame].size.height < f_threshold_height)
        b_splitviewShouldBeHidden = YES;

    [self setDelegate: self];
    [self setExcludedFromWindowsMenu: YES];
    [self setAcceptsMouseMovedEvents: YES];
    // Set that here as IB seems to be buggy
    if (b_dark_interface)
        [self setContentMinSize:NSMakeSize(604., 288. + [o_titlebar_view frame].size.height)];
    else
        [self setContentMinSize:NSMakeSize(604., 288.)];

    [self setTitle: _NS("VLC media player")];

    b_dropzone_active = YES;
    [o_dropzone_view setFrame: [o_playlist_table frame]];
    [o_left_split_view setFrame: [o_sidebar_view frame]];

    if (!OSX_SNOW_LEOPARD) {
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
    char **ppsz_longnames = NULL;
    int *p_categories = NULL;
    char **ppsz_names = vlc_sd_GetNames(pl_Get(VLCIntf), &ppsz_longnames, &p_categories);
    if (!ppsz_names)
        msg_Err(VLCIntf, "no sd item found"); //TODO
    char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
    int *p_category = p_categories;
    NSMutableArray *internetItems = [[NSMutableArray alloc] init];
    NSMutableArray *devicesItems = [[NSMutableArray alloc] init];
    NSMutableArray *lanItems = [[NSMutableArray alloc] init];
    NSMutableArray *mycompItems = [[NSMutableArray alloc] init];
    NSString *o_identifier;
    for (; ppsz_name && *ppsz_name; ppsz_name++, ppsz_longname++, p_category++) {
        o_identifier = [NSString stringWithCString: *ppsz_name encoding: NSUTF8StringEncoding];
        switch (*p_category) {
            case SD_CAT_INTERNET:
                    [internetItems addObject: [SideBarItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                    if (!strncmp(*ppsz_name, "podcast", 7))
                        [[internetItems lastObject] setIcon: [NSImage imageNamed:@"sidebar-podcast"]];
                    else
                        [[internetItems lastObject] setIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
                    [[internetItems lastObject] setSdtype: SD_CAT_INTERNET];
                    [[internetItems lastObject] setUntranslatedTitle: [NSString stringWithUTF8String:*ppsz_longname]];
                break;
            case SD_CAT_DEVICES:
                    [devicesItems addObject: [SideBarItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                    [[devicesItems lastObject] setIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
                    [[devicesItems lastObject] setSdtype: SD_CAT_DEVICES];
                    [[devicesItems lastObject] setUntranslatedTitle: [NSString stringWithUTF8String:*ppsz_longname]];
                break;
            case SD_CAT_LAN:
                    [lanItems addObject: [SideBarItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                    [[lanItems lastObject] setIcon: [NSImage imageNamed:@"sidebar-local"]];
                    [[lanItems lastObject] setSdtype: SD_CAT_LAN];
                    [[lanItems lastObject] setUntranslatedTitle: [NSString stringWithUTF8String:*ppsz_longname]];
                break;
            case SD_CAT_MYCOMPUTER:
                    [mycompItems addObject: [SideBarItem itemWithTitle: _NS(*ppsz_longname) identifier: o_identifier]];
                    if (!strncmp(*ppsz_name, "video_dir", 9))
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"sidebar-movie"]];
                    else if (!strncmp(*ppsz_name, "audio_dir", 9))
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"sidebar-music"]];
                    else if (!strncmp(*ppsz_name, "picture_dir", 11))
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"sidebar-pictures"]];
                    else
                        [[mycompItems lastObject] setIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
                    [[mycompItems lastObject] setUntranslatedTitle: [NSString stringWithUTF8String:*ppsz_longname]];
                    [[mycompItems lastObject] setSdtype: SD_CAT_MYCOMPUTER];
                break;
            default:
                msg_Warn(VLCIntf, "unknown SD type found, skipping (%s)", *ppsz_name);
                break;
        }

        free(*ppsz_name);
        free(*ppsz_longname);
    }
    [mycompItem setChildren: [NSArray arrayWithArray: mycompItems]];
    [devicesItem setChildren: [NSArray arrayWithArray: devicesItems]];
    [lanItem setChildren: [NSArray arrayWithArray: lanItems]];
    [internetItem setChildren: [NSArray arrayWithArray: internetItems]];
    [mycompItems release];
    [devicesItems release];
    [lanItems release];
    [internetItems release];
    free(ppsz_names);
    free(ppsz_longnames);
    free(p_categories);

    [libraryItem setChildren: [NSArray arrayWithObjects:playlistItem, medialibraryItem, nil]];
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
    [o_sidebar_view setDropItem:playlistItem dropChildIndex:NSOutlineViewDropOnItemIndex];
    [o_sidebar_view registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, @"VLCPlaylistItemPboardType", nil]];

    [o_sidebar_view setAutosaveName:@"mainwindow-sidebar"];
    [(PXSourceList *)o_sidebar_view setDataSource:self];
    [o_sidebar_view setDelegate:self];
    [o_sidebar_view setAutosaveExpandedItems:YES];

    [o_sidebar_view expandItem: libraryItem expandChildren: YES];

    o_fspanel = [[VLCFSPanel alloc] initWithContentRect:NSMakeRect(110.,267.,549.,87.)
                                              styleMask:NSTexturedBackgroundWindowMask
                                                backing:NSBackingStoreBuffered
                                                  defer:YES];

    /* make sure we display the desired default appearance when VLC launches for the first time */
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:@"VLCFirstRun"]) {
        [defaults setObject:[NSDate date] forKey:@"VLCFirstRun"];

        NSUInteger i_sidebaritem_count = [o_sidebaritems count];
        for (NSUInteger x = 0; x < i_sidebaritem_count; x++)
            [o_sidebar_view expandItem: [o_sidebaritems objectAtIndex:x] expandChildren: YES];

        [o_fspanel center];

        NSAlert *albumArtAlert = [NSAlert alertWithMessageText:_NS("Check for album art and metadata?") defaultButton:_NS("Enable Metadata Retrieval") alternateButton:_NS("No, Thanks") otherButton:nil informativeTextWithFormat:@"%@",_NS("VLC can check online for album art and metadata to enrich your playback experience, e.g. by providing track information when playing Audio CDs. To provide this functionality, VLC will send information about your contents to trusted services in an anonymized form.")];
        NSInteger returnValue = [albumArtAlert runModal];
        config_PutInt(VLCIntf, "metadata-network-access", returnValue == NSAlertDefaultReturn);
    }

    // select playlist item by default
    [o_sidebar_view selectRowIndexes:[NSIndexSet indexSetWithIndex:1] byExtendingSelection:NO];

    if (b_dark_interface) {
        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowResizedOrMoved:) name: NSWindowDidResizeNotification object: nil];
        [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(windowResizedOrMoved:) name: NSWindowDidMoveNotification object: nil];

        [self setBackgroundColor: [NSColor clearColor]];
        [self setOpaque: NO];
        [self display];
        [self setHasShadow:NO];
        [self setHasShadow:YES];

        NSRect winrect = [self frame];
        CGFloat f_titleBarHeight = [o_titlebar_view frame].size.height;

        [o_titlebar_view setFrame: NSMakeRect(0, winrect.size.height - f_titleBarHeight,
                                              winrect.size.width, f_titleBarHeight)];
        [[self contentView] addSubview: o_titlebar_view positioned: NSWindowAbove relativeTo: o_split_view];

        if (winrect.size.height > 100) {
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
    } else {
        [o_video_view setFrame: [o_split_view frame]];
        [o_playlist_table setBorderType: NSNoBorder];
        [o_sidebar_scrollview setBorderType: NSNoBorder];
    }

    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(someWindowWillClose:) name: NSWindowWillCloseNotification object: nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(someWindowWillMiniaturize:) name: NSWindowWillMiniaturizeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(applicationWillTerminate:) name: NSApplicationWillTerminateNotification object: nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(mainSplitViewDidResizeSubviews:) name: NSSplitViewDidResizeSubviewsNotification object:o_split_view];

    if (b_splitviewShouldBeHidden) {
        [self hideSplitView: YES];
        f_lastSplitViewHeight = 300;
    }

    /* sanity check for the window size */
    NSRect frame = [self frame];
    NSSize screenSize = [[self screen] frame].size;
    if (screenSize.width <= frame.size.width || screenSize.height <= frame.size.height) {
        nativeVideoSize = screenSize;
        [self resizeWindow];
    }

    /* update fs button to reflect state for next startup */
    if (var_InheritBool(pl_Get(VLCIntf), "fullscreen"))
        [o_controls_bar setFullscreenState:YES];

    /* restore split view */
    f_lastLeftSplitViewWidth = 200;
    /* trick NSSplitView implementation, which pretends to know better than us */
    if (!config_GetInt(VLCIntf, "macosx-show-sidebar"))
        [self performSelector:@selector(toggleLeftSubSplitView) withObject:nil afterDelay:0.05];
}

#pragma mark -
#pragma mark appearance management

- (VLCMainWindowControlsBar *)controlsBar;
{
    return (VLCMainWindowControlsBar *)o_controls_bar;
}

- (void)resizePlaylistAfterCollapse
{
    // no animation here since we might be in the middle of another resize animation
    NSRect plrect;
    plrect = [o_playlist_table frame];
    plrect.size.height = [o_split_view frame].size.height - 20.0; // actual pl top bar height, which differs from its frame
    [o_playlist_table setFrame: plrect];
    [o_playlist_table setNeedsDisplay: YES];

    NSRect rightSplitRect;
    rightSplitRect = [o_right_split_view frame];
    plrect = [o_dropzone_box frame];
    plrect.origin.x = (rightSplitRect.size.width - plrect.size.width) / 2;
    plrect.origin.y = (rightSplitRect.size.height - plrect.size.height) / 2;
    [o_dropzone_view setFrame: [o_playlist_table frame]];
    [o_dropzone_box setFrame: plrect];
    [o_dropzone_view setNeedsDisplay: YES];
}

- (void)makeSplitViewVisible
{
    if (b_dark_interface)
        [self setContentMinSize: NSMakeSize(604., 288. + [o_titlebar_view frame].size.height)];
    else
        [self setContentMinSize: NSMakeSize(604., 288.)];

    NSRect old_frame = [self frame];
    CGFloat newHeight = [self minSize].height;
    if (old_frame.size.height < newHeight) {
        NSRect new_frame = old_frame;
        new_frame.origin.y = old_frame.origin.y + old_frame.size.height - newHeight;
        new_frame.size.height = newHeight;

        [[self animator] setFrame: new_frame display: YES animate: YES];
    }

    [o_video_view setHidden: YES];
    [o_split_view setHidden: NO];
    if (b_nativeFullscreenMode && [self fullscreen]) {
        [[o_controls_bar bottomBarView] setHidden: NO];
        [o_fspanel setNonActive:nil];
    }

    [self makeFirstResponder: o_playlist_table];
}

- (void)makeSplitViewHidden
{
    if (b_dark_interface)
        [self setContentMinSize: NSMakeSize(604., f_min_video_height + [o_titlebar_view frame].size.height)];
    else
        [self setContentMinSize: NSMakeSize(604., f_min_video_height)];

    [o_split_view setHidden: YES];
    [o_video_view setHidden: NO];
    if (b_nativeFullscreenMode && [self fullscreen]) {
        [[o_controls_bar bottomBarView] setHidden: YES];
        [o_fspanel setActive:nil];
    }

    if ([[o_video_view subviews] count] > 0)
        [self makeFirstResponder: [[o_video_view subviews] objectAtIndex:0]];
}


- (void)changePlaylistState:(VLCPlaylistStateEvent)event
{
    // Beware, this code is really ugly

    msg_Dbg(VLCIntf, "toggle playlist from state: removed splitview %i, minimized view %i. Event %i", b_splitview_removed, b_minimized_view, event);
    if (![self isVisible] && event == psUserMenuEvent) {
        [self makeKeyAndOrderFront: nil];
        return;
    }

    BOOL b_activeVideo = [[VLCMain sharedInstance] activeVideoPlayback];
    BOOL b_restored = NO;

    // ignore alt if triggered through main menu shortcut
    BOOL b_have_alt_key = ([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask) != 0;
    if (event == psUserMenuEvent)
        b_have_alt_key = NO;

    // eUserMenuEvent is now handled same as eUserEvent
    if(event == psUserMenuEvent)
        event = psUserEvent;

    if (b_dropzone_active && b_have_alt_key) {
        [self hideDropZone];
        return;
    }

    if (!(b_nativeFullscreenMode && b_fullscreen) && !b_splitview_removed && ((b_have_alt_key && b_activeVideo)
                                                                              || (b_nonembedded && event == psUserEvent)
                                                                              || (!b_activeVideo && event == psUserEvent)
                                                                              || (b_minimized_view && event == psVideoStartedOrStoppedEvent))) {
        // for starting playback, window is resized through resized events
        // for stopping playback, resize through reset to previous frame
        [self hideSplitView: event != psVideoStartedOrStoppedEvent];
        b_minimized_view = NO;
    } else {
        if (b_splitview_removed) {
            if (!b_nonembedded || (event == psUserEvent && b_nonembedded))
                [self showSplitView: event != psVideoStartedOrStoppedEvent];

            if (event != psUserEvent)
                b_minimized_view = YES;
            else
                b_minimized_view = NO;

            if (b_activeVideo)
                b_restored = YES;
        }

        if (!b_nonembedded) {
            if (([o_video_view isHidden] && b_activeVideo) || b_restored || (b_activeVideo && event != psUserEvent))
                [self makeSplitViewHidden];
            else
                [self makeSplitViewVisible];
        } else {
            [o_split_view setHidden: NO];
            [o_playlist_table setHidden: NO];
            [o_video_view setHidden: YES];
        }
    }

    msg_Dbg(VLCIntf, "toggle playlist to state: removed splitview %i, minimized view %i", b_splitview_removed, b_minimized_view);
}

- (IBAction)dropzoneButtonAction:(id)sender
{
    [[[VLCMain sharedInstance] open] openFileGeneric];
}

#pragma mark -
#pragma mark overwritten default functionality

- (void)windowResizedOrMoved:(NSNotification *)notification
{
    [self saveFrameUsingName: [self frameAutosaveName]];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    config_PutInt(VLCIntf, "macosx-show-sidebar", ![o_split_view isSubviewCollapsed:o_left_split_view]);

    [self saveFrameUsingName: [self frameAutosaveName]];
}


- (void)someWindowWillClose:(NSNotification *)notification
{
    id obj = [notification object];

    // hasActiveVideo is defined for VLCVideoWindowCommon and subclasses
    if ([obj respondsToSelector:@selector(hasActiveVideo)] && [obj hasActiveVideo]) {
        if ([[VLCMain sharedInstance] activeVideoPlayback])
            [[VLCCoreInteraction sharedInstance] stop];
    }
}

- (void)someWindowWillMiniaturize:(NSNotification *)notification
{
    if (config_GetInt(VLCIntf, "macosx-pause-minimized")) {
        id obj = [notification object];

        if ([obj class] == [VLCVideoWindowCommon class] || [obj class] == [VLCDetachedVideoWindow class] || ([obj class] == [VLCMainWindow class] && !b_nonembedded)) {
            if ([[VLCMain sharedInstance] activeVideoPlayback])
                [[VLCCoreInteraction sharedInstance] pause];
        }
    }
}

#pragma mark -
#pragma mark Update interface and respond to foreign events
- (void)showDropZone
{
    b_dropzone_active = YES;
    [o_right_split_view addSubview: o_dropzone_view positioned:NSWindowAbove relativeTo:o_playlist_table];
    [o_dropzone_view setFrame: [o_playlist_table frame]];
    [o_playlist_table setHidden:YES];
}

- (void)hideDropZone
{
    b_dropzone_active = NO;
    [o_dropzone_view removeFromSuperview];
    [o_playlist_table setHidden: NO];
}

- (void)hideSplitView:(BOOL)b_with_resize
{
    if (b_with_resize) {
        NSRect winrect = [self frame];
        f_lastSplitViewHeight = [o_split_view frame].size.height;
        winrect.size.height = winrect.size.height - f_lastSplitViewHeight;
        winrect.origin.y = winrect.origin.y + f_lastSplitViewHeight;
        [self setFrame: winrect display: YES animate: YES];
    }

    [self performSelector:@selector(hideDropZone) withObject:nil afterDelay:0.1];
    if (b_dark_interface) {
        [self setContentMinSize: NSMakeSize(604., [o_controls_bar height] + [o_titlebar_view frame].size.height)];
        [self setContentMaxSize: NSMakeSize(FLT_MAX, [o_controls_bar height] + [o_titlebar_view frame].size.height)];
    } else {
        [self setContentMinSize: NSMakeSize(604., [o_controls_bar height])];
        [self setContentMaxSize: NSMakeSize(FLT_MAX, [o_controls_bar height])];
    }

    b_splitview_removed = YES;
}

- (void)showSplitView:(BOOL)b_with_resize
{
    [self updateWindow];
    if (b_dark_interface)
        [self setContentMinSize:NSMakeSize(604., 288. + [o_titlebar_view frame].size.height)];
    else
        [self setContentMinSize:NSMakeSize(604., 288.)];
    [self setContentMaxSize: NSMakeSize(FLT_MAX, FLT_MAX)];

    if (b_with_resize) {
        NSRect winrect;
        winrect = [self frame];
        winrect.size.height = winrect.size.height + f_lastSplitViewHeight;
        winrect.origin.y = winrect.origin.y - f_lastSplitViewHeight;
        [self setFrame: winrect display: YES animate: YES];
    }

    [self performSelector:@selector(resizePlaylistAfterCollapse) withObject: nil afterDelay:0.75];

    b_splitview_removed = NO;
}

- (void)updateTimeSlider
{
    [o_controls_bar updateTimeSlider];
    [o_fspanel updatePositionAndTime];

    [[[VLCMain sharedInstance] voutController] updateWindowsControlsBarWithSelector:@selector(updateTimeSlider)];
}

- (void)updateName
{
    input_thread_t * p_input;
    p_input = pl_CurrentInput(VLCIntf);
    if (p_input) {
        NSString *aString;

        if (!config_GetPsz(VLCIntf, "video-title")) {
            char *format = var_InheritString(VLCIntf, "input-title-format");
            char *formated = str_format_meta(p_input, format);
            free(format);
            aString = [NSString stringWithUTF8String:formated];
            free(formated);
        } else
            aString = [NSString stringWithUTF8String:config_GetPsz(VLCIntf, "video-title")];

        char *uri = input_item_GetURI(input_GetItem(p_input));

        NSURL * o_url = [NSURL URLWithString:[NSString stringWithUTF8String:uri]];
        if ([o_url isFileURL]) {
            [self setRepresentedURL: o_url];
            [[[VLCMain sharedInstance] voutController] updateWindowsUsingBlock:^(VLCVideoWindowCommon *o_window) {
                [o_window setRepresentedURL:o_url];
            }];
        } else {
            [self setRepresentedURL: nil];
            [[[VLCMain sharedInstance] voutController] updateWindowsUsingBlock:^(VLCVideoWindowCommon *o_window) {
                [o_window setRepresentedURL:nil];
            }];
        }
        free(uri);

        if ([aString isEqualToString:@""]) {
            if ([o_url isFileURL])
                aString = [[NSFileManager defaultManager] displayNameAtPath: [o_url path]];
            else
                aString = [o_url absoluteString];
        }

        if ([aString length] > 0) {
            [self setTitle: aString];
            [[[VLCMain sharedInstance] voutController] updateWindowsUsingBlock:^(VLCVideoWindowCommon *o_window) {
                [o_window setTitle:aString];
            }];

            [o_fspanel setStreamTitle: aString];
        } else {
            [self setTitle: _NS("VLC media player")];
            [self setRepresentedURL: nil];
        }

        vlc_object_release(p_input);
    } else {
        [self setTitle: _NS("VLC media player")];
        [self setRepresentedURL: nil];
    }
}

- (void)updateWindow
{
    [o_controls_bar updateControls];
    [[[VLCMain sharedInstance] voutController] updateWindowsControlsBarWithSelector:@selector(updateControls)];

    bool b_seekable = false;

    playlist_t * p_playlist = pl_Get(VLCIntf);
    input_thread_t * p_input = playlist_CurrentInput(p_playlist);
    if (p_input) {
        /* seekable streams */
        b_seekable = var_GetBool(p_input, "can-seek");

        vlc_object_release(p_input);
    }

    [self updateTimeSlider];
    if ([o_fspanel respondsToSelector:@selector(setSeekable:)])
        [o_fspanel setSeekable: b_seekable];

    PL_LOCK;
    if ([[[VLCMain sharedInstance] playlist] currentPlaylistRoot] != p_playlist->p_local_category || p_playlist->p_local_category->i_children > 0)
        [self hideDropZone];
    else
        [self showDropZone];
    PL_UNLOCK;
    [o_sidebar_view setNeedsDisplay:YES];

    [self _updatePlaylistTitle];
}

- (void)setPause
{
    [o_controls_bar setPause];
    [o_fspanel setPause];

    [[[VLCMain sharedInstance] voutController] updateWindowsControlsBarWithSelector:@selector(setPause)];
}

- (void)setPlay
{
    [o_controls_bar setPlay];
    [o_fspanel setPlay];

    [[[VLCMain sharedInstance] voutController] updateWindowsControlsBarWithSelector:@selector(setPlay)];
}

- (void)updateVolumeSlider
{
    [[self controlsBar] updateVolumeSlider];
    [o_fspanel setVolumeLevel: [[VLCCoreInteraction sharedInstance] volume]];
}

#pragma mark -
#pragma mark Video Output handling

- (void)videoplayWillBeStarted
{
    if (!b_fullscreen)
        frameBeforePlayback = [self frame];
}

- (void)setVideoplayEnabled
{
    BOOL b_videoPlayback = [[VLCMain sharedInstance] activeVideoPlayback];
        
    if (!b_videoPlayback) {
        if (!b_nonembedded && (!b_nativeFullscreenMode || (b_nativeFullscreenMode && !b_fullscreen)) && frameBeforePlayback.size.width > 0 && frameBeforePlayback.size.height > 0) {

            // only resize back to minimum view of this is still desired final state
            CGFloat f_threshold_height = f_min_video_height + [o_controls_bar height];
            if(frameBeforePlayback.size.height > f_threshold_height || b_minimized_view) {
                [[self animator] setFrame:frameBeforePlayback display:YES];
            }
        }

        frameBeforePlayback = NSMakeRect(0, 0, 0, 0);

        // update fs button to reflect state for next startup
        if (var_InheritBool(VLCIntf, "fullscreen") || var_GetBool(pl_Get(VLCIntf), "fullscreen")) {
            [o_controls_bar setFullscreenState:YES];
        }

        [self makeFirstResponder: o_playlist_table];
        [[[VLCMain sharedInstance] voutController] updateWindowLevelForHelperWindows: NSNormalWindowLevel];

        // restore alpha value to 1 for the case that macosx-opaqueness is set to < 1
        [self setAlphaValue:1.0];
    }

    if (b_nativeFullscreenMode) {
        if ([self hasActiveVideo] && [self fullscreen]) {
            [[o_controls_bar bottomBarView] setHidden: b_videoPlayback];
            [o_fspanel setActive: nil];
        } else {
            [[o_controls_bar bottomBarView] setHidden: NO];
            [o_fspanel setNonActive: nil];
        }
    }
}

#pragma mark -
#pragma mark Lion native fullscreen handling
- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    [super windowWillEnterFullScreen:notification];

    // update split view frame after removing title bar
    if (b_dark_interface) {
        NSRect frame = [[self contentView] frame];
        frame.origin.y += [o_controls_bar height];
        frame.size.height -= [o_controls_bar height];
        [o_split_view setFrame:frame];
    }
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
    [super windowWillExitFullScreen: notification];

    // update split view frame after readding title bar
    if (b_dark_interface) {
        NSRect frame = [o_split_view frame];
        frame.size.height -= [o_titlebar_view frame].size.height;
        [o_split_view setFrame:frame];
    }
}
#pragma mark -
#pragma mark Fullscreen support

- (void)showFullscreenController
{
    id currentWindow = [NSApp keyWindow];
    if ([currentWindow respondsToSelector:@selector(hasActiveVideo)] && [currentWindow hasActiveVideo]) {
        if ([currentWindow respondsToSelector:@selector(fullscreen)] && [currentWindow fullscreen] && ![[currentWindow videoView] isHidden]) {

            if ([[VLCMain sharedInstance] activeVideoPlayback])
                [o_fspanel fadeIn];
        }
    }

}

#pragma mark -
#pragma mark split view delegate
- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMax ofSubviewAt:(NSInteger)dividerIndex
{
    if (dividerIndex == 0)
        return 300.;
    else
        return proposedMax;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMin ofSubviewAt:(NSInteger)dividerIndex
{
    if (dividerIndex == 0)
        return 100.;
    else
        return proposedMin;
}

- (BOOL)splitView:(NSSplitView *)splitView canCollapseSubview:(NSView *)subview
{
    return ([subview isEqual:o_left_split_view]);
}

- (BOOL)splitView:(NSSplitView *)splitView shouldAdjustSizeOfSubview:(NSView *)subview
{
    if ([subview isEqual:o_left_split_view])
        return NO;
    return YES;
}

- (void)mainSplitViewDidResizeSubviews:(id)object
{
    f_lastLeftSplitViewWidth = [o_left_split_view frame].size.width;
    config_PutInt(VLCIntf, "macosx-show-sidebar", ![o_split_view isSubviewCollapsed:o_left_split_view]);
    [[[VLCMain sharedInstance] mainMenu] updateSidebarMenuItem];
}

- (void)toggleLeftSubSplitView
{
    [o_split_view adjustSubviews];
    if ([o_split_view isSubviewCollapsed:o_left_split_view])
        [o_split_view setPosition:f_lastLeftSplitViewWidth ofDividerAtIndex:0];
    else
        [o_split_view setPosition:[o_split_view minPossiblePositionOfDividerAtIndex:0] ofDividerAtIndex:0];
    [[[VLCMain sharedInstance] mainMenu] updateSidebarMenuItem];
}

#pragma mark -
#pragma mark private playlist magic
- (void)_updatePlaylistTitle
{
    playlist_t * p_playlist = pl_Get(VLCIntf);
    PL_LOCK;
    playlist_item_t * currentPlaylistRoot = [[[VLCMain sharedInstance] playlist] currentPlaylistRoot];
    PL_UNLOCK;
    if (currentPlaylistRoot == p_playlist->p_local_category || currentPlaylistRoot == p_playlist->p_ml_category) {
        if (currentPlaylistRoot == p_playlist->p_local_category)
            [o_chosen_category_lbl setStringValue: [_NS("Playlist") stringByAppendingString:[self _playbackDurationOfNode:p_playlist->p_local_category]]];
        else
            [o_chosen_category_lbl setStringValue: [_NS("Media Library") stringByAppendingString:[self _playbackDurationOfNode:p_playlist->p_ml_category]]];
    }
}

- (NSString *)_playbackDurationOfNode:(playlist_item_t*)node
{
    if (!node)
        return @"";

    playlist_t * p_playlist = pl_Get(VLCIntf);
    PL_LOCK;
    mtime_t mt_duration = playlist_GetNodeDuration( node );
    PL_UNLOCK;

    if (mt_duration < 1)
        return @"";

    mt_duration = mt_duration / 1000000;

    NSDate *date = [NSDate dateWithTimeIntervalSince1970:mt_duration];
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"HH:mm:ss"];
    [formatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];

    NSString *playbackDuration = [NSString stringWithFormat:@" — %@",[formatter stringFromDate:date]];
    [formatter release];
    return playbackDuration;
}

#pragma mark -
#pragma mark Side Bar Data handling
/* taken under BSD-new from the PXSourceList sample project, adapted for VLC */
- (NSUInteger)sourceList:(PXSourceList*)sourceList numberOfChildrenOfItem:(id)item
{
    //Works the same way as the NSOutlineView data source: `nil` means a parent item
    if (item==nil)
        return [o_sidebaritems count];
    else
        return [[item children] count];
}


- (id)sourceList:(PXSourceList*)aSourceList child:(NSUInteger)index ofItem:(id)item
{
    //Works the same way as the NSOutlineView data source: `nil` means a parent item
    if (item==nil)
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
    playlist_t * p_playlist = pl_Get(VLCIntf);
    NSInteger i_playlist_size = 0;

    if ([[item identifier] isEqualToString: @"playlist"]) {
        PL_LOCK;
        i_playlist_size = p_playlist->p_local_category->i_children;
        PL_UNLOCK;

        return i_playlist_size;
    }
    if ([[item identifier] isEqualToString: @"medialibrary"]) {
        PL_LOCK;
        if (p_playlist->p_ml_category)
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
    if ([theEvent type] == NSRightMouseDown || ([theEvent type] == NSLeftMouseDown && ([theEvent modifierFlags] & NSControlKeyMask) == NSControlKeyMask)) {
        if (item != nil) {
            if ([item sdtype] > 0)
            {
                NSMenu *m = [[NSMenu alloc] init];
                playlist_t * p_playlist = pl_Get(VLCIntf);
                BOOL sd_loaded = playlist_IsServicesDiscoveryLoaded(p_playlist, [[item identifier] UTF8String]);
                if (!sd_loaded)
                    [m addItemWithTitle:_NS("Enable") action:@selector(sdmenuhandler:) keyEquivalent:@""];
                else
                    [m addItemWithTitle:_NS("Disable") action:@selector(sdmenuhandler:) keyEquivalent:@""];
                [[m itemAtIndex:0] setRepresentedObject: [item identifier]];

                return [m autorelease];
            }
        }
    }

    return nil;
}

- (IBAction)sdmenuhandler:(id)sender
{
    NSString * identifier = [sender representedObject];
    if ([identifier length] > 0 && ![identifier isEqualToString:@"lua{sd='freebox',longname='Freebox TV'}"]) {
        playlist_t * p_playlist = pl_Get(VLCIntf);
        BOOL sd_loaded = playlist_IsServicesDiscoveryLoaded(p_playlist, [identifier UTF8String]);

        if (!sd_loaded)
            playlist_ServicesDiscoveryAdd(p_playlist, [identifier UTF8String]);
        else
            playlist_ServicesDiscoveryRemove(p_playlist, [identifier UTF8String]);
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
    playlist_t * p_playlist = pl_Get(VLCIntf);

    NSIndexSet *selectedIndexes = [o_sidebar_view selectedRowIndexes];
    id item = [o_sidebar_view itemAtRow:[selectedIndexes firstIndex]];

    //Set the label text to represent the new selection
    if ([item sdtype] > -1 && [[item identifier] length] > 0) {
        BOOL sd_loaded = playlist_IsServicesDiscoveryLoaded(p_playlist, [[item identifier] UTF8String]);
        if (!sd_loaded)
            playlist_ServicesDiscoveryAdd(p_playlist, [[item identifier] UTF8String]);
    }

    [o_chosen_category_lbl setStringValue:[item title]];

    if ([[item identifier] isEqualToString:@"playlist"]) {
        [[[VLCMain sharedInstance] playlist] setPlaylistRoot:p_playlist->p_local_category];
        [o_chosen_category_lbl setStringValue: [[o_chosen_category_lbl stringValue] stringByAppendingString:[self _playbackDurationOfNode:p_playlist->p_local_category]]];
    } else if ([[item identifier] isEqualToString:@"medialibrary"]) {
        if (p_playlist->p_ml_category) {
            [[[VLCMain sharedInstance] playlist] setPlaylistRoot:p_playlist->p_ml_category];
            [o_chosen_category_lbl setStringValue: [[o_chosen_category_lbl stringValue] stringByAppendingString:[self _playbackDurationOfNode:p_playlist->p_ml_category]]];
        }
    } else {
        playlist_item_t * pl_item;
        PL_LOCK;
        pl_item = playlist_ChildSearchName(p_playlist->p_root, [[item untranslatedTitle] UTF8String]);
        PL_UNLOCK;
        [[[VLCMain sharedInstance] playlist] setPlaylistRoot: pl_item];
    }

    PL_LOCK;
    if ([[[VLCMain sharedInstance] playlist] currentPlaylistRoot] != p_playlist->p_local_category || p_playlist->p_local_category->i_children > 0)
        [self hideDropZone];
    else
        [self showDropZone];
    PL_UNLOCK;

    if ([[item identifier] isEqualToString:@"podcast{longname=\"Podcasts\"}"])
        [self showPodcastControls];
    else
        [self hidePodcastControls];

    [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCMediaKeySupportSettingChanged"
                                                        object: nil
                                                      userInfo: nil];
}

- (NSDragOperation)sourceList:(PXSourceList *)aSourceList validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(NSInteger)index
{
    if ([[item identifier] isEqualToString:@"playlist"] || [[item identifier] isEqualToString:@"medialibrary"]) {
        NSPasteboard *o_pasteboard = [info draggingPasteboard];
        if ([[o_pasteboard types] containsObject: @"VLCPlaylistItemPboardType"] || [[o_pasteboard types] containsObject: NSFilenamesPboardType])
            return NSDragOperationGeneric;
    }
    return NSDragOperationNone;
}

- (BOOL)sourceList:(PXSourceList *)aSourceList acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(NSInteger)index
{
    NSPasteboard *o_pasteboard = [info draggingPasteboard];

    playlist_t * p_playlist = pl_Get(VLCIntf);
    playlist_item_t *p_node;

    if ([[item identifier] isEqualToString:@"playlist"])
        p_node = p_playlist->p_local_category;
    else
        p_node = p_playlist->p_ml_category;

    if ([[o_pasteboard types] containsObject: NSFilenamesPboardType]) {
        NSArray *o_values = [[o_pasteboard propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector: @selector(caseInsensitiveCompare:)];
        NSUInteger count = [o_values count];
        NSMutableArray *o_array = [NSMutableArray arrayWithCapacity:count];

        for(NSUInteger i = 0; i < count; i++) {
            NSDictionary *o_dic;
            char *psz_uri = vlc_path2uri([[o_values objectAtIndex:i] UTF8String], NULL);
            if (!psz_uri)
                continue;

            o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];

            free(psz_uri);

            [o_array addObject: o_dic];
        }

        [[[VLCMain sharedInstance] playlist] appendNodeArray:o_array inNode: p_node atPos:-1 enqueue:YES];
        return YES;
    }
    else if ([[o_pasteboard types] containsObject: @"VLCPlaylistItemPboardType"]) {
        NSArray * array = [[[VLCMain sharedInstance] playlist] draggedItems];

        NSUInteger count = [array count];
        playlist_item_t * p_item = NULL;

        PL_LOCK;
        for(NSUInteger i = 0; i < count; i++) {
            p_item = [[array objectAtIndex:i] pointerValue];
            if (!p_item) continue;
            playlist_NodeAddCopy(p_playlist, p_item, p_node, PLAYLIST_END);
        }
        PL_UNLOCK;

        return YES;
    }
    return NO;
}

- (id)sourceList:(PXSourceList *)aSourceList persistentObjectForItem:(id)item
{
    return [item identifier];
}

- (id)sourceList:(PXSourceList *)aSourceList itemForPersistentObject:(id)object
{
    /* the following code assumes for sakes of simplicity that only the top level
     * items are allowed to have children */

    NSArray * array = [NSArray arrayWithArray: o_sidebaritems]; // read-only arrays are noticebly faster
    NSUInteger count = [array count];
    if (count < 1)
        return nil;

    for (NSUInteger x = 0; x < count; x++) {
        id item = [array objectAtIndex:x]; // save one objc selector call
        if ([[item identifier] isEqualToString:object])
            return item;
    }

    return nil;
}

#pragma mark -
#pragma mark Podcast

- (IBAction)addPodcast:(id)sender
{
    [NSApp beginSheet:o_podcast_subscribe_window modalForWindow:self modalDelegate:self didEndSelector:NULL contextInfo:nil];
}

- (IBAction)addPodcastWindowAction:(id)sender
{
    [o_podcast_subscribe_window orderOut:sender];
    [NSApp endSheet: o_podcast_subscribe_window];

    if (sender == o_podcast_subscribe_ok_btn && [[o_podcast_subscribe_url_fld stringValue] length] > 0) {
        NSMutableString * podcastConf = [[NSMutableString alloc] init];
        if (config_GetPsz(VLCIntf, "podcast-urls") != NULL)
            [podcastConf appendFormat:@"%s|", config_GetPsz(VLCIntf, "podcast-urls")];

        [podcastConf appendString: [o_podcast_subscribe_url_fld stringValue]];
        config_PutPsz(VLCIntf, "podcast-urls", [podcastConf UTF8String]);
        var_SetString(pl_Get(VLCIntf), "podcast-urls", [podcastConf UTF8String]);
        [podcastConf release];
    }
}

- (IBAction)removePodcast:(id)sender
{
    if (config_GetPsz(VLCIntf, "podcast-urls") != NULL) {
        [o_podcast_unsubscribe_pop removeAllItems];
        [o_podcast_unsubscribe_pop addItemsWithTitles:[[NSString stringWithUTF8String:config_GetPsz(VLCIntf, "podcast-urls")] componentsSeparatedByString:@"|"]];
        [NSApp beginSheet:o_podcast_unsubscribe_window modalForWindow:self modalDelegate:self didEndSelector:NULL contextInfo:nil];
    }
}

- (IBAction)removePodcastWindowAction:(id)sender
{
    [o_podcast_unsubscribe_window orderOut:sender];
    [NSApp endSheet: o_podcast_unsubscribe_window];

    if (sender == o_podcast_unsubscribe_ok_btn) {
        NSMutableArray * urls = [[NSMutableArray alloc] initWithArray:[[NSString stringWithUTF8String:config_GetPsz(VLCIntf, "podcast-urls")] componentsSeparatedByString:@"|"]];
        [urls removeObjectAtIndex: [o_podcast_unsubscribe_pop indexOfSelectedItem]];
        config_PutPsz(VLCIntf, "podcast-urls", [[urls componentsJoinedByString:@"|"] UTF8String]);
        var_SetString(pl_Get(VLCIntf), "podcast-urls", config_GetPsz(VLCIntf, "podcast-urls"));
        [urls release];

        /* reload the podcast module, since it won't update its list when removing podcasts */
        playlist_t * p_playlist = pl_Get(VLCIntf);
        if (playlist_IsServicesDiscoveryLoaded(p_playlist, "podcast{longname=\"Podcasts\"}")) {
            playlist_ServicesDiscoveryRemove(p_playlist, "podcast{longname=\"Podcasts\"}");
            playlist_ServicesDiscoveryAdd(p_playlist, "podcast{longname=\"Podcasts\"}");
            [[[VLCMain sharedInstance] playlist] playlistUpdated];
        }
    }
}

- (void)showPodcastControls
{
    NSRect podcastViewDimensions = [o_podcast_view frame];
    NSRect rightSplitRect = [o_right_split_view frame];
    NSRect playlistTableRect = [o_playlist_table frame];

    podcastViewDimensions.size.width = rightSplitRect.size.width;
    podcastViewDimensions.origin.x = podcastViewDimensions.origin.y = .0;
    [o_podcast_view setFrame:podcastViewDimensions];

    playlistTableRect.origin.y = playlistTableRect.origin.y + podcastViewDimensions.size.height;
    playlistTableRect.size.height = playlistTableRect.size.height - podcastViewDimensions.size.height;
    [o_playlist_table setFrame:playlistTableRect];
    [o_playlist_table setNeedsDisplay:YES];

    [o_right_split_view addSubview: o_podcast_view positioned: NSWindowAbove relativeTo: o_right_split_view];
    b_podcastView_displayed = YES;
}

- (void)hidePodcastControls
{
    if (b_podcastView_displayed) {
        NSRect podcastViewDimensions = [o_podcast_view frame];
        NSRect playlistTableRect = [o_playlist_table frame];

        playlistTableRect.origin.y = playlistTableRect.origin.y - podcastViewDimensions.size.height;
        playlistTableRect.size.height = playlistTableRect.size.height + podcastViewDimensions.size.height;

        [o_podcast_view removeFromSuperviewWithoutNeedingDisplay];
        [o_playlist_table setFrame: playlistTableRect];
        b_podcastView_displayed = NO;
    }
}

@end

@implementation VLCDetachedVideoWindow

- (void)awakeFromNib
{
    // sets lion fullscreen behaviour
    [super awakeFromNib];
    [self setAcceptsMouseMovedEvents: YES];

    if (b_dark_interface) {
        [self setBackgroundColor: [NSColor clearColor]];

        [self setOpaque: NO];
        [self display];
        [self setHasShadow:NO];
        [self setHasShadow:YES];

        NSRect winrect = [self frame];
        CGFloat f_titleBarHeight = [o_titlebar_view frame].size.height;

        [self setTitle: _NS("VLC media player")];
        [o_titlebar_view setFrame: NSMakeRect(0, winrect.size.height - f_titleBarHeight, winrect.size.width, f_titleBarHeight)];
        [[self contentView] addSubview: o_titlebar_view positioned: NSWindowAbove relativeTo: nil];

    } else {
        [self setBackgroundColor: [NSColor blackColor]];
    }

    NSRect videoViewRect = [[self contentView] bounds];
    if (b_dark_interface)
        videoViewRect.size.height -= [o_titlebar_view frame].size.height;
    CGFloat f_bottomBarHeight = [[self controlsBar] height];
    videoViewRect.size.height -= f_bottomBarHeight;
    videoViewRect.origin.y = f_bottomBarHeight;
    [o_video_view setFrame: videoViewRect];

    if (b_dark_interface) {
        o_color_backdrop = [[VLCColorView alloc] initWithFrame: [o_video_view frame]];
        [[self contentView] addSubview: o_color_backdrop positioned: NSWindowBelow relativeTo: o_video_view];
        [o_color_backdrop setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];

        [self setContentMinSize: NSMakeSize(363., f_min_video_height + [[self controlsBar] height] + [o_titlebar_view frame].size.height)];
    } else {
        [self setContentMinSize: NSMakeSize(363., f_min_video_height + [[self controlsBar] height])];
    }
}

- (void)dealloc
{
    if (b_dark_interface)
        [o_color_backdrop release];

    [super dealloc];
}

@end
