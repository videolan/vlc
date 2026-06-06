/*****************************************************************************
 * NSScrollView+VLCAdditions.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

@interface NSScrollView (VLCAdditions)

/// Creates an NSScrollView pre-configured for use in the library UI with
/// the given @p contentInsets and standard library scroller insets.
+ (instancetype)libraryScrollViewWithDocumentView:(NSView *)documentView
                                    contentInsets:(NSEdgeInsets)contentInsets;

/// Creates an NSScrollView pre-configured for use in the library UI,
/// with standard library content insets and scroller insets applied.
+ (instancetype)libraryScrollViewWithDocumentView:(NSView *)documentView;

/// Creates an NSScrollView pre-configured for use as a detail (secondary) pane
/// in the library UI, with detail list content insets and standard scroller insets.
+ (instancetype)libraryDetailScrollViewWithDocumentView:(NSView *)documentView;

@end

NS_ASSUME_NONNULL_END
