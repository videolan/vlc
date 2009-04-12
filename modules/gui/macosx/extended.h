/*****************************************************************************
 * extended.h: MacOS X Extended interface panel
 *****************************************************************************
 * Copyright (C) 2005-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne@videolan.org>
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

/*****************************************************************************
 * VLCExtended
 *****************************************************************************/

#import <Cocoa/Cocoa.h>
#import "intf.h"
#import <vlc_common.h>

@interface VLCExtended : NSObject
{
    /* views and window */
    IBOutlet id o_adjustImg_view;
    IBOutlet id o_audioFlts_view;
    IBOutlet id o_videoFilters_view;
    IBOutlet id o_extended_window;

    /* window content */
    IBOutlet id o_selector_pop;
    IBOutlet id o_top_controls_box;

    /* video filters */
    IBOutlet id o_btn_vidFlts_mrInfo;
    IBOutlet id o_ckb_blur;
    IBOutlet id o_ckb_imgClone;
    IBOutlet id o_ckb_imgCrop;
    IBOutlet id o_ckb_imgInvers;
    IBOutlet id o_ckb_trnsform;
    IBOutlet id o_ckb_intZoom;
    IBOutlet id o_ckb_wave;
    IBOutlet id o_ckb_ripple;
    IBOutlet id o_ckb_psycho;
    IBOutlet id o_ckb_gradient;
    IBOutlet id o_lbl_general;
    IBOutlet id o_lbl_distort;

    /* audio filters */
    IBOutlet id o_ckb_vlme_norm;
    IBOutlet id o_ckb_hdphnVirt;
    IBOutlet id o_lbl_maxLevel;
    IBOutlet id o_sld_maxLevel;

    /* adjust image */
    IBOutlet id o_btn_rstrDefaults;
    IBOutlet id o_ckb_enblAdjustImg;
    IBOutlet id o_lbl_brightness;
    IBOutlet id o_lbl_contrast;
    IBOutlet id o_lbl_gamma;
    IBOutlet id o_lbl_hue;
    IBOutlet id o_lbl_saturation;
    IBOutlet id o_lbl_opaque;
    IBOutlet id o_sld_brightness;
    IBOutlet id o_sld_contrast;
    IBOutlet id o_sld_gamma;
    IBOutlet id o_sld_hue;
    IBOutlet id o_sld_saturation;
    IBOutlet id o_sld_opaque;

    /* global variables */
    NSView * o_currentlyshown_view;
    BOOL o_config_changed;
}

- (IBAction)viewSelectorAction:(id)sender;
- (IBAction)enableAdjustImage:(id)sender;
- (IBAction)restoreDefaultsForAdjustImage:(id)sender;
- (IBAction)sliderActionAdjustImage:(id)sender;
- (IBAction)opaqueSliderAction:(id)sender;
- (IBAction)enableHeadphoneVirtualizer:(id)sender;
- (IBAction)sliderActionMaximumAudioLevel:(id)sender;
- (IBAction)enableVolumeNormalization:(id)sender;
- (IBAction)videoFilterAction:(id)sender;
- (IBAction)moreInfoVideoFilters:(id)sender;

+ (VLCExtended *)sharedInstance;
- (BOOL)configChanged;

- (void)showPanel;
- (void)initStrings;
- (void)changeVoutFiltersString: (char *)psz_name onOrOff: (bool )b_add;
- (void)changeVideoFiltersString: (char *)psz_name onOrOff: (bool )b_add;
- (void)changeAFiltersString: (char *)psz_name onOrOff: (bool )b_add;
- (void)savePrefs;
@end
