/*****************************************************************************
 * playlistinfo.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

@class VLCInfoTreeItem;

@interface VLCInfo : NSObject
{
    IBOutlet id o_info_window;
    IBOutlet id o_uri_lbl;
    IBOutlet id o_title_lbl;
    IBOutlet id o_author_lbl;
    IBOutlet id o_uri_txt;
    IBOutlet id o_title_txt;
    IBOutlet id o_author_txt;
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
    IBOutlet id o_encodedby_lbl;
    IBOutlet id o_encodedby_txt;
    IBOutlet id o_genre_lbl;
    IBOutlet id o_genre_txt;
    IBOutlet id o_language_lbl;
    IBOutlet id o_language_txt;
    IBOutlet id o_nowPlaying_lbl;
    IBOutlet id o_nowPlaying_txt;
    IBOutlet id o_publisher_lbl;
    IBOutlet id o_publisher_txt;
    IBOutlet id o_seqNum_lbl;
    IBOutlet id o_seqNum_txt;
    IBOutlet id o_image_well;
    IBOutlet id o_saveMetaData_btn;

    IBOutlet id o_audio_lbl;
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
    IBOutlet id o_input_lbl;
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
    IBOutlet id o_sout_lbl;
    IBOutlet id o_video_lbl;
    IBOutlet id o_video_decoded_lbl;
    IBOutlet id o_video_decoded_txt;

    VLCInfoTreeItem * rootItem;

    input_item_t * p_item;

    BOOL b_awakeFromNib;
    BOOL b_stats;
}
@property (readonly) input_item_t * item;

- (void)updateCocoaWindowLevel:(NSInteger)i_level;
- (void)initPanel;

- (IBAction)metaFieldChanged:(id)sender;
- (IBAction)saveMetaData:(id)sender;
- (IBAction)downloadCoverArt:(id)sender;
- (void)initMediaPanelStats;
- (void)updatePanelWithItem:(input_item_t *)_p_item;
- (void)setMeta: (char *)meta forLabel: (id)theItem;
- (void)updateStatistics;

+ (VLCInfo *)sharedInstance;
@end

@interface VLCInfoTreeItem : NSObject
{
    NSString *o_name;
    NSString *o_value;
    int i_object_id;
    input_item_t * p_item;
    VLCInfoTreeItem *o_parent;
    NSMutableArray *o_children;
}

@property (readonly) int numberOfChildren;
@property (readonly) NSString * name;
@property (readonly) NSString * value;

- (VLCInfoTreeItem *)childAtIndex:(NSUInteger)i_index;
- (void)refresh;

@end

