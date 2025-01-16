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
        self.internalChildNodes = @[[[VLCLibraryVideoShowsSubSegment alloc] init]];
    }
    return self;
}

@end


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
            [[VLCLibraryMusicArtistSubSegment alloc] init],
            [[VLCLibraryMusicAlbumSubSegment alloc] init],
            [[VLCLibraryMusicSongSubSegment alloc] init],
            [[VLCLibraryMusicGenreSubSegment alloc] init],
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
        case VLCLibraryVideoShowsSubSegment:
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
        case VLCLibraryLowSentinelSegment:
        case VLCLibraryHighSentinelSegment:
            NSAssert(NO, @"Invalid segment value");
    }
    return nil;
}

- (NSImage *)oldIconImageForType:(VLCLibrarySegmentType)segmentType
{
    switch (segmentType) {
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
        case VLCLibraryHeaderSegmentType:
            return nil;
        case VLCLibraryHomeSegmentType:
            return VLCLibraryHomeViewController.class;
        case VLCLibraryVideoSegmentType:
        case VLCLibraryShowsVideoSubSegmentType:
            return VLCLibraryVideoViewController.class;
        case VLCLibraryArtistsMusicSubSegmentType:
        case VLCLibraryAlbumsMusicSubSegmentType:
        case VLCLibrarySongsMusicSubSegmentType:
        case VLCLibraryGenresMusicSubSegmentType:
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
        case VLCLibraryHeaderSegmentType:
            return nil;
        case VLCLibraryHomeSegmentType:
            return [[VLCLibraryHomeViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        case VLCLibraryVideoSegmentType:
        case VLCLibraryShowsVideoSubSegmentType:
            return [[VLCLibraryVideoViewController alloc] initWithLibraryWindow:VLCMain.sharedInstance.libraryWindow];
        case VLCLibraryMusicSegmentType:
        case VLCLibraryArtistsMusicSubSegmentType:
        case VLCLibraryAlbumsMusicSubSegmentType:
        case VLCLibrarySongsMusicSubSegmentType:
        case VLCLibraryGenresMusicSubSegmentType:
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
