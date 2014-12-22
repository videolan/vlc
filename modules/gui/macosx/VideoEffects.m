/*****************************************************************************
 * VideoEffects.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2012 Felix Paul Kühne
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
#import "VideoEffects.h"
#import "SharedDialogs.h"
#import "CoreInteraction.h"

@interface VLCVideoEffects (Internal)
- (void)resetProfileSelector;
@end

#pragma mark -
#pragma mark Initialization

@implementation VLCVideoEffects
static VLCVideoEffects *_o_sharedInstance = nil;

@synthesize cropLeftValue, cropTopValue, cropRightValue, cropBottomValue;
@synthesize puzzleRowsValue, puzzleColumnsValue;
@synthesize wallRowsValue, wallColumnsValue;
@synthesize cloneValue;
@synthesize sepiaValue;
@synthesize posterizeValue;

+ (VLCVideoEffects *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

+ (void)initialize
{
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:[NSArray arrayWithObject:@";;;0;1.000000;1.000000;1.000000;1.000000;0.050000;16;2.000000;OTA=;4;4;16711680;20;15;120;Z3JhZGllbnQ=;1;0;16711680;6;80;VkxD;-1;;-1;255;2;3;3"], @"VideoEffectProfiles",
                                 [NSArray arrayWithObject:_NS("Default")], @"VideoEffectProfileNames", nil];
    [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
}

- (id)init
{
    if (_o_sharedInstance)
        [self dealloc];
    else {
        p_intf = VLCIntf;
        i_old_profile_index = -1;
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)awakeFromNib
{
    [o_window setTitle: _NS("Video Effects")];
    [o_window setExcludedFromWindowsMenu:YES];
    if (!OSX_SNOW_LEOPARD)
        [o_window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"basic"]] setLabel:_NS("Basic")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"crop"]] setLabel:_NS("Crop")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"geometry"]] setLabel:_NS("Geometry")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"color"]] setLabel:_NS("Color")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"misc"]] setLabel:_NS("Miscellaneous")];

    [self resetProfileSelector];

    [o_adjust_ckb setTitle:_NS("Image Adjust")];
    [o_adjust_hue_lbl setStringValue:_NS("Hue")];
    [o_adjust_contrast_lbl setStringValue:_NS("Contrast")];
    [o_adjust_brightness_lbl setStringValue:_NS("Brightness")];
    [o_adjust_brightness_ckb setTitle:_NS("Brightness Threshold")];
    [o_adjust_saturation_lbl setStringValue:_NS("Saturation")];
    [o_adjust_gamma_lbl setStringValue:_NS("Gamma")];
    [o_adjust_reset_btn setTitle: _NS("Reset")];
    [o_sharpen_ckb setTitle:_NS("Sharpen")];
    [o_sharpen_lbl setStringValue:_NS("Sigma")];
    [o_banding_ckb setTitle:_NS("Banding removal")];
    [o_banding_lbl setStringValue:_NS("Radius")];
    [o_grain_ckb setTitle:_NS("Film Grain")];
    [o_grain_lbl setStringValue:_NS("Variance")];
    [o_crop_top_lbl setStringValue:_NS("Top")];
    [o_crop_left_lbl setStringValue:_NS("Left")];
    [o_crop_right_lbl setStringValue:_NS("Right")];
    [o_crop_bottom_lbl setStringValue:_NS("Bottom")];
    [o_crop_sync_top_bottom_ckb setTitle:_NS("Synchronize top and bottom")];
    [o_crop_sync_left_right_ckb setTitle:_NS("Synchronize left and right")];

    [o_transform_ckb setTitle:_NS("Transform")];
    [o_transform_pop removeAllItems];
    [o_transform_pop addItemWithTitle: _NS("Rotate by 90 degrees")];
    [[o_transform_pop lastItem] setTag: 90];
    [o_transform_pop addItemWithTitle: _NS("Rotate by 180 degrees")];
    [[o_transform_pop lastItem] setTag: 180];
    [o_transform_pop addItemWithTitle: _NS("Rotate by 270 degrees")];
    [[o_transform_pop lastItem] setTag: 270];
    [o_transform_pop addItemWithTitle: _NS("Flip horizontally")];
    [[o_transform_pop lastItem] setTag: 1];
    [o_transform_pop addItemWithTitle: _NS("Flip vertically")];
    [[o_transform_pop lastItem] setTag: 2];
    [o_zoom_ckb setTitle:_NS("Magnification/Zoom")];
    [o_puzzle_ckb setTitle:_NS("Puzzle game")];
    [o_puzzle_rows_lbl setStringValue:_NS("Rows")];
    [o_puzzle_columns_lbl setStringValue:_NS("Columns")];
    [o_clone_ckb setTitle:_NS("Clone")];
    [o_clone_number_lbl setStringValue:_NS("Number of clones")];
    [o_wall_ckb setTitle:_NS("Wall")];
    [o_wall_numofrows_lbl setStringValue:_NS("Rows")];
    [o_wall_numofcols_lbl setStringValue:_NS("Columns")];

    [o_threshold_ckb setTitle:_NS("Color threshold")];
    [o_threshold_color_lbl setStringValue:_NS("Color")];
    [o_threshold_saturation_lbl setStringValue:_NS("Saturation")];
    [o_threshold_similarity_lbl setStringValue:_NS("Similarity")];
    [o_sepia_ckb setTitle:_NS("Sepia")];
    [o_sepia_lbl setStringValue:_NS("Intensity")];
    [o_noise_ckb setTitle:_NS("Noise")];
    [o_gradient_ckb setTitle:_NS("Gradient")];
    [o_gradient_mode_lbl setStringValue:_NS("Mode")];
    [o_gradient_mode_pop removeAllItems];
    [o_gradient_mode_pop addItemWithTitle: _NS("Gradient")];
    [[o_gradient_mode_pop lastItem] setTag: 1];
    [o_gradient_mode_pop addItemWithTitle: _NS("Edge")];
    [[o_gradient_mode_pop lastItem] setTag: 2];
    [o_gradient_mode_pop addItemWithTitle: _NS("Hough")];
    [[o_gradient_mode_pop lastItem] setTag: 3];
    [o_gradient_color_ckb setTitle:_NS("Color")];
    [o_gradient_cartoon_ckb setTitle:_NS("Cartoon")];
    [o_extract_ckb setTitle:_NS("Color extraction")];
    [o_extract_lbl setStringValue:_NS("Color")];
    [o_invert_ckb setTitle:_NS("Invert colors")];
    [o_posterize_ckb setTitle:_NS("Posterize")];
    [o_posterize_lbl setStringValue:_NS("Posterize level")];
    [o_blur_ckb setTitle:_NS("Motion blur")];
    [o_blur_lbl setStringValue:_NS("Factor")];
    [o_motiondetect_ckb setTitle:_NS("Motion Detect")];
    [o_watereffect_ckb setTitle:_NS("Water effect")];
    [o_waves_ckb setTitle:_NS("Waves")];
    [o_psychedelic_ckb setTitle:_NS("Psychedelic")];
    [o_anaglyph_ckb setTitle:_NS("Anaglyph")];

    [o_addtext_ckb setTitle:_NS("Add text")];
    [o_addtext_text_lbl setStringValue:_NS("Text")];
    [o_addtext_pos_lbl setStringValue:_NS("Position")];
    [o_addtext_pos_pop removeAllItems];
    [o_addtext_pos_pop addItemWithTitle: _NS("Center")];
    [[o_addtext_pos_pop lastItem] setTag: 0];
    [o_addtext_pos_pop addItemWithTitle: _NS("Left")];
    [[o_addtext_pos_pop lastItem] setTag: 1];
    [o_addtext_pos_pop addItemWithTitle: _NS("Right")];
    [[o_addtext_pos_pop lastItem] setTag: 2];
    [o_addtext_pos_pop addItemWithTitle: _NS("Top")];
    [[o_addtext_pos_pop lastItem] setTag: 4];
    [o_addtext_pos_pop addItemWithTitle: _NS("Bottom")];
    [[o_addtext_pos_pop lastItem] setTag: 8];
    [o_addtext_pos_pop addItemWithTitle: _NS("Top-Left")];
    [[o_addtext_pos_pop lastItem] setTag: 5];
    [o_addtext_pos_pop addItemWithTitle: _NS("Top-Right")];
    [[o_addtext_pos_pop lastItem] setTag: 6];
    [o_addtext_pos_pop addItemWithTitle: _NS("Bottom-Left")];
    [[o_addtext_pos_pop lastItem] setTag: 9];
    [o_addtext_pos_pop addItemWithTitle: _NS("Bottom-Right")];
    [[o_addtext_pos_pop lastItem] setTag: 10];
    [o_addlogo_ckb setTitle:_NS("Add logo")];
    [o_addlogo_logo_lbl setStringValue:_NS("Logo")];
    [o_addlogo_pos_lbl setStringValue:_NS("Position")];
    [o_addlogo_pos_pop removeAllItems];
    [o_addlogo_pos_pop addItemWithTitle: _NS("Center")];
    [[o_addlogo_pos_pop lastItem] setTag: 0];
    [o_addlogo_pos_pop addItemWithTitle: _NS("Left")];
    [[o_addlogo_pos_pop lastItem] setTag: 1];
    [o_addlogo_pos_pop addItemWithTitle: _NS("Right")];
    [[o_addlogo_pos_pop lastItem] setTag: 2];
    [o_addlogo_pos_pop addItemWithTitle: _NS("Top")];
    [[o_addlogo_pos_pop lastItem] setTag: 4];
    [o_addlogo_pos_pop addItemWithTitle: _NS("Bottom")];
    [[o_addlogo_pos_pop lastItem] setTag: 8];
    [o_addlogo_pos_pop addItemWithTitle: _NS("Top-Left")];
    [[o_addlogo_pos_pop lastItem] setTag: 5];
    [o_addlogo_pos_pop addItemWithTitle: _NS("Top-Right")];
    [[o_addlogo_pos_pop lastItem] setTag: 6];
    [o_addlogo_pos_pop addItemWithTitle: _NS("Bottom-Left")];
    [[o_addlogo_pos_pop lastItem] setTag: 9];
    [o_addlogo_pos_pop addItemWithTitle: _NS("Bottom-Right")];
    [[o_addlogo_pos_pop lastItem] setTag: 10];
    [o_addlogo_transparency_lbl setStringValue:_NS("Transparency")];

    [o_tableView selectFirstTabViewItem:self];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(inputChangedEvent:)
                                                 name:VLCInputChangedNotification
                                               object:nil];


    [self resetValues];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [super dealloc];
}

- (void)updateCocoaWindowLevel:(NSInteger)i_level
{
    if (o_window && [o_window isVisible] && [o_window level] != i_level)
        [o_window setLevel: i_level];
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
    [o_profile_pop removeAllItems];

    NSArray * profileNames = [defaults objectForKey:@"VideoEffectProfileNames"];
    [o_profile_pop addItemsWithTitles:profileNames];

    [[o_profile_pop menu] addItem:[NSMenuItem separatorItem]];
    [o_profile_pop addItemWithTitle:_NS("Duplicate current profile...")];
    [[o_profile_pop lastItem] setTarget: self];
    [[o_profile_pop lastItem] setAction: @selector(addProfile:)];

    if ([profileNames count] > 1) {
        [o_profile_pop addItemWithTitle:_NS("Organize profiles...")];
        [[o_profile_pop lastItem] setTarget: self];
        [[o_profile_pop lastItem] setAction: @selector(removeProfile:)];
    }

    [o_profile_pop selectItemAtIndex:[defaults integerForKey:@"VideoEffectSelectedProfile"]];
    [self profileSelectorAction:self];
}

- (void)resetValues
{
    NSString *tmpString;
    char *tmpChar;
    BOOL b_state;

    /* do we have any filter enabled? if yes, show it. */
    char * psz_vfilters;
    psz_vfilters = config_GetPsz(p_intf, "video-filter");
    if (psz_vfilters) {
        [o_adjust_ckb setState: (NSInteger)strstr(psz_vfilters, "adjust")];
        [o_sharpen_ckb setState: (NSInteger)strstr(psz_vfilters, "sharpen")];
        [o_banding_ckb setState: (NSInteger)strstr(psz_vfilters, "gradfun")];
        [o_grain_ckb setState: (NSInteger)strstr(psz_vfilters, "grain")];
        [o_transform_ckb setState: (NSInteger)strstr(psz_vfilters, "transform")];
        [o_zoom_ckb setState: (NSInteger)strstr(psz_vfilters, "magnify")];
        [o_puzzle_ckb setState: (NSInteger)strstr(psz_vfilters, "puzzle")];
        [o_threshold_ckb setState: (NSInteger)strstr(psz_vfilters, "colorthres")];
        [o_sepia_ckb setState: (NSInteger)strstr(psz_vfilters, "sepia")];
        [o_noise_ckb setState: (NSInteger)strstr(psz_vfilters, "noise")];
        [o_gradient_ckb setState: (NSInteger)strstr(psz_vfilters, "gradient")];
        [o_extract_ckb setState: (NSInteger)strstr(psz_vfilters, "extract")];
        [o_invert_ckb setState: (NSInteger)strstr(psz_vfilters, "invert")];
        [o_posterize_ckb setState: (NSInteger)strstr(psz_vfilters, "posterize")];
        [o_blur_ckb setState: (NSInteger)strstr(psz_vfilters, "motionblur")];
        [o_motiondetect_ckb setState: (NSInteger)strstr(psz_vfilters, "motiondetect")];
        [o_watereffect_ckb setState: (NSInteger)strstr(psz_vfilters, "ripple")];
        [o_waves_ckb setState: (NSInteger)strstr(psz_vfilters, "wave")];
        [o_psychedelic_ckb setState: (NSInteger)strstr(psz_vfilters, "psychedelic")];
        [o_anaglyph_ckb setState: (NSInteger)strstr(psz_vfilters, "anaglyph")];
        free(psz_vfilters);
    } else {
        [o_adjust_ckb setState: NSOffState];
        [o_sharpen_ckb setState: NSOffState];
        [o_banding_ckb setState: NSOffState];
        [o_grain_ckb setState: NSOffState];
        [o_transform_ckb setState: NSOffState];
        [o_zoom_ckb setState: NSOffState];
        [o_puzzle_ckb setState: NSOffState];
        [o_threshold_ckb setState: NSOffState];
        [o_sepia_ckb setState: NSOffState];
        [o_noise_ckb setState: NSOffState];
        [o_gradient_ckb setState: NSOffState];
        [o_extract_ckb setState: NSOffState];
        [o_invert_ckb setState: NSOffState];
        [o_posterize_ckb setState: NSOffState];
        [o_blur_ckb setState: NSOffState];
        [o_motiondetect_ckb setState: NSOffState];
        [o_watereffect_ckb setState: NSOffState];
        [o_waves_ckb setState: NSOffState];
        [o_psychedelic_ckb setState: NSOffState];
        [o_anaglyph_ckb setState: NSOffState];
    }

    psz_vfilters = config_GetPsz(p_intf, "sub-source");
    if (psz_vfilters) {
        [o_addtext_ckb setState: (NSInteger)strstr(psz_vfilters, "marq")];
        [o_addlogo_ckb setState: (NSInteger)strstr(psz_vfilters, "logo")];
        free(psz_vfilters);
    } else {
        [o_addtext_ckb setState: NSOffState];
        [o_addlogo_ckb setState: NSOffState];
    }

    psz_vfilters = config_GetPsz(p_intf, "video-splitter");
    if (psz_vfilters) {
        [o_clone_ckb setState: (NSInteger)strstr(psz_vfilters, "clone")];
        [o_wall_ckb setState: (NSInteger)strstr(psz_vfilters, "wall")];
        free(psz_vfilters);
    } else {
        [o_clone_ckb setState: NSOffState];
        [o_wall_ckb setState: NSOffState];
    }

    /* fetch and show the various values */
    [o_adjust_hue_sld setFloatValue: config_GetFloat(p_intf, "hue")];
    [o_adjust_contrast_sld setFloatValue: config_GetFloat(p_intf, "contrast")];
    [o_adjust_brightness_sld setFloatValue: config_GetFloat(p_intf, "brightness")];
    [o_adjust_saturation_sld setFloatValue: config_GetFloat(p_intf, "saturation")];
    [o_adjust_brightness_ckb setState:(config_GetInt(p_intf, "brightness-threshold") != 0 ? NSOnState : NSOffState)];
    [o_adjust_gamma_sld setFloatValue: config_GetFloat(p_intf, "gamma")];
    [o_adjust_brightness_sld setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "brightness")]];
    [o_adjust_contrast_sld setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "contrast")]];
    [o_adjust_gamma_sld setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "gamma")]];
    [o_adjust_hue_sld setToolTip: [NSString stringWithFormat:@"%.0f", config_GetFloat(p_intf, "hue")]];
    [o_adjust_saturation_sld setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "saturation")]];
    b_state = [o_adjust_ckb state];
    [o_adjust_brightness_sld setEnabled: b_state];
    [o_adjust_brightness_ckb setEnabled: b_state];
    [o_adjust_contrast_sld setEnabled: b_state];
    [o_adjust_gamma_sld setEnabled: b_state];
    [o_adjust_hue_sld setEnabled: b_state];
    [o_adjust_saturation_sld setEnabled: b_state];
    [o_adjust_brightness_lbl setEnabled: b_state];
    [o_adjust_contrast_lbl setEnabled: b_state];
    [o_adjust_gamma_lbl setEnabled: b_state];
    [o_adjust_hue_lbl setEnabled: b_state];
    [o_adjust_saturation_lbl setEnabled: b_state];
    [o_adjust_reset_btn setEnabled: b_state];

    [o_sharpen_sld setFloatValue: config_GetFloat(p_intf, "sharpen-sigma")];
    [o_sharpen_sld setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "sharpen-sigma")]];
    [o_sharpen_sld setEnabled: [o_sharpen_ckb state]];
    [o_sharpen_lbl setEnabled: [o_sharpen_ckb state]];

    [o_banding_sld setIntValue: config_GetInt(p_intf, "gradfun-radius")];
    [o_banding_sld setToolTip: [NSString stringWithFormat:@"%lli", config_GetInt(p_intf, "gradfun-radius")]];
    [o_banding_sld setEnabled: [o_banding_ckb state]];
    [o_banding_lbl setEnabled: [o_banding_ckb state]];

    [o_grain_sld setFloatValue: config_GetFloat(p_intf, "grain-variance")];
    [o_grain_sld setToolTip: [NSString stringWithFormat:@"%0.3f", config_GetFloat(p_intf, "grain-variance")]];
    [o_grain_sld setEnabled: [o_grain_ckb state]];
    [o_grain_lbl setEnabled: [o_grain_ckb state]];

    [self setCropLeftValue: 0];
    [self setCropTopValue: 0];
    [self setCropRightValue: 0];
    [self setCropBottomValue: 0];
    [o_crop_sync_top_bottom_ckb setState: NSOffState];
    [o_crop_sync_left_right_ckb setState: NSOffState];

    tmpChar = config_GetPsz(p_intf, "transform-type");
    tmpString = [NSString stringWithUTF8String:tmpChar];
    if ([tmpString isEqualToString:@"hflip"])
        [o_transform_pop selectItemWithTag: 1];
    else if ([tmpString isEqualToString:@"vflip"])
        [o_transform_pop selectItemWithTag: 2];
    else
        [o_transform_pop selectItemWithTag:[tmpString intValue]];
    FREENULL(tmpChar);
    [o_transform_pop setEnabled: [o_transform_ckb state]];

    [self setPuzzleColumnsValue: config_GetInt(p_intf, "puzzle-cols")];
    [self setPuzzleRowsValue: config_GetInt(p_intf, "puzzle-rows")];
    b_state = [o_puzzle_ckb state];
    [o_puzzle_rows_fld setEnabled: b_state];
    [o_puzzle_rows_stp setEnabled: b_state];
    [o_puzzle_rows_lbl setEnabled: b_state];
    [o_puzzle_columns_fld setEnabled: b_state];
    [o_puzzle_columns_stp setEnabled: b_state];
    [o_puzzle_columns_lbl setEnabled: b_state];

    [self setCloneValue: config_GetInt(p_intf, "clone-count")];
    b_state = [o_clone_ckb state];
    [o_clone_number_lbl setEnabled: b_state];
    [o_clone_number_fld setEnabled: b_state];
    [o_clone_number_stp setEnabled: b_state];

    b_state = [o_wall_ckb state];
    [self setWallRowsValue: config_GetInt(p_intf, "wall-rows")];
    [o_wall_numofrows_lbl setEnabled: b_state];
    [o_wall_numofrows_fld setEnabled: b_state];
    [o_wall_numofrows_stp setEnabled: b_state];
    [self setWallColumnsValue: config_GetInt(p_intf, "wall-cols")];
    [o_wall_numofcols_lbl setEnabled: b_state];
    [o_wall_numofcols_fld setEnabled: b_state];
    [o_wall_numofcols_stp setEnabled: b_state];

    [o_threshold_color_fld setStringValue: [[NSString stringWithFormat:@"%llx", config_GetInt(p_intf, "colorthres-color")] uppercaseString]];
    [o_threshold_saturation_sld setIntValue: config_GetInt(p_intf, "colorthres-saturationthres")];
    [o_threshold_saturation_sld setToolTip: [NSString stringWithFormat:@"%lli", config_GetInt(p_intf, "colorthres-saturationthres")]];
    [o_threshold_similarity_sld setIntValue: config_GetInt(p_intf, "colorthres-similaritythres")];
    [o_threshold_similarity_sld setToolTip: [NSString stringWithFormat:@"%lli", config_GetInt(p_intf, "colorthres-similaritythres")]];

    b_state = [o_threshold_ckb state];
    [o_threshold_color_fld setEnabled: b_state];
    [o_threshold_color_lbl setEnabled: b_state];
    [o_threshold_saturation_sld setEnabled: b_state];
    [o_threshold_saturation_lbl setEnabled: b_state];
    [o_threshold_similarity_sld setEnabled: b_state];
    [o_threshold_similarity_lbl setEnabled: b_state];

    [self setSepiaValue: config_GetInt(p_intf, "sepia-intensity")];
    b_state = [o_sepia_ckb state];
    [o_sepia_fld setEnabled: b_state];
    [o_sepia_stp setEnabled: b_state];
    [o_sepia_lbl setEnabled: b_state];

    tmpChar = config_GetPsz(p_intf, "gradient-mode");
    tmpString = [NSString stringWithUTF8String:tmpChar];
    if ([tmpString isEqualToString:@"hough"])
        [o_gradient_mode_pop selectItemWithTag: 3];
    else if ([tmpString isEqualToString:@"edge"])
        [o_gradient_mode_pop selectItemWithTag: 2];
    else
        [o_gradient_mode_pop selectItemWithTag: 1];
    FREENULL(tmpChar);
    [o_gradient_cartoon_ckb setState: config_GetInt(p_intf, "gradient-cartoon")];
    [o_gradient_color_ckb setState: config_GetInt(p_intf, "gradient-type")];
    b_state = [o_gradient_ckb state];
    [o_gradient_mode_pop setEnabled: b_state];
    [o_gradient_mode_lbl setEnabled: b_state];
    [o_gradient_cartoon_ckb setEnabled: b_state];
    [o_gradient_color_ckb setEnabled: b_state];

    [o_extract_fld setStringValue: [[NSString stringWithFormat:@"%llx", config_GetInt(p_intf, "extract-component")] uppercaseString]];
    [o_extract_fld setEnabled: [o_extract_ckb state]];
    [o_extract_lbl setEnabled: [o_extract_ckb state]];

    [self setPosterizeValue: config_GetInt(p_intf, "posterize-level")];
    b_state = [o_posterize_ckb state];
    [o_posterize_fld setEnabled: b_state];
    [o_posterize_stp setEnabled: b_state];
    [o_posterize_lbl setEnabled: b_state];

    [o_blur_sld setIntValue: config_GetInt(p_intf, "blur-factor")];
    [o_blur_sld setToolTip: [NSString stringWithFormat:@"%lli", config_GetInt(p_intf, "blur-factor")]];
    [o_blur_sld setEnabled: [o_blur_ckb state]];
    [o_blur_lbl setEnabled: [o_blur_ckb state]];

    tmpChar = config_GetPsz(p_intf, "marq-marquee");
    if (tmpChar) {
        [o_addtext_text_fld setStringValue: [NSString stringWithUTF8String:tmpChar]];
        FREENULL(tmpChar);
    } else
        [o_addtext_text_fld setStringValue: @""];
    [o_addtext_pos_pop selectItemWithTag: config_GetInt(p_intf, "marq-position")];
    b_state = [o_addtext_ckb state];
    [o_addtext_pos_pop setEnabled: b_state];
    [o_addtext_pos_lbl setEnabled: b_state];
    [o_addtext_text_lbl setEnabled: b_state];
    [o_addtext_text_fld setEnabled: b_state];

    tmpChar = config_GetPsz(p_intf, "logo-file");
    if (tmpChar) {
        [o_addlogo_logo_fld setStringValue: [NSString stringWithUTF8String:tmpChar]];
        FREENULL(tmpChar);
    } else
        [o_addlogo_logo_fld setStringValue: @""];
    [o_addlogo_pos_pop selectItemWithTag: config_GetInt(p_intf, "logo-position")];
    [o_addlogo_transparency_sld setIntValue: config_GetInt(p_intf, "logo-opacity")];
    [o_addlogo_transparency_sld setToolTip: [NSString stringWithFormat:@"%lli", config_GetInt(p_intf, "logo-opacity")]];
    b_state = [o_addlogo_ckb state];
    [o_addlogo_pos_pop setEnabled: b_state];
    [o_addlogo_pos_lbl setEnabled: b_state];
    [o_addlogo_logo_fld setEnabled: b_state];
    [o_addlogo_logo_lbl setEnabled: b_state];
    [o_addlogo_transparency_sld setEnabled: b_state];
    [o_addlogo_transparency_lbl setEnabled: b_state];
}

- (NSString *)generateProfileString
{
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
    [workArray release];
    [defaults synchronize];
}

- (IBAction)toggleWindow:(id)sender
{
    if ([o_window isKeyWindow])
        [o_window orderOut:sender];
    else {
        [o_window setLevel: [[[VLCMain sharedInstance] voutController] currentStatusWindowLevel]];
        [o_window makeKeyAndOrderFront:sender];
    }
}

- (IBAction)profileSelectorAction:(id)sender
{
    [self saveCurrentProfile];
    i_old_profile_index = [o_profile_pop indexOfSelectedItem];
    VLCCoreInteraction *vci_si = [VLCCoreInteraction sharedInstance];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSUInteger selectedProfile = [o_profile_pop indexOfSelectedItem];

    /* fetch preset */
    NSArray *items = [[[defaults objectForKey:@"VideoEffectProfiles"] objectAtIndex:selectedProfile] componentsSeparatedByString:@";"];

    // version 1 of profile string has 32 entries
    if ([items count] < 32) {
        msg_Err(p_intf, "Error in parsing profile string");
        [self resetValues];
        return;
    }

    /* filter handling */
    NSString *tempString = B64DecNSStr([items objectAtIndex:0]);
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

- (IBAction)addProfile:(id)sender
{
    /* show panel */
    VLCEnterTextPanel * panel = [VLCEnterTextPanel sharedInstance];
    [panel setTitle: _NS("Duplicate current profile for a new profile")];
    [panel setSubTitle: _NS("Enter a name for the new profile:")];
    [panel setCancelButtonLabel: _NS("Cancel")];
    [panel setOKButtonLabel: _NS("Save")];
    [panel setTarget:self];

    [panel runModalForWindow:o_window];
}

- (void)panel:(VLCEnterTextPanel *)panel returnValue:(NSUInteger)value text:(NSString *)text
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if (value != NSOKButton) {
        [o_profile_pop selectItemAtIndex:[defaults integerForKey:@"VideoEffectSelectedProfile"]];
        return;
    }

    NSArray *profileNames = [defaults objectForKey:@"VideoEffectProfileNames"];

    // duplicate names are not allowed in the popup control
    if ([text length] == 0 || [profileNames containsObject:text]) {
        [o_profile_pop selectItemAtIndex:[defaults integerForKey:@"VideoEffectSelectedProfile"]];

        NSAlert *alert = [[[NSAlert alloc] init] autorelease];
        [alert setAlertStyle:NSCriticalAlertStyle];
        [alert setMessageText:_NS("Please enter a unique name for the new profile.")];
        [alert setInformativeText:_NS("Multiple profiles with the same name are not allowed.")];

        [alert beginSheetModalForWindow:o_window
                          modalDelegate:nil
                         didEndSelector:nil
                            contextInfo:nil];
        return;
    }

    /* fetch all the current settings in a uniform string */
    NSString *newProfile = [self generateProfileString];

    /* add string to user defaults as well as a label */

    NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"VideoEffectProfiles"]];
    [workArray addObject:newProfile];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"VideoEffectProfiles"];
    [defaults setInteger:[workArray count] - 1 forKey:@"VideoEffectSelectedProfile"];
    [workArray release];

    workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"VideoEffectProfileNames"]];
    [workArray addObject:text];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"VideoEffectProfileNames"];
    [workArray release];

    /* save defaults */
    [defaults synchronize];

    /* refresh UI */
    [self resetProfileSelector];
}

- (IBAction)removeProfile:(id)sender
{
    /* show panel */
    VLCSelectItemInPopupPanel * panel = [VLCSelectItemInPopupPanel sharedInstance];
    [panel setTitle:_NS("Remove a preset")];
    [panel setSubTitle:_NS("Select the preset you would like to remove:")];
    [panel setOKButtonLabel:_NS("Remove")];
    [panel setCancelButtonLabel:_NS("Cancel")];
    [panel setPopupButtonContent:[[NSUserDefaults standardUserDefaults] objectForKey:@"VideoEffectProfileNames"]];
    [panel setTarget:self];

    [panel runModalForWindow:o_window];
}

- (void)panel:(VLCSelectItemInPopupPanel *)panel returnValue:(NSUInteger)value item:(NSUInteger)item
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if (value != NSOKButton) {
        [o_profile_pop selectItemAtIndex:[defaults integerForKey:@"VideoEffectSelectedProfile"]];
        return;
    }

    /* remove selected profile from settings */
    NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray: [defaults objectForKey:@"VideoEffectProfiles"]];
    [workArray removeObjectAtIndex:item];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"VideoEffectProfiles"];
    [workArray release];

    workArray = [[NSMutableArray alloc] initWithArray: [defaults objectForKey:@"VideoEffectProfileNames"]];
    [workArray removeObjectAtIndex:item];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"VideoEffectProfileNames"];
    [workArray release];

    if (i_old_profile_index >= item)
        [defaults setInteger:i_old_profile_index - 1 forKey:@"VideoEffectSelectedProfile"];

    /* save defaults */
    [defaults synchronize];

    /* do not save deleted profile */
    i_old_profile_index = -1;
    /* refresh UI */
    [self resetProfileSelector];
}

#pragma mark -
#pragma mark basic
- (IBAction)enableAdjust:(id)sender
{
    BOOL b_state = [o_adjust_ckb state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "adjust" on: b_state];
    [o_adjust_brightness_sld setEnabled: b_state];
    [o_adjust_brightness_ckb setEnabled: b_state];
    [o_adjust_brightness_lbl setEnabled: b_state];
    [o_adjust_contrast_sld setEnabled: b_state];
    [o_adjust_contrast_lbl setEnabled: b_state];
    [o_adjust_gamma_sld setEnabled: b_state];
    [o_adjust_gamma_lbl setEnabled: b_state];
    [o_adjust_hue_sld setEnabled: b_state];
    [o_adjust_hue_lbl setEnabled: b_state];
    [o_adjust_saturation_sld setEnabled: b_state];
    [o_adjust_saturation_lbl setEnabled: b_state];
    [o_adjust_reset_btn setEnabled: b_state];
}

- (IBAction)adjustSliderChanged:(id)sender
{
    if (sender == o_adjust_brightness_sld)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "brightness" forFilter: "adjust" float: [o_adjust_brightness_sld floatValue]];
    else if (sender == o_adjust_contrast_sld)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "contrast" forFilter: "adjust" float: [o_adjust_contrast_sld floatValue]];
    else if (sender == o_adjust_gamma_sld)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gamma" forFilter: "adjust" float: [o_adjust_gamma_sld floatValue]];
    else if (sender == o_adjust_hue_sld)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "hue" forFilter: "adjust" float: [o_adjust_hue_sld floatValue]];
    else if (sender == o_adjust_saturation_sld)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "saturation" forFilter: "adjust" float: [o_adjust_saturation_sld floatValue]];

    if (sender == o_adjust_hue_sld)
        [o_adjust_hue_sld setToolTip: [NSString stringWithFormat:@"%.0f", [o_adjust_hue_sld floatValue]]];
    else
        [sender setToolTip: [NSString stringWithFormat:@"%0.3f", [sender floatValue]]];
}

- (IBAction)enableAdjustBrightnessThreshold:(id)sender
{
    VLCCoreInteraction *vci_si = [VLCCoreInteraction sharedInstance];

    if (sender == o_adjust_reset_btn) {
        [o_adjust_brightness_sld setFloatValue: 1.0];
        [o_adjust_contrast_sld setFloatValue: 1.0];
        [o_adjust_gamma_sld setFloatValue: 1.0];
        [o_adjust_hue_sld setFloatValue: 0];
        [o_adjust_saturation_sld setFloatValue: 1.0];
        [o_adjust_brightness_sld setToolTip: [NSString stringWithFormat:@"%0.3f", 1.0]];
        [o_adjust_contrast_sld setToolTip: [NSString stringWithFormat:@"%0.3f", 1.0]];
        [o_adjust_gamma_sld setToolTip: [NSString stringWithFormat:@"%0.3f", 1.0]];
        [o_adjust_hue_sld setToolTip: [NSString stringWithFormat:@"%.0f", 0.0]];
        [o_adjust_saturation_sld setToolTip: [NSString stringWithFormat:@"%0.3f", 1.0]];
        [vci_si setVideoFilterProperty: "brightness" forFilter: "adjust" float: 1.0];
        [vci_si setVideoFilterProperty: "contrast" forFilter: "adjust" float: 1.0];
        [vci_si setVideoFilterProperty: "gamma" forFilter: "adjust" float: 1.0];
        [vci_si setVideoFilterProperty: "hue" forFilter: "adjust" float: 0.0];
        [vci_si setVideoFilterProperty: "saturation" forFilter: "adjust" float: 1.0];
    } else
        [vci_si setVideoFilterProperty: "brightness-threshold" forFilter: "adjust" boolean: [o_adjust_brightness_ckb state]];

}

- (IBAction)enableSharpen:(id)sender
{
    BOOL b_state = [o_sharpen_ckb state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "sharpen" on: b_state];
    [o_sharpen_sld setEnabled: b_state];
    [o_sharpen_lbl setEnabled: b_state];
}

- (IBAction)sharpenSliderChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "sharpen-sigma" forFilter: "sharpen" float: [sender floatValue]];
    [sender setToolTip: [NSString stringWithFormat:@"%0.3f", [sender floatValue]]];
}

- (IBAction)enableBanding:(id)sender
{
    BOOL b_state = [o_banding_ckb state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "gradfun" on: b_state];
    [o_banding_sld setEnabled: b_state];
    [o_banding_lbl setEnabled: b_state];
}

- (IBAction)bandingSliderChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradfun-radius" forFilter: "gradfun" integer: [sender intValue]];
    [sender setToolTip: [NSString stringWithFormat:@"%i", [sender intValue]]];
}

- (IBAction)enableGrain:(id)sender
{
    BOOL b_state = [o_grain_ckb state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "grain" on: b_state];
    [o_grain_sld setEnabled: b_state];
    [o_grain_lbl setEnabled: b_state];
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
    if ([o_crop_sync_top_bottom_ckb state]) {
        if (sender == o_crop_bottom_fld || sender == o_crop_bottom_stp)
            [self setCropTopValue: [self cropBottomValue]];
        else
            [self setCropBottomValue: [self cropTopValue]];
    }
    if ([o_crop_sync_left_right_ckb state]) {
        if (sender == o_crop_right_fld || sender == o_crop_right_stp)
            [self setCropLeftValue: [self cropRightValue]];
        else
            [self setCropRightValue: [self cropLeftValue]];
    }

    vout_thread_t *p_vout = getVout();
    if (p_vout) {
        var_SetInteger(p_vout, "crop-top", [o_crop_top_fld intValue]);
        var_SetInteger(p_vout, "crop-bottom", [o_crop_bottom_fld intValue]);
        var_SetInteger(p_vout, "crop-left", [o_crop_left_fld intValue]);
        var_SetInteger(p_vout, "crop-right", [o_crop_right_fld intValue]);
        vlc_object_release(p_vout);
    }
}

#pragma mark -
#pragma mark geometry
- (IBAction)enableTransform:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "transform" on: [o_transform_ckb state]];
    [o_transform_pop setEnabled: [o_transform_ckb state]];
}

- (IBAction)transformModifierChanged:(id)sender
{
    NSInteger tag = [[o_transform_pop selectedItem] tag];
    const char *psz_string = [[NSString stringWithFormat:@"%li", tag] UTF8String];
    if (tag == 1)
        psz_string = "hflip";
    else if (tag == 2)
        psz_string = "vflip";

    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "transform-type" forFilter: "transform" string: psz_string];
}

- (IBAction)enableZoom:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "magnify" on: [o_zoom_ckb state]];
}

- (IBAction)enablePuzzle:(id)sender
{
    BOOL b_state = [o_puzzle_ckb state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "puzzle" on: b_state];
    [o_puzzle_columns_fld setEnabled: b_state];
    [o_puzzle_columns_stp setEnabled: b_state];
    [o_puzzle_columns_lbl setEnabled: b_state];
    [o_puzzle_rows_fld setEnabled: b_state];
    [o_puzzle_rows_stp setEnabled: b_state];
    [o_puzzle_rows_lbl setEnabled: b_state];
}

- (IBAction)puzzleModifierChanged:(id)sender
{
    if (sender == o_puzzle_columns_fld || sender == o_puzzle_columns_stp)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "puzzle-cols" forFilter: "puzzle" integer: [sender intValue]];
    else
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "puzzle-rows" forFilter: "puzzle" integer: [sender intValue]];
}

- (IBAction)enableClone:(id)sender
{
    BOOL b_state = [o_clone_ckb state];

    if (b_state && [o_wall_ckb state]) {
        [o_wall_ckb setState: NSOffState];
        [self enableWall: o_wall_ckb];
    }

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "clone" on: b_state];
    [o_clone_number_lbl setEnabled: b_state];
    [o_clone_number_fld setEnabled: b_state];
    [o_clone_number_stp setEnabled: b_state];
}

- (IBAction)cloneModifierChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "clone-count" forFilter: "clone" integer: [o_clone_number_fld intValue]];
}

- (IBAction)enableWall:(id)sender
{
    BOOL b_state = [o_wall_ckb state];

    if (b_state && [o_clone_ckb state]) {
        [o_clone_ckb setState: NSOffState];
        [self enableClone: o_clone_ckb];
    }

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "wall" on: b_state];
    [o_wall_numofcols_fld setEnabled: b_state];
    [o_wall_numofcols_stp setEnabled: b_state];
    [o_wall_numofcols_lbl setEnabled: b_state];

    [o_wall_numofrows_fld setEnabled: b_state];
    [o_wall_numofrows_stp setEnabled: b_state];
    [o_wall_numofrows_lbl setEnabled: b_state];
}

- (IBAction)wallModifierChanged:(id)sender
{
    if (sender == o_wall_numofcols_fld || sender == o_wall_numofcols_stp)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "wall-cols" forFilter: "wall" integer: [sender intValue]];
    else
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "wall-rows" forFilter: "wall" integer: [sender intValue]];
}

#pragma mark -
#pragma mark color
- (IBAction)enableThreshold:(id)sender
{
    BOOL b_state = [o_threshold_ckb state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "colorthres" on: b_state];
    [o_threshold_color_fld setEnabled: b_state];
    [o_threshold_color_lbl setEnabled: b_state];
    [o_threshold_saturation_sld setEnabled: b_state];
    [o_threshold_saturation_lbl setEnabled: b_state];
    [o_threshold_similarity_sld setEnabled: b_state];
    [o_threshold_similarity_lbl setEnabled: b_state];
}

- (IBAction)thresholdModifierChanged:(id)sender
{
    if (sender == o_threshold_color_fld)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "colorthres-color" forFilter: "colorthres" integer: [o_threshold_color_fld intValue]];
    else if (sender == o_threshold_saturation_sld) {
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "colorthres-saturationthres" forFilter: "colorthres" integer: [o_threshold_saturation_sld intValue]];
        [o_threshold_saturation_sld setToolTip: [NSString stringWithFormat:@"%i", [o_threshold_saturation_sld intValue]]];
    } else {
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "colorthres-similaritythres" forFilter: "colorthres" integer: [o_threshold_similarity_sld intValue]];
        [o_threshold_similarity_sld setToolTip: [NSString stringWithFormat:@"%i", [o_threshold_similarity_sld intValue]]];
    }
}

- (IBAction)enableSepia:(id)sender
{
    BOOL b_state = [o_sepia_ckb state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "sepia" on: b_state];
    [o_sepia_fld setEnabled: b_state];
    [o_sepia_stp setEnabled: b_state];
    [o_sepia_lbl setEnabled: b_state];
}

- (IBAction)sepiaModifierChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "sepia-intensity" forFilter: "sepia" integer: [o_sepia_fld intValue]];
}

- (IBAction)enableNoise:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "noise" on: [o_noise_ckb state]];
}

- (IBAction)enableGradient:(id)sender
{
    BOOL b_state = [o_gradient_ckb state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "gradient" on: b_state];
    [o_gradient_mode_pop setEnabled: b_state];
    [o_gradient_mode_lbl setEnabled: b_state];
    [o_gradient_color_ckb setEnabled: b_state];
    [o_gradient_cartoon_ckb setEnabled: b_state];
}

- (IBAction)gradientModifierChanged:(id)sender
{
    if (sender == o_gradient_mode_pop) {
        if ([[o_gradient_mode_pop selectedItem] tag] == 3)
            [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradient-mode" forFilter: "gradient" string: "hough"];
        else if ([[o_gradient_mode_pop selectedItem] tag] == 2)
            [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradient-mode" forFilter: "gradient" string: "edge"];
        else
            [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradient-mode" forFilter: "gradient" string: "gradient"];
    } else if (sender == o_gradient_color_ckb)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradient-type" forFilter: "gradient" integer: [o_gradient_color_ckb state]];
    else
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "gradient-cartoon" forFilter: "gradient" boolean: [o_gradient_cartoon_ckb state]];
}

- (IBAction)enableExtract:(id)sender
{
    BOOL b_state = [o_extract_ckb state];
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "extract" on: b_state];
    [o_extract_fld setEnabled: b_state];
    [o_extract_lbl setEnabled: b_state];
}

- (IBAction)extractModifierChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "extract-component" forFilter: "extract" integer: [o_extract_fld intValue]];
}

- (IBAction)enableInvert:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "invert" on: [o_invert_ckb state]];
}

- (IBAction)enablePosterize:(id)sender
{
    BOOL b_state = [o_posterize_ckb state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "posterize" on: b_state];
    [o_posterize_fld setEnabled: b_state];
    [o_posterize_stp setEnabled: b_state];
    [o_posterize_lbl setEnabled: b_state];
}

- (IBAction)posterizeModifierChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "posterize-level" forFilter: "posterize" integer: [o_posterize_fld intValue]];
}

- (IBAction)enableBlur:(id)sender
{
    BOOL b_state = [o_blur_ckb state];

    [[VLCCoreInteraction sharedInstance] setVideoFilter: "motionblur" on: b_state];
    [o_blur_sld setEnabled: b_state];
    [o_blur_lbl setEnabled: b_state];
}

- (IBAction)blurModifierChanged:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "blur-factor" forFilter: "motionblur" integer: [sender intValue]];
    [sender setToolTip: [NSString stringWithFormat:@"%i", [sender intValue]]];
}

- (IBAction)enableMotionDetect:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "motiondetect" on: [o_motiondetect_ckb state]];
}

- (IBAction)enableWaterEffect:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "ripple" on: [o_watereffect_ckb state]];
}

- (IBAction)enableWaves:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "wave" on: [o_waves_ckb state]];
}

- (IBAction)enablePsychedelic:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "psychedelic" on: [o_psychedelic_ckb state]];
}

#pragma mark -
#pragma mark Miscellaneous
- (IBAction)enableAddText:(id)sender
{
    BOOL b_state = [o_addtext_ckb state];
    VLCCoreInteraction *vci_si = [VLCCoreInteraction sharedInstance];

    [o_addtext_pos_pop setEnabled: b_state];
    [o_addtext_pos_lbl setEnabled: b_state];
    [o_addtext_text_lbl setEnabled: b_state];
    [o_addtext_text_fld setEnabled: b_state];
    [vci_si setVideoFilter: "marq" on: b_state];
    [vci_si setVideoFilterProperty: "marq-marquee" forFilter: "marq" string: [[o_addtext_text_fld stringValue] UTF8String]];
    [vci_si setVideoFilterProperty: "marq-position" forFilter: "marq" integer: [[o_addtext_pos_pop selectedItem] tag]];
}

- (IBAction)addTextModifierChanged:(id)sender
{
    if (sender == o_addtext_text_fld)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "marq-marquee" forFilter: "marq" string:[[o_addtext_text_fld stringValue] UTF8String]];
    else
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "marq-position" forFilter: "marq" integer: [[o_addtext_pos_pop selectedItem] tag]];
}

- (IBAction)enableAddLogo:(id)sender
{
    BOOL b_state = [o_addlogo_ckb state];

    [o_addlogo_pos_pop setEnabled: b_state];
    [o_addlogo_pos_lbl setEnabled: b_state];
    [o_addlogo_logo_fld setEnabled: b_state];
    [o_addlogo_logo_lbl setEnabled: b_state];
    [o_addlogo_transparency_sld setEnabled: b_state];
    [o_addlogo_transparency_lbl setEnabled: b_state];
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "logo" on: b_state];
}

- (IBAction)addLogoModifierChanged:(id)sender
{
    if (sender == o_addlogo_logo_fld)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "logo-file" forFilter: "logo" string: [[o_addlogo_logo_fld stringValue] UTF8String]];
    else if (sender == o_addlogo_pos_pop)
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "logo-position" forFilter: "logo" integer: [[o_addlogo_pos_pop selectedItem] tag]];
    else {
        [[VLCCoreInteraction sharedInstance] setVideoFilterProperty: "logo-opacity" forFilter: "logo" integer: [o_addlogo_transparency_sld intValue]];
        [o_addlogo_transparency_sld setToolTip: [NSString stringWithFormat:@"%i", [o_addlogo_transparency_sld intValue]]];
    }
}

- (IBAction)enableAnaglyph:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVideoFilter: "anaglyph" on: [o_anaglyph_ckb state]];
}

@end
