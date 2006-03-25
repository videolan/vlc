/*****************************************************************************
 * playlistinfo.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Felix Kühne <fkuehne at videolan dot org>
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

/*****************************************************************************
 * VLCPlaylistInfo interface 
 *****************************************************************************/


@interface VLCInfo : NSObject
{
    IBOutlet id o_info_window;
    IBOutlet id o_uri_lbl;
    IBOutlet id o_title_lbl;
    IBOutlet id o_author_lbl;
    IBOutlet id o_uri_txt;
    IBOutlet id o_title_txt;
    IBOutlet id o_author_txt;
    IBOutlet id o_btn_ok;
    IBOutlet id o_btn_cancel;
    IBOutlet id o_btn_delete_group;
    IBOutlet id o_btn_add_group;
    IBOutlet id o_outline_view;
    
    IBOutlet id o_tab_view;

    IBOutlet id o_collection_lbl;
    IBOutlet id o_collection_txt;
    IBOutlet id o_copyright_lbl;
    IBOutlet id o_copyright_txt;
    IBOutlet id o_date_lbl;
    IBOutlet id o_date_txt;
    IBOutlet id o_description_lbl;
    IBOutlet id o_description_txt;
    IBOutlet id o_genre_lbl;
    IBOutlet id o_genre_txt;
    IBOutlet id o_language_lbl;
    IBOutlet id o_language_txt;
    IBOutlet id o_nowPlaying_lbl;
    IBOutlet id o_nowPlaying_txt;
    IBOutlet id o_publisher_lbl;
    IBOutlet id o_publisher_txt;
    IBOutlet id o_rating_lbl;
    IBOutlet id o_rating_txt;
    IBOutlet id o_seqNum_lbl;
    IBOutlet id o_seqNum_txt;

    IBOutlet id o_audio_box;
    IBOutlet id o_audio_decoded_lbl;
    IBOutlet id o_audio_decoded_txt;
    IBOutlet id o_demux_bitrate_lbl;
    IBOutlet id o_demux_bitrate_txt;
    IBOutlet id o_demux_bytes_lbl;
    IBOutlet id o_demux_bytes_txt;
    IBOutlet id o_displayed_lbl;
    IBOutlet id o_displayed_txt;
    IBOutlet id o_input_bitrate_lbl;
    IBOutlet id o_input_bitrate_txt;
    IBOutlet id o_input_box;
    IBOutlet id o_lost_abuffers_lbl;
    IBOutlet id o_lost_abuffers_txt;
    IBOutlet id o_lost_frames_lbl;
    IBOutlet id o_lost_frames_txt;
    IBOutlet id o_played_abuffers_lbl;
    IBOutlet id o_played_abuffers_txt;
    IBOutlet id o_read_bytes_lbl;
    IBOutlet id o_read_bytes_txt;
    IBOutlet id o_sent_bitrate_lbl;
    IBOutlet id o_sent_bitrate_txt;
    IBOutlet id o_sent_bytes_lbl;
    IBOutlet id o_sent_bytes_txt;
    IBOutlet id o_sent_packets_lbl;
    IBOutlet id o_sent_packets_txt;
    IBOutlet id o_sout_box;
    IBOutlet id o_video_box;
    IBOutlet id o_video_decoded_lbl;
    IBOutlet id o_video_decoded_txt;

    playlist_item_t * p_item;
    NSTimer * o_statUpdateTimer;
}

- (IBAction)togglePlaylistInfoPanel:(id)sender;
- (IBAction)toggleInfoPanel:(id)sender;
- (void)initPanel:(id)sender;
- (void)updatePanel;
- (IBAction)infoCancel:(id)sender;
- (IBAction)infoOk:(id)sender;
- (playlist_item_t *)getItem;
- (BOOL)isItemInPlaylist:(playlist_item_t *)p_item;

- (void)setMeta: (char *)meta forLabel: (id)theItem;
- (void)updateStatistics: (NSTimer*)theTimer;
@end

@interface VLCInfoTreeItem : NSObject
{
    NSString *o_name;
    NSString *o_value;
    int i_object_id;
    playlist_item_t * p_item;
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

