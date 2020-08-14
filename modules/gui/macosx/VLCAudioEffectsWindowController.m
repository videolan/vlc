/*****************************************************************************
 * VLCAudioEffectsWindowController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2004-2017 VLC authors and VideoLAN
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

    NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
                                 [NSArray arrayWithArray:workValues], @"EQValues",
                                 [NSArray arrayWithArray:workPreamp], @"EQPreampValues",
                                 [NSArray arrayWithArray:workTitles], @"EQTitles",
                                 [NSArray arrayWithArray:workNames], @"EQNames",
                                 [NSArray arrayWithObject:[VLCAudioEffectsWindowController defaultProfileString]], @"AudioEffectProfiles",
                                 [NSArray arrayWithObject:_NS("Default")], @"AudioEffectProfileNames",
                                  nil];
    [defaults registerDefaults:appDefaults];
}

+ (NSString *)defaultProfileString
{
    return [NSString stringWithFormat:@"ZmxhdA==;;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%i",
            .0,25.,100.,-11.,8.,2.5,7.,.85,1.,.4,.5,.5,2.,0];
}

- (id)init
{
    self = [super initWithWindowNibName:@"AudioEffects"];
    if (self) {
        self.popupPanel = [[VLCPopupPanelController alloc] init];
        self.textfieldPanel = [[VLCTextfieldPanelController alloc] init];

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        if ([defaults boolForKey:@"AudioEffectApplyProfileOnStartup"])
        {
            // This does not reset the UI (which does not exist yet), but it initalizes needed playlist vars
            [self equalizerUpdated];
            [self resetCompressor];
            [self resetSpatializer];
            [self resetAudioFilters];

            [self loadProfile];
        } else {
            [self saveCurrentProfileIndex:0];
        }
    }

    return self;
}

- (NSInteger)getPresetIndexForProfile:(NSInteger)profileIndex
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *profile = [[defaults objectForKey:@"AudioEffectProfiles"] objectAtIndex:profileIndex];
    NSString *presetName = B64DecNSStr([[profile componentsSeparatedByString:@";"] firstObject]);
    return [[defaults objectForKey:@"EQNames"] indexOfObject:presetName];
}

/// Loads values from profile into variables
- (void)loadProfile
{
    intf_thread_t *p_intf = getIntf();
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSInteger profileIndex = [self currentProfileIndex];
    playlist_t *p_playlist = pl_Get(p_intf);

    /* disable existing filters */
    playlist_EnableAudioFilter(p_playlist, "equalizer", false);
    playlist_EnableAudioFilter(p_playlist, "compressor", false);
    playlist_EnableAudioFilter(p_playlist, "spatializer", false);
    playlist_EnableAudioFilter(p_playlist, "compressor", false);
    playlist_EnableAudioFilter(p_playlist, "headphone", false);
    playlist_EnableAudioFilter(p_playlist, "normvol", false);
    playlist_EnableAudioFilter(p_playlist, "karaoke", false);

    /* fetch preset */
    NSString *profileString;
    if (profileIndex == 0)
        profileString = [VLCAudioEffectsWindowController defaultProfileString];
    else
        profileString = [[defaults objectForKey:@"AudioEffectProfiles"] objectAtIndex:profileIndex];

    NSArray *items = [profileString componentsSeparatedByString:@";"];

    /* eq preset */
    char const *psz_eq_preset = [B64DecNSStr([items firstObject]) UTF8String];
    audio_output_t *p_aout = getAout();
    if (p_aout)
        var_SetString(p_aout, "equalizer-preset", psz_eq_preset);
    var_SetString(p_playlist, "equalizer-preset", psz_eq_preset);

    /* filter handling */
    NSString *audioFilters = B64DecNSStr([items objectAtIndex:1]);
    if (p_aout)
        var_SetString(p_aout, "audio-filter", audioFilters.UTF8String);
    var_SetString(p_playlist, "audio-filter", audioFilters.UTF8String);

    NSInteger presetIndex = [self getPresetIndexForProfile:profileIndex];

    /* values */
    var_SetFloat(p_playlist, "compressor-rms-peak",[[items objectAtIndex:2] floatValue]);
    var_SetFloat(p_playlist, "compressor-attack",[[items objectAtIndex:3] floatValue]);
    var_SetFloat(p_playlist, "compressor-release",[[items objectAtIndex:4] floatValue]);
    var_SetFloat(p_playlist, "compressor-threshold",[[items objectAtIndex:5] floatValue]);
    var_SetFloat(p_playlist, "compressor-ratio",[[items objectAtIndex:6] floatValue]);
    var_SetFloat(p_playlist, "compressor-knee",[[items objectAtIndex:7] floatValue]);
    var_SetFloat(p_playlist, "compressor-makeup-gain",[[items objectAtIndex:8] floatValue]);
    var_SetFloat(p_playlist, "spatializer-roomsize",[[items objectAtIndex:9] floatValue]);
    var_SetFloat(p_playlist, "spatializer-width",[[items objectAtIndex:10] floatValue]);
    var_SetFloat(p_playlist, "spatializer-wet",[[items objectAtIndex:11] floatValue]);
    var_SetFloat(p_playlist, "spatializer-dry",[[items objectAtIndex:12] floatValue]);
    var_SetFloat(p_playlist, "spatializer-damp",[[items objectAtIndex:13] floatValue]);
    var_SetFloat(p_playlist, "norm-max-level",[[items objectAtIndex:14] floatValue]);
    var_SetBool(p_playlist, "equalizer-2pass",(BOOL)[[items objectAtIndex:15] intValue]);
    var_SetString(p_playlist, "equalizer-bands", [[[defaults objectForKey:@"EQValues"] objectAtIndex:presetIndex] UTF8String]);
    var_SetFloat(p_playlist, "equalizer-preamp", [[[defaults objectForKey:@"EQPreampValues"] objectAtIndex:presetIndex] floatValue]);
    var_SetString(p_playlist, "equalizer-preset", [[[defaults objectForKey:@"EQNames"] objectAtIndex:presetIndex] UTF8String]);

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
        var_SetString(p_aout, "equalizer-bands", [[[defaults objectForKey:@"EQValues"] objectAtIndex:presetIndex] UTF8String]);
        var_SetFloat(p_aout, "equalizer-preamp", [[[defaults objectForKey:@"EQPreampValues"] objectAtIndex:presetIndex] floatValue]);
        var_SetString(p_aout, "equalizer-preset", [[[defaults objectForKey:@"EQNames"] objectAtIndex:presetIndex] UTF8String]);
    }

    if (p_aout)
        vlc_object_release(p_aout);
}

- (void)windowDidLoad
{
    [_applyProfileCheckbox setState:[[NSUserDefaults standardUserDefaults] boolForKey:@"AudioEffectApplyProfileOnStartup"]];
    [_applyProfileCheckbox setTitle:_NS("Apply profile at next launch")];

    /* setup the user's language */
    /* Equalizer */
    [_equalizerEnableCheckbox setTitle:_NS("Enable")];
    [_equalizerTwoPassCheckbox setTitle:_NS("2 Pass")];
    [_equalizerTwoPassCheckbox setToolTip:_NS("Filter the audio twice. This provides a more "  \
                                              "intense effect.")];
    [_equalizerPreampLabel setStringValue:_NS("Preamp")];
    [_equalizerPreampLabel setToolTip:_NS("Set the global gain in dB (-20 ... 20).")];

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
    [_spatializerBand1Label setToolTip:_NS("Defines the virtual surface of the room" \
                                           " emulated by the filter.")];
    [_spatializerBand2Label setStringValue:_NS("Width")];
    [_spatializerBand2Label setToolTip:_NS("Width of the virtual room")];
    [_spatializerBand3Label setStringValue:_NS("Wet")];
    [_spatializerBand4Label setStringValue:_NS("Dry")];
    [_spatializerBand5Label setStringValue:_NS("Damp")];

    /* Filter */
    [_filterHeadPhoneCheckbox setTitle:_NS("Headphone virtualization")];
    [_filterHeadPhoneCheckbox setToolTip:_NS("This effect gives you the feeling that you are standing in a room " \
                                             "with a complete 7.1 speaker set when using only a headphone, " \
                                             "providing a more realistic sound experience. It should also be " \
                                             "more comfortable and less tiring when listening to music for " \
                                             "long periods of time.\nIt works with any source format from mono " \
                                             "to 7.1.")];
    [_filterNormLevelCheckbox setTitle:_NS("Volume normalization")];
    [_filterNormLevelCheckbox setToolTip:_NS("Volume normalizer")];
    [_filterNormLevelLabel setToolTip:_NS("If the average power over the last N buffers " \
                                          "is higher than this value, the volume will be normalized. " \
                                          "This value is a positive floating point number. A value " \
                                          "between 0.5 and 10 seems sensible.")];
    [_filterNormLevelLabel setStringValue:_NS("Maximum level")];
    [_filterKaraokeCheckbox setTitle:_NS("Karaoke")];
    [_filterKaraokeCheckbox setToolTip:_NS("Simple Karaoke filter")];
    [_filterScaleTempoCheckbox setTitle:_NS("Scaletempo")];
    [_filterScaleTempoCheckbox setToolTip:_NS("Audio tempo scaler synched with rate")];
    [_filterStereoEnhancerCheckbox setTitle:_NS("Stereo Enhancer")];
    [_filterStereoEnhancerCheckbox setToolTip:_NS("This filter enhances the stereo effect by "\
                                                  "suppressing mono (signal common to both channels) "\
                                                  "and by delaying the signal of left into right and vice versa, "\
                                                  "thereby widening the stereo effect.")];

    /* generic */
    [_segmentView setLabel:_NS("Equalizer") forSegment:0];
    [_segmentView setLabel:_NS("Compressor") forSegment:1];
    [_segmentView setLabel:_NS("Spatializer") forSegment:2];
    [_segmentView setLabel:_NS("Filter") forSegment:3];

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


- (void)saveCurrentProfileIndex:(NSInteger)index
{
    [[NSUserDefaults standardUserDefaults] setInteger:index forKey:@"AudioEffectSelectedProfile"];
}

- (NSInteger)currentProfileIndex
{
    return [[NSUserDefaults standardUserDefaults] integerForKey:@"AudioEffectSelectedProfile"];
}

/// Returns the list of profile names (omitting the Default entry)
- (NSArray *)nonDefaultProfileNames
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    NSMutableArray *names = [[defaults stringArrayForKey:@"AudioEffectProfileNames"] mutableCopy];
    [names removeObjectAtIndex:0];
    return [names copy];
}

- (void)setAudioFilter: (char *)psz_name on:(BOOL)b_on
{
    playlist_EnableAudioFilter(pl_Get(getIntf()), psz_name, b_on);
}

- (void)resetProfileSelector
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [_profilePopup removeAllItems];

    // Ignore "Default" index 0 from settings
    [_profilePopup addItemWithTitle:_NS("Default")];

    [_profilePopup addItemsWithTitles:[self nonDefaultProfileNames]];

    [[_profilePopup menu] addItem:[NSMenuItem separatorItem]];
    [_profilePopup addItemWithTitle:_NS("Duplicate current profile...")];
    [[_profilePopup lastItem] setTarget: self];
    [[_profilePopup lastItem] setAction: @selector(addAudioEffectsProfile:)];

    if ([[self nonDefaultProfileNames] count] > 0) {
        [_profilePopup addItemWithTitle:_NS("Organize Profiles...")];
        [[_profilePopup lastItem] setTarget: self];
        [[_profilePopup lastItem] setAction: @selector(removeAudioEffectsProfile:)];
    }

    [_profilePopup selectItemAtIndex:[self currentProfileIndex]];
    // Loading only non-default profiles ensures that vlcrc or command line settings are not overwritten
    if ([self currentProfileIndex] > 0)
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
    playlist_t *p_playlist = pl_Get(getIntf());

    return [NSString stringWithFormat:@"%@;%@;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%i",
                     B64EncAndFree(var_GetNonEmptyString(p_playlist, "equalizer-preset")),
                     B64EncAndFree(var_InheritString(p_playlist, "audio-filter")),
                     var_InheritFloat(p_playlist, "compressor-rms-peak"),
                     var_InheritFloat(p_playlist, "compressor-attack"),
                     var_InheritFloat(p_playlist, "compressor-release"),
                     var_InheritFloat(p_playlist, "compressor-threshold"),
                     var_InheritFloat(p_playlist, "compressor-ratio"),
                     var_InheritFloat(p_playlist, "compressor-knee"),
                     var_InheritFloat(p_playlist, "compressor-makeup-gain"),
                     var_InheritFloat(p_playlist, "spatializer-roomsize"),
                     var_InheritFloat(p_playlist, "spatializer-width"),
                     var_InheritFloat(p_playlist, "spatializer-wet"),
                     var_InheritFloat(p_playlist, "spatializer-dry"),
                     var_InheritFloat(p_playlist, "spatializer-damp"),
                     var_InheritFloat(p_playlist, "norm-max-level"),
                     var_InheritBool(p_playlist,"equalizer-2pass")];
}

- (void)saveCurrentProfile
{
    NSInteger currentProfileIndex = [self currentProfileIndex];
    if (currentProfileIndex == 0)
        return;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    /* fetch all the current settings in a uniform string */
    NSString *newProfile = [self generateProfileString];

    NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfiles"]];
    if (currentProfileIndex >= [workArray count])
        return;

    [workArray replaceObjectAtIndex:currentProfileIndex withObject:newProfile];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfiles"];
    [defaults synchronize];
}

- (void)saveCurrentProfileAtTerminate
{
    if ([self currentProfileIndex] > 0) {
        [self saveCurrentProfile];
        return;
    }

    if (_applyProfileCheckbox.state == NSOffState)
        return;

    playlist_t *p_playlist = pl_Get(getIntf());
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([[self generateProfileString] compare:[VLCAudioEffectsWindowController defaultProfileString]] == NSOrderedSame)
        return;

    // FIXME: Current code does not allow auto save of equalizer profiles, those profiles currently need to be saved
    // individually with the popup menu, before saving the overall profile.
    // Below code for auto duplication is not enough to fix current behaviour and results in too many duplicated profiles
    // For auto-saving of eq profiles, a different stragety needs to be found, involving save also once overall profile changes.
/*
    NSString *newPresetString = [NSString stringWithCString:var_InheritString(p_playlist, "equalizer-bands") encoding:NSASCIIStringEncoding];
    float newPresetPreamp = var_InheritFloat(p_playlist, "equalizer-preamp");

    // TODO: Comparing against profile 0 is mostly useless and looks wrong (profile 0 is flat usually)
    NSInteger defaultPresetIndex = [self getPresetIndexForProfile:0];
    NSString *defaultPresetString = [[defaults objectForKey:@"EQValues"] objectAtIndex:defaultPresetIndex];
    float defaultPresetPreamp = [[[defaults objectForKey:@"EQPreampValues"] objectAtIndex:defaultPresetIndex] floatValue];

    NSMutableArray *workArray;
    int num_custom;

    if ([newPresetString compare:defaultPresetString] != NSOrderedSame ||
        newPresetPreamp != defaultPresetPreamp)
    {
        // preset title
        NSArray<NSString *> *presetTitles = [defaults objectForKey:@"EQTitles"];
        NSString *newPresetTitle;

        num_custom = 0;
        do
            newPresetTitle = [@"Custom" stringByAppendingString:[NSString stringWithFormat:@"%03i",num_custom++]];
        while ([presetTitles containsObject:newPresetTitle]);

        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQTitles"]];
        [workArray addObject:newPresetTitle];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQTitles"];

        // preset name
        NSString *decomposedStringWithCanonicalMapping = [newPresetTitle decomposedStringWithCanonicalMapping];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQNames"]];
        [workArray addObject:decomposedStringWithCanonicalMapping];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQNames"];
        var_SetString(p_playlist, "equalizer-preset", [decomposedStringWithCanonicalMapping UTF8String]);

        // preset bands
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQValues"]];
        [workArray addObject:newPresetString];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQValues"];

        // preset preamp
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"EQPreampValues"]];
        [workArray addObject:[NSString stringWithFormat:@"%.1f", [_equalizerPreampSlider floatValue]]];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQPreampValues"];
    }
*/

    NSMutableArray *workArray;
    /* profile string */
    workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfiles"]];
    [workArray addObject:[self generateProfileString]];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfiles"];

    /* profile name */
    NSArray<NSString *> *profileNames = [defaults objectForKey:@"AudioEffectProfileNames"];
    NSString *newProfileName;

    int num_custom = 0;
    do
        newProfileName = [@"Custom" stringByAppendingString:[NSString stringWithFormat:@"%03i",num_custom++]];
    while ([profileNames containsObject:newProfileName]);

    workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfileNames"]];
    [workArray addObject:newProfileName];
    [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfileNames"];

    [self saveCurrentProfileIndex:([workArray count] - 1)];

    [defaults synchronize];
}

- (IBAction)profileSelectorAction:(id)sender
{
    [self saveCurrentProfile];

    [self saveCurrentProfileIndex:[_profilePopup indexOfSelectedItem]];
    [self loadProfile];

    /* update UI */
    [self equalizerUpdated];
    [self resetCompressor];
    [self resetSpatializer];
    [self resetAudioFilters];
    [self updatePresetSelector];
}

- (void)addAudioEffectsProfile:(id)sender
{
    /* show panel */
    [_textfieldPanel setTitleString:_NS("Duplicate current profile for a new profile")];
    [_textfieldPanel setSubTitleString:_NS("Enter a name for the new profile:")];
    [_textfieldPanel setCancelButtonString:_NS("Cancel")];
    [_textfieldPanel setOkButtonString:_NS("Save")];

    __unsafe_unretained typeof(self) _self = self;
    [_textfieldPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSString *resultingText) {

        NSInteger currentProfileIndex = [_self currentProfileIndex];
        if (returnCode != NSOKButton) {
            [_profilePopup selectItemAtIndex:currentProfileIndex];
            return;
        }

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSArray *profileNames = [defaults objectForKey:@"AudioEffectProfileNames"];

        // duplicate names are not allowed in the popup control
        if ([resultingText length] == 0 || [profileNames containsObject:resultingText]) {
            [_profilePopup selectItemAtIndex:currentProfileIndex];

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

        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfileNames"]];
        [workArray addObject:resultingText];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfileNames"];

        [_self saveCurrentProfileIndex:([workArray count] - 1)];


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
    [_popupPanel setPopupButtonContent:[self nonDefaultProfileNames]];

    __unsafe_unretained typeof(self) _self = self;
    [_popupPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSInteger selectedIndex) {

        NSInteger currentProfileIndex = [_self currentProfileIndex];
        if (returnCode != NSOKButton) {
            [_profilePopup selectItemAtIndex:currentProfileIndex];
            return;
        }

        // Popup panel does not contain the "Default" entry
        selectedIndex++;

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        /* remove selected profile from settings */
        NSMutableArray *workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfiles"]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfiles"];
        workArray = [[NSMutableArray alloc] initWithArray:[defaults objectForKey:@"AudioEffectProfileNames"]];
        [workArray removeObjectAtIndex:selectedIndex];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"AudioEffectProfileNames"];

        if (currentProfileIndex >= selectedIndex)
            [_self saveCurrentProfileIndex:(currentProfileIndex - 1)];

        /* save defaults */
        [defaults synchronize];
        [_self resetProfileSelector];
    }];
}

- (IBAction)applyProfileCheckboxChanged:(id)sender
{
    [[NSUserDefaults standardUserDefaults] setBool:[sender state] forKey:@"AudioEffectApplyProfileOnStartup"];
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

    psz_string = var_InheritString(pl_Get(p_custom_intf), "audio-filter");

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
    playlist_t *p_playlist = pl_Get(p_intf);
    bool b_vlcfreqs = var_InheritBool(p_playlist, "equalizer-vlcfreqs");
    bool b_2p = var_CreateGetBool(p_playlist, "equalizer-2pass");
    bool bEnabled = GetEqualizerStatus(p_intf, (char *)"equalizer");

    /* Setup sliders */
    var_Create(p_playlist, "equalizer-preset",
               VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create(p_playlist, "equalizer-bands",
               VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create(p_playlist, "equalizer-preamp",
               VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    [self updatePresetSelector];

    /* Set the the checkboxes */
    [_equalizerView enableSubviews: bEnabled];
    [_equalizerEnableCheckbox setState: bEnabled];
    [_equalizerTwoPassCheckbox setState: b_2p];

    /* Set the frequency labels */
    if (b_vlcfreqs)
    {
        [_equalizerBand1TextField setStringValue:@"60"];
        [_equalizerBand2TextField setStringValue:@"170"];
        [_equalizerBand3TextField setStringValue:@"310"];
        [_equalizerBand4TextField setStringValue:@"600"];
        [_equalizerBand5TextField setStringValue:@"1K"];
        [_equalizerBand6TextField setStringValue:@"3K"];
        [_equalizerBand7TextField setStringValue:@"6K"];
        [_equalizerBand8TextField setStringValue:@"12K"];
        [_equalizerBand9TextField setStringValue:@"14K"];
        [_equalizerBand10TextField setStringValue:@"16K"];
    }
    else
    {
        [_equalizerBand1TextField setStringValue:@"31"];
        [_equalizerBand2TextField setStringValue:@"63"];
        [_equalizerBand3TextField setStringValue:@"125"];
        [_equalizerBand4TextField setStringValue:@"250"];
        [_equalizerBand5TextField setStringValue:@"500"];
        [_equalizerBand6TextField setStringValue:@"1K"];
        [_equalizerBand7TextField setStringValue:@"2K"];
        [_equalizerBand8TextField setStringValue:@"4K"];
        [_equalizerBand9TextField setStringValue:@"8K"];
        [_equalizerBand10TextField setStringValue:@"16K"];
    }
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
    char const *psz_preset_values = [[self generatePresetString] UTF8String];
    if (p_aout) {
        var_SetString(p_aout, "equalizer-bands", psz_preset_values);
        vlc_object_release(p_aout);
    }
    var_SetString(pl_Get(getIntf()), "equalizer-bands", psz_preset_values);
}

- (IBAction)equalizerChangePreset:(id)sender
{
    NSInteger numberOfChosenPreset = [sender indexOfSelectedItem];
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    char const *psz_eq_bands = [[[defaults objectForKey:@"EQValues"] objectAtIndex:numberOfChosenPreset] UTF8String];
    float f_eq_preamp = [[[defaults objectForKey:@"EQPreampValues"] objectAtIndex:numberOfChosenPreset] floatValue];
    char const *psz_eq_preset = [[[defaults objectForKey:@"EQNames"] objectAtIndex:numberOfChosenPreset] UTF8String];

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetString(p_aout, "equalizer-bands", psz_eq_bands);
        var_SetFloat(p_aout, "equalizer-preamp", f_eq_preamp);
        var_SetString(p_aout, "equalizer-preset" , psz_eq_preset);
        vlc_object_release(p_aout);
    }

    [_equalizerPreampSlider setFloatValue: f_eq_preamp];
    [self setBandSliderValuesForPreset:numberOfChosenPreset];

    var_SetString(pl_Get(getIntf()), "equalizer-bands", psz_eq_bands);
    var_SetFloat(pl_Get(getIntf()), "equalizer-preamp", f_eq_preamp);
    var_SetString(pl_Get(getIntf()), "equalizer-preset", psz_eq_preset);
}

- (IBAction)equalizerPreAmpSliderUpdated:(id)sender
{
    float fPreamp = [sender floatValue] ;

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetFloat(p_aout, "equalizer-preamp", fPreamp);
        vlc_object_release(p_aout);
    }
    var_SetFloat(pl_Get(getIntf()), "equalizer-preamp", fPreamp);
}

- (IBAction)equalizerTwoPass:(id)sender
{
    bool b_2p = [sender state] ? true : false;

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetBool(p_aout, "equalizer-2pass", b_2p);
        vlc_object_release(p_aout);
    }

    var_SetBool(pl_Get(getIntf()), "equalizer-2pass", b_2p);
}

- (IBAction)addPresetAction:(id)sender
{
    /* show panel */
    [_textfieldPanel setTitleString:_NS("Save current selection as new preset")];
    [_textfieldPanel setSubTitleString:_NS("Enter a name for the new preset:")];
    [_textfieldPanel setCancelButtonString:_NS("Cancel")];
    [_textfieldPanel setOkButtonString:_NS("Save")];

    __unsafe_unretained typeof(self) _self = self;
    [_textfieldPanel runModalForWindow:self.window completionHandler:^(NSInteger returnCode, NSString *resultingText) {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

        // EQ settings
        if (returnCode != NSOKButton || [resultingText length] == 0)
            return;

        NSString *decomposedStringWithCanonicalMapping = [resultingText decomposedStringWithCanonicalMapping];
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
        [workArray addObject:decomposedStringWithCanonicalMapping];
        [defaults setObject:[NSArray arrayWithArray:workArray] forKey:@"EQNames"];
        [defaults synchronize];

        /* update VLC internals */
        char const *psz_eq_preset = [decomposedStringWithCanonicalMapping UTF8String];
        audio_output_t *p_aout = getAout();
        if (p_aout) {
            var_SetString(p_aout, "equalizer-preset", psz_eq_preset);
            vlc_object_release(p_aout);
        }

        var_SetString(pl_Get(getIntf()), "equalizer-preset", psz_eq_preset);

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

    __unsafe_unretained typeof(self) _self = self;
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
    playlist_t *p_playlist = pl_Get(p_intf);
    BOOL bEnable_compressor = NO;
    char *psz_afilters;
    psz_afilters = var_InheritString(p_playlist, "audio-filter");
    if (psz_afilters) {
        bEnable_compressor = strstr(psz_afilters, "compressor") != NULL;
        [_compressorEnableCheckbox setState: (NSInteger)strstr(psz_afilters, "compressor") ];
        free(psz_afilters);
    }

    [_compressorView enableSubviews:bEnable_compressor];
    [_compressorEnableCheckbox setState:(bEnable_compressor ? NSOnState : NSOffState)];

    [_compressorBand1Slider setFloatValue: var_CreateGetFloat(p_playlist, "compressor-rms-peak")];
    [_compressorBand1TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", [_compressorBand1Slider floatValue]]];
    [_compressorBand2Slider setFloatValue: var_CreateGetFloat(p_playlist, "compressor-attack")];
    [_compressorBand2TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f ms", [_compressorBand2Slider floatValue]]];
    [_compressorBand3Slider setFloatValue: var_CreateGetFloat(p_playlist, "compressor-release")];
    [_compressorBand3TextField setStringValue:[NSString localizedStringWithFormat:@"%3.1f ms", [_compressorBand3Slider floatValue]]];
    [_compressorBand4Slider setFloatValue: var_CreateGetFloat(p_playlist, "compressor-threshold")];
    [_compressorBand4TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f dB", [_compressorBand4Slider floatValue]]];
    [_compressorBand5Slider setFloatValue: var_CreateGetFloat(p_playlist, "compressor-ratio")];
    [_compressorBand5TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f:1", [_compressorBand5Slider floatValue]]];
    [_compressorBand6Slider setFloatValue: var_CreateGetFloat(p_playlist, "compressor-knee")];
    [_compressorBand6TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [_compressorBand6Slider floatValue]]];
    [_compressorBand7Slider setFloatValue: var_CreateGetFloat(p_playlist, "compressor-makeup-gain")];
    [_compressorBand7TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", [_compressorBand7Slider floatValue]]];
}

- (IBAction)resetCompressorValues:(id)sender
{
    playlist_t *p_playlist = pl_Get(getIntf());
    var_SetFloat(p_playlist, "compressor-rms-peak", 0.000000);
    var_SetFloat(p_playlist, "compressor-attack", 25.000000);
    var_SetFloat(p_playlist, "compressor-release", 100.000000);
    var_SetFloat(p_playlist, "compressor-threshold", -11.000000);
    var_SetFloat(p_playlist, "compressor-ratio", 8.000000);
    var_SetFloat(p_playlist, "compressor-knee", 2.500000);
    var_SetFloat(p_playlist, "compressor-makeup-gain", 7.000000);

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
    char *psz_property = nil;
    float f_value = [sender floatValue];

    if (sender == _compressorBand1Slider)
        psz_property = "compressor-rms-peak";
    else if (sender == _compressorBand2Slider)
        psz_property = "compressor-attack";
    else if (sender == _compressorBand3Slider)
        psz_property = "compressor-release";
    else if (sender == _compressorBand4Slider)
        psz_property = "compressor-threshold";
    else if (sender == _compressorBand5Slider)
        psz_property = "compressor-ratio";
    else if (sender == _compressorBand6Slider)
        psz_property = "compressor-knee";
    else if (sender == _compressorBand7Slider)
        psz_property = "compressor-makeup-gain";

    assert(psz_property);

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetFloat(p_aout, psz_property, f_value);
        vlc_object_release(p_aout);
    }
    var_SetFloat(pl_Get(getIntf()), psz_property, f_value);

    if (sender == _compressorBand1Slider)
        [_compressorBand1TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
    else if (sender == _compressorBand2Slider)
        [_compressorBand2TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f ms", f_value]];
    else if (sender == _compressorBand3Slider)
        [_compressorBand3TextField setStringValue:[NSString localizedStringWithFormat:@"%3.1f ms", f_value]];
    else if (sender == _compressorBand4Slider)
        [_compressorBand4TextField setStringValue:[NSString localizedStringWithFormat:@"%2.1f dB", f_value]];
    else if (sender == _compressorBand5Slider)
        [_compressorBand5TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f:1", f_value]];
    else if (sender == _compressorBand6Slider)
        [_compressorBand6TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", f_value]];
    else if (sender == _compressorBand7Slider)
        [_compressorBand7TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f dB", f_value]];
}

#pragma mark -
#pragma mark Spatializer
- (void)resetSpatializer
{
    playlist_t *p_playlist = pl_Get(getIntf());
    BOOL bEnable_spatializer = NO;
    char *psz_afilters;
    psz_afilters = var_InheritString(p_playlist, "audio-filter");
    if (psz_afilters) {
        bEnable_spatializer = strstr(psz_afilters, "spatializer") != NULL;
        free(psz_afilters);
    }

    [_spatializerView enableSubviews:bEnable_spatializer];
    [_spatializerEnableCheckbox setState:(bEnable_spatializer ? NSOnState : NSOffState)];


#define setSlider(bandsld, bandfld, var) \
[bandsld setFloatValue: var_CreateGetFloat(p_playlist, var) * 10.]; \
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
    playlist_t *p_playlist = pl_Get(getIntf());
    var_SetFloat(p_playlist, "spatializer-roomsize", .85);
    var_SetFloat(p_playlist, "spatializer-width", 1.);
    var_SetFloat(p_playlist, "spatializer-wet", .4);
    var_SetFloat(p_playlist, "spatializer-dry", .5);
    var_SetFloat(p_playlist, "spatializer-damp", .5);

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
    char *psz_property = nil;
    float f_value = [sender floatValue];

    if (sender == _spatializerBand1Slider)
        psz_property = "spatializer-roomsize";
    else if (sender == _spatializerBand2Slider)
        psz_property = "spatializer-width";
    else if (sender == _spatializerBand3Slider)
        psz_property = "spatializer-wet";
    else if (sender == _spatializerBand4Slider)
        psz_property = "spatializer-dry";
    else if (sender == _spatializerBand5Slider)
        psz_property = "spatializer-damp";

    assert(psz_property);

    audio_output_t *p_aout = getAout();
    if (p_aout) {
        var_SetFloat(p_aout, psz_property, f_value / 10.f);
        vlc_object_release(p_aout);
    }
    var_SetFloat(pl_Get(getIntf()), psz_property, f_value / 10.f);

    if (sender == _spatializerBand1Slider)
        [_spatializerBand1TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
    else if (sender == _spatializerBand2Slider)
        [_spatializerBand2TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
    else if (sender == _spatializerBand3Slider)
        [_spatializerBand3TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
    else if (sender == _spatializerBand4Slider)
        [_spatializerBand4TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
    else if (sender == _spatializerBand5Slider)
        [_spatializerBand5TextField setStringValue:[NSString localizedStringWithFormat:@"%1.1f", f_value]];
}

#pragma mark -
#pragma mark Filter
- (void)resetAudioFilters
{
    playlist_t *p_playlist = pl_Get(getIntf());
    BOOL bEnable_normvol = NO;
    char *psz_afilters;
    psz_afilters = var_InheritString(p_playlist, "audio-filter");
    if (psz_afilters) {
        [_filterHeadPhoneCheckbox setState: (NSInteger)strstr(psz_afilters, "headphone") ];
        [_filterKaraokeCheckbox setState: (NSInteger)strstr(psz_afilters, "karaoke") ];
        [_filterScaleTempoCheckbox setState: (NSInteger)strstr(psz_afilters, "scaletempo") ];
        [_filterStereoEnhancerCheckbox setState: (NSInteger)strstr(psz_afilters, "stereo_widen") ];
        bEnable_normvol = strstr(psz_afilters, "normvol") != NULL;
        free(psz_afilters);
    } else {
        [_filterHeadPhoneCheckbox setState: NSOffState];
        [_filterKaraokeCheckbox setState: NSOffState];
        [_filterScaleTempoCheckbox setState: NSOffState];
        [_filterStereoEnhancerCheckbox setState: NSOffState];
    }

    [_filterNormLevelSlider setEnabled:bEnable_normvol];
    [_filterNormLevelLabel setEnabled:bEnable_normvol];
    [_filterNormLevelCheckbox setState:(bEnable_normvol ? NSOnState : NSOffState)];

    [_filterNormLevelSlider setFloatValue: var_CreateGetFloat(p_playlist, "norm-max-level")];
}

- (IBAction)filterEnableHeadPhoneVirt:(id)sender
{
    [self setAudioFilter:"headphone" on:[sender state]];
}

- (IBAction)filterEnableVolumeNorm:(id)sender
{
    [_filterNormLevelSlider setEnabled:[sender state]];
    [_filterNormLevelLabel setEnabled:[sender state]];
    [self setAudioFilter:"normvol" on:[sender state]];
}

- (IBAction)filterVolumeNormSliderUpdated:(id)sender
{
    audio_output_t *p_aout = getAout();
    float f_value = [_filterNormLevelSlider floatValue];

    if (p_aout) {
        var_SetFloat(p_aout, "norm-max-level", f_value);
        vlc_object_release(p_aout);
    }

    var_SetFloat(pl_Get(getIntf()), "norm-max-level", f_value);
}

- (IBAction)filterEnableKaraoke:(id)sender
{
    [self setAudioFilter:"karaoke" on:[sender state]];
}

- (IBAction)filterEnableScaleTempo:(id)sender
{
    [self setAudioFilter:"scaletempo" on:[sender state]];
}

- (IBAction)filterEnableStereoEnhancer:(id)sender
{
    [self setAudioFilter:"stereo_widen" on:[sender state]];
}

@end
