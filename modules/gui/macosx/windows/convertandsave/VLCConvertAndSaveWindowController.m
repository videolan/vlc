/*****************************************************************************
 * VLCConvertAndSaveWindowController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012, 2019 Felix Paul Kühne
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

#import "VLCConvertAndSaveWindowController.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"
#import "main/VLCMain.h"
#import "panels/dialogs/VLCPopupPanelController.h"
#import "panels/dialogs/VLCTextfieldPanelController.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlaylistModel.h"
#import "views/VLCDragDropView.h"
#import "windows/VLCOpenInputMetadata.h"

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

NSString *VLCConvertAndSaveProfilesKey = @"CASProfiles";
NSString *VLCConvertAndSaveProfileNamesKey = @"CASProfileNames";

@interface VLCConvertAndSaveWindowController() <VLCDragDropTarget>
{
    NSArray *_videoCodecs;
    NSArray *_audioCodecs;
    NSArray *_subsCodecs;
    BOOL b_streaming;
}
@property (readwrite, nonatomic, retain) NSString *MRL;
@property (readwrite, nonatomic, retain) NSString *outputDestination;
@property (readwrite, retain) NSArray *profileNames;
@property (readwrite, retain) NSArray *profileValueList;
@property (readwrite, retain) NSMutableArray *currentProfile;

- (void)updateDropView;
- (void)updateOKButton;
- (void)resetCustomizationSheetBasedOnProfile:(NSString *)profileString;
- (void)selectCellByEncapsulationFormat:(NSString *)format;
- (NSString *)currentEncapsulationFormatAsFileExtension:(BOOL)b_extension;
- (NSString *)composedOptions;
- (void)updateCurrentProfile;
- (void)storeProfilesOnDisk;
- (void)recreateProfilePopup;
@end

@implementation VLCConvertAndSaveWindowController

#pragma mark -
#pragma mark Initialization

+ (void)initialize
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    /* We are using the same format as the Qt intf here:
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
                                 @"ogg;0;1;0;none;800;1;0;0;0;vorb;128;2;44100;none;0",
                                 @"raw;0;1;0;none;800;1;0;0;0;mp3;128;2;44100;none;0",
                                 @"mp4;0;1;0;none;800;1;0;0;0;mpga;128;2;44100;none;0",
                                 @"raw;0;1;0;none;800;1;0;0;0;flac;128;2;44100;none;0",
                                 @"wav;0;1;0;none;800;1;0;0;0;s16l;128;2;44100;none;0", nil];

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

    NSDictionary *appDefaults = @{defaultProfiles : VLCConvertAndSaveProfilesKey,
                                  defaultProfileNames : VLCConvertAndSaveProfileNamesKey};

    [defaults registerDefaults:appDefaults];
}

- (id)init
{
    self = [super initWithWindowNibName:@"ConvertAndSave"];
    if (self) {
        self.popupPanel = [[VLCPopupPanelController alloc] init];
        self.textfieldPanel = [[VLCTextfieldPanelController alloc] init];
    }
    return self;
}

- (void)windowDidLoad
{
    [self initStrings];

    /* there is no way to hide single cells, so replace the existing ones with empty cells.. */
    NSCell *blankCell = [[NSCell alloc] init];
    [blankCell setEnabled:NO];
    [_customizeEncapMatrix putCell:blankCell atRow:3 column:1];
    [_customizeEncapMatrix putCell:blankCell atRow:3 column:2];
    [_customizeEncapMatrix putCell:blankCell atRow:3 column:3];

    /* fetch profiles from defaults */
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [self setProfileValueList: [defaults arrayForKey:VLCConvertAndSaveProfilesKey]];
    [self setProfileNames: [defaults arrayForKey:VLCConvertAndSaveProfileNamesKey]];
    [self recreateProfilePopup];

    [self initCodecStructures];

    [self populatePopupButtons];

    [_okButton setEnabled: NO];

    // setup drop view
    [_dropBox enablePlaylistItems];
    [_dropBox setDropTarget:self];

    [self resetCustomizationSheetBasedOnProfile:[self.profileValueList firstObject]];
}

- (void)initStrings
{
    [self.window setTitle: _NS("Convert & Stream")];
    [_okButton setTitle: _NS("Go!")];
    [_dropLabel setStringValue: _NS("Drop media here")];
    [_dropButton setTitle: _NS("Open media...")];
    [_profileLabel setStringValue: _NS("Choose Profile")];
    [_customizeButton setTitle: _NS("Customize...")];
    [_destinationLabel setStringValue: _NS("Choose Destination")];
    [_fileDestinationFileNameStub setStringValue: _NS("Choose an output location")];
    [_fileDestinationFileName setHidden: YES];
    [_fileDestinationBrowseButton setTitle:_NS("Browse...")];
    [_streamDestinationButton setTitle:_NS("Setup Streaming...")];
    [_streamDestinationURLLabel setStringValue:_NS("Select Streaming Method")];
    [_destinationFileButton setTitle:_NS("Save as File")];
    [_destinationStreamButton setTitle:_NS("Stream")];
    [_destinationCancelBtn setHidden:YES];

    [_customizeOkButton setTitle: _NS("Apply")];
    [_customizeCancelButton setTitle: _NS("Cancel")];
    [_customizeNewProfileButton setTitle: _NS("Save as new Profile...")];
    [[_customizeTabView tabViewItemAtIndex:0] setLabel: _NS("Encapsulation")];
    [[_customizeTabView tabViewItemAtIndex:1] setLabel: _NS("Video codec")];
    [[_customizeTabView tabViewItemAtIndex:2] setLabel: _NS("Audio codec")];
    [[_customizeTabView tabViewItemAtIndex:3] setLabel: _NS("Subtitles")];
    [_customizeTabView selectTabViewItemAtIndex: 0];
    [_customizeVidCheckbox setTitle: _NS("Video")];
    [_customizeVidKeepCheckbox setTitle: _NS("Keep original video track")];
    [_customizeVidCodecLabel setStringValue: _NS("Codec")];
    [_customizeVidBitrateLabel setStringValue: _NS("Bitrate")];
    [_customizeVidFramerateLabel setStringValue: _NS("Frame rate")];
    [_customizeVidResolutionBox setTitle: _NS("Resolution")];
    [_customizeVidResLabel setStringValue: _NS("You just need to fill one of the three following parameters, VLC will autodetect the other using the original aspect ratio")];
    [_customizeVidWidthLabel setStringValue: _NS("Width")];
    [_customizeVidHeightLabel setStringValue: _NS("Height")];
    [_customizeVidScaleLabel setStringValue: _NS("Scale")];

    [_customizeAudCheckbox setTitle: _NS("Audio")];
    [_customizeAudKeepCheckbox setTitle: _NS("Keep original audio track")];
    [_customizeAudCodecLabel setStringValue: _NS("Codec")];
    [_customizeAudBitrateLabel setStringValue: _NS("Bitrate")];
    [_customizeAudChannelsLabel setStringValue: _NS("Channels")];
    [_customizeAudSamplerateLabel setStringValue: _NS("Samplerate")];

    [_customizeSubsCheckbox setTitle: _NS("Subtitles")];
    [_customizeSubsOverlayCheckbox setTitle: _NS("Overlay subtitles on the video")];

    [_streamOkButton setTitle: _NS("Apply")];
    [_streamCancelButton setTitle: _NS("Cancel")];
    [_streamDestinationLabel setStringValue:_NS("Stream Destination")];
    [_streamAnnouncementLabel setStringValue:_NS("Stream Announcement")];
    [_streamTypeLabel setStringValue:_NS("Type")];
    [_streamAddressLabel setStringValue:_NS("Address")];
    [_streamTTLLabel setStringValue:_NS("TTL")];
    [_streamTTLStepper setEnabled:NO];
    [_streamPortLabel setStringValue:_NS("Port")];
    [_streamSAPCheckbox setStringValue:_NS("SAP Announcement")];
    [[_streamSDPMatrix cellWithTag:0] setTitle:_NS("None")];
    [[_streamSDPMatrix cellWithTag:1] setTitle:_NS("HTTP Announcement")];
    [[_streamSDPMatrix cellWithTag:2] setTitle:_NS("RTSP Announcement")];
    [[_streamSDPMatrix cellWithTag:3] setTitle:_NS("Export SDP as file")];
    [_streamSAPCheckbox setState:NSOffState];
    [_streamSDPMatrix setEnabled:NO];
    [_streamSDPFileBrowseButton setStringValue:_NS("Browse...")];
    [_streamChannelLabel setStringValue:_NS("Channel Name")];
    [_streamSDPLabel setStringValue:_NS("SDP URL")];
}

- (void)initCodecStructures
{
    _videoCodecs = @[
                     @[@"MPEG-1", @"MPEG-2", @"MPEG-4", @"DIVX 1", @"DIVX 2", @"DIVX 3", @"H.263", @"H.264", @"VP8", @"WMV1", @"WMV2", @"M-JPEG", @"Theora", @"Dirac"],
                     @[@"mpgv", @"mp2v", @"mp4v", @"DIV1", @"DIV2", @"DIV3", @"H263", @"h264", @"VP80", @"WMV1", @"WMV2", @"MJPG", @"theo", @"drac"],
                     ];
    _audioCodecs = @[
                     @[@"MPEG Audio", @"MP3", @"MPEG 4 Audio (AAC)", @"A52/AC-3", @"Vorbis", @"Flac", @"Speex", @"WAV", @"WMA2"],
                     @[@"mpga", @"mp3", @"mp4a", @"a52", @"vorb", @"flac", @"spx", @"s16l", @"wma2"],
                     ];
    _subsCodecs = @[
                    @[@"DVB subtitle", @"T.140"],
                    @[@"dvbs", @"t140"],
                    ];
}

- (void)populatePopupButtons
{
    [_customizeVidCodecPopup removeAllItems];
    [_customizeVidScalePopup removeAllItems];
    [_customizeAudCodecPopup removeAllItems];
    [_customizeAudSampleratePopup removeAllItems];
    [_customizeSubsPopup removeAllItems];

    [_customizeVidCodecPopup addItemsWithTitles:[_videoCodecs firstObject]];
    [_customizeAudCodecPopup addItemsWithTitles:[_audioCodecs firstObject]];
    [_customizeSubsPopup addItemsWithTitles:[_subsCodecs firstObject]];

    [_customizeAudSampleratePopup addItemWithTitle:@"8000"];
    [_customizeAudSampleratePopup addItemWithTitle:@"11025"];
    [_customizeAudSampleratePopup addItemWithTitle:@"22050"];
    [_customizeAudSampleratePopup addItemWithTitle:@"44100"];
    [_customizeAudSampleratePopup addItemWithTitle:@"48000"];

    [_customizeVidScalePopup addItemWithTitle:@"1"];
    [_customizeVidScalePopup addItemWithTitle:@"0.25"];
    [_customizeVidScalePopup addItemWithTitle:@"0.5"];
    [_customizeVidScalePopup addItemWithTitle:@"0.75"];
    [_customizeVidScalePopup addItemWithTitle:@"1.25"];
    [_customizeVidScalePopup addItemWithTitle:@"1.5"];
    [_customizeVidScalePopup addItemWithTitle:@"1.75"];
    [_customizeVidScalePopup addItemWithTitle:@"2"];
}

# pragma mark -
# pragma mark User Interaction - main window

- (IBAction)finalizePanel:(id)sender
{
    if (b_streaming) {
        if ([[[_streamTypePopup selectedItem] title] isEqualToString:@"HTTP"]) {
            NSString *muxformat = [self.currentProfile firstObject];
            if ([muxformat isEqualToString:@"wav"] || [muxformat isEqualToString:@"mov"] || [muxformat isEqualToString:@"mp4"] || [muxformat isEqualToString:@"mkv"]) {
                NSAlert *alert = [[NSAlert alloc] init];
                [alert setAlertStyle:NSInformationalAlertStyle];
                [alert setMessageText:_NS("Invalid container format for HTTP streaming")];
                [alert setInformativeText:[NSString stringWithFormat:_NS("Media encapsulated as %@ cannot be streamed through the HTTP protocol for technical reasons."),
                                           [[self currentEncapsulationFormatAsFileExtension:YES] uppercaseString]]];
                [alert beginSheetModalForWindow:self.window
                              completionHandler:nil];
                return;
            }
        }
    }

    VLCOpenInputMetadata *inputMetaItem = [[VLCOpenInputMetadata alloc] init];
    inputMetaItem.MRLString = _MRL;
    inputMetaItem.itemName = [_dropinMediaLabel stringValue];

    NSMutableArray *options = [[NSMutableArray alloc] init];
    [options addObject:[self composedOptions]];
    if (b_streaming) {
        [options addObject:[NSString stringWithFormat:@"ttl=%@", [_streamTTLField stringValue]]];
    }
    inputMetaItem.playbackOptions = options;

    VLCPlaylistController *playlistController = [[VLCMain sharedInstance] playlistController];
    [playlistController addPlaylistItems:@[inputMetaItem]];
    [playlistController playItemAtIndex:(playlistController.playlistModel.numberOfPlaylistItems -1)];

    [self.window performClose:sender];
}

- (IBAction)openMedia:(id)sender
{
    /* preliminary implementation until the open panel is cleaned up */
    NSOpenPanel * openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseDirectories:NO];
    [openPanel setResolvesAliases:YES];
    [openPanel setAllowsMultipleSelection:NO];
    [openPanel beginSheetModalForWindow:self.window completionHandler:^(NSInteger returnCode) {
        if (returnCode == NSModalResponseOK) {
            [self setMRL: [[openPanel URL] absoluteString]];
            [self updateOKButton];
            [self updateDropView];
        }
    }];
}

- (IBAction)switchProfile:(id)sender
{
    NSUInteger index = [_profilePopup indexOfSelectedItem];
    // last index is "custom"
    if (index <= ([self.profileValueList count] - 1))
        [self resetCustomizationSheetBasedOnProfile:[self.profileValueList objectAtIndex:index]];
}

- (IBAction)deleteProfileAction:(id)sender
{
    /* show panel */
    [_popupPanel setTitleString:_NS("Remove a profile")];
    [_popupPanel setSubTitleString:_NS("Select the profile you would like to remove:")];
    [_popupPanel setOkButtonString:_NS("Remove")];
    [_popupPanel setCancelButtonString:_NS("Cancel")];
    [_popupPanel setPopupButtonContent:self.profileNames];

    __weak typeof(self) _self = self;
    [_popupPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSInteger selectedIndex) {

        if (returnCode != NSModalResponseOK)
            return;

        /* remove requested profile from the arrays */
        NSMutableArray * workArray = [[NSMutableArray alloc] initWithArray:_self.profileNames];
        [workArray removeObjectAtIndex:selectedIndex];
        [_self setProfileNames:[[NSArray alloc] initWithArray:workArray]];
        workArray = [[NSMutableArray alloc] initWithArray:_self.profileValueList];
        [workArray removeObjectAtIndex:selectedIndex];
        [_self setProfileValueList:[[NSArray alloc] initWithArray:workArray]];

        /* update UI */
        [_self recreateProfilePopup];

        /* update internals */
        [_self switchProfile:_self];
        [_self storeProfilesOnDisk];
    }];
}

- (IBAction)iWantAFile:(id)sender
{
    NSRect boxFrame = [_destinationBox frame];
    NSRect subViewFrame = [_fileDestinationView frame];
    subViewFrame.origin.x = (boxFrame.size.width - subViewFrame.size.width) / 2;
    subViewFrame.origin.y = ((boxFrame.size.height - subViewFrame.size.height) / 2) - 15.;
    [_fileDestinationView setFrame: subViewFrame];
    [[_destinationFileButton animator] setHidden: YES];
    [[_destinationStreamButton animator] setHidden: YES];
    [_destinationBox performSelector:@selector(addSubview:) withObject:_fileDestinationView afterDelay:0.2];
    [[_destinationCancelBtn animator] setHidden:NO];
    b_streaming = NO;
    [_okButton setTitle:_NS("Save")];
}

- (IBAction)iWantAStream:(id)sender
{
    NSRect boxFrame = [_destinationBox frame];
    NSRect subViewFrame = [_streamDestinationView frame];
    subViewFrame.origin.x = (boxFrame.size.width - subViewFrame.size.width) / 2;
    subViewFrame.origin.y = ((boxFrame.size.height - subViewFrame.size.height) / 2) - 15.;
    [_streamDestinationView setFrame: subViewFrame];
    [[_destinationFileButton animator] setHidden: YES];
    [[_destinationStreamButton animator] setHidden: YES];
    [_destinationBox performSelector:@selector(addSubview:) withObject:_streamDestinationView afterDelay:0.2];
    [[_destinationCancelBtn animator] setHidden:NO];
    b_streaming = YES;
    [_okButton setTitle:_NS("Stream")];
}

- (void)resetDestination
{
    [self setOutputDestination:@""];

    // File panel
    [[_fileDestinationFileName animator] setHidden: YES];
    [[_fileDestinationFileNameStub animator] setHidden: NO];

    // Stream panel
    [_streamDestinationURLLabel setStringValue:_NS("Select Streaming Method")];
    b_streaming = NO;

    [self updateOKButton];
}

- (IBAction)cancelDestination:(id)sender
{
    if ([_streamDestinationView superview] != nil)
        [_streamDestinationView removeFromSuperview];
    if ([_fileDestinationView superview] != nil)
        [_fileDestinationView removeFromSuperview];

    [_destinationCancelBtn setHidden:YES];
    [[_destinationFileButton animator] setHidden: NO];
    [[_destinationStreamButton animator] setHidden: NO];

    [self resetDestination];
}

- (IBAction)browseFileDestination:(id)sender
{
    NSSavePanel * saveFilePanel = [NSSavePanel savePanel];
    [saveFilePanel setCanSelectHiddenExtension: YES];
    [saveFilePanel setCanCreateDirectories: YES];
    if ([[_customizeEncapMatrix selectedCell] tag] != RAW) // there is no clever guess for this
        [saveFilePanel setAllowedFileTypes:[NSArray arrayWithObject:[self currentEncapsulationFormatAsFileExtension:YES]]];
    [saveFilePanel beginSheetModalForWindow:self.window completionHandler:^(NSInteger returnCode) {
        if (returnCode == NSModalResponseOK) {
            [self setOutputDestination:[[saveFilePanel URL] path]];
            [self->_fileDestinationFileName setStringValue: [[NSFileManager defaultManager] displayNameAtPath:self->_outputDestination]];
            [[self->_fileDestinationFileNameStub animator] setHidden: YES];
            [[self->_fileDestinationFileName animator] setHidden: NO];
        }

        [self updateOKButton];
    }];
}

#pragma mark -
#pragma mark User interaction - customization panel

- (IBAction)customizeProfile:(id)sender
{
    [self.window beginSheet:_customizePanel completionHandler:nil];
}

- (IBAction)closeCustomizationSheet:(id)sender
{
    [_customizePanel orderOut:sender];
    [NSApp endSheet: _customizePanel];

    if (sender == _customizeOkButton)
        [self updateCurrentProfile];
}

- (IBAction)videoSettingsChanged:(id)sender
{
    bool enableSettings = [_customizeVidCheckbox state] == NSOnState && [_customizeVidKeepCheckbox state] == NSOffState;
    [_customizeVidSettingsBox enableSubviews:enableSettings];
    [_customizeVidKeepCheckbox setEnabled:[_customizeVidCheckbox state] == NSOnState];
}

- (IBAction)audioSettingsChanged:(id)sender
{
    bool enableSettings = [_customizeAudCheckbox state] == NSOnState && [_customizeAudKeepCheckbox state] == NSOffState;
    [_customizeAudSettingsBox enableSubviews:enableSettings];
    [_customizeAudKeepCheckbox setEnabled:[_customizeAudCheckbox state] == NSOnState];
}

- (IBAction)subSettingsChanged:(id)sender
{
    bool enableSettings = [_customizeSubsCheckbox state] == NSOnState;
    [_customizeSubsOverlayCheckbox setEnabled:enableSettings];
    [_customizeSubsPopup setEnabled:enableSettings];
}

- (IBAction)newProfileAction:(id)sender
{
    /* show panel */
    [_textfieldPanel setTitleString: _NS("Save as new profile")];
    [_textfieldPanel setSubTitleString: _NS("Enter a name for the new profile:")];
    [_textfieldPanel setCancelButtonString: _NS("Cancel")];
    [_textfieldPanel setOkButtonString: _NS("Save")];

    __weak typeof(self) _self = self;
    [_textfieldPanel runModalForWindow:_customizePanel completionHandler:^(NSInteger returnCode, NSString *resultingText) {
        if (returnCode != NSModalResponseOK || [resultingText length] == 0)
            return;

        /* prepare current data */
        [_self updateCurrentProfile];

        /* add profile to arrays */
        NSMutableArray * workArray = [[NSMutableArray alloc] initWithArray:self.profileNames];
        [workArray addObject:resultingText];
        [_self setProfileNames:[[NSArray alloc] initWithArray:workArray]];

        workArray = [[NSMutableArray alloc] initWithArray:self.profileValueList];
        [workArray addObject:[self.currentProfile componentsJoinedByString:@";"]];
        [_self setProfileValueList:[[NSArray alloc] initWithArray:workArray]];

        /* update UI */
        [_self recreateProfilePopup];
        [self->_profilePopup selectItemWithTitle:resultingText];

        /* update internals */
        [_self switchProfile:self];
        [_self storeProfilesOnDisk];
    }];
}

#pragma mark -
#pragma mark User interaction - stream panel

- (IBAction)showStreamPanel:(id)sender
{
    [self.window beginSheet:_streamPanel completionHandler:nil];
}

- (IBAction)closeStreamPanel:(id)sender
{
    [_streamPanel orderOut:sender];
    [NSApp endSheet: _streamPanel];

    if (sender == _streamCancelButton)
        return;

    /* provide a summary of the user selections */
    NSMutableString * labelContent = [[NSMutableString alloc] initWithFormat:_NS("%@ stream to %@:%@"), [_streamTypePopup titleOfSelectedItem], [_streamAddressField stringValue], [_streamPortField stringValue]];

    if ([_streamTypePopup indexOfSelectedItem] > 1)
        [labelContent appendFormat:@" (\"%@\")", [_streamChannelField stringValue]];

    [_streamDestinationURLLabel setStringValue:labelContent];

    /* catch obvious errors */
    if ([[_streamAddressField stringValue] length] == 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSInformationalAlertStyle];
        [alert setMessageText:_NS("No Address given")];
        [alert setInformativeText:_NS("In order to stream, a valid destination address is required.")];
        [alert beginSheetModalForWindow:_streamPanel
                      completionHandler:nil];
        return;
    }

    if ([_streamSAPCheckbox state] && [[_streamChannelField stringValue] length] == 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSInformationalAlertStyle];
        [alert setMessageText:_NS("No Channel Name given")];
        [alert setInformativeText:_NS("SAP stream announcement is enabled. However, no channel name is provided.")];
        [alert beginSheetModalForWindow:_streamPanel
                      completionHandler:nil];
        return;
    }

    if ([_streamSDPMatrix isEnabled] && [_streamSDPMatrix selectedCell] != [_streamSDPMatrix cellWithTag:0] && [[_streamSDPField stringValue] length] == 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSInformationalAlertStyle];
        [alert setMessageText:_NS("No SDP URL given")];
        [alert setInformativeText:_NS("A SDP export is requested, but no URL is provided.")];
        [alert beginSheetModalForWindow:_streamPanel
                      completionHandler:nil];
        return;
    }

    /* store destination for further reference and update UI */
    [self setOutputDestination: [_streamAddressField stringValue]];
    [self updateOKButton];
}

- (IBAction)streamTypeToggle:(id)sender
{
    NSUInteger index = [_streamTypePopup indexOfSelectedItem];
    if (index <= 1) { // HTTP, MMSH
        [_streamTTLField setEnabled:NO];
        [_streamTTLStepper setEnabled:NO];
        [_streamSAPCheckbox setEnabled:NO];
        [_streamSDPMatrix setEnabled:NO];
    } else if (index == 2) { // RTP
        [_streamTTLField setEnabled:YES];
        [_streamTTLStepper setEnabled:YES];
        [_streamSAPCheckbox setEnabled:YES];
        [_streamSDPMatrix setEnabled:YES];
    } else { // UDP
        [_streamTTLField setEnabled:YES];
        [_streamTTLStepper setEnabled:YES];
        [_streamSAPCheckbox setEnabled:YES];
        [_streamSDPMatrix setEnabled:NO];
    }
    [self streamAnnouncementToggle:sender];
}

- (IBAction)streamAnnouncementToggle:(id)sender
{
    [_streamChannelField setEnabled:[_streamSAPCheckbox state] && [_streamSAPCheckbox isEnabled]];
    [_streamSDPField setEnabled:[_streamSDPMatrix isEnabled] && ([_streamSDPMatrix selectedCell] != [_streamSDPMatrix cellWithTag:0])];

    if ([[_streamSDPMatrix selectedCell] tag] == 3)
        [_streamSDPFileBrowseButton setEnabled: YES];
    else
        [_streamSDPFileBrowseButton setEnabled: NO];
}

- (IBAction)sdpFileLocationSelector:(id)sender
{
    NSSavePanel * saveFilePanel = [NSSavePanel savePanel];
    [saveFilePanel setCanSelectHiddenExtension: YES];
    [saveFilePanel setCanCreateDirectories: YES];
    [saveFilePanel setAllowedFileTypes:[NSArray arrayWithObject:@"sdp"]];
    [saveFilePanel beginSheetModalForWindow:_streamPanel completionHandler:^(NSInteger returnCode) {
        if (returnCode == NSModalResponseOK)
            [self->_streamSDPField setStringValue:[[saveFilePanel URL] path]];
    }];
}

#pragma mark -
#pragma mark User interaction - misc

- (BOOL)handlePasteBoardFromDragSession:(NSPasteboard *)paste
{
    NSArray *types = [NSArray arrayWithObjects:NSFilenamesPboardType, nil];
    NSString *desired_type = [paste availableTypeFromArray: types];
    NSData *carried_data = [paste dataForType: desired_type];

    if (carried_data == nil)
        return NO;

    if ([desired_type isEqualToString:NSFilenamesPboardType]) {
        NSArray *values = [[paste propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

        if ([values count] > 0) {
            NSURL *url = [NSURL fileURLWithPath:[values firstObject] isDirectory:NO];
            if (!url) {
                return NO;
            }
            [self setMRL:url.absoluteString];
            [self updateOKButton];
            [self updateDropView];
            return YES;
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
        [_dropinMediaLabel setStringValue: [[NSFileManager defaultManager] displayNameAtPath: path]];
        NSImage * image = [[NSWorkspace sharedWorkspace] iconForFile: path];
        [image setSize:NSMakeSize(128,128)];
        [_dropinIcon setImage: image];

        if (![_dropinView superview]) {
            NSRect boxFrame = [_dropBox frame];
            NSRect subViewFrame = [_dropinView frame];
            subViewFrame.origin.x = (boxFrame.size.width - subViewFrame.size.width) / 2;
            subViewFrame.origin.y = (boxFrame.size.height - subViewFrame.size.height) / 2;
            [_dropinView setFrame: subViewFrame];
            [[_dropImage animator] setHidden: YES];
            [_dropBox performSelector:@selector(addSubview:) withObject:_dropinView afterDelay:0.6];
        }
    } else {
        [_dropinView removeFromSuperview];
        [[_dropImage animator] setHidden: NO];
    }
}

- (void)updateOKButton
{
    if ([_outputDestination length] > 0 && [_MRL length] > 0)
        [_okButton setEnabled: YES];
    else
        [_okButton setEnabled: NO];
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
        msg_Err(getIntf(), "CAS: the requested profile '%s' is invalid", [profileString UTF8String]);
        return;
    }

    [self selectCellByEncapsulationFormat:[components firstObject]];
    [_customizeVidCheckbox setState:[[components objectAtIndex:1] intValue]];
    [_customizeAudCheckbox setState:[[components objectAtIndex:2] intValue]];
    [_customizeSubsCheckbox setState:[[components objectAtIndex:3] intValue]];
    [self setVidBitrate:[[components objectAtIndex:5] intValue]];
    [_customizeVidScalePopup selectItemWithTitle:[components objectAtIndex:6]];
    [self setVidFramerate:[[components objectAtIndex:7] intValue]];
    [_customizeVidWidthField setStringValue:[components objectAtIndex:8]];
    [_customizeVidHeightField setStringValue:[components objectAtIndex:9]];
    [self setAudBitrate:[[components objectAtIndex:11] intValue]];
    [self setAudChannels:[[components objectAtIndex:12] intValue]];
    [_customizeAudSampleratePopup selectItemWithTitle:[components objectAtIndex:13]];
    [_customizeSubsOverlayCheckbox setState:[[components objectAtIndex:15] intValue]];

    /* since there is no proper lookup mechanism in arrays, we need to implement a string specific one ourselves */
    NSArray * tempArray = [_videoCodecs objectAtIndex:1];
    NSUInteger count = [tempArray count];
    NSString * searchString = [components objectAtIndex:4];
    int videoKeep = [searchString isEqualToString:@"copy"];
    [_customizeVidKeepCheckbox setState:videoKeep];
    if ([searchString isEqualToString:@"none"] || [searchString isEqualToString:@"0"] || videoKeep) {
        [_customizeVidCodecPopup selectItemAtIndex:-1];
    } else {
        for (NSUInteger x = 0; x < count; x++) {
            if ([[tempArray objectAtIndex:x] isEqualToString: searchString]) {
                [_customizeVidCodecPopup selectItemAtIndex:x];
                break;
            }
        }
    }

    tempArray = [_audioCodecs objectAtIndex:1];
    count = [tempArray count];
    searchString = [components objectAtIndex:10];
    int audioKeep = [searchString isEqualToString:@"copy"];
    [_customizeAudKeepCheckbox setState:audioKeep];
    if ([searchString isEqualToString:@"none"] || [searchString isEqualToString:@"0"] || audioKeep) {
        [_customizeAudCodecPopup selectItemAtIndex:-1];
    } else {
        for (NSUInteger x = 0; x < count; x++) {
            if ([[tempArray objectAtIndex:x] isEqualToString: searchString]) {
                [_customizeAudCodecPopup selectItemAtIndex:x];
                break;
            }
        }
    }

    tempArray = [_subsCodecs objectAtIndex:1];
    count = [tempArray count];
    searchString = [components objectAtIndex:14];
    if ([searchString isEqualToString:@"none"] || [searchString isEqualToString:@"0"]) {
        [_customizeSubsPopup selectItemAtIndex:-1];
    } else {
        for (NSUInteger x = 0; x < count; x++) {
            if ([[tempArray objectAtIndex:x] isEqualToString: searchString]) {
                [_customizeSubsPopup selectItemAtIndex:x];
                break;
            }
        }
    }

    [self videoSettingsChanged:nil];
    [self audioSettingsChanged:nil];
    [self subSettingsChanged:nil];

    [self setCurrentProfile: [[NSMutableArray alloc] initWithArray:[profileString componentsSeparatedByString:@";"]]];
}

- (void)selectCellByEncapsulationFormat:(NSString *)format
{
    if ([format isEqualToString:@"ts"])
        [_customizeEncapMatrix selectCellWithTag:MPEGTS];
    else if ([format isEqualToString:@"webm"])
        [_customizeEncapMatrix selectCellWithTag:WEBM];
    else if ([format isEqualToString:@"ogg"])
        [_customizeEncapMatrix selectCellWithTag:OGG];
    else if ([format isEqualToString:@"ogm"])
        [_customizeEncapMatrix selectCellWithTag:OGG];
    else if ([format isEqualToString:@"mp4"])
        [_customizeEncapMatrix selectCellWithTag:MP4];
    else if ([format isEqualToString:@"mov"])
        [_customizeEncapMatrix selectCellWithTag:MP4];
    else if ([format isEqualToString:@"ps"])
        [_customizeEncapMatrix selectCellWithTag:MPEGPS];
    else if ([format isEqualToString:@"mpjpeg"])
        [_customizeEncapMatrix selectCellWithTag:MJPEG];
    else if ([format isEqualToString:@"wav"])
        [_customizeEncapMatrix selectCellWithTag:WAV];
    else if ([format isEqualToString:@"flv"])
        [_customizeEncapMatrix selectCellWithTag:FLV];
    else if ([format isEqualToString:@"mpeg1"])
        [_customizeEncapMatrix selectCellWithTag:MPEG1];
    else if ([format isEqualToString:@"mkv"])
        [_customizeEncapMatrix selectCellWithTag:MKV];
    else if ([format isEqualToString:@"raw"])
        [_customizeEncapMatrix selectCellWithTag:RAW];
    else if ([format isEqualToString:@"avi"])
        [_customizeEncapMatrix selectCellWithTag:AVI];
    else if ([format isEqualToString:@"asf"])
        [_customizeEncapMatrix selectCellWithTag:ASF];
    else if ([format isEqualToString:@"wmv"])
        [_customizeEncapMatrix selectCellWithTag:ASF];
    else
        msg_Err(getIntf(), "CAS: unknown encap format requested for customization");
}

- (NSString *)currentEncapsulationFormatAsFileExtension:(BOOL)b_extension
{
    NSUInteger cellTag = (NSUInteger) [[_customizeEncapMatrix selectedCell] tag];
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
    BOOL haveVideo = YES;
    if ([[self.currentProfile objectAtIndex:1] intValue]) {
        // video is enabled
        if (![[self.currentProfile objectAtIndex:4] isEqualToString:@"copy"]) {
            [composedOptions appendFormat:@"vcodec=%@", [self.currentProfile objectAtIndex:4]];
            if ([[self.currentProfile objectAtIndex:5] intValue] > 0) // bitrate
                [composedOptions appendFormat:@",vb=%@", [self.currentProfile objectAtIndex:5]];
            if ([[self.currentProfile objectAtIndex:6] floatValue] > 0.) // scale
                [composedOptions appendFormat:@",scale=%@", [self.currentProfile objectAtIndex:6]];
            if ([[self.currentProfile objectAtIndex:7] floatValue] > 0.) // fps
                [composedOptions appendFormat:@",fps=%@", [self.currentProfile objectAtIndex:7]];
            if ([[self.currentProfile objectAtIndex:8] intValue] > 0) // width
                [composedOptions appendFormat:@",width=%@", [self.currentProfile objectAtIndex:8]];
            if ([[self.currentProfile objectAtIndex:9] intValue] > 0) // height
                [composedOptions appendFormat:@",height=%@", [self.currentProfile objectAtIndex:9]];
        } else {
            haveVideo = NO;
        }
    } else {
        [composedOptions appendString:@"vcodec=none"];
    }

    BOOL haveAudio = YES;
    if ([[self.currentProfile objectAtIndex:2] intValue]) {
        // audio is enabled
        if (![[self.currentProfile objectAtIndex:10] isEqualToString:@"copy"]) {
            if(haveVideo)
                [composedOptions appendString:@","];
            [composedOptions appendFormat:@"acodec=%@", [self.currentProfile objectAtIndex:10]];
            [composedOptions appendFormat:@",ab=%@", [self.currentProfile objectAtIndex:11]]; // bitrate
            [composedOptions appendFormat:@",channels=%@", [self.currentProfile objectAtIndex:12]]; // channel number
            [composedOptions appendFormat:@",samplerate=%@", [self.currentProfile objectAtIndex:13]]; // sample rate
        } else {
            haveAudio = NO;
        }
    } else {
        if(haveVideo)
            [composedOptions appendString:@","];

        [composedOptions appendString:@"acodec=none"];
    }
    if ([self.currentProfile objectAtIndex:3]) {
        if(haveVideo || haveAudio)
            [composedOptions appendString:@","];
        // subtitles enabled
        [composedOptions appendFormat:@"scodec=%@", [self.currentProfile objectAtIndex:14]];
        if ([[self.currentProfile objectAtIndex:15] intValue])
            [composedOptions appendFormat:@",soverlay"];
    }

    // Close transcode
    [composedOptions appendString:@"}"];

    if (!b_streaming) {
        /* file transcoding */
        // add muxer
        [composedOptions appendFormat:@":standard{mux=%@", [self.currentProfile firstObject]];


        // add output destination
        _outputDestination = [_outputDestination stringByReplacingOccurrencesOfString:@"\""
                                                                           withString:@"\\\""];
        [composedOptions appendFormat:@",access=file{no-overwrite},dst=\"%@\"}", _outputDestination];
    } else {
        NSString *destination = [NSString stringWithFormat:@"\"%@:%@\"", _outputDestination, [_streamPortField stringValue]];

        /* streaming */
        if ([[[_streamTypePopup selectedItem] title] isEqualToString:@"RTP"])
            [composedOptions appendFormat:@":rtp{mux=ts,dst=%@,port=%@", _outputDestination, [_streamPortField stringValue]];
        else if ([[[_streamTypePopup selectedItem] title] isEqualToString:@"UDP"])
            [composedOptions appendFormat:@":standard{mux=ts,dst=%@,access=udp", destination];
        else if ([[[_streamTypePopup selectedItem] title] isEqualToString:@"MMSH"])
            [composedOptions appendFormat:@":standard{mux=asfh,dst=%@,access=mmsh", destination];
        else
            [composedOptions appendFormat:@":standard{mux=%@,dst=%@,access=http", [self.currentProfile firstObject], destination];

        if ([_streamSAPCheckbox state])
            [composedOptions appendFormat:@",sap,name=\"%@\"", [_streamChannelField stringValue]];
        if ([_streamSDPMatrix selectedCell] != [_streamSDPMatrix cellWithTag:0]) {
            NSInteger tag = [[_streamSDPMatrix selectedCell] tag];
            switch (tag) {
                case 1:
                    [composedOptions appendFormat:@",sdp=\"http://%@\"", [_streamSDPField stringValue]];
                    break;
                case 2:
                    [composedOptions appendFormat:@",sdp=\"rtsp://%@\"", [_streamSDPField stringValue]];
                    break;
                case 3:
                    [composedOptions appendFormat:@",sdp=\"file://%s\"", vlc_path2uri([[_streamSDPField stringValue] UTF8String], NULL)];
                default:
                    break;
            }
        }

        [composedOptions appendString:@"}"];
    }

    return [NSString stringWithString:composedOptions];
}

- (void)updateCurrentProfile
{
    [self.currentProfile removeAllObjects];

    NSInteger i;
    [self.currentProfile addObject: [self currentEncapsulationFormatAsFileExtension:NO]];
    [self.currentProfile addObject: [NSString stringWithFormat:@"%li", [_customizeVidCheckbox state]]];
    [self.currentProfile addObject: [NSString stringWithFormat:@"%li", [_customizeAudCheckbox state]]];
    [self.currentProfile addObject: [NSString stringWithFormat:@"%li", [_customizeSubsCheckbox state]]];
    
    NSString *videoCodec;
    if([_customizeVidKeepCheckbox state] == NSOnState)
        videoCodec = @"copy";
    else {
        i = [_customizeVidCodecPopup indexOfSelectedItem];
        if (i >= 0)
            videoCodec = [[_videoCodecs objectAtIndex:1] objectAtIndex:i];
        else
            videoCodec = @"none";
    }
    [self.currentProfile addObject: videoCodec];

    [self.currentProfile addObject: [NSString stringWithFormat:@"%i", [self vidBitrate]]];
    [self.currentProfile addObject: [NSString stringWithFormat:@"%i", [[[_customizeVidScalePopup selectedItem] title] intValue]]];
    [self.currentProfile addObject: [NSString stringWithFormat:@"%i", [self vidFramerate]]];
    [self.currentProfile addObject: [NSString stringWithFormat:@"%i", [_customizeVidWidthField intValue]]];
    [self.currentProfile addObject: [NSString stringWithFormat:@"%i", [_customizeVidHeightField intValue]]];

    NSString *audioCodec;
    if([_customizeAudKeepCheckbox state] == NSOnState)
        audioCodec = @"copy";
    else {
        i = [_customizeAudCodecPopup indexOfSelectedItem];
        if (i >= 0)
            audioCodec = [[_audioCodecs objectAtIndex:1] objectAtIndex:i];
        else
            audioCodec = @"none";
    }
    [self.currentProfile addObject: audioCodec];
    
    [self.currentProfile addObject: [NSString stringWithFormat:@"%i", [self audBitrate]]];
    [self.currentProfile addObject: [NSString stringWithFormat:@"%i", [self audChannels]]];
    [self.currentProfile addObject: [[_customizeAudSampleratePopup selectedItem] title]];
    i = [_customizeSubsPopup indexOfSelectedItem];
    if (i >= 0)
        [self.currentProfile addObject: [[_subsCodecs objectAtIndex:1] objectAtIndex:i]];
    else
        [self.currentProfile addObject: @"none"];
    [self.currentProfile addObject: [NSString stringWithFormat:@"%li", [_customizeSubsOverlayCheckbox state]]];
}

- (void)storeProfilesOnDisk
{
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:_profileNames forKey:VLCConvertAndSaveProfileNamesKey];
    [defaults setObject:_profileValueList forKey:VLCConvertAndSaveProfilesKey];
}

- (void)recreateProfilePopup
{
    [_profilePopup removeAllItems];
    [_profilePopup addItemsWithTitles:self.profileNames];
    [_profilePopup addItemWithTitle:_NS("Custom")];
    [[_profilePopup menu] addItem:[NSMenuItem separatorItem]];
    [_profilePopup addItemWithTitle:_NS("Organize Profiles...")];
    [[_profilePopup lastItem] setTarget: self];
    [[_profilePopup lastItem] setAction: @selector(deleteProfileAction:)];
}

- (IBAction)customizeSubsCheckbox:(id)sender {
}
@end
