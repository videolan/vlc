/*****************************************************************************
 * PLItem.m: MacOS X interface module
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

#import "PLModel.h"

#import "misc.h"

#include <vlc_playlist.h>
#include <vlc_input_item.h>
#include <vlc_url.h>

#define TRACKNUM_COLUMN @"tracknumber"
#define TITLE_COLUMN @"name"
#define ARTIST_COLUMN @"artist"
#define DURATION_COLUMN @"duration"
#define GENRE_COLUMN @"genre"
#define ALBUM_COLUMN @"album"
#define DESCRIPTION_COLUMN @"description"
#define DATE_COLUMN @"date"
#define LANGUAGE_COLUMN @"language"
#define URI_COLUMN @"uri"
#define FILESIZE_COLUMN @"file-size"

#pragma mark -

@implementation PLModel

@synthesize rootItem=_rootItem;

- (id)initWithOutlineView:(NSOutlineView *)outlineView playlist:(playlist_t *)pl rootItem:(playlist_item_t *)root;
{
    self = [super init];
    if(self) {
        p_playlist = pl;
        _outlineView = [outlineView retain];

        PL_LOCK;
        _rootItem = [[PLItem alloc] initWithPlaylistItem:root parent:nil];
        [self rebuildPLItem:_rootItem];
        PL_UNLOCK;

    }

    return self;
}

- (void)changeRootItem:(playlist_item_t *)p_root;
{
    NSLog(@"change root item to %p", p_root);
    PL_ASSERT_LOCKED;
    [_rootItem release];
    _rootItem = [[PLItem alloc] initWithPlaylistItem:p_root parent:nil];
    [self rebuildPLItem:_rootItem];
    [_outlineView reloadData];
}

- (BOOL)hasChildren
{
    return [[_rootItem children] count] > 0;
}

- (PLRootType)currentRootType
{
    int i_root_id = [_rootItem plItemId];
    if (i_root_id == p_playlist->p_playing->i_id)
        return ROOT_TYPE_PLAYLIST;
    if (p_playlist->p_media_library && i_root_id == p_playlist->p_media_library->i_id)
        return ROOT_TYPE_MEDIALIBRARY;

    return ROOT_TYPE_OTHER;
}

- (BOOL)editAllowed
{
    return [self currentRootType] == ROOT_TYPE_MEDIALIBRARY ||
    [self currentRootType] == ROOT_TYPE_PLAYLIST;
}



- (void)rebuildPLItem:(PLItem *)o_item
{
    [o_item clear];
    playlist_item_t *p_item = playlist_ItemGetById(p_playlist, [o_item plItemId]);
    if (p_item) {
        for(int i = 0; i < p_item->i_children; ++i) {
            PLItem *o_child = [[[PLItem alloc] initWithPlaylistItem:p_item->pp_children[i] parent:o_item] autorelease];
            [o_item addChild:o_child atPos:i];

            if (p_item->pp_children[i]->i_children >= 0) {
                [self rebuildPLItem:o_child];
            }

        }
    }

}

- (PLItem *)findItemByPlaylistId:(int)i_pl_id
{
    return [self findItemInnerByPlaylistId:i_pl_id node:_rootItem];
}

- (PLItem *)findItemInnerByPlaylistId:(int)i_pl_id node:(PLItem *)node
{
    if ([node plItemId] == i_pl_id) {
        return node;
    }

    for (NSUInteger i = 0; i < [[node children] count]; ++i) {
        PLItem *o_sub_item = [[node children] objectAtIndex:i];
        if ([o_sub_item plItemId] == i_pl_id) {
            return o_sub_item;
        }

        if (![o_sub_item isLeaf]) {
            PLItem *o_returned = [self findItemInnerByPlaylistId:i_pl_id node:o_sub_item];
            if (o_returned)
                return o_returned;
        }
    }

    return nil;
}


- (void)addItem:(int)i_item withParentNode:(int)i_node
{
    NSLog(@"add item with index %d, parent: %d", i_item, i_node);

    PLItem *o_parent = [self findItemByPlaylistId:i_node];
    if (!o_parent) {
        return;
    }

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById(p_playlist, i_item);
    if (!p_item || p_item->i_flags & PLAYLIST_DBL_FLAG)
    {
        PL_UNLOCK; return;
    }

    int pos;
    for(pos = p_item->p_parent->i_children - 1; pos >= 0; pos--)
        if(p_item->p_parent->pp_children[pos] == p_item)
            break;

    PLItem *o_new_item = [[[PLItem alloc] initWithPlaylistItem:p_item parent:o_parent] autorelease];
    PL_UNLOCK;
    if (pos < 0)
        return;

    [o_parent addChild:o_new_item atPos:pos];

    if ([o_parent plItemId] == [_rootItem plItemId])
        [_outlineView reloadData];
    else // only reload leafs this way, doing it with nil collapses width of title column
        [_outlineView reloadItem:o_parent reloadChildren:YES];
}

- (void)removeItem:(int)i_item
{
    NSLog(@"remove item with index %d", i_item);

    PLItem *o_item = [self findItemByPlaylistId:i_item];
    if (!o_item) {
        return;
    }

    PLItem *o_parent = [o_item parent];
    [o_parent deleteChild:o_item];

    if ([o_parent plItemId] == [_rootItem plItemId])
        [_outlineView reloadData];
    else
        [_outlineView reloadItem:o_parent reloadChildren:YES];
}

- (void)sortForColumn:(NSString *)o_column withMode:(int)i_mode
{
    int i_column = 0;
    if ([o_column isEqualToString:TRACKNUM_COLUMN])
        i_column = SORT_TRACK_NUMBER;
    else if ([o_column isEqualToString:TITLE_COLUMN])
        i_column = SORT_TITLE;
    else if ([o_column isEqualToString:ARTIST_COLUMN])
        i_column = SORT_ARTIST;
    else if ([o_column isEqualToString:GENRE_COLUMN])
        i_column = SORT_GENRE;
    else if ([o_column isEqualToString:DURATION_COLUMN])
        i_column = SORT_DURATION;
    else if ([o_column isEqualToString:ALBUM_COLUMN])
        i_column = SORT_ALBUM;
    else if ([o_column isEqualToString:DESCRIPTION_COLUMN])
        i_column = SORT_DESCRIPTION;
    else if ([o_column isEqualToString:URI_COLUMN])
        i_column = SORT_URI;
    else
        return;

    PL_LOCK;
    playlist_item_t *p_root = playlist_ItemGetById(p_playlist, [_rootItem plItemId]);
    if (!p_root) {
        PL_UNLOCK;
        return;
    }

    playlist_RecursiveNodeSort(p_playlist, p_root, i_column, i_mode);

    [self rebuildPLItem:_rootItem];
    [_outlineView reloadData];
    PL_UNLOCK;
}

@end

#pragma mark -
#pragma mark Outline view data source

@implementation PLModel(NSOutlineViewDataSource)

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    return !item ? [[_rootItem children] count] : [[item children] count];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    return !item ? YES : [[item children] count] > 0;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
    id obj = !item ? _rootItem : item;
    return [[obj children] objectAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    id o_value = nil;
    char * psz_value;
    playlist_item_t *p_item;

    input_item_t *p_input = [item input];

    NSString * o_identifier = [tableColumn identifier];

    if ([o_identifier isEqualToString:TRACKNUM_COLUMN]) {
        psz_value = input_item_GetTrackNumber(p_input);
        if (psz_value) {
            o_value = [NSString stringWithUTF8String:psz_value];
            free(psz_value);
        }
    } else if ([o_identifier isEqualToString:TITLE_COLUMN]) {
        /* sanity check to prevent the NSString class from crashing */
        char *psz_title =  input_item_GetTitleFbName(p_input);
        if (psz_title) {
            o_value = [NSString stringWithUTF8String:psz_title];
            free(psz_title);
        }
    } else if ([o_identifier isEqualToString:ARTIST_COLUMN]) {
        psz_value = input_item_GetArtist(p_input);
        if (psz_value) {
            o_value = [NSString stringWithUTF8String:psz_value];
            free(psz_value);
        }
    } else if ([o_identifier isEqualToString:@"duration"]) {
        char psz_duration[MSTRTIME_MAX_SIZE];
        mtime_t dur = input_item_GetDuration(p_input);
        if (dur != -1) {
            secstotimestr(psz_duration, dur/1000000);
            o_value = [NSString stringWithUTF8String:psz_duration];
        }
        else
            o_value = @"--:--";
    } else if ([o_identifier isEqualToString:GENRE_COLUMN]) {
        psz_value = input_item_GetGenre(p_input);
        if (psz_value) {
            o_value = [NSString stringWithUTF8String:psz_value];
            free(psz_value);
        }
    } else if ([o_identifier isEqualToString:ALBUM_COLUMN]) {
        psz_value = input_item_GetAlbum(p_input);
        if (psz_value) {
            o_value = [NSString stringWithUTF8String:psz_value];
            free(psz_value);
        }
    } else if ([o_identifier isEqualToString:DESCRIPTION_COLUMN]) {
        psz_value = input_item_GetDescription(p_input);
        if (psz_value) {
            o_value = [NSString stringWithUTF8String:psz_value];
            free(psz_value);
        }
    } else if ([o_identifier isEqualToString:DATE_COLUMN]) {
        psz_value = input_item_GetDate(p_input);
        if (psz_value) {
            o_value = [NSString stringWithUTF8String:psz_value];
            free(psz_value);
        }
    } else if ([o_identifier isEqualToString:LANGUAGE_COLUMN]) {
        psz_value = input_item_GetLanguage(p_input);
        if (psz_value) {
            o_value = [NSString stringWithUTF8String:psz_value];
            free(psz_value);
        }
    }
    else if ([o_identifier isEqualToString:URI_COLUMN]) {
        psz_value = decode_URI(input_item_GetURI(p_input));
        if (psz_value) {
            o_value = [NSString stringWithUTF8String:psz_value];
            free(psz_value);
        }
    }
    else if ([o_identifier isEqualToString:FILESIZE_COLUMN]) {
        psz_value = input_item_GetURI(p_item->p_input);
        o_value = @"";
        if (psz_value) {
            NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:psz_value]];
            if ([url isFileURL]) {
                NSFileManager *fileManager = [NSFileManager defaultManager];
                if ([fileManager fileExistsAtPath:[url path]]) {
                    NSError *error;
                    NSDictionary *attributes = [fileManager attributesOfItemAtPath:[url path] error:&error];
                    o_value = [VLCByteCountFormatter stringFromByteCount:[attributes fileSize] countStyle:NSByteCountFormatterCountStyleDecimal];
                }
            }
            free(psz_value);
        }

    }
    else if ([o_identifier isEqualToString:@"status"]) {
        if (input_item_HasErrorWhenReading(p_input)) {
            o_value = [[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeForHFSTypeCode(kAlertCautionIcon)];
            [o_value setSize: NSMakeSize(16,16)];
        }
    }

    return o_value;
}

@end
