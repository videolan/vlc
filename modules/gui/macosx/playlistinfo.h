/*****************************************************************************
 * playlistinfo.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2004 VideoLAN
 * $Id: playlist.h 7015 2004-03-08 15:22:58Z bigben $
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org> 
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * VLCPlaylistInfo interface 
 *****************************************************************************/


@interface VLCPlaylistInfo : NSObject
{
    IBOutlet id o_info_window;
    IBOutlet id o_uri_lbl;
    IBOutlet id o_title_lbl;
    IBOutlet id o_author_lbl;
    IBOutlet id o_uri_txt;
    IBOutlet id o_title_txt;
    IBOutlet id o_author_txt;
    IBOutlet id o_btn_info_ok;
    IBOutlet id o_btn_info_cancel;
    IBOutlet id o_outline_view;
    IBOutlet id o_vlc_playlist;
    IBOutlet id o_group_lbl;
    IBOutlet id o_group_cbx;
    IBOutlet id o_group_color;
}

- (IBAction)togglePlaylistInfoPanel:(id)sender;
- (IBAction)infoCancel:(id)sender;
- (IBAction)infoOk:(id)sender;
- (IBAction)handleGroup:(id)sender;

@end

@interface VLCInfoTreeItem : NSObject
{
    NSString *o_name;
    NSString *o_value;
    int i_object_id;
    int i_item;
    VLCInfoTreeItem *o_parent;
    NSMutableArray *o_children;
}

+ (VLCInfoTreeItem *)rootItem;
- (int)numberOfChildren;
- (VLCInfoTreeItem *)childAtIndex:(int)i_index;
- (NSString *)getName;
- (NSString *)getValue;
- (void)refresh;

@end

