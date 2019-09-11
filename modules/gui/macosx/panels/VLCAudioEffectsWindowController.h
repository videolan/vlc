/*****************************************************************************
 * VLCAudioEffectsWindowController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2004-2017 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Jérôme Decoodt <djc@videolan.org>
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

@class VLCPopupPanelController;
@class VLCTextfieldPanelController;

@interface VLCAudioEffectsWindowController : NSWindowController

/* generic */
@property (readwrite, weak) IBOutlet NSSegmentedControl *segmentView;
@property (readwrite, weak) IBOutlet NSPopUpButton *profilePopup;
@property (readwrite, weak) IBOutlet NSButton *applyProfileCheckbox;

/* Equalizer */
@property (readwrite, weak) IBOutlet NSView *equalizerView;
@property (readwrite, weak) IBOutlet NSButton *equalizerEnableCheckbox;
@property (readwrite, weak) IBOutlet NSButton *equalizerTwoPassCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *equalizerPreampLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *equalizerPresetsPopup;
@property (readwrite, weak) IBOutlet NSSlider *equalizerBand1Slider;
@property (readwrite, weak) IBOutlet NSSlider *equalizerBand2Slider;
@property (readwrite, weak) IBOutlet NSSlider *equalizerBand3Slider;
@property (readwrite, weak) IBOutlet NSSlider *equalizerBand4Slider;
@property (readwrite, weak) IBOutlet NSSlider *equalizerBand5Slider;
@property (readwrite, weak) IBOutlet NSSlider *equalizerBand6Slider;
@property (readwrite, weak) IBOutlet NSSlider *equalizerBand7Slider;
@property (readwrite, weak) IBOutlet NSSlider *equalizerBand8Slider;
@property (readwrite, weak) IBOutlet NSSlider *equalizerBand9Slider;
@property (readwrite, weak) IBOutlet NSSlider *equalizerBand10Slider;
@property (readwrite, weak) IBOutlet NSSlider *equalizerPreampSlider;

/* Compressor */
@property (readwrite, weak) IBOutlet NSView *compressorView;
@property (readwrite, weak) IBOutlet NSButton *compressorEnableCheckbox;
@property (readwrite, weak) IBOutlet NSButton *compressorResetButton;
@property (readwrite, weak) IBOutlet NSSlider *compressorBand1Slider;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand1TextField;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand1Label;
@property (readwrite, weak) IBOutlet NSSlider *compressorBand2Slider;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand2TextField;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand2Label;
@property (readwrite, weak) IBOutlet NSSlider *compressorBand3Slider;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand3TextField;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand3Label;
@property (readwrite, weak) IBOutlet NSSlider *compressorBand4Slider;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand4TextField;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand4Label;
@property (readwrite, weak) IBOutlet NSSlider *compressorBand5Slider;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand5TextField;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand5Label;
@property (readwrite, weak) IBOutlet NSSlider *compressorBand6Slider;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand6TextField;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand6Label;
@property (readwrite, weak) IBOutlet NSSlider *compressorBand7Slider;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand7TextField;
@property (readwrite, weak) IBOutlet NSTextField *compressorBand7Label;

/* Spatializer */
@property (readwrite, weak) IBOutlet NSView *spatializerView;
@property (readwrite, weak) IBOutlet NSButton *spatializerEnableCheckbox;
@property (readwrite, weak) IBOutlet NSButton *spatializerResetButton;
@property (readwrite, weak) IBOutlet NSSlider *spatializerBand1Slider;
@property (readwrite, weak) IBOutlet NSTextField *spatializerBand1TextField;
@property (readwrite, weak) IBOutlet NSTextField *spatializerBand1Label;
@property (readwrite, weak) IBOutlet NSSlider *spatializerBand2Slider;
@property (readwrite, weak) IBOutlet NSTextField *spatializerBand2TextField;
@property (readwrite, weak) IBOutlet NSTextField *spatializerBand2Label;
@property (readwrite, weak) IBOutlet NSSlider *spatializerBand3Slider;
@property (readwrite, weak) IBOutlet NSTextField *spatializerBand3TextField;
@property (readwrite, weak) IBOutlet NSTextField *spatializerBand3Label;
@property (readwrite, weak) IBOutlet NSSlider *spatializerBand4Slider;
@property (readwrite, weak) IBOutlet NSTextField *spatializerBand4TextField;
@property (readwrite, weak) IBOutlet NSTextField *spatializerBand4Label;
@property (readwrite, weak) IBOutlet NSSlider *spatializerBand5Slider;
@property (readwrite, weak) IBOutlet NSTextField *spatializerBand5TextField;
@property (readwrite, weak) IBOutlet NSTextField *spatializerBand5Label;

/* Filter */
@property (readwrite, weak) IBOutlet NSButton *filterHeadPhoneCheckbox;
@property (readwrite, weak) IBOutlet NSButton *filterNormLevelCheckbox;
@property (readwrite, weak) IBOutlet NSSlider *filterNormLevelSlider;
@property (readwrite, weak) IBOutlet NSTextField *filterNormLevelLabel;
@property (readwrite, weak) IBOutlet NSButton *filterKaraokeCheckbox;
@property (readwrite, weak) IBOutlet NSButton *filterScaleTempoCheckbox;
@property (readwrite, weak) IBOutlet NSButton *filterStereoEnhancerCheckbox;

/* generic */
- (IBAction)profileSelectorAction:(id)sender;
- (IBAction)applyProfileCheckboxChanged:(id)sender;

- (void)toggleWindow:(id)sender;

/* Equalizer */
- (IBAction)equalizerBandSliderUpdated:(id)sender;
- (IBAction)equalizerChangePreset:(id)sender;
- (IBAction)equalizerEnable:(id)sender;
- (IBAction)equalizerPreAmpSliderUpdated:(id)sender;
- (IBAction)equalizerTwoPass:(id)sender;

/* Compressor */
- (IBAction)resetCompressorValues:(id)sender;
- (IBAction)compressorEnable:(id)sender;
- (IBAction)compressorSliderUpdated:(id)sender;

/* Spatializer */
- (IBAction)resetSpatializerValues:(id)sender;
- (IBAction)spatializerEnable:(id)sender;
- (IBAction)spatializerSliderUpdated:(id)sender;

/* Filter */
- (IBAction)filterEnableHeadPhoneVirt:(id)sender;
- (IBAction)filterEnableVolumeNorm:(id)sender;
- (IBAction)filterVolumeNormSliderUpdated:(id)sender;
- (IBAction)filterEnableKaraoke:(id)sender;
- (IBAction)filterEnableScaleTempo:(id)sender;
- (IBAction)filterEnableStereoEnhancer:(id)sender;

@end
