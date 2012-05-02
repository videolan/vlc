/*****************************************************************************
 * VLCMediaDiscoverer.h: VLCKit.framework VLCMediaDiscoverer header
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import <Foundation/Foundation.h>
#import "VLCMediaList.h"

@class VLCMediaList;

/**
 * TODO: Documentation VLCMediaDiscoverer
 */
@interface VLCMediaDiscoverer : NSObject
{
    NSString * localizedName;       //< TODO: Documentation VLCMediaDiscoverer.localizedName
    VLCMediaList * discoveredMedia; //< TODO: Documentation VLCMediaDiscoverer.discoveredMedia
    void * mdis;                    //< TODO: Documentation VLCMediaDiscoverer.mdis
    BOOL running;                   //< TODO: Documentation VLCMediaDiscoverer.running
}

/**
 * Maintains a list of available media discoverers.  This list is populated as new media
 * discoverers are created.
 * \return A list of available media discoverers.
 */
+ (NSArray *)availableMediaDiscoverer;

/* Initializers */
/**
 * Initializes new object with specified name.
 * \param aSerchName Name of the service for this VLCMediaDiscoverer object.
 * \returns Newly created media discoverer.
 */
- (id)initWithName:(NSString *)aServiceName;

/**
 * TODO: Documentation VLCMediaDiscoverer.discoveredMedia
 */
@property (readonly) VLCMediaList * discoveredMedia;

/**
 * TODO: Documentation VLCMediaDiscoverer.localizedName
 */
@property (readonly) NSString * localizedName;

/**
 * TODO: Documentation VLCMediaDiscoverer.isRunning
 */
@property (readonly) BOOL isRunning;
@end
