/*****************************************************************************
 * VLCStatusBarIcon.m: Status bar icon controller/delegate
 *****************************************************************************
 * Copyright (C) 2016-2019 VLC authors and VideoLAN
 *
 * Authors: Goran Dokic <vlc at 8hz dot com>
 *          Felix Paul Kühne <fkuehne # videolan.org>
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

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "library/VLCInputItem.h"
#import "windows/VLCDetachedAudioWindow.h"

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
    IBOutlet NSButton *randomButton;

    /* Outlets for menu items */
    IBOutlet NSMenuItem *pathActionItem;
    IBOutlet NSMenuItem *showMainWindowItem;
    IBOutlet NSMenuItem *quitItem;

    BOOL _showTimeElapsed;
    NSString *_currentPlaybackUrl;

    VLCDetachedAudioWindow *_detachedAudioWindow;
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
    randomButton.accessibilityLabel = _NS("Toggle random order playback");

    // Populate menu items with localized strings
    [showMainWindowItem setTitle:_NS("Show Main Window")];
    [pathActionItem setTitle:_NS("Path/URL Action")];
    [quitItem setTitle:_NS("Quit")];

    _showTimeElapsed = YES;

    // Set our selves up as delegate, to receive menuNeedsUpdate messages, so
    // we can update our menu as needed/before it's drawn
    [_vlcStatusBarIconMenu setDelegate:self];
    
    // Register notifications
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(updateTimeAndPosition:)
                               name:VLCPlayerTimeAndPositionChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(inputItemChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(hasPreviousChanged:)
                               name:VLCPlaybackHasPreviousChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(hasNextChanged:)
                               name:VLCPlaybackHasNextChanged
                             object:nil];

    [notificationCenter addObserver:self
                           selector:@selector(configurationChanged:)
                               name:VLCConfigurationChangedNotification
                             object:nil];

    [self inputItemChanged:nil];

    [self setMetadataTitle:_NS("VLC media player") artist:_NS("Nothing playing") album:nil andCover:[NSImage imageNamed:@"noart.png"]];
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

- (void)configurationChanged:(NSNotification *)aNotification
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
    [self updateMenuItemRandom];
    [self updateDynamicMenuItemText];
}

/* Callback to update current playback time
 * Called by InputManager
 */
- (void)updateTimeAndPosition:(NSNotification *)aNotification
{
    VLCPlayerController *playerController = aNotification.object;

    VLCInputItem *inputItem = playerController.currentMedia;

    if (inputItem) {
        vlc_tick_t duration = inputItem.duration;
        vlc_tick_t time = playerController.time;

        if (duration == 0) {
            /* Infinite duration */
            [progressField setStringValue:[NSString stringWithDuration:duration currentTime:time negative:NO]];
            [totalField setStringValue:@"∞"];
        } else {
            /* Not unknown, update displayed duration */
            if (_showTimeElapsed) {
                [progressField setStringValue:[NSString stringWithDuration:duration currentTime:time negative:NO]];
            } else {
                [progressField setStringValue:[NSString stringWithDuration:duration currentTime:time negative:YES]];
            }

            [totalField setStringValue:[NSString stringWithTimeFromTicks:duration]];
        }
        [self setStoppedStatus:NO];
    } else {
        /* Nothing playing */
        [progressField setStringValue:@"--:--"];
        [totalField setStringValue:@"--:--"];
        [self setStoppedStatus:YES];
    }
}

#pragma mark -
#pragma mark Update functions

- (void)updateCachedURLOfCurrentMedia:(VLCInputItem *)inputItem
{
    if (!inputItem) {
        _currentPlaybackUrl = nil;
        return;
    }

    _currentPlaybackUrl = inputItem.decodedMRL;
}

- (void)hasPreviousChanged:(NSNotification *)aNotification
{
    backwardsButton.enabled = [[VLCMain sharedInstance] playlistController].hasPreviousPlaylistItem;
}

- (void)hasNextChanged:(NSNotification *)aNotification
{
    forwardButton.enabled = [[VLCMain sharedInstance] playlistController].hasNextPlaylistItem;
}

/* Updates the Metadata for the currently
 * playing item or resets it if nothing is playing
 */
- (void)inputItemChanged:(NSNotification *)aNotification
{
    NSImage         *coverArtImage;
    NSString        *title;
    NSString        *nowPlaying;
    NSString        *artist;
    NSString        *album;

    VLCPlayerController *playerController = aNotification.object;
    enum vlc_player_state playerState = playerController.playerState;
    VLCInputItem *inputItem = playerController.currentMedia;

    switch (playerState) {
        case VLC_PLAYER_STATE_PLAYING:
            [self setStoppedStatus:NO];
            [self setProgressTimeEnabled:YES];
            [pathActionItem setEnabled:YES];
            [self updateCachedURLOfCurrentMedia:inputItem];
            break;
        case VLC_PLAYER_STATE_STOPPED:
            [self setStoppedStatus:YES];
            [self setProgressTimeEnabled:NO];
            [pathActionItem setEnabled:NO];
            _currentPlaybackUrl = nil;
            break;
        case VLC_PLAYER_STATE_PAUSED:
            [self setStoppedStatus:NO];
            [self setProgressTimeEnabled:YES];
            [pathActionItem setEnabled:YES];
            [self updateCachedURLOfCurrentMedia:inputItem];
            [playPauseButton setState:NSOffState];
        default:
            break;
    }

    if (inputItem) {
        coverArtImage = [[NSImage alloc] initWithContentsOfURL:inputItem.artworkURL];
        title = inputItem.title;
        nowPlaying = inputItem.nowPlaying;
        artist = inputItem.artist;
        album = inputItem.albumName;
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
    [randomButton setState:[[VLCMain sharedInstance] playlistController].playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM ? NSOnState : NSOffState];
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
- (IBAction)statusBarIconShowMainWindow:(id)sender
{
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
    [(NSWindow *)[[VLCMain sharedInstance] libraryWindow] makeKeyAndOrderFront:sender];
}

// Action: Toggle Play / Pause
- (IBAction)statusBarIconTogglePlayPause:(id)sender
{
    VLCPlaylistController *playlistController = [[VLCMain sharedInstance] playlistController];
    VLCPlayerController *playerController = playlistController.playerController;
    enum vlc_player_state playerState = playerController.playerState;
    if (playerState != VLC_PLAYER_STATE_PAUSED) {
        [playerController pause];
    } else if (playerState == VLC_PLAYER_STATE_PAUSED) {
        [playerController resume];
    } else {
        [playlistController startPlaylist];
    }
}

// Action: Go to next track
- (IBAction)statusBarIconNext:(id)sender
{
    [[[VLCMain sharedInstance] playlistController] playNextItem];
}

// Action: Go to previous track
- (IBAction)statusBarIconPrevious:(id)sender
{
    [[[VLCMain sharedInstance] playlistController] playPreviousItem];
}

// Action: Toggle random playback (shuffle)
- (IBAction)statusBarIconToggleRandom:(id)sender
{
    VLCPlaylistController *playlistController = [[VLCMain sharedInstance] playlistController];
    playlistController.playbackOrder = (playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM) ? VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL : VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
}

// Action: Toggle between elapsed and remaining time
- (IBAction)toggelProgressTime:(id)sender
{
    _showTimeElapsed = (!_showTimeElapsed);
}

// Action: Quit VLC
- (IBAction)quitAction:(id)sender
{
    [[NSApplication sharedApplication] terminate:nil];
}

- (IBAction)statusBarIconShowMiniAudioPlayer:(id)sender
{
    if (!_detachedAudioWindow) {
        NSWindowController *windowController = [[NSWindowController alloc] initWithWindowNibName:@"VLCDetachedAudioWindow"];
        [windowController loadWindow];
        _detachedAudioWindow = (VLCDetachedAudioWindow *)[windowController window];
    }

    [_detachedAudioWindow makeKeyAndOrderFront:sender];
}

@end
