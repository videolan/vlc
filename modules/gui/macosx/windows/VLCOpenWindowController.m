/*****************************************************************************
 * VLCOpenWindowController.m: Open dialogues for VLC's MacOS X port
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Benjamin Pracht <bigben at videolan dot org>
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

#import "VLCOpenWindowController.h"

#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>

#import <vlc_common.h>
#import <vlc_url.h>

#import "extensions/NSString+Helpers.h"
#import "extensions/VLCPositionFormatter.h"
#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "windows/convertandsave/VLCOutput.h"
#import "windows/VLCOpenInputMetadata.h"

NSString *const VLCOpenFileTabViewId = @"file";
NSString *const VLCOpenDiscTabViewId = @"disc";
NSString *const VLCOpenNetworkTabViewId = @"network";
NSString *const VLCOpenCaptureTabViewId = @"capture";
NSString *const VLCOpenTextFieldWasClicked = @"VLCOpenTextFieldWasClicked";

@interface VLCOpenBlockDeviceDescription : NSObject

@property (readwrite, retain) NSString *path;
@property (readwrite, retain) NSString *devicePath;
@property (readwrite, retain) NSString *mediaType;
@property (readwrite, retain) NSImage *mediaIcon;

@end

@implementation VLCOpenBlockDeviceDescription
@end

@interface VLCOpenDisplayInformation : NSObject

@property (readwrite) CGRect displayBounds;
@property (readwrite) CGDirectDisplayID displayID;

@end

@implementation VLCOpenDisplayInformation
@end

@interface VLCOpenTextField : NSTextField

@end

@implementation VLCOpenTextField

- (void)mouseDown:(NSEvent *)theEvent
{
    [[NSNotificationCenter defaultCenter] postNotificationName: VLCOpenTextFieldWasClicked
                                                        object: self];
    [super mouseDown: theEvent];
}

@end

@interface VLCOpenWindowController()
{
    VLCOutput *_output;
    BOOL b_outputNibLoaded;
    NSArray *_avvideoDevices;
    NSArray *_avaudioDevices;
    NSString *_avCurrentDeviceUID;
    NSString *_avCurrentAudioDeviceUID;

    BOOL b_autoplay;
    BOOL b_nodvdmenus;
    NSView *_currentOpticalMediaView;
    NSImageView *_currentOpticalMediaIconView;
    NSMutableArray <VLCOpenBlockDeviceDescription *>*_allMediaDevices;
    NSArray <VLCOpenBlockDeviceDescription *>*_opticalDevices;
    NSMutableArray <VLCOpenBlockDeviceDescription *>*_specialMediaFolders;
    NSString *_filePath;
    NSString *_fileSlavePath;
    NSString *_subPath;
    NSString *_MRL;
    NSMutableArray *_displayInfos;
}

@property (readwrite, assign) NSString *MRL;

@end

@implementation VLCOpenWindowController

#pragma mark -
#pragma mark Init

- (id)init
{
    self = [super initWithWindowNibName:@"Open"];
    return self;
}

- (void)windowDidLoad
{
    _output = [VLCOutput new];

    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [self initStrings];

    // setup start / stop time fields
    [_fileStartTimeTextField setFormatter:[[VLCPositionFormatter alloc] init]];
    [_fileStopTimeTextField setFormatter:[[VLCPositionFormatter alloc] init]];

    // Auto collapse MRL field
    self.mrlViewHeightConstraint.constant = 0;

    [self updateVideoDevicesAndRepresentation];

    [self updateAudioDevicesAndRepresentation];

    [self setupSubtitlesPanel];

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver: self
                           selector: @selector(openNetInfoChanged:)
                               name: NSControlTextDidChangeNotification
                             object: _netUDPPortTextField];
    [notificationCenter addObserver: self
                           selector: @selector(openNetInfoChanged:)
                               name: NSControlTextDidChangeNotification
                             object: _netUDPMAddressTextField];
    [notificationCenter addObserver: self
                           selector: @selector(openNetInfoChanged:)
                               name: NSControlTextDidChangeNotification
                             object: _netUDPMPortTextField];
    [notificationCenter addObserver: self
                           selector: @selector(openNetInfoChanged:)
                               name: NSControlTextDidChangeNotification
                             object: _netHTTPURLTextField];

    [notificationCenter addObserver: self
                           selector: @selector(screenFPSfieldChanged:)
                               name: NSControlTextDidChangeNotification
                             object: _screenFPSTextField];

    /* register clicks on text fields */
    [notificationCenter addObserver: self
                           selector: @selector(textFieldWasClicked:)
                               name: VLCOpenTextFieldWasClicked
                             object: nil];

    /* we want to be notified about removed or added media */
    _allMediaDevices = [[NSMutableArray alloc] init];
    _specialMediaFolders = [[NSMutableArray alloc] init];
    _displayInfos = [[NSMutableArray alloc] init];
    NSNotificationCenter *sharedNotificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
    [sharedNotificationCenter addObserver:self selector:@selector(scanOpticalMedia:) name:NSWorkspaceDidMountNotification object:nil];
    [sharedNotificationCenter addObserver:self selector:@selector(scanOpticalMedia:) name:NSWorkspaceDidUnmountNotification object:nil];

    [self qtkToggleUIElements:nil];
    [self updateMediaSelector:NO];
    [self scanOpticalMedia:nil];

    [self setMRL: @""];
}

- (void)initStrings
{
    [self.window setTitle: _NS("Open Source")];
    [_mrlButtonLabel setTitle: _NS("Media Resource Locator (MRL)")];

    [_okButton setTitle: _NS("Open")];
    [_cancelButton setTitle: _NS("Cancel")];

    [_outputCheckbox setTitle:_NS("Stream output:")];
    [_outputSettingsButton setTitle:_NS("Settings...")];

    _tabView.accessibilityLabel = _NS("Choose media input type");
    [[_tabView tabViewItemAtIndex: 0] setLabel: _NS("File")];
    [[_tabView tabViewItemAtIndex: 1] setLabel: _NS("Disc")];
    [[_tabView tabViewItemAtIndex: 2] setLabel: _NS("Network")];
    [[_tabView tabViewItemAtIndex: 3] setLabel: _NS("Capture")];
    [_fileNameLabel setStringValue: @""];
    [_fileNameStubLabel setStringValue: _NS("Choose a file")];
    [_fileIconWell setImage: [NSImage imageNamed:@"generic"]];
    [_fileBrowseButton setTitle: _NS("Browse...")];
    _fileBrowseButton.accessibilityLabel = _NS("Select a file for playback");
    [_fileTreatAsPipeButton setTitle: _NS("Treat as a pipe rather than as a file")];
    [_fileTreatAsPipeButton setHidden: NO];
    [_fileSlaveCheckbox setTitle: _NS("Play another media synchronously")];
    [_fileSelectSlaveButton setTitle: _NS("Choose...")];
    _fileBrowseButton.accessibilityLabel = _NS("Select another file to play in sync with the previously selected file");
    [_fileSlaveFilenameLabel setStringValue: @""];
    [_fileSlaveIconWell setImage: NULL];
    [_fileSubtitlesFilenameLabel setStringValue: @""];
    [_fileSubtitlesIconWell setImage: NULL];
    [_fileCustomTimingCheckbox setTitle: _NS("Custom playback")];
    [_fileStartTimeLabel setStringValue: _NS("Start time")];
    [_fileStartTimeTextField setStringValue: @""];
    [_fileStopTimeLabel setStringValue: _NS("Stop time")];
    [_fileStopTimeTextField setStringValue: @""];

    [_discSelectorPopup removeAllItems];
    [_discSelectorPopup setHidden: NO];
    NSString *oVideoTS = _NS("Open VIDEO_TS / BDMV folder");
    [_discNoDiscLabel setStringValue: _NS("Insert Disc")];
    [_discNoDiscVideoTSButton setTitle: oVideoTS];
    [_discAudioCDLabel setStringValue: _NS("Audio CD")];
    [_discAudioCDTrackCountLabel setStringValue: @""];
    [_discAudioCDVideoTSButton setTitle: oVideoTS];
    [_discDVDLabel setStringValue: @""];
    [_discDVDDisableMenusButton setTitle: _NS("Disable DVD menus")];
    [_discDVDVideoTSButton setTitle: oVideoTS];
    [_discDVDwomenusLabel setStringValue: @""];
    [_discDVDwomenusEnableMenusButton setTitle: _NS("Enable DVD menus")];
    [_discDVDwomenusVideoTSButton setTitle: oVideoTS];
    [_discDVDwomenusTitleLabel setStringValue: _NS("Title")];
    [_discDVDwomenusChapterLabel setStringValue: _NS("Chapter")];
    [_discVCDTitleLabel setStringValue: _NS("Title")];
    [_discVCDChapterLabel setStringValue: _NS("Chapter")];
    [_discVCDVideoTSButton setTitle: oVideoTS];
    [_discBDVideoTSButton setTitle: oVideoTS];

    [_netUDPPortLabel setStringValue: _NS("Port")];
    [_netUDPMAddressLabel setStringValue: _NS("IP Address")];
    [_netUDPMPortLabel setStringValue: _NS("Port")];
    [_netHTTPURLLabel setStringValue: _NS("URL")];
    [_netHelpLabel setStringValue: _NS("To Open a usual network stream (HTTP, RTSP, RTMP, MMS, FTP, etc.), just enter the URL in the field above. If you want to open a RTP or UDP stream, press the button below.")];
    [_netHelpUDPLabel setStringValue: _NS("If you want to open a multicast stream, enter the respective IP address given by the stream provider. In unicast mode, VLC will use your machine's IP automatically.\n\nTo open a stream using a different protocol, just press Cancel to close this sheet.")];
    _netHTTPURLTextField.accessibilityLabel = _NS("Enter a stream URL here. To open RTP or UDP streams, use the respective button below.");
    [_netUDPCancelButton setTitle: _NS("Cancel")];
    [_netUDPOKButton setTitle: _NS("Open")];
    [_netOpenUDPButton setTitle: _NS("Open RTP/UDP Stream")];
    [_netUDPModeLabel setStringValue: _NS("Mode")];
    [_netUDPProtocolLabel setStringValue: _NS("Protocol")];
    [_netUDPAddressLabel setStringValue: _NS("Address")];

    [[_netModeMatrix cellAtRow:0 column:0] setTitle: _NS("Unicast")];
    [[_netModeMatrix cellAtRow:1 column:0] setTitle: _NS("Multicast")];

    [_netUDPPortTextField setIntegerValue: config_GetInt("server-port")];
    [_netUDPPortStepper setIntegerValue: config_GetInt("server-port")];

    [_captureModePopup removeAllItems];
    [_captureModePopup addItemWithTitle: _NS("Input Devices")];
    [_captureModePopup addItemWithTitle: _NS("Screen")];
    [_screenFPSLabel setStringValue: [NSString stringWithFormat:@"%@:",_NS("Frames per Second")]];
    [_screenLabel setStringValue: [NSString stringWithFormat:@"%@:",_NS("Screen")]];
    [_screenLeftLabel setStringValue: [NSString stringWithFormat:@"%@:",_NS("Subscreen left")]];
    [_screenTopLabel setStringValue: [NSString stringWithFormat:@"%@:",_NS("Subscreen top")]];
    [_screenWidthLabel setStringValue: [NSString stringWithFormat:@"%@:",_NS("Subscreen Width")]];
    [_screenHeightLabel setStringValue: [NSString stringWithFormat:@"%@:",_NS("Subscreen Height")]];
    [_screenFollowMouseCheckbox setTitle: _NS("Follow the mouse")];
    [_screenqtkAudioCheckbox setTitle: _NS("Capture Audio")];
}

- (void)setupSubtitlesPanel
{
    int i_index;
    module_config_t * p_item;

    [self initSubtitlesPanelStrings];

    [[_fileSubDelayTextField formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("s")]];
    [[_fileSubFPSTextField formatter] setFormat:[NSString stringWithFormat:@"#,##0.000 %@", _NS("fps")]];
    self.fileSubFps = 1.0;

    p_item = config_FindConfig("subsdec-encoding");

    if (p_item) {
        for (int i = 0; i < p_item->list_count; i++) {
            [_fileSubEncodingPopup addItemWithTitle: _NS(p_item->list_text[i])];
            [[_fileSubEncodingPopup lastItem] setRepresentedObject:[NSString stringWithFormat:@"%s", p_item->list.psz[i]]];
            if (p_item->value.psz && !strcmp(p_item->value.psz, p_item->list.psz[i]))
                [_fileSubEncodingPopup selectItem: [_fileSubEncodingPopup lastItem]];
        }

        if ([_fileSubEncodingPopup indexOfSelectedItem] < 0)
            [_fileSubEncodingPopup selectItemAtIndex:0];
    }

    p_item = config_FindConfig("subsdec-align");

    if (p_item) {
        for (i_index = 0; i_index < p_item->list_count; i_index++)
            [_fileSubAlignPopup addItemWithTitle: _NS(p_item->list_text[i_index])];

        [_fileSubAlignPopup selectItemAtIndex: p_item->value.i];
    }
}

- (void)initSubtitlesPanelStrings
{
    [_fileSubCheckbox setTitle: _NS("Add Subtitle File:")];
    [_fileSubPathLabel setStringValue: _NS("Choose a file")];
    [_fileSubPathLabel setHidden: NO];
    [_fileSubPathTextField setStringValue: @""];
    [_fileSubSettingsButton setTitle: _NS("Choose...")];
    _fileSubSettingsButton.accessibilityLabel = _NS("Setup subtitle playback details");
    _fileBrowseButton.accessibilityLabel = _NS("Select a file for playback");
    [_fileSubBrowseButton setTitle: _NS("Browse...")];
    _fileSubBrowseButton.accessibilityLabel = _NS("Select a subtitle file");
    [_fileSubOverrideCheckbox setTitle: _NS("Override parameters")];
    [_fileSubDelayLabel setStringValue: _NS("Delay")];
    [_fileSubDelayStepper setEnabled: NO];
    [_fileSubFPSLabel setStringValue: _NS("FPS")];
    [_fileSubFPSStepper setEnabled: NO];
    [_fileSubEncodingLabel setStringValue: _NS("Subtitle encoding")];
    [_fileSubEncodingPopup removeAllItems];
    [_fileSubAlignLabel setStringValue: _NS("Subtitle alignment")];
    [_fileSubAlignPopup removeAllItems];
    [_fileSubOKButton setStringValue: _NS("OK")];
    _fileSubOKButton.accessibilityLabel = _NS("Dismiss the subtitle setup dialog");
    [_fileSubFontBox setTitle: _NS("Font Properties")];
    [_fileSubFileBox setTitle: _NS("Subtitle File")];
}

#pragma mark - property handling

- (void)setMRL:(NSString *)newMRL
{
    if (!newMRL)
        newMRL = @"";

    _MRL = newMRL;

    dispatch_async(dispatch_get_main_queue(), ^{
        [self.mrlTextField setStringValue:self.MRL];
        if ([self.MRL length] > 0)
            [self.okButton setEnabled: YES];
        else
            [self.okButton setEnabled: NO];
    });
}

- (NSString *)MRL
{
    return _MRL;
}

#pragma mark -
#pragma mark Main Actions

- (void)openTarget:(NSString *)identifier
{
    /* check whether we already run a modal dialog */
    if ([NSApp modalWindow] != nil)
        return;

    // load window
    [self window];

    [_tabView selectTabViewItemWithIdentifier:identifier];
    [_fileSubCheckbox setState: NSOffState];

    NSModalResponse i_result = [NSApp runModalForWindow: self.window];
    [self.window close];

    // Check if dialog was canceled or stopped (NSModalResponseStop)
    if (i_result <= 0)
        return;

    [self fetchMRLcreateOptionsAndStartPlayback];
}

- (void)fetchMRLcreateOptionsAndStartPlayback
{
    NSMutableArray *options = [NSMutableArray array];
    VLCOpenInputMetadata *inputMetadata = [[VLCOpenInputMetadata alloc] init];
    inputMetadata.MRLString = [self MRL];

    if ([_fileSubCheckbox state] == NSOnState) {
        [self addSubtitleOptionsToArray:options];
    }
    if ([_fileCustomTimingCheckbox state] == NSOnState) {
        [self addTimingOptionsToArray:options];
    }
    if ([_outputCheckbox state] == NSOnState) {
        [self addStreamOutputOptionsToArray:options];
    }
    if ([_fileSlaveCheckbox state] && _fileSlavePath)
        [options addObject: [NSString stringWithFormat: @"input-slave=%@", _fileSlavePath]];
    if ([[[_tabView selectedTabViewItem] identifier] isEqualToString: VLCOpenCaptureTabViewId]) {
        if ([[[_captureModePopup selectedItem] title] isEqualToString: _NS("Screen")]) {
            [self addScreenRecordingOptionsToArray:options];
        }
        else if ([[[_captureModePopup selectedItem] title] isEqualToString: _NS("Input Devices")]) {
            [self avDeviceOptionsToArray:options];
        }
    }

    /* apply the options to our item(s) */
    inputMetadata.playbackOptions = options;

    [[[VLCMain sharedInstance] playlistController] addPlaylistItems:@[inputMetadata]];
}

- (void)addSubtitleOptionsToArray:(NSMutableArray *)options
{
    [options addObject: [NSString stringWithFormat: @"sub-file=%@", _subPath]];
    if ([_fileSubOverrideCheckbox state] == NSOnState) {
        [options addObject: [NSString stringWithFormat: @"sub-delay=%f", ([self fileSubDelay] * 10)]];
        [options addObject: [NSString stringWithFormat: @"sub-fps=%f", [self fileSubFps]]];
    }
    [options addObject: [NSString stringWithFormat:
                         @"subsdec-encoding=%@", [[_fileSubEncodingPopup selectedItem] representedObject]]];
    [options addObject: [NSString stringWithFormat:
                         @"subsdec-align=%li", [_fileSubAlignPopup indexOfSelectedItem]]];
}

- (void)addTimingOptionsToArray:(NSMutableArray *)options
{
    NSInteger startTime = [NSString timeInSecondsFromStringWithColons:[_fileStartTimeTextField stringValue]];
    if (startTime > 0) {
        [options addObject: [NSString stringWithFormat:@"start-time=%li", startTime]];
    }

    NSInteger stopTime = [NSString timeInSecondsFromStringWithColons:[_fileStopTimeTextField stringValue]];
    if (stopTime > 0) {
        [options addObject: [NSString stringWithFormat:@"stop-time=%li", stopTime]];
    }
}

- (void)addStreamOutputOptionsToArray:(NSMutableArray *)options
{
    NSArray *soutMRL = [_output soutMRL];
    NSUInteger count = [soutMRL count];
    for (NSUInteger i = 0 ; i < count ; i++)
        [options addObject: [NSString stringWithString: [soutMRL objectAtIndex:i]]];
}

- (void)addScreenRecordingOptionsToArray:(NSMutableArray *)options
{
    NSInteger selected_index = [_screenPopup indexOfSelectedItem];
    VLCOpenDisplayInformation *displayInformation = [_displayInfos objectAtIndex:selected_index];

    [options addObject: [NSString stringWithFormat: @"screen-fps=%f", [_screenFPSTextField floatValue]]];
    [options addObject: [NSString stringWithFormat: @"screen-display-id=%i", displayInformation.displayID]];
    [options addObject: [NSString stringWithFormat: @"screen-left=%i", [_screenLeftTextField intValue]]];
    [options addObject: [NSString stringWithFormat: @"screen-top=%i", [_screenTopTextField intValue]]];
    [options addObject: [NSString stringWithFormat: @"screen-width=%i", [_screenWidthTextField intValue]]];
    [options addObject: [NSString stringWithFormat: @"screen-height=%i", [_screenHeightTextField intValue]]];
    if ([_screenFollowMouseCheckbox intValue] == YES)
        [options addObject: @"screen-follow-mouse"];
    else
        [options addObject: @"no-screen-follow-mouse"];
    if ([_screenqtkAudioCheckbox state] && _avCurrentAudioDeviceUID)
        [options addObject: [NSString stringWithFormat: @"input-slave=avaudiocapture://%@", _avCurrentAudioDeviceUID]];
}

- (void)avDeviceOptionsToArray:(NSMutableArray *)options
{
    if ([_qtkVideoCheckbox state]) {
        if ([_qtkAudioCheckbox state] && _avCurrentAudioDeviceUID)
            [options addObject: [NSString stringWithFormat: @"input-slave=avaudiocapture://%@", _avCurrentAudioDeviceUID]];
    }
}

#pragma mark - UI interaction

- (void)tabView:(NSTabView *)o_tv didSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
    NSString *identifier = [tabViewItem identifier];

    if ([identifier isEqualToString: VLCOpenFileTabViewId])
        [self openFilePathChanged: nil];
    else if ([identifier isEqualToString: VLCOpenDiscTabViewId])
        [self scanOpticalMedia: nil];
    else if ([identifier isEqualToString: VLCOpenNetworkTabViewId]) {
        [self openNetInfoChanged: nil];
        [_netHTTPURLTextField selectText:nil];
    } else if ([identifier isEqualToString: VLCOpenCaptureTabViewId])
        [self openCaptureModeChanged: nil];
}

- (IBAction)expandMRLfieldAction:(id)sender
{
    if ([_mrlButton state] == NSOffState) {
        self.mrlViewHeightConstraint.animator.constant = 0;
    } else {
        self.mrlViewHeightConstraint.animator.constant = 39;
    }
}

- (void)openFileGeneric
{
    [self openFilePathChanged: nil];
    [self openTarget: VLCOpenFileTabViewId];
}

- (void)openDisc
{
    @synchronized (self) {
        [_specialMediaFolders removeAllObjects];
    }

    [self scanOpticalMedia: nil];
    [self openTarget: VLCOpenDiscTabViewId];
}

- (void)openNet
{
    [self openNetInfoChanged: nil];
    [self openTarget: VLCOpenNetworkTabViewId];
    [_netHTTPURLTextField selectText:nil];
}

- (void)openCapture
{
    [self openCaptureModeChanged: nil];
    [self openTarget: VLCOpenCaptureTabViewId];
}

- (void)openFileWithAction:(void (^)(NSArray *files))action;
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setAllowsMultipleSelection: YES];
    [openPanel setCanChooseDirectories: YES];
    [openPanel setTitle: _NS("Open File")];
    [openPanel setPrompt: _NS("Open")];

    if ([openPanel runModal] == NSModalResponseOK) {
        NSArray *URLs = [openPanel URLs];
        NSUInteger count = [URLs count];
        NSMutableArray *values = [NSMutableArray arrayWithCapacity:count];
        NSMutableArray *array = [NSMutableArray arrayWithCapacity:count];
        for (NSUInteger i = 0; i < count; i++)
            [values addObject: [[URLs objectAtIndex:i] path]];
        [values sortUsingSelector:@selector(caseInsensitiveCompare:)];

        for (NSUInteger i = 0; i < count; i++) {
            VLCOpenInputMetadata *inputMetadata;
            char *psz_uri = vlc_path2uri([[values objectAtIndex:i] UTF8String], "file");
            if (!psz_uri)
                continue;
            inputMetadata = [[VLCOpenInputMetadata alloc] init];
            inputMetadata.MRLString = toNSStr(psz_uri);
            free(psz_uri);
            [array addObject:inputMetadata];
        }

        action(array);
    }
}

- (IBAction)outputSettings:(id)sender
{
    if (sender == self.outputCheckbox) {
        self.outputSettingsButton.enabled = self.outputCheckbox.state;
        return;
    }

    if (!b_outputNibLoaded) {
        b_outputNibLoaded = [[NSBundle mainBundle] loadNibNamed:@"StreamOutput" owner:_output topLevelObjects:nil];
    }

    [self.window beginSheet:_output.outputSheet completionHandler:nil];
}

#pragma mark -
#pragma mark File Panel

- (void)openFilePathChanged:(NSNotification *)o_notification
{
    if (_filePath && [_filePath length] > 0) {
        bool b_stream = [_fileTreatAsPipeButton state];
        BOOL b_dir = NO;

        [[NSFileManager defaultManager] fileExistsAtPath:_filePath isDirectory:&b_dir];

        char *psz_uri = vlc_path2uri([_filePath UTF8String], "file");
        if (!psz_uri) return;

        NSMutableString *mrlString = [NSMutableString stringWithUTF8String:psz_uri];
        NSRange offile = [mrlString rangeOfString:@"file"];
        free(psz_uri);

        if (b_dir)
            [mrlString replaceCharactersInRange:offile withString: @"directory"];
        else if (b_stream)
            [mrlString replaceCharactersInRange:offile withString: @"stream"];

        [_fileNameLabel setStringValue: [[NSFileManager defaultManager] displayNameAtPath:_filePath]];
        [_fileNameStubLabel setHidden: YES];
        [_fileTreatAsPipeButton setHidden: NO];
        [_fileIconWell setImage: [[NSWorkspace sharedWorkspace] iconForFile: _filePath]];
        [_fileIconWell setHidden: NO];
        [self setMRL: mrlString];
    } else {
        [_fileNameLabel setStringValue: @""];
        [_fileNameStubLabel setHidden: NO];
        [_fileTreatAsPipeButton setHidden: YES];
        [_fileIconWell setImage: [NSImage imageNamed:@"generic"]];
        [self setMRL: @""];
    }
}

- (IBAction)openFileBrowse:(id)sender
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setAllowsMultipleSelection: NO];
    [openPanel setCanChooseDirectories: YES];
    [openPanel setTitle: _NS("Open File")];
    [openPanel setPrompt: _NS("Open")];
    [openPanel beginSheetModalForWindow:[sender window] completionHandler:^(NSInteger returnCode) {
        if (returnCode == NSFileHandlingPanelOKButton) {
            self->_filePath = [[[openPanel URLs] firstObject] path];
            [self openFilePathChanged: nil];
        }
    }];
}

- (IBAction)openFileStreamChanged:(id)sender
{
    [self openFilePathChanged: nil];
}

- (IBAction)inputSlaveAction:(id)sender
{
    if (sender == _fileSlaveCheckbox)
        [_fileSelectSlaveButton setEnabled: [_fileSlaveCheckbox state]];
    else {
        NSOpenPanel *openPanel;
        openPanel = [NSOpenPanel openPanel];
        [openPanel setCanChooseFiles: YES];
        [openPanel setCanChooseDirectories: NO];
        if ([openPanel runModal] == NSModalResponseOK) {
            _fileSlavePath = [[[openPanel URLs] firstObject] path];
        }
    }
    if (_fileSlavePath && [_fileSlaveCheckbox state] == NSOnState) {
        [_fileSlaveFilenameLabel setStringValue: [[NSFileManager defaultManager] displayNameAtPath:_fileSlavePath]];
        [_fileSlaveIconWell setImage: [[NSWorkspace sharedWorkspace] iconForFile: _fileSlavePath]];
    } else {
        [_fileSlaveFilenameLabel setStringValue: @""];
        [_fileSlaveIconWell setImage: NULL];
    }
}

- (IBAction)fileTimeCustomization:(id)sender
{
    BOOL b_value = [_fileCustomTimingCheckbox state];
    [_fileStartTimeTextField setEnabled: b_value];
    [_fileStartTimeLabel setEnabled: b_value];
    [_fileStopTimeTextField setEnabled: b_value];
    [_fileStopTimeLabel setEnabled: b_value];
}

#pragma mark -
#pragma mark Optical Media Panel

- (void)showOpticalMediaView:(NSView *)theView withIcon:(NSImage *)icon
{
    NSRect viewRect = [theView frame];
    [theView setFrame: NSMakeRect(233, 0, viewRect.size.width, viewRect.size.height)];
    [theView setAutoresizesSubviews: YES];

    NSView *opticalTabView = [[_tabView tabViewItemAtIndex: [_tabView indexOfTabViewItemWithIdentifier:VLCOpenDiscTabViewId]] view];
    if (_currentOpticalMediaView) {
        [[opticalTabView animator] replaceSubview: _currentOpticalMediaView with: theView];
    } else {
        [[opticalTabView animator] addSubview: theView];
    }
    _currentOpticalMediaView = theView;

    NSImageView *imageView = [[NSImageView alloc] init];
    [imageView setFrame: NSMakeRect(53, 61, 128, 128)];
    [icon setSize: NSMakeSize(128,128)];
    [imageView setImage: icon];
    if (_currentOpticalMediaIconView) {
        [[opticalTabView animator] replaceSubview: _currentOpticalMediaIconView with: imageView];
    } else {
        [[opticalTabView animator] addSubview: imageView];
    }
    _currentOpticalMediaIconView = imageView;
    [_currentOpticalMediaView setNeedsDisplay: YES];
    [_currentOpticalMediaIconView setNeedsDisplay: YES];
    [opticalTabView setNeedsDisplay: YES];
    [opticalTabView displayIfNeeded];
}

- (void)showOpticalAtPath:(VLCOpenBlockDeviceDescription *)deviceDescription
{
    NSString *diskType = deviceDescription.mediaType;
    NSString *opticalDevicePath = deviceDescription.path;
    NSString *devicePath = deviceDescription.devicePath;
    NSImage *mediaIcon = deviceDescription.mediaIcon;
    NSFileManager *fileManager = [NSFileManager defaultManager];

    if ([diskType isEqualToString: kVLCMediaDVD] || [diskType isEqualToString: kVLCMediaVideoTSFolder]) {
        [_discDVDLabel setStringValue: [[NSFileManager defaultManager] displayNameAtPath:opticalDevicePath]];
        [_discDVDwomenusLabel setStringValue: [_discDVDLabel stringValue]];

        if (!b_nodvdmenus) {
            [self setMRL: [NSString stringWithFormat: @"dvdnav://%@", devicePath]];
            [self showOpticalMediaView: _discDVDView withIcon:mediaIcon];
        } else {
            [self setMRL: [NSString stringWithFormat: @"dvdread://%@#%i:%i-", devicePath, [_discDVDwomenusTitleTextField intValue], [_discDVDwomenusChapterTextField intValue]]];
            [self showOpticalMediaView: _discDVDwomenusView withIcon:mediaIcon];
        }
    } else if ([diskType isEqualToString: kVLCMediaAudioCD]) {
        [_discAudioCDLabel setStringValue: [fileManager displayNameAtPath: opticalDevicePath]];
        [_discAudioCDTrackCountLabel setStringValue: [NSString stringWithFormat:_NS("%i tracks"), [[fileManager subpathsOfDirectoryAtPath: opticalDevicePath error:NULL] count] - 1]]; // minus .TOC.plist
        [self showOpticalMediaView: _discAudioCDView withIcon: mediaIcon];
        [self setMRL: [NSString stringWithFormat: @"cdda://%@", devicePath]];
    } else if ([diskType isEqualToString: kVLCMediaVCD]) {
        [_discVCDLabel setStringValue: [fileManager displayNameAtPath: opticalDevicePath]];
        [self showOpticalMediaView: _discVCDView withIcon: mediaIcon];
        [self setMRL: [NSString stringWithFormat: @"vcd://%@#%i:%i", devicePath, [_discVCDTitleTextField intValue], [_discVCDChapterTextField intValue]]];
    } else if ([diskType isEqualToString: kVLCMediaSVCD]) {
        [_discVCDLabel setStringValue: [fileManager displayNameAtPath: opticalDevicePath]];
        [self showOpticalMediaView: _discVCDView withIcon: mediaIcon];
        [self setMRL: [NSString stringWithFormat: @"vcd://%@@%i:%i", devicePath, [_discVCDTitleTextField intValue], [_discVCDChapterTextField intValue]]];
    } else if ([diskType isEqualToString: kVLCMediaBD] || [diskType isEqualToString: kVLCMediaBDMVFolder]) {
        [_discBDLabel setStringValue: [fileManager displayNameAtPath: opticalDevicePath]];
        [self showOpticalMediaView: _discBDView withIcon: mediaIcon];
        [self setMRL: [NSString stringWithFormat: @"bluray://%@", opticalDevicePath]];
    } else {
        msg_Warn(getIntf(), "unknown disk type, no idea what to display");

        [self showOpticalMediaView: _discNoDiscView withIcon: [NSImage imageNamed:@"NSApplicationIcon"]];
    }
}

- (VLCOpenBlockDeviceDescription *)scanPath:(NSURL *)url
{
    NSString *path = [url path];
    NSString *type = getVolumeTypeFromMountPath(path);
    NSImage *image = [[NSWorkspace sharedWorkspace] iconForFile: path];
    NSString *devicePath;

    // BDMV path must not end with BDMV directory
    if ([type isEqualToString: kVLCMediaBDMVFolder]) {
        if ([[path lastPathComponent] isEqualToString: @"BDMV"]) {
            path = [path stringByDeletingLastPathComponent];
        }
    }

    if ([type isEqualToString: kVLCMediaVideoTSFolder] ||
        [type isEqualToString: kVLCMediaBD] ||
        [type isEqualToString: kVLCMediaBDMVFolder] ||
        [type isEqualToString: kVLCMediaUnknown])
        devicePath = path;
    else
        devicePath = getBSDNodeFromMountPath(path);

    VLCOpenBlockDeviceDescription *deviceDescription = [[VLCOpenBlockDeviceDescription alloc] init];
    deviceDescription.path = path;
    deviceDescription.devicePath = devicePath;
    deviceDescription.mediaType = type;
    deviceDescription.mediaIcon = image;

    return deviceDescription;
}

- (void)scanDevices
{
    @autoreleasepool {
        NSArray *mountURLs = [[NSFileManager defaultManager] mountedVolumeURLsIncludingResourceValuesForKeys:@[NSURLVolumeIsRemovableKey] options:NSVolumeEnumerationSkipHiddenVolumes];
        NSUInteger count = [mountURLs count];
        NSMutableArray *o_result = [NSMutableArray array];
        for (NSUInteger i = 0; i < count; i++) {
            NSURL *currentURL = [mountURLs objectAtIndex:i];

            NSNumber *isRemovable = nil;
            if (![currentURL getResourceValue:&isRemovable forKey:NSURLVolumeIsRemovableKey error:nil] || !isRemovable) {
                msg_Warn(getIntf(), "Cannot find removable flag for mount point");
                continue;
            }

            if (!isRemovable.boolValue)
                continue;

            [o_result addObject: [self scanPath:currentURL]];
        }

        @synchronized (self) {
            _opticalDevices = [[NSArray alloc] initWithArray: o_result];
        }

        [self updateMediaSelector:NO];
    }
}

- (void)scanSpecialPath:(NSURL *)oPath
{
    @autoreleasepool {
        VLCOpenBlockDeviceDescription *deviceDescription = [self scanPath:oPath];

        @synchronized (self) {
            [_specialMediaFolders addObject:deviceDescription];
        }

        [self updateMediaSelector:YES];
    }
}

- (void)scanOpticalMedia:(NSNotification *)o_notification
{
    [NSThread detachNewThreadSelector:@selector(scanDevices) toTarget:self withObject:nil];
}

- (void)updateMediaSelector:(BOOL)selected
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self->_allMediaDevices removeAllObjects];
        [self->_discSelectorPopup removeAllItems];

        @synchronized (self) {
            [self->_allMediaDevices addObjectsFromArray:self->_opticalDevices];
            [self->_allMediaDevices addObjectsFromArray:self->_specialMediaFolders];
        }

        NSUInteger count = [self->_allMediaDevices count];
        if (count > 0) {
            for (NSUInteger i = 0; i < count ; i++) {
                VLCOpenBlockDeviceDescription *deviceDescription = [self->_allMediaDevices objectAtIndex:i];
                [self->_discSelectorPopup addItemWithTitle: [[NSFileManager defaultManager] displayNameAtPath:deviceDescription.path]];
            }

            if ([self->_discSelectorPopup numberOfItems] <= 1)
                [self->_discSelectorPopup setHidden: YES];
            else
                [self->_discSelectorPopup setHidden: NO];

            // select newly added media folder
            if (selected)
                [self->_discSelectorPopup selectItemAtIndex: [[self->_discSelectorPopup itemArray] count] - 1];

            // only trigger MRL update if the tab view is active
            if ([[[self->_tabView selectedTabViewItem] identifier] isEqualToString:VLCOpenDiscTabViewId])
                [self discSelectorChanged:nil];
        } else {
            msg_Dbg(getIntf(), "no optical media found");
            [self->_discSelectorPopup setHidden: YES];
            [self setMRL:@""];
            [self showOpticalMediaView: self->_discNoDiscView withIcon: [NSImage imageNamed: @"NSApplicationIcon"]];
        }
    });
}

- (IBAction)discSelectorChanged:(id)sender
{
    [self showOpticalAtPath:[_allMediaDevices objectAtIndex:[_discSelectorPopup indexOfSelectedItem]]];
}

- (IBAction)openSpecialMediaFolder:(id)sender
{
    /* this is currently for VIDEO_TS and BDMV folders */
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setAllowsMultipleSelection: NO];
    [openPanel setCanChooseDirectories: YES];
    [openPanel setTitle: [sender title]];
    [openPanel setPrompt: _NS("Open")];

    /* work-around for Mountain Lion, which treats folders called "BDMV" including an item named "INDEX.BDM"
     * as a _FILE_. Don't ask, move on. There is nothing to see here */
    [openPanel setCanChooseFiles: YES];
    [openPanel setAllowedFileTypes:[NSArray arrayWithObject:@"public.directory"]];

    if ([openPanel runModal] == NSModalResponseOK) {
        NSURL *path = openPanel.URL;
        if (path) {
            [NSThread detachNewThreadSelector:@selector(scanSpecialPath:) toTarget:self withObject:path];
        }
    }
}

- (IBAction)dvdreadOptionChanged:(id)sender
{
    NSString *devicePath = [[_allMediaDevices objectAtIndex:[_discSelectorPopup indexOfSelectedItem]] devicePath];

    if (sender == _discDVDwomenusEnableMenusButton) {
        b_nodvdmenus = NO;
        [self setMRL: [NSString stringWithFormat: @"dvdnav://%@", devicePath]];
        [self showOpticalMediaView:_discDVDView withIcon:[_currentOpticalMediaIconView image]];
        return;
    }
    if (sender == _discDVDDisableMenusButton) {
        b_nodvdmenus = YES;
        [self showOpticalMediaView:_discDVDwomenusView withIcon:[_currentOpticalMediaIconView image]];
    }

    if (sender == _discDVDwomenusTitleTextField)
        [_discDVDwomenusTitleStepper setIntValue: [_discDVDwomenusTitleTextField intValue]];
    if (sender == _discDVDwomenusTitleStepper)
        [_discDVDwomenusTitleTextField setIntValue: [_discDVDwomenusTitleStepper intValue]];
    if (sender == _discDVDwomenusChapterTextField)
        [_discDVDwomenusChapterStepper setIntValue: [_discDVDwomenusChapterTextField intValue]];
    if (sender == _discDVDwomenusChapterStepper)
        [_discDVDwomenusChapterTextField setIntValue: [_discDVDwomenusChapterStepper intValue]];

    [self setMRL: [NSString stringWithFormat: @"dvdread://%@#%i:%i-", devicePath, [_discDVDwomenusTitleTextField intValue], [_discDVDwomenusChapterTextField intValue]]];
}

- (IBAction)vcdOptionChanged:(id)sender
{
    if (sender == _discVCDTitleTextField)
        [_discVCDTitleStepper setIntValue: [_discVCDTitleTextField intValue]];
    if (sender == _discVCDTitleStepper)
        [_discVCDTitleTextField setIntValue: [_discVCDTitleStepper intValue]];
    if (sender == _discVCDChapterTextField)
        [_discVCDChapterStepper setIntValue: [_discVCDChapterTextField intValue]];
    if (sender == _discVCDChapterStepper)
        [_discVCDChapterTextField setIntValue: [_discVCDChapterStepper intValue]];

    NSString *devicePath = [[_allMediaDevices objectAtIndex:[_discSelectorPopup indexOfSelectedItem]] devicePath];
    [self setMRL: [NSString stringWithFormat: @"vcd://%@@%i:%i", devicePath, [_discVCDTitleTextField intValue], [_discVCDChapterTextField intValue]]];
}

#pragma mark -
#pragma mark Network Panel

- (void)textFieldWasClicked:(NSNotification *)notification
{
    if ([notification object] == _netUDPPortTextField)
        [_netModeMatrix selectCellAtRow: 0 column: 0];
    else if ([notification object] == _netUDPMAddressTextField ||
             [notification object] == _netUDPMPortTextField)
        [_netModeMatrix selectCellAtRow: 1 column: 0];
    else
        [_netModeMatrix selectCellAtRow: 2 column: 0];

    [self openNetInfoChanged: nil];
}

- (IBAction)openNetModeChanged:(id)sender
{
    if (sender == _netModeMatrix) {
        if ([[sender selectedCell] tag] == 0)
            [self.window makeFirstResponder: _netUDPPortTextField];
        else if ([[sender selectedCell] tag] == 1)
            [self.window makeFirstResponder: _netUDPMAddressTextField];
        else
            msg_Warn(getIntf(), "Unknown sender tried to change UDP/RTP mode");
    }

    [self openNetInfoChanged: nil];
}

- (IBAction)openNetStepperChanged:(id)sender
{
    NSInteger i_tag = [sender tag];

    if (i_tag == 0) {
        [_netUDPPortTextField setIntValue: [_netUDPPortStepper intValue]];
        [[NSNotificationCenter defaultCenter] postNotificationName: VLCOpenTextFieldWasClicked
                                                            object: _netUDPPortTextField];
        [self.window makeFirstResponder: _netUDPPortTextField];
    }
    else if (i_tag == 1) {
        [_netUDPMPortTextField setIntValue: [_netUDPMPortStepper intValue]];
        [[NSNotificationCenter defaultCenter] postNotificationName: VLCOpenTextFieldWasClicked
                                                            object: _netUDPMPortTextField];
        [self.window makeFirstResponder: _netUDPMPortTextField];
    }

    [self openNetInfoChanged: nil];
}

- (void)openNetInfoChanged:(NSNotification *)o_notification
{
    NSString *mrlString;

    if ([_netUDPPanel isVisible]) {
        NSString *mode;
        mode = [[_netModeMatrix selectedCell] title];

        if ([mode isEqualToString: _NS("Unicast")]) {
            int port = [_netUDPPortTextField intValue];

            if ([[_netUDPProtocolMatrix selectedCell] tag] == 0)
                mrlString = @"udp://";
            else
                mrlString = @"rtp://";

            if (port != config_GetInt("server-port")) {
                mrlString =
                [mrlString stringByAppendingFormat: @"@:%i", port];
            }
        }
        else if ([mode isEqualToString: _NS("Multicast")]) {
            NSString *oAddress = [_netUDPMAddressTextField stringValue];
            int iPort = [_netUDPMPortTextField intValue];

            if ([[_netUDPProtocolMatrix selectedCell] tag] == 0)
                mrlString = [NSString stringWithFormat: @"udp://@%@", oAddress];
            else
                mrlString = [NSString stringWithFormat: @"rtp://@%@", oAddress];

            if (iPort != config_GetInt("server-port")) {
                mrlString =
                [mrlString stringByAppendingFormat: @":%i", iPort];
            }
        }
    } else
        mrlString = [_netHTTPURLTextField stringValue];

    [self setMRL: mrlString];
}

- (IBAction)openNetUDPButtonAction:(id)sender
{
    if (sender == _netOpenUDPButton) {
        [self.window beginSheet:self.netUDPPanel
              completionHandler:nil];
        [self openNetInfoChanged:nil];
    }
    else if (sender == _netUDPCancelButton) {
        [_netUDPPanel orderOut: sender];
        [NSApp endSheet: _netUDPPanel];
    }
    else if (sender == _netUDPOKButton) {
        NSString *mrlString;
        if ([[[_netModeMatrix selectedCell] title] isEqualToString: _NS("Unicast")]) {
            int port = [_netUDPPortTextField intValue];

            if ([[_netUDPProtocolMatrix selectedCell] tag] == 0)
                mrlString = @"udp://";
            else
                mrlString = @"rtp://";

            if (port != config_GetInt("server-port")) {
                mrlString =
                [mrlString stringByAppendingFormat: @"@:%i", port];
            }
        }
        else if ([[[_netModeMatrix selectedCell] title] isEqualToString: _NS("Multicast")]) {
            NSString *oAddress = [_netUDPMAddressTextField stringValue];
            int iPort = [_netUDPMPortTextField intValue];

            if ([[_netUDPProtocolMatrix selectedCell] tag] == 0)
                mrlString = [NSString stringWithFormat: @"udp://@%@", oAddress];
            else
                mrlString = [NSString stringWithFormat: @"rtp://@%@", oAddress];

            if (iPort != config_GetInt("server-port")) {
                mrlString =
                [mrlString stringByAppendingFormat: @":%i", iPort];
            }
        }
        [self setMRL: mrlString];
        [_netHTTPURLTextField setStringValue: mrlString];
        [_netUDPPanel orderOut: sender];
        [NSApp endSheet: _netUDPPanel];
    }
}

#pragma mark -
#pragma mark Capture Panel

- (IBAction)openCaptureModeChanged:(id)sender
{
    if ([[[_captureModePopup selectedItem] title] isEqualToString: _NS("Screen")]) {
        [_captureTabView selectTabViewItemAtIndex:1];

        [self setMRL: @"screen://"];
        [_screenHeightTextField setIntegerValue: config_GetInt("screen-height")];
        [_screenWidthTextField setIntegerValue: config_GetInt("screen-width")];
        [_screenFPSTextField setFloatValue: config_GetFloat("screen-fps")];
        [_screenLeftTextField setIntegerValue: config_GetInt("screen-left")];
        [_screenTopTextField setIntegerValue: config_GetInt("screen-top")];
        [_screenFollowMouseCheckbox setIntegerValue: config_GetInt("screen-follow-mouse")];

        NSInteger screenIindex = config_GetInt("screen-index");
        NSInteger displayID = config_GetInt("screen-display-id");
        unsigned int displayCount = 0;
        CGError returnedError;
        VLCOpenDisplayInformation *displayInformation;

        returnedError = CGGetOnlineDisplayList(0, NULL, &displayCount);
        if (!returnedError) {
            CGDirectDisplayID *ids;
            ids = (CGDirectDisplayID *)vlc_alloc(displayCount, sizeof(CGDirectDisplayID));
            returnedError = CGGetOnlineDisplayList(displayCount, ids, &displayCount);
            if (!returnedError) {
                [_displayInfos removeAllObjects];
                [_screenPopup removeAllItems];
                for (unsigned int i = 0; i < displayCount; i ++) {
                    displayInformation = [[VLCOpenDisplayInformation alloc] init];
                    displayInformation.displayID = ids[i];
                    NSRect displayBounds = CGDisplayBounds(displayInformation.displayID);
                    displayInformation.displayBounds = displayBounds;
                    [_screenPopup addItemWithTitle: [NSString stringWithFormat:@"Screen %d (%dx%d)", i + 1, (int)displayBounds.size.width, (int)displayBounds.size.height]];
                    [_displayInfos addObject:displayInformation];
                    if (i == 0 || displayID == displayInformation.displayID || screenIindex - 1 == i) {
                        [_screenPopup selectItemAtIndex: i];
                        [_screenLeftStepper setMaxValue: displayBounds.size.width];
                        [_screenTopStepper setMaxValue: displayBounds.size.height];
                        [_screenWidthStepper setMaxValue: displayBounds.size.width];
                        [_screenHeightStepper setMaxValue: displayBounds.size.height];
                    }
                }
            }
            free(ids);
        }
    }
    else if ([[[_captureModePopup selectedItem] title] isEqualToString: _NS("Input Devices")]) {
        [_captureTabView selectTabViewItemAtIndex:0];

        [self qtkChanged:nil];
        [self qtkAudioChanged:nil];

        [self setMRL: @""];

        if ([_qtkVideoCheckbox state] && _avCurrentDeviceUID)
            [self setMRL:[NSString stringWithFormat:@"avcapture://%@", _avCurrentDeviceUID]];
        else if ([_qtkAudioCheckbox state] && _avCurrentAudioDeviceUID)
            [self setMRL:[NSString stringWithFormat:@"avaudiocapture://%@", _avCurrentAudioDeviceUID]];
    }
}

// Screen actions
- (void)screenFPSfieldChanged:(NSNotification *)o_notification
{
    [_screenFPSStepper setFloatValue: [_screenFPSTextField floatValue]];
    if ([[_screenFPSTextField stringValue] isEqualToString: @""])
        [_screenFPSTextField setFloatValue: 1.0];
    [self setMRL: @"screen://"];
}

- (IBAction)screenChanged:(id)sender
{
    NSInteger selected_index = [_screenPopup indexOfSelectedItem];
    if (selected_index >= [_displayInfos count])
        return;

    VLCOpenDisplayInformation *displayInformation = [_displayInfos objectAtIndex:selected_index];
    CGRect displayBounds = displayInformation.displayBounds;

    [_screenLeftStepper setMaxValue: displayBounds.size.width];
    [_screenTopStepper setMaxValue: displayBounds.size.height];
    [_screenWidthStepper setMaxValue: displayBounds.size.width];
    [_screenHeightStepper setMaxValue: displayBounds.size.height];

    [_screenqtkAudioPopup setEnabled: [_screenqtkAudioCheckbox state]];
}

- (IBAction)screenAudioChanged:(id)sender
{
    [_screenqtkAudioPopup setEnabled:_screenqtkAudioCheckbox.state];
}

// QTKit Recording actions
- (IBAction)qtkChanged:(id)sender
{
    NSInteger selectedDevice = [_qtkVideoDevicePopup indexOfSelectedItem];
    if (selectedDevice >= _avvideoDevices.count)
        return;

    _avCurrentDeviceUID = [[(AVCaptureDevice *)[_avvideoDevices objectAtIndex:selectedDevice] uniqueID] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}

- (IBAction)qtkAudioChanged:(id)sender
{
    NSInteger selectedDevice = [_qtkAudioDevicePopup indexOfSelectedItem];
    if (selectedDevice >= _avaudioDevices.count)
        return;

    _avCurrentAudioDeviceUID = [[(AVCaptureDevice *)[_avaudioDevices objectAtIndex:selectedDevice] uniqueID] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}

- (IBAction)qtkToggleUIElements:(id)sender
{
    [_qtkAudioDevicePopup setEnabled:[_qtkAudioCheckbox state]];
    BOOL b_state = [_qtkVideoCheckbox state];
    [_qtkVideoDevicePopup setEnabled:b_state];
    [self qtkAudioChanged:sender];
    [self qtkChanged:sender];
    [self openCaptureModeChanged:sender];
}

#pragma mark -
#pragma mark Subtitle Settings

- (IBAction)subsChanged:(id)sender
{
    if ([_fileSubCheckbox state] == NSOnState) {
        [_fileSubSettingsButton setEnabled:YES];
        if (_subPath) {
            [_fileSubtitlesFilenameLabel setStringValue: [[NSFileManager defaultManager] displayNameAtPath:_subPath]];
            [_fileSubtitlesIconWell setImage: [[NSWorkspace sharedWorkspace] iconForFile:_subPath]];
        }
    } else {
        [_fileSubSettingsButton setEnabled:NO];
        [_fileSubtitlesFilenameLabel setStringValue: @""];
        [_fileSubtitlesIconWell setImage: NULL];
    }
}

- (IBAction)subSettings:(id)sender
{
    [[self window] beginSheet:self.fileSubSheet completionHandler:nil];
}

- (IBAction)subCloseSheet:(id)sender
{
    [self subsChanged: nil];
    [_fileSubSheet orderOut:sender];
    [NSApp endSheet: _fileSubSheet];
}

- (IBAction)subFileBrowse:(id)sender
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setAllowsMultipleSelection: NO];
    [openPanel setTitle: _NS("Open File")];
    [openPanel setPrompt: _NS("Open")];

    if ([openPanel runModal] == NSModalResponseOK) {
        _subPath = [[[openPanel URLs] firstObject] path];
        [_fileSubtitlesFilenameLabel setStringValue: [[NSFileManager defaultManager] displayNameAtPath:_subPath]];
        [_fileSubPathTextField setStringValue: [_fileSubtitlesFilenameLabel stringValue]];
        [_fileSubPathLabel setHidden: YES];
        [_fileSubtitlesIconWell setImage: [[NSWorkspace sharedWorkspace] iconForFile:_subPath]];
        [_fileSubIconView setImage: [_fileSubtitlesIconWell image]];
    } else {
        [_fileSubPathLabel setHidden: NO];
        [_fileSubPathTextField setStringValue:@""];
        [_fileSubtitlesFilenameLabel setStringValue:@""];
        [_fileSubtitlesIconWell setImage: nil];
        [_fileSubIconView setImage: nil];
    }
}

- (IBAction)subOverride:(id)sender
{
    BOOL b_state = [_fileSubOverrideCheckbox state];
    [_fileSubDelayTextField setEnabled: b_state];
    [_fileSubDelayStepper setEnabled: b_state];
    [_fileSubFPSTextField setEnabled: b_state];
    [_fileSubFPSStepper setEnabled: b_state];
}

#pragma mark -
#pragma mark Miscellaneous

- (IBAction)panelCancel:(id)sender
{
    [NSApp stopModalWithCode: 0];
}

- (IBAction)panelOk:(id)sender
{
    if ([[self MRL] length])
        [NSApp stopModalWithCode: 1];
    else
        vlc_assert_unreachable();
}

#pragma mark - audio and video device management

- (void)updateVideoDevicesAndRepresentation
{
    _avvideoDevices = [[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]
                         arrayByAddingObjectsFromArray:[AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed]];

    [_qtkVideoDevicePopup removeAllItems];
    msg_Dbg(getIntf(), "Found %lu video capture devices", _avvideoDevices.count);

    if (_avvideoDevices.count >= 1) {
        if (!_avCurrentDeviceUID)
            _avCurrentDeviceUID = [[[AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo] uniqueID]
                                   stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

        NSUInteger deviceCount = _avvideoDevices.count;
        for (int ivideo = 0; ivideo < deviceCount; ivideo++) {
            AVCaptureDevice *avDevice = [_avvideoDevices objectAtIndex:ivideo];
            // allow same name for multiple times
            [[_qtkVideoDevicePopup menu] addItemWithTitle:[avDevice localizedName] action:nil keyEquivalent:@""];

            if ([[[avDevice uniqueID] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] isEqualToString:_avCurrentDeviceUID])
                [_qtkVideoDevicePopup selectItemAtIndex:ivideo];
        }
    } else {
        [_qtkVideoDevicePopup addItemWithTitle: _NS("None")];
    }
}

- (void)updateAudioDevicesAndRepresentation
{
    _avaudioDevices = [[AVCaptureDevice devicesWithMediaType:AVMediaTypeAudio]
                        arrayByAddingObjectsFromArray:[AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed]];

    [_qtkAudioDevicePopup removeAllItems];
    [_screenqtkAudioPopup removeAllItems];
    msg_Dbg(getIntf(), "Found %lu audio capture devices", _avaudioDevices.count);

    if (_avaudioDevices.count >= 1) {
        if (!_avCurrentAudioDeviceUID)
            _avCurrentAudioDeviceUID = [[[AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeAudio] uniqueID]
                                        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

        NSUInteger deviceCount = _avaudioDevices.count;
        for (int iaudio = 0; iaudio < deviceCount; iaudio++) {
            AVCaptureDevice *avAudioDevice = [_avaudioDevices objectAtIndex:iaudio];

            // allow same name for multiple times
            NSString *localizedName = [avAudioDevice localizedName];
            [[_qtkAudioDevicePopup menu] addItemWithTitle:localizedName action:nil keyEquivalent:@""];
            [[_screenqtkAudioPopup menu] addItemWithTitle:localizedName action:nil keyEquivalent:@""];

            if ([[[avAudioDevice uniqueID] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] isEqualToString:_avCurrentAudioDeviceUID]) {
                [_qtkAudioDevicePopup selectItemAtIndex:iaudio];
                [_screenqtkAudioPopup selectItemAtIndex:iaudio];
            }
        }
    } else {
        [_qtkAudioDevicePopup addItemWithTitle: _NS("None")];
        [_screenqtkAudioPopup addItemWithTitle: _NS("None")];
    }
}

@end
