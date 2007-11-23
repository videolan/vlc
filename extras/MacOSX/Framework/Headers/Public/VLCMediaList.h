/*****************************************************************************
 * VLCMediaList.h: VLC.framework VLCMediaList header
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id: VLCMediaList.h 21564 2007-08-29 21:09:27Z pdherbemont $
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import "VLCMedia.h"

/* Notification Messages */
extern NSString * VLCMediaListItemAdded;
extern NSString * VLCMediaListItemDeleted;

@class VLCMedia;
@class VLCMediaList;
@class VLCMediaListAspect;

// TODO: Documentation
@protocol VLCMediaListDelegate
- (void)mediaList:(VLCMediaList *)aMediaList mediaAdded:(VLCMedia *)media atIndex:(int)index;
- (void)mediaList:(VLCMediaList *)aMediaList mediaRemovedAtIndex:(int)index;
@end

// TODO: Documentation
@interface VLCMediaList : NSObject
{
    void * p_mlist;                //< Internal instance of media list
    id <VLCMediaListDelegate,NSObject> delegate;                //< Delegate object
    NSMutableArray *cachedMedia; /* We need that private copy because of Cocoa Bindings, that need to be working on first thread */
}

/* Properties */
- (void)setDelegate:(id)value;
- (id)delegate;

/* Operations */
- (void)lock;
- (void)unlock;

- (int)addMedia:(VLCMedia *)media;
- (void)insertMedia:(VLCMedia *)media atIndex:(int)index;
- (void)removeMediaAtIndex:(int)index;
- (VLCMedia *)mediaAtIndex:(int)index;
- (int)indexOfMedia:(VLCMedia *)media;
- (int)count;

/* Media list aspect */
- (VLCMediaListAspect *)hierarchicalAspect;
- (VLCMediaListAspect *)flatAspect;
@end