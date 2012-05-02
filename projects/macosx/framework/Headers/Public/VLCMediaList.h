/*****************************************************************************
 * VLCMediaList.h: VLCKit.framework VLCMediaList header
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
#import "VLCMedia.h"

/* Notification Messages */
extern NSString * VLCMediaListItemAdded;
extern NSString * VLCMediaListItemDeleted;

@class VLCMedia;
@class VLCMediaList;

/**
 * TODO: Documentation VLCMediaListDelegate
 */
@protocol VLCMediaListDelegate
/**
 * TODO: Documentation - [VLCMediaListDelegate mediaList:mediaAdded:atIndex:]
 */
- (void)mediaList:(VLCMediaList *)aMediaList mediaAdded:(VLCMedia *)media atIndex:(NSInteger)index;

/**
 * TODO: Documentation - [VLCMediaListDelegate mediaList:mediaRemovedAtIndex:]
 */
- (void)mediaList:(VLCMediaList *)aMediaList mediaRemovedAtIndex:(NSInteger)index;
@end

/**
 * TODO: Documentation VLCMediaList
 */
@interface VLCMediaList : NSObject
{
    void * p_mlist;                                 //< Internal instance of media list
    id <VLCMediaListDelegate,NSObject> delegate;    //< Delegate object
    /* We need that private copy because of Cocoa Bindings, that need to be working on first thread */
    NSMutableArray * cachedMedia;                   //< Private copy of media objects.
}

/* Operations */
/**
 * TODO: Documentation - [VLCMediaList lock]
 */
- (void)lock;

/**
 * TODO: Documentation - [VLCMediaList unlock]
 */
- (void)unlock;

/**
 * TODO: Documentation - [VLCMediaList addMedia:]
 */
- (NSInteger)addMedia:(VLCMedia *)media;

/**
 * TODO: Documentation - [VLCMediaList insertMedia:atIndex:]
 */
- (void)insertMedia:(VLCMedia *)media atIndex:(NSInteger)index;

/**
 * TODO: Documentation - [VLCMediaList removeMediaAtIndex:]
 */
- (void)removeMediaAtIndex:(NSInteger)index;

/**
 * TODO: Documentation - [VLCMediaList mediaAtIndex:]
 */
- (VLCMedia *)mediaAtIndex:(NSInteger)index;

/**
 * TODO: Documentation - [VLCMediaList indexOfMedia:]
 */
- (NSInteger)indexOfMedia:(VLCMedia *)media;

/* Properties */
/**
 * TODO: Documentation VLCMediaList.count
 */
@property (readonly) NSInteger count;

/**
 * TODO: Documentation VLCMediaList.delegate
 */
@property (assign) id delegate;

/**
 * TODO: Documentation VLCMediaList.isReadOnly
 */
@property (readonly) BOOL isReadOnly;

@end
