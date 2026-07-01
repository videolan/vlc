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
#import "extensions/NSView+VLCAdditions.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryController.h"
#import "library/VLCInputNodePathControl.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"

#import "main/VLCMain.h"

#import "views/VLCLoadingOverlayView.h"
#import "views/VLCMediaItemCollectionViewItem.h"

@interface VLCLibraryMediaSourceViewController ()
{
    VLCLoadingOverlayView *_loadingOverlayView;
}

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
@property (readonly) NSGlassEffectView *pathControlGlassEffectView API_AVAILABLE(macos(26.0));
#endif

@end

@implementation VLCLibraryMediaSourceViewController

// The shared mediaSourceView is the source of truth for the active overlay.
- (nullable VLCLoadingOverlayView *)currentLoadingOverlayInMediaSourceView
{
    for (NSView * const subview in self.mediaSourceView.subviews) {
        if ([subview isKindOfClass:VLCLoadingOverlayView.class]) {
            return (VLCLoadingOverlayView *)subview;
        }
    }
    return nil;
}

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
        [defaultCenter addObserver:self
                          selector:@selector(mediaSourceLoadingStarted:)
                              name:VLCMediaSourceDataSourceLoadingStarted
                            object:nil];
        [defaultCenter addObserver:self
                          selector:@selector(mediaSourceLoadingEnded:)
                              name:VLCMediaSourceDataSourceLoadingEnded
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
    mediaSourceCollectionViewLayout.itemSize = VLCLibraryUIUnits.defaultVideoItemCollectionViewItemSize;
}

- (void)setupMediaSourceLibraryViews
{
    _mediaSourceTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;

    const NSEdgeInsets defaultInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _collectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _collectionViewScrollView.contentInsets = defaultInsets;
    _collectionViewScrollView.scrollerInsets = scrollerInsets;

    _tableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _tableViewScrollView.contentInsets = NSEdgeInsetsMake(0, 0, defaultInsets.bottom, 0);
    _tableViewScrollView.scrollerInsets = NSEdgeInsetsMake(0, 0, -defaultInsets.bottom, 0);
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

        NSLayoutYAxisAnchor *topAnchor = self.mediaSourceView.topAnchor;
        const CGFloat topConstant = VLCLibraryUIUnits.libraryWindowContentSafeTopInset;

        _pathControlViewTopConstraintToSuperview =
            [self.pathControlGlassEffectView.topAnchor constraintEqualToAnchor:topAnchor constant:topConstant];
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
        NSLayoutYAxisAnchor *topAnchor = self.mediaSourceView.topAnchor;
        const CGFloat topConstant = VLCLibraryUIUnits.libraryWindowContentSafeTopInset;

        _pathControlViewTopConstraintToSuperview =
            [self.pathControlVisualEffectView.topAnchor constraintEqualToAnchor:topAnchor constant:topConstant];
    }
    _pathControlViewTopConstraintToSuperview.active = YES;
}

- (void)updatePlaceholderLabel:(NSNotification *)notification
{
    _loadingOverlayView = [self currentLoadingOverlayInMediaSourceView];
    const BOOL overlayPresent = _loadingOverlayView != nil;
    self.browsePlaceholderLabel.hidden = self.baseDataSource.hasDisplayedItems || overlayPresent;
}

- (void)prepareLoadingOverlay
{
    _loadingOverlayView = [self currentLoadingOverlayInMediaSourceView];

    if (_loadingOverlayView != nil)
        return;
    if (!_loadingOverlayView)
        _loadingOverlayView = [[VLCLoadingOverlayView alloc] init];
    _loadingOverlayView.translatesAutoresizingMaskIntoConstraints = NO;
    _loadingOverlayView.wantsLayer = YES;
    _loadingOverlayView.alphaValue = 0.0;
    [self.mediaSourceView addSubview:_loadingOverlayView];
    [_loadingOverlayView applyConstraintsToFillSuperview];
    [_loadingOverlayView.indicator startAnimation:self];
}

- (void)mediaSourceLoadingStarted:(NSNotification *)notification
{
    _loadingOverlayView = [self currentLoadingOverlayInMediaSourceView];

    [self prepareLoadingOverlay];
    _loadingOverlayView.alphaValue = 1.0;
    [self updatePlaceholderLabel:nil];
}

- (void)mediaSourceLoadingEnded:(NSNotification *)notification
{
    _loadingOverlayView = [self currentLoadingOverlayInMediaSourceView];

    if (_loadingOverlayView == nil)
        return;

    _loadingOverlayView.alphaValue = 1.0;
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        _loadingOverlayView.animator.alphaValue = 0.0;
    } completionHandler:^{
        [_loadingOverlayView removeFromSuperview];
        [_loadingOverlayView.indicator stopAnimation:self];
        [self updatePlaceholderLabel:nil];
    }];
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
    const BOOL isStreams = viewSegment != VLCLibraryBrowseSegmentType;
    _loadingOverlayView = [self currentLoadingOverlayInMediaSourceView];

    if (!isStreams) {
        [self mediaSourceLoadingEnded:nil];
    }

    _baseDataSource.mediaSourceMode = isStreams ? VLCMediaSourceModeInternet : VLCMediaSourceModeLAN;
    [_baseDataSource reloadViews];
}

- (void)browseFolderByMrl:(NSString *)mrl
{
    [self presentBrowseView];
    [self.baseDataSource browseFolderByMrl:mrl];
}

@end
