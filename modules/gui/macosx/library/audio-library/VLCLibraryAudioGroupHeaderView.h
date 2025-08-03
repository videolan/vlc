/*****************************************************************************
 * VLCLibraryAudioGroupHeaderView.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

@class VLCLibraryRepresentedItem;
@protocol VLCMediaLibraryAudioGroupProtocol;

extern NSString * const VLCLibraryAudioGroupHeaderViewIdentifier;

@interface VLCLibraryAudioGroupHeaderView : NSView

@property (class, readonly) const CGSize defaultHeaderSize;

@property (readwrite, retain, nonatomic) VLCLibraryRepresentedItem *representedItem;

@property (readwrite, weak) IBOutlet NSStackView *stackView;
@property (readwrite, weak) IBOutlet NSTextField *titleTextField;
@property (readwrite, weak) IBOutlet NSTextField *detailTextField;
@property (readwrite, weak) IBOutlet NSButton *playButton;
@property (readwrite, weak) IBOutlet NSButton *queueButton;
@property (readwrite, weak) IBOutlet NSVisualEffectView *backgroundEffectView;

- (IBAction)play:(id)sender;
- (IBAction)enqueue:(id)sender;

@end

NS_ASSUME_NONNULL_END
