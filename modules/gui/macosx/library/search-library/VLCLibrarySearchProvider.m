/*****************************************************************************
 * VLCLibrarySearchProvider.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import "VLCLibrarySearchProvider.h"

NSString * const VLCLibrarySearchProviderResultsUpdated = @"VLCLibrarySearchProviderResultsUpdated";

@implementation VLCLibrarySearchProvider
{
    VLCLibrarySearchProviderFetchBlock _fetchBlock;
}

- (instancetype)initWithDisplayTitle:(NSString *)displayTitle
                        displayImage:(NSImage *)displayImage
                          fetchBlock:(VLCLibrarySearchProviderFetchBlock)fetchBlock
{
    self = [super init];
    if (self) {
        _displayTitle = displayTitle;
        _displayImage = displayImage;
        _results = @[];
        _fetchBlock = [fetchBlock copy];
    }
    return self;
}

- (void)searchForString:(NSString *)string
{
    if (string.length == 0) {
        [self clearSearch];
        return;
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        NSArray<id<VLCMediaLibraryItemProtocol>> * const fetchedResults =
            self->_fetchBlock(string);
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_results = fetchedResults;
            [NSNotificationCenter.defaultCenter
                postNotificationName:VLCLibrarySearchProviderResultsUpdated
                              object:self];
        });
    });
}

- (void)clearSearch
{
    _results = @[];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibrarySearchProviderResultsUpdated
                                                      object:self];
}

@end
