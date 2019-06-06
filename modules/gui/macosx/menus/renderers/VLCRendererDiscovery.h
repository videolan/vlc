/*****************************************************************************
 * VLCRendererDiscovery.h: Wrapper class for vlc_renderer_discovery_t
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

#import "menus/renderers/VLCRendererItem.h"

@protocol VLCRendererDiscoveryDelegate;

/**
 \c VLCRendererDiscovery is a simple wrapper class for libvlc’s
 \c vlc_renderer_discovery_t. It's initialized with the renderer name and
 manages the underlying renderer discovery.
 */
@interface VLCRendererDiscovery : NSObject

/**
 The delegate that is called when a \c VLCRendererItem is added or deleted
 */
@property (assign) id<VLCRendererDiscoveryDelegate> delegate;

/**
 The name of the renderer discovery module
 */
@property (readonly) NSString *name;

/**
 The longname of the renderer discovery module
 */
@property (readonly) NSString *longName;

/**
 Array of \c VLCRendererItems that the module discovered
 */
@property (readonly) NSMutableArray<VLCRendererItem*> *rendererItems;


/**
 Indicates if the discovery has been started
 */
@property (readonly) bool discoveryStarted;

/**
 Initialize the class with a renderer name and (optional) longname retrieved with
 the \c vlc_rd_get_names function.

 \note In case the renderer discovery service creation fails, nil is returned.

 \param name        Renderer name as C string
 \param longname    Renderer longname as C string

 \returns   Initialized class that already created the underlying
            \c vlc_renderer_discovery_t structure or nil on failure.
 */
- (instancetype)initWithName:(const char*)name andLongname:(const char*)longname;

/**
 Starts the renderer discovery

 \return YES if the renderer was successfully started, NO otherwise.

 \sa -stopDiscovery
 */
- (bool)startDiscovery;

/**
 Stops the renderer discovery

 \note Stopping an already stopped renderer discovery has no effect.

 \sa -startDiscovery
 */
- (void)stopDiscovery;

@end

#pragma mark Delegate Protocol
/**
 \c VLCRendererDiscoveryDelegate protocol defines the required methods
 to be implemented by a \c VLCRendererDiscovery delegate.
 */
@protocol VLCRendererDiscoveryDelegate
@required

/**
 Invoked when a \c VLCRendererItem was added

 \param item    The renderer item that was added
 \param sender  The \c VLCRendererDiscovery object that added the Item.

 \sa -removedRendererItem:from:
 */
- (void)addedRendererItem:(VLCRendererItem *)item from:(VLCRendererDiscovery *)sender;

/**
 Invoked when a \c VLCRendererItem was removed

 \param item    The renderer item that was removed
 \param sender  The \c VLCRendererDiscovery object that removed the Item.

 \sa -addedRendererItem:from:
 */
- (void)removedRendererItem:(VLCRendererItem *)item from:(VLCRendererDiscovery *)sender;

@end
