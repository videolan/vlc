/*****************************************************************************
 * VLCLibrarySection.h: MacOS X interface module
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

@implementation VLCLibrarySegment

+ (NSArray<VLCLibrarySegment *> *)librarySegments
{
    return @[
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryHomeSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryVideoSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryMusicSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryBrowseSegment],
        [VLCLibrarySegment segmentWithSegmentType:VLCLibraryStreamsSegment],
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
    NSNumber * const segmentNumber = (NSNumber *)modelObject;
    const NSInteger segmentValue = segmentNumber.integerValue;
    NSAssert(segmentNumber != nil &&
             segmentValue > VLCLibraryLowSentinelSegment &&
             segmentValue < VLCLibraryHighSentinelSegment,
             @"VLCLibrarySegment represented object must be a library segment type value!");

    self = [super initWithRepresentedObject:modelObject];
    if (self) {
        _segmentType = segmentValue;
        [self updateSegmentTypeRepresentation];
    }
    return self;
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
        case VLCLibraryVideoSegment:
            return _NS("Videos");
        case VLCLibraryBrowseSegment:
            return _NS("Browse");
        case VLCLibraryStreamsSegment:
            return _NS("Streams");
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
            return [NSImage imageNamed:@"sidebar-music"];
        case VLCLibraryVideoSegment:
            return [NSImage imageNamed:@"sidebar-movie"];
        case VLCLibraryBrowseSegment:
            return [NSImage imageNamed:@"NSFolder"];
        case VLCLibraryStreamsSegment:
            return [NSImage imageNamed:@"NSActionTemplate"];
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
        case VLCLibraryVideoSegment:
            return [NSImage imageWithSystemSymbolName:@"film.stack"
                             accessibilityDescription:@"Video icon"];
        case VLCLibraryBrowseSegment:
            return [NSImage imageWithSystemSymbolName:@"folder"
                             accessibilityDescription:@"Browse icon"];
        case VLCLibraryStreamsSegment:
            return [NSImage imageWithSystemSymbolName:@"antenna.radiowaves.left.and.right"
                             accessibilityDescription:@"Streams icon"];
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
    _displayString = [self displayStringForType:_segmentType];
    _displayImage = [self iconForType:_segmentType];
}

@end
