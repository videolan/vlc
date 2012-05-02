/*****************************************************************************
 * VLCPlaylistDataSource.h: VLC.framework VLCPlaylistDataSource header
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

#import <VLC/VLCPlaylist.h>
#import <VLC/VLCVideoView.h>

/* This class can be used as a data source for an NSOutlineView
 * it will display the playlist content. If provided the videoView
 * will automatically be associated to the given playlist, and actions
 * in the outlineView will trigger the videoView, visual feedback of the
 * current item in the videoview will be displayed in the outlineview
 */
@interface VLCPlaylistDataSource : NSObject
{
    VLCPlaylist  * playlist;
    VLCVideoView * videoView;
    
    NSOutlineView *outlineView;
}
- (id)initWithPlaylist:(VLCPlaylist *)aPlaylist;
- (id)initWithPlaylist:(VLCPlaylist *)aPlaylist videoView:(VLCVideoView *)aVideoView;

- (VLCPlaylist *)playlist;
- (VLCVideoView *)videoView;
@end

/* It could be really useful to use that, this probably need to be reviewed to see
 * if it really belongs here */
@interface VLCPlaylistDataSource (OutlineViewDataSource)
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;
@end

@interface VLCPlaylistDataSource (OutlineViewDataSourceDropping)
- (BOOL)outlineView:(NSOutlineView *)outlineView acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(int)index;
- (NSDragOperation)outlineView:(NSOutlineView *)outlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index;
@end


