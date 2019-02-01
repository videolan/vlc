/*****************************************************************************
 * VLCStatusBarIcon.m: Status bar icon controller/delegate
 *****************************************************************************
 * Copyright (C) 2016-2019 VLC authors and VideoLAN
 *
 * Authors: Goran Dokic <vlc at 8hz dot com>
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

#import "VLCStatusBarIcon.h"

#import <vlc_common.h>
#import <vlc_playlist_legacy.h>
#import <vlc_input.h>

#import "coreinteraction/VLCCoreInteraction.h"
#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"


@interface VLCStatusBarIcon ()
{
    NSMenuItem *_vlcStatusBarMenuItem;

    /* Outlets for Now Playing labels */
    IBOutlet NSTextField *titleField;
    IBOutlet NSTextField *artistField;
    IBOutlet NSTextField *albumField;
    IBOutlet NSTextField *progressField;
    IBOutlet NSTextField *separatorField;
    IBOutlet NSTextField *totalField;
    IBOutlet NSImageView *coverImageView;

    /* Outlets for player controls */
    IBOutlet NSButton *backwardsButton;
    IBOutlet NSButton *playPauseButton;
    IBOutlet NSButton *forwardButton;
    IBOutlet NSButton *randButton;

    /* Outlets for menu items */
    IBOutlet NSMenuItem *pathActionItem;
    IBOutlet NSMenuItem *showMainWindowItem;
    IBOutlet NSMenuItem *quitItem;

    BOOL isStopped;
    BOOL showTimeElapsed;
    NSString *_currentPlaybackUrl;
}
@end

#pragma mark -
#pragma mark Implementation

@implementation VLCStatusBarIcon

#pragma mark -
#pragma mark Init

- (instancetype)init
{
    self = [super init];
    if (self) {
        [[NSBundle mainBundle] loadNibNamed:@"VLCStatusBarIconMainMenu" owner:self topLevelObjects:nil];
    }
    return self;
}

- (void)awakeFromNib
{
    [super awakeFromNib];

    [_controlsView setAutoresizingMask:NSViewWidthSizable];
    [_playbackInfoView setAutoresizingMask:NSViewWidthSizable];

    [self configurationChanged:nil];

    // Set Accessibility Attributes for Image Buttons
    backwardsButton.accessibilityLabel = _NS("Go to previous item");
    playPauseButton.accessibilityLabel = _NS("Toggle Play/Pause");
    forwardButton.accessibilityLabel = _NS("Go to next item");
    randButton.accessibilityLabel = _NS("Toggle random order playback");

    // Populate menu items with localized strings
    [showMainWindowItem setTitle:_NS("Show Main Window")];
    [pathActionItem setTitle:_NS("Path/URL Action")];
    [quitItem setTitle:_NS("Quit")];

    showTimeElapsed = YES;

    // Set our selves up as delegate, to receive menuNeedsUpdate messages, so
    // we can update our menu as needed/before it's drawn
    [_vlcStatusBarIconMenu setDelegate:self];
    
    // Register notifications
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(updateNowPlayingInfo)
                               name:VLCInputChangedNotification
                             object:nil];

    [notificationCenter addObserver:self
                           selector:@selector(configurationChanged:)
                               name:VLCConfigurationChangedNotification
                             object:nil];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    if ([keyPath isEqualToString: NSStringFromSelector(@selector(isVisible))]) {
        bool isVisible = [[change objectForKey:NSKeyValueChangeNewKey] boolValue];

        // Sync status bar visibility with VLC setting
        msg_Dbg(getIntf(), "Status bar icon visibility changed to %i", isVisible);
        config_PutInt("macosx-statusicon", isVisible ? 1 : 0);
    } else {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
    }
}

- (void)configurationChanged:(id)obj
{
    if (var_InheritBool(getIntf(), "macosx-statusicon"))
        [self enableMenuIcon];
    else
        [self disableStatusItem];
}

/* Enables the Status Bar Item and initializes it's image
 * and context menu
 */
- (void)enableMenuIcon
{
    if (!self.statusItem) {
        // Init the status item
        self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
        [self.statusItem setHighlightMode:YES];
        [self.statusItem setEnabled:YES];

        // Set the status item image
        NSImage *menuIcon = [NSImage imageNamed:@"VLCStatusBarIcon"];
        [menuIcon setTemplate:YES];
        [self.statusItem setImage:menuIcon];

        // Attach pull-down menu
        [self.statusItem setMenu:_vlcStatusBarIconMenu];

        if (@available(macOS 10.12, *)) {
            [self.statusItem setBehavior:NSStatusItemBehaviorRemovalAllowed];
            [self.statusItem setAutosaveName:@"statusBarItem"];
            [self.statusItem addObserver:self forKeyPath:NSStringFromSelector(@selector(isVisible))
                                 options:NSKeyValueObservingOptionNew context:NULL];
        }
    }

    if (@available(macOS 10.12, *)) {
        // Sync VLC setting with status bar visibility setting (10.12 runtime only)
        [self.statusItem setVisible:YES];
    }
}

- (void)disableStatusItem
{
    if (!self.statusItem)
        return;

    // Lets keep alive the object in Sierra, and destroy it in older OS versions
    if (@available(macOS 10.12, *)) {
        self.statusItem.visible = NO;
    } else {
        [[NSStatusBar systemStatusBar] removeStatusItem:self.statusItem];
        self.statusItem = nil;
    }
}

- (void)dealloc
{
    if (self.statusItem && [self.statusItem respondsToSelector:@selector(isVisible)]) {
        [self.statusItem removeObserver:self forKeyPath:NSStringFromSelector(@selector(isVisible)) context:NULL];
    }

    // Cleanup
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark -
#pragma mark Event callback functions

/* Menu update delegate
 * Called before menu is opened/displayed
 */
- (void)menuNeedsUpdate:(NSMenu *)menu
{
    [self updateMetadata];
    [self updateMenuItemRandom];
    [self updateDynamicMenuItemText];
}

/* This is called whenever the playback status for VLC changes and here
 * we can update our information in the menu/view
 */
- (void) updateNowPlayingInfo
{
    [self updateMetadata];
    [self updateProgress];
    [self updateDynamicMenuItemText];
}

/* Callback to update current playback time
 * Called by InputManager
 */
- (void)updateProgress
{
    input_thread_t *input = pl_CurrentInput(getIntf());

    if (input) {
        NSString *elapsedTime;
        NSString *remainingTime;
        NSString *totalTime;

        /* Get elapsed and remaining time */
        elapsedTime = [NSString stringWithTimeFromInput:input negative:NO];
        remainingTime = [NSString stringWithTimeFromInput:input negative:YES];

        /* Check item duration */
        vlc_tick_t dur = input_item_GetDuration(input_GetItem(input));

        if (dur == -1) {
            /* Unknown duration, possibly due to buffering */
            [progressField setStringValue:@"--:--"];
            [totalField setStringValue:@"--:--"];
        } else if (dur == 0) {
            /* Infinite duration */
            [progressField setStringValue:elapsedTime];
            [totalField setStringValue:@"∞"];
        } else {
            /* Not unknown, update displayed duration */
            totalTime = [NSString stringWithTime:SEC_FROM_VLC_TICK(dur)];
            [progressField setStringValue:(showTimeElapsed) ? elapsedTime : remainingTime];
            [totalField setStringValue:totalTime];
        }
        [self setStoppedStatus:NO];
        vlc_object_release(input);
    } else {
        /* Nothing playing */
        [progressField setStringValue:@"--:--"];
        [totalField setStringValue:@"--:--"];
        [self setStoppedStatus:YES];
    }
}


#pragma mark -
#pragma mark Update functions

/* Updates the Metadata for the currently
 * playing item or resets it if nothing is playing
 */
- (void)updateMetadata
{
    NSImage         *coverArtImage;
    NSString        *title;
    NSString        *nowPlaying;
    NSString        *artist;
    NSString        *album;
    input_thread_t  *input = pl_CurrentInput(getIntf());
    input_item_t    *item  = NULL;

    // Update play/pause status
    switch ([self getPlaylistPlayStatus]) {
        case PLAYLIST_RUNNING:
            [self setStoppedStatus:NO];
            [self setProgressTimeEnabled:YES];
            [pathActionItem setEnabled:YES];
            _currentPlaybackUrl = [[[VLCCoreInteraction sharedInstance]
                                    URLOfCurrentPlaylistItem] absoluteString];
            break;
        case PLAYLIST_STOPPED:
            [self setStoppedStatus:YES];
            [self setProgressTimeEnabled:NO];
            [pathActionItem setEnabled:NO];
            _currentPlaybackUrl = nil;
            break;
        case PLAYLIST_PAUSED:
            [self setStoppedStatus:NO];
            [self setProgressTimeEnabled:YES];
            [pathActionItem setEnabled:YES];
            _currentPlaybackUrl = [[[VLCCoreInteraction sharedInstance]
                                    URLOfCurrentPlaylistItem] absoluteString];
            [playPauseButton setState:NSOffState];
        default:
            break;
    }

    if (input) {
        item = input_GetItem(input);
    }

    if (item) {
        /* Something is playing */
        static char *tmp_cstr = NULL;

        // Get Coverart
        tmp_cstr = input_item_GetArtworkURL(item);
        if (tmp_cstr) {
            NSString *tempStr = toNSStr(tmp_cstr);
            if (![tempStr hasPrefix:@"attachment://"]) {
                coverArtImage = [[NSImage alloc]
                                 initWithContentsOfURL:[NSURL URLWithString:tempStr]];
            }
            FREENULL(tmp_cstr);
        }

        // Get Titel
        tmp_cstr = input_item_GetTitleFbName(item);
        if (tmp_cstr) {
            title = toNSStr(tmp_cstr);
            FREENULL(tmp_cstr);
        }

        // Get Now Playing
        tmp_cstr = input_item_GetNowPlaying(item);
        if (tmp_cstr) {
            nowPlaying = toNSStr(tmp_cstr);
            FREENULL(tmp_cstr);
        }

        // Get author
        tmp_cstr = input_item_GetArtist(item);
        if (tmp_cstr) {
            artist = toNSStr(tmp_cstr);
            FREENULL(tmp_cstr);
        }

        // Get album
        tmp_cstr = input_item_GetAlbum(item);
        if (tmp_cstr) {
            album = toNSStr(tmp_cstr);
            FREENULL(tmp_cstr);
        }
    } else {
        /* Nothing playing */
        title = _NS("VLC media player");
        artist = _NS("Nothing playing");
    }

    // Set fallback coverart
    if (!coverArtImage) {
        coverArtImage = [NSImage imageNamed:@"noart.png"];
    }

    // Hack to show now playing for streams (ICY)
    if (nowPlaying && !artist) {
        artist = nowPlaying;
    }

    // Set the metadata in the UI
    [self setMetadataTitle:title artist:artist album:album andCover:coverArtImage];

    // Cleanup
    if (input)
        vlc_object_release(input);
}



// Update dynamic copy/open menu item status
- (void)updateDynamicMenuItemText
{
    if (!_currentPlaybackUrl) {
        pathActionItem.hidden = YES;
        return;
    }

    NSURL *itemURI = [NSURL URLWithString:_currentPlaybackUrl];
    pathActionItem.hidden = NO;

    if ([itemURI.scheme isEqualToString:@"file"]) {
        [pathActionItem setTitle:_NS("Reveal in Finder")];
    } else {
        [pathActionItem setTitle:_NS("Copy URL to clipboard")];
    }
}

// Update the random menu item status
- (void)updateMenuItemRandom
{
    // Get current random status
    bool random;
    playlist_t *playlist = pl_Get(getIntf());
    random = var_GetBool(playlist, "random");

    [randButton setState:(random) ? NSOnState : NSOffState];
}

#pragma mark -
#pragma mark Utility functions

/* Update the UI to the specified metadata
 * Any of the values can be nil and will be replaced with empty strings
 * or no cover Image at all
 */
- (void)setMetadataTitle:(NSString *)title
                  artist:(NSString *)artist
                   album:(NSString *)album
                andCover:(NSImage *)cover
{
    [titleField setStringValue:(title) ? title : @""];
    [artistField setStringValue:(artist) ? artist : @""];
    [albumField setStringValue:(album) ? album : @""];
    [coverImageView setImage:cover];
}

// Set the play/pause menu item status
- (void)setStoppedStatus:(BOOL)stopped
{
    isStopped = stopped;
    if (stopped) {
        [playPauseButton setState:NSOffState];
    } else {
        [playPauseButton setState:NSOnState];
    }
}

- (void)setProgressTimeEnabled:(BOOL)enabled
{
    [progressField setEnabled:enabled];
    [separatorField setEnabled:enabled];
    [totalField setEnabled:enabled];
}

/* Returns VLC playlist status
 * Check for constants:
 *   PLAYLIST_RUNNING, PLAYLIST_STOPPED, PLAYLIST_PAUSED
 */
- (int)getPlaylistPlayStatus
{
    int res;
    playlist_t *p_playlist = pl_Get(getIntf());

    PL_LOCK;
    res = playlist_Status( p_playlist );
    PL_UNLOCK;

    return res;
}

#pragma mark -
#pragma mark Menu item Actions

/* Action: Select the currently playing file in Finder
 *         or in case of a network stream, copy the URL
 */
- (IBAction)copyOrOpenCurrentPlaybackItem:(id)sender
{
    // If nothing playing, there is nothing to do
    if (!_currentPlaybackUrl) {
        return;
    }

    // Check if path or URL
    NSURL *itemURI = [NSURL URLWithString:_currentPlaybackUrl];

    if ([itemURI.scheme isEqualToString:@"file"]) {
        // Local file, open in Finder
        [[NSWorkspace sharedWorkspace] selectFile:itemURI.path
                         inFileViewerRootedAtPath:itemURI.path];
    } else {
        // URL, copy to pasteboard
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        [pasteboard setString:_currentPlaybackUrl forType:NSPasteboardTypeString];
    }
}

// Action: Show VLC main window
- (IBAction)restoreMainWindow:(id)sender
{
    [[NSApp sharedApplication] activateIgnoringOtherApps:YES];
    [[[VLCMain sharedInstance] mainWindow] makeKeyAndOrderFront:sender];
}

// Action: Toggle Play / Pause
- (IBAction)statusBarIconTogglePlayPause:(id)sender
{
    [[VLCCoreInteraction sharedInstance] playOrPause];
}

// Action: Stop playback
- (IBAction)statusBarIconStop:(id)sender
{
    [[VLCCoreInteraction sharedInstance] stop];
}

// Action: Go to next track
- (IBAction)statusBarIconNext:(id)sender
{
    [[VLCCoreInteraction sharedInstance] next];
}

// Action: Go to previous track
- (IBAction)statusBarIconPrevious:(id)sender
{
    [[VLCCoreInteraction sharedInstance] previous];
}

// Action: Toggle random playback (shuffle)
- (IBAction)statusBarIconToggleRandom:(id)sender
{
    [[VLCCoreInteraction sharedInstance] shuffle];
}

// Action: Toggle between elapsed and remaining time
- (IBAction)toggelProgressTime:(id)sender
{
    showTimeElapsed = (!showTimeElapsed);
}

// Action: Quit VLC
- (IBAction)quitAction:(id)sender
{
    [[NSApplication sharedApplication] terminate:nil];
}

@end
