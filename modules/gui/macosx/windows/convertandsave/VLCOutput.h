/*****************************************************************************
 * VLCOutput.h: MacOS X Output Dialog
 *****************************************************************************
 * Copyright (C) 2002-2015 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
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

#import <Cocoa/Cocoa.h>

@interface VLCOutput : NSObject

@property (readwrite) IBOutlet NSPanel *outputSheet;
@property (readwrite, weak) IBOutlet NSButton *okButton;

@property (readwrite, weak) IBOutlet NSBox *optionsBox;
@property (readwrite, weak) IBOutlet NSButton *displayOnLocalScreenCheckbox;
@property (readwrite, weak) IBOutlet NSMatrix *outputMethodMatrix;
@property (readwrite, weak) IBOutlet NSTextField *fileTextField;
@property (readwrite, weak) IBOutlet NSButton *dumpCheckbox;
@property (readwrite, weak) IBOutlet NSButton *browseButton;
@property (readwrite, weak) IBOutlet NSTextField *streamAddressTextField;
@property (readwrite, weak) IBOutlet NSTextField *streamAddressLabel;
@property (readwrite, weak) IBOutlet NSTextField *streamPortTextField;
@property (readwrite, weak) IBOutlet NSStepper *streamPortStepper;
@property (readwrite, weak) IBOutlet NSTextField *streamPortLabel;
@property (readwrite, weak) IBOutlet NSTextField *streamTTLTextField;
@property (readwrite, weak) IBOutlet NSStepper *streamTTLStepper;
@property (readwrite, weak) IBOutlet NSTextField *streamTTLLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *streamTypePopup;
@property (readwrite, weak) IBOutlet NSTextField *streamTypeLabel;

@property (readwrite, weak) IBOutlet NSTextField *muxLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *muxSelectorPopup;

@property (readwrite, weak) IBOutlet NSBox *transcodeBox;
@property (readwrite, weak) IBOutlet NSComboBox *transcodeAudioBitrateComboBox;
@property (readwrite, weak) IBOutlet NSTextField *transcodeAudioBitrateLabel;
@property (readwrite, weak) IBOutlet NSComboBox *transcodeAudioChannelsComboBox;
@property (readwrite, weak) IBOutlet NSTextField *transcodeAudioChannelsLabel;
@property (readwrite, weak) IBOutlet NSButton *transcodeAudioCheckbox;
@property (readwrite, weak) IBOutlet NSPopUpButton *transcodeAudioSelectorPopup;
@property (readwrite, weak) IBOutlet NSComboBox *transcodeVideoBitrateComboBox;
@property (readwrite, weak) IBOutlet NSTextField *transcodeVideoBitrateLabel;
@property (readwrite, weak) IBOutlet NSComboBox *transcodeVideoScaleComboBox;
@property (readwrite, weak) IBOutlet NSTextField *transcodeVideoScaleLabel;
@property (readwrite, weak) IBOutlet NSButton *transcodeVideoCheckbox;
@property (readwrite, weak) IBOutlet NSPopUpButton *transcodeVideoSelectorPopup;

@property (readwrite, weak) IBOutlet NSBox *miscBox;
@property (readwrite, weak) IBOutlet NSButton *sapCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *channelNameTextField;
@property (readwrite, weak) IBOutlet NSTextField *channelNameLabel;

@property (readwrite, weak) IBOutlet NSTextField *sdpURLTextField;
@property (readwrite, weak) IBOutlet NSTextField *sdpURLLabel;
@property (readwrite, weak) IBOutlet NSButton *rtspCheckbox;
@property (readwrite, weak) IBOutlet NSButton *httpCheckbox;
@property (readwrite, weak) IBOutlet NSButton *fileCheckbox;

@property (readonly) NSArray *soutMRL;

- (IBAction)outputCloseSheet:(id)sender;
- (IBAction)outputMethodChanged:(id)sender;
- (IBAction)outputInfoChanged:(id)object;
- (IBAction)outputFileBrowse:(id)sender;
- (IBAction)streamPortStepperChanged:(id)sender;
- (IBAction)streamTTLStepperChanged:(id)sender;
- (IBAction)transcodeChanged:(id)sender;
- (IBAction)transcodeInfoChanged:(id)object;
- (IBAction)announceChanged:(id)sender;

@end
