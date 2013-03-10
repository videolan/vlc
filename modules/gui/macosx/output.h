/*****************************************************************************
 * output.h: MacOS X Output Dialog
 *****************************************************************************
 * Copyright (C) 2002-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
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
 * VLCOutput interface
 *****************************************************************************/
@interface VLCOutput : NSObject
{
    IBOutlet id o_open_panel;
    IBOutlet id o_output_ckbox;
    IBOutlet id o_output_settings;
    IBOutlet id o_output_sheet;
    IBOutlet id o_btn_ok;

    IBOutlet id o_options_lbl;
    IBOutlet id o_display;
    IBOutlet id o_method;
    IBOutlet id o_file_field;
    IBOutlet id o_dump_chkbox;
    IBOutlet id o_btn_browse;
    IBOutlet id o_stream_address;
    IBOutlet id o_stream_address_lbl;
    IBOutlet id o_stream_port;
    IBOutlet id o_stream_port_stp;
    IBOutlet id o_stream_port_lbl;
    IBOutlet id o_stream_ttl;
    IBOutlet id o_stream_ttl_stp;
    IBOutlet id o_stream_ttl_lbl;
    IBOutlet id o_stream_type;
    IBOutlet id o_stream_type_lbl;

    IBOutlet id o_mux_lbl;
    IBOutlet id o_mux_selector;

    IBOutlet id o_transcode_audio_bitrate;
    IBOutlet id o_transcode_audio_bitrate_lbl;
    IBOutlet id o_transcode_audio_channels;
    IBOutlet id o_transcode_audio_channels_lbl;
    IBOutlet id o_transcode_audio_chkbox;
    IBOutlet id o_transcode_audio_selector;
    IBOutlet id o_transcode_lbl;
    IBOutlet id o_transcode_video_bitrate;
    IBOutlet id o_transcode_video_bitrate_lbl;
    IBOutlet id o_transcode_video_scale;
    IBOutlet id o_transcode_video_scale_lbl;
    IBOutlet id o_transcode_video_chkbox;
    IBOutlet id o_transcode_video_selector;

    IBOutlet id o_misc_lbl;
    IBOutlet id o_sap_chkbox;
    IBOutlet id o_channel_name;
    IBOutlet id o_channel_name_lbl;

    IBOutlet id o_sdp_url;
    IBOutlet id o_sdp_url_lbl;
    IBOutlet id o_rtsp_chkbox;
    IBOutlet id o_http_chkbox;
    IBOutlet id o_file_chkbox;

    NSArray *o_mrl;
    NSString *o_transcode;
}
@property (readwrite, retain) NSArray * soutMRL;

- (void)setTranscode:(NSString *)o_transcode_string;

- (void)initStrings;

- (IBAction)outputChanged:(id)sender;
- (IBAction)outputSettings:(id)sender;
- (IBAction)outputCloseSheet:(id)sender;
- (void)outputMethodChanged:(NSNotification *)o_notification;
- (void)outputInfoChanged:(NSNotification *)o_notification;
- (void)TTLChanged:(NSNotification *)o_notification;
- (IBAction)outputFileBrowse:(id)sender;
- (void)transcodeChanged:(NSNotification *)o_notification;
- (void)transcodeInfoChanged:(NSNotification *)o_notification;
- (IBAction)announceChanged:(id)sender;
@end
