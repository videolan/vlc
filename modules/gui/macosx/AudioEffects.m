/*****************************************************************************
 * AudioEffects.m: MacOS X interface module
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

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import "intf.h"
#import "AudioEffects.h"
#import "../../audio_filter/equalizer_presets.h"
#import "CompatibilityFixes.h"
#import "SharedDialogs.h"

#import <vlc_common.h>

#import <math.h>

@interface VLCAudioEffects (Internal)
- (void)resetProfileSelector;
- (void)updatePresetSelector;
- (void)setBandSliderValuesForPreset:(NSInteger)presetID;
@end

#pragma mark -
#pragma mark Initialization

@implementation VLCAudioEffects
static VLCAudioEffects *_o_sharedInstance = nil;

+ (VLCAudioEffects *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

+ (void)initialize{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    NSString *workString;
    NSMutableArray *workValues = [[NSMutableArray alloc] initWithCapacity:NB_PRESETS];
    NSMutableArray *workPreamp = [[NSMutableArray alloc] initWithCapacity:NB_PRESETS];
    NSMutableArray *workTitles = [[NSMutableArray alloc] initWithCapacity:NB_PRESETS];
    NSMutableArray *workNames = [[NSMutableArray alloc] initWithCapacity:NB_PRESETS];

    for (int i = 0 ; i < NB_PRESETS ; i++) {
        workString = [NSString stringWithFormat:@"%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f",
                      eqz_preset_10b[i].f_amp[0],
                      eqz_preset_10b[i].f_amp[1],
                      eqz_preset_10b[i].f_amp[2],
                      eqz_preset_10b[i].f_amp[3],
                      eqz_preset_10b[i].f_amp[4],
                      eqz_preset_10b[i].f_amp[5],
                      eqz_preset_10b[i].f_amp[6],
                      eqz_preset_10b[i].f_amp[7],
                      eqz_preset_10b[i].f_amp[8],
                      eqz_preset_10b[i].f_amp[9]];
        [workValues addObject:workString];
        [workPreamp addObject:[NSString stringWithFormat:@"%1.f", eqz_preset_10b[i].f_preamp]];
        [workTitles addObject:[NSString stringWithUTF8String:preset_list_text[i]]];
        [workNames addObject:[NSString stringWithUTF8String:preset_list[i]]];
    }

    NSString *defaultProfile = [NSString stringWithFormat:@"ZmxhdA==;;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%i",
                                .0,25.,100.,-11.,8.,2.5,7.,.85,1.,.4,.5,.5,2.,0];

    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:[NSArray arrayWithArray:workValues], @"EQValues", [NSArray arrayWithArray:workPreamp], @"EQPreampValues", [NSArray arrayWithArray:workTitles], @"EQTitles", [NSArray arrayWithArray:workNames], @"EQNames", [NSArray arrayWithObject:defaultProfile], @"AudioEffectProfiles", [NSArray arrayWithObject:_NS("Default")], @"AudioEffectProfileNames", nil];
    [defaults registerDefaults:appDefaults];

    [workValues release];
    [workPreamp release];
    [workTitles release];
    [workNames release];
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
    /* setup the user's language */
    /* Equalizer */
    [o_eq_enable_ckb setTitle:_NS("Enable")];
    [o_eq_twopass_ckb setTitle:_NS("2 Pass")];
    [o_eq_preamp_lbl setStringValue:_NS("Preamp")];

    /* Compressor */
    [o_comp_enable_ckb setTitle:_NS("Enable dynamic range compressor")];
    [o_comp_reset_btn setTitle:_NS("Reset")];
    [o_comp_band1_lbl setStringValue:_NS("RMS/peak")];;
    [o_comp_band2_lbl setStringValue:_NS("Attack")];
    [o_comp_band3_lbl setStringValue:_NS("Release")];
    [o_comp_band4_lbl setStringValue:_NS("Threshold")];
    [o_comp_band5_lbl setStringValue:_NS("Ratio")];
    [o_comp_band6_lbl setStringValue:_NS("Knee radius")];
    [o_comp_band7_lbl setStringValue:_NS("Makeup gain")];

    /* Spatializer */
    [o_spat_enable_ckb setTitle:_NS("Enable Spatializer")];
    [o_spat_reset_btn setTitle:_NS("Reset")];
    [o_spat_band1_lbl setStringValue:_NS("Size")];
    [o_spat_band2_lbl setStringValue:_NS("Width")];
    [o_spat_band3_lbl setStringValue:_NS("Wet")];
    [o_spat_band4_lbl setStringValue:_NS("Dry")];
    [o_spat_band5_lbl setStringValue:_NS("Damp")];

    /* Filter */
    [o_filter_headPhone_ckb setTitle:_NS("Headphone virtualization")];
    [o_filter_normLevel_ckb setTitle:_NS("Volume normalization")];
    [o_filter_normLevel_lbl setStringValue:_NS("Maximum level")];
    [o_filter_karaoke_ckb setTitle:_NS("Karaoke")];

    /* generic */
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"equalizer"]] setLabel:_NS("Equalizer")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"compressor"]] setLabel:_NS("Compressor")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"spatializer"]] setLabel:_NS("Spatializer")];
    [[o_tableView tabViewItemAtIndex:[o_tableView indexOfTabViewItemWithIdentifier:@"filter"]] setLabel:_NS("Filter")];
    [o_window setTitle:_NS("Audio Effects")];
    [o_window setExcludedFromWindowsMenu:YES];
    if (!OSX_SNOW_LEOPARD)
        [o_window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [self setupEqualizer];
    [self resetCompressor];
    [self resetSpatializer];
    [self resetAudioFilters];
    [self resetProfileSelector];
}

#pragma mark -
#pragma mark internal functions

- (void)setAudioFilter: (char *)psz_name on:(BOOL)b_on
{
    char *psz_tmp;
    audio_output_t *p_aout = getAout();
    if (p_aout)
        psz_tmp = var_GetNonEmptyString(p_aout, "audio-filter");
    else
        psz_tmp = config_GetPsz(p_intf, "audio-filter");

    if (b_on) {
        if (!psz_tmp)
            config_PutPsz(p_intf, "audio-filter", psz_name);
        else if (strstr(psz_tmp, psz_name) == NULL) {
            psz_tmp = (char *)[[NSString stringWithFormat: @"%s:%s", psz_tmp, psz_name] UTF8String];
            config_PutPsz(p_intf, "audio-filter", psz_tmp);
        }
    } else {
        if (psz_tmp) {
            psz_tmp = (char *)[[[NSString stringWithUTF8String:psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:[NSString stringWithFormat:@":%s",psz_name]]] UTF8String];
            psz_tmp = (char *)[[[NSString stringWithUTF8String:psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:[NSString stringWithFormat:@"%s:",psz_name]]] UTF8String];
            psz_tmp = (char *)[[[NSString stringWithUTF8String:psz_tmp] stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:[NSString stringWithUTF8String:psz_name]]] UTF8String];
            config_PutPsz(p_intf, "audio-filter", psz_tmp);
        }
    }

    if (p_aout) {
        playlist_EnableAudioFilter(pl_Get(p_intf), psz_name, b_on);
        vlc_object_release(p_aout);
    }
}

- (void)resetProfileSelector
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [o_profile_pop removeAllItems];

    NSArray *profileNames = [defaults objectForKey:@"AudioEffectProfileNames"];
    [o_profile_pop addItemsWithTitles:profileNames];

    [[o_profile_pop menu] addItem:[NSMenuItem separatorItem]];
    [o_profile_pop addItemWithTitle:_NS("Duplicate current profile...")];
    [[o_profile_pop lastItem] setTarget: self];
    [[o_profile_pop lastItem] setAction: @selector(addAudioEffectsProfile:)];

    if ([profileNames count] > 1) {
        [o_profile_pop addItemWithTitle:_NS("Organize Profiles...")];
        [[o_profile_pop lastItem] setTarget: self];
        [[o_profile_pop lastItem] setAction: @selector(removeAudioEffectsProfile:)];
    }

    [o_profile_pop selectItemAtIndex:[defaults integerForKey:@"AudioEffectSelectedProfile"]];
    [self profileSelectorAction:self];
}

#pragma mark -
#pragma mark generic code
- (void)updateCocoaWindowLevel:(NSInteger)i_level
{
    if (o_window && [o_window isVisible] && [o_window level] != i_level)
        [o_window setLevel: i_level];
}

- (IBAction)toggleWindow:(id)sender
{
    if ([o_window isVisible])
        [o_window orderOut:sender];
    else {
        [o_window setLevel: [[[VLCMain sharedInstance] voutController] currentWindowLevel]];
        [o_window makeKeyAndOrderFront:sender];
    }
}

- (NSString *)generateProfileString
{
    vlc_object_t *p_object = VLC_OBJECT(getAout());
    if (p_object == NULL)
        p_object = vlc_object_hold(pl_Get(p_intf));

    NSString *o_str = [NSString stringWithFormat:@"%@;%@;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%lli",
                       B64EncAndFree(var_GetNonEmptyString(p_object, "equalizer-preset")),
                       B64EncAndFree(config_GetPsz(p_intf, "audio-filter")),
                       config_GetFloat(p_intf, "compressor-rms-peak"),
                       config_GetFloat(p_intf, "compressor-attack"),
                       config_GetFloat(p_intf, "compressor-release"),
                       config_GetFloat(p_intf, "compressor-threshold"),
                       config_GetFloat(p_intf, "compressor-ratio"),
                       config_GetFloat(p_intf, "compressor-knee"),
                       config_GetFloat(p_intf, "compressor-makeup-gain"),
                       config_GetFloat(p_intf, "spatializer-roomsize"),
                       config_GetFloat(p_intf, "spatializer-width"),
                       config_GetFloat(p_intf, "spatializer-wet"),
                       config_GetFloat(p_intf, "spatializer-dry"),
                       config_GetFloat(p_intf, "spatializer-damp"),
                       config_GetFloat(p_intf, "norm-max-level"),
                       config_GetInt(p_intf,"equalizer-2pass")];

    vlc_object_release(p_object);
    return o_str;
}

- (void)saveCurrentProfile
{
    if (i_old_profile_index == -1)
        return;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    /* fetch all the current settings in a uniform string */
    NSString *newProfile = [self generateProfileString];

    NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfiles"]];
    if (i_old_profile_index >= [workArray count])
        return;

    [workArray replaceObjectAtIndex:i_old_profile_index withObject:newProfile];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfiles"];
    [workArray release];
    [defaults synchronize];
}

- (IBAction)profileSelectorAction:(id)sender
{
    [self saveCurrentProfile];
    i_old_profile_index = [o_profile_pop indexOfSelectedItem];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSUInteger selectedProfile = [o_profile_pop indexOfSelectedItem];

    audio_output_t *p_aout = getAout();
    playlist_t *p_playlist = pl_Get(p_intf);

    if (p_aout) {
        /* disable existing filters */
        playlist_EnableAudioFilter(p_playlist, "equalizer", false);
        playlist_EnableAudioFilter(p_playlist, "compressor", false);
        playlist_EnableAudioFilter(p_playlist, "spatializer", false);
        playlist_EnableAudioFilter(p_playlist, "compressor", false);
        playlist_EnableAudioFilter(p_playlist, "headphone", false);
        playlist_EnableAudioFilter(p_playlist, "normvol", false);
        playlist_EnableAudioFilter(p_playlist, "karaoke", false);
    }

    /* fetch preset */
    NSArray *items = [[[defaults objectForKey:@"AudioEffectProfiles"] objectAtIndex:selectedProfile] componentsSeparatedByString:@";"];

    /* eq preset */
    vlc_object_t *p_object = VLC_OBJECT(getAout());
    if (p_object == NULL)
        p_object = vlc_object_hold(pl_Get(p_intf));
    var_SetString(p_object, "equalizer-preset", [B64DecNSStr([items objectAtIndex:0]) UTF8String]);
    vlc_object_release(p_object);

    /* filter handling */
    NSString *tempString = B64DecNSStr([items objectAtIndex:1]);
    NSArray *tempArray;
    NSUInteger count;
    /* enable the new filters, if we have an aout */
    if (p_aout) {
        if ([tempString length] > 0) {
            tempArray = [tempString componentsSeparatedByString:@":"];
            count = [tempArray count];
            for (NSUInteger x = 0; x < count; x++)
                playlist_EnableAudioFilter(p_playlist, (char *)[[tempArray objectAtIndex:x] UTF8String], true);
        }
    }
    config_PutPsz(p_intf,"audio-filter",[tempString UTF8String]);

    /* values */
    config_PutFloat(p_intf, "compressor-rms-peak",[[items objectAtIndex:2] floatValue]);
    config_PutFloat(p_intf, "compressor-attack",[[items objectAtIndex:3] floatValue]);
    config_PutFloat(p_intf, "compressor-release",[[items objectAtIndex:4] floatValue]);
    config_PutFloat(p_intf, "compressor-threshold",[[items objectAtIndex:5] floatValue]);
    config_PutFloat(p_intf, "compressor-ratio",[[items objectAtIndex:6] floatValue]);
    config_PutFloat(p_intf, "compressor-knee",[[items objectAtIndex:7] floatValue]);
    config_PutFloat(p_intf, "compressor-makeup-gain",[[items objectAtIndex:8] floatValue]);
    config_PutFloat(p_intf, "spatializer-roomsize",[[items objectAtIndex:9] floatValue]);
    config_PutFloat(p_intf, "spatializer-width",[[items objectAtIndex:10] floatValue]);
    config_PutFloat(p_intf, "spatializer-wet",[[items objectAtIndex:11] floatValue]);
    config_PutFloat(p_intf, "spatializer-dry",[[items objectAtIndex:12] floatValue]);
    config_PutFloat(p_intf, "spatializer-damp",[[items objectAtIndex:13] floatValue]);
    config_PutFloat(p_intf, "norm-max-level",[[items objectAtIndex:14] floatValue]);
    config_PutInt(p_intf, "equalizer-2pass",[[items objectAtIndex:15] intValue]);

    /* set values on-the-fly if we have an aout */
    if (p_aout) {
        var_SetFloat(p_aout, "compressor-rms-peak", [[items objectAtIndex:2] floatValue]);
        var_SetFloat(p_aout, "compressor-attack", [[items objectAtIndex:3] floatValue]);
        var_SetFloat(p_aout, "compressor-release", [[items objectAtIndex:4] floatValue]);
        var_SetFloat(p_aout, "compressor-threshold", [[items objectAtIndex:5] floatValue]);
        var_SetFloat(p_aout, "compressor-ratio", [[items objectAtIndex:6] floatValue]);
        var_SetFloat(p_aout, "compressor-knee", [[items objectAtIndex:7] floatValue]);
        var_SetFloat(p_aout, "compressor-makeup-gain", [[items objectAtIndex:8] floatValue]);
        var_SetFloat(p_aout, "spatializer-roomsize", [[items objectAtIndex:9] floatValue]);
        var_SetFloat(p_aout, "spatializer-width", [[items objectAtIndex:10] floatValue]);
        var_SetFloat(p_aout, "spatializer-wet", [[items objectAtIndex:11] floatValue]);
        var_SetFloat(p_aout, "spatializer-dry", [[items objectAtIndex:12] floatValue]);
        var_SetFloat(p_aout, "spatializer-damp", [[items objectAtIndex:13] floatValue]);
        var_SetFloat(p_aout, "norm-max-level", [[items objectAtIndex:14] floatValue]);
        var_SetBool(p_aout, "equalizer-2pass", (BOOL)[[items objectAtIndex:15] intValue]);
    }

    /* update UI */
    if ([tempString rangeOfString:@"equalizer"].location == NSNotFound)
        [o_eq_enable_ckb setState:NSOffState];
    else
        [o_eq_enable_ckb setState:NSOnState];
    [o_eq_twopass_ckb setState:[[items objectAtIndex:15] intValue]];
    [self resetCompressor];
    [self resetSpatializer];
    [self resetAudioFilters];
    [self updatePresetSelector];

    /* store current profile selection */
    [defaults setInteger:selectedProfile forKey:@"AudioEffectSelectedProfile"];
    [defaults synchronize];

    if (p_aout)
        vlc_object_release(p_aout);
}

- (IBAction)addAudioEffectsProfile:(id)sender
{
    /* show panel */
    VLCEnterTextPanel *panel = [VLCEnterTextPanel sharedInstance];
    [panel setTitle: _NS("Duplicate current profile for a new profile")];
    [panel setSubTitle: _NS("Enter a name for the new profile:")];
    [panel setCancelButtonLabel: _NS("Cancel")];
    [panel setOKButtonLabel: _NS("Save")];
    [panel setTarget:self];
    b_genericAudioProfileInInteraction = YES;

    [panel runModalForWindow:o_window];
}

- (IBAction)removeAudioEffectsProfile:(id)sender
{
    /* show panel */
    VLCSelectItemInPopupPanel *panel = [VLCSelectItemInPopupPanel sharedInstance];
    [panel setTitle:_NS("Remove a preset")];
    [panel setSubTitle:_NS("Select the preset you would like to remove:")];
    [panel setOKButtonLabel:_NS("Remove")];
    [panel setCancelButtonLabel:_NS("Cancel")];
    [panel setPopupButtonContent:[[NSUserDefaults standardUserDefaults] objectForKey:@"AudioEffectProfileNames"]];
    [panel setTarget:self];
    b_genericAudioProfileInInteraction = YES;

    [panel runModalForWindow:o_window];
}

#pragma mark -
#pragma mark Equalizer
static bool GetEqualizerStatus(intf_thread_t *p_custom_intf,
                               char *psz_name)
{
    char *psz_parser, *psz_string = NULL;
    audio_output_t *p_aout = getAout();
    if (!p_aout)
        return false;

    psz_string = config_GetPsz(p_custom_intf, "audio-filter");

    if (!psz_string)
        psz_string = var_GetNonEmptyString(p_aout, "audio-filter");

    vlc_object_release(p_aout);

    if (!psz_string)
        return false;

    psz_parser = strstr(psz_string, psz_name);

    free(psz_string);

    if (psz_parser)
        return true;
    else
        return false;
}

- (void)setupEqualizer
{
    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_Create(p_aout, "equalizer-preset", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
        var_Create(p_aout, "equalizer-preamp", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
        var_Create(p_aout, "equalizer-bands", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
        vlc_object_release(p_aout);
    }

    [self equalizerUpdated];
}

- (void)updatePresetSelector
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *presets = [defaults objectForKey:@"EQNames"];

    [o_eq_presets_popup removeAllItems];
    [o_eq_presets_popup addItemsWithTitles:[[NSUserDefaults standardUserDefaults] objectForKey:@"EQTitles"]];
    [[o_eq_presets_popup menu] addItem:[NSMenuItem separatorItem]];
    [o_eq_presets_popup addItemWithTitle:_NS("Add new Preset...")];
    [[o_eq_presets_popup lastItem] setTarget: self];
    [[o_eq_presets_popup lastItem] setAction: @selector(addPresetAction:)];

    if ([presets count] > 1) {
        [o_eq_presets_popup addItemWithTitle:_NS("Organize Presets...")];
        [[o_eq_presets_popup lastItem] setTarget: self];
        [[o_eq_presets_popup lastItem] setAction: @selector(deletePresetAction:)];
    }

    audio_output_t *p_aout = getAout();

    NSString *currentPreset = nil;
    if (p_aout) {
        char *psz_preset_string = var_GetNonEmptyString(p_aout, "equalizer-preset");
        currentPreset = toNSStr(psz_preset_string);
        free(psz_preset_string);
        vlc_object_release(p_aout);
    }

    NSUInteger currentPresetIndex = 0;
    if (currentPreset && [currentPreset length] > 0) {
        currentPresetIndex = [presets indexOfObjectPassingTest:^(id obj, NSUInteger idx, BOOL *stop) {
            return [obj isEqualToString:currentPreset];
        }];

        if (currentPresetIndex == NSNotFound)
            currentPresetIndex = [presets count] - 1;
    }    

    [o_eq_presets_popup selectItemAtIndex:currentPresetIndex];
    [self eq_changePreset: o_eq_presets_popup];

    
    [o_eq_preamp_sld setFloatValue:[[[defaults objectForKey:@"EQPreampValues"] objectAtIndex:currentPresetIndex] floatValue]];
    [self setBandSliderValuesForPreset:currentPresetIndex];
}

- (void)equalizerUpdated
{
    float f_preamp = config_GetFloat(p_intf, "equalizer-preamp");
    bool b_2p = (BOOL)config_GetInt(p_intf, "equalizer-2pass");
    bool b_enabled = GetEqualizerStatus(p_intf, (char *)"equalizer");

    /* Setup sliders */
    [self updatePresetSelector];

    /* Set the the checkboxes */
    [o_eq_enable_ckb setState: b_enabled];
    [o_eq_twopass_ckb setState: b_2p];
}

- (id)sliderByIndex:(int)index
{
    switch(index) {
        case 0 : return o_eq_band1_sld;
        case 1 : return o_eq_band2_sld;
        case 2 : return o_eq_band3_sld;
        case 3 : return o_eq_band4_sld;
        case 4 : return o_eq_band5_sld;
        case 5 : return o_eq_band6_sld;
        case 6 : return o_eq_band7_sld;
        case 7 : return o_eq_band8_sld;
        case 8 : return o_eq_band9_sld;
        case 9 : return o_eq_band10_sld;
        default : return nil;
    }
}

- (void)setBandSliderValuesForPreset:(NSInteger)presetID
{
    NSString *preset = [[[NSUserDefaults standardUserDefaults] objectForKey:@"EQValues"] objectAtIndex:presetID];
    NSArray *values = [preset componentsSeparatedByString:@" "];
    NSUInteger count = [values count];
    for (NSUInteger x = 0; x < count; x++)
        [self setValue:[[values objectAtIndex:x] floatValue] forSlider:x];
}

- (NSString *)generatePresetString
{
    return [NSString stringWithFormat:@"%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f",
            [o_eq_band1_sld floatValue],
            [o_eq_band2_sld floatValue],
            [o_eq_band3_sld floatValue],
            [o_eq_band4_sld floatValue],
            [o_eq_band5_sld floatValue],
            [o_eq_band6_sld floatValue],
            [o_eq_band7_sld floatValue],
            [o_eq_band8_sld floatValue],
            [o_eq_band9_sld floatValue],
            [o_eq_band10_sld floatValue]];
}

- (void)setValue:(float)value forSlider:(int)index
{
    id slider = [self sliderByIndex:index];

    if (slider != nil)
        [slider setFloatValue:value];
}

- (IBAction)eq_enable:(id)sender
{
    [self setAudioFilter: "equalizer" on:[sender state]];
}

- (IBAction)eq_bandSliderUpdated:(id)sender
{
    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetString(p_aout, "equalizer-bands", [[self generatePresetString] UTF8String]);
        vlc_object_release(p_aout);
    }

    /* save changed to config */
    config_PutPsz(p_intf, "equalizer-bands", [[self generatePresetString] UTF8String]);

}

- (IBAction)eq_changePreset:(id)sender
{
    NSInteger numberOfChosenPreset = [sender indexOfSelectedItem];
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    NSString *preset = [[defaults objectForKey:@"EQValues"] objectAtIndex:numberOfChosenPreset];
    NSString *preamp = [[defaults objectForKey:@"EQPreampValues"] objectAtIndex:numberOfChosenPreset];

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetString(p_aout, "equalizer-bands", [preset UTF8String]);
        var_SetFloat(p_aout, "equalizer-preamp", [preamp floatValue]);
        var_SetString(p_aout, "equalizer-preset" , [[[defaults objectForKey:@"EQNames"] objectAtIndex:numberOfChosenPreset] UTF8String]);
        vlc_object_release(p_aout);
    }

    [o_eq_preamp_sld setFloatValue: [preamp floatValue]];
    [self setBandSliderValuesForPreset:numberOfChosenPreset];

    /* save changed to config */
    config_PutPsz(p_intf, "equalizer-bands", [preset UTF8String]);
    config_PutFloat(p_intf, "equalizer-preamp", [preamp floatValue]);
    config_PutPsz(p_intf, "equalizer-preset", [[[defaults objectForKey:@"EQNames"] objectAtIndex:numberOfChosenPreset] UTF8String]);

}

- (IBAction)eq_preampSliderUpdated:(id)sender
{
    float f_preamp = [sender floatValue] ;

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetFloat(p_aout, "equalizer-preamp", f_preamp);
        vlc_object_release(p_aout);
    }
    
    /* save changed to config */
    config_PutFloat(p_intf, "equalizer-preamp", f_preamp);

}
- (IBAction)eq_twopass:(id)sender
{
    bool b_2p = [sender state] ? true : false;

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetBool(p_aout, "equalizer-2pass", b_2p);
        vlc_object_release(p_aout);
    }

    /* save changed to config */
    config_PutInt(p_intf, "equalizer-2pass", (int)b_2p);

}

- (IBAction)addPresetAction:(id)sender
{
    /* show panel */
    VLCEnterTextPanel *panel = [VLCEnterTextPanel sharedInstance];
    [panel setTitle: _NS("Save current selection as new preset")];
    [panel setSubTitle: _NS("Enter a name for the new preset:")];
    [panel setCancelButtonLabel: _NS("Cancel")];
    [panel setOKButtonLabel: _NS("Save")];
    [panel setTarget:self];
    b_genericAudioProfileInInteraction = NO;

    [panel runModalForWindow:o_window];
}

- (void)panel:(VLCEnterTextPanel *)panel returnValue:(NSUInteger)value text:(NSString *)text
{

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    // EQ settings
    if (!b_genericAudioProfileInInteraction) {
        if (value == NSOKButton && [text length] > 0) {
            NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQValues"]];
            [workArray addObject:[self generatePresetString]];
            [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQValues"];
            [workArray release];
            workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQTitles"]];
            [workArray addObject:text];
            [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQTitles"];
            [workArray release];
            workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQPreampValues"]];
            [workArray addObject:[NSString stringWithFormat:@"%.1f", [o_eq_preamp_sld floatValue]]];
            [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQPreampValues"];
            [workArray release];
            workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQNames"]];
            [workArray addObject:[text decomposedStringWithCanonicalMapping]];
            [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQNames"];
            [workArray release];
            [defaults synchronize];

            /* update VLC internals */
            audio_output_t *p_aout = getAout();
            if (p_aout) {
                var_SetString(p_aout, "equalizer-preset", [[text decomposedStringWithCanonicalMapping] UTF8String]);
                vlc_object_release(p_aout);
            }

            config_PutPsz(p_intf, "equalizer-preset", [[text decomposedStringWithCanonicalMapping] UTF8String]);


            /* update UI */
            [self updatePresetSelector];
        }

    // profile settings
    } else {

        if (value != NSOKButton) {
            [o_profile_pop selectItemAtIndex:[defaults integerForKey:@"AudioEffectSelectedProfile"]];
            return;
        }

        NSArray *profileNames = [defaults objectForKey:@"AudioEffectProfileNames"];

        // duplicate names are not allowed in the popup control
        if ([text length] == 0 || [profileNames containsObject:text]) {
            [o_profile_pop selectItemAtIndex:[defaults integerForKey:@"AudioEffectSelectedProfile"]];

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
        
        NSString *newProfile = [self generateProfileString];

        /* add string to user defaults as well as a label */
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfiles"]];
        [workArray addObject:newProfile];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfiles"];
        [defaults setInteger:[workArray count] - 1 forKey:@"AudioEffectSelectedProfile"];
        [workArray release];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfileNames"]];
        [workArray addObject:text];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfileNames"];
        [workArray release];

        /* save defaults */
        [defaults synchronize];
        [self resetProfileSelector];
    }
}

- (IBAction)deletePresetAction:(id)sender
{
    VLCSelectItemInPopupPanel *panel = [VLCSelectItemInPopupPanel sharedInstance];
    [panel setTitle:_NS("Remove a preset")];
    [panel setSubTitle:_NS("Select the preset you would like to remove:")];
    [panel setOKButtonLabel:_NS("Remove")];
    [panel setCancelButtonLabel:_NS("Cancel")];
    [panel setPopupButtonContent:[[NSUserDefaults standardUserDefaults] objectForKey:@"EQTitles"]];
    [panel setTarget:self];
    b_genericAudioProfileInInteraction = NO;

    [panel runModalForWindow:o_window];
}

- (void)panel:(VLCSelectItemInPopupPanel *)panel returnValue:(NSUInteger)value item:(NSUInteger)item
{
    if (value == NSOKButton) {
        if (!b_genericAudioProfileInInteraction) {
            /* remove requested profile from the arrays */
            NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
            NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQValues"]];
            [workArray removeObjectAtIndex:item];
            [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQValues"];
            [workArray release];
            workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQTitles"]];
            [workArray removeObjectAtIndex:item];
            [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQTitles"];
            [workArray release];
            workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQPreampValues"]];
            [workArray removeObjectAtIndex:item];
            [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQPreampValues"];
            [workArray release];
            workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQNames"]];
            [workArray removeObjectAtIndex:item];
            [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQNames"];
            [workArray release];
            [defaults synchronize];

            /* update UI */
            [self updatePresetSelector];
        } else {
            /* remove selected profile from settings */
            NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
            NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfiles"]];
            [workArray removeObjectAtIndex:item];
            [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfiles"];
            [workArray release];
            workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfileNames"]];
            [workArray removeObjectAtIndex:item];
            [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfileNames"];
            [workArray release];

            if (i_old_profile_index >= item)
                [defaults setInteger:i_old_profile_index - 1 forKey:@"AudioEffectSelectedProfile"];

            /* save defaults */
            [defaults synchronize];
            [self resetProfileSelector];
        }
    }
}

#pragma mark -
#pragma mark Compressor
- (void)resetCompressor
{
    char *psz_afilters;
    psz_afilters = config_GetPsz(p_intf, "audio-filter");
    if (psz_afilters) {
        [o_comp_enable_ckb setState: (NSInteger)strstr(psz_afilters, "compressor") ];
        free(psz_afilters);
    }
    else
        [o_comp_enable_ckb setState: NSOffState];

    [o_comp_band1_sld setFloatValue: config_GetFloat(p_intf, "compressor-rms-peak")];
    [o_comp_band1_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [o_comp_band1_sld floatValue]]];
    [o_comp_band2_sld setFloatValue: config_GetFloat(p_intf, "compressor-attack")];
    [o_comp_band2_fld setStringValue:[NSString localizedStringWithFormat:@"%2.1f ms", [o_comp_band2_sld floatValue]]];
    [o_comp_band3_sld setFloatValue: config_GetFloat(p_intf, "compressor-release")];
    [o_comp_band3_fld setStringValue:[NSString localizedStringWithFormat:@"%3.1f ms", [o_comp_band3_sld floatValue]]];
    [o_comp_band4_sld setFloatValue: config_GetFloat(p_intf, "compressor-threshold")];
    [o_comp_band4_fld setStringValue:[NSString localizedStringWithFormat:@"%2.1f dB", [o_comp_band4_sld floatValue]]];
    [o_comp_band5_sld setFloatValue: config_GetFloat(p_intf, "compressor-ratio")];
    [o_comp_band5_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f:1", [o_comp_band5_sld floatValue]]];
    [o_comp_band6_sld setFloatValue: config_GetFloat(p_intf, "compressor-knee")];
    [o_comp_band6_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [o_comp_band6_sld floatValue]]];
    [o_comp_band7_sld setFloatValue: config_GetFloat(p_intf, "compressor-makeup-gain")];
    [o_comp_band7_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [o_comp_band7_sld floatValue]]];
}

- (IBAction)resetCompressorValues:(id)sender
{
    config_PutFloat(p_intf, "compressor-rms-peak", 0.000000);
    config_PutFloat(p_intf, "compressor-attack", 25.000000);
    config_PutFloat(p_intf, "compressor-release", 100.000000);
    config_PutFloat(p_intf, "compressor-threshold", -11.000000);
    config_PutFloat(p_intf, "compressor-ratio", 8.000000);
    config_PutFloat(p_intf, "compressor-knee", 2.500000);
    config_PutFloat(p_intf, "compressor-makeup-gain", 7.000000);

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetFloat(p_aout, "compressor-rms-peak", 0.000000);
        var_SetFloat(p_aout, "compressor-attack", 25.000000);
        var_SetFloat(p_aout, "compressor-release", 100.000000);
        var_SetFloat(p_aout, "compressor-threshold", -11.000000);
        var_SetFloat(p_aout, "compressor-ratio", 8.000000);
        var_SetFloat(p_aout, "compressor-knee", 2.500000);
        var_SetFloat(p_aout, "compressor-makeup-gain", 7.000000);
        vlc_object_release(p_aout);
    }
    [self resetCompressor];
}

- (IBAction)comp_enable:(id)sender
{
    [self setAudioFilter:"compressor" on:[sender state]];
}

- (IBAction)comp_sliderUpdated:(id)sender
{
    audio_output_t *p_aout = getAout();
    char *value;
    if (sender == o_comp_band1_sld)
        value = "compressor-rms-peak";
    else if (sender == o_comp_band2_sld)
        value = "compressor-attack";
    else if (sender == o_comp_band3_sld)
        value = "compressor-release";
    else if (sender == o_comp_band4_sld)
        value = "compressor-threshold";
    else if (sender == o_comp_band5_sld)
        value = "compressor-ratio";
    else if (sender == o_comp_band6_sld)
        value = "compressor-knee";
    else if (sender == o_comp_band7_sld)
        value = "compressor-makeup-gain";

    if (p_aout) {
        var_SetFloat(p_aout, value, [sender floatValue]);
        vlc_object_release(p_aout);
    }
    config_PutFloat(p_intf, value, [sender floatValue]);

    if (sender == o_comp_band1_sld)
        [o_comp_band1_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if (sender == o_comp_band2_sld)
        [o_comp_band2_fld setStringValue:[NSString localizedStringWithFormat:@"%2.1f ms", [sender floatValue]]];
    else if (sender == o_comp_band3_sld)
        [o_comp_band3_fld setStringValue:[NSString localizedStringWithFormat:@"%3.1f ms", [sender floatValue]]];
    else if (sender == o_comp_band4_sld)
        [o_comp_band4_fld setStringValue:[NSString localizedStringWithFormat:@"%2.1f dB", [sender floatValue]]];
    else if (sender == o_comp_band5_sld)
        [o_comp_band5_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f:1", [sender floatValue]]];
    else if (sender == o_comp_band6_sld)
        [o_comp_band6_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [sender floatValue]]];
    else if (sender == o_comp_band7_sld)
        [o_comp_band7_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [sender floatValue]]];
}

#pragma mark -
#pragma mark Spatializer
- (void)resetSpatializer
{
    char *psz_afilters;
    psz_afilters = config_GetPsz(p_intf, "audio-filter");
    if (psz_afilters) {
        [o_spat_enable_ckb setState: (NSInteger)strstr(psz_afilters, "spatializer") ];
        free(psz_afilters);
    }
    else
        [o_spat_enable_ckb setState: NSOffState];

#define setSlider(bandsld, bandfld, var) \
[bandsld setFloatValue: config_GetFloat(p_intf, var) * 10.]; \
[bandfld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [bandsld floatValue]]]

    setSlider(o_spat_band1_sld, o_spat_band1_fld, "spatializer-roomsize");
    setSlider(o_spat_band2_sld, o_spat_band2_fld, "spatializer-width");
    setSlider(o_spat_band3_sld, o_spat_band3_fld, "spatializer-wet");
    setSlider(o_spat_band4_sld, o_spat_band4_fld, "spatializer-dry");
    setSlider(o_spat_band5_sld, o_spat_band5_fld, "spatializer-damp");

#undef setSlider
}

- (IBAction)resetSpatializerValues:(id)sender
{
    config_PutFloat(p_intf, "spatializer-roomsize", .85);
    config_PutFloat(p_intf, "spatializer-width", 1.);
    config_PutFloat(p_intf, "spatializer-wet", .4);
    config_PutFloat(p_intf, "spatializer-dry", .5);
    config_PutFloat(p_intf, "spatializer-damp", .5);

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetFloat(p_aout, "spatializer-roomsize", .85);
        var_SetFloat(p_aout, "spatializer-width", 1.);
        var_SetFloat(p_aout, "spatializer-wet", .4);
        var_SetFloat(p_aout, "spatializer-dry", .5);
        var_SetFloat(p_aout, "spatializer-damp", .5);
        vlc_object_release(p_aout);
    }
    [self resetSpatializer];
}

- (IBAction)spat_enable:(id)sender
{
    [self setAudioFilter:"spatializer" on:[sender state]];
}

- (IBAction)spat_sliderUpdated:(id)sender
{
    audio_output_t *p_aout = getAout();
    char *value;
    if (sender == o_spat_band1_sld)
        value = "spatializer-roomsize";
    else if (sender == o_spat_band2_sld)
        value = "spatializer-width";
    else if (sender == o_spat_band3_sld)
        value = "spatializer-wet";
    else if (sender == o_spat_band4_sld)
        value = "spatializer-dry";
    else if (sender == o_spat_band5_sld)
        value = "spatializer-damp";

    if (p_aout) {
        var_SetFloat(p_aout, value, [sender floatValue] / 10.);
        vlc_object_release(p_aout);
    }
    config_PutFloat(p_intf, value, [sender floatValue] / 10.);

    if (sender == o_spat_band1_sld)
        [o_spat_band1_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if (sender == o_spat_band2_sld)
        [o_spat_band2_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if (sender == o_spat_band3_sld)
        [o_spat_band3_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if (sender == o_spat_band4_sld)
        [o_spat_band4_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if (sender == o_spat_band5_sld)
        [o_spat_band5_fld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
}

#pragma mark -
#pragma mark Filter
- (void)resetAudioFilters
{
    char *psz_afilters;
    psz_afilters = config_GetPsz(p_intf, "audio-filter");
    if (psz_afilters) {
        [o_filter_headPhone_ckb setState: (NSInteger)strstr(psz_afilters, "headphone") ];
        [o_filter_normLevel_ckb setState: (NSInteger)strstr(psz_afilters, "normvol") ];
        [o_filter_karaoke_ckb setState: (NSInteger)strstr(psz_afilters, "karaoke") ];
        free(psz_afilters);
    } else {
        [o_filter_headPhone_ckb setState: NSOffState];
        [o_filter_normLevel_ckb setState: NSOffState];
        [o_filter_karaoke_ckb setState: NSOffState];
    }
    [o_filter_normLevel_sld setFloatValue: config_GetFloat(p_intf, "norm-max-level")];
}

- (IBAction)filter_enableHeadPhoneVirt:(id)sender
{
    [self setAudioFilter: "headphone" on:[sender state]];
}

- (IBAction)filter_enableVolumeNorm:(id)sender
{
    [self setAudioFilter: "normvol" on:[sender state]];
}

- (IBAction)filter_volNormSliderUpdated:(id)sender
{
    audio_output_t *p_aout = getAout();

    if (p_aout) {
        var_SetFloat(p_aout, "norm-max-level", [o_filter_normLevel_sld floatValue]);
        vlc_object_release(p_aout);
    }

    config_PutFloat(p_intf, "norm-max-level", [o_filter_normLevel_sld floatValue]);
}

- (IBAction)filter_enableKaraoke:(id)sender
{
    [self setAudioFilter: "karaoke" on:[sender state]];
}

@end
