/*****************************************************************************
 * VLCOpenWindowController.h: Open dialogues for VLC's MacOS X port
 *****************************************************************************
 * Copyright (C) 2002-2016 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
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

#import <Cocoa/Cocoa.h>

@interface VLCOpenWindowController : NSWindowController <NSTabViewDelegate>

@property (readwrite, weak) IBOutlet NSTextField *mrlTextField;
@property (readwrite, weak) IBOutlet NSButton *mrlButton;
@property (readwrite, weak) IBOutlet NSButton *mrlButtonLabel;
@property (readwrite, weak) IBOutlet NSTabView *tabView;
@property (readwrite, weak) IBOutlet NSLayoutConstraint *mrlViewHeightConstraint;

@property (readwrite, weak) IBOutlet NSButton *okButton;
@property (readwrite, weak) IBOutlet NSButton *cancelButton;

/* bottom-line items */
@property (readwrite, weak) IBOutlet NSButton *outputCheckbox;
@property (readwrite, weak) IBOutlet NSButton *outputSettingsButton;

/* open file */
@property (readwrite, weak) IBOutlet NSTextField *fileNameLabel;
@property (readwrite, weak) IBOutlet NSTextField *fileNameStubLabel;
@property (readwrite, weak) IBOutlet NSImageView *fileIconWell;
@property (readwrite, weak) IBOutlet NSButton *fileBrowseButton;
@property (readwrite, weak) IBOutlet NSButton *fileTreatAsPipeButton;
@property (readwrite, weak) IBOutlet NSButton *fileSlaveCheckbox;
@property (readwrite, weak) IBOutlet NSButton *fileSelectSlaveButton;
@property (readwrite, weak) IBOutlet NSTextField *fileSlaveFilenameLabel;
@property (readwrite, weak) IBOutlet NSImageView *fileSlaveIconWell;
@property (readwrite, weak) IBOutlet NSTextField *fileSubtitlesFilenameLabel;
@property (readwrite, weak) IBOutlet NSImageView *fileSubtitlesIconWell;
@property (readwrite, weak) IBOutlet NSButton *fileCustomTimingCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *fileStartTimeTextField;
@property (readwrite, weak) IBOutlet NSTextField *fileStartTimeLabel;
@property (readwrite, weak) IBOutlet NSTextField *fileStopTimeTextField;
@property (readwrite, weak) IBOutlet NSTextField *fileStopTimeLabel;

/* open disc */
@property (readwrite, weak) IBOutlet NSPopUpButton *discSelectorPopup;

@property (readwrite, weak) IBOutlet NSView *discNoDiscView;
@property (readwrite, weak) IBOutlet NSTextField *discNoDiscLabel;
@property (readwrite, weak) IBOutlet NSButton *discNoDiscVideoTSButton;

@property (readwrite, weak) IBOutlet NSView *discAudioCDView;
@property (readwrite, weak) IBOutlet NSTextField *discAudioCDLabel;
@property (readwrite, weak) IBOutlet NSTextField *discAudioCDTrackCountLabel;
@property (readwrite, weak) IBOutlet NSButton *discAudioCDVideoTSButton;

@property (readwrite, weak) IBOutlet NSView *discDVDView;
@property (readwrite, weak) IBOutlet NSTextField *discDVDLabel;
@property (readwrite, weak) IBOutlet NSButton *discDVDDisableMenusButton;
@property (readwrite, weak) IBOutlet NSButton *discDVDVideoTSButton;

@property (readwrite, weak) IBOutlet NSView *discDVDwomenusView;
@property (readwrite, weak) IBOutlet NSTextField *discDVDwomenusLabel;
@property (readwrite, weak) IBOutlet NSButton *discDVDwomenusEnableMenusButton;
@property (readwrite, weak) IBOutlet NSButton *discDVDwomenusVideoTSButton;
@property (readwrite, weak) IBOutlet NSTextField *discDVDwomenusTitleTextField;
@property (readwrite, weak) IBOutlet NSTextField *discDVDwomenusTitleLabel;
@property (readwrite, weak) IBOutlet NSStepper *discDVDwomenusTitleStepper;
@property (readwrite, weak) IBOutlet NSTextField *discDVDwomenusChapterTextField;
@property (readwrite, weak) IBOutlet NSTextField *discDVDwomenusChapterLabel;
@property (readwrite, weak) IBOutlet NSStepper *discDVDwomenusChapterStepper;

@property (readwrite, weak) IBOutlet NSView *discVCDView;
@property (readwrite, weak) IBOutlet NSTextField *discVCDLabel;
@property (readwrite, weak) IBOutlet NSButton *discVCDVideoTSButton;
@property (readwrite, weak) IBOutlet NSTextField *discVCDTitleTextField;
@property (readwrite, weak) IBOutlet NSTextField *discVCDTitleLabel;
@property (readwrite, weak) IBOutlet NSStepper *discVCDTitleStepper;
@property (readwrite, weak) IBOutlet NSTextField *discVCDChapterTextField;
@property (readwrite, weak) IBOutlet NSTextField *discVCDChapterLabel;
@property (readwrite, weak) IBOutlet NSStepper *discVCDChapterStepper;

@property (readwrite, weak) IBOutlet NSView *discBDView;
@property (readwrite, weak) IBOutlet NSTextField *discBDLabel;
@property (readwrite, weak) IBOutlet NSButton *discBDVideoTSButton;

/* open network */
@property (readwrite, weak) IBOutlet NSTextField *netHTTPURLLabel;
@property (readwrite, weak) IBOutlet NSTextField *netHTTPURLTextField;
@property (readwrite, weak) IBOutlet NSTextField *netHelpLabel;

/* open UDP stuff panel */
@property (readwrite, weak) IBOutlet NSTextField *netHelpUDPLabel;
@property (readwrite, weak) IBOutlet NSMatrix *netUDPProtocolMatrix;
@property (readwrite, weak) IBOutlet NSTextField *netUDPProtocolLabel;
@property (readwrite, weak) IBOutlet NSTextField *netUDPAddressLabel;
@property (readwrite, weak) IBOutlet NSTextField *netUDPModeLabel;
@property (readwrite, weak) IBOutlet NSMatrix *netModeMatrix;
@property (readwrite, weak) IBOutlet NSButton *netOpenUDPButton;
@property (readwrite, weak) IBOutlet NSButton *netUDPCancelButton;
@property (readwrite, weak) IBOutlet NSButton *netUDPOKButton;
@property (readwrite)       IBOutlet NSWindow *netUDPPanel;
@property (readwrite, weak) IBOutlet NSTextField *netUDPPortTextField;
@property (readwrite, weak) IBOutlet NSTextField *netUDPPortLabel;
@property (readwrite, weak) IBOutlet NSStepper *netUDPPortStepper;
@property (readwrite, weak) IBOutlet NSTextField *netUDPMAddressTextField;
@property (readwrite, weak) IBOutlet NSTextField *netUDPMAddressLabel;
@property (readwrite, weak) IBOutlet NSTextField *netUDPMPortTextField;
@property (readwrite, weak) IBOutlet NSTextField *netUDPMPortLabel;
@property (readwrite, weak) IBOutlet NSStepper *netUDPMPortStepper;

/* open subtitle file */
@property (readwrite, weak) IBOutlet NSButton *fileSubCheckbox;
@property (readwrite, weak) IBOutlet NSButton *fileSubSettingsButton;
@property (readwrite) IBOutlet NSPanel *fileSubSheet;
@property (readwrite, weak) IBOutlet NSTextField *fileSubPathLabel;
@property (readwrite, weak) IBOutlet NSTextField *fileSubPathTextField;
@property (readwrite, weak) IBOutlet NSImageView *fileSubIconView;
@property (readwrite, weak) IBOutlet NSButton *fileSubBrowseButton;
@property (readwrite, weak) IBOutlet NSButton *fileSubOverrideCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *fileSubDelayTextField;
@property (readwrite, weak) IBOutlet NSTextField *fileSubDelayLabel;
@property (readwrite, weak) IBOutlet NSStepper *fileSubDelayStepper;
@property (readwrite, weak) IBOutlet NSTextField *fileSubFPSTextField;
@property (readwrite, weak) IBOutlet NSTextField *fileSubFPSLabel;
@property (readwrite, weak) IBOutlet NSStepper *fileSubFPSStepper;
@property (readwrite, weak) IBOutlet NSPopUpButton *fileSubEncodingPopup;
@property (readwrite, weak) IBOutlet NSTextField *fileSubEncodingLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *fileSubAlignPopup;
@property (readwrite, weak) IBOutlet NSTextField *fileSubAlignLabel;
@property (readwrite, weak) IBOutlet NSButton *fileSubOKButton;
@property (readwrite, weak) IBOutlet NSBox *fileSubFontBox;
@property (readwrite, weak) IBOutlet NSBox *fileSubFileBox;

/* generic capturing stuff */
@property (readwrite, weak) IBOutlet NSPopUpButton *captureModePopup;
@property (readwrite, weak) IBOutlet NSTabView *captureTabView;

/* screen support */
@property (readwrite, weak) IBOutlet NSTextField *screenFPSTextField;
@property (readwrite, weak) IBOutlet NSTextField *screenFPSLabel;
@property (readwrite, weak) IBOutlet NSStepper *screenFPSStepper;
@property (readwrite, weak) IBOutlet NSTextField *screenLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *screenPopup;
@property (readwrite, weak) IBOutlet NSTextField *screenLeftTextField;
@property (readwrite, weak) IBOutlet NSTextField *screenLeftLabel;
@property (readwrite, weak) IBOutlet NSStepper *screenLeftStepper;
@property (readwrite, weak) IBOutlet NSTextField *screenTopTextField;
@property (readwrite, weak) IBOutlet NSTextField *screenTopLabel;
@property (readwrite, weak) IBOutlet NSStepper *screenTopStepper;
@property (readwrite, weak) IBOutlet NSTextField *screenWidthTextField;
@property (readwrite, weak) IBOutlet NSTextField *screenWidthLabel;
@property (readwrite, weak) IBOutlet NSStepper *screenWidthStepper;
@property (readwrite, weak) IBOutlet NSTextField *screenHeightTextField;
@property (readwrite, weak) IBOutlet NSTextField *screenHeightLabel;
@property (readwrite, weak) IBOutlet NSStepper *screenHeightStepper;
@property (readwrite, weak) IBOutlet NSButton *screenFollowMouseCheckbox;
@property (readwrite, weak) IBOutlet NSPopUpButton *screenqtkAudioPopup;
@property (readwrite, weak) IBOutlet NSButton *screenqtkAudioCheckbox;

/* QTK support */
@property (readwrite, weak) IBOutlet NSPopUpButton *qtkVideoDevicePopup;
@property (readwrite, weak) IBOutlet NSButton *qtkVideoCheckbox;
@property (readwrite, weak) IBOutlet NSPopUpButton *qtkAudioDevicePopup;
@property (readwrite, weak) IBOutlet NSButton *qtkAudioCheckbox;

/* text field / stepper binding values - subs panel */
@property (nonatomic) float fileSubDelay;
@property (nonatomic) float fileSubFps;

- (IBAction)outputSettings:(id)sender;
- (void)tabView:(NSTabView *)o_tv didSelectTabViewItem:(NSTabViewItem *)o_tvi;
- (void)textFieldWasClicked:(NSNotification *)o_notification;
- (IBAction)expandMRLfieldAction:(id)sender;
- (IBAction)inputSlaveAction:(id)sender;
- (IBAction)fileTimeCustomization:(id)sender;

- (void)openFileGeneric;
- (IBAction)openFileBrowse:(id)sender;
- (IBAction)openFileStreamChanged:(id)sender;

- (void)openDisc;
- (IBAction)discSelectorChanged:(id)sender;
- (IBAction)openSpecialMediaFolder:(id)sender;
- (IBAction)dvdreadOptionChanged:(id)sender;
- (IBAction)vcdOptionChanged:(id)sender;

- (void)openNet;
- (IBAction)openNetModeChanged:(id)sender;
- (IBAction)openNetStepperChanged:(id)sender;
- (void)openNetInfoChanged:(NSNotification *)o_notification;
- (IBAction)openNetUDPButtonAction:(id)sender;

- (void)openCapture;
- (IBAction)openCaptureModeChanged:(id)sender;

// Screen actions
- (IBAction)screenChanged:(id)sender;
- (IBAction)screenAudioChanged:(id)sender;

// QTKit actions
- (IBAction)qtkChanged:(id)sender;
- (IBAction)qtkAudioChanged:(id)sender;
- (IBAction)qtkToggleUIElements:(id)sender;

- (IBAction)subsChanged:(id)sender;
- (IBAction)subSettings:(id)sender;
- (IBAction)subFileBrowse:(id)sender;
- (IBAction)subOverride:(id)sender;

- (IBAction)subCloseSheet:(id)sender;

- (IBAction)panelCancel:(id)sender;
- (IBAction)panelOk:(id)sender;

- (void)openFileWithAction:(void (^)(NSArray *files))action;

@end
