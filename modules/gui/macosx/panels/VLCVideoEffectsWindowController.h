/*****************************************************************************
 * VLCVideoEffectsWindowController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan dot org>
 *          David Fuhrmann <dfuhrmann # videolan dot org>
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

@interface VLCVideoEffectsWindowController : NSWindowController

/* generic */
@property (readwrite, weak) IBOutlet NSSegmentedControl *segmentView;
@property (readwrite, weak) IBOutlet NSPopUpButton *profilePopup;
@property (readwrite, weak) IBOutlet NSButton *applyProfileCheckbox;

/* basic */
@property (readwrite, weak) IBOutlet NSButton *adjustCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *adjustHueLabel;
@property (readwrite, weak) IBOutlet NSSlider *adjustHueSlider;
@property (readwrite, weak) IBOutlet NSTextField *adjustContrastLabel;
@property (readwrite, weak) IBOutlet NSSlider *adjustContrastSlider;
@property (readwrite, weak) IBOutlet NSTextField *adjustBrightnessLabel;
@property (readwrite, weak) IBOutlet NSSlider *adjustBrightnessSlider;
@property (readwrite, weak) IBOutlet NSButton *adjustBrightnessCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *adjustSaturationLabel;
@property (readwrite, weak) IBOutlet NSSlider *adjustSaturationSlider;
@property (readwrite, weak) IBOutlet NSTextField *adjustGammaLabel;
@property (readwrite, weak) IBOutlet NSSlider *adjustGammaSlider;
@property (readwrite, weak) IBOutlet NSButton *adjustResetButton;
@property (readwrite, weak) IBOutlet NSButton *sharpenCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *sharpenLabel;
@property (readwrite, weak) IBOutlet NSSlider *sharpenSlider;
@property (readwrite, weak) IBOutlet NSButton *bandingCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *bandingLabel;
@property (readwrite, weak) IBOutlet NSSlider *bandingSlider;
@property (readwrite, weak) IBOutlet NSButton *grainCheckbox;
@property (readwrite, weak) IBOutlet NSSlider *grainSlider;
@property (readwrite, weak) IBOutlet NSTextField *grainLabel;

/* crop */
@property (readwrite, weak) IBOutlet NSTextField *cropTopLabel;
@property (readwrite, weak) IBOutlet NSTextField *cropTopTextField;
@property (readwrite, weak) IBOutlet NSStepper *cropTopStepper;
@property (readwrite, weak) IBOutlet NSTextField *cropLeftLabel;
@property (readwrite, weak) IBOutlet NSTextField *cropLeftTextField;
@property (readwrite, weak) IBOutlet NSStepper *cropLeftStepper;
@property (readwrite, weak) IBOutlet NSTextField *cropRightLabel;
@property (readwrite, weak) IBOutlet NSTextField *cropRightTextField;
@property (readwrite, weak) IBOutlet NSStepper *cropRightStepper;
@property (readwrite, weak) IBOutlet NSTextField *cropBottomLabel;
@property (readwrite, weak) IBOutlet NSTextField *cropBottomTextField;
@property (readwrite, weak) IBOutlet NSStepper *cropBottomStepper;
@property (readwrite, weak) IBOutlet NSButton *cropSyncTopBottomCheckbox;
@property (readwrite, weak) IBOutlet NSButton *cropSyncLeftRightCheckbox;

/* geometry */
@property (readwrite, weak) IBOutlet NSButton *transformCheckbox;
@property (readwrite, weak) IBOutlet NSPopUpButton *transformPopup;
@property (readwrite, weak) IBOutlet NSButton *zoomCheckbox;
@property (readwrite, weak) IBOutlet NSButton *puzzleCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *puzzleRowsLabel;
@property (readwrite, weak) IBOutlet NSTextField *puzzleRowsTextField;
@property (readwrite, weak) IBOutlet NSStepper *puzzleRowsStepper;
@property (readwrite, weak) IBOutlet NSTextField *puzzleColumnsLabel;
@property (readwrite, weak) IBOutlet NSTextField *puzzleColumnsTextField;
@property (readwrite, weak) IBOutlet NSStepper *puzzleColumnsStepper;
@property (readwrite, weak) IBOutlet NSButton *cloneCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *cloneNumberTextField;
@property (readwrite, weak) IBOutlet NSStepper *cloneNumberStepper;
@property (readwrite, weak) IBOutlet NSTextField *cloneNumberLabel;
@property (readwrite, weak) IBOutlet NSButton *wallCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *wallNumbersOfRowsTextField;
@property (readwrite, weak) IBOutlet NSStepper *wallNumbersOfRowsStepper;
@property (readwrite, weak) IBOutlet NSTextField *wallNumbersOfRowsLabel;
@property (readwrite, weak) IBOutlet NSTextField *wallNumberOfColumnsTextField;
@property (readwrite, weak) IBOutlet NSStepper *wallNumberOfColumnsStepper;
@property (readwrite, weak) IBOutlet NSTextField *wallNumberOfColumnsLabel;

/* color */
@property (readwrite, weak) IBOutlet NSButton *thresholdCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *thresholdColorLabel;
@property (readwrite, weak) IBOutlet NSTextField *thresholdColorTextField;
@property (readwrite, weak) IBOutlet NSTextField *thresholdSaturationLabel;
@property (readwrite, weak) IBOutlet NSSlider *thresholdSaturationSlider;
@property (readwrite, weak) IBOutlet NSTextField *thresholdSimilarityLabel;
@property (readwrite, weak) IBOutlet NSSlider *thresholdSimilaritySlider;
@property (readwrite, weak) IBOutlet NSButton *sepiaCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *sepiaLabel;
@property (readwrite, weak) IBOutlet NSTextField *sepiaTextField;
@property (readwrite, weak) IBOutlet NSStepper *sepiaStepper;
@property (readwrite, weak) IBOutlet NSButton *gradientCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *gradientModeLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *gradientModePopup;
@property (readwrite, weak) IBOutlet NSButton *gradientColorCheckbox;
@property (readwrite, weak) IBOutlet NSButton *gradientCartoonCheckbox;
@property (readwrite, weak) IBOutlet NSButton *extractCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *extractLabel;
@property (readwrite, weak) IBOutlet NSTextField *extractTextField;
@property (readwrite, weak) IBOutlet NSButton *invertCheckbox;
@property (readwrite, weak) IBOutlet NSButton *posterizeCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *posterizeLabel;
@property (readwrite, weak) IBOutlet NSTextField *posterizeTextField;
@property (readwrite, weak) IBOutlet NSStepper *posterizeStepper;
@property (readwrite, weak) IBOutlet NSButton *blurCheckbox;
@property (readwrite, weak) IBOutlet NSSlider *blurSlider;
@property (readwrite, weak) IBOutlet NSTextField *blurLabel;
@property (readwrite, weak) IBOutlet NSButton *motiondetectCheckbox;
@property (readwrite, weak) IBOutlet NSButton *watereffectCheckbox;
@property (readwrite, weak) IBOutlet NSButton *wavesCheckbox;
@property (readwrite, weak) IBOutlet NSButton *psychedelicCheckbox;

/* misc */
@property (readwrite, weak) IBOutlet NSButton *addTextCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *addTextTextTextField;
@property (readwrite, weak) IBOutlet NSTextField *addTextTextLabel;
@property (readwrite, weak) IBOutlet NSTextField *addTextPositionLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *addTextPositionPopup;
@property (readwrite, weak) IBOutlet NSButton *addLogoCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *addLogoLogoLabel;
@property (readwrite, weak) IBOutlet NSTextField *addLogoLogoTextField;
@property (readwrite, weak) IBOutlet NSTextField *addLogoPositionLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *addLogoPositionPopup;
@property (readwrite, weak) IBOutlet NSTextField *addLogoTransparencyLabel;
@property (readwrite, weak) IBOutlet NSSlider *addLogoTransparencySlider;
@property (readwrite, weak) IBOutlet NSButton *anaglyphCheckbox;

/* text field / stepper binding values */
/* use setter to modify gui elements */
@property (nonatomic) int cropLeftValue;
@property (nonatomic) int cropTopValue;
@property (nonatomic) int cropRightValue;
@property (nonatomic) int cropBottomValue;

@property (nonatomic) int puzzleRowsValue;
@property (nonatomic) int puzzleColumnsValue;

@property (nonatomic) int wallRowsValue;
@property (nonatomic) int wallColumnsValue;

@property (nonatomic) int cloneValue;

@property (nonatomic) int sepiaValue;

@property (nonatomic) int posterizeValue;

/* generic */
- (void)toggleWindow:(id)sender;
- (IBAction)profileSelectorAction:(id)sender;
- (IBAction)applyProfileCheckboxChanged:(id)sender;

/* basic */
- (IBAction)enableAdjust:(id)sender;
- (IBAction)adjustSliderChanged:(id)sender;
- (IBAction)enableAdjustBrightnessThreshold:(id)sender;
- (IBAction)enableSharpen:(id)sender;
- (IBAction)sharpenSliderChanged:(id)sender;
- (IBAction)enableBanding:(id)sender;
- (IBAction)bandingSliderChanged:(id)sender;
- (IBAction)enableGrain:(id)sender;
- (IBAction)grainSliderChanged:(id)sender;

/* crop */
- (IBAction)cropObjectChanged:(id)sender;

/* geometry */
- (IBAction)enableTransform:(id)sender;
- (IBAction)transformModifierChanged:(id)sender;
- (IBAction)enableZoom:(id)sender;
- (IBAction)enablePuzzle:(id)sender;
- (IBAction)puzzleModifierChanged:(id)sender;
- (IBAction)enableClone:(id)sender;
- (IBAction)cloneModifierChanged:(id)sender;
- (IBAction)enableWall:(id)sender;
- (IBAction)wallModifierChanged:(id)sender;

/* color */
- (IBAction)enableThreshold:(id)sender;
- (IBAction)thresholdModifierChanged:(id)sender;
- (IBAction)enableSepia:(id)sender;
- (IBAction)sepiaModifierChanged:(id)sender;
- (IBAction)enableGradient:(id)sender;
- (IBAction)gradientModifierChanged:(id)sender;
- (IBAction)enableExtract:(id)sender;
- (IBAction)extractModifierChanged:(id)sender;
- (IBAction)enableInvert:(id)sender;
- (IBAction)enablePosterize:(id)sender;
- (IBAction)posterizeModifierChanged:(id)sender;
- (IBAction)enableBlur:(id)sender;
- (IBAction)blurModifierChanged:(id)sender;
- (IBAction)enableMotionDetect:(id)sender;
- (IBAction)enableWaterEffect:(id)sender;
- (IBAction)enableWaves:(id)sender;
- (IBAction)enablePsychedelic:(id)sender;

/* miscellaneous */
- (IBAction)enableAddText:(id)sender;
- (IBAction)addTextModifierChanged:(id)sender;
- (IBAction)enableAddLogo:(id)sender;
- (IBAction)addLogoModifierChanged:(id)sender;
- (IBAction)enableAnaglyph:(id)sender;

@end
