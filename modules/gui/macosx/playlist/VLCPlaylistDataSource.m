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

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlaylistTableCellView.h"
#import "playlist/VLCPlaylistItem.h"
#import "playlist/VLCPlaylistModel.h"
#import "views/VLCImageView.h"

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
            msg_Err(getIntf(), "Failed to load nib file to show playlist items");
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
        msg_Err(getIntf(), "playlist model did not return an item for representation");
        return cellView;
    }

    NSString *artist = item.artistName;
    if (artist && artist.length > 0) {
        cellView.mediaTitleTextField.hidden = YES;
        cellView.secondaryMediaTitleTextField.hidden = NO;
        cellView.artistTextField.hidden = NO;
        cellView.secondaryMediaTitleTextField.stringValue = item.title;
        cellView.artistTextField.stringValue = artist;
    } else {
        cellView.mediaTitleTextField.hidden = NO;
        cellView.secondaryMediaTitleTextField.hidden = YES;
        cellView.artistTextField.hidden = YES;
        cellView.mediaTitleTextField.stringValue = item.title;
    }

    cellView.durationTextField.stringValue = [NSString stringWithTimeFromTicks:item.duration];
    cellView.mediaImageView.image = item.artworkImage;
    cellView.representsCurrentPlaylistItem = _playlistController.currentPlaylistIndex == row;

    return cellView;
}

- (void)playlistUpdated
{
    [_tableView reloadData];
}

@end
