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
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryItemInternalMediaItemsDataSource.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"

#import "library/audio-library/VLCLibraryAlbumTracksTableViewDelegate.h"

NSString * const VLCAudioLibraryCellIdentifier = @"VLCAudioLibraryCellIdentifier";
NSString * const VLCLibraryAlbumTableCellTableViewIdentifier = @"VLCLibraryAlbumTableCellTableViewIdentifier";
NSString * const VLCLibraryAlbumTableCellTableViewColumnIdentifier = @"VLCLibraryAlbumTableCellTableViewColumnIdentifier";

const CGFloat VLCLibraryAlbumTableCellViewDefaultHeight = 168.;

@interface VLCNonScrollableScrollView : NSScrollView
@end

@implementation VLCNonScrollableScrollView

- (void)scrollWheel:(NSEvent *)event
{
    // Pass the scroll event to the next responder instead of handling it
    [self.nextResponder scrollWheel:event];
}

@end

@interface VLCLibraryAlbumTableCellView ()
{
    VLCLibraryController *_libraryController;
    VLCLibraryItemInternalMediaItemsDataSource *_tracksDataSource;
    VLCLibraryAlbumTracksTableViewDelegate *_tracksTableViewDelegate;
    VLCLibraryTableView *_tracksTableView;
    NSScrollView *_tracksScrollView;
    NSTableColumn *_column;
}

@property (readwrite, atomic) VLCLibraryRepresentedItem *protectedRepresentedItem;

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
    if (self.representedItem == nil) {
        return -1;
    }

    const CGFloat artworkAndSecondaryLabelsHeight = VLCLibraryUIUnits.largeSpacing +
                                                    _representedImageView.frame.size.height +
                                                    VLCLibraryUIUnits.mediumSpacing +
                                                    _summaryTextField.frame.size.height +
                                                    VLCLibraryUIUnits.smallSpacing +
                                                    _yearTextField.frame.size.height +
                                                    VLCLibraryUIUnits.largeSpacing;

    if(_tracksTableView == nil) {
        return artworkAndSecondaryLabelsHeight;
    }

    const CGFloat titleAndTableViewHeight = VLCLibraryUIUnits.largeSpacing +
                                            _albumNameTextField.frame.size.height +
                                            VLCLibraryUIUnits.smallSpacing +
                                            _artistNameTextButton.frame.size.height +
                                            VLCLibraryUIUnits.smallSpacing +
                                            [self expectedTableViewHeight] +
                                            VLCLibraryUIUnits.largeSpacing;

    return titleAndTableViewHeight > artworkAndSecondaryLabelsHeight ? titleAndTableViewHeight : artworkAndSecondaryLabelsHeight;
}

- (CGFloat)expectedTableViewWidth
{
    // We are positioning the table view to the right of the album art, which means we need
    // to take into account the album's left spacing, right spacing, and the table view's
    // right spacing. In this case we are using large spacing for all of these. We also
    // throw in a little bit extra spacing to compensate for some mysterious internal spacing.
    return self.frame.size.width -
           _representedImageView.frame.size.width -
           VLCLibraryUIUnits.largeSpacing * 3.75;
}

- (CGFloat)expectedTableViewHeight
{
    if (self.representedItem == nil) {
        return -1;
    }

    VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)self.representedItem.item;
    if (album == nil) {
        return -1;
    }

    const NSUInteger numberOfTracks = album.numberOfTracks;
    const CGFloat intercellSpacing = numberOfTracks > 1 ? (numberOfTracks - 1) * _tracksTableView.intercellSpacing.height : 0;
    return numberOfTracks * VLCLibraryInternalMediaItemRowHeight + intercellSpacing + VLCLibraryUIUnits.mediumSpacing;
}

- (void)awakeFromNib
{
    [self setupTracksTableView];
    self.albumNameTextField.font = NSFont.VLCLibrarySubsectionHeaderFont;
    self.artistNameTextButton.font = NSFont.VLCLibrarySubsectionSubheaderFont;
    self.genreNameTextButton.font = NSFont.VLCLibrarySubsectionSubheaderFont;
    self.artistNameTextButton.action = @selector(primaryDetailAction:);
    self.genreNameTextButton.action = @selector(secondaryDetailAction:);
    self.trackingView.viewToHide = self.playInstantlyButton;

    if (@available(macOS 26.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        self.playInstantlyButton.bordered = YES;
        self.playInstantlyButton.bezelStyle = NSBezelStyleGlass;
        self.playInstantlyButton.borderShape = NSControlBorderShapeCircle;
        self.playInstantlyButton.image = [NSImage imageWithSystemSymbolName:@"play.fill" accessibilityDescription:nil];
        self.playInstantlyButton.imageScaling = NSImageScaleProportionallyUpOrDown;
        self.playInstantlyButton.controlSize = NSControlSizeExtraLarge;
        [NSLayoutConstraint activateConstraints:@[
            [self.playInstantlyButton.widthAnchor constraintEqualToConstant:VLCLibraryUIUnits.mediumPlaybackControlButtonSize],
            [self.playInstantlyButton.heightAnchor constraintEqualToConstant:VLCLibraryUIUnits.mediumPlaybackControlButtonSize],
        ]];
#endif
    }

    if (@available(macOS 10.14, *)) {
        self.artistNameTextButton.contentTintColor = NSColor.VLCAccentColor;
        self.genreNameTextButton.contentTintColor = NSColor.secondaryLabelColor;
    }

    [self prepareForReuse];

    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(handleAlbumUpdated:)
                               name:VLCLibraryModelAlbumUpdated
                             object:nil];
}

- (void)setupTracksTableView
{
    // Create scroll view container that doesn't intercept scroll events
    _tracksScrollView = [[VLCNonScrollableScrollView alloc] initWithFrame:NSZeroRect];
    _tracksScrollView.borderType = NSNoBorder;
    _tracksScrollView.hasVerticalScroller = NO;
    _tracksScrollView.hasHorizontalScroller = NO;
    _tracksScrollView.drawsBackground = NO;
    _tracksScrollView.verticalScrollElasticity = NSScrollElasticityNone;
    _tracksScrollView.horizontalScrollElasticity = NSScrollElasticityNone;
    
    _tracksTableView = [[VLCLibraryTableView alloc] initWithFrame:NSZeroRect];
    _tracksTableView.identifier = VLCLibraryAlbumTableCellTableViewIdentifier;
    _tracksTableView.allowsMultipleSelection = YES;
    _tracksTableView.headerView = nil;
    _column = [[NSTableColumn alloc] initWithIdentifier:VLCLibraryAlbumTableCellTableViewColumnIdentifier];
    _column.width = [self expectedTableViewWidth];
    _column.maxWidth = MAXFLOAT;
    [_tracksTableView addTableColumn:_column];

    if(@available(macOS 11.0, *)) {
        _tracksTableView.style = NSTableViewStyleFullWidth;
    }
    _tracksTableView.gridStyleMask = NSTableViewSolidHorizontalGridLineMask;
    _tracksTableView.rowHeight = VLCLibraryInternalMediaItemRowHeight;
    _tracksTableView.backgroundColor = [NSColor clearColor];

    _tracksDataSource = [[VLCLibraryItemInternalMediaItemsDataSource alloc] init];
    _tracksTableViewDelegate = [[VLCLibraryAlbumTracksTableViewDelegate alloc] init];
    _tracksTableView.dataSource = _tracksDataSource;
    _tracksTableView.delegate = _tracksTableViewDelegate;
    _tracksTableView.doubleAction = @selector(tracksTableViewDoubleClickAction:);
    _tracksTableView.target = self;

    _tracksScrollView.documentView = _tracksTableView;
    _tracksScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_tracksScrollView];
    NSString *horizontalVisualConstraints = [NSString stringWithFormat:@"H:|-%f-[_representedImageView]-%f-[_tracksScrollView]-%f-|",
                                             VLCLibraryUIUnits.largeSpacing,
                                             VLCLibraryUIUnits.largeSpacing,
                                             VLCLibraryUIUnits.largeSpacing];
    NSString *verticalVisualContraints = [NSString stringWithFormat:@"V:|-%f-[_albumNameTextField]-%f-[_artistNameTextButton]-%f-[_tracksScrollView]->=%f-|",
                                          VLCLibraryUIUnits.largeSpacing,
                                          VLCLibraryUIUnits.smallSpacing,
                                          VLCLibraryUIUnits.mediumSpacing,
                                          VLCLibraryUIUnits.largeSpacing];
    NSDictionary *dict = NSDictionaryOfVariableBindings(_tracksScrollView, _representedImageView, _albumNameTextField, _artistNameTextButton);
    [self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:horizontalVisualConstraints options:0 metrics:0 views:dict]];
    [self addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:verticalVisualContraints options:0 metrics:0 views:dict]];

    NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(handleTableViewSelectionIsChanging:)
                               name:NSTableViewSelectionIsChangingNotification
                             object:nil];
}

- (void)prepareForReuse
{
    [super prepareForReuse];

    self.representedImageView.image = nil;
    self.albumNameTextField.stringValue = @"";
    self.artistNameTextButton.title = @"";
    self.genreNameTextButton.title = @"";
    self.yearTextField.stringValue = @"";
    self.summaryTextField.stringValue = @"";
    self.yearTextField.hidden = NO;
    self.playInstantlyButton.hidden = YES;

    if (@available(macOS 10.14, *)) {
        self.artistNameTextButton.contentTintColor = NSColor.VLCAccentColor;
        self.genreNameTextButton.contentTintColor = NSColor.secondaryLabelColor;
    }

    _tracksDataSource.representedItem = nil;
    [_tracksTableView reloadData];
}

- (void)handleAlbumUpdated:(NSNotification *)notification
{
    NSParameterAssert(notification);
    if (self.representedItem == nil) {
        return;
    }

    VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)notification.object;
    if (album == nil || self.representedItem.item.libraryID != album.libraryID) {
        return;
    }

    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:album parentType:self.representedItem.parentType];
    self.representedItem = representedItem;
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
    [self.representedItem play];
}

- (void)primaryDetailAction:(id)sender
{
    VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)self.representedItem.item;
    if (album == nil || !album.primaryActionableDetail) {
        return;
    }

    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    const id<VLCMediaLibraryItemProtocol> libraryItem = album.primaryActionableDetailLibraryItem;
    [libraryWindow presentLibraryItem:libraryItem];
}

- (void)secondaryDetailAction:(id)sender
{
    VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)self.representedItem.item;
    if (album == nil || !album.secondaryActionableDetail) {
        return;
    }

    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    id<VLCMediaLibraryItemProtocol> libraryItem = album.secondaryActionableDetailLibraryItem;
    [libraryWindow presentLibraryItem:libraryItem];
}

- (VLCLibraryRepresentedItem *)representedItem
{
    return self.protectedRepresentedItem;
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    if (representedItem == nil) {
        return;
    }

    self.protectedRepresentedItem = representedItem;

    VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)self.representedItem.item;
    NSAssert(album != nil, @"Represented item should be a medialibraryalbum!");

    self.albumNameTextField.stringValue = album.displayString;
    self.artistNameTextButton.title = album.artistName;
    self.genreNameTextButton.title = album.genreString;

    if (album.year > 0) {
        self.yearTextField.intValue = album.year;
    } else {
        self.yearTextField.hidden = YES;
    }

    if (album.summary.length > 0) {
        self.summaryTextField.stringValue = album.summary;
    } else {
        self.summaryTextField.stringValue = album.durationString;
    }

    const BOOL primaryActionableDetail = album.primaryActionableDetail;
    const BOOL secondaryActionableDetail = album.secondaryActionableDetail;
    self.artistNameTextButton.enabled = primaryActionableDetail;
    self.genreNameTextButton.enabled = secondaryActionableDetail;
    if (@available(macOS 10.14, *)) {
        self.artistNameTextButton.contentTintColor = primaryActionableDetail ? NSColor.VLCAccentColor : NSColor.secondaryLabelColor;
        self.genreNameTextButton.contentTintColor = secondaryActionableDetail ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor;
    }

    __weak typeof(self) weakSelf = self;
    [VLCLibraryImageCache thumbnailForLibraryItem:album withCompletion:^(NSImage * const thumbnail) {
        if (!weakSelf || weakSelf.representedItem.item != album) {
            return;
        }
        weakSelf.representedImageView.image = thumbnail;
    }];

    [_tracksDataSource setRepresentedItem:album withCompletion:^{
        __strong typeof(self) strongSelf = weakSelf;

        if (strongSelf) {
            [strongSelf->_tracksTableView reloadData];
        }
    }];
}

- (void)tracksTableViewDoubleClickAction:(id)sender
{
    if (!_libraryController) {
        _libraryController = VLCMain.sharedInstance.libraryController;
    }

    VLCMediaLibraryAlbum * const album = (VLCMediaLibraryAlbum *)self.representedItem.item;
    NSArray * const tracks = album.mediaItems;
    const NSUInteger trackCount = tracks.count;
    const NSInteger clickedRow = _tracksTableView.clickedRow;
    if (clickedRow >= 0 && (NSUInteger)clickedRow < trackCount) {
        VLCMediaLibraryMediaItem * const mediaItem = tracks[clickedRow];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:mediaItem parentType:VLCMediaLibraryParentGroupTypeAlbum];

        [representedItem play];
    }
}

- (void)handleTableViewSelectionIsChanging:(NSNotification *)notification
{
    NSParameterAssert(notification);
    NSTableView * const tableView = notification.object;
    NSAssert(tableView, @"Table view selection changing notification should carry valid table view");

    if (tableView != _tracksTableView &&
        tableView.identifier == VLCLibraryAlbumTableCellTableViewIdentifier) {

        [_tracksTableView deselectAll:self];
    }
}

@end
