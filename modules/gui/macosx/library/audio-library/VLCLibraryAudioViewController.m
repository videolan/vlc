/*****************************************************************************
 * VLCLibraryAudioViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

#import "VLCLibraryAudioViewController.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryNavigationStack.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryAudioDataSource.h"
#import "library/audio-library/VLCLibraryAudioGroupDataSource.h"
#import "library/audio-library/VLCLibraryAudioGroupTableViewDelegate.h"
#import "library/audio-library/VLCLibraryAudioTableViewDelegate.h"

#import "library/video-library/VLCLibraryVideoViewController.h"

#import "main/VLCMain.h"

#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCMainVideoViewController.h"

NSString *VLCLibraryPlaceholderAudioViewIdentifier = @"VLCLibraryPlaceholderAudioViewIdentifier";

@interface VLCLibraryAudioViewController()
{
    NSArray<NSString *> *_placeholderImageNames;
    NSArray<NSString *> *_placeholderLabelStrings;

    VLCLibraryCollectionViewDelegate *_audioLibraryCollectionViewDelegate;
    VLCLibraryAudioTableViewDelegate *_audioLibraryTableViewDelegate;
    VLCLibraryAudioGroupTableViewDelegate *_audioGroupLibraryTableViewDelegate;
}
@end

@implementation VLCLibraryAudioViewController

#pragma mark - Set up the view controller

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super init];

    if(self) {
        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupAudioDataSource];

        _audioLibraryCollectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
        _audioLibraryTableViewDelegate = [[VLCLibraryAudioTableViewDelegate alloc] init];
        _audioGroupLibraryTableViewDelegate = [[VLCLibraryAudioGroupTableViewDelegate alloc] init];

        [self setupAudioCollectionView];
        [self setupGridModeSplitView];
        [self setupAudioTableViews];
        [self setupAudioSegmentedControl];
        [self setupAudioLibraryViews];

        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelAudioMediaListReset
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelAudioMediaItemDeleted
                                 object:nil];
    }

    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow*)libraryWindow
{
    NSParameterAssert(libraryWindow);

    _libraryWindow = libraryWindow;
    _libraryTargetView = libraryWindow.libraryTargetView;
    _audioLibraryView = libraryWindow.audioLibraryView;
    _audioLibrarySplitView = libraryWindow.audioLibrarySplitView;
    _audioCollectionSelectionTableViewScrollView = libraryWindow.audioCollectionSelectionTableViewScrollView;
    _audioCollectionSelectionTableView = libraryWindow.audioCollectionSelectionTableView;
    _audioGroupSelectionTableViewScrollView = libraryWindow.audioGroupSelectionTableViewScrollView;
    _audioGroupSelectionTableView = libraryWindow.audioGroupSelectionTableView;
    _audioSongTableViewScrollView = libraryWindow.audioLibrarySongsTableViewScrollView;
    _audioSongTableView = libraryWindow.audioLibrarySongsTableView;
    _audioCollectionViewScrollView = libraryWindow.audioCollectionViewScrollView;
    _audioLibraryCollectionView = libraryWindow.audioLibraryCollectionView;
    _audioLibraryGridModeSplitView = libraryWindow.audioLibraryGridModeSplitView;
    _audioLibraryGridModeSplitViewListTableViewScrollView = libraryWindow.audioLibraryGridModeSplitViewListTableViewScrollView;
    _audioLibraryGridModeSplitViewListTableView = libraryWindow.audioLibraryGridModeSplitViewListTableView;
    _audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView = libraryWindow.audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView;
    _audioLibraryGridModeSplitViewListSelectionCollectionView = libraryWindow.audioLibraryGridModeSplitViewListSelectionCollectionView;

    _audioSegmentedControl = libraryWindow.audioSegmentedControl;
    _segmentedTitleControl = libraryWindow.segmentedTitleControl;
    _placeholderImageView = libraryWindow.placeholderImageView;
    _placeholderLabel = libraryWindow.placeholderLabel;
    _emptyLibraryView = libraryWindow.emptyLibraryView;
    _optionBarView = libraryWindow.optionBarView;
}

- (void)setupAudioDataSource
{
    _audioDataSource = [[VLCLibraryAudioDataSource alloc] init];
    _audioDataSource.libraryModel = [VLCMain sharedInstance].libraryController.libraryModel;
    _audioDataSource.collectionSelectionTableView = _audioCollectionSelectionTableView;
    _audioDataSource.groupSelectionTableView = _audioGroupSelectionTableView;
    _audioDataSource.songsTableView = _audioSongTableView;
    _audioDataSource.collectionView = _audioLibraryCollectionView;
    _audioDataSource.gridModeListTableView = _audioLibraryGridModeSplitViewListTableView;
    _audioDataSource.gridModeListSelectionCollectionView = _audioLibraryGridModeSplitViewListSelectionCollectionView;
    [_audioDataSource setup];

    _audioGroupDataSource = [[VLCLibraryAudioGroupDataSource alloc] init];
    _audioDataSource.audioGroupDataSource = _audioGroupDataSource;
}

- (void)setupAudioCollectionView
{
    _audioLibraryCollectionView.dataSource = _audioDataSource;
    _audioLibraryCollectionView.delegate = _audioLibraryCollectionViewDelegate;

    _audioLibraryCollectionView.selectable = YES;
    _audioLibraryCollectionView.allowsMultipleSelection = NO;
    _audioLibraryCollectionView.allowsEmptySelection = YES;

    const CGFloat collectionItemSpacing = [VLCLibraryUIUnits collectionViewItemSpacing];
    const NSEdgeInsets collectionViewSectionInset = [VLCLibraryUIUnits collectionViewSectionInsets];

    NSCollectionViewFlowLayout *audioLibraryCollectionViewLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];
    _audioLibraryCollectionView.collectionViewLayout = audioLibraryCollectionViewLayout;
    audioLibraryCollectionViewLayout.minimumLineSpacing = collectionItemSpacing;
    audioLibraryCollectionViewLayout.minimumInteritemSpacing = collectionItemSpacing;
    audioLibraryCollectionViewLayout.sectionInset = collectionViewSectionInset;
}

- (void)setupAudioTableViews
{
    _audioCollectionSelectionTableView.dataSource = _audioDataSource;
    _audioCollectionSelectionTableView.delegate = _audioLibraryTableViewDelegate;

    _audioGroupSelectionTableView.dataSource = _audioGroupDataSource;
    _audioGroupSelectionTableView.delegate = _audioGroupLibraryTableViewDelegate;

    if(@available(macOS 11.0, *)) {
        _audioGroupSelectionTableView.style = NSTableViewStyleFullWidth;
    }

    _audioSongTableView.dataSource = _audioDataSource;
    _audioSongTableView.delegate = _audioLibraryTableViewDelegate;
}

- (void)setupGridModeSplitView
{
    _audioLibraryGridModeSplitViewListTableView.dataSource = _audioDataSource;
    _audioLibraryGridModeSplitViewListTableView.delegate = _audioLibraryTableViewDelegate;

    _audioLibraryGridModeSplitViewListSelectionCollectionView.dataSource = _audioGroupDataSource;
    _audioLibraryGridModeSplitViewListSelectionCollectionView.delegate = _audioLibraryCollectionViewDelegate;

    _audioLibraryGridModeSplitViewListSelectionCollectionView.selectable = YES;
    _audioLibraryGridModeSplitViewListSelectionCollectionView.allowsMultipleSelection = NO;
    _audioLibraryGridModeSplitViewListSelectionCollectionView.allowsEmptySelection = YES;

    const CGFloat collectionItemSpacing = [VLCLibraryUIUnits collectionViewItemSpacing];
    const NSEdgeInsets collectionViewSectionInset = [VLCLibraryUIUnits collectionViewSectionInsets];

    NSCollectionViewFlowLayout *audioLibraryGridModeListSelectionCollectionViewLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];
    _audioLibraryGridModeSplitViewListSelectionCollectionView.collectionViewLayout = audioLibraryGridModeListSelectionCollectionViewLayout;
    audioLibraryGridModeListSelectionCollectionViewLayout.minimumLineSpacing = collectionItemSpacing;
    audioLibraryGridModeListSelectionCollectionViewLayout.minimumInteritemSpacing = collectionItemSpacing;
    audioLibraryGridModeListSelectionCollectionViewLayout.sectionInset = collectionViewSectionInset;

}

- (void)setupAudioPlaceholderView
{
    _audioPlaceholderImageViewSizeConstraints = @[
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

    _placeholderImageNames = @[@"placeholder-group2", @"placeholder-music", @"placeholder-music", @"placeholder-music"];
    _placeholderLabelStrings = @[
        _NS("Your favorite artists will appear here.\nGo to the Browse section to add artists you love."),
        _NS("Your favorite albums will appear here.\nGo to the Browse section to add albums you love."),
        _NS("Your favorite tracks will appear here.\nGo to the Browse section to add tracks you love."),
        _NS("Your favorite genres will appear here.\nGo to the Browse section to add genres you love."),
    ];
}

- (void)setupAudioSegmentedControl
{
    _audioSegmentedControl.segmentCount = 4;
    [_audioSegmentedControl setLabel:_NS("Artists") forSegment:VLCAudioLibraryArtistsSegment];
    [_audioSegmentedControl setLabel:_NS("Albums") forSegment:VLCAudioLibraryAlbumsSegment];
    [_audioSegmentedControl setLabel:_NS("Songs") forSegment:VLCAudioLibrarySongsSegment];
    [_audioSegmentedControl setLabel:_NS("Genres") forSegment:VLCAudioLibraryGenresSegment];
    _audioSegmentedControl.selectedSegment = 0;
}

- (void)configureAudioSegmentedControl
{
    [_audioSegmentedControl setTarget:self];
    [_audioSegmentedControl setAction:@selector(segmentedControlAction:)];
}

- (void)setupAudioLibraryViews
{
    _audioCollectionSelectionTableView.rowHeight = [VLCLibraryUIUnits mediumTableViewRowHeight];
    _audioLibraryGridModeSplitViewListTableView.rowHeight = [VLCLibraryUIUnits mediumTableViewRowHeight];
    _audioGroupSelectionTableView.rowHeight = [VLCLibraryAlbumTableCellView defaultHeight];

    const NSEdgeInsets defaultContentInsets = [VLCLibraryUIUnits libraryViewScrollViewContentInsets];
    const CGFloat topAudioScrollViewContentInset = defaultContentInsets.top + _optionBarView.frame.size.height;
    const NSEdgeInsets audioScrollViewContentInsets = NSEdgeInsetsMake(topAudioScrollViewContentInset,
                                                                       defaultContentInsets.left,
                                                                       defaultContentInsets.bottom,
                                                                       defaultContentInsets.right);
    const NSEdgeInsets audioScrollViewScrollerInsets = [VLCLibraryUIUnits libraryViewScrollViewScrollerInsets];

    _audioCollectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioCollectionViewScrollView.contentInsets = audioScrollViewContentInsets;
    _audioCollectionViewScrollView.scrollerInsets = audioScrollViewScrollerInsets;

    _audioCollectionSelectionTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioCollectionSelectionTableViewScrollView.contentInsets = audioScrollViewContentInsets;
    _audioCollectionSelectionTableViewScrollView.scrollerInsets = audioScrollViewScrollerInsets;
    _audioGroupSelectionTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioGroupSelectionTableViewScrollView.contentInsets = audioScrollViewContentInsets;
    _audioGroupSelectionTableViewScrollView.scrollerInsets = audioScrollViewScrollerInsets;

    _audioLibraryGridModeSplitViewListTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioLibraryGridModeSplitViewListTableViewScrollView.contentInsets = audioScrollViewContentInsets;
    _audioLibraryGridModeSplitViewListTableViewScrollView.scrollerInsets = audioScrollViewScrollerInsets;
    _audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView.contentInsets = audioScrollViewContentInsets;
    _audioLibraryGridModeSplitViewListSelectionCollectionViewScrollView.scrollerInsets = audioScrollViewScrollerInsets;
}

#pragma mark - Show the audio view

- (void)presentAudioView
{
    _libraryTargetView.subviews = @[];

    [self configureAudioSegmentedControl];
    [self segmentedControlAction:VLCMain.sharedInstance.libraryWindow.navigationStack];
}

- (void)presentPlaceholderAudioView
{
    for (NSLayoutConstraint *constraint in _libraryWindow.libraryVideoViewController.videoPlaceholderImageViewSizeConstraints) {
        constraint.active = NO;
    }
    for (NSLayoutConstraint *constraint in _audioPlaceholderImageViewSizeConstraints) {
        constraint.active = YES;
    }

    NSInteger selectedLibrarySegment = _audioSegmentedControl.selectedSegment;

    if(selectedLibrarySegment < _placeholderImageNames.count && selectedLibrarySegment >= 0) {
        _placeholderImageView.image = [NSImage imageNamed:_placeholderImageNames[selectedLibrarySegment]];
    }

    if(selectedLibrarySegment < _placeholderLabelStrings.count && selectedLibrarySegment >= 0) {
        _placeholderLabel.stringValue = _placeholderLabelStrings[selectedLibrarySegment];
    }

    _emptyLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    _libraryTargetView.subviews = @[_emptyLibraryView];
    NSDictionary *dict = NSDictionaryOfVariableBindings(_emptyLibraryView);
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_emptyLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_emptyLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];

    _emptyLibraryView.identifier = VLCLibraryPlaceholderAudioViewIdentifier;
}

- (void)prepareAudioLibraryView
{
    _audioLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    _libraryTargetView.subviews = @[_audioLibraryView];
    NSDictionary *dict = NSDictionaryOfVariableBindings(_audioLibraryView);
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_audioLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_audioLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];
}

- (void)hideAllViews
{
    _audioLibrarySplitView.hidden = YES;
    _audioLibraryGridModeSplitView.hidden = YES;
    _audioSongTableViewScrollView.hidden = YES;
    _audioCollectionViewScrollView.hidden = YES;
}

- (void)presentAudioGridModeView
{
    if (_audioSegmentedControl.selectedSegment == VLCAudioLibrarySongsSegment ||
        _audioSegmentedControl.selectedSegment == VLCAudioLibraryAlbumsSegment) {

        [_audioLibraryCollectionView deselectAll:self];
        [(VLCLibraryCollectionViewFlowLayout *)_audioLibraryCollectionView.collectionViewLayout resetLayout];

        _audioCollectionViewScrollView.hidden = NO;
    } else {
        _audioLibraryGridModeSplitView.hidden = NO;
    }
}

- (void)presentAudioTableView
{
    if (_audioSegmentedControl.selectedSegment == VLCAudioLibrarySongsSegment) {
        _audioSongTableViewScrollView.hidden = NO;
    } else {
        _audioLibrarySplitView.hidden = NO;
    }
}

- (void)updatePresentedView
{
    const VLCAudioLibrarySegment audioLibrarySegment = _audioSegmentedControl.selectedSegment;
    _audioDataSource.audioLibrarySegment = audioLibrarySegment;

    if (_audioDataSource.libraryModel.listOfAudioMedia.count == 0) {
        [self presentPlaceholderAudioView];
    } else {
        [self prepareAudioLibraryView];
        [self hideAllViews];

        VLCLibraryViewModeSegment viewModeSegment = VLCLibraryGridViewModeSegment; // default value
        VLCLibraryWindowPersistentPreferences * const libraryWindowPrefs = VLCLibraryWindowPersistentPreferences.sharedInstance;

        switch (audioLibrarySegment) {
            case VLCAudioLibraryArtistsSegment:
                viewModeSegment = libraryWindowPrefs.artistLibraryViewMode;
                break;
            case VLCAudioLibraryGenresSegment:
                viewModeSegment = libraryWindowPrefs.genreLibraryViewMode;
                break;
            case VLCAudioLibrarySongsSegment:
                viewModeSegment = libraryWindowPrefs.songsLibraryViewMode;
                break;
            case VLCAudioLibraryAlbumsSegment:
                viewModeSegment = libraryWindowPrefs.albumLibraryViewMode;
                break;
            default:
                break;
        }

        if (viewModeSegment == VLCLibraryListViewModeSegment) {
            [self presentAudioTableView];
        } else if (viewModeSegment == VLCLibraryGridViewModeSegment) {
            [self presentAudioGridModeView];
        } else {
            NSAssert(false, @"View mode must be grid or list mode");
        }
    }
}

- (IBAction)segmentedControlAction:(id)sender
{
    [self updatePresentedView];
}

- (void)reloadData
{
    [_audioDataSource reloadData];
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    NSParameterAssert(aNotification);
    VLCLibraryModel *model = (VLCLibraryModel *)aNotification.object;
    NSAssert(model, @"Notification object should be a VLCLibraryModel");
    NSArray<VLCMediaLibraryMediaItem *> * audioList = model.listOfAudioMedia;

    if (_segmentedTitleControl.selectedSegment == VLCLibraryMusicSegment &&
        ((audioList.count == 0 && ![_libraryTargetView.subviews containsObject:_emptyLibraryView]) ||
         (audioList.count > 0 && ![_libraryTargetView.subviews containsObject:_audioLibraryView])) &&
        _libraryWindow.videoViewController.view.hidden) {

        [self updatePresentedView];
    }
}

@end
