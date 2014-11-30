/*****************************************************************************
 * PLItem.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 * $Id$
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

#import "PLItem.h"

#include <vlc_common.h>

@interface PLModel : NSObject<NSOutlineViewDataSource>
{
    PLItem *_rootItem;

    playlist_t *p_playlist;
    NSOutlineView *_outlineView;
}

@property(readonly) PLItem *rootItem;

- (id)initWithOutlineView:(NSOutlineView *)outlineView playlist:(playlist_t *)pl rootItem:(playlist_item_t *)root;

- (void)changeRootItem:(playlist_item_t *)p_root;

- (BOOL)hasChildren;

typedef enum {
    ROOT_TYPE_PLAYLIST,
    ROOT_TYPE_MEDIALIBRARY,
    ROOT_TYPE_OTHER
} PLRootType;

- (PLRootType)currentRootType;

- (BOOL)editAllowed;

- (void)addItem:(int)i_item withParentNode:(int)i_node;
- (void)removeItem:(int)i_item;

- (void)sortForColumn:(NSString *)o_column withMode:(int)i_mode;

- (void)searchUpdate:(NSString *)o_search;


@end

