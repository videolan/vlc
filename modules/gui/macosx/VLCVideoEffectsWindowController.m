/*****************************************************************************
 * VLCVideoEffectsWindowController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2015 Felix Paul Kühne
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

#import "CompatibilityFixes.h"
#import "intf.h"
#import "VLCVideoEffectsWindowController.h"
#import "SharedDialogs.h"
#import "CoreInteraction.h"

@interface VLCVideoEffectsWindowController()
{
    NSInteger i_old_profile_index;
}
@end

#pragma mark -
#pragma mark Initialization

@implementation VLCVideoEffectsWindowController

+ (void)initialize
{
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:[NSArray arrayWithObject:@";;;0;1.000000;1.000000;1.000000;1.000000;0.050000;16;2.000000;OTA=;4;4;16711680;20;15;120;Z3JhZGllbnQ=;1;0;16711680;6;80;VkxD;-1;;-1;255;2;3;3"], @"VideoEffectProfiles",
                                 [NSArray arrayWithObject:_NS("Default")], @"VideoEffectProfileNames", nil];
    [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
}

- (id)init
{
    self = [super initWithWindowNibName:@"VideoEffects"];
    if (self) {
        i_old_profile_index = -1;

        self.popupPanel = [[VLCPopupPanelController alloc] init];
        self.textfieldPanel = [[VLCTextfieldPanelController alloc] init];
    }

    return self;
}

- (void)windowDidLoad
{
    [self.window setTitle: _NS("Video Effects")];
    [self.window setExcludedFromWindowsMenu:YES];
    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [[_tabView tabViewItemAtIndex:[_tabView indexOfTabViewItemWithIdentifier:@"basic"]] setLabel:_NS("Basic")];
    [[_tabView tabViewItemAtIndex:[_tabView indexOfTabViewItemWithIdentifier:@"crop"]] setLabel:_NS("Crop")];
    [[_tabView tabViewItemAtIndex:[_tabView indexOfTabViewItemWithIdentifier:@"geometry"]] setLabel:_NS("Geometry")];
    [[_tabView tabViewItemAtIndex:[_tabView indexOfTabViewItemWithIdentifier:@"color"]] setLabel:_NS("Color")];
    [[_tabView tabViewItemAtIndex:[_tabView indexOfTabViewItemWithIdentifier:@"misc"]] setLabel:_NS("Miscellaneous")];

    [self resetProfileSelector];

    [_adjustCheckbox setTitle:_NS("Image Adjust")];
    [_adjustHueLabel setStringValue:_NS("Hue")];
    [_adjustContrastLabel setStringValue:_NS("Contrast")];
    [_adjustBrightnessLabel setStringValue:_NS("Brightness")];
    [_adjustBrightnessCheckbox setTitle:_NS("Brightness Threshold")];
    [_adjustSaturationLabel setStringValue:_NS("Saturation")];
    [_adjustGammaLabel setStringValue:_NS("Gamma")];
    [_adjustResetButton setTitle: _NS("Reset")];
    [_sharpenCheckbox setTitle:_NS("Sharpen")];
    [_sharpenLabel setStringValue:_NS("Sigma")];
    [_bandingCheckbox setTitle:_NS("Banding removal")];
    [_bandingLabel setStringValue:_NS("Radius")];
    [_grainCheckbox setTitle:_NS("Film Grain")];
    [_grainLabel setStringValue:_NS("Variance")];
    [_cropTopLabel setStringValue:_NS("Top")];
    [_cropLeftLabel setStringValue:_NS("Left")];
    [_cropRightLabel setStringValue:_NS("Right")];
    [_cropBottomLabel setStringValue:_NS("Bottom")];
    [_cropSyncTopBottomCheckbox setTitle:_NS("Synchronize top and bottom")];
    [_cropSyncLeftRightCheckbox setTitle:_NS("Synchronize left and right")];

    [_transformCheckbox setTitle:_NS("Transform")];
    [_transformPopup removeAllItems];
    [_transformPopup addItemWithTitle: _NS("Rotate by 90 degrees")];
    [[_transformPopup lastItem] setTag: 90];
    [_transformPopup addItemWithTitle: _NS("Rotate by 180 degrees")];
    [[_transformPopup lastItem] setTag: 180];
    [_transformPopup addItemWithTitle: _NS("Rotate by 270 degrees")];
    [[_transformPopup lastItem] setTag: 270];
    [_transformPopup addItemWithTitle: _NS("Flip horizontally")];
    [[_transformPopup lastItem] setTag: 1];
    [_transformPopup addItemWithTitle: _NS("Flip vertically")];
    [[_transformPopup lastItem] setTag: 2];
    [_zoomCheckbox setTitle:_NS("Magnification/Zoom")];
    [_puzzleCheckbox setTitle:_NS("Puzzle game")];
    [_puzzleRowsLabel setStringValue:_NS("Rows")];
    [_puzzleColumnsLabel setStringValue:_NS("Columns")];
    [_cloneCheckbox setTitle:_NS("Clone")];
    [_cloneNumberLabel setStringValue:_NS("Number of clones")];
    [_wallCheckbox setTitle:_NS("Wall")];
    [_wallNumbersOfRowsLabel setStringValue:_NS("Rows")];
    [_wallNumberOfColumnsLabel setStringValue:_NS("Columns")];

    [_thresholdCheckbox setTitle:_NS("Color threshold")];
    [_thresholdColorLabel setStringValue:_NS("Color")];
    [_thresholdSaturationLabel setStringValue:_NS("Saturation")];
    [_thresholdSimilarityLabel setStringValue:_NS("Similarity")];
    [_sepiaCheckbox setTitle:_NS("Sepia")];
    [_sepiaLabel setStringValue:_NS("Intensity")];
    [_noiseCheckbox setTitle:_NS("Noise")];
    [_gradientCheckbox setTitle:_NS("Gradient")];
    [_gradientModeLabel setStringValue:_NS("Mode")];
    [_gradientModePopup removeAllItems];
    [_gradientModePopup addItemWithTitle: _NS("Gradient")];
    [[_gradientModePopup lastItem] setTag: 1];
    [_gradientModePopup addItemWithTitle: _NS("Edge")];
    [[_gradientModePopup lastItem] setTag: 2];
    [_gradientModePopup addItemWithTitle: _NS("Hough")];
    [[_gradientModePopup lastItem] setTag: 3];
    [_gradientColorCheckbox setTitle:_NS("Color")];
    [_gradientCartoonCheckbox setTitle:_NS("Cartoon")];
    [_extractCheckbox setTitle:_NS("Color extraction")];
    [_extractLabel setStringValue:_NS("Color")];
    [_invertCheckbox setTitle:_NS("Invert colors")];
    [_posterizeCheckbox setTitle:_NS("Posterize")];
    [_posterizeLabel setStringValue:_NS("Posterize level")];
    [_blurCheckbox setTitle:_NS("Motion blur")];
    [_blurLabel setStringValue:_NS("Factor")];
    [_motiondetectCheckbox setTitle:_NS("Motion Detect")];
    [_watereffectCheckbox setTitle:_NS("Water effect")];
    [_wavesCheckbox setTitle:_NS("Waves")];
    [_psychedelicCheckbox setTitle:_NS("Psychedelic")];
    [_anaglyphCheckbox setTitle:_NS("Anaglyph")];

    [_addTextCheckbox setTitle:_NS("Add text")];
    [_addTextTextLabel setStringValue:_NS("Text")];
    [_addTextPositionLabel setStringValue:_NS("Position")];
    [_addTextPositionPopup removeAllItems];
    [_addTextPositionPopup addItemWithTitle: _NS("Center")];
    [[_addTextPositionPopup lastItem] setTag: 0];
    [_addTextPositionPopup addItemWithTitle: _NS("Left")];
    [[_addTextPositionPopup lastItem] setTag: 1];
    [_addTextPositionPopup addItemWithTitle: _NS("Right")];
    [[_addTextPositionPopup lastItem] setTag: 2];
    [_addTextPositionPopup addItemWithTitle: _NS("Top")];
    [[_addTextPositionPopup lastItem] setTag: 4];
    [_addTextPositionPopup addItemWithTitle: _NS("Bottom")];
    [[_addTextPositionPopup lastItem] setTag: 8];
    [_addTextPositionPopup addItemWithTitle: _NS("Top-Left")];
    [[_addTextPositionPopup lastItem] setTag: 5];
    [_addTextPositionPopup addItemWithTitle: _NS("Top-Right")];
    [[_addTextPositionPopup lastItem] setTag: 6];
    [_addTextPositionPopup addItemWithTitle: _NS("Bottom-Left")];
    [[_addTextPositionPopup lastItem] setTag: 9];
    [_addTextPositionPopup addItemWithTitle: _NS("Bottom-Right")];
    [[_addTextPositionPopup lastItem] setTag: 10];
    [_addLogoCheckbox setTitle:_NS("Add logo")];
    [_addLogoLogoLabel setStringValue:_NS("Logo")];
    [_addLogoPositionLabel setStringValue:_NS("Position")];
    [_addLogoPositionPopup removeAllItems];
    [_addLogoPositionPopup addItemWithTitle: _NS("Center")];
    [[_addLogoPositionPopup lastItem] setTag: 0];
    [_addLogoPositionPopup addItemWithTitle: _NS("Left")];
    [[_addLogoPositionPopup lastItem] setTag: 1];
    [_addLogoPositionPopup addItemWithTitle: _NS("Right")];
    [[_addLogoPositionPopup lastItem] setTag: 2];
    [_addLogoPositionPopup addItemWithTitle: _NS("Top")];
    [[_addLogoPositionPopup lastItem] setTag: 4];
    [_addLogoPositionPopup addItemWithTitle: _NS("Bottom")];
    [[_addLogoPositionPopup lastItem] setTag: 8];
    [_addLogoPositionPopup addItemWithTitle: _NS("Top-Left")];
    [[_addLogoPositionPopup lastItem] setTag: 5];
    [_addLogoPositionPopup addItemWithTitle: _NS("Top-Right")];
    [[_addLogoPositionPopup lastItem] setTag: 6];
    [_addLogoPositionPopup addItemWithTitle: _NS("Bottom-Left")];
    [[_addLogoPositionPopup lastItem] setTag: 9];
    [_addLogoPositionPopup addItemWithTitle: _NS("Bottom-Right")];
    [[_addLogoPositionPopup lastItem] setTag: 10];
    [_addLogoTransparencyLabel setStringValue:_NS("Transparency")];

    [_tabView selectFirstTabViewItem:self];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(inputChangedEvent:)
                                                 name:VLCInputChangedNotification
                                               object:nil];


    [self resetValues];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)updateCocoaWindowLevel:(NSInteger)i_level
{
    if (self.isWindowLoaded && [self.window isVisible] && [self.window level] != i_level)
        [self.window setLevel: i_level];
}

#pragma mark -
#pragma mark internal functions

-(void)inputChangedEvent:(NSNotification *)o_notification
{
    // reset crop values when input changed
    [self setCropBottomValue:0];
    [self setCropTopValue:0];
    [self setCropLeftValue:0];
    [self setCropRightValue:0];
}

- (void)resetProfileSelector
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [_profilePopup removeAllItems];

    NSArray *profileNames = [defaults objectForKey:@"VideoEffectProfileNames"];
    [_profilePopup addItemsWithTitles:profileNames];

    [[_profilePopup menu] addItem:[NSMenuItem separatorItem]];
    [_profilePopup addItemWithTitle:_NS("Duplicate current profile...")];
    [[_profilePopup lastItem] setTarget: self];
    [[_profilePopup lastItem] setAction: @selector(addProfile:)];

    if ([profileNames count] > 1) {
        [_profilePopup addItemWithTitle:_NS("Organize profiles...")];
        [[_profilePopup lastItem] setTarget: self];
        [[_profilePopup lastItem] setAction: @selector(removeProfile:)];
    }

    [_profilePopup selectItemAtIndex:[defaults integerForKey:@"VideoEffectSelectedProfile"]];
    [self profileSelectorAction:self];
}

- (void)resetValues
{
    intf_thread_t *p_intf = getIntf();
    NSString *tmpString;
    char *tmpChar;
    BOOL b_state;

    /* do we have any filter enabled? if yes, show it. */
    char * psz_vfilters;
    psz_vfilters = config_GetPsz(p_intf, "video-filter");
    if (psz_vfilters) {
        [_adjustCheckbox setState: (NSInteger)strstr(psz_vfilters, "adjust")];
        [_sharpenCheckbox setState: (NSInteger)strstr(psz_vfilters, "sharpen")];
        [_bandingCheckbox setState: (NSInteger)strstr(psz_vfilters, "gradfun")];
        [_grainCheckbox setState: (NSInteger)strstr(psz_vfilters, "grain")];
        [_transformCheckbox setState: (NSInteger)strstr(psz_vfilters, "transform")];
        [_zoomCheckbox setState: (NSInteger)strstr(psz_vfilters, "magnify")];
        [_puzzleCheckbox setState: (NSInteger)strstr(psz_vfilters, "puzzle")];
        [_thresholdCheckbox setState: (NSInteger)strstr(psz_vfilters, "colorthres")];
        [_sepiaCheckbox setState: (NSInteger)strstr(psz_vfilters, "sepia")];
        [_noiseCheckbox setState: (NSInteger)strstr(psz_vfilters, "noise")];
        [_gradientCheckbox setState: (NSInteger)strstr(psz_vfilters, "gradient")];
        [_extractCheckbox setState: (NSInteger)strstr(psz_vfilters, "extract")];
        [_invertCheckbox setState: (NSInteger)strstr(psz_vfilters, "invert")];
        [_posterizeCheckbox setState: (NSInteger)strstr(psz_vfilters, "posterize")];
        [_blurCheckbox setState: (NSInteger)strstr(psz_vfilters, "motionblur")];
        [_motiondetectCheckbox setState: (NSInteger)strstr(psz_vfilters, "motiondetect")];
        [_watereffectCheckbox setState: (NSInteger)strstr(psz_vfilters, "ripple")];
        [_wavesCheckbox setState: (NSInteger)strstr(psz_vfilters, "wave")];
        [_psychedelicCheckbox setState: (NSInteger)strstr(psz_vfilters, "psychedelic")];
        [_anaglyphCheckbox setState: (NSInteger)strstr(psz_vfilters, "anaglyph")];
        free(psz_vfilters);
    } else {
        [_adjustCheckbox setState: NSOffState];
        [_sharpenCheckbox setState: NSOffState];
        [_bandingCheckbox setState: NSOffState];
        [_grainCheckbox setState: NSOffState];
        [_transformCheckbox setState: NSOffState];
        [_zoomCheckbox setState: NSOffState];
        [_puzzleCheckbox setState: NSOffState];
        [_thresholdCheckbox setState: NSOffState];
        [_sepiaCheckbox setState: NSOffState];
        [_noiseCheckbox setState: NSOffState];
        [_gradientCheckbox setState: NSOffState];
        [_extractCheckbox setState: NSOffState];
        [_invertCheckbox setState: NSOffState];
        [_posterizeCheckbox setState: NSOffState];
        [_blurCheckbox setState: NSOffState];
        [_motiondetectCheckbox setState: NSOffState];
        [_watereffectCheckbox setState: NSOffState];
        [_wavesCheckbox setState: NSOffState];
        [_psychedelicCheckbox setState: NSOffState];
        [_anaglyphCheckbox setState: NSOffState];
    }

    psz_vfilters = config_GetPsz(p_intf, "sub-source");
    if (psz_vfilters) {
        [_addTextCheckbox setState: (NSInteger)strstr(psz_vfilters, "marq")];
        [_addLogoCheckbox setState: (NSInteger)strstr(psz_vfilters, "logo")];
        free(psz_vfilters);
    } else {
        [_addTextCheckbox setState: NSOffState];
        [_addLogoCheckbox setState: NSOffState];
    }

    psz_vfilters = config_GetPsz(p_intf, "video-splitter");
    if (psz_vfilters) {
        [_cloneCheckbox setState: (NSInteger)strstr(psz_vfilters, "clone")];
        [_wallCheckbox setState: (NSInteger)strstr(psz_vfilters, "wall")];
        free(psz_vfilters);
    } else {
        [_cloneCheckbox setState: NSOffState];
        [_wallCheckbox setState: NSOffState];
    }

    /* fetch and show the various values */
    [_adjustHueSlider setFloatValue: config_GetFloat(p_intf, "hue")];
    [_adjustContrastSlider setFloatValue: config_GetFloat(p_intf, "contrast")];
    [_adjustBrightnessSlider setFloatValue: config_GetFloat(p_intf, "brightness")];
    [_adjustSaturationSlider setFloatValue: config_GetFloat(p_intf, "saturation")];
    [_adjustBrightnessCheckbox setState:(config_GetInt(p_intf, "brightness-threshold") != 0 ? NSOnState : NSOffState)];
    [_adjustGammaSlider setFloatValue: config_GetFloat(p_intf, "gamma")];
    [_adjustBrightnessSlider setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "brightness")]];
    [_adjustContrastSlider setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "contrast")]];
    [_adjustGammaSlider setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "gamma")]];
    [_adjustHueSlider setToolTip: [NSString stringWithFormat:@"%.0f", config_GetFloat(p_intf, "hue")]];
    [_adjustSaturationSlider setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "saturation")]];
    b_state = [_adjustCheckbox state];

    [_adjustBrightnessSlider setEnabled: b_state];
    [_adjustBrightnessCheckbox setEnabled: b_state];
    [_adjustContrastSlider setEnabled: b_state];
    [_adjustGammaSlider setEnabled: b_state];
    [_adjustHueSlider setEnabled: b_state];
    [_adjustSaturationSlider setEnabled: b_state];
    [_adjustBrightnessLabel setEnabled: b_state];
    [_adjustContrastLabel setEnabled: b_state];
    [_adjustGammaLabel setEnabled: b_state];
    [_adjustHueLabel setEnabled: b_state];
    [_adjustSaturationLabel setEnabled: b_state];
    [_adjustResetButton setEnabled: b_state];

    [_sharpenSlider setFloatValue: config_GetFloat(p_intf, "sharpen-sigma")];
    [_sharpenSlider setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "sharpen-sigma")]];
    [_sharpenSlider setEnabled: [_sharpenCheckbox state]];
    [_sharpenLabel setEnabled: [_sharpenCheckbox state]];

    [_bandingSlider setIntValue: config_GetInt(p_intf, "gradfun-radius")];
    [_bandingSlider setToolTip: [NSString stringWithFormat:@"%lli", config_GetInt(p_intf, "gradfun-radius")]];
    [_bandingSlider setEnabled: [_bandingCheckbox state]];
    [_bandingLabel setEnabled: [_bandingCheckbox state]];

    [_grainSlider setFloatValue: config_GetFloat(p_intf, "grain-variance")];
    [_grainSlider setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "grain-variance")]];
    [_grainSlider setEnabled: [_grainCheckbox state]];
    [_grainLabel setEnabled: [_grainCheckbox state]];

    [self setCropLeftValue: 0];
    [self setCropTopValue: 0];
    [self setCropRightValue: 0];
    [self setCropBottomValue: 0];
    [_cropSyncTopBottomCheckbox setState: NSOffState];
    [_cropSyncLeftRightCheckbox setState: NSOffState];

    tmpChar = config_GetPsz(p_intf, "transform-type");
    tmpString = toNSStr(tmpChar);
    if ([tmpString isEqualToString:@"hflip"])
        [_transformPopup selectItemWithTag: 1];
    else if ([tmpString isEqualToString:@"vflip"])
        [_transformPopup selectItemWithTag: 2];
    else
        [_transformPopup selectItemWithTag:[tmpString intValue]];
    FREENULL(tmpChar);
    [_transformPopup setEnabled: [_transformCheckbox state]];

    [self setPuzzleColumnsValue: config_GetInt(p_intf, "puzzle-cols")];
    [self setPuzzleRowsValue: config_GetInt(p_intf, "puzzle-rows")];
    b_state = [_puzzleCheckbox state];
    [_puzzleRowsTextField setEnabled: b_state];
    [_puzzleRowsStepper setEnabled: b_state];
    [_puzzleRowsLabel setEnabled: b_state];
    [_puzzleColumnsTextField setEnabled: b_state];
    [_puzzleColumnsStepper setEnabled: b_state];
    [_puzzleColumnsLabel setEnabled: b_state];

    [self setCloneValue: config_GetInt(p_intf, "clone-count")];
    b_state = [_cloneCheckbox state];
    [_cloneNumberLabel setEnabled: b_state];
    [_cloneNumberTextField setEnabled: b_state];
    [_cloneNumberStepper setEnabled: b_state];

    b_state = [_wallCheckbox state];
    [self setWallRowsValue: config_GetInt(p_intf, "wall-rows")];
    [_wallNumbersOfRowsLabel setEnabled: b_state];
    [_wallNumbersOfRowsTextField setEnabled: b_state];
    [_wallNumbersOfRowsStepper setEnabled: b_state];
    [self setWallColumnsValue: config_GetInt(p_intf, "wall-cols")];
    [_wallNumberOfColumnsLabel setEnabled: b_state];
    [_wallNumberOfColumnsTextField setEnabled: b_state];
    [_wallNumberOfColumnsStepper setEnabled: b_state];

    [_thresholdColorTextField setStringValue: [[NSString stringWithFormat:@"%llx", config_GetInt(p_intf, "colorthres-color")] uppercaseString]];
    [_thresholdSaturationSlider setIntValue: config_GetInt(p_intf, "colorthres-saturationthres")];
    [_thresholdSaturationSlider setToolTip: [NSString stringWithFormat:@"%lli", config_GetInt(p_intf, "colorthres-saturationthres")]];
    [_thresholdSimilaritySlider setIntValue: config_GetInt(p_intf, "colorthres-similaritythres")];
    [_thresholdSimilaritySlider setToolTip: [NSString stringWithFormat:@"%lli", config_GetInt(p_intf, "colorthres-similaritythres")]];

    b_state = [_thresholdCheckbox state];
    [_thresholdColorTextField setEnabled: b_state];
    [_thresholdColorLabel setEnabled: b_state];
    [_thresholdSaturationSlider setEnabled: b_state];
    [_thresholdSaturationLabel setEnabled: b_state];
    [_thresholdSimilaritySlider setEnabled: b_state];
    [_thresholdSimilarityLabel setEnabled: b_state];

    [self setSepiaValue: config_GetInt(p_intf, "sepia-intensity")];
    b_state = [_sepiaCheckbox state];
    [_sepiaTextField setEnabled: b_state];
    [_sepiaStepper setEnabled: b_state];
    [_sepiaLabel setEnabled: b_state];

    tmpChar = config_GetPsz(p_intf, "gradient-mode");
    tmpString = toNSStr(tmpChar);
    if ([tmpString isEqualToString:@"hough"])
        [_gradientModePopup selectItemWithTag: 3];
    else if ([tmpString isEqualToString:@"edge"])
        [_gradientModePopup selectItemWithTag: 2];
    else
        [_gradientModePopup selectItemWithTag: 1];
    FREENULL(tmpChar);
    [_gradientCartoonCheckbox setState: config_GetInt(p_intf, "gradient-cartoon")];
    [_gradientColorCheckbox setState: config_GetInt(p_intf, "gradient-type")];
    b_state = [_gradientCheckbox state];
    [_gradientModePopup setEnabled: b_state];
    [_gradientModeLabel setEnabled: b_state];
    [_gradientCartoonCheckbox setEnabled: b_state];
    [_gradientColorCheckbox setEnabled: b_state];

    [_extractTextField setStringValue: [[NSString stringWithFormat:@"%llx", config_GetInt(p_intf, "extract-component")] uppercaseString]];
    [_extractTextField setEnabled: [_extractCheckbox state]];
    [_extractLabel setEnabled: [_extractCheckbox state]];

    [self setPosterizeValue: config_GetInt(p_intf, "posterize-level")];
    b_state = [_posterizeCheckbox state];
    [_posterizeTextField setEnabled: b_state];
    [_posterizeStepper setEnabled: b_state];
    [_posterizeLabel setEnabled: b_state];

    [_blurSlider setIntValue: config_GetInt(p_intf, "blur-factor")];
    [_blurSlider setToolTip: [NSString stringWithFormat:@"%lli", config_GetInt(p_intf, "blur-factor")]];
    [_blurSlider setEnabled: [_blurCheckbox state]];
    [_blurLabel setEnabled: [_blurCheckbox state]];

    tmpChar = config_GetPsz(p_intf, "marq-marquee");
    [_addTextTextTextField setStringValue:toNSStr(tmpChar)];
    if (tmpChar)
        FREENULL(tmpChar);
    [_addTextPositionPopup selectItemWithTag: config_GetInt(p_intf, "marq-position")];
    b_state = [_addTextCheckbox state];
    [_addTextPositionPopup setEnabled: b_state];
    [_addTextPositionLabel setEnabled: b_state];
    [_addTextTextLabel setEnabled: b_state];
    [_addTextTextTextField setEnabled: b_state];

    tmpChar = config_GetPsz(p_intf, "logo-file");
    [_addLogoLogoTextField setStringValue: toNSStr(tmpChar)];
    if (tmpChar)
        FREENULL(tmpChar);
    [_addLogoPositionPopup selectItemWithTag: config_GetInt(p_intf, "logo-position")];
    [_addLogoTransparencySlider setIntValue: config_GetInt(p_intf, "logo-opacity")];
    [_addLogoTransparencySlider setToolTip: [NSString stringWithFormat:@"%lli", config_GetInt(p_intf, "logo-opacity")]];
    b_state = [_addLogoCheckbox state];
    [_addLogoPositionPopup setEnabled: b_state];
    [_addLogoPositionLabel setEnabled: b_state];
    [_addLogoLogoTextField setEnabled: b_state];
    [_addLogoLogoLabel setEnabled: b_state];
    [_addLogoTransparencySlider setEnabled: b_state];
    [_addLogoTransparencyLabel setEnabled: b_state];
}

- (NSString *)generateProfileString
{
    intf_thread_t *p_intf = getIntf();
    return [NSString stringWithFormat:@"%@;%@;%@;%lli;%f;%f;%f;%f;%f;%lli;%f;%@;%lli;%lli;%lli;%lli;%lli;%lli;%@;%lli;%lli;%lli;%lli;%lli;%@;%lli;%@;%lli;%lli;%lli;%lli;%lli;%lli;%f",
            B64EncAndFree(config_GetPsz(p_intf, "video-filter")),
            B64EncAndFree(config_GetPsz(p_intf, "sub-source")),
            B64EncAndFree(config_GetPsz(p_intf, "video-splitter")),
            0LL, // former "hue" value, deprecated since 3.0.0
            config_GetFloat(p_intf, "contrast"),
            config_GetFloat(p_intf, "brightness"),
            config_GetFloat(p_intf, "saturation"),
            config_GetFloat(p_intf, "gamma"),
            config_GetFloat(p_intf, "sharpen-sigma"),
            config_GetInt(p_intf, "gradfun-radius"),
            config_GetFloat(p_intf, "grain-variance"),
            B64EncAndFree(config_GetPsz(p_intf, "transform-type")),
            config_GetInt(p_intf, "puzzle-rows"),
            config_GetInt(p_intf, "puzzle-cols"),
            config_GetInt(p_intf, "colorthres-color"),
            config_GetInt(p_intf, "colorthres-saturationthres"),
            config_GetInt(p_intf, "colorthres-similaritythres"),
            config_GetInt(p_intf, "sepia-intensity"),
            B64EncAndFree(config_GetPsz(p_intf, "gradient-mode")),
            config_GetInt(p_intf, "gradient-cartoon"),
            config_GetInt(p_intf, "gradient-type"),
            config_GetInt(p_intf, "extract-component"),
            config_GetInt(p_intf, "posterize-level"),
            config_GetInt(p_intf, "blur-factor"),
            B64EncAndFree(config_GetPsz(p_intf, "marq-marquee")),
            config_GetInt(p_intf, "marq-position"),
            B64EncAndFree(config_GetPsz(p_intf, "logo-file")),
            config_GetInt(p_intf, "logo-position"),
            config_GetInt(p_intf, "logo-opacity"),
            config_GetInt(p_intf, "clone-count"),
            config_GetInt(p_intf, "wall-rows"),
            config_GetInt(p_intf, "wall-cols"),
            // version 2 of profile string:
            config_GetInt(p_intf, "brightness-threshold"), // index: 32
            // version 3 of profile string: (vlc-3.0.0)
            config_GetFloat(p_intf, "hue") // index: 33
            ];
}

#pragma mark -
#pragma mark generic UI code

- (void)saveCurrentProfile
{
    if (i_old_profile_index == -1)
        return;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    /* fetch all the current settings in a uniform string */
    NSString *newProfile = [self generateProfileString];

    NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"VideoEffectProfiles"]];
    if (i_old_profile_index >= [workArray count])
        return;

    [workArray replaceObjectAtIndex:i_old_profile_index withObject:newProfile];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"VideoEffectProfiles"];
    [defaults synchronize];
}

- (IBAction)toggleWindow:(id)sender
{
    if ([self.window isKeyWindow])
        [self.window orderOut:sender];
    else {
        [self.window setLevel: [[[VLCMain sharedInstance] voutController] currentStatusWindowLevel]];
        [self.window makeKeyAndOrderFront:sender];
    }
}

- (IBAction)profileSelectorAction:(id)sender
{
    intf_thread_t *p_intf = getIntf();
    [self saveCurrentProfile];
    i_old_profile_index = [_profilePopup indexOfSelectedItem];
    VLCCoreInteraction *vci_si = [VLCCoreInteraction sharedInstance];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSUInteger selectedProfile = [_profilePopup indexOfSelectedItem];

    /* fetch preset */
    NSArray *items = [[[defaults objectForKey:@"VideoEffectProfiles"] objectAtIndex:selectedProfile] componentsSeparatedByString:@";"];

    // version 1 of profile string has 32 entries
    if ([items count] < 32) {
        msg_Err(p_intf, "Error in parsing profile string");
        [self resetValues];
        return;
    }

    /* filter handling */
    NSString *tempString = B64DecNSStr([items firstObject]);
    vout_thread_t *p_vout = getVout();

    /* enable the new filters */
    config_PutPsz(p_intf, "video-filter", [tempString UTF8String]);
    if (p_vout) {
        var_SetString(p_vout, "video-filter", [tempString UTF8String]);
    }

    tempString = B64DecNSStr([items objectAtIndex:1]);
    /* enable another round of new filters */
    config_PutPsz(p_intf, "sub-source", [tempString UTF8String]);
    if (p_vout) {
        var_SetString(p_vout, "sub-source", [tempString UTF8String]);
    }

    if (p_vout) {
        vlc_object_release(p_vout);
    }

    tempString = B64DecNSStr([items objectAtIndex:2]);
    /* enable another round of new filters */
    char *psz_current_splitter = var_GetString(pl_Get(p_intf), "video-splitter");
    bool b_filter_changed = ![tempString isEqualToString:toNSStr(psz_current_splitter)];
    free(psz_current_splitter);

    if (b_filter_changed) {
        config_PutPsz(p_intf, "video-splitter", [tempString UTF8String]);
        var_SetString(pl_Get(p_intf), "video-splitter", [tempString UTF8String]);
    }

    /* try to set filter values on-the-fly and store them appropriately */
    // index 3 is deprecated
    [vci_si setVideoFilterProperty:"contrast" forFilter:"adjust" float:[[items objectAtIndex:4] floatValue]];
    [vci_si setVideoFilterProperty:"brightness" forFilter:"adjust" float:[[items objectAtIndex:5] floatValue]];
    [vci_si setVideoFilterProperty:"saturation" forFilter:"adjust" float:[[items objectAtIndex:6] floatValue]];
    [vci_si setVideoFilterProperty:"gamma" forFilter:"adjust" float:[[items objectAtIndex:7] floatValue]];
    [vci_si setVideoFilterProperty:"sharpen-sigma" forFilter:"sharpen" float:[[items objectAtIndex:8] floatValue]];
    [vci_si setVideoFilterProperty:"gradfun-radius" forFilter:"gradfun" integer:[[items objectAtIndex:9] intValue]];
    [vci_si setVideoFilterProperty:"grain-variance" forFilter:"grain" float:[[items objectAtIndex:10] floatValue]];
    [vci_si setVideoFilterProperty:"transform-type" forFilter:"transform" string:[B64DecNSStr([items objectAtIndex:11]) UTF8String]];
    [vci_si setVideoFilterProperty:"puzzle-rows" forFilter:"puzzle" integer:[[items objectAtIndex:12] intValue]];
    [vci_si setVideoFilterProperty:"puzzle-cols" forFilter:"puzzle" integer:[[items objectAtIndex:13] intValue]];
    [vci_si setVideoFilterProperty:"colorthres-color" forFilter:"colorthres" integer:[[items objectAtIndex:14] intValue]];
    [vci_si setVideoFilterProperty:"colorthres-saturationthres" forFilter:"colorthres" integer:[[items objectAtIndex:15] intValue]];
    [vci_si setVideoFilterProperty:"colorthres-similaritythres" forFilter:"colorthres" integer:[[items objectAtIndex:16] intValue]];
    [vci_si setVideoFilterProperty:"sepia-intensity" forFilter:"sepia" integer:[[items objectAtIndex:17] intValue]];
    [vci_si setVideoFilterProperty:"gradient-mode" forFilter:"gradient" string:[B64DecNSStr([items objectAtIndex:18]) UTF8String]];
    [vci_si setVideoFilterProperty:"gradient-cartoon" forFilter:"gradient" boolean:[[items objectAtIndex:19] intValue]];
    [vci_si setVideoFilterProperty:"gradient-type" forFilter:"gradient" integer:[[items objectAtIndex:20] intValue]];
    [vci_si setVideoFilterProperty:"extract-component" forFilter:"extract" integer:[[items objectAtIndex:21] intValue]];
    [vci_si setVideoFilterProperty:"posterize-level" forFilter:"posterize" integer:[[items objectAtIndex:22] intValue]];
    [vci_si setVideoFilterProperty:"blur-factor" forFilter:"motionblur" integer:[[items objectAtIndex:23] intValue]];
    [vci_si setVideoFilterProperty:"marq-marquee" forFilter:"marq" string:[B64DecNSStr([items objectAtIndex:24]) UTF8String]];
    [vci_si setVideoFilterProperty:"marq-position" forFilter:"marq" integer:[[items objectAtIndex:25] intValue]];
    [vci_si setVideoFilterProperty:"logo-file" forFilter:"logo" string:[B64DecNSStr([items objectAtIndex:26]) UTF8String]];
    [vci_si setVideoFilterProperty:"logo-position" forFilter:"logo" integer:[[items objectAtIndex:27] intValue]];
    [vci_si setVideoFilterProperty:"logo-opacity" forFilter:"logo" integer:[[items objectAtIndex:28] intValue]];
    [vci_si setVideoFilterProperty:"clone-count" forFilter:"clone" integer:[[items objectAtIndex:29] intValue]];
    [vci_si setVideoFilterProperty:"wall-rows" forFilter:"wall" integer:[[items objectAtIndex:30] intValue]];
    [vci_si setVideoFilterProperty:"wall-cols" forFilter:"wall" integer:[[items objectAtIndex:31] intValue]];

    if ([items count] >= 33) { // version >=2 of profile string
        [vci_si setVideoFilterProperty: "brightness-threshold" forFilter: "adjust" boolean: [[items objectAtIndex:32] intValue]];
    }

    float hueValue;
    if ([items count] >= 34) { // version >=3 of profile string
        hueValue = [[items objectAtIndex:33] floatValue];
    } else {
        hueValue = [[items objectAtIndex:3] intValue]; // deprecated since 3.0.0
        // convert to new scale ([0,360] --> [-180,180])
        hueValue -= 180;
    }
    [vci_si setVideoFilterProperty:"hue" forFilter:"adjust" float:hueValue];

    [defaults setInteger:selectedProfile forKey:@"VideoEffectSelectedProfile"];
    [defaults synchronize];

    [self resetValues];
}

- (void)addProfile:(id)sender
{
    /* show panel */
    [_textfieldPanel setTitleString:_NS("Duplicate current profile for a new profile")];
    [_textfieldPanel setSubTitleString:_NS("Enter a name for the new profile:")];
    [_textfieldPanel setCancelButtonString:_NS("Cancel")];
    [_textfieldPanel setOkButtonString:_NS("Save")];

    __weak typeof(self) _self = self;
    [_textfieldPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSString *resultingText) {

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

        if (returnCode != NSOKButton) {
            [_profilePopup selectItemAtIndex:[defaults integerForKey:@"VideoEffectSelectedProfile"]];
            return;
        }

        NSArray *profileNames = [defaults objectForKey:@"VideoEffectProfileNames"];

        // duplicate names are not allowed in the popup control
        if ([resultingText length] == 0 || [profileNames containsObject:resultingText]) {
            [_profilePopup selectItemAtIndex:[defaults integerForKey:@"VideoEffectSelectedProfile"]];

            NSAlert *alert = [[NSAlert alloc] init];
            [alert setAlertStyle:NSCriticalAlertStyle];
            [alert setMessageText:_NS("Please enter a unique name for the new profile.")];
            [alert setInformativeText:_NS("Multiple profiles with the same name are not allowed.")];

            [alert beginSheetModalForWindow:_self.window
                              modalDelegate:nil
                             didEndSelector:nil
                                contextInfo:nil];
            return;
        }

        /* fetch all the current settings in a uniform string */
        NSString *newProfile = [_self generateProfileString];

        /* add string to user defaults as well as a label */

        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"VideoEffectProfiles"]];
        [workArray addObject:newProfile];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"VideoEffectProfiles"];
        [defaults setInteger:[workArray count] - 1 forKey:@"VideoEffectSelectedProfile"];

        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"VideoEffectProfileNames"]];
        [workArray addObject:resultingText];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"VideoEffectProfileNames"];

        /* save defaults */
        [defaults synchronize];

        /* refresh UI */
        [_self resetProfileSelector];
    }];
}

- (void)removeProfile:(id)sender
{
    /* show panel */
    [_popupPanel setTitleString:_NS("Remove a preset")];
    [_popupPanel setSubTitleString:_NS("Select the preset you would like to remove:")];
    [_popupPanel setOkButtonString:_NS("Remove")];
    [_popupPanel setCancelButtonString:_NS("Cancel")];
    [_popupPanel setPopupButtonContent:[[NSUserDefaults standardUserDefaults] objectForKey:@"VideoEffectProfileNames"]];

    __weak typeof(self) _self = self;
    [_popupPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSInteger selectedIndex) {

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

        if (returnCode != NSOKButton) {
            [_profilePopup selectItemAtIndex:[defaults integerForKey:@"VideoEffectSelectedProfile"]];
            return;
        }

        /* remove selected profile from settings */
        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray: [defaults objectForKey:@"VideoEffectProfiles"]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"VideoEffectProfiles"];

        workArray = [[NSMutableArray alloc] initWithArray: [defaults objectForKey:@"VideoEffectProfileNames"]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"VideoEffectProfileNames"];

        if (i_old_profile_index >= selectedIndex)
            [defaults setInteger:i_old_profile_index - 1 forKey:@"VideoEffectSelectedProfile"];

        /* save defaults */
        [defaults synchronize];

        /* do not save deleted profile */
        i_old_profile_index = -1;
        /* refresh UI */
        [_self resetProfileSelector];
    }];
}

#pragma mark -
#pragma mark basic
- (IBAction)enableAdjust:(id)sender
{
    BOOL b_state = [_adjustCheckbox state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "adjust" on: b_state];
    [_adjustBrightnessSlider setEnabled: b_state];
    [_adjustBrightnessCheckbox setEnabled: b_state];
    [_adjustBrightnessLabel setEnabled: b_state];
    [_adjustContrastSlider setEnabled: b_state];
    [_adjustContrastLabel setEnabled: b_state];
    [_adjustGammaSlider setEnabled: b_state];
    [_adjustGammaLabel setEnabled: b_state];
    [_adjustHueSlider setEnabled: b_state];
    [_adjustHueLabel setEnabled: b_state];
    [_adjustSaturationSlider setEnabled: b_state];
    [_adjustSaturationLabel setEnabled: b_state];
    [_adjustResetButton setEnabled: b_state];
}

- (IBAction)adjustSliderChanged:(id)sender
{
    if (sender == _adjustBrightnessSlider)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "brightness" forFilter: "adjust" float: [_adjustBrightnessSlider floatValue]];
    else if (sender == _adjustContrastSlider)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "contrast" forFilter: "adjust" float: [_adjustContrastSlider floatValue]];
    else if (sender == _adjustGammaSlider)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gamma" forFilter: "adjust" float: [_adjustGammaSlider floatValue]];
    else if (sender == _adjustHueSlider)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "hue" forFilter: "adjust" float: [_adjustHueSlider floatValue]];
    else if (sender == _adjustSaturationSlider)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "saturation" forFilter: "adjust" float: [_adjustSaturationSlider floatValue]];

    if (sender == _adjustHueSlider)
        [_adjustHueSlider setToolTip: [NSString stringWithFormat:@"%.0f", [_adjustHueSlider floatValue]]];
    else
        [sender setToolTip: [NSString stringWithFormat:@"%0.3f", [sender floatValue]]];
}

- (IBAction)enableAdjustBrightnessThreshold:(id)sender
{
    VLCCoreInteraction *vci_si = [VLCCoreInteraction sharedInstance];

    if (sender == _adjustResetButton) {
        [_adjustBrightnessSlider setFloatValue: 1.0];
        [_adjustContrastSlider setFloatValue: 1.0];
        [_adjustGammaSlider setFloatValue: 1.0];
        [_adjustHueSlider setFloatValue: 0];
        [_adjustSaturationSlider setFloatValue: 1.0];
        [_adjustBrightnessSlider setToolTip: [NSString stringWithFormat:@"%0.3f", 1.0]];
        [_adjustContrastSlider setToolTip: [NSString stringWithFormat:@"%0.3f", 1.0]];
        [_adjustGammaSlider setToolTip: [NSString stringWithFormat:@"%0.3f", 1.0]];
        [_adjustHueSlider setToolTip: [NSString stringWithFormat:@"%.0f", 0.0]];
        [_adjustSaturationSlider setToolTip: [NSString stringWithFormat:@"%0.3f", 1.0]];
        [vci_si setVideoFilterProperty: "brightness" forFilter: "adjust" float: 1.0];
        [vci_si setVideoFilterProperty: "contrast" forFilter: "adjust" float: 1.0];
        [vci_si setVideoFilterProperty: "gamma" forFilter: "adjust" float: 1.0];
        [vci_si setVideoFilterProperty: "hue" forFilter: "adjust" float: 0.0];
        [vci_si setVideoFilterProperty: "saturation" forFilter: "adjust" float: 1.0];
    } else
        [vci_si setVideoFilterProperty: "brightness-threshold" forFilter: "adjust" boolean: [_adjustBrightnessCheckbox state]];

}

- (IBAction)enableSharpen:(id)sender
{
    BOOL b_state = [_sharpenCheckbox state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "sharpen" on: b_state];
    [_sharpenSlider setEnabled: b_state];
    [_sharpenLabel setEnabled: b_state];
}

- (IBAction)sharpenSliderChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "sharpen-sigma" forFilter: "sharpen" float: [sender floatValue]];
    [sender setToolTip: [NSString stringWithFormat:@"%0.3f", [sender floatValue]]];
}

- (IBAction)enableBanding:(id)sender
{
    BOOL b_state = [_bandingCheckbox state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "gradfun" on: b_state];
    [_bandingSlider setEnabled: b_state];
    [_bandingLabel setEnabled: b_state];
}

- (IBAction)bandingSliderChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradfun-radius" forFilter: "gradfun" integer: [sender intValue]];
    [sender setToolTip: [NSString stringWithFormat:@"%i", [sender intValue]]];
}

- (IBAction)enableGrain:(id)sender
{
    BOOL b_state = [_grainCheckbox state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "grain" on: b_state];
    [_grainSlider setEnabled: b_state];
    [_grainLabel setEnabled: b_state];
}

- (IBAction)grainSliderChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "grain-variance" forFilter: "grain" float: [sender floatValue]];
    [sender setToolTip: [NSString stringWithFormat:@"%0.3f", [sender floatValue]]];
}


#pragma mark -
#pragma mark crop

- (IBAction)cropObjectChanged:(id)sender
{
    if ([_cropSyncTopBottomCheckbox state]) {
        if (sender == _cropBottomTextField || sender == _cropBottomStepper)
            [self setCropTopValue: [self cropBottomValue]];
        else
            [self setCropBottomValue: [self cropTopValue]];
    }
    if ([_cropSyncLeftRightCheckbox state]) {
        if (sender == _cropRightTextField || sender == _cropRightStepper)
            [self setCropLeftValue: [self cropRightValue]];
        else
            [self setCropRightValue: [self cropLeftValue]];
    }

    vout_thread_t *p_vout = getVout();
    if (p_vout) {
        var_SetInteger(p_vout, "crop-top", [_cropTopTextField intValue]);
        var_SetInteger(p_vout, "crop-bottom", [_cropBottomTextField intValue]);
        var_SetInteger(p_vout, "crop-left", [_cropLeftTextField intValue]);
        var_SetInteger(p_vout, "crop-right", [_cropRightTextField intValue]);
        vlc_object_release(p_vout);
    }
}

#pragma mark -
#pragma mark geometry
- (IBAction)enableTransform:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "transform" on: [_transformCheckbox state]];
    [_transformPopup setEnabled: [_transformCheckbox state]];
}

- (IBAction)transformModifierChanged:(id)sender
{
    NSInteger tag = [[_transformPopup selectedItem] tag];
    const char *psz_string = [[NSString stringWithFormat:@"%li", tag] UTF8String];
    if (tag == 1)
        psz_string = "hflip";
    else if (tag == 2)
        psz_string = "vflip";

    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "transform-type" forFilter: "transform" string: psz_string];
}

- (IBAction)enableZoom:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "magnify" on: [_zoomCheckbox state]];
}

- (IBAction)enablePuzzle:(id)sender
{
    BOOL b_state = [_puzzleCheckbox state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "puzzle" on: b_state];
    [_puzzleColumnsTextField setEnabled: b_state];
    [_puzzleColumnsStepper setEnabled: b_state];
    [_puzzleColumnsLabel setEnabled: b_state];
    [_puzzleRowsTextField setEnabled: b_state];
    [_puzzleRowsStepper setEnabled: b_state];
    [_puzzleRowsLabel setEnabled: b_state];
}

- (IBAction)puzzleModifierChanged:(id)sender
{
    if (sender == _puzzleColumnsTextField || sender == _puzzleColumnsStepper)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "puzzle-cols" forFilter: "puzzle" integer: [sender intValue]];
    else
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "puzzle-rows" forFilter: "puzzle" integer: [sender intValue]];
}

- (IBAction)enableClone:(id)sender
{
    BOOL b_state = [_cloneCheckbox state];

    if (b_state && [_wallCheckbox state]) {
        [_wallCheckbox setState: NSOffState];
        [self enableWall:_wallCheckbox];
    }

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "clone" on: b_state];
    [_cloneNumberLabel setEnabled: b_state];
    [_cloneNumberTextField setEnabled: b_state];
    [_cloneNumberStepper setEnabled: b_state];
}

- (IBAction)cloneModifierChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "clone-count" forFilter: "clone" integer: [_cloneNumberTextField intValue]];
}

- (IBAction)enableWall:(id)sender
{
    BOOL b_state = [_wallCheckbox state];

    if (b_state && [_cloneCheckbox state]) {
        [_cloneCheckbox setState: NSOffState];
        [self enableClone:_cloneCheckbox];
    }

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "wall" on: b_state];
    [_wallNumberOfColumnsTextField setEnabled: b_state];
    [_wallNumberOfColumnsStepper setEnabled: b_state];
    [_wallNumberOfColumnsLabel setEnabled: b_state];

    [_wallNumbersOfRowsTextField setEnabled: b_state];
    [_wallNumbersOfRowsStepper setEnabled: b_state];
    [_wallNumbersOfRowsLabel setEnabled: b_state];
}

- (IBAction)wallModifierChanged:(id)sender
{
    if (sender == _wallNumberOfColumnsTextField || sender == _wallNumberOfColumnsStepper)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "wall-cols" forFilter: "wall" integer: [sender intValue]];
    else
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "wall-rows" forFilter: "wall" integer: [sender intValue]];
}

#pragma mark -
#pragma mark color
- (IBAction)enableThreshold:(id)sender
{
    BOOL b_state = [_thresholdCheckbox state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "colorthres" on: b_state];
    [_thresholdColorTextField setEnabled: b_state];
    [_thresholdColorLabel setEnabled: b_state];
    [_thresholdSaturationSlider setEnabled: b_state];
    [_thresholdSaturationLabel setEnabled: b_state];
    [_thresholdSimilaritySlider setEnabled: b_state];
    [_thresholdSimilarityLabel setEnabled: b_state];
}

- (IBAction)thresholdModifierChanged:(id)sender
{
    if (sender == _thresholdColorTextField)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "colorthres-color" forFilter: "colorthres" integer: [_thresholdColorTextField intValue]];
    else if (sender == _thresholdSaturationSlider) {
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "colorthres-saturationthres" forFilter: "colorthres" integer: [_thresholdSaturationSlider intValue]];
        [_thresholdSaturationSlider setToolTip: [NSString stringWithFormat:@"%i", [_thresholdSaturationSlider intValue]]];
    } else {
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "colorthres-similaritythres" forFilter: "colorthres" integer: [_thresholdSimilaritySlider intValue]];
        [_thresholdSimilaritySlider setToolTip: [NSString stringWithFormat:@"%i", [_thresholdSimilaritySlider intValue]]];
    }
}

- (IBAction)enableSepia:(id)sender
{
    BOOL b_state = [_sepiaCheckbox state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "sepia" on: b_state];
    [_sepiaTextField setEnabled: b_state];
    [_sepiaStepper setEnabled: b_state];
    [_sepiaLabel setEnabled: b_state];
}

- (IBAction)sepiaModifierChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "sepia-intensity" forFilter: "sepia" integer: [_sepiaTextField intValue]];
}

- (IBAction)enableNoise:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "noise" on: [_noiseCheckbox state]];
}

- (IBAction)enableGradient:(id)sender
{
    BOOL b_state = [_gradientCheckbox state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "gradient" on: b_state];
    [_gradientModePopup setEnabled: b_state];
    [_gradientModeLabel setEnabled: b_state];
    [_gradientColorCheckbox setEnabled: b_state];
    [_gradientCartoonCheckbox setEnabled: b_state];
}

- (IBAction)gradientModifierChanged:(id)sender
{
    if (sender == _gradientModePopup) {
        if ([[_gradientModePopup selectedItem] tag] == 3)
            [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradient-mode" forFilter: "gradient" string: "hough"];
        else if ([[_gradientModePopup selectedItem] tag] == 2)
            [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradient-mode" forFilter: "gradient" string: "edge"];
        else
            [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradient-mode" forFilter: "gradient" string: "gradient"];
    } else if (sender == _gradientColorCheckbox)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradient-type" forFilter: "gradient" integer: [_gradientColorCheckbox state]];
    else
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradient-cartoon" forFilter: "gradient" boolean: [_gradientCartoonCheckbox state]];
}

- (IBAction)enableExtract:(id)sender
{
    BOOL b_state = [_extractCheckbox state];
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "extract" on: b_state];
    [_extractTextField setEnabled: b_state];
    [_extractLabel setEnabled: b_state];
}

- (IBAction)extractModifierChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "extract-component" forFilter: "extract" integer: [_extractTextField intValue]];
}

- (IBAction)enableInvert:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "invert" on: [_invertCheckbox state]];
}

- (IBAction)enablePosterize:(id)sender
{
    BOOL b_state = [_posterizeCheckbox state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "posterize" on: b_state];
    [_posterizeTextField setEnabled: b_state];
    [_posterizeStepper setEnabled: b_state];
    [_posterizeLabel setEnabled: b_state];
}

- (IBAction)posterizeModifierChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "posterize-level" forFilter: "posterize" integer: [_posterizeTextField intValue]];
}

- (IBAction)enableBlur:(id)sender
{
    BOOL b_state = [_blurCheckbox state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "motionblur" on: b_state];
    [_blurSlider setEnabled: b_state];
    [_blurLabel setEnabled: b_state];
}

- (IBAction)blurModifierChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "blur-factor" forFilter: "motionblur" integer: [sender intValue]];
    [sender setToolTip: [NSString stringWithFormat:@"%i", [sender intValue]]];
}

- (IBAction)enableMotionDetect:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "motiondetect" on: [_motiondetectCheckbox state]];
}

- (IBAction)enableWaterEffect:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "ripple" on: [_watereffectCheckbox state]];
}

- (IBAction)enableWaves:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "wave" on: [_wavesCheckbox state]];
}

- (IBAction)enablePsychedelic:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "psychedelic" on: [_psychedelicCheckbox state]];
}

#pragma mark -
#pragma mark Miscellaneous
- (IBAction)enableAddText:(id)sender
{
    BOOL b_state = [_addTextCheckbox state];
    VLCCoreInteraction *vci_si = [VLCCoreInteraction sharedInstance];

    [_addTextPositionPopup setEnabled: b_state];
    [_addTextPositionLabel setEnabled: b_state];
    [_addTextTextLabel setEnabled: b_state];
    [_addTextTextTextField setEnabled: b_state];
    [vci_si setVideoFilter: "marq" on: b_state];
    [vci_si setVideoFilterProperty: "marq-marquee" forFilter: "marq" string: [[_addTextTextTextField stringValue] UTF8String]];
    [vci_si setVideoFilterProperty: "marq-position" forFilter: "marq" integer: [[_addTextPositionPopup selectedItem] tag]];
}

- (IBAction)addTextModifierChanged:(id)sender
{
    if (sender == _addTextTextTextField)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "marq-marquee" forFilter: "marq" string:[[_addTextTextTextField stringValue] UTF8String]];
    else
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "marq-position" forFilter: "marq" integer: [[_addTextPositionPopup selectedItem] tag]];
}

- (IBAction)enableAddLogo:(id)sender
{
    BOOL b_state = [_addLogoCheckbox state];

    [_addLogoPositionPopup setEnabled: b_state];
    [_addLogoPositionLabel setEnabled: b_state];
    [_addLogoLogoTextField setEnabled: b_state];
    [_addLogoLogoLabel setEnabled: b_state];
    [_addLogoTransparencySlider setEnabled: b_state];
    [_addLogoTransparencyLabel setEnabled: b_state];
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "logo" on: b_state];
}

- (IBAction)addLogoModifierChanged:(id)sender
{
    if (sender == _addLogoLogoTextField)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "logo-file" forFilter: "logo" string: [[_addLogoLogoTextField stringValue] UTF8String]];
    else if (sender == _addLogoPositionPopup)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "logo-position" forFilter: "logo" integer: [[_addLogoPositionPopup selectedItem] tag]];
    else {
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "logo-opacity" forFilter: "logo" integer: [_addLogoTransparencySlider intValue]];
        [_addLogoTransparencySlider setToolTip: [NSString stringWithFormat:@"%i", [_addLogoTransparencySlider intValue]]];
    }
}

- (IBAction)enableAnaglyph:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "anaglyph" on: [_anaglyphCheckbox state]];
}

@end
