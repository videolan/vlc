/*****************************************************************************
 * VLCAudioEffectsWindowController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2004-2015 VLC authors and VideoLAN
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

#import "VLCMain.h"
#import "VLCAudioEffectsWindowController.h"
#import "../../audio_filter/equalizer_presets.h"
#import "CompatibilityFixes.h"
#import "VLCPopupPanelController.h"
#import "VLCTextfieldPanelController.h"

#import <vlc_common.h>

#import <math.h>

@interface VLCAudioEffectsWindowController ()
{
    NSInteger i_old_profile_index;
}
- (void)resetProfileSelector;
- (void)updatePresetSelector;
- (void)setBandSliderValuesForPreset:(NSInteger)presetID;
@end

#pragma mark -
#pragma mark Initialization

@implementation VLCAudioEffectsWindowController

+ (void)initialize
{
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
        [workTitles addObject:toNSStr(preset_list_text[i])];
        [workNames addObject:toNSStr(preset_list[i])];
    }

    NSString *defaultProfile = [NSString stringWithFormat:@"ZmxhdA==;;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%i",
                                .0,25.,100.,-11.,8.,2.5,7.,.85,1.,.4,.5,.5,2.,0];

    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:[NSArray arrayWithArray:workValues], @"EQValues", [NSArray arrayWithArray:workPreamp], @"EQPreampValues", [NSArray arrayWithArray:workTitles], @"EQTitles", [NSArray arrayWithArray:workNames], @"EQNames", [NSArray arrayWithObject:defaultProfile], @"AudioEffectProfiles", [NSArray arrayWithObject:_NS("Default")], @"AudioEffectProfileNames", nil];
    [defaults registerDefaults:appDefaults];
}

- (id)init
{
    self = [super initWithWindowNibName:@"AudioEffects"];
    if (self) {
        i_old_profile_index = -1;

        self.popupPanel = [[VLCPopupPanelController alloc] init];
        self.textfieldPanel = [[VLCTextfieldPanelController alloc] init];
    }

    return self;
}

- (void)windowDidLoad
{
    /* setup the user's language */
    /* Equalizer */
    [_equalizerEnableCheckbox setTitle:_NS("Enable")];
    [_equalizerTwoPassCheckbox setTitle:_NS("2 Pass")];
    [_equalizerPreampLabel setStringValue:_NS("Preamp")];

    /* Compressor */
    [_compressorEnableCheckbox setTitle:_NS("Enable dynamic range compressor")];
    [_compressorResetButton setTitle:_NS("Reset")];
    [_compressorBand1Label setStringValue:_NS("RMS/peak")];;
    [_compressorBand2Label setStringValue:_NS("Attack")];
    [_compressorBand3Label setStringValue:_NS("Release")];
    [_compressorBand4Label setStringValue:_NS("Threshold")];
    [_compressorBand5Label setStringValue:_NS("Ratio")];
    [_compressorBand6Label setStringValue:_NS("Knee radius")];
    [_compressorBand7Label setStringValue:_NS("Makeup gain")];


    /* Spatializer */
    [_spatializerEnableCheckbox setTitle:_NS("Enable Spatializer")];
    [_spatializerResetButton setTitle:_NS("Reset")];
    [_spatializerBand1Label setStringValue:_NS("Size")];
    [_spatializerBand2Label setStringValue:_NS("Width")];
    [_spatializerBand3Label setStringValue:_NS("Wet")];
    [_spatializerBand4Label setStringValue:_NS("Dry")];
    [_spatializerBand5Label setStringValue:_NS("Damp")];

    /* Filter */
    [_filterHeadPhoneCheckbox setTitle:_NS("Headphone virtualization")];
    [_filterNormLevelCheckbox setTitle:_NS("Volume normalization")];
    [_filterNormLevelLabel setStringValue:_NS("Maximum level")];
    [_filterKaraokeCheckbox setTitle:_NS("Karaoke")];

    /* generic */
    [[_tabView tabViewItemAtIndex:[_tabView indexOfTabViewItemWithIdentifier:@"equalizer"]] setLabel:_NS("Equalizer")];
    [[_tabView tabViewItemAtIndex:[_tabView indexOfTabViewItemWithIdentifier:@"compressor"]] setLabel:_NS("Compressor")];
    [[_tabView tabViewItemAtIndex:[_tabView indexOfTabViewItemWithIdentifier:@"spatializer"]] setLabel:_NS("Spatializer")];
    [[_tabView tabViewItemAtIndex:[_tabView indexOfTabViewItemWithIdentifier:@"filter"]] setLabel:_NS("Filter")];
    [self.window setTitle:_NS("Audio Effects")];
    [self.window setExcludedFromWindowsMenu:YES];
    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [self equalizerUpdated];
    [self resetCompressor];
    [self resetSpatializer];
    [self resetAudioFilters];
    [self resetProfileSelector];
}

#pragma mark -
#pragma mark internal functions

- (void)setAudioFilter: (char *)psz_name on:(BOOL)b_on
{
    audio_output_t *p_aout = getAout();
    intf_thread_t *p_intf = getIntf();
    playlist_EnableAudioFilter(pl_Get(p_intf), psz_name, b_on);

    if (p_aout) {
        char *psz_new = var_GetNonEmptyString(p_aout, "audio-filter");
        config_PutPsz(p_intf, "audio-filter", psz_new);
        free(psz_new);

        vlc_object_release(p_aout);
    }
}

- (void)resetProfileSelector
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [_profilePopup removeAllItems];

    NSArray *profileNames = [defaults objectForKey:@"AudioEffectProfileNames"];
    [_profilePopup addItemsWithTitles:profileNames];

    [[_profilePopup menu] addItem:[NSMenuItem separatorItem]];
    [_profilePopup addItemWithTitle:_NS("Duplicate current profile...")];
    [[_profilePopup lastItem] setTarget: self];
    [[_profilePopup lastItem] setAction: @selector(addAudioEffectsProfile:)];

    if ([profileNames count] > 1) {
        [_profilePopup addItemWithTitle:_NS("Organize Profiles...")];
        [[_profilePopup lastItem] setTarget: self];
        [[_profilePopup lastItem] setAction: @selector(removeAudioEffectsProfile:)];
    }

    [_profilePopup selectItemAtIndex:[defaults integerForKey:@"AudioEffectSelectedProfile"]];
    [self profileSelectorAction:self];
}

#pragma mark -
#pragma mark generic code
- (void)updateCocoaWindowLevel:(NSInteger)i_level
{
    if (self.isWindowLoaded && [self.window isVisible] && [self.window level] != i_level)
        [self.window setLevel: i_level];
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

- (NSString *)generateProfileString
{
    intf_thread_t *p_intf = getIntf();
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
    [defaults synchronize];
}

- (IBAction)profileSelectorAction:(id)sender
{
    intf_thread_t *p_intf = getIntf();
    [self saveCurrentProfile];
    i_old_profile_index = [_profilePopup indexOfSelectedItem];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSInteger selectedProfile = [_profilePopup indexOfSelectedItem];
    if (selectedProfile < 0)
        return;

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
    NSArray *items = [[[defaults objectForKey:@"AudioEffectProfiles"] objectAtIndex:(NSUInteger) selectedProfile] componentsSeparatedByString:@";"];

    /* eq preset */
    vlc_object_t *p_object = VLC_OBJECT(getAout());
    if (p_object == NULL)
        p_object = vlc_object_hold(pl_Get(p_intf));
    var_SetString(p_object, "equalizer-preset", [B64DecNSStr([items firstObject]) UTF8String]);
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
                playlist_EnableAudioFilter(p_playlist, [[tempArray objectAtIndex:x] UTF8String], true);
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
    BOOL b_equalizerEnabled = [tempString rangeOfString:@"equalizer"].location != NSNotFound;
    [_equalizerView enableSubviews:b_equalizerEnabled];
    [_equalizerEnableCheckbox setState:(b_equalizerEnabled ? NSOnState : NSOffState)];

    [_equalizerTwoPassCheckbox setState:[[items objectAtIndex:15] intValue]];
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

- (void)addAudioEffectsProfile:(id)sender
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
            [_profilePopup selectItemAtIndex:[defaults integerForKey:@"AudioEffectSelectedProfile"]];
            return;
        }

        NSArray *profileNames = [defaults objectForKey:@"AudioEffectProfileNames"];

        // duplicate names are not allowed in the popup control
        if ([resultingText length] == 0 || [profileNames containsObject:resultingText]) {
            [_profilePopup selectItemAtIndex:[defaults integerForKey:@"AudioEffectSelectedProfile"]];

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

        NSString *newProfile = [_self generateProfileString];

        /* add string to user defaults as well as a label */
        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfiles"]];
        [workArray addObject:newProfile];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfiles"];
        [defaults setInteger:[workArray count] - 1 forKey:@"AudioEffectSelectedProfile"];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfileNames"]];
        [workArray addObject:resultingText];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfileNames"];

        /* save defaults */
        [defaults synchronize];
        [_self resetProfileSelector];

    }];
}

- (void)removeAudioEffectsProfile:(id)sender
{
    /* show panel */
    [_popupPanel setTitleString:_NS("Remove a preset")];
    [_popupPanel setSubTitleString:_NS("Select the preset you would like to remove:")];
    [_popupPanel setOkButtonString:_NS("Remove")];
    [_popupPanel setCancelButtonString:_NS("Cancel")];
    [_popupPanel setPopupButtonContent:[[NSUserDefaults standardUserDefaults] objectForKey:@"AudioEffectProfileNames"]];

    __weak typeof(self) _self = self;
    [_popupPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSInteger selectedIndex) {

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        if (returnCode != NSOKButton) {
            [_profilePopup selectItemAtIndex:[defaults integerForKey:@"AudioEffectSelectedProfile"]];
            return;
        }

        /* remove selected profile from settings */
        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfiles"]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfiles"];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfileNames"]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfileNames"];

        if (i_old_profile_index >= selectedIndex)
            [defaults setInteger:i_old_profile_index - 1 forKey:@"AudioEffectSelectedProfile"];

        /* save defaults */
        [defaults synchronize];
        [_self resetProfileSelector];
    }];
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

- (void)updatePresetSelector
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *presets = [defaults objectForKey:@"EQNames"];

    [_equalizerPresetsPopup removeAllItems];
    [_equalizerPresetsPopup addItemsWithTitles:[[NSUserDefaults standardUserDefaults] objectForKey:@"EQTitles"]];
    [[_equalizerPresetsPopup menu] addItem:[NSMenuItem separatorItem]];
    [_equalizerPresetsPopup addItemWithTitle:_NS("Add new Preset...")];
    [[_equalizerPresetsPopup lastItem] setTarget: self];
    [[_equalizerPresetsPopup lastItem] setAction: @selector(addPresetAction:)];

    if ([presets count] > 1) {
        [_equalizerPresetsPopup addItemWithTitle:_NS("Organize Presets...")];
        [[_equalizerPresetsPopup lastItem] setTarget: self];
        [[_equalizerPresetsPopup lastItem] setAction: @selector(deletePresetAction:)];
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
        currentPresetIndex = [presets indexOfObject:currentPreset];

        if (currentPresetIndex == NSNotFound)
            currentPresetIndex = [presets count] - 1;
    }    

    [_equalizerPresetsPopup selectItemAtIndex:currentPresetIndex];
    [self equalizerChangePreset:_equalizerPresetsPopup];

    
    [_equalizerPreampSlider setFloatValue:[[[defaults objectForKey:@"EQPreampValues"] objectAtIndex:currentPresetIndex] floatValue]];
    [self setBandSliderValuesForPreset:currentPresetIndex];
}

- (void)equalizerUpdated
{
    intf_thread_t *p_intf = getIntf();
    float fPreamp = config_GetFloat(p_intf, "equalizer-preamp");
    bool b_2p = (BOOL)config_GetInt(p_intf, "equalizer-2pass");
    bool bEnabled = GetEqualizerStatus(p_intf, (char *)"equalizer");

    /* Setup sliders */
    [self updatePresetSelector];

    /* Set the the checkboxes */
    [_equalizerView enableSubviews: bEnabled];
    [_equalizerEnableCheckbox setState: bEnabled];
    [_equalizerTwoPassCheckbox setState: b_2p];
}

- (id)sliderByIndex:(int)index
{
    switch(index) {
        case 0 : return _equalizerBand1Slider;
        case 1 : return _equalizerBand2Slider;
        case 2 : return _equalizerBand3Slider;
        case 3 : return _equalizerBand4Slider;
        case 4 : return _equalizerBand5Slider;
        case 5 : return _equalizerBand6Slider;
        case 6 : return _equalizerBand7Slider;
        case 7 : return _equalizerBand8Slider;
        case 8 : return _equalizerBand9Slider;
        case 9 : return _equalizerBand10Slider;
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
            [_equalizerBand1Slider floatValue],
            [_equalizerBand2Slider floatValue],
            [_equalizerBand3Slider floatValue],
            [_equalizerBand4Slider floatValue],
            [_equalizerBand5Slider floatValue],
            [_equalizerBand6Slider floatValue],
            [_equalizerBand7Slider floatValue],
            [_equalizerBand8Slider floatValue],
            [_equalizerBand9Slider floatValue],
            [_equalizerBand10Slider floatValue]];
}

- (void)setValue:(float)value forSlider:(int)index
{
    id slider = [self sliderByIndex:index];

    if (slider != nil)
        [slider setFloatValue:value];
}

- (IBAction)equalizerEnable:(id)sender
{
    [_equalizerView enableSubviews:[sender state]];
    [self setAudioFilter: "equalizer" on:[sender state]];
}

- (IBAction)equalizerBandSliderUpdated:(id)sender
{
    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetString(p_aout, "equalizer-bands", [[self generatePresetString] UTF8String]);
        vlc_object_release(p_aout);
    }

    /* save changed to config */
    config_PutPsz(getIntf(), "equalizer-bands", [[self generatePresetString] UTF8String]);

}

- (IBAction)equalizerChangePreset:(id)sender
{
    intf_thread_t *p_intf = getIntf();
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

    [_equalizerPreampSlider setFloatValue: [preamp floatValue]];
    [self setBandSliderValuesForPreset:numberOfChosenPreset];

    /* save changed to config */
    config_PutPsz(p_intf, "equalizer-bands", [preset UTF8String]);
    config_PutFloat(p_intf, "equalizer-preamp", [preamp floatValue]);
    config_PutPsz(p_intf, "equalizer-preset", [[[defaults objectForKey:@"EQNames"] objectAtIndex:numberOfChosenPreset] UTF8String]);
}

- (IBAction)equalizerPreAmpSliderUpdated:(id)sender
{
    float fPreamp = [sender floatValue] ;

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetFloat(p_aout, "equalizer-preamp", fPreamp);
        vlc_object_release(p_aout);
    }
    
    /* save changed to config */
    config_PutFloat(getIntf(), "equalizer-preamp", fPreamp);
}

- (IBAction)equalizerTwoPass:(id)sender
{
    bool b_2p = [sender state] ? true : false;

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetBool(p_aout, "equalizer-2pass", b_2p);
        vlc_object_release(p_aout);
    }

    /* save changed to config */
    config_PutInt(getIntf(), "equalizer-2pass", (int)b_2p);
}

- (IBAction)addPresetAction:(id)sender
{
    /* show panel */
    [_textfieldPanel setTitleString:_NS("Save current selection as new preset")];
    [_textfieldPanel setSubTitleString:_NS("Enter a name for the new preset:")];
    [_textfieldPanel setCancelButtonString:_NS("Cancel")];
    [_textfieldPanel setOkButtonString:_NS("Save")];

    __weak typeof(self) _self = self;
    [_textfieldPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSString *resultingText) {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

        // EQ settings
        if (returnCode != NSOKButton || [resultingText length] == 0)
            return;

        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQValues"]];
        [workArray addObject:[self generatePresetString]];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQValues"];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQTitles"]];
        [workArray addObject:resultingText];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQTitles"];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQPreampValues"]];
        [workArray addObject:[NSString stringWithFormat:@"%.1f", [_equalizerPreampSlider floatValue]]];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQPreampValues"];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQNames"]];
        [workArray addObject:[resultingText decomposedStringWithCanonicalMapping]];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQNames"];
        [defaults synchronize];

        /* update VLC internals */
        audio_output_t *p_aout = getAout();
        if (p_aout) {
            var_SetString(p_aout, "equalizer-preset", [[resultingText decomposedStringWithCanonicalMapping] UTF8String]);
            vlc_object_release(p_aout);
        }

        config_PutPsz(getIntf(), "equalizer-preset", [[resultingText decomposedStringWithCanonicalMapping] UTF8String]);

        /* update UI */
        [_self updatePresetSelector];
    }];
}

- (IBAction)deletePresetAction:(id)sender
{
    [_popupPanel setTitleString:_NS("Remove a preset")];
    [_popupPanel setSubTitleString:_NS("Select the preset you would like to remove:")];
    [_popupPanel setOkButtonString:_NS("Remove")];
    [_popupPanel setCancelButtonString:_NS("Cancel")];
    [_popupPanel setPopupButtonContent:[[NSUserDefaults standardUserDefaults] objectForKey:@"EQTitles"]];

    __weak typeof(self) _self = self;
    [_popupPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSInteger selectedIndex) {

        if (returnCode != NSOKButton)
            return;

        /* remove requested profile from the arrays */
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQValues"]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQValues"];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQTitles"]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQTitles"];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQPreampValues"]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQPreampValues"];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQNames"]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQNames"];
        [defaults synchronize];

        /* update UI */
        [_self updatePresetSelector];
    }];
}

#pragma mark -
#pragma mark Compressor
- (void)resetCompressor
{
    intf_thread_t *p_intf = getIntf();
    BOOL bEnable_compressor = NO;
    char *psz_afilters;
    psz_afilters = config_GetPsz(p_intf, "audio-filter");
    if (psz_afilters) {
        bEnable_compressor = strstr(psz_afilters, "compressor") != NULL;
        [_compressorEnableCheckbox setState: (NSInteger)strstr(psz_afilters, "compressor") ];
        free(psz_afilters);
    }

    [_compressorView enableSubviews:bEnable_compressor];
    [_compressorEnableCheckbox setState:(bEnable_compressor ? NSOnState : NSOffState)];

    [_compressorBand1Slider setFloatValue: config_GetFloat(p_intf, "compressor-rms-peak")];
    [_compressorBand1TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [_compressorBand1Slider floatValue]]];
    [_compressorBand2Slider setFloatValue: config_GetFloat(p_intf, "compressor-attack")];
    [_compressorBand2TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f ms", [_compressorBand2Slider floatValue]]];
    [_compressorBand3Slider setFloatValue: config_GetFloat(p_intf, "compressor-release")];
    [_compressorBand3TextField setStringValue:[NSString localizedStringWithFormat:@"%3.1f ms", [_compressorBand3Slider floatValue]]];
    [_compressorBand4Slider setFloatValue: config_GetFloat(p_intf, "compressor-threshold")];
    [_compressorBand4TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f dB", [_compressorBand4Slider floatValue]]];
    [_compressorBand5Slider setFloatValue: config_GetFloat(p_intf, "compressor-ratio")];
    [_compressorBand5TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f:1", [_compressorBand5Slider floatValue]]];
    [_compressorBand6Slider setFloatValue: config_GetFloat(p_intf, "compressor-knee")];
    [_compressorBand6TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [_compressorBand6Slider floatValue]]];
    [_compressorBand7Slider setFloatValue: config_GetFloat(p_intf, "compressor-makeup-gain")];
    [_compressorBand7TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [_compressorBand7Slider floatValue]]];
}

- (IBAction)resetCompressorValues:(id)sender
{
    intf_thread_t *p_intf = getIntf();
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

- (IBAction)compressorEnable:(id)sender
{
    [_compressorView enableSubviews:[sender state]];
    [self setAudioFilter:"compressor" on:[sender state]];
}

- (IBAction)compressorSliderUpdated:(id)sender
{
    audio_output_t *p_aout = getAout();
    char *value;
    if (sender == _compressorBand1Slider)
        value = "compressor-rms-peak";
    else if (sender == _compressorBand2Slider)
        value = "compressor-attack";
    else if (sender == _compressorBand3Slider)
        value = "compressor-release";
    else if (sender == _compressorBand4Slider)
        value = "compressor-threshold";
    else if (sender == _compressorBand5Slider)
        value = "compressor-ratio";
    else if (sender == _compressorBand6Slider)
        value = "compressor-knee";
    else if (sender == _compressorBand7Slider)
        value = "compressor-makeup-gain";

    if (p_aout) {
        var_SetFloat(p_aout, value, [sender floatValue]);
        vlc_object_release(p_aout);
    }
    config_PutFloat(getIntf(), value, [sender floatValue]);

    if (sender == _compressorBand1Slider)
        [_compressorBand1TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if (sender == _compressorBand2Slider)
        [_compressorBand2TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f ms", [sender floatValue]]];
    else if (sender == _compressorBand3Slider)
        [_compressorBand3TextField setStringValue:[NSString localizedStringWithFormat:@"%3.1f ms", [sender floatValue]]];
    else if (sender == _compressorBand4Slider)
        [_compressorBand4TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f dB", [sender floatValue]]];
    else if (sender == _compressorBand5Slider)
        [_compressorBand5TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f:1", [sender floatValue]]];
    else if (sender == _compressorBand6Slider)
        [_compressorBand6TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [sender floatValue]]];
    else if (sender == _compressorBand7Slider)
        [_compressorBand7TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [sender floatValue]]];
}

#pragma mark -
#pragma mark Spatializer
- (void)resetSpatializer
{
    intf_thread_t *p_intf = getIntf();
    BOOL bEnable_spatializer = NO;
    char *psz_afilters;
    psz_afilters = config_GetPsz(p_intf, "audio-filter");
    if (psz_afilters) {
        bEnable_spatializer = strstr(psz_afilters, "spatializer") != NULL;
        free(psz_afilters);
    }

    [_spatializerView enableSubviews:bEnable_spatializer];
    [_spatializerEnableCheckbox setState:(bEnable_spatializer ? NSOnState : NSOffState)];


#define setSlider(bandsld, bandfld, var) \
[bandsld setFloatValue: config_GetFloat(p_intf, var) * 10.]; \
[bandfld setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [bandsld floatValue]]]

    setSlider(_spatializerBand1Slider, _spatializerBand1TextField, "spatializer-roomsize");
    setSlider(_spatializerBand2Slider, _spatializerBand2TextField, "spatializer-width");
    setSlider(_spatializerBand3Slider, _spatializerBand3TextField, "spatializer-wet");
    setSlider(_spatializerBand4Slider, _spatializerBand4TextField, "spatializer-dry");
    setSlider(_spatializerBand5Slider, _spatializerBand5TextField, "spatializer-damp");

#undef setSlider
}

- (IBAction)resetSpatializerValues:(id)sender
{
    intf_thread_t *p_intf = getIntf();
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

- (IBAction)spatializerEnable:(id)sender
{
    [_spatializerView enableSubviews:[sender state]];
    [self setAudioFilter:"spatializer" on:[sender state]];
}

- (IBAction)spatializerSliderUpdated:(id)sender
{
    audio_output_t *p_aout = getAout();
    char *value = NULL;
    if (sender == _spatializerBand1Slider)
        value = "spatializer-roomsize";
    else if (sender == _spatializerBand2Slider)
        value = "spatializer-width";
    else if (sender == _spatializerBand3Slider)
        value = "spatializer-wet";
    else if (sender == _spatializerBand4Slider)
        value = "spatializer-dry";
    else if (sender == _spatializerBand5Slider)
        value = "spatializer-damp";

    if (p_aout) {
        var_SetFloat(p_aout, value, [sender floatValue] / 10.);
        vlc_object_release(p_aout);
    }
    config_PutFloat(getIntf(), value, [sender floatValue] / 10.);

    if (sender == _spatializerBand1Slider)
        [_spatializerBand1TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if (sender == _spatializerBand2Slider)
        [_spatializerBand2TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if (sender == _spatializerBand3Slider)
        [_spatializerBand3TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if (sender == _spatializerBand4Slider)
        [_spatializerBand4TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
    else if (sender == _spatializerBand5Slider)
        [_spatializerBand5TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [sender floatValue]]];
}

#pragma mark -
#pragma mark Filter
- (void)resetAudioFilters
{
    intf_thread_t *p_intf = getIntf();
    BOOL bEnable_normvol = NO;
    char *psz_afilters;
    psz_afilters = config_GetPsz(p_intf, "audio-filter");
    if (psz_afilters) {
        [_filterHeadPhoneCheckbox setState: (NSInteger)strstr(psz_afilters, "headphone") ];
        [_filterKaraokeCheckbox setState: (NSInteger)strstr(psz_afilters, "karaoke") ];
        bEnable_normvol = strstr(psz_afilters, "normvol") != NULL;
        free(psz_afilters);
    } else {
        [_filterHeadPhoneCheckbox setState: NSOffState];
        [_filterKaraokeCheckbox setState: NSOffState];
    }

    [_filterNormLevelSlider setEnabled:bEnable_normvol];
    [_filterNormLevelLabel setEnabled:bEnable_normvol];
    [_filterNormLevelCheckbox setState:(bEnable_normvol ? NSOnState : NSOffState)];

    [_filterNormLevelSlider setFloatValue: config_GetFloat(p_intf, "norm-max-level")];
}

- (IBAction)filterEnableHeadPhoneVirt:(id)sender
{
    [self setAudioFilter: "headphone" on:[sender state]];
}

- (IBAction)filterEnableVolumeNorm:(id)sender
{
    [_filterNormLevelSlider setEnabled:[sender state]];
    [_filterNormLevelLabel setEnabled:[sender state]];
    [self setAudioFilter: "normvol" on:[sender state]];
}

- (IBAction)filterVolumeNormSliderUpdated:(id)sender
{
    audio_output_t *p_aout = getAout();

    if (p_aout) {
        var_SetFloat(p_aout, "norm-max-level", [_filterNormLevelSlider floatValue]);
        vlc_object_release(p_aout);
    }

    config_PutFloat(getIntf(), "norm-max-level", [_filterNormLevelSlider floatValue]);
}

- (IBAction)filterEnableKaraoke:(id)sender
{
    [self setAudioFilter: "karaoke" on:[sender state]];
}

@end
