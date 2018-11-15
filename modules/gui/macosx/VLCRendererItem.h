/*****************************************************************************
 * VLCRendererItem.h: Wrapper class for vlc_renderer_item_t
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#include <vlc_common.h>
#include <vlc_playlist_legacy.h>

/**
 \c VLCRendererItem is a simple wrapper class for libvlc’s
 \c vlc_renderer_item_t. It's initialized with the renderer item and
 manages it's lifetime.
 */
@interface VLCRendererItem : NSObject

/**
 Initialize the object with a renderer item, typically received from
 a \c vlc_renderer_discovery_t event.
 */
- (instancetype)initWithRendererItem:(vlc_renderer_item_t*)item;

/**
 The underlying \c vlc_renderer_item_t item
 */
@property (readonly, assign) vlc_renderer_item_t* rendererItem;

/**
 The name of the renderer item
 */
- (NSString*)name;

- (NSString*)identifier;

/**
 The iconURI of the renderer item
 */
- (NSString*)iconURI;

/**
 Flags indicating capabilities of the renderer item

 Compare it to:
    \li \c VLC_RENDERER_CAN_AUDIO
    \li \c VLC_RENDERER_CAN_VIDEO
 */
- (int)capabilityFlags;

/**
 Sets the renderer represented by this \c VLCRendererItem as active
 for the given playlist.

 \param playlist The playlist for which to set the renderer
 */
- (void)setRendererForPlaylist:(playlist_t*)playlist;

@end
