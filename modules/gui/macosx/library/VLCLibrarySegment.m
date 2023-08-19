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

- (NSString *)displayStringForType:(VLCLibrarySegmentType)segmentType
{
    switch (segmentType) {
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

- (void)updateSegmentTypeRepresentation
{
    _displayString = [self displayStringForType:_segmentType];
}

@end
