/*****************************************************************************
 * VLCPlaylist.h: VLC.framework VLCPlaylist header
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
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

#import <Foundation/Foundation.h>

#import <VLC/VLCMedia.h>

/* Hack to avoid troubles with modules/macosx for now */
#define VLCPlaylist VLCPlaylistBis

/* Notification Posted */
extern NSString * VLCPlaylistItemChanged;
extern NSString * VLCPlaylistItemDeleted;
extern NSString * VLCPlaylistItemAdded;

@class VLCMedia;

@interface VLCPlaylist : NSObject
{
    NSMutableArray * items;
    void * p_mlist;
}
- (id)init;

- (void)lock;
- (void)unlock;

- (VLCMedia *)mediaAtIndex: (int)index;
- (int)countMedia;

- (NSArray *)sublists;
- (VLCPlaylist *)flatPlaylist;

- (VLCMedia *)providerMedia;

- (void)addMedia: (VLCMedia *)item;
- (void)insertMedia: (VLCMedia *)item atIndex: (int)index;
- (void)removeMediaAtIndex: (int)index;
@end

