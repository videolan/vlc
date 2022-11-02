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
#include "VLCLibraryDataTypes.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"
#import "main/VLCMain.h"

#import "playlist/VLCPlayerController.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlaylistDataSource.h"
#import "playlist/VLCPlaylistSortingMenuController.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryAudioDataSource.h"
#import "library/VLCLibraryVideoDataSource.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibrarySortingMenuController.h"
#import "library/VLCLibraryAlbumTableCellView.h"
#import "library/VLCLibraryNavigationStack.h"

#import "media-source/VLCMediaSourceBaseDataSource.h"

#import "views/VLCCustomWindowButton.h"
#import "views/VLCDragDropView.h"
#import "views/VLCRoundedCornerTextField.h"

#import "windows/mainwindow/VLCControlsBarCommon.h"
#import "windows/video/VLCFSPanelController.h"
#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "windows/VLCOpenWindowController.h"
#import "windows/VLCOpenInputMetadata.h"

#import <vlc_common.h>
#import <vlc_url.h>

const CGFloat VLCLibraryWindowMinimalWidth = 604.;
const CGFloat VLCLibraryWindowMinimalHeight = 307.;
const CGFloat VLCLibraryWindowLargePlaylistRowHeight = 60.;
const CGFloat VLCLibraryWindowSmallPlaylistRowHeight = 45.;
const CGFloat VLCLibraryWindowSmallRowHeight = 24.;
const CGFloat VLCLibraryWindowLargeRowHeight = 50.;
const CGFloat VLCLibraryWindowDefaultPlaylistWidth = 340.;
const CGFloat VLCLibraryWindowMinimalPlaylistWidth = 170.;

static NSArray<NSLayoutConstraint *> *videoPlaceholderImageViewSizeConstraints;
static NSArray<NSLayoutConstraint *> *audioPlaceholderImageViewSizeConstraints;
static NSUserInterfaceItemIdentifier const kVLCLibraryWindowIdentifier = @"VLCLibraryWindow";

@interface VLCLibraryWindow () <VLCDragDropTarget, NSSplitViewDelegate>
{
    NSRect _windowFrameBeforePlayback;
    CGFloat _lastPlaylistWidthBeforeCollaps;
    
    NSInteger _currentSelectedSegment;
    NSInteger _currentSelectedViewModeSegment;
}

@property (nonatomic, readwrite, strong) IBOutlet NSView *emptyLibraryView;
@property (nonatomic, readwrite, strong) IBOutlet NSImageView *placeholderImageView;
@property (nonatomic, readwrite, strong) IBOutlet NSTextField *placeholderLabel;
@property (nonatomic, readwrite, strong) IBOutlet VLCCustomEmptyLibraryBrowseButton *placeholderGoToBrowseButton;

- (IBAction)goToBrowseSection:(id)sender;

@end

static int ShowFullscreenController(vlc_object_t *p_this, const char *psz_variable,
                                    vlc_value_t old_val, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] postNotificationName:VLCVideoWindowShouldShowFullscreenController
                                                                object:nil];
        });

        return VLC_SUCCESS;
    }
}

static int ShowController(vlc_object_t *p_this, const char *psz_variable,
                          vlc_value_t old_val, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] postNotificationName:VLCWindowShouldShowController
                                                                object:nil];
        });

        return VLC_SUCCESS;
    }
}

static void addShadow(NSImageView *__unsafe_unretained imageView)
{
    NSShadow *buttonShadow = [[NSShadow alloc] init];

    buttonShadow.shadowBlurRadius = 15.0f;
    buttonShadow.shadowOffset = CGSizeMake(0.0f, -5.0f);
    buttonShadow.shadowColor = [NSColor blackColor];

    imageView.wantsLayer = YES;
    imageView.shadow = buttonShadow;
}

@implementation VLCLibraryWindow

- (void)awakeFromNib
{
    self.identifier = kVLCLibraryWindowIdentifier;
    
    VLCMain *mainInstance = [VLCMain sharedInstance];
    _playlistController = [mainInstance playlistController];

    libvlc_int_t *libvlc = vlc_object_instance(getIntf());
    var_AddCallback(libvlc, "intf-toggle-fscontrol", ShowFullscreenController, (__bridge void *)self);
    var_AddCallback(libvlc, "intf-show", ShowController, (__bridge void *)self);

    self.navigationStack = [[VLCLibraryNavigationStack alloc] init];
    self.navigationStack.delegate = self;

    self.videoView = [[VLCVoutView alloc] initWithFrame:self.mainSplitView.frame];
    self.videoView.hidden = YES;
    
    videoPlaceholderImageViewSizeConstraints = @[
        [NSLayoutConstraint constraintWithItem:_placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:182.f],
        [NSLayoutConstraint constraintWithItem:_placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:114.f],
    ];
    audioPlaceholderImageViewSizeConstraints = @[
        [NSLayoutConstraint constraintWithItem:_placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
        [NSLayoutConstraint constraintWithItem:_placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
    ];

    [self.gridVsListSegmentedControl setToolTip: _NS("Grid View or List View")];
    [self.librarySortButton setToolTip: _NS("Select Sorting Mode")];
    [self.playQueueToggle setToolTip: _NS("Toggle Playqueue")];

    [self.gridVsListSegmentedControl setHidden:NO];
    [self.librarySortButton setHidden:NO];
    [self.librarySearchField setEnabled:YES];

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(shouldShowFullscreenController:)
                               name:VLCVideoWindowShouldShowFullscreenController
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(shouldShowController:)
                               name:VLCWindowShouldShowController
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelAudioMediaListUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelArtistListUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelAlbumListUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelGenreListUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelVideoMediaListUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelRecentMediaListUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(shuffleStateUpdated:)
                               name:VLCPlaybackOrderChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(repeatStateUpdated:)
                               name:VLCPlaybackRepeatChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateViewCellDimensionsBasedOnSetting:)
                               name:VLCConfigurationChangedNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerStateChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerStateChanged:)
                               name:VLCPlayerStateChanged
                             object:nil];

    if (@available(macOS 10.14, *)) {
        [[NSApplication sharedApplication] addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                               options:0
                                               context:nil];
        
        _mediaToolBar.centeredItemIdentifier = _segmentedTitleControlToolbarItem.itemIdentifier;
    }

    _fspanel = [[VLCFSPanelController alloc] init];
    [_fspanel showWindow:self];

    _currentSelectedSegment = -1; // To enforce action on the selected segment
    _segmentedTitleControl.segmentCount = 4;
    [_segmentedTitleControl setTarget:self];
    [_segmentedTitleControl setLabel:_NS("Video") forSegment:VLCLibraryVideoSegment];
    [_segmentedTitleControl setLabel:_NS("Music") forSegment:VLCLibraryMusicSegment];
    [_segmentedTitleControl setLabel:_NS("Browse") forSegment:VLCLibraryBrowseSegment];
    [_segmentedTitleControl setLabel:_NS("Streams") forSegment:VLCLibraryStreamsSegment];
    [_segmentedTitleControl sizeToFit];

    _playlistDragDropView.dropTarget = self;
    _playlistCounterTextField.useStrongRounding = YES;
    _playlistCounterTextField.font = [NSFont VLCplaylistSelectedItemLabelFont];
    _playlistCounterTextField.textColor = [NSColor VLClibraryAnnotationColor];
    _playlistCounterTextField.hidden = YES;

    _playlistDataSource = [[VLCPlaylistDataSource alloc] init];
    _playlistDataSource.playlistController = _playlistController;
    _playlistDataSource.tableView = _playlistTableView;
    _playlistDataSource.dragDropView = _playlistDragDropView;
    _playlistDataSource.counterTextField = _playlistCounterTextField;
    [_playlistDataSource prepareForUse];
    _playlistController.playlistDataSource = _playlistDataSource;

    _playlistTableView.dataSource = _playlistDataSource;
    _playlistTableView.delegate = _playlistDataSource;
    [self updateViewCellDimensionsBasedOnSetting:nil];
    [_playlistTableView reloadData];

    _libraryVideoDataSource = [[VLCLibraryVideoDataSource alloc] init];
    _libraryVideoDataSource.libraryModel = mainInstance.libraryController.libraryModel;
    _libraryVideoDataSource.libraryMediaCollectionView = _videoLibraryCollectionView;
    [_libraryVideoDataSource setupAppearance];

    _libraryAudioDataSource = [[VLCLibraryAudioDataSource alloc] init];
    _libraryAudioDataSource.libraryModel = mainInstance.libraryController.libraryModel;
    _libraryAudioDataSource.collectionSelectionTableView = _audioCollectionSelectionTableView;
    _libraryAudioDataSource.groupSelectionTableView = _audioGroupSelectionTableView;
    _libraryAudioDataSource.segmentedControl = self.audioSegmentedControl;
    _libraryAudioDataSource.collectionView = self.audioLibraryCollectionView;
    _libraryAudioDataSource.placeholderImageView = _placeholderImageView;
    _libraryAudioDataSource.placeholderLabel = _placeholderLabel;
    [_libraryAudioDataSource setupAppearance];
    _audioCollectionSelectionTableView.dataSource = _libraryAudioDataSource;
    _audioCollectionSelectionTableView.delegate = _libraryAudioDataSource;
    _audioCollectionSelectionTableView.rowHeight = VLCLibraryWindowLargeRowHeight;
    _libraryAudioGroupDataSource = [[VLCLibraryGroupDataSource alloc] init];
    _libraryAudioDataSource.groupDataSource = _libraryAudioGroupDataSource;
    _audioGroupSelectionTableView.dataSource = _libraryAudioGroupDataSource;
    _audioGroupSelectionTableView.delegate = _libraryAudioGroupDataSource;
    _audioGroupSelectionTableView.rowHeight = [VLCLibraryAlbumTableCellView defaultHeight];

    if(@available(macOS 11.0, *)) {
        _audioGroupSelectionTableView.style = NSTableViewStyleFullWidth;
    }

    _audioLibraryCollectionView.selectable = YES;
    _audioLibraryCollectionView.allowsMultipleSelection = NO;
    _audioLibraryCollectionView.allowsEmptySelection = YES;

    _mediaSourceDataSource = [[VLCMediaSourceBaseDataSource alloc] init];
    _mediaSourceDataSource.collectionView = _mediaSourceCollectionView;
    _mediaSourceDataSource.collectionViewScrollView = _mediaSourceCollectionViewScrollView;
    _mediaSourceDataSource.homeButton = _mediaSourceHomeButton;
    _mediaSourceDataSource.pathControl = _mediaSourcePathControl;
    _mediaSourceDataSource.gridVsListSegmentedControl = _gridVsListSegmentedControl;
    _mediaSourceTableView.rowHeight = VLCLibraryWindowLargeRowHeight;
    _mediaSourceDataSource.tableView = _mediaSourceTableView;
    [_mediaSourceDataSource setupViews];

    self.upNextLabel.font = [NSFont VLClibrarySectionHeaderFont];
    self.upNextLabel.stringValue = _NS("Playlist");
    [self updateColorsBasedOnAppearance];
    self.openMediaButton.title = _NS("Open media...");
    self.dragDropImageBackgroundBox.fillColor = [NSColor VLClibrarySeparatorLightColor];

    _mainSplitView.delegate = self;
    _lastPlaylistWidthBeforeCollaps = VLCLibraryWindowDefaultPlaylistWidth;

    [self setViewForSelectedSegment];
    [self repeatStateUpdated:nil];
    [self shuffleStateUpdated:nil];
    
    const CGFloat scrollViewTopInset = 16.;
    const CGFloat scrollViewRightInset = 0.;
    const CGFloat scrollViewBottomInset = 16.;
    const CGFloat scrollViewLeftInset = 16.;

    // Need to account for the audio collection switcher at the top
    const CGFloat audioScrollViewTopInset = scrollViewTopInset + 32.;

    const NSEdgeInsets defaultInsets = NSEdgeInsetsMake(scrollViewTopInset,
                                                        scrollViewLeftInset,
                                                        scrollViewBottomInset,
                                                        scrollViewRightInset);
    const NSEdgeInsets audioScrollViewInsets = NSEdgeInsetsMake(audioScrollViewTopInset,
                                                                scrollViewLeftInset,
                                                                scrollViewBottomInset,
                                                                scrollViewRightInset);
    // We might want to give the content some insets, but this will also affect the scrollbars.
    // We need to compensate for this or they will look wrong.
    const NSEdgeInsets scrollerInsets = NSEdgeInsetsMake(-scrollViewTopInset,
                                                        -scrollViewLeftInset,
                                                        -scrollViewBottomInset,
                                                        -scrollViewRightInset);
    
    _audioCollectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioCollectionViewScrollView.contentInsets = audioScrollViewInsets;
    _audioCollectionViewScrollView.scrollerInsets = scrollerInsets;

    _audioCollectionSelectionTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioCollectionSelectionTableViewScrollView.contentInsets = audioScrollViewInsets;
    _audioCollectionSelectionTableViewScrollView.scrollerInsets = scrollerInsets;
    _audioGroupSelectionTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioGroupSelectionTableViewScrollView.contentInsets = audioScrollViewInsets;
    _audioGroupSelectionTableViewScrollView.scrollerInsets = scrollerInsets;
    
    _videoLibraryScrollView.automaticallyAdjustsContentInsets = NO;
    _videoLibraryScrollView.contentInsets = defaultInsets;
    _videoLibraryScrollView.scrollerInsets = scrollerInsets;

    _mediaSourceCollectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _mediaSourceCollectionViewScrollView.contentInsets = defaultInsets;
    _mediaSourceCollectionViewScrollView.scrollerInsets = scrollerInsets;

    _mediaSourceTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _mediaSourceTableViewScrollView.contentInsets = defaultInsets;
    _mediaSourceTableViewScrollView.scrollerInsets = scrollerInsets;

    const CGFloat collectionItemSpacing = 20.;
    const NSEdgeInsets collectionViewSectionInset = NSEdgeInsetsMake(20., 20., 20., 20.);

    NSCollectionViewFlowLayout *audioLibraryCollectionViewLayout = _audioLibraryCollectionView.collectionViewLayout;
    audioLibraryCollectionViewLayout.itemSize = CGSizeMake(214., 260.);
    audioLibraryCollectionViewLayout.minimumLineSpacing = collectionItemSpacing;
    audioLibraryCollectionViewLayout.minimumInteritemSpacing = collectionItemSpacing;
    audioLibraryCollectionViewLayout.sectionInset = collectionViewSectionInset;

    NSCollectionViewFlowLayout *videoLibraryCollectionViewLayout = _videoLibraryCollectionView.collectionViewLayout;
    videoLibraryCollectionViewLayout.itemSize = CGSizeMake(214., 260.);
    videoLibraryCollectionViewLayout.minimumLineSpacing = collectionItemSpacing;
    videoLibraryCollectionViewLayout.minimumInteritemSpacing = collectionItemSpacing;
    videoLibraryCollectionViewLayout.sectionInset = collectionViewSectionInset;

    NSCollectionViewFlowLayout *mediaSourceCollectionViewLayout = _mediaSourceCollectionView.collectionViewLayout;
    mediaSourceCollectionViewLayout.itemSize = CGSizeMake(214., 246.);
    mediaSourceCollectionViewLayout.minimumLineSpacing = collectionItemSpacing;
    mediaSourceCollectionViewLayout.minimumInteritemSpacing = collectionItemSpacing;
    mediaSourceCollectionViewLayout.sectionInset = collectionViewSectionInset;

    // HACK: The size of the segmented title buttons is not always correctly calculated
    // especially when the text we are setting differs from what is set in the storyboard.
    // Hiding and showing the toolbar again must trigger something that causes the width
    // of the buttons to be correctly recalculated, working around this issue
    [self toggleToolbarShown:self];
    [self toggleToolbarShown:self];

    // The playlist toggle button's default state is OFF so we set it to ON if the playlist
    // is not collapsed when we open the library window
    if (![_mainSplitView isSubviewCollapsed:_playlistView]) {
        _playQueueToggle.state = NSControlStateValueOn;
    }
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (@available(macOS 10.14, *)) {
        [[NSApplication sharedApplication] removeObserver:self forKeyPath:@"effectiveAppearance"];
    }

    libvlc_int_t *libvlc = vlc_object_instance(getIntf());
    var_DelCallback(libvlc, "intf-toggle-fscontrol", ShowFullscreenController, (__bridge void *)self);
    var_DelCallback(libvlc, "intf-show", ShowController, (__bridge void *)self);
}

- (void)encodeRestorableStateWithCoder:(NSCoder *)coder
{
    [super encodeRestorableStateWithCoder:coder];
    [coder encodeInteger:_segmentedTitleControl.selectedSegment forKey:@"macosx-library-selected-segment"];
    [coder encodeInteger:_gridVsListSegmentedControl.selectedSegment forKey:@"macosx-library-view-mode-selected-segment"];
    [coder encodeInteger:_libraryAudioDataSource.segmentedControl.selectedSegment forKey:@"macosx-library-audio-view-selected-segment"];
}

#pragma mark - appearance setters

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    [self updateColorsBasedOnAppearance];
}

- (void)updateColorsBasedOnAppearance
{
    // If we try to pull the view's effectiveAppearance we are going to get the previous appearance's name despite
    // responding to the effectiveAppearance change (???) so it is a better idea to pull from the general system
    // theme preference, which is always up-to-date
    if ([[[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"] isEqualToString:@"Dark"]) {
        self.upNextLabel.textColor = [NSColor VLClibraryDarkTitleColor];
        self.upNextSeparator.borderColor = [NSColor VLClibrarySeparatorDarkColor];
        self.clearPlaylistSeparator.borderColor = [NSColor VLClibrarySeparatorDarkColor];
        self.dragDropImageBackgroundBox.hidden = NO;
    } else {
        self.upNextLabel.textColor = [NSColor VLClibraryLightTitleColor];
        self.upNextSeparator.borderColor = [NSColor VLClibrarySeparatorLightColor];
        self.clearPlaylistSeparator.borderColor = [NSColor VLClibrarySeparatorLightColor];
        self.dragDropImageBackgroundBox.hidden = YES;
    }
}

- (void)updateViewCellDimensionsBasedOnSetting:(NSNotification *)aNotification
{
    _playlistTableView.rowHeight = config_GetInt("macosx-large-text") ? VLCLibraryWindowLargePlaylistRowHeight : VLCLibraryWindowSmallPlaylistRowHeight;
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

- (void)setViewForSelectedSegment
{
    _currentSelectedSegment = _segmentedTitleControl.selectedSegment;
    _currentSelectedViewModeSegment = _gridVsListSegmentedControl.selectedSegment;

    switch (_segmentedTitleControl.selectedSegment) {
        case VLCLibraryVideoSegment:
            [self showVideoLibrary];
            break;
        case VLCLibraryMusicSegment:
            [self showAudioLibrary];
            break;
        case VLCLibraryBrowseSegment:
        case VLCLibraryStreamsSegment:
            [self showMediaSourceAppearance];
            break;
        default:
            break;
    }
}

- (IBAction)segmentedControlAction:(id)sender
{
    if (_segmentedTitleControl.selectedSegment == _currentSelectedSegment && 
        _gridVsListSegmentedControl.selectedSegment == _currentSelectedViewModeSegment) {
        return;
    }

    [self setViewForSelectedSegment];
    [self invalidateRestorableState];

    if(sender != _navigationStack) {
        [self.navigationStack appendCurrentLibraryState];
    }
}

- (void)showVideoLibrary
{
    for (NSView *subview in _libraryTargetView.subviews) {
        [subview removeFromSuperview];
    }
    
    if (_libraryVideoDataSource.libraryModel.numberOfVideoMedia == 0) { // empty library
        for (NSLayoutConstraint *constraint in audioPlaceholderImageViewSizeConstraints) {
            constraint.active = NO;
        }
        for (NSLayoutConstraint *constraint in videoPlaceholderImageViewSizeConstraints) {
            constraint.active = YES;
        }
        
        _emptyLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
        [_libraryTargetView addSubview:_emptyLibraryView];
        NSDictionary *dict = NSDictionaryOfVariableBindings(_emptyLibraryView);
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_emptyLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_emptyLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];
        
        _placeholderImageView.image = [NSImage imageNamed:@"placeholder-video"];
        _placeholderLabel.stringValue = _NS("Your favorite videos will appear here.\nGo to the Browse section to add videos you love.");
    }
    else {
        _videoLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
        [_libraryTargetView addSubview:_videoLibraryView];
        NSDictionary *dict = NSDictionaryOfVariableBindings(_videoLibraryView);
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_videoLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_videoLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];
        
        [_libraryVideoDataSource reloadData];
    }
    
    _librarySortButton.hidden = NO;
    _librarySearchField.enabled = YES;
    _optionBarView.hidden = YES;
    _audioSegmentedControl.hidden = YES;

    self.gridVsListSegmentedControl.target = self;
    self.gridVsListSegmentedControl.action = @selector(segmentedControlAction:);
}

- (void)showAudioLibrary
{
    for (NSView *subview in _libraryTargetView.subviews) {
        [subview removeFromSuperview];
    }
    
    if (_libraryAudioDataSource.libraryModel.numberOfAudioMedia == 0) { // empty library
        for (NSLayoutConstraint *constraint in videoPlaceholderImageViewSizeConstraints) {
            constraint.active = NO;
        }
        for (NSLayoutConstraint *constraint in audioPlaceholderImageViewSizeConstraints) {
            constraint.active = YES;
        }
        
        [_libraryAudioDataSource reloadEmptyViewAppearance];
        
        _emptyLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
        [_libraryTargetView addSubview:_emptyLibraryView];
        
        NSDictionary *dict = NSDictionaryOfVariableBindings(_emptyLibraryView);
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_emptyLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_emptyLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];
    }
    else {
        _audioLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
        [_libraryTargetView addSubview:_audioLibraryView];
        NSDictionary *dict = NSDictionaryOfVariableBindings(_audioLibraryView);
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_audioLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_audioLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];
        
        
         if (self.gridVsListSegmentedControl.selectedSegment == VLCGridViewModeSegment) {
            _audioLibrarySplitView.hidden = YES;
            _audioCollectionViewScrollView.hidden = NO;
            [_libraryAudioDataSource reloadAppearance];
        } else {
            _audioLibrarySplitView.hidden = NO;
            _audioCollectionViewScrollView.hidden = YES;
            [_libraryAudioDataSource reloadAppearance];
            [_audioCollectionSelectionTableView reloadData];
        }
    }
    
    _librarySortButton.hidden = NO;
    _librarySearchField.enabled = YES;
    _optionBarView.hidden = NO;
    _audioSegmentedControl.hidden = NO;
    
    self.gridVsListSegmentedControl.target = self;
    self.gridVsListSegmentedControl.action = @selector(segmentedControlAction:);
}

- (void)showMediaSourceAppearance
{
    if (_videoLibraryView.superview != nil) {
        [_videoLibraryView removeFromSuperview];
    }
    if (_audioLibraryView.superview != nil) {
        [_audioLibraryView removeFromSuperview];
    }
    if (_emptyLibraryView.superview != nil) {
        [_emptyLibraryView removeFromSuperview];
    }
    if (_mediaSourceView.superview == nil) {
        _mediaSourceView.translatesAutoresizingMaskIntoConstraints = NO;
        [_libraryTargetView addSubview:_mediaSourceView];
        NSDictionary *dict = NSDictionaryOfVariableBindings(_mediaSourceView);
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_mediaSourceView(>=572.)]|" options:0 metrics:0 views:dict]];
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_mediaSourceView(>=444.)]|" options:0 metrics:0 views:dict]];
    }
    _mediaSourceDataSource.mediaSourceMode = _segmentedTitleControl.selectedSegment == 2 ? VLCMediaSourceModeLAN : VLCMediaSourceModeInternet;
    _librarySortButton.hidden = YES;
    _librarySearchField.enabled = NO;
    [self clearLibraryFilterString];
    _optionBarView.hidden = YES;
    _audioSegmentedControl.hidden = YES;
    [_mediaSourceDataSource reloadViews];
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

- (IBAction)sortPlaylist:(id)sender
{
    if (!_playlistSortingMenuController) {
        _playlistSortingMenuController = [[VLCPlaylistSortingMenuController alloc] init];
    }
    [NSMenu popUpContextMenu:_playlistSortingMenuController.playlistSortingMenu withEvent:[NSApp currentEvent] forView:sender];
}

- (IBAction)sortLibrary:(id)sender
{
    if (!_librarySortingMenuController) {
        _librarySortingMenuController = [[VLCLibrarySortingMenuController alloc] init];
    }
    [NSMenu popUpContextMenu:_librarySortingMenuController.librarySortingMenu withEvent:[NSApp currentEvent] forView:sender];
}

- (IBAction)filterLibrary:(id)sender
{
    [[[VLCMain sharedInstance] libraryController] filterByString:_librarySearchField.stringValue];
}

- (void)clearLibraryFilterString
{
    _librarySearchField.stringValue = @"";
    [self filterLibrary:self];
}

- (IBAction)openMedia:(id)sender
{
    [[[VLCMain sharedInstance] open] openFileGeneric];
}

- (BOOL)handlePasteBoardFromDragSession:(NSPasteboard *)paste
{
    id propertyList = [paste propertyListForType:NSFilenamesPboardType];
    if (propertyList == nil) {
        return NO;
    }

    NSArray *values = [propertyList sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
    NSUInteger valueCount = [values count];
    if (valueCount > 0) {
        NSMutableArray *metadataArray = [NSMutableArray arrayWithCapacity:valueCount];

        for (NSString *filepath in values) {
            VLCOpenInputMetadata *inputMetadata;

            inputMetadata = [VLCOpenInputMetadata inputMetaWithPath:filepath];
            if (!inputMetadata)
                continue;

            [metadataArray addObject:inputMetadata];
        }
        [_playlistController addPlaylistItems:metadataArray];

        return YES;
    }

    return NO;
}

- (IBAction)goToBrowseSection:(id)sender
{
    [_segmentedTitleControl setSelected:YES forSegment:2];
    [self segmentedControlAction:_segmentedTitleControl];
}

#pragma mark - split view delegation

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMinimumPosition ofSubviewAt:(NSInteger)dividerIndex
{
    switch (dividerIndex) {
        case 0:
            return VLCLibraryWindowMinimalWidth;
        default:
            break;
    }

    return proposedMinimumPosition;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMaximumPosition ofSubviewAt:(NSInteger)dividerIndex
{
    switch (dividerIndex) {
        case 0:
            return splitView.frame.size.width - VLCLibraryWindowMinimalPlaylistWidth;
        default:
            break;
    }

    return proposedMaximumPosition;
}

- (BOOL)splitView:(NSSplitView *)splitView canCollapseSubview:(NSView *)subview
{
    return [subview isEqual:_playlistView];
}

- (BOOL)splitView:(NSSplitView *)splitView shouldCollapseSubview:(NSView *)subview forDoubleClickOnDividerAtIndex:(NSInteger)dividerIndex
{
    return [subview isEqual:_playlistView];
}

- (void)splitViewDidResizeSubviews:(NSNotification *)notification
{
    _lastPlaylistWidthBeforeCollaps = [_playlistView frame].size.width;

    if (![_mainSplitView isSubviewCollapsed:_playlistView]) {
        _playQueueToggle.state = NSControlStateValueOn;
    } else {
        _playQueueToggle.state = NSControlStateValueOff;
    }
}

- (void)togglePlaylist
{
    [_mainSplitView adjustSubviews];
    CGFloat splitViewWidth = _mainSplitView.frame.size.width;
    if ([_mainSplitView isSubviewCollapsed:_playlistView]) {
        [_mainSplitView setPosition:splitViewWidth - _lastPlaylistWidthBeforeCollaps ofDividerAtIndex:0];
        _playQueueToggle.state = NSControlStateValueOn;
    } else {
        [_mainSplitView setPosition:splitViewWidth ofDividerAtIndex:0];
        _playQueueToggle.state = NSControlStateValueOff;
    }
}

- (IBAction)showAndHidePlaylist:(id)sender
{
    [self togglePlaylist];
}

- (IBAction)backwardsNavigationAction:(id)sender
{
    self.videoView.hidden ? [_navigationStack backwards] : [self disableVideoPlaybackAppearance];
}

- (IBAction)forwardsNavigationAction:(id)sender
{
    [_navigationStack forwards];
}

#pragma mark - video output controlling

- (void)videoPlaybackWillBeStarted
{
    if (!self.fullscreen)
        _windowFrameBeforePlayback = [self frame];
}

- (void)setHasActiveVideo:(BOOL)hasActiveVideo
{
    [super setHasActiveVideo:hasActiveVideo];
    hasActiveVideo ? [self enableVideoPlaybackAppearance] : [self disableVideoPlaybackAppearance];
}

- (void)playerStateChanged:(NSNotification *)notification
{
    if(_playlistController.playerController.playerState != VLC_PLAYER_STATE_PLAYING) {
        return;
    }

    [self reopenVideoView];
}

// This handles reopening the video view when the user has closed it.
- (void)reopenVideoView
{
    if(!self.hasActiveVideo) {
        return;
    }

    VLCMediaLibraryMediaItem *mediaItem = [VLCMediaLibraryMediaItem mediaItemForURL:_playlistController.playerController.URLOfCurrentMediaItem];

    if(mediaItem == nil || mediaItem.mediaType != VLC_ML_MEDIA_TYPE_VIDEO) {
        return;
    }

    [self enableVideoPlaybackAppearance];
}

- (void)presentVideoView
{
    for (NSView *subview in _libraryTargetView.subviews) {
        [subview removeFromSuperview];
    }
    
    NSLog(@"Presenting video view in main library window.");
    
    VLCVoutView *videoView = self.videoView;
    videoView.translatesAutoresizingMaskIntoConstraints = NO;
    videoView.hidden = NO;
    
    [_libraryTargetView addSubview:videoView];
    NSDictionary *dict = NSDictionaryOfVariableBindings(videoView);
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[videoView(>=572.)]|"
                                                                               options:0
                                                                               metrics:0
                                                                                 views:dict]];
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[videoView(>=444.)]|"
                                                                               options:0
                                                                               metrics:0
                                                                                 views:dict]];
}

- (void)enableVideoPlaybackAppearance
{
    [self presentVideoView];

    [self.segmentedTitleControl setHidden:YES];
    [self.forwardsNavigationButton setHidden:YES];
    [self.gridVsListSegmentedControl setHidden:YES];
    [self.librarySortButton setHidden:YES];
    [self.librarySearchField setEnabled:NO];
    [self clearLibraryFilterString];

    // Repurpose the back button
    [self.backwardsNavigationButton setEnabled:YES];

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
        CGFloat f_threshold_height = VLCVideoWindowCommonMinimalHeight + [self.controlsBar height];
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

    [self.segmentedTitleControl setHidden:NO];
    [self.forwardsNavigationButton setHidden:NO];
    [self.gridVsListSegmentedControl setHidden:NO];
    [self.librarySortButton setHidden:NO];
    [self.librarySearchField setEnabled:YES];

    // Reset the back button to navigation state
    [self.backwardsNavigationButton setEnabled:_navigationStack.backwardsAvailable];

    [self setViewForSelectedSegment];

    if (self.nativeFullscreenMode) {
        [self showControlsBar];
        [_fspanel shouldBecomeInactive:nil];
    }
}

#pragma mark - library representation and interaction
- (void)updateLibraryRepresentation:(NSNotification *)aNotification
{
    if (_videoLibraryView.superview != nil) {
        [_libraryVideoDataSource reloadData];
    } else if (_audioLibraryView.superview != nil) {
        [_libraryAudioDataSource reloadAppearance];
    }
}

#pragma mark -
#pragma mark respond to core events

- (void)shouldShowController:(NSNotification *)aNotification
{
    [self makeKeyAndOrderFront:nil];
}

- (void)shouldShowFullscreenController:(NSNotification *)aNotification
{
    id currentWindow = [NSApp keyWindow];
    if ([currentWindow respondsToSelector:@selector(hasActiveVideo)] && [currentWindow hasActiveVideo]) {
        if ([currentWindow respondsToSelector:@selector(fullscreen)] && [currentWindow fullscreen] && ![[currentWindow videoView] isHidden]) {
            if ([_playlistController.playerController activeVideoPlayback]) {
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
    [window setRestorationClass:[self class]];
    [window setExcludedFromWindowsMenu:YES];
    [window setAcceptsMouseMovedEvents:YES];
    [window setContentMinSize:NSMakeSize(VLCLibraryWindowMinimalWidth, VLCLibraryWindowMinimalHeight)];

    // HACK: On initialisation, the window refuses to accept any border resizing. It seems the split view
    // holds a monopoly on the edges of the window (which can be seen as the right-side of the split view
    // lets you resize the playlist, and after doing so the window becomes resizeable.
    
    // This can be worked around by maximizing the window, or toggling the playlist.
    // Toggling the playlist is simplest.
    [window togglePlaylist];
    [window togglePlaylist];
}

+ (void)restoreWindowWithIdentifier:(NSUserInterfaceItemIdentifier)identifier 
                              state:(NSCoder *)state 
                  completionHandler:(void (^)(NSWindow *, NSError *))completionHandler
{
    if([identifier isEqualToString:kVLCLibraryWindowIdentifier] == NO) {
        return;
    }
    
    if([VLCMain sharedInstance].libraryWindowController == nil) {
        [VLCMain sharedInstance].libraryWindowController = [[VLCLibraryWindowController alloc] initWithLibraryWindow];
    }

    VLCLibraryWindow *libraryWindow = [VLCMain sharedInstance].libraryWindow;

    NSInteger rememberedSelectedLibrarySegment = [state decodeIntegerForKey:@"macosx-library-selected-segment"];
    NSInteger rememberedSelectedLibraryViewModeSegment = [state decodeIntegerForKey:@"macosx-library-view-mode-selected-segment"];
    NSInteger rememberedSelectedLibraryViewAudioSegment = [state decodeIntegerForKey:@"macosx-library-audio-view-selected-segment"];

    [libraryWindow.segmentedTitleControl setSelectedSegment:rememberedSelectedLibrarySegment];
    [libraryWindow.gridVsListSegmentedControl setSelectedSegment:rememberedSelectedLibraryViewModeSegment];
    [libraryWindow.libraryAudioDataSource.segmentedControl setSelectedSegment:rememberedSelectedLibraryViewAudioSegment];

    // We don't want to add these to the navigation stack...
    [libraryWindow.libraryAudioDataSource segmentedControlAction:libraryWindow.navigationStack];
    [libraryWindow segmentedControlAction:libraryWindow.navigationStack];

    // But we do want the "final" initial position to be added. So we manually invoke the navigation stack
    [libraryWindow.navigationStack appendCurrentLibraryState];

    completionHandler(libraryWindow, nil);
}

@end
