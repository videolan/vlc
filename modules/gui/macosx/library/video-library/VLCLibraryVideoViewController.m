/*****************************************************************************
 * VLCLibraryVideoViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCLibraryVideoViewController.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"

#import "library/audio-library/VLCLibraryAudioViewController.h"

#import "library/video-library/VLCLibraryVideoCollectionViewsStackViewController.h"
#import "library/video-library/VLCLibraryVideoTableViewDataSource.h"
#import "library/video-library/VLCLibraryVideoTableViewDelegate.h"

#import "main/VLCMain.h"

#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCMainVideoViewController.h"

@interface VLCLibraryVideoViewController ()
{
    VLCLibraryVideoTableViewDelegate *_videoLibraryTableViewDelegate;
}
@end

@implementation VLCLibraryVideoViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super init];

    if(self) {
        _videoLibraryTableViewDelegate = [[VLCLibraryVideoTableViewDelegate alloc] init];

        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupTableViewDataSource];
        [self setupTableViews];
        [self setupGridViewController];
        [self setupVideoPlaceholderView];
        [self setupVideoLibraryViews];

        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelVideoMediaListReset
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelVideoMediaItemDeleted
                                 object:nil];
    }

    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    NSParameterAssert(libraryWindow);
    _libraryWindow = libraryWindow;
    _libraryTargetView = libraryWindow.libraryTargetView;
    _videoLibraryView = libraryWindow.videoLibraryView;
    _videoLibrarySplitView = libraryWindow.videoLibrarySplitView;
    _videoLibraryCollectionViewsStackViewScrollView = libraryWindow.videoLibraryCollectionViewsStackViewScrollView;
    _videoLibraryCollectionViewsStackView = libraryWindow.videoLibraryCollectionViewsStackView;
    _videoLibraryGroupSelectionTableViewScrollView = libraryWindow.videoLibraryGroupSelectionTableViewScrollView;
    _videoLibraryGroupSelectionTableView = libraryWindow.videoLibraryGroupSelectionTableView;
    _videoLibraryGroupsTableViewScrollView = libraryWindow.videoLibraryGroupsTableViewScrollView;
    _videoLibraryGroupsTableView = libraryWindow.videoLibraryGroupsTableView;

    _gridVsListSegmentedControl = libraryWindow.gridVsListSegmentedControl;
    _segmentedTitleControl = libraryWindow.segmentedTitleControl;
    _placeholderImageView = libraryWindow.placeholderImageView;
    _placeholderLabel = libraryWindow.placeholderLabel;
    _emptyLibraryView = libraryWindow.emptyLibraryView;
}

- (void)setupTableViewDataSource
{
    _libraryVideoTableViewDataSource = [[VLCLibraryVideoTableViewDataSource alloc] init];
    _libraryVideoTableViewDataSource.libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    _libraryVideoTableViewDataSource.groupsTableView = _videoLibraryGroupsTableView;
    _libraryVideoTableViewDataSource.groupSelectionTableView = _videoLibraryGroupSelectionTableView;
}

- (void)setupTableViews
{
    _videoLibraryGroupsTableView.dataSource = _libraryVideoTableViewDataSource;
    _videoLibraryGroupsTableView.target = _libraryVideoTableViewDataSource;
    _videoLibraryGroupsTableView.delegate = _videoLibraryTableViewDelegate;

    _videoLibraryGroupSelectionTableView.dataSource = _libraryVideoTableViewDataSource;
    _videoLibraryGroupSelectionTableView.target = _libraryVideoTableViewDataSource;
    _videoLibraryGroupSelectionTableView.delegate = _videoLibraryTableViewDelegate;
}

- (void)setupGridViewController
{
    _libraryVideoCollectionViewsStackViewController = [[VLCLibraryVideoCollectionViewsStackViewController alloc] init];
    _libraryVideoCollectionViewsStackViewController.collectionsStackViewScrollView = _videoLibraryCollectionViewsStackViewScrollView;
    _libraryVideoCollectionViewsStackViewController.collectionsStackView = _videoLibraryCollectionViewsStackView;
}

- (void)setupVideoPlaceholderView
{
    _videoPlaceholderImageViewSizeConstraints = @[
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
}

- (void)setupVideoLibraryViews
{
    _videoLibraryGroupsTableView.rowHeight = [VLCLibraryUIUnits mediumTableViewRowHeight];
    _videoLibraryGroupSelectionTableView.rowHeight = [VLCLibraryUIUnits mediumTableViewRowHeight];

    const NSEdgeInsets defaultInsets = [VLCLibraryUIUnits libraryViewScrollViewContentInsets];
    const NSEdgeInsets scrollerInsets = [VLCLibraryUIUnits libraryViewScrollViewScrollerInsets];

    _videoLibraryCollectionViewsStackViewScrollView.automaticallyAdjustsContentInsets = NO;
    _videoLibraryCollectionViewsStackViewScrollView.contentInsets = defaultInsets;
    _videoLibraryCollectionViewsStackViewScrollView.scrollerInsets = scrollerInsets;

    _videoLibraryGroupsTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _videoLibraryGroupsTableViewScrollView.contentInsets = defaultInsets;
    _videoLibraryGroupsTableViewScrollView.scrollerInsets = scrollerInsets;
    _videoLibraryGroupSelectionTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _videoLibraryGroupSelectionTableViewScrollView.contentInsets = defaultInsets;
    _videoLibraryGroupSelectionTableViewScrollView.scrollerInsets = scrollerInsets;
}

#pragma mark - Show the video library view

- (void)updatePresentedView
{
    if (_libraryVideoTableViewDataSource.libraryModel.numberOfVideoMedia == 0) { // empty library
        [self presentPlaceholderVideoLibraryView];
    } else {
        [self presentVideoLibraryView];
    }
}

- (void)presentVideoView
{
    _libraryTargetView.subviews = @[];
    [self updatePresentedView];
}

- (void)presentPlaceholderVideoLibraryView
{
    for (NSLayoutConstraint *constraint in _libraryWindow.libraryAudioViewController.audioPlaceholderImageViewSizeConstraints) {
        constraint.active = NO;
    }
    for (NSLayoutConstraint *constraint in _videoPlaceholderImageViewSizeConstraints) {
        constraint.active = YES;
    }

    _emptyLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    _libraryTargetView.subviews = @[_emptyLibraryView];
    NSDictionary *dict = NSDictionaryOfVariableBindings(_emptyLibraryView);
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_emptyLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_emptyLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];

    _placeholderImageView.image = [NSImage imageNamed:@"placeholder-video"];
    _placeholderLabel.stringValue = _NS("Your favorite videos will appear here.\nGo to the Browse section to add videos you love.");
}

- (void)presentVideoLibraryView
{
    _videoLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    _libraryTargetView.subviews = @[_videoLibraryView];

    NSDictionary *dict = NSDictionaryOfVariableBindings(_videoLibraryView);
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_videoLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_videoLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];

    if (self.gridVsListSegmentedControl.selectedSegment == VLCLibraryGridViewModeSegment) {
        _videoLibrarySplitView.hidden = YES;
        _videoLibraryCollectionViewsStackViewScrollView.hidden = NO;
        [_libraryVideoCollectionViewsStackViewController reloadData];
    } else {
        _videoLibrarySplitView.hidden = NO;
        _videoLibraryCollectionViewsStackViewScrollView.hidden = YES;
        [_libraryVideoTableViewDataSource reloadData];
    }
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    NSParameterAssert(aNotification);
    VLCLibraryModel *model = VLCMain.sharedInstance.libraryController.libraryModel;
    NSArray<VLCMediaLibraryMediaItem *> * videoList = model.listOfVideoMedia;

    if (_segmentedTitleControl.selectedSegment == VLCLibraryVideoSegment &&
        ((videoList.count == 0 && ![_libraryTargetView.subviews containsObject:_emptyLibraryView]) ||
         (videoList.count > 0 && ![_libraryTargetView.subviews containsObject:_videoLibraryView])) &&
        _libraryWindow.videoViewController.view.hidden) {

        [self updatePresentedView];
    }
}

@end
