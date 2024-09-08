/*****************************************************************************
 * VLCLibraryAlbumTableCellView.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

#import "library/VLCLibraryTableCellViewProtocol.h"

NS_ASSUME_NONNULL_BEGIN

@class VLCImageView;
@class VLCTrackingView;
@class VLCLibraryRepresentedItem;

@interface VLCLibraryAlbumTableCellView : NSTableCellView<VLCLibraryTableCellViewProtocol>

extern NSString * const VLCAudioLibraryCellIdentifier;
extern NSString * const VLCLibraryAlbumTableCellTableViewColumnIdentifier;

@property (class, readonly) CGFloat defaultHeight;

+ (instancetype)fromNibWithOwner:(id)owner;

@property (readwrite, weak) IBOutlet VLCTrackingView *trackingView;
@property (readwrite, weak) IBOutlet VLCImageView *representedImageView;
@property (readwrite, weak) IBOutlet NSTextField *albumNameTextField;
@property (readwrite, weak) IBOutlet NSButton *artistNameTextButton;
@property (readwrite, weak) IBOutlet NSButton *genreNameTextButton;
@property (readwrite, weak) IBOutlet NSTextField *summaryTextField;
@property (readwrite, weak) IBOutlet NSTextField *yearTextField;
@property (readwrite, weak) IBOutlet NSButton *playInstantlyButton;

@property (readonly) CGFloat height;
@property (readwrite, assign, nonatomic) VLCLibraryRepresentedItem *representedItem;

- (IBAction)playInstantly:(id)sender;
- (IBAction)primaryDetailAction:(id)sender;
- (IBAction)secondaryDetailAction:(id)sender;

@end

NS_ASSUME_NONNULL_END
