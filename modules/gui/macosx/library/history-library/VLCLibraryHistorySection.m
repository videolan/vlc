/*****************************************************************************
 * VLCLibraryHistorySection.m: MacOS X interface module
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

#import "VLCLibraryHistorySection.h"

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryDataTypes.h"

@implementation VLCLibraryHistorySection

- (instancetype)initWithDate:(NSDate *)date mediaItems:(NSArray<VLCMediaLibraryMediaItem *> *)mediaItems
{
    self = [super init];
    if (self) {
        _date = date.copy;
        _mediaItems = mediaItems.copy;
    }
    return self;
}

+ (NSArray<VLCLibraryHistorySection *> *)sectionsForMediaItems:(NSArray<VLCMediaLibraryMediaItem *> *)mediaItems
                                                      calendar:(NSCalendar *)calendar
{
    NSArray<VLCMediaLibraryMediaItem *> * const sortedItems =
        [mediaItems sortedArrayWithOptions:NSSortStable
                           usingComparator:^NSComparisonResult(VLCMediaLibraryMediaItem *left,
                                                               VLCMediaLibraryMediaItem *right) {
        if (left.lastPlayedDate > right.lastPlayedDate) {
            return NSOrderedAscending;
        }
        if (left.lastPlayedDate < right.lastPlayedDate) {
            return NSOrderedDescending;
        }
        return NSOrderedSame;
    }];

    NSMutableArray<VLCLibraryHistorySection *> * const sections = NSMutableArray.array;
    NSMutableArray<VLCMediaLibraryMediaItem *> * const dayItems = NSMutableArray.array;
    NSDate *day = nil;
    for (VLCMediaLibraryMediaItem * const item in sortedItems) {
        if (item.lastPlayedDate <= 0) {
            continue;
        }
        NSDate * const playedDate = [NSDate dateWithTimeIntervalSince1970:item.lastPlayedDate];
        NSDate * const itemDay = [calendar startOfDayForDate:playedDate];
        if (day != nil && ![day isEqualToDate:itemDay]) {
            [sections addObject:[[self alloc] initWithDate:day mediaItems:dayItems]];
            [dayItems removeAllObjects];
        }
        day = itemDay;
        [dayItems addObject:item];
    }
    if (dayItems.count > 0) {
        [sections addObject:[[self alloc] initWithDate:day mediaItems:dayItems]];
    }
    return sections.copy;
}

- (NSString *)titleWithCalendar:(NSCalendar *)calendar now:(NSDate *)now
{
    if ([calendar isDate:self.date inSameDayAsDate:now]) {
        return _NS("Today");
    }
    NSDate * const yesterday = [calendar dateByAddingUnit:NSCalendarUnitDay value:-1 toDate:now options:0];
    if ([calendar isDate:self.date inSameDayAsDate:yesterday]) {
        return _NS("Yesterday");
    }

    NSDateFormatter * const formatter = NSDateFormatter.new;
    formatter.calendar = calendar;
    formatter.timeZone = calendar.timeZone;
    formatter.timeStyle = NSDateFormatterNoStyle;
    if ([calendar isDate:self.date equalToDate:now toUnitGranularity:NSCalendarUnitWeekOfYear]) {
        [formatter setLocalizedDateFormatFromTemplate:@"EEEE"];
    } else {
        formatter.dateStyle = NSDateFormatterFullStyle;
    }
    return [formatter stringFromDate:self.date];
}

@end
