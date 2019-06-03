/*****************************************************************************
 * VLCConvertAndSaveWindowController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012 Felix Paul Kühne
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

#import <Cocoa/Cocoa.h>

@class VLCDropDisabledImageView;
@class VLCDragDropView;
@class VLCPopupPanelController;
@class VLCTextfieldPanelController;

@interface VLCConvertAndSaveWindowController : NSWindowController

// main panel
@property (weak) IBOutlet VLCDragDropView *dropBox;
@property (weak) IBOutlet NSTextField *dropLabel;

@property (weak) IBOutlet VLCDropDisabledImageView *dropImage;
@property (weak) IBOutlet NSButton *dropButton;
@property (weak) IBOutlet NSTextField *profileLabel;
@property (weak) IBOutlet NSPopUpButton *profilePopup;
@property (weak) IBOutlet NSButton *customizeButton;

@property (weak) IBOutlet NSTextField *destinationLabel;
@property (weak) IBOutlet NSBox *destinationBox;

@property (weak) IBOutlet NSButton *destinationCancelBtn;
@property (weak) IBOutlet NSButton *destinationStreamButton;
@property (weak) IBOutlet NSButton *destinationFileButton;

@property (weak) IBOutlet NSButton *okButton;

@property (weak) IBOutlet NSView *dropinView;
@property (weak) IBOutlet VLCDropDisabledImageView *dropinIcon;
@property (weak) IBOutlet NSTextField *dropinMediaLabel;
@property (strong) IBOutlet NSView *fileDestinationView;
@property (weak) IBOutlet NSImageView *fileDestinationIcon;
@property (weak) IBOutlet NSTextField *fileDestinationFileName;
@property (weak) IBOutlet NSTextField *fileDestinationFileNameStub;
@property (weak) IBOutlet NSButton *fileDestinationBrowseButton;
@property (strong) IBOutlet NSView *streamDestinationView;
@property (weak) IBOutlet NSTextField *streamDestinationURLLabel;
@property (weak) IBOutlet NSButton *streamDestinationButton;

// customize panel
@property ()     IBOutlet NSWindow *customizePanel;
@property (weak) IBOutlet NSButton *customizeNewProfileButton;
@property (weak) IBOutlet NSButton *customizeCancelButton;
@property (weak) IBOutlet NSButton *customizeOkButton;
@property (weak) IBOutlet NSTabView *customizeTabView;
@property (weak) IBOutlet NSMatrix *customizeEncapMatrix;

// customize panel: video
@property (weak) IBOutlet NSButton *customizeVidCheckbox;
@property (weak) IBOutlet NSButton *customizeVidKeepCheckbox;
@property (weak) IBOutlet NSBox *customizeVidSettingsBox;
@property (weak) IBOutlet NSTextField *customizeVidCodecLabel;
@property (weak) IBOutlet NSTextField *customizeVidBitrateLabel;
@property (weak) IBOutlet NSTextField *customizeVidFramerateLabel;
@property (weak) IBOutlet NSBox *customizeVidResolutionBox;
@property (weak) IBOutlet NSTextField *customizeVidWidthLabel;
@property (weak) IBOutlet NSTextField *customizeVidHeightLabel;
@property (weak) IBOutlet NSTextField *customizeVidScaleLabel;
@property (weak) IBOutlet NSTextField *customizeVidResLabel;
@property (weak) IBOutlet NSPopUpButton *customizeVidCodecPopup;
@property (weak) IBOutlet NSTextField *customizeVidBitrateField;
@property (weak) IBOutlet NSTextField *customizeVidFramerateField;
@property (weak) IBOutlet NSTextField *customizeVidWidthField;
@property (weak) IBOutlet NSTextField *customizeVidHeightField;
@property (weak) IBOutlet NSPopUpButton *customizeVidScalePopup;

// customize panel: audio
@property (weak) IBOutlet NSButton *customizeAudCheckbox;
@property (weak) IBOutlet NSButton *customizeAudKeepCheckbox;
@property (weak) IBOutlet NSBox *customizeAudSettingsBox;
@property (weak) IBOutlet NSTextField *customizeAudCodecLabel;
@property (weak) IBOutlet NSTextField *customizeAudBitrateLabel;
@property (weak) IBOutlet NSTextField *customizeAudChannelsLabel;
@property (weak) IBOutlet NSTextField *customizeAudSamplerateLabel;
@property (weak) IBOutlet NSPopUpButton *customizeAudCodecPopup;
@property (weak) IBOutlet NSTextField *customizeAudBitrateField;
@property (weak) IBOutlet NSTextField *customizeAudChannelsField;
@property (weak) IBOutlet NSPopUpButton *customizeAudSampleratePopup;

// customize panel: subs
@property (weak) IBOutlet NSButton *customizeSubsCheckbox;
@property (weak) IBOutlet NSButton *customizeSubsOverlayCheckbox;
@property (weak) IBOutlet NSPopUpButton *customizeSubsPopup;

// stream panel
@property ()     IBOutlet NSWindow *streamPanel;
@property (weak) IBOutlet NSTextField *streamDestinationLabel;
@property (weak) IBOutlet NSTextField *streamTypeLabel;
@property (weak) IBOutlet NSTextField *streamAddressLabel;
@property (weak) IBOutlet NSPopUpButton *streamTypePopup;
@property (weak) IBOutlet NSTextField *streamAddressField;
@property (weak) IBOutlet NSTextField *streamTTLLabel;
@property (weak) IBOutlet NSTextField *streamTTLField;
@property (weak) IBOutlet NSStepper *streamTTLStepper;
@property (weak) IBOutlet NSTextField *streamPortLabel;
@property (weak) IBOutlet NSTextField *streamPortField;
@property (weak) IBOutlet NSTextField *streamAnnouncementLabel;
@property (weak) IBOutlet NSButton *streamSAPCheckbox;
@property (weak) IBOutlet NSTextField *streamChannelLabel;
@property (weak) IBOutlet NSTextField *streamChannelField;
@property (weak) IBOutlet NSMatrix *streamSDPMatrix;
@property (weak) IBOutlet NSButton *streamSDPFileBrowseButton;
@property (weak) IBOutlet NSTextField *streamSDPLabel;
@property (weak) IBOutlet NSTextField *streamSDPField;
@property (weak) IBOutlet NSButton *streamCancelButton;
@property (weak) IBOutlet NSButton *streamOkButton;

// other properties
@property (strong) VLCPopupPanelController *popupPanel;
@property (strong) VLCTextfieldPanelController *textfieldPanel;

// Bindings for field / stepper combis
@property (nonatomic) int vidBitrate;
@property (nonatomic) int vidFramerate;
@property (nonatomic) int audBitrate;
@property (nonatomic) int audChannels;


- (IBAction)finalizePanel:(id)sender;
- (IBAction)openMedia:(id)sender;
- (IBAction)switchProfile:(id)sender;
- (IBAction)iWantAFile:(id)sender;
- (IBAction)iWantAStream:(id)sender;
- (IBAction)cancelDestination:(id)sender;
- (IBAction)browseFileDestination:(id)sender;

- (IBAction)customizeProfile:(id)sender;
- (IBAction)closeCustomizationSheet:(id)sender;
- (IBAction)videoSettingsChanged:(id)sender;
- (IBAction)audioSettingsChanged:(id)sender;
- (IBAction)subSettingsChanged:(id)sender;
- (IBAction)newProfileAction:(id)sender;

- (IBAction)showStreamPanel:(id)sender;
- (IBAction)closeStreamPanel:(id)sender;
- (IBAction)streamTypeToggle:(id)sender;
- (IBAction)streamAnnouncementToggle:(id)sender;
- (IBAction)sdpFileLocationSelector:(id)sender;

@end
