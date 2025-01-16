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
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySegmentBookmarkedLocation.h"
#import "library/VLCLibraryWindow.h"

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


@interface VLCLibrarySegment ()

@property NSString *internalDisplayString;
@property NSImage *internalDisplayImage;
@property (nullable) Class internalLibraryViewControllerClass;
@property (nullable) NSArray<NSTreeNode *> *internalChildNodes;

- (instancetype)initWithSegmentType:(VLCLibrarySegmentType)segmentType;

@end


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
        self.internalChildNodes =
            @[[VLCLibrarySegment segmentWithSegmentType:VLCLibraryShowsVideoSubSegment]];
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
        self.internalChildNodes = @[
            [VLCLibrarySegment segmentWithSegmentType:VLCLibraryArtistsMusicSubSegment],
            [VLCLibrarySegment segmentWithSegmentType:VLCLibraryAlbumsMusicSubSegment],
            [VLCLibrarySegment segmentWithSegmentType:VLCLibrarySongsMusicSubSegment],
            [VLCLibrarySegment segmentWithSegmentType:VLCLibraryGenresMusicSubSegment],
        ];
    }
    return self;
}

@end


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
        self.internalChildNodes = @[
            [[VLCLibraryPlaylistMusicPlaylistSubSegment alloc] init],
            [[VLCLibraryPlaylistVideoPlaylistSubSegment alloc] init]
        ];
    }
    return self;
}

@end


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

        NSUserDefaults * const defaults = NSUserDefaults.standardUserDefaults;
        NSArray<NSString *> *bookmarkedLocations =
            [defaults stringArrayForKey:VLCLibraryBookmarkedLocationsKey];
        if (bookmarkedLocations == nil) {
            bookmarkedLocations = defaultBookmarkedLocations();
            [defaults setObject:bookmarkedLocations forKey:VLCLibraryBookmarkedLocationsKey];
        }

        const VLCLibrarySegmentType segmentType = VLCLibraryBrowseBookmarkedLocationSubSegmentType;
        NSMutableArray<NSTreeNode *> * const bookmarkedLocationNodes = NSMutableArray.array;

        for (NSString * const locationMrl in bookmarkedLocations) {
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
    }
    return self;
}

@end


@implementation VLCLibrarySegment

+ (NSArray<VLCLibrarySegment *> *)librarySegments
{
    return @[
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryHomeSegmentType],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryHeaderSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryVideoSegmentType],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryMusicSegmentType],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryPlaylistsSegmentType],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryGroupsSegmentType],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryExploreHeaderSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryBrowseSegmentType],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryStreamsSegmentType],
    ];
}

+ (instancetype)segmentWithSegmentType:(VLCLibrarySegmentType)segmentType
{
    if (segmentType == VLCLibraryHomeSegmentType) {
        return [[VLCLibraryHomeSegment alloc] init];
    } else if (segmentType == VLCLibraryVideoSegmentType) {
        return [[VLCLibraryVideoSegment alloc] init];
    } else if (segmentType == VLCLibraryMusicSegmentType) {
        return [[VLCLibraryMusicSegment alloc] init];
    } else if (segmentType == VLCLibraryPlaylistsSegmentType) {
        return [[VLCLibraryPlaylistSegment alloc] init];
    } else if (segmentType == VLCLibraryGroupsSegmentType) {
        return [[VLCLibraryGroupSegment alloc] init];
    } else if (segmentType == VLCLibraryBrowseSegmentType) {
        return [[VLCLibraryBrowseSegment alloc] init];
    } else if (segmentType == VLCLibraryStreamsSegmentType) {
        return [[VLCLibraryStreamsSegment alloc] init];
    }
    return [[VLCLibrarySegment alloc] initWithSegmentType:segmentType];
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
        [self updateSegmentTypeRepresentation];
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

- (NSString *)displayStringForType:(VLCLibrarySegmentType)segmentType
{
    switch (segmentType) {
        case VLCLibraryHeaderSegment:
            return _NS("Library");
        case VLCLibraryArtistsMusicSubSegment:
            return _NS("Artists");
        case VLCLibraryAlbumsMusicSubSegment:
            return _NS("Albums");
        case VLCLibrarySongsMusicSubSegment:
            return _NS("Songs");
        case VLCLibraryGenresMusicSubSegment:
            return _NS("Genres");
        case VLCLibraryShowsVideoSubSegment:
            return _NS("Shows");
        case VLCLibraryExploreHeaderSegment:
            return _NS("Explore");
        case VLCLibraryLowSentinelSegment:
        case VLCLibraryHighSentinelSegment:
            NSAssert(NO, @"Invalid segment value");
    }
    return nil;
}

- (NSImage *)oldIconImageForType:(VLCLibrarySegmentType)segmentType
{
    switch (segmentType) {
        case VLCLibraryHeaderSegment:
            return nil;
        case VLCLibraryArtistsMusicSubSegment:
        case VLCLibraryAlbumsMusicSubSegment:
        case VLCLibrarySongsMusicSubSegment:
        case VLCLibraryGenresMusicSubSegment:
            return [NSImage imageNamed:@"sidebar-music"];
        case VLCLibraryShowsVideoSubSegment:
            return [NSImage imageNamed:@"sidebar-movie"];
        case VLCLibraryExploreHeaderSegment:
            return nil;
        case VLCLibraryLowSentinelSegment:
        case VLCLibraryHighSentinelSegment:
            NSAssert(NO, @"Invalid segment value");
            return nil;
    }

    return nil;
}

- (NSImage *)newIconImageForType:(VLCLibrarySegmentType)segmentType
{
    if (@available(macOS 11.0, *)) {
        switch (segmentType) {
        case VLCLibraryHeaderSegment:
            return [NSImage imageWithSystemSymbolName:@"books.vertical.fill"
                             accessibilityDescription:@"Library icon"];
        case VLCLibraryArtistsMusicSubSegment:
            return [NSImage imageWithSystemSymbolName:@"music.mic"
                             accessibilityDescription:@"Music artists icon"];
        case VLCLibraryAlbumsMusicSubSegment:
            return [NSImage imageWithSystemSymbolName:@"square.stack"
                             accessibilityDescription:@"Music albums icon"];
        case VLCLibrarySongsMusicSubSegment:
            return [NSImage imageWithSystemSymbolName:@"music.note"
                             accessibilityDescription:@"Music songs icon"];
        case VLCLibraryGenresMusicSubSegment:
                return [NSImage imageWithSystemSymbolName:@"guitars"
                                 accessibilityDescription:@"Music genres icon"];
        case VLCLibraryShowsVideoSubSegment:
            return [NSImage imageWithSystemSymbolName:@"tv"
                             accessibilityDescription:@"Shows icon"];
        case VLCLibraryExploreHeaderSegment:
            return [NSImage imageWithSystemSymbolName:@"sailboat.fill"
                             accessibilityDescription:@"Explore icon"];
        case VLCLibraryLowSentinelSegment:
        case VLCLibraryHighSentinelSegment:
            NSAssert(NO, @"Invalid segment value");
            return nil;
        }
    }
    return nil;
}

- (NSImage *)iconForType:(VLCLibrarySegmentType)segmentType
{
    NSImage *iconImage;
    if (@available(macOS 11.0, *)) {
        iconImage = [self newIconImageForType:segmentType];
    } else {
        iconImage = [self oldIconImageForType:segmentType];
        iconImage.template = YES;
    }

    return iconImage;
}

- (void)updateSegmentTypeRepresentation
{
    NSString * const displayString = [self displayStringForType:_segmentType];
    NSImage * const displayImage = [self iconForType:_segmentType];

    if (displayString) {
        self.internalDisplayString = displayString;
    }

    if (displayImage) {
        self.internalDisplayImage = displayImage;
    }
}

+ (nullable Class)libraryViewControllerClassForSegmentType:(VLCLibrarySegmentType)segmentType
{
    switch (segmentType) {
        case VLCLibraryLowSentinelSegment:
        case VLCLibraryHighSentinelSegment:
        case VLCLibraryHeaderSegment:
        case VLCLibraryExploreHeaderSegment:
            return nil;
        case VLCLibraryHomeSegmentType:
            return VLCLibraryHomeViewController.class;
        case VLCLibraryVideoSegmentType:
        case VLCLibraryShowsVideoSubSegment:
            return VLCLibraryVideoViewController.class;
        case VLCLibraryArtistsMusicSubSegment:
        case VLCLibraryAlbumsMusicSubSegment:
        case VLCLibrarySongsMusicSubSegment:
        case VLCLibraryGenresMusicSubSegment:
            return VLCLibraryAudioViewController.class;
        case VLCLibraryPlaylistsSegmentType:
        case VLCLibraryPlaylistsMusicOnlyPlaylistsSubSegmentType:
        case VLCLibraryPlaylistsVideoOnlyPlaylistsSubSegmentType:
            return VLCLibraryPlaylistViewController.class;
        case VLCLibraryGroupsSegmentType:
        case VLCLibraryGroupsGroupSubSegmentType:
            return VLCLibraryGroupsViewController.class;
        case VLCLibraryBrowseSegmentType:
        case VLCLibraryBrowseBookmarkedLocationSubSegmentType:
        case VLCLibraryStreamsSegmentType:
            return VLCLibraryMediaSourceViewController.class;
    }
}

+ (VLCLibraryAbstractSegmentViewController *)libraryViewControllerForSegmentType:(VLCLibrarySegmentType)segmentType
{
    switch (segmentType) {
        case VLCLibraryLowSentinelSegment:
        case VLCLibraryHighSentinelSegment:
        case VLCLibraryHeaderSegment:
        case VLCLibraryExploreHeaderSegment:
            return nil;
        case VLCLibraryHomeSegmentType:
            return [[VLCLibraryHomeViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        case VLCLibraryVideoSegmentType:
        case VLCLibraryShowsVideoSubSegment:
            return [[VLCLibraryVideoViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        case VLCLibraryMusicSegmentType:
        case VLCLibraryArtistsMusicSubSegment:
        case VLCLibraryAlbumsMusicSubSegment:
        case VLCLibrarySongsMusicSubSegment:
        case VLCLibraryGenresMusicSubSegment:
            return [[VLCLibraryAudioViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        case VLCLibraryPlaylistsSegmentType:
        case VLCLibraryPlaylistsMusicOnlyPlaylistsSubSegmentType:
        case VLCLibraryPlaylistsVideoOnlyPlaylistsSubSegmentType:
            return [[VLCLibraryPlaylistViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        case VLCLibraryGroupsSegmentType:
        case VLCLibraryGroupsGroupSubSegmentType:
            return [[VLCLibraryGroupsViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        case VLCLibraryBrowseSegmentType:
        case VLCLibraryBrowseBookmarkedLocationSubSegmentType:
        case VLCLibraryStreamsSegmentType:
            return [[VLCLibraryMediaSourceViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
    }
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

@end
