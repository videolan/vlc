/*****************************************************************************
 * output.m: MacOS X Output Dialog
 *****************************************************************************
 * Copyright (C) 2002-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Benjamin Pracht <bigben AT videolan DOT org>
 *          Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include "intf.h"
#include "output.h"

/*****************************************************************************
 * VLCOutput implementation
 *****************************************************************************/
@implementation VLCOutput
@synthesize soutMRL=o_mrl;

- (id)init
{
    self = [super init];
    o_mrl = [[NSArray alloc] init];
    o_transcode = [[NSString alloc] init];
    return self;
}

- (void)dealloc
{
    [o_mrl release];
    [o_transcode release];
    [super dealloc];
}

- (void)setTranscode:(NSString *)o_transcode_string
{
    [o_transcode autorelease];
    o_transcode = [o_transcode_string copy];
}

- (void)awakeFromNib
{
    [self initStrings];

    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(outputInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_file_field];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(outputInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_stream_address];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(outputInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_stream_port];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(TTLChanged:)
        name: NSControlTextDidChangeNotification
        object: o_stream_ttl];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_transcode_video_bitrate];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_transcode_video_scale];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_transcode_audio_bitrate];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_transcode_audio_channels];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_channel_name];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: o_sdp_url];

    [o_mux_selector setAutoenablesItems: NO];
    [self transcodeChanged:nil];
}

- (void)initStrings
{
    NSArray *o_muxers = [NSArray arrayWithObjects:@"MPEG TS", @"MPEG PS", @"MPEG 1",
        @"Ogg", @"AVI", @"ASF", @"MPEG 4", @"Quicktime", @"Raw", nil];
    NSArray *o_a_channels = [NSArray arrayWithObjects:@"1", @"2", @"4", @"6", nil];
    NSArray *o_a_bitrates = [NSArray arrayWithObjects:@"16", @"32", @"64", @"96",
        @"128", @"192", @"256", @"512", nil];
    NSArray *o_v_bitrates = [NSArray arrayWithObjects:@"16", @"32", @"64", @"96",
        @"128", @"192", @"256", @"384", @"512", @"768", @"1024", @"2048", @"3072", nil];
    NSArray *o_v_scales = [NSArray arrayWithObjects:@"0.25", @"0.5", @"0.75", @"1", @"1.25", @"1.5", @"1.75", @"2", nil];
    NSArray *o_a_codecs = [NSArray arrayWithObjects:@"mpga", @"mp3 ", @"mp4a", @"a52 ", @"vorb", @"flac", @"spx ", nil];
    NSArray *o_v_codecs = [NSArray arrayWithObjects:@"mp1v", @"mp2v", @"mp4v", @"DIV1",
        @"DIV2", @"DIV3", @"h263", @"h264", @"WMV1", @"WMV2", @"MJPG", @"theo", nil];

    [o_output_ckbox setTitle: _NS("Streaming/Saving:")];
    [o_output_settings setTitle: _NS("Settings...")];
    [o_btn_ok setTitle: _NS("OK")];

    [o_options_lbl setTitle: _NS("Streaming and Transcoding Options")];
    [o_display setTitle: _NS("Display the stream locally")];
    [[o_method cellAtRow:0 column:0] setTitle: _NS("File")];
    [[o_method cellAtRow:1 column:0] setTitle: _NS("Stream")];
    [o_dump_chkbox setTitle: _NS("Dump raw input")];
    [o_btn_browse setTitle: _NS("Browse...")];
    [o_stream_address_lbl setStringValue: _NS("Address")];
    [o_stream_port_lbl setStringValue: _NS("Port")];
    [o_stream_ttl_lbl setStringValue: @"TTL"];
    [[o_stream_type itemAtIndex: 0] setTitle: @"HTTP"];
    [[o_stream_type itemAtIndex: 1] setTitle: @"MMSH"];
    [[o_stream_type itemAtIndex: 2] setTitle: @"UDP"];
    [[o_stream_type itemAtIndex: 3] setTitle: @"RTP"];
    [o_stream_type_lbl setStringValue: _NS("Type")];

    [o_mux_lbl setStringValue: _NS("Encapsulation Method")];
    [o_mux_selector removeAllItems];
    [o_mux_selector addItemsWithTitles: o_muxers];

    [o_transcode_lbl setTitle: _NS("Transcoding options")];
    [o_transcode_video_chkbox setTitle: _NS("Video")];
    [o_transcode_video_selector removeAllItems];
    [o_transcode_video_selector addItemsWithTitles: o_v_codecs];
    [o_transcode_video_bitrate_lbl setStringValue: _NS("Bitrate (kb/s)")];
    [o_transcode_video_bitrate removeAllItems];
    [o_transcode_video_bitrate addItemsWithObjectValues: o_v_bitrates];
    [o_transcode_video_scale_lbl setStringValue: _NS("Scale")];
    [o_transcode_video_scale removeAllItems];
    [o_transcode_video_scale addItemsWithObjectValues: o_v_scales];
    [o_transcode_video_scale selectItemWithObjectValue: @"1"];
    [o_transcode_audio_chkbox setTitle: _NS("Audio")];
    [o_transcode_audio_selector removeAllItems];
    [o_transcode_audio_selector addItemsWithTitles: o_a_codecs];
    [o_transcode_audio_bitrate_lbl setStringValue: _NS("Bitrate (kb/s)")];
    [o_transcode_audio_bitrate removeAllItems];
    [o_transcode_audio_bitrate addItemsWithObjectValues: o_a_bitrates];
    [o_transcode_audio_channels_lbl setStringValue: _NS("Channels")];
    [o_transcode_audio_channels removeAllItems];
    [o_transcode_audio_channels addItemsWithObjectValues: o_a_channels];

    [o_misc_lbl setTitle: _NS("Stream Announcing")];
    [o_sap_chkbox setTitle: _NS("SAP Announcement")];
    [o_rtsp_chkbox setTitle: _NS("RTSP Announcement")];
    [o_http_chkbox setTitle:_NS("HTTP Announcement")];
    [o_file_chkbox setTitle:_NS("Export SDP as file")];

    [o_channel_name_lbl setStringValue: _NS("Channel Name")];
    [o_sdp_url_lbl setStringValue: _NS("SDP URL")];
}

- (IBAction)outputChanged:(id)sender;
{
    if ([o_output_ckbox state] == NSOnState)
        [o_output_settings setEnabled:YES];
    else
        [o_output_settings setEnabled:NO];
}

- (IBAction)outputSettings:(id)sender
{
    [NSApp beginSheet: o_output_sheet
        modalForWindow: o_open_panel
        modalDelegate: self
        didEndSelector: NULL
        contextInfo: nil];
}

- (IBAction)outputCloseSheet:(id)sender
{
    [o_output_sheet orderOut:sender];
    [NSApp endSheet: o_output_sheet];
}

- (void)outputMethodChanged:(NSNotification *)o_notification
{
    NSString *o_mode;
    o_mode = [[o_method selectedCell] title];

    [o_sap_chkbox setEnabled: NO];
    [o_http_chkbox setEnabled: NO];
    [o_rtsp_chkbox setEnabled: NO];
    [o_file_chkbox setEnabled: NO];
    [o_channel_name setEnabled: NO];
    [o_sdp_url setEnabled: NO];
    [[o_mux_selector itemAtIndex: 0] setEnabled: YES];

    if ([o_mode isEqualToString: _NS("File")]) {
        [o_file_field setEnabled: YES];
        [o_btn_browse setEnabled: YES];
        [o_dump_chkbox setEnabled: YES];
        [o_stream_address setEnabled: NO];
        [o_stream_port setEnabled: NO];
        [o_stream_ttl setEnabled: NO];
        [o_stream_port_stp setEnabled: NO];
        [o_stream_ttl_stp setEnabled: NO];
        [o_stream_type setEnabled: NO];
        [o_mux_selector setEnabled: YES];
        [[o_mux_selector itemAtIndex: 1] setEnabled: YES]; // MPEG PS
        [[o_mux_selector itemAtIndex: 2] setEnabled: YES]; // MPEG 1
        [[o_mux_selector itemAtIndex: 3] setEnabled: YES]; // Ogg
        [[o_mux_selector itemAtIndex: 4] setEnabled: YES]; // AVI
        [[o_mux_selector itemAtIndex: 5] setEnabled: YES]; // ASF
        [[o_mux_selector itemAtIndex: 6] setEnabled: YES]; // MPEG 4
        [[o_mux_selector itemAtIndex: 7] setEnabled: YES]; // QuickTime
        [[o_mux_selector itemAtIndex: 8] setEnabled: YES]; // Raw
    } else if ([o_mode isEqualToString: _NS("Stream")]) {
        [o_file_field setEnabled: NO];
        [o_dump_chkbox setEnabled: NO];
        [o_btn_browse setEnabled: NO];
        [o_stream_port setEnabled: YES];
        [o_stream_port_stp setEnabled: YES];
        [o_stream_type setEnabled: YES];
        [o_mux_selector setEnabled: YES];

        o_mode = [o_stream_type titleOfSelectedItem];

        if ([o_mode isEqualToString: @"HTTP"]) {
            [o_stream_address setEnabled: YES];
            [o_stream_ttl setEnabled: NO];
            [o_stream_ttl_stp setEnabled: NO];
            [[o_mux_selector itemAtIndex: 1] setEnabled: YES];
            [[o_mux_selector itemAtIndex: 2] setEnabled: YES];
            [[o_mux_selector itemAtIndex: 3] setEnabled: YES];
            [[o_mux_selector itemAtIndex: 4] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 5] setEnabled: YES];
            [[o_mux_selector itemAtIndex: 6] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 7] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 8] setEnabled: YES];
        } else if ([o_mode isEqualToString: @"MMSH"]) {
            [o_stream_address setEnabled: YES];
            [o_stream_ttl setEnabled: NO];
            [o_stream_ttl_stp setEnabled: NO];
            [[o_mux_selector itemAtIndex: 0] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 1] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 2] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 3] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 4] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 5] setEnabled: YES];
            [[o_mux_selector itemAtIndex: 6] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 7] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 8] setEnabled: NO];
            [o_mux_selector selectItemAtIndex: 5];
        } else if ([o_mode isEqualToString: @"UDP"]) {
            [o_stream_address setEnabled: YES];
            [o_stream_ttl setEnabled: YES];
            [o_stream_ttl_stp setEnabled: YES];
            [[o_mux_selector itemAtIndex: 1] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 2] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 3] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 4] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 5] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 6] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 7] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 8] setEnabled: YES];
            [o_sap_chkbox setEnabled: YES];
            [o_channel_name setEnabled: YES];
        } else if ([o_mode isEqualToString: @"RTP"]) {
            [o_stream_address setEnabled: YES];
            [o_stream_ttl setEnabled: YES];
            [o_stream_ttl_stp setEnabled: YES];
            [[o_mux_selector itemAtIndex: 0] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 1] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 2] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 3] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 4] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 5] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 6] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 7] setEnabled: NO];
            [[o_mux_selector itemAtIndex: 8] setEnabled: YES];
            [o_mux_selector selectItemAtIndex: 8];
            [o_sap_chkbox setEnabled: YES];
            [o_rtsp_chkbox setEnabled: YES];
            [o_http_chkbox setEnabled: YES];
            [o_file_chkbox setEnabled: YES];
            [o_channel_name setEnabled: YES];
        }
    }

    if (![[o_mux_selector selectedItem] isEnabled] && ![o_mode isEqualToString: @"RTP"])
        [o_mux_selector selectItemAtIndex: 0];
    else if (![[o_mux_selector selectedItem] isEnabled] && [o_mode isEqualToString: @"RTP"])
        [o_mux_selector selectItemAtIndex: 8];

    [self outputInfoChanged: nil];
}

- (void)outputInfoChanged:(NSNotification *)o_notification
{
    NSString *o_mode, *o_mux, *o_mux_string;
    NSMutableString *o_announce = [NSMutableString stringWithString:@""];
    NSMutableString *o_mrl_string = [NSMutableString stringWithString:@":sout=#"];
    NSArray *o_sout_options;

    [o_mrl_string appendString: o_transcode];
    if ([o_display state] == NSOnState)
        [o_mrl_string appendString: @"duplicate{dst=display,dst="];

    o_mode = [[o_method selectedCell] title];
    o_mux = [o_mux_selector titleOfSelectedItem];

    if ([o_mux isEqualToString: @"AVI"]) o_mux_string = @"avi";
    else if ([o_mux isEqualToString: @"Ogg"]) o_mux_string = @"ogg";
    else if ([o_mux isEqualToString: @"MPEG PS"]) o_mux_string = @"ps";
    else if ([o_mux isEqualToString: @"MPEG 4"]) o_mux_string = @"mp4";
    else if ([o_mux isEqualToString: @"MPEG 1"]) o_mux_string = @"mpeg1";
    else if ([o_mux isEqualToString: @"Quicktime"]) o_mux_string = @"mov";
    else if ([o_mux isEqualToString: @"ASF"]) o_mux_string = @"asf";
    else if ([o_mux isEqualToString: @"Raw"]) o_mux_string = @"raw";
    else o_mux_string = @"ts";

    if ([o_mode isEqualToString: _NS("File")]) {
        if ([o_dump_chkbox state] == NSOnState) {
            o_sout_options = [NSArray arrayWithObjects:@":demux=dump",
                               [NSString stringWithFormat:
                               @":demuxdump-file=%@",
                               [o_file_field stringValue]], nil];
            [self setSoutMRL:o_sout_options];
            return;
        } else
                [o_mrl_string appendFormat: @"standard{mux=%@,access=file{no-overwrite},dst=\"%@\"}", o_mux_string, [o_file_field stringValue]];
    }
    else if ([o_mode isEqualToString: _NS("Stream")]) {
        o_mode = [o_stream_type titleOfSelectedItem];

        if ([o_mode isEqualToString: @"HTTP"])
            o_mode = @"http";
        else if ([o_mode isEqualToString: @"MMSH"]) {
            if ([o_mux isEqualToString: @"ASF"])
                o_mux_string = @"asfh";
            o_mode = @"mmsh";
        } else if ([o_mode isEqualToString: @"UDP"]) {
            o_mode = @"udp";
            if ([o_sap_chkbox state] == NSOnState) {
                if (![[o_channel_name stringValue] isEqualToString: @""])
                    [o_announce appendFormat:@",sap,name=%@", [o_channel_name stringValue]];
                else
                    [o_announce appendFormat:@",sap"];
            }
        }
        if (![o_mode isEqualToString: @"RTP"]) {
            /* split up the hostname and the following path to paste the
             * port correctly. Not need, if there isn't any path following the
             * hostname. */
            NSArray * o_urlItems = [[o_stream_address stringValue] componentsSeparatedByString: @"/"];
            NSMutableString * o_finalStreamAddress;
            o_finalStreamAddress = [[NSMutableString alloc] init];

            if ([o_urlItems count] == 1)
                [o_finalStreamAddress appendFormat: @"\"%@:%@\"", [o_stream_address stringValue],[o_stream_port stringValue]];
            else {
                [o_finalStreamAddress appendFormat: @"\"%@:%@", [o_urlItems objectAtIndex:0], [o_stream_port stringValue]];
                NSUInteger itemCount = [o_urlItems count];
                for (NSUInteger x = 0; x < itemCount; x++)
                    [o_finalStreamAddress appendFormat: @"/%@", [o_urlItems objectAtIndex:x]];
                [o_finalStreamAddress appendString: @"\""];
            }

            [o_mrl_string appendFormat:
                        @"standard{mux=%@,access=%@,dst=%@%@}",
                        o_mux_string, o_mode, o_finalStreamAddress, o_announce];
        } else {
            NSString * o_stream_name;

            if (![[o_channel_name stringValue] isEqualToString: @""])
                o_stream_name = [NSString stringWithFormat:@",name=%@", [o_channel_name stringValue]];
            else
                o_stream_name = @"";

            if ([o_sap_chkbox state] == NSOnState)
                [o_announce appendString: @",sdp=sap"];

            if ([o_rtsp_chkbox state] == NSOnState)
                [o_announce appendFormat:@",sdp=\"rtsp://%@\"",[o_sdp_url stringValue]];

            if ([o_http_chkbox state] == NSOnState)
                [o_announce appendFormat:@",sdp=\"http://%@\"",[o_sdp_url stringValue]];

            if ([o_file_chkbox state] == NSOnState)
                [o_announce appendFormat:@",sdp=\"file://%@\"",[o_sdp_url stringValue]];

            [o_mrl_string appendFormat:
                        @"rtp{mux=ts,dst=\"%@\",port=%@%@%@}",[o_stream_address stringValue],
                        [o_stream_port stringValue], o_stream_name, o_announce];
        }

    }
    if ([o_display state] == NSOnState)
        [o_mrl_string appendString: @"}"];

    o_sout_options = [NSArray arrayWithObject:o_mrl_string];
    [self setSoutMRL:o_sout_options];
}

- (void)TTLChanged:(NSNotification *)o_notification
{
    intf_thread_t * p_intf = VLCIntf;
    config_PutInt(p_intf, "ttl", [o_stream_ttl intValue]);
}

- (IBAction)outputFileBrowse:(id)sender
{
    NSSavePanel *o_save_panel = [NSSavePanel savePanel];
    NSString *o_mux_string;
    if ([[o_mux_selector titleOfSelectedItem] isEqualToString: @"MPEG PS"])
        o_mux_string = @"vob";
    else if ([[o_mux_selector titleOfSelectedItem] isEqualToString: @"MPEG 1"])
        o_mux_string = @"mpg";
    else if ([[o_mux_selector titleOfSelectedItem] isEqualToString: @"AVI"])
        o_mux_string = @"avi";
    else if ([[o_mux_selector titleOfSelectedItem] isEqualToString: @"ASF"])
        o_mux_string = @"asf";
    else if ([[o_mux_selector titleOfSelectedItem] isEqualToString: @"Ogg"])
        o_mux_string = @"ogm";
    else if ([[o_mux_selector titleOfSelectedItem] isEqualToString: @"MPEG 4"])
        o_mux_string = @"mp4";
    else if ([[o_mux_selector titleOfSelectedItem] isEqualToString: @"Quicktime"])
        o_mux_string = @"mov";
    else if ([[o_mux_selector titleOfSelectedItem] isEqualToString: @"Raw"])
        o_mux_string = @"raw";
    else
        o_mux_string = @"ts";

    NSString * o_name = [NSString stringWithFormat: @"vlc-output.%@",
                         o_mux_string];

    [o_save_panel setTitle: _NS("Save File")];
    [o_save_panel setPrompt: _NS("Save")];
    [o_save_panel setNameFieldStringValue: o_name];

    if ([o_save_panel runModal] == NSFileHandlingPanelOKButton) {
        [o_file_field setStringValue: [[o_save_panel URL] path]];
        [self outputInfoChanged: nil];
    }
}

- (IBAction)streamPortStepperChanged:(id)sender
{
    [o_stream_port setIntValue: [o_stream_port_stp intValue]];
    [self outputInfoChanged: nil];
}

- (IBAction)streamTTLStepperChanged:(id)sender
{
    [o_stream_ttl setIntValue: [o_stream_ttl_stp intValue]];
    [self TTLChanged:nil];
}

- (void)transcodeChanged:(NSNotification *)o_notification
{
    if ([o_transcode_video_chkbox state] == NSOnState) {
        [o_transcode_video_selector setEnabled: YES];
        [o_transcode_video_bitrate setEnabled: YES];
        [o_transcode_video_scale setEnabled: YES];
    } else {
        [o_transcode_video_selector setEnabled: NO];
        [o_transcode_video_bitrate setEnabled: NO];
        [o_transcode_video_scale setEnabled: NO];
    }
    if ([o_transcode_audio_chkbox state] == NSOnState) {
        [o_transcode_audio_selector setEnabled: YES];
        [o_transcode_audio_bitrate setEnabled: YES];
        [o_transcode_audio_channels setEnabled: YES];
    } else {
        [o_transcode_audio_selector setEnabled: NO];
        [o_transcode_audio_bitrate setEnabled: NO];
        [o_transcode_audio_channels setEnabled: NO];
    }

    [self transcodeInfoChanged:nil];
}

- (void)transcodeInfoChanged:(NSNotification *)o_notification
{
    NSMutableString *o_transcode_string = [NSMutableString stringWithCapacity:200];

    if ([o_transcode_video_chkbox state] == NSOnState ||
        [o_transcode_audio_chkbox state] == NSOnState) {
        [o_transcode_string appendString:@"transcode{"];
        if ([o_transcode_video_chkbox state] == NSOnState) {
            [o_transcode_string appendFormat: @"vcodec=\"%@\",vb=\"%@\"" \
                                                            ",scale=\"%@\"",
                [o_transcode_video_selector titleOfSelectedItem],
                [o_transcode_video_bitrate stringValue],
                [o_transcode_video_scale stringValue]];
            if ([o_transcode_audio_chkbox state] == NSOnState)
                [o_transcode_string appendString: @","];
        }
        if ([o_transcode_audio_chkbox state] == NSOnState) {
            [o_transcode_string appendFormat: @"acodec=\"%@\",ab=\"%@\"",
                [o_transcode_audio_selector titleOfSelectedItem],
                [o_transcode_audio_bitrate stringValue]];
            if (![[o_transcode_audio_channels stringValue] isEqualToString: @""])
                [o_transcode_string appendFormat: @",channels=\"%@\"", [o_transcode_audio_channels stringValue]];
        }
        [o_transcode_string appendString:@"}:"];
    }
    else
        [o_transcode_string setString: @""];

    [self setTranscode: o_transcode_string];
    [self outputInfoChanged:nil];
}

- (IBAction)announceChanged:(id)sender
{
    NSString *o_mode;
    o_mode = [[o_stream_type selectedCell] title];
    [o_channel_name setEnabled: [o_sap_chkbox state] ||
                [o_mode isEqualToString: @"RTP"]];

    if ([o_mode isEqualToString: @"RTP"]) {
/*        if ([[sender title] isEqualToString: _NS("SAP Announcement")]) {
            [o_rtsp_chkbox setState:NSOffState];
            [o_http_chkbox setState:NSOffState];
        }*/
        if ([[sender title] isEqualToString:_NS("RTSP Announcement")]) {
//            [o_sap_chkbox setState:NSOffState];
            [o_http_chkbox setState:NSOffState];
            [o_file_chkbox setState:NSOffState];
        } else if ([[sender title] isEqualToString:_NS("HTTP Announcement")]) {
//            [o_sap_chkbox setState:NSOffState];
            [o_rtsp_chkbox setState:NSOffState];
            [o_file_chkbox setState:NSOffState];
        } else if ([[sender title] isEqualToString:_NS("Export SDP as file")]) {
            [o_rtsp_chkbox setState:NSOffState];
            [o_http_chkbox setState:NSOffState];
        }

        if ([o_rtsp_chkbox state] == NSOnState ||
            [o_http_chkbox state] == NSOnState ||
            [o_file_chkbox state] == NSOnState)
            [o_sdp_url setEnabled: YES];
        else
            [o_sdp_url setEnabled: NO];
    }
    [self outputInfoChanged: nil];
}

@end
