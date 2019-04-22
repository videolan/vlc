/*****************************************************************************
 * VLCMediaSourceDataSource.m: MacOS X interface module
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

#import "VLCMediaSourceDataSource.h"

#import "media-source/VLCMediaSourceProvider.h"
#import "media-source/VLCMediaSource.h"
#import "playlist/VLCPlaylistTableCellView.h"

#import "main/VLCMain.h"
#import "library/VLCInputItem.h"
#import "extensions/NSString+Helpers.h"

static NSString *VLCMediaSourceCellIdentifier = @"VLCMediaSourceCellIdentifier";

@interface VLCMediaSourceDataSource ()
{
    NSArray *_mediaDiscovery;
}
@end

@implementation VLCMediaSourceDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self lazyLoadMediaSources];
        });
    }
    return self;
}

- (void)lazyLoadMediaSources
{
    NSMutableArray *mutableArray = [[NSMutableArray alloc] init];
    NSArray *mediaDiscoveryForDevices = [VLCMediaSourceProvider listOfMediaSourcesForCategory:SD_CAT_DEVICES];
    if (mediaDiscoveryForDevices.count > 0) {
        [mutableArray addObject:_NS("Devices")];
        [mutableArray addObjectsFromArray:mediaDiscoveryForDevices];
    }

    NSArray *mediaDiscoveryForLAN = [VLCMediaSourceProvider listOfMediaSourcesForCategory:SD_CAT_LAN];
    if (mediaDiscoveryForLAN.count > 0) {
        [mutableArray addObject:_NS("Local Network")];
        [mutableArray addObjectsFromArray:mediaDiscoveryForLAN];
    }

    NSArray *mediaDiscoveryForInternet = [VLCMediaSourceProvider listOfMediaSourcesForCategory:SD_CAT_INTERNET];
    if (mediaDiscoveryForInternet.count > 0) {
        [mutableArray addObject:_NS("Internet")];
        [mutableArray addObjectsFromArray:mediaDiscoveryForInternet];
    }

    NSArray *mediaDiscoveryForMyComputer = [VLCMediaSourceProvider listOfMediaSourcesForCategory:SD_CAT_MYCOMPUTER];
    if (mediaDiscoveryForMyComputer.count > 0) {
        [mutableArray addObject:_NS("My Computer")];
        [mutableArray addObjectsFromArray:mediaDiscoveryForMyComputer];
    }

    _mediaDiscovery = [mutableArray copy];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _mediaDiscovery.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCPlaylistTableCellView *cellView = [tableView makeViewWithIdentifier:VLCMediaSourceCellIdentifier owner:self];

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
        cellView.identifier = VLCMediaSourceCellIdentifier;
    }

    if ([self tableView:tableView isGroupRow:row]) {
        NSString *labelString = _mediaDiscovery[row];
        cellView.mediaTitleTextField.hidden = NO;
        cellView.secondaryMediaTitleTextField.hidden = YES;
        cellView.artistTextField.hidden = YES;
        cellView.mediaTitleTextField.stringValue = labelString;
        cellView.durationTextField.stringValue = @"";
    } else {
        VLCMediaSource *mediaSource = _mediaDiscovery[row];

        VLCInputItem *inputItem = mediaSource.rootNode.inputItem;
        if (inputItem) {
            cellView.mediaTitleTextField.hidden = YES;
            cellView.secondaryMediaTitleTextField.hidden = NO;
            cellView.artistTextField.hidden = NO;
            cellView.secondaryMediaTitleTextField.stringValue = mediaSource.mediaSourceDescription;
            cellView.artistTextField.stringValue = inputItem.name;
            cellView.durationTextField.stringValue = [NSString stringWithTimeFromTicks:inputItem.duration];
        } else {
            cellView.mediaTitleTextField.hidden = NO;
            cellView.secondaryMediaTitleTextField.hidden = YES;
            cellView.artistTextField.hidden = YES;
            cellView.mediaTitleTextField.stringValue = mediaSource.mediaSourceDescription;
            cellView.durationTextField.stringValue = @"";
        }
    }

    return cellView;
}

- (BOOL)tableView:(NSTableView *)tableView isGroupRow:(NSInteger)row
{
    return [_mediaDiscovery[row] isKindOfClass:[NSString class]];
}

@end
