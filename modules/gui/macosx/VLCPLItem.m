/*****************************************************************************
 * VLCPLItem.m: MacOS X interface module
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

#import "VLCPLItem.h"

#include <vlc_playlist_legacy.h>
#include <vlc_input_item.h>

#pragma mark -

@implementation VLCPLItem

- (id)initWithPlaylistItem:(playlist_item_t *)p_item;
{
    self = [super init];
    if(self) {
        _plItemId = p_item->i_id;

        _input = p_item->p_input;
        input_item_Hold(_input);
        _children = [[NSMutableArray alloc] init];
    }

    return self;
}

- (void)dealloc
{
    input_item_Release(_input);
}

// own hash and isEqual methods are important to retain expandable state
// when items are moved / recreated
- (NSUInteger)hash
{
    return (NSUInteger)[self plItemId];
}

- (BOOL)isEqual:(id)other
{
    if (other == self)
        return YES;
    if (!other || ![other isKindOfClass:[self class]])
        return NO;
    return [self plItemId] == [other plItemId];
}

- (BOOL)isLeaf
{
    return [_children count] == 0;
}

- (void)clear
{
    [_children removeAllObjects];
}

- (void)addChild:(VLCPLItem *)item atPos:(int)pos
{
    [_children insertObject:item atIndex:pos];
    [item setParent: self];

}

- (void)deleteChild:(VLCPLItem *)child
{
    [child setParent:nil];
    [_children removeObject:child];
}

@end
