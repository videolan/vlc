/*****************************************************************************
 * VLCLibraryWindow.m: MacOS X interface module
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

#import "VLCLibraryWindow.h"
#import "NSString+Helpers.h"
#import "VLCPlaylistTableCellView.h"

static const float f_min_window_width = 604.;
static const float f_min_window_height = 307.;
static const float f_playlist_row_height = 40.;

static NSString *VLCPlaylistCellIdentifier = @"VLCPlaylistCellIdentifier";

@interface VLCLibraryWindow ()
{
    VLCPlaylistDataSource *_playlistDataSource;
}
@end

@implementation VLCLibraryWindow

- (void)awakeFromNib
{
    _segmentedTitleControl.segmentCount = 3;
    [_segmentedTitleControl setTarget:self];
    [_segmentedTitleControl setAction:@selector(segmentedControlAction)];
    [_segmentedTitleControl setLabel:_NS("Music") forSegment:0];
    [_segmentedTitleControl setLabel:_NS("Video") forSegment:1];
    [_segmentedTitleControl setLabel:_NS("Network") forSegment:2];
    [_segmentedTitleControl sizeToFit];

    _playlistDataSource = [[VLCPlaylistDataSource alloc] init];

    _playlistTableView.dataSource = _playlistDataSource;
    _playlistTableView.delegate = _playlistDataSource;
    _playlistTableView.rowHeight = f_playlist_row_height;
    [_playlistTableView reloadData];
}

- (void)segmentedControlAction
{
}

@end

@implementation VLCPlaylistDataSource

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return 2;
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

    cellView.mediaTitleTextField.stringValue = @"Custom Cell Label Text";
    cellView.durationTextField.stringValue = @"00:00";
    cellView.mediaImageView.image = [NSImage imageNamed: @"noart.png"];
    return cellView;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSLog(@"playlist selection changed: %li", (long)[(NSTableView *)notification.object selectedRow]);
}

@end

@implementation VLCLibraryWindowController

- (instancetype)initWithLibraryWindow
{
    self = [super initWithWindowNibName:@"VLCLibraryWindow"];
    return self;
}

- (void)windowDidLoad
{
    VLCLibraryWindow *window = (VLCLibraryWindow *)self.window;
    [window setRestorable:NO];
    [window setExcludedFromWindowsMenu:YES];
    [window setAcceptsMouseMovedEvents:YES];
    [window setContentMinSize:NSMakeSize(f_min_window_width, f_min_window_height)];
}

@end
