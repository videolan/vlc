/*****************************************************************************
 * VLCLibraryMediaSourceViewNavigationStack.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

@class VLCLibraryWindow;
@class VLCMediaSource;
@class VLCMediaSourceBaseDataSource;
@class VLCLibraryMediaSourceViewNavigationState;

typedef struct input_item_node_t input_item_node_t;

@interface VLCLibraryMediaSourceViewNavigationStack : NSObject

@property (readwrite, nonatomic, weak) VLCLibraryWindow *libraryWindow;
@property (readwrite, weak) VLCMediaSourceBaseDataSource *baseDataSource;
@property (readonly) BOOL forwardsAvailable;
@property (readonly) BOOL backwardsAvailable;

- (void)backwards;
- (void)forwards;

- (void)appendCurrentLibraryState;
- (void)clear;

- (void)installHandlersOnMediaSource:(VLCMediaSource *)mediaSource;

@end

NS_ASSUME_NONNULL_END
