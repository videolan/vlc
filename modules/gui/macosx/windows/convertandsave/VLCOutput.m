/*****************************************************************************
 * VLCOutput.m: MacOS X Output Dialog
 *****************************************************************************
 * Copyright (C) 2002-2015 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Benjamin Pracht <bigben AT videolan DOT org>
 *          Felix Paul Kühne <fkuehne # videolan org>
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

#import "VLCOutput.h"

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"

@interface VLCOutput()
{
    NSString *_transcode;
    NSArray *_soutMRL;
}
@end

@implementation VLCOutput

- (NSArray *)soutMRL
{
    return _soutMRL;
}

- (void)awakeFromNib
{
    NSArray *muxers = [NSArray arrayWithObjects:@"MPEG TS", @"MPEG PS", @"MPEG 1",
                         @"Ogg", @"AVI", @"ASF", @"MPEG 4", @"Quicktime", @"Raw", nil];
    NSArray *a_channels = [NSArray arrayWithObjects:@"1", @"2", @"4", @"6", nil];
    NSArray *a_bitrates = [NSArray arrayWithObjects:@"16", @"32", @"64", @"96",
                             @"128", @"192", @"256", @"512", nil];
    NSArray *v_bitrates = [NSArray arrayWithObjects:@"16", @"32", @"64", @"96",
                             @"128", @"192", @"256", @"384", @"512", @"768", @"1024", @"2048", @"3072", nil];
    NSArray *v_scales = [NSArray arrayWithObjects:@"0.25", @"0.5", @"0.75", @"1", @"1.25", @"1.5", @"1.75", @"2", nil];
    NSArray *a_codecs = [NSArray arrayWithObjects:@"mpga", @"mp3 ", @"mp4a", @"a52 ", @"vorb", @"flac", @"spx ", nil];
    NSArray *v_codecs = [NSArray arrayWithObjects:@"mp1v", @"mp2v", @"mp4v", @"DIV1",
                           @"DIV2", @"DIV3", @"h263", @"h264", @"WMV1", @"WMV2", @"MJPG", @"theo", nil];

    [_okButton setTitle: _NS("OK")];
    [_optionsBox setTitle: _NS("Streaming and Transcoding Options")];

    [_displayOnLocalScreenCheckbox setTitle: _NS("Display the stream locally")];
    [[_outputMethodMatrix cellAtRow:0 column:0] setTitle: _NS("File")];
    [[_outputMethodMatrix cellAtRow:1 column:0] setTitle: _NS("Stream")];
    [_dumpCheckbox setTitle: _NS("Dump raw input")];
    [_browseButton setTitle: _NS("Browse...")];
    [_streamAddressLabel setStringValue: _NS("Address")];
    [_streamPortLabel setStringValue: _NS("Port")];
    [_streamTTLLabel setStringValue: @"TTL"];
    [[_streamTypePopup itemAtIndex: 0] setTitle: @"HTTP"];
    [[_streamTypePopup itemAtIndex: 1] setTitle: @"MMSH"];
    [[_streamTypePopup itemAtIndex: 2] setTitle: @"UDP"];
    [[_streamTypePopup itemAtIndex: 3] setTitle: @"RTP"];
    [_streamTypeLabel setStringValue: _NS("Type")];

    [_muxLabel setStringValue: _NS("Encapsulation Method")];
    [_muxSelectorPopup removeAllItems];
    [_muxSelectorPopup addItemsWithTitles: muxers];

    [_transcodeBox setTitle: _NS("Transcoding options")];
    [_transcodeVideoCheckbox setTitle: _NS("Video")];
    [_transcodeVideoSelectorPopup removeAllItems];
    [_transcodeVideoSelectorPopup addItemsWithTitles: v_codecs];
    [_transcodeVideoBitrateLabel setStringValue: _NS("Bitrate (kb/s)")];
    [_transcodeVideoBitrateComboBox removeAllItems];
    [_transcodeVideoBitrateComboBox addItemsWithObjectValues: v_bitrates];
    [_transcodeVideoScaleLabel setStringValue: _NS("Scale")];
    [_transcodeVideoScaleComboBox removeAllItems];
    [_transcodeVideoScaleComboBox addItemsWithObjectValues: v_scales];
    [_transcodeVideoScaleComboBox selectItemWithObjectValue: @"1"];
    [_transcodeAudioCheckbox setTitle: _NS("Audio")];
    [_transcodeAudioSelectorPopup removeAllItems];
    [_transcodeAudioSelectorPopup addItemsWithTitles: a_codecs];
    [_transcodeAudioBitrateLabel setStringValue: _NS("Bitrate (kb/s)")];
    [_transcodeAudioBitrateComboBox removeAllItems];
    [_transcodeAudioBitrateComboBox addItemsWithObjectValues: a_bitrates];
    [_transcodeAudioChannelsLabel setStringValue: _NS("Channels")];
    [_transcodeAudioChannelsComboBox removeAllItems];
    [_transcodeAudioChannelsComboBox addItemsWithObjectValues: a_channels];

    [_miscBox setTitle: _NS("Stream Announcing")];
    [_sapCheckbox setTitle: _NS("SAP Announcement")];
    [_rtspCheckbox setTitle: _NS("RTSP Announcement")];
    [_httpCheckbox setTitle:_NS("HTTP Announcement")];
    [_fileCheckbox setTitle:_NS("Export SDP as file")];

    [_channelNameLabel setStringValue: _NS("Channel Name")];
    [_sdpURLLabel setStringValue: _NS("SDP URL")];

    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(outputInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: _fileTextField];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(outputInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: _streamAddressTextField];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(outputInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: _streamPortTextField];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(TTLChanged:)
        name: NSControlTextDidChangeNotification
        object: _streamTTLTextField];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: _transcodeVideoBitrateComboBox];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: _transcodeVideoScaleComboBox];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: _transcodeAudioBitrateComboBox];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: _transcodeAudioChannelsComboBox];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: _channelNameTextField];
    [[NSNotificationCenter defaultCenter] addObserver: self
        selector: @selector(transcodeInfoChanged:)
        name: NSControlTextDidChangeNotification
        object: _sdpURLTextField];

    [_muxSelectorPopup setAutoenablesItems: NO];
    [self transcodeChanged:nil];
}

- (IBAction)outputCloseSheet:(id)sender
{
    [self.outputSheet orderOut:sender];
    [NSApp endSheet:self.outputSheet];
}

- (IBAction)outputMethodChanged:(id)sender
{
    NSString *mode;
    mode = [[self.outputMethodMatrix selectedCell] title];

    [self.sapCheckbox setEnabled: NO];
    [self.httpCheckbox setEnabled: NO];
    [self.rtspCheckbox setEnabled: NO];
    [self.fileCheckbox setEnabled: NO];
    [self.channelNameTextField setEnabled: NO];
    [self.sdpURLTextField setEnabled: NO];
    [[self.muxSelectorPopup itemAtIndex: 0] setEnabled: YES];

    if ([mode isEqualToString: _NS("File")]) {
        [self.fileTextField setEnabled: YES];
        [self.browseButton setEnabled: YES];
        [self.dumpCheckbox setEnabled: YES];
        [self.streamAddressTextField setEnabled: NO];
        [self.streamPortTextField setEnabled: NO];
        [self.streamTTLTextField setEnabled: NO];
        [self.streamPortStepper setEnabled: NO];
        [self.streamTTLStepper setEnabled: NO];
        [self.streamTypePopup setEnabled: NO];
        [self.muxSelectorPopup setEnabled: YES];
        [[self.muxSelectorPopup itemAtIndex: 1] setEnabled: YES]; // MPEG PS
        [[self.muxSelectorPopup itemAtIndex: 2] setEnabled: YES]; // MPEG 1
        [[self.muxSelectorPopup itemAtIndex: 3] setEnabled: YES]; // Ogg
        [[self.muxSelectorPopup itemAtIndex: 4] setEnabled: YES]; // AVI
        [[self.muxSelectorPopup itemAtIndex: 5] setEnabled: YES]; // ASF
        [[self.muxSelectorPopup itemAtIndex: 6] setEnabled: YES]; // MPEG 4
        [[self.muxSelectorPopup itemAtIndex: 7] setEnabled: YES]; // QuickTime
        [[self.muxSelectorPopup itemAtIndex: 8] setEnabled: YES]; // Raw
    } else if ([mode isEqualToString: _NS("Stream")]) {
        [self.fileTextField setEnabled: NO];
        [self.dumpCheckbox setEnabled: NO];
        [self.browseButton setEnabled: NO];
        [self.streamPortTextField setEnabled: YES];
        [self.streamPortStepper setEnabled: YES];
        [self.streamTypePopup setEnabled: YES];
        [self.muxSelectorPopup setEnabled: YES];

        mode = [self.streamTypePopup titleOfSelectedItem];

        if ([mode isEqualToString: @"HTTP"]) {
            [self.streamAddressTextField setEnabled: YES];
            [self.streamTTLTextField setEnabled: NO];
            [self.streamTTLStepper setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 1] setEnabled: YES];
            [[self.muxSelectorPopup itemAtIndex: 2] setEnabled: YES];
            [[self.muxSelectorPopup itemAtIndex: 3] setEnabled: YES];
            [[self.muxSelectorPopup itemAtIndex: 4] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 5] setEnabled: YES];
            [[self.muxSelectorPopup itemAtIndex: 6] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 7] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 8] setEnabled: YES];
        } else if ([mode isEqualToString: @"MMSH"]) {
            [self.streamAddressTextField setEnabled: YES];
            [self.streamTTLTextField setEnabled: NO];
            [self.streamTTLStepper setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 0] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 1] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 2] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 3] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 4] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 5] setEnabled: YES];
            [[self.muxSelectorPopup itemAtIndex: 6] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 7] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 8] setEnabled: NO];
            [self.muxSelectorPopup selectItemAtIndex: 5];
        } else if ([mode isEqualToString: @"UDP"]) {
            [self.streamAddressTextField setEnabled: YES];
            [self.streamTTLTextField setEnabled: YES];
            [self.streamTTLStepper setEnabled: YES];
            [[self.muxSelectorPopup itemAtIndex: 1] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 2] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 3] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 4] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 5] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 6] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 7] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 8] setEnabled: YES];
            [self.sapCheckbox setEnabled: YES];
            [self.channelNameTextField setEnabled: YES];
        } else if ([mode isEqualToString: @"RTP"]) {
            [self.streamAddressTextField setEnabled: YES];
            [self.streamTTLTextField setEnabled: YES];
            [self.streamTTLStepper setEnabled: YES];
            [[self.muxSelectorPopup itemAtIndex: 0] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 1] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 2] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 3] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 4] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 5] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 6] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 7] setEnabled: NO];
            [[self.muxSelectorPopup itemAtIndex: 8] setEnabled: YES];
            [self.muxSelectorPopup selectItemAtIndex: 8];
            [self.sapCheckbox setEnabled: YES];
            [self.rtspCheckbox setEnabled: YES];
            [self.httpCheckbox setEnabled: YES];
            [self.fileCheckbox setEnabled: YES];
            [self.channelNameTextField setEnabled: YES];
        }
    }

    if (![[self.muxSelectorPopup selectedItem] isEnabled] && ![mode isEqualToString: @"RTP"])
        [self.muxSelectorPopup selectItemAtIndex: 0];
    else if (![[self.muxSelectorPopup selectedItem] isEnabled] && [mode isEqualToString: @"RTP"])
        [self.muxSelectorPopup selectItemAtIndex: 8];

    [self outputInfoChanged: nil];
}

- (IBAction)outputInfoChanged:(id)object
{
    NSString *mode, *mux, *mux_string;
    NSMutableString *announce = [NSMutableString stringWithString:@""];
    NSMutableString *mrl_string = [NSMutableString stringWithString:@":sout=#"];

    [mrl_string appendString: _transcode];
    if ([self.displayOnLocalScreenCheckbox state] == NSOnState)
        [mrl_string appendString: @"duplicate{dst=display,dst="];

    mode = [[self.outputMethodMatrix selectedCell] title];
    mux = [self.muxSelectorPopup titleOfSelectedItem];

    if ([mux isEqualToString: @"AVI"]) mux_string = @"avi";
    else if ([mux isEqualToString: @"Ogg"]) mux_string = @"ogg";
    else if ([mux isEqualToString: @"MPEG PS"]) mux_string = @"ps";
    else if ([mux isEqualToString: @"MPEG 4"]) mux_string = @"mp4";
    else if ([mux isEqualToString: @"MPEG 1"]) mux_string = @"mpeg1";
    else if ([mux isEqualToString: @"Quicktime"]) mux_string = @"mov";
    else if ([mux isEqualToString: @"ASF"]) mux_string = @"asf";
    else if ([mux isEqualToString: @"Raw"]) mux_string = @"raw";
    else mux_string = @"ts";

    NSString *filename_string =
            [[self.fileTextField stringValue] stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];

    if ([mode isEqualToString: _NS("File")]) {
        if ([self.dumpCheckbox state] == NSOnState) {
            _soutMRL = [NSArray arrayWithObjects:@":demux=dump",
                        [NSString stringWithFormat:
                        @":demuxdump-file=\"%@\"",
                        filename_string], nil];
            return;
        } else
            [mrl_string appendFormat:@"standard{mux=%@,access=file{no-overwrite},dst=\"%@\"}",
             mux_string,
             filename_string];
    }
    else if ([mode isEqualToString: _NS("Stream")]) {
        mode = [self.streamTypePopup titleOfSelectedItem];

        if ([mode isEqualToString: @"HTTP"])
            mode = @"http";
        else if ([mode isEqualToString: @"MMSH"]) {
            if ([mux isEqualToString: @"ASF"])
                mux_string = @"asfh";
            mode = @"mmsh";
        } else if ([mode isEqualToString: @"UDP"]) {
            mode = @"udp";
            if ([self.sapCheckbox state] == NSOnState) {
                if (![[self.channelNameTextField stringValue] isEqualToString: @""])
                    [announce appendFormat:@",sap,name=%@", [self.channelNameTextField stringValue]];
                else
                    [announce appendFormat:@",sap"];
            }
        }
        if (![mode isEqualToString: @"RTP"]) {
            /* split up the hostname and the following path to paste the
             * port correctly. Not need, if there isn't any path following the
             * hostname. */
            NSArray *urlItems = [[self.streamAddressTextField stringValue] componentsSeparatedByString: @"/"];
            NSMutableString *finalStreamAddress = [[NSMutableString alloc] init];

            if ([urlItems count] == 1)
                [finalStreamAddress appendFormat: @"\"%@:%@\"", [self.streamAddressTextField stringValue],[self.streamPortTextField stringValue]];
            else {
                [finalStreamAddress appendFormat: @"\"%@:%@", [urlItems firstObject], [self.streamPortTextField stringValue]];
                NSUInteger itemCount = [urlItems count];
                for (NSUInteger x = 0; x < itemCount; x++)
                    [finalStreamAddress appendFormat: @"/%@", [urlItems objectAtIndex:x]];
                [finalStreamAddress appendString: @"\""];
            }

            [mrl_string appendFormat:
                        @"standard{mux=%@,access=%@,dst=%@%@}",
                        mux_string, mode, finalStreamAddress, announce];
        } else {
            NSString *stream_name;

            if (![[self.channelNameTextField stringValue] isEqualToString: @""])
                stream_name = [NSString stringWithFormat:@",name=%@", [self.channelNameTextField stringValue]];
            else
                stream_name = @"";

            if ([self.sapCheckbox state] == NSOnState)
                [announce appendString: @",sdp=sap"];

            if ([self.rtspCheckbox state] == NSOnState)
                [announce appendFormat:@",sdp=\"rtsp://%@\"",[self.sdpURLTextField stringValue]];

            if ([self.httpCheckbox state] == NSOnState)
                [announce appendFormat:@",sdp=\"http://%@\"",[self.sdpURLTextField stringValue]];

            if ([self.fileCheckbox state] == NSOnState)
                [announce appendFormat:@",sdp=\"file://%@\"",[self.sdpURLTextField stringValue]];

            [mrl_string appendFormat:
                        @"rtp{mux=ts,dst=\"%@\",port=%@%@%@}", [self.streamAddressTextField stringValue],
                        [self.streamPortTextField stringValue], stream_name, announce];
        }

    }
    if ([self.displayOnLocalScreenCheckbox state] == NSOnState)
        [mrl_string appendString: @"}"];

    _soutMRL = [NSArray arrayWithObject:mrl_string];
}

- (void)TTLChanged:(NSNotification *)notification
{
    config_PutInt("ttl", [self.streamTTLTextField intValue]);
}

- (IBAction)outputFileBrowse:(id)sender
{
    NSString *mux_string;
    if ([[self.muxSelectorPopup titleOfSelectedItem] isEqualToString: @"MPEG PS"])
        mux_string = @"vob";
    else if ([[self.muxSelectorPopup titleOfSelectedItem] isEqualToString: @"MPEG 1"])
        mux_string = @"mpg";
    else if ([[self.muxSelectorPopup titleOfSelectedItem] isEqualToString: @"AVI"])
        mux_string = @"avi";
    else if ([[self.muxSelectorPopup titleOfSelectedItem] isEqualToString: @"ASF"])
        mux_string = @"asf";
    else if ([[self.muxSelectorPopup titleOfSelectedItem] isEqualToString: @"Ogg"])
        mux_string = @"ogm";
    else if ([[self.muxSelectorPopup titleOfSelectedItem] isEqualToString: @"MPEG 4"])
        mux_string = @"mp4";
    else if ([[self.muxSelectorPopup titleOfSelectedItem] isEqualToString: @"Quicktime"])
        mux_string = @"mov";
    else if ([[self.muxSelectorPopup titleOfSelectedItem] isEqualToString: @"Raw"])
        mux_string = @"raw";
    else
        mux_string = @"ts";

    NSString *name = [NSString stringWithFormat: @"vlc-output.%@", mux_string];

    NSSavePanel *save_panel = [NSSavePanel savePanel];
    [save_panel setTitle: _NS("Save File")];
    [save_panel setPrompt: _NS("Save")];
    [save_panel setNameFieldStringValue: name];

    if ([save_panel runModal] == NSFileHandlingPanelOKButton) {
        [self.fileTextField setStringValue: [[save_panel URL] path]];
        [self outputInfoChanged: nil];
    }
}

- (IBAction)streamPortStepperChanged:(id)sender
{
    [self.streamPortTextField setIntValue:[self.streamPortStepper intValue]];
    [self outputInfoChanged: nil];
}

- (IBAction)streamTTLStepperChanged:(id)sender
{
    [self.streamTTLTextField setIntValue:[self.streamTTLStepper intValue]];
    [self TTLChanged:nil];
}

- (IBAction)transcodeChanged:(id)sender
{
    if ([self.transcodeVideoCheckbox state] == NSOnState) {
        [self.transcodeVideoSelectorPopup setEnabled: YES];
        [self.transcodeVideoBitrateComboBox setEnabled: YES];
        [self.transcodeVideoScaleComboBox setEnabled: YES];
    } else {
        [self.transcodeVideoSelectorPopup setEnabled: NO];
        [self.transcodeVideoBitrateComboBox setEnabled: NO];
        [self.transcodeVideoScaleComboBox setEnabled: NO];
    }
    if ([self.transcodeAudioCheckbox state] == NSOnState) {
        [self.transcodeAudioSelectorPopup setEnabled: YES];
        [self.transcodeAudioBitrateComboBox setEnabled: YES];
        [self.transcodeAudioChannelsComboBox setEnabled: YES];
    } else {
        [self.transcodeAudioSelectorPopup setEnabled: NO];
        [self.transcodeAudioBitrateComboBox setEnabled: NO];
        [self.transcodeAudioChannelsComboBox setEnabled: NO];
    }

    [self transcodeInfoChanged:nil];
}

- (IBAction)transcodeInfoChanged:(id)object
{
    NSMutableString *transcode_string = [NSMutableString stringWithCapacity:200];

    if ([self.transcodeVideoCheckbox state] == NSOnState ||
        [self.transcodeAudioCheckbox state] == NSOnState) {
        [transcode_string appendString:@"transcode{"];
        if ([self.transcodeVideoCheckbox state] == NSOnState) {
            [transcode_string appendFormat: @"vcodec=\"%@\",vb=\"%@\"" \
                                                            ",scale=\"%@\"",
                [self.transcodeVideoSelectorPopup titleOfSelectedItem],
                [self.transcodeVideoBitrateComboBox stringValue],
                [self.transcodeVideoScaleComboBox stringValue]];
            if ([self.transcodeAudioCheckbox state] == NSOnState)
                [transcode_string appendString: @","];
        }
        if ([self.transcodeAudioCheckbox state] == NSOnState) {
            [transcode_string appendFormat: @"acodec=\"%@\",ab=\"%@\"",
                [self.transcodeAudioSelectorPopup titleOfSelectedItem],
                [self.transcodeAudioBitrateComboBox stringValue]];
            if (![[self.transcodeAudioChannelsComboBox stringValue] isEqualToString: @""])
                [transcode_string appendFormat: @",channels=\"%@\"", [self.transcodeAudioChannelsComboBox stringValue]];
        }
        [transcode_string appendString:@"}:"];
    }
    else
        [transcode_string setString: @""];

    _transcode = [NSString stringWithString:transcode_string];
    [self outputInfoChanged:nil];
}

- (IBAction)announceChanged:(id)sender
{
    NSString *mode;
    mode = [[self.streamTypePopup selectedCell] title];
    [self.channelNameTextField setEnabled:[self.sapCheckbox state] || [mode isEqualToString: @"RTP"]];

    if ([mode isEqualToString: @"RTP"]) {
/*        if ([[sender title] isEqualToString: _NS("SAP Announcement")]) {
            [self.rtspCheckbox setState:NSOffState];
            [self.httpCheckbox setState:NSOffState];
        }*/
        if ([[sender title] isEqualToString:_NS("RTSP Announcement")]) {
//            [self.sapCheckbox setState:NSOffState];
            [self.httpCheckbox setState:NSOffState];
            [self.fileCheckbox setState:NSOffState];
        } else if ([[sender title] isEqualToString:_NS("HTTP Announcement")]) {
//            [self.sapCheckbox setState:NSOffState];
            [self.rtspCheckbox setState:NSOffState];
            [self.fileCheckbox setState:NSOffState];
        } else if ([[sender title] isEqualToString:_NS("Export SDP as file")]) {
            [self.rtspCheckbox setState:NSOffState];
            [self.httpCheckbox setState:NSOffState];
        }

        if ([self.rtspCheckbox state] == NSOnState ||
            [self.httpCheckbox state] == NSOnState ||
            [self.fileCheckbox state] == NSOnState)
            [self.sdpURLTextField setEnabled: YES];
        else
            [self.sdpURLTextField setEnabled: NO];
    }
    [self outputInfoChanged: nil];
}

@end
