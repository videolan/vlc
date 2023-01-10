/*****************************************************************************
 * VLCLibraryCollectionViewItem.h: MacOS X interface module
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

NS_ASSUME_NONNULL_BEGIN

@class VLCImageView;
@class VLCLinearProgressIndicator;
@protocol VLCMediaLibraryItemProtocol;

extern NSString *VLCLibraryCellIdentifier;

@interface VLCLibraryCollectionViewItem : NSCollectionViewItem

// NOTE: These will need to be changed after changes to XIB
+ (NSSize)defaultSize;
+ (NSSize)defaultVideoItemSize;
+ (CGFloat)bottomTextViewsHeight;

@property (readwrite, assign) IBOutlet NSTextField *mediaTitleTextField;
@property (readwrite, assign) IBOutlet NSTextField *annotationTextField;
@property (readwrite, assign) IBOutlet NSTextField *unplayedIndicatorTextField;
@property (readwrite, assign) IBOutlet NSTextField *secondaryInfoTextField;
@property (readwrite, assign) IBOutlet VLCImageView *mediaImageView;
@property (readwrite, assign) IBOutlet NSButton *playInstantlyButton;
@property (readwrite, assign) IBOutlet NSButton *addToPlaylistButton;
@property (readwrite, assign) IBOutlet VLCLinearProgressIndicator *progressIndicator;
@property (readwrite, assign) IBOutlet NSBox *highlightBox;
@property (readwrite, assign) IBOutlet NSLayoutConstraint *imageViewAspectRatioConstraint;

@property (readwrite, retain, nonatomic) id<VLCMediaLibraryItemProtocol> representedItem;

@end

NS_ASSUME_NONNULL_END
