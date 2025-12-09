/*****************************************************************************
 * VLCLibraryMediaSourceViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#import "VLCLibraryMediaSourceViewController.h"

#import "VLCLibraryMediaSourceViewNavigationStack.h"
#import "VLCMediaSourceBaseDataSource.h"
#import "VLCMediaSourceDataSource.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSTextField+VLCAdditions.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCInputNodePathControl.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"

#import "main/VLCMain.h"

@interface VLCLibraryMediaSourceViewController ()

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
@property (readonly) NSGlassEffectView *pathControlGlassEffectView API_AVAILABLE(macos(26.0));
#endif

@end

@implementation VLCLibraryMediaSourceViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];
    if (self) {
        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupPathControlView];
        [self setupBaseDataSource];
        [self setupCollectionView];
        [self setupMediaSourceLibraryViews];
        [self setupPlaceholderLabel];

        NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
        [defaultCenter addObserver:self 
                          selector:@selector(updatePlaceholderLabel:) 
                              name:VLCMediaSourceBaseDataSourceNodeChanged 
                            object:nil];
        [defaultCenter addObserver:self 
                          selector:@selector(updatePlaceholderLabel:) 
                              name:VLCMediaSourceDataSourceNodeChanged 
                            object:nil];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [self.browsePlaceholderLabel removeFromSuperview];
    if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        [self.pathControlGlassEffectView removeFromSuperview];
#endif
    }
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    NSParameterAssert(libraryWindow);
    _mediaSourceView = libraryWindow.mediaSourceView;
    _mediaSourceTableView = libraryWindow.mediaSourceTableView;
    _collectionView = libraryWindow.mediaSourceCollectionView;
    _collectionViewScrollView = libraryWindow.mediaSourceCollectionViewScrollView;
    _tableView = libraryWindow.mediaSourceTableView;
    _tableViewScrollView = libraryWindow.mediaSourceTableViewScrollView;
    _homeButton = libraryWindow.mediaSourceHomeButton;
    _pathControl = libraryWindow.mediaSourcePathControl;
    _pathControlVisualEffectView = libraryWindow.mediaSourcePathControlVisualEffectView;
    _gridVsListSegmentedControl = libraryWindow.gridVsListSegmentedControl;
}

- (void)setupBaseDataSource
{
    _baseDataSource = [[VLCMediaSourceBaseDataSource alloc] init];
    _baseDataSource.collectionView = _collectionView;
    _baseDataSource.collectionViewScrollView = _collectionViewScrollView;
    _baseDataSource.homeButton = _homeButton;
    _baseDataSource.pathControl = _pathControl;
    _baseDataSource.pathControlContainerView = self.pathControlContainerView;
    _baseDataSource.tableView = _tableView;
    _baseDataSource.tableViewScrollView = _tableViewScrollView;
    [_baseDataSource setupViews];

    _navigationStack = [[VLCLibraryMediaSourceViewNavigationStack alloc] init];
    self.navigationStack.libraryWindow = self.libraryWindow;
    self.navigationStack.baseDataSource = self.baseDataSource;

    self.baseDataSource.navigationStack = self.navigationStack;
}

- (void)setupCollectionView
{
    self.collectionView.allowsMultipleSelection = YES;

    VLCLibraryCollectionViewFlowLayout * const mediaSourceCollectionViewLayout = VLCLibraryCollectionViewFlowLayout.standardLayout;
    self.collectionView.collectionViewLayout = mediaSourceCollectionViewLayout;
    mediaSourceCollectionViewLayout.itemSize = VLCLibraryCollectionViewItem.defaultSize;
}

- (void)setupMediaSourceLibraryViews
{
    _mediaSourceTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;

    const NSEdgeInsets defaultInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _collectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _collectionViewScrollView.contentInsets = defaultInsets;
    _collectionViewScrollView.scrollerInsets = scrollerInsets;
}

- (void)setupPlaceholderLabel
{
    _browsePlaceholderLabel = [NSTextField defaultLabelWithString:_NS("No files")];
    self.browsePlaceholderLabel.font = NSFont.VLClibrarySectionHeaderFont;
    self.browsePlaceholderLabel.textColor = NSColor.secondaryLabelColor;
    self.browsePlaceholderLabel.alignment = NSTextAlignmentCenter;
    self.browsePlaceholderLabel.drawsBackground = NO;
    [self.mediaSourceView addSubview:self.browsePlaceholderLabel];
    [self.mediaSourceView addConstraints:@[
        [self.browsePlaceholderLabel.centerXAnchor constraintEqualToAnchor:self.mediaSourceView.centerXAnchor],
        [self.browsePlaceholderLabel.centerYAnchor constraintEqualToAnchor:self.mediaSourceView.centerYAnchor],
    ]];
    [self updatePlaceholderLabel:nil];
}

- (void)setupPathControlView
{
    if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        NSButton * const homeButton = self.homeButton;
        VLCInputNodePathControl * const pathControl = self.pathControl;
        pathControl.translatesAutoresizingMaskIntoConstraints = NO;
        
        _pathControlGlassEffectView = [[NSGlassEffectView alloc] initWithFrame:self.pathControlVisualEffectView.frame];
        self.pathControlGlassEffectView.translatesAutoresizingMaskIntoConstraints = NO;
        [self.pathControlVisualEffectView removeFromSuperview];
        [self.mediaSourceView addSubview:self.pathControlGlassEffectView];

        _pathControlViewTopConstraintToSuperview =
            [self.pathControlGlassEffectView.topAnchor constraintEqualToAnchor:self.mediaSourceView.topAnchor constant:self.libraryWindow.titlebarHeight];
        [NSLayoutConstraint activateConstraints:@[
            [self.pathControlGlassEffectView.leadingAnchor constraintEqualToAnchor:self.mediaSourceView.leadingAnchor constant:VLCLibraryUIUnits.smallSpacing],
            [self.pathControlGlassEffectView.trailingAnchor constraintEqualToAnchor:self.mediaSourceView.trailingAnchor constant:-VLCLibraryUIUnits.smallSpacing],
        ]];
        NSView * const pathControlContainer = [[NSView alloc] init];
        pathControlContainer.translatesAutoresizingMaskIntoConstraints = NO;
        homeButton.translatesAutoresizingMaskIntoConstraints = NO;
        [pathControlContainer addSubview:homeButton];
        [pathControlContainer addSubview:pathControl];
        [NSLayoutConstraint activateConstraints:@[
            [homeButton.leadingAnchor constraintEqualToAnchor:pathControlContainer.leadingAnchor constant:VLCLibraryUIUnits.smallSpacing],
            [homeButton.centerYAnchor constraintEqualToAnchor:pathControlContainer.centerYAnchor],
            [homeButton.topAnchor constraintGreaterThanOrEqualToAnchor:pathControlContainer.topAnchor constant:VLCLibraryUIUnits.smallSpacing],
            [homeButton.bottomAnchor constraintLessThanOrEqualToAnchor:pathControlContainer.bottomAnchor constant:-VLCLibraryUIUnits.smallSpacing],
            [pathControl.leadingAnchor constraintEqualToAnchor:homeButton.trailingAnchor constant:VLCLibraryUIUnits.smallSpacing],
            [pathControl.trailingAnchor constraintEqualToAnchor:pathControlContainer.trailingAnchor constant:-VLCLibraryUIUnits.smallSpacing],
            [pathControl.centerYAnchor constraintEqualToAnchor:pathControlContainer.centerYAnchor],
            [pathControl.topAnchor constraintGreaterThanOrEqualToAnchor:pathControlContainer.topAnchor constant:VLCLibraryUIUnits.smallSpacing],
            [pathControl.bottomAnchor constraintLessThanOrEqualToAnchor:pathControlContainer.bottomAnchor constant:-VLCLibraryUIUnits.smallSpacing],
        ]];
        self.pathControlGlassEffectView.contentView = pathControlContainer;
#endif
    } else {
        _pathControlViewTopConstraintToSuperview =
            [self.pathControlVisualEffectView.topAnchor constraintEqualToAnchor:self.mediaSourceView.topAnchor constant:self.libraryWindow.titlebarHeight];
    }
    _pathControlViewTopConstraintToSuperview.active = YES;
}

- (void)updatePlaceholderLabel:(NSNotification *)notification
{
    self.browsePlaceholderLabel.hidden = self.mediaSourceTableView.numberOfRows > 0;
}

- (NSView *)pathControlContainerView
{
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    if (@available(macOS 26.0, *))
        return self.pathControlGlassEffectView;
#endif
    return self.pathControlVisualEffectView;
}

- (void)presentBrowseView
{
    [self presentMediaSourceView:VLCLibraryBrowseSegmentType];
}

- (void)presentStreamsView
{
    [self presentMediaSourceView:VLCLibraryStreamsSegmentType];
}

- (void)presentMediaSourceView:(VLCLibrarySegmentType)viewSegment
{
    [self.libraryWindow displayLibraryView:self.mediaSourceView];
    _baseDataSource.mediaSourceMode = viewSegment == VLCLibraryBrowseSegmentType ? VLCMediaSourceModeLAN : VLCMediaSourceModeInternet;
    [_baseDataSource reloadViews];
}

- (void)presentLocalFolderMrl:(NSString *)mrl
{
    [self presentBrowseView];
    [self.baseDataSource presentLocalFolderMrl:mrl];
}

@end
