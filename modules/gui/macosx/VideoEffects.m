/*****************************************************************************
 * VideoEffects.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011 Felix Paul Kühne
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
#import <vlc_common.h>
#import <vlc_modules.h>
#import "VideoEffects.h"

#pragma mark -
#pragma mark Initialization & Generic code

@implementation VLCVideoEffects
static VLCVideoEffects *_o_sharedInstance = nil;

+ (VLCVideoEffects *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        p_intf = VLCIntf;
        _o_sharedInstance = [super init];
    }
    
    return _o_sharedInstance;
}

- (IBAction)toggleWindow:(id)sender
{
    if( [o_window isVisible] )
        [o_window orderOut:sender];
    else
        [o_window makeKeyAndOrderFront:sender];
}

- (void)awakeFromNib
{
    [o_window setTitle: _NS("Video Effects")];
    [o_window setExcludedFromWindowsMenu:YES];
    if (OSX_LION)
        [o_window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"basic"]] setLabel:_NS("Basic")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"crop"]] setLabel:_NS("Crop")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"geometry"]] setLabel:_NS("Geometry")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"color"]] setLabel:_NS("Color")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"misc"]] setLabel:_NS("Miscellaneous")];

    [o_adjust_ckb setTitle:_NS("Image Adjust")];
    [o_adjust_hue_lbl setStringValue:_NS("Hue")];
    [o_adjust_contrast_lbl setStringValue:_NS("Contrast")];
    [o_adjust_brightness_lbl setStringValue:_NS("Brightness")];
    [o_adjust_brightness_ckb setTitle:_NS("Brightness Threshold")];
    [o_adjust_saturation_lbl setStringValue:_NS("Saturation")];
    [o_adjust_gamma_lbl setStringValue:_NS("Gamma")];
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
    [o_puzzle_blackslot_ckb setTitle:_NS("Black Slot")];

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

    [o_clone_ckb setTitle:_NS("Image clone")];
    [o_clone_lbl setStringValue:_NS("Number of clones")];
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
    [o_addlogo_top_lbl setStringValue:_NS("Top")];
    [o_addlogo_left_lbl setStringValue:_NS("Left")];
    [o_addlogo_transparency_lbl setStringValue:_NS("Transparency")];

    [o_tableView selectFirstTabViewItem:self];

    [self resetValues];
}

- (void)resetValues
{
    NSString *tmpString;
    char *tmpChar;
    /* do we have any filter enabled? if yes, show it. */
    char * psz_vfilters;
    psz_vfilters = config_GetPsz( p_intf, "video-filter" );
    if( psz_vfilters ) {
        [o_adjust_ckb setState: (NSInteger)strstr( psz_vfilters, "adjust")];
        [o_sharpen_ckb setState: (NSInteger)strstr( psz_vfilters, "sharpen")];
        [o_banding_ckb setState: (NSInteger)strstr( psz_vfilters, "gradfun")];
        [o_grain_ckb setState: (NSInteger)strstr( psz_vfilters, "grain")];
        [o_transform_ckb setState: (NSInteger)strstr( psz_vfilters, "transform")];
        [o_zoom_ckb setState: (NSInteger)strstr( psz_vfilters, "magnify")];
        [o_puzzle_ckb setState: (NSInteger)strstr( psz_vfilters, "puzzle")];
        [o_threshold_ckb setState: (NSInteger)strstr( psz_vfilters, "colorthres")];
        [o_sepia_ckb setState: (NSInteger)strstr( psz_vfilters, "sepia")];
        [o_noise_ckb setState: (NSInteger)strstr( psz_vfilters, "noise")];
        [o_gradient_ckb setState: (NSInteger)strstr( psz_vfilters, "gradient")];
        [o_extract_ckb setState: (NSInteger)strstr( psz_vfilters, "extract")];
        [o_invert_ckb setState: (NSInteger)strstr( psz_vfilters, "invert")];
        [o_posterize_ckb setState: (NSInteger)strstr( psz_vfilters, "posterize")];
        [o_blur_ckb setState: (NSInteger)strstr( psz_vfilters, "motionblur")];
        [o_motiondetect_ckb setState: (NSInteger)strstr( psz_vfilters, "motiondetect")];
        [o_watereffect_ckb setState: (NSInteger)strstr( psz_vfilters, "ripple")];
        [o_waves_ckb setState: (NSInteger)strstr( psz_vfilters, "wave")];
        [o_psychedelic_ckb setState: (NSInteger)strstr( psz_vfilters, "psychedelic")];
        [o_clone_ckb setState: (NSInteger)strstr( psz_vfilters, "clone")];
        free( psz_vfilters );
    }
    // TODO: don't forget about o_addtext_ckb, o_addlogo_ckb, o_eraselogo_ckb

    /* fetch and show the various values */
    [o_adjust_hue_sld setIntValue: config_GetInt( p_intf, "hue" )];
    [o_adjust_contrast_sld setFloatValue: config_GetFloat( p_intf, "contrast" )];
    [o_adjust_brightness_sld setFloatValue: config_GetFloat( p_intf, "brightness" )];
    [o_adjust_saturation_sld setFloatValue: config_GetFloat( p_intf, "saturation" )];
    [o_adjust_gamma_sld setFloatValue: config_GetFloat( p_intf, "gamma" )];
    [o_adjust_brightness_sld setEnabled: [o_adjust_ckb state]];
    [o_adjust_brightness_ckb setEnabled: [o_adjust_ckb state]];
    [o_adjust_contrast_sld setEnabled: [o_adjust_ckb state]];
    [o_adjust_gamma_sld setEnabled: [o_adjust_ckb state]];
    [o_adjust_hue_sld setEnabled: [o_adjust_ckb state]];
    [o_adjust_saturation_sld setEnabled: [o_adjust_ckb state]];
    [o_adjust_brightness_lbl setEnabled: [o_adjust_ckb state]];
    [o_adjust_contrast_lbl setEnabled: [o_adjust_ckb state]];
    [o_adjust_gamma_lbl setEnabled: [o_adjust_ckb state]];
    [o_adjust_hue_lbl setEnabled: [o_adjust_ckb state]];
    [o_adjust_saturation_lbl setEnabled: [o_adjust_ckb state]];
    [o_sharpen_sld setFloatValue: config_GetFloat( p_intf, "sharpen-sigma" )];
    [o_sharpen_sld setEnabled: [o_sharpen_ckb state]];
    [o_sharpen_lbl setEnabled: [o_sharpen_ckb state]];
    [o_banding_sld setIntValue: config_GetInt( p_intf, "gradfun-radius" )];
    [o_banding_sld setEnabled: [o_banding_ckb state]];
    [o_banding_lbl setEnabled: [o_banding_ckb state]];
    [o_grain_sld setFloatValue: config_GetFloat( p_intf, "grain-variance" )];
    [o_grain_sld setEnabled: [o_grain_ckb state]];
    [o_grain_lbl setEnabled: [o_grain_ckb state]];

    [o_crop_top_fld setIntValue: 0];
    [o_crop_left_fld setIntValue: 0];
    [o_crop_right_fld setIntValue: 0];
    [o_crop_bottom_fld setIntValue: 0];
    [o_crop_sync_top_bottom_ckb setState: NSOffState];
    [o_crop_sync_left_right_ckb setState: NSOffState];

    tmpChar = config_GetPsz( p_intf, "transform-type" );
    tmpString = [NSString stringWithUTF8String: tmpChar];
    if( [tmpString isEqualToString:@"hflip"] )
        [o_transform_pop selectItemWithTag: 1];
    else if( [tmpString isEqualToString:@"vflip"] )
        [o_transform_pop selectItemWithTag: 2];
    else
        [o_transform_pop selectItemWithTag:[tmpString intValue]];
    FREENULL( tmpChar );
    [o_transform_pop setEnabled: [o_transform_ckb state]];
    [o_puzzle_rows_fld setIntValue: config_GetInt( p_intf, "puzzle-rows" )];
    [o_puzzle_columns_fld setIntValue: config_GetInt( p_intf, "puzzle-cols" )];
    [o_puzzle_blackslot_ckb setState: config_GetInt( p_intf, "puzzle-black-slot" )];
    [o_puzzle_rows_fld setEnabled: [o_puzzle_ckb state]];
    [o_puzzle_rows_lbl setEnabled: [o_puzzle_ckb state]];
    [o_puzzle_columns_fld setEnabled: [o_puzzle_ckb state]];
    [o_puzzle_columns_lbl setEnabled: [o_puzzle_ckb state]];
    [o_puzzle_blackslot_ckb setEnabled: [o_puzzle_ckb state]];

    [o_threshold_color_fld setStringValue: [[NSString stringWithFormat:@"%x", config_GetInt( p_intf, "colorthres-color" )] uppercaseString]];
    [o_threshold_saturation_sld setIntValue: config_GetInt( p_intf, "colorthres-saturationthres" )];
    [o_threshold_similarity_sld setIntValue: config_GetInt( p_intf, "colorthres-similaritythres" )];
    [o_threshold_color_fld setEnabled: [o_threshold_ckb state]];
    [o_threshold_color_lbl setEnabled: [o_threshold_ckb state]];
    [o_threshold_saturation_sld setEnabled: [o_threshold_ckb state]];
    [o_threshold_saturation_lbl setEnabled: [o_threshold_ckb state]];
    [o_threshold_similarity_sld setEnabled: [o_threshold_ckb state]];
    [o_threshold_similarity_lbl setEnabled: [o_threshold_ckb state]];
    [o_sepia_fld setIntValue: config_GetInt( p_intf, "sepia-intensity" )];
    [o_sepia_fld setEnabled: [o_sepia_ckb state]];
    [o_sepia_lbl setEnabled: [o_sepia_ckb state]];
    tmpChar = config_GetPsz( p_intf, "gradient-mode" );
    tmpString = [NSString stringWithUTF8String: tmpChar];
    if( [tmpString isEqualToString:@"hough"] )
        [o_gradient_mode_pop selectItemWithTag: 3];
    else if( [tmpString isEqualToString:@"edge"] )
        [o_gradient_mode_pop selectItemWithTag: 2];
    else
        [o_gradient_mode_pop selectItemWithTag: 1];
    FREENULL( tmpChar );
    [o_gradient_cartoon_ckb setState: config_GetInt( p_intf, "gradient-cartoon" )];
    [o_gradient_color_ckb setState: config_GetInt( p_intf, "gradient-type" )];
    [o_gradient_mode_pop setEnabled: [o_gradient_ckb state]];
    [o_gradient_mode_lbl setEnabled: [o_gradient_ckb state]];
    [o_gradient_cartoon_ckb setEnabled: [o_gradient_ckb state]];
    [o_gradient_color_ckb setEnabled: [o_gradient_ckb state]];
    [o_extract_fld setStringValue: [[NSString stringWithFormat:@"%x", config_GetInt( p_intf, "extract-component" )] uppercaseString]];
    [o_extract_fld setEnabled: [o_extract_ckb state]];
    [o_extract_lbl setEnabled: [o_extract_ckb state]];
    [o_posterize_fld setIntValue: config_GetInt( p_intf, "posterize-level" )];
    [o_posterize_fld setEnabled: [o_posterize_ckb state]];
    [o_posterize_lbl setEnabled: [o_posterize_ckb state]];
    [o_blur_sld setIntValue: config_GetInt( p_intf, "blur-factor" )];
    [o_blur_sld setEnabled: [o_blur_ckb state]];
    [o_blur_lbl setEnabled: [o_blur_ckb state]];

    [o_clone_fld setIntValue: config_GetInt( p_intf, "clone-count" )];
    [o_clone_fld setEnabled: [o_clone_ckb state]];
    [o_clone_lbl setEnabled: [o_clone_ckb state]];
    tmpChar = config_GetPsz( p_intf, "marq-marquee" );
    if( tmpChar )
    {
        [o_addtext_text_fld setStringValue: [NSString stringWithUTF8String: tmpChar]];
        FREENULL( tmpChar );
    }
    [o_addtext_pos_pop selectItemWithTag: config_GetInt( p_intf, "marq-position" )];
    [o_addtext_pos_pop setEnabled: [o_addtext_ckb state]];
    [o_addtext_pos_lbl setEnabled: [o_addtext_ckb state]];
    [o_addtext_text_lbl setEnabled: [o_addtext_ckb state]];
    [o_addtext_text_fld setEnabled: [o_addtext_ckb state]];

    tmpChar = config_GetPsz( p_intf, "logo-file" );
    if( tmpChar )
    {
       [o_addlogo_logo_fld setStringValue: [NSString stringWithUTF8String: tmpChar]];
        FREENULL( tmpChar );
    }
    [o_addlogo_top_fld setIntValue: config_GetInt( p_intf, "logo-x" )];
    [o_addlogo_left_fld setIntValue: config_GetInt( p_intf, "logo-y" )];
    [o_addlogo_transparency_sld setIntValue: config_GetInt( p_intf, "logo-opacity" )];
    [o_addlogo_logo_fld setEnabled: [o_addlogo_ckb state]];
    [o_addlogo_logo_lbl setEnabled: [o_addlogo_ckb state]];
    [o_addlogo_left_fld setEnabled: [o_addlogo_ckb state]];
    [o_addlogo_left_lbl setEnabled: [o_addlogo_ckb state]];
    [o_addlogo_top_fld setEnabled: [o_addlogo_ckb state]];
    [o_addlogo_top_lbl setEnabled: [o_addlogo_ckb state]];
    [o_addlogo_transparency_sld setEnabled: [o_addlogo_ckb state]];
    [o_addlogo_transparency_lbl setEnabled: [o_addlogo_ckb state]];
}

- (void)setVideoFilter: (char *)psz_name on:(BOOL)b_on
{
    char *psz_string, *psz_parser;
    const char *psz_filter_type;

    module_t *p_obj = module_find( psz_name );
    if( !p_obj )
    {
        msg_Err( p_intf, "Unable to find filter module \"%s\".", psz_name );
        return;
    }
    msg_Dbg( p_intf, "will set filter %s", psz_name );

    if( module_provides( p_obj, "video splitter" ) )
    {
        psz_filter_type = "video-splitter";
    }
    else if( module_provides( p_obj, "video filter2" ) )
    {
        psz_filter_type = "video-filter";
    }
    else if( module_provides( p_obj, "sub source" ) )
    {
        psz_filter_type = "sub-source";
    }
    else if( module_provides( p_obj, "sub filter" ) )
    {
        psz_filter_type = "sub-filter";
    }
    else
    {
        msg_Err( p_intf, "Unknown video filter type." );
        return;
    }

    psz_string = config_GetPsz( p_intf, psz_filter_type );

    if (b_on) {
        if(! psz_string)
            psz_string = psz_name;
        else if( (NSInteger)strstr( psz_string, psz_name ) == NO )
            psz_string = (char *)[[NSString stringWithFormat: @"%s:%s", psz_string, psz_name] UTF8String];
    } else {
        psz_parser = strstr( psz_string, psz_name );
        if( psz_parser )
        {
            if( *( psz_parser + strlen( psz_name ) ) == ':' )
            {
                memmove( psz_parser, psz_parser + strlen( psz_name ) + 1,
                        strlen( psz_parser + strlen( psz_name ) + 1 ) + 1 );
            }
            else
            {
                *psz_parser = '\0';
            }

            /* Remove trailing : : */
            if( strlen( psz_string ) > 0 &&
               *( psz_string + strlen( psz_string ) -1 ) == ':' )
            {
                *( psz_string + strlen( psz_string ) -1 ) = '\0';
            }
        }
        else
        {
            free( psz_string );
            return;
        }
    }
    config_PutPsz( p_intf, psz_filter_type, psz_string );
    msg_Dbg( p_intf, "set string '%s'", psz_string );

    /* Try to set on the fly */
    if( !strcmp( psz_filter_type, "video-splitter" ) )
    {
        playlist_t *p_playlist = pl_Get( p_intf );
        var_SetString( p_playlist, psz_filter_type, psz_string );
    }
    else
    {
        vout_thread_t *p_vout = getVout();
        if( p_vout )
        {
            NSLog( @"set on the fly" );
            var_SetString( p_vout, psz_filter_type, psz_string );
            vlc_object_release( p_vout );
        }
    }
}

- (void)setVideoFilterProperty: (char *)psz_name forFilter: (char *)psz_filter integer: (int)i_value
{
    vout_thread_t *p_vout = getVout();
    vlc_object_t *p_filter;

    if( p_vout == NULL ) {
        config_PutInt( p_intf , psz_name , i_value );
    } else {
        p_filter = vlc_object_find_name( pl_Get(p_intf), psz_filter );

        if(! p_filter ) {
            msg_Err( p_intf, "we're unable to find the filter '%s'", psz_filter );
            vlc_object_release( p_vout );
            return;
        }
        var_SetInteger( p_filter, psz_name, i_value );
        config_PutInt( p_intf, psz_name, i_value );
        vlc_object_release( p_vout );
    }
}

- (void)setVideoFilterProperty: (char *)psz_name forFilter: (char *)psz_filter float: (float)f_value
{
    vout_thread_t *p_vout = getVout();
    vlc_object_t *p_filter;

    if( p_vout == NULL ) {
        config_PutFloat( p_intf , psz_name , f_value );
    } else {
        p_filter = vlc_object_find_name( pl_Get(p_intf), psz_filter );

        if(! p_filter ) {
            msg_Err( p_intf, "we're unable to find the filter '%s'", psz_filter );
            vlc_object_release( p_vout );
            return;
        }
        var_SetFloat( p_filter, psz_name, f_value );
        config_PutFloat( p_intf, psz_name, f_value );
        vlc_object_release( p_vout );
    }
}

- (void)setVideoFilterProperty: (char *)psz_name forFilter: (char *)psz_filter string: (char *)psz_value
{
    vout_thread_t *p_vout = getVout();
    vlc_object_t *p_filter;

    if( p_vout == NULL ) {
        config_PutPsz( p_intf, psz_name, psz_value );
    } else {
        p_filter = vlc_object_find_name( pl_Get(p_intf), psz_filter );

        if(! p_filter ) {
            msg_Err( p_intf, "we're unable to find the filter '%s'", psz_filter );
            vlc_object_release( p_vout );
            return;
        }
        var_SetString( p_filter, psz_name, psz_value );
        config_PutPsz( p_intf, psz_name, psz_value );
        vlc_object_release( p_vout );
    }
}

- (void)setVideoFilterProperty: (char *)psz_name forFilter: (char *)psz_filter boolean: (BOOL)b_value
{
    vout_thread_t *p_vout = getVout();
    vlc_object_t *p_filter;

    if( p_vout == NULL ) {
        config_PutInt( p_intf, psz_name, b_value );
    } else {
        p_filter = vlc_object_find_name( pl_Get(p_intf), psz_filter );

        if(! p_filter ) {
            msg_Err( p_intf, "we're unable to find the filter '%s'", psz_filter );
            vlc_object_release( p_vout );
            return;
        }
        var_SetBool( p_filter, psz_name, b_value );
        config_PutInt( p_intf, psz_name, b_value );
        vlc_object_release( p_vout );
    }
}

#pragma mark -
#pragma mark basic
- (IBAction)enableAdjust:(id)sender
{
    BOOL state = [o_adjust_ckb state];
    [self setVideoFilter: "adjust" on:[o_adjust_ckb state]];
    [o_adjust_brightness_sld setEnabled: state];
    [o_adjust_brightness_ckb setEnabled: state];
    [o_adjust_brightness_lbl setEnabled: state];
    [o_adjust_contrast_sld setEnabled: state];
    [o_adjust_contrast_lbl setEnabled: state];
    [o_adjust_gamma_sld setEnabled: state];
    [o_adjust_gamma_lbl setEnabled: state];
    [o_adjust_hue_sld setEnabled: state];
    [o_adjust_hue_lbl setEnabled: state];
    [o_adjust_saturation_sld setEnabled: state];
    [o_adjust_saturation_lbl setEnabled: state];
}

- (IBAction)adjustSliderChanged:(id)sender
{
        if( sender == o_adjust_brightness_sld )
        [self setVideoFilterProperty: "brightness" forFilter: "adjust" float: [o_adjust_brightness_sld floatValue]];
    else if( sender == o_adjust_contrast_sld )
        [self setVideoFilterProperty: "contrast" forFilter: "adjust" float: [o_adjust_contrast_sld floatValue]];
    else if( sender == o_adjust_gamma_sld )
        [self setVideoFilterProperty: "gamma" forFilter: "adjust" float: [o_adjust_gamma_sld floatValue]];
    else if( sender == o_adjust_hue_sld )
        [self setVideoFilterProperty: "hue" forFilter: "adjust" integer: [o_adjust_hue_sld intValue]];
    else if( sender == o_adjust_saturation_sld )
        [self setVideoFilterProperty: "saturation" forFilter: "adjust" float: [o_adjust_saturation_sld floatValue]];
}

- (IBAction)enableAdjustBrightnessThreshold:(id)sender
{
    config_PutInt( p_intf, "brightness-threshold", [o_adjust_brightness_ckb state] );
}

- (IBAction)enableSharpen:(id)sender
{
    [self setVideoFilter: "sharpen" on: [o_sharpen_ckb state]];
    [o_sharpen_sld setEnabled: [o_sharpen_ckb state]];
    [o_sharpen_lbl setEnabled: [o_sharpen_ckb state]];
}

- (IBAction)sharpenSliderChanged:(id)sender
{
    [self setVideoFilterProperty: "sharpen-sigma" forFilter: "sharpen" float: [o_sharpen_sld floatValue]];
}

- (IBAction)enableBanding:(id)sender
{
    [self setVideoFilter: "gradfun" on: [o_banding_ckb state]];
    [o_banding_sld setEnabled: [o_banding_ckb state]];
    [o_banding_lbl setEnabled: [o_banding_ckb state]];
}

- (IBAction)bandingSliderChanged:(id)sender
{
    [self setVideoFilterProperty: "gradfun-radius" forFilter: "gradfun" integer: [o_banding_sld intValue]];
}

- (IBAction)enableGrain:(id)sender
{
    [self setVideoFilter: "grain" on: [o_grain_ckb state]];
    [o_grain_sld setEnabled: [o_grain_ckb state]];
    [o_grain_lbl setEnabled: [o_grain_ckb state]];
}

- (IBAction)grainSliderChanged:(id)sender
{
    [self setVideoFilterProperty: "grain-variance" forFilter: "grain" float: [o_grain_sld floatValue]];
}


#pragma mark -
#pragma mark crop

#define updateopposite( giver, taker ) \
    if (sender == giver) \
        [taker setIntValue: [giver intValue]]

- (IBAction)cropObjectChanged:(id)sender
{
    updateopposite( o_crop_top_fld, o_crop_top_stp );
    updateopposite( o_crop_top_stp, o_crop_top_fld );
    updateopposite( o_crop_left_fld, o_crop_left_stp );
    updateopposite( o_crop_left_stp, o_crop_left_fld );
    updateopposite( o_crop_right_fld, o_crop_right_stp );
    updateopposite( o_crop_right_stp, o_crop_right_fld );
    updateopposite( o_crop_bottom_fld, o_crop_bottom_stp );
    updateopposite( o_crop_bottom_stp, o_crop_bottom_fld );

    if( [o_crop_sync_top_bottom_ckb state] ) {
        if (sender == o_crop_top_fld || sender == o_crop_top_stp ) {
            [o_crop_bottom_fld setIntValue: [o_crop_top_fld intValue]];
            [o_crop_bottom_stp setIntValue: [o_crop_top_fld intValue]];
        }
        else
        {
            [o_crop_top_fld setIntValue: [o_crop_bottom_fld intValue]];
            [o_crop_top_stp setIntValue: [o_crop_bottom_fld intValue]];
        }
    }
    if( [o_crop_sync_left_right_ckb state] ) {
        if (sender == o_crop_left_fld || sender == o_crop_left_stp ) {
            [o_crop_right_fld setIntValue: [o_crop_left_fld intValue]];
            [o_crop_right_stp setIntValue: [o_crop_left_fld intValue]];
        }
        else
        {
            [o_crop_left_fld setIntValue: [o_crop_right_fld intValue]];
            [o_crop_left_stp setIntValue: [o_crop_right_fld intValue]];
        }
    }

    vout_thread_t *p_vout = getVout();
    if( p_vout ) {
        var_SetInteger( p_vout, "crop-top", [o_crop_top_fld intValue] );
        var_SetInteger( p_vout, "crop-bottom", [o_crop_bottom_fld intValue] );
        var_SetInteger( p_vout, "crop-left", [o_crop_left_fld intValue] );
        var_SetInteger( p_vout, "crop-right", [o_crop_right_fld intValue] );
        vlc_object_release( p_vout );
    }
}

#undef updateopposite

#pragma mark -
#pragma mark geometry
- (IBAction)enableTransform:(id)sender
{
    [self setVideoFilter: "transform" on: [o_transform_ckb state]];
    [o_transform_pop setEnabled: [o_transform_ckb state]];
}

- (IBAction)transformModifierChanged:(id)sender
{
    if( [[o_transform_pop selectedItem] tag] == 1 )
        config_PutPsz( p_intf, "transform-type", "hflip" );
    else if( [[o_transform_pop selectedItem] tag] == 2 )
        config_PutPsz( p_intf, "transform-type", "vflip" );
    else
        config_PutPsz( p_intf, "transform-type", (char *)[o_transform_pop tag] );
}

- (IBAction)enableZoom:(id)sender
{
    [self setVideoFilter: "magnify" on: [o_zoom_ckb state]];
}

- (IBAction)enablePuzzle:(id)sender
{
    BOOL state = [o_puzzle_ckb state];
    [self setVideoFilter: "puzzle" on: state];
    [o_puzzle_columns_fld setEnabled: state];
    [o_puzzle_columns_lbl setEnabled: state];
    [o_puzzle_rows_fld setEnabled: state];
    [o_puzzle_rows_lbl setEnabled: state];
    [o_puzzle_blackslot_ckb setEnabled: state];
}

- (IBAction)puzzleModifierChanged:(id)sender
{
    if( sender == o_puzzle_blackslot_ckb )
        [self setVideoFilterProperty: "puzzle-black-slot" forFilter: "puzzle" boolean: [o_puzzle_blackslot_ckb state]];
    else if( sender == o_puzzle_columns_fld )
        [self setVideoFilterProperty: "puzzle-cols" forFilter: "puzzle" integer: [o_puzzle_columns_fld intValue]];
    else
        [self setVideoFilterProperty: "puzzle-rows" forFilter: "puzzle" integer: [o_puzzle_rows_fld intValue]];
}


#pragma mark -
#pragma mark color
- (IBAction)enableThreshold:(id)sender
{
    BOOL state = [o_threshold_ckb state];
    [self setVideoFilter: "colorthres" on: state];
    [o_threshold_color_fld setEnabled: state];
    [o_threshold_color_lbl setEnabled: state];
    [o_threshold_saturation_sld setEnabled: state];
    [o_threshold_saturation_lbl setEnabled: state];
    [o_threshold_similarity_sld setEnabled: state];
    [o_threshold_similarity_lbl setEnabled: state];
}

- (IBAction)thresholdModifierChanged:(id)sender
{
    if( sender == o_threshold_color_fld )
        [self setVideoFilterProperty: "colorthres-color" forFilter: "colorthres" integer: [o_threshold_color_fld intValue]];
    else if( sender == o_threshold_saturation_sld )
        [self setVideoFilterProperty: "colorthres-saturationthres" forFilter: "colorthres" integer: [o_threshold_saturation_sld intValue]];
    else
        [self setVideoFilterProperty: "colorthres-similaritythres" forFilter: "colorthres" integer: [o_threshold_similarity_sld intValue]];
}

- (IBAction)enableSepia:(id)sender
{
    [self setVideoFilter: "sepia" on: [o_sepia_ckb state]];
    [o_sepia_fld setEnabled: [o_sepia_ckb state]];
    [o_sepia_lbl setEnabled: [o_sepia_ckb state]];
}

- (IBAction)sepiaModifierChanged:(id)sender
{
    [self setVideoFilterProperty: "sepia-intensity" forFilter: "sepia" integer: [o_sepia_fld intValue]];
}

- (IBAction)enableNoise:(id)sender
{
    [self setVideoFilter: "noise" on: [o_noise_ckb state]];
}

- (IBAction)enableGradient:(id)sender
{
    BOOL state = [o_gradient_ckb state];
    [self setVideoFilter: "gradient" on: state];
    [o_gradient_mode_pop setEnabled: state];
    [o_gradient_mode_lbl setEnabled: state];
    [o_gradient_color_ckb setEnabled: state];
    [o_gradient_cartoon_ckb setEnabled: state];
}

- (IBAction)gradientModifierChanged:(id)sender
{
    if( sender == o_gradient_mode_pop ) {
        if( [[o_gradient_mode_pop selectedItem] tag] == 3 )
            [self setVideoFilterProperty: "gradient-mode" forFilter: "gradient" string: "hough"];
        else if( [[o_gradient_mode_pop selectedItem] tag] == 2 )
            [self setVideoFilterProperty: "gradient-mode" forFilter: "gradient" string: "edge"];
        else
            [self setVideoFilterProperty: "gradient-mode" forFilter: "gradient" string: "gradient"];
    }
    else if( sender == o_gradient_color_ckb )
        [self setVideoFilterProperty: "gradient-type" forFilter: "gradient" integer: [o_gradient_color_ckb state]];
    else
        [self setVideoFilterProperty: "gradient-cartoon" forFilter: "gradient" boolean: [o_gradient_cartoon_ckb state]];
}

- (IBAction)enableExtract:(id)sender
{
    [self setVideoFilter: "extract" on: [o_extract_ckb state]];
    [o_extract_fld setEnabled: [o_extract_ckb state]];
    [o_extract_lbl setEnabled: [o_extract_ckb state]];
}

- (IBAction)extractModifierChanged:(id)sender
{
    [self setVideoFilterProperty: "extract-component" forFilter: "extract" integer: [o_extract_fld intValue]];  
}

- (IBAction)enableInvert:(id)sender
{
    [self setVideoFilter: "invert" on: [o_invert_ckb state]];
}

- (IBAction)enablePosterize:(id)sender
{
    [self setVideoFilter: "posterize" on: [o_posterize_ckb state]];
    [o_posterize_fld setEnabled: [o_posterize_ckb state]];
    [o_posterize_lbl setEnabled: [o_posterize_ckb state]];
}

- (IBAction)posterizeModifierChanged:(id)sender
{
    [self setVideoFilterProperty: "posterize-level" forFilter: "posterize" integer: [o_extract_fld intValue]];
}

- (IBAction)enableBlur:(id)sender
{
    [self setVideoFilter: "motionblur" on: [o_blur_ckb state]];
    [o_blur_sld setEnabled: [o_blur_ckb state]];
    [o_blur_lbl setEnabled: [o_blur_ckb state]];
}

- (IBAction)blurModifierChanged:(id)sender
{
    [self setVideoFilterProperty: "blur-factor" forFilter: "motionblur" integer: [o_blur_sld intValue]];
}

- (IBAction)enableMotionDetect:(id)sender
{
    [self setVideoFilter: "motiondetect" on: [o_motiondetect_ckb state]];
}

- (IBAction)enableWaterEffect:(id)sender
{
    [self setVideoFilter: "ripple" on: [o_watereffect_ckb state]];
}

- (IBAction)enableWaves:(id)sender
{
    [self setVideoFilter: "wave" on: [o_waves_ckb state]];
}

- (IBAction)enablePsychedelic:(id)sender
{
    [self setVideoFilter: "psychedelic" on: [o_psychedelic_ckb state]];
}


#pragma mark -
#pragma mark Miscellaneous
- (IBAction)enableClone:(id)sender
{
    msg_Dbg( p_intf, "not yet implemented" );
}

- (IBAction)cloneModifierChanged:(id)sender
{
    msg_Dbg( p_intf, "not yet implemented" );
}

- (IBAction)enableAddText:(id)sender
{
    msg_Dbg( p_intf, "not yet implemented" );
}

- (IBAction)addTextModifierChanged:(id)sender
{
    msg_Dbg( p_intf, "not yet implemented" );
}

- (IBAction)enableAddLogo:(id)sender
{
    msg_Dbg( p_intf, "not yet implemented" );
}

- (IBAction)addLogoModifierChanged:(id)sender
{
    msg_Dbg( p_intf, "not yet implemented" );
}

@end