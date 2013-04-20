/*****************************************************************************
 * AudioEffects.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2004-2012 VLC authors and VideoLAN
 * $Id$
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

@interface VLCAudioEffects : NSObject {
    /* generic */
    IBOutlet id o_tableView;
    IBOutlet id o_window;
    intf_thread_t *p_intf;
    IBOutlet id o_profile_pop;
    BOOL b_genericAudioProfileInInteraction;

    /* Equalizer */
    IBOutlet id o_eq_enable_ckb;
    IBOutlet id o_eq_twopass_ckb;
    IBOutlet id o_eq_preamp_lbl;
    IBOutlet id o_eq_presets_popup;
    IBOutlet id o_eq_band1_sld;
    IBOutlet id o_eq_band2_sld;
    IBOutlet id o_eq_band3_sld;
    IBOutlet id o_eq_band4_sld;
    IBOutlet id o_eq_band5_sld;
    IBOutlet id o_eq_band6_sld;
    IBOutlet id o_eq_band7_sld;
    IBOutlet id o_eq_band8_sld;
    IBOutlet id o_eq_band9_sld;
    IBOutlet id o_eq_band10_sld;
    IBOutlet id o_eq_preamp_sld;

    /* Compressor */
    IBOutlet id o_comp_enable_ckb;
    IBOutlet id o_comp_reset_btn;
    IBOutlet id o_comp_band1_sld;
    IBOutlet id o_comp_band1_fld;
    IBOutlet id o_comp_band1_lbl;
    IBOutlet id o_comp_band2_sld;
    IBOutlet id o_comp_band2_fld;
    IBOutlet id o_comp_band2_lbl;
    IBOutlet id o_comp_band3_sld;
    IBOutlet id o_comp_band3_fld;
    IBOutlet id o_comp_band3_lbl;
    IBOutlet id o_comp_band4_sld;
    IBOutlet id o_comp_band4_fld;
    IBOutlet id o_comp_band4_lbl;
    IBOutlet id o_comp_band5_sld;
    IBOutlet id o_comp_band5_fld;
    IBOutlet id o_comp_band5_lbl;
    IBOutlet id o_comp_band6_sld;
    IBOutlet id o_comp_band6_fld;
    IBOutlet id o_comp_band6_lbl;
    IBOutlet id o_comp_band7_sld;
    IBOutlet id o_comp_band7_fld;
    IBOutlet id o_comp_band7_lbl;

    /* Spatializer */
    IBOutlet id o_spat_enable_ckb;
    IBOutlet id o_spat_reset_btn;
    IBOutlet id o_spat_band1_sld;
    IBOutlet id o_spat_band1_fld;
    IBOutlet id o_spat_band1_lbl;
    IBOutlet id o_spat_band2_sld;
    IBOutlet id o_spat_band2_fld;
    IBOutlet id o_spat_band2_lbl;
    IBOutlet id o_spat_band3_sld;
    IBOutlet id o_spat_band3_fld;
    IBOutlet id o_spat_band3_lbl;
    IBOutlet id o_spat_band4_sld;
    IBOutlet id o_spat_band4_fld;
    IBOutlet id o_spat_band4_lbl;
    IBOutlet id o_spat_band5_sld;
    IBOutlet id o_spat_band5_fld;
    IBOutlet id o_spat_band5_lbl;

    /* Filter */
    IBOutlet id o_filter_headPhone_ckb;
    IBOutlet id o_filter_normLevel_ckb;
    IBOutlet id o_filter_normLevel_sld;
    IBOutlet id o_filter_normLevel_lbl;
    IBOutlet id o_filter_karaoke_ckb;

    NSInteger i_old_profile_index;
}

/* generic */
+ (VLCAudioEffects *)sharedInstance;

- (void)updateCocoaWindowLevel:(NSInteger)i_level;
- (IBAction)toggleWindow:(id)sender;
- (void)setAudioFilter: (char *)psz_name on:(BOOL)b_on;
- (IBAction)profileSelectorAction:(id)sender;
- (IBAction)addAudioEffectsProfile:(id)sender;
- (IBAction)removeAudioEffectsProfile:(id)sender;

- (void)saveCurrentProfile;

/* Equalizer */
- (void)setupEqualizer;
- (void)equalizerUpdated;
- (void)setValue:(float)value forSlider:(int)index;
- (IBAction)eq_bandSliderUpdated:(id)sender;
- (IBAction)eq_changePreset:(id)sender;
- (IBAction)eq_enable:(id)sender;
- (IBAction)eq_preampSliderUpdated:(id)sender;
- (IBAction)eq_twopass:(id)sender;

/* Compressor */
- (void)resetCompressor;
- (IBAction)resetCompressorValues:(id)sender;
- (IBAction)comp_enable:(id)sender;
- (IBAction)comp_sliderUpdated:(id)sender;

/* Spatializer */
- (void)resetSpatializer;
- (IBAction)resetSpatializerValues:(id)sender;
- (IBAction)spat_enable:(id)sender;
- (IBAction)spat_sliderUpdated:(id)sender;

/* Filter */
- (void)resetAudioFilters;
- (IBAction)filter_enableHeadPhoneVirt:(id)sender;
- (IBAction)filter_enableVolumeNorm:(id)sender;
- (IBAction)filter_volNormSliderUpdated:(id)sender;
- (IBAction)filter_enableKaraoke:(id)sender;

@end
