/*****************************************************************************
 * VLCLibraryAlbumTableself.m: MacOS X interface module
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

#import "VLCLibraryAlbumTableCellView.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"
#import "main/VLCMain.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"

NSString *VLCAudioLibraryCellIdentifier = @"VLCAudioLibraryCellIdentifier";
const CGFloat VLCLibraryTracksRowHeight = 50.;

@interface VLCLibraryTracksDataSource : NSObject <NSTableViewDataSource, NSTableViewDelegate>

@property (readwrite, retain, nonatomic, nullable) VLCMediaLibraryAlbum *representedAlbum;

@end

@interface VLCLibraryAlbumTableCellView ()
{
    VLCLibraryController *_libraryController;
    VLCLibraryTracksDataSource *_tracksDataSource;
}
@end

@implementation VLCLibraryAlbumTableCellView

- (void)awakeFromNib
{
    self.albumNameTextField.font = [NSFont VLClibraryCellTitleFont];
    self.yearTextField.font = [NSFont VLClibraryCellTitleFont];
    self.summaryTextField.font = [NSFont VLClibraryCellSubtitleFont];
    self.trackingView.viewToHide = self.playInstantlyButton;
    [self prepareForReuse];
}

- (void)prepareForReuse
{
    [super prepareForReuse];
    self.representedImageView.image = nil;
    self.albumNameTextField.stringValue = @"";
    self.yearTextField.stringValue = @"";
    self.summaryTextField.stringValue = @"";
    self.playInstantlyButton.hidden = YES;
}

- (IBAction)playInstantly:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    NSArray *tracks = [_representedAlbum tracksAsMediaItems];
    NSUInteger trackCount = tracks.count;
    BOOL playImmediately = YES;
    for (NSUInteger x = 0; x < trackCount; x++) {
        [_libraryController appendItemToPlaylist:tracks[x] playImmediately:playImmediately];
        if (playImmediately) {
            playImmediately = NO;
        }
    }
}

- (void)setRepresentedAlbum:(VLCMediaLibraryAlbum *)representedAlbum
{
    _representedAlbum = representedAlbum;
    self.albumNameTextField.stringValue = _representedAlbum.title;
    if (_representedAlbum.year > 0) {
        self.yearTextField.intValue = _representedAlbum.year;
    }
    self.summaryTextField.stringValue = _representedAlbum.summary;

    NSImage *image;
    if (_representedAlbum.artworkMRL.length > 0) {
        image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:_representedAlbum.artworkMRL]];
    }
    if (!image) {
        image = [NSImage imageNamed: @"noart.png"];
    }
    self.representedImageView.image = image;

    self.tracksTableView.rowHeight = VLCLibraryTracksRowHeight;
    _tracksDataSource = [[VLCLibraryTracksDataSource alloc] init];
    _tracksDataSource.representedAlbum = _representedAlbum;
    self.tracksTableView.dataSource = _tracksDataSource;
    self.tracksTableView.delegate = _tracksDataSource;
}

@end

@interface VLCLibraryTracksDataSource ()
{
    NSArray *_tracks;
}
@end

@implementation VLCLibraryTracksDataSource

- (void)setRepresentedAlbum:(VLCMediaLibraryAlbum *)representedAlbum
{
    _representedAlbum = representedAlbum;
    _tracks = [_representedAlbum tracksAsMediaItems];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_representedAlbum != nil) {
        return _representedAlbum.numberOfTracks;
    }

    return 0;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibraryTableCellView *cellView = [tableView makeViewWithIdentifier:VLCAudioLibraryCellIdentifier owner:self];

    if (cellView == nil) {
        /* the following code saves us an instance of NSViewController which we don't need */
        NSNib *nib = [[NSNib alloc] initWithNibNamed:@"VLCLibraryTableCellView" bundle:nil];
        NSArray *topLevelObjects;
        if (![nib instantiateWithOwner:self topLevelObjects:&topLevelObjects]) {
            NSAssert(1, @"Failed to load nib file to show audio library items");
            return nil;
        }

        for (id topLevelObject in topLevelObjects) {
            if ([topLevelObject isKindOfClass:[VLCLibraryTableCellView class]]) {
                cellView = topLevelObject;
                break;
            }
        }
        cellView.identifier = VLCAudioLibraryCellIdentifier;
    }

    VLCMediaLibraryMediaItem *mediaItem = _tracks[row];

    NSImage *image;
    if (mediaItem.artworkGenerated) {
        if (mediaItem.artworkMRL.length > 0) {
            image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:mediaItem.artworkMRL]];
        }
    }
    if (!image) {
        image = [NSImage imageNamed: @"noart.png"];
    }
    cellView.representedImageView.image = image;
    cellView.representedMediaItem = mediaItem;

    NSString *title = mediaItem.title;
    cellView.primaryTitleTextField.hidden = NO;
    cellView.secondaryTitleTextField.hidden = NO;
    cellView.primaryTitleTextField.stringValue = title;
    cellView.secondaryTitleTextField.stringValue = [NSString stringWithTime:mediaItem.duration / 1000];

    return cellView;
}

@end
