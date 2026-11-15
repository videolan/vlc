/*****************************************************************************
 * VLCLibraryMusicViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLibraryMusicViewController.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSScrollView+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryWindow.h"
#import "library/audio-library/VLCLibraryMusicCarouselContainerView.h"
#import "library/search-library/VLCLibrarySearchProvider.h"
#import "library/search-library/VLCLibrarySearchViewController.h"

#import "views/VLCUIUnits.h"

@interface VLCLibraryMusicDocumentView : NSView
@end

@implementation VLCLibraryMusicDocumentView

- (BOOL)isFlipped
{
    return YES;
}

@end

@implementation VLCLibraryMusicViewController
{
    NSScrollView *_scrollView;
    NSArray<VLCLibraryMusicCarouselContainerView *> *_carousels;
}

@synthesize placeholderImageViewSizeConstraints = _placeholderImageViewSizeConstraints;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];
    if (self) {
        _placeholderImageViewSizeConstraints = @[
            [self.placeholderImageView.widthAnchor constraintEqualToConstant:149.],
            [self.placeholderImageView.heightAnchor constraintEqualToConstant:149.],
        ];
        [self setupCarousels];
        [self setupScrollView];
    }
    return self;
}

- (void)setupCarousels
{
    _carousels = @[
        [[VLCLibraryMusicCarouselContainerView alloc]
                   initWithTitle:_NS("Recently added")
                      parentType:VLCMediaLibraryParentGroupTypeRecentlyAddedAudios
                  librarySegment:VLCLibraryLowSentinelSegment
                   itemsProvider:^NSArray<id<VLCMediaLibraryItemProtocol>> *(VLCLibraryModel *model) {
                       return model.listOfRecentlyAddedAudioMedia;
                   }
              resetNotifications:@[VLCLibraryModelRecentlyAddedAudioMediaListReset]
                    numberOfRows:2],
        [[VLCLibraryMusicCarouselContainerView alloc]
                   initWithTitle:_NS("Recently played")
                      parentType:VLCMediaLibraryParentGroupTypeRecentAudios
                  librarySegment:VLCLibraryLowSentinelSegment
                   itemsProvider:^NSArray<id<VLCMediaLibraryItemProtocol>> *(VLCLibraryModel *model) {
                       return model.listOfRecentAudioMedia;
                   }
              resetNotifications:@[
                  VLCLibraryModelRecentAudioMediaListReset,
                  VLCLibraryModelRecentAudioMediaItemUpdated,
                  VLCLibraryModelRecentAudioMediaItemDeleted,
              ]],
        [[VLCLibraryMusicCarouselContainerView alloc]
                   initWithTitle:_NS("Favorites")
                      parentType:VLCMediaLibraryParentGroupTypeFavoriteAudios
                  librarySegment:VLCLibraryLowSentinelSegment
                   itemsProvider:^NSArray<id<VLCMediaLibraryItemProtocol>> *(VLCLibraryModel *model) {
                       return model.listOfFavoriteAudioMedia;
                   }
              resetNotifications:@[
                  VLCLibraryModelFavoriteAudioMediaListReset,
                  VLCLibraryModelMediaItemUpdated,
                  VLCLibraryModelMediaItemDeleted,
              ]
                    numberOfRows:3
                       itemStyle:VLCLibraryMusicCarouselItemStyleCompact],
        [[VLCLibraryMusicCarouselContainerView alloc]
                   initWithTitle:_NS("Most played")
                      parentType:VLCMediaLibraryParentGroupTypeMostPlayedAudios
                  librarySegment:VLCLibraryLowSentinelSegment
                   itemsProvider:^NSArray<id<VLCMediaLibraryItemProtocol>> *(VLCLibraryModel *model) {
                       return model.listOfMostPlayedAudioMedia;
                   }
              resetNotifications:@[
                  VLCLibraryModelAudioMediaListReset,
                  VLCLibraryModelRecentAudioMediaListReset,
                  VLCLibraryModelMediaItemUpdated,
                  VLCLibraryModelMediaItemDeleted,
              ]],
        [[VLCLibraryMusicCarouselContainerView alloc]
                   initWithTitle:_NS("Playlists")
                      parentType:VLCMediaLibraryParentGroupTypeUnknown
                  librarySegment:VLCLibraryPlaylistsMusicOnlyPlaylistsSubSegmentType
                   itemsProvider:^NSArray<id<VLCMediaLibraryItemProtocol>> *(VLCLibraryModel *model) {
                       return [model listOfPlaylistsOfType:VLC_ML_PLAYLIST_TYPE_AUDIO_ONLY] ?: @[];
                   }
              resetNotifications:@[
                  VLCLibraryModelPlaylistAdded,
                  VLCLibraryModelPlaylistUpdated,
                  VLCLibraryModelPlaylistDeleted,
              ]],
        [[VLCLibraryMusicCarouselContainerView alloc]
                   initWithTitle:_NS("Genres")
                      parentType:VLCMediaLibraryParentGroupTypeUnknown
                  librarySegment:VLCLibraryGenresMusicSubSegmentType
                   itemsProvider:^NSArray<id<VLCMediaLibraryItemProtocol>> *(VLCLibraryModel *model) {
                       return model.listOfGenres;
                   }
              resetNotifications:@[
                  VLCLibraryModelGenreListReset,
                  VLCLibraryModelGenreUpdated,
                  VLCLibraryModelGenreDeleted,
              ]
                    numberOfRows:3
                       itemStyle:VLCLibraryMusicCarouselItemStyleCompact],
    ];

    for (VLCLibraryMusicCarouselContainerView * const carousel in _carousels) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(carouselItemsChanged:)
                                                   name:VLCLibraryMusicCarouselItemsChangedNotification
                                                 object:carousel];
    }
}

- (void)setupScrollView
{
    NSStackView * const stackView = [[NSStackView alloc] init];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.orientation = NSUserInterfaceLayoutOrientationVertical;
    stackView.alignment = NSLayoutAttributeLeading;
    stackView.distribution = NSStackViewDistributionFill;
    stackView.detachesHiddenViews = YES;
    stackView.spacing = VLCUIUnits.largeSpacing;

    NSTextField * const titleLabel = [NSTextField labelWithString:_NS("Music")];
    titleLabel.font = NSFont.VLCLibraryPageTitleFont;
    [stackView addArrangedSubview:titleLabel];

    NSView * const documentView = [[VLCLibraryMusicDocumentView alloc] init];
    documentView.translatesAutoresizingMaskIntoConstraints = NO;
    [documentView addSubview:stackView];

    NSEdgeInsets contentInsets = VLCUIUnits.libraryViewScrollViewContentInsets;
    contentInsets.left = 0.;
    contentInsets.right = 0.;
    _scrollView = [NSScrollView libraryScrollViewWithDocumentView:documentView
                                                    contentInsets:contentInsets];
    NSEdgeInsets scrollerInsets = _scrollView.scrollerInsets;
    scrollerInsets.left = 0.;
    scrollerInsets.right = 0.;
    _scrollView.scrollerInsets = scrollerInsets;
    _scrollView.hasVerticalScroller = YES;
    _scrollView.autohidesScrollers = YES;
    _scrollView.drawsBackground = NO;
    NSClipView * const clipView = _scrollView.contentView;

    // The carousel scroll views and their arrows extend beyond the row edges.
    const CGFloat horizontalPadding = 2. * VLCUIUnits.mediumSpacing;
    [NSLayoutConstraint activateConstraints:@[
        [stackView.topAnchor constraintEqualToAnchor:documentView.topAnchor],
        [stackView.bottomAnchor constraintEqualToAnchor:documentView.bottomAnchor],
        [stackView.leadingAnchor constraintEqualToAnchor:documentView.leadingAnchor constant:horizontalPadding],
        [stackView.trailingAnchor constraintEqualToAnchor:documentView.trailingAnchor constant:-horizontalPadding],
        [documentView.topAnchor constraintEqualToAnchor:clipView.topAnchor],
        [documentView.leadingAnchor constraintEqualToAnchor:clipView.leadingAnchor],
        [documentView.trailingAnchor constraintEqualToAnchor:clipView.trailingAnchor],
    ]];

    for (VLCLibraryMusicCarouselContainerView * const carousel in _carousels) {
        carousel.translatesAutoresizingMaskIntoConstraints = NO;
        [stackView addArrangedSubview:carousel];
        [carousel.widthAnchor constraintEqualToAnchor:stackView.widthAnchor].active = YES;
    }
}

- (void)presentMusicView
{
    if (!self.connected && !self.libraryWindow.embeddedVideoPlaybackActive) {
        [self connect];
    }
    [self presentOverview];
}

- (VLCLibrarySearchViewController *)newSearchViewController
{
    return [[VLCLibrarySearchViewController alloc] initWithLibraryWindow:self.libraryWindow
                                                               providers:VLCLibrarySearchProvider.audioProviders];
}

- (void)connect
{
    [super connect];
    for (VLCLibraryMusicCarouselContainerView * const carousel in _carousels) {
        [carousel connect];
    }
}

- (void)disconnect
{
    [super disconnect];
    for (VLCLibraryMusicCarouselContainerView * const carousel in _carousels) {
        [carousel disconnect];
    }
}

- (void)carouselItemsChanged:(NSNotification *)notification
{
    if (self.libraryWindow.librarySegmentViewController == self &&
        !self.libraryWindow.embeddedVideoPlaybackActive &&
        !self.isShowingSearchOverlay) {
        [self presentOverviewContent];
    }
}

- (void)presentOverviewContent
{
    BOOL showPlaceholder = YES;
    for (VLCLibraryMusicCarouselContainerView * const carousel in _carousels) {
        if (!carousel.hasLoadedItems || !carousel.hidden) {
            showPlaceholder = NO;
            break;
        }
    }

    if (showPlaceholder) {
        if (self.emptyLibraryView.superview != self.libraryTargetView) {
            [self.libraryWindow displayLibraryPlaceholderViewWithImage:NSImage.VLCPlaceholderMusicImage
                                                      usingConstraints:self.placeholderImageViewSizeConstraints
                                                     displayingMessage:_NS("Your music will appear here.\nGo to the Browse section to add music you love.")];
        }
    } else if (_scrollView.superview != self.libraryTargetView) {
        [self.libraryWindow displayLibraryView:_scrollView];
    }
}

@end
