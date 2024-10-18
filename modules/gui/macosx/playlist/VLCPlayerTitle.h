/*****************************************************************************
 * VLCPlayerTitle.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#import <Foundation/Foundation.h>

#include <vlc_player.h>

@class VLCPlayerChapter;

NS_ASSUME_NONNULL_BEGIN

@interface VLCPlayerTitle : NSObject

@property (readonly) size_t index;
@property (readonly) NSString *name;
@property (readonly) vlc_tick_t length;
@property (readonly) NSString *lengthString;
@property (readonly) NSUInteger flags;
@property (readonly) NSUInteger chapterCount;
@property (readonly) NSArray<VLCPlayerChapter *> *chapters;

- (instancetype)initWithTitle:(const struct vlc_player_title *)p_title atIndex:(size_t)index;

@end

NS_ASSUME_NONNULL_END
