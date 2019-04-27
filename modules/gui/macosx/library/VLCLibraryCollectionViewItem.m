/*****************************************************************************
 * VLCLibraryCollectionViewItem.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCLibraryCollectionViewItem.h"

#import "main/VLCMain.h"
#import "library/VLCLibraryController.h"

NSString *VLCLibraryCellIdentifier = @"VLCLibraryCellIdentifier";

@interface VLCLibraryCollectionViewItem()
{
    VLCLibraryController *_libraryController;
}
@end

@implementation VLCLibraryCollectionViewItem

- (IBAction)playInstantly:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    NSIndexPath *indexPath = [[self collectionView] indexPathForItem:self];
    [_libraryController appendItemAtIndexPathToPlaylist:indexPath playImmediately:YES];
}

- (IBAction)addToPlaylist:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    NSIndexPath *indexPath = [[self collectionView] indexPathForItem:self];
    [_libraryController appendItemAtIndexPathToPlaylist:indexPath playImmediately:NO];
}

@end
