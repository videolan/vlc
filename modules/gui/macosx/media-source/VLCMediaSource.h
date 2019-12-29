/*****************************************************************************
 * VLCMediaSource.h: MacOS X interface module
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

#import <Foundation/Foundation.h>

#import <vlc_media_source.h>

@class VLCInputItem;
@class VLCInputNode;

NS_ASSUME_NONNULL_BEGIN

extern NSString *VLCMediaSourceChildrenReset;
extern NSString *VLCMediaSourceChildrenAdded;
extern NSString *VLCMediaSourceChildrenRemoved;
extern NSString *VLCMediaSourcePreparsingEnded;

@interface VLCMediaSource : NSObject

- (instancetype)initWithMediaSource:(vlc_media_source_t *)p_mediaSource andLibVLCInstance:(libvlc_int_t *)p_libvlcInstance;

- (void)preparseInputItemWithinTree:(VLCInputItem *)inputItem;

@property (readonly) NSString *mediaSourceDescription;
@property (readonly) VLCInputNode *rootNode;

@end

NS_ASSUME_NONNULL_END
