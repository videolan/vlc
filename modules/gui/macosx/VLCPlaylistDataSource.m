/*****************************************************************************
 * VLCPlaylistDataSource.m: MacOS X interface module
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

#import "VLCPlaylistDataSource.h"
#import "VLCPlaylistTableCellView.h"
#import "VLCPlaylistController.h"
#import "VLCPlaylistModel.h"
#import "VLCPlaylistItem.h"
#import "NSString+Helpers.h"

static NSString *VLCPlaylistCellIdentifier = @"VLCPlaylistCellIdentifier";

@interface VLCPlaylistDataSource ()
{
    VLCPlaylistModel *_playlistModel;
}
@end

@implementation VLCPlaylistDataSource

- (void)setPlaylistController:(VLCPlaylistController *)playlistController
{
    _playlistController = playlistController;
    _playlistModel = _playlistController.playlistModel;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _playlistModel.numberOfPlaylistItems;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCPlaylistTableCellView *cellView = [tableView makeViewWithIdentifier:VLCPlaylistCellIdentifier owner:self];

    if (cellView == nil) {
        /* the following code saves us an instance of NSViewController which we don't need */
        NSNib *nib = [[NSNib alloc] initWithNibNamed:@"VLCPlaylistTableCellView" bundle:nil];
        NSArray *topLevelObjects;
        if (![nib instantiateWithOwner:self topLevelObjects:&topLevelObjects]) {
            NSLog(@"Failed to load nib %@", nib);
            return nil;
        }

        for (id topLevelObject in topLevelObjects) {
            if ([topLevelObject isKindOfClass:[VLCPlaylistTableCellView class]]) {
                cellView = topLevelObject;
                break;
            }
        }
        cellView.identifier = VLCPlaylistCellIdentifier;
    }

    VLCPlaylistItem *item = [_playlistModel playlistItemAtIndex:row];
    if (!item) {
        NSLog(@"%s: model did not return an item!", __func__);
        return cellView;
    }

    cellView.mediaTitleTextField.stringValue = item.title;
    cellView.durationTextField.stringValue = [NSString stringWithTimeFromTicks:item.duration];
    cellView.mediaImageView.image = [NSImage imageNamed: @"noart.png"];
    // TODO: show more data if available

    return cellView;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSLog(@"playlist selection changed: %li", (long)[(NSTableView *)notification.object selectedRow]);
}

- (void)playlistUpdated
{
    [_tableView reloadData];
}

@end
