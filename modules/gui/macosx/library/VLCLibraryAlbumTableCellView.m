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

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"
#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"
#import "main/VLCMain.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryAlbumTracksDataSource.h"

NSString *VLCAudioLibraryCellIdentifier = @"VLCAudioLibraryCellIdentifier";
NSString *VLCLibraryAlbumTableCellTableViewColumnIdentifier = @"VLCLibraryAlbumTableCellTableViewColumnIdentifier";
const CGFloat VLCLibraryAlbumTableCellViewDefaultHeight = 168.;

// Note that these values are not necessarily linked to the layout defined in the .xib files.
// If the spacing in the layout is changed you will want to change these values too.
const CGFloat VLCLibraryAlbumTableCellViewLargeSpacing = 20;
const CGFloat VLCLibraryAlbumTableCellViewMediumSpacing = 10;
const CGFloat VLCLibraryAlbumTableCellViewSmallSpacing = 5;

@interface VLCLibraryAlbumTableCellView ()
{
    VLCLibraryController *_libraryController;
    VLCLibraryAlbumTracksDataSource *_tracksDataSource;
    VLCLibraryTableView *_tracksTableView;
    NSTableColumn *_column;
}
@end

@implementation VLCLibraryAlbumTableCellView

+ (instancetype)fromNibWithOwner:(id)owner
{
    return (VLCLibraryAlbumTableCellView*)[NSView fromNibNamed:@"VLCLibraryAlbumTableCellView"
                                                     withClass:[VLCLibraryAlbumTableCellView class]
                                                     withOwner:owner];
}

+ (CGFloat)defaultHeight
{
    return VLCLibraryAlbumTableCellViewDefaultHeight;
}

- (CGFloat)height
{
    if (_representedAlbum == nil) {
        return -1;
    }

    const CGFloat artworkAndSecondaryLabelsHeight = VLCLibraryAlbumTableCellViewLargeSpacing + 
                                                    _representedImageView.frame.size.height + 
                                                    VLCLibraryAlbumTableCellViewMediumSpacing + 
                                                    _summaryTextField.frame.size.height + 
                                                    VLCLibraryAlbumTableCellViewSmallSpacing +
                                                    _yearTextField.frame.size.height + 
                                                    VLCLibraryAlbumTableCellViewLargeSpacing;
    
    if(_tracksTableView == nil) {
        return artworkAndSecondaryLabelsHeight;
    }

    const CGFloat titleAndTableViewHeight = VLCLibraryAlbumTableCellViewLargeSpacing +
                                            _albumNameTextField.frame.size.height +
                                            VLCLibraryAlbumTableCellViewSmallSpacing +
                                            _artistNameTextField.frame.size.height + 
                                            VLCLibraryAlbumTableCellViewSmallSpacing +
                                            [self expectedTableViewHeight] +
                                            VLCLibraryAlbumTableCellViewLargeSpacing;

    return titleAndTableViewHeight > artworkAndSecondaryLabelsHeight ? titleAndTableViewHeight : artworkAndSecondaryLabelsHeight;
}

- (CGFloat)expectedTableViewWidth
{
    // We are positioning the table view to the right of the album art, which means we need
    // to take into account the album's left spacing, right spacing, and the table view's
    // right spacing. In this case we are using large spacing for all of these. We also
    // throw in a little bit extra spacing to compensate for some mysterious internal spacing.
    return self.frame.size.width - _representedImageView.frame.size.width - VLCLibraryAlbumTableCellViewLargeSpacing * 3.75;
}

- (CGFloat)expectedTableViewHeight
{
    const NSUInteger numberOfTracks = _representedAlbum.numberOfTracks;
    const CGFloat intercellSpacing = numberOfTracks > 1 ? (numberOfTracks - 1) * _tracksTableView.intercellSpacing.height : 0;
    return numberOfTracks * VLCLibraryTracksRowHeight + intercellSpacing + VLCLibraryAlbumTableCellViewMediumSpacing;
}

- (void)awakeFromNib
{
    [self setupTracksTableView];
    self.albumNameTextField.font = [NSFont VLClibraryLargeCellTitleFont];
    self.artistNameTextField.font = [NSFont VLClibraryLargeCellSubtitleFont];
    self.yearTextField.font = [NSFont VLClibrarySmallCellTitleFont];
    self.summaryTextField.font = [NSFont VLClibrarySmallCellTitleFont];
    self.trackingView.viewToHide = self.playInstantlyButton;
    self.artistNameTextField.textColor = [NSColor VLCAccentColor];

    [self prepareForReuse];
}

- (void)setupTracksTableView
{
    _tracksTableView = [[VLCLibraryTableView alloc] initWithFrame:NSZeroRect];
    _column = [[NSTableColumn alloc] initWithIdentifier:VLCLibraryAlbumTableCellTableViewColumnIdentifier];
    _column.width = [self expectedTableViewWidth];
    _column.maxWidth = MAXFLOAT;
    [_tracksTableView addTableColumn:_column];

    if(@available(macOS 11.0, *)) {
        _tracksTableView.style = NSTableViewStyleFullWidth;
    }
    _tracksTableView.gridStyleMask = NSTableViewSolidHorizontalGridLineMask;
    _tracksTableView.rowHeight = VLCLibraryTracksRowHeight;
    _tracksTableView.backgroundColor = [NSColor clearColor];

    _tracksDataSource = [[VLCLibraryAlbumTracksDataSource alloc] init];
    _tracksTableView.dataSource = _tracksDataSource;
    _tracksTableView.delegate = _tracksDataSource;
    _tracksTableView.doubleAction = @selector(tracksTableViewDoubleClickAction:);
    _tracksTableView.target = self;

    _tracksTableView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_tracksTableView];
    NSString *horizontalVisualConstraints = [NSString stringWithFormat:@"H:|-%f-[_representedImageView]-%f-[_tracksTableView]-%f-|",
        VLCLibraryAlbumTableCellViewLargeSpacing,
        VLCLibraryAlbumTableCellViewLargeSpacing,
        VLCLibraryAlbumTableCellViewLargeSpacing];
    NSString *verticalVisualContraints = [NSString stringWithFormat:@"V:|-%f-[_albumNameTextField]-%f-[_artistNameTextField]-%f-[_tracksTableView]->=%f-|",
        VLCLibraryAlbumTableCellViewLargeSpacing,
        VLCLibraryAlbumTableCellViewSmallSpacing,
        VLCLibraryAlbumTableCellViewMediumSpacing,
        VLCLibraryAlbumTableCellViewLargeSpacing];
    NSDictionary *dict = NSDictionaryOfVariableBindings(_tracksTableView, _representedImageView, _albumNameTextField, _artistNameTextField);
    [self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:horizontalVisualConstraints options:0 metrics:0 views:dict]];
    [self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:verticalVisualContraints options:0 metrics:0 views:dict]];
}

- (void)prepareForReuse
{
    [super prepareForReuse];

    self.representedImageView.image = nil;
    self.albumNameTextField.stringValue = @"";
    self.artistNameTextField.stringValue = @"";
    self.yearTextField.stringValue = @"";
    self.summaryTextField.stringValue = @"";
    self.yearTextField.hidden = NO;
    self.playInstantlyButton.hidden = YES;

    _tracksDataSource.representedAlbum = nil;
    [_tracksTableView reloadData];
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];

    // As it expects a scrollview as a parent, the table view will always resize itself and
    // we cannot directly set its size. However, it resizes itself according to its columns
    // and rows. We can therefore implicitly set its width by resizing the single column we
    // are using.
    //
    // Since a column is just an NSObject and not an actual NSView object, however, we cannot
    // use the normal autosizing/constraint systems and must instead calculate and set its
    // size manually.
    _column.width = [self expectedTableViewWidth];
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
    self.artistNameTextField.stringValue = _representedAlbum.artistName;
    
    if (_representedAlbum.year > 0) {
        self.yearTextField.intValue = _representedAlbum.year;
    } else {
        self.yearTextField.hidden = YES;
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
