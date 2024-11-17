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
@class VLCLibraryRepresentedItem;
@protocol VLCMediaLibraryItemProtocol;

extern NSString *VLCLibraryCellIdentifier;

@interface VLCLibraryCollectionViewItem : NSCollectionViewItem

// NOTE: These will need to be changed after changes to XIB
@property (class, readonly) const NSSize defaultSize;
@property (class, readonly) const NSSize defaultVideoItemSize;
@property (class, readonly) const CGFloat defaultWidth;
@property (class, readonly) const CGFloat bottomTextViewsHeight;
@property (class, readonly) const CGFloat videoHeightAspectRatioMultiplier;

@property (readwrite, assign) BOOL deselectWhenClickedIfSelected;

@property (readwrite, weak) IBOutlet NSTextField *mediaTitleTextField;
@property (readwrite, weak) IBOutlet NSTextField *annotationTextField;
@property (readwrite, weak) IBOutlet NSTextField *unplayedIndicatorTextField;
@property (readwrite, weak) IBOutlet NSTextField *secondaryInfoTextField;
@property (readwrite, weak) IBOutlet VLCImageView *mediaImageView;
@property (readwrite, weak) IBOutlet NSButton *playInstantlyButton;
@property (readwrite, weak) IBOutlet NSButton *addToPlayQueueButton;
@property (readwrite, weak) IBOutlet VLCLinearProgressIndicator *progressIndicator;
@property (readwrite, weak) IBOutlet NSBox *highlightBox;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *imageViewAspectRatioConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *trailingSecondaryTextToLeadingUnplayedIndicatorConstraint;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *trailingSecondaryTextToTrailingSuperviewConstraint;

@property (readwrite, retain, nonatomic) VLCLibraryRepresentedItem *representedItem;

@end

NS_ASSUME_NONNULL_END
