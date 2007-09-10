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
#import <vlc/vlc.h>

@interface VLCExtended : NSObject
{
    /* views and window */
    IBOutlet id o_adjustImg_view;
    IBOutlet id o_audioFlts_view;
    IBOutlet id o_videoFilters_view;
    IBOutlet id o_extended_window;
 
    /* window content */
    IBOutlet id o_expBtn_adjustImage;
    IBOutlet id o_expBtn_audioFlts;
    IBOutlet id o_expBtn_videoFlts;
    IBOutlet id o_lbl_audioFlts;
    IBOutlet id o_lbl_videoFlts;
    IBOutlet id o_lbl_adjustImage;
    IBOutlet id o_lbl_video;
    IBOutlet id o_lbl_audio;
    IBOutlet id o_box_vidFlts;
    IBOutlet id o_box_audFlts;
    IBOutlet id o_box_adjImg;
 
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
    BOOL o_adjImg_expanded;
    BOOL o_audFlts_expanded;
    BOOL o_vidFlts_expanded;
 
    BOOL o_config_changed;
}

- (IBAction)enableAdjustImage:(id)sender;
- (IBAction)restoreDefaultsForAdjustImage:(id)sender;
- (IBAction)sliderActionAdjustImage:(id)sender;
- (IBAction)opaqueSliderAction:(id)sender;
- (IBAction)enableHeadphoneVirtualizer:(id)sender;
- (IBAction)sliderActionMaximumAudioLevel:(id)sender;
- (IBAction)enableVolumeNormalization:(id)sender;
- (IBAction)expandAdjustImage:(id)sender;
- (IBAction)expandAudioFilters:(id)sender;
- (IBAction)expandVideoFilters:(id)sender;
- (IBAction)videoFilterAction:(id)sender;
- (IBAction)moreInfoVideoFilters:(id)sender;

+ (VLCExtended *)sharedInstance;
- (BOOL)getConfigChanged;
- (void)collapsAll;

- (void)showPanel;
- (void)initStrings;
- (void)changeVoutFiltersString: (char *)psz_name onOrOff: (vlc_bool_t )b_add;
- (void)changeVideoFiltersString: (char *)psz_name onOrOff: (vlc_bool_t )b_add;
- (void)changeAFiltersString: (char *)psz_name onOrOff: (vlc_bool_t )b_add;
- (void)savePrefs;
@end
