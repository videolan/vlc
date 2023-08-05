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

#import "library/media-source/VLCMediaSource.h"
#import "library/media-source/VLCMediaSourceProvider.h"

#import "main/VLCMain.h"

NSString * const VLCLibraryBookmarkedLocationsKey = @"VLCLibraryBookmarkedLocations";
NSString * const VLCLibraryBookmarkedLocationsChanged = @"VLCLibraryBookmarkedLocationsChanged";

@implementation VLCLibrarySegment

+ (NSArray<VLCLibrarySegment *> *)librarySegments
{
    return @[
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryHomeSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryVideoSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryMusicSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryPlaylistsSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryBrowseSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryStreamsSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryGroupsSegment]
    ];
}

+ (instancetype)segmentWithSegmentType:(VLCLibrarySegmentType)segmentType
{
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
        segmentValue = VLCLibraryGroupsGroupSubSegment;
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
    if (self.segmentType == VLCLibraryVideoSegment) {
        return @[[VLCLibrarySegment segmentWithSegmentType:VLCLibraryShowsVideoSubSegment]];
    } else if (self.segmentType == VLCLibraryMusicSegment) {
        return @[
            [VLCLibrarySegment segmentWithSegmentType:VLCLibraryArtistsMusicSubSegment],
            [VLCLibrarySegment segmentWithSegmentType:VLCLibraryAlbumsMusicSubSegment],
            [VLCLibrarySegment segmentWithSegmentType:VLCLibrarySongsMusicSubSegment],
            [VLCLibrarySegment segmentWithSegmentType:VLCLibraryGenresMusicSubSegment],
        ];
    } else if (self.segmentType == VLCLibraryBrowseSegment) {
        NSUserDefaults * const defaults = NSUserDefaults.standardUserDefaults;
        NSArray<NSString *> *bookmarkedLocations =
            [defaults stringArrayForKey:VLCLibraryBookmarkedLocationsKey];
        if (bookmarkedLocations == nil) {
            bookmarkedLocations = self.defaultBookmarkedLocations;
            [defaults setObject:bookmarkedLocations forKey:VLCLibraryBookmarkedLocationsKey];
        }

        const VLCLibrarySegmentType segmentType = VLCLibraryBrowseBookmarkedLocationSubSegment;
        NSMutableArray<NSTreeNode *> * const bookmarkedLocationNodes = NSMutableArray.array;

        for (NSString * const locationMrl in bookmarkedLocations) {
            NSString * const locationName = locationMrl.lastPathComponent;
            VLCLibrarySegmentBookmarkedLocation * const descriptor =
                [[VLCLibrarySegmentBookmarkedLocation alloc] initWithSegmentType:segmentType
                                                                            name:locationName
                                                                             mrl:locationMrl];
            VLCLibrarySegment * const node = 
                [VLCLibrarySegment treeNodeWithRepresentedObject:descriptor];
            [bookmarkedLocationNodes addObject:node];
        }

        return bookmarkedLocationNodes.copy;
    } else if (self.segmentType == VLCLibraryGroupsSegment) {
        VLCLibraryModel * const libraryModel =
            VLCMain.sharedInstance.libraryController.libraryModel;
        const NSUInteger groupCount = libraryModel.numberOfGroups;
        if (groupCount == 0) {
            return nil;
        }

        NSArray<VLCMediaLibraryGroup *> * const groups = libraryModel.listOfGroups;
        NSMutableArray<VLCLibrarySegment *> * const groupNodes =
            [NSMutableArray arrayWithCapacity:groupCount];

        for (VLCMediaLibraryGroup * const group in groups) {
            VLCLibrarySegment * const node =
                [VLCLibrarySegment treeNodeWithRepresentedObject:group];
            [groupNodes addObject:node];
        }

        return groupNodes.copy;
    }

    return nil;
}

- (NSArray<NSString *> *)defaultBookmarkedLocations
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

- (NSInteger)childCount
{
    return [self childNodes].count;
}

- (NSString *)displayStringForType:(VLCLibrarySegmentType)segmentType
{
    switch (segmentType) {
        case VLCLibraryHomeSegment:
            return _NS("Home");
        case VLCLibraryMusicSegment:
            return _NS("Music");
        case VLCLibraryArtistsMusicSubSegment:
            return _NS("Artists");
        case VLCLibraryAlbumsMusicSubSegment:
            return _NS("Albums");
        case VLCLibrarySongsMusicSubSegment:
            return _NS("Songs");
        case VLCLibraryGenresMusicSubSegment:
            return _NS("Genres");
        case VLCLibraryVideoSegment:
            return _NS("Videos");
        case VLCLibraryShowsVideoSubSegment:
            return _NS("Shows");
        case VLCLibraryPlaylistsSegment:
            return _NS("Playlists");
        case VLCLibraryBrowseSegment:
            return _NS("Browse");
        case VLCLibraryBrowseBookmarkedLocationSubSegment:
            NSAssert(NO, @"displayStringForType should not be called for this segment type");
        case VLCLibraryStreamsSegment:
            return _NS("Streams");
        case VLCLibraryGroupsSegment:
            return _NS("Groups");
        case VLCLibraryGroupsGroupSubSegment:
            NSAssert(NO, @"displayStringForType should not be called for this segment type");
        case VLCLibraryLowSentinelSegment:
        case VLCLibraryHighSentinelSegment:
        default:
            NSAssert(true, @"Invalid segment value");
    }
    return nil;
}

- (NSImage *)oldIconImageForType:(VLCLibrarySegmentType)segmentType
{
    switch (segmentType) {
        case VLCLibraryHomeSegment:
            return [NSImage imageNamed:@"bw-home"];
        case VLCLibraryMusicSegment:
        case VLCLibraryArtistsMusicSubSegment:
        case VLCLibraryAlbumsMusicSubSegment:
        case VLCLibrarySongsMusicSubSegment:
        case VLCLibraryGenresMusicSubSegment:
            return [NSImage imageNamed:@"sidebar-music"];
        case VLCLibraryVideoSegment:
        case VLCLibraryShowsVideoSubSegment:
            return [NSImage imageNamed:@"sidebar-movie"];
        case VLCLibraryPlaylistsSegment:
            return [NSImage imageNamed:@"sidebar-music"];
        case VLCLibraryBrowseSegment:
        case VLCLibraryBrowseBookmarkedLocationSubSegment:
            return [NSImage imageNamed:@"NSFolder"];
        case VLCLibraryStreamsSegment:
            return [NSImage imageNamed:@"NSActionTemplate"];
        case VLCLibraryGroupsSegment:
        case VLCLibraryGroupsGroupSubSegment:
            return [NSImage imageNamed:@"NSTouchBarTagIcon"];
        default:
            NSAssert(true, @"Invalid segment value");
            return nil;
    }
}

- (NSImage *)newIconImageForType:(VLCLibrarySegmentType)segmentType
{
    if (@available(macOS 11.0, *)) {
        switch (segmentType) {
        case VLCLibraryHomeSegment:
            return [NSImage imageWithSystemSymbolName:@"house"
                             accessibilityDescription:@"Home icon"];
        case VLCLibraryMusicSegment:
            return [NSImage imageWithSystemSymbolName:@"music.note"
                              accessibilityDescription:@"Music icon"];
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
        case VLCLibraryVideoSegment:
            return [NSImage imageWithSystemSymbolName:@"film.stack"
                             accessibilityDescription:@"Video icon"];
        case VLCLibraryShowsVideoSubSegment:
            return [NSImage imageWithSystemSymbolName:@"tv"
                             accessibilityDescription:@"Shows icon"];
        case VLCLibraryPlaylistsSegment:
            return [NSImage imageWithSystemSymbolName:@"music.note.list"
                             accessibilityDescription:@"Playlists icon"];
        case VLCLibraryBrowseSegment:
            return [NSImage imageWithSystemSymbolName:@"folder"
                             accessibilityDescription:@"Browse icon"];
        case VLCLibraryBrowseBookmarkedLocationSubSegment:
            return [NSImage imageWithSystemSymbolName:@"folder"
                             accessibilityDescription:@"Bookmarked location icon"];
        case VLCLibraryStreamsSegment:
            return [NSImage imageWithSystemSymbolName:@"antenna.radiowaves.left.and.right"
                             accessibilityDescription:@"Streams icon"];
        case VLCLibraryGroupsSegment:
            return [NSImage imageWithSystemSymbolName:@"rectangle.3.group"
                             accessibilityDescription:@"Groups icon"];
        case VLCLibraryGroupsGroupSubSegment:
            return [NSImage imageWithSystemSymbolName:@"play.rectangle"
                             accessibilityDescription:@"Group icon"];
        default:
            NSAssert(true, @"Invalid segment value");
            return nil;
        }
    } else {
        return nil;
    }
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
    if ([self.representedObject isKindOfClass:VLCLibrarySegmentBookmarkedLocation.class]) {
        VLCLibrarySegmentBookmarkedLocation * const descriptor =
            (VLCLibrarySegmentBookmarkedLocation *)self.representedObject;
        _displayString = descriptor.name;
    } else if ([self.representedObject isKindOfClass:VLCMediaLibraryGroup.class]) {
        VLCMediaLibraryGroup * const group = (VLCMediaLibraryGroup *)self.representedObject;
        _displayString = group.displayString;
    } else {
        _displayString = [self displayStringForType:_segmentType];
    }
    _displayImage = [self iconForType:_segmentType];
}

@end
