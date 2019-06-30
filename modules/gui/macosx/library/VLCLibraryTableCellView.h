/*****************************************************************************
 * VLCLibraryTableCellView.h: MacOS X interface module
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
@class VLCTrackingView;
@class VLCMediaLibraryMediaItem;
@class VLCInputItem;

@interface VLCLibraryTableCellView : NSTableCellView

@property (readwrite, assign) IBOutlet VLCTrackingView *trackingView;
@property (readwrite, assign) IBOutlet NSTextField *singlePrimaryTitleTextField;
@property (readwrite, assign) IBOutlet NSTextField *secondaryTitleTextField;
@property (readwrite, assign) IBOutlet NSTextField *primaryTitleTextField;
@property (readwrite, assign) IBOutlet VLCImageView *representedImageView;
@property (readwrite, assign) IBOutlet NSButton *playInstantlyButton;
@property (readwrite, strong, nonatomic) VLCMediaLibraryMediaItem *representedMediaItem;
@property (readwrite, strong, nonatomic) VLCInputItem *representedInputItem;

@end

NS_ASSUME_NONNULL_END
