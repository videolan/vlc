/*****************************************************************************
 * VLCLibraryAlbumTableCellView.m: MacOS X interface module
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
#import "library/VLCLibraryAlbumTracksDataSource.h"

NSString *VLCAudioLibraryCellIdentifier = @"VLCAudioLibraryCellIdentifier";
const CGFloat VLCLibraryTracksRowHeight = 50.;
const CGFloat VLCLibraryAlbumTableCellViewDefaultHeight = 168.;
const CGFloat LayoutSpacer;

@interface VLCLibraryAlbumTableCellView ()
{
    VLCLibraryController *_libraryController;
    VLCLibraryAlbumTracksDataSource *_tracksDataSource;
    NSTableView *_tracksTableView;
}
@end

@implementation VLCLibraryAlbumTableCellView

+ (CGFloat)defaultHeight
{
    return VLCLibraryAlbumTableCellViewDefaultHeight;
}

+ (CGFloat)heightForAlbum:(VLCMediaLibraryAlbum *)album
{
    if (!album) {
        return [VLCLibraryAlbumTableCellView defaultHeight];
    }

    size_t numberOfTracks = album.numberOfTracks;
    return [VLCLibraryAlbumTableCellView defaultHeight] + numberOfTracks * VLCLibraryTracksRowHeight + numberOfTracks * 0.5;
}

- (void)awakeFromNib
{
    CGRect frame = self.frame;
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"theOnlyColumn"];
    column.width = frame.size.width - LayoutSpacer * 2.;
    _tracksTableView = [[NSTableView alloc] initWithFrame:CGRectMake(LayoutSpacer, 14., frame.size.width - LayoutSpacer * 2., 0.)];
    _tracksTableView.rowHeight = VLCLibraryTracksRowHeight;
    [_tracksTableView addTableColumn:column];
    _tracksTableView.translatesAutoresizingMaskIntoConstraints = NO;
    _tracksDataSource = [[VLCLibraryAlbumTracksDataSource alloc] init];
    _tracksTableView.dataSource = _tracksDataSource;
    _tracksTableView.delegate = _tracksDataSource;
    _tracksTableView.doubleAction = @selector(tracksTableViewDoubleClickAction:);
    _tracksTableView.target = self;
    [self addSubview:_tracksTableView];
    NSDictionary *dict = NSDictionaryOfVariableBindings(_tracksTableView, _representedImageView);
    [self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-20-[_tracksTableView]-20-|" options:0 metrics:0 views:dict]];
    [self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|-20-[_representedImageView]-14-[_tracksTableView]-14-|" options:0 metrics:0 views:dict]];

    self.albumNameTextField.font = [NSFont VLClibraryLargeCellTitleFont];
    self.yearTextField.font = [NSFont VLClibraryLargeCellTitleFont];
    self.summaryTextField.font = [NSFont VLClibraryLargeCellSubtitleFont];
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

    BOOL playImmediately = YES;
    for (VLCMediaLibraryMediaItem *mediaItem in [_representedAlbum tracksAsMediaItems]) {
        [_libraryController appendItemToPlaylist:mediaItem playImmediately:playImmediately];
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

    if (_representedAlbum.summary.length > 0) {
        self.summaryTextField.stringValue = _representedAlbum.summary;
    } else {
        self.summaryTextField.stringValue = _representedAlbum.durationString;
    }

    self.representedImageView.image = _representedAlbum.smallArtworkImage;

    _tracksDataSource.representedAlbum = _representedAlbum;
    [_tracksTableView reloadData];
}

- (void)tracksTableViewDoubleClickAction:(id)sender
{
    if (!_libraryController) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }

    NSArray *tracks = [_representedAlbum tracksAsMediaItems];
    NSUInteger trackCount = tracks.count;
    NSInteger clickedRow = _tracksTableView.clickedRow;
    if (clickedRow < trackCount) {
        [_libraryController appendItemToPlaylist:tracks[_tracksTableView.clickedRow] playImmediately:YES];
    }
}

@end
