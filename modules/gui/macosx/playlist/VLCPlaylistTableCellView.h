/*****************************************************************************
 * VLCPlaylistTableViewCell.h: MacOS X interface module
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

@class VLCImageView;
@class VLCPlaylistItem;

NS_ASSUME_NONNULL_BEGIN

@interface VLCPlaylistTableCellView : NSTableCellView

@property (readwrite, assign, nonatomic) VLCPlaylistItem *representedPlaylistItem;
@property (readwrite, nonatomic) BOOL representsCurrentPlaylistItem;

@property (readwrite, assign) IBOutlet NSTextField *artistTextField;
@property (readwrite, assign) IBOutlet NSTextField *secondaryMediaTitleTextField;
@property (readwrite, assign) IBOutlet NSTextField *mediaTitleTextField;
@property (readwrite, assign) IBOutlet NSTextField *durationTextField;
@property (readwrite, assign) IBOutlet VLCImageView *mediaImageView;
@property (readwrite, assign) IBOutlet NSImageView *audioMediaTypeIndicator;
@property (readwrite, assign) IBOutlet VLCImageView *audioArtworkImageView;

@end

NS_ASSUME_NONNULL_END
