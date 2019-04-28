/*****************************************************************************
 * VLCLibraryWindow.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCLibraryWindow.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "main/VLCMain.h"

#import "playlist/VLCPlaylistTableCellView.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlaylistDataSource.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataSource.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryMenuController.h"

#import "media-source/VLCMediaSourceCollectionViewItem.h"
#import "media-source/VLCMediaSourceDataSource.h"

#import "windows/mainwindow/VLCControlsBarCommon.h"
#import "windows/video/VLCFSPanelController.h"
#import "windows/video/VLCVoutView.h"

static const float f_min_window_width = 604.;
static const float f_min_window_height = 307.;
static const float f_playlist_row_height = 72.;

@interface VLCLibraryWindow ()
{
    VLCPlaylistDataSource *_playlistDataSource;
    VLCLibraryDataSource *_libraryDataSource;
    VLCMediaSourceDataSource *_mediaSourceDataSource;

    VLCPlaylistController *_playlistController;

    NSRect _windowFrameBeforePlayback;

    VLCFSPanelController *_fspanel;
}
@end

@implementation VLCLibraryWindow

- (void)awakeFromNib
{
    VLCMain *mainInstance = [VLCMain sharedInstance];
    _playlistController = [mainInstance playlistController];

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(shouldShowFullscreenController:)
                               name:VLCVideoWindowShouldShowFullscreenController
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelAudioMediaListUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelVideoMediaListUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(shuffleStateUpdated:)
                               name:VLCPlaybackOrderChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(repeatStateUpdated:)
                               name:VLCPlaybackRepeatChanged
                             object:nil];

    if (@available(macOS 10_14, *)) {
        [[NSApplication sharedApplication] addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                               options:0
                                               context:nil];
    }

    _fspanel = [[VLCFSPanelController alloc] init];
    [_fspanel showWindow:self];

    _segmentedTitleControl.segmentCount = 4;
    [_segmentedTitleControl setTarget:self];
    [_segmentedTitleControl setAction:@selector(segmentedControlAction:)];
    [_segmentedTitleControl setLabel:_NS("Music") forSegment:0];
    [_segmentedTitleControl setLabel:_NS("Video") forSegment:1];
    [_segmentedTitleControl setLabel:_NS("Local Network") forSegment:2];
    [_segmentedTitleControl setLabel:_NS("Internet") forSegment:3];
    [_segmentedTitleControl sizeToFit];

    _playlistDataSource = [[VLCPlaylistDataSource alloc] init];
    _playlistDataSource.playlistController = _playlistController;
    _playlistDataSource.tableView = _playlistTableView;
    _playlistController.playlistDataSource = _playlistDataSource;

    _playlistTableView.dataSource = _playlistDataSource;
    _playlistTableView.delegate = _playlistDataSource;
    _playlistTableView.rowHeight = f_playlist_row_height;
    [_playlistTableView reloadData];

    _libraryDataSource = [[VLCLibraryDataSource alloc] init];
    _libraryDataSource.libraryModel = mainInstance.libraryController.libraryModel;
    _libraryCollectionView.dataSource = _libraryDataSource;
    _libraryCollectionView.delegate = _libraryDataSource;
    [_libraryCollectionView registerClass:[VLCLibraryCollectionViewItem class] forItemWithIdentifier:VLCLibraryCellIdentifier];

    _mediaSourceDataSource = [[VLCMediaSourceDataSource alloc] init];
    _mediaSourceCollectionView.dataSource = _mediaSourceDataSource;
    _mediaSourceCollectionView.delegate = _mediaSourceDataSource;
    [_mediaSourceCollectionView registerClass:[VLCMediaSourceCollectionViewItem class] forItemWithIdentifier:VLCMediaSourceCellIdentifier];

    self.upNextLabel.font = [NSFont VLClibrarySectionHeaderFont];
    self.upNextLabel.stringValue = _NS("Up next");
    NSAttributedString *attributedTitle = [[NSAttributedString alloc] initWithString:_NS("Clear queue")
                                                                          attributes:@{NSFontAttributeName : [NSFont VLClibraryButtonFont],
                                                                                       NSForegroundColorAttributeName : [NSColor VLClibraryHighlightColor]}];
    self.clearPlaylistButton.attributedTitle = attributedTitle;
    [self updateColorsBasedOnAppearance];

    [self segmentedControlAction:nil];
    [self repeatStateUpdated:nil];
    [self shuffleStateUpdated:nil];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (@available(macOS 10_14, *)) {
        [[NSApplication sharedApplication] removeObserver:self forKeyPath:@"effectiveAppearance"];
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    [self updateColorsBasedOnAppearance];
}

- (void)updateColorsBasedOnAppearance
{
    if (@available(macOS 10_14, *)) {
        if ([self.effectiveAppearance.name isEqualToString:NSAppearanceNameDarkAqua]) {
            self.upNextLabel.textColor = [NSColor VLClibraryDarkTitleColor];
            self.upNextSeparator.borderColor = [NSColor VLClibrarySeparatorDarkColor];
            self.clearPlaylistSeparator.borderColor = [NSColor VLClibrarySeparatorDarkColor];
        } else {
            self.upNextLabel.textColor = [NSColor VLClibraryLightTitleColor];
            self.upNextSeparator.borderColor = [NSColor VLClibrarySeparatorLightColor];
            self.clearPlaylistSeparator.borderColor = [NSColor VLClibrarySeparatorLightColor];
        }
    } else {
        self.upNextLabel.textColor = [NSColor VLClibraryLightTitleColor];
        self.upNextSeparator.borderColor = [NSColor VLClibrarySeparatorLightColor];
        self.clearPlaylistSeparator.borderColor = [NSColor VLClibrarySeparatorLightColor];
    }
}

#pragma mark - playmode state display and interaction

- (IBAction)shuffleAction:(id)sender
{
    if (_playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL) {
        _playlistController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
    } else {
        _playlistController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
    }
}

- (void)shuffleStateUpdated:(NSNotification *)aNotification
{
    if (_playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL) {
        self.shufflePlaylistButton.image = [NSImage imageNamed:@"shuffleOff"];
    } else {
        self.shufflePlaylistButton.image = [NSImage imageNamed:@"shuffleOn"];
    }
}

- (IBAction)repeatAction:(id)sender
{
    enum vlc_playlist_playback_repeat currentRepeatState = _playlistController.playbackRepeat;
    switch (currentRepeatState) {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
            break;

        default:
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;
            break;
    }
}

- (void)repeatStateUpdated:(NSNotification *)aNotification
{
    enum vlc_playlist_playback_repeat currentRepeatState = _playlistController.playbackRepeat;
    switch (currentRepeatState) {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            self.repeatPlaylistButton.image = [NSImage imageNamed:@"repeatAll"];
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            self.repeatPlaylistButton.image = [NSImage imageNamed:@"repeatOne"];
            break;

        default:
            self.repeatPlaylistButton.image = [NSImage imageNamed:@"repeatOff"];
            break;
    }
}

#pragma mark - misc. user interactions

- (void)segmentedControlAction:(id)sender
{
    switch (_segmentedTitleControl.selectedSegment) {
        case 0:
            _libraryDataSource.libraryModel.libraryMode = VLCLibraryModeAudio;
            _mediaSourceScrollView.hidden = YES;
            _libraryCollectionView.hidden = NO;
            [_libraryCollectionView reloadData];
            break;

        case 1:
            _libraryDataSource.libraryModel.libraryMode = VLCLibraryModeVideo;
            _mediaSourceScrollView.hidden = YES;
            _libraryCollectionView.hidden = NO;
            [_libraryCollectionView reloadData];
            break;

        default:
            _mediaSourceScrollView.hidden = NO;
            _libraryCollectionView.hidden = YES;
            [_mediaSourceCollectionView reloadData];
            break;
    }
}

- (IBAction)playlistDoubleClickAction:(id)sender
{
    NSInteger selectedRow = self.playlistTableView.selectedRow;
    if (selectedRow == -1)
        return;

    [[[VLCMain sharedInstance] playlistController] playItemAtIndex:selectedRow];
}

- (IBAction)clearPlaylist:(id)sender
{
    [_playlistController clearPlaylist];
}

#pragma mark - video output controlling

- (void)videoPlaybackWillBeStarted
{
    if (!self.fullscreen)
        _windowFrameBeforePlayback = [self frame];
}

- (void)enableVideoPlaybackAppearance
{
    [self.videoView setHidden:NO];

    if (self.nativeFullscreenMode) {
        if ([self hasActiveVideo] && [self fullscreen]) {
            [self hideControlsBar];
            [_fspanel shouldBecomeActive:nil];
        }
    }
}

- (void)disableVideoPlaybackAppearance
{
    if (!self.nonembedded
        && (!self.nativeFullscreenMode || (self.nativeFullscreenMode && !self.fullscreen))
        && _windowFrameBeforePlayback.size.width > 0
        && _windowFrameBeforePlayback.size.height > 0) {

        // only resize back to minimum view of this is still desired final state
        CGFloat f_threshold_height = f_min_video_height + [self.controlsBar height];
        if (_windowFrameBeforePlayback.size.height > f_threshold_height) {
            if ([[VLCMain sharedInstance] isTerminating]) {
                [self setFrame:_windowFrameBeforePlayback display:YES];
            } else {
                [[self animator] setFrame:_windowFrameBeforePlayback display:YES];
            }
        }
    }

    _windowFrameBeforePlayback = NSMakeRect(0, 0, 0, 0);

    [self makeFirstResponder: _playlistTableView];
    [[[VLCMain sharedInstance] voutProvider] updateWindowLevelForHelperWindows: NSNormalWindowLevel];

    // restore alpha value to 1 for the case that macosx-opaqueness is set to < 1
    [self setAlphaValue:1.0];
    [self.videoView setHidden:YES];

    if (self.nativeFullscreenMode) {
        [self showControlsBar];
        [_fspanel shouldBecomeInactive:nil];
    }
}

#pragma mark - library representation and interaction
- (void)updateLibraryRepresentation:(NSNotification *)aNotification
{
    [_libraryCollectionView reloadData];
}

#pragma mark -
#pragma mark Fullscreen support

- (void)shouldShowFullscreenController:(NSNotification *)aNotification
{
    id currentWindow = [NSApp keyWindow];
    if ([currentWindow respondsToSelector:@selector(hasActiveVideo)] && [currentWindow hasActiveVideo]) {
        if ([currentWindow respondsToSelector:@selector(fullscreen)] && [currentWindow fullscreen] && ![[currentWindow videoView] isHidden]) {
            if ([[VLCMain sharedInstance] activeVideoPlayback]) {
                [_fspanel fadeIn];
            }
        }
    }

}

@end

@implementation VLCLibraryWindowController

- (instancetype)initWithLibraryWindow
{
    self = [super initWithWindowNibName:@"VLCLibraryWindow"];
    return self;
}

- (void)windowDidLoad
{
    VLCLibraryWindow *window = (VLCLibraryWindow *)self.window;
    [window setRestorable:NO];
    [window setExcludedFromWindowsMenu:YES];
    [window setAcceptsMouseMovedEvents:YES];
    [window setContentMinSize:NSMakeSize(f_min_window_width, f_min_window_height)];
}

@end

@interface VLCLibraryCollectionView()
{
    VLCLibraryMenuController *_menuController;
}

@end

@implementation VLCLibraryCollectionView

-(void)mouseDown:(NSEvent *)theEvent
{
    if (theEvent.modifierFlags & NSControlKeyMask) {
        if (!_menuController) {
            _menuController = [[VLCLibraryMenuController alloc] init];
        }
        [_menuController popupMenuWithEvent:theEvent forView:self];
    }

    [super mouseDown:theEvent];
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    if (!_menuController) {
        _menuController = [[VLCLibraryMenuController alloc] init];
    }
    [_menuController popupMenuWithEvent:theEvent forView:self];

    [super rightMouseDown:theEvent];
}

@end
