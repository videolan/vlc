/*****************************************************************************
 * VLCLibraryAbstractMediaLibrarySegmentViewController.m: MacOS X interface module
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

#import "VLCLibraryAbstractMediaLibrarySegmentViewController.h"

#import "library/VLCLibraryDataSource.h"

@implementation VLCLibraryAbstractMediaLibrarySegmentViewController

- (id<VLCLibraryDataSource>)currentDataSource
{
    return nil;
}

// A note on the connected property.
// This does not necessarily reflect the connection state of the data sources themselves.
// We may disconnect the data sources via the view controllers when we detect that long loads are
// taking place. However, keeping the connection state in the view controller allows us to
// reconnect the data sources once the long loads are over (or not reconnect them, if the view
// controller was originally disconnected, for example if the embedded video view is open)

- (void)connect
{
    [self.currentDataSource connect];
    _connected = YES;
}

- (void)disconnect
{
    [self.currentDataSource disconnect];
    _connected = NO;
}

@end
