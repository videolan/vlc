/*****************************************************************************
 * VLCLibrarySegment.m: MacOS X interface module
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

#import "VLCLibrarySegment.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySegmentBookmarkedLocation.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"
#import "library/VLCLibraryWindowToolbarDelegate.h"

#import "library/audio-library/VLCLibraryAudioViewController.h"

#import "library/groups-library/VLCLibraryGroupsViewController.h"

#import "library/home-library/VLCLibraryHomeViewController.h"

#import "library/media-source/VLCLibraryMediaSourceViewController.h"
#import "library/media-source/VLCMediaSource.h"
#import "library/media-source/VLCMediaSourceProvider.h"

#import "library/playlist-library/VLCLibraryPlaylistViewController.h"

#import "library/video-library/VLCLibraryVideoViewController.h"

#import "main/VLCMain.h"

NSString * const VLCLibraryBookmarkedLocationsKey = @"VLCLibraryBookmarkedLocations";
NSString * const VLCLibraryBookmarkedLocationsChanged = @"VLCLibraryBookmarkedLocationsChanged";

static const VLCLibraryWindowToolbarDisplayFlags standardLibraryViewToolbarDisplayFlags =
    VLCLibraryWindowToolbarDisplayFlagSortOrderButton |
    VLCLibraryWindowToolbarDisplayFlagLibrarySearchBar |
    VLCLibraryWindowToolbarDisplayFlagToggleViewModeSegmentButton;

static const VLCLibraryWindowToolbarDisplayFlags mediaSourceViewToolbarDisplayFlags =
    VLCLibraryWindowToolbarDisplayFlagNavigationButtons |
    VLCLibraryWindowToolbarDisplayFlagToggleViewModeSegmentButton;

NSArray<NSString *> *defaultBookmarkedLocations()
{
    NSMutableArray<NSString *> * const locationMrls = NSMutableArray.array;
    NSArray<VLCMediaSource *> * const localMediaSources =
        VLCMediaSourceProvider.listOfLocalMediaSources;

    for (VLCMediaSource * const mediaSource in localMediaSources) {
        VLCInputNode * const rootNode = mediaSource.rootNode;
        [mediaSource preparseInputNodeWithinTree:rootNode];

        for (VLCInputNode * const node in rootNode.children) {
            [locationMrls addObject:node.inputItem.MRL];
        }
    }

    return locationMrls.copy;
}


// MARK: - Properties used by public VLCLibrarySegment methods internally and set by subclasses

@interface VLCLibrarySegment ()

@property NSString *internalDisplayString;
@property NSImage *internalDisplayImage;
@property (nullable) Class internalLibraryViewControllerClass;
@property (nullable) NSArray<NSTreeNode *> *internalChildNodes;
@property (nullable) VLCLibraryAbstractSegmentViewController *(^internalLibraryViewControllerCreator)(void);
@property (nullable) void (^internalLibraryViewPresenter)(VLCLibraryAbstractSegmentViewController *);
@property (nullable) void (^internalSaveViewModePreference)(NSInteger);
@property (nullable) NSInteger (^internalGetViewModePreference)(void);
@property VLCLibraryWindowToolbarDisplayFlags internalToolbarDisplayFlags;

- (instancetype)initWithSegmentType:(VLCLibrarySegmentType)segmentType;

@end


// MARK: - VLCLibraryHeaderSegment

@interface VLCLibraryHeaderSegment : VLCLibrarySegment

- (instancetype)initWithDisplayString:(NSString *)displayString;

@end

@implementation VLCLibraryHeaderSegment

- (instancetype)initWithDisplayString:(NSString *)displayString{
    self = [super initWithSegmentType:VLCLibraryHeaderSegmentType];
    if (self) {
        self.internalDisplayString = displayString;
    }
    return self;
}

@end


// MARK: - VLCLibraryHomeSegment

@interface VLCLibraryHomeSegment : VLCLibrarySegment
@end

@implementation VLCLibraryHomeSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryHomeSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Home");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"house"
                                                  accessibilityDescription:@"Home icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"bw-home"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryHomeViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryHomeViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryHomeViewController *)controller presentHomeView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.homeLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.homeLibraryViewMode;
        };
    }
    return self;
}

@end


// MARK: - Video library view segments

@interface VLCLibraryVideoShowsSubSegment : VLCLibrarySegment
@end

@implementation VLCLibraryVideoShowsSubSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryShowsVideoSubSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Shows");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"tv"
                                                  accessibilityDescription:@"Shows icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"sidebar-movie"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryVideoViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryVideoViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryVideoViewController *)controller presentShowsView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.showsLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.showsLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
    }
    return self;
}

@end


@interface VLCLibraryVideoSegment : VLCLibrarySegment
@end

@implementation VLCLibraryVideoSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryVideoSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Videos");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"film.stack"
                                                  accessibilityDescription:@"Video icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"sidebar-movie"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryVideoViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryVideoViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryVideoViewController *)controller presentVideoView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.videoLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.videoLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
        self.internalChildNodes = @[[[VLCLibraryVideoShowsSubSegment alloc] init]];
    }
    return self;
}

@end


// MARK: - Audio library view segments

@interface VLCLibraryMusicArtistSubSegment : VLCLibrarySegment
@end

@implementation VLCLibraryMusicArtistSubSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryArtistsMusicSubSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Artists");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"music.mic"
                                                  accessibilityDescription:@"Music artists icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"sidebar-music"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryAudioViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryAudioViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryAudioViewController *)controller presentAudioView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.artistLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.artistLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
    }
    return self;
}

@end


@interface VLCLibraryMusicAlbumSubSegment : VLCLibrarySegment
@end

@implementation VLCLibraryMusicAlbumSubSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryAlbumsMusicSubSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Albums");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"square.stack"
                                                  accessibilityDescription:@"Music albums icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"sidebar-music"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryAudioViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryAudioViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryAudioViewController *)controller presentAudioView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.albumLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.albumLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
    }
    return self;
}

@end


@interface VLCLibraryMusicSongSubSegment : VLCLibrarySegment
@end

@implementation VLCLibraryMusicSongSubSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibrarySongsMusicSubSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Songs");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"music.note"
                                                  accessibilityDescription:@"Music songs icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"sidebar-music"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryAudioViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryAudioViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryAudioViewController *)controller presentAudioView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.songsLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.songsLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
    }
    return self;
}

@end


@interface VLCLibraryMusicGenreSubSegment : VLCLibrarySegment
@end

@implementation VLCLibraryMusicGenreSubSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryGenresMusicSubSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Genres");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"guitars"
                                                  accessibilityDescription:@"Music genres icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"sidebar-music"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryAudioViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryAudioViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryAudioViewController *)controller presentAudioView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.genreLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.genreLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
    }
    return self;
}

@end


@interface VLCLibraryMusicSegment : VLCLibrarySegment
@end

@implementation VLCLibraryMusicSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryMusicSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Music");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"music.note"
                                                  accessibilityDescription:@"Music icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"sidebar-music"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryAudioViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryAudioViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryAudioViewController *)controller presentAudioView];
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
        self.internalChildNodes = @[
            [[VLCLibraryMusicArtistSubSegment alloc] init],
            [[VLCLibraryMusicAlbumSubSegment alloc] init],
            [[VLCLibraryMusicSongSubSegment alloc] init],
            [[VLCLibraryMusicGenreSubSegment alloc] init],
        ];
    }
    return self;
}

@end


// MARK: - Playlist library view segments

@interface VLCLibraryPlaylistMusicPlaylistSubSegment : VLCLibrarySegment
@end

@implementation VLCLibraryPlaylistMusicPlaylistSubSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryPlaylistsMusicOnlyPlaylistsSubSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Music playlists");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"music.note.list"
                                                  accessibilityDescription:@"Music playlists icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"sidebar-music"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryPlaylistViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryPlaylistViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryPlaylistViewController *)controller presentPlaylistsViewForPlaylistType:VLC_ML_PLAYLIST_TYPE_AUDIO_ONLY];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.musicOnlyPlaylistLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.musicOnlyPlaylistLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
    }
    return self;
}

@end


@interface VLCLibraryPlaylistVideoPlaylistSubSegment : VLCLibrarySegment
@end

@implementation VLCLibraryPlaylistVideoPlaylistSubSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryPlaylistsVideoOnlyPlaylistsSubSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Video playlists");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"list.and.film"
                                                  accessibilityDescription:@"Video playlists icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"sidebar-movie"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryPlaylistViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryPlaylistViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryPlaylistViewController *)controller presentPlaylistsViewForPlaylistType:VLC_ML_PLAYLIST_TYPE_VIDEO_ONLY];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.videoOnlyPlaylistLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.videoOnlyPlaylistLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
    }
    return self;
}

@end


@interface VLCLibraryPlaylistSegment : VLCLibrarySegment
@end

@implementation VLCLibraryPlaylistSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryPlaylistsSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Playlists");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"list.triangle"
                                                  accessibilityDescription:@"Playlists icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"NSListViewTemplate"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryPlaylistViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryPlaylistViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryPlaylistViewController *)controller presentPlaylistsViewForPlaylistType:VLC_ML_PLAYLIST_TYPE_ALL];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.playlistLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.playlistLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
        self.internalChildNodes = @[
            [[VLCLibraryPlaylistMusicPlaylistSubSegment alloc] init],
            [[VLCLibraryPlaylistVideoPlaylistSubSegment alloc] init]
        ];
    }
    return self;
}

@end


// MARK: - Group library view segments

@interface VLCLibraryGroupSubSegment : VLCLibrarySegment

- (instancetype)initWithGroup:(VLCMediaLibraryGroup *)group;

@end

@implementation VLCLibraryGroupSubSegment

- (instancetype)initWithGroup:(VLCMediaLibraryGroup *)group
{
    self = [super initWithRepresentedObject:group];
    if (self) {
        self.internalDisplayString = group.displayString;
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"play.rectangle"
                                                  accessibilityDescription:@"Group icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"NSTouchBarTagIcon"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryGroupsViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryGroupsViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryGroupsViewController *)controller presentGroupsView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.groupsLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.groupsLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;
    }
    return self;
}

@end


@interface VLCLibraryGroupSegment : VLCLibrarySegment
@end

@implementation VLCLibraryGroupSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryGroupsSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Groups");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"rectangle.3.group"
                                                  accessibilityDescription:@"Groups icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"NSTouchBarTagIcon"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryGroupsViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryGroupsViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryGroupsViewController *)controller presentGroupsView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.groupsLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.groupsLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = standardLibraryViewToolbarDisplayFlags;

        VLCLibraryModel * const libraryModel =
            VLCMain.sharedInstance.libraryController.libraryModel;
        const NSUInteger groupCount = libraryModel.numberOfGroups;

        if (groupCount > 0) {
            NSArray<VLCMediaLibraryGroup *> * const groups = libraryModel.listOfGroups;
            NSMutableArray<VLCLibrarySegment *> * const groupNodes =
                [NSMutableArray arrayWithCapacity:groupCount];

            for (VLCMediaLibraryGroup * const group in groups) {
                [groupNodes addObject:[[VLCLibraryGroupSubSegment alloc] initWithGroup:group]];
            }

            self.internalChildNodes = groupNodes.copy;
        }
    }
    return self;
}

@end


// MARK: - Media source-based library view segments

@interface VLCLibraryBrowseBookmarkedLocationSubSegment : VLCLibrarySegment

- (instancetype)initWithBookmarkedLocation:(VLCLibrarySegmentBookmarkedLocation *)descriptor;

@end

@implementation VLCLibraryBrowseBookmarkedLocationSubSegment

- (instancetype)initWithBookmarkedLocation:(VLCLibrarySegmentBookmarkedLocation *)descriptor
{
    self = [super initWithRepresentedObject:descriptor];
    if (self) {
        self.internalDisplayString = descriptor.name;
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage =
                [NSImage imageWithSystemSymbolName:@"folder"
                          accessibilityDescription:@"Bookmarked location icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"NSFolder"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryGroupsViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryMediaSourceViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryMediaSourceViewController *)controller presentBrowseView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.browseLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.browseLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = mediaSourceViewToolbarDisplayFlags;
    }
    return self;
}

@end


@interface VLCLibraryBrowseSegment : VLCLibrarySegment
@end

@implementation VLCLibraryBrowseSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryBrowseSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Browse");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage = [NSImage imageWithSystemSymbolName:@"folder"
                                                  accessibilityDescription:@"Browse icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"NSFolder"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryMediaSourceViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryMediaSourceViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryMediaSourceViewController *)controller presentBrowseView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.browseLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.browseLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = mediaSourceViewToolbarDisplayFlags;

        NSUserDefaults * const defaults = NSUserDefaults.standardUserDefaults;
        NSArray<NSString *> *bookmarkedLocations =
            [defaults stringArrayForKey:VLCLibraryBookmarkedLocationsKey];
        if (bookmarkedLocations == nil) {
            bookmarkedLocations = defaultBookmarkedLocations();
            [defaults setObject:bookmarkedLocations forKey:VLCLibraryBookmarkedLocationsKey];
        }

        const VLCLibrarySegmentType segmentType = VLCLibraryBrowseBookmarkedLocationSubSegmentType;
        NSMutableArray<NSTreeNode *> * const bookmarkedLocationNodes = NSMutableArray.array;
        NSMutableArray<NSString *> * const remainingBookmarkedLocations = bookmarkedLocations.mutableCopy;

        for (NSString * const locationMrl in bookmarkedLocations) {
            if (![NSFileManager.defaultManager fileExistsAtPath:[NSURL URLWithString:locationMrl].path]) {
                [remainingBookmarkedLocations removeObject:locationMrl];
                continue;
            }
            NSString * const locationName = locationMrl.lastPathComponent;
            VLCLibrarySegmentBookmarkedLocation * const descriptor =
                [[VLCLibrarySegmentBookmarkedLocation alloc] initWithSegmentType:segmentType
                                                                            name:locationName
                                                                             mrl:locationMrl];
            VLCLibraryBrowseBookmarkedLocationSubSegment * const node =
                [[VLCLibraryBrowseBookmarkedLocationSubSegment alloc] initWithBookmarkedLocation:descriptor];
            [bookmarkedLocationNodes addObject:node];
        }

        self.internalChildNodes = bookmarkedLocationNodes.copy;

        if (bookmarkedLocations.count != remainingBookmarkedLocations.count) {
            [defaults setObject:remainingBookmarkedLocations forKey:VLCLibraryBookmarkedLocationsKey];
        }
    }
    return self;
}

@end


@interface VLCLibraryStreamsSegment : VLCLibrarySegment
@end

@implementation VLCLibraryStreamsSegment

- (instancetype)init
{
    self = [super initWithSegmentType:VLCLibraryStreamsSegmentType];
    if (self) {
        self.internalDisplayString = _NS("Streams");
        if (@available(macOS 11.0, *)) {
            self.internalDisplayImage =
                [NSImage imageWithSystemSymbolName:@"antenna.radiowaves.left.and.right"
                          accessibilityDescription:@"Streams icon"];
        } else {
            self.internalDisplayImage = [NSImage imageNamed:@"NSActionTemplate"];
            self.internalDisplayImage.template = YES;
        }
        self.internalLibraryViewControllerClass = VLCLibraryMediaSourceViewController.class;
        self.internalLibraryViewControllerCreator = ^{
            return [[VLCLibraryMediaSourceViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        };
        self.internalLibraryViewPresenter = ^(VLCLibraryAbstractSegmentViewController * const controller) {
            [(VLCLibraryMediaSourceViewController *)controller presentStreamsView];
        };
        self.internalSaveViewModePreference = ^(const NSInteger viewMode) {
            VLCLibraryWindowPersistentPreferences.sharedInstance.streamLibraryViewMode = viewMode;
        };
        self.internalGetViewModePreference = ^{
            return VLCLibraryWindowPersistentPreferences.sharedInstance.streamLibraryViewMode;
        };
        self.internalToolbarDisplayFlags = mediaSourceViewToolbarDisplayFlags;
    }
    return self;
}

@end


// MARK: - VLCLibrarySegment

@implementation VLCLibrarySegment

+ (NSArray<VLCLibrarySegment *> *)librarySegments
{
    return @[
        [[VLCLibraryHomeSegment alloc] init],
        [[VLCLibraryHeaderSegment alloc] initWithDisplayString:_NS("Library")],
        [[VLCLibraryVideoSegment alloc] init],
        [[VLCLibraryMusicSegment alloc] init],
        [[VLCLibraryPlaylistSegment alloc] init],
        [[VLCLibraryGroupSegment alloc] init],
        [[VLCLibraryHeaderSegment alloc] initWithDisplayString:_NS("Explore")],
        [[VLCLibraryBrowseSegment alloc] init],
        [[VLCLibraryStreamsSegment alloc] init],
    ];
}

+ (instancetype)segmentWithSegmentType:(VLCLibrarySegmentType)segmentType
{
    switch (segmentType) {
        case VLCLibraryHomeSegmentType:
            return [[VLCLibraryHomeSegment alloc] init];
        case VLCLibraryVideoSegmentType:
            return [[VLCLibraryVideoSegment alloc] init];
        case VLCLibraryShowsVideoSubSegmentType:
            return [[VLCLibraryVideoShowsSubSegment alloc] init];
        case VLCLibraryMusicSegmentType:
            return [[VLCLibraryMusicSegment alloc] init];
        case VLCLibraryArtistsMusicSubSegmentType:
            return [[VLCLibraryMusicArtistSubSegment alloc] init];
        case VLCLibraryAlbumsMusicSubSegmentType:
            return [[VLCLibraryMusicAlbumSubSegment alloc] init];
        case VLCLibrarySongsMusicSubSegmentType:
            return [[VLCLibraryMusicSongSubSegment alloc] init];
        case VLCLibraryGenresMusicSubSegmentType:
            return [[VLCLibraryMusicGenreSubSegment alloc] init];
        case VLCLibraryPlaylistsSegmentType:
            return [[VLCLibraryPlaylistSegment alloc] init];
        case VLCLibraryPlaylistsMusicOnlyPlaylistsSubSegmentType:
            return [[VLCLibraryPlaylistMusicPlaylistSubSegment alloc] init];
        case VLCLibraryPlaylistsVideoOnlyPlaylistsSubSegmentType:
            return [[VLCLibraryPlaylistVideoPlaylistSubSegment alloc] init];
        case VLCLibraryGroupsSegmentType:
            return [[VLCLibraryGroupSegment alloc] init];
        case VLCLibraryBrowseSegmentType:
            return [[VLCLibraryBrowseSegment alloc] init];
        case VLCLibraryStreamsSegmentType:
            return [[VLCLibraryStreamsSegment alloc] init];
        default:
            return nil;
    }
}

+ (instancetype)segmentForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if ([libraryItem isKindOfClass:VLCMediaLibraryAlbum.class]) {
        return [VLCLibrarySegment segmentWithSegmentType:VLCLibraryAlbumsMusicSubSegmentType];
    } else if ([libraryItem isKindOfClass:VLCMediaLibraryArtist.class]) {
        return [VLCLibrarySegment segmentWithSegmentType:VLCLibraryArtistsMusicSubSegmentType];
    } else if ([libraryItem isKindOfClass:VLCMediaLibraryGenre.class]) {
        return [VLCLibrarySegment segmentWithSegmentType:VLCLibraryGenresMusicSubSegmentType];
    } else if ([libraryItem isKindOfClass:VLCMediaLibraryGroup.class]) {
        return [VLCLibrarySegment segmentWithSegmentType:VLCLibraryGroupsSegmentType];
    }

    VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)libraryItem;
    const BOOL validMediaItem = mediaItem != nil;
    if (validMediaItem && mediaItem.mediaType == VLC_ML_MEDIA_TYPE_AUDIO) {
        return [VLCLibrarySegment segmentWithSegmentType:VLCLibraryMusicSegmentType];
    } else if (validMediaItem && mediaItem.mediaType == VLC_ML_MEDIA_TYPE_VIDEO) {
        if (mediaItem.mediaSubType == VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE) {
            return [VLCLibrarySegment segmentWithSegmentType:VLCLibraryShowsVideoSubSegmentType];
        }
        return [VLCLibrarySegment segmentWithSegmentType:VLCLibraryVideoSegmentType];
    }

    NSLog(@"Unknown library item type provided, cannot find segment for it: %@", libraryItem.displayString);
    return nil;
}

- (instancetype)initWithSegmentType:(VLCLibrarySegmentType)segmentType
{
    return [VLCLibrarySegment treeNodeWithRepresentedObject:@(segmentType)];
}

- (instancetype)initWithRepresentedObject:(id)modelObject
{
    NSInteger segmentValue = VLCLibraryLowSentinelSegment;

    if ([modelObject isKindOfClass:NSNumber.class]) {
        NSNumber * const segmentNumber = (NSNumber *)modelObject;
        segmentValue = segmentNumber.integerValue;
    } else if ([modelObject isKindOfClass:VLCLibrarySegmentBookmarkedLocation.class]) {
        VLCLibrarySegmentBookmarkedLocation * const descriptor =
            (VLCLibrarySegmentBookmarkedLocation *)modelObject;
        segmentValue = descriptor.segmentType;
    } else if ([modelObject isKindOfClass:VLCMediaLibraryGroup.class]) {
        segmentValue = VLCLibraryGroupsGroupSubSegmentType;
    }

    NSAssert(segmentValue > VLCLibraryLowSentinelSegment &&
             segmentValue < VLCLibraryHighSentinelSegment,
             @"VLCLibrarySegment represented object must be a library segment type value!");

    self = [super initWithRepresentedObject:modelObject];
    if (self) {
        _segmentType = segmentValue;
    }
    return self;
}

- (NSArray<NSTreeNode *> *)childNodes
{
    return self.internalChildNodes;
}

- (NSInteger)childCount
{
    return [self childNodes].count;
}

- (NSString *)displayString
{
    return self.internalDisplayString;
}

- (NSImage *)displayImage
{
    return self.internalDisplayImage;
}

- (nullable Class)libraryViewControllerClass
{
    return self.internalLibraryViewControllerClass;
}

- (nullable VLCLibraryAbstractSegmentViewController *)newLibraryViewController
{
    return self.internalLibraryViewControllerCreator();
}

- (void)presentLibraryViewUsingController:(VLCLibraryAbstractSegmentViewController *)controller
{
    self.internalLibraryViewPresenter(controller);
}

- (NSInteger)viewMode
{
    return self.internalGetViewModePreference();
}

- (void)setViewMode:(NSInteger)viewMode
{
    self.internalSaveViewModePreference(viewMode);
}

- (NSUInteger)toolbarDisplayFlags
{
    return self.internalToolbarDisplayFlags;
}

@end
