/*****************************************************************************
 * ConvertAndSave.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012 Felix Paul Kühne
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import "ConvertAndSave.h"
#import "intf.h"
#import "playlist.h"
#import <vlc_common.h>
#import <vlc_url.h>

/* mini doc:
 * the used NSMatrix includes a bunch of cells referenced most easily by tags. There you go: */
#define MPEGTS 0
#define WEBM 1
#define OGG 2
#define MP4 3
#define MPEGPS 4
#define MJPEG 5
#define WAV 6
#define FLV 7
#define MPEG1 8
#define MKV 9
#define RAW 10
#define AVI 11
#define ASF 12
/* 13-15 are present, but not set */

@implementation VLCConvertAndSave

@synthesize MRL=_MRL, outputDestination=_outputDestination, profileNames=_profileNames, profileValueList=_profileValueList, currentProfile=_currentProfile;

static VLCConvertAndSave *_o_sharedInstance = nil;

#pragma mark -
#pragma mark Initialization

+ (void)initialize{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    /* We are using the same format as the Qt4 intf here:
     * Container(string), transcode video(bool), transcode audio(bool),
     * use subtitles(bool), video codec(string), video bitrate(integer),
     * scale(float), fps(float), width(integer, height(integer),
     * audio codec(string), audio bitrate(integer), channels(integer),
     * samplerate(integer), subtitle codec(string), subtitle overlay(bool) */
    NSArray * defaultProfiles = [[NSArray alloc] initWithObjects:
                                 @"mp4;1;1;0;h264;0;0;0;0;0;mpga;128;2;44100;0;1",
                                 @"webm;1;1;0;VP80;2000;0;0;0;0;vorb;128;2;44100;0;1",
                                 @"ts;1;1;0;h264;800;1;0;0;0;mpga;128;2;44100;0;0",
                                 @"ts;1;1;0;drac;800;1;0;0;0;mpga;128;2;44100;0;0",
                                 @"ogg;1;1;0;theo;800;1;0;0;0;vorb;128;2;44100;0;0",
                                 @"ogg;1;1;0;theo;800;1;0;0;0;flac;128;2;44100;0;0",
                                 @"ts;1;1;0;mp2v;800;1;0;0;0;mpga;128;2;44100;0;0",
                                 @"asf;1;1;0;WMV2;800;1;0;0;0;wma2;128;2;44100;0;0",
                                 @"asf;1;1;0;DIV3;800;1;0;0;0;mp3;128;2;44100;0;0",
                                 @"ogg;1;1;0;none;800;1;0;0;0;vorb;128;2;44100;none;0",
                                 @"raw;1;1;0;none;800;1;0;0;0;mp3;128;2;44100;none;0",
                                 @"mp4;1;1;0;none;800;1;0;0;0;mpga;128;2;44100;none;0",
                                 @"raw;1;1;0;none;800;1;0;0;0;flac;128;2;44100;none;0",
                                 @"wav;1;1;0;none;800;1;0;0;0;s16l;128;2;44100;none;0", nil];

    NSArray * defaultProfileNames = [[NSArray alloc] initWithObjects:
                                     @"Video - H.264 + MP3 (MP4)",
                                     @"Video - VP80 + Vorbis (Webm)",
                                     @"Video - H.264 + MP3 (TS)",
                                     @"Video - Dirac + MP3 (TS)",
                                     @"Video - Theora + Vorbis (OGG)",
                                     @"Video - Theora + Flac (OGG)",
                                     @"Video - MPEG-2 + MPGA (TS)",
                                     @"Video - WMV + WMA (ASF)",
                                     @"Video - DIV3 + MP3 (ASF)",
                                     @"Audio - Vorbis (OGG)",
                                     @"Audio - MP3",
                                     @"Audio - MP3 (MP4)",
                                     @"Audio - FLAC",
                                     @"Audio - CD",
                                     nil];

    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:defaultProfiles, @"CASProfiles", defaultProfileNames, @"CASProfileNames", nil];

    [defaults registerDefaults:appDefaults];
    [defaultProfiles release];
}

+ (VLCConvertAndSave *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)dealloc
{
    if (_currentProfile)
        [_currentProfile release];

    [_profileNames release];
    [_profileValueList release];
    [_videoCodecs release];
    [_audioCodecs release];
    [_subsCodecs release];

    [super dealloc];
}

- (void)awakeFromNib
{
    [_window setTitle: _NS("Convert & Save")];
    [_ok_btn setTitle: _NS("Save")];
    [_drop_lbl setStringValue: _NS("Drop media here")];
    [_drop_btn setTitle: _NS("Open media...")];
    [_profile_lbl setStringValue: _NS("Choose Profile")];
    [_profile_btn setTitle: _NS("Customize")];
    [_destination_lbl setStringValue: _NS("Choose Destination")];
    [_destination_filename_stub_lbl setStringValue: _NS("Choose an output location")];
    [_destination_filename_lbl setHidden: YES];
    [_destination_browse_btn setTitle:_NS("Browse...")];
    [_destination_stream_btn setTitle:_NS("Setup Streaming...")];
    [_destination_stream_lbl setStringValue:@"Select Streaming Method"];
    [_destination_itwantafile_btn setTitle:_NS("Save as File")];
    [_destination_itwantastream_btn setTitle:_NS("Stream")];
    [_destination_cancel_btn setHidden:YES];
    [_customize_ok_btn setTitle: _NS("Apply")];
    [_customize_cancel_btn setTitle: _NS("Cancel")];
    [[_customize_tabview tabViewItemAtIndex:0] setLabel: _NS("Encapsulation")];
    [[_customize_tabview tabViewItemAtIndex:1] setLabel: _NS("Video codec")];
    [[_customize_tabview tabViewItemAtIndex:2] setLabel: _NS("Audio codec")];
    [[_customize_tabview tabViewItemAtIndex:3] setLabel: _NS("Subtitles")];
    [_customize_tabview selectTabViewItemAtIndex: 0];
    [_customize_vid_ckb setTitle: _NS("Video")];
    [_customize_vid_keep_ckb setTitle: _NS("Keep original video track")];
    [_customize_vid_codec_lbl setStringValue: _NS("Codec")];
    [_customize_vid_bitrate_lbl setStringValue: _NS("Bitrate")];
    [_customize_vid_framerate_lbl setStringValue: _NS("Frame Rate")];
    [_customize_vid_res_box setTitle: _NS("Resolution")];
    [_customize_vid_res_lbl setStringValue: _NS("You just need to fill one of the three following parameters, VLC will autodetect the other using the original aspect ratio")];
    [_customize_vid_width_lbl setStringValue: _NS("Width")];
    [_customize_vid_height_lbl setStringValue: _NS("Height")];
    [_customize_vid_scale_lbl setStringValue: _NS("Scale")];
    [_customize_aud_ckb setTitle: _NS("Audio")];
    [_customize_aud_keep_ckb setTitle: _NS("Keep original audio track")];
    [_customize_aud_codec_lbl setStringValue: _NS("Codec")];
    [_customize_aud_bitrate_lbl setStringValue: _NS("Bitrate")];
    [_customize_aud_channels_lbl setStringValue: _NS("Channels")];
    [_customize_aud_samplerate_lbl setStringValue: _NS("Sample Rate")];
    [_customize_subs_ckb setTitle: _NS("Subtitles")];
    [_customize_subs_overlay_ckb setTitle: _NS("Overlay subtitles on the video")];
    [_stream_ok_btn setTitle:_NS("Close")];
    [_stream_destination_lbl setStringValue:_NS("Stream Destination")];
    [_stream_announcement_lbl setStringValue:_NS("Stream Announcement")];
    [_stream_type_lbl setStringValue:_NS("Type")];
    [_stream_address_lbl setStringValue:_NS("Address")];
    [_stream_ttl_lbl setStringValue:_NS("TTL")];
    [_stream_ttl_fld setEnabled:NO];
    [_stream_ttl_stepper setEnabled:NO];
    [_stream_port_lbl setStringValue:_NS("Port")];
    [_stream_sap_ckb setStringValue:_NS("SAP Announcement")];
    [_stream_http_ckb setStringValue:_NS("HTTP Announcement")];
    [_stream_rtsp_ckb setStringValue:_NS("RTSP Announcement")];
    [_stream_sdp_ckb setStringValue:_NS("Export SDP as file")];
    [_stream_sap_ckb setState:NSOffState];
    [_stream_http_ckb setState:NSOffState];
    [_stream_rtsp_ckb setState:NSOffState];
    [_stream_sdp_ckb setState:NSOffState];

    /* there is no way to hide single cells, so replace the existing ones with empty cells.. */
    id blankCell = [[[NSCell alloc] init] autorelease];
    [blankCell setEnabled:NO];
    [_customize_encap_matrix putCell:blankCell atRow:3 column:1];
    [_customize_encap_matrix putCell:blankCell atRow:3 column:2];
    [_customize_encap_matrix putCell:blankCell atRow:3 column:3];

    /* fetch profiles from defaults */
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    _profileValueList = [[defaults arrayForKey:@"CASProfiles"] retain];
    _profileNames = [[defaults arrayForKey:@"CASProfileNames"] retain];

    [_profile_pop removeAllItems];
    [_profile_pop addItemsWithTitles:_profileNames];
    [_profile_pop addItemWithTitle:_NS("Custom")];

    _videoCodecs = [[NSArray alloc] initWithObjects:
                    [NSArray arrayWithObjects:@"MPEG-1", @"MPEG-2", @"MPEG-4", @"DIVX 1", @"DIVX 2", @"DIVX 3", @"H.263", @"H.264", @"VP8", @"WMV1", @"WMV2", @"M-JPEG", @"Theora", @"Dirac", nil],
                    [NSArray arrayWithObjects:@"mpgv", @"mp2v", @"mp4v", @"DIV1", @"DIV2", @"DIV3", @"H263", @"h264", @"VP80", @"WMV1", @"WMV2", @"MJPG", @"theo", @"drac", nil],
                    nil];
    _audioCodecs = [[NSArray alloc] initWithObjects:
                    [NSArray arrayWithObjects:@"MPEG Audio", @"MP3", @"MPEG 4 Audio (AAC)", @"A52/AC-3", @"Vorbis", @"Flac", @"Speex", @"WAV", @"WMA2", nil],
                    [NSArray arrayWithObjects:@"mpga", @"mp3", @"mp4a", @"a52", @"vorb", @"flac", @"spx", @"s16l", @"wma2", nil],
                    nil];
    _subsCodecs = [[NSArray alloc] initWithObjects:
                   [NSArray arrayWithObjects:@"DVB subtitle", @"T.140", nil],
                   [NSArray arrayWithObjects:@"dvbs", @"t140", nil],
                   nil];

    [_customize_vid_codec_pop removeAllItems];
    [_customize_vid_scale_pop removeAllItems];
    [_customize_aud_codec_pop removeAllItems];
    [_customize_aud_samplerate_pop removeAllItems];
    [_customize_subs_pop removeAllItems];

    [_customize_vid_codec_pop addItemsWithTitles:[_videoCodecs objectAtIndex:0]];
    [_customize_aud_codec_pop addItemsWithTitles:[_audioCodecs objectAtIndex:0]];
    [_customize_subs_pop addItemsWithTitles:[_subsCodecs objectAtIndex:0]];

    [_customize_aud_samplerate_pop addItemWithTitle:@"8000"];
    [_customize_aud_samplerate_pop addItemWithTitle:@"11025"];
    [_customize_aud_samplerate_pop addItemWithTitle:@"22050"];
    [_customize_aud_samplerate_pop addItemWithTitle:@"44100"];
    [_customize_aud_samplerate_pop addItemWithTitle:@"48000"];

    [_customize_vid_scale_pop addItemWithTitle:@"1"];
    [_customize_vid_scale_pop addItemWithTitle:@"0.25"];
    [_customize_vid_scale_pop addItemWithTitle:@"0.5"];
    [_customize_vid_scale_pop addItemWithTitle:@"0.75"];
    [_customize_vid_scale_pop addItemWithTitle:@"1.25"];
    [_customize_vid_scale_pop addItemWithTitle:@"1.5"];
    [_customize_vid_scale_pop addItemWithTitle:@"1.75"];
    [_customize_vid_scale_pop addItemWithTitle:@"2"];

    [_ok_btn setEnabled: NO];

    [self resetCustomizationSheetBasedOnProfile:[_profileValueList objectAtIndex:0]];
}

# pragma mark -
# pragma mark Code to Communicate with other objects

- (void)toggleWindow
{
    [_window makeKeyAndOrderFront: nil];
}

# pragma mark -
# pragma mark User Interaction

- (IBAction)finalizePanel:(id)sender
{
    playlist_t * p_playlist = pl_Get(VLCIntf);

    input_item_t *p_input = input_item_New([_MRL UTF8String], [[_dropin_media_lbl stringValue] UTF8String]);
    if (!p_input)
        return;

    input_item_AddOption(p_input, [[self composedOptions] UTF8String], VLC_INPUT_OPTION_TRUSTED);

    int returnValue;
    returnValue = playlist_AddInput( p_playlist, p_input, PLAYLIST_STOP, PLAYLIST_END, true, pl_Unlocked );

    if (returnValue == VLC_SUCCESS) {
        /* let's "play" */
        PL_LOCK;
        playlist_item_t *p_item = playlist_ItemGetByInput( p_playlist, p_input );
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, pl_Locked, NULL,
                         p_item );
        PL_UNLOCK;
    }
    else
        msg_Err( VLCIntf, "CAS: playlist add input failed :(");

    /* we're done with this input */
    vlc_gc_decref( p_input );

    [_window performClose:sender];
}

- (IBAction)openMedia:(id)sender
{
    /* preliminary implementation until the open panel is cleaned up */
    NSOpenPanel * openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseDirectories:NO];
    [openPanel setResolvesAliases:YES];
    [openPanel setAllowsMultipleSelection:NO];
    [openPanel beginSheetForDirectory:nil file:nil types:nil modalForWindow: _window modalDelegate:self didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:) contextInfo:nil];
}

- (void)openPanelDidEnd:(NSOpenPanel *)panel returnCode:(int)returnCode  contextInfo:(void  *)contextInfo
{
    if (returnCode == NSOKButton)
    {
        [self setMRL: [NSString stringWithUTF8String:make_URI([[[panel URL] path] UTF8String], NULL)]];
        [self updateOKButton];
        [self updateDropView];
    }
}

- (IBAction)switchProfile:(id)sender
{
    NSUInteger index = [_profile_pop indexOfSelectedItem];
    if (index < ([_profileValueList count] - 1))
        [self resetCustomizationSheetBasedOnProfile:[_profileValueList objectAtIndex:index]];
}

- (IBAction)customizeProfile:(id)sender
{
    [NSApp beginSheet:_customize_panel modalForWindow:_window modalDelegate:self didEndSelector:NULL contextInfo:nil];
}

- (IBAction)closeCustomizationSheet:(id)sender
{
    [_customize_panel orderOut:sender];
    [NSApp endSheet: _customize_panel];

    /* update current profile based upon the sheet's values */
    /* Container(string), transcode video(bool), transcode audio(bool),
     * use subtitles(bool), video codec(string), video bitrate(integer),
     * scale(float), fps(float), width(integer, height(integer),
     * audio codec(string), audio bitrate(integer), channels(integer),
     * samplerate(integer), subtitle codec(string), subtitle overlay(bool) */

    if (sender == _customize_ok_btn && [_currentProfile count] == 16) {
        NSInteger i;
        [_currentProfile replaceObjectAtIndex:0 withObject:[self currentEncapsulationFormatAsFileExtension:NO]];
        [_currentProfile replaceObjectAtIndex:1 withObject:[NSString stringWithFormat:@"%li", [_customize_vid_ckb state]]];
        [_currentProfile replaceObjectAtIndex:2 withObject:[NSString stringWithFormat:@"%li", [_customize_aud_ckb state]]];
        [_currentProfile replaceObjectAtIndex:3 withObject:[NSString stringWithFormat:@"%li", [_customize_subs_ckb state]]];
        i = [_customize_vid_codec_pop indexOfSelectedItem];
        if (i >= 0)
            [_currentProfile replaceObjectAtIndex:4 withObject:[_videoCodecs objectAtIndex:i]];
        else
            [_currentProfile replaceObjectAtIndex:4 withObject:@"none"];
        [_currentProfile replaceObjectAtIndex:5 withObject:[_customize_vid_bitrate_fld stringValue]];
        [_currentProfile replaceObjectAtIndex:6 withObject:[[_customize_vid_scale_pop selectedItem] title]];
        [_currentProfile replaceObjectAtIndex:7 withObject:[_customize_vid_framerate_fld stringValue]];
        [_currentProfile replaceObjectAtIndex:8 withObject:[_customize_vid_width_fld stringValue]];
        [_currentProfile replaceObjectAtIndex:9 withObject:[_customize_vid_height_fld stringValue]];
        i = [_customize_aud_codec_pop indexOfSelectedItem];
        if (i >= 0)
            [_currentProfile replaceObjectAtIndex:10 withObject:[_audioCodecs objectAtIndex:i]];
        else
            [_currentProfile replaceObjectAtIndex:10 withObject:@"none"];
        [_currentProfile replaceObjectAtIndex:11 withObject:[_customize_aud_bitrate_fld stringValue]];
        [_currentProfile replaceObjectAtIndex:12 withObject:[_customize_aud_channels_fld stringValue]];
        [_currentProfile replaceObjectAtIndex:13 withObject:[[_customize_aud_samplerate_pop selectedItem] title]];
        i = [_customize_subs_pop indexOfSelectedItem];
        if (i >= 0)
            [_currentProfile replaceObjectAtIndex:14 withObject:[_subsCodecs objectAtIndex:i]];
        else
            [_currentProfile replaceObjectAtIndex:14 withObject:@"none"];
        [_currentProfile replaceObjectAtIndex:15 withObject:[NSString stringWithFormat:@"%li", [_customize_subs_overlay_ckb state]]];
    }
}

- (IBAction)iWantAFile:(id)sender
{
    NSRect boxFrame = [_destination_box frame];
    NSRect subViewFrame = [_destination_itwantafile_view frame];
    subViewFrame.origin.x = (boxFrame.size.width - subViewFrame.size.width) / 2;
    subViewFrame.origin.y = ((boxFrame.size.height - subViewFrame.size.height) / 2) - 15.;
    [_destination_itwantafile_view setFrame: subViewFrame];
    [[_destination_itwantafile_btn animator] setHidden: YES];
    [[_destination_itwantastream_btn animator] setHidden: YES];
    [_destination_box performSelector:@selector(addSubview:) withObject:_destination_itwantafile_view afterDelay:0.2];
    [[_destination_cancel_btn animator] setHidden:NO];
    b_streaming = NO;
    [_ok_btn setTitle:_NS("Save")];
}

- (IBAction)iWantAStream:(id)sender
{
    NSRect boxFrame = [_destination_box frame];
    NSRect subViewFrame = [_destination_itwantastream_view frame];
    subViewFrame.origin.x = (boxFrame.size.width - subViewFrame.size.width) / 2;
    subViewFrame.origin.y = ((boxFrame.size.height - subViewFrame.size.height) / 2) - 15.;
    [_destination_itwantastream_view setFrame: subViewFrame];
    [[_destination_itwantafile_btn animator] setHidden: YES];
    [[_destination_itwantastream_btn animator] setHidden: YES];
    [_destination_box performSelector:@selector(addSubview:) withObject:_destination_itwantastream_view afterDelay:0.2];
    [[_destination_cancel_btn animator] setHidden:NO];
    b_streaming = YES;
    [_ok_btn setTitle:_NS("Stream")];
}

- (IBAction)cancelDestination:(id)sender
{
    if ([_destination_itwantastream_view superview] != nil)
        [_destination_itwantastream_view removeFromSuperview];
    if ([_destination_itwantafile_view superview] != nil)
        [_destination_itwantafile_view removeFromSuperview];

    [_destination_cancel_btn setHidden:YES];
    [[_destination_itwantafile_btn animator] setHidden: NO];
    [[_destination_itwantastream_btn animator] setHidden: NO];
    b_streaming = NO;
}

- (IBAction)browseFileDestination:(id)sender
{
    NSSavePanel * saveFilePanel = [NSSavePanel savePanel];
    [saveFilePanel setCanSelectHiddenExtension: YES];
    [saveFilePanel setCanCreateDirectories: YES];
    if ([[_customize_encap_matrix selectedCell] tag] != RAW) // there is no clever guess for this
        [saveFilePanel setAllowedFileTypes:[NSArray arrayWithObject:[self currentEncapsulationFormatAsFileExtension:YES]]];
    [saveFilePanel beginSheetForDirectory:nil file:nil modalForWindow:_window modalDelegate:self didEndSelector:@selector(savePanelDidEnd:returnCode:contextInfo:) contextInfo:nil];
}

- (void)savePanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if (returnCode == NSOKButton) {
        [self setOutputDestination:[[sheet URL] path]];
        [_destination_filename_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath:_outputDestination]];
        [[_destination_filename_stub_lbl animator] setHidden: YES];
        [[_destination_filename_lbl animator] setHidden: NO];
    } else {
        [self setOutputDestination:@""];
        [[_destination_filename_lbl animator] setHidden: YES];
        [[_destination_filename_stub_lbl animator] setHidden: NO];
    }
    [self updateOKButton];
}

- (IBAction)showStreamPanel:(id)sender
{
    [NSApp beginSheet:_stream_panel modalForWindow:_window modalDelegate:self didEndSelector:NULL contextInfo:nil];
}

- (IBAction)closeStreamPanel:(id)sender
{
    NSMutableString * labelContent = [[NSMutableString alloc] initWithFormat:@"%@ @ %@:%@", [_stream_type_pop titleOfSelectedItem], [_stream_address_fld stringValue], [_stream_port_fld stringValue]];

    if ([_stream_type_pop indexOfSelectedItem] > 1) {
        [labelContent appendFormat:@" TTL:%@", [_stream_ttl_fld stringValue]];
        if ([_stream_sap_ckb state] || [_stream_rtsp_ckb state] || [_stream_http_ckb state] || [_stream_sdp_ckb state])
            [labelContent appendFormat:@" — %@:\"%@\" (", _NS("Channel"), [_stream_channel_fld stringValue]];

        BOOL b_gotSomething;
        if (_stream_sap_ckb) {
            [labelContent appendString:@"SAP"];
            b_gotSomething = YES;
        }
        if (_stream_rtsp_ckb) {
            if (b_gotSomething)
                [labelContent appendString:@", "];
            [labelContent appendString:@"RTSP"];
            b_gotSomething = YES;
        }
        if (_stream_http_ckb) {
            if (b_gotSomething)
                [labelContent appendString:@", "];
            [labelContent appendString:@"HTTP"];
            b_gotSomething = YES;
        }
        if ([_stream_sdp_ckb state]) {
            if (b_gotSomething)
                [labelContent appendString:@", "];
            [labelContent appendString:@"SDP"];
        }

        if ([_stream_sap_ckb state] || [_stream_rtsp_ckb state] || [_stream_http_ckb state] || [_stream_sdp_ckb state])
            [labelContent appendString:@")"];
    }
    [_destination_stream_lbl setStringValue:labelContent];
    [labelContent release];

    [_stream_panel orderOut:sender];
    [NSApp endSheet: _stream_panel];
}

- (IBAction)streamTypeToggle:(id)sender
{
    NSUInteger index = [_stream_type_pop indexOfSelectedItem];
    if (index <= 1) { // HTTP, MMSH
        [_stream_ttl_fld setEnabled:NO];
        [_stream_ttl_stepper setEnabled:NO];
        [_stream_sap_ckb setEnabled:NO];
        [_stream_rtsp_ckb setEnabled:NO];
        [_stream_http_ckb setEnabled:NO];
        [_stream_sdp_ckb setEnabled:NO];
    } else if (index == 2) { // RTP
        [_stream_ttl_fld setEnabled:YES];
        [_stream_ttl_stepper setEnabled:YES];
        [_stream_sap_ckb setEnabled:YES];
        [_stream_rtsp_ckb setEnabled:YES];
        [_stream_http_ckb setEnabled:YES];
        [_stream_sdp_ckb setEnabled:YES];
        [_stream_channel_fld setEnabled:YES];
        [_stream_sdp_fld setEnabled:[_stream_sdp_ckb state]];
    } else { // UDP
        [_stream_ttl_fld setEnabled:YES];
        [_stream_ttl_stepper setEnabled:YES];
        [_stream_sap_ckb setEnabled:YES];
        [_stream_rtsp_ckb setEnabled:NO];
        [_stream_http_ckb setEnabled:NO];
        [_stream_sdp_ckb setEnabled:NO];
        [_stream_channel_fld setEnabled:YES];
    }
}

- (IBAction)streamAnnouncementToggle:(id)sender
{
    [_stream_sdp_fld setEnabled:[_stream_sdp_ckb state]];
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *paste = [sender draggingPasteboard];
    NSArray *types = [NSArray arrayWithObjects: NSFilenamesPboardType, @"VLCPlaylistItemPboardType", nil];
    NSString *desired_type = [paste availableTypeFromArray: types];
    NSData *carried_data = [paste dataForType: desired_type];

    if( carried_data ) {
        if( [desired_type isEqualToString:NSFilenamesPboardType] ) {
            NSArray *values = [[paste propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

            if ([values count] > 0) {
                [self setMRL: [NSString stringWithUTF8String:make_URI([[values objectAtIndex:0] UTF8String], NULL)]];
                [self updateOKButton];
                [self updateDropView];
                return YES;
            }
        } else if( [desired_type isEqualToString:@"VLCPlaylistItemPboardType"] ) {
            NSArray * array = [[[VLCMain sharedInstance] playlist] draggedItems];
            NSUInteger count = [array count];
            if (count > 0) {
                playlist_t * p_playlist = pl_Get( VLCIntf );
                playlist_item_t * p_item = NULL;

                PL_LOCK;
                /* let's look for the first proper input item */
                for (NSUInteger x = 0; x < count; x++) {
                    p_item = [[array objectAtIndex:x] pointerValue];
                    if (p_item) {
                        if (p_item->p_input) {
                            if (p_item->p_input->psz_uri != nil) {
                                [self setMRL: [NSString stringWithFormat:@"%s", p_item->p_input->psz_uri]];
                                [self updateDropView];
                                [self updateOKButton];

                                PL_UNLOCK;

                                return YES;
                            }
                        }
                    }
                }
                PL_UNLOCK;
            }
        }
    }
    return NO;
}

# pragma mark -
# pragma mark Private Functionality
- (void)updateDropView
{
    if ([_MRL length] > 0) {
        NSString * path = [[NSURL URLWithString:_MRL] path];
        [_dropin_media_lbl setStringValue: [[NSFileManager defaultManager] displayNameAtPath: path]];
        NSImage * image = [[NSWorkspace sharedWorkspace] iconForFile: path];
        [image setSize:NSMakeSize(128,128)];
        [_dropin_icon_view setImage: image];

        if (![_dropin_view superview]) {
            NSRect boxFrame = [_drop_box frame];
            NSRect subViewFrame = [_dropin_view frame];
            subViewFrame.origin.x = (boxFrame.size.width - subViewFrame.size.width) / 2;
            subViewFrame.origin.y = (boxFrame.size.height - subViewFrame.size.height) / 2;
            [_dropin_view setFrame: subViewFrame];
            [[_drop_image_view animator] setHidden: YES];
            [_drop_box performSelector:@selector(addSubview:) withObject:_dropin_view afterDelay:0.6];
        }
    } else {
        [_dropin_view removeFromSuperview];
        [[_drop_image_view animator] setHidden: NO];
    }
}

- (void)updateOKButton
{
    if ([_outputDestination length] > 0 && [_MRL length] > 0)
        [_ok_btn setEnabled: YES];
    else
        [_ok_btn setEnabled: NO];
}

- (void)resetCustomizationSheetBasedOnProfile:(NSString *)profileString
{
    /* Container(string), transcode video(bool), transcode audio(bool),
    * use subtitles(bool), video codec(string), video bitrate(integer),
    * scale(float), fps(float), width(integer, height(integer),
    * audio codec(string), audio bitrate(integer), channels(integer),
    * samplerate(integer), subtitle codec(string), subtitle overlay(bool) */

    NSArray * components = [profileString componentsSeparatedByString:@";"];
    if ([components count] != 16) {
        msg_Err(VLCIntf, "CAS: the requested profile '%s' is invalid", [profileString UTF8String]);
        return;
    }

    [self selectCellByEncapsulationFormat:[components objectAtIndex:0]];
    [_customize_vid_ckb setState:[[components objectAtIndex:1] intValue]];
    [_customize_aud_ckb setState:[[components objectAtIndex:2] intValue]];
    [_customize_subs_ckb setState:[[components objectAtIndex:3] intValue]];
    [_customize_vid_bitrate_fld setStringValue:[components objectAtIndex:5]];
    [_customize_vid_scale_pop selectItemWithTitle:[components objectAtIndex:6]];
    [_customize_vid_framerate_fld setStringValue:[components objectAtIndex:7]];
    [_customize_vid_width_fld setStringValue:[components objectAtIndex:8]];
    [_customize_vid_height_fld setStringValue:[components objectAtIndex:9]];
    [_customize_aud_bitrate_fld setStringValue:[components objectAtIndex:11]];
    [_customize_aud_channels_fld setStringValue:[components objectAtIndex:12]];
    [_customize_aud_samplerate_pop selectItemWithTitle:[components objectAtIndex:13]];
    [_customize_subs_overlay_ckb setState:[[components objectAtIndex:15] intValue]];

    /* since there is no proper lookup mechanism in arrays, we need to implement a string specific one ourselves */
    NSArray * tempArray = [_videoCodecs objectAtIndex:1];
    NSUInteger count = [tempArray count];
    NSString * searchString = [components objectAtIndex:4];
    if ([searchString isEqualToString:@"none"] || [searchString isEqualToString:@"0"]) {
        [_customize_vid_codec_pop selectItemAtIndex:-1];
    } else {
        for (NSUInteger x = 0; x < count; x++) {
            if ([[tempArray objectAtIndex:x] isEqualToString: searchString]) {
                [_customize_vid_codec_pop selectItemAtIndex:x];
                break;
            }
        }
    }

    tempArray = [_audioCodecs objectAtIndex:1];
    count = [tempArray count];
    searchString = [components objectAtIndex:10];
    if ([searchString isEqualToString:@"none"] || [searchString isEqualToString:@"0"]) {
        [_customize_aud_codec_pop selectItemAtIndex:-1];
    } else {
        for (NSUInteger x = 0; x < count; x++) {
            if ([[tempArray objectAtIndex:x] isEqualToString: searchString]) {
                [_customize_aud_codec_pop selectItemAtIndex:x];
                break;
            }
        }
    }

    tempArray = [_subsCodecs objectAtIndex:1];
    count = [tempArray count];
    searchString = [components objectAtIndex:14];
    if ([searchString isEqualToString:@"none"] || [searchString isEqualToString:@"0"]) {
        [_customize_subs_pop selectItemAtIndex:-1];
    } else {
        for (NSUInteger x = 0; x < count; x++) {
            if ([[tempArray objectAtIndex:x] isEqualToString: searchString]) {
                [_customize_subs_pop selectItemAtIndex:x];
                break;
            }
        }
    }

    if (_currentProfile)
        [_currentProfile release];
    _currentProfile = [[NSMutableArray alloc] initWithArray: [profileString componentsSeparatedByString:@";"]];
}

- (void)selectCellByEncapsulationFormat:(NSString *)format
{
    if ([format isEqualToString:@"ts"])
        [_customize_encap_matrix selectCellWithTag:MPEGTS];
    else if([format isEqualToString:@"webm"])
        [_customize_encap_matrix selectCellWithTag:WEBM];
    else if([format isEqualToString:@"ogg"])
        [_customize_encap_matrix selectCellWithTag:OGG];
    else if([format isEqualToString:@"ogm"])
        [_customize_encap_matrix selectCellWithTag:OGG];
    else if([format isEqualToString:@"mp4"])
        [_customize_encap_matrix selectCellWithTag:MP4];
    else if([format isEqualToString:@"mov"])
        [_customize_encap_matrix selectCellWithTag:MP4];
    else if([format isEqualToString:@"ps"])
        [_customize_encap_matrix selectCellWithTag:MPEGPS];
    else if([format isEqualToString:@"mpjpeg"])
        [_customize_encap_matrix selectCellWithTag:MJPEG];
    else if([format isEqualToString:@"wav"])
        [_customize_encap_matrix selectCellWithTag:WAV];
    else if([format isEqualToString:@"flv"])
        [_customize_encap_matrix selectCellWithTag:FLV];
    else if([format isEqualToString:@"mpeg1"])
        [_customize_encap_matrix selectCellWithTag:MPEG1];
    else if([format isEqualToString:@"mkv"])
        [_customize_encap_matrix selectCellWithTag:MKV];
    else if([format isEqualToString:@"raw"])
        [_customize_encap_matrix selectCellWithTag:RAW];
    else if([format isEqualToString:@"avi"])
        [_customize_encap_matrix selectCellWithTag:AVI];
    else if([format isEqualToString:@"asf"])
        [_customize_encap_matrix selectCellWithTag:ASF];
    else if([format isEqualToString:@"wmv"])
        [_customize_encap_matrix selectCellWithTag:ASF];
    else
        msg_Err(VLCIntf, "CAS: unknown encap format requested for customization");
}

- (NSString *)currentEncapsulationFormatAsFileExtension:(BOOL)b_extension
{
    NSUInteger cellTag = [[_customize_encap_matrix selectedCell] tag];
    NSString * returnValue;
    switch (cellTag) {
        case MPEGTS:
            returnValue = @"ts";
            break;
        case WEBM:
            returnValue = @"webm";
            break;
        case OGG:
            returnValue = @"ogg";
            break;
        case MP4:
        {
            if (b_extension)
                returnValue = @"m4v";
            else
                returnValue = @"mp4";
            break;
        }
        case MPEGPS:
        {
            if (b_extension)
                returnValue = @"mpg";
            else
                returnValue = @"ps";
            break;
        }
        case MJPEG:
            returnValue = @"mjpeg";
            break;
        case WAV:
            returnValue = @"wav";
            break;
        case FLV:
            returnValue = @"flv";
            break;
        case MPEG1:
        {
            if (b_extension)
                returnValue = @"mpg";
            else
                returnValue = @"mpeg1";
            break;
        }
        case MKV:
            returnValue = @"mkv";
            break;
        case RAW:
            returnValue = @"raw";
            break;
        case AVI:
            returnValue = @"avi";
            break;
        case ASF:
            returnValue = @"asf";
            break;

        default:
            returnValue = @"none";
            break;
    }

    return returnValue;
}

- (NSString *)composedOptions
{
    NSMutableString *composedOptions = [[NSMutableString alloc] initWithString:@":sout=#transcode{"];
    if ([[_currentProfile objectAtIndex:1] intValue]) {
        // video is enabled
        [composedOptions appendFormat:@"vcodec=%@", [_currentProfile objectAtIndex:4]];
        if (![[_currentProfile objectAtIndex:4] isEqualToString:@"none"]) {
            if ([[_currentProfile objectAtIndex:5] intValue] > 0) // bitrate
                [composedOptions appendFormat:@",vb=%@", [_currentProfile objectAtIndex:5]];
            if ([[_currentProfile objectAtIndex:6] floatValue] > 0.) // scale
                [composedOptions appendFormat:@",scale=%@", [_currentProfile objectAtIndex:6]];
            if ([[_currentProfile objectAtIndex:7] floatValue] > 0.) // fps
                [composedOptions appendFormat:@",fps=%@", [_currentProfile objectAtIndex:7]];
            if ([[_currentProfile objectAtIndex:8] intValue] > 0) // width
                [composedOptions appendFormat:@",width=%@", [_currentProfile objectAtIndex:8]];
            if ([[_currentProfile objectAtIndex:9] intValue] > 0) // height
                [composedOptions appendFormat:@",height=%@", [_currentProfile objectAtIndex:9]];
        }
    }
    if ([[_currentProfile objectAtIndex:2] intValue]) {
        // audio is enabled

        // add another comma in case video is enabled
        if ([[_currentProfile objectAtIndex:1] intValue])
            [composedOptions appendString:@","];

        [composedOptions appendFormat:@"acodec=%@", [_currentProfile objectAtIndex:10]];
        if (![[_currentProfile objectAtIndex:10] isEqualToString:@"none"]) {
            [composedOptions appendFormat:@",ab=%@", [_currentProfile objectAtIndex:11]]; // bitrate
            [composedOptions appendFormat:@",channels=%@", [_currentProfile objectAtIndex:12]]; // channel number
            [composedOptions appendFormat:@",samplerate=%@", [_currentProfile objectAtIndex:13]]; // sample rate
        }
    }
    if ([_currentProfile objectAtIndex:3]) {
        // subtitles enabled
        [composedOptions appendFormat:@",scodec=%@", [_currentProfile objectAtIndex:14]];
        if ([[_currentProfile objectAtIndex:15] intValue])
            [composedOptions appendFormat:@",soverlay"];
    }

    // add muxer
    [composedOptions appendFormat:@"}:standard{mux=%@", [_currentProfile objectAtIndex:0]];

    // add output destination (file only at this point)
    [composedOptions appendFormat:@",dst=%@,access=file}", _outputDestination];

    NSString * returnString = [NSString stringWithString:composedOptions];
    [composedOptions release];

    return returnString;
}

@end

# pragma mark -
# pragma mark Drag and drop handling

@implementation VLCDropEnabledBox

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects: NSFilenamesPboardType, @"VLCPlaylistItemPboardType", nil]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    b_activeDragAndDrop = YES;
    [self setNeedsDisplay:YES];

    if (OSX_SNOW_LEOPARD || OSX_LION)
        [[NSCursor dragCopyCursor] set];

    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric)
        return NSDragOperationGeneric;

    return NSDragOperationNone;
}

- (void)draggingEnded:(id < NSDraggingInfo >)sender
{
    [[NSCursor arrowCursor] set];
    b_activeDragAndDrop = NO;
    [self setNeedsDisplay:YES];
}

- (void)draggingExited:(id < NSDraggingInfo >)sender
{
    [[NSCursor arrowCursor] set];
    b_activeDragAndDrop = NO;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect
{
    if (b_activeDragAndDrop) {
        [[NSColor colorWithCalibratedRed:(.154/.255) green:(.154/.255) blue:(.154/.255) alpha:1.] setFill];
        NSRect frameRect = [[self contentView] bounds];
        frameRect.origin.x += 10;
        frameRect.origin.y += 10;
        frameRect.size.width -= 17;
        frameRect.size.height -= 17;
        NSFrameRectWithWidthUsingOperation(frameRect, 4., NSCompositeHighlight);
    }

    [super drawRect:dirtyRect];
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    return [[VLCConvertAndSave sharedInstance] performDragOperation: sender];
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end

@implementation VLCDropEnabledImageView

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects: NSFilenamesPboardType, @"VLCPlaylistItemPboardType", nil]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    return [[[self superview] superview] draggingEntered:sender];
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    return [[VLCConvertAndSave sharedInstance] performDragOperation: sender];
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end

@implementation VLCDropEnabledButton

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects: NSFilenamesPboardType, @"VLCPlaylistItemPboardType", nil]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    return [[[self superview] superview] draggingEntered:sender];
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    return [[VLCConvertAndSave sharedInstance] performDragOperation: sender];
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end
